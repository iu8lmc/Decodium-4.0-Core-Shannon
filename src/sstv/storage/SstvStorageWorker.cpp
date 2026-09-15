// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvStorageWorker.h"

#include "src/sstv/diagnostics/SstvDiagnosticLogging.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLockFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSaveFile>
#include <QThread>
#include <QTimeZone>
#include <QUuid>
#include <QVariant>
#include <QStringList>

#include <algorithm>
#include <chrono>
#include <functional>
#include <limits>
#include <optional>
#include <utility>

namespace decodium::sstv {
namespace {

bool fail(QString* error, const QString& detail)
{
    if (error) {
        *error = detail;
    }
    return false;
}

template<typename SaveOperation>
SstvImageSaveResult measuredImageSave(
    SstvStoragePerformanceCounters& counters,
    SaveOperation&& operation)
{
    const auto started = std::chrono::steady_clock::now();
    SstvImageSaveResult result = operation();
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - started).count();
    counters.recordImageSave(
        elapsed <= 0 ? 0U : static_cast<quint64>(elapsed), result.ok);
    return result;
}

QString queryFailure(const QString& operation, const QSqlQuery& query)
{
    return QStringLiteral("%1: %2")
        .arg(operation, query.lastError().text());
}

QString databaseFailure(const QString& operation,
                        const QSqlDatabase& database)
{
    return QStringLiteral("%1: %2")
        .arg(operation, database.lastError().text());
}

bool executeSql(QSqlDatabase& database,
                const QString& sql,
                const QString& operation,
                QString* error)
{
    QSqlQuery query(database);
    if (!query.exec(sql)) {
        return fail(error, queryFailure(operation, query));
    }
    return true;
}

bool currentUserVersion(QSqlDatabase& database,
                        int* version,
                        QString* error)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        return fail(error, queryFailure(QStringLiteral("read schema version"),
                                        query));
    }
    bool ok = false;
    const int value = query.value(0).toInt(&ok);
    if (!ok || value < 0) {
        return fail(error, QStringLiteral("invalid SQLite schema version"));
    }
    *version = value;
    return true;
}

bool tableHasColumn(QSqlDatabase& database,
                    const QString& table,
                    const QString& column,
                    bool* present,
                    QString* error)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        return fail(error, queryFailure(QStringLiteral("inspect table"), query));
    }
    *present = false;
    while (query.next()) {
        if (query.value(1).toString() == column) {
            *present = true;
            break;
        }
    }
    return true;
}

QString recordColumns()
{
    return QStringLiteral(
        "id, category, captured_at_ms, created_at_ms, updated_at_ms, "
        "mode, vis_code, remote_callsign, local_callsign, source, "
        "frequency_hz, complete, image_path, metadata_path, sha256_hex, "
        "file_size_bytes, width, height, note, remote, upload_state, tags_json, "
        "event_at_ms, thumbnail_path, mime_type, original_width, "
        "original_height, digital, vis_valid, fsk_id, local_grid, remote_grid, "
        "audio_frequency_hz, source_sample_rate_hz, completion_percent, "
        "quality_json, slant_correction_ppm, raw_audio_path, related_qso_id, "
        "remote_provider, remote_object_id, expires_at_ms, privacy_flags, "
        "favorite");
}

QString tagsJson(const QStringList& tags)
{
    QJsonArray array;
    for (const QString& tag : tags) {
        array.append(tag);
    }
    return QString::fromUtf8(
        QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void normalizeUserMetadata(QString* note, QStringList* tags)
{
    Q_ASSERT(note);
    Q_ASSERT(tags);
    // Notes intentionally keep their whitespace and line breaks. Tags are
    // canonicalized just like Gallery query tags so the indexed, sidecar and
    // visible spellings agree. SstvImageRecord::validate remains the
    // authoritative bound/control-character/duplicate check.
    *note = note->normalized(QString::NormalizationForm_C);
    for (QString& tag : *tags) {
        tag = tag.trimmed().normalized(QString::NormalizationForm_C);
    }
}

bool parseTagsJson(const QString& encoded, QStringList* tags, QString* error)
{
    if (!tags) {
        return fail(error, QStringLiteral("tag output is null"));
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        encoded.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        return fail(error, QStringLiteral("database record has invalid tag JSON"));
    }
    QStringList parsed;
    const QJsonArray array = document.array();
    parsed.reserve(array.size());
    for (const QJsonValue& value : array) {
        if (!value.isString()) {
            return fail(error, QStringLiteral("database record has a non-text tag"));
        }
        parsed.append(value.toString());
    }
    *tags = std::move(parsed);
    return true;
}

QString qualityJson(const QJsonObject& metrics)
{
    return QString::fromUtf8(
        QJsonDocument(metrics).toJson(QJsonDocument::Compact));
}

bool parseQualityJson(const QString& encoded,
                      QJsonObject* metrics,
                      QString* error)
{
    if (!metrics) {
        return fail(error, QStringLiteral("quality metrics output is null"));
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        encoded.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(error,
                    QStringLiteral("database record has invalid quality JSON"));
    }
    *metrics = document.object();
    return true;
}

QString canonicalAbsolutePath(const QString& path)
{
    if (path.isEmpty()) {
        return {};
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool canonicalUuid(const QString& value)
{
    const QUuid uuid(value);
    return value.size() == 36 && value == value.toLower()
        && !uuid.isNull()
        && uuid.toString(QUuid::WithoutBraces) == value;
}

constexpr quint64 kMaximumIncomingBytes = 64ULL * 1024ULL * 1024ULL;
constexpr quint64 kMaximumIncomingPixels = 16'777'216ULL;
constexpr quint32 kMaximumIncomingDimension = 8'192U;
constexpr quint64 kMaximumIncomingDecodeBytes = 128ULL * 1024ULL * 1024ULL;

bool safeHandoffIdentifier(const QString& value)
{
    if (value.isEmpty() || value.size() > 128
        || value.contains(QStringLiteral(".."))) {
        return false;
    }
    for (QChar character : value) {
        const ushort code = character.unicode();
        const bool allowed = (code >= 'a' && code <= 'z')
            || (code >= 'A' && code <= 'Z')
            || (code >= '0' && code <= '9')
            || character == QLatin1Char('.') || character == QLatin1Char('_')
            || character == QLatin1Char(':') || character == QLatin1Char('@')
            || character == QLatin1Char('+') || character == QLatin1Char('-');
        if (!allowed) {
            return false;
        }
    }
    return true;
}

bool safeHandoffText(const QString& value, qsizetype maximumCharacters)
{
    if (value.isEmpty() || value.size() > maximumCharacters
        || value != value.trimmed()
        || value != value.normalized(QString::NormalizationForm_C)) {
        return false;
    }
    for (QChar character : value) {
        const ushort code = character.unicode();
        if (code < 0x20U || code == 0x7fU) {
            return false;
        }
    }
    return true;
}

bool safeHandoffFilename(const QString& value)
{
    if (!safeHandoffText(value, 128) || value.contains(QStringLiteral(".."))) {
        return false;
    }
    static const QString forbidden = QStringLiteral("/\\:*?\"<>|");
    for (QChar character : value) {
        if (forbidden.contains(character)) {
            return false;
        }
    }
    return value != QStringLiteral(".") && value != QStringLiteral("..");
}

bool lowercaseSha256(const QString& value)
{
    if (value.size() != 64) {
        return false;
    }
    for (QChar character : value) {
        const ushort code = character.unicode();
        if (!((code >= '0' && code <= '9')
              || (code >= 'a' && code <= 'f'))) {
            return false;
        }
    }
    return true;
}

bool exactUtc(const QDateTime& value)
{
    return value.isValid() && value.offsetFromUtc() == 0
        && value.timeSpec() != Qt::LocalTime
        && value.toMSecsSinceEpoch() > 0;
}

bool validIncomingHandoff(
    const sharing::SstvValidatedIncomingHandoff& handoff,
    QString* error)
{
    const bool sourceMimeValid = handoff.sourceMimeType
            == QStringLiteral("image/png")
        || handoff.sourceMimeType == QStringLiteral("image/jpeg");
    const bool dimensionsValid = handoff.width > 0U && handoff.height > 0U
        && handoff.width <= kMaximumIncomingDimension
        && handoff.height <= kMaximumIncomingDimension
        && static_cast<quint64>(handoff.width)
            <= kMaximumIncomingPixels / handoff.height;
    if (handoff.schemaVersion != 1 || !canonicalUuid(handoff.transferId)
        || !safeHandoffIdentifier(handoff.providerId)
        || !safeHandoffIdentifier(handoff.incomingId)
        || !safeHandoffIdentifier(handoff.senderId)
        || !safeHandoffFilename(handoff.safeDisplayFilename)
        || !safeHandoffText(handoff.sstvMode, 64)
        || !sourceMimeValid || !lowercaseSha256(handoff.sourceSha256)
        || handoff.sourceByteSize == 0U
        || handoff.sourceByteSize > kMaximumIncomingBytes
        || handoff.stagedMimeType != QStringLiteral("image/png")
        || !lowercaseSha256(handoff.stagedSha256)
        || handoff.stagedByteSize == 0U
        || handoff.stagedByteSize > kMaximumIncomingBytes
        || handoff.stagedCanonicalPath.isEmpty()
        || handoff.stagedCanonicalPath.size() > 4096
        || !QFileInfo(handoff.stagedCanonicalPath).isAbsolute()
        || QDir::cleanPath(handoff.stagedCanonicalPath)
            != handoff.stagedCanonicalPath
        || !dimensionsValid || !exactUtc(handoff.receivedUtc)
        || !exactUtc(handoff.expiresUtc)
        || handoff.expiresUtc <= handoff.receivedUtc) {
        return fail(error, QStringLiteral(
            "validated incoming handoff schema is invalid"));
    }
    return true;
}

bool exactString(const QVariantMap& map,
                 const QString& key,
                 QString* output)
{
    const auto iterator = map.constFind(key);
    if (iterator == map.cend()
        || iterator->metaType().id() != QMetaType::QString) {
        return false;
    }
    *output = iterator->toString();
    return true;
}

bool exactUnsigned(const QVariantMap& map,
                   const QString& key,
                   quint64* output)
{
    const auto iterator = map.constFind(key);
    if (iterator == map.cend()) {
        return false;
    }
    const int type = iterator->metaType().id();
    if (type != QMetaType::UInt && type != QMetaType::ULongLong) {
        return false;
    }
    bool ok = false;
    const quint64 value = iterator->toULongLong(&ok);
    if (!ok) {
        return false;
    }
    *output = value;
    return true;
}

bool exactDateTime(const QVariantMap& map,
                   const QString& key,
                   QDateTime* output)
{
    const auto iterator = map.constFind(key);
    if (iterator == map.cend()
        || iterator->metaType().id() != QMetaType::QDateTime) {
        return false;
    }
    *output = iterator->toDateTime();
    return true;
}

bool parseIncomingHandoffMap(
    const QVariantMap& map,
    sharing::SstvValidatedIncomingHandoff* output,
    QString* error)
{
    if (!output) {
        return fail(error, QStringLiteral("incoming handoff output is null"));
    }
    static const QSet<QString> expectedKeys {
        QStringLiteral("schemaVersion"),
        QStringLiteral("transferId"),
        QStringLiteral("providerId"),
        QStringLiteral("incomingId"),
        QStringLiteral("senderId"),
        QStringLiteral("safeDisplayFilename"),
        QStringLiteral("sstvMode"),
        QStringLiteral("sourceMimeType"),
        QStringLiteral("sourceSha256"),
        QStringLiteral("sourceByteSize"),
        QStringLiteral("stagedCanonicalPath"),
        QStringLiteral("stagedMimeType"),
        QStringLiteral("stagedSha256"),
        QStringLiteral("stagedByteSize"),
        QStringLiteral("width"),
        QStringLiteral("height"),
        QStringLiteral("receivedUtc"),
        QStringLiteral("expiresUtc")
    };
    QSet<QString> actualKeys;
    for (auto iterator = map.cbegin(); iterator != map.cend(); ++iterator) {
        actualKeys.insert(iterator.key());
    }
    const auto schema = map.constFind(QStringLiteral("schemaVersion"));
    if (actualKeys != expectedKeys || schema == map.cend()
        || schema->metaType().id() != QMetaType::Int) {
        return fail(error, QStringLiteral(
            "validated incoming handoff fields are invalid"));
    }

    sharing::SstvValidatedIncomingHandoff parsed;
    parsed.schemaVersion = schema->toInt();
    quint64 width = 0U;
    quint64 height = 0U;
    if (!exactString(map, QStringLiteral("transferId"), &parsed.transferId)
        || !exactString(map, QStringLiteral("providerId"), &parsed.providerId)
        || !exactString(map, QStringLiteral("incomingId"), &parsed.incomingId)
        || !exactString(map, QStringLiteral("senderId"), &parsed.senderId)
        || !exactString(map, QStringLiteral("safeDisplayFilename"),
                        &parsed.safeDisplayFilename)
        || !exactString(map, QStringLiteral("sstvMode"), &parsed.sstvMode)
        || !exactString(map, QStringLiteral("sourceMimeType"),
                        &parsed.sourceMimeType)
        || !exactString(map, QStringLiteral("sourceSha256"),
                        &parsed.sourceSha256)
        || !exactUnsigned(map, QStringLiteral("sourceByteSize"),
                          &parsed.sourceByteSize)
        || !exactString(map, QStringLiteral("stagedCanonicalPath"),
                        &parsed.stagedCanonicalPath)
        || !exactString(map, QStringLiteral("stagedMimeType"),
                        &parsed.stagedMimeType)
        || !exactString(map, QStringLiteral("stagedSha256"),
                        &parsed.stagedSha256)
        || !exactUnsigned(map, QStringLiteral("stagedByteSize"),
                          &parsed.stagedByteSize)
        || !exactUnsigned(map, QStringLiteral("width"), &width)
        || !exactUnsigned(map, QStringLiteral("height"), &height)
        || width > std::numeric_limits<quint32>::max()
        || height > std::numeric_limits<quint32>::max()
        || !exactDateTime(map, QStringLiteral("receivedUtc"),
                          &parsed.receivedUtc)
        || !exactDateTime(map, QStringLiteral("expiresUtc"),
                          &parsed.expiresUtc)) {
        return fail(error, QStringLiteral(
            "validated incoming handoff types are invalid"));
    }
    parsed.width = static_cast<quint32>(width);
    parsed.height = static_cast<quint32>(height);
    if (!validIncomingHandoff(parsed, error)) {
        return false;
    }
    *output = std::move(parsed);
    return true;
}

struct ValidatedStagedPng final
{
    QByteArray bytes;
    QImage image;
};

bool isPrivateStagedPath(
    const SstvStorageLayout& layout,
    const sharing::SstvValidatedIncomingHandoff& handoff,
    bool requireFile,
    QString* error)
{
    const QString rootPath = layout.rootPath();
    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir() || rootInfo.isSymLink()
        || rootInfo.canonicalFilePath().isEmpty()) {
        return fail(error, QStringLiteral("SSTV storage root is unsafe"));
    }
    QString directory = rootPath;
    const QStringList components {QStringLiteral("sharing"),
                                  QStringLiteral("downloads"),
                                  QStringLiteral("validated")};
    for (const QString& component : components) {
        directory = QDir(directory).absoluteFilePath(component);
        const QFileInfo info(directory);
        if (!info.exists() || !info.isDir() || info.isSymLink()
            || info.canonicalFilePath().isEmpty()) {
            return fail(error, QStringLiteral(
                "validated incoming staging directory is unsafe"));
        }
    }
    const QFileInfo validatedRoot(directory);
    const auto permissions = validatedRoot.permissions();
    const auto requiredDirectoryPermissions = QFileDevice::ReadOwner
        | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
    const auto forbiddenPermissions = QFileDevice::ReadGroup
        | QFileDevice::WriteGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::WriteOther
        | QFileDevice::ExeOther;
    if ((permissions & requiredDirectoryPermissions)
            != requiredDirectoryPermissions
        || (permissions & forbiddenPermissions)) {
        return fail(error, QStringLiteral(
            "validated incoming staging directory is not private"));
    }
    const QString canonicalRoot = QDir::cleanPath(
        validatedRoot.canonicalFilePath());
    const QString canonicalStorage = QDir::cleanPath(
        rootInfo.canonicalFilePath());
    const QString rootRelative = QDir(canonicalStorage).relativeFilePath(
        canonicalRoot);
    if (rootRelative == QStringLiteral("..")
        || rootRelative.startsWith(QStringLiteral("../"))
        || QDir::isAbsolutePath(rootRelative)) {
        return fail(error, QStringLiteral(
            "validated incoming staging directory escapes storage"));
    }
    const QString expectedPath = QDir(canonicalRoot).absoluteFilePath(
        handoff.transferId + QStringLiteral(".png"));
    if (handoff.stagedCanonicalPath != QDir::cleanPath(expectedPath)) {
        return fail(error, QStringLiteral(
            "validated incoming staging path is not canonical"));
    }
    const QFileInfo fileInfo(handoff.stagedCanonicalPath);
    if (!fileInfo.exists()) {
        return !requireFile || fail(error, QStringLiteral(
            "validated incoming staging PNG is missing"));
    }
    if (!fileInfo.isFile() || fileInfo.isSymLink()
        || QDir::cleanPath(fileInfo.canonicalFilePath())
            != handoff.stagedCanonicalPath
        || QDir::cleanPath(fileInfo.absolutePath()) != canonicalRoot) {
        return fail(error, QStringLiteral(
            "validated incoming staging PNG is unsafe"));
    }
    const auto filePermissions = fileInfo.permissions();
    const auto requiredFilePermissions = QFileDevice::ReadOwner
        | QFileDevice::WriteOwner;
    if ((filePermissions & requiredFilePermissions) != requiredFilePermissions
        || (filePermissions & (forbiddenPermissions | QFileDevice::ExeOwner))) {
        return fail(error, QStringLiteral(
            "validated incoming staging PNG is not private"));
    }
    return true;
}

bool readAndValidateStagedPng(
    const SstvStorageLayout& layout,
    const SstvStorageLimits& storageLimits,
    const sharing::SstvValidatedIncomingHandoff& handoff,
    ValidatedStagedPng* output,
    QString* error)
{
    if (!output || !isPrivateStagedPath(layout, handoff, true, error)) {
        return false;
    }
    const QFileInfo before(handoff.stagedCanonicalPath);
    const quint64 maximumBytes = std::min(
        kMaximumIncomingBytes,
        static_cast<quint64>(storageLimits.maximumPngBytes));
    if (before.size() <= 0
        || static_cast<quint64>(before.size()) != handoff.stagedByteSize
        || handoff.stagedByteSize > maximumBytes) {
        return fail(error, QStringLiteral(
            "validated incoming staging PNG size changed"));
    }
    QFile file(handoff.stagedCanonicalPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(error, QStringLiteral(
            "validated incoming staging PNG cannot be opened"));
    }
    const QByteArray bytes = file.read(
        static_cast<qint64>(maximumBytes) + 1);
    if (file.error() != QFileDevice::NoError
        || static_cast<quint64>(bytes.size()) != handoff.stagedByteSize
        || !file.atEnd()) {
        return fail(error, QStringLiteral(
            "validated incoming staging PNG cannot be read safely"));
    }
    file.close();
    static const QByteArray pngMagic = QByteArray::fromHex(
        QByteArrayLiteral("89504e470d0a1a0a"));
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    if (!bytes.startsWith(pngMagic) || hash != handoff.stagedSha256) {
        return fail(error, QStringLiteral(
            "validated incoming staging PNG integrity changed"));
    }
    const QFileInfo after(handoff.stagedCanonicalPath);
    if (!after.exists() || after.isSymLink()
        || after.size() != before.size()
        || QDir::cleanPath(after.canonicalFilePath())
            != handoff.stagedCanonicalPath) {
        return fail(error, QStringLiteral(
            "validated incoming staging PNG identity changed"));
    }

    QBuffer input;
    input.setData(bytes);
    if (!input.open(QIODevice::ReadOnly)) {
        return fail(error, QStringLiteral(
            "validated incoming PNG decode source is unavailable"));
    }
    QImageReader reader(&input, QByteArrayLiteral("png"));
    reader.setAutoDetectImageFormat(false);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(false);
    const QSize dimensions = reader.size();
    const quint64 pixels = static_cast<quint64>(handoff.width)
        * static_cast<quint64>(handoff.height);
    const int allocationLimitMb = QImageReader::allocationLimit();
    const quint64 qtAllocationBytes = allocationLimitMb > 0
        ? static_cast<quint64>(allocationLimitMb) * 1024ULL * 1024ULL
        : 0U;
    const quint64 allocationBytes = std::min(
        {kMaximumIncomingDecodeBytes,
         static_cast<quint64>(storageLimits.maximumDecodedBytes),
         qtAllocationBytes});
    if (reader.format().toLower() != QByteArrayLiteral("png")
        || dimensions.width() != static_cast<int>(handoff.width)
        || dimensions.height() != static_cast<int>(handoff.height)
        || handoff.width > static_cast<quint32>(storageLimits.maximumWidth)
        || handoff.height > static_cast<quint32>(storageLimits.maximumHeight)
        || pixels > static_cast<quint64>(storageLimits.maximumPixels)
        || allocationBytes == 0U || pixels > allocationBytes / 8U
        || reader.supportsAnimation() || reader.imageCount() > 1
        || !reader.textKeys().isEmpty()) {
        return fail(error, QStringLiteral(
            "validated incoming PNG header exceeds storage limits"));
    }
    const QImage decoded = reader.read();
    if (decoded.isNull() || decoded.size() != dimensions
        || decoded.sizeInBytes() <= 0
        || static_cast<quint64>(decoded.sizeInBytes()) > allocationBytes) {
        return fail(error, QStringLiteral(
            "validated incoming PNG cannot be fully decoded"));
    }
    QImage sanitized = decoded.convertToFormat(QImage::Format_RGB888);
    if (sanitized.isNull() || sanitized.size() != dimensions
        || sanitized.sizeInBytes() <= 0
        || static_cast<quint64>(sanitized.sizeInBytes()) > allocationBytes) {
        return fail(error, QStringLiteral(
            "validated incoming PNG pixels exceed storage limits"));
    }
    output->bytes = bytes;
    output->image = std::move(sanitized);
    return true;
}

SstvIncomingImportResult incomingFailure(
    const QString& transferId,
    SstvIncomingImportFailure failure,
    bool retryable,
    const QString& error,
    SstvImageRecord record = {})
{
    SstvIncomingImportResult result;
    result.transferId = transferId;
    result.failure = failure;
    result.retryable = retryable;
    result.record = std::move(record);
    result.error = error.left(512);
    return result;
}

SstvIncomingImportResult incomingSuccess(const SstvImageRecord& record,
                                         bool idempotent)
{
    SstvIncomingImportResult result;
    result.ok = true;
    result.idempotent = idempotent;
    result.transferId = record.id;
    result.failure = SstvIncomingImportFailure::None;
    result.record = record;
    return result;
}

bool recordMatchesIncoming(
    const SstvImageRecord& record,
    const sharing::SstvValidatedIncomingHandoff& handoff)
{
    return record.id == handoff.transferId
        && record.category == SstvImageCategory::Imported
        && record.remote
        && record.source == QStringLiteral("remote-sharing")
        && record.remoteProvider == handoff.providerId
        && record.remoteObjectId == handoff.incomingId
        && record.mode == handoff.sstvMode
        && record.capturedAtUtc == handoff.receivedUtc.toUTC()
        && record.eventAtUtc == handoff.receivedUtc.toUTC()
        && record.expiresAtUtc == handoff.expiresUtc.toUTC()
        && record.mimeType == QStringLiteral("image/png")
        && record.sha256
            == QByteArray::fromHex(handoff.stagedSha256.toLatin1())
        && record.fileSizeBytes
            == static_cast<qint64>(handoff.stagedByteSize)
        && record.width == static_cast<int>(handoff.width)
        && record.height == static_cast<int>(handoff.height)
        && record.originalWidth == static_cast<int>(handoff.width)
        && record.originalHeight == static_cast<int>(handoff.height);
}

} // namespace

bool SstvRetentionSettings::validate(QString* error) const
{
    if (maximumAgeDays < 0 || maximumAgeDays > 36'500) {
        return fail(error, QStringLiteral(
            "retention maximum age must be between 0 and 36500 days"));
    }
    const auto validQuota = [](qint64 value) {
        return value >= 0 && value <= kMaximumQuotaBytes;
    };
    if (!validQuota(imageQuotaBytes)
        || !validQuota(thumbnailQuotaBytes)
        || !validQuota(rawAudioQuotaBytes)) {
        return fail(error, QStringLiteral(
            "retention quota exceeds its fail-closed bound"));
    }
    if (sharedPolicy != SstvSharedRetentionPolicy::Protect
        && sharedPolicy != SstvSharedRetentionPolicy::AllowUploaded) {
        return fail(error, QStringLiteral("invalid shared-file retention policy"));
    }
    if (maximumDeletesPerRun <= 0 || maximumDeletesPerRun > 500) {
        return fail(error, QStringLiteral(
            "retention delete batch must be between 1 and 500"));
    }
    return true;
}

QVariantMap SstvRetentionSettings::toVariantMap() const
{
    return {
        {QStringLiteral("automaticEnabled"), automaticEnabled},
        {QStringLiteral("maximumAgeDays"), maximumAgeDays},
        {QStringLiteral("imageQuotaBytes"), imageQuotaBytes},
        {QStringLiteral("thumbnailQuotaBytes"), thumbnailQuotaBytes},
        {QStringLiteral("rawAudioQuotaBytes"), rawAudioQuotaBytes},
        {QStringLiteral("sharedPolicy"), static_cast<int>(sharedPolicy)},
        {QStringLiteral("maximumDeletesPerRun"), maximumDeletesPerRun},
    };
}

bool SstvRetentionSettings::fromVariantMap(const QVariantMap& values,
                                           SstvRetentionSettings* settings,
                                           QString* error)
{
    if (!settings) {
        return fail(error, QStringLiteral("retention settings output is null"));
    }
    static const QSet<QString> requiredKeys {
        QStringLiteral("automaticEnabled"),
        QStringLiteral("maximumAgeDays"),
        QStringLiteral("imageQuotaBytes"),
        QStringLiteral("thumbnailQuotaBytes"),
        QStringLiteral("rawAudioQuotaBytes"),
        QStringLiteral("sharedPolicy"),
        QStringLiteral("maximumDeletesPerRun"),
    };
    const QSet<QString> actualKeys(values.keyBegin(), values.keyEnd());
    if (actualKeys != requiredKeys) {
        return fail(error, QStringLiteral(
            "retention settings require the exact documented fields"));
    }
    const QVariant automatic = values.value(QStringLiteral("automaticEnabled"));
    if (automatic.metaType().id() != QMetaType::Bool) {
        return fail(error, QStringLiteral("automaticEnabled must be boolean"));
    }
    const auto integer = [&values, error](const QString& key,
                                          qint64 minimum,
                                          qint64 maximum,
                                          qint64* output) {
        bool ok = false;
        const qint64 value = values.value(key).toLongLong(&ok);
        if (!ok || value < minimum || value > maximum) {
            return fail(error, QStringLiteral("invalid retention field %1")
                                   .arg(key));
        }
        *output = value;
        return true;
    };
    qint64 maximumAge = 0;
    qint64 imageQuota = 0;
    qint64 thumbnailQuota = 0;
    qint64 rawAudioQuota = 0;
    qint64 sharedPolicyValue = 0;
    qint64 maximumDeletes = 0;
    if (!integer(QStringLiteral("maximumAgeDays"), 0, 36'500, &maximumAge)
        || !integer(QStringLiteral("imageQuotaBytes"), 0,
                    kMaximumQuotaBytes, &imageQuota)
        || !integer(QStringLiteral("thumbnailQuotaBytes"), 0,
                    kMaximumQuotaBytes, &thumbnailQuota)
        || !integer(QStringLiteral("rawAudioQuotaBytes"), 0,
                    kMaximumQuotaBytes, &rawAudioQuota)
        || !integer(QStringLiteral("sharedPolicy"), 0, 1,
                    &sharedPolicyValue)
        || !integer(QStringLiteral("maximumDeletesPerRun"), 1, 500,
                    &maximumDeletes)) {
        return false;
    }
    SstvRetentionSettings parsed;
    parsed.automaticEnabled = automatic.toBool();
    parsed.maximumAgeDays = static_cast<int>(maximumAge);
    parsed.imageQuotaBytes = imageQuota;
    parsed.thumbnailQuotaBytes = thumbnailQuota;
    parsed.rawAudioQuotaBytes = rawAudioQuota;
    parsed.sharedPolicy = static_cast<SstvSharedRetentionPolicy>(
        sharedPolicyValue);
    parsed.maximumDeletesPerRun = static_cast<int>(maximumDeletes);
    if (!parsed.validate(error)) {
        return false;
    }
    *settings = parsed;
    return true;
}

QVariantMap SstvQuotaSummary::toVariantMap() const
{
    return {
        {QStringLiteral("imageBytes"), imageBytes},
        {QStringLiteral("thumbnailBytes"), thumbnailBytes},
        {QStringLiteral("rawAudioBytes"), rawAudioBytes},
        {QStringLiteral("metadataBytes"), metadataBytes},
        {QStringLiteral("recordCount"), recordCount},
        {QStringLiteral("missingFileCount"), missingFileCount},
        {QStringLiteral("unsafePathCount"), unsafePathCount},
        {QStringLiteral("complete"), complete},
    };
}

QVariantMap SstvRetentionPlan::toVariantMap() const
{
    return {
        {QStringLiteral("token"), token},
        {QStringLiteral("createdAtUtc"), createdAtUtc},
        {QStringLiteral("settings"), settings.toVariantMap()},
        {QStringLiteral("recordIds"), recordIds},
        {QStringLiteral("recordCount"), recordIds.size()},
        {QStringLiteral("imageBytes"), imageBytes},
        {QStringLiteral("thumbnailBytes"), thumbnailBytes},
        {QStringLiteral("rawAudioBytes"), rawAudioBytes},
        {QStringLiteral("protectedFavoriteCount"), protectedFavoriteCount},
        {QStringLiteral("protectedQsoCount"), protectedQsoCount},
        {QStringLiteral("protectedSharedCount"), protectedSharedCount},
        {QStringLiteral("protectedUnsafeCount"), protectedUnsafeCount},
        {QStringLiteral("targetsSatisfied"), targetsSatisfied},
        {QStringLiteral("confirmationPhrase"), confirmationPhrase},
        {QStringLiteral("warning"), warning},
    };
}

bool SstvImagePageRequest::validate(QString* error) const
{
    if (categoryFilter != 0
        && !isValidSstvImageCategory(
            static_cast<SstvImageCategory>(categoryFilter))) {
        return fail(error, QStringLiteral("invalid page category filter"));
    }
    bool modeHasControl = false;
    for (QChar character : modeFilter) {
        const ushort code = character.unicode();
        if (code < 0x20U || code == 0x7fU) {
            modeHasControl = true;
            break;
        }
    }
    if (modeFilter.size() > 64 || modeHasControl) {
        return fail(error, QStringLiteral("invalid page mode filter"));
    }
    if (limit <= 0 || limit > SstvStorageWorker::kMaximumPageSize) {
        return fail(error, QStringLiteral("page size must be between 1 and 200"));
    }
    if (hasCursor) {
        if (beforeCapturedAtMs <= 0 || !canonicalUuid(beforeId)) {
            return fail(error, QStringLiteral("invalid keyset page cursor"));
        }
    } else if (!beforeId.isEmpty()) {
        return fail(error, QStringLiteral("cursor id supplied without a cursor"));
    }
    return true;
}

QVariantMap SstvStoragePerformanceSnapshot::toVariantMap() const
{
    const auto number = [](quint64 value) {
        return QVariant::fromValue<qulonglong>(value);
    };
    return {
        {QStringLiteral("acceptingDatabaseOperations"),
         acceptingDatabaseOperations},
        {QStringLiteral("databaseQueueDepth"), number(databaseQueueDepth)},
        {QStringLiteral("peakDatabaseQueueDepth"),
         number(peakDatabaseQueueDepth)},
        {QStringLiteral("databaseOperationsQueued"),
         number(databaseOperationsQueued)},
        {QStringLiteral("databaseOperationsDispatched"),
         number(databaseOperationsDispatched)},
        {QStringLiteral("databaseOperationsCompleted"),
         number(databaseOperationsCompleted)},
        {QStringLiteral("databaseOperationsRejected"),
         number(databaseOperationsRejected)},
        {QStringLiteral("databaseOperationsCancelled"),
         number(databaseOperationsCancelled)},
        {QStringLiteral("databaseQueueFailures"),
         number(databaseQueueFailures)},
        {QStringLiteral("imageSaveAttempts"), number(imageSaveAttempts)},
        {QStringLiteral("imageSaveSuccesses"), number(imageSaveSuccesses)},
        {QStringLiteral("imageSaveFailures"), number(imageSaveFailures)},
        {QStringLiteral("lastImageSaveNanoseconds"),
         number(lastImageSaveNanoseconds)},
        {QStringLiteral("averageImageSaveNanoseconds"),
         number(averageImageSaveNanoseconds)},
        {QStringLiteral("maximumImageSaveNanoseconds"),
         number(maximumImageSaveNanoseconds)},
    };
}

void SstvStoragePerformanceCounters::saturatingAdd(
    quint64& value,
    quint64 increment) noexcept
{
    value = increment > std::numeric_limits<quint64>::max() - value
        ? std::numeric_limits<quint64>::max() : value + increment;
}

void SstvStoragePerformanceCounters::beginLifecycle()
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    saturatingAdd(m_lifecycleGeneration);
    m_snapshot.acceptingDatabaseOperations = true;
    m_snapshot.databaseQueueDepth = 0U;
}

void SstvStoragePerformanceCounters::endLifecycle()
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    const quint64 pending = m_snapshot.databaseQueueDepth;
    saturatingAdd(m_snapshot.databaseOperationsCancelled, pending);
    saturatingAdd(m_snapshot.databaseQueueFailures, pending);
    m_snapshot.databaseQueueDepth = 0U;
    m_snapshot.acceptingDatabaseOperations = false;
}

std::optional<quint64>
SstvStoragePerformanceCounters::tryQueueDatabaseOperation()
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_snapshot.acceptingDatabaseOperations
        || m_snapshot.databaseQueueDepth >= kMaximumDatabaseQueueDepth) {
        saturatingAdd(m_snapshot.databaseOperationsRejected);
        saturatingAdd(m_snapshot.databaseQueueFailures);
        return std::nullopt;
    }
    saturatingAdd(m_snapshot.databaseQueueDepth);
    m_snapshot.peakDatabaseQueueDepth = std::max(
        m_snapshot.peakDatabaseQueueDepth, m_snapshot.databaseQueueDepth);
    saturatingAdd(m_snapshot.databaseOperationsQueued);
    return m_lifecycleGeneration;
}

