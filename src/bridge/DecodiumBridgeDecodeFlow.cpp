#include "DecodiumBridge.h"
#include "DecodiumBridgeTraceScope.h"
#include "DecodeListModel.h"

#include <QSettings>
#include <QTimer>
#include <utility>

QString decodiumBridgeMirrorEntryKey(const QVariantMap&);
QString decodiumBridgeDecodeRxClearEntryKey(const QVariantMap&);
void decodiumBridgeNormalizeDecodeEntries(QVariantList&, int, QString const&);
void decodiumBridgeCoalesceRxPaneTxRows(QVariantList&, QString const&);
void decodiumBridgeDecodeFlowLog(const QString&);

void DecodiumBridge::setSmoothDecodeFlow(bool v)
{
    if (m_smoothDecodeFlow == v) return;
    m_smoothDecodeFlow = v;
    QSettings().setValue(QStringLiteral("Decoder/SmoothDecodeFlow"), v);
    emit smoothDecodeFlowChanged();
    decodiumBridgeDecodeFlowLog(QStringLiteral("[SmoothFlow] %1").arg(v ? "ON" : "OFF"));
    // Se disattivato a meta' rilascio, flush immediato della queue residua.
    if (!v && !m_pendingDecodeReleaseQueue.isEmpty()) {
        for (auto const& entry : std::as_const(m_pendingDecodeReleaseQueue)) {
            m_decodeList.append(QVariant(entry));
            rememberDecodeDedupEntry(entry);
            // 1.0.239 (Phase 5.2 fix): hook persistenza + counter anche
            // sul path smooth-flow (vedi appendDecodeMapToList).
            noteDecodeCommitted();
            persistDecodeHistoryEntry(entry);
        }
        m_pendingDecodeReleaseQueue.clear();
        if (m_decodeReleaseTimer) m_decodeReleaseTimer->stop();
        trimDecodeListsIfNeeded();  // bounded in-memory decode history
        emitDecodeListChangedThrottled();
    }
}

