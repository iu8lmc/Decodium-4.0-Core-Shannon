// SPDX-License-Identifier: GPL-3.0-or-later
// Candidate signing-backend diagnostic; not an authenticated sharing format.
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/x509.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

namespace {
using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using KeyContext = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using Context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
using Bytes = std::vector<unsigned char>;

Key generate(int curve)
{
    KeyContext context(EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr), EVP_PKEY_CTX_free);
    EVP_PKEY* raw = nullptr;
    if (!context || EVP_PKEY_keygen_init(context.get()) != 1
        || EVP_PKEY_CTX_set_ec_paramgen_curve_nid(context.get(), curve) != 1
        || EVP_PKEY_keygen(context.get(), &raw) != 1) return Key(nullptr, EVP_PKEY_free);
    return Key(raw, EVP_PKEY_free);
}

Bytes publicDer(const Key& key)
{
    const int size = i2d_PUBKEY(key.get(), nullptr);
    if (size <= 0 || size > 512) return {};
    Bytes result(static_cast<std::size_t>(size));
    unsigned char* cursor = result.data();
    if (i2d_PUBKEY(key.get(), &cursor) != size) return {};
    return result;
}

Key checkedP256(const Bytes& der)
{
    // Exact DER SubjectPublicKeyInfo prefix: id-ecPublicKey, prime256v1,
    // a 65-byte uncompressed point, with no trailing or alternate encoding.
    static constexpr std::array<unsigned char, 27> prefix {
        0x30,0x59,0x30,0x13,0x06,0x07,0x2a,0x86,0x48,0xce,0x3d,0x02,0x01,
        0x06,0x08,0x2a,0x86,0x48,0xce,0x3d,0x03,0x01,0x07,0x03,0x42,0x00,0x04};
    if (der.size() != 91 || !std::equal(prefix.begin(), prefix.end(), der.begin()))
        return Key(nullptr, EVP_PKEY_free);
    const unsigned char* cursor = der.data();
    Key key(d2i_PUBKEY(nullptr, &cursor, long(der.size())), EVP_PKEY_free);
    if (!key || cursor != der.data() + der.size()) return Key(nullptr, EVP_PKEY_free);
    KeyContext context(EVP_PKEY_CTX_new(key.get(), nullptr), EVP_PKEY_CTX_free);
    if (!context || EVP_PKEY_public_check(context.get()) != 1) return Key(nullptr, EVP_PKEY_free);
    return key;
}

Bytes sign(const Key& key, const Bytes& message)
{
    Context context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    size_t size = 0;
    if (!context || EVP_DigestSignInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1
        || EVP_DigestSign(context.get(), nullptr, &size, message.data(), message.size()) != 1
        || size > 80 || size == 0) return {};
    Bytes result(size);
    if (EVP_DigestSign(context.get(), result.data(), &size, message.data(), message.size()) != 1) return {};
    result.resize(size);
    return result;
}

bool verify(const Key& key, const Bytes& message, const Bytes& signature)
{
    Context context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    return key && context && !signature.empty()
        && EVP_DigestVerifyInit(context.get(), nullptr, EVP_sha256(), nullptr, key.get()) == 1
        && EVP_DigestVerify(context.get(), signature.data(), signature.size(), message.data(), message.size()) == 1;
}
}

int main()
{
    const auto fromHex = [](const char* text) {
        Bytes result;
        const auto nibble = [](char c) -> unsigned char {
            return static_cast<unsigned char>(c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10);
        };
        for (std::size_t i = 0; text[i] && text[i + 1]; i += 2)
            result.push_back(static_cast<unsigned char>((nibble(text[i]) << 4) | nibble(text[i + 1])));
        return result;
    };
    // RFC 6979 section A.2.5, P-256 / SHA-256 verification vectors.
    const auto rfcPublic = checkedP256(fromHex(
        "3059301306072a8648ce3d020106082a8648ce3d03010703420004"
        "60fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"
        "7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299"));
    const auto rfcSample = fromHex(
        "3046022100efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716"
        "022100f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff4064dc4ab2f843acda8");
    const auto rfcTest = fromHex(
        "3045022100f1abb023518351cd71d881567b1ea663ed3efcf6c5132b354f28d3b0b7d38367"
        "0220019f4113742a2b14bd25926b49c649155f267e60d3814b4c0cc84250e46f0083");
    const Bytes sample {'s','a','m','p','l','e'}, test {'t','e','s','t'};
    if (!rfcPublic || !verify(rfcPublic, sample, rfcSample)
        || !verify(rfcPublic, test, rfcTest) || verify(rfcPublic, test, rfcSample)) return 6;
    const auto a = generate(NID_X9_62_prime256v1);
    const auto b = generate(NID_X9_62_prime256v1);
    const auto wrongCurve = generate(NID_secp384r1);
    if (!a || !b || !wrongCurve) return 1;
    const auto aDer = publicDer(a), bDer = publicDer(b);
    const auto aPublic = checkedP256(aDer), bPublic = checkedP256(bDer);
    if (!aPublic || !bPublic || aDer == bDer || checkedP256(publicDer(wrongCurve))) return 2;
    const Bytes messageA {'a','l','i','c','e'}, messageB {'b','o','b'};
    const auto signatureA = sign(a, messageA), signatureB = sign(b, messageB);
    if (!verify(aPublic, messageA, signatureA) || !verify(bPublic, messageB, signatureB)
        || verify(aPublic, messageB, signatureA) || verify(bPublic, messageA, signatureA)) return 3;
    auto alteredSignature = signatureB;
    alteredSignature.back() ^= 1;
    if (verify(bPublic, messageB, alteredSignature)) return 4;
    auto zeros = aDer; std::fill(zeros.begin() + 27, zeros.end(), 0);
    auto wrongFormat = aDer; wrongFormat[26] = 0;
    auto trailing = aDer; trailing.push_back(0);
    if (checkedP256(zeros) || checkedP256(wrongFormat) || checkedP256(trailing)) return 5;
    std::cout << "P-256 A/B signatures verified; changed messages, wrong keys, invalid points, wrong curves and trailing DER rejected\n";
    return 0;
}
