// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvShareP256.h"

#include <QCryptographicHash>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/x509.h>
#include <algorithm>
#include <array>
#include <memory>

namespace decodium::sstv::sharing {
namespace {
using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using KeyContext = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using DigestContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
using Signature = std::unique_ptr<ECDSA_SIG, decltype(&ECDSA_SIG_free)>;
using Number = std::unique_ptr<BIGNUM, decltype(&BN_free)>;
const unsigned char* bytes(const QByteArray& value)
{
    return reinterpret_cast<const unsigned char*>(value.constData());
}
constexpr std::array<unsigned char, 27> kP256SpkiPrefix {
    0x30,0x59,0x30,0x13,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x02,0x01,
    0x06,0x08,0x2a,0x86,0x48,0xce,0x3d,0x03,0x01,0x07,0x03,0x42,0x00,0x04};

Key checkedPublicKey(const QByteArray& der)
{
    if (der.size() != 91 || !std::equal(kP256SpkiPrefix.begin(), kP256SpkiPrefix.end(), bytes(der)))
        return Key(nullptr, EVP_PKEY_free);
    const unsigned char* cursor = bytes(der);
    Key key(d2i_PUBKEY(nullptr, &cursor, long(der.size())), EVP_PKEY_free);
    if (!key || cursor != bytes(der) + der.size()) return Key(nullptr, EVP_PKEY_free);
    KeyContext context(EVP_PKEY_CTX_new(key.get(), nullptr), EVP_PKEY_CTX_free);
    if (!context || EVP_PKEY_public_check(context.get()) != 1) return Key(nullptr, EVP_PKEY_free);
    return key;
}

std::optional<QByteArray> publicDer(const Key& key)
{
    if (!key) return std::nullopt;
    const int size = i2d_PUBKEY(key.get(), nullptr);
    if (size != 91) return std::nullopt;
    QByteArray result(size, Qt::Uninitialized);
    unsigned char* cursor = reinterpret_cast<unsigned char*>(result.data());
    if (i2d_PUBKEY(key.get(), &cursor) != size || !checkedPublicKey(result)) return std::nullopt;
    return result;
}

// Canonical DER and low-S prevent trivial ECDSA signature malleability from
// changing the persisted manifest idempotency key. Values are public.
std::optional<QByteArray> canonicalSignature(const QByteArray& der, bool normalize)
{
    if (der.size() < 8 || der.size() > 72) return std::nullopt;
    const unsigned char* cursor = bytes(der);
    Signature signature(d2i_ECDSA_SIG(nullptr, &cursor, long(der.size())), ECDSA_SIG_free);
    if (!signature || cursor != bytes(der) + der.size()) return std::nullopt;
    const int canonicalSize = i2d_ECDSA_SIG(signature.get(), nullptr);
    if (canonicalSize != der.size()) return std::nullopt;
    QByteArray canonical(canonicalSize, Qt::Uninitialized);
    unsigned char* out = reinterpret_cast<unsigned char*>(canonical.data());
    if (i2d_ECDSA_SIG(signature.get(), &out) != canonicalSize || canonical != der) return std::nullopt;
    static const QByteArray orderBytes = QByteArray::fromHex(
        "ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551");
    Number order(BN_bin2bn(bytes(orderBytes), int(orderBytes.size()), nullptr), BN_free);
    Number half(BN_new(), BN_free);
    const BIGNUM *r = nullptr, *s = nullptr;
    ECDSA_SIG_get0(signature.get(), &r, &s);
    if (!order || !half || !r || !s || BN_rshift1(half.get(), order.get()) != 1
        || BN_is_negative(r) || BN_is_zero(r) || BN_cmp(r, order.get()) >= 0
        || BN_is_negative(s) || BN_is_zero(s) || BN_cmp(s, order.get()) >= 0) return std::nullopt;
    if (BN_cmp(s, half.get()) <= 0) return canonical;
    if (!normalize) return std::nullopt;
    BIGNUM* newR = BN_dup(r);
    BIGNUM* newS = BN_new();
    if (!newR || !newS || BN_sub(newS, order.get(), s) != 1
        || ECDSA_SIG_set0(signature.get(), newR, newS) != 1) {
        BN_free(newR); BN_free(newS);
        return std::nullopt;
    }
    const int size = i2d_ECDSA_SIG(signature.get(), nullptr);
    if (size < 8 || size > 72) return std::nullopt;
    QByteArray result(size, Qt::Uninitialized);
    out = reinterpret_cast<unsigned char*>(result.data());
    if (i2d_ECDSA_SIG(signature.get(), &out) != size) return std::nullopt;
    return result;
}

Key checkedPrivateKey(const QByteArray& der)
{
    if (der.isEmpty() || der.size() > 512) return Key(nullptr, EVP_PKEY_free);
    const unsigned char* cursor = bytes(der);
    Key key(d2i_AutoPrivateKey(nullptr, &cursor, long(der.size())), EVP_PKEY_free);
    if (!key || cursor != bytes(der) + der.size() || !publicDer(key)) return Key(nullptr, EVP_PKEY_free);
    KeyContext context(EVP_PKEY_CTX_new(key.get(), nullptr), EVP_PKEY_CTX_free);
    if (!context || EVP_PKEY_check(context.get()) != 1) return Key(nullptr, EVP_PKEY_free);
    // Require the unique DER serialization emitted by this backend.
    const int size = i2d_PrivateKey(key.get(), nullptr);
    if (size != der.size()) return Key(nullptr, EVP_PKEY_free);
    QByteArray encoded(size, Qt::Uninitialized);
    unsigned char* output = reinterpret_cast<unsigned char*>(encoded.data());
    const bool match = i2d_PrivateKey(key.get(), &output) == size && encoded == der;
    OPENSSL_cleanse(encoded.data(), size_t(encoded.size()));
    if (!match) return Key(nullptr, EVP_PKEY_free);
    return key;
}
}

std::optional<SstvShareP256KeyPair> sstvShareGenerateP256KeyPair()
{
    KeyContext context(EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr), EVP_PKEY_CTX_free);
    EVP_PKEY* raw = nullptr;
    if (!context || EVP_PKEY_keygen_init(context.get()) != 1
        || EVP_PKEY_CTX_set_ec_paramgen_curve_nid(context.get(), NID_X9_62_prime256v1) != 1
        || EVP_PKEY_keygen(context.get(), &raw) != 1) return std::nullopt;
    Key key(raw, EVP_PKEY_free);
    const int size = i2d_PrivateKey(key.get(), nullptr);
    const auto publicKey = publicDer(key);
    if (size <= 0 || size > 512 || !publicKey) return std::nullopt;
    QByteArray privateKey(size, Qt::Uninitialized);
    unsigned char* cursor = reinterpret_cast<unsigned char*>(privateKey.data());
    if (i2d_PrivateKey(key.get(), &cursor) != size || !checkedPrivateKey(privateKey)) {
        OPENSSL_cleanse(privateKey.data(), size_t(privateKey.size()));
        return std::nullopt;
    }
    return SstvShareP256KeyPair {std::move(privateKey), *publicKey};
}

