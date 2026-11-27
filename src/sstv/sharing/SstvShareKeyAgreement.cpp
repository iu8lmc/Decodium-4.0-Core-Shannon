// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvShareKeyAgreement.h"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <array>
#include <memory>

namespace decodium::sstv::sharing {
namespace {
using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using Context = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
const unsigned char* bytes(const QByteArray& b) { return reinterpret_cast<const unsigned char*>(b.constData()); }
Key privateKey(const QByteArray& b) { return Key(EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, bytes(b), size_t(b.size())), EVP_PKEY_free); }
Key publicKey(const QByteArray& b) { return Key(EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, bytes(b), size_t(b.size())), EVP_PKEY_free); }
}
std::optional<QByteArray> sstvShareX25519PublicKey(const QByteArray& input)
{
    if (input.size() != 32) return std::nullopt;
    const auto key = privateKey(input);
    QByteArray result(32, Qt::Uninitialized);
    size_t length = 32;
    if (!key || EVP_PKEY_get_raw_public_key(key.get(), reinterpret_cast<unsigned char*>(result.data()), &length) != 1 || length != 32)
        return std::nullopt;
    return result;
}
std::optional<SstvShareX25519KeyPair> sstvShareGenerateX25519KeyPair()
{
    Context ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr), EVP_PKEY_CTX_free);
    EVP_PKEY* raw = nullptr;
    if (!ctx || EVP_PKEY_keygen_init(ctx.get()) != 1 || EVP_PKEY_keygen(ctx.get(), &raw) != 1)
        return std::nullopt;
    Key key(raw, EVP_PKEY_free);
    SstvShareX25519KeyPair result {QByteArray(32, Qt::Uninitialized), QByteArray(32, Qt::Uninitialized)};
    size_t privlen = 32, publen = 32;
    if (EVP_PKEY_get_raw_private_key(key.get(), reinterpret_cast<unsigned char*>(result.privateKey.data()), &privlen) != 1
        || EVP_PKEY_get_raw_public_key(key.get(), reinterpret_cast<unsigned char*>(result.publicKey.data()), &publen) != 1
        || privlen != 32 || publen != 32) {
        OPENSSL_cleanse(result.privateKey.data(), size_t(result.privateKey.size()));
        return std::nullopt;
    }
    return result;
}
std::optional<QByteArray> sstvShareDeriveTransferKey(
    SstvShareKeyRole role, const QByteArray& privateBytes,
    const QByteArray& senderPublic, const QByteArray& recipientPublic,
    const QByteArray& context)
{
    if (privateBytes.size() != 32 || senderPublic.size() != 32 || recipientPublic.size() != 32
        || context.isEmpty() || context.size() > 256
        || (role != SstvShareKeyRole::SenderEphemeral && role != SstvShareKeyRole::Recipient)) return std::nullopt;
    const auto ownPublic = sstvShareX25519PublicKey(privateBytes);
    const auto& expectedOwn = role == SstvShareKeyRole::SenderEphemeral ? senderPublic : recipientPublic;
    const auto& peerPublic = role == SstvShareKeyRole::SenderEphemeral ? recipientPublic : senderPublic;
    if (!ownPublic || *ownPublic != expectedOwn) return std::nullopt;
    const auto own = privateKey(privateBytes);
    const auto peer = publicKey(peerPublic);
    Context derive(own ? EVP_PKEY_CTX_new(own.get(), nullptr) : nullptr, EVP_PKEY_CTX_free);
    std::array<unsigned char, 32> shared {};
    size_t sharedLength = shared.size();
    if (!peer || !derive || EVP_PKEY_derive_init(derive.get()) != 1
        || EVP_PKEY_derive_set_peer(derive.get(), peer.get()) != 1
        || EVP_PKEY_derive(derive.get(), shared.data(), &sharedLength) != 1 || sharedLength != shared.size()) {
        OPENSSL_cleanse(shared.data(), shared.size());
        return std::nullopt;
    }
    // Fixed domain salt and role-ordered public keys provide separation across
    // protocols and recipients; transfer context is at the end and bounded.
    const QByteArray salt = QByteArrayLiteral("Decodium-SSTV-X25519-HKDF-v1");
    const QByteArray info = QByteArrayLiteral("AES-256-GCM\0") + senderPublic + recipientPublic + context;
    Context hkdf(EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr), EVP_PKEY_CTX_free);
    QByteArray result(32, Qt::Uninitialized);
    size_t outlen = 32;
    const bool ok = hkdf && EVP_PKEY_derive_init(hkdf.get()) == 1
        && EVP_PKEY_CTX_set_hkdf_md(hkdf.get(), EVP_sha256()) == 1
        && EVP_PKEY_CTX_set1_hkdf_salt(hkdf.get(), bytes(salt), int(salt.size())) == 1
        && EVP_PKEY_CTX_set1_hkdf_key(hkdf.get(), shared.data(), int(shared.size())) == 1
        && EVP_PKEY_CTX_add1_hkdf_info(hkdf.get(), bytes(info), int(info.size())) == 1
        && EVP_PKEY_derive(hkdf.get(), reinterpret_cast<unsigned char*>(result.data()), &outlen) == 1 && outlen == 32;
    OPENSSL_cleanse(shared.data(), shared.size());
    if (!ok) {
        OPENSSL_cleanse(result.data(), size_t(result.size()));
        return std::nullopt;
    }
    return result;
}
} // namespace decodium::sstv::sharing
