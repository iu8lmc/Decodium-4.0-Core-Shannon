#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
void decodiumBridgeDecodeFlowLog(const QString&);
int decodiumBridgeSpecialOpHound();
bool decodiumBridgeTruthyEnvironmentFlag(const char*);
bool decodiumBridgeCatSuppressedForTx();
#define bridgeLog decodiumBridgeDecodeFlowLog
#define kSpecialOpHound decodiumBridgeSpecialOpHound()
#define truthyEnvironmentFlag decodiumBridgeTruthyEnvironmentFlag
#define catSuppressedByEnvironment decodiumBridgeCatSuppressedForTx

void DecodiumBridge::reportJttyPanelTransmitResult(bool accepted, const QString& text,
                                                    const QString& reason)
{
    const QString message = text.simplified();
    if (accepted) {
        bridgeLog(QStringLiteral("JTTY: standard TX panel request accepted: msg=[%1]").arg(message));
        emit statusMessage(tr("JTTY TX: %1").arg(message));
        return;
    }

    const QString detail = reason.trimmed().isEmpty()
        ? tr("The JTTY transmitter could not be started.")
        : reason.trimmed();
    bridgeLog(QStringLiteral("JTTY: standard TX panel request rejected: %1 msg=[%2]")
                  .arg(detail, message));
    emit errorMessage(tr("JTTY TX: %1").arg(detail));
}

void DecodiumBridge::sendTx(int n)
{
    // JTTY messages are free-text frames produced by JttyController, not
    // WSJT-style slot waveforms. Keep this branch before every FT/JT
    // validation, legacy-backend decision and deferred-slot scheduler.
    // Calling sendTx is an explicit user/remote command; startTx remains
    // blocked above so an old automatic timer can never key JTTY.
    if (m_mode.compare(QStringLiteral("JTTY"), Qt::CaseInsensitive) == 0) {
        if (!selectCurrentTxIfAllowed(n, QStringLiteral("sendTx JTTY"))) {
            return;
        }
        const QString message = txMessage(n).trimmed();
        if (message.isEmpty()) {
            const QString detail = tr("No text is available in this TX slot. Use the JTTY window to enter a message.");
            bridgeLog(QStringLiteral("sendTx JTTY: empty TX%1 request rejected").arg(n));
            emit errorMessage(tr("JTTY TX: %1").arg(detail));
            return;
        }

        // A deliberate click is allowed to resume after HALT, but must not
        // leave an FT/JT manual authorisation that could survive a later mode
        // change. JttyController owns the actual PTT/audio admission check.
        clearManualTxHold(QStringLiteral("explicit JTTY sendTx"));
        m_explicitManualTxRequest = 0;
        bridgeLog(QStringLiteral("sendTx JTTY: forwarding explicit TX%1 to native controller msg=[%2]")
                      .arg(n)
                      .arg(message));
        emit jttyPanelTransmitRequested(message);
        return;
    }

    if (m_specialOperationActivity == kSpecialOpHound && n != 3) {
        n = 1;
    }
    if (!selectCurrentTxIfAllowed(n, QStringLiteral("sendTx"))) {
        return;
    }
    if (legacyTxBackendRequested() && !legacyBackendAvailable() && !ensureLegacyBackendAvailable()) {
        emit errorMessage(QStringLiteral("Backend legacy non disponibile per Fox/Hound"));
        return;
    }
    if (!prepareHoundTxSelectionForStart(QStringLiteral("sendTx"))) {
        return;
    }
    if (!ensureCurrentTxCanTransmit(QStringLiteral("sendTx after hound prepare"))) {
        return;
    }
    QString selectedMessage = buildCurrentTxMessage();
    if (!selectedMessage.trimmed().isEmpty()
        && !repairOrRejectStalePartnerTxMessage(selectedMessage, QStringLiteral("sendTx"))) {
        emit statusMessage(QStringLiteral("TX bloccato: messaggio non coerente con il QSO attivo"));
        return;
    }
    clearManualTxHold(QStringLiteral("explicit sendTx"));
    armExplicitManualTxRequest(n, QStringLiteral("TX panel"));
    if (usingLegacyBackendForTx() && !m_ft2LinkTxActive && !catSuppressedByEnvironment()) {
        m_explicitManualTxRequest = 0;
        bridgeLog(QStringLiteral("sendTx: delegating TX%1 to legacy backend").arg(n));
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->armCurrentTx();
        scheduleLegacyStateRefreshBurst();
        return;
    }
    if (usingLegacyBackendForTx() && !m_ft2LinkTxActive && catSuppressedByEnvironment()) {
        bridgeLog(QStringLiteral("sendTx: CAT disabled, using bridge audio-only TX path for TX%1").arg(n));
    }

    bool const forceImmediateLabTx = truthyEnvironmentFlag("DECODIUM_LAB_FORCE_TX_IMMEDIATE");
    if (!forceImmediateLabTx && shouldDeferManualSyncTxStart()) {
        m_deferredManualSyncTx = true;
        ensureSyncTxSchedulerActive(QStringLiteral("manual-sync-tx"));
        scheduleTxAudioPrecompute(25);
        scheduleSyncTxAtNextValidSlot(QStringLiteral("manual-sync-tx"), -1, 0);
        bridgeLog(QStringLiteral("manualSyncTx: queued TX%1 for next valid slot (mode=%2 txPeriod=%3 transmitting=%4)")
                      .arg(n)
                      .arg(m_mode)
                      .arg(m_txPeriod)
                      .arg(m_transmitting ? 1 : 0));
        emit statusMessage(QStringLiteral("TX%1 armato per il prossimo slot").arg(n));
        return;
    }
    if (forceImmediateLabTx) {
        bridgeLog(QStringLiteral("sendTx: lab force immediate TX timing bypass active"));
    }

    clearDeferredManualSyncTx(QStringLiteral("manual-immediate"));
    startTx();
}

