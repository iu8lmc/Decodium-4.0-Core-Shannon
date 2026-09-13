// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvImageStorage.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace decodium::sstv {
namespace {

constexpr qsizetype kHashBytes = 32;
constexpr qsizetype kHashHexCharacters = kHashBytes * 2;
constexpr qsizetype kMaximumIdCharacters = 36;
constexpr qsizetype kMaximumModeCharacters = 64;
constexpr qsizetype kMaximumCallsignCharacters = 64;
constexpr qsizetype kMaximumGridCharacters = 16;
constexpr qsizetype kMaximumSourceCharacters = 128;
constexpr qsizetype kMaximumFskIdCharacters = 128;
constexpr qsizetype kMaximumNoteCharacters = 4096;
constexpr qsizetype kMaximumProviderCharacters = 128;
constexpr qsizetype kMaximumRemoteObjectCharacters = 512;
constexpr qsizetype kMaximumPathCharacters = 4096;
constexpr qsizetype kMaximumQualityMetrics = 64;
constexpr qsizetype kMaximumQualityMetricNameCharacters = 64;
constexpr qsizetype kMaximumQualityJsonBytes = 8192;
constexpr qint64 kMaximumFrequencyHz = 10'000'000'000'000LL;
constexpr qint64 kMaximumAudioFrequencyHz = 10'000'000LL;
constexpr int kMaximumSourceSampleRateHz = 10'000'000;
constexpr double kMaximumSlantCorrectionPpm = 10'000.0;

bool fail(QString* error, const QString& detail)
{
    if (error) {
        *error = detail;
    }
    return false;
}

bool validText(const QString& value,
               qsizetype maximumCharacters,
               bool allowEmpty,
               bool allowLineBreaks,
               const QString& field,
               QString* error)
{
    if (!allowEmpty && value.trimmed().isEmpty()) {
        return fail(error, field + QStringLiteral(" must not be empty"));
    }
    if (value.size() > maximumCharacters) {
        return fail(error, field + QStringLiteral(" exceeds its length limit"));
    }
    for (QChar character : value) {
        const ushort code = character.unicode();
        if (code == 0U || code == 0x7fU
            || (code < 0x20U
                && !(allowLineBreaks
                     && (code == '\n' || code == '\r' || code == '\t')))) {
            return fail(error, field + QStringLiteral(" contains control characters"));
        }
    }
    return true;
}

bool canonicalUuid(const QString& value)
{
    if (value.size() != kMaximumIdCharacters
        || value != value.toLower()) {
        return false;
    }
    const QUuid uuid(value);
    return !uuid.isNull()
        && uuid.toString(QUuid::WithoutBraces) == value;
}

bool utcDateTime(const QDateTime& value)
{
    return value.isValid() && value.timeSpec() == Qt::UTC;
}

bool checkedPixels(int width, int height, qint64* pixels)
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (static_cast<qint64>(width)
        > std::numeric_limits<qint64>::max() / static_cast<qint64>(height)) {
        return false;
    }
    if (pixels) {
        *pixels = static_cast<qint64>(width) * static_cast<qint64>(height);
    }
    return true;
}

QString absoluteCleanPath(const QString& path)
{
    if (path.isEmpty()) {
        return {};
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool readRequiredString(const QJsonObject& object,
                        const QString& key,
                        QString* value,
                        QString* error)
{
    const QJsonValue json = object.value(key);
    if (!json.isString()) {
        return fail(error, key + QStringLiteral(" must be a string"));
    }
    *value = json.toString();
    return true;
}

bool readRequiredInteger(const QJsonObject& object,
                         const QString& key,
                         qint64 minimum,
                         qint64 maximum,
                         qint64* value,
                         QString* error)
{
    const QJsonValue json = object.value(key);
    if (!json.isDouble()) {
        return fail(error, key + QStringLiteral(" must be an integer"));
    }
    const double numeric = json.toDouble();
    if (!std::isfinite(numeric) || std::trunc(numeric) != numeric
        || numeric < static_cast<double>(minimum)
        || numeric > static_cast<double>(maximum)) {
        return fail(error, key + QStringLiteral(" is outside its valid range"));
    }
    *value = static_cast<qint64>(numeric);
    return true;
}

bool readRequiredNumber(const QJsonObject& object,
                        const QString& key,
                        double minimum,
                        double maximum,
                        double* value,
                        QString* error)
{
    const QJsonValue json = object.value(key);
    if (!json.isDouble()) {
        return fail(error, key + QStringLiteral(" must be a number"));
    }
    const double numeric = json.toDouble();
    if (!std::isfinite(numeric) || numeric < minimum || numeric > maximum) {
        return fail(error, key + QStringLiteral(" is outside its valid range"));
    }
    *value = numeric;
    return true;
}

bool validQualityMetrics(const QJsonObject& metrics, QString* error)
{
    if (metrics.size() > kMaximumQualityMetrics
        || QJsonDocument(metrics).toJson(QJsonDocument::Compact).size()
            > kMaximumQualityJsonBytes) {
        return fail(error, QStringLiteral("quality metrics exceed storage limits"));
    }
    for (auto iterator = metrics.constBegin(); iterator != metrics.constEnd();
         ++iterator) {
        if (!validText(iterator.key(), kMaximumQualityMetricNameCharacters,
                       false, false, QStringLiteral("quality metric name"),
                       error)
            || !iterator.value().isDouble()
            || !std::isfinite(iterator.value().toDouble())) {
            return fail(error,
                        QStringLiteral("quality metrics must be finite numeric values"));
        }
    }
    return true;
}

QString thumbnailPathForImage(const QString& imagePath)
{
    if (imagePath.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
        return imagePath.left(imagePath.size() - 4)
            + QStringLiteral(".thumb.png");
    }
    return imagePath + QStringLiteral(".thumb.png");
}

bool canonicalAbsolutePath(const QString& path, bool allowEmpty)
{
    return (allowEmpty && path.isEmpty())
        || (!path.isEmpty() && path.size() <= kMaximumPathCharacters
            && QFileInfo(path).isAbsolute() && QDir::cleanPath(path) == path);
}

QString canonicalExistingPath(const QString& path)
{
    const QFileInfo info(path);
    return QDir::cleanPath(info.canonicalFilePath());
}

bool relativePathEscapes(const QString& relative)
{
    return relative.isEmpty() || relative == QLatin1String(".")
        || QDir::isAbsolutePath(relative)
        || relative == QLatin1String("..")
        || relative.startsWith(QStringLiteral("../"))
        || relative.startsWith(QStringLiteral("..\\"));
}

QString errorWithPath(const QString& prefix, const QString& path)
{
    return prefix + QStringLiteral(": ") + QDir::toNativeSeparators(path);
}

bool writeBytesAtomically(const QString& path,
                          const QByteArray& bytes,
                          QString* error)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(error, errorWithPath(file.errorString(), path));
    }
    if (file.write(bytes) != bytes.size()) {
        const QString detail = file.errorString();
        file.cancelWriting();
        return fail(error, errorWithPath(detail, path));
    }
    if (!file.commit()) {
        return fail(error, errorWithPath(file.errorString(), path));
    }
    return true;
}

SstvImageSaveResult saveFailure(SstvStoreError code, const QString& detail)
{
    SstvImageSaveResult result;
    result.code = code;
    result.error = detail;
    return result;
}

} // namespace

