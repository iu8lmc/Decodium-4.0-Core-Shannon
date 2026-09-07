// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvShareManifest.h"

#include <QByteArray>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <cmath>
#include <limits>

namespace decodium::sstv::sharing {
namespace {

QString utcTimestamp(const QDateTime& value)
{
    return value.toUTC().toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss.zzz'Z'"));
}

std::optional<QDateTime> parseUtcTimestamp(const QString& text)
{
    QDateTime parsed = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!parsed.isValid() || parsed.offsetFromUtc() != 0) {
        return std::nullopt;
    }
    parsed = parsed.toUTC();
    if (utcTimestamp(parsed) != text) {
        return std::nullopt;
    }
    return parsed;
}

SstvShareValidationError exactKeys(const QJsonObject& object,
                                   const QSet<QString>& expected,
                                   const QString& prefix)
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!expected.contains(it.key())) {
            return SstvShareValidationError::failure(
                SstvShareValidationCode::UnknownField,
                prefix + it.key());
        }
    }
    for (const QString& key : expected) {
        if (!object.contains(key)) {
            return SstvShareValidationError::failure(
                SstvShareValidationCode::MissingField,
                prefix + key);
        }
    }
    return {};
}

SstvShareValidationError requireType(const QJsonObject& object,
                                     const QString& key,
                                     QJsonValue::Type type,
                                     const QString& prefix = {})
{
    if (!object.contains(key)) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::MissingField, prefix + key);
    }
    if (object.value(key).type() != type) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::WrongType, prefix + key);
    }
    return {};
}

std::optional<quint64> unsignedInteger(const QJsonValue& value)
{
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    constexpr double maximumExactJsonInteger = 9'007'199'254'740'991.0;
    if (!std::isfinite(number) || number < 0.0 || std::trunc(number) != number
        || number > maximumExactJsonInteger) {
        return std::nullopt;
    }
    return static_cast<quint64>(number);
}

bool isKnownImageMimeType(const QString& value)
{
    return value == QStringLiteral("image/png")
        || value == QStringLiteral("image/jpeg");
}

bool isSafeCallsign(const QString& value)
{
    if (value.isEmpty()) {
        return true;
    }
    static const QRegularExpression pattern(
        QStringLiteral(R"(^[A-Z0-9][A-Z0-9/-]{1,22}[A-Z0-9]$)"));
    return pattern.match(value).hasMatch();
}

bool isSafeGrid(const QString& value)
{
    if (value.isEmpty()) {
        return true;
    }
    static const QRegularExpression pattern(
        QStringLiteral(R"(^[A-R]{2}[0-9]{2}(?:[A-X]{2}(?:[0-9]{2})?)?$)"));
    return pattern.match(value).hasMatch();
}

bool parseMediaSource(const QString& value, SstvShareMediaSource& output)
{
    if (value == QStringLiteral("analog-reception")) {
        output = SstvShareMediaSource::AnalogReception;
    } else if (value == QStringLiteral("analog-transmission")) {
        output = SstvShareMediaSource::AnalogTransmission;
    } else if (value == QStringLiteral("digital-reception")) {
        output = SstvShareMediaSource::DigitalReception;
    } else if (value == QStringLiteral("digital-transmission")) {
        output = SstvShareMediaSource::DigitalTransmission;
    } else {
        return false;
    }
    return true;
}

bool parseCompletion(const QString& value, SstvShareContentCompletion& output)
{
    if (value == QStringLiteral("complete")) {
        output = SstvShareContentCompletion::Complete;
    } else if (value == QStringLiteral("partial")) {
        output = SstvShareContentCompletion::Partial;
    } else {
        return false;
    }
    return true;
}

bool parseDisposition(const QString& value, SstvShareContentDisposition& output)
{
    if (value == QStringLiteral("attachment")) {
        output = SstvShareContentDisposition::Attachment;
    } else if (value == QStringLiteral("inline-preview")) {
        output = SstvShareContentDisposition::InlinePreview;
    } else {
        return false;
    }
    return true;
}

bool parseEncryptionMode(const QString& value, SstvShareEncryptionMode& output)
{
    if (value == QStringLiteral("transport-tls")) {
        output = SstvShareEncryptionMode::TransportTls;
    } else if (value == QStringLiteral("end-to-end")) {
        output = SstvShareEncryptionMode::EndToEnd;
    } else {
        return false;
    }
    return true;
}

