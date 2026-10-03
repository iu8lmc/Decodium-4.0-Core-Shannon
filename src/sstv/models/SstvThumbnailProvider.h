// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QMap>
#include <QMutex>
#include <QPointer>
#include <QQuickAsyncImageProvider>
#include <QSize>
#include <QString>
#include <QThread>
#include <QtGlobal>

#include <atomic>
#include <memory>

namespace decodium::sstv {

struct SstvThumbnailLimits final
{
    int maximumPendingRequests {64};
    int maximumRegisteredSources {4096};
    qint64 maximumCacheBytes {64LL * 1024LL * 1024LL};
    int maximumCacheEntries {1024};
    int maximumEdge {1024};
    qint64 maximumSourceBytes {64LL * 1024LL * 1024LL};
    int maximumSourceWidth {8192};
    int maximumSourceHeight {8192};
    qint64 maximumSourcePixels {32LL * 1024LL * 1024LL};

    bool validate(QString* error = nullptr) const;
};

class SstvThumbnailWorker;
class SstvThumbnailResponse;
struct SstvThumbnailSharedState;

// Actual image:// provider.  Identifiers are UUIDs registered with a path by
// registerSource(); arbitrary paths are never accepted from a QML URL.  All
// QFileInfo, QImageReader and cache activity runs on m_thread.
class SstvThumbnailProvider final : public QQuickAsyncImageProvider
{
    Q_OBJECT
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY pendingCountChanged)
    Q_PROPERTY(int registeredSourceCount READ registeredSourceCount NOTIFY sourceRegistryChanged)
    Q_PROPERTY(qint64 cacheBytes READ cacheBytes NOTIFY cacheStatsChanged)
    Q_PROPERTY(int cacheEntries READ cacheEntries NOTIFY cacheStatsChanged)
    Q_PROPERTY(quintptr lastWorkerThreadToken READ lastWorkerThreadToken NOTIFY workerDiagnosticsChanged)
    Q_PROPERTY(bool shuttingDown READ shuttingDown NOTIFY shuttingDownChanged)

public:
    explicit SstvThumbnailProvider(SstvThumbnailLimits limits = {});
    ~SstvThumbnailProvider() override;

    QQuickImageResponse* requestImageResponse(
        const QString& id,
        const QSize& requestedSize) override;

    int pendingCount() const noexcept
    {
        return m_pendingCount.load(std::memory_order_acquire);
    }
    int registeredSourceCount() const;
    qint64 cacheBytes() const noexcept;
    int cacheEntries() const noexcept;
    bool shuttingDown() const noexcept;
    quintptr lastWorkerThreadToken() const noexcept;
    SstvThumbnailLimits limits() const { return m_limits; }

    Q_INVOKABLE bool registerSource(const QString& recordId,
                                    const QString& absoluteImagePath);
    Q_INVOKABLE void unregisterSource(const QString& recordId);
    Q_INVOKABLE void invalidateThumbnail(const QString& recordId);
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE void shutdown();

signals:
    void pendingCountChanged();
    void sourceRegistryChanged();
    void cacheStatsChanged();
    void workerDiagnosticsChanged();
    void requestRejected(QString recordId, QString error);
    void requestCompleted(quint64 requestId,
                          QString recordId,
                          bool fromCache,
                          QString error);
    void cacheInvalidated(QString recordId);
    void shuttingDownChanged();
    void shutdownFinished();

private slots:
    void handleCompleted(quint64 requestId,
                         QString recordId,
                         QImage image,
                         QString error,
                         bool fromCache,
                         qint64 cacheBytes,
                         int cacheEntries);
    void handleCacheChanged(qint64 cacheBytes, int cacheEntries);

private:
    friend class SstvThumbnailResponse;

    struct SourceEntry final
    {
        QString path;
        quint64 lastUse {0};
    };

    QQuickImageResponse* rejectedResponse(const QString& recordId,
                                          const QString& error);
    QSize boundedRequestedSize(const QSize& requestedSize) const;
    quint64 nextRequestId();
    quint64 nextSourceUse();
    void touchSource(QHash<QString, SourceEntry>::iterator iterator);

    SstvThumbnailLimits m_limits;
    std::shared_ptr<SstvThumbnailSharedState> m_shared;
    QThread m_thread;
    QPointer<SstvThumbnailWorker> m_worker;
    mutable QMutex m_stateMutex;
    QHash<QString, SourceEntry> m_sources;
    QMap<quint64, QString> m_sourceOrder;
    QHash<quint64, QPointer<SstvThumbnailResponse>> m_responses;
    std::atomic_int m_pendingCount {0};
    quint64 m_nextRequestId {1};
    quint64 m_sourceClock {0};
    QString m_configurationError;
};

} // namespace decodium::sstv