bool validateSstvQsoId(const QString& qsoId,
                       bool allowEmpty,
                       QString* error)
{
    if (qsoId.isEmpty()) {
        return allowEmpty
            || fail(error, QStringLiteral("QSO ID must not be empty"));
    }
    if (qsoId.size() > kMaximumSstvQsoIdCharacters) {
        return fail(error, QStringLiteral("QSO ID exceeds its length limit"));
    }
    if (qsoId != qsoId.trimmed()) {
        return fail(error, QStringLiteral(
            "QSO ID must not contain leading or trailing whitespace"));
    }
    if (qsoId == QStringLiteral(".") || qsoId == QStringLiteral("..")) {
        return fail(error, QStringLiteral("QSO ID must not be a path"));
    }
    for (const QChar character : qsoId) {
        const auto category = character.category();
        if (character.isNull()
            || category == QChar::Other_Control
            || category == QChar::Separator_Line
            || category == QChar::Separator_Paragraph) {
            return fail(error, QStringLiteral(
                "QSO ID contains a control character"));
        }
        if (character == QLatin1Char('/')
            || character == QLatin1Char('\\')) {
            return fail(error, QStringLiteral(
                "QSO ID must be opaque and must not contain a path"));
        }
    }
    return true;
}

QString sstvImageCategoryName(SstvImageCategory category)
{
    switch (category) {
    case SstvImageCategory::Received:
        return QStringLiteral("received");
    case SstvImageCategory::Transmitted:
        return QStringLiteral("transmitted");
    case SstvImageCategory::Imported:
        return QStringLiteral("imported");
    case SstvImageCategory::Draft:
        return QStringLiteral("drafts");
    }
    return {};
}

bool sstvImageCategoryFromName(const QString& name,
                               SstvImageCategory* category) noexcept
{
    if (!category) {
        return false;
    }
    const QString normalized = name.trimmed().toLower();
    if (normalized == QLatin1String("received")) {
        *category = SstvImageCategory::Received;
        return true;
    }
    if (normalized == QLatin1String("transmitted")) {
        *category = SstvImageCategory::Transmitted;
        return true;
    }
    if (normalized == QLatin1String("imported")) {
        *category = SstvImageCategory::Imported;
        return true;
    }
    if (normalized == QLatin1String("drafts")) {
        *category = SstvImageCategory::Draft;
        return true;
    }
    return false;
}

bool isValidSstvImageCategory(SstvImageCategory category) noexcept
{
    return !sstvImageCategoryName(category).isEmpty();
}

QString sstvUploadStateName(SstvUploadState state)
{
    switch (state) {
    case SstvUploadState::NotRequested:
        return QStringLiteral("not-requested");
    case SstvUploadState::Pending:
        return QStringLiteral("pending");
    case SstvUploadState::Uploading:
        return QStringLiteral("uploading");
    case SstvUploadState::Uploaded:
        return QStringLiteral("uploaded");
    case SstvUploadState::Failed:
        return QStringLiteral("failed");
    }
    return {};
}

bool sstvUploadStateFromName(const QString& name,
                             SstvUploadState* state) noexcept
{
    if (!state) {
        return false;
    }
    const QString normalized = name.trimmed().toLower();
    for (int value = static_cast<int>(SstvUploadState::NotRequested);
         value <= static_cast<int>(SstvUploadState::Failed); ++value) {
        const auto candidate = static_cast<SstvUploadState>(value);
        if (sstvUploadStateName(candidate) == normalized) {
            *state = candidate;
            return true;
        }
    }
    return false;
}

bool isValidSstvUploadState(SstvUploadState state) noexcept
{
    return !sstvUploadStateName(state).isEmpty();
}

bool SstvStorageLimits::validate(QString* error) const
{
    if (maximumWidth <= 0 || maximumHeight <= 0
        || maximumPixels <= 0 || maximumDecodedBytes <= 0
        || maximumPngBytes <= 0 || maximumMetadataBytes <= 0
        || maximumFileNameUtf8Bytes < 32
        || maximumFileNameUtf8Bytes > 220
        || maximumTags <= 0 || maximumTags > 256
        || maximumTagCharacters <= 0 || maximumTagCharacters > 64) {
        return fail(error, QStringLiteral("invalid SSTV storage limits"));
    }
    qint64 maximumDimensionPixels = 0;
    if (!checkedPixels(maximumWidth, maximumHeight,
                       &maximumDimensionPixels)
        || maximumPixels > maximumDimensionPixels) {
        return fail(error, QStringLiteral("pixel limits exceed dimension limits"));
    }
    return true;
}

bool SstvImageRecord::validate(const SstvStorageLimits& limits,
                               QString* error) const
{
    if (!limits.validate(error)) {
        return false;
    }
    if (!canonicalUuid(id)) {
        return fail(error, QStringLiteral("id must be a canonical lowercase UUID"));
    }
    if (!isValidSstvImageCategory(category)) {
        return fail(error, QStringLiteral("invalid SSTV image category"));
    }
    if (!utcDateTime(capturedAtUtc) || !utcDateTime(eventAtUtc)
        || !utcDateTime(createdAtUtc) || !utcDateTime(updatedAtUtc)) {
        return fail(error, QStringLiteral("record timestamps must be valid UTC values"));
    }
    if (updatedAtUtc < createdAtUtc) {
        return fail(error, QStringLiteral("updated timestamp precedes creation"));
    }
    if (expiresAtUtc.isValid() && !utcDateTime(expiresAtUtc)) {
        return fail(error, QStringLiteral("expiry timestamp must be UTC"));
    }
    if (!validText(mode, kMaximumModeCharacters, false, false,
                   QStringLiteral("mode"), error)
        || !validText(fskId, kMaximumFskIdCharacters, true, false,
                      QStringLiteral("fskId"), error)
        || !validText(remoteCallsign, kMaximumCallsignCharacters, true, false,
                      QStringLiteral("remoteCallsign"), error)
        || !validText(remoteGrid, kMaximumGridCharacters, true, false,
                      QStringLiteral("remoteGrid"), error)
        || !validText(localCallsign, kMaximumCallsignCharacters, true, false,
                      QStringLiteral("localCallsign"), error)
        || !validText(localGrid, kMaximumGridCharacters, true, false,
                      QStringLiteral("localGrid"), error)
        || !validText(source, kMaximumSourceCharacters, false, false,
                      QStringLiteral("source"), error)
        || !validateSstvQsoId(relatedQsoId, true, error)
        || !validText(remoteProvider, kMaximumProviderCharacters, true, false,
                      QStringLiteral("remoteProvider"), error)
        || !validText(remoteObjectId, kMaximumRemoteObjectCharacters, true,
                      false, QStringLiteral("remoteObjectId"), error)
        || !validText(note, kMaximumNoteCharacters, true, true,
                      QStringLiteral("note"), error)) {
        return false;
    }
    if (visCode < -1 || visCode > 255) {
        return fail(error, QStringLiteral("VIS code must be -1 or 0..255"));
    }
    if (frequencyHz < 0 || frequencyHz > kMaximumFrequencyHz) {
        return fail(error, QStringLiteral("frequency is outside the supported range"));
    }
    if (audioFrequencyHz < -kMaximumAudioFrequencyHz
        || audioFrequencyHz > kMaximumAudioFrequencyHz) {
        return fail(error, QStringLiteral(
            "audio frequency or offset is outside the supported range"));
    }
    if (sourceSampleRateHz < 0
        || sourceSampleRateHz > kMaximumSourceSampleRateHz) {
        return fail(error, QStringLiteral("source sample rate is outside its valid range"));
    }
    if (completionPercent < 0 || completionPercent > 100
        || complete != (completionPercent == 100)) {
        return fail(error, QStringLiteral(
            "completion percentage and partial/complete state disagree"));
    }
    if (!std::isfinite(slantCorrectionPpm)
        || std::abs(slantCorrectionPpm) > kMaximumSlantCorrectionPpm) {
        return fail(error, QStringLiteral("slant correction is outside its valid range"));
    }
    if (!validQualityMetrics(qualityMetrics, error)) {
        return false;
    }
    if (!isValidSstvUploadState(uploadState)) {
        return fail(error, QStringLiteral("invalid SSTV upload state"));
    }
    if (tags.size() > limits.maximumTags) {
        return fail(error, QStringLiteral("record contains too many tags"));
    }
    QSet<QString> foldedTags;
    for (const QString& tag : tags) {
        const QString canonical = tag.trimmed().normalized(
            QString::NormalizationForm_C);
        if (tag != canonical) {
            return fail(error, QStringLiteral("tags must be trimmed NFC text"));
        }
        if (!validText(tag, limits.maximumTagCharacters, false, false,
                       QStringLiteral("tag"), error)) {
            return false;
        }
        const QString folded = canonical.toCaseFolded();
        if (folded.isEmpty() || folded.size() > limits.maximumTagCharacters) {
            return fail(error, QStringLiteral("case-folded tag exceeds its length limit"));
        }
        if (foldedTags.contains(folded)) {
            return fail(error, QStringLiteral("record contains duplicate tags"));
        }
        foldedTags.insert(folded);
    }
    qint64 pixels = 0;
    if (!checkedPixels(width, height, &pixels)
        || width > limits.maximumWidth || height > limits.maximumHeight
        || pixels > limits.maximumPixels) {
        return fail(error, QStringLiteral("image dimensions exceed storage limits"));
    }
    qint64 originalPixels = 0;
    if (!checkedPixels(originalWidth, originalHeight, &originalPixels)
        || originalWidth > limits.maximumWidth
        || originalHeight > limits.maximumHeight
        || originalPixels > limits.maximumPixels) {
        return fail(error, QStringLiteral(
            "original image dimensions exceed storage limits"));
    }
    if (fileSizeBytes <= 0 || fileSizeBytes > limits.maximumPngBytes) {
        return fail(error, QStringLiteral("PNG byte size exceeds storage limits"));
    }
    if (sha256.size() != kHashBytes) {
        return fail(error, QStringLiteral("SHA-256 digest must contain exactly 32 bytes"));
    }
    if (!canonicalAbsolutePath(imagePath, false)
        || !canonicalAbsolutePath(thumbnailPath, false)
        || !canonicalAbsolutePath(metadataPath, false)
        || !canonicalAbsolutePath(rawAudioPath, true)
        || imagePath == metadataPath || imagePath == thumbnailPath
        || metadataPath == thumbnailPath
        || (!rawAudioPath.isEmpty()
            && (rawAudioPath == imagePath || rawAudioPath == thumbnailPath
                || rawAudioPath == metadataPath))
        || !imagePath.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)
        || !thumbnailPath.endsWith(QStringLiteral(".png"),
                                   Qt::CaseInsensitive)
        || !metadataPath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        return fail(error, QStringLiteral(
            "record paths are not distinct canonical absolute paths"));
    }
    if (mimeType != QLatin1String("image/png")) {
        return fail(error, QStringLiteral("stored SSTV image MIME type must be image/png"));
    }
    return true;
}