SstvShareValidationError parseEncryption(const QJsonObject& object,
                                         SstvShareEncryptionInfo& output)
{
    static const QSet<QString> keys {
        QStringLiteral("algorithm"),
        QStringLiteral("downgradeProtected"),
        QStringLiteral("keyId"),
        QStringLiteral("manifestBoundAsAuthenticatedData"),
        QStringLiteral("mode"),
        QStringLiteral("nonceBase64"),
        QStringLiteral("recipientKeyFingerprint"),
    };
    if (const auto error = exactKeys(object, keys, QStringLiteral("encryption."));
        !error.ok()) {
        return error;
    }
    for (const QString& key : {QStringLiteral("algorithm"),
                               QStringLiteral("keyId"),
                               QStringLiteral("mode"),
                               QStringLiteral("nonceBase64"),
                               QStringLiteral("recipientKeyFingerprint")}) {
        if (const auto error = requireType(object, key, QJsonValue::String,
                                           QStringLiteral("encryption."));
            !error.ok()) {
            return error;
        }
    }
    for (const QString& key : {QStringLiteral("downgradeProtected"),
                               QStringLiteral("manifestBoundAsAuthenticatedData")}) {
        if (const auto error = requireType(object, key, QJsonValue::Bool,
                                           QStringLiteral("encryption."));
            !error.ok()) {
            return error;
        }
    }
    if (!parseEncryptionMode(object.value(QStringLiteral("mode")).toString(),
                             output.mode)) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidEncryption,
            QStringLiteral("encryption.mode"));
    }
    output.algorithm = object.value(QStringLiteral("algorithm")).toString();
    output.keyId = object.value(QStringLiteral("keyId")).toString();
    output.recipientKeyFingerprint =
        object.value(QStringLiteral("recipientKeyFingerprint")).toString();
    output.nonceBase64 = object.value(QStringLiteral("nonceBase64")).toString();
    output.manifestBoundAsAuthenticatedData =
        object.value(QStringLiteral("manifestBoundAsAuthenticatedData")).toBool();
    output.downgradeProtected =
        object.value(QStringLiteral("downgradeProtected")).toBool();
    return {};
}

SstvShareValidationError parsePrivacy(const QJsonObject& object,
                                      SstvSharePrivacyFlags& output)
{
    static const QSet<QString> keys {
        QStringLiteral("automaticIncomingDownloadAllowed"),
        QStringLiteral("automaticUploadAllowed"),
        QStringLiteral("callsignIncluded"),
        QStringLiteral("exifRetained"),
        QStringLiteral("explicitExpiry"),
        QStringLiteral("gridIncluded"),
        QStringLiteral("locationIncluded"),
        QStringLiteral("meteredNetworkAllowed"),
        QStringLiteral("publicShare"),
        QStringLiteral("recipientConfirmed"),
    };
    if (const auto error = exactKeys(object, keys, QStringLiteral("privacy."));
        !error.ok()) {
        return error;
    }
    for (const QString& key : keys) {
        if (const auto error = requireType(object, key, QJsonValue::Bool,
                                           QStringLiteral("privacy."));
            !error.ok()) {
            return error;
        }
    }
    output.automaticIncomingDownloadAllowed =
        object.value(QStringLiteral("automaticIncomingDownloadAllowed")).toBool();
    output.automaticUploadAllowed =
        object.value(QStringLiteral("automaticUploadAllowed")).toBool();
    output.callsignIncluded = object.value(QStringLiteral("callsignIncluded")).toBool();
    output.exifRetained = object.value(QStringLiteral("exifRetained")).toBool();
    output.explicitExpiry = object.value(QStringLiteral("explicitExpiry")).toBool();
    output.gridIncluded = object.value(QStringLiteral("gridIncluded")).toBool();
    output.locationIncluded = object.value(QStringLiteral("locationIncluded")).toBool();
    output.meteredNetworkAllowed =
        object.value(QStringLiteral("meteredNetworkAllowed")).toBool();
    output.publicShare = object.value(QStringLiteral("publicShare")).toBool();
    output.recipientConfirmed =
        object.value(QStringLiteral("recipientConfirmed")).toBool();
    return {};
}

