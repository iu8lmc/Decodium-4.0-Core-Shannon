// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvGalleryModel.h"

#include "SstvThumbnailProvider.h"
#include "../storage/SstvStorageWorker.h"

#include <QMetaObject>
#include <QSet>
#include <QTimeZone>
#include <QVariant>

#include <algorithm>
#include <limits>
#include <utility>

namespace decodium::sstv {
namespace {

constexpr qsizetype kMaximumPendingQsoAssociations = 64;
constexpr qsizetype kMaximumPendingUserMetadataUpdates = 64;
constexpr qsizetype kMaximumUserMetadataNoteCharacters = 4096;
constexpr qsizetype kMaximumUserMetadataTags = 32;

QString normalizedUserNote(const QString& value)
{
    // Preserve meaningful leading/trailing whitespace and new lines in a
    // free-form note.  NFC only gives the sidecar and SQLite a stable spelling
    // for canonically equivalent Unicode input.
    return value.normalized(QString::NormalizationForm_C);
}

QStringList normalizedUserTags(QStringList values)
{
    for (QString& value : values) {
        value = value.trimmed().normalized(QString::NormalizationForm_C);
    }
    return values;
}

bool validUserMetadataText(const QString& value,
                           qsizetype maximumCharacters,
                           bool allowEmpty,
                           bool allowLineBreaks,
                           const QString& field,
                           QString* error)
{
    if (!allowEmpty && value.trimmed().isEmpty()) {
        if (error) {
            *error = field + QStringLiteral(" must not be empty");
        }
        return false;
    }
    if (value.size() > maximumCharacters) {
        if (error) {
            *error = field + QStringLiteral(" exceeds its length limit");
        }
        return false;
    }
    for (QChar character : value) {
        const ushort code = character.unicode();
        if (code == 0U || code == 0x7fU
            || (code < 0x20U
                && !(allowLineBreaks
                     && (code == '\n' || code == '\r' || code == '\t')))) {
            if (error) {
                *error = field + QStringLiteral(" contains control characters");
            }
            return false;
        }
    }
    return true;
}

bool validateUserMetadata(const QString& note,
                          const QStringList& tags,
                          QString* error)
{
    if (!validUserMetadataText(note, kMaximumUserMetadataNoteCharacters,
                               true, true, QStringLiteral("SSTV note"),
                               error)) {
        return false;
    }
    if (tags.size() > kMaximumUserMetadataTags) {
        if (error) {
            *error = QStringLiteral("SSTV record contains too many tags");
        }
        return false;
    }
    QSet<QString> foldedTags;
    for (const QString& tag : tags) {
        if (!validUserMetadataText(tag, 64, false, false,
                                   QStringLiteral("SSTV tag"), error)) {
            return false;
        }
        const QString folded = tag.toCaseFolded();
        if (folded.size() > 64) {
            if (error) {
                *error = QStringLiteral(
                    "SSTV case-folded tag exceeds its length limit");
            }
            return false;
        }
        if (foldedTags.contains(folded)) {
            if (error) {
                *error = QStringLiteral("SSTV record contains duplicate tags");
            }
            return false;
        }
        foldedTags.insert(folded);
    }
    return true;
}

quint8 categoryFlag(SstvImageCategory category)
{
    switch (category) {
    case SstvImageCategory::Received:
        return SstvGalleryReceived;
    case SstvImageCategory::Transmitted:
        return SstvGalleryTransmitted;
    case SstvImageCategory::Imported:
        return SstvGalleryImported;
    case SstvImageCategory::Draft:
        return SstvGalleryDraft;
    }
    return 0U;
}

QString displayCallsign(const SstvImageRecord& record)
{
    return record.remoteCallsign.isEmpty()
        ? record.localCallsign : record.remoteCallsign;
}

int compareText(const QString& left, const QString& right)
{
    return QString::compare(left, right, Qt::CaseInsensitive);
}

} // namespace

SstvGalleryModel::SstvGalleryModel(QObject* parent)
    : QAbstractListModel(parent)
{
    qRegisterMetaType<SstvGalleryQuery>();
    qRegisterMetaType<SstvGalleryPage>();
    qRegisterMetaType<SstvImageRecord>();
    qRegisterMetaType<SstvStorageOperation>();
    qRegisterMetaType<SstvRetentionSettings>();
    qRegisterMetaType<SstvQuotaSummary>();
    qRegisterMetaType<SstvRetentionPlan>();
    m_retentionSettings = SstvRetentionSettings {}.toVariantMap();
    m_quotaSummary = SstvQuotaSummary {}.toVariantMap();
}

SstvGalleryModel::~SstvGalleryModel()
{
    shutdown();
}

int SstvGalleryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_records.size());
}

QVariant SstvGalleryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0
        || index.row() >= static_cast<int>(m_records.size())) {
        return {};
    }
    const SstvImageRecord& record = m_records.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return record.remoteCallsign.isEmpty() ? record.mode
                                               : record.remoteCallsign;
    case IdRole:
        if (m_thumbnailProvider) {
            m_thumbnailProvider->registerSource(record.id, record.imagePath);
        }
        return record.id;
    case CategoryRole:
        return static_cast<int>(record.category);
    case CategoryNameRole:
        return sstvImageCategoryName(record.category);
    case CapturedAtRole:
        return record.capturedAtUtc;
    case UpdatedAtRole:
        return record.updatedAtUtc;
    case ModeRole:
        return record.mode;
    case VisCodeRole:
        return record.visCode;
    case RemoteCallsignRole:
        return record.remoteCallsign;
    case LocalCallsignRole:
        return record.localCallsign;
    case SourceRole:
        return record.source;
    case FrequencyHzRole:
        return QVariant::fromValue(record.frequencyHz);
    case CompleteRole:
        return record.complete;
    case PartialRole:
        return !record.complete;
    case RemoteRole:
        return record.remote;
    case UploadStateRole:
        return static_cast<int>(record.uploadState);
    case UploadStateNameRole:
        return sstvUploadStateName(record.uploadState);
    case TagsRole:
        return record.tags;
    case NoteRole:
        return record.note;
    case ImagePathRole:
        return record.imagePath;
    case MetadataPathRole:
        return record.metadataPath;
    case WidthRole:
        return record.width;
    case HeightRole:
        return record.height;
    case SelectedRole:
        return m_selected.contains(record.id);
    case EventAtRole:
        return record.eventAtUtc;
    case CreatedAtRole:
        return record.createdAtUtc;
    case VisValidRole:
        return record.visValid;
    case FskIdRole:
        return record.fskId;
    case RemoteGridRole:
        return record.remoteGrid;
    case LocalGridRole:
        return record.localGrid;
    case AudioFrequencyHzRole:
        return QVariant::fromValue(record.audioFrequencyHz);
    case SourceSampleRateHzRole:
        return record.sourceSampleRateHz;
    case DigitalRole:
        return record.digital;
    case CompletionPercentRole:
        return record.completionPercent;
    case QualityMetricsRole:
        return record.qualityMetrics.toVariantMap();
    case SlantCorrectionPpmRole:
        return record.slantCorrectionPpm;
    case RawAudioPathRole:
        return record.rawAudioPath;
    case RelatedQsoIdRole:
        return record.relatedQsoId;
    case RemoteProviderRole:
        return record.remoteProvider;
    case RemoteObjectIdRole:
        return record.remoteObjectId;
    case ExpiresAtRole:
        return record.expiresAtUtc;
    case PrivacyFlagsRole:
        return record.privacyFlags;
    case ThumbnailPathRole:
        return record.thumbnailPath;
    case MimeTypeRole:
        return record.mimeType;
    case FileSizeBytesRole:
        return QVariant::fromValue(record.fileSizeBytes);
    case OriginalWidthRole:
        return record.originalWidth;
    case OriginalHeightRole:
        return record.originalHeight;
    case Sha256HexRole:
        return QString::fromLatin1(record.sha256.toHex());
    case FavoriteRole:
        return record.favorite;
    default:
        return {};
    }
}