QJsonObject SstvImageRecord::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("schemaVersion"), kSidecarSchemaVersion);
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("category"), sstvImageCategoryName(category));
    object.insert(QStringLiteral("capturedAtUtc"),
                  capturedAtUtc.toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("eventAtUtc"),
                  eventAtUtc.toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("createdAtUtc"),
                  createdAtUtc.toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("updatedAtUtc"),
                  updatedAtUtc.toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("mode"), mode);
    object.insert(QStringLiteral("visCode"), visCode);
    object.insert(QStringLiteral("visValid"), visValid);
    object.insert(QStringLiteral("fskId"), fskId);
    object.insert(QStringLiteral("remoteCallsign"), remoteCallsign);
    object.insert(QStringLiteral("remoteGrid"), remoteGrid);
    object.insert(QStringLiteral("localCallsign"), localCallsign);
    object.insert(QStringLiteral("localGrid"), localGrid);
    object.insert(QStringLiteral("source"), source);
    object.insert(QStringLiteral("frequencyHz"), frequencyHz);
    object.insert(QStringLiteral("audioFrequencyHz"), audioFrequencyHz);
    object.insert(QStringLiteral("sourceSampleRateHz"), sourceSampleRateHz);
    object.insert(QStringLiteral("digital"), digital);
    object.insert(QStringLiteral("completionPercent"), completionPercent);
    object.insert(QStringLiteral("complete"), complete);
    object.insert(QStringLiteral("qualityMetrics"), qualityMetrics);
    object.insert(QStringLiteral("slantCorrectionPpm"), slantCorrectionPpm);
    object.insert(QStringLiteral("rawAudioPath"), rawAudioPath);
    object.insert(QStringLiteral("relatedQsoId"), relatedQsoId);
    object.insert(QStringLiteral("remote"), remote);
    object.insert(QStringLiteral("uploadState"),
                  sstvUploadStateName(uploadState));
    object.insert(QStringLiteral("remoteProvider"), remoteProvider);
    object.insert(QStringLiteral("remoteObjectId"), remoteObjectId);
    object.insert(QStringLiteral("expiresAtUtc"), expiresAtUtc.isValid()
                      ? QJsonValue(expiresAtUtc.toString(Qt::ISODateWithMs))
                      : QJsonValue(QJsonValue::Null));
    object.insert(QStringLiteral("privacyFlags"),
                  static_cast<qint64>(privacyFlags));
    object.insert(QStringLiteral("favorite"), favorite);
    QJsonArray tagArray;
    for (const QString& tag : tags) {
        tagArray.append(tag);
    }
    object.insert(QStringLiteral("tags"), tagArray);
    object.insert(QStringLiteral("imagePath"), imagePath);
    object.insert(QStringLiteral("thumbnailPath"), thumbnailPath);
    object.insert(QStringLiteral("metadataPath"), metadataPath);
    object.insert(QStringLiteral("sha256"),
                  QString::fromLatin1(sha256.toHex()));
    object.insert(QStringLiteral("fileSizeBytes"), fileSizeBytes);
    object.insert(QStringLiteral("mimeType"), mimeType);
    object.insert(QStringLiteral("width"), width);
    object.insert(QStringLiteral("height"), height);
    object.insert(QStringLiteral("originalWidth"), originalWidth);
    object.insert(QStringLiteral("originalHeight"), originalHeight);
    object.insert(QStringLiteral("note"), note);
    return object;
}