bool SstvStoragePerformanceCounters::beginQueuedDatabaseOperation(
    quint64 lifecycleGeneration)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_snapshot.acceptingDatabaseOperations
        || lifecycleGeneration != m_lifecycleGeneration
        || m_snapshot.databaseQueueDepth == 0U) {
        return false;
    }
    --m_snapshot.databaseQueueDepth;
    saturatingAdd(m_snapshot.databaseOperationsDispatched);
    return true;
}

void SstvStoragePerformanceCounters::finishQueuedDatabaseOperation(
    bool dispatched)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (dispatched) {
        saturatingAdd(m_snapshot.databaseOperationsCompleted);
    } else {
        saturatingAdd(m_snapshot.databaseQueueFailures);
    }
}

void SstvStoragePerformanceCounters::cancelQueuedDatabaseOperation(
    quint64 lifecycleGeneration)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    // endLifecycle() accounts for every still-pending callable as one batch.
    // A concurrent invokeMethod() failure arriving afterwards must not count
    // that same accepted callable twice.
    if (lifecycleGeneration != m_lifecycleGeneration
        || (!m_snapshot.acceptingDatabaseOperations
            && m_snapshot.databaseQueueDepth == 0U)) {
        return;
    }
    if (m_snapshot.databaseQueueDepth != 0U) {
        --m_snapshot.databaseQueueDepth;
    }
    saturatingAdd(m_snapshot.databaseOperationsCancelled);
    saturatingAdd(m_snapshot.databaseQueueFailures);
}

void SstvStoragePerformanceCounters::recordImageSave(
    quint64 elapsedNanoseconds,
    bool success)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    saturatingAdd(m_snapshot.imageSaveAttempts);
    if (success) {
        saturatingAdd(m_snapshot.imageSaveSuccesses);
    } else {
        saturatingAdd(m_snapshot.imageSaveFailures);
    }
    m_snapshot.lastImageSaveNanoseconds = elapsedNanoseconds;
    m_snapshot.maximumImageSaveNanoseconds = std::max(
        m_snapshot.maximumImageSaveNanoseconds, elapsedNanoseconds);
    saturatingAdd(m_totalImageSaveNanoseconds, elapsedNanoseconds);
}

SstvStoragePerformanceSnapshot
SstvStoragePerformanceCounters::snapshot() const
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    SstvStoragePerformanceSnapshot result = m_snapshot;
    result.averageImageSaveNanoseconds = result.imageSaveAttempts == 0U
        ? 0U : m_totalImageSaveNanoseconds / result.imageSaveAttempts;
    return result;
}

SstvStorageWorker::SstvStorageWorker(QString databasePath,
                                     QString storageRoot,
                                     SstvStorageLimits limits,
                                     QObject* parent)
    : QObject(parent)
    , m_databasePath(canonicalAbsolutePath(std::move(databasePath)))
    , m_layout(std::move(storageRoot))
    , m_limits(limits)
    , m_connectionName(QStringLiteral("sstv_storage_%1")
          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    qRegisterMetaType<sharing::SstvValidatedIncomingHandoff>();
    qRegisterMetaType<SstvIncomingImportResult>();
    qRegisterMetaType<SstvRetentionSettings>();
    qRegisterMetaType<SstvQuotaSummary>();
    qRegisterMetaType<SstvRetentionPlan>();
    qRegisterMetaType<SstvStoragePerformanceSnapshot>();
}

SstvStorageWorker::~SstvStorageWorker()
{
    if (!m_initialized.load(std::memory_order_acquire)) {
        return;
    }
    if (QThread::currentThread() == thread()) {
        closeConnection();
        return;
    }
    qCritical("SstvStorageWorker destroyed before owner-thread shutdown");
}

quintptr SstvStorageWorker::currentThreadToken() noexcept
{
    return reinterpret_cast<quintptr>(QThread::currentThreadId());
}

bool SstvStorageWorker::requireOwnerThread(const QString& operation)
{
    if (QThread::currentThread() == thread()) {
        return true;
    }
    emit threadOwnershipViolation(operation);
    return false;
}

bool SstvStorageWorker::enqueueDatabaseOperation(DatabaseOperation operation)
{
    if (!operation) {
        return false;
    }
    const std::optional<quint64> lifecycleGeneration =
        m_performance.tryQueueDatabaseOperation();
    if (!lifecycleGeneration) {
        return false;
    }
    const bool queued = QMetaObject::invokeMethod(
        this,
        [this, operation = std::move(operation),
         lifecycleGeneration = *lifecycleGeneration]() mutable {
            if (!m_performance.beginQueuedDatabaseOperation(
                    lifecycleGeneration)) {
                return;
            }
            try {
                operation(*this);
                m_performance.finishQueuedDatabaseOperation();
            } catch (...) {
                m_performance.finishQueuedDatabaseOperation(false);
                qCritical("SSTV database operation raised an unexpected exception");
            }
        },
        Qt::QueuedConnection);
    if (!queued) {
        m_performance.cancelQueuedDatabaseOperation(*lifecycleGeneration);
    }
    return queued;
}

SstvStoragePerformanceSnapshot
SstvStorageWorker::performanceSnapshot() const
{
    return m_performance.snapshot();
}

void SstvStorageWorker::initialize()
{
    const quintptr token = currentThreadToken();
    if (!requireOwnerThread(QStringLiteral("initialize"))) {
        recordSstvDiagnosticEvent(
            sstvStorageLog(), QtWarningMsg,
            QStringLiteral("storage.initialize-rejected"),
            {{QStringLiteral("reasonCode"),
              QStringLiteral("wrong-thread")},
             {QStringLiteral("success"), false}});
        emit initialized(false, QStringLiteral("initialize called outside owner thread"),
                         0, token);
        return;
    }
    if (m_initialized.load(std::memory_order_acquire)) {
        emit initialized(true, {},
                         m_schemaVersion.load(std::memory_order_acquire), token);
        return;
    }

    QString error;
    if (!m_limits.validate(&error) || !m_layout.ensure(&error)
        || m_databasePath.isEmpty()
        || !m_layout.containsPath(m_databasePath, true, &error)) {
        if (error.isEmpty()) {
            error = QStringLiteral("invalid SSTV database path");
        }
        recordSstvDiagnosticEvent(
            sstvStorageLog(), QtWarningMsg,
            QStringLiteral("storage.initialize-failed"),
            {{QStringLiteral("reasonCode"),
              QStringLiteral("invalid-layout")},
             {QStringLiteral("success"), false}});
        emit initialized(false, error, 0, token);
        return;
    }
    if (!QSqlDatabase::drivers().contains(QStringLiteral("QSQLITE"))) {
        recordSstvDiagnosticEvent(
            sstvStorageLog(), QtCriticalMsg,
            QStringLiteral("storage.initialize-failed"),
            {{QStringLiteral("reasonCode"),
              QStringLiteral("qsqlite-unavailable")},
             {QStringLiteral("success"), false}});
        emit initialized(false, QStringLiteral("QSQLITE driver is unavailable"),
                         0, token);
        return;
    }
    if (QSqlDatabase::contains(m_connectionName)) {
        recordSstvDiagnosticEvent(
            sstvStorageLog(), QtCriticalMsg,
            QStringLiteral("storage.initialize-failed"),
            {{QStringLiteral("reasonCode"),
              QStringLiteral("connection-collision")},
             {QStringLiteral("success"), false}});
        emit initialized(false,
                         QStringLiteral("SSTV database connection name collision"),
                         0, token);
        return;
    }

    bool opened = false;
    bool migrated = false;
    int version = 0;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), m_connectionName);
        database.setDatabaseName(m_databasePath);
        if (!database.open()) {
            error = databaseFailure(QStringLiteral("open SSTV database"),
                                    database);
        } else {
            opened = true;
            executeSql(database, QStringLiteral("PRAGMA busy_timeout=5000"),
                       QStringLiteral("set busy timeout"), &error);
            if (error.isEmpty()) {
                executeSql(database, QStringLiteral("PRAGMA foreign_keys=ON"),
                           QStringLiteral("enable foreign keys"), &error);
            }
            if (error.isEmpty()) {
                executeSql(database, QStringLiteral("PRAGMA synchronous=NORMAL"),
                           QStringLiteral("set synchronous mode"), &error);
            }

            int preflightVersion = 0;
            if (error.isEmpty()
                && currentUserVersion(database, &preflightVersion, &error)) {
                version = preflightVersion;
                if (preflightVersion > kCurrentSchemaVersion) {
                    error = QStringLiteral("database schema version %1 is newer than supported %2")
                        .arg(preflightVersion)
                        .arg(kCurrentSchemaVersion);
                }
            }
            if (error.isEmpty()) {
                QSqlQuery journal(database);
                if (!journal.exec(QStringLiteral("PRAGMA journal_mode=WAL"))
                    || !journal.next()
                    || journal.value(0).toString().compare(
                           QStringLiteral("wal"), Qt::CaseInsensitive) != 0) {
                    error = queryFailure(QStringLiteral("enable WAL journal"),
                                         journal);
                }
            }
            if (error.isEmpty()) {
                migrated = migrateSchema(database, &version, &error);
            }
            if (error.isEmpty() && migrated) {
                migrated = recoverDeletionStaging(database, &error);
            }
            if (!error.isEmpty()) {
                database.close();
            }
        }
        database = QSqlDatabase();
    }

    if (!opened || !migrated) {
        if (QSqlDatabase::contains(m_connectionName)) {
            QSqlDatabase::removeDatabase(m_connectionName);
        }
        recordSstvDiagnosticEvent(
            sstvStorageLog(), QtCriticalMsg,
            QStringLiteral("storage.initialize-failed"),
            {{QStringLiteral("reasonCode"),
              QStringLiteral("database-failed")},
             {QStringLiteral("schemaVersion"), version},
             {QStringLiteral("success"), false}});
        emit initialized(false,
                         error.isEmpty()
                             ? QStringLiteral("SSTV database initialization failed")
                             : error,
                         version, token);
        return;
    }
    m_schemaVersion.store(version, std::memory_order_release);
    m_initialized.store(true, std::memory_order_release);
    m_performance.beginLifecycle();
    recordSstvDiagnosticEvent(
        sstvStorageLog(), QtInfoMsg,
        QStringLiteral("storage.initialized"),
        {{QStringLiteral("schemaVersion"), version},
         {QStringLiteral("success"), true}});
    emit initialized(true, {}, version, token);
}

