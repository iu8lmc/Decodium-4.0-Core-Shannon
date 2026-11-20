#include "DecodiumBridge.h"
#include <QDateTime>

void DecodiumBridge::resetQsoStateForNextSequence(bool preserveAutoTx)
{
    m_courtesySignoffBase.clear();
    invalidateDeferredTxRequests(QStringLiteral("QSO complete; next sequence"));
    noteAutoCallQsoCompleted();
    m_qsoLogged = false;  // reset per il prossimo QSO
    m_qsoFirstReplyOn = QDateTime {};
    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    m_pendingAutoSeqTxAfterActiveTx = 0;
    // 1.0.225 - reset anche le stringhe pending (BUG #9 agente-tx). Pre-1.0.225 solo
    // l'integer era resettato; le stringhe associate rimanevano dal QSO precedente ->
    // completeTxPlayback leggeva STALE m_pendingAutoSeqMessage/PartnerBase/Mode ->
    // falso positivo "partner does not mention" -> deferred TX legittimo droppato.
    m_pendingAutoSeqPartnerBase.clear();
    m_pendingAutoSeqMessage.clear();
    m_pendingAutoSeqMode.clear();
    m_quickPeerSignaled = false;
    if (!preserveAutoTx) {
        setTxEnabled(false);
    }
    if (m_qsoProgress != 6) { m_qsoProgress = 6; emit qsoProgressChanged(); } // IDLE_QSO
    m_nTx73 = 0;
    m_autoSeqRogerReportBase.clear();
    m_txRetryCount = 0;
    m_lastNtx = -1;
    m_lastCqPidx = -1;
    // 1.0.446 - P2-9: azzera l'SNR partner a fine QSO (sentinel 127 = sconosciuto). Senza
    // questo il cap signoff adattivo del QSO SUCCESSIVO usava l'SNR del partner precedente
    // finche' il nuovo partner non veniva decodificato.
    m_currentPartnerSnrDb = 127;
    clearAutoCqPartnerLock();
    setDxCall(QString());
    setDxGrid(QString());
}

