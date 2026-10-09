// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvThumbnailProvider.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QMutexLocker>
#include <QSet>
#include <QUuid>

#include <limits>
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

bool canonicalUuid(const QString& value)
{
    const QUuid uuid(value);
    return value.size() == 36 && value == value.toLower()
        && !uuid.isNull()
        && uuid.toString(QUuid::WithoutBraces) == value;
}

qint64 checkedPixels(const QSize& size)
{
    if (size.width() <= 0 || size.height() <= 0
        || static_cast<qint64>(size.width())
            > std::numeric_limits<qint64>::max()
                / static_cast<qint64>(size.height())) {
        return -1;
    }
    return static_cast<qint64>(size.width())
        * static_cast<qint64>(size.height());
}

} // namespace

struct SstvThumbnailSharedState final
{
    std::atomic_bool shuttingDown {false};
    std::atomic_llong cacheBytes {0};
    std::atomic_int cacheEntries {0};
    std::atomic<quintptr> lastWorkerThreadToken {0};
    QMutex cancellationMutex;
    QSet<quint64> cancelled;
};

class SstvThumbnailResponse final : public QQuickImageResponse
{
    Q_OBJECT
    Q_PROPERTY(QString sstvRecordId READ recordId CONSTANT)
    Q_PROPERTY(bool sstvFromCache READ fromCache)

public:
    SstvThumbnailResponse(quint64 requestId,
                          QString recordId,
                          std::shared_ptr<SstvThumbnailSharedState> shared)
        : m_requestId(requestId)
        , m_recordId(std::move(recordId))
        , m_shared(std::move(shared))
    {
    }

    QString recordId() const { return m_recordId; }

    bool fromCache() const
    {
        QMutexLocker locker(&m_mutex);
        return m_fromCache;
    }

    QQuickTextureFactory* textureFactory() const override
    {
        QMutexLocker locker(&m_mutex);
        return m_image.isNull()
            ? nullptr : QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override
    {
        QMutexLocker locker(&m_mutex);
        return m_error;
    }

    void cancel() override
    {
        if (m_completed.load(std::memory_order_acquire)) {
            return;
        }
        {
            QMutexLocker locker(&m_shared->cancellationMutex);
            m_shared->cancelled.insert(m_requestId);
        }
        complete({}, QStringLiteral("thumbnail request cancelled"), false);
    }

    void complete(QImage image, QString error, bool fromCache)
    {
        bool expected = false;
        if (!m_completed.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel)) {
            return;
        }
        {
            QMutexLocker locker(&m_mutex);
            m_image = std::move(image);
            m_error = std::move(error);
            m_fromCache = fromCache;
        }
        emit finished();
    }

private:
    quint64 m_requestId {0};
    QString m_recordId;
    std::shared_ptr<SstvThumbnailSharedState> m_shared;
    mutable QMutex m_mutex;
    QImage m_image;
    QString m_error;
    bool m_fromCache {false};
    std::atomic_bool m_completed {false};
};