bool SstvStorageWorker::recoverDeletionStaging(QSqlDatabase& database,
                                                QString* error)
{
    constexpr qint64 kMaximumJournalBytes = 1024 * 1024;
    constexpr qsizetype kMaximumStageDirectories = 512;
    constexpr qsizetype kMaximumJournalFiles = 2'000;
    const QString stagingRoot = QDir(m_layout.rootPath()).absoluteFilePath(
        QStringLiteral(".delete-staging"));
    const QFileInfo rootInfo(stagingRoot);
    if (!rootInfo.exists()) {
        return true;
    }
    if (!rootInfo.isDir() || rootInfo.isSymLink()
        || !m_layout.containsPath(stagingRoot, true, error)) {
        return fail(error, QStringLiteral(
            "SSTV deletion recovery root is unsafe"));
    }
    QDir root(stagingRoot);
    const QFileInfoList unexpected = root.entryInfoList(
        QDir::Files | QDir::System | QDir::NoDotAndDotDot,
        QDir::Name);
    if (!unexpected.isEmpty()) {
        return fail(error, QStringLiteral(
            "SSTV deletion recovery root contains unexpected entries"));
    }
    const QFileInfoList stages = root.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
        QDir::Name);
    const QFileInfoList allRootEntries = root.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
    if (allRootEntries.size() != stages.size()) {
        return fail(error, QStringLiteral(
            "SSTV deletion recovery root contains unsafe entries"));
    }
    if (stages.size() > kMaximumStageDirectories) {
        return fail(error, QStringLiteral(
            "too many SSTV deletion recovery directories"));
    }

    struct JournalFile final
    {
        QString original;
        QString staged;
    };
    for (const QFileInfo& stageInfo : stages) {
        if (!stageInfo.isDir() || stageInfo.isSymLink()
            || stageInfo.fileName().size() > 128
            || !m_layout.containsPath(stageInfo.absoluteFilePath(), true,
                                      error)) {
            return fail(error, QStringLiteral(
                "an SSTV deletion recovery directory is unsafe"));
        }
        QDir stage(stageInfo.absoluteFilePath());
        const QString journalPath = stage.absoluteFilePath(
            QStringLiteral("journal.json"));
        const QFileInfo journalInfo(journalPath);
        const QFileInfoList stageEntries = stage.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
        if (!journalInfo.exists()) {
            if (stageEntries.isEmpty() && root.rmdir(stageInfo.fileName())) {
                continue;
            }
            return fail(error, QStringLiteral(
                "an SSTV deletion recovery directory has no journal"));
        }
        if (!journalInfo.isFile() || journalInfo.isSymLink()
            || journalInfo.size() <= 0
            || journalInfo.size() > kMaximumJournalBytes) {
            return fail(error, QStringLiteral(
                "an SSTV deletion recovery journal is unsafe"));
        }
        QFile journal(journalPath);
        if (!journal.open(QIODevice::ReadOnly)) {
            return fail(error, QStringLiteral(
                "could not open an SSTV deletion recovery journal"));
        }
        const QByteArray encoded = journal.read(kMaximumJournalBytes + 1);
        if (encoded.size() != journalInfo.size()) {
            return fail(error, QStringLiteral(
                "an SSTV deletion recovery journal changed while reading"));
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            encoded, &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
            return fail(error, QStringLiteral(
                "an SSTV deletion recovery journal is malformed"));
        }
        const QJsonObject object = document.object();
        static const QSet<QString> rootKeys {
            QStringLiteral("schemaVersion"), QStringLiteral("recordIds"),
            QStringLiteral("files")};
        QSet<QString> actualRootKeys;
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            actualRootKeys.insert(it.key());
        }
        const QJsonArray idArray = object.value(
            QStringLiteral("recordIds")).toArray();
        const QJsonArray fileArray = object.value(
            QStringLiteral("files")).toArray();
        if (actualRootKeys != rootKeys
            || object.value(QStringLiteral("schemaVersion")).toInt(-1) != 1
            || !object.value(QStringLiteral("recordIds")).isArray()
            || idArray.isEmpty() || idArray.size() > 500
            || !object.value(QStringLiteral("files")).isArray()
            || fileArray.isEmpty()
            || fileArray.size() > kMaximumJournalFiles) {
            return fail(error, QStringLiteral(
                "an SSTV deletion recovery journal has invalid bounds"));
        }

        QStringList ids;
        QSet<QString> uniqueIds;
        for (const QJsonValue& value : idArray) {
            const QString id = value.toString();
            if (!value.isString() || !canonicalUuid(id)
                || uniqueIds.contains(id)) {
                return fail(error, QStringLiteral(
                    "an SSTV deletion recovery journal has invalid UUIDs"));
            }
            uniqueIds.insert(id);
            ids.append(id);
        }

        QVector<JournalFile> files;
        files.reserve(fileArray.size());
        QSet<QString> uniqueOriginals;
        QSet<QString> uniqueStaged;
        for (const QJsonValue& value : fileArray) {
            if (!value.isObject()) {
                return fail(error, QStringLiteral(
                    "an SSTV deletion recovery file entry is malformed"));
            }
            const QJsonObject entry = value.toObject();
            if (entry.size() != 2
                || !entry.value(QStringLiteral("original")).isString()
                || !entry.value(QStringLiteral("staged")).isString()) {
                return fail(error, QStringLiteral(
                    "an SSTV deletion recovery file entry is invalid"));
            }
            const QString original = entry.value(
                QStringLiteral("original")).toString();
            const QString stagedName = entry.value(
                QStringLiteral("staged")).toString();
            QString containmentError;
            if (original.isEmpty() || original.size() > 4'096
                || !QFileInfo(original).isAbsolute()
                || QDir::cleanPath(original) != original
                || !m_layout.containsPath(original, true, &containmentError)
                || stagedName.isEmpty() || stagedName.size() > 255
                || QFileInfo(stagedName).fileName() != stagedName
                || stagedName == QStringLiteral("journal.json")
                || uniqueOriginals.contains(original)
                || uniqueStaged.contains(stagedName)) {
                return fail(error, QStringLiteral(
                    "an SSTV deletion recovery path is unsafe"));
            }
            const QString staged = stage.absoluteFilePath(stagedName);
            if (QDir::cleanPath(staged) != staged
                || !m_layout.containsPath(staged, true, &containmentError)) {
                return fail(error, QStringLiteral(
                    "an SSTV deletion recovery staged path is unsafe"));
            }
            uniqueOriginals.insert(original);
            uniqueStaged.insert(stagedName);
            files.append({original, staged});
        }
        if (stageEntries.size() > files.size() + 1) {
            return fail(error, QStringLiteral(
                "an SSTV deletion recovery directory has unexpected files"));
        }
        for (const QFileInfo& entry : stageEntries) {
            if (entry.fileName() == QStringLiteral("journal.json")) {
                continue;
            }
            if (!uniqueStaged.contains(entry.fileName())
                || !entry.isFile() || entry.isSymLink()) {
                return fail(error, QStringLiteral(
                    "an SSTV deletion recovery entry is not journalled"));
            }
        }

        qsizetype existingRecords = 0;
        QSet<QString> referencedPaths;
        QSqlQuery record(database);
        if (!record.prepare(QStringLiteral(
                "SELECT image_path,metadata_path,thumbnail_path,raw_audio_path "
                "FROM sstv_images WHERE id=:id"))) {
            return fail(error, queryFailure(QStringLiteral(
                "prepare SSTV deletion recovery lookup"), record));
        }
        for (const QString& id : std::as_const(ids)) {
            record.bindValue(QStringLiteral(":id"), id);
            if (!record.exec()) {
                return fail(error, queryFailure(QStringLiteral(
                    "read SSTV deletion recovery record"), record));
            }
            if (record.next()) {
                ++existingRecords;
                for (int column = 0; column < 4; ++column) {
                    const QString path = record.value(column).toString();
                    if (!path.isEmpty()) {
                        referencedPaths.insert(path);
                    }
                }
            }
            record.finish();
        }
        if (existingRecords != 0 && existingRecords != ids.size()) {
            return fail(error, QStringLiteral(
                "SSTV deletion recovery found a partial database commit"));
        }
        if (existingRecords == ids.size()) {
            for (const JournalFile& file : std::as_const(files)) {
                if (!referencedPaths.contains(file.original)) {
                    return fail(error, QStringLiteral(
                        "SSTV deletion recovery path does not match its records"));
                }
                const QFileInfo stagedInfo(file.staged);
                const QFileInfo originalInfo(file.original);
                if (stagedInfo.exists()) {
                    if (!stagedInfo.isFile() || stagedInfo.isSymLink()
                        || originalInfo.exists()
                        || !QFile::rename(file.staged, file.original)) {
                        return fail(error, QStringLiteral(
                            "could not restore an interrupted SSTV deletion"));
                    }
                } else if (!originalInfo.exists() || !originalInfo.isFile()
                           || originalInfo.isSymLink()) {
                    return fail(error, QStringLiteral(
                        "an interrupted SSTV deletion lost a staged file"));
                }
            }
        } else {
            for (const JournalFile& file : std::as_const(files)) {
                const QFileInfo stagedInfo(file.staged);
                if (stagedInfo.exists()
                    && (!stagedInfo.isFile() || stagedInfo.isSymLink()
                        || !QFile::remove(file.staged))) {
                    return fail(error, QStringLiteral(
                        "could not finish an interrupted SSTV deletion"));
                }
            }
        }
        if (!QFile::remove(journalPath)
            || !root.rmdir(stageInfo.fileName())) {
            return fail(error, QStringLiteral(
                "could not finish SSTV deletion recovery cleanup"));
        }
    }
    return true;
}

bool SstvStorageWorker::migrateSchema(QSqlDatabase& database,
                                      int* resultingVersion,
                                      QString* error)
{
    int version = 0;
    if (!currentUserVersion(database, &version, error)) {
        return false;
    }
    if (version > kCurrentSchemaVersion) {
        if (resultingVersion) {
            *resultingVersion = version;
        }
        return fail(error,
                    QStringLiteral("database schema version is newer than this build"));
    }
    if (version == kCurrentSchemaVersion) {
        if (!validateSchema(database, error)) {
            return false;
        }
        if (resultingVersion) {
            *resultingVersion = version;
        }
        return true;
    }
    if (!database.transaction()) {
        return fail(error, databaseFailure(QStringLiteral("begin schema migration"),
                                           database));
    }
    auto rollbackFailure = [&](const QString& detail) {
        database.rollback();
        return fail(error, detail);
    };

    if (version == 0) {
        const QString createV1 = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sstv_images ("
            "id TEXT PRIMARY KEY NOT NULL CHECK(length(id)=36),"
            "category INTEGER NOT NULL CHECK(category BETWEEN 1 AND 4),"
            "captured_at_ms INTEGER NOT NULL CHECK(captured_at_ms>0),"
            "created_at_ms INTEGER NOT NULL CHECK(created_at_ms>0),"
            "mode TEXT NOT NULL CHECK(length(mode) BETWEEN 1 AND 64),"
            "vis_code INTEGER NOT NULL DEFAULT -1 CHECK(vis_code BETWEEN -1 AND 255),"
            "remote_callsign TEXT NOT NULL DEFAULT '' CHECK(length(remote_callsign)<=64),"
            "local_callsign TEXT NOT NULL DEFAULT '' CHECK(length(local_callsign)<=64),"
            "source TEXT NOT NULL CHECK(length(source) BETWEEN 1 AND 128),"
            "frequency_hz INTEGER NOT NULL DEFAULT 0 CHECK(frequency_hz>=0),"
            "complete INTEGER NOT NULL CHECK(complete IN (0,1)),"
            "image_path TEXT NOT NULL UNIQUE,"
            "metadata_path TEXT NOT NULL UNIQUE,"
            "sha256_hex TEXT NOT NULL CHECK(length(sha256_hex)=64),"
            "file_size_bytes INTEGER NOT NULL CHECK(file_size_bytes>0),"
            "width INTEGER NOT NULL CHECK(width>0),"
            "height INTEGER NOT NULL CHECK(height>0)"
            ")");
        if (!executeSql(database, createV1,
                        QStringLiteral("create SSTV schema v1"), error)
            || !executeSql(database, QStringLiteral("PRAGMA user_version=1"),
                           QStringLiteral("mark SSTV schema v1"), error)) {
            const QString detail = error ? *error
                                         : QStringLiteral("schema v1 migration failed");
            return rollbackFailure(detail);
        }
        version = 1;
    }
    if (version == 1) {
        bool hasUpdated = false;
        bool hasNote = false;
        if (!tableHasColumn(database, QStringLiteral("sstv_images"),
                            QStringLiteral("updated_at_ms"), &hasUpdated, error)
            || !tableHasColumn(database, QStringLiteral("sstv_images"),
                               QStringLiteral("note"), &hasNote, error)) {
            const QString detail = error ? *error
                                         : QStringLiteral("schema v2 inspection failed");
            return rollbackFailure(detail);
        }
        if (!hasUpdated
            && !executeSql(database,
                           QStringLiteral(
                               "ALTER TABLE sstv_images ADD COLUMN "
                               "updated_at_ms INTEGER NOT NULL DEFAULT 0"),
                           QStringLiteral("add updated timestamp"), error)) {
            return rollbackFailure(error ? *error
                                         : QStringLiteral("schema v2 migration failed"));
        }
        if (!hasNote
            && !executeSql(database,
                           QStringLiteral(
                               "ALTER TABLE sstv_images ADD COLUMN "
                               "note TEXT NOT NULL DEFAULT ''"),
                           QStringLiteral("add image note"), error)) {
            return rollbackFailure(error ? *error
                                         : QStringLiteral("schema v2 migration failed"));
        }
        if (!executeSql(database,
                        QStringLiteral(
                            "UPDATE sstv_images SET updated_at_ms=created_at_ms "
                            "WHERE updated_at_ms=0"),
                        QStringLiteral("backfill updated timestamp"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "CREATE INDEX IF NOT EXISTS idx_sstv_images_page "
                               "ON sstv_images(captured_at_ms DESC, id DESC)"),
                           QStringLiteral("create gallery page index"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "CREATE INDEX IF NOT EXISTS idx_sstv_images_category_page "
                               "ON sstv_images(category, captured_at_ms DESC, id DESC)"),
                           QStringLiteral("create category page index"), error)
            || !executeSql(database, QStringLiteral("PRAGMA user_version=2"),
                           QStringLiteral("mark SSTV schema v2"), error)) {
            return rollbackFailure(error ? *error
                                         : QStringLiteral("schema v2 migration failed"));
        }
        version = 2;
    }
    if (version == 2) {
        bool hasRemote = false;
        bool hasUploadState = false;
        bool hasTagsJson = false;
        if (!tableHasColumn(database, QStringLiteral("sstv_images"),
                            QStringLiteral("remote"), &hasRemote, error)
            || !tableHasColumn(database, QStringLiteral("sstv_images"),
                               QStringLiteral("upload_state"),
                               &hasUploadState, error)
            || !tableHasColumn(database, QStringLiteral("sstv_images"),
                               QStringLiteral("tags_json"), &hasTagsJson,
                               error)) {
            return rollbackFailure(error ? *error
                                         : QStringLiteral("schema v3 inspection failed"));
        }
        if (!hasRemote
            && !executeSql(database,
                           QStringLiteral(
                               "ALTER TABLE sstv_images ADD COLUMN "
                               "remote INTEGER NOT NULL DEFAULT 0 "
                               "CHECK(remote IN (0,1))"),
                           QStringLiteral("add remote origin"), error)) {
            return rollbackFailure(error ? *error
                                         : QStringLiteral("schema v3 migration failed"));
        }
        if (!hasUploadState
            && !executeSql(database,
                           QStringLiteral(
                               "ALTER TABLE sstv_images ADD COLUMN "
                               "upload_state INTEGER NOT NULL DEFAULT 0 "
                               "CHECK(upload_state BETWEEN 0 AND 4)"),
                           QStringLiteral("add upload state"), error)) {
            return rollbackFailure(error ? *error
                                         : QStringLiteral("schema v3 migration failed"));
        }
        if (!hasTagsJson
            && !executeSql(database,
                           QStringLiteral(
                               "ALTER TABLE sstv_images ADD COLUMN "
                               "tags_json TEXT NOT NULL DEFAULT '[]'"),
                           QStringLiteral("add tag projection"), error)) {
            return rollbackFailure(error ? *error
                                         : QStringLiteral("schema v3 migration failed"));
        }
        const QString createTags = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sstv_image_tags ("
            "image_id TEXT NOT NULL REFERENCES sstv_images(id) ON DELETE CASCADE,"
            "tag TEXT NOT NULL CHECK(length(tag) BETWEEN 1 AND 64),"
            "tag_folded TEXT NOT NULL CHECK(length(tag_folded) BETWEEN 1 AND 64),"
            "PRIMARY KEY(image_id,tag_folded))");
        if (!executeSql(database, createTags,
                        QStringLiteral("create normalized SSTV tags"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "CREATE INDEX IF NOT EXISTS idx_sstv_tags_lookup "
                               "ON sstv_image_tags(tag_folded,image_id)"),
                           QStringLiteral("create tag lookup index"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "CREATE INDEX IF NOT EXISTS idx_sstv_images_updated "
                               "ON sstv_images(updated_at_ms DESC,id DESC)"),
                           QStringLiteral("create updated gallery index"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "CREATE INDEX IF NOT EXISTS idx_sstv_images_filters "
                               "ON sstv_images(remote,upload_state,complete,category)"),
                           QStringLiteral("create gallery filter index"), error)
            || !executeSql(database, QStringLiteral("PRAGMA user_version=3"),
                           QStringLiteral("mark SSTV schema v3"), error)) {
            return rollbackFailure(error ? *error
                                         : QStringLiteral("schema v3 migration failed"));
        }
        version = 3;
    }
    if (version == 3) {
        // Each column probe makes the migration tolerant of a previously
        // committed partial schema while the enclosing transaction guarantees
        // that an ordinary interrupted startup leaves v3 untouched.
        const QList<QPair<QString, QString>> columns {
            {QStringLiteral("event_at_ms"),
             QStringLiteral("event_at_ms INTEGER NOT NULL DEFAULT 0 "
                            "CHECK(event_at_ms>=0)")},
            {QStringLiteral("thumbnail_path"),
             QStringLiteral("thumbnail_path TEXT NOT NULL DEFAULT '' "
                            "CHECK(length(thumbnail_path)<=4096)")},
            {QStringLiteral("mime_type"),
             QStringLiteral("mime_type TEXT NOT NULL DEFAULT 'image/png' "
                            "CHECK(mime_type='image/png')")},
            {QStringLiteral("original_width"),
             QStringLiteral("original_width INTEGER NOT NULL DEFAULT 0 "
                            "CHECK(original_width>=0)")},
            {QStringLiteral("original_height"),
             QStringLiteral("original_height INTEGER NOT NULL DEFAULT 0 "
                            "CHECK(original_height>=0)")},
            {QStringLiteral("digital"),
             QStringLiteral("digital INTEGER NOT NULL DEFAULT 0 "
                            "CHECK(digital IN (0,1))")},
            {QStringLiteral("vis_valid"),
             QStringLiteral("vis_valid INTEGER NOT NULL DEFAULT 0 "
                            "CHECK(vis_valid IN (0,1))")},
            {QStringLiteral("fsk_id"),
             QStringLiteral("fsk_id TEXT NOT NULL DEFAULT '' "
                            "CHECK(length(fsk_id)<=128)")},
            {QStringLiteral("local_grid"),
             QStringLiteral("local_grid TEXT NOT NULL DEFAULT '' "
                            "CHECK(length(local_grid)<=16)")},
            {QStringLiteral("remote_grid"),
             QStringLiteral("remote_grid TEXT NOT NULL DEFAULT '' "
                            "CHECK(length(remote_grid)<=16)")},
            {QStringLiteral("audio_frequency_hz"),
             QStringLiteral("audio_frequency_hz INTEGER NOT NULL DEFAULT 0 "
                            "CHECK(audio_frequency_hz BETWEEN -10000000 AND 10000000)")},
            {QStringLiteral("source_sample_rate_hz"),
             QStringLiteral("source_sample_rate_hz INTEGER NOT NULL DEFAULT 0 "
                            "CHECK(source_sample_rate_hz BETWEEN 0 AND 10000000)")},
            {QStringLiteral("completion_percent"),
             QStringLiteral("completion_percent INTEGER NOT NULL DEFAULT 0 "
                            "CHECK(completion_percent BETWEEN 0 AND 100)")},
            {QStringLiteral("quality_json"),
             QStringLiteral("quality_json TEXT NOT NULL DEFAULT '{}'")},
            {QStringLiteral("slant_correction_ppm"),
             QStringLiteral("slant_correction_ppm REAL NOT NULL DEFAULT 0 "
                            "CHECK(slant_correction_ppm BETWEEN -10000 AND 10000)")},
            {QStringLiteral("raw_audio_path"),
             QStringLiteral("raw_audio_path TEXT NOT NULL DEFAULT '' "
                            "CHECK(length(raw_audio_path)<=4096)")},
            {QStringLiteral("related_qso_id"),
             QStringLiteral("related_qso_id TEXT NOT NULL DEFAULT '' "
                            "CHECK(length(related_qso_id)<=256)")},
            {QStringLiteral("remote_provider"),
             QStringLiteral("remote_provider TEXT NOT NULL DEFAULT '' "
                            "CHECK(length(remote_provider)<=128)")},
            {QStringLiteral("remote_object_id"),
             QStringLiteral("remote_object_id TEXT NOT NULL DEFAULT '' "
                            "CHECK(length(remote_object_id)<=512)")},
            {QStringLiteral("expires_at_ms"),
             QStringLiteral("expires_at_ms INTEGER NOT NULL DEFAULT 0 "
                            "CHECK(expires_at_ms>=0)")},
            {QStringLiteral("privacy_flags"),
             QStringLiteral("privacy_flags INTEGER NOT NULL DEFAULT 0 "
                            "CHECK(privacy_flags BETWEEN 0 AND 4294967295)")}
        };
        for (const auto& column : columns) {
            bool present = false;
            if (!tableHasColumn(database, QStringLiteral("sstv_images"),
                                column.first, &present, error)) {
                return rollbackFailure(error ? *error
                    : QStringLiteral("schema v4 inspection failed"));
            }
            if (!present
                && !executeSql(database,
                               QStringLiteral("ALTER TABLE sstv_images ADD COLUMN ")
                                   + column.second,
                               QStringLiteral("add SSTV metadata column %1")
                                   .arg(column.first), error)) {
                return rollbackFailure(error ? *error
                    : QStringLiteral("schema v4 migration failed"));
            }
        }
        if (!executeSql(database,
                        QStringLiteral(
                            "UPDATE sstv_images SET event_at_ms=captured_at_ms "
                            "WHERE event_at_ms=0"),
                        QStringLiteral("backfill SSTV event timestamp"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "UPDATE sstv_images SET thumbnail_path="
                               "CASE WHEN lower(image_path) LIKE '%.png' "
                               "THEN substr(image_path,1,length(image_path)-4)"
                               "||'.thumb.png' ELSE image_path||'.thumb.png' END "
                               "WHERE thumbnail_path=''"),
                           QStringLiteral("backfill SSTV thumbnail path"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "UPDATE sstv_images SET original_width=width "
                               "WHERE original_width=0"),
                           QStringLiteral("backfill SSTV original width"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "UPDATE sstv_images SET original_height=height "
                               "WHERE original_height=0"),
                           QStringLiteral("backfill SSTV original height"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "UPDATE sstv_images SET vis_valid=1 "
                               "WHERE vis_code>=0 AND vis_valid=0"),
                           QStringLiteral("backfill SSTV VIS validity"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "UPDATE sstv_images SET completion_percent=100 "
                               "WHERE complete=1 AND completion_percent=0"),
                           QStringLiteral("backfill SSTV completion"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "CREATE TRIGGER IF NOT EXISTS "
                               "trg_sstv_images_v4_compat_insert "
                               "AFTER INSERT ON sstv_images "
                               "WHEN NEW.event_at_ms=0 OR NEW.thumbnail_path='' "
                               "OR NEW.original_width=0 OR NEW.original_height=0 "
                               "OR (NEW.complete=1 AND NEW.completion_percent=0) "
                               "BEGIN UPDATE sstv_images SET "
                               "event_at_ms=CASE WHEN event_at_ms=0 "
                               "THEN captured_at_ms ELSE event_at_ms END,"
                               "thumbnail_path=CASE WHEN thumbnail_path='' "
                               "THEN CASE WHEN lower(image_path) LIKE '%.png' "
                               "THEN substr(image_path,1,length(image_path)-4)"
                               "||'.thumb.png' ELSE image_path||'.thumb.png' END "
                               "ELSE thumbnail_path END,"
                               "original_width=CASE WHEN original_width=0 "
                               "THEN width ELSE original_width END,"
                               "original_height=CASE WHEN original_height=0 "
                               "THEN height ELSE original_height END,"
                               "completion_percent=CASE "
                               "WHEN complete=1 AND completion_percent=0 "
                               "THEN 100 ELSE completion_percent END "
                               "WHERE id=NEW.id; END"),
                           QStringLiteral("create SSTV v4 compatibility trigger"),
                           error)
            || !executeSql(database,
                           QStringLiteral(
                               "CREATE UNIQUE INDEX IF NOT EXISTS "
                               "idx_sstv_images_thumbnail_path "
                               "ON sstv_images(thumbnail_path)"),
                           QStringLiteral("create SSTV thumbnail path index"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "CREATE INDEX IF NOT EXISTS idx_sstv_images_event "
                               "ON sstv_images(event_at_ms DESC,id DESC)"),
                           QStringLiteral("create SSTV event index"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "CREATE INDEX IF NOT EXISTS idx_sstv_images_remote_object "
                               "ON sstv_images(remote_provider,remote_object_id)"),
                           QStringLiteral("create SSTV remote object index"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "CREATE INDEX IF NOT EXISTS idx_sstv_images_qso "
                               "ON sstv_images(related_qso_id,id)"),
                           QStringLiteral("create SSTV QSO index"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "CREATE INDEX IF NOT EXISTS idx_sstv_images_expiry "
                               "ON sstv_images(expires_at_ms,upload_state)"),
                           QStringLiteral("create SSTV expiry index"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "CREATE INDEX IF NOT EXISTS "
                               "idx_sstv_images_source_classification "
                               "ON sstv_images(source,digital,mode)"),
                           QStringLiteral("create SSTV source index"), error)
            || !executeSql(database, QStringLiteral("PRAGMA user_version=4"),
                           QStringLiteral("mark SSTV schema v4"), error)) {
            return rollbackFailure(error ? *error
                                         : QStringLiteral("schema v4 migration failed"));
        }
        version = 4;
    }
    if (version == 4) {
        bool hasFavorite = false;
        if (!tableHasColumn(database, QStringLiteral("sstv_images"),
                            QStringLiteral("favorite"), &hasFavorite, error)) {
            return rollbackFailure(error ? *error
                : QStringLiteral("schema v5 inspection failed"));
        }
        if (!hasFavorite
            && !executeSql(database,
                           QStringLiteral(
                               "ALTER TABLE sstv_images ADD COLUMN "
                               "favorite INTEGER NOT NULL DEFAULT 0 "
                               "CHECK(favorite IN (0,1))"),
                           QStringLiteral("add SSTV favourite metadata"),
                           error)) {
            return rollbackFailure(error ? *error
                : QStringLiteral("schema v5 favourite migration failed"));
        }
        const QString retentionTable = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sstv_retention_settings ("
            "id INTEGER PRIMARY KEY NOT NULL CHECK(id=1),"
            "automatic_enabled INTEGER NOT NULL DEFAULT 0 "
            "CHECK(automatic_enabled IN (0,1)),"
            "maximum_age_days INTEGER NOT NULL DEFAULT 0 "
            "CHECK(maximum_age_days BETWEEN 0 AND 36500),"
            "image_quota_bytes INTEGER NOT NULL DEFAULT 0 "
            "CHECK(image_quota_bytes BETWEEN 0 AND 17592186044416),"
            "thumbnail_quota_bytes INTEGER NOT NULL DEFAULT 0 "
            "CHECK(thumbnail_quota_bytes BETWEEN 0 AND 17592186044416),"
            "raw_audio_quota_bytes INTEGER NOT NULL DEFAULT 0 "
            "CHECK(raw_audio_quota_bytes BETWEEN 0 AND 17592186044416),"
            "shared_policy INTEGER NOT NULL DEFAULT 0 "
            "CHECK(shared_policy IN (0,1)),"
            "maximum_deletes_per_run INTEGER NOT NULL DEFAULT 100 "
            "CHECK(maximum_deletes_per_run BETWEEN 1 AND 500),"
            "updated_at_ms INTEGER NOT NULL DEFAULT 1 CHECK(updated_at_ms>0)"
            ")");
        if (!executeSql(database, retentionTable,
                        QStringLiteral("create SSTV retention settings"), error)
            || !executeSql(database,
                           QStringLiteral(
                               "INSERT OR IGNORE INTO sstv_retention_settings(id) "
                               "VALUES(1)"),
                           QStringLiteral("seed safe SSTV retention settings"),
                           error)
            || !executeSql(database,
                           QStringLiteral(
                               "CREATE INDEX IF NOT EXISTS "
                               "idx_sstv_images_retention "
                               "ON sstv_images(favorite,captured_at_ms,id)"),
                           QStringLiteral("create SSTV retention index"), error)
            || !executeSql(database, QStringLiteral("PRAGMA user_version=5"),
                           QStringLiteral("mark SSTV schema v5"), error)) {
            return rollbackFailure(error ? *error
                                         : QStringLiteral("schema v5 migration failed"));
        }
        version = 5;
    }
    if (!validateSchema(database, error)) {
        return rollbackFailure(error ? *error
                                     : QStringLiteral("schema validation failed"));
    }
    if (!database.commit()) {
        const QString detail = databaseFailure(
            QStringLiteral("commit schema migration"), database);
        database.rollback();
        return fail(error, detail);
    }
    if (resultingVersion) {
        *resultingVersion = version;
    }
    return true;
}