bool SstvImageRecord::fromJson(const QJsonObject& object,
                               SstvImageRecord* record,
                               QString* error,
                               const SstvStorageLimits& limits)
{
    if (!record) {
        return fail(error, QStringLiteral("record output is null"));
    }
    qint64 schemaVersion = 0;
    if (!readRequiredInteger(object, QStringLiteral("schemaVersion"), 1,
                             kSidecarSchemaVersion, &schemaVersion, error)) {
        return fail(error, QStringLiteral("unsupported sidecar schema version"));
    }

    SstvImageRecord parsed;
    QString categoryName;
    QString captured;
    QString created;
    QString updated;
    QString hashHex;
    qint64 vis = 0;
    qint64 frequency = 0;
    qint64 fileSize = 0;
    qint64 parsedWidth = 0;
    qint64 parsedHeight = 0;
    if (!readRequiredString(object, QStringLiteral("id"), &parsed.id, error)
        || !readRequiredString(object, QStringLiteral("category"),
                               &categoryName, error)
        || !readRequiredString(object, QStringLiteral("capturedAtUtc"),
                               &captured, error)
        || !readRequiredString(object, QStringLiteral("createdAtUtc"),
                               &created, error)
        || !readRequiredString(object, QStringLiteral("updatedAtUtc"),
                               &updated, error)
        || !readRequiredString(object, QStringLiteral("mode"),
                               &parsed.mode, error)
        || !readRequiredInteger(object, QStringLiteral("visCode"), -1, 255,
                                &vis, error)
        || !readRequiredString(object, QStringLiteral("remoteCallsign"),
                               &parsed.remoteCallsign, error)
        || !readRequiredString(object, QStringLiteral("localCallsign"),
                               &parsed.localCallsign, error)
        || !readRequiredString(object, QStringLiteral("source"),
                               &parsed.source, error)
        || !readRequiredInteger(object, QStringLiteral("frequencyHz"), 0,
                                kMaximumFrequencyHz, &frequency, error)
        || !object.value(QStringLiteral("complete")).isBool()
        || !readRequiredString(object, QStringLiteral("imagePath"),
                               &parsed.imagePath, error)
        || !readRequiredString(object, QStringLiteral("metadataPath"),
                               &parsed.metadataPath, error)
        || !readRequiredString(object, QStringLiteral("sha256"),
                               &hashHex, error)
        || !readRequiredInteger(object, QStringLiteral("fileSizeBytes"), 1,
                                limits.maximumPngBytes, &fileSize, error)
        || !readRequiredInteger(object, QStringLiteral("width"), 1,
                                limits.maximumWidth, &parsedWidth, error)
        || !readRequiredInteger(object, QStringLiteral("height"), 1,
                                limits.maximumHeight, &parsedHeight, error)
        || !readRequiredString(object, QStringLiteral("note"),
                               &parsed.note, error)) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("complete must be a boolean");
        }
        return false;
    }
    if (!sstvImageCategoryFromName(categoryName, &parsed.category)) {
        return fail(error, QStringLiteral("invalid sidecar category"));
    }
    parsed.capturedAtUtc = QDateTime::fromString(captured, Qt::ISODateWithMs);
    parsed.createdAtUtc = QDateTime::fromString(created, Qt::ISODateWithMs);
    parsed.updatedAtUtc = QDateTime::fromString(updated, Qt::ISODateWithMs);
    if (parsed.capturedAtUtc.isValid()) {
        parsed.capturedAtUtc = parsed.capturedAtUtc.toUTC();
    }
    if (parsed.createdAtUtc.isValid()) {
        parsed.createdAtUtc = parsed.createdAtUtc.toUTC();
    }
    if (parsed.updatedAtUtc.isValid()) {
        parsed.updatedAtUtc = parsed.updatedAtUtc.toUTC();
    }
    if (hashHex.size() != kHashHexCharacters
        || !QRegularExpression(QStringLiteral("^[0-9A-Fa-f]{64}$"))
                .match(hashHex).hasMatch()) {
        return fail(error, QStringLiteral("invalid sidecar SHA-256 value"));
    }
    parsed.sha256 = QByteArray::fromHex(hashHex.toLatin1());
    parsed.visCode = static_cast<int>(vis);
    parsed.frequencyHz = frequency;
    parsed.complete = object.value(QStringLiteral("complete")).toBool();
    parsed.fileSizeBytes = fileSize;
    parsed.width = static_cast<int>(parsedWidth);
    parsed.height = static_cast<int>(parsedHeight);

    // Versions 1 and 2 remain readable.  Their missing canonical metadata is
    // deterministically derived from the fields they did persist, matching the
    // SQLite v4 backfill.
    parsed.eventAtUtc = parsed.capturedAtUtc;
    parsed.visValid = parsed.visCode >= 0;
    parsed.completionPercent = parsed.complete ? 100 : 0;
    parsed.thumbnailPath = thumbnailPathForImage(parsed.imagePath);
    parsed.originalWidth = parsed.width;
    parsed.originalHeight = parsed.height;

    if (schemaVersion >= 2) {
        const QJsonValue remoteValue = object.value(QStringLiteral("remote"));
        QString uploadStateName;
        const QJsonValue tagsValue = object.value(QStringLiteral("tags"));
        if (!remoteValue.isBool()
            || !readRequiredString(object, QStringLiteral("uploadState"),
                                   &uploadStateName, error)
            || !tagsValue.isArray()
            || !sstvUploadStateFromName(uploadStateName,
                                        &parsed.uploadState)) {
            return fail(error, QStringLiteral(
                "invalid remote, upload state or tags sidecar fields"));
        }
        parsed.remote = remoteValue.toBool();
        const QJsonArray tagArray = tagsValue.toArray();
        if (tagArray.size() > limits.maximumTags) {
            return fail(error, QStringLiteral("sidecar contains too many tags"));
        }
        for (const QJsonValue& tag : tagArray) {
            if (!tag.isString()) {
                return fail(error, QStringLiteral("sidecar tags must be strings"));
            }
            parsed.tags.append(tag.toString());
        }
    }

    if (schemaVersion >= 3) {
        QString event;
        QString expiry;
        qint64 audioFrequency = 0;
        qint64 sampleRate = 0;
        qint64 completion = 0;
        qint64 parsedOriginalWidth = 0;
        qint64 parsedOriginalHeight = 0;
        qint64 parsedPrivacyFlags = 0;
        double slantCorrection = 0.0;
        const QJsonValue expiryValue = object.value(
            QStringLiteral("expiresAtUtc"));
        const QJsonValue qualityValue = object.value(
            QStringLiteral("qualityMetrics"));
        if (!readRequiredString(object, QStringLiteral("eventAtUtc"),
                                &event, error)
            || !object.value(QStringLiteral("visValid")).isBool()
            || !readRequiredString(object, QStringLiteral("fskId"),
                                   &parsed.fskId, error)
            || !readRequiredString(object, QStringLiteral("remoteGrid"),
                                   &parsed.remoteGrid, error)
            || !readRequiredString(object, QStringLiteral("localGrid"),
                                   &parsed.localGrid, error)
            || !readRequiredInteger(object, QStringLiteral("audioFrequencyHz"),
                                    -kMaximumAudioFrequencyHz,
                                    kMaximumAudioFrequencyHz,
                                    &audioFrequency, error)
            || !readRequiredInteger(object, QStringLiteral("sourceSampleRateHz"),
                                    0, kMaximumSourceSampleRateHz,
                                    &sampleRate, error)
            || !object.value(QStringLiteral("digital")).isBool()
            || !readRequiredInteger(object, QStringLiteral("completionPercent"),
                                    0, 100, &completion, error)
            || !qualityValue.isObject()
            || !readRequiredNumber(object, QStringLiteral("slantCorrectionPpm"),
                                   -kMaximumSlantCorrectionPpm,
                                   kMaximumSlantCorrectionPpm,
                                   &slantCorrection, error)
            || !readRequiredString(object, QStringLiteral("rawAudioPath"),
                                   &parsed.rawAudioPath, error)
            || !readRequiredString(object, QStringLiteral("relatedQsoId"),
                                   &parsed.relatedQsoId, error)
            || !readRequiredString(object, QStringLiteral("remoteProvider"),
                                   &parsed.remoteProvider, error)
            || !readRequiredString(object, QStringLiteral("remoteObjectId"),
                                   &parsed.remoteObjectId, error)
            || (!expiryValue.isNull() && !expiryValue.isString())
            || !readRequiredInteger(object, QStringLiteral("privacyFlags"), 0,
                                    std::numeric_limits<quint32>::max(),
                                    &parsedPrivacyFlags, error)
            || !readRequiredString(object, QStringLiteral("thumbnailPath"),
                                   &parsed.thumbnailPath, error)
            || !readRequiredString(object, QStringLiteral("mimeType"),
                                   &parsed.mimeType, error)
            || !readRequiredInteger(object, QStringLiteral("originalWidth"), 1,
                                    limits.maximumWidth,
                                    &parsedOriginalWidth, error)
            || !readRequiredInteger(object, QStringLiteral("originalHeight"), 1,
                                    limits.maximumHeight,
                                    &parsedOriginalHeight, error)) {
            return fail(error, error && !error->isEmpty()
                ? *error : QStringLiteral("invalid sidecar schema v3 fields"));
        }
        parsed.eventAtUtc = QDateTime::fromString(event, Qt::ISODateWithMs);
        if (parsed.eventAtUtc.isValid()) {
            parsed.eventAtUtc = parsed.eventAtUtc.toUTC();
        }
        if (expiryValue.isString()) {
            expiry = expiryValue.toString();
            parsed.expiresAtUtc = QDateTime::fromString(expiry,
                                                        Qt::ISODateWithMs);
            if (!parsed.expiresAtUtc.isValid()) {
                return fail(error, QStringLiteral(
                    "expiresAtUtc must be null or a valid ISO timestamp"));
            }
            parsed.expiresAtUtc = parsed.expiresAtUtc.toUTC();
        }
        parsed.visValid = object.value(QStringLiteral("visValid")).toBool();
        parsed.audioFrequencyHz = audioFrequency;
        parsed.sourceSampleRateHz = static_cast<int>(sampleRate);
        parsed.digital = object.value(QStringLiteral("digital")).toBool();
        parsed.completionPercent = static_cast<int>(completion);
        parsed.qualityMetrics = qualityValue.toObject();
        parsed.slantCorrectionPpm = slantCorrection;
        parsed.privacyFlags = static_cast<quint32>(parsedPrivacyFlags);
        parsed.originalWidth = static_cast<int>(parsedOriginalWidth);
        parsed.originalHeight = static_cast<int>(parsedOriginalHeight);
    }
    // Sidecar v1-v3 records predate favourites and migrate deterministically
    // to the safe, non-favourite default.  New writes always use v4.
    if (schemaVersion >= 4) {
        const QJsonValue favoriteValue = object.value(
            QStringLiteral("favorite"));
        if (!favoriteValue.isBool()) {
            return fail(error, QStringLiteral(
                "favorite must be a boolean in sidecar schema v4"));
        }
        parsed.favorite = favoriteValue.toBool();
    }
    if (!parsed.validate(limits, error)) {
        return false;
    }
    *record = std::move(parsed);
    return true;
}