SstvShareValidationError parseTransport(const QJsonObject& object,
                                        SstvShareTransportSecurity& output)
{
    static const QSet<QString> keys {
        QStringLiteral("certificateValidationRequired"),
        QStringLiteral("providerCanReadContent"),
        QStringLiteral("sameOriginRedirectsOnly"),
        QStringLiteral("tlsRequired"),
    };
    if (const auto error = exactKeys(object, keys, QStringLiteral("transport."));
        !error.ok()) {
        return error;
    }
    for (const QString& key : keys) {
        if (const auto error = requireType(object, key, QJsonValue::Bool,
                                           QStringLiteral("transport."));
            !error.ok()) {
            return error;
        }
    }
    output.certificateValidationRequired =
        object.value(QStringLiteral("certificateValidationRequired")).toBool();
    output.providerCanReadContent =
        object.value(QStringLiteral("providerCanReadContent")).toBool();
    output.sameOriginRedirectsOnly =
        object.value(QStringLiteral("sameOriginRedirectsOnly")).toBool();
    output.tlsRequired = object.value(QStringLiteral("tlsRequired")).toBool();
    return {};
}

SstvShareValidationError parseCallsign(const QJsonObject& object,
                                       SstvShareCallsignMetadata& output)
{
    static const QSet<QString> keys {
        QStringLiteral("grid"),
        QStringLiteral("remoteCallsign"),
        QStringLiteral("senderCallsign"),
    };
    if (const auto error = exactKeys(object, keys, QStringLiteral("callsign."));
        !error.ok()) {
        return error;
    }
    for (const QString& key : keys) {
        if (const auto error = requireType(object, key, QJsonValue::String,
                                           QStringLiteral("callsign."));
            !error.ok()) {
            return error;
        }
    }
    output.grid = object.value(QStringLiteral("grid")).toString();
    output.remoteCallsign = object.value(QStringLiteral("remoteCallsign")).toString();
    output.senderCallsign = object.value(QStringLiteral("senderCallsign")).toString();
    return {};
}

} // namespace

QString sstvShareMediaSourceName(SstvShareMediaSource source)
{
    switch (source) {
    case SstvShareMediaSource::AnalogReception: return QStringLiteral("analog-reception");
    case SstvShareMediaSource::AnalogTransmission: return QStringLiteral("analog-transmission");
    case SstvShareMediaSource::DigitalReception: return QStringLiteral("digital-reception");
    case SstvShareMediaSource::DigitalTransmission: return QStringLiteral("digital-transmission");
    }
    return {};
}

QString sstvShareContentCompletionName(SstvShareContentCompletion completion)
{
    switch (completion) {
    case SstvShareContentCompletion::Complete: return QStringLiteral("complete");
    case SstvShareContentCompletion::Partial: return QStringLiteral("partial");
    }
    return {};
}

QString sstvShareContentDispositionName(SstvShareContentDisposition disposition)
{
    switch (disposition) {
    case SstvShareContentDisposition::Attachment: return QStringLiteral("attachment");
    case SstvShareContentDisposition::InlinePreview: return QStringLiteral("inline-preview");
    }
    return {};
}

QString sstvShareEncryptionModeName(SstvShareEncryptionMode mode)
{
    switch (mode) {
    case SstvShareEncryptionMode::TransportTls: return QStringLiteral("transport-tls");
    case SstvShareEncryptionMode::EndToEnd: return QStringLiteral("end-to-end");
    }
    return {};
}