bool SstvStorageWorker::validateSchema(QSqlDatabase& database,
                                       QString* error) const
{
    const QHash<QString, QString> required {
        {QStringLiteral("id"), QStringLiteral("TEXT")},
        {QStringLiteral("category"), QStringLiteral("INTEGER")},
        {QStringLiteral("captured_at_ms"), QStringLiteral("INTEGER")},
        {QStringLiteral("created_at_ms"), QStringLiteral("INTEGER")},
        {QStringLiteral("updated_at_ms"), QStringLiteral("INTEGER")},
        {QStringLiteral("mode"), QStringLiteral("TEXT")},
        {QStringLiteral("vis_code"), QStringLiteral("INTEGER")},
        {QStringLiteral("remote_callsign"), QStringLiteral("TEXT")},
        {QStringLiteral("local_callsign"), QStringLiteral("TEXT")},
        {QStringLiteral("source"), QStringLiteral("TEXT")},
        {QStringLiteral("frequency_hz"), QStringLiteral("INTEGER")},
        {QStringLiteral("complete"), QStringLiteral("INTEGER")},
        {QStringLiteral("image_path"), QStringLiteral("TEXT")},
        {QStringLiteral("metadata_path"), QStringLiteral("TEXT")},
        {QStringLiteral("sha256_hex"), QStringLiteral("TEXT")},
        {QStringLiteral("file_size_bytes"), QStringLiteral("INTEGER")},
        {QStringLiteral("width"), QStringLiteral("INTEGER")},
        {QStringLiteral("height"), QStringLiteral("INTEGER")},
        {QStringLiteral("note"), QStringLiteral("TEXT")},
        {QStringLiteral("remote"), QStringLiteral("INTEGER")},
        {QStringLiteral("upload_state"), QStringLiteral("INTEGER")},
        {QStringLiteral("tags_json"), QStringLiteral("TEXT")},
        {QStringLiteral("event_at_ms"), QStringLiteral("INTEGER")},
        {QStringLiteral("thumbnail_path"), QStringLiteral("TEXT")},
        {QStringLiteral("mime_type"), QStringLiteral("TEXT")},
        {QStringLiteral("original_width"), QStringLiteral("INTEGER")},
        {QStringLiteral("original_height"), QStringLiteral("INTEGER")},
        {QStringLiteral("digital"), QStringLiteral("INTEGER")},
        {QStringLiteral("vis_valid"), QStringLiteral("INTEGER")},
        {QStringLiteral("fsk_id"), QStringLiteral("TEXT")},
        {QStringLiteral("local_grid"), QStringLiteral("TEXT")},
        {QStringLiteral("remote_grid"), QStringLiteral("TEXT")},
        {QStringLiteral("audio_frequency_hz"), QStringLiteral("INTEGER")},
        {QStringLiteral("source_sample_rate_hz"), QStringLiteral("INTEGER")},
        {QStringLiteral("completion_percent"), QStringLiteral("INTEGER")},
        {QStringLiteral("quality_json"), QStringLiteral("TEXT")},
        {QStringLiteral("slant_correction_ppm"), QStringLiteral("REAL")},
        {QStringLiteral("raw_audio_path"), QStringLiteral("TEXT")},
        {QStringLiteral("related_qso_id"), QStringLiteral("TEXT")},
        {QStringLiteral("remote_provider"), QStringLiteral("TEXT")},
        {QStringLiteral("remote_object_id"), QStringLiteral("TEXT")},
        {QStringLiteral("expires_at_ms"), QStringLiteral("INTEGER")},
        {QStringLiteral("privacy_flags"), QStringLiteral("INTEGER")},
        {QStringLiteral("favorite"), QStringLiteral("INTEGER")}
    };
    QSet<QString> actual;
    QSqlQuery columns(database);
    if (!columns.exec(QStringLiteral("PRAGMA table_info(sstv_images)"))) {
        return fail(error, queryFailure(QStringLiteral("validate SSTV schema"),
                                        columns));
    }
    while (columns.next()) {
        const QString name = columns.value(1).toString();
        actual.insert(name);
        const auto expected = required.constFind(name);
        if (expected == required.cend()) {
            continue;
        }
        const QString declaredType = columns.value(2).toString().toUpper();
        const bool notNull = columns.value(3).toInt() != 0;
        const int primaryKeyPosition = columns.value(5).toInt();
        if (declaredType != expected.value() || !notNull
            || (name == QLatin1String("id") && primaryKeyPosition != 1)
            || (name != QLatin1String("id") && primaryKeyPosition != 0)) {
            return fail(error,
                        QStringLiteral("SSTV schema column invariant failed: %1")
                            .arg(name));
        }
    }
    const QSet<QString> missing = QSet<QString>(required.keyBegin(),
                                                required.keyEnd()) - actual;
    if (!missing.isEmpty()) {
        QStringList names = missing.values();
        names.sort();
        return fail(error, QStringLiteral("SSTV schema is missing columns: %1")
                               .arg(names.join(QLatin1Char(','))));
    }
    QSqlQuery definition(database);
    definition.prepare(QStringLiteral(
        "SELECT sql FROM sqlite_master WHERE type='table' AND name='sstv_images'"));
    if (!definition.exec() || !definition.next()) {
        return fail(error, queryFailure(QStringLiteral("read SSTV schema SQL"),
                                        definition));
    }
    if (definition.value(0).toString().contains(
            QStringLiteral("BLOB"), Qt::CaseInsensitive)) {
        return fail(error, QStringLiteral("SSTV schema must remain path-only"));
    }
    QSqlQuery compatibilityTrigger(database);
    compatibilityTrigger.prepare(QStringLiteral(
        "SELECT sql FROM sqlite_master WHERE type='trigger' "
        "AND name='trg_sstv_images_v4_compat_insert'"));
    if (!compatibilityTrigger.exec() || !compatibilityTrigger.next()
        || compatibilityTrigger.value(0).toString().contains(
            QStringLiteral("BLOB"), Qt::CaseInsensitive)) {
        return fail(error, QStringLiteral(
            "SSTV schema compatibility trigger is missing or invalid"));
    }

    QSqlQuery tagDefinition(database);
    tagDefinition.prepare(QStringLiteral(
        "SELECT sql FROM sqlite_master WHERE type='table' "
        "AND name='sstv_image_tags'"));
    if (!tagDefinition.exec() || !tagDefinition.next()) {
        return fail(error, queryFailure(QStringLiteral("read SSTV tag schema"),
                                        tagDefinition));
    }
    const QString tagSchema = tagDefinition.value(0).toString();
    if (tagSchema.contains(QStringLiteral("BLOB"), Qt::CaseInsensitive)
        || !tagSchema.contains(QStringLiteral("ON DELETE CASCADE"),
                               Qt::CaseInsensitive)) {
        return fail(error, QStringLiteral("SSTV tag schema invariant failed"));
    }
    QHash<QString, QString> requiredTagColumns {
        {QStringLiteral("image_id"), QStringLiteral("TEXT")},
        {QStringLiteral("tag"), QStringLiteral("TEXT")},
        {QStringLiteral("tag_folded"), QStringLiteral("TEXT")}
    };
    QSet<QString> actualTagColumns;
    QSqlQuery tagColumns(database);
    if (!tagColumns.exec(QStringLiteral("PRAGMA table_info(sstv_image_tags)"))) {
        return fail(error, queryFailure(QStringLiteral("validate SSTV tag schema"),
                                        tagColumns));
    }
    while (tagColumns.next()) {
        const QString name = tagColumns.value(1).toString();
        const auto expected = requiredTagColumns.constFind(name);
        if (expected == requiredTagColumns.cend()) {
            continue;
        }
        actualTagColumns.insert(name);
        const int primaryKeyPosition = tagColumns.value(5).toInt();
        const int expectedPrimaryKeyPosition = name == QLatin1String("image_id")
            ? 1 : name == QLatin1String("tag_folded") ? 2 : 0;
        if (tagColumns.value(2).toString().toUpper() != expected.value()
            || tagColumns.value(3).toInt() == 0
            || primaryKeyPosition != expectedPrimaryKeyPosition) {
            return fail(error, QStringLiteral(
                "SSTV tag schema column invariant failed: %1").arg(name));
        }
    }
    if (actualTagColumns.size() != requiredTagColumns.size()) {
        return fail(error, QStringLiteral("SSTV tag schema is incomplete"));
    }
    bool cascadeForeignKey = false;
    QSqlQuery foreignKeys(database);
    if (!foreignKeys.exec(QStringLiteral(
            "PRAGMA foreign_key_list(sstv_image_tags)"))) {
        return fail(error, queryFailure(QStringLiteral("inspect SSTV tag foreign key"),
                                        foreignKeys));
    }
    while (foreignKeys.next()) {
        if (foreignKeys.value(2).toString() == QLatin1String("sstv_images")
            && foreignKeys.value(3).toString() == QLatin1String("image_id")
            && foreignKeys.value(4).toString() == QLatin1String("id")
            && foreignKeys.value(6).toString().compare(
                   QStringLiteral("CASCADE"), Qt::CaseInsensitive) == 0) {
            cascadeForeignKey = true;
        }
    }
    if (!cascadeForeignKey) {
        return fail(error, QStringLiteral("SSTV tag cascade foreign key is missing"));
    }

    bool imagePathUnique = false;
    bool metadataPathUnique = false;
    bool thumbnailPathUnique = false;
    QSet<QString> indexNames;
    QSqlQuery indexes(database);
    if (!indexes.exec(QStringLiteral("PRAGMA index_list(sstv_images)"))) {
        return fail(error, queryFailure(QStringLiteral("inspect SSTV indexes"),
                                        indexes));
    }
    while (indexes.next()) {
        const QString indexName = indexes.value(1).toString();
        indexNames.insert(indexName);
        if (indexes.value(2).toInt() == 0) {
            continue;
        }
        QString escapedIndex = indexName;
        escapedIndex.replace(QLatin1Char('\''), QStringLiteral("''"));
        QSqlQuery indexColumns(database);
        if (!indexColumns.exec(
                QStringLiteral("PRAGMA index_info('%1')").arg(escapedIndex))) {
            return fail(error, queryFailure(
                QStringLiteral("inspect unique SSTV index"), indexColumns));
        }
        QStringList indexedColumns;
        while (indexColumns.next()) {
            indexedColumns.append(indexColumns.value(2).toString());
        }
        if (indexedColumns == QStringList {QStringLiteral("image_path")}) {
            imagePathUnique = true;
        } else if (indexedColumns
                   == QStringList {QStringLiteral("metadata_path")}) {
            metadataPathUnique = true;
        } else if (indexedColumns
                   == QStringList {QStringLiteral("thumbnail_path")}) {
            thumbnailPathUnique = true;
        }
    }
    if (!imagePathUnique || !metadataPathUnique || !thumbnailPathUnique
        || !indexNames.contains(QStringLiteral("idx_sstv_images_page"))
        || !indexNames.contains(QStringLiteral("idx_sstv_images_category_page"))
        || !indexNames.contains(QStringLiteral("idx_sstv_images_updated"))
        || !indexNames.contains(QStringLiteral("idx_sstv_images_filters"))
        || !indexNames.contains(QStringLiteral("idx_sstv_images_event"))
        || !indexNames.contains(QStringLiteral("idx_sstv_images_remote_object"))
        || !indexNames.contains(QStringLiteral("idx_sstv_images_qso"))
        || !indexNames.contains(QStringLiteral("idx_sstv_images_expiry"))
        || !indexNames.contains(QStringLiteral("idx_sstv_images_retention"))
        || !indexNames.contains(
            QStringLiteral("idx_sstv_images_source_classification"))) {
        return fail(error, QStringLiteral("SSTV schema indexes are incomplete"));
    }
    QSet<QString> tagIndexes;
    QSqlQuery tagIndexQuery(database);
    if (!tagIndexQuery.exec(QStringLiteral(
            "PRAGMA index_list(sstv_image_tags)"))) {
        return fail(error, queryFailure(QStringLiteral("inspect SSTV tag indexes"),
                                        tagIndexQuery));
    }
    while (tagIndexQuery.next()) {
        tagIndexes.insert(tagIndexQuery.value(1).toString());
    }
    if (!tagIndexes.contains(QStringLiteral("idx_sstv_tags_lookup"))) {
        return fail(error, QStringLiteral("SSTV tag lookup index is missing"));
    }
    const QHash<QString, QString> requiredRetentionColumns {
        {QStringLiteral("id"), QStringLiteral("INTEGER")},
        {QStringLiteral("automatic_enabled"), QStringLiteral("INTEGER")},
        {QStringLiteral("maximum_age_days"), QStringLiteral("INTEGER")},
        {QStringLiteral("image_quota_bytes"), QStringLiteral("INTEGER")},
        {QStringLiteral("thumbnail_quota_bytes"), QStringLiteral("INTEGER")},
        {QStringLiteral("raw_audio_quota_bytes"), QStringLiteral("INTEGER")},
        {QStringLiteral("shared_policy"), QStringLiteral("INTEGER")},
        {QStringLiteral("maximum_deletes_per_run"), QStringLiteral("INTEGER")},
        {QStringLiteral("updated_at_ms"), QStringLiteral("INTEGER")},
    };
    QSet<QString> actualRetentionColumns;
    QSqlQuery retentionColumns(database);
    if (!retentionColumns.exec(QStringLiteral(
            "PRAGMA table_info(sstv_retention_settings)"))) {
        return fail(error, queryFailure(
            QStringLiteral("validate SSTV retention schema"),
            retentionColumns));
    }
    while (retentionColumns.next()) {
        const QString name = retentionColumns.value(1).toString();
        const auto expected = requiredRetentionColumns.constFind(name);
        if (expected == requiredRetentionColumns.cend()) {
            continue;
        }
        actualRetentionColumns.insert(name);
        const int expectedPrimaryKey = name == QLatin1String("id") ? 1 : 0;
        if (retentionColumns.value(2).toString().toUpper() != expected.value()
            || retentionColumns.value(3).toInt() == 0
            || retentionColumns.value(5).toInt() != expectedPrimaryKey) {
            return fail(error, QStringLiteral(
                "SSTV retention schema column invariant failed: %1")
                    .arg(name));
        }
    }
    if (actualRetentionColumns.size() != requiredRetentionColumns.size()) {
        return fail(error, QStringLiteral(
            "SSTV retention settings schema is incomplete"));
    }
    QSqlQuery retentionSingleton(database);
    if (!retentionSingleton.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sstv_retention_settings WHERE id=1"))
        || !retentionSingleton.next()
        || retentionSingleton.value(0).toInt() != 1) {
        return fail(error, QStringLiteral(
            "SSTV retention settings singleton is missing"));
    }
    return true;
}

void SstvStorageWorker::closeConnection()
{
    if (!QThread::currentThread() || QThread::currentThread() != thread()) {
        return;
    }
    m_performance.endLifecycle();
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase database = QSqlDatabase::database(m_connectionName,
                                                            false);
            if (database.isValid()) {
                database.close();
            }
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    m_schemaVersion.store(0, std::memory_order_release);
    m_initialized.store(false, std::memory_order_release);
    m_retentionPlan.reset();
}

void SstvStorageWorker::shutdown()
{
    const quintptr token = currentThreadToken();
    if (!requireOwnerThread(QStringLiteral("shutdown"))) {
        return;
    }
    closeConnection();
    emit shutdownFinished(token);
}

bool SstvStorageWorker::validateRecordForWrite(
    const SstvImageRecord& record,
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
    const SstvImageStore store(m_layout, m_limits);
    return store.verify(record, true, error);
}

bool SstvStorageWorker::readRetentionSettings(
    QSqlDatabase& database,
    SstvRetentionSettings* settings,
    QString* error) const
{
    if (!settings) {
        return fail(error, QStringLiteral("retention settings output is null"));
    }
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT automatic_enabled,maximum_age_days,image_quota_bytes,"
            "thumbnail_quota_bytes,raw_audio_quota_bytes,shared_policy,"
            "maximum_deletes_per_run FROM sstv_retention_settings WHERE id=1"))
        || !query.next()) {
        return fail(error, queryFailure(
            QStringLiteral("read SSTV retention settings"), query));
    }
    SstvRetentionSettings parsed;
    parsed.automaticEnabled = query.value(0).toInt() != 0;
    parsed.maximumAgeDays = query.value(1).toInt();
    parsed.imageQuotaBytes = query.value(2).toLongLong();
    parsed.thumbnailQuotaBytes = query.value(3).toLongLong();
    parsed.rawAudioQuotaBytes = query.value(4).toLongLong();
    parsed.sharedPolicy = static_cast<SstvSharedRetentionPolicy>(
        query.value(5).toInt());
    parsed.maximumDeletesPerRun = query.value(6).toInt();
    if (!parsed.validate(error) || query.next()) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral(
                "retention settings singleton contains multiple rows");
        }
        return false;
    }
    *settings = parsed;
    return true;
}

bool SstvStorageWorker::retentionRecordIsProtected(
    const SstvImageRecord& record,
    const SstvRetentionSettings& settings,
    QString* reason,
    QString* error) const
{
    if (reason) {
        reason->clear();
    }
    if (record.favorite) {
        if (reason) {
            *reason = QStringLiteral("favorite");
        }
        return true;
    }
    if (!record.relatedQsoId.isEmpty()) {
        if (reason) {
            *reason = QStringLiteral("qso");
        }
        return true;
    }
    const bool hasShareState = record.uploadState != SstvUploadState::NotRequested
        || !record.remoteProvider.isEmpty() || !record.remoteObjectId.isEmpty();
    const bool explicitlyDeletableUploaded =
        settings.sharedPolicy == SstvSharedRetentionPolicy::AllowUploaded
        && record.uploadState == SstvUploadState::Uploaded
        && !record.remoteProvider.isEmpty()
        && !record.remoteObjectId.isEmpty();
    if (hasShareState && !explicitlyDeletableUploaded) {
        if (reason) {
            *reason = QStringLiteral("shared");
        }
        return true;
    }

    const auto safeOwnedFile = [this](const QString& path,
                                      bool required,
                                      QString* detail) {
        if (path.isEmpty()) {
            if (required) {
                return fail(detail, QStringLiteral(
                    "required retention path is empty"));
            }
            return true;
        }
        const QFileInfo info(path);
        if (!info.exists()) {
            if (required) {
                return fail(detail, QStringLiteral(
                    "required retention file is missing"));
            }
            return true;
        }
        if (!info.isFile() || info.isSymLink()
            || info.size() < 0
            || !m_layout.containsPath(path, true, detail)) {
            if (detail && detail->isEmpty()) {
                *detail = QStringLiteral(
                    "retention path is not an owned regular file");
            }
            return false;
        }
        return true;
    };
    QString pathError;
    if (!safeOwnedFile(record.imagePath, true, &pathError)
        || !safeOwnedFile(record.metadataPath, true, &pathError)
        || !safeOwnedFile(record.thumbnailPath, false, &pathError)
        || !safeOwnedFile(record.rawAudioPath, false, &pathError)) {
        if (reason) {
            *reason = QStringLiteral("unsafe");
        }
        if (error) {
            *error = pathError;
        }
        return true;
    }
    return false;
}