std::optional<QByteArray> sstvShareP256PublicKey(const QByteArray& privateDer)
{
    return publicDer(checkedPrivateKey(privateDer));
}
bool sstvShareP256PublicKeyValid(const QByteArray& publicKey)
{
    return bool(checkedPublicKey(publicKey));
}
QString sstvShareP256Fingerprint(const QByteArray& publicKey)
{
    if (!sstvShareP256PublicKeyValid(publicKey)) return {};
    return QString::fromLatin1(QCryptographicHash::hash(publicKey, QCryptographicHash::Sha256).toHex());
}
std::optional<QByteArray> sstvShareP256Sign(const QByteArray& privateDer, const QByteArray& message)
{
    if (message.size() > kSstvShareP256MaximumMessageBytes) return std::nullopt;
    const Key key = checkedPrivateKey(privateDer);
    DigestContext context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    size_t length = 0;
    if (!key || !context || EVP_DigestSignInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1
        || EVP_DigestSign(context.get(), nullptr, &length, bytes(message), size_t(message.size())) != 1
        || length == 0 || length > 72) return std::nullopt;
    QByteArray signature(qsizetype(length), Qt::Uninitialized);
    if (EVP_DigestSign(context.get(), reinterpret_cast<unsigned char*>(signature.data()), &length,
                       bytes(message), size_t(message.size())) != 1 || length > 72) return std::nullopt;
    signature.resize(qsizetype(length));
    return canonicalSignature(signature, true);
}
bool sstvShareP256Verify(const QByteArray& publicKey, const QByteArray& message, const QByteArray& signature)
{
    if (message.size() > kSstvShareP256MaximumMessageBytes || signature.isEmpty() || signature.size() > 72)
        return false;
    if (!canonicalSignature(signature, false)) return false;
    const Key key = checkedPublicKey(publicKey);
    DigestContext context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    return key && context && EVP_DigestVerifyInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) == 1
        && EVP_DigestVerify(context.get(), bytes(signature), size_t(signature.size()),
                            bytes(message), size_t(message.size())) == 1;
}
} // namespace decodium::sstv::sharing