bool operator==(const SstvImageRecord& left,
                const SstvImageRecord& right) noexcept
{
    return left.id == right.id
        && left.category == right.category
        && left.capturedAtUtc == right.capturedAtUtc
        && left.eventAtUtc == right.eventAtUtc
        && left.createdAtUtc == right.createdAtUtc
        && left.updatedAtUtc == right.updatedAtUtc
        && left.mode == right.mode
        && left.visCode == right.visCode
        && left.visValid == right.visValid
        && left.fskId == right.fskId
        && left.remoteCallsign == right.remoteCallsign
        && left.remoteGrid == right.remoteGrid
        && left.localCallsign == right.localCallsign
        && left.localGrid == right.localGrid
        && left.source == right.source
        && left.frequencyHz == right.frequencyHz
        && left.audioFrequencyHz == right.audioFrequencyHz
        && left.sourceSampleRateHz == right.sourceSampleRateHz
        && left.digital == right.digital
        && left.completionPercent == right.completionPercent
        && left.complete == right.complete
        && left.qualityMetrics == right.qualityMetrics
        && left.slantCorrectionPpm == right.slantCorrectionPpm
        && left.rawAudioPath == right.rawAudioPath
        && left.relatedQsoId == right.relatedQsoId
        && left.remote == right.remote
        && left.uploadState == right.uploadState
        && left.remoteProvider == right.remoteProvider
        && left.remoteObjectId == right.remoteObjectId
        && left.expiresAtUtc == right.expiresAtUtc
        && left.privacyFlags == right.privacyFlags
        && left.favorite == right.favorite
        && left.tags == right.tags
        && left.imagePath == right.imagePath
        && left.thumbnailPath == right.thumbnailPath
        && left.metadataPath == right.metadataPath
        && left.sha256 == right.sha256
        && left.mimeType == right.mimeType
        && left.fileSizeBytes == right.fileSizeBytes
        && left.width == right.width
        && left.height == right.height
        && left.originalWidth == right.originalWidth
        && left.originalHeight == right.originalHeight
        && left.note == right.note;
}

bool operator!=(const SstvImageRecord& left,
                const SstvImageRecord& right) noexcept
{
    return !(left == right);
}

SstvStorageLayout::SstvStorageLayout(QString rootPath)
    : m_rootPath(absoluteCleanPath(std::move(rootPath)))
{
}

SstvStorageLayout SstvStorageLayout::fromStandardPaths()
{
    QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
    }
    return SstvStorageLayout(base.isEmpty()
        ? QString()
        : QDir(base).absoluteFilePath(QStringLiteral("sstv")));
}

QString SstvStorageLayout::rootPath() const
{
    return m_rootPath;
}

QString SstvStorageLayout::databasePath() const
{
    return m_rootPath.isEmpty()
        ? QString()
        : QDir(m_rootPath).absoluteFilePath(QStringLiteral("sstv-index.sqlite3"));
}

QString SstvStorageLayout::wavExportRoot() const
{
    return m_rootPath.isEmpty()
        ? QString()
        : QDir(m_rootPath).absoluteFilePath(QStringLiteral("exports/wav"));
}

QString SstvStorageLayout::categoryRoot(SstvImageCategory category) const
{
    const QString categoryName = sstvImageCategoryName(category);
    if (m_rootPath.isEmpty() || categoryName.isEmpty()) {
        return {};
    }
    return QDir(m_rootPath).absoluteFilePath(
        QStringLiteral("images/") + categoryName);
}

QString SstvStorageLayout::datedCategoryDirectory(
    SstvImageCategory category,
    const QDate& utcDate) const
{
    const QString categoryPath = categoryRoot(category);
    if (categoryPath.isEmpty() || !utcDate.isValid()) {
        return {};
    }
    return QDir(categoryPath).absoluteFilePath(
        QStringLiteral("%1/%2")
            .arg(utcDate.year(), 4, 10, QLatin1Char('0'))
            .arg(utcDate.month(), 2, 10, QLatin1Char('0')));
}

bool SstvStorageLayout::ensure(QString* error) const
{
    if (m_rootPath.isEmpty() || !QFileInfo(m_rootPath).isAbsolute()) {
        return fail(error, QStringLiteral("SSTV storage root is empty or relative"));
    }
    QDir root;
    if (!root.mkpath(m_rootPath)) {
        return fail(error, errorWithPath(
            QStringLiteral("cannot create SSTV storage root"), m_rootPath));
    }
    const QString canonicalRoot = canonicalExistingPath(m_rootPath);
    if (canonicalRoot.isEmpty()) {
        return fail(error, QStringLiteral("cannot canonicalize SSTV storage root"));
    }
    const SstvImageCategory categories[] = {
        SstvImageCategory::Received,
        SstvImageCategory::Transmitted,
        SstvImageCategory::Imported,
        SstvImageCategory::Draft
    };
    for (SstvImageCategory category : categories) {
        const QString path = categoryRoot(category);
        if (!root.mkpath(path)) {
            return fail(error, errorWithPath(
                QStringLiteral("cannot create SSTV category directory"), path));
        }
        if (!containsPath(path, true, error)) {
            return false;
        }
    }
    const QString wavPath = wavExportRoot();
    if (!root.mkpath(wavPath)) {
        return fail(error, errorWithPath(
            QStringLiteral("cannot create SSTV WAV export directory"),
            wavPath));
    }
    if (!containsPath(wavPath, true, error)) {
        return false;
    }
    return true;
}

bool SstvStorageLayout::ensureDatedCategoryDirectory(
    SstvImageCategory category,
    const QDate& utcDate,
    QString* directory,
    QString* error) const
{
    if (!directory) {
        return fail(error, QStringLiteral("directory output is null"));
    }
    if (!ensure(error)) {
        return false;
    }
    const QString path = datedCategoryDirectory(category, utcDate);
    if (path.isEmpty() || !QDir().mkpath(path)) {
        return fail(error, errorWithPath(
            QStringLiteral("cannot create dated SSTV directory"), path));
    }
    if (!containsPath(path, true, error)) {
        return false;
    }
    *directory = absoluteCleanPath(path);
    return true;
}