bool SstvStorageWorker::buildRetentionPlan(
    QSqlDatabase& database,
    const SstvRetentionSettings& settings,
    SstvRetentionPlan* plan,
    SstvQuotaSummary* quota,
    QString* error) const
{
    constexpr int kMaximumScannedRecords = 100'000;
    if (!plan || !quota || !settings.validate(error)) {
        return plan && quota ? false
                             : fail(error, QStringLiteral(
                                   "retention plan outputs are null"));
    }

    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT %1 FROM sstv_images "
            "ORDER BY captured_at_ms ASC,id ASC LIMIT 100001")
            .arg(recordColumns()))) {
        return fail(error, queryFailure(
            QStringLiteral("scan SSTV retention records"), query));
    }
    QVector<SstvImageRecord> records;
    records.reserve(1024);
    while (query.next()) {
        if (records.size() == kMaximumScannedRecords) {
            return fail(error, QStringLiteral(
                "retention scan exceeds 100000 records"));
        }
        SstvImageRecord record;
        if (!readRecord(query, m_limits, &record, error)) {
            return false;
        }
        records.append(std::move(record));
    }

    SstvQuotaSummary measured;
    measured.recordCount = static_cast<int>(records.size());
    QSet<QString> measuredImages;
    QSet<QString> measuredThumbnails;
    QSet<QString> measuredRawAudio;
    QSet<QString> measuredMetadata;
    QHash<QString, int> rawAudioReferences;
    for (const SstvImageRecord& record : std::as_const(records)) {
        if (!record.rawAudioPath.isEmpty()) {
            rawAudioReferences[record.rawAudioPath] += 1;
        }
    }
    const auto measurePath = [this, &measured](
        const QString& path,
        bool required,
        QSet<QString>* seen,
        qint64* total) {
        if (path.isEmpty()) {
            if (required) {
                ++measured.missingFileCount;
            }
            return qint64 {0};
        }
        if (seen->contains(path)) {
            return qint64 {0};
        }
        seen->insert(path);
        const QFileInfo info(path);
        if (!info.exists()) {
            if (required) {
                ++measured.missingFileCount;
            }
            return qint64 {0};
        }
        QString containmentError;
        if (!info.isFile() || info.isSymLink() || info.size() < 0
            || !m_layout.containsPath(path, true, &containmentError)) {
            ++measured.unsafePathCount;
            return qint64 {0};
        }
        const qint64 bytes = info.size();
        if (*total > std::numeric_limits<qint64>::max() - bytes) {
            measured.complete = false;
            return qint64 {-1};
        }
        *total += bytes;
        return bytes;
    };

    struct Candidate final
    {
        SstvImageRecord record;
        qint64 imageBytes {0};
        qint64 thumbnailBytes {0};
        qint64 rawAudioBytes {0};
        bool protectedRecord {false};
        QString protection;
    };
    QVector<Candidate> candidates;
    candidates.reserve(records.size());
    for (const SstvImageRecord& record : std::as_const(records)) {
        Candidate candidate;
        candidate.record = record;
        candidate.imageBytes = measurePath(
            record.imagePath, true, &measuredImages, &measured.imageBytes);
        const qint64 metadataBytes = measurePath(
            record.metadataPath, true, &measuredMetadata,
            &measured.metadataBytes);
        candidate.thumbnailBytes = measurePath(
            record.thumbnailPath, false, &measuredThumbnails,
            &measured.thumbnailBytes);
        const qint64 rawBytes = measurePath(
            record.rawAudioPath, false, &measuredRawAudio,
            &measured.rawAudioBytes);
        candidate.rawAudioBytes =
            rawAudioReferences.value(record.rawAudioPath) == 1 ? rawBytes : 0;
        if (candidate.imageBytes < 0 || metadataBytes < 0
            || candidate.thumbnailBytes < 0 || rawBytes < 0) {
            return fail(error, QStringLiteral(
                "retention quota arithmetic overflowed"));
        }
        QString protectionError;
        candidate.protectedRecord = retentionRecordIsProtected(
            record, settings, &candidate.protection, &protectionError);
        candidates.append(std::move(candidate));
    }
    measured.complete = measured.missingFileCount == 0
        && measured.unsafePathCount == 0;

    SstvRetentionPlan built;
    built.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    built.createdAtUtc = QDateTime::currentDateTimeUtc();
    built.settings = settings;
    qint64 projectedImages = measured.imageBytes;
    qint64 projectedThumbnails = measured.thumbnailBytes;
    qint64 projectedRawAudio = measured.rawAudioBytes;
    const QDateTime ageCutoff = settings.maximumAgeDays > 0
        ? built.createdAtUtc.addDays(-settings.maximumAgeDays) : QDateTime {};
    bool eligibleExpiredRemains = false;

    const auto overQuota = [&]() {
        return (settings.imageQuotaBytes > 0
                && projectedImages > settings.imageQuotaBytes)
            || (settings.thumbnailQuotaBytes > 0
                && projectedThumbnails > settings.thumbnailQuotaBytes)
            || (settings.rawAudioQuotaBytes > 0
                && projectedRawAudio > settings.rawAudioQuotaBytes);
    };
    for (const Candidate& candidate : std::as_const(candidates)) {
        if (candidate.protectedRecord) {
            if (candidate.protection == QLatin1String("favorite")) {
                ++built.protectedFavoriteCount;
            } else if (candidate.protection == QLatin1String("qso")) {
                ++built.protectedQsoCount;
            } else if (candidate.protection == QLatin1String("shared")) {
                ++built.protectedSharedCount;
            } else {
                ++built.protectedUnsafeCount;
            }
            continue;
        }
        const bool expired = ageCutoff.isValid()
            && candidate.record.capturedAtUtc < ageCutoff;
        const bool helpsQuota =
            (settings.imageQuotaBytes > 0
             && projectedImages > settings.imageQuotaBytes
             && candidate.imageBytes > 0)
            || (settings.thumbnailQuotaBytes > 0
                && projectedThumbnails > settings.thumbnailQuotaBytes
                && candidate.thumbnailBytes > 0)
            || (settings.rawAudioQuotaBytes > 0
                && projectedRawAudio > settings.rawAudioQuotaBytes
                && candidate.rawAudioBytes > 0);
        if (!expired && !helpsQuota) {
            continue;
        }
        if (built.recordIds.size() >= settings.maximumDeletesPerRun) {
            if (expired) {
                eligibleExpiredRemains = true;
            }
            continue;
        }
        built.recordIds.append(candidate.record.id);
        built.imageBytes += candidate.imageBytes;
        built.thumbnailBytes += candidate.thumbnailBytes;
        built.rawAudioBytes += candidate.rawAudioBytes;
        projectedImages -= candidate.imageBytes;
        projectedThumbnails -= candidate.thumbnailBytes;
        projectedRawAudio -= candidate.rawAudioBytes;
    }
    built.targetsSatisfied = !overQuota() && !eligibleExpiredRemains;
    if (!measured.complete) {
        built.targetsSatisfied = false;
        built.warning = QStringLiteral(
            "Quota is incomplete because indexed files are missing or unsafe; "
            "those records are protected.");
    } else if (!built.targetsSatisfied) {
        built.warning = QStringLiteral(
            "Protected records or the per-run bound prevent this plan from "
            "meeting every configured target.");
    }
    if (!built.recordIds.isEmpty()) {
        built.confirmationPhrase = QStringLiteral("DELETE %1 GALLERY ITEMS")
            .arg(built.recordIds.size());
    }
    *plan = std::move(built);
    *quota = measured;
    return true;
}

bool SstvStorageWorker::bindRecord(QSqlQuery& query,
                                   const SstvImageRecord& record,
                                   QString* error)
{
    query.bindValue(QStringLiteral(":id"), record.id);
    query.bindValue(QStringLiteral(":category"),
                    static_cast<int>(record.category));
    query.bindValue(QStringLiteral(":captured_at_ms"),
                    record.capturedAtUtc.toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":created_at_ms"),
                    record.createdAtUtc.toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":updated_at_ms"),
                    record.updatedAtUtc.toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":mode"), record.mode);
    query.bindValue(QStringLiteral(":vis_code"), record.visCode);
    query.bindValue(QStringLiteral(":remote_callsign"),
                    record.remoteCallsign.isNull()
                        ? QStringLiteral("") : record.remoteCallsign);
    query.bindValue(QStringLiteral(":local_callsign"),
                    record.localCallsign.isNull()
                        ? QStringLiteral("") : record.localCallsign);
    query.bindValue(QStringLiteral(":source"), record.source);
    query.bindValue(QStringLiteral(":frequency_hz"), record.frequencyHz);
    query.bindValue(QStringLiteral(":complete"), record.complete ? 1 : 0);
    query.bindValue(QStringLiteral(":image_path"), record.imagePath);
    query.bindValue(QStringLiteral(":metadata_path"), record.metadataPath);
    query.bindValue(QStringLiteral(":sha256_hex"),
                    QString::fromLatin1(record.sha256.toHex()));
    query.bindValue(QStringLiteral(":file_size_bytes"), record.fileSizeBytes);
    query.bindValue(QStringLiteral(":width"), record.width);
    query.bindValue(QStringLiteral(":height"), record.height);
    query.bindValue(QStringLiteral(":note"),
                    record.note.isNull() ? QStringLiteral("") : record.note);
    query.bindValue(QStringLiteral(":remote"), record.remote ? 1 : 0);
    query.bindValue(QStringLiteral(":upload_state"),
                    static_cast<int>(record.uploadState));
    query.bindValue(QStringLiteral(":tags_json"), tagsJson(record.tags));
    query.bindValue(QStringLiteral(":event_at_ms"),
                    record.eventAtUtc.toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":thumbnail_path"), record.thumbnailPath);
    query.bindValue(QStringLiteral(":mime_type"), record.mimeType);
    query.bindValue(QStringLiteral(":original_width"), record.originalWidth);
    query.bindValue(QStringLiteral(":original_height"), record.originalHeight);
    query.bindValue(QStringLiteral(":digital"), record.digital ? 1 : 0);
    query.bindValue(QStringLiteral(":vis_valid"), record.visValid ? 1 : 0);
    query.bindValue(QStringLiteral(":fsk_id"),
                    record.fskId.isNull() ? QStringLiteral("") : record.fskId);
    query.bindValue(QStringLiteral(":local_grid"),
                    record.localGrid.isNull()
                        ? QStringLiteral("") : record.localGrid);
    query.bindValue(QStringLiteral(":remote_grid"),
                    record.remoteGrid.isNull()
                        ? QStringLiteral("") : record.remoteGrid);
    query.bindValue(QStringLiteral(":audio_frequency_hz"),
                    record.audioFrequencyHz);
    query.bindValue(QStringLiteral(":source_sample_rate_hz"),
                    record.sourceSampleRateHz);
    query.bindValue(QStringLiteral(":completion_percent"),
                    record.completionPercent);
    query.bindValue(QStringLiteral(":quality_json"),
                    qualityJson(record.qualityMetrics));
    query.bindValue(QStringLiteral(":slant_correction_ppm"),
                    record.slantCorrectionPpm);
    query.bindValue(QStringLiteral(":raw_audio_path"),
                    record.rawAudioPath.isNull()
                        ? QStringLiteral("") : record.rawAudioPath);
    query.bindValue(QStringLiteral(":related_qso_id"),
                    record.relatedQsoId.isNull()
                        ? QStringLiteral("") : record.relatedQsoId);
    query.bindValue(QStringLiteral(":remote_provider"),
                    record.remoteProvider.isNull()
                        ? QStringLiteral("") : record.remoteProvider);
    query.bindValue(QStringLiteral(":remote_object_id"),
                    record.remoteObjectId.isNull()
                        ? QStringLiteral("") : record.remoteObjectId);
    query.bindValue(QStringLiteral(":expires_at_ms"),
                    record.expiresAtUtc.isValid()
                        ? record.expiresAtUtc.toMSecsSinceEpoch() : 0);
    query.bindValue(QStringLiteral(":privacy_flags"),
                    static_cast<qint64>(record.privacyFlags));
    query.bindValue(QStringLiteral(":favorite"), record.favorite ? 1 : 0);
    if (query.lastError().isValid()) {
        return fail(error, queryFailure(QStringLiteral("bind SSTV record"), query));
    }
    return true;
}

bool SstvStorageWorker::readRecord(QSqlQuery& query,
                                   const SstvStorageLimits& limits,
                                   SstvImageRecord* record,
                                   QString* error)
{
    if (!record) {
        return fail(error, QStringLiteral("record output is null"));
    }
    SstvImageRecord parsed;
    parsed.id = query.value(0).toString();
    parsed.category = static_cast<SstvImageCategory>(query.value(1).toInt());
    parsed.capturedAtUtc = QDateTime::fromMSecsSinceEpoch(
        query.value(2).toLongLong(), QTimeZone(QTimeZone::UTC));
    parsed.createdAtUtc = QDateTime::fromMSecsSinceEpoch(
        query.value(3).toLongLong(), QTimeZone(QTimeZone::UTC));
    parsed.updatedAtUtc = QDateTime::fromMSecsSinceEpoch(
        query.value(4).toLongLong(), QTimeZone(QTimeZone::UTC));
    parsed.mode = query.value(5).toString();
    parsed.visCode = query.value(6).toInt();
    parsed.remoteCallsign = query.value(7).toString();
    parsed.localCallsign = query.value(8).toString();
    parsed.source = query.value(9).toString();
    parsed.frequencyHz = query.value(10).toLongLong();
    parsed.complete = query.value(11).toInt() != 0;
    parsed.imagePath = query.value(12).toString();
    parsed.metadataPath = query.value(13).toString();
    const QByteArray hashHex = query.value(14).toString().toLatin1();
    if (hashHex.size() != 64) {
        return fail(error, QStringLiteral("database record has an invalid SHA-256"));
    }
    parsed.sha256 = QByteArray::fromHex(hashHex);
    parsed.fileSizeBytes = query.value(15).toLongLong();
    parsed.width = query.value(16).toInt();
    parsed.height = query.value(17).toInt();
    parsed.note = query.value(18).toString();
    parsed.remote = query.value(19).toInt() != 0;
    parsed.uploadState = static_cast<SstvUploadState>(query.value(20).toInt());
    if (!parseTagsJson(query.value(21).toString(), &parsed.tags, error)) {
        return false;
    }
    parsed.eventAtUtc = QDateTime::fromMSecsSinceEpoch(
        query.value(22).toLongLong(), QTimeZone(QTimeZone::UTC));
    parsed.thumbnailPath = query.value(23).toString();
    parsed.mimeType = query.value(24).toString();
    parsed.originalWidth = query.value(25).toInt();
    parsed.originalHeight = query.value(26).toInt();
    parsed.digital = query.value(27).toInt() != 0;
    parsed.visValid = query.value(28).toInt() != 0;
    parsed.fskId = query.value(29).toString();
    parsed.localGrid = query.value(30).toString();
    parsed.remoteGrid = query.value(31).toString();
    parsed.audioFrequencyHz = query.value(32).toLongLong();
    parsed.sourceSampleRateHz = query.value(33).toInt();
    parsed.completionPercent = query.value(34).toInt();
    if (!parseQualityJson(query.value(35).toString(),
                          &parsed.qualityMetrics, error)) {
        return false;
    }
    parsed.slantCorrectionPpm = query.value(36).toDouble();
    parsed.rawAudioPath = query.value(37).toString();
    parsed.relatedQsoId = query.value(38).toString();
    parsed.remoteProvider = query.value(39).toString();
    parsed.remoteObjectId = query.value(40).toString();
    const qint64 expiresAtMs = query.value(41).toLongLong();
    if (expiresAtMs > 0) {
        parsed.expiresAtUtc = QDateTime::fromMSecsSinceEpoch(
            expiresAtMs, QTimeZone(QTimeZone::UTC));
    }
    const quint64 privacyFlags = query.value(42).toULongLong();
    if (privacyFlags > std::numeric_limits<quint32>::max()) {
        return fail(error, QStringLiteral(
            "database record has invalid privacy flags"));
    }
    parsed.privacyFlags = static_cast<quint32>(privacyFlags);
    parsed.favorite = query.value(43).toInt() != 0;
    if (!parsed.validate(limits, error)) {
        return false;
    }
    *record = std::move(parsed);
    return true;
}

bool SstvStorageWorker::replaceTags(QSqlDatabase& database,
                                    const SstvImageRecord& record,
                                    QString* error)
{
    QSqlQuery remove(database);
    remove.prepare(QStringLiteral(
        "DELETE FROM sstv_image_tags WHERE image_id=:image_id"));
    remove.bindValue(QStringLiteral(":image_id"), record.id);
    if (!remove.exec()) {
        return fail(error, queryFailure(QStringLiteral("clear SSTV tags"),
                                        remove));
    }
    if (record.tags.isEmpty()) {
        return true;
    }
    QSqlQuery insert(database);
    if (!insert.prepare(QStringLiteral(
            "INSERT INTO sstv_image_tags(image_id,tag,tag_folded) "
            "VALUES(:image_id,:tag,:tag_folded)"))) {
        return fail(error, queryFailure(QStringLiteral("prepare SSTV tags"),
                                        insert));
    }
    for (const QString& tag : record.tags) {
        insert.bindValue(QStringLiteral(":image_id"), record.id);
        insert.bindValue(QStringLiteral(":tag"), tag);
        insert.bindValue(QStringLiteral(":tag_folded"), tag.toCaseFolded());
        if (!insert.exec()) {
            return fail(error, queryFailure(QStringLiteral("insert SSTV tag"),
                                            insert));
        }
    }
    return true;
}

void SstvStorageWorker::emitOperationFailure(
    quint64 requestId,
    SstvStorageOperation operation,
    const QString& error)
{
    emit operationFinished(requestId, operation, false, error);
}

void SstvStorageWorker::insertRecord(SstvImageRecord record,
                                     quint64 requestId)
{
    if (!requireOwnerThread(QStringLiteral("insertRecord"))) {
        emitOperationFailure(requestId, SstvStorageOperation::Insert,
                             QStringLiteral("insert called outside owner thread"));
        return;
    }
    if (!m_initialized.load(std::memory_order_acquire)) {
        emitOperationFailure(requestId, SstvStorageOperation::Insert,
                             QStringLiteral("SSTV storage is not initialized"));
        return;
    }
    QString error;
    if (!insertRecordTransaction(record, &error)) {
        emitOperationFailure(requestId, SstvStorageOperation::Insert, error);
        return;
    }
    m_retentionPlan.reset();
    emit operationFinished(requestId, SstvStorageOperation::Insert, true, {});
    emit recordChanged(record);
}

bool SstvStorageWorker::insertRecordTransaction(
    const SstvImageRecord& record,
    QString* error)
{
    if (!validateRecordForWrite(record, error)) {
        return false;
    }
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.transaction()) {
        return fail(error,
                    databaseFailure(QStringLiteral("begin SSTV insert"),
                                    database));
    }
    QSqlQuery query(database);
    const QString sql = QStringLiteral(
        "INSERT INTO sstv_images (%1) VALUES ("
        ":id,:category,:captured_at_ms,:created_at_ms,:updated_at_ms,"
        ":mode,:vis_code,:remote_callsign,:local_callsign,:source,"
        ":frequency_hz,:complete,:image_path,:metadata_path,:sha256_hex,"
        ":file_size_bytes,:width,:height,:note,:remote,:upload_state,"
        ":tags_json,:event_at_ms,:thumbnail_path,:mime_type,:original_width,"
        ":original_height,:digital,:vis_valid,:fsk_id,:local_grid,:remote_grid,"
        ":audio_frequency_hz,:source_sample_rate_hz,:completion_percent,"
        ":quality_json,:slant_correction_ppm,:raw_audio_path,:related_qso_id,"
        ":remote_provider,:remote_object_id,:expires_at_ms,:privacy_flags,"
        ":favorite)")
        .arg(recordColumns());
    QString detail;
    if (!query.prepare(sql) || !bindRecord(query, record, &detail)
        || !query.exec()) {
        database.rollback();
        if (detail.isEmpty()) {
            detail = queryFailure(QStringLiteral("insert SSTV record"), query);
        }
        return fail(error, detail);
    }
    if (!replaceTags(database, record, &detail) || !database.commit()) {
        if (detail.isEmpty()) {
            detail = databaseFailure(QStringLiteral("commit SSTV insert"),
                                     database);
        }
        database.rollback();
        return fail(error, detail);
    }
    return true;
}

void SstvStorageWorker::storeAndInsertImage(SstvImageSaveRequest request,
                                            quint64 requestId)
{
    SstvImageRecord empty;
    if (!requireOwnerThread(QStringLiteral("storeAndInsertImage"))) {
        const QString error = QStringLiteral(
            "image store called outside owner thread");
        emitOperationFailure(requestId,
                             SstvStorageOperation::StoreAndInsert, error);
        emit imageStoreFinished(requestId, false, empty, error);
        return;
    }
    if (!m_initialized.load(std::memory_order_acquire)) {
        const QString error = QStringLiteral("SSTV storage is not initialized");
        emitOperationFailure(requestId,
                             SstvStorageOperation::StoreAndInsert, error);
        emit imageStoreFinished(requestId, false, empty, error);
        return;
    }

    const SstvImageStore store(m_layout, m_limits);
    SstvImageSaveResult saved = measuredImageSave(
        m_performance, [&store, &request]() { return store.save(request); });
    if (!saved.ok) {
        const QString error = saved.error.isEmpty()
            ? QStringLiteral("SSTV image encoding failed") : saved.error;
        emitOperationFailure(requestId,
                             SstvStorageOperation::StoreAndInsert, error);
        emit imageStoreFinished(requestId, false, {}, error);
        return;
    }

    QString error;
    if (!insertRecordTransaction(saved.record, &error)) {
        // These exact paths were created by store.save() above and remain
        // contained by the worker's immutable layout.  Best-effort rollback
        // prevents a rejected database insert from leaving an ordinary orphan.
        QString cleanupError;
        const bool contained = m_layout.containsPath(
            saved.record.imagePath, true, &cleanupError)
            && m_layout.containsPath(
                saved.record.metadataPath, true, &cleanupError);
        const bool metadataRemoved = contained
            && QFile::remove(saved.record.metadataPath);
        const bool imageRemoved = contained
            && QFile::remove(saved.record.imagePath);
        if (!metadataRemoved || !imageRemoved) {
            error += QStringLiteral(
                "; rollback could not remove the newly stored SSTV files");
        }
        emitOperationFailure(requestId,
                             SstvStorageOperation::StoreAndInsert, error);
        emit imageStoreFinished(requestId, false, {}, error);
        return;
    }

    m_retentionPlan.reset();
    emit operationFinished(requestId,
                           SstvStorageOperation::StoreAndInsert, true, {});
    m_retentionPlan.reset();
    emit recordChanged(saved.record);
    emit imageStoreFinished(requestId, true, saved.record, {});
}

void SstvStorageWorker::importValidatedIncomingHandoff(QVariantMap handoffMap)
{
    QString transferId;
    const QVariant transferValue = handoffMap.value(QStringLiteral("transferId"));
    if (transferValue.metaType().id() == QMetaType::QString
        && canonicalUuid(transferValue.toString())) {
        transferId = transferValue.toString();
    }
    if (!requireOwnerThread(QStringLiteral("importValidatedIncomingHandoff"))) {
        emit incomingImportFinished(incomingFailure(
            transferId, SstvIncomingImportFailure::InvalidHandoff, false,
            QStringLiteral("incoming import called outside owner thread")));
        return;
    }
    sharing::SstvValidatedIncomingHandoff handoff;
    QString error;
    if (!parseIncomingHandoffMap(handoffMap, &handoff, &error)) {
        emit incomingImportFinished(incomingFailure(
            transferId, SstvIncomingImportFailure::InvalidHandoff, false,
            error));
        return;
    }
    importValidatedIncoming(handoff);
}

