#include "DecodiumBridge.h"
#include "DecodiumBridgeTraceScope.h"
#include <QTimer>

void decodiumBridgeAudioLevelLog(const QString&);

bool DecodiumBridge::visualFeedsDeferredForTx() const
{
    return m_transmitting || m_tuning || decodeUiBurstActive();
}

void DecodiumBridge::scheduleDeferredWorldMapFeedFlush(int delayMs)
{
    if (m_worldMapFeedFlushScheduled
        || (!m_worldMapFullReplayDeferred && m_deferredWorldMapFeedQueue.isEmpty())) {
        return;
    }
    if (!worldMapConsumerReady()) {
        return;
    }
    m_worldMapFeedFlushScheduled = true;
    QTimer::singleShot(qMax(0, delayMs), this, [this]() {
        m_worldMapFeedFlushScheduled = false;
        flushDeferredWorldMapFeed();
    });
}

void DecodiumBridge::flushDeferredWorldMapFeed()
{
    MainThreadTraceScope trace(QStringLiteral("flush_deferred_world_map_feed"),
                               QStringLiteral("queued=%1 full_replay=%2 tx=%3 tune=%4")
                                   .arg(m_deferredWorldMapFeedQueue.size())
                                   .arg(m_worldMapFullReplayDeferred ? 1 : 0)
                                   .arg(m_transmitting ? 1 : 0)
                                   .arg(m_tuning ? 1 : 0),
                               25);
    if (!m_worldMapFullReplayDeferred && m_deferredWorldMapFeedQueue.isEmpty()) {
        return;
    }
    if (!worldMapConsumerReady()) {
        // Keep the queue intact. A later QML loader completion explicitly
        // calls setWorldMapConsumerReady(), which schedules the replay.
        m_worldMapFullReplayDeferred = true;
        return;
    }
    if (visualFeedsDeferredForTx()) {
        scheduleDeferredWorldMapFeedFlush();
        return;
    }

    if (m_worldMapFullReplayDeferred) {
        m_worldMapFullReplayDeferred = false;
        m_deferredWorldMapFeedQueue.clear();
        m_deferredWorldMapFeedKeys.clear();
        if (m_worldMapDeferredLogActive) {
            decodiumBridgeAudioLevelLog(QStringLiteral("LiveMap: running deferred full replay after critical radio/UI work"));
            m_worldMapDeferredLogActive = false;
        }
        replayWorldMapFeed();
        return;
    }

    // Map projection, label layout and path signals are non-essential during
    // decode delivery. Two contacts per turn keep this queue visibly current
    // without creating another 100-250 ms main-thread burst.
    static constexpr int kWorldMapFlushChunk = 2;
    int processed = 0;
    while (!m_deferredWorldMapFeedQueue.isEmpty() && processed < kWorldMapFlushChunk) {
        QVariantMap queued = m_deferredWorldMapFeedQueue.takeFirst();
        bool const skipCleared = queued.take(QStringLiteral("__decodiumSkipClearedFeedEntry")).toBool();
        QString const key = queued.take(QStringLiteral("__decodiumDeferredKey")).toString();
        if (!key.isEmpty()) {
            m_deferredWorldMapFeedKeys.remove(key);
        }
        replayWorldMapEntry(queued, skipCleared);
        ++processed;
    }

    if (!m_deferredWorldMapFeedQueue.isEmpty()) {
        scheduleDeferredWorldMapFeedFlush(75);
    } else if (m_worldMapDeferredLogActive) {
        decodiumBridgeAudioLevelLog(QStringLiteral("LiveMap: flushed deferred decode feed after critical radio/UI work"));
        m_worldMapDeferredLogActive = false;
    }
    if (m_deferredWorldMapFeedQueue.isEmpty()) {
        emitCurrentWorldMapQsoPath();
    }
}

void DecodiumBridge::scheduleDxClusterSpotsChanged(int delayMs)
{
    m_dxClusterSpotsChangedPending = true;
    if (visualFeedsDeferredForTx()) {
        m_dxClusterSpotsDeferredDuringTx = true;
        delayMs = qMax(delayMs, 1000);
    }

    if (!m_dxClusterSpotsNotifyTimer) {
        flushDxClusterSpotsChanged();
        return;
    }
    if (!m_dxClusterSpotsNotifyTimer->isActive()) {
        m_dxClusterSpotsNotifyTimer->start(qMax(0, delayMs));
    }
}

void DecodiumBridge::flushDxClusterSpotsChanged()
{
    if (!m_dxClusterSpotsChangedPending) {
        return;
    }
    if (visualFeedsDeferredForTx()) {
        m_dxClusterSpotsDeferredDuringTx = true;
        if (m_dxClusterSpotsNotifyTimer && !m_dxClusterSpotsNotifyTimer->isActive()) {
            m_dxClusterSpotsNotifyTimer->start(1000);
        }
        return;
    }

    bool const wasDeferred = m_dxClusterSpotsDeferredDuringTx;
    m_dxClusterSpotsChangedPending = false;
    m_dxClusterSpotsDeferredDuringTx = false;
    if (wasDeferred) {
        decodiumBridgeAudioLevelLog(QStringLiteral("DX Cluster: flushed deferred spot update after TX/tune"));
    }
    emit dxClusterSpotsChanged();
}

