#include "DecodiumBridge.h"
#include "MapIntelligenceService.h"
#include "DxccLookup.h"

void decodiumBridgeAudioLevelLog(const QString&);
bool decodiumBridgeUnresolvedGridOnlyDecode(const QString&, QString*);
bool decodiumBridgeFst4wUnresolvedHashDecode(QString const&, QString const&);

void DecodiumBridge::queueWorldMapEntryForReplay(const QVariantMap& entry,
                                                 bool skipClearedFeedEntry,
                                                 int delayMs,
                                                 bool logTxDeferral)
{
    if (m_mapIntelligenceService) {
        QString band;
        if (m_bandManager) {
            band = m_bandManager->currentBandLambda().trimmed();
        }
        QVariantMap intelligenceEntry = entry;
        QString mapCall = intelligenceEntry.value(QStringLiteral("fromCall"))
                              .toString().trimmed().toUpper();
        if (mapCall.isEmpty()) {
            mapCall = extractDecodedCallsign(
                intelligenceEntry.value(QStringLiteral("message")).toString(), true)
                          .trimmed().toUpper();
        }
        if (!mapCall.isEmpty() && m_dxccLookup && m_dxccLookup->isLoaded()) {
            DxccEntity const entity = m_dxccLookup->lookup(mapCall);
            if (entity.isValid()) {
                intelligenceEntry.insert(QStringLiteral("dxcc"), entity.name);
                intelligenceEntry.insert(QStringLiteral("continent"), entity.continent);
                intelligenceEntry.insert(QStringLiteral("cqZone"), entity.cqZone);
                intelligenceEntry.insert(QStringLiteral("ituZone"), entity.ituZone);
            }
        }
        m_mapIntelligenceService->ingestDecodeEntry(
            intelligenceEntry, static_cast<qint64>(m_frequency), band);
    }
    if (!worldMapFeedEnabled()) {
        return;
    }
    QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
    QString const mode = entry.value(QStringLiteral("mode"), m_mode).toString().trimmed();
    if (entry.value(QStringLiteral("partialDecode")).toBool()
        || entry.value(QStringLiteral("unresolvedHash")).toBool()
        || entry.value(QStringLiteral("hasUnresolvedGridOnly")).toBool()
        || decodiumBridgeUnresolvedGridOnlyDecode(message, nullptr)
        || decodiumBridgeFst4wUnresolvedHashDecode(mode, message)) {
        return;
    }

    QString const key = worldMapFeedEntryKey(entry);
    if (!key.isEmpty() && m_deferredWorldMapFeedKeys.contains(key)) {
        return;
    }

    QVariantMap queued = entry;
    queued.insert(QStringLiteral("__decodiumSkipClearedFeedEntry"), skipClearedFeedEntry);
    queued.insert(QStringLiteral("__decodiumDeferredKey"), key);

    static constexpr int kMaxDeferredWorldMapEntries = 300;
    while (m_deferredWorldMapFeedQueue.size() >= kMaxDeferredWorldMapEntries) {
        QVariantMap dropped = m_deferredWorldMapFeedQueue.takeFirst();
        QString const droppedKey = dropped.value(QStringLiteral("__decodiumDeferredKey")).toString();
        if (!droppedKey.isEmpty()) {
            m_deferredWorldMapFeedKeys.remove(droppedKey);
        }
    }

    m_deferredWorldMapFeedQueue.append(queued);
    if (!key.isEmpty()) {
        m_deferredWorldMapFeedKeys.insert(key);
    }
    if (logTxDeferral && !m_worldMapDeferredLogActive) {
        m_worldMapDeferredLogActive = true;
        decodiumBridgeAudioLevelLog(QStringLiteral("LiveMap: deferring decode feed during TX/tune/decode burst"));
    }
    scheduleDeferredWorldMapFeedFlush(qMax(0, delayMs));
}

void DecodiumBridge::deferWorldMapEntryForTx(const QVariantMap& entry, bool skipClearedFeedEntry)
{
    queueWorldMapEntryForReplay(entry, skipClearedFeedEntry, 600, true);
}

