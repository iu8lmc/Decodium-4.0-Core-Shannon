// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvShareSecureEnvelope.h"
#include <openssl/crypto.h>

namespace decodium::sstv::sharing {
std::function<std::optional<QByteArray>(const SstvShareManifestV1&, const QByteArray&)>
sstvShareMakeStoredEnvelopeOpener(std::shared_ptr<const secure_settings::Backend> backend,
                                 QString profile, QString expectedProvider,
                                 QString expectedRecipient)
{
    return [backend = std::move(backend), profile = std::move(profile),
            expectedProvider = std::move(expectedProvider), expectedRecipient = std::move(expectedRecipient)](
        const SstvShareManifestV1& manifest, const QByteArray& payload) -> std::optional<QByteArray> {
        if (!backend) return std::nullopt;
        const SstvShareKeyStore keys(*backend, profile);
        return sstvShareOpenStoredEnvelope(keys, expectedProvider, expectedRecipient, manifest, payload);
    };
}

std::optional<SstvShareEncryptedPayload> sstvShareSealPinnedEnvelope(
    const SstvShareKeyStore& keys, SstvShareManifestV1 manifest,
    const QByteArray& plaintext)
{
    const auto recipient = keys.pinnedRecipient(manifest.providerId, manifest.recipientId);
    if (!recipient.ok()) return std::nullopt;
    return sstvShareEncryptForRecipient(manifest, *recipient.key,
        sstvShareX25519Fingerprint(*recipient.key), plaintext);
}

std::optional<QByteArray> sstvShareOpenStoredEnvelope(
    const SstvShareKeyStore& keys, const QString& expectedProvider,
    const QString& expectedRecipient, const SstvShareManifestV1& manifest,
    const QByteArray& payload)
{
    if (!isSafeShareIdentifier(expectedProvider) || !isSafeShareIdentifier(expectedRecipient)
        || manifest.providerId != expectedProvider || manifest.recipientId != expectedRecipient
        || manifest.encryption.mode != SstvShareEncryptionMode::EndToEnd)
        return std::nullopt;
    auto identity = keys.loadPrivateKey();
    if (!identity.ok()) return std::nullopt;
    auto plaintext = sstvShareDecryptForRecipient(manifest, *identity.key, payload);
    OPENSSL_cleanse(identity.key->data(), size_t(identity.key->size()));
    return plaintext;
}
} // namespace decodium::sstv::sharing
