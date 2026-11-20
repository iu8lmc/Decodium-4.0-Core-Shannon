#include "DecodiumBridge.h"
#include "DecodiumBridgeTraceScope.h"
#include "Sequencer/MessageTokenRules.hpp"
#include <QDateTime>
#include <utility>

using decodium::seq::normalizedBaseCall;
using decodium::seq::messageContainsCallToken;
QString decodiumBridgeCurrentTxVisualTimeToken(QString const&);
qint64 decodiumBridgeDecodeEntryTimestampMsecs(QString const&, qint64);

void decodiumBridgeNormalizeDecodeEntries(QVariantList&, int, QString const&);
void decodiumBridgeCoalesceRxPaneTxRows(QVariantList&, QString const&);

void decodiumBridgeDecodeFlowLog(const QString&);

QString decodiumBridgeDecodeRxClearEntryKey(const QVariantMap&);
QString decodiumBridgeMirrorEntryKey(const QVariantMap&);

void DecodiumBridge::appendRxDecodeEntry(const QVariantMap& entry)
{
#if DECODIUM_HAS_SSTV
    if (m_sstvWorkspaceActive) {
        return;
    }
#endif
    MainThreadTraceScope trace(QStringLiteral("append_rx_decode_entry"),
                               QStringLiteral("rx_rows=%1 msg=[%2]")
                                   .arg(m_rxDecodeList.size())
                                   .arg(entry.value(QStringLiteral("message")).toString().trimmed()),
                               30);
    bool const isTx = entry.value(QStringLiteral("isTx")).toBool();
    if ((usingLegacyBackendForTx() && !isTx) || !shouldMirrorToRxPane(entry)) {
        return;
    }

    QVariantMap rxEntry = entry;
    rxEntry[QStringLiteral("forceRxPane")] = true;

    QString const clearKey = decodiumBridgeDecodeRxClearEntryKey(rxEntry);
    if (!clearKey.isEmpty() && m_clearedRxDecodeKeys.contains(clearKey)) {
        return;
    }
    QString const key = decodiumBridgeMirrorEntryKey(rxEntry);

    if (m_rxDecodeMirrorKeys.isEmpty() && !m_rxDecodeList.isEmpty()) {
        rebuildRxDecodeMirrorIndex();
    }
    if (!key.isEmpty() && m_rxDecodeMirrorKeys.contains(key)) {
        return;
    }
    if (key.isEmpty()) {
        // Malformed legacy rows occasionally have no stable mirror key. Keep
        // the historical fallback for those rows only; normal decode traffic
        // is an O(1) indexed lookup.
        for (const QVariant& existingValue : std::as_const(m_rxDecodeList)) {
            if (decodiumBridgeMirrorEntryKey(existingValue.toMap()).isEmpty()) {
                return;
            }
        }
    }

    if (!isTx) {
        bool snrOk = false;
        int const snrDb = rxEntry.value(QStringLiteral("snr")).toString().trimmed().toInt(&snrOk);
        emit rotorDecodeHeard(rxEntry.value(QStringLiteral("message")).toString(), snrDb, snrOk,
                              rxEntry.value(QStringLiteral("mode")).toString().isEmpty()
                                  ? m_mode : rxEntry.value(QStringLiteral("mode")).toString());
    }
    m_rxDecodeList.append(rxEntry);
    if (!key.isEmpty()) {
        m_rxDecodeMirrorKeys.insert(key);
    }
    trimDecodeListsIfNeeded();  // bounded in-memory decode history
    emitRxDecodeListChangedThrottled(true);
    trace.addDetail(QStringLiteral("new_rx_rows=%1 emit_pending=1").arg(m_rxDecodeList.size()));
}