class SstvThumbnailWorker final : public QObject
{
    Q_OBJECT

public:
    SstvThumbnailWorker(SstvThumbnailLimits limits,
                        std::shared_ptr<SstvThumbnailSharedState> shared)
        : m_limits(limits)
        , m_shared(std::move(shared))
    {
    }

public slots:
    void process(quint64 requestId,
                 QString recordId,
                 QString path,
                 QSize requestedSize)
    {
        m_shared->lastWorkerThreadToken.store(
            reinterpret_cast<quintptr>(QThread::currentThreadId()),
            std::memory_order_release);
        auto finish = [&](QImage image, const QString& error, bool fromCache) {
            {
                QMutexLocker locker(&m_shared->cancellationMutex);
                m_shared->cancelled.remove(requestId);
            }
            emit completed(requestId, recordId, std::move(image), error,
                           fromCache,
                           m_shared->cacheBytes.load(std::memory_order_acquire),
                           m_shared->cacheEntries.load(std::memory_order_acquire));
        };
        if (isCancelled(requestId)) {
            finish({}, QStringLiteral("thumbnail request cancelled"), false);
            return;
        }

        const QFileInfo file(path);
        if (!file.exists() || !file.isFile() || !file.isReadable()) {
            finish({}, QStringLiteral("thumbnail source image is missing or unreadable"),
                   false);
            return;
        }
        const qint64 sourceBytes = file.size();
        if (sourceBytes <= 0 || sourceBytes > m_limits.maximumSourceBytes) {
            finish({}, QStringLiteral("thumbnail source exceeds its byte limit"),
                   false);
            return;
        }
        const QString canonicalPath = QDir::cleanPath(file.canonicalFilePath());
        if (canonicalPath.isEmpty()) {
            finish({}, QStringLiteral("thumbnail source path cannot be canonicalized"),
                   false);
            return;
        }
        const QString key = QStringLiteral("%1\x1f%2\x1f%3\x1f%4x%5\x1f%6")
            .arg(recordId, canonicalPath)
            .arg(file.lastModified().toMSecsSinceEpoch())
            .arg(requestedSize.width())
            .arg(requestedSize.height())
            .arg(sourceBytes);
        auto cached = m_cache.find(key);
        if (cached != m_cache.end()) {
            cached->lastUse = ++m_clock;
            finish(cached->image, {}, true);
            return;
        }

        QFile sourceFile(canonicalPath);
        if (!sourceFile.open(QIODevice::ReadOnly)
            || sourceFile.size() <= 0
            || sourceFile.size() > m_limits.maximumSourceBytes) {
            finish({}, QStringLiteral("thumbnail source changed or cannot be opened"),
                   false);
            return;
        }
        QImageReader reader(&sourceFile);
        reader.setAutoTransform(true);
        reader.setDecideFormatFromContent(true);
        const QSize sourceSize = reader.size();
        const qint64 pixels = checkedPixels(sourceSize);
        if (pixels <= 0 || sourceSize.width() > m_limits.maximumSourceWidth
            || sourceSize.height() > m_limits.maximumSourceHeight
            || pixels > m_limits.maximumSourcePixels) {
            finish({}, QStringLiteral("thumbnail source dimensions exceed limits"),
                   false);
            return;
        }
        QSize decodeSize = sourceSize;
        if (sourceSize.width() > requestedSize.width()
            || sourceSize.height() > requestedSize.height()) {
            decodeSize.scale(requestedSize, Qt::KeepAspectRatio);
            reader.setScaledSize(decodeSize);
        }
        if (isCancelled(requestId)) {
            finish({}, QStringLiteral("thumbnail request cancelled"), false);
            return;
        }
        QImage image = reader.read();
        if (image.isNull()) {
            finish({}, QStringLiteral("thumbnail decode failed: %1")
                           .arg(reader.errorString()), false);
            return;
        }
        if (image.width() > requestedSize.width()
            || image.height() > requestedSize.height()) {
            image = image.scaled(requestedSize, Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
        }
        image = image.convertToFormat(QImage::Format_RGBA8888);
        if (isCancelled(requestId)) {
            finish({}, QStringLiteral("thumbnail request cancelled"), false);
            return;
        }

        const qint64 imageBytes = static_cast<qint64>(image.sizeInBytes());
        if (imageBytes > 0 && imageBytes <= m_limits.maximumCacheBytes) {
            while (!m_cache.isEmpty()
                   && (m_cache.size() >= m_limits.maximumCacheEntries
                       || m_cacheBytes
                           > m_limits.maximumCacheBytes - imageBytes)) {
                auto oldest = m_cache.begin();
                for (auto iterator = m_cache.begin();
                     iterator != m_cache.end(); ++iterator) {
                    if (iterator->lastUse < oldest->lastUse) {
                        oldest = iterator;
                    }
                }
                m_cacheBytes -= oldest->bytes;
                m_cache.erase(oldest);
            }
            CacheEntry entry;
            entry.recordId = recordId;
            entry.path = canonicalPath;
            entry.image = image;
            entry.bytes = imageBytes;
            entry.lastUse = ++m_clock;
            m_cache.insert(key, std::move(entry));
            m_cacheBytes += imageBytes;
            publishCacheStats();
        }
        finish(std::move(image), {}, false);
    }

    void invalidate(const QString& recordId)
    {
        for (auto iterator = m_cache.begin(); iterator != m_cache.end();) {
            if (iterator->recordId == recordId) {
                m_cacheBytes -= iterator->bytes;
                iterator = m_cache.erase(iterator);
            } else {
                ++iterator;
            }
        }
        publishCacheStats();
        emit invalidated(recordId);
    }

    void clearCache()
    {
        m_cache.clear();
        m_cacheBytes = 0;
        publishCacheStats();
    }

    void clearAndStop()
    {
        clearCache();
    }

signals:
    void completed(quint64 requestId,
                   QString recordId,
                   QImage image,
                   QString error,
                   bool fromCache,
                   qint64 cacheBytes,
                   int cacheEntries);
    void cacheChanged(qint64 cacheBytes, int cacheEntries);
    void invalidated(QString recordId);

private:
    struct CacheEntry final
    {
        QString recordId;
        QString path;
        QImage image;
        qint64 bytes {0};
        quint64 lastUse {0};
    };

    bool isCancelled(quint64 requestId) const
    {
        if (m_shared->shuttingDown.load(std::memory_order_acquire)) {
            return true;
        }
        QMutexLocker locker(&m_shared->cancellationMutex);
        return m_shared->cancelled.contains(requestId);
    }

    void publishCacheStats()
    {
        const int entries = static_cast<int>(m_cache.size());
        m_shared->cacheBytes.store(m_cacheBytes, std::memory_order_release);
        m_shared->cacheEntries.store(entries, std::memory_order_release);
        emit cacheChanged(m_cacheBytes, entries);
    }

    SstvThumbnailLimits m_limits;
    std::shared_ptr<SstvThumbnailSharedState> m_shared;
    QHash<QString, CacheEntry> m_cache;
    qint64 m_cacheBytes {0};
    quint64 m_clock {0};
};

bool SstvThumbnailLimits::validate(QString* error) const
{
    if (maximumPendingRequests <= 0 || maximumPendingRequests > 4096
        || maximumRegisteredSources <= 0
        || maximumRegisteredSources > 1'000'000
        || maximumCacheBytes <= 0
        || maximumCacheBytes > 2LL * 1024LL * 1024LL * 1024LL
        || maximumCacheEntries <= 0 || maximumCacheEntries > 65'536
        || maximumEdge < 16 || maximumEdge > 4096
        || maximumSourceBytes <= 0
        || maximumSourceBytes > 2LL * 1024LL * 1024LL * 1024LL
        || maximumSourceWidth <= 0 || maximumSourceWidth > 32768
        || maximumSourceHeight <= 0 || maximumSourceHeight > 32768
        || maximumSourcePixels <= 0) {
        return fail(error, QStringLiteral("invalid SSTV thumbnail limits"));
    }
    const qint64 maximumDimensionPixels = checkedPixels(
        QSize(maximumSourceWidth, maximumSourceHeight));
    if (maximumDimensionPixels <= 0
        || maximumSourcePixels > maximumDimensionPixels) {
        return fail(error, QStringLiteral(
            "thumbnail pixel limit exceeds dimension limits"));
    }
    return true;
}

SstvThumbnailProvider::SstvThumbnailProvider(SstvThumbnailLimits limits)
    : m_limits(limits)
    , m_shared(std::make_shared<SstvThumbnailSharedState>())
{
    if (!m_limits.validate(&m_configurationError)) {
        m_shared->shuttingDown.store(true, std::memory_order_release);
        return;
    }
    m_worker = new SstvThumbnailWorker(m_limits, m_shared);
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished,
            m_worker, &QObject::deleteLater);
    connect(m_worker, &SstvThumbnailWorker::completed,
            this, &SstvThumbnailProvider::handleCompleted,
            Qt::QueuedConnection);
    connect(m_worker, &SstvThumbnailWorker::cacheChanged,
            this, &SstvThumbnailProvider::handleCacheChanged,
            Qt::QueuedConnection);
    connect(m_worker, &SstvThumbnailWorker::invalidated,
            this, &SstvThumbnailProvider::cacheInvalidated,
            Qt::QueuedConnection);
    m_thread.setObjectName(QStringLiteral("SSTV thumbnail worker"));
    m_thread.start();
}

