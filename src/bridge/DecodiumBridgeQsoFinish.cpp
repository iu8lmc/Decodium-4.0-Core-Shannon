#include "DecodiumBridge.h"
#include <QTimer>

void decodiumBridgeQsoFinishLog(const QString& message);

bool DecodiumBridge::finishAutoSequenceQsoFromCompletedSignoff(const QString& reason, bool logNow)
{
    if (releaseCourtesySignoffIfActive(reason)) {
        return true;
    }
    decodiumBridgeQsoFinishLog(reason);
    QString completedPartner = inferredPartnerForAutolog();
    if (logNow) {
        capturePendingAutoLogSnapshot();
        if (completedPartner.trimmed().isEmpty() && m_pendingAutoLogValid) {
            completedPartner = m_pendingAutoLogCall;
        }
        markWorldMapQsoClosed(completedPartner, reason);
        rememberCompletedAutoCqPartner(completedPartner, true, reason);
        if (!m_qsoLogged) {
            logQso();
        }
    } else {
        markWorldMapQsoClosed(completedPartner, reason);
        rememberCompletedAutoCqPartner(completedPartner, false, reason);
    }

    bool const continueQueuedCaller = (m_multiAnswerMode || m_autoCqRepeat) && !m_callerQueue.isEmpty();
    bool const continueAutoCq = autoCqCanRestartTx6();
    bool const preserveAutoTx = continueQueuedCaller || continueAutoCq;
    resetQsoStateForNextSequence(preserveAutoTx);
    if (applyDeferredSpecialOperationActivityIfReady()) {
        return true;
    }
    if (continueQueuedCaller) {
        processNextInQueue();
    } else if (continueAutoCq) {
        if (!advanceQsoState(6)) {
            return true;
        }
        if (!m_txEnabled) {
            setTxEnabled(true);
        }
        if (!m_periodicTxCheckScheduled) {
            m_periodicTxCheckScheduled = true;
            quint64 const requestGeneration = m_txRequestGeneration;
            QTimer::singleShot(0, this, [this, requestGeneration]() {
                m_periodicTxCheckScheduled = false;
                if (requestGeneration == m_txRequestGeneration
                    && !m_transmitting && !m_tuning) {
                    checkAndStartPeriodicTx();
                }
            });
        }
    }
    return true;
}