QHash<int, QByteArray> SstvGalleryModel::roleNames() const
{
    return {
        {IdRole, "recordId"},
        {CategoryRole, "category"},
        {CategoryNameRole, "categoryName"},
        {CapturedAtRole, "capturedAtUtc"},
        {UpdatedAtRole, "updatedAtUtc"},
        {ModeRole, "mode"},
        {VisCodeRole, "visCode"},
        {RemoteCallsignRole, "remoteCallsign"},
        {LocalCallsignRole, "localCallsign"},
        {SourceRole, "source"},
        {FrequencyHzRole, "frequencyHz"},
        {CompleteRole, "complete"},
        {PartialRole, "partial"},
        {RemoteRole, "remote"},
        {UploadStateRole, "uploadState"},
        {UploadStateNameRole, "uploadStateName"},
        {TagsRole, "tags"},
        {NoteRole, "note"},
        {ImagePathRole, "imagePath"},
        {MetadataPathRole, "metadataPath"},
        {WidthRole, "imageWidth"},
        {HeightRole, "imageHeight"},
        {SelectedRole, "selected"},
        {EventAtRole, "eventAtUtc"},
        {CreatedAtRole, "createdAtUtc"},
        {VisValidRole, "visValid"},
        {FskIdRole, "fskId"},
        {RemoteGridRole, "remoteGrid"},
        {LocalGridRole, "localGrid"},
        {AudioFrequencyHzRole, "audioFrequencyHz"},
        {SourceSampleRateHzRole, "sourceSampleRateHz"},
        {DigitalRole, "digital"},
        {CompletionPercentRole, "completionPercent"},
        {QualityMetricsRole, "qualityMetrics"},
        {SlantCorrectionPpmRole, "slantCorrectionPpm"},
        {RawAudioPathRole, "rawAudioPath"},
        {RelatedQsoIdRole, "relatedQsoId"},
        {RemoteProviderRole, "remoteProvider"},
        {RemoteObjectIdRole, "remoteObjectId"},
        {ExpiresAtRole, "expiresAtUtc"},
        {PrivacyFlagsRole, "privacyFlags"},
        {ThumbnailPathRole, "thumbnailPath"},
        {MimeTypeRole, "mimeType"},
        {FileSizeBytesRole, "fileSizeBytes"},
        {OriginalWidthRole, "originalWidth"},
        {OriginalHeightRole, "originalHeight"},
        {Sha256HexRole, "sha256Hex"},
        {FavoriteRole, "favorite"}
    };
}

bool SstvGalleryModel::canFetchMore(const QModelIndex& parent) const
{
    return !parent.isValid() && m_acceptingResults && m_worker
        && m_hasMore && !m_loading;
}

void SstvGalleryModel::fetchMore(const QModelIndex& parent)
{
    if (!parent.isValid()) {
        requestNextPage();
    }
}

void SstvGalleryModel::setStorageWorker(SstvStorageWorker* worker)
{
    if (m_worker == worker) {
        return;
    }
    setHasMore(false);
    cancelPending();
    for (const QMetaObject::Connection& connection : std::as_const(m_connections)) {
        QObject::disconnect(connection);
    }
    m_connections.clear();
    const auto abandonedDeletes = m_deleteRequests;
    m_deleteRequests.clear();
    for (auto iterator = abandonedDeletes.cbegin();
         iterator != abandonedDeletes.cend(); ++iterator) {
        emit deleteFinished(iterator.key(), false,
                            QStringLiteral("storage worker changed; delete outcome is unknown"));
    }
    const auto abandonedExports = m_exportRequests;
    m_exportRequests.clear();
    for (auto iterator = abandonedExports.cbegin();
         iterator != abandonedExports.cend(); ++iterator) {
        emit exportFinished(iterator.key(), false, {},
                            QStringLiteral("storage worker changed; export outcome is unknown"));
    }
    const auto abandonedFileDeletes = m_fileDeleteRequests;
    m_fileDeleteRequests.clear();
    for (auto iterator = abandonedFileDeletes.cbegin();
         iterator != abandonedFileDeletes.cend(); ++iterator) {
        emit deleteFilesFinished(iterator.key(), false,
                                 QStringLiteral("storage worker changed; file deletion outcome is unknown"));
    }
    const auto abandonedFavorites = m_favoriteRequests;
    m_favoriteRequests.clear();
    for (auto iterator = abandonedFavorites.cbegin();
         iterator != abandonedFavorites.cend(); ++iterator) {
        emit favoriteFinished(iterator.key(), iterator.value(), false,
                              QStringLiteral(
                                  "storage worker changed; favourite outcome is unknown"));
    }
    const auto abandonedQsoAssociations = m_qsoAssociationRequests;
    m_qsoAssociationRequests.clear();
    for (auto iterator = abandonedQsoAssociations.cbegin();
         iterator != abandonedQsoAssociations.cend(); ++iterator) {
        emit qsoAssociationFinished(
            iterator.key(), iterator.value().imageId,
            iterator.value().qsoId, false,
            QStringLiteral(
                "storage worker changed; QSO association outcome is unknown"));
    }
    const auto abandonedUserMetadata = m_userMetadataRequests;
    m_userMetadataRequests.clear();
    for (auto iterator = abandonedUserMetadata.cbegin();
         iterator != abandonedUserMetadata.cend(); ++iterator) {
        emit userMetadataUpdateFinished(
            iterator.key(), iterator.value().imageId,
            iterator.value().note, iterator.value().tags, false,
            QStringLiteral(
                "storage worker changed; metadata update outcome is unknown"));
    }
    clearRetentionState(QStringLiteral(
        "storage worker changed; retention outcome is unknown"));
    m_worker = worker;
    m_acceptingResults = true;
    if (!worker) {
        clearRowsIncrementally();
        setHasMore(false);
        return;
    }
    m_connections.append(connect(
        worker, &SstvStorageWorker::galleryPageFetched,
        this, &SstvGalleryModel::handleGalleryPage));
    m_connections.append(connect(
        worker, &SstvStorageWorker::recordChanged,
        this, &SstvGalleryModel::handleRecordChanged));
    m_connections.append(connect(
        worker, &SstvStorageWorker::recordsRemoved,
        this, &SstvGalleryModel::handleRecordsRemoved));
    m_connections.append(connect(
        worker, &SstvStorageWorker::recordsDeletedWithFiles,
        this, &SstvGalleryModel::handleRecordsDeletedWithFiles));
    m_connections.append(connect(
        worker, &SstvStorageWorker::operationFinished,
        this, &SstvGalleryModel::handleOperationFinished));
    m_connections.append(connect(
        worker, &SstvStorageWorker::recordExported,
        this, &SstvGalleryModel::handleRecordExported));
    m_connections.append(connect(
        worker, &SstvStorageWorker::retentionSettingsLoaded,
        this, &SstvGalleryModel::handleRetentionSettingsLoaded));
    m_connections.append(connect(
        worker, &SstvStorageWorker::retentionSettingsUpdated,
        this, &SstvGalleryModel::handleRetentionSettingsUpdated));
    m_connections.append(connect(
        worker, &SstvStorageWorker::quotaCalculated,
        this, &SstvGalleryModel::handleQuotaCalculated));
    m_connections.append(connect(
        worker, &SstvStorageWorker::retentionPreviewReady,
        this, &SstvGalleryModel::handleRetentionPreviewReady));
    m_connections.append(connect(worker, &QObject::destroyed, this, [this]() {
        m_worker = nullptr;
        cancelPending();
        setHasMore(false);
        setErrorString(QStringLiteral("SSTV storage worker was destroyed"));
        const auto pendingDeletes = m_deleteRequests;
        m_deleteRequests.clear();
        for (auto iterator = pendingDeletes.cbegin();
             iterator != pendingDeletes.cend(); ++iterator) {
            emit deleteFinished(iterator.key(), false,
                                QStringLiteral("storage worker was destroyed; delete outcome is unknown"));
        }
        const auto pendingExports = m_exportRequests;
        m_exportRequests.clear();
        for (auto iterator = pendingExports.cbegin();
             iterator != pendingExports.cend(); ++iterator) {
            emit exportFinished(iterator.key(), false, {},
                                QStringLiteral("storage worker was destroyed; export outcome is unknown"));
        }
        const auto pendingFileDeletes = m_fileDeleteRequests;
        m_fileDeleteRequests.clear();
        for (auto iterator = pendingFileDeletes.cbegin();
             iterator != pendingFileDeletes.cend(); ++iterator) {
            emit deleteFilesFinished(iterator.key(), false,
                                     QStringLiteral("storage worker was destroyed; file deletion outcome is unknown"));
        }
        const auto pendingFavorites = m_favoriteRequests;
        m_favoriteRequests.clear();
        for (auto iterator = pendingFavorites.cbegin();
             iterator != pendingFavorites.cend(); ++iterator) {
            emit favoriteFinished(iterator.key(), iterator.value(), false,
                                  QStringLiteral(
                                      "storage worker was destroyed; favourite outcome is unknown"));
        }
        const auto pendingQsoAssociations = m_qsoAssociationRequests;
        m_qsoAssociationRequests.clear();
        for (auto iterator = pendingQsoAssociations.cbegin();
             iterator != pendingQsoAssociations.cend(); ++iterator) {
            emit qsoAssociationFinished(
                iterator.key(), iterator.value().imageId,
                iterator.value().qsoId, false,
                QStringLiteral(
                    "storage worker was destroyed; QSO association outcome is unknown"));
        }
        const auto pendingUserMetadata = m_userMetadataRequests;
        m_userMetadataRequests.clear();
        for (auto iterator = pendingUserMetadata.cbegin();
             iterator != pendingUserMetadata.cend(); ++iterator) {
            emit userMetadataUpdateFinished(
                iterator.key(), iterator.value().imageId,
                iterator.value().note, iterator.value().tags, false,
                QStringLiteral(
                    "storage worker was destroyed; metadata update outcome is unknown"));
        }
        clearRetentionState(QStringLiteral(
            "storage worker was destroyed; retention outcome is unknown"));
    }));
    reload();
    if (!worker->enqueueDatabaseOperation(
            [](SstvStorageWorker& storage) {
                storage.loadRetentionSettings();
            })) {
        setErrorString(QStringLiteral(
            "could not queue retention settings load"));
    }
    refreshQuota();
}