SstvThumbnailProvider::~SstvThumbnailProvider()
{
    shutdown();
}

qint64 SstvThumbnailProvider::cacheBytes() const noexcept
{
    return m_shared->cacheBytes.load(std::memory_order_acquire);
}

int SstvThumbnailProvider::cacheEntries() const noexcept
{
    return m_shared->cacheEntries.load(std::memory_order_acquire);
}

int SstvThumbnailProvider::registeredSourceCount() const
{
    QMutexLocker locker(&m_stateMutex);
    return static_cast<int>(m_sources.size());
}

bool SstvThumbnailProvider::shuttingDown() const noexcept
{
    return m_shared->shuttingDown.load(std::memory_order_acquire);
}

quintptr SstvThumbnailProvider::lastWorkerThreadToken() const noexcept
{
    return m_shared->lastWorkerThreadToken.load(std::memory_order_acquire);
}

bool SstvThumbnailProvider::registerSource(
    const QString& recordId,
    const QString& absoluteImagePath)
{
    if (!canonicalUuid(recordId)
        || absoluteImagePath.isEmpty()
        || !QFileInfo(absoluteImagePath).isAbsolute()
        || QDir::cleanPath(absoluteImagePath) != absoluteImagePath) {
        emit requestRejected(recordId,
                             QStringLiteral("invalid thumbnail source registration"));
        return false;
    }
    QString invalidatedId;
    bool registryChanged = false;
    bool rejected = false;
    {
        QMutexLocker locker(&m_stateMutex);
        if (shuttingDown()) {
            rejected = true;
        } else {
            auto existing = m_sources.find(recordId);
            if (existing != m_sources.end()
                && existing->path == absoluteImagePath) {
                touchSource(existing);
                return true;
            }
            if (existing != m_sources.end()) {
                m_sourceOrder.remove(existing->lastUse);
                existing->path = absoluteImagePath;
                touchSource(existing);
                invalidatedId = recordId;
            } else {
                if (m_sources.size()
                    >= m_limits.maximumRegisteredSources) {
                    const auto oldest = m_sourceOrder.cbegin();
                    if (oldest != m_sourceOrder.cend()) {
                        invalidatedId = oldest.value();
                        m_sourceOrder.erase(oldest);
                        m_sources.remove(invalidatedId);
                    }
                }
                SourceEntry entry;
                entry.path = absoluteImagePath;
                entry.lastUse = nextSourceUse();
                m_sourceOrder.insert(entry.lastUse, recordId);
                m_sources.insert(recordId, std::move(entry));
                registryChanged = true;
            }
        }
    }
    if (rejected) {
        emit requestRejected(
            recordId, QStringLiteral("thumbnail provider is shut down"));
        return false;
    }
    if (!invalidatedId.isEmpty()) {
        invalidateThumbnail(invalidatedId);
    }
    if (registryChanged) {
        emit sourceRegistryChanged();
    }
    return true;
}