void DecodiumBridge::trimDecodeListsIfNeeded()
{
    if (m_decodeList.size() >= kDecodeListCap) {
        int const totalBefore = m_decodeList.size();
        int const preserveFrom = qMax(0, totalBefore - kDecodeListRetainRows);
        QVariantList preservedRows;
        preservedRows.reserve(totalBefore - preserveFrom);

        for (int i = 0; i < totalBefore; ++i) {
            QVariant const value = m_decodeList.at(i);
            QVariantMap const entry = value.toMap();
            if (i < preserveFrom) {
                QString const key = decodiumBridgeMirrorEntryKey(entry);
                if (!key.isEmpty()) {
                    m_legacyPrunedBandMirrorKeys.insert(key);
                }
                continue;
            }
            preservedRows.append(value);
        }

        int const pruned = preserveFrom;
        decodiumBridgeNormalizeDecodeEntries(preservedRows, 1500, m_mode);
        MainThreadTraceScope trace(QStringLiteral("trim_decode_list_cap_full_spectrum"),
                                   QStringLiteral("rows=%1 cap=%2 pruned=%3 retained=%4")
                                       .arg(totalBefore)
                                       .arg(kDecodeListCap)
                                       .arg(pruned)
                                       .arg(preservedRows.size()),
                                   20);
        m_decodeList = preservedRows;
        decodiumBridgeDecodeFlowLog(QStringLiteral("decode auto-prune: Full Spectrum reached %1 rows, pruned oldest=%2, retained=%3")
                      .arg(totalBefore)
                      .arg(pruned)
                      .arg(m_decodeList.size()));
        emitDecodeListChangedThrottled();
    }

    if (m_rxDecodeList.size() < kRxDecodeListCap) {
        return;
    }

    int rxDecodeRows = 0;
    for (QVariant const& value : std::as_const(m_rxDecodeList)) {
        if (!value.toMap().value(QStringLiteral("isTx")).toBool()) {
            ++rxDecodeRows;
        }
    }

    if (rxDecodeRows >= kRxDecodeListCap) {
        QVariantList preservedRows;
        int preservedRxRows = 0;
        int preservedTxRows = 0;
        int prunedRxRows = 0;
        for (auto it = m_rxDecodeList.crbegin(); it != m_rxDecodeList.crend(); ++it) {
            QVariantMap const entry = it->toMap();
            bool const isTx = entry.value(QStringLiteral("isTx")).toBool();
            if (isTx) {
                if (preservedTxRows < kRxDecodeListRetainTxRows) {
                    preservedRows.prepend(entry);
                    ++preservedTxRows;
                }
                continue;
            }

            if (preservedRxRows < kRxDecodeListRetainRxRows) {
                preservedRows.prepend(entry);
                ++preservedRxRows;
                continue;
            }

            QString const clearKey = decodiumBridgeDecodeRxClearEntryKey(entry);
            if (!clearKey.isEmpty()) {
                m_clearedRxDecodeKeys.insert(clearKey);
            }
            QString const mirrorKey = decodiumBridgeMirrorEntryKey(entry);
            if (!mirrorKey.isEmpty()) {
                m_legacyClearedRxMirrorKeys.insert(mirrorKey);
            }
            ++prunedRxRows;
        }

        int const totalBefore = m_rxDecodeList.size();
        MainThreadTraceScope trace(QStringLiteral("trim_decode_list_cap_signal_rx"),
                                   QStringLiteral("rx_decode_rows=%1 total_rows=%2 cap=%3 pruned_rx=%4 retained_rx=%5 retained_tx=%6")
                                       .arg(rxDecodeRows)
                                       .arg(totalBefore)
                                       .arg(kRxDecodeListCap)
                                       .arg(prunedRxRows)
                                       .arg(preservedRxRows)
                                       .arg(preservedTxRows),
                                   20);
        m_rxDecodeList = preservedRows;
        decodiumBridgeCoalesceRxPaneTxRows(m_rxDecodeList, m_mode);
        decodiumBridgeNormalizeDecodeEntries(m_rxDecodeList, 1500, m_mode);
        rebuildRxDecodeMirrorIndex();
        if (m_rxDecodeModel) {
            rebuildRxDecodeModel();
        }
        decodiumBridgeDecodeFlowLog(QStringLiteral("decode auto-prune: Signal RX reached %1 RX decode rows (%2 total rows), pruned RX=%3, retained RX=%4, retained TX=%5")
                      .arg(rxDecodeRows)
                      .arg(totalBefore)
                      .arg(prunedRxRows)
                      .arg(preservedRxRows)
                      .arg(preservedTxRows));
        emitRxDecodeListChangedThrottled(false);
    }
}

// 1.0.179 — Drain timer slot: estrae 1-2 entry dalla queue smooth e le
// appende al modello, poi emit throttled. Si auto-ferma quando la coda
// e' vuota.
void DecodiumBridge::drainDecodeReleaseQueue()
{
    if (m_pendingDecodeReleaseQueue.isEmpty()) {
        if (m_decodeReleaseTimer) m_decodeReleaseTimer->stop();
        return;
    }
    int const take = qMin(m_decodeReleaseChunkSize,
                          m_pendingDecodeReleaseQueue.size());
    for (int i = 0; i < take; ++i) {
        QVariantMap const entry = m_pendingDecodeReleaseQueue.first();
        m_decodeList.append(QVariant(entry));
        rememberDecodeDedupEntry(entry);
        m_pendingDecodeReleaseQueue.removeFirst();
        // 1.0.239 (Phase 5.2 fix): hook persistenza + counter sul path
        // smooth-flow drain chunked (vedi appendDecodeMapToList).
        noteDecodeCommitted();
        persistDecodeHistoryEntry(entry);
    }
    trimDecodeListsIfNeeded();  // bounded in-memory decode history
    emitDecodeListChangedThrottled();
    if (m_pendingDecodeReleaseQueue.isEmpty() && m_decodeReleaseTimer) {
        m_decodeReleaseTimer->stop();
    }
}

void DecodiumBridge::setDecodeShowPeriodSeparator(bool v)
{
    if (m_decodeShowPeriodSeparator == v) return;
    m_decodeShowPeriodSeparator = v;
    decodiumBridgeDecodeFlowLog(QStringLiteral("setDecodeShowPeriodSeparator: %1")
        .arg(v ? "ON" : "OFF"));
    saveSettingsAsync();
    invalidateLegacyDecodeModelDeltaFastPath();
    rebuildBandActivityModel();
    rebuildRxDecodeModel();
}