bool SstvStorageLayout::containsPath(const QString& candidatePath,
                                     bool requireExistingParent,
                                     QString* error) const
{
    if (m_rootPath.isEmpty() || candidatePath.isEmpty()
        || !QFileInfo(candidatePath).isAbsolute()) {
        return fail(error, QStringLiteral("path containment requires absolute paths"));
    }
    const QString canonicalRoot = canonicalExistingPath(m_rootPath);
    if (canonicalRoot.isEmpty()) {
        return fail(error, QStringLiteral("storage root does not exist"));
    }

    const QFileInfo candidateInfo(candidatePath);
    QString resolvedCandidate;
    if (candidateInfo.exists()) {
        resolvedCandidate = canonicalExistingPath(candidatePath);
    } else {
        const QString canonicalParent = canonicalExistingPath(
            candidateInfo.absolutePath());
        if (canonicalParent.isEmpty()) {
            if (requireExistingParent) {
                return fail(error, QStringLiteral("candidate parent does not exist"));
            }
            resolvedCandidate = absoluteCleanPath(candidatePath);
        } else {
            resolvedCandidate = QDir(canonicalParent).absoluteFilePath(
                candidateInfo.fileName());
        }
    }
    if (resolvedCandidate.isEmpty()) {
        return fail(error, QStringLiteral("cannot resolve candidate path"));
    }
    const QString relative = QDir(canonicalRoot).relativeFilePath(
        QDir::cleanPath(resolvedCandidate));
    if (relativePathEscapes(relative)) {
        return fail(error, QStringLiteral("candidate path escapes SSTV storage root"));
    }
    return true;
}

SstvImageStore::SstvImageStore(SstvStorageLayout layout,
                               SstvStorageLimits limits)
    : m_layout(std::move(layout))
    , m_limits(limits)
{
}

QString SstvImageStore::sanitizeFileComponent(const QString& input,
                                              int maximumUtf8Bytes)
{
    if (maximumUtf8Bytes <= 0) {
        return {};
    }
    const QString normalized = input.normalized(QString::NormalizationForm_C);
    QString sanitized;
    sanitized.reserve(normalized.size());
    bool previousReplacement = false;
    for (qsizetype index = 0; index < normalized.size(); ++index) {
        QString unit;
        const QChar first = normalized.at(index);
        if (first.isHighSurrogate() && index + 1 < normalized.size()
            && normalized.at(index + 1).isLowSurrogate()) {
            unit.append(first);
            unit.append(normalized.at(++index));
        } else {
            unit.append(first);
        }

        const ushort code = first.unicode();
        const bool forbidden = code < 0x20U || code == 0x7fU
            || first == QLatin1Char('/') || first == QLatin1Char('\\')
            || first == QLatin1Char(':') || first == QLatin1Char('*')
            || first == QLatin1Char('?') || first == QLatin1Char('"')
            || first == QLatin1Char('<') || first == QLatin1Char('>')
            || first == QLatin1Char('|');
        const bool repeatedDot = first == QLatin1Char('.')
            && !sanitized.isEmpty() && sanitized.endsWith(QLatin1Char('.'));
        if (forbidden || repeatedDot) {
            if (!previousReplacement) {
                sanitized.append(QLatin1Char('_'));
            }
            previousReplacement = true;
            continue;
        }
        if (first.isSpace()) {
            if (!sanitized.isEmpty() && !sanitized.endsWith(QLatin1Char(' '))) {
                sanitized.append(QLatin1Char(' '));
            }
            previousReplacement = false;
            continue;
        }
        const QByteArray prospective = (sanitized + unit).toUtf8();
        if (prospective.size() > maximumUtf8Bytes) {
            break;
        }
        sanitized.append(unit);
        previousReplacement = false;
    }

    sanitized = sanitized.trimmed();
    while (sanitized.startsWith(QLatin1Char('.'))
           || sanitized.endsWith(QLatin1Char('.'))
           || sanitized.endsWith(QLatin1Char(' '))) {
        if (sanitized.startsWith(QLatin1Char('.'))) {
            sanitized.remove(0, 1);
        } else {
            sanitized.chop(1);
        }
    }
    if (sanitized.isEmpty() || sanitized == QLatin1String("..")) {
        sanitized = QStringLiteral("sstv");
    }

    const QString baseName = sanitized.section(QLatin1Char('.'), 0, 0)
                                 .trimmed().toUpper();
    static const QRegularExpression windowsReserved(
        QStringLiteral("^(CON|PRN|AUX|NUL|CLOCK\\$|COM[1-9]|LPT[1-9])$"));
    if (windowsReserved.match(baseName).hasMatch()) {
        sanitized.prepend(QLatin1Char('_'));
    }
    while (sanitized.toUtf8().size() > maximumUtf8Bytes
           && !sanitized.isEmpty()) {
        sanitized.chop(1);
        if (!sanitized.isEmpty() && sanitized.back().isHighSurrogate()) {
            sanitized.chop(1);
        }
    }
    return sanitized.isEmpty() ? QStringLiteral("sstv") : sanitized;
}

bool SstvImageStore::renderFileBase(const QString& nameTemplate,
                                    const SstvImageRecord& record,
                                    int maximumUtf8Bytes,
                                    QString* fileBase,
                                    QString* error)
{
    if (!fileBase || nameTemplate.trimmed().isEmpty()
        || maximumUtf8Bytes < 32 || maximumUtf8Bytes > 220) {
        return fail(error, QStringLiteral("invalid filename template request"));
    }
    const QDateTime captured = record.capturedAtUtc.toUTC();
    if (!captured.isValid()) {
        return fail(error, QStringLiteral("filename template needs a capture timestamp"));
    }
    const QHash<QString, QString> tokens {
        {QStringLiteral("date"), captured.toString(QStringLiteral("yyyyMMdd"))},
        {QStringLiteral("time"), captured.toString(QStringLiteral("HHmmsszzz"))},
        {QStringLiteral("mode"), record.mode},
        {QStringLiteral("remoteCall"), record.remoteCallsign},
        {QStringLiteral("localCall"), record.localCallsign},
        {QStringLiteral("source"), record.source},
        {QStringLiteral("category"), sstvImageCategoryName(record.category)},
        {QStringLiteral("id"), record.id}
    };

    QString rendered;
    qsizetype cursor = 0;
    static const QRegularExpression placeholder(
        QStringLiteral("\\{([A-Za-z][A-Za-z0-9]*)\\}"));
    QRegularExpressionMatchIterator matches = placeholder.globalMatch(nameTemplate);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        rendered.append(nameTemplate.mid(cursor,
                                         match.capturedStart() - cursor));
        const QString key = match.captured(1);
        const auto token = tokens.constFind(key);
        if (token == tokens.cend()) {
            return fail(error, QStringLiteral("unknown filename token: {%1}").arg(key));
        }
        rendered.append(token.value());
        cursor = match.capturedEnd();
    }
    rendered.append(nameTemplate.mid(cursor));
    if (rendered.contains(QLatin1Char('{'))
        || rendered.contains(QLatin1Char('}'))) {
        return fail(error, QStringLiteral("malformed filename template"));
    }
    if (rendered.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
        rendered.chop(4);
    }
    *fileBase = sanitizeFileComponent(rendered, maximumUtf8Bytes);
    if (fileBase->isEmpty()) {
        return fail(error, QStringLiteral("filename template produced an empty name"));
    }
    return true;
}

QByteArray SstvImageStore::sha256File(const QString& path,
                                      qint64 maximumBytes,
                                      qint64* byteCount,
                                      QString* error)
{
    if (byteCount) {
        *byteCount = 0;
    }
    if (maximumBytes <= 0) {
        fail(error, QStringLiteral("invalid hash byte limit"));
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(error, errorWithPath(file.errorString(), path));
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 total = 0;
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1024 * 1024);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            fail(error, errorWithPath(file.errorString(), path));
            return {};
        }
        if (total > maximumBytes - chunk.size()) {
            fail(error, QStringLiteral("file exceeds hash byte limit"));
            return {};
        }
        total += chunk.size();
        hash.addData(chunk);
    }
    if (total <= 0) {
        fail(error, QStringLiteral("cannot hash an empty file"));
        return {};
    }
    if (byteCount) {
        *byteCount = total;
    }
    return hash.result();
}

SstvImageSaveResult SstvImageStore::save(
    const SstvImageSaveRequest& request) const
{
    return saveImpl(request, nullptr);
}