void SstvThumbnailProvider::unregisterSource(const QString& recordId)
{
    bool removed = false;
    {
        QMutexLocker locker(&m_stateMutex);
        const auto iterator = m_sources.find(recordId);
        if (iterator != m_sources.end()) {
            m_sourceOrder.remove(iterator->lastUse);
            m_sources.erase(iterator);
            removed = true;
        }
    }
    if (!removed) {
        return;
    }
    emit sourceRegistryChanged();
    invalidateThumbnail(recordId);
}

void SstvThumbnailProvider::invalidateThumbnail(const QString& recordId)
{
    if (recordId.isEmpty() || shuttingDown()) {
        return;
    }
    QPointer<SstvThumbnailWorker> worker;
    {
        QMutexLocker locker(&m_stateMutex);
        if (shuttingDown()) {
            return;
        }
        worker = m_worker;
    }
    if (!worker) {
        return;
    }
    QMetaObject::invokeMethod(
        worker.data(),
        [worker, recordId]() {
            if (worker) {
                worker->invalidate(recordId);
            }
        }, Qt::QueuedConnection);
}

void SstvThumbnailProvider::clearCache()
{
    if (shuttingDown()) {
        return;
    }
    QPointer<SstvThumbnailWorker> worker;
    {
        QMutexLocker locker(&m_stateMutex);
        if (shuttingDown()) {
            return;
        }
        worker = m_worker;
    }
    if (!worker) {
        return;
    }
    QMetaObject::invokeMethod(
        worker.data(),
        [worker]() {
            if (worker) {
                worker->clearCache();
            }
        }, Qt::QueuedConnection);
}

