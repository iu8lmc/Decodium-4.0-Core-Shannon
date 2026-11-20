#include "DecodiumBridge.h"
#include "Radio.hpp"
#include "Sequencer/MessageTokenRules.hpp"
#include <QDateTime>

using decodium::seq::normalizedBaseCall;
using decodium::seq::messageContainsCallToken;
QString decodiumBridgeInferPartnerForAutolog(const QString&, const QString&, const QString&);
int decodiumBridgeLateAutoLogGraceSeconds();
void decodiumBridgeAudioLevelLog(const QString&);

QString DecodiumBridge::inferredPartnerForAutolog() const
{
    QString snapshotCall = m_dxCall.trimmed();
    if (snapshotCall.isEmpty()) {
        snapshotCall = m_autoCqLockedCall.trimmed();
    }
    // The final TX5 playback callback can run after AutoCQ has already
    // re-armed CQ and cleared the active DX.  A pending snapshot was captured
    // from the accepted RR73/73 and is therefore a stronger source than an
    // unrelated current CQ payload.
    if (snapshotCall.isEmpty() && m_pendingAutoLogValid) {
        snapshotCall = m_pendingAutoLogCall.trimmed();
    }
    if (!snapshotCall.isEmpty()) {
        return snapshotCall;
    }

    QString const myCall = m_callsign.trimmed().toUpper();
    QString const myBase = Radio::base_callsign(m_callsign).trimmed().toUpper();
    if (myCall.isEmpty() && myBase.isEmpty()) {
        return {};
    }

    auto const inferFromMessage = [&](QString const& message) {
        return decodiumBridgeInferPartnerForAutolog(message.trimmed(), myCall, myBase).trimmed();
    };

    snapshotCall = inferFromMessage(buildCurrentTxMessage());
    if (snapshotCall.isEmpty()) {
        snapshotCall = inferFromMessage(m_lastTransmittedMessage);
    }
    return snapshotCall;
}

void DecodiumBridge::capturePendingAutoLogSnapshot()
{
    QString const snapshotCall = inferredPartnerForAutolog();
    if (snapshotCall.isEmpty()) {
        return;
    }

    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    m_pendingAutoLogValid = true;
    m_pendingAutoLogCall = snapshotCall;
    m_pendingAutoLogGrid = !m_dxGrid.trimmed().isEmpty() ? m_dxGrid.trimmed() : m_autoCqLockedGrid.trimmed();
    if (m_pendingAutoLogGrid.isEmpty()) {
        // The grid the partner sent was cleared with the QSO state (M0PWL, 26/10):
        // the one remembered from its decodes is still known.
        m_pendingAutoLogGrid = lookupWorldMapGrid(snapshotCall);
    }
    m_pendingAutoLogRptSent = m_reportSent.trimmed();
    // RST_SENT is the report we actually put on the air for this partner, not the
    // last SNR measured on it (KV4T: sent +05 and -02, logged -01).
    if (!m_lastReportTxMessage.isEmpty()
        && messageContainsCallToken(m_lastReportTxMessage, snapshotCall, normalizedBaseCall(snapshotCall))) {
        QString const sentOnAir = decodium::seq::signalReportFromMessage(m_lastReportTxMessage).trimmed();
        if (!sentOnAir.isEmpty()) {
            m_pendingAutoLogRptSent = sentOnAir;
        }
    }
    if (m_pendingAutoLogRptSent.isEmpty()) {
        m_pendingAutoLogRptSent =
            decodium::seq::signalReportFromMessage(m_lastTransmittedMessage);
    }
    if (m_pendingAutoLogRptSent.isEmpty()) {
        m_pendingAutoLogRptSent =
            decodium::seq::signalReportFromMessage(buildCurrentTxMessage());
    }
    // RST_RCVD only if the partner really sent a report: the -10 shown before that
    // is a placeholder, not something received.
    m_pendingAutoLogRptRcvd = m_reportReceivedReal ? m_reportReceived.trimmed() : QString();
    QDateTime const effectiveLogOn = effectiveQsoLogTimeOnUtc();
    m_pendingAutoLogOn = effectiveLogOn.isValid() ? effectiveLogOn : nowUtc;
    m_pendingAutoLogOff = nowUtc < m_pendingAutoLogOn ? m_pendingAutoLogOn : nowUtc;
    m_pendingAutoLogDialFreq = m_frequency;
}

QDateTime DecodiumBridge::effectiveQsoLogTimeOnUtc() const
{
    return m_seqState.effectiveQsoLogTimeOnUtc();
}

void DecodiumBridge::clearPendingAutoLogSnapshot()
{
    m_pendingAutoLogValid = false;
    m_pendingAutoLogCall.clear();
    m_pendingAutoLogGrid.clear();
    m_pendingAutoLogRptSent.clear();
    m_pendingAutoLogRptRcvd.clear();
    m_pendingAutoLogOn = QDateTime {};
    m_pendingAutoLogOff = QDateTime {};
    m_pendingAutoLogDialFreq = 0.0;
}

void DecodiumBridge::armLateAutoLogSnapshot()
{
    capturePendingAutoLogSnapshot();
    if (!m_pendingAutoLogValid) {
        return;
    }

    m_lateAutoLogValid = true;
    m_lateAutoLogCall = m_pendingAutoLogCall;
    m_lateAutoLogGrid = m_pendingAutoLogGrid;
    m_lateAutoLogRptSent = m_pendingAutoLogRptSent;
    m_lateAutoLogRptRcvd = m_pendingAutoLogRptRcvd;
    m_lateAutoLogOn = m_pendingAutoLogOn;
    m_lateAutoLogOff = m_pendingAutoLogOff;
    m_lateAutoLogDialFreq = m_pendingAutoLogDialFreq;
    m_lateAutoLogExpires = QDateTime::currentDateTimeUtc().addSecs(decodiumBridgeLateAutoLogGraceSeconds());
    rememberRecentAutoCqAbandoned(m_lateAutoLogCall, m_lateAutoLogDialFreq, m_mode);
    decodiumBridgeAudioLevelLog(QStringLiteral("late-signoff-arm: %1 until %2")
                  .arg(Radio::base_callsign(m_lateAutoLogCall).trimmed().toUpper(),
                       m_lateAutoLogExpires.toString(Qt::ISODate)));
}

void DecodiumBridge::clearLateAutoLogSnapshot()
{
    m_lateAutoLogValid = false;
    m_lateAutoLogCall.clear();
    m_lateAutoLogGrid.clear();
    m_lateAutoLogRptSent.clear();
    m_lateAutoLogRptRcvd.clear();
    m_lateAutoLogOn = QDateTime {};
    m_lateAutoLogOff = QDateTime {};
    m_lateAutoLogDialFreq = 0.0;
    m_lateAutoLogExpires = QDateTime {};
}

