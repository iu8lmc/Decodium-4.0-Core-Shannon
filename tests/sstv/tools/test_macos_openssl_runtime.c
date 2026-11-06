/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <stdio.h>
#include <string.h>

static int encryption_case(unsigned char seed)
{
    unsigned char key[32], nonce[12], plaintext[64], encrypted[64], decrypted[64], tag[16];
    int length = 0, final = 0, ok = 0;
    for (unsigned i = 0; i < sizeof(key); ++i) key[i] = (unsigned char)(seed + i);
    for (unsigned i = 0; i < sizeof(nonce); ++i) nonce[i] = (unsigned char)(seed * 3U + i);
    for (unsigned i = 0; i < sizeof(plaintext); ++i) plaintext[i] = (unsigned char)(seed ^ i);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return 0;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, nonce) != 1
        || EVP_EncryptUpdate(ctx, encrypted, &length, plaintext, sizeof(plaintext)) != 1
        || length != (int)sizeof(plaintext)
        || EVP_EncryptFinal_ex(ctx, encrypted + length, &final) != 1 || final != 0
        || EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, sizeof(tag), tag) != 1)
        goto done;
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, nonce) != 1
        || EVP_DecryptUpdate(ctx, decrypted, &length, encrypted, sizeof(encrypted)) != 1
        || EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, sizeof(tag), tag) != 1
        || EVP_DecryptFinal_ex(ctx, decrypted + length, &final) != 1
        || memcmp(plaintext, decrypted, sizeof(plaintext)) != 0)
        goto done;
    tag[0] ^= 1U;
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, nonce) != 1
        || EVP_DecryptUpdate(ctx, decrypted, &length, encrypted, sizeof(encrypted)) != 1
        || EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, sizeof(tag), tag) != 1
        || EVP_DecryptFinal_ex(ctx, decrypted + length, &final) != 0)
        goto done;
    ok = 1;
done:
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

static int key_exchange_case(void)
{
    EVP_PKEY *a = EVP_PKEY_Q_keygen(NULL, NULL, "X25519");
    EVP_PKEY *b = EVP_PKEY_Q_keygen(NULL, NULL, "X25519");
    EVP_PKEY_CTX *ac = a ? EVP_PKEY_CTX_new(a, NULL) : NULL;
    EVP_PKEY_CTX *bc = b ? EVP_PKEY_CTX_new(b, NULL) : NULL;
    unsigned char ab[32], ba[32];
    size_t ab_len = sizeof(ab), ba_len = sizeof(ba);
    int ok = ac && bc && EVP_PKEY_derive_init(ac) == 1 && EVP_PKEY_derive_set_peer(ac, b) == 1
        && EVP_PKEY_derive(ac, ab, &ab_len) == 1 && EVP_PKEY_derive_init(bc) == 1
        && EVP_PKEY_derive_set_peer(bc, a) == 1 && EVP_PKEY_derive(bc, ba, &ba_len) == 1
        && ab_len == sizeof(ab) && ba_len == sizeof(ba) && CRYPTO_memcmp(ab, ba, sizeof(ab)) == 0;
    EVP_PKEY_CTX_free(ac); EVP_PKEY_CTX_free(bc); EVP_PKEY_free(a); EVP_PKEY_free(b);
    return ok;
}

int main(void)
{
    if (!encryption_case(7U) || !encryption_case(193U)
        || !key_exchange_case() || !key_exchange_case()) {
        fputs("OpenSSL runtime crypto checks failed\n", stderr);
        return 1;
    }
    puts("PASS OpenSSL AES-GCM A/B, tamper rejection and X25519 A/B");
    return 0;
}