QSize SstvThumbnailProvider::boundedRequestedSize(
    const QSize& requestedSize) const
{
    int width = requestedSize.width();
    int height = requestedSize.height();
    if (width <= 0 && height <= 0) {
        width = 256;
        height = 256;
    } else if (width <= 0) {
        width = height;
    } else if (height <= 0) {
        height = width;
    }
    width = qBound(1, width, m_limits.maximumEdge);
    height = qBound(1, height, m_limits.maximumEdge);
    return QSize(width, height);
}

QQuickImageResponse* SstvThumbnailProvider::rejectedResponse(
    const QString& recordId,
    const QString& error)
{
    auto* response = new SstvThumbnailResponse(0, recordId, m_shared);
    emit requestRejected(recordId, error);
    const QPointer<SstvThumbnailResponse> guarded = response;
    QMetaObject::invokeMethod(
        this,
        [guarded, error]() {
            if (guarded) {
                guarded->complete({}, error, false);
            }
        }, Qt::QueuedConnection);
    return response;
}

QQuickImageResponse* SstvThumbnailProvider::requestImageResponse(
    const QString& id,
    const QSize& requestedSize)
{
    if (!m_configurationError.isEmpty()) {
        return rejectedResponse(id, m_configurationError);
    }
    QString rejection;
    QString path;
    quint64 requestId = 0;
    QPointer<SstvThumbnailWorker> worker;
    SstvThumbnailResponse* response = nullptr;
    {
        QMutexLocker locker(&m_stateMutex);
        if (shuttingDown() || !m_worker) {
            rejection = QStringLiteral(
                "SSTV thumbnail provider is shut down");
        } else if (!canonicalUuid(id)) {
            rejection = QStringLiteral(
                "thumbnail identifier is not registered");
        } else {
            auto source = m_sources.find(id);
            if (source == m_sources.end()) {
                rejection = QStringLiteral(
                    "thumbnail identifier is not registered");
            } else if (m_pendingCount.load(std::memory_order_acquire)
                       >= m_limits.maximumPendingRequests) {
                rejection = QStringLiteral(
                    "thumbnail request queue is full");
            } else {
                touchSource(source);
                path = source->path;
                requestId = nextRequestId();
                response = new SstvThumbnailResponse(
                    requestId, id, m_shared);
                m_responses.insert(requestId, response);
                m_pendingCount.fetch_add(1, std::memory_order_acq_rel);
                worker = m_worker;
            }
        }
    }
    if (!rejection.isEmpty()) {
        return rejectedResponse(id, rejection);
    }
    emit pendingCountChanged();

    const QSize boundedSize = boundedRequestedSize(requestedSize);
    if (!QMetaObject::invokeMethod(
            worker.data(),
            [worker, requestId, id, path, boundedSize]() {
                if (worker) {
                    worker->process(requestId, id, path, boundedSize);
                }
            }, Qt::QueuedConnection)) {
        bool ownedPending = false;
        {
            QMutexLocker locker(&m_stateMutex);
            if (m_responses.remove(requestId) > 0) {
                m_pendingCount.fetch_sub(1, std::memory_order_acq_rel);
                ownedPending = true;
            }
        }
        if (ownedPending) {
            emit pendingCountChanged();
        }
        const QPointer<SstvThumbnailResponse> guarded = response;
        QMetaObject::invokeMethod(
            this,
            [guarded]() {
                if (guarded) {
                    guarded->complete(
                        {}, QStringLiteral(
                            "could not queue thumbnail request"), false);
                }
            }, Qt::QueuedConnection);
    }
    return response;
}

void SstvThumbnailProvider::handleCompleted(
    quint64 requestId,
    QString recordId,
    QImage image,
    QString error,
    bool fromCache,
    qint64 cacheByteCount,
    int cacheEntryCount)
{
    QPointer<SstvThumbnailResponse> response;
    {
        QMutexLocker locker(&m_stateMutex);
        const auto iterator = m_responses.find(requestId);
        if (iterator == m_responses.end()) {
            return;
        }
        response = iterator.value();
        m_responses.erase(iterator);
        m_pendingCount.fetch_sub(1, std::memory_order_acq_rel);
    }
    emit pendingCountChanged();
    m_shared->cacheBytes.store(cacheByteCount, std::memory_order_release);
    m_shared->cacheEntries.store(cacheEntryCount, std::memory_order_release);
    emit cacheStatsChanged();
    emit workerDiagnosticsChanged();
    if (response) {
        response->complete(std::move(image), error, fromCache);
    }
    emit requestCompleted(requestId, std::move(recordId), fromCache, error);
}