void SstvGalleryModel::setUnavailableError(const QString& error)
{
    if (m_worker) {
        setStorageWorker(nullptr);
    } else {
        cancelPending();
        clearRowsIncrementally();
        setHasMore(false);
    }
    setErrorString(error.isEmpty()
                       ? QStringLiteral("SSTV gallery storage is unavailable")
                       : error);
}

void SstvGalleryModel::setThumbnailProvider(SstvThumbnailProvider* provider)
{
    if (m_thumbnailProvider == provider) {
        return;
    }
    if (m_thumbnailProvider) {
        for (const SstvImageRecord& record : std::as_const(m_records)) {
            m_thumbnailProvider->unregisterSource(record.id);
        }
    }
    m_thumbnailProvider = provider;
    if (!provider) {
        return;
    }
    for (const SstvImageRecord& record : std::as_const(m_records)) {
        provider->registerSource(record.id, record.imagePath);
    }
}

bool SstvGalleryModel::setQuery(const SstvGalleryQuery& query, QString* error)
{
    SstvGalleryQuery candidate = query;
    candidate.offset = 0;
    QString validationError;
    QString* const targetError = error ? error : &validationError;
    if (!candidate.validate(targetError)) {
        emit queryRejected(*targetError);
        return false;
    }
    m_query = std::move(candidate);
    emit filtersChanged();
    reload();
    return true;
}

bool SstvGalleryModel::applyCandidateQuery(SstvGalleryQuery candidate)
{
    QString error;
    if (!setQuery(candidate, &error)) {
        return false;
    }
    return true;
}