SstvShareValidationError SstvShareManifestV1::validate(bool forTransmission,
                                                       QDateTime nowUtc) const
{
    if (protocolVersion != kSstvShareManifestProtocolVersion) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::UnknownProtocolVersion,
            QStringLiteral("protocolVersion"));
    }
    if (transferId.isNull()
        || transferId.toString(QUuid::WithoutBraces)
               != transferId.toString(QUuid::WithoutBraces).toLower()) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidUuid, QStringLiteral("transferId"));
    }
    for (const auto& field : {std::pair {QStringLiteral("providerId"), providerId},
                              std::pair {QStringLiteral("senderId"), senderId},
                              std::pair {QStringLiteral("recipientId"), recipientId}}) {
        if (!isSafeShareIdentifier(field.second)) {
            return SstvShareValidationError::failure(
                containsNetworkUrl(field.second)
                    ? SstvShareValidationCode::UrlNotAllowed
                    : SstvShareValidationCode::InvalidIdentifier,
                field.first);
        }
    }
    if (!createdUtc.isValid() || createdUtc.offsetFromUtc() != 0
        || !expiresUtc.isValid() || expiresUtc.offsetFromUtc() != 0
        || !mediaUtc.isValid() || mediaUtc.offsetFromUtc() != 0) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidTimestamp, QStringLiteral("timestamps"));
    }
    const qint64 lifetime = createdUtc.secsTo(expiresUtc);
    if (!privacy.explicitExpiry || lifetime <= 0
        || lifetime > kMaximumShareLifetimeSeconds) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidExpiry, QStringLiteral("expiresUtc"));
    }
    if (mediaUtc > createdUtc.addSecs(300)) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidTimestamp, QStringLiteral("mediaUtc"));
    }
    if (forTransmission) {
        if (!nowUtc.isValid()) {
            nowUtc = QDateTime::currentDateTimeUtc();
        }
        if (expiresUtc <= nowUtc.toUTC()) {
            return SstvShareValidationError::failure(
                SstvShareValidationCode::Expired, QStringLiteral("expiresUtc"));
        }
        if (!privacy.recipientConfirmed) {
            return SstvShareValidationError::failure(
                SstvShareValidationCode::InvalidPrivacy,
                QStringLiteral("privacy.recipientConfirmed"));
        }
    }
    if (!isSafeShareFilename(originalFilename)
        || !isSafeShareFilename(safeDisplayFilename)) {
        return SstvShareValidationError::failure(
            containsNetworkUrl(originalFilename) || containsNetworkUrl(safeDisplayFilename)
                ? SstvShareValidationCode::UrlNotAllowed
                : SstvShareValidationCode::InvalidFilename,
            QStringLiteral("filename"));
    }
    if (!isKnownImageMimeType(mimeType)) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidMimeType, QStringLiteral("mimeType"));
    }
    if (byteSize == 0U || byteSize > kMaximumSharedImageBytes) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidByteSize, QStringLiteral("byteSize"));
    }
    if (!isLowercaseSha256(sha256)) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidHash, QStringLiteral("sha256"));
    }
    if (width == 0U || height == 0U
        || width > kMaximumSharedImageDimension
        || height > kMaximumSharedImageDimension
        || static_cast<quint64>(width) > kMaximumSharedImagePixels / height) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidDimensions, QStringLiteral("dimensions"));
    }
    if (chunkCount == 0U || chunkCount > kMaximumShareChunks
        || chunkCount > byteSize) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidChunkCount, QStringLiteral("chunkCount"));
    }
    if (sstvMode.isEmpty() || sstvMode.size() > 64
        || sanitizeShareDisplayText(sstvMode, 64) != sstvMode
        || containsNetworkUrl(sstvMode)) {
        return SstvShareValidationError::failure(
            containsNetworkUrl(sstvMode)
                ? SstvShareValidationCode::UrlNotAllowed
                : SstvShareValidationCode::InvalidText,
            QStringLiteral("sstvMode"));
    }
    if (message.size() > 1'000
        || sanitizeShareDisplayText(message, 1'000, true) != message
        || containsNetworkUrl(message)) {
        return SstvShareValidationError::failure(
            containsNetworkUrl(message)
                ? SstvShareValidationCode::UrlNotAllowed
                : SstvShareValidationCode::InvalidText,
            QStringLiteral("message"));
    }
    if (!isSafeCallsign(callsign.senderCallsign)
        || !isSafeCallsign(callsign.remoteCallsign)
        || !isSafeGrid(callsign.grid)
        || (!privacy.callsignIncluded
            && (!callsign.senderCallsign.isEmpty()
                || !callsign.remoteCallsign.isEmpty()))
        || (!privacy.gridIncluded && !callsign.grid.isEmpty())) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidPrivacy, QStringLiteral("callsign"));
    }
    if (!transport.tlsRequired || !transport.certificateValidationRequired
        || !transport.sameOriginRedirectsOnly) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidTransportSecurity,
            QStringLiteral("transport"));
    }
    if (!encryption.downgradeProtected) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidEncryption,
            QStringLiteral("encryption.downgradeProtected"));
    }
    if (encryption.mode == SstvShareEncryptionMode::TransportTls) {
        if (encryption.algorithm != QStringLiteral("none")
            || !encryption.keyId.isEmpty()
            || !encryption.recipientKeyFingerprint.isEmpty()
            || !encryption.nonceBase64.isEmpty()
            || encryption.manifestBoundAsAuthenticatedData
            || !transport.providerCanReadContent) {
            return SstvShareValidationError::failure(
                SstvShareValidationCode::InvalidEncryption,
                QStringLiteral("encryption"));
        }
    } else {
        const bool xchacha =
            encryption.algorithm == QStringLiteral("xchacha20-poly1305");
        const bool aesGcm = encryption.algorithm == QStringLiteral("aes-256-gcm");
        const QByteArray nonceText = encryption.nonceBase64.toLatin1();
        const QByteArray nonce = QByteArray::fromBase64(
            nonceText, QByteArray::AbortOnBase64DecodingErrors);
        const qsizetype requiredNonceBytes = xchacha ? 24 : (aesGcm ? 12 : 0);
        if ((!xchacha && !aesGcm) || !isSafeShareIdentifier(encryption.keyId)
            || !isLowercaseSha256(encryption.recipientKeyFingerprint)
            || nonce.size() != requiredNonceBytes
            || nonce.toBase64() != nonceText
            || !encryption.manifestBoundAsAuthenticatedData
            || transport.providerCanReadContent) {
            return SstvShareValidationError::failure(
                SstvShareValidationCode::InvalidEncryption,
                QStringLiteral("encryption"));
        }
    }
    return {};
}