void SstvThumbnailProvider::handleCacheChanged(qint64 cacheByteCount,
                                               int cacheEntryCount)
{
    if (shuttingDown()) {
        return;
    }
    m_shared->cacheBytes.store(cacheByteCount, std::memory_order_release);
    m_shared->cacheEntries.store(cacheEntryCount, std::memory_order_release);
    emit cacheStatsChanged();
}

void SstvThumbnailProvider::shutdown()
{
    const bool wasShuttingDown = m_shared->shuttingDown.exchange(
        true, std::memory_order_acq_rel);
    if (wasShuttingDown) {
        return;
    }
    emit shuttingDownChanged();

    QVector<QPair<quint64, QPointer<SstvThumbnailResponse>>> responses;
    QPointer<SstvThumbnailWorker> worker;
    bool pendingChanged = false;
    bool registryChanged = false;
    {
        QMutexLocker locker(&m_stateMutex);
        responses.reserve(m_responses.size());
        for (auto iterator = m_responses.cbegin();
             iterator != m_responses.cend(); ++iterator) {
            responses.append({iterator.key(), iterator.value()});
        }
        m_responses.clear();
        pendingChanged = m_pendingCount.exchange(
            0, std::memory_order_acq_rel) != 0;
        registryChanged = !m_sources.isEmpty();
        m_sources.clear();
        m_sourceOrder.clear();
        worker = m_worker;
    }
    {
        QMutexLocker locker(&m_shared->cancellationMutex);
        for (const auto& response : std::as_const(responses)) {
            m_shared->cancelled.insert(response.first);
        }
    }
    for (const auto& response : std::as_const(responses)) {
        if (response.second) {
            response.second->complete(
                {}, QStringLiteral("SSTV thumbnail provider shut down"), false);
        }
    }
    if (pendingChanged) {
        emit pendingCountChanged();
    }
    if (registryChanged) {
        emit sourceRegistryChanged();
    }

    if (worker && m_thread.isRunning()) {
        QMetaObject::invokeMethod(worker.data(),
                                  &SstvThumbnailWorker::clearAndStop,
                                  Qt::BlockingQueuedConnection);
        QObject::disconnect(worker.data(), nullptr, this, nullptr);
        m_thread.quit();
        m_thread.wait();
        QMutexLocker locker(&m_stateMutex);
        m_worker = nullptr;
    }
    m_shared->cacheBytes.store(0, std::memory_order_release);
    m_shared->cacheEntries.store(0, std::memory_order_release);
    emit cacheStatsChanged();
    emit shutdownFinished();
}

quint64 SstvThumbnailProvider::nextRequestId()
{
    if (m_nextRequestId == 0) {
        m_nextRequestId = 1;
    }
    return m_nextRequestId++;
}

quint64 SstvThumbnailProvider::nextSourceUse()
{
    if (m_sourceClock == std::numeric_limits<quint64>::max()) {
        QMap<quint64, QString> renormalized;
        m_sourceClock = 0;
        for (auto order = m_sourceOrder.cbegin();
             order != m_sourceOrder.cend(); ++order) {
            auto source = m_sources.find(order.value());
            if (source != m_sources.end()) {
                source->lastUse = ++m_sourceClock;
                renormalized.insert(source->lastUse, source.key());
            }
        }
        m_sourceOrder = std::move(renormalized);
    }
    return ++m_sourceClock;
}

void SstvThumbnailProvider::touchSource(
    QHash<QString, SourceEntry>::iterator iterator)
{
    m_sourceOrder.remove(iterator->lastUse);
    iterator->lastUse = nextSourceUse();
    m_sourceOrder.insert(iterator->lastUse, iterator.key());
}

} // namespace decodium::sstv

#include "SstvThumbnailProvider.moc"