void SstvStorageWorker::importValidatedIncomingHandoffTyped(
    sharing::SstvValidatedIncomingHandoff handoff)
{
    const QString transferId = canonicalUuid(handoff.transferId)
        ? handoff.transferId : QString {};
    if (!requireOwnerThread(
            QStringLiteral("importValidatedIncomingHandoffTyped"))) {
        emit incomingImportFinished(incomingFailure(
            transferId, SstvIncomingImportFailure::InvalidHandoff, false,
            QStringLiteral("incoming import called outside owner thread")));
        return;
    }
    QString error;
    if (!validIncomingHandoff(handoff, &error)) {
        emit incomingImportFinished(incomingFailure(
            transferId, SstvIncomingImportFailure::InvalidHandoff, false,
            error));
        return;
    }
    importValidatedIncoming(handoff);
}

void SstvStorageWorker::importValidatedIncoming(
    const sharing::SstvValidatedIncomingHandoff& handoff)
{
    const QString transferId = handoff.transferId;
    if (!m_initialized.load(std::memory_order_acquire)) {
        emit incomingImportFinished(incomingFailure(
            transferId, SstvIncomingImportFailure::StorageUnavailable, true,
            QStringLiteral("SSTV storage is not initialized")));
        return;
    }

    QString error;
    if (!isPrivateStagedPath(m_layout, handoff, false, &error)) {
        emit incomingImportFinished(incomingFailure(
            transferId, SstvIncomingImportFailure::UnsafeStagingPath, false,
            error));
        return;
    }

    // Serialize this UUID's filesystem/SQLite promotion across cooperating
    // Decodium processes. The lock remains held through DB commit and staged
    // cleanup, closing the race between an exact orphan recovery and another
    // process finishing its insert.
    QLockFile importLock(handoff.stagedCanonicalPath
                         + QStringLiteral(".gallery-import.lock"));
    importLock.setStaleLockTime(30'000);
    if (!importLock.tryLock(0)) {
        emit incomingImportFinished(incomingFailure(
            transferId, SstvIncomingImportFailure::StorageFailure, true,
            QStringLiteral("Gallery import is busy")));
        return;
    }

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT %1 FROM sstv_images WHERE id=:id")
                      .arg(recordColumns()));
    query.bindValue(QStringLiteral(":id"), transferId);
    if (!query.exec()) {
        emit incomingImportFinished(incomingFailure(
            transferId, SstvIncomingImportFailure::StorageFailure, true,
            QStringLiteral("Gallery duplicate check failed")));
        return;
    }

    const QByteArray expectedHash = QByteArray::fromHex(
        handoff.stagedSha256.toLatin1());
    if (query.next()) {
        SstvImageRecord existing;
        if (!readRecord(query, m_limits, &existing, &error)) {
            emit incomingImportFinished(incomingFailure(
                transferId, SstvIncomingImportFailure::IntegrityFailure,
                false, QStringLiteral("existing Gallery record is invalid")));
            return;
        }
        if (!recordMatchesIncoming(existing, handoff)) {
            emit incomingImportFinished(incomingFailure(
                transferId, SstvIncomingImportFailure::Conflict, false,
                QStringLiteral(
                    "transfer UUID conflicts with a different Gallery record")));
            return;
        }
        const SstvImageStore store(m_layout, m_limits);
        if (!store.verify(existing, true, &error)) {
            emit incomingImportFinished(incomingFailure(
                transferId, SstvIncomingImportFailure::IntegrityFailure,
                false, QStringLiteral(
                    "idempotent Gallery record failed integrity verification")));
            return;
        }
        if (QFileInfo::exists(handoff.stagedCanonicalPath)) {
            ValidatedStagedPng staged;
            if (!readAndValidateStagedPng(m_layout, m_limits, handoff,
                                          &staged, &error)) {
                emit incomingImportFinished(incomingFailure(
                    transferId, SstvIncomingImportFailure::IntegrityFailure,
                    false, error, existing));
                return;
            }
            if (!QFile::remove(handoff.stagedCanonicalPath)
                || QFileInfo::exists(handoff.stagedCanonicalPath)) {
                emit incomingImportFinished(incomingFailure(
                    transferId, SstvIncomingImportFailure::CleanupPending,
                    true, QStringLiteral(
                        "Gallery record exists but staging cleanup is pending"),
                    existing));
                return;
            }
        }
        emit incomingImportFinished(incomingSuccess(existing, true));
        return;
    }

    if (!QFileInfo::exists(handoff.stagedCanonicalPath)) {
        emit incomingImportFinished(incomingFailure(
            transferId, SstvIncomingImportFailure::IntegrityFailure, false,
            QStringLiteral("validated incoming staging PNG is missing")));
        return;
    }

    ValidatedStagedPng staged;
    if (!readAndValidateStagedPng(m_layout, m_limits, handoff,
                                  &staged, &error)) {
        emit incomingImportFinished(incomingFailure(
            transferId, SstvIncomingImportFailure::IntegrityFailure, false,
            error));
        return;
    }

    SstvImageSaveRequest request;
    request.record.id = transferId;
    request.record.category = SstvImageCategory::Imported;
    request.record.capturedAtUtc = handoff.receivedUtc.toUTC();
    request.record.eventAtUtc = handoff.receivedUtc.toUTC();
    request.record.mode = handoff.sstvMode;
    request.record.source = QStringLiteral("remote-sharing");
    request.record.completionPercent = 100;
    request.record.complete = true;
    request.record.remote = true;
    request.record.uploadState = SstvUploadState::NotRequested;
    request.record.remoteProvider = handoff.providerId;
    request.record.remoteObjectId = handoff.incomingId;
    request.record.expiresAtUtc = handoff.expiresUtc.toUTC();
    request.record.mimeType = QStringLiteral("image/png");
    request.record.originalWidth = static_cast<int>(handoff.width);
    request.record.originalHeight = static_cast<int>(handoff.height);
    request.image = staged.image;
    request.fileNameTemplate = QStringLiteral(
        "remote_{date}_{time}_{mode}_{id}");

    const SstvImageStore store(m_layout, m_limits);
    SstvImageSaveResult saved = measuredImageSave(
        m_performance,
        [&store, &request, &staged]() {
            return store.savePreservingPng(request, staged.bytes);
        });
    if (!saved.ok && saved.code == SstvStoreError::Collision) {
        // A process may have stopped after atomically publishing the PNG and
        // sidecar but before inserting SQLite. Deterministic UUID naming lets
        // restart adopt only an exact, fully verified orphan. An exact lone
        // PNG from the narrower rename crash window is removed and replayed;
        // any differing object is a permanent conflict.
        QString fileBase;
        const QString directory = m_layout.datedCategoryDirectory(
            request.record.category, request.record.capturedAtUtc.date());
        const bool rendered = SstvImageStore::renderFileBase(
            request.fileNameTemplate, request.record,
            m_limits.maximumFileNameUtf8Bytes, &fileBase, &error);
        const QString imagePath = QDir(directory).absoluteFilePath(
            fileBase + QStringLiteral(".png"));
        const QString metadataPath = QDir(directory).absoluteFilePath(
            fileBase + QStringLiteral(".json"));
        const bool imageExists = rendered && QFileInfo::exists(imagePath);
        const bool metadataExists = rendered
            && QFileInfo::exists(metadataPath);
        if (imageExists && metadataExists) {
            SstvImageRecord recovered;
            const QFileInfo imageInfo(imagePath);
            const QFileInfo metadataInfo(metadataPath);
            if (imageInfo.isFile() && !imageInfo.isSymLink()
                && metadataInfo.isFile() && !metadataInfo.isSymLink()
                && m_layout.containsPath(imagePath, true, nullptr)
                && m_layout.containsPath(metadataPath, true, nullptr)
                && SstvImageStore::loadMetadata(metadataPath, &recovered,
                                                 &error, m_limits)
                && recordMatchesIncoming(recovered, handoff)
                && store.verify(recovered, true, &error)) {
                saved.ok = true;
                saved.code = SstvStoreError::None;
                saved.record = std::move(recovered);
                saved.error.clear();
            }
        } else if (imageExists && !metadataExists) {
            qint64 orphanBytes = 0;
            const QFileInfo imageInfo(imagePath);
            const bool safeImage = imageInfo.isFile()
                && !imageInfo.isSymLink()
                && m_layout.containsPath(imagePath, true, nullptr);
            const QByteArray orphanHash = safeImage
                ? SstvImageStore::sha256File(
                    imagePath, m_limits.maximumPngBytes,
                    &orphanBytes, &error)
                : QByteArray {};
            if (safeImage && orphanHash == expectedHash
                && orphanBytes
                    == static_cast<qint64>(handoff.stagedByteSize)
                && QFile::remove(imagePath)) {
                saved = measuredImageSave(
                    m_performance,
                    [&store, &request, &staged]() {
                        return store.savePreservingPng(request, staged.bytes);
                    });
            }
        } else if (!imageExists && metadataExists) {
            SstvImageRecord orphan;
            const QFileInfo metadataInfo(metadataPath);
            if (metadataInfo.isFile() && !metadataInfo.isSymLink()
                && m_layout.containsPath(metadataPath, true, nullptr)
                && SstvImageStore::loadMetadata(metadataPath, &orphan,
                                                 &error, m_limits)
                && recordMatchesIncoming(orphan, handoff)
                && QFile::remove(metadataPath)) {
                saved = measuredImageSave(
                    m_performance,
                    [&store, &request, &staged]() {
                        return store.savePreservingPng(request, staged.bytes);
                    });
            }
        }
    }
    if (!saved.ok) {
        const bool retryable = saved.code == SstvStoreError::IoFailure
            || saved.code == SstvStoreError::InvalidLayout;
        const SstvIncomingImportFailure failure =
            saved.code == SstvStoreError::Collision
            ? SstvIncomingImportFailure::Conflict
            : (retryable ? SstvIncomingImportFailure::StorageFailure
                         : SstvIncomingImportFailure::IntegrityFailure);
        emit incomingImportFinished(incomingFailure(
            transferId, failure, retryable,
            retryable
                ? QStringLiteral("Gallery file publication failed")
                : QStringLiteral("validated PNG could not be published")));
        return;
    }
    if (saved.record.sha256 != expectedHash
        || saved.record.fileSizeBytes
            != static_cast<qint64>(handoff.stagedByteSize)) {
        QFile::remove(saved.record.metadataPath);
        QFile::remove(saved.record.imagePath);
        emit incomingImportFinished(incomingFailure(
            transferId, SstvIncomingImportFailure::IntegrityFailure, false,
            QStringLiteral("published Gallery PNG changed identity")));
        return;
    }

    if (!insertRecordTransaction(saved.record, &error)) {
        bool cleanupOk = true;
        const QStringList paths {saved.record.thumbnailPath,
                                 saved.record.metadataPath,
                                 saved.record.imagePath};
        for (const QString& path : paths) {
            if (QFileInfo::exists(path)
                && (!m_layout.containsPath(path, true, nullptr)
                    || !QFile::remove(path) || QFileInfo::exists(path))) {
                cleanupOk = false;
            }
        }
        emit incomingImportFinished(incomingFailure(
            transferId, SstvIncomingImportFailure::StorageFailure, true,
            cleanupOk
                ? QStringLiteral("Gallery database commit failed")
                : QStringLiteral(
                    "Gallery database commit failed and file cleanup is pending")));
        return;
    }

    m_retentionPlan.reset();
    emit recordChanged(saved.record);

    // The database row and exact PNG/sidecar are durable now. Recheck the
    // private path and hash before transferring ownership by unlinking it.
    qint64 cleanupBytes = 0;
    const bool cleanupPathSafe = isPrivateStagedPath(
        m_layout, handoff, true, &error);
    const QByteArray cleanupHash = cleanupPathSafe
        ? SstvImageStore::sha256File(
            handoff.stagedCanonicalPath, m_limits.maximumPngBytes,
            &cleanupBytes, &error)
        : QByteArray {};
    if (!cleanupPathSafe
        || cleanupHash != expectedHash
        || cleanupBytes != static_cast<qint64>(handoff.stagedByteSize)
        || !QFile::remove(handoff.stagedCanonicalPath)
        || QFileInfo::exists(handoff.stagedCanonicalPath)) {
        emit incomingImportFinished(incomingFailure(
            transferId, SstvIncomingImportFailure::CleanupPending, true,
            QStringLiteral(
                "Gallery import committed but staging cleanup is pending"),
            saved.record));
        return;
    }

    emit incomingImportFinished(incomingSuccess(saved.record, false));
}

void SstvStorageWorker::updateRecord(SstvImageRecord record,
                                     quint64 requestId)
{
    if (!requireOwnerThread(QStringLiteral("updateRecord"))) {
        emitOperationFailure(requestId, SstvStorageOperation::Update,
                             QStringLiteral("update called outside owner thread"));
        return;
    }
    if (!m_initialized.load(std::memory_order_acquire)) {
        emitOperationFailure(requestId, SstvStorageOperation::Update,
                             QStringLiteral("SSTV storage is not initialized"));
        return;
    }
    QString error;
    if (!validateRecordForWrite(record, &error)) {
        emitOperationFailure(requestId, SstvStorageOperation::Update, error);
        return;
    }
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.transaction()) {
        emitOperationFailure(requestId, SstvStorageOperation::Update,
                             databaseFailure(QStringLiteral("begin SSTV update"),
                                             database));
        return;
    }
    QSqlQuery query(database);
    if (!query.prepare(QStringLiteral(
            "UPDATE sstv_images SET "
            "category=:category,captured_at_ms=:captured_at_ms,"
            "created_at_ms=:created_at_ms,updated_at_ms=:updated_at_ms,"
            "mode=:mode,vis_code=:vis_code,remote_callsign=:remote_callsign,"
            "local_callsign=:local_callsign,source=:source,"
            "frequency_hz=:frequency_hz,complete=:complete,"
            "image_path=:image_path,metadata_path=:metadata_path,"
            "sha256_hex=:sha256_hex,file_size_bytes=:file_size_bytes,"
            "width=:width,height=:height,note=:note,remote=:remote,"
            "upload_state=:upload_state,tags_json=:tags_json,"
            "event_at_ms=:event_at_ms,thumbnail_path=:thumbnail_path,"
            "mime_type=:mime_type,original_width=:original_width,"
            "original_height=:original_height,digital=:digital,"
            "vis_valid=:vis_valid,fsk_id=:fsk_id,local_grid=:local_grid,"
            "remote_grid=:remote_grid,audio_frequency_hz=:audio_frequency_hz,"
            "source_sample_rate_hz=:source_sample_rate_hz,"
            "completion_percent=:completion_percent,quality_json=:quality_json,"
            "slant_correction_ppm=:slant_correction_ppm,"
            "raw_audio_path=:raw_audio_path,related_qso_id=:related_qso_id,"
            "remote_provider=:remote_provider,remote_object_id=:remote_object_id,"
            "expires_at_ms=:expires_at_ms,privacy_flags=:privacy_flags,"
            "favorite=:favorite "
            "WHERE id=:id"))
        || !bindRecord(query, record, &error)
        || !query.exec()) {
        database.rollback();
        if (error.isEmpty()) {
            error = queryFailure(QStringLiteral("update SSTV record"), query);
        }
        emitOperationFailure(requestId, SstvStorageOperation::Update, error);
        return;
    }
    if (query.numRowsAffected() != 1) {
        database.rollback();
        emitOperationFailure(requestId, SstvStorageOperation::Update,
                             QStringLiteral("SSTV record was not found"));
        return;
    }
    if (!replaceTags(database, record, &error) || !database.commit()) {
        if (error.isEmpty()) {
            error = databaseFailure(QStringLiteral("commit SSTV update"),
                                    database);
        }
        database.rollback();
        emitOperationFailure(requestId, SstvStorageOperation::Update, error);
        return;
    }
    m_retentionPlan.reset();
    emit operationFinished(requestId, SstvStorageOperation::Update, true, {});
    emit recordChanged(record);
}

void SstvStorageWorker::setFavorite(QString id,
                                    bool favorite,
                                    quint64 requestId)
{
    const auto reject = [this, requestId](const QString& error) {
        emitOperationFailure(requestId, SstvStorageOperation::SetFavorite,
                             error);
    };
    if (!requireOwnerThread(QStringLiteral("setFavorite"))) {
        reject(QStringLiteral("favourite update called outside owner thread"));
        return;
    }
    if (!m_initialized.load(std::memory_order_acquire)
        || !canonicalUuid(id)) {
        reject(m_initialized.load(std::memory_order_acquire)
            ? QStringLiteral("invalid SSTV record UUID")
            : QStringLiteral("SSTV storage is not initialized"));
        return;
    }

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery fetch(database);
    fetch.prepare(QStringLiteral(
        "SELECT %1 FROM sstv_images WHERE id=:id").arg(recordColumns()));
    fetch.bindValue(QStringLiteral(":id"), id);
    SstvImageRecord previous;
    QString error;
    const SstvImageStore store(m_layout, m_limits);
    if (!fetch.exec() || !fetch.next()
        || !readRecord(fetch, m_limits, &previous, &error)
        || !store.verify(previous, true, &error)) {
        reject(!error.isEmpty() ? error
            : fetch.lastError().isValid()
                ? queryFailure(QStringLiteral("fetch favourite record"), fetch)
                : QStringLiteral("SSTV record was not found"));
        return;
    }
    if (previous.favorite == favorite) {
        emit operationFinished(requestId, SstvStorageOperation::SetFavorite,
                               true, {});
        emit recordChanged(previous);
        return;
    }

    SstvImageRecord updated = previous;
    updated.favorite = favorite;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    updated.updatedAtUtc = now > previous.updatedAtUtc
        ? now : previous.updatedAtUtc.addMSecs(1);
    if (!updated.validate(m_limits, &error)
        || !store.updateMetadata(updated, &error)) {
        reject(error.isEmpty()
            ? QStringLiteral("could not update favourite sidecar") : error);
        return;
    }

    const auto restoreSidecar = [&]() {
        QString restoreError;
        if (!store.updateMetadata(previous, &restoreError)) {
            error += QStringLiteral(
                "; original sidecar restoration failed: %1")
                .arg(restoreError);
        }
    };
    if (!database.transaction()) {
        error = databaseFailure(
            QStringLiteral("begin favourite update"), database);
        restoreSidecar();
        reject(error);
        return;
    }
    QSqlQuery update(database);
    update.prepare(QStringLiteral(
        "UPDATE sstv_images SET favorite=:favorite,updated_at_ms=:updated "
        "WHERE id=:id AND favorite=:previous_favorite "
        "AND updated_at_ms=:previous_updated"));
    update.bindValue(QStringLiteral(":favorite"), favorite ? 1 : 0);
    update.bindValue(QStringLiteral(":updated"),
                     updated.updatedAtUtc.toMSecsSinceEpoch());
    update.bindValue(QStringLiteral(":id"), id);
    update.bindValue(QStringLiteral(":previous_favorite"),
                     previous.favorite ? 1 : 0);
    update.bindValue(QStringLiteral(":previous_updated"),
                     previous.updatedAtUtc.toMSecsSinceEpoch());
    if (!update.exec() || update.numRowsAffected() != 1
        || !database.commit()) {
        error = update.lastError().isValid()
            ? queryFailure(QStringLiteral("update favourite"), update)
            : database.lastError().isValid()
                ? databaseFailure(QStringLiteral("commit favourite update"),
                                  database)
                : QStringLiteral("favourite record changed concurrently");
        database.rollback();
        restoreSidecar();
        reject(error);
        return;
    }
    m_retentionPlan.reset();
    emit operationFinished(requestId, SstvStorageOperation::SetFavorite,
                           true, {});
    emit recordChanged(updated);
}

void SstvStorageWorker::associateWithQso(QString imageId,
                                         QString qsoId,
                                         quint64 requestId)
{
    const auto reject = [this, requestId](const QString& error) {
        emitOperationFailure(requestId, SstvStorageOperation::AssociateQso,
                             error);
    };
    if (!requireOwnerThread(QStringLiteral("associateWithQso"))) {
        reject(QStringLiteral(
            "QSO association called outside the storage owner thread"));
        return;
    }
    if (!m_initialized.load(std::memory_order_acquire)) {
        reject(QStringLiteral("SSTV storage is not initialized"));
        return;
    }
    if (!canonicalUuid(imageId)) {
        reject(QStringLiteral("invalid SSTV record UUID"));
        return;
    }
    if (qsoId.isNull()) {
        qsoId = QStringLiteral("");
    }
    QString error;
    if (!validateSstvQsoId(qsoId, true, &error)) {
        reject(error);
        return;
    }

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery fetch(database);
    fetch.prepare(QStringLiteral(
        "SELECT %1 FROM sstv_images WHERE id=:id").arg(recordColumns()));
    fetch.bindValue(QStringLiteral(":id"), imageId);
    SstvImageRecord previous;
    const SstvImageStore store(m_layout, m_limits);
    if (!fetch.exec() || !fetch.next()
        || !readRecord(fetch, m_limits, &previous, &error)
        || !store.verify(previous, true, &error)) {
        reject(!error.isEmpty() ? error
            : fetch.lastError().isValid()
                ? queryFailure(QStringLiteral("fetch QSO image record"), fetch)
                : QStringLiteral("SSTV record was not found"));
        return;
    }
    if (previous.relatedQsoId == qsoId) {
        emit operationFinished(requestId, SstvStorageOperation::AssociateQso,
                               true, {});
        emit recordChanged(previous);
        return;
    }

    SstvImageRecord updated = previous;
    updated.relatedQsoId = qsoId;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    updated.updatedAtUtc = now > previous.updatedAtUtc
        ? now : previous.updatedAtUtc.addMSecs(1);
    if (!updated.validate(m_limits, &error)
        || !store.updateMetadata(updated, &error)) {
        reject(error.isEmpty()
            ? QStringLiteral("could not update QSO association sidecar")
            : error);
        return;
    }

    const auto restoreSidecar = [&]() {
        QString restoreError;
        if (!store.updateMetadata(previous, &restoreError)) {
            error += QStringLiteral(
                "; original sidecar restoration failed: %1")
                .arg(restoreError);
        }
    };
    if (!database.transaction()) {
        error = databaseFailure(
            QStringLiteral("begin QSO association update"), database);
        restoreSidecar();
        reject(error);
        return;
    }
    QSqlQuery update(database);
    update.prepare(QStringLiteral(
        "UPDATE sstv_images SET related_qso_id=:qso_id,"
        "updated_at_ms=:updated WHERE id=:id "
        "AND related_qso_id=:previous_qso_id "
        "AND updated_at_ms=:previous_updated"));
    update.bindValue(QStringLiteral(":qso_id"), qsoId);
    update.bindValue(QStringLiteral(":updated"),
                     updated.updatedAtUtc.toMSecsSinceEpoch());
    update.bindValue(QStringLiteral(":id"), imageId);
    update.bindValue(QStringLiteral(":previous_qso_id"),
                     previous.relatedQsoId);
    update.bindValue(QStringLiteral(":previous_updated"),
                     previous.updatedAtUtc.toMSecsSinceEpoch());
    if (!update.exec() || update.numRowsAffected() != 1
        || !database.commit()) {
        error = update.lastError().isValid()
            ? queryFailure(QStringLiteral("update QSO association"), update)
            : database.lastError().isValid()
                ? databaseFailure(
                    QStringLiteral("commit QSO association update"), database)
                : QStringLiteral("QSO image record changed concurrently");
        database.rollback();
        restoreSidecar();
        reject(error);
        return;
    }
    m_retentionPlan.reset();
    emit operationFinished(requestId, SstvStorageOperation::AssociateQso,
                           true, {});
    emit recordChanged(updated);
}

