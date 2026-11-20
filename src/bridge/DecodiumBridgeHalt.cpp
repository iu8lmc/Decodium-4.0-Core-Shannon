#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "Modulator/Modulator.hpp"

void decodiumBridgeAudioLevelLog(const QString&);
#define bridgeLog decodiumBridgeAudioLevelLog

void DecodiumBridge::halt()
{
    haltWithReason(QStringLiteral("halt"));
}

void DecodiumBridge::haltWithReason(const QString& reason)
{
    QString const haltReason = reason.trimmed().isEmpty()
        ? QStringLiteral("halt")
        : reason.trimmed();

    invalidateDeferredTxRequests(QStringLiteral("HALT: %1").arg(haltReason));
    engageManualTxHold(haltReason, true);
    clearDeferredManualSyncTx(haltReason);
    clearPendingAutoSeqTx(haltReason);
    clearMamPendingTxPayload(QStringLiteral("halt:%1").arg(haltReason));
    // Halt is an explicit operator abort. Unlike a temporary TX disable, it
    // must discard every active MAM QSO on both native and legacy paths.
    const bool hadMamSlots = !m_mamSlots.isEmpty();
    m_mamSlots.clear();
    if (hadMamSlots) {
        emit mamActiveSlotsChanged();
    }

    // Direct CW, FT2-Link and telemetry audio can be owned by the bridge
    // while the legacy backend handles ordinary digital TX.
    bool const bridgeOwnedLegacyAudio = usingLegacyBackendForTx()
        && (m_cwTxActive || m_ft2LinkTxActive || m_telemetryTxActive);
    if (bridgeOwnedLegacyAudio) {
        stopTx();
    }

#if defined(Q_OS_MAC)
    cancelPendingLegacyBridgeAudioStart(haltReason);
    // Native macOS PCM TX owns the physical CAT PTT, while HALT previously
    // delegated only to the legacy backend.  That stops the UI/audio but can
    // leave the rig keyed because no CAT PTT-OFF is queued.  Use the abort
    // path first: it sends PTT-OFF immediately, then the existing backend
    // stop remains harmless and is covered by the per-transition guard.
    if (usingLegacyBackendForTx() && !bridgeOwnedLegacyAudio) {
        abortLegacyBridgeTxRequest(QStringLiteral("halt:%1").arg(haltReason));
    }
    bool const bridgeTuneMaybeActive =
        m_bridgeAudioTuneActive || (m_tuning && m_modulator && m_modulator->isActive());
    if (m_bridgeAudioLegacyTxActive) {
        bridgeLog(QStringLiteral("halt(%1): stopping bridge-owned legacy TX audio before legacy stop")
                      .arg(haltReason));
        stopBridgeAudioForLegacyDigitalTx(haltReason);
    }
    if (bridgeTuneMaybeActive) {
        bridgeLog(QStringLiteral("halt(%1): stopping bridge-owned tune audio before legacy stop")
                      .arg(haltReason));
        stopTune();
    }
#endif

    if (usingLegacyBackendForTx()) {
        bridgeLog(QStringLiteral("halt(%1): delegating stop to legacy backend").arg(haltReason));
        m_legacyBackend->stopTransmission();
        m_legacyBackend->startTune(false);
        m_bridgeAudioTuneActive = false;
        if (m_transmitting) {
            m_transmitting = false;
            emit transmittingChanged();
        }
        if (m_tuning) {
            m_tuning = false;
            emit tuningChanged();
        }
        setTxEnabled(false);
        scheduleLegacyStateRefreshBurst();
        return;
    }

    stopTx();
    stopTune();
    setTxEnabled(false);
}

#undef bridgeLog
