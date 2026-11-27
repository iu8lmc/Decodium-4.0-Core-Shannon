#include "DecodiumBridge.h"
#include "Sequencer/QsoSequencerRules.hpp"
#include <QDateTime>
#include <QTimer>

void decodiumBridgeCourtesyLog(const QString&);
int decodiumBridgeRecentAutoCqPartnerWindowSeconds();

bool DecodiumBridge::maybeStartCourtesySignoff(const QString& partnerCall, const QString& partnerBase,
                                               const QString& msg)
{
    QString const mode = m_mode.trimmed().toUpper();
    if (!m_autoCqRepeat
        || (mode != QStringLiteral("FT2") && mode != QStringLiteral("FT4") && mode != QStringLiteral("FT8"))) {
        return false;
    }
    if (partnerBase.isEmpty() || !m_courtesySignoffBase.isEmpty()
        || m_transmitting || m_tuning || m_manualTxHold) {
        return false;
    }
    // Only when idle in CQ: never over a QSO in progress, a queued caller or a halted station.
    if (!m_dxCall.trimmed().isEmpty() || !m_autoCqLockedCall.trimmed().isEmpty()
        || m_currentTx != 6 || !m_callerQueue.isEmpty() || isTxDisabled(5)) {
        return false;
    }
    // Only for a QSO that was really completed or logged: an abandoned partner is not owed a 73.
    if (!isRecentAutoCqWorkedOrLoggedDuplicate(partnerBase, m_frequency, m_mode)) {
        return false;
    }
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    QList<qint64>& sent = m_courtesySignoffSentMs[partnerBase];
    while (!sent.isEmpty() && nowMs - sent.first() > qint64(decodiumBridgeRecentAutoCqPartnerWindowSeconds()) * 1000) {
        sent.removeFirst();
    }
    if (!decodium::seq::courtesySignoffDue(msg, int(sent.size()), 2)) {
        return false;
    }
    sent.append(nowMs);
    m_courtesySignoffBase = partnerBase;
    decodiumBridgeCourtesyLog(QStringLiteral("courtesy 73: %1 keeps calling after a completed QSO (msg=%2), sending one 73 (%3 of 2)")
                  .arg(partnerBase, msg.trimmed()).arg(sent.size()));
    setDxCall(partnerCall);
    genStdMsgs(partnerCall, QString());
    m_txRetryCount = 0;
    m_nTx73 = 0;
    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    m_qsoLogged = true;   // the QSO is already in the log: nothing in this exchange may log it again
    if (!advanceQsoState(5)) {
        m_courtesySignoffBase.clear();
        setDxCall(QString());
        setDxGrid(QString());
        return false;
    }
    if (!m_txEnabled) {
        setTxEnabled(true);
    }
    if (!m_periodicTxCheckScheduled) {
        m_periodicTxCheckScheduled = true;
        quint64 const requestGeneration = m_txRequestGeneration;
        QTimer::singleShot(0, this, [this, requestGeneration]() {
            m_periodicTxCheckScheduled = false;
            if (requestGeneration == m_txRequestGeneration && !m_transmitting && !m_tuning) {
                checkAndStartPeriodicTx();
            }
        });
    }
    return true;
}

bool DecodiumBridge::releaseCourtesySignoffIfActive(const QString& reason)
{
    if (m_courtesySignoffBase.isEmpty()) {
        return false;
    }
    QString const base = m_courtesySignoffBase;
    m_courtesySignoffBase.clear();
    decodiumBridgeCourtesyLog(QStringLiteral("courtesy 73: %1 finished (%2), released without logging").arg(base, reason));
    finishAutoSequenceQsoFromCompletedSignoff(
        QStringLiteral("courtesy 73 to %1 complete, QSO was already logged").arg(base), false);
    // finishing without a log records the partner as abandoned (90 s): put the worked entry back
    clearRecentAutoCqAbandoned(base, m_frequency, m_mode, QStringLiteral("courtesy 73"));
    rememberRecentAutoCqWorked(base, m_frequency, m_mode);
    return true;
}





