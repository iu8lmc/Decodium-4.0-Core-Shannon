#include "DecodiumBridge.h"
#include "Radio.hpp"
#include "Sequencer/MessageTokenRules.hpp"
#include <QDateTime>

using decodium::seq::normalizedBaseCall;
void decodiumBridgeCallerQueueLog(const QString& message);

void DecodiumBridge::processNextInQueue()
{
    if (m_manualTxHold) {
        return;
    }

    removeActiveMamCallerFromQueue(QStringLiteral("process-next"));
    rankCallerQueue(QStringLiteral("process-next"));

    while (!m_callerQueue.isEmpty()) {
        QString entry = m_callerQueue.takeFirst();
        emit callerQueueChanged();

        QStringList const parts = entry.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            continue;
        }

        QString const nextCall = Radio::base_callsign(parts.first()).trimmed().toUpper();
        if (nextCall.isEmpty()) {
            continue;
        }
        if (isRecentAutoCqDuplicate(nextCall, m_frequency, m_mode)) {
            decodiumBridgeCallerQueueLog("processNextInQueue: skip recent " + nextCall);
            continue;
        }

        int const rxFreq = parts.size() >= 2 ? parts.at(1).toInt() : m_rxFrequency;
        int const snr = parts.size() >= 3 ? qBound(-50, parts.at(2).toInt(), 49)
                                          : (m_reportSent.isEmpty() ? -10 : m_reportSent.toInt());

        decodiumBridgeCallerQueueLog("processNextInQueue: prossimo caller=" + nextCall);

        clearPendingAutoLogSnapshot();
        clearAutoCqPartnerLock();
        m_nTx73 = 0;
        m_autoSeqRogerReportBase.clear();
        m_txRetryCount = 0;
        m_lastNtx = -1;
        m_lastCqPidx = -1;
        m_txWatchdogTicks = 0;
        m_autoCQPeriodsMissed = 0;
        m_logAfterOwn73 = false;
        m_ft2DeferredLogPending = false;
        m_quickPeerSignaled = false;
        m_qsoLogged = false;
        m_asyncLastTxEndMs = 0;
        clearPendingAutoSeqTx(QStringLiteral("process-next-in-queue"));
        m_lastTransmittedMessage.clear();
        m_qsoStartedOn = QDateTime::currentDateTimeUtc();
        m_qsoFirstReplyOn = QDateTime {};
        setReportSent(QString::number(snr));
        setReportReceivedPlaceholder(QStringLiteral("-10"));

        setDxCall(nextCall);
        setDxGrid(QString());
        if (rxFreq > 0) {
            setRxFrequency(rxFreq);
        }
        genStdMsgs(nextCall, m_grid.left(4));

        // Il caller è già in coda: andiamo direttamente a TX2 (report)
        if (!advanceQsoState(2)) {
            return;
        }
        updateAutoCqPartnerLock();
        setTxEnabled(true);
        return;
    }

    if (autoCqCanRestartTx6()) {
        if (!advanceQsoState(6)) {
            return;
        }
        setTxEnabled(true);
        decodiumBridgeCallerQueueLog("processNextInQueue: coda vuota → torno a CQ");
    }
}

void DecodiumBridge::enqueueStation(const QString& call)
{
    m_callerQueuePinned.insert(normalizedBaseCall(call));   // accodata a mano: il punteggio non la supera
    enqueueCallerInternal(call);
    rankCallerQueue(QStringLiteral("manual-enqueue"));
}

void DecodiumBridge::dequeueStation(const QString& call)
{
    removeCallerFromQueue(call);
}

void DecodiumBridge::moveCallerQueueItem(int fromIndex, int toIndex)
{
    int const count = m_callerQueue.size();
    if (count <= 1) {
        return;
    }

    if (fromIndex < 0 || fromIndex >= count) {
        decodiumBridgeCallerQueueLog(QStringLiteral("callerQueue: move ignored, invalid source %1 size=%2")
                      .arg(fromIndex)
                      .arg(count));
        return;
    }

    int const boundedTarget = qBound(0, toIndex, count - 1);
    if (fromIndex == boundedTarget) {
        return;
    }

    QString const entry = m_callerQueue.takeAt(fromIndex);
    m_callerQueue.insert(boundedTarget, entry);
    m_callerQueueManualOrder = true;   // ordine scelto a mano: il punteggio non lo riordina piu'
    decodiumBridgeCallerQueueLog(QStringLiteral("callerQueue: manual move %1 -> %2 entry=%3")
                  .arg(fromIndex + 1)
                  .arg(boundedTarget + 1)
                  .arg(entry));
    emit callerQueueChanged();
}

void DecodiumBridge::clearCallerQueue()
{
    if (m_callerQueue.isEmpty()) return;
    m_callerQueue.clear();
    m_callerQueueManualOrder = false;
    m_callerQueueArrivalMs.clear();
    m_callerQueuePinned.clear();
    emit callerQueueChanged();
}