void SstvStorageWorker::updateUserMetadata(QString imageId,
                                            QString note,
                                            QStringList tags,
                                            quint64 requestId)
{
    const auto reject = [this, requestId](const QString& error) {
        emitOperationFailure(requestId,
                             SstvStorageOperation::UpdateUserMetadata,
                             error);
    };
    if (!requireOwnerThread(QStringLiteral("updateUserMetadata"))) {
        reject(QStringLiteral(
            "metadata update called outside the storage owner thread"));
        return;
    }
    if (!m_initialized.load(std::memory_order_acquire)) {
        reject(QStringLiteral("SSTV storage is not initialized"));
        return;
    }
    if (!canonicalUuid(imageId)) {
        reject(QStringLiteral("invalid SSTV record UUID"));
        return;
    }
    normalizeUserMetadata(&note, &tags);

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery fetch(database);
    fetch.prepare(QStringLiteral(
        "SELECT %1 FROM sstv_images WHERE id=:id").arg(recordColumns()));
    fetch.bindValue(QStringLiteral(":id"), imageId);
    SstvImageRecord previous;
    QString error;
    const SstvImageStore store(m_layout, m_limits);
    if (!fetch.exec() || !fetch.next()
        || !readRecord(fetch, m_limits, &previous, &error)
        || !store.verify(previous, true, &error)) {
        reject(!error.isEmpty() ? error
            : fetch.lastError().isValid()
                ? queryFailure(QStringLiteral("fetch metadata record"), fetch)
                : QStringLiteral("SSTV record was not found"));
        return;
    }
    if (previous.note == note && previous.tags == tags) {
        emit operationFinished(requestId,
                               SstvStorageOperation::UpdateUserMetadata,
                               true, {});
        emit recordChanged(previous);
        return;
    }

    SstvImageRecord updated = previous;
    updated.note = std::move(note);
    updated.tags = std::move(tags);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    updated.updatedAtUtc = now > previous.updatedAtUtc
        ? now : previous.updatedAtUtc.addMSecs(1);
    if (!updated.validate(m_limits, &error)
        || !store.updateMetadata(updated, &error)) {
        reject(error.isEmpty()
            ? QStringLiteral("could not update Gallery metadata sidecar")
            : error);
        return;
    }

    const auto restoreSidecar = [&]() {
        QString restoreError;
        if (!store.updateMetadata(previous, &restoreError)) {
            error += QStringLiteral(
                "; original sidecar restoration failed: %1")
                .arg(restoreError);
        }
    };
    if (!database.transaction()) {
        error = databaseFailure(
            QStringLiteral("begin Gallery metadata update"), database);
        restoreSidecar();
        reject(error);
        return;
    }
    QSqlQuery update(database);
    if (!update.prepare(QStringLiteral(
            "UPDATE sstv_images SET note=:note,tags_json=:tags_json,"
            "updated_at_ms=:updated WHERE id=:id "
            "AND updated_at_ms=:previous_updated"))) {
        error = queryFailure(QStringLiteral("prepare Gallery metadata update"),
                             update);
        database.rollback();
        restoreSidecar();
        reject(error);
        return;
    }
    update.bindValue(QStringLiteral(":note"), updated.note);
    update.bindValue(QStringLiteral(":tags_json"), tagsJson(updated.tags));
    update.bindValue(QStringLiteral(":updated"),
                     updated.updatedAtUtc.toMSecsSinceEpoch());
    update.bindValue(QStringLiteral(":id"), imageId);
    update.bindValue(QStringLiteral(":previous_updated"),
                     previous.updatedAtUtc.toMSecsSinceEpoch());
    if (!update.exec() || update.numRowsAffected() != 1
        || !replaceTags(database, updated, &error) || !database.commit()) {
        if (error.isEmpty()) {
            error = update.lastError().isValid()
                ? queryFailure(QStringLiteral("update Gallery metadata"), update)
                : database.lastError().isValid()
                    ? databaseFailure(
                        QStringLiteral("commit Gallery metadata update"),
                        database)
                    : QStringLiteral("Gallery record changed concurrently");
        }
        database.rollback();
        restoreSidecar();
        reject(error);
        return;
    }
    m_retentionPlan.reset();
    emit operationFinished(requestId,
                           SstvStorageOperation::UpdateUserMetadata,
                           true, {});
    emit recordChanged(updated);
}

void SstvStorageWorker::removeRecord(QString id, quint64 requestId)
{
    if (!requireOwnerThread(QStringLiteral("removeRecord"))) {
        emitOperationFailure(requestId, SstvStorageOperation::Remove,
                             QStringLiteral("remove called outside owner thread"));
        return;
    }
    const bool initialized = m_initialized.load(std::memory_order_acquire);
    if (!initialized || !canonicalUuid(id)) {
        emitOperationFailure(requestId, SstvStorageOperation::Remove,
                             initialized
                                 ? QStringLiteral("invalid SSTV record UUID")
                                 : QStringLiteral("SSTV storage is not initialized"));
        return;
    }
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    query.prepare(QStringLiteral("DELETE FROM sstv_images WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        emitOperationFailure(requestId, SstvStorageOperation::Remove,
                             queryFailure(QStringLiteral("remove SSTV record"), query));
        return;
    }
    if (query.numRowsAffected() != 1) {
        emitOperationFailure(requestId, SstvStorageOperation::Remove,
                             QStringLiteral("SSTV record was not found"));
        return;
    }
    m_retentionPlan.reset();
    emit operationFinished(requestId, SstvStorageOperation::Remove, true, {});
    emit recordsRemoved(QStringList {id}, requestId);
}

void SstvStorageWorker::removeRecords(QStringList ids, quint64 requestId)
{
    if (!requireOwnerThread(QStringLiteral("removeRecords"))) {
        emitOperationFailure(requestId, SstvStorageOperation::RemoveMany,
                             QStringLiteral("bulk remove called outside owner thread"));
        return;
    }
    if (!m_initialized.load(std::memory_order_acquire)) {
        emitOperationFailure(requestId, SstvStorageOperation::RemoveMany,
                             QStringLiteral("SSTV storage is not initialized"));
        return;
    }
    if (ids.isEmpty() || ids.size() > 500) {
        emitOperationFailure(requestId, SstvStorageOperation::RemoveMany,
                             QStringLiteral("bulk remove requires 1..500 UUIDs"));
        return;
    }
    QSet<QString> unique;
    for (const QString& id : ids) {
        if (!canonicalUuid(id) || unique.contains(id)) {
            emitOperationFailure(requestId, SstvStorageOperation::RemoveMany,
                                 QStringLiteral("bulk remove contains an invalid or duplicate UUID"));
            return;
        }
        unique.insert(id);
    }

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.transaction()) {
        emitOperationFailure(requestId, SstvStorageOperation::RemoveMany,
                             databaseFailure(QStringLiteral("begin bulk SSTV remove"),
                                             database));
        return;
    }
    QSqlQuery remove(database);
    if (!remove.prepare(QStringLiteral(
            "DELETE FROM sstv_images WHERE id=:id"))) {
        database.rollback();
        emitOperationFailure(requestId, SstvStorageOperation::RemoveMany,
                             queryFailure(QStringLiteral("prepare bulk SSTV remove"),
                                          remove));
        return;
    }
    for (const QString& id : ids) {
        remove.bindValue(QStringLiteral(":id"), id);
        if (!remove.exec() || remove.numRowsAffected() != 1) {
            const QString error = remove.lastError().isValid()
                ? queryFailure(QStringLiteral("bulk SSTV remove"), remove)
                : QStringLiteral("bulk SSTV remove record was not found");
            database.rollback();
            emitOperationFailure(requestId, SstvStorageOperation::RemoveMany,
                                 error);
            return;
        }
    }
    if (!database.commit()) {
        const QString error = databaseFailure(
            QStringLiteral("commit bulk SSTV remove"), database);
        database.rollback();
        emitOperationFailure(requestId, SstvStorageOperation::RemoveMany,
                             error);
        return;
    }
    m_retentionPlan.reset();
    emit operationFinished(requestId, SstvStorageOperation::RemoveMany,
                           true, {});
    emit recordsRemoved(ids, requestId);
}

void SstvStorageWorker::deleteRecordsWithFiles(QStringList ids,
                                                quint64 requestId)
{
    const auto failDelete = [this, requestId](const QString& error) {
        emitOperationFailure(requestId,
                             SstvStorageOperation::DeleteManyFiles, error);
    };
    if (!requireOwnerThread(QStringLiteral("deleteRecordsWithFiles"))) {
        failDelete(QStringLiteral("file deletion called outside owner thread"));
        return;
    }
    if (!m_initialized.load(std::memory_order_acquire)) {
        failDelete(QStringLiteral("SSTV storage is not initialized"));
        return;
    }
    if (ids.isEmpty() || ids.size() > 500) {
        failDelete(QStringLiteral("file deletion requires 1..500 UUIDs"));
        return;
    }
    ids.sort();
    QSet<QString> uniqueIds;
    for (const QString& id : std::as_const(ids)) {
        if (!canonicalUuid(id) || uniqueIds.contains(id)) {
            failDelete(QStringLiteral(
                "file deletion contains an invalid or duplicate UUID"));
            return;
        }
        uniqueIds.insert(id);
    }

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    QVector<SstvImageRecord> records;
    records.reserve(ids.size());
    QSqlQuery fetch(database);
    if (!fetch.prepare(QStringLiteral(
            "SELECT %1 FROM sstv_images WHERE id=:id").arg(recordColumns()))) {
        failDelete(queryFailure(QStringLiteral(
            "prepare SSTV file-deletion lookup"), fetch));
        return;
    }
    const SstvImageStore store(m_layout, m_limits);
    for (const QString& id : std::as_const(ids)) {
        fetch.bindValue(QStringLiteral(":id"), id);
        if (!fetch.exec() || !fetch.next()) {
            failDelete(fetch.lastError().isValid()
                ? queryFailure(QStringLiteral("fetch SSTV file-deletion record"),
                               fetch)
                : QStringLiteral("SSTV file-deletion record was not found"));
            return;
        }
        QString error;
        SstvImageRecord record;
        if (!readRecord(fetch, m_limits, &record, &error)
            || !store.verify(record, true, &error)) {
            failDelete(error.isEmpty()
                ? QStringLiteral("SSTV file-deletion record failed verification")
                : error);
            return;
        }
        records.append(std::move(record));
        fetch.finish();
    }

    const auto databaseReferenceCount = [&database](
        const QString& column, const QString& path, qint64* count,
        QString* error) {
        static const QSet<QString> allowedColumns {
            QStringLiteral("image_path"), QStringLiteral("metadata_path"),
            QStringLiteral("thumbnail_path"), QStringLiteral("raw_audio_path")};
        if (!count || !allowedColumns.contains(column)) {
            return fail(error, QStringLiteral("invalid file reference query"));
        }
        QSqlQuery query(database);
        query.prepare(QStringLiteral("SELECT COUNT(*) FROM sstv_images WHERE %1=:path")
                          .arg(column));
        query.bindValue(QStringLiteral(":path"), path);
        if (!query.exec() || !query.next()) {
            return fail(error, queryFailure(
                QStringLiteral("count SSTV file references"), query));
        }
        *count = query.value(0).toLongLong();
        return true;
    };
    const auto selectedReferenceCount = [&records](
        const QString& path,
        const std::function<QString(const SstvImageRecord&)>& accessor) {
        qint64 count = 0;
        for (const SstvImageRecord& record : records) {
            if (accessor(record) == path) {
                ++count;
            }
        }
        return count;
    };

    struct PlannedFile final
    {
        QString original;
        QString staged;
    };
    QVector<PlannedFile> planned;
    QSet<QString> plannedOriginals;
    QString planningError;
    const QString stagingRoot = QDir(m_layout.rootPath()).absoluteFilePath(
        QStringLiteral(".delete-staging"));
    if (!QDir().mkpath(stagingRoot)
        || !QFile::setPermissions(stagingRoot,
                                 QFileDevice::ReadOwner
                                     | QFileDevice::WriteOwner
                                     | QFileDevice::ExeOwner)) {
        failDelete(QStringLiteral("could not create private deletion staging"));
        return;
    }
    const QString stageName = QStringLiteral("%1-%2")
        .arg(requestId)
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!QDir(stagingRoot).mkdir(stageName)) {
        failDelete(QStringLiteral("could not allocate private deletion staging"));
        return;
    }
    const QString stageDirectory = QDir(stagingRoot).absoluteFilePath(stageName);
    if (!QFile::setPermissions(stageDirectory,
                               QFileDevice::ReadOwner
                                   | QFileDevice::WriteOwner
                                   | QFileDevice::ExeOwner)) {
        QDir(stagingRoot).rmdir(stageName);
        failDelete(QStringLiteral("could not restrict deletion staging permissions"));
        return;
    }

    const auto planPath = [&](const QString& path,
                              const QString& kind,
                              bool required,
                              bool deleteWhenExclusive,
                              const QString& databaseColumn,
                              const std::function<QString(
                                  const SstvImageRecord&)>& accessor) {
        if (!planningError.isEmpty() || path.isEmpty()
            || plannedOriginals.contains(path)) {
            return;
        }
        const QFileInfo info(path);
        if (!info.exists()) {
            if (required) {
                planningError = QStringLiteral(
                    "a required indexed SSTV file is missing");
            }
            return;
        }
        if (!info.isFile() || info.isSymLink()
            || !m_layout.containsPath(path, true, &planningError)) {
            if (planningError.isEmpty()) {
                planningError = QStringLiteral(
                    "an indexed SSTV deletion path is unsafe");
            }
            return;
        }
        qint64 totalReferences = 0;
        if (!databaseReferenceCount(databaseColumn, path,
                                    &totalReferences, &planningError)) {
            return;
        }
        const qint64 selectedReferences = selectedReferenceCount(path, accessor);
        if (totalReferences != selectedReferences) {
            if (required || !deleteWhenExclusive) {
                planningError = QStringLiteral(
                    "an indexed SSTV file is still referenced by an unselected record");
            }
            return;
        }
        const QString suffix = info.suffix().isEmpty()
            ? QString {} : QStringLiteral(".") + info.suffix();
        const QString staged = QDir(stageDirectory).absoluteFilePath(
            QStringLiteral("%1-%2%3")
                .arg(planned.size(), 4, 10, QLatin1Char('0'))
                .arg(kind, suffix));
        planned.append({path, staged});
        plannedOriginals.insert(path);
    };
    const auto imagePath = [](const SstvImageRecord& record) {
        return record.imagePath;
    };
    const auto metadataPath = [](const SstvImageRecord& record) {
        return record.metadataPath;
    };
    const auto thumbnailPath = [](const SstvImageRecord& record) {
        return record.thumbnailPath;
    };
    const auto rawAudioPath = [](const SstvImageRecord& record) {
        return record.rawAudioPath;
    };
    for (const SstvImageRecord& record : std::as_const(records)) {
        planPath(record.imagePath, QStringLiteral("image"), true, true,
                 QStringLiteral("image_path"), imagePath);
        planPath(record.metadataPath, QStringLiteral("metadata"), true, true,
                 QStringLiteral("metadata_path"), metadataPath);
        planPath(record.thumbnailPath, QStringLiteral("thumbnail"), false, true,
                 QStringLiteral("thumbnail_path"), thumbnailPath);
        planPath(record.rawAudioPath, QStringLiteral("raw-audio"), false, true,
                 QStringLiteral("raw_audio_path"), rawAudioPath);
    }
    if (!planningError.isEmpty()) {
        QDir(stagingRoot).rmdir(stageName);
        failDelete(planningError);
        return;
    }

    QJsonArray journalIds;
    for (const QString& id : std::as_const(ids)) {
        journalIds.append(id);
    }
    QJsonArray journalFiles;
    for (const PlannedFile& file : std::as_const(planned)) {
        journalFiles.append(QJsonObject {
            {QStringLiteral("original"), file.original},
            {QStringLiteral("staged"), QFileInfo(file.staged).fileName()},
        });
    }
    const QJsonObject journalObject {
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("recordIds"), journalIds},
        {QStringLiteral("files"), journalFiles},
    };
    const QString journalPath = QDir(stageDirectory).absoluteFilePath(
        QStringLiteral("journal.json"));
    QSaveFile journal(journalPath);
    journal.setDirectWriteFallback(false);
    const QByteArray journalBytes = QJsonDocument(journalObject).toJson(
        QJsonDocument::Compact);
    if (journalBytes.isEmpty() || journalBytes.size() > 1024 * 1024
        || !journal.open(QIODevice::WriteOnly)
        || !journal.setPermissions(QFileDevice::ReadOwner
                                   | QFileDevice::WriteOwner)
        || journal.write(journalBytes) != journalBytes.size()
        || !journal.commit()) {
        journal.cancelWriting();
        QFile::remove(journalPath);
        QDir(stagingRoot).rmdir(stageName);
        failDelete(QStringLiteral(
            "could not persist the SSTV deletion recovery journal"));
        return;
    }

    const auto removeJournalAndStage = [&]() {
        const bool journalRemoved = !QFileInfo::exists(journalPath)
            || QFile::remove(journalPath);
        return journalRemoved && QDir(stagingRoot).rmdir(stageName);
    };

    qsizetype movedCount = 0;
    const auto restoreMoved = [&planned, &movedCount]() {
        bool restored = true;
        while (movedCount > 0) {
            --movedCount;
            const PlannedFile& file = planned.at(movedCount);
            if (QFileInfo::exists(file.original)
                || !QFile::rename(file.staged, file.original)) {
                restored = false;
            }
        }
        return restored;
    };
    for (const PlannedFile& file : std::as_const(planned)) {
        if (QFileInfo::exists(file.staged)
            || !QFile::rename(file.original, file.staged)) {
            const bool restored = restoreMoved();
            const bool cleaned = restored && removeJournalAndStage();
            failDelete(cleaned
                ? QStringLiteral("could not stage every SSTV file for deletion")
                : QStringLiteral("deletion staging failed and file restoration is incomplete"));
            return;
        }
        ++movedCount;
    }

    if (!database.transaction()) {
        const bool restored = restoreMoved();
        const bool cleaned = restored && removeJournalAndStage();
        failDelete(cleaned
            ? databaseFailure(QStringLiteral("begin SSTV file deletion"), database)
            : QStringLiteral("database transaction failed and file restoration is incomplete"));
        return;
    }
    QSqlQuery remove(database);
    remove.prepare(QStringLiteral("DELETE FROM sstv_images WHERE id=:id"));
    QString databaseError;
    for (const QString& id : std::as_const(ids)) {
        remove.bindValue(QStringLiteral(":id"), id);
        if (!remove.exec() || remove.numRowsAffected() != 1) {
            databaseError = remove.lastError().isValid()
                ? queryFailure(QStringLiteral("delete SSTV record and files"),
                               remove)
                : QStringLiteral("SSTV file-deletion record disappeared");
            break;
        }
    }
    if (!databaseError.isEmpty() || !database.commit()) {
        if (databaseError.isEmpty()) {
            databaseError = databaseFailure(
                QStringLiteral("commit SSTV file deletion"), database);
        }
        database.rollback();
        const bool restored = restoreMoved();
        const bool cleaned = restored && removeJournalAndStage();
        failDelete(cleaned ? databaseError
                           : databaseError + QStringLiteral(
                                 "; staged file restoration is incomplete"));
        return;
    }

    QStringList cleanupFailures;
    for (const PlannedFile& file : std::as_const(planned)) {
        if (QFileInfo::exists(file.staged) && !QFile::remove(file.staged)) {
            cleanupFailures.append(QFileInfo(file.original).fileName());
        }
    }
    if (cleanupFailures.isEmpty() && !removeJournalAndStage()) {
        cleanupFailures.append(QStringLiteral("deletion-journal"));
    }
    const QString warning = cleanupFailures.isEmpty()
        ? QString {}
        : QStringLiteral(
              "records were deleted but %1 staged file(s) need later cleanup")
              .arg(cleanupFailures.size());
    m_retentionPlan.reset();
    emit operationFinished(requestId,
                           SstvStorageOperation::DeleteManyFiles,
                           true, warning);
    emit recordsRemoved(ids, requestId);
    emit recordsDeletedWithFiles(ids, requestId, warning);
}

void SstvStorageWorker::fetchRecord(QString id, quint64 requestId)
{
    SstvImageRecord empty;
    if (!requireOwnerThread(QStringLiteral("fetchRecord"))) {
        emit recordFetched(requestId, false, empty,
                           QStringLiteral("fetch called outside owner thread"));
        return;
    }
    const bool initialized = m_initialized.load(std::memory_order_acquire);
    if (!initialized || !canonicalUuid(id)) {
        emit recordFetched(requestId, false, empty,
                           initialized
                               ? QStringLiteral("invalid SSTV record UUID")
                               : QStringLiteral("SSTV storage is not initialized"));
        return;
    }
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT %1 FROM sstv_images WHERE id=:id")
                      .arg(recordColumns()));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        emit recordFetched(requestId, false, empty,
                           queryFailure(QStringLiteral("fetch SSTV record"), query));
        return;
    }
    if (!query.next()) {
        emit recordFetched(requestId, false, empty, {});
        return;
    }
    QString error;
    SstvImageRecord record;
    if (!readRecord(query, m_limits, &record, &error)) {
        emit recordFetched(requestId, false, empty, error);
        return;
    }
    emit recordFetched(requestId, true, record, {});
}

void SstvStorageWorker::exportRecord(QString id,
                                     QString destinationPath,
                                     bool replaceExisting,
                                     quint64 requestId)
{
    const auto finish = [this, requestId](bool ok,
                                          const QString& path,
                                          const QString& error) {
        emit operationFinished(requestId, SstvStorageOperation::Export,
                               ok, error);
        emit recordExported(requestId, ok, path, error);
    };
    if (!requireOwnerThread(QStringLiteral("exportRecord"))) {
        finish(false, {}, QStringLiteral("export called outside owner thread"));
        return;
    }
    if (!m_initialized.load(std::memory_order_acquire) || !canonicalUuid(id)) {
        finish(false, {}, m_initialized.load(std::memory_order_acquire)
                   ? QStringLiteral("invalid SSTV record UUID")
                   : QStringLiteral("SSTV storage is not initialized"));
        return;
    }

    const QFileInfo requested(QDir::cleanPath(destinationPath));
    const QFileInfo parentInfo(requested.absolutePath());
    if (!requested.isAbsolute()
        || requested.suffix().compare(QStringLiteral("png"),
                                      Qt::CaseInsensitive) != 0
        || requested.fileName().toUtf8().size() > 255
        || !parentInfo.exists() || !parentInfo.isDir()
        || parentInfo.isSymLink()
        || parentInfo.canonicalFilePath().isEmpty()) {
        finish(false, {}, QStringLiteral(
            "export destination must be an existing non-linked directory and a PNG filename"));
        return;
    }
    const QString destination = QDir(parentInfo.canonicalFilePath())
        .absoluteFilePath(requested.fileName());
    const QFileInfo destinationInfo(destination);
    if (destinationInfo.isSymLink()
        || (destinationInfo.exists() && !destinationInfo.isFile())
        || (destinationInfo.exists() && !replaceExisting)) {
        finish(false, {}, destinationInfo.exists() && !replaceExisting
                   ? QStringLiteral("export destination already exists")
                   : QStringLiteral("export destination is unsafe"));
        return;
    }

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT %1 FROM sstv_images WHERE id=:id")
                      .arg(recordColumns()));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec() || !query.next()) {
        finish(false, {}, query.lastError().isValid()
                   ? queryFailure(QStringLiteral("fetch SSTV export record"),
                                  query)
                   : QStringLiteral("SSTV export record was not found"));
        return;
    }
    QString error;
    SstvImageRecord record;
    const SstvImageStore store(m_layout, m_limits);
    if (!readRecord(query, m_limits, &record, &error)
        || !store.verify(record, true, &error)) {
        finish(false, {}, error.isEmpty()
                   ? QStringLiteral("indexed SSTV image failed verification")
                   : error);
        return;
    }
    if (QFileInfo(record.imagePath).canonicalFilePath()
            == destinationInfo.canonicalFilePath()
        || QDir::cleanPath(record.imagePath) == QDir::cleanPath(destination)) {
        finish(false, {}, QStringLiteral(
            "export destination cannot replace the indexed source"));
        return;
    }

    QFile source(record.imagePath);
    QSaveFile output(destination);
    output.setDirectWriteFallback(false);
    if (!source.open(QIODevice::ReadOnly)
        || !output.open(QIODevice::WriteOnly)
        || !output.setPermissions(QFileDevice::ReadOwner
                                  | QFileDevice::WriteOwner)) {
        output.cancelWriting();
        finish(false, {}, QStringLiteral("could not open atomic SSTV export"));
        return;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 copied = 0;
    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    while (copied < record.fileSizeBytes) {
        const qint64 wanted = std::min<qint64>(
            buffer.size(), record.fileSizeBytes - copied);
        const qint64 count = source.read(buffer.data(), wanted);
        if (count <= 0 || output.write(buffer.constData(), count) != count) {
            output.cancelWriting();
            finish(false, {}, QStringLiteral("SSTV export copy failed"));
            return;
        }
        hash.addData(QByteArrayView(buffer.constData(), count));
        copied += count;
    }
    if (!source.atEnd() || copied != record.fileSizeBytes
        || hash.result() != record.sha256 || !output.commit()) {
        output.cancelWriting();
        finish(false, {}, QStringLiteral(
            "SSTV source changed or atomic export commit failed"));
        return;
    }
    finish(true, destination, {});
}

