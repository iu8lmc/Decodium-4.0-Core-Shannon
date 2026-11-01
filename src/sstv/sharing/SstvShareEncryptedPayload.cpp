// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvShareEncryptedPayload.h"
#include "SstvShareAead.h"
#include <QCryptographicHash>
#include <QJsonObject>

namespace decodium::sstv::sharing {
namespace {
QString digest(const QByteArray& data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}
std::optional<QByteArray> associatedData(const SstvShareManifestV1& manifest)
{
    if (!manifest.validate().ok() || manifest.encryption.mode != SstvShareEncryptionMode::EndToEnd
        || manifest.encryption.algorithm != QStringLiteral("aes-256-gcm")) return std::nullopt;
    auto object = manifest.toJsonObject();
    // The payload digest depends on the authentication tag, and the nonce is
    // already an authenticated input to GCM. Neither can be known before seal.
    object.remove(QStringLiteral("sha256"));
    auto encryption = object.value(QStringLiteral("encryption")).toObject();
    encryption.remove(QStringLiteral("nonceBase64"));
    object.insert(QStringLiteral("encryption"), encryption);
    SstvShareValidationError error;
    const auto canonical = canonicalJson(object, &error);
    if (!error.ok()) return std::nullopt;
    return QByteArrayLiteral("Decodium-SSTV-share-AAD-v1\0") + canonical;
}
}

std::optional<SstvShareEncryptedPayload> sstvShareEncryptPayload(
    SstvShareManifestV1 manifest, const QByteArray& key, const QByteArray& plaintext)
{
    if (plaintext.isEmpty() || quint64(plaintext.size()) > kMaximumSharedImageBytes - 16U)
        return std::nullopt;
    manifest.byteSize = quint64(plaintext.size()) + 16U;
    manifest.sha256 = QString(64, QLatin1Char('0'));
    manifest.encryption.nonceBase64 = QString::fromLatin1(QByteArray(12, '\0').toBase64());
    const auto aad = associatedData(manifest);
    if (!aad) return std::nullopt;
    const auto encrypted = sstvShareAeadSeal(key, plaintext, *aad);
    if (!encrypted) return std::nullopt;
    SstvShareEncryptedPayload result {manifest, encrypted->ciphertext + encrypted->tag};
    result.manifest.encryption.nonceBase64 = QString::fromLatin1(encrypted->nonce.toBase64());
    result.manifest.sha256 = digest(result.bytes);
    if (!result.manifest.validate().ok()) return std::nullopt;
    return result;
}

std::optional<QByteArray> sstvShareDecryptPayload(
    const SstvShareManifestV1& manifest, const QByteArray& key, const QByteArray& payload)
{
    const auto aad = associatedData(manifest);
    if (!aad || payload.size() < 17 || quint64(payload.size()) != manifest.byteSize
        || digest(payload) != manifest.sha256) return std::nullopt;
    const SstvShareAeadEnvelope envelope {
        QByteArray::fromBase64(manifest.encryption.nonceBase64.toLatin1()),
        payload.left(payload.size() - 16), payload.right(16)};
    return sstvShareAeadOpen(key, envelope, *aad);
}
} // namespace decodium::sstv::sharing