void SstvGalleryModel::setPageSize(int value)
{
    if (m_query.limit == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.limit = value;
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setCategoryMask(int value)
{
    if (value < 0 || value > std::numeric_limits<quint8>::max()) {
        emit queryRejected(QStringLiteral("invalid gallery category mask"));
        return;
    }
    if (m_query.categoryMask == static_cast<quint8>(value)) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.categoryMask = static_cast<quint8>(value);
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setRemoteFilter(int value)
{
    if (value < static_cast<int>(SstvGalleryTruthFilter::Any)
        || value > static_cast<int>(SstvGalleryTruthFilter::OnlyFalse)) {
        emit queryRejected(QStringLiteral("invalid remote filter"));
        return;
    }
    if (remoteFilter() == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.remote = static_cast<SstvGalleryTruthFilter>(value);
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setModeFilter(const QString& value)
{
    if (m_query.mode == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.mode = value;
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setCallsignFilter(const QString& value)
{
    if (m_query.callsign == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.callsign = value;
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setCapturedFromUtc(QDateTime value)
{
    if (value.isValid()) {
        value = value.toUTC();
    }
    if (m_query.capturedFromUtc == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.capturedFromUtc = std::move(value);
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setCapturedToUtc(QDateTime value)
{
    if (value.isValid()) {
        value = value.toUTC();
    }
    if (m_query.capturedToUtc == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.capturedToUtc = std::move(value);
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setMinimumFrequencyHz(qint64 value)
{
    if (m_query.minimumFrequencyHz == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.minimumFrequencyHz = value;
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setMaximumFrequencyHz(qint64 value)
{
    if (m_query.maximumFrequencyHz == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.maximumFrequencyHz = value;
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setTags(const QStringList& value)
{
    if (m_query.tags == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.tags = value;
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setRequireAllTags(bool value)
{
    if (m_query.requireAllTags == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.requireAllTags = value;
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setPartialFilter(int value)
{
    if (value < static_cast<int>(SstvGalleryTruthFilter::Any)
        || value > static_cast<int>(SstvGalleryTruthFilter::OnlyFalse)) {
        emit queryRejected(QStringLiteral("invalid partial filter"));
        return;
    }
    if (partialFilter() == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.partial = static_cast<SstvGalleryTruthFilter>(value);
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setUploadStateFilter(int value)
{
    if (m_query.uploadState == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.uploadState = value;
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setSearch(const QString& value)
{
    if (m_query.search == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.search = value;
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::setSortOrder(int value)
{
    if (value < static_cast<int>(SstvGallerySort::CapturedNewest)
        || value > static_cast<int>(SstvGallerySort::UpdatedOldest)) {
        emit queryRejected(QStringLiteral("invalid gallery sort order"));
        return;
    }
    if (sortOrder() == value) {
        return;
    }
    SstvGalleryQuery candidate = m_query;
    candidate.sort = static_cast<SstvGallerySort>(value);
    applyCandidateQuery(std::move(candidate));
}

void SstvGalleryModel::reload()
{
    if (!m_acceptingResults) {
        return;
    }
    // Make a loadingChanged re-entrant fetch harmless while the query cursor
    // and rows are being replaced.
    setHasMore(false);
    cancelPending();
    clearRowsIncrementally();
    m_nextOffset = 0;
    setErrorString({});
    setHasMore(m_worker != nullptr);
    if (m_worker) {
        requestNextPage();
    }
}

void SstvGalleryModel::loadMore()
{
    requestNextPage();
}

bool SstvGalleryModel::applyFilters(const QVariantMap& filters)
{
    SstvGalleryQuery candidate = m_query;
    candidate.offset = 0;
    QString error;

    auto reject = [this, &error](const QString& detail) {
        error = detail;
        emit queryRejected(error);
        return false;
    };
    auto readInt = [&filters, &reject](const QString& key, int* target) {
        const auto iterator = filters.constFind(key);
        if (iterator == filters.cend()) {
            return true;
        }
        bool ok = false;
        const int value = iterator.value().toInt(&ok);
        if (!ok) {
            return reject(QStringLiteral("invalid integer gallery filter: %1")
                              .arg(key));
        }
        *target = value;
        return true;
    };
    auto readLongLong = [&filters, &reject](const QString& key,
                                            qint64* target) {
        const auto iterator = filters.constFind(key);
        if (iterator == filters.cend()) {
            return true;
        }
        bool ok = false;
        const qint64 value = iterator.value().toLongLong(&ok);
        if (!ok) {
            return reject(QStringLiteral("invalid numeric gallery filter: %1")
                              .arg(key));
        }
        *target = value;
        return true;
    };
    auto readDateTime = [&filters, &reject](const QString& key,
                                            QDateTime* target) {
        const auto iterator = filters.constFind(key);
        if (iterator == filters.cend()) {
            return true;
        }
        const QVariant value = iterator.value();
        QDateTime result;
        if (!value.isValid() || value.isNull()
            || (value.metaType().id() == QMetaType::QString
                && value.toString().trimmed().isEmpty())) {
            *target = {};
            return true;
        }
        if (value.canConvert<QDateTime>()) {
            result = value.toDateTime();
        }
        if (!result.isValid()) {
            const QString text = value.toString().trimmed();
            result = QDateTime::fromString(text, Qt::ISODateWithMs);
            if (!result.isValid()) {
                result = QDateTime::fromString(text, Qt::ISODate);
            }
        }
        if (!result.isValid()) {
            return reject(QStringLiteral("invalid UTC date gallery filter: %1")
                              .arg(key));
        }
        *target = result.toUTC();
        return true;
    };

    int integerValue = 0;
    if (filters.contains(QStringLiteral("categoryMask"))) {
        if (!readInt(QStringLiteral("categoryMask"), &integerValue)
            || integerValue < 0
            || integerValue > std::numeric_limits<quint8>::max()) {
            if (error.isEmpty()) {
                reject(QStringLiteral("invalid gallery category mask"));
            }
            return false;
        }
        candidate.categoryMask = static_cast<quint8>(integerValue);
    }
    if (filters.contains(QStringLiteral("remoteFilter"))) {
        if (!readInt(QStringLiteral("remoteFilter"), &integerValue)) {
            return false;
        }
        candidate.remote = static_cast<SstvGalleryTruthFilter>(integerValue);
    }
    if (filters.contains(QStringLiteral("partialFilter"))) {
        if (!readInt(QStringLiteral("partialFilter"), &integerValue)) {
            return false;
        }
        candidate.partial = static_cast<SstvGalleryTruthFilter>(integerValue);
    }
    if (filters.contains(QStringLiteral("uploadStateFilter"))) {
        if (!readInt(QStringLiteral("uploadStateFilter"), &integerValue)) {
            return false;
        }
        candidate.uploadState = integerValue;
    }
    if (filters.contains(QStringLiteral("sortOrder"))) {
        if (!readInt(QStringLiteral("sortOrder"), &integerValue)) {
            return false;
        }
        candidate.sort = static_cast<SstvGallerySort>(integerValue);
    }
    if (filters.contains(QStringLiteral("pageSize"))) {
        if (!readInt(QStringLiteral("pageSize"), &integerValue)) {
            return false;
        }
        candidate.limit = integerValue;
    }
    if (!readLongLong(QStringLiteral("minimumFrequencyHz"),
                      &candidate.minimumFrequencyHz)
        || !readLongLong(QStringLiteral("maximumFrequencyHz"),
                         &candidate.maximumFrequencyHz)
        || !readDateTime(QStringLiteral("capturedFromUtc"),
                         &candidate.capturedFromUtc)
        || !readDateTime(QStringLiteral("capturedToUtc"),
                         &candidate.capturedToUtc)) {
        return false;
    }

    auto assignTrimmedText = [&filters](const QString& key, QString* target) {
        const auto iterator = filters.constFind(key);
        if (iterator != filters.cend()) {
            *target = iterator.value().toString().trimmed();
        }
    };
    assignTrimmedText(QStringLiteral("mode"), &candidate.mode);
    assignTrimmedText(QStringLiteral("callsign"), &candidate.callsign);
    assignTrimmedText(QStringLiteral("search"), &candidate.search);

    if (const auto iterator = filters.constFind(QStringLiteral("tags"));
        iterator != filters.cend()) {
        QStringList tags;
        if (iterator.value().metaType().id() == QMetaType::QStringList) {
            tags = iterator.value().toStringList();
        } else {
            const QVariantList values = iterator.value().toList();
            tags.reserve(values.size());
            for (const QVariant& value : values) {
                tags.push_back(value.toString());
            }
        }
        for (QString& tag : tags) {
            tag = tag.trimmed().normalized(QString::NormalizationForm_C);
        }
        tags.removeAll(QString());
        candidate.tags = std::move(tags);
    }
    if (const auto iterator = filters.constFind(
            QStringLiteral("requireAllTags"));
        iterator != filters.cend()) {
        candidate.requireAllTags = iterator.value().toBool();
    }

    return setQuery(candidate, &error);
}

QVariantMap SstvGalleryModel::filters() const
{
    QVariantMap result;
    result.insert(QStringLiteral("categoryMask"), m_query.categoryMask);
    result.insert(QStringLiteral("remoteFilter"), remoteFilter());
    result.insert(QStringLiteral("mode"), m_query.mode);
    result.insert(QStringLiteral("callsign"), m_query.callsign);
    result.insert(QStringLiteral("capturedFromUtc"), m_query.capturedFromUtc);
    result.insert(QStringLiteral("capturedToUtc"), m_query.capturedToUtc);
    result.insert(QStringLiteral("minimumFrequencyHz"),
                  m_query.minimumFrequencyHz);
    result.insert(QStringLiteral("maximumFrequencyHz"),
                  m_query.maximumFrequencyHz);
    result.insert(QStringLiteral("tags"), m_query.tags);
    result.insert(QStringLiteral("requireAllTags"), m_query.requireAllTags);
    result.insert(QStringLiteral("partialFilter"), partialFilter());
    result.insert(QStringLiteral("uploadStateFilter"), m_query.uploadState);
    result.insert(QStringLiteral("search"), m_query.search);
    result.insert(QStringLiteral("sortOrder"), sortOrder());
    result.insert(QStringLiteral("pageSize"), m_query.limit);
    return result;
}

void SstvGalleryModel::cancelPending()
{
    m_pendingPageRequest = 0;
    setLoading(false);
}

void SstvGalleryModel::requestNextPage()
{
    if (!m_acceptingResults || !m_worker || m_loading || !m_hasMore) {
        return;
    }
    SstvGalleryQuery request = m_query;
    request.offset = m_nextOffset;
    const quint64 requestId = nextRequestId();
    m_pendingPageRequest = requestId;
    setLoading(true);
    if (!m_worker->enqueueDatabaseOperation(
            [request = std::move(request), requestId](
                SstvStorageWorker& storage) mutable {
                storage.queryGallery(std::move(request), requestId);
            })) {
        m_pendingPageRequest = 0;
        setLoading(false);
        setErrorString(QStringLiteral("could not queue SSTV gallery query"));
    }
}

void SstvGalleryModel::handleGalleryPage(quint64 requestId,
                                         SstvGalleryPage page,
                                         const QString& error)
{
    if (!m_acceptingResults || requestId != m_pendingPageRequest) {
        return;
    }
    m_pendingPageRequest = 0;
    if (!error.isEmpty()) {
        setErrorString(error);
        setHasMore(false);
        setLoading(false);
        return;
    }
    setErrorString({});
    for (const SstvImageRecord& record : std::as_const(page.records)) {
        const int existing = indexOfId(record.id);
        if (existing >= 0) {
            if (m_thumbnailProvider
                && m_records.at(existing).imagePath != record.imagePath) {
                m_thumbnailProvider->registerSource(record.id,
                                                    record.imagePath);
            }
            m_records[existing] = record;
            emit dataChanged(index(existing), index(existing));
            continue;
        }
        insertRecordIncrementally(record, false);
    }
    m_nextOffset = page.nextOffset;
    setHasMore(page.hasMore);
    setLoading(false);
}

bool SstvGalleryModel::recordMatches(const SstvImageRecord& record) const
{
    if ((m_query.categoryMask & categoryFlag(record.category)) == 0U) {
        return false;
    }
    if ((m_query.remote == SstvGalleryTruthFilter::OnlyTrue && !record.remote)
        || (m_query.remote == SstvGalleryTruthFilter::OnlyFalse
            && record.remote)) {
        return false;
    }
    if (!m_query.mode.isEmpty()
        && record.mode.compare(m_query.mode, Qt::CaseInsensitive) != 0) {
        return false;
    }
    if (!m_query.callsign.isEmpty()
        && !record.remoteCallsign.contains(m_query.callsign,
                                           Qt::CaseInsensitive)
        && !record.localCallsign.contains(m_query.callsign,
                                          Qt::CaseInsensitive)) {
        return false;
    }
    if (m_query.capturedFromUtc.isValid()
        && record.capturedAtUtc < m_query.capturedFromUtc) {
        return false;
    }
    if (m_query.capturedToUtc.isValid()
        && record.capturedAtUtc > m_query.capturedToUtc) {
        return false;
    }
    if ((m_query.minimumFrequencyHz >= 0
         && record.frequencyHz < m_query.minimumFrequencyHz)
        || (m_query.maximumFrequencyHz >= 0
            && record.frequencyHz > m_query.maximumFrequencyHz)) {
        return false;
    }
    if ((m_query.partial == SstvGalleryTruthFilter::OnlyTrue
         && record.complete)
        || (m_query.partial == SstvGalleryTruthFilter::OnlyFalse
            && !record.complete)) {
        return false;
    }
    if (m_query.uploadState >= 0
        && static_cast<int>(record.uploadState) != m_query.uploadState) {
        return false;
    }
    if (!m_query.tags.isEmpty()) {
        QSet<QString> folded;
        for (const QString& tag : record.tags) {
            folded.insert(tag.toCaseFolded());
        }
        bool matchedAny = false;
        for (const QString& tag : m_query.tags) {
            const bool matched = folded.contains(tag.toCaseFolded());
            matchedAny = matchedAny || matched;
            if (m_query.requireAllTags && !matched) {
                return false;
            }
        }
        if (!m_query.requireAllTags && !matchedAny) {
            return false;
        }
    }
    if (!m_query.search.isEmpty()) {
        bool matched = record.mode.contains(m_query.search, Qt::CaseInsensitive)
            || record.remoteCallsign.contains(m_query.search,
                                              Qt::CaseInsensitive)
            || record.localCallsign.contains(m_query.search,
                                             Qt::CaseInsensitive)
            || record.remoteGrid.contains(m_query.search,
                                          Qt::CaseInsensitive)
            || record.localGrid.contains(m_query.search,
                                         Qt::CaseInsensitive)
            || record.fskId.contains(m_query.search, Qt::CaseInsensitive)
            || record.source.contains(m_query.search, Qt::CaseInsensitive)
            || record.note.contains(m_query.search, Qt::CaseInsensitive)
            || record.relatedQsoId.contains(m_query.search,
                                            Qt::CaseInsensitive)
            || record.remoteProvider.contains(m_query.search,
                                              Qt::CaseInsensitive)
            || record.remoteObjectId.contains(m_query.search,
                                              Qt::CaseInsensitive);
        for (const QString& tag : record.tags) {
            matched = matched
                || tag.contains(m_query.search, Qt::CaseInsensitive);
        }
        if (!matched) {
            return false;
        }
    }
    return true;
}

bool SstvGalleryModel::comesBefore(const SstvImageRecord& left,
                                   const SstvImageRecord& right) const
{
    int comparison = 0;
    bool descendingTie = false;
    switch (m_query.sort) {
    case SstvGallerySort::CapturedNewest:
        if (left.capturedAtUtc != right.capturedAtUtc) {
            return left.capturedAtUtc > right.capturedAtUtc;
        }
        descendingTie = true;
        break;
    case SstvGallerySort::CapturedOldest:
        if (left.capturedAtUtc != right.capturedAtUtc) {
            return left.capturedAtUtc < right.capturedAtUtc;
        }
        break;
    case SstvGallerySort::CallsignAscending:
    case SstvGallerySort::CallsignDescending:
        comparison = compareText(displayCallsign(left), displayCallsign(right));
        if (comparison != 0) {
            return m_query.sort == SstvGallerySort::CallsignAscending
                ? comparison < 0 : comparison > 0;
        }
        descendingTie = m_query.sort == SstvGallerySort::CallsignDescending;
        break;
    case SstvGallerySort::ModeAscending:
    case SstvGallerySort::ModeDescending:
        comparison = compareText(left.mode, right.mode);
        if (comparison != 0) {
            return m_query.sort == SstvGallerySort::ModeAscending
                ? comparison < 0 : comparison > 0;
        }
        descendingTie = m_query.sort == SstvGallerySort::ModeDescending;
        break;
    case SstvGallerySort::FrequencyAscending:
    case SstvGallerySort::FrequencyDescending:
        if (left.frequencyHz != right.frequencyHz) {
            return m_query.sort == SstvGallerySort::FrequencyAscending
                ? left.frequencyHz < right.frequencyHz
                : left.frequencyHz > right.frequencyHz;
        }
        descendingTie = m_query.sort == SstvGallerySort::FrequencyDescending;
        break;
    case SstvGallerySort::UpdatedNewest:
        if (left.updatedAtUtc != right.updatedAtUtc) {
            return left.updatedAtUtc > right.updatedAtUtc;
        }
        descendingTie = true;
        break;
    case SstvGallerySort::UpdatedOldest:
        if (left.updatedAtUtc != right.updatedAtUtc) {
            return left.updatedAtUtc < right.updatedAtUtc;
        }
        break;
    }
    return descendingTie ? left.id > right.id : left.id < right.id;
}

int SstvGalleryModel::insertionIndex(const SstvImageRecord& record) const
{
    const auto position = std::lower_bound(
        m_records.cbegin(), m_records.cend(), record,
        [this](const SstvImageRecord& existing,
               const SstvImageRecord& candidate) {
            return comesBefore(existing, candidate);
        });
    return static_cast<int>(std::distance(m_records.cbegin(), position));
}

void SstvGalleryModel::insertRecordIncrementally(
    const SstvImageRecord& record,
    bool adjustNextOffset)
{
    const int row = insertionIndex(record);
    beginInsertRows({}, row, row);
    m_records.insert(row, record);
    endInsertRows();
    if (m_thumbnailProvider) {
        m_thumbnailProvider->registerSource(record.id, record.imagePath);
    }
    if (adjustNextOffset) {
        ++m_nextOffset;
    }
}

void SstvGalleryModel::removeRowIncrementally(int row, bool adjustNextOffset)
{
    if (row < 0 || row >= static_cast<int>(m_records.size())) {
        return;
    }
    const QString id = m_records.at(row).id;
    if (m_thumbnailProvider) {
        m_thumbnailProvider->unregisterSource(id);
    }
    beginRemoveRows({}, row, row);
    m_records.removeAt(row);
    endRemoveRows();
    if (adjustNextOffset && m_nextOffset > 0) {
        --m_nextOffset;
    }
    if (m_selected.remove(id)) {
        emit selectedCountChanged();
    }
}

void SstvGalleryModel::handleRecordChanged(const SstvImageRecord& record)
{
    if (!m_acceptingResults) {
        return;
    }
    if (!m_retentionPreview.isEmpty()) {
        m_retentionPreview.clear();
        emit retentionPreviewChanged();
    }
    requestAutomaticRetention();
    const int existing = indexOfId(record.id);
    if (existing >= 0) {
        if (!recordMatches(record)) {
            removeRowIncrementally(existing, true);
            return;
        }
        const bool fitsBefore = existing == 0
            || !comesBefore(record, m_records.at(existing - 1));
        const bool fitsAfter = existing + 1 >= static_cast<int>(m_records.size())
            || !comesBefore(m_records.at(existing + 1), record);
        if (fitsBefore && fitsAfter) {
            if (m_thumbnailProvider
                && m_records.at(existing).imagePath != record.imagePath) {
                m_thumbnailProvider->registerSource(record.id,
                                                    record.imagePath);
            }
            m_records[existing] = record;
            emit dataChanged(index(existing), index(existing));
            return;
        }
        beginRemoveRows({}, existing, existing);
        m_records.removeAt(existing);
        endRemoveRows();
        if (m_hasMore && !m_records.isEmpty()
            && !comesBefore(record, m_records.constLast())) {
            if (m_thumbnailProvider) {
                m_thumbnailProvider->unregisterSource(record.id);
            }
            if (m_nextOffset > 0) {
                --m_nextOffset;
            }
            return;
        }
        insertRecordIncrementally(record, false);
        return;
    }
    if (!recordMatches(record)) {
        return;
    }
    if (!m_hasMore || m_records.isEmpty()
        || comesBefore(record, m_records.constLast())) {
        insertRecordIncrementally(record, true);
    }
}

void SstvGalleryModel::handleRecordsRemoved(const QStringList& ids,
                                            quint64 requestId)
{
    if (!m_acceptingResults) {
        return;
    }
    const QSet<QString> removed(ids.cbegin(), ids.cend());
    for (int row = static_cast<int>(m_records.size()) - 1; row >= 0; --row) {
        if (removed.contains(m_records.at(row).id)) {
            removeRowIncrementally(row, true);
        }
    }
    const auto pending = m_deleteRequests.find(requestId);
    if (pending != m_deleteRequests.end()) {
        m_deleteRequests.erase(pending);
        emit deleteFinished(requestId, true, {});
    }
}

void SstvGalleryModel::handleOperationFinished(
    quint64 requestId,
    SstvStorageOperation operation,
    bool ok,
    const QString& error)
{
    if (operation == SstvStorageOperation::UpdateUserMetadata) {
        const auto pending = m_userMetadataRequests.find(requestId);
        if (pending != m_userMetadataRequests.end()) {
            const PendingUserMetadata metadata = pending.value();
            m_userMetadataRequests.erase(pending);
            emit userMetadataUpdateFinished(
                requestId, metadata.imageId, metadata.note, metadata.tags,
                ok, error);
        }
        return;
    }
    if (operation == SstvStorageOperation::SetFavorite) {
        const auto pending = m_favoriteRequests.find(requestId);
        if (pending != m_favoriteRequests.end()) {
            const QString id = pending.value();
            m_favoriteRequests.erase(pending);
            emit favoriteFinished(requestId, id, ok, error);
        }
        return;
    }
    if (operation == SstvStorageOperation::AssociateQso) {
        const auto pending = m_qsoAssociationRequests.find(requestId);
        if (pending != m_qsoAssociationRequests.end()) {
            const PendingQsoAssociation association = pending.value();
            m_qsoAssociationRequests.erase(pending);
            emit qsoAssociationFinished(
                requestId, association.imageId, association.qsoId, ok, error);
        }
        return;
    }
    if (operation == SstvStorageOperation::DeleteManyFiles) {
        const auto retention = m_retentionApplyRequests.find(requestId);
        if (retention != m_retentionApplyRequests.end()) {
            const bool automatic = retention.value();
            m_retentionApplyRequests.erase(retention);
            setRetentionBusy(!m_quotaRequests.isEmpty()
                             || !m_retentionPreviewRequests.isEmpty()
                             || !m_retentionSettingsRequests.isEmpty()
                             || !m_retentionApplyRequests.isEmpty());
            emit retentionApplyFinished(requestId, automatic, ok, error);
            if (ok) {
                if (!m_retentionPreview.isEmpty()) {
                    m_retentionPreview.clear();
                    emit retentionPreviewChanged();
                }
                refreshQuota();
            }
            return;
        }
        const auto pending = m_fileDeleteRequests.find(requestId);
        if (pending != m_fileDeleteRequests.end() && !ok) {
            m_fileDeleteRequests.erase(pending);
            emit deleteFilesFinished(requestId, false, error);
        }
        return;
    }
    if (operation != SstvStorageOperation::RemoveMany) {
        return;
    }
    const auto pending = m_deleteRequests.find(requestId);
    if (pending == m_deleteRequests.end() || ok) {
        return;
    }
    m_deleteRequests.erase(pending);
    emit deleteFinished(requestId, false, error);
}

void SstvGalleryModel::handleRecordsDeletedWithFiles(
    const QStringList& ids,
    quint64 requestId,
    const QString& cleanupWarning)
{
    Q_UNUSED(ids)
    const auto pending = m_fileDeleteRequests.find(requestId);
    if (pending == m_fileDeleteRequests.end()) {
        return;
    }
    m_fileDeleteRequests.erase(pending);
    emit deleteFilesFinished(requestId, true, cleanupWarning);
}

bool SstvGalleryModel::isSelected(const QString& id) const
{
    return m_selected.contains(id);
}

void SstvGalleryModel::setSelected(const QString& id, bool selected)
{
    const int row = indexOfId(id);
    if (row < 0 || m_selected.contains(id) == selected) {
        return;
    }
    if (selected) {
        m_selected.insert(id);
    } else {
        m_selected.remove(id);
    }
    emit dataChanged(index(row), index(row), {SelectedRole});
    emit selectedCountChanged();
}

void SstvGalleryModel::toggleSelected(int row)
{
    if (row < 0 || row >= static_cast<int>(m_records.size())) {
        return;
    }
    const QString id = m_records.at(row).id;
    setSelected(id, !m_selected.contains(id));
}

void SstvGalleryModel::clearSelection()
{
    if (m_selected.isEmpty()) {
        return;
    }
    m_selected.clear();
    if (!m_records.isEmpty()) {
        emit dataChanged(index(0), index(static_cast<int>(m_records.size()) - 1),
                         {SelectedRole});
    }
    emit selectedCountChanged();
}

quint64 SstvGalleryModel::setFavorite(const QString& id, bool favorite)
{
    const int row = indexOfId(id);
    if (!m_acceptingResults || !m_worker || row < 0
        || m_records.at(row).favorite == favorite) {
        return 0;
    }
    const quint64 requestId = nextRequestId();
    m_favoriteRequests.insert(requestId, id);
    if (!m_worker->enqueueDatabaseOperation(
            [id, favorite, requestId](SstvStorageWorker& storage) {
                storage.setFavorite(id, favorite, requestId);
            })) {
        m_favoriteRequests.remove(requestId);
        emit favoriteFinished(requestId, id, false,
                              QStringLiteral(
                                  "could not queue favourite update"));
        return 0;
    }
    return requestId;
}

quint64 SstvGalleryModel::toggleFavorite(int row)
{
    if (row < 0 || row >= static_cast<int>(m_records.size())) {
        return 0;
    }
    const SstvImageRecord& record = m_records.at(row);
    return setFavorite(record.id, !record.favorite);
}

quint64 SstvGalleryModel::updateUserMetadata(const QString& imageId,
                                             const QString& note,
                                             const QStringList& tags)
{
    const int row = indexOfId(imageId);
    if (!m_acceptingResults || !m_worker || row < 0) {
        return 0;
    }
    const QString normalizedNote = normalizedUserNote(note);
    const QStringList normalizedTags = normalizedUserTags(tags);
    QString validationError;
    if (!validateUserMetadata(normalizedNote, normalizedTags,
                              &validationError)) {
        setErrorString(validationError);
        return 0;
    }
    for (auto iterator = m_userMetadataRequests.cbegin();
         iterator != m_userMetadataRequests.cend(); ++iterator) {
        if (iterator.value().imageId == imageId) {
            return 0;
        }
    }
    if (m_userMetadataRequests.size() >= kMaximumPendingUserMetadataUpdates) {
        setErrorString(QStringLiteral(
            "too many SSTV metadata updates are already pending"));
        return 0;
    }

    const quint64 requestId = nextRequestId();
    m_userMetadataRequests.insert(
        requestId, PendingUserMetadata {imageId, normalizedNote,
                                        normalizedTags});
    if (!m_worker->enqueueDatabaseOperation(
            [imageId, normalizedNote, normalizedTags, requestId](
                SstvStorageWorker& storage) {
                storage.updateUserMetadata(imageId, normalizedNote,
                                           normalizedTags, requestId);
            })) {
        m_userMetadataRequests.remove(requestId);
        emit userMetadataUpdateFinished(
            requestId, imageId, normalizedNote, normalizedTags, false,
            QStringLiteral("could not queue SSTV metadata update"));
        return 0;
    }
    return requestId;
}

quint64 SstvGalleryModel::associateWithQso(const QString& imageId,
                                           const QString& qsoId)
{
    const int row = indexOfId(imageId);
    QString validationError;
    if (!m_acceptingResults || !m_worker || row < 0
        || !validateSstvQsoId(qsoId, true, &validationError)
        || m_records.at(row).relatedQsoId == qsoId
        || m_qsoAssociationRequests.size()
            >= kMaximumPendingQsoAssociations) {
        if (!validationError.isEmpty()) {
            setErrorString(validationError);
        }
        return 0;
    }
    for (auto iterator = m_qsoAssociationRequests.cbegin();
         iterator != m_qsoAssociationRequests.cend(); ++iterator) {
        if (iterator.value().imageId == imageId) {
            return 0;
        }
    }

    const quint64 requestId = nextRequestId();
    m_qsoAssociationRequests.insert(
        requestId, PendingQsoAssociation {imageId, qsoId});
    if (!m_worker->enqueueDatabaseOperation(
            [imageId, qsoId, requestId](SstvStorageWorker& storage) {
                storage.associateWithQso(imageId, qsoId, requestId);
            })) {
        m_qsoAssociationRequests.remove(requestId);
        emit qsoAssociationFinished(
            requestId, imageId, qsoId, false,
            QStringLiteral("could not queue QSO association update"));
        return 0;
    }
    return requestId;
}

quint64 SstvGalleryModel::requestDeleteSelectedFromIndex()
{
    QStringList ids = m_selected.values();
    ids.sort();
    return requestDeleteFromIndex(ids);
}

quint64 SstvGalleryModel::requestDeleteFromIndex(const QStringList& ids)
{
    if (!m_acceptingResults || !m_worker || ids.isEmpty()
        || ids.size() > 500) {
        return 0;
    }
    const quint64 requestId = nextRequestId();
    m_deleteRequests.insert(requestId, ids);
    emit deleteRequested(requestId, ids);
    if (!m_worker->enqueueDatabaseOperation(
            [ids, requestId](SstvStorageWorker& storage) {
                storage.removeRecords(ids, requestId);
            })) {
        m_deleteRequests.remove(requestId);
        emit deleteFinished(requestId, false,
                            QStringLiteral("could not queue SSTV index deletion"));
        return 0;
    }
    return requestId;
}

quint64 SstvGalleryModel::requestDeleteSelectedWithFiles()
{
    QStringList ids = m_selected.values();
    ids.sort();
    return requestDeleteWithFiles(ids);
}

quint64 SstvGalleryModel::requestDeleteWithFiles(const QStringList& ids)
{
    if (!m_acceptingResults || !m_worker || ids.isEmpty()
        || ids.size() > 500) {
        return 0;
    }
    const quint64 requestId = nextRequestId();
    m_fileDeleteRequests.insert(requestId, ids);
    if (!m_worker->enqueueDatabaseOperation(
            [ids, requestId](SstvStorageWorker& storage) {
                storage.deleteRecordsWithFiles(ids, requestId);
            })) {
        m_fileDeleteRequests.remove(requestId);
        emit deleteFilesFinished(
            requestId, false,
            QStringLiteral("could not queue SSTV file deletion"));
        return 0;
    }
    return requestId;
}

quint64 SstvGalleryModel::requestExportRecord(const QString& id,
                                               const QUrl& destination,
                                               bool replaceExisting)
{
    if (!m_acceptingResults || !m_worker || indexOfId(id) < 0
        || !destination.isLocalFile()
        || destination.toLocalFile().isEmpty()) {
        return 0;
    }
    const quint64 requestId = nextRequestId();
    const QString destinationPath = destination.toLocalFile();
    m_exportRequests.insert(requestId, id);
    if (!m_worker->enqueueDatabaseOperation(
            [id, destinationPath, replaceExisting, requestId](
                SstvStorageWorker& storage) {
                storage.exportRecord(id, destinationPath,
                                     replaceExisting, requestId);
            })) {
        m_exportRequests.remove(requestId);
        emit exportFinished(requestId, false, {},
                            QStringLiteral("could not queue SSTV image export"));
        return 0;
    }
    return requestId;
}

quint64 SstvGalleryModel::refreshQuota()
{
    if (!m_acceptingResults || !m_worker) {
        return 0;
    }
    const quint64 requestId = nextRequestId();
    m_quotaRequests.insert(requestId);
    setRetentionBusy(true);
    if (!m_worker->enqueueDatabaseOperation(
            [requestId](SstvStorageWorker& storage) {
                storage.calculateQuota(requestId);
            })) {
        m_quotaRequests.remove(requestId);
        setRetentionBusy(!m_retentionPreviewRequests.isEmpty()
                         || !m_retentionApplyRequests.isEmpty()
                         || !m_retentionSettingsRequests.isEmpty());
        emit quotaRefreshFinished(requestId, false,
                                  QStringLiteral(
                                      "could not queue quota calculation"));
        return 0;
    }
    return requestId;
}

quint64 SstvGalleryModel::requestRetentionPreview()
{
    if (!m_acceptingResults || !m_worker) {
        return 0;
    }
    SstvRetentionSettings settings;
    QString error;
    if (!SstvRetentionSettings::fromVariantMap(
            m_retentionSettings, &settings, &error)) {
        setErrorString(error);
        return 0;
    }
    const quint64 requestId = nextRequestId();
    m_retentionPreviewRequests.insert(requestId);
    setRetentionBusy(true);
    if (!m_worker->enqueueDatabaseOperation(
            [settings, requestId](SstvStorageWorker& storage) {
                storage.previewRetention(settings, requestId);
            })) {
        m_retentionPreviewRequests.remove(requestId);
        setRetentionBusy(!m_quotaRequests.isEmpty()
                         || !m_retentionApplyRequests.isEmpty()
                         || !m_retentionSettingsRequests.isEmpty());
        emit retentionPreviewFinished(
            requestId, false,
            QStringLiteral("could not queue retention preview"));
        return 0;
    }
    return requestId;
}

quint64 SstvGalleryModel::applyRetentionPreview(
    const QString& token,
    const QString& confirmationPhrase)
{
    if (!m_acceptingResults || !m_worker || token.isEmpty()
        || confirmationPhrase.isEmpty()
        || token != m_retentionPreview.value(
                        QStringLiteral("token")).toString()) {
        return 0;
    }
    const quint64 requestId = nextRequestId();
    m_retentionApplyRequests.insert(requestId, false);
    setRetentionBusy(true);
    if (!m_worker->enqueueDatabaseOperation(
            [token, confirmationPhrase, requestId](
                SstvStorageWorker& storage) {
                storage.applyRetentionPlan(token, confirmationPhrase,
                                           requestId);
            })) {
        m_retentionApplyRequests.remove(requestId);
        setRetentionBusy(!m_quotaRequests.isEmpty()
                         || !m_retentionPreviewRequests.isEmpty()
                         || !m_retentionSettingsRequests.isEmpty());
        emit retentionApplyFinished(
            requestId, false, false,
            QStringLiteral("could not queue retention apply"));
        return 0;
    }
    return requestId;
}

quint64 SstvGalleryModel::updateRetentionSettings(
    const QVariantMap& values)
{
    if (!m_acceptingResults || !m_worker) {
        return 0;
    }
    SstvRetentionSettings settings;
    QString error;
    if (!SstvRetentionSettings::fromVariantMap(values, &settings, &error)) {
        setErrorString(error);
        return 0;
    }
    const quint64 requestId = nextRequestId();
    m_retentionSettingsRequests.insert(requestId);
    setRetentionBusy(true);
    if (!m_worker->enqueueDatabaseOperation(
            [settings, requestId](SstvStorageWorker& storage) {
                storage.updateRetentionSettings(settings, requestId);
            })) {
        m_retentionSettingsRequests.remove(requestId);
        setRetentionBusy(!m_quotaRequests.isEmpty()
                         || !m_retentionPreviewRequests.isEmpty()
                         || !m_retentionApplyRequests.isEmpty());
        emit retentionSettingsFinished(
            requestId, false,
            QStringLiteral("could not queue retention settings update"));
        return 0;
    }
    return requestId;
}

quint64 SstvGalleryModel::requestAutomaticRetention()
{
    if (!m_acceptingResults || !m_worker
        || !m_retentionSettings.value(
                QStringLiteral("automaticEnabled")).toBool()) {
        return 0;
    }
    for (auto iterator = m_retentionApplyRequests.cbegin();
         iterator != m_retentionApplyRequests.cend(); ++iterator) {
        if (iterator.value()) {
            return 0;
        }
    }
    const quint64 requestId = nextRequestId();
    m_retentionApplyRequests.insert(requestId, true);
    setRetentionBusy(true);
    if (!m_worker->enqueueDatabaseOperation(
            [requestId](SstvStorageWorker& storage) {
                storage.runAutomaticRetention(requestId);
            })) {
        m_retentionApplyRequests.remove(requestId);
        setRetentionBusy(!m_quotaRequests.isEmpty()
                         || !m_retentionPreviewRequests.isEmpty()
                         || !m_retentionSettingsRequests.isEmpty());
        emit retentionApplyFinished(
            requestId, true, false,
            QStringLiteral("could not queue automatic retention"));
        return 0;
    }
    return requestId;
}

void SstvGalleryModel::handleRecordExported(
    quint64 requestId,
    bool ok,
    const QString& destinationPath,
    const QString& error)
{
    const auto pending = m_exportRequests.find(requestId);
    if (pending == m_exportRequests.end()) {
        return;
    }
    m_exportRequests.erase(pending);
    emit exportFinished(requestId, ok,
                        ok ? QUrl::fromLocalFile(destinationPath) : QUrl {},
                        error);
}

void SstvGalleryModel::handleRetentionSettingsLoaded(
    const SstvRetentionSettings& settings,
    const QString& error)
{
    if (!m_acceptingResults || !error.isEmpty()) {
        if (!error.isEmpty()) {
            setErrorString(error);
        }
        return;
    }
    const QVariantMap values = settings.toVariantMap();
    if (m_retentionSettings != values) {
        m_retentionSettings = values;
        emit retentionSettingsChanged();
    }
    if (settings.automaticEnabled) {
        requestAutomaticRetention();
    }
}

void SstvGalleryModel::handleRetentionSettingsUpdated(
    quint64 requestId,
    bool ok,
    const SstvRetentionSettings& settings,
    const QString& error)
{
    if (!m_retentionSettingsRequests.remove(requestId)) {
        return;
    }
    if (ok) {
        const QVariantMap values = settings.toVariantMap();
        if (m_retentionSettings != values) {
            m_retentionSettings = values;
            emit retentionSettingsChanged();
        }
        if (!m_retentionPreview.isEmpty()) {
            m_retentionPreview.clear();
            emit retentionPreviewChanged();
        }
    }
    setRetentionBusy(!m_quotaRequests.isEmpty()
                     || !m_retentionPreviewRequests.isEmpty()
                     || !m_retentionApplyRequests.isEmpty()
                     || !m_retentionSettingsRequests.isEmpty());
    emit retentionSettingsFinished(requestId, ok, error);
    if (ok && settings.automaticEnabled) {
        requestAutomaticRetention();
    }
}

void SstvGalleryModel::handleQuotaCalculated(
    quint64 requestId,
    const SstvQuotaSummary& summary,
    const QString& error)
{
    if (!m_quotaRequests.remove(requestId)) {
        return;
    }
    if (error.isEmpty()) {
        const QVariantMap values = summary.toVariantMap();
        if (m_quotaSummary != values) {
            m_quotaSummary = values;
            emit quotaSummaryChanged();
        }
    }
    setRetentionBusy(!m_quotaRequests.isEmpty()
                     || !m_retentionPreviewRequests.isEmpty()
                     || !m_retentionApplyRequests.isEmpty()
                     || !m_retentionSettingsRequests.isEmpty());
    emit quotaRefreshFinished(requestId, error.isEmpty(), error);
}

void SstvGalleryModel::handleRetentionPreviewReady(
    quint64 requestId,
    const SstvRetentionPlan& plan,
    const QString& error)
{
    if (!m_retentionPreviewRequests.remove(requestId)) {
        return;
    }
    const QVariantMap values = error.isEmpty() ? plan.toVariantMap()
                                                : QVariantMap {};
    if (m_retentionPreview != values) {
        m_retentionPreview = values;
        emit retentionPreviewChanged();
    }
    setRetentionBusy(!m_quotaRequests.isEmpty()
                     || !m_retentionPreviewRequests.isEmpty()
                     || !m_retentionApplyRequests.isEmpty()
                     || !m_retentionSettingsRequests.isEmpty());
    emit retentionPreviewFinished(requestId, error.isEmpty(), error);
}

void SstvGalleryModel::shutdown()
{
    if (!m_acceptingResults) {
        return;
    }
    m_acceptingResults = false;
    cancelPending();
    for (const QMetaObject::Connection& connection : std::as_const(m_connections)) {
        QObject::disconnect(connection);
    }
    m_connections.clear();
    m_worker = nullptr;
    setHasMore(false);
    const auto pendingDeletes = m_deleteRequests;
    m_deleteRequests.clear();
    for (auto iterator = pendingDeletes.cbegin();
         iterator != pendingDeletes.cend(); ++iterator) {
        emit deleteFinished(iterator.key(), false,
                            QStringLiteral("gallery model shut down; delete outcome is unknown"));
    }
    const auto pendingExports = m_exportRequests;
    m_exportRequests.clear();
    for (auto iterator = pendingExports.cbegin();
         iterator != pendingExports.cend(); ++iterator) {
        emit exportFinished(iterator.key(), false, {},
                            QStringLiteral("gallery model shut down; export outcome is unknown"));
    }
    const auto pendingFileDeletes = m_fileDeleteRequests;
    m_fileDeleteRequests.clear();
    for (auto iterator = pendingFileDeletes.cbegin();
         iterator != pendingFileDeletes.cend(); ++iterator) {
        emit deleteFilesFinished(iterator.key(), false,
                                 QStringLiteral("gallery model shut down; file deletion outcome is unknown"));
    }
    const auto pendingFavorites = m_favoriteRequests;
    m_favoriteRequests.clear();
    for (auto iterator = pendingFavorites.cbegin();
         iterator != pendingFavorites.cend(); ++iterator) {
        emit favoriteFinished(iterator.key(), iterator.value(), false,
                              QStringLiteral(
                                  "gallery model shut down; favourite outcome is unknown"));
    }
    const auto pendingQsoAssociations = m_qsoAssociationRequests;
    m_qsoAssociationRequests.clear();
    for (auto iterator = pendingQsoAssociations.cbegin();
         iterator != pendingQsoAssociations.cend(); ++iterator) {
        emit qsoAssociationFinished(
            iterator.key(), iterator.value().imageId,
            iterator.value().qsoId, false,
            QStringLiteral(
                "gallery model shut down; QSO association outcome is unknown"));
    }
    const auto pendingUserMetadata = m_userMetadataRequests;
    m_userMetadataRequests.clear();
    for (auto iterator = pendingUserMetadata.cbegin();
         iterator != pendingUserMetadata.cend(); ++iterator) {
        emit userMetadataUpdateFinished(
            iterator.key(), iterator.value().imageId,
            iterator.value().note, iterator.value().tags, false,
            QStringLiteral(
                "gallery model shut down; metadata update outcome is unknown"));
    }
    clearRetentionState(QStringLiteral(
        "gallery model shut down; retention outcome is unknown"));
}

void SstvGalleryModel::setLoading(bool value)
{
    if (m_loading == value) {
        return;
    }
    m_loading = value;
    emit loadingChanged();
}

void SstvGalleryModel::setHasMore(bool value)
{
    if (m_hasMore == value) {
        return;
    }
    m_hasMore = value;
    emit hasMoreChanged();
}

void SstvGalleryModel::setErrorString(const QString& value)
{
    if (m_errorString == value) {
        return;
    }
    m_errorString = value;
    emit errorStringChanged();
}

void SstvGalleryModel::setRetentionBusy(bool value)
{
    if (m_retentionBusy == value) {
        return;
    }
    m_retentionBusy = value;
    emit retentionBusyChanged();
}

void SstvGalleryModel::clearRetentionState(const QString& reason)
{
    const auto settings = m_retentionSettingsRequests;
    m_retentionSettingsRequests.clear();
    for (quint64 requestId : settings) {
        emit retentionSettingsFinished(requestId, false, reason);
    }
    const auto quotas = m_quotaRequests;
    m_quotaRequests.clear();
    for (quint64 requestId : quotas) {
        emit quotaRefreshFinished(requestId, false, reason);
    }
    const auto previews = m_retentionPreviewRequests;
    m_retentionPreviewRequests.clear();
    for (quint64 requestId : previews) {
        emit retentionPreviewFinished(requestId, false, reason);
    }
    const auto applies = m_retentionApplyRequests;
    m_retentionApplyRequests.clear();
    for (auto iterator = applies.cbegin(); iterator != applies.cend(); ++iterator) {
        emit retentionApplyFinished(iterator.key(), iterator.value(), false,
                                    reason);
    }
    setRetentionBusy(false);
    const QVariantMap safeSettings = SstvRetentionSettings {}.toVariantMap();
    if (m_retentionSettings != safeSettings) {
        m_retentionSettings = safeSettings;
        emit retentionSettingsChanged();
    }
    const QVariantMap emptyQuota = SstvQuotaSummary {}.toVariantMap();
    if (m_quotaSummary != emptyQuota) {
        m_quotaSummary = emptyQuota;
        emit quotaSummaryChanged();
    }
    if (!m_retentionPreview.isEmpty()) {
        m_retentionPreview.clear();
        emit retentionPreviewChanged();
    }
}

void SstvGalleryModel::clearRowsIncrementally()
{
    if (!m_records.isEmpty()) {
        if (m_thumbnailProvider) {
            for (const SstvImageRecord& record : std::as_const(m_records)) {
                m_thumbnailProvider->unregisterSource(record.id);
            }
        }
        beginRemoveRows({}, 0, static_cast<int>(m_records.size()) - 1);
        m_records.clear();
        endRemoveRows();
    }
    clearSelection();
}

int SstvGalleryModel::indexOfId(const QString& id) const
{
    for (qsizetype row = 0; row < m_records.size(); ++row) {
        if (m_records.at(row).id == id) {
            return static_cast<int>(row);
        }
    }
    return -1;
}

quint64 SstvGalleryModel::nextRequestId()
{
    if (m_nextRequestId == 0) {
        m_nextRequestId = 1;
    }
    return m_nextRequestId++;
}

} // namespace decodium::sstv