QJsonObject SstvShareManifestV1::toJsonObject() const
{
    const QJsonObject callsignObject {
        {QStringLiteral("grid"), callsign.grid},
        {QStringLiteral("remoteCallsign"), callsign.remoteCallsign},
        {QStringLiteral("senderCallsign"), callsign.senderCallsign},
    };
    const QJsonObject encryptionObject {
        {QStringLiteral("algorithm"), encryption.algorithm},
        {QStringLiteral("downgradeProtected"), encryption.downgradeProtected},
        {QStringLiteral("keyId"), encryption.keyId},
        {QStringLiteral("manifestBoundAsAuthenticatedData"),
         encryption.manifestBoundAsAuthenticatedData},
        {QStringLiteral("mode"), sstvShareEncryptionModeName(encryption.mode)},
        {QStringLiteral("nonceBase64"), encryption.nonceBase64},
        {QStringLiteral("recipientKeyFingerprint"),
         encryption.recipientKeyFingerprint},
    };
    const QJsonObject privacyObject {
        {QStringLiteral("automaticIncomingDownloadAllowed"),
         privacy.automaticIncomingDownloadAllowed},
        {QStringLiteral("automaticUploadAllowed"), privacy.automaticUploadAllowed},
        {QStringLiteral("callsignIncluded"), privacy.callsignIncluded},
        {QStringLiteral("exifRetained"), privacy.exifRetained},
        {QStringLiteral("explicitExpiry"), privacy.explicitExpiry},
        {QStringLiteral("gridIncluded"), privacy.gridIncluded},
        {QStringLiteral("locationIncluded"), privacy.locationIncluded},
        {QStringLiteral("meteredNetworkAllowed"), privacy.meteredNetworkAllowed},
        {QStringLiteral("publicShare"), privacy.publicShare},
        {QStringLiteral("recipientConfirmed"), privacy.recipientConfirmed},
    };
    const QJsonObject transportObject {
        {QStringLiteral("certificateValidationRequired"),
         transport.certificateValidationRequired},
        {QStringLiteral("providerCanReadContent"), transport.providerCanReadContent},
        {QStringLiteral("sameOriginRedirectsOnly"), transport.sameOriginRedirectsOnly},
        {QStringLiteral("tlsRequired"), transport.tlsRequired},
    };
    return {
        {QStringLiteral("byteSize"), static_cast<qint64>(byteSize)},
        {QStringLiteral("callsign"), callsignObject},
        {QStringLiteral("chunkCount"), static_cast<qint64>(chunkCount)},
        {QStringLiteral("completion"), sstvShareContentCompletionName(completion)},
        {QStringLiteral("contentDisposition"),
         sstvShareContentDispositionName(disposition)},
        {QStringLiteral("createdUtc"), utcTimestamp(createdUtc)},
        {QStringLiteral("encryption"), encryptionObject},
        {QStringLiteral("expiresUtc"), utcTimestamp(expiresUtc)},
        {QStringLiteral("height"), static_cast<qint64>(height)},
        {QStringLiteral("mediaSource"), sstvShareMediaSourceName(source)},
        {QStringLiteral("mediaUtc"), utcTimestamp(mediaUtc)},
        {QStringLiteral("message"), message},
        {QStringLiteral("mimeType"), mimeType},
        {QStringLiteral("originalFilename"), originalFilename},
        {QStringLiteral("privacy"), privacyObject},
        {QStringLiteral("protocolVersion"), protocolVersion},
        {QStringLiteral("providerId"), providerId},
        {QStringLiteral("recipientId"), recipientId},
        {QStringLiteral("safeDisplayFilename"), safeDisplayFilename},
        {QStringLiteral("senderId"), senderId},
        {QStringLiteral("sha256"), sha256},
        {QStringLiteral("sstvMode"), sstvMode},
        {QStringLiteral("transferId"),
         transferId.toString(QUuid::WithoutBraces).toLower()},
        {QStringLiteral("transport"), transportObject},
        {QStringLiteral("width"), static_cast<qint64>(width)},
    };
}

