#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumTransceiverManager.h"
#include "DecodiumOmniRigManager.h"
#include "DecodiumCat4OmManager.h"
#include "CwSidetone.h"
#include "Audio/soundout.h"
#include "Modulator/Modulator.hpp"
#include <QAudioSink>
#include <QBuffer>
#include <QDateTime>
#include <QThread>

void decodiumBridgeDecodeFlowLog(const QString&);
void decodiumBridgeRetireAudioSink(QAudioSink*, QBuffer*, const QString&);
bool decodiumBridgeIsHamlibFamilyBackend(QString const&);
bool decodiumBridgeSharedCatCanPtt(QObject const*, DecodiumTransceiverManager*, const QString&, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
void decodiumBridgeSharedActiveCatSetPtt(QObject const*, DecodiumTransceiverManager*, const QString&, bool, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
#define bridgeLog decodiumBridgeDecodeFlowLog
#define retireAudioSink decodiumBridgeRetireAudioSink
#define isHamlibFamilyBackend decodiumBridgeIsHamlibFamilyBackend
#define activeCatCanPtt decodiumBridgeSharedCatCanPtt
#define activeCatSetPtt decodiumBridgeSharedActiveCatSetPtt

void DecodiumBridge::stopTx()
{
    ssbStop();
    // Qualunque sia il motivo per cui si smette, la radio remota deve smettere
    // con noi: e' l'unica che non puo' accorgersene da sola.
    if (m_decoPortUseRemote || !m_decoPortTxFrames.isEmpty())
        decoPortStopTx(QStringLiteral("stopTx"));
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, &DecodiumBridge::stopTx, Qt::QueuedConnection);
        return;
    }
    // JTTY owns the shared audio transmitter outside m_transmitting. The
    // generic stop path therefore needs an explicit hand-off to its controller
    // or HALT could leave a native JTTY transmission running.
    if (m_mode.compare(QStringLiteral("JTTY"), Qt::CaseInsensitive) == 0) {
        emit jttyAbortRequested();
    }
    // An aborted call must not be counted by a subsequent idle callback.
    m_activeTxWasPureAutoCqCq = false;

    if (sstvTxActive()) {
        bridgeLog(QStringLiteral(
            "stopTx routed to native SSTV coordinator"));
        cancelSstvTx();
        return;
    }

    bool const satelliteHalfDuplex = ft2LinkSatelliteHalfDuplexOperationActive();
    bool const catReportsPttActive =
        (m_catBackend == QStringLiteral("cat4om") && m_cat4OmCat && m_cat4OmCat->pttActive())
        || (isHamlibFamilyBackend(m_catBackend) && m_hamlibCat && m_hamlibCat->pttActive())
        || (m_catBackend == QStringLiteral("omnirig") && m_omniRigCat && m_omniRigCat->pttActive());
    bool const pttReleaseNeeded =
        m_transmitting
        || m_bridgeAudioLegacyTxActive
        || m_ft2LinkTxActive
        || m_cwTxActive
        || m_telemetryTxActive
        || m_txAudioSink
        || m_txPcmBuffer
        || (m_modulator && m_modulator->isActive())
        || catReportsPttActive;

    if (m_ft2LinkTxActive) {
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        bridgeLog(QStringLiteral("stopTx: aborting active FT2-Link TX tx=%1 bridgeLegacy=%2 pcm=%3/%4 hold_ms=%5 hard_ms=%6 ptt_needed=%7 catPtt=%8")
                      .arg(m_transmitting ? 1 : 0)
                      .arg(m_bridgeAudioLegacyTxActive ? 1 : 0)
                      .arg(m_txPcmBuffer ? m_txPcmBuffer->pos() : -1)
                      .arg(m_txPcmBuffer ? m_txPcmBuffer->size() : -1)
                      .arg(qMax<qint64>(0, m_txPlaybackHoldUntilMs - nowMs))
                      .arg(qMax<qint64>(0, m_txPlaybackHardDeadlineMs - nowMs))
                      .arg(pttReleaseNeeded ? 1 : 0)
                      .arg(catReportsPttActive ? 1 : 0));
    }

    // Direct CW, FT2-Link and telemetry audio belongs to the bridge even
    // when the legacy backend owns normal digital TX.  Use native cleanup
    // to stop its sound output and release CAT PTT.
    if (usingLegacyBackendForTx()
        && !m_cwTxActive && !m_ft2LinkTxActive && !m_telemetryTxActive) {
#if defined(Q_OS_MAC)
        cancelPendingLegacyBridgeAudioStart(QStringLiteral("stopTx"));
        if (m_bridgeAudioLegacyTxActive) {
            stopBridgeAudioForLegacyDigitalTx(QStringLiteral("stopTx"));
        }
#endif
        if (pttReleaseNeeded || (m_legacyBackend && m_legacyBackend->transmitting())) {
            bridgeLog("stopTx: delegating to legacy backend");
            m_legacyBackend->stopTransmission();
            scheduleLegacyStateRefreshBurst();
        }
        return;
    }

    m_txPlaybackHoldUntilMs = 0;
    m_txPlaybackHardDeadlineMs = 0;
    m_txPlaybackReleasePending = false;
    m_txAudioRestartPending = false;
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();
    // CW-audio interrotto manualmente: disarma lo stato CW.
    if (m_cwTxActive && m_cwSidetone) m_cwSidetone->clear();
    m_cwTxActive = false;
    m_pendingCwText.clear();
    // Telemetria stazione+meteo interrotta manualmente: idem.
    m_telemetryTxActive = false;
    m_pendingTelemetryHex.clear();
    if (!satelliteHalfDuplex) {
        m_ft2LinkTxActive = false;
        m_pendingFt2LinkText.clear();
        m_pendingFt2LinkWave.clear();
        m_pendingFt2LinkPlan.clear();
    }

#if defined(Q_OS_MAC)
    if (m_txAudioSink || m_txPcmBuffer) {
        retireAudioSink(m_txAudioSink, m_txPcmBuffer, QStringLiteral("stopTx-mac"));
        m_txAudioSink = nullptr;
        m_txPcmBuffer = nullptr;
    }
    if (m_modulator && m_modulator->isActive())
        m_modulator->stop(true);
    if (m_soundOutput)
        m_soundOutput->stop();
    m_txPcmData.clear();
    if (pttReleaseNeeded
        && activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
        activeCatSetPtt(this, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
    }
    if (m_transmitting) {
        m_transmitting = false;
        emit transmittingChanged();
        emit statusMessage(tr("Transmission stopped"));
    }
    if (satelliteHalfDuplex) {
        cancelFt2LinkSatelliteHalfDuplexTx(QStringLiteral("TX cancelled by operator"));
        return;
    }
    restoreTxAudioSchedulingBoost(QStringLiteral("stopTx"));
    resumeRxAudioAfterTx(QStringLiteral("stopTx"));
    resumeNonAudioTxWork(QStringLiteral("stopTx"));
    return;
#endif

    if (m_txAudioSink || m_txPcmBuffer) {
#if defined(Q_OS_WIN)
        if (m_bridgeAudioLegacyTxActive && m_soundOutput) {
            m_soundOutput->stop();
            m_bridgeAudioLegacyTxActive = false;
        }
#endif
        retireAudioSink(m_txAudioSink, m_txPcmBuffer, QStringLiteral("stopTx"));
        m_txAudioSink = nullptr;
        m_txPcmBuffer = nullptr;
    }
    m_txPcmData.clear();
    if (pttReleaseNeeded
        && activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
        activeCatSetPtt(this, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
    }
    if (usingTciAudioInput()) stopTciTxAudioStream(true);
    if (m_transmitting) {
        m_transmitting = false;
        emit transmittingChanged();
        emit statusMessage(tr("Transmission stopped"));
    }
    if (satelliteHalfDuplex) {
        cancelFt2LinkSatelliteHalfDuplexTx(QStringLiteral("TX cancelled by operator"));
        return;
    }
    restoreTxAudioSchedulingBoost(QStringLiteral("stopTx"));
    resumeRxAudioAfterTx(QStringLiteral("stopTx"));
    resumeNonAudioTxWork(QStringLiteral("stopTx"));
}