SstvImageSaveResult SstvImageStore::savePreservingPng(
    const SstvImageSaveRequest& request,
    const QByteArray& encodedPng) const
{
    return saveImpl(request, &encodedPng);
}

SstvImageSaveResult SstvImageStore::saveImpl(
    const SstvImageSaveRequest& request,
    const QByteArray* encodedPng) const
{
    QString detail;
    if (!m_limits.validate(&detail)) {
        return saveFailure(SstvStoreError::InvalidRequest, detail);
    }
    if (request.image.isNull()) {
        return saveFailure(SstvStoreError::InvalidRequest,
                           QStringLiteral("cannot store a null image"));
    }
    const int width = request.image.width();
    const int height = request.image.height();
    qint64 pixels = 0;
    if (!checkedPixels(width, height, &pixels)
        || width > m_limits.maximumWidth
        || height > m_limits.maximumHeight
        || pixels > m_limits.maximumPixels
        || request.image.sizeInBytes() <= 0
        || request.image.sizeInBytes() > m_limits.maximumDecodedBytes) {
        return saveFailure(SstvStoreError::LimitExceeded,
                           QStringLiteral("decoded image exceeds storage limits"));
    }
    if (encodedPng) {
        static const QByteArray pngMagic = QByteArray::fromHex(
            QByteArrayLiteral("89504e470d0a1a0a"));
        if (encodedPng->size() <= 0
            || encodedPng->size() > m_limits.maximumPngBytes
            || !encodedPng->startsWith(pngMagic)) {
            return saveFailure(SstvStoreError::IntegrityFailure,
                               QStringLiteral("preserved PNG bytes are invalid"));
        }
        QBuffer input;
        input.setData(*encodedPng);
        if (!input.open(QIODevice::ReadOnly)) {
            return saveFailure(SstvStoreError::IntegrityFailure,
                               QStringLiteral("preserved PNG could not be opened"));
        }
        QImageReader reader(&input, QByteArrayLiteral("png"));
        reader.setAutoDetectImageFormat(false);
        reader.setDecideFormatFromContent(true);
        reader.setAutoTransform(false);
        const QSize encodedSize = reader.size();
        const int allocationLimitMb = QImageReader::allocationLimit();
        const quint64 qtAllocationBytes = allocationLimitMb > 0
            ? static_cast<quint64>(allocationLimitMb) * 1024ULL * 1024ULL
            : 0U;
        const quint64 allocationBytes = std::min(
            static_cast<quint64>(m_limits.maximumDecodedBytes),
            qtAllocationBytes);
        if (reader.format().toLower() != QByteArrayLiteral("png")
            || encodedSize != request.image.size()
            || allocationBytes == 0U
            || static_cast<quint64>(pixels) > allocationBytes / 8U
            || reader.supportsAnimation() || reader.imageCount() > 1
            || !reader.textKeys().isEmpty()) {
            return saveFailure(SstvStoreError::IntegrityFailure,
                               QStringLiteral("preserved PNG structure is invalid"));
        }
        const QImage decoded = reader.read();
        if (decoded.isNull() || decoded.size() != encodedSize
            || decoded.sizeInBytes() <= 0
            || decoded.sizeInBytes() > m_limits.maximumDecodedBytes
            || decoded.convertToFormat(QImage::Format_RGBA8888)
                != request.image.convertToFormat(QImage::Format_RGBA8888)) {
            return saveFailure(SstvStoreError::IntegrityFailure,
                               QStringLiteral("preserved PNG pixels changed"));
        }
    }

    SstvImageRecord record = request.record;
    if (record.id.isEmpty()) {
        record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    } else {
        record.id = record.id.toLower();
    }
    if (!canonicalUuid(record.id)) {
        return saveFailure(SstvStoreError::InvalidRequest,
                           QStringLiteral("invalid image record UUID"));
    }
    if (!record.capturedAtUtc.isValid()) {
        return saveFailure(SstvStoreError::InvalidRequest,
                           QStringLiteral("capture timestamp is required"));
    }
    record.capturedAtUtc = record.capturedAtUtc.toUTC();
    if (record.eventAtUtc.isValid()) {
        record.eventAtUtc = record.eventAtUtc.toUTC();
    } else {
        record.eventAtUtc = record.capturedAtUtc;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    record.createdAtUtc = now;
    record.updatedAtUtc = now;
    record.width = width;
    record.height = height;
    if (record.originalWidth <= 0 || record.originalHeight <= 0) {
        record.originalWidth = width;
        record.originalHeight = height;
    }
    if (record.complete && record.completionPercent == 0) {
        record.completionPercent = 100;
    }
    if (record.mimeType.isEmpty()) {
        record.mimeType = QStringLiteral("image/png");
    }
    record.fileSizeBytes = 1;
    record.sha256 = QByteArray(kHashBytes, '\0');

    QString directory;
    if (!m_layout.ensureDatedCategoryDirectory(
            record.category, record.capturedAtUtc.date(), &directory, &detail)) {
        return saveFailure(SstvStoreError::InvalidLayout, detail);
    }
    QString fileBase;
    if (!renderFileBase(request.fileNameTemplate, record,
                        m_limits.maximumFileNameUtf8Bytes,
                        &fileBase, &detail)) {
        return saveFailure(SstvStoreError::InvalidRequest, detail);
    }
    record.imagePath = QDir::cleanPath(
        QDir(directory).absoluteFilePath(fileBase + QStringLiteral(".png")));
    record.thumbnailPath = QDir::cleanPath(
        QDir(directory).absoluteFilePath(fileBase
                                         + QStringLiteral(".thumb.png")));
    record.metadataPath = QDir::cleanPath(
        QDir(directory).absoluteFilePath(fileBase + QStringLiteral(".json")));
    if (!m_layout.containsPath(record.imagePath, true, &detail)
        || !m_layout.containsPath(record.thumbnailPath, true, &detail)
        || !m_layout.containsPath(record.metadataPath, true, &detail)) {
        return saveFailure(SstvStoreError::InvalidLayout, detail);
    }

    // Serialize publication across threads and Decodium processes.  QFile's
    // native rename behaviour is not a portable atomic "no replace" primitive;
    // without this lock a loser in a same-name race could mistake the winner's
    // PNG for its own and remove it while rolling back its sidecar.
    QLockFile publishLock(record.imagePath + QStringLiteral(".lock"));
    publishLock.setStaleLockTime(30'000);
    if (!publishLock.tryLock(0)) {
        const SstvStoreError code = publishLock.error()
                == QLockFile::LockFailedError
            ? SstvStoreError::Collision
            : SstvStoreError::IoFailure;
        return saveFailure(code,
                           QStringLiteral("SSTV image name is locked by another writer"));
    }
    if (QFileInfo::exists(record.imagePath)
        || QFileInfo::exists(record.thumbnailPath)
        || QFileInfo::exists(record.metadataPath)) {
        return saveFailure(SstvStoreError::Collision,
                           QStringLiteral("SSTV image name already exists"));
    }
    if (!record.validate(m_limits, &detail)) {
        return saveFailure(SstvStoreError::InvalidRequest, detail);
    }

    const QString stageSuffix = QStringLiteral(".stage-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString imageStage = record.imagePath + stageSuffix;
    const QString metadataStage = record.metadataPath + stageSuffix;
    auto cleanupStages = [&]() {
        if (QFileInfo::exists(imageStage)) {
            QFile::remove(imageStage);
        }
        if (QFileInfo::exists(metadataStage)) {
            QFile::remove(metadataStage);
        }
    };
    if (QFileInfo::exists(imageStage) || QFileInfo::exists(metadataStage)) {
        return saveFailure(SstvStoreError::Collision,
                           QStringLiteral("temporary SSTV image name collision"));
    }

    if (encodedPng) {
        if (!writeBytesAtomically(imageStage, *encodedPng, &detail)) {
            cleanupStages();
            return saveFailure(SstvStoreError::IoFailure, detail);
        }
    } else {
        QSaveFile output(imageStage);
        output.setDirectWriteFallback(false);
        if (!output.open(QIODevice::WriteOnly)) {
            return saveFailure(SstvStoreError::IoFailure,
                               errorWithPath(output.errorString(), imageStage));
        }
        QImageWriter writer(&output, QByteArrayLiteral("png"));
        writer.setCompression(6);
        if (!writer.write(request.image)) {
            const QString writerError = writer.errorString();
            output.cancelWriting();
            cleanupStages();
            return saveFailure(SstvStoreError::EncodingFailed, writerError);
        }
        if (output.size() <= 0 || output.size() > m_limits.maximumPngBytes) {
            output.cancelWriting();
            cleanupStages();
            return saveFailure(SstvStoreError::LimitExceeded,
                               QStringLiteral("encoded PNG exceeds storage limits"));
        }
        if (!output.commit()) {
            const QString outputError = output.errorString();
            cleanupStages();
            return saveFailure(SstvStoreError::IoFailure,
                               errorWithPath(outputError, imageStage));
        }
    }

    record.sha256 = sha256File(imageStage, m_limits.maximumPngBytes,
                               &record.fileSizeBytes, &detail);
    if (record.sha256.size() != kHashBytes
        || !record.validate(m_limits, &detail)) {
        cleanupStages();
        return saveFailure(SstvStoreError::IntegrityFailure, detail);
    }
    const QByteArray metadata = QJsonDocument(record.toJson())
                                    .toJson(QJsonDocument::Indented);
    if (metadata.isEmpty()
        || metadata.size() > m_limits.maximumMetadataBytes) {
        cleanupStages();
        return saveFailure(SstvStoreError::LimitExceeded,
                           QStringLiteral("metadata sidecar exceeds storage limits"));
    }
    if (!writeBytesAtomically(metadataStage, metadata, &detail)) {
        cleanupStages();
        return saveFailure(SstvStoreError::IoFailure, detail);
    }

    // The inter-process record lock stays held across both destination checks,
    // staged renames and verification.  Every cooperating writer therefore
    // observes an all-or-collision publish and never silently replaces a name.
    if (QFileInfo::exists(record.imagePath)
        || !QFile::rename(imageStage, record.imagePath)) {
        cleanupStages();
        return saveFailure(SstvStoreError::Collision,
                           QStringLiteral("PNG publish collision or rename failure"));
    }
    if (QFileInfo::exists(record.metadataPath)
        || !QFile::rename(metadataStage, record.metadataPath)) {
        QFile::remove(record.imagePath);
        cleanupStages();
        return saveFailure(SstvStoreError::Collision,
                           QStringLiteral("metadata publish collision or rename failure"));
    }

    if (!verify(record, true, &detail)) {
        QFile::remove(record.metadataPath);
        QFile::remove(record.imagePath);
        return saveFailure(SstvStoreError::IntegrityFailure, detail);
    }
    SstvImageSaveResult result;
    result.ok = true;
    result.code = SstvStoreError::None;
    result.record = std::move(record);
    return result;
}

bool SstvImageStore::updateMetadata(const SstvImageRecord& record,
                                    QString* error) const
{
    if (!record.validate(m_limits, error)
        || !m_layout.containsPath(record.imagePath, true, error)
        || !m_layout.containsPath(record.thumbnailPath, true, error)
        || (!record.rawAudioPath.isEmpty()
            && !m_layout.containsPath(record.rawAudioPath, true, error))
        || !m_layout.containsPath(record.metadataPath, true, error)) {
        return false;
    }
    const QFileInfo imageInfo(record.imagePath);
    const QFileInfo metadataInfo(record.metadataPath);
    if (!imageInfo.isFile() || imageInfo.isSymLink()
        || !metadataInfo.isFile() || metadataInfo.isSymLink()
        || imageInfo.size() != record.fileSizeBytes) {
        return fail(error, QStringLiteral("metadata update record files are invalid"));
    }
    const QByteArray hash = sha256File(record.imagePath,
                                       m_limits.maximumPngBytes,
                                       nullptr, error);
    if (hash != record.sha256) {
        return fail(error, QStringLiteral("metadata update PNG SHA-256 mismatch"));
    }
    QImageReader reader(record.imagePath, QByteArrayLiteral("png"));
    const QSize dimensions = reader.size();
    if (dimensions.width() != record.width
        || dimensions.height() != record.height) {
        return fail(error, QStringLiteral("metadata update PNG dimensions mismatch"));
    }
    const QByteArray metadata = QJsonDocument(record.toJson())
                                    .toJson(QJsonDocument::Indented);
    if (metadata.isEmpty()
        || metadata.size() > m_limits.maximumMetadataBytes) {
        return fail(error, QStringLiteral("updated sidecar exceeds storage limits"));
    }
    if (!writeBytesAtomically(record.metadataPath, metadata, error)) {
        return false;
    }
    return verify(record, true, error);
}

bool SstvImageStore::verify(const SstvImageRecord& record,
                            bool verifyHash,
                            QString* error) const
{
    if (!record.validate(m_limits, error)
        || !m_layout.containsPath(record.imagePath, true, error)
        || !m_layout.containsPath(record.thumbnailPath, true, error)
        || (!record.rawAudioPath.isEmpty()
            && !m_layout.containsPath(record.rawAudioPath, true, error))
        || !m_layout.containsPath(record.metadataPath, true, error)) {
        return false;
    }
    const QFileInfo imageInfo(record.imagePath);
    const QFileInfo metadataInfo(record.metadataPath);
    if (!imageInfo.isFile() || imageInfo.isSymLink()
        || !metadataInfo.isFile() || metadataInfo.isSymLink()) {
        return fail(error, QStringLiteral("record files are missing or symbolic links"));
    }
    if (imageInfo.size() != record.fileSizeBytes
        || imageInfo.size() <= 0
        || imageInfo.size() > m_limits.maximumPngBytes
        || metadataInfo.size() <= 0
        || metadataInfo.size() > m_limits.maximumMetadataBytes) {
        return fail(error, QStringLiteral("record file sizes do not match metadata"));
    }

    QImageReader reader(record.imagePath, QByteArrayLiteral("png"));
    const QSize dimensions = reader.size();
    qint64 pixels = 0;
    if (!dimensions.isValid()
        || !checkedPixels(dimensions.width(), dimensions.height(), &pixels)
        || dimensions.width() != record.width
        || dimensions.height() != record.height
        || dimensions.width() > m_limits.maximumWidth
        || dimensions.height() > m_limits.maximumHeight
        || pixels > m_limits.maximumPixels) {
        return fail(error, QStringLiteral("PNG dimensions do not match metadata"));
    }
    if (verifyHash) {
        const QByteArray hash = sha256File(record.imagePath,
                                           m_limits.maximumPngBytes,
                                           nullptr, error);
        if (hash != record.sha256) {
            return fail(error, QStringLiteral("PNG SHA-256 mismatch"));
        }
    }
    SstvImageRecord sidecar;
    if (!loadMetadata(record.metadataPath, &sidecar, error, m_limits)
        || sidecar != record) {
        return fail(error, QStringLiteral("metadata sidecar does not match record"));
    }
    return true;
}

bool SstvImageStore::loadMetadata(const QString& metadataPath,
                                  SstvImageRecord* record,
                                  QString* error,
                                  const SstvStorageLimits& limits)
{
    if (!record || !limits.validate(error)) {
        return false;
    }
    QFile file(metadataPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(error, errorWithPath(file.errorString(), metadataPath));
    }
    if (file.size() <= 0 || file.size() > limits.maximumMetadataBytes) {
        return fail(error, QStringLiteral("metadata sidecar exceeds storage limits"));
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(error, QStringLiteral("invalid metadata JSON: %1")
                               .arg(parseError.errorString()));
    }
    return SstvImageRecord::fromJson(document.object(), record, error, limits);
}

} // namespace decodium::sstv
