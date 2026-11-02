// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvShareSecureEnvelope.h"
#include "SstvShareP256.h"
#include <QJsonObject>
#include <openssl/crypto.h>

namespace decodium::sstv::sharing {
namespace {
std::optional<QByteArray> signedManifestBytes(const SstvShareManifestV1& manifest)
{
    if (!manifest.validate().ok() || manifest.encryption.senderAuthAlgorithm != QStringLiteral("ecdsa-p256-sha256-v1"))
        return std::nullopt;
    QJsonObject object = manifest.toJsonObject();
    QJsonObject encryption = object.value(QStringLiteral("encryption")).toObject();
    encryption.remove(QStringLiteral("senderSignatureBase64"));
    object.insert(QStringLiteral("encryption"), encryption);
    SstvShareValidationError error;
    const QByteArray canonical = canonicalJson(object, &error);
    if (!error.ok()) return std::nullopt;
    return QByteArrayLiteral("Decodium-SSTV-sender-signature-v1\0") + canonical;
}
}
std::function<std::optional<QByteArray>(const SstvShareManifestV1&, const QByteArray&)>
sstvShareMakeStoredEnvelopeOpener(std::shared_ptr<const secure_settings::Backend> backend,
                                 QString profile, QString expectedProvider,
                                 QString expectedRecipient, bool requireSenderAuthentication)
{
    return [backend = std::move(backend), profile = std::move(profile),
            expectedProvider = std::move(expectedProvider), expectedRecipient = std::move(expectedRecipient),
            requireSenderAuthentication](
        const SstvShareManifestV1& manifest, const QByteArray& payload) -> std::optional<QByteArray> {
        if (!backend) return std::nullopt;
        const SstvShareKeyStore keys(*backend, profile);
        return sstvShareOpenStoredEnvelope(keys, expectedProvider, expectedRecipient, manifest, payload, requireSenderAuthentication);
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

std::optional<SstvShareEncryptedPayload> sstvShareSealSignedEnvelope(
    const SstvShareKeyStore& keys, SstvShareManifestV1 manifest, const QByteArray& plaintext,
    const SstvShareKeyStore* signingKeys)
{
    auto privateKey = (signingKeys ? *signingKeys : keys).loadSigningPrivateKey();
    if (!privateKey.ok()) return std::nullopt;
    const auto publicKey = sstvShareP256PublicKey(*privateKey.key);
    if (!publicKey) {
        OPENSSL_cleanse(privateKey.key->data(), size_t(privateKey.key->size()));
        return std::nullopt;
    }
    manifest.encryption.senderAuthAlgorithm = QStringLiteral("ecdsa-p256-sha256-v1");
    manifest.encryption.senderKeyFingerprint = sstvShareP256Fingerprint(*publicKey);
    // Valid canonical low-S DER placeholder; the signature is excluded from
    // AEAD associated data and replaced only after the ciphertext hash exists.
    manifest.encryption.senderSignatureBase64 = QString::fromLatin1(QByteArray::fromHex("3006020101020101").toBase64());
    auto sealed = sstvShareSealPinnedEnvelope(keys, std::move(manifest), plaintext);
    const auto message = sealed ? signedManifestBytes(sealed->manifest) : std::nullopt;
    const auto signature = message ? sstvShareP256Sign(*privateKey.key, *message) : std::nullopt;
    OPENSSL_cleanse(privateKey.key->data(), size_t(privateKey.key->size()));
    if (!sealed || !signature) return std::nullopt;
    sealed->manifest.encryption.senderSignatureBase64 = QString::fromLatin1(signature->toBase64());
    if (!sealed->manifest.validate().ok()) return std::nullopt;
    return sealed;
}

std::optional<QByteArray> sstvShareOpenStoredEnvelope(
    const SstvShareKeyStore& keys, const QString& expectedProvider,
    const QString& expectedRecipient, const SstvShareManifestV1& manifest,
    const QByteArray& payload, bool requireSenderAuthentication)
{
    if (!isSafeShareIdentifier(expectedProvider) || !isSafeShareIdentifier(expectedRecipient)
        || manifest.providerId != expectedProvider || manifest.recipientId != expectedRecipient
        || manifest.encryption.mode != SstvShareEncryptionMode::EndToEnd)
        return std::nullopt;
    const bool hasSenderAuthentication = !manifest.encryption.senderAuthAlgorithm.isEmpty();
    if (requireSenderAuthentication && !hasSenderAuthentication) return std::nullopt;
    if (hasSenderAuthentication) {
        const auto sender = keys.pinnedSenderSigningKey(manifest.providerId, manifest.senderId);
        const auto message = signedManifestBytes(manifest);
        const QByteArray signature = QByteArray::fromBase64(
            manifest.encryption.senderSignatureBase64.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
        if (!sender.ok() || !message || sstvShareP256Fingerprint(*sender.key) != manifest.encryption.senderKeyFingerprint
            || !sstvShareP256Verify(*sender.key, *message, signature)) return std::nullopt;
    }
    auto identity = keys.loadPrivateKey();
    if (!identity.ok()) return std::nullopt;
    auto plaintext = sstvShareDecryptForRecipient(manifest, *identity.key, payload);
    OPENSSL_cleanse(identity.key->data(), size_t(identity.key->size()));
    return plaintext;
}
} // namespace decodium::sstv::sharing
