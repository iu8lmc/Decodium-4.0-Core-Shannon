// SPDX-License-Identifier: GPL-3.0-or-later
// Diagnostic only: demonstrate the configured OpenSSL Ed25519 key checks.
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <array>
#include <cstddef>
#include <iostream>
#include <memory>

namespace {
using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using KeyContext = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using Context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

bool verify(const Key& key, const unsigned char* msg, std::size_t size,
            const std::array<unsigned char, 64>& signature)
{
    Context ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    return ctx && EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1
        && EVP_DigestVerify(ctx.get(), signature.data(), signature.size(), msg, size) == 1;
}
}

int main()
{
    // RFC 8032 section 7.1, test vector 2.
    const std::array<unsigned char, 32> publicKey {
        0x3d,0x40,0x17,0xc3,0xe8,0x43,0x89,0x5a,0x92,0xb7,0x0a,0xa7,0x4d,0x1b,0x7e,0xbc,
        0x9c,0x98,0x2c,0xcf,0x2e,0xc4,0x96,0x8c,0xc0,0xcd,0x55,0xf1,0x2a,0xf4,0x66,0x0c};
    const std::array<unsigned char, 64> signature {
        0x92,0xa0,0x09,0xa9,0xf0,0xd4,0xca,0xb8,0x72,0x0e,0x82,0x0b,0x5f,0x64,0x25,0x40,
        0xa2,0xb2,0x7b,0x54,0x16,0x50,0x3f,0x8f,0xb3,0x76,0x22,0x23,0xeb,0xdb,0x69,0xda,
        0x08,0x5a,0xc1,0xe4,0x3e,0x15,0x99,0x6e,0x45,0x8f,0x36,0x13,0xd0,0xf1,0x1d,0x8c,
        0x38,0x7b,0x2e,0xae,0xb4,0x30,0x2a,0xee,0xb0,0x0d,0x29,0x16,0x12,0xbb,0x0c,0x00};
    const unsigned char message[] {0x72};
    const unsigned char altered[] {0x73};
    Key valid(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, publicKey.data(), publicKey.size()), EVP_PKEY_free);
    if (!valid || !verify(valid, message, sizeof message, signature) || verify(valid, altered, sizeof altered, signature))
        return 1;
    std::array<unsigned char, 32> identity {}; identity[0] = 1;
    std::array<unsigned char, 64> forged {}; forged[0] = 1;
    Key weak(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, identity.data(), identity.size()), EVP_PKEY_free);
    if (!weak) return 2;
    const bool forgedA = verify(weak, message, sizeof message, forged);
    const bool forgedB = verify(weak, altered, sizeof altered, forged);
    if (forgedA != forgedB) return 3;
    KeyContext check(EVP_PKEY_CTX_new(weak.get(), nullptr), EVP_PKEY_CTX_free);
    const int publicCheck = check ? EVP_PKEY_public_check(check.get()) : -99;
    std::cout << OPENSSL_VERSION_TEXT << ": RFC vector valid; altered message rejected; identity forgery "
              << (forgedA ? "accepted" : "rejected") << "; public_check=" << publicCheck << '\n';
    return 0;
}