void DecodiumBridge::appendTxDecodeEntry(const QString& message)
{
    QString const msg = message.trimmed();
    if (msg.isEmpty()) {
        return;
    }

    // The legacy backend can report a TX row without going through the bridge
    // audio start path. Keep the sequencer's "last TX" state aligned with the
    // visible TX timeline so a repeated final 73 from the same partner closes
    // the QSO instead of retrying RR73/73.
    if (m_currentTx >= 1 && m_currentTx <= 6) {
        m_lastNtx = m_currentTx;
    }
    m_lastTransmittedMessage = msg;
    m_lastTxActivityUtc = QDateTime::currentDateTimeUtc();
    QString const transmittedReport = decodium::seq::signalReportFromMessage(msg);
    if (!transmittedReport.isEmpty()) {
        m_lastReportTxMessage = msg;
    }
    if (!transmittedReport.isEmpty() && m_reportSent != transmittedReport) {
        m_reportSent = transmittedReport;
        emit reportSentChanged();
    }

    QVariantMap txEntry;
    QString const txTime = decodiumBridgeCurrentTxVisualTimeToken(m_mode);
    txEntry["time"]     = txTime;
    txEntry["timestamp"] = static_cast<qulonglong>(
        decodiumBridgeDecodeEntryTimestampMsecs(txTime, QDateTime::currentMSecsSinceEpoch()));
    txEntry["db"]       = "TX";
    txEntry["dt"]       = "0.0";
    txEntry["freq"]     = QString::number(m_txFrequency);
    txEntry["message"]  = msg;
    txEntry["aptype"]   = "";
    txEntry["quality"]  = "";
    txEntry["isTx"]     = true;
    txEntry["mode"]     = m_mode;
    txEntry["localTxMirror"] = true;
    bool const isCQ = msg.startsWith("CQ ", Qt::CaseInsensitive)
                   || msg.compare(QStringLiteral("CQ"), Qt::CaseInsensitive) == 0
                   || msg.startsWith("QRZ ", Qt::CaseInsensitive);
    txEntry["isCQ"] = isCQ;
    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);
    txEntry["isMyCall"] = messageContainsCallToken(msg, myCallUpper, myBaseUpper);
    txEntry["fromCall"] = extractDecodedCallsign(msg, isCQ);
    enrichDecodeEntry(txEntry);

    // TX rows are a Signal RX timeline aid. Do not inject them into the
    // band activity model: Full Spectrum should show decoded over-the-air
    // activity only, never our own local transmit echo.
    appendRxDecodeEntry(txEntry);
}


void DecodiumBridge::rebuildRxDecodeList()
{
    if (usingLegacyBackendForTx()) {
        return;
    }

    QVariantList rebuilt;
    rebuilt.reserve(m_decodeList.size() + m_rxDecodeList.size());

    QSet<QString> seen;
    auto appendRelevantEntry = [this, &rebuilt, &seen](QVariant const& value, bool alreadyAcceptedForRx) {
        QVariantMap entry = value.toMap();
        if (!alreadyAcceptedForRx && !shouldMirrorToRxPane(entry)) {
            return;
        }
        entry[QStringLiteral("forceRxPane")] = true;

        QString const clearKey = decodiumBridgeDecodeRxClearEntryKey(entry);
        if (!clearKey.isEmpty() && m_clearedRxDecodeKeys.contains(clearKey)) {
            return;
        }
        QString const key = decodiumBridgeMirrorEntryKey(entry);
        if (seen.contains(key)) {
            return;
        }
        seen.insert(key);
        rebuilt.append(entry);
    };

    for (const QVariant& value : std::as_const(m_rxDecodeList)) {
        appendRelevantEntry(value, true);
    }
    for (const QVariant& value : std::as_const(m_decodeList)) {
        appendRelevantEntry(value, false);
    }

    decodiumBridgeCoalesceRxPaneTxRows(rebuilt, m_mode);
    decodiumBridgeNormalizeDecodeEntries(rebuilt, 1500, m_mode);

    if (m_rxDecodeList != rebuilt) {
        m_rxDecodeList = rebuilt;
        rebuildRxDecodeMirrorIndex();
        trimDecodeListsIfNeeded();
        emitRxDecodeListChangedThrottled(false);
    }
}


