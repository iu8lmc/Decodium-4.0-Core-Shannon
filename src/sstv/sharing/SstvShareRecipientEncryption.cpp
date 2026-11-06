// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvShareRecipientEncryption.h"
#include "SstvShareKeyAgreement.h"
#include <QCryptographicHash>
#include <openssl/crypto.h>

namespace decodium::sstv::sharing {
namespace {
const QString kKeyPrefix = QStringLiteral("x25519-v1-");
void wipe(QByteArray& value) { if (!value.isEmpty()) OPENSSL_cleanse(value.data(), size_t(value.size())); }
QByteArray context(const SstvShareManifestV1& manifest)
{
    return manifest.transferId.toString(QUuid::WithoutBraces).toLower().toLatin1();
}
}
QString sstvShareX25519Fingerprint(const QByteArray& publicKey)
{
    if (publicKey.size() != 32) return {};
    return QString::fromLatin1(QCryptographicHash::hash(publicKey, QCryptographicHash::Sha256).toHex());
}
std::optional<SstvShareEncryptedPayload> sstvShareEncryptForRecipient(
    SstvShareManifestV1 manifest, const QByteArray& recipientPublicKey,
    const QString& verifiedFingerprint, const QByteArray& plaintext)
{
    const auto fingerprint = sstvShareX25519Fingerprint(recipientPublicKey);
    if (fingerprint.isEmpty() || fingerprint != verifiedFingerprint || manifest.transferId.isNull())
        return std::nullopt;
    auto ephemeral = sstvShareGenerateX25519KeyPair();
    if (!ephemeral) return std::nullopt;
    auto key = sstvShareDeriveTransferKey(SstvShareKeyRole::SenderEphemeral,
        ephemeral->privateKey, ephemeral->publicKey, recipientPublicKey, context(manifest));
    wipe(ephemeral->privateKey);
    if (!key) return std::nullopt;
    manifest.encryption.mode = SstvShareEncryptionMode::EndToEnd;
    manifest.encryption.algorithm = QStringLiteral("aes-256-gcm");
    manifest.encryption.keyId = kKeyPrefix + QString::fromLatin1(ephemeral->publicKey.toHex());
    manifest.encryption.recipientKeyFingerprint = fingerprint;
    manifest.encryption.manifestBoundAsAuthenticatedData = true;
    manifest.encryption.downgradeProtected = true;
    manifest.transport.providerCanReadContent = false;
    auto result = sstvShareEncryptPayload(manifest, *key, plaintext);
    wipe(*key);
    return result;
}
std::optional<QByteArray> sstvShareDecryptForRecipient(
    const SstvShareManifestV1& manifest, const QByteArray& recipientPrivateKey,
    const QByteArray& payload)
{
    if (!manifest.encryption.keyId.startsWith(kKeyPrefix) || manifest.transferId.isNull())
        return std::nullopt;
    const auto encoded = manifest.encryption.keyId.mid(kKeyPrefix.size()).toLatin1();
    const auto senderPublic = QByteArray::fromHex(encoded);
    const auto recipientPublic = sstvShareX25519PublicKey(recipientPrivateKey);
    if (encoded.size() != 64 || senderPublic.size() != 32 || senderPublic.toHex() != encoded
        || !recipientPublic || sstvShareX25519Fingerprint(*recipientPublic) != manifest.encryption.recipientKeyFingerprint)
        return std::nullopt;
    auto key = sstvShareDeriveTransferKey(SstvShareKeyRole::Recipient,
        recipientPrivateKey, senderPublic, *recipientPublic, context(manifest));
    if (!key) return std::nullopt;
    auto plaintext = sstvShareDecryptPayload(manifest, *key, payload);
    wipe(*key);
    return plaintext;
}
} // namespace decodium::sstv::sharing