void SstvStorageWorker::listRecords(SstvImagePageRequest request,
                                    quint64 requestId)
{
    SstvImagePage page;
    if (!requireOwnerThread(QStringLiteral("listRecords"))) {
        emit pageFetched(requestId, page,
                         QStringLiteral("list called outside owner thread"));
        return;
    }
    QString error;
    if (!m_initialized.load(std::memory_order_acquire)
        || !request.validate(&error)) {
        if (error.isEmpty()) {
            error = QStringLiteral("SSTV storage is not initialized");
        }
        emit pageFetched(requestId, page, error);
        return;
    }

    QString sql = QStringLiteral("SELECT %1 FROM sstv_images WHERE 1=1")
                      .arg(recordColumns());
    if (request.categoryFilter != 0) {
        sql += QStringLiteral(" AND category=:category");
    }
    if (!request.modeFilter.isEmpty()) {
        sql += QStringLiteral(" AND mode=:mode COLLATE NOCASE");
    }
    if (request.hasCursor) {
        sql += QStringLiteral(
            " AND (captured_at_ms<:before_ms OR "
            "(captured_at_ms=:before_ms AND id<:before_id))");
    }
    sql += QStringLiteral(
        " ORDER BY captured_at_ms DESC,id DESC LIMIT :fetch_limit");

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    if (!query.prepare(sql)) {
        emit pageFetched(requestId, page,
                         queryFailure(QStringLiteral("prepare SSTV page"), query));
        return;
    }
    if (request.categoryFilter != 0) {
        query.bindValue(QStringLiteral(":category"), request.categoryFilter);
    }
    if (!request.modeFilter.isEmpty()) {
        query.bindValue(QStringLiteral(":mode"), request.modeFilter);
    }
    if (request.hasCursor) {
        query.bindValue(QStringLiteral(":before_ms"),
                        request.beforeCapturedAtMs);
        query.bindValue(QStringLiteral(":before_id"), request.beforeId);
    }
    query.bindValue(QStringLiteral(":fetch_limit"), request.limit + 1);
    if (!query.exec()) {
        emit pageFetched(requestId, page,
                         queryFailure(QStringLiteral("fetch SSTV page"), query));
        return;
    }

    page.records.reserve(request.limit);
    while (query.next()) {
        if (page.records.size() == request.limit) {
            page.hasMore = true;
            break;
        }
        SstvImageRecord record;
        if (!readRecord(query, m_limits, &record, &error)) {
            emit pageFetched(requestId, {}, error);
            return;
        }
        page.records.append(std::move(record));
    }
    if (page.hasMore && !page.records.isEmpty()) {
        const SstvImageRecord& last = page.records.constLast();
        page.nextBeforeCapturedAtMs = last.capturedAtUtc.toMSecsSinceEpoch();
        page.nextBeforeId = last.id;
    }
    emit pageFetched(requestId, page, {});
}

void SstvStorageWorker::loadRetentionSettings()
{
    if (!requireOwnerThread(QStringLiteral("loadRetentionSettings"))) {
        emit retentionSettingsLoaded({}, QStringLiteral(
            "retention settings read called outside owner thread"));
        return;
    }
    if (!m_initialized.load(std::memory_order_acquire)) {
        emit retentionSettingsLoaded({}, QStringLiteral(
            "SSTV storage is not initialized"));
        return;
    }
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    SstvRetentionSettings settings;
    QString error;
    if (!readRetentionSettings(database, &settings, &error)) {
        emit retentionSettingsLoaded({}, error);
        return;
    }
    emit retentionSettingsLoaded(settings, {});
}

void SstvStorageWorker::updateRetentionSettings(
    SstvRetentionSettings settings,
    quint64 requestId)
{
    const auto finish = [this, requestId, &settings](bool ok,
                                                     const QString& error) {
        emit operationFinished(
            requestId, SstvStorageOperation::UpdateRetentionSettings,
            ok, error);
        emit retentionSettingsUpdated(requestId, ok, settings, error);
    };
    if (!requireOwnerThread(QStringLiteral("updateRetentionSettings"))) {
        finish(false, QStringLiteral(
            "retention settings update called outside owner thread"));
        return;
    }
    QString error;
    if (!m_initialized.load(std::memory_order_acquire)
        || !settings.validate(&error)) {
        finish(false, error.isEmpty()
            ? QStringLiteral("SSTV storage is not initialized") : error);
        return;
    }
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.transaction()) {
        finish(false, databaseFailure(
            QStringLiteral("begin retention settings update"), database));
        return;
    }
    QSqlQuery update(database);
    update.prepare(QStringLiteral(
        "UPDATE sstv_retention_settings SET "
        "automatic_enabled=:automatic_enabled,"
        "maximum_age_days=:maximum_age_days,"
        "image_quota_bytes=:image_quota_bytes,"
        "thumbnail_quota_bytes=:thumbnail_quota_bytes,"
        "raw_audio_quota_bytes=:raw_audio_quota_bytes,"
        "shared_policy=:shared_policy,"
        "maximum_deletes_per_run=:maximum_deletes_per_run,"
        "updated_at_ms=:updated_at_ms WHERE id=1"));
    update.bindValue(QStringLiteral(":automatic_enabled"),
                     settings.automaticEnabled ? 1 : 0);
    update.bindValue(QStringLiteral(":maximum_age_days"),
                     settings.maximumAgeDays);
    update.bindValue(QStringLiteral(":image_quota_bytes"),
                     settings.imageQuotaBytes);
    update.bindValue(QStringLiteral(":thumbnail_quota_bytes"),
                     settings.thumbnailQuotaBytes);
    update.bindValue(QStringLiteral(":raw_audio_quota_bytes"),
                     settings.rawAudioQuotaBytes);
    update.bindValue(QStringLiteral(":shared_policy"),
                     static_cast<int>(settings.sharedPolicy));
    update.bindValue(QStringLiteral(":maximum_deletes_per_run"),
                     settings.maximumDeletesPerRun);
    update.bindValue(QStringLiteral(":updated_at_ms"),
                     QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
    if (!update.exec() || update.numRowsAffected() != 1
        || !database.commit()) {
        const QString detail = update.lastError().isValid()
            ? queryFailure(QStringLiteral("update retention settings"), update)
            : databaseFailure(QStringLiteral("commit retention settings"),
                              database);
        database.rollback();
        finish(false, detail);
        return;
    }
    m_retentionPlan.reset();
    finish(true, {});
}

void SstvStorageWorker::calculateQuota(quint64 requestId)
{
    const auto finish = [this, requestId](const SstvQuotaSummary& summary,
                                          const QString& error) {
        emit operationFinished(requestId, SstvStorageOperation::CalculateQuota,
                               error.isEmpty(), error);
        emit quotaCalculated(requestId, summary, error);
    };
    if (!requireOwnerThread(QStringLiteral("calculateQuota"))) {
        finish({}, QStringLiteral("quota calculation called outside owner thread"));
        return;
    }
    if (!m_initialized.load(std::memory_order_acquire)) {
        finish({}, QStringLiteral("SSTV storage is not initialized"));
        return;
    }
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    SstvRetentionSettings settings;
    SstvRetentionPlan unusedPlan;
    SstvQuotaSummary summary;
    QString error;
    if (!readRetentionSettings(database, &settings, &error)
        || !buildRetentionPlan(database, settings, &unusedPlan, &summary,
                               &error)) {
        finish({}, error);
        return;
    }
    finish(summary, {});
}

void SstvStorageWorker::previewRetention(SstvRetentionSettings settings,
                                         quint64 requestId)
{
    const auto finish = [this, requestId](const SstvRetentionPlan& plan,
                                          const QString& error) {
        emit operationFinished(requestId,
                               SstvStorageOperation::PreviewRetention,
                               error.isEmpty(), error);
        emit retentionPreviewReady(requestId, plan, error);
    };
    if (!requireOwnerThread(QStringLiteral("previewRetention"))) {
        finish({}, QStringLiteral("retention preview called outside owner thread"));
        return;
    }
    QString error;
    if (!m_initialized.load(std::memory_order_acquire)
        || !settings.validate(&error)) {
        finish({}, error.isEmpty()
            ? QStringLiteral("SSTV storage is not initialized") : error);
        return;
    }
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    SstvRetentionPlan plan;
    SstvQuotaSummary quota;
    if (!buildRetentionPlan(database, settings, &plan, &quota, &error)) {
        m_retentionPlan.reset();
        finish({}, error);
        return;
    }
    m_retentionPlan = plan;
    finish(plan, {});
}

void SstvStorageWorker::applyRetentionPlan(QString token,
                                           QString confirmationPhrase,
                                           quint64 requestId)
{
    const auto reject = [this, requestId](const QString& error) {
        emitOperationFailure(requestId, SstvStorageOperation::DeleteManyFiles,
                             error);
    };
    if (!requireOwnerThread(QStringLiteral("applyRetentionPlan"))) {
        reject(QStringLiteral("retention apply called outside owner thread"));
        return;
    }
    if (!m_initialized.load(std::memory_order_acquire)
        || !m_retentionPlan.has_value()) {
        reject(m_initialized.load(std::memory_order_acquire)
            ? QStringLiteral("retention preview token is stale or unknown")
            : QStringLiteral("SSTV storage is not initialized"));
        return;
    }
    const SstvRetentionPlan plan = *m_retentionPlan;
    if (token != plan.token || plan.recordIds.isEmpty()
        || confirmationPhrase != plan.confirmationPhrase
        || !plan.createdAtUtc.isValid()
        || plan.createdAtUtc.msecsTo(QDateTime::currentDateTimeUtc()) < 0
        || plan.createdAtUtc.msecsTo(QDateTime::currentDateTimeUtc())
               > 10LL * 60LL * 1000LL) {
        m_retentionPlan.reset();
        reject(QStringLiteral(
            "retention confirmation is invalid, empty or expired"));
        return;
    }

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery fetch(database);
    if (!fetch.prepare(QStringLiteral(
            "SELECT %1 FROM sstv_images WHERE id=:id").arg(recordColumns()))) {
        m_retentionPlan.reset();
        reject(queryFailure(QStringLiteral(
            "prepare retention revalidation"), fetch));
        return;
    }
    for (const QString& id : plan.recordIds) {
        fetch.bindValue(QStringLiteral(":id"), id);
        SstvImageRecord record;
        QString error;
        QString protection;
        if (!fetch.exec() || !fetch.next()
            || !readRecord(fetch, m_limits, &record, &error)
            || retentionRecordIsProtected(record, plan.settings,
                                          &protection, &error)) {
            m_retentionPlan.reset();
            reject(!error.isEmpty() ? error
                : protection.isEmpty()
                    ? QStringLiteral("retention record disappeared")
                    : QStringLiteral("retention record became protected: %1")
                          .arg(protection));
            return;
        }
        fetch.finish();
    }
    m_retentionPlan.reset();
    deleteRecordsWithFiles(plan.recordIds, requestId);
}

void SstvStorageWorker::runAutomaticRetention(quint64 requestId)
{
    const auto reject = [this, requestId](const QString& error) {
        emitOperationFailure(requestId, SstvStorageOperation::DeleteManyFiles,
                             error);
    };
    if (!requireOwnerThread(QStringLiteral("runAutomaticRetention"))) {
        reject(QStringLiteral("automatic retention called outside owner thread"));
        return;
    }
    if (!m_initialized.load(std::memory_order_acquire)) {
        reject(QStringLiteral("SSTV storage is not initialized"));
        return;
    }
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    SstvRetentionSettings settings;
    SstvRetentionPlan plan;
    SstvQuotaSummary quota;
    QString error;
    if (!readRetentionSettings(database, &settings, &error)
        || !settings.automaticEnabled
        || !buildRetentionPlan(database, settings, &plan, &quota, &error)) {
        reject(!error.isEmpty() ? error
            : QStringLiteral("automatic retention is disabled"));
        return;
    }
    if (plan.recordIds.isEmpty()) {
        emit operationFinished(requestId,
                               SstvStorageOperation::DeleteManyFiles,
                               true, plan.warning);
        return;
    }
    // Planning and deletion execute synchronously on the same storage owner
    // thread. DeleteManyFiles performs the final hash/path/reference checks
    // and uses the existing recovery journal.
    deleteRecordsWithFiles(plan.recordIds, requestId);
}

void SstvStorageWorker::queryGallery(SstvGalleryQuery request,
                                     quint64 requestId)
{
    SstvGalleryPage page;
    if (!requireOwnerThread(QStringLiteral("queryGallery"))) {
        emit galleryPageFetched(requestId, page,
                                QStringLiteral("gallery query called outside owner thread"));
        return;
    }
    QString error;
    if (!m_initialized.load(std::memory_order_acquire)
        || !request.validate(&error)) {
        if (error.isEmpty()) {
            error = QStringLiteral("SSTV storage is not initialized");
        }
        emit galleryPageFetched(requestId, page, error);
        return;
    }

    QStringList categories;
    if ((request.categoryMask & SstvGalleryReceived) != 0U) {
        categories.append(QString::number(
            static_cast<int>(SstvImageCategory::Received)));
    }
    if ((request.categoryMask & SstvGalleryTransmitted) != 0U) {
        categories.append(QString::number(
            static_cast<int>(SstvImageCategory::Transmitted)));
    }
    if ((request.categoryMask & SstvGalleryImported) != 0U) {
        categories.append(QString::number(
            static_cast<int>(SstvImageCategory::Imported)));
    }
    if ((request.categoryMask & SstvGalleryDraft) != 0U) {
        categories.append(QString::number(
            static_cast<int>(SstvImageCategory::Draft)));
    }

    QString sql = QStringLiteral(
        "SELECT %1 FROM sstv_images WHERE category IN (%2)")
        .arg(recordColumns(), categories.join(QLatin1Char(',')));
    if (request.remote != SstvGalleryTruthFilter::Any) {
        sql += QStringLiteral(" AND remote=:remote");
    }
    if (!request.mode.isEmpty()) {
        sql += QStringLiteral(" AND mode=:mode COLLATE NOCASE");
    }
    if (!request.callsign.isEmpty()) {
        sql += QStringLiteral(
            " AND (instr(lower(remote_callsign),lower(:callsign))>0 "
            "OR instr(lower(local_callsign),lower(:callsign))>0)");
    }
    if (request.capturedFromUtc.isValid()) {
        sql += QStringLiteral(" AND captured_at_ms>=:captured_from");
    }
    if (request.capturedToUtc.isValid()) {
        sql += QStringLiteral(" AND captured_at_ms<=:captured_to");
    }
    if (request.minimumFrequencyHz >= 0) {
        sql += QStringLiteral(" AND frequency_hz>=:minimum_frequency");
    }
    if (request.maximumFrequencyHz >= 0) {
        sql += QStringLiteral(" AND frequency_hz<=:maximum_frequency");
    }
    if (request.partial != SstvGalleryTruthFilter::Any) {
        sql += QStringLiteral(" AND complete=:complete");
    }
    if (request.uploadState >= 0) {
        sql += QStringLiteral(" AND upload_state=:upload_state");
    }
    if (!request.tags.isEmpty()) {
        if (request.requireAllTags) {
            for (qsizetype index = 0; index < request.tags.size(); ++index) {
                sql += QStringLiteral(
                    " AND EXISTS (SELECT 1 FROM sstv_image_tags tag_%1 "
                    "WHERE tag_%1.image_id=sstv_images.id "
                    "AND tag_%1.tag_folded=:tag_%1)").arg(index);
            }
        } else {
            QStringList placeholders;
            for (qsizetype index = 0; index < request.tags.size(); ++index) {
                placeholders.append(QStringLiteral(":tag_%1").arg(index));
            }
            sql += QStringLiteral(
                " AND EXISTS (SELECT 1 FROM sstv_image_tags selected_tag "
                "WHERE selected_tag.image_id=sstv_images.id "
                "AND selected_tag.tag_folded IN (%1))")
                .arg(placeholders.join(QLatin1Char(',')));
        }
    }
    if (!request.search.isEmpty()) {
        sql += QStringLiteral(
            " AND (instr(lower(mode),lower(:search))>0 "
            "OR instr(lower(remote_callsign),lower(:search))>0 "
            "OR instr(lower(local_callsign),lower(:search))>0 "
            "OR instr(lower(remote_grid),lower(:search))>0 "
            "OR instr(lower(local_grid),lower(:search))>0 "
            "OR instr(lower(fsk_id),lower(:search))>0 "
            "OR instr(lower(source),lower(:search))>0 "
            "OR instr(lower(note),lower(:search))>0 "
            "OR instr(lower(related_qso_id),lower(:search))>0 "
            "OR instr(lower(remote_provider),lower(:search))>0 "
            "OR instr(lower(remote_object_id),lower(:search))>0 "
            "OR EXISTS (SELECT 1 FROM sstv_image_tags search_tag "
            "WHERE search_tag.image_id=sstv_images.id "
            "AND instr(search_tag.tag_folded,:search_folded)>0))");
    }

    switch (request.sort) {
    case SstvGallerySort::CapturedNewest:
        sql += QStringLiteral(" ORDER BY captured_at_ms DESC,id DESC");
        break;
    case SstvGallerySort::CapturedOldest:
        sql += QStringLiteral(" ORDER BY captured_at_ms ASC,id ASC");
        break;
    case SstvGallerySort::CallsignAscending:
        sql += QStringLiteral(
            " ORDER BY lower(CASE WHEN remote_callsign<>'' "
            "THEN remote_callsign ELSE local_callsign END) ASC,id ASC");
        break;
    case SstvGallerySort::CallsignDescending:
        sql += QStringLiteral(
            " ORDER BY lower(CASE WHEN remote_callsign<>'' "
            "THEN remote_callsign ELSE local_callsign END) DESC,id DESC");
        break;
    case SstvGallerySort::ModeAscending:
        sql += QStringLiteral(" ORDER BY lower(mode) ASC,id ASC");
        break;
    case SstvGallerySort::ModeDescending:
        sql += QStringLiteral(" ORDER BY lower(mode) DESC,id DESC");
        break;
    case SstvGallerySort::FrequencyAscending:
        sql += QStringLiteral(" ORDER BY frequency_hz ASC,id ASC");
        break;
    case SstvGallerySort::FrequencyDescending:
        sql += QStringLiteral(" ORDER BY frequency_hz DESC,id DESC");
        break;
    case SstvGallerySort::UpdatedNewest:
        sql += QStringLiteral(" ORDER BY updated_at_ms DESC,id DESC");
        break;
    case SstvGallerySort::UpdatedOldest:
        sql += QStringLiteral(" ORDER BY updated_at_ms ASC,id ASC");
        break;
    }
    sql += QStringLiteral(" LIMIT :fetch_limit OFFSET :offset");

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(database);
    if (!query.prepare(sql)) {
        emit galleryPageFetched(
            requestId, page,
            queryFailure(QStringLiteral("prepare SSTV gallery query"), query));
        return;
    }
    if (request.remote != SstvGalleryTruthFilter::Any) {
        query.bindValue(QStringLiteral(":remote"),
                        request.remote == SstvGalleryTruthFilter::OnlyTrue
                            ? 1 : 0);
    }
    if (!request.mode.isEmpty()) {
        query.bindValue(QStringLiteral(":mode"), request.mode);
    }
    if (!request.callsign.isEmpty()) {
        query.bindValue(QStringLiteral(":callsign"), request.callsign);
    }
    if (request.capturedFromUtc.isValid()) {
        query.bindValue(QStringLiteral(":captured_from"),
                        request.capturedFromUtc.toMSecsSinceEpoch());
    }
    if (request.capturedToUtc.isValid()) {
        query.bindValue(QStringLiteral(":captured_to"),
                        request.capturedToUtc.toMSecsSinceEpoch());
    }
    if (request.minimumFrequencyHz >= 0) {
        query.bindValue(QStringLiteral(":minimum_frequency"),
                        request.minimumFrequencyHz);
    }
    if (request.maximumFrequencyHz >= 0) {
        query.bindValue(QStringLiteral(":maximum_frequency"),
                        request.maximumFrequencyHz);
    }
    if (request.partial != SstvGalleryTruthFilter::Any) {
        query.bindValue(QStringLiteral(":complete"),
                        request.partial == SstvGalleryTruthFilter::OnlyTrue
                            ? 0 : 1);
    }
    if (request.uploadState >= 0) {
        query.bindValue(QStringLiteral(":upload_state"), request.uploadState);
    }
    for (qsizetype index = 0; index < request.tags.size(); ++index) {
        query.bindValue(QStringLiteral(":tag_%1").arg(index),
                        request.tags.at(index).toCaseFolded());
    }
    if (!request.search.isEmpty()) {
        query.bindValue(QStringLiteral(":search"), request.search);
        query.bindValue(QStringLiteral(":search_folded"),
                        request.search.toCaseFolded());
    }
    query.bindValue(QStringLiteral(":fetch_limit"), request.limit + 1);
    query.bindValue(QStringLiteral(":offset"), request.offset);
    if (!query.exec()) {
        emit galleryPageFetched(
            requestId, page,
            queryFailure(QStringLiteral("fetch SSTV gallery page"), query));
        return;
    }

    page.records.reserve(request.limit);
    while (query.next()) {
        if (page.records.size() == request.limit) {
            page.hasMore = true;
            break;
        }
        SstvImageRecord record;
        if (!readRecord(query, m_limits, &record, &error)) {
            emit galleryPageFetched(requestId, {}, error);
            return;
        }
        page.records.append(std::move(record));
    }
    page.nextOffset = request.offset + static_cast<int>(page.records.size());
    emit galleryPageFetched(requestId, page, {});
}

} // namespace decodium::sstv