bool DecodiumBridge::messageMentionsLocalCall(const QString& message, QString* matchedCall) const
{
    QStringList const localCalls = localDecodeCallCandidates();
    for (QString const& call : localCalls) {
        QString const base = normalizedBaseCall(call);
        if (messageContainsCallToken(message, call, base)) {
            if (matchedCall) {
                *matchedCall = call;
            }
            return true;
        }
    }
    if (matchedCall) {
        matchedCall->clear();
    }
    return false;
}

bool DecodiumBridge::shouldMirrorToRxPane(const QVariantMap& entry,
                                          bool ghostAlreadyAccepted) const
{
    if (entry.value("isTx").toBool()) {
        return true;
    }
    if (!ghostAlreadyAccepted
        && m_hideGhostDecodes
        && looksLikeGhostDecode(entry)) {
        decodiumBridgeDecodeFlowLog(QStringLiteral("[GhostFilter] Signal RX reject ghost decode msg=%1")
                      .arg(entry.value("message").toString().trimmed()));
        return false;
    }

    QString const message = entry.value("message").toString().trimmed();
    QString matchedLocalCall;
    bool const directedToMe = entry.value("isMyCall").toBool()
        || messageMentionsLocalCall(message, &matchedLocalCall);

    if (directedToMe) {
        if (shouldSuppressDirectedGhostDecode(entry, QStringLiteral("signal-rx"))) {
            if (qEnvironmentVariableIsSet("DECODIUM_SIGRX_DEBUG")) {
                decodiumBridgeDecodeFlowLog(QStringLiteral("[SIGRX] reject directed ghost msg='%1' matched=%2 entryMyCall=%3 calls=%4")
                              .arg(message,
                                   matchedLocalCall.isEmpty() ? QStringLiteral("<entry>") : matchedLocalCall,
                                   entry.value(QStringLiteral("isMyCall")).toBool() ? QStringLiteral("1") : QStringLiteral("0"),
                                   localDecodeCallCandidates().join(QLatin1Char(','))));
            }
            return false;
        }
        if (!entry.value(QStringLiteral("isMyCall")).toBool()
            && qEnvironmentVariableIsSet("DECODIUM_SIGRX_DEBUG")) {
            decodiumBridgeDecodeFlowLog(QStringLiteral("[SIGRX] accept directed fallback msg='%1' matched=%2 calls=%3")
                          .arg(message,
                               matchedLocalCall.isEmpty() ? QStringLiteral("<unknown>") : matchedLocalCall,
                               localDecodeCallCandidates().join(QLatin1Char(','))));
        }
        return true;
    }

    if (entry.value("forceRxPane").toBool()) {
        return true;
    }

    QString activePartner = m_dxCall.trimmed();
    if (activePartner.isEmpty()) {
        activePartner = m_autoCqLockedCall.trimmed();
    }
    if (activePartner.isEmpty()) {
        activePartner = inferredPartnerForAutolog().trimmed();
    }
    QString const activePartnerBase = normalizedBaseCall(activePartner);
    QString const fromCallBase = normalizedBaseCall(entry.value("fromCall").toString());
    if (!activePartnerBase.isEmpty()) {
        bool const mentionsActivePartner =
            messageContainsCallToken(message, activePartner, activePartnerBase);
        bool const fromActivePartner = fromCallBase == activePartnerBase;
        if (mentionsActivePartner || fromActivePartner) {
            return true;
        }
    }

    // 1.0.267 (fork-only iu8lmc) — RIMOSSO il fallback "audioFreqHz +/- 200Hz".
    // Allineato a entryBelongsToCurrentQso (1.0.266): il pannello Signal RX mostra
    // solo entries collegate a mycall (TX nostre, chiamate verso di noi, repliche
    // QSO con partner attivo). Il traffico della porzione di banda selezionata nel
    // waterfall NON viene piu' mirrorato qui.
    return false;
}

