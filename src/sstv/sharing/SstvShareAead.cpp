// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvShareAead.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <memory>

namespace decodium::sstv::sharing {
namespace {
using Context = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
constexpr qsizetype kMaximumBytes = 64 * 1024 * 1024;
bool bounded(const QByteArray& data) noexcept { return data.size() <= kMaximumBytes; }
const unsigned char* bytes(const QByteArray& data) noexcept
{ return reinterpret_cast<const unsigned char*>(data.constData()); }
unsigned char* mutableBytes(QByteArray& data) noexcept
{ return reinterpret_cast<unsigned char*>(data.data()); }
}

std::optional<SstvShareAeadEnvelope> sstvShareAeadSeal(
    const QByteArray& key, const QByteArray& plaintext, const QByteArray& aad)
{
    if (key.size() != 32 || !bounded(plaintext) || !bounded(aad)) return std::nullopt;
    Context ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) return std::nullopt;
    SstvShareAeadEnvelope result;
    result.nonce.resize(12);
    result.ciphertext.resize(plaintext.size() + 16);
    result.tag.resize(16);
    int n = 0, output = 0;
    if (RAND_bytes(mutableBytes(result.nonce), 12) != 1
        || EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1
        || EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, 12, nullptr) != 1
        || EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, bytes(key), bytes(result.nonce)) != 1
        || (!aad.isEmpty() && EVP_EncryptUpdate(ctx.get(), nullptr, &n, bytes(aad), int(aad.size())) != 1)
        || EVP_EncryptUpdate(ctx.get(), mutableBytes(result.ciphertext), &output, bytes(plaintext), int(plaintext.size())) != 1
        || EVP_EncryptFinal_ex(ctx.get(), mutableBytes(result.ciphertext) + output, &n) != 1)
        return std::nullopt;
    result.ciphertext.resize(output + n);
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_GET_TAG, 16, mutableBytes(result.tag)) != 1)
        return std::nullopt;
    return result;
}

std::optional<QByteArray> sstvShareAeadOpen(
    const QByteArray& key, const SstvShareAeadEnvelope& envelope, const QByteArray& aad)
{
    if (key.size() != 32 || envelope.nonce.size() != 12 || envelope.tag.size() != 16
        || !bounded(envelope.ciphertext) || !bounded(aad)) return std::nullopt;
    Context ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) return std::nullopt;
    QByteArray plaintext(envelope.ciphertext.size() + 16, Qt::Uninitialized);
    int n = 0, output = 0;
    bool ok = EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
        && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, 12, nullptr) == 1
        && EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, bytes(key), bytes(envelope.nonce)) == 1
        && (aad.isEmpty() || EVP_DecryptUpdate(ctx.get(), nullptr, &n, bytes(aad), int(aad.size())) == 1)
        && EVP_DecryptUpdate(ctx.get(), mutableBytes(plaintext), &output, bytes(envelope.ciphertext), int(envelope.ciphertext.size())) == 1
        && EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_TAG, 16, const_cast<char*>(envelope.tag.constData())) == 1
        && EVP_DecryptFinal_ex(ctx.get(), mutableBytes(plaintext) + output, &n) == 1;
    if (!ok) {
        OPENSSL_cleanse(plaintext.data(), static_cast<std::size_t>(plaintext.size()));
        return std::nullopt;
    }
    plaintext.resize(output + n);
    return plaintext;
}

} // namespace decodium::sstv::sharing