QByteArray SstvShareManifestV1::toCanonicalJson(
    SstvShareValidationError* error) const
{
    if (error) {
        *error = validate(false);
        if (!error->ok()) {
            return {};
        }
    } else if (!validate(false).ok()) {
        return {};
    }
    return canonicalJson(toJsonObject(), error);
}

SstvShareManifestParseResult parseSstvShareManifestV1(
    const QByteArray& json,
    bool forTransmission,
    QDateTime nowUtc)
{
    const SstvShareBoundedJsonResult bounded = parseBoundedJsonObject(json);
    if (!bounded.ok()) {
        return {std::nullopt, bounded.error};
    }
    static const QSet<QString> keys {
        QStringLiteral("byteSize"), QStringLiteral("callsign"),
        QStringLiteral("chunkCount"), QStringLiteral("completion"),
        QStringLiteral("contentDisposition"), QStringLiteral("createdUtc"),
        QStringLiteral("encryption"), QStringLiteral("expiresUtc"),
        QStringLiteral("height"), QStringLiteral("mediaSource"),
        QStringLiteral("mediaUtc"), QStringLiteral("message"),
        QStringLiteral("mimeType"), QStringLiteral("originalFilename"),
        QStringLiteral("privacy"), QStringLiteral("protocolVersion"),
        QStringLiteral("providerId"), QStringLiteral("recipientId"),
        QStringLiteral("safeDisplayFilename"), QStringLiteral("senderId"),
        QStringLiteral("sha256"), QStringLiteral("sstvMode"),
        QStringLiteral("transferId"), QStringLiteral("transport"),
        QStringLiteral("width"),
    };
    if (const auto error = exactKeys(bounded.object, keys, {}); !error.ok()) {
        return {std::nullopt, error};
    }
    for (const QString& key : {QStringLiteral("byteSize"),
                               QStringLiteral("chunkCount"),
                               QStringLiteral("height"),
                               QStringLiteral("protocolVersion"),
                               QStringLiteral("width")}) {
        if (const auto error = requireType(bounded.object, key, QJsonValue::Double);
            !error.ok()) {
            return {std::nullopt, error};
        }
    }
    for (const QString& key : {QStringLiteral("completion"),
                               QStringLiteral("contentDisposition"),
                               QStringLiteral("createdUtc"),
                               QStringLiteral("expiresUtc"),
                               QStringLiteral("mediaSource"),
                               QStringLiteral("mediaUtc"),
                               QStringLiteral("message"),
                               QStringLiteral("mimeType"),
                               QStringLiteral("originalFilename"),
                               QStringLiteral("providerId"),
                               QStringLiteral("recipientId"),
                               QStringLiteral("safeDisplayFilename"),
                               QStringLiteral("senderId"),
                               QStringLiteral("sha256"),
                               QStringLiteral("sstvMode"),
                               QStringLiteral("transferId")}) {
        if (const auto error = requireType(bounded.object, key, QJsonValue::String);
            !error.ok()) {
            return {std::nullopt, error};
        }
    }
    for (const QString& key : {QStringLiteral("callsign"),
                               QStringLiteral("encryption"),
                               QStringLiteral("privacy"),
                               QStringLiteral("transport")}) {
        if (const auto error = requireType(bounded.object, key, QJsonValue::Object);
            !error.ok()) {
            return {std::nullopt, error};
        }
    }

    const auto protocol = unsignedInteger(
        bounded.object.value(QStringLiteral("protocolVersion")));
    if (!protocol || *protocol != kSstvShareManifestProtocolVersion) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::UnknownProtocolVersion,
                                  QStringLiteral("protocolVersion"))};
    }

    SstvShareManifestV1 manifest;
    manifest.protocolVersion = static_cast<int>(*protocol);
    const QString transferText = bounded.object.value(QStringLiteral("transferId")).toString();
    manifest.transferId = QUuid::fromString(transferText);
    if (manifest.transferId.isNull()
        || manifest.transferId.toString(QUuid::WithoutBraces).toLower()
               != transferText) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::InvalidUuid,
                                  QStringLiteral("transferId"))};
    }
    manifest.providerId = bounded.object.value(QStringLiteral("providerId")).toString();
    manifest.senderId = bounded.object.value(QStringLiteral("senderId")).toString();
    manifest.recipientId = bounded.object.value(QStringLiteral("recipientId")).toString();
    const auto created = parseUtcTimestamp(
        bounded.object.value(QStringLiteral("createdUtc")).toString());
    const auto expires = parseUtcTimestamp(
        bounded.object.value(QStringLiteral("expiresUtc")).toString());
    const auto media = parseUtcTimestamp(
        bounded.object.value(QStringLiteral("mediaUtc")).toString());
    if (!created || !expires || !media) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::InvalidTimestamp,
                                  QStringLiteral("timestamps"))};
    }
    manifest.createdUtc = *created;
    manifest.expiresUtc = *expires;
    manifest.mediaUtc = *media;
    manifest.originalFilename =
        bounded.object.value(QStringLiteral("originalFilename")).toString();
    manifest.safeDisplayFilename =
        bounded.object.value(QStringLiteral("safeDisplayFilename")).toString();
    manifest.mimeType = bounded.object.value(QStringLiteral("mimeType")).toString();
    manifest.sha256 = bounded.object.value(QStringLiteral("sha256")).toString();
    manifest.sstvMode = bounded.object.value(QStringLiteral("sstvMode")).toString();
    manifest.message = bounded.object.value(QStringLiteral("message")).toString();

    const auto byteSize = unsignedInteger(bounded.object.value(QStringLiteral("byteSize")));
    const auto chunkCount = unsignedInteger(bounded.object.value(QStringLiteral("chunkCount")));
    const auto width = unsignedInteger(bounded.object.value(QStringLiteral("width")));
    const auto height = unsignedInteger(bounded.object.value(QStringLiteral("height")));
    if (!byteSize || !chunkCount || !width || !height
        || *chunkCount > std::numeric_limits<quint32>::max()
        || *width > std::numeric_limits<quint32>::max()
        || *height > std::numeric_limits<quint32>::max()) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::WrongType,
                                  QStringLiteral("numeric-fields"))};
    }
    manifest.byteSize = *byteSize;
    manifest.chunkCount = static_cast<quint32>(*chunkCount);
    manifest.width = static_cast<quint32>(*width);
    manifest.height = static_cast<quint32>(*height);

    if (!parseMediaSource(bounded.object.value(QStringLiteral("mediaSource")).toString(),
                          manifest.source)
        || !parseCompletion(bounded.object.value(QStringLiteral("completion")).toString(),
                            manifest.completion)
        || !parseDisposition(
            bounded.object.value(QStringLiteral("contentDisposition")).toString(),
            manifest.disposition)) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::InvalidText,
                                  QStringLiteral("enum"))};
    }
    if (const auto error = parseCallsign(
            bounded.object.value(QStringLiteral("callsign")).toObject(),
            manifest.callsign);
        !error.ok()) {
        return {std::nullopt, error};
    }
    if (const auto error = parseEncryption(
            bounded.object.value(QStringLiteral("encryption")).toObject(),
            manifest.encryption);
        !error.ok()) {
        return {std::nullopt, error};
    }
    if (const auto error = parsePrivacy(
            bounded.object.value(QStringLiteral("privacy")).toObject(),
            manifest.privacy);
        !error.ok()) {
        return {std::nullopt, error};
    }
    if (const auto error = parseTransport(
            bounded.object.value(QStringLiteral("transport")).toObject(),
            manifest.transport);
        !error.ok()) {
        return {std::nullopt, error};
    }
    const SstvShareValidationError validation =
        manifest.validate(forTransmission, nowUtc);
    if (!validation.ok()) {
        return {std::nullopt, validation};
    }
    return {manifest, {}};
}

} // namespace decodium::sstv::sharing
