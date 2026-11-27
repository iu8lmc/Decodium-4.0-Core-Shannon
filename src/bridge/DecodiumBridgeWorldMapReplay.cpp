#include "DecodiumBridge.h"
#include "DecodiumBridgeTraceScope.h"
#include <QTimer>
#include <utility>

void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::replayWorldMapFeed()
{
    MainThreadTraceScope trace(QStringLiteral("replay_world_map_feed"),
                               QStringLiteral("decode_rows=%1 rx_rows=%2 tx=%3 tune=%4")
                                   .arg(m_decodeList.size())
                                   .arg(m_rxDecodeList.size())
                                   .arg(m_transmitting ? 1 : 0)
                                   .arg(m_tuning ? 1 : 0));

    // Invalidate a previously queued replay before evaluating the current
    // state.  A full replay is a visual operation and must never monopolize
    // the GUI event loop while decode/audio callbacks are arriving.
    const quint64 generation = ++m_worldMapReplayGeneration;
    m_worldMapReplayActive = false;
    m_worldMapReplayEntries.clear();
    m_worldMapReplayIndex = 0;

    if (!worldMapFeedEnabled()) {
        return;
    }
    if (!worldMapConsumerReady()) {
        m_worldMapFullReplayDeferred = true;
        return;
    }
    if (visualFeedsDeferredForTx()) {
        m_worldMapFullReplayDeferred = true;
        if (!m_worldMapDeferredLogActive) {
            m_worldMapDeferredLogActive = true;
            decodiumBridgeAudioLevelLog(QStringLiteral("LiveMap: deferring full replay while TX/tune is active"));
        }
        scheduleDeferredWorldMapFeedFlush();
        return;
    }

    emit worldMapResetRequested();

    QSet<QString> seen;
    QVariantList replayEntries;
    // A map can be reopened minutes after the associated decode. Keep a
    // bounded history for the consumer-ready replay only; live incremental
    // delivery remains limited to 120 seconds.
    auto collectReplayEntries = [this, &seen, &replayEntries](QVariantList const& entries) {
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            QString const key = worldMapFeedEntryKey(entry);
            if (!key.isEmpty() && seen.contains(key)) {
                continue;
            }
            if (!key.isEmpty()) {
                seen.insert(key);
            }
            replayEntries.append(entry);
        }
    };

    collectReplayEntries(m_decodeList);
    collectReplayEntries(m_rxDecodeList);

    m_worldMapReplayEntries = std::move(replayEntries);
    m_worldMapReplayActive = true;
    QTimer::singleShot(0, this, [this, generation]() {
        flushWorldMapReplayChunk(generation);
    });
}

void DecodiumBridge::flushWorldMapReplayChunk(quint64 generation)
{
    if (!m_worldMapReplayActive || generation != m_worldMapReplayGeneration) {
        return;
    }

    if (!worldMapFeedEnabled() || !worldMapConsumerReady()) {
        m_worldMapReplayActive = false;
        m_worldMapReplayEntries.clear();
        m_worldMapReplayIndex = 0;
        return;
    }

    if (visualFeedsDeferredForTx()) {
        m_worldMapReplayActive = false;
        m_worldMapReplayEntries.clear();
        m_worldMapReplayIndex = 0;
        m_worldMapFullReplayDeferred = true;
        scheduleDeferredWorldMapFeedFlush(600);
        return;
    }

    static constexpr int kWorldMapReplayLifetimeSeconds = 15 * 60;
    // One entry per event-loop turn keeps parsing, callsign lookup and QML
    // scene-graph updates interruptible.  This is intentionally smaller than
    // the deferred live-feed chunk because a full replay can contain hundreds
    // of historical entries.
    static constexpr int kReplayChunkSize = 1;
    int processed = 0;
    while (m_worldMapReplayIndex < m_worldMapReplayEntries.size()
           && processed < kReplayChunkSize) {
        QVariantMap const entry =
            m_worldMapReplayEntries.at(m_worldMapReplayIndex++).toMap();
        replayWorldMapEntry(entry, true, kWorldMapReplayLifetimeSeconds);
        ++processed;
    }

    if (m_worldMapReplayIndex < m_worldMapReplayEntries.size()) {
        QTimer::singleShot(0, this, [this, generation]() {
            flushWorldMapReplayChunk(generation);
        });
        return;
    }

    m_worldMapReplayActive = false;
    m_worldMapReplayEntries.clear();
    m_worldMapReplayIndex = 0;
    emitCurrentWorldMapQsoPath();
}

