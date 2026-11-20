#include "DecodiumBridge.h"
#include "Audio/soundout.h"
#include <QAudioSink>
#include <QBuffer>
#include <QDateTime>
#include <QRandomGenerator>
#include <QThread>
#include <QTimer>

void decodiumBridgeDecodeFlowLog(const QString&);
void decodiumBridgeTxPrepareTimelineLog(const QString&);
void decodiumBridgeRetireAudioSink(QAudioSink*, QBuffer*, const QString&);
bool decodiumBridgeSharedCatCanPtt(QObject const*, DecodiumTransceiverManager*, const QString&, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
void decodiumBridgeSharedActiveCatSetPtt(QObject const*, DecodiumTransceiverManager*, const QString&, bool, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
#define bridgeLog decodiumBridgeDecodeFlowLog
#define txTimelineLog decodiumBridgeTxPrepareTimelineLog
#define retireAudioSink decodiumBridgeRetireAudioSink
#define activeCatCanPtt decodiumBridgeSharedCatCanPtt
#define activeCatSetPtt decodiumBridgeSharedActiveCatSetPtt

void DecodiumBridge::completeTxPlayback(const QString& reason, bool error)
{
    if (QThread::currentThread() != thread()) {
        const QString queuedReason = reason;
        QMetaObject::invokeMethod(this, [this, queuedReason, error]() {
            completeTxPlayback(queuedReason, error);
        }, Qt::QueuedConnection);
        return;
    }

    if (sstvTxActive()) {
        bridgeLog(QStringLiteral(
            "completeTxPlayback ignored while native SSTV owns TX: %1")
                      .arg(reason));
        return;
    }

    bridgeLog("completeTxPlayback: reason=" + reason +
              " transmitting=" + QString::number(m_transmitting) +
              " rxSuspended=" + QString::number(m_rxAudioSuspendedForTx));
    // 1.0.218 — [TX-TL] entry timestamp con stato chiave per misurare la
    // latenza end-to-end fra "audio finito" e "TX terminato" (sospetto
    // singhiozzi).
    qint64 const completeEntryMs = QDateTime::currentMSecsSinceEpoch();
    qint64 const holdRemaining = qMax<qint64>(0, m_txPlaybackHoldUntilMs - completeEntryMs);
    qint64 const bufRemaining = m_txPcmBuffer
        ? qMax<qint64>(0, m_txPcmBuffer->size() - m_txPcmBuffer->pos())
        : -1;
    txTimelineLog(QStringLiteral("[TX-TL] complete_entry reason=%1 err=%2 tx=%3 hold_ms=%4 buf_rem=%5")
                  .arg(reason).arg(error ? 1 : 0).arg(m_transmitting ? 1 : 0)
                  .arg(holdRemaining).arg(bufRemaining));

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!error && m_transmitting && m_txPlaybackHoldUntilMs > nowMs) {
        // 1.0.218 — Cap rescheduling reason chain "-held" a 5 livelli. Senza
        // questo, se m_txPlaybackHoldUntilMs viene bumped ripetutamente dal
        // partial-idle path (line 11829), il reschedule sarebbe infinito.
        int const heldDepth = reason.count(QStringLiteral("-held"));
        if (heldDepth >= 5) {
            bridgeLog(QStringLiteral("completeTxPlayback: held chain cap raggiunto (%1), forcing complete: reason=%2")
                          .arg(heldDepth).arg(reason));
            m_txPlaybackHoldUntilMs = 0;
            m_txPlaybackReleasePending = false;
            // Fall through to normale cleanup path below
        } else {
            qint64 const delayMs = qMax<qint64>(1, m_txPlaybackHoldUntilMs - nowMs);
            if (!m_txPlaybackReleasePending) {
                m_txPlaybackReleasePending = true;
                bridgeLog(QStringLiteral("completeTxPlayback delayed: reason=%1 hold_ms=%2 depth=%3")
                              .arg(reason).arg(delayMs).arg(heldDepth));
                QTimer::singleShot(delayMs, this, [this, reason]() {
                    m_txPlaybackReleasePending = false;
                    completeTxPlayback(reason + QStringLiteral("-held"));
                });
            }
            return;
        }
    }

    if (!error && delayTxFinishUntilPcmConsumed(QStringLiteral("completeTxPlayback"),
                                                reason,
                                                true)) {
        return;
    }

    m_txPlaybackReleasePending = false;
    m_txPlaybackHoldUntilMs = 0;
    m_txPlaybackHardDeadlineMs = 0;
    m_txAudioRestartPending = false;
    logTxAudioTelemetrySummary(reason);
    bool const wasBridgeLegacyTx = m_bridgeAudioLegacyTxActive;
    m_bridgeAudioLegacyTxActive = false;

    if (m_txAudioSink) {
        retireAudioSink(m_txAudioSink, m_txPcmBuffer, reason);
        m_txAudioSink = nullptr;
        m_txPcmBuffer = nullptr;
        m_txPcmData.clear();
    }
    if (wasBridgeLegacyTx && m_soundOutput) {
        m_soundOutput->finishPlayback();
    }
    if (wasBridgeLegacyTx && m_txPcmBuffer) {
        m_txPcmBuffer->close();
        delete m_txPcmBuffer;
        m_txPcmBuffer = nullptr;
        m_txPcmData.clear();
    }

    // 1.0.225 — Chiudi PRIMA la finestra m_transmitting=true, poi setPtt.
    // Pre-1.0.225 l'ordine era inverso: setPtt(false) -> 5-30ms di radio
    // gia' in RX con m_transmitting still true -> autoSequenceStep dropping
    // legitimate decode FT2 con guard "ignore stale TX% payload while TX
    // active". Risultato percepito: sequencer non risponde subito al
    // partner al fine TX. Spostando l'emit transmittingChanged() PRIMA di
    // setPtt, decode-receiver vede m_transmitting=false e non droppa.
    bool const wasTransmitting = m_transmitting;
    int const finishedTx = m_activeTxNumber;
    if (m_transmitting) {
        m_transmitting = false;
        emit transmittingChanged();
    }

    if (wasBridgeLegacyTx) {
        requestLegacyPttOffOnce(QStringLiteral("complete:%1").arg(reason));
        clearLegacyPttTransition(QStringLiteral("complete:%1").arg(reason));
    } else if (activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
        activeCatSetPtt(this, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
    }
    if (usingTciAudioInput()) {
        stopTciTxAudioStream(true);
    }

    if (m_ft2LinkTxActive) {
        if (ft2LinkSatelliteHalfDuplexOperationActive()) {
            // PTT is already down.  Keep RX suspended until the explicit VFO
            // restore has completed; returning early here prevents a receive
            // restart on the uplink VFO.
            restoreFt2LinkSatelliteHalfDuplexRx(reason, error);
            return;
        }
        qint64 const guardNowMs = QDateTime::currentMSecsSinceEpoch();
        m_ft2LinkPostTxAckGuardUntilMs = guardNowMs + 15000;
        m_ft2LinkTxActive = false;
        m_pendingFt2LinkText.clear();
        m_pendingFt2LinkWave.clear();
        m_pendingFt2LinkPlan.clear();
        restoreTxAudioSchedulingBoost(reason);
        resumeRxAudioAfterTx(reason);
        resumeNonAudioTxWork(reason);
        if (error) emit errorMessage(QStringLiteral("FT2-Link: playback TX terminato con errore"));
        else       emit statusMessage(QStringLiteral("FT2-Link TX completato"));
        bridgeLog(QStringLiteral("completeTxPlayback: FT2-Link done reason=%1 err=%2")
                      .arg(reason).arg(error ? 1 : 0));
        emit ft2LinkTxFinished();
        return;
    }

    // CW-audio: PTT gia' abbassata sopra; ripristina RX e termina senza la
    // logica QSO/auto-sequence FT (signoff, re-arm, freq hop).
    if (m_cwTxActive) {
        if (m_cwSidetone) m_cwSidetone->clear();
        m_cwTxActive = false;
        m_pendingCwText.clear();
        restoreTxAudioSchedulingBoost(reason);
        resumeRxAudioAfterTx(reason);
        resumeNonAudioTxWork(reason);
        if (error) emit errorMessage(QStringLiteral("CW: playback TX terminato con errore"));
        else       emit statusMessage(QStringLiteral("CW completato"));
        bridgeLog(QStringLiteral("completeTxPlayback: CW done reason=%1 err=%2")
                      .arg(reason).arg(error ? 1 : 0));
        return;
    }

    // Telemetria stazione+meteo: come CW, termina senza la logica
    // QSO/auto-sequence FT (signoff, re-arm, freq hop) — e' un TX extra
    // one-shot dopo un QSO gia' chiuso, non parte del QSO stesso.
    if (m_telemetryTxActive) {
        m_telemetryTxActive = false;
        m_pendingTelemetryHex.clear();
        restoreTxAudioSchedulingBoost(reason);
        resumeRxAudioAfterTx(reason);
        resumeNonAudioTxWork(reason);
        if (error) emit errorMessage(QStringLiteral("Info stazione: playback TX terminato con errore"));
        else       emit statusMessage(QStringLiteral("Info stazione+meteo inviate"));
        bridgeLog(QStringLiteral("completeTxPlayback: telemetry done reason=%1 err=%2")
                      .arg(reason).arg(error ? 1 : 0));
        return;
    }

    bool const txPlaybackFinalizedQso = noteTxPlaybackFinished(reason, error);

    restoreTxAudioSchedulingBoost(reason);
    resumeRxAudioAfterTx(reason);
    resumeNonAudioTxWork(reason);
    m_foxElasticTxNow = false;
    if (wasTransmitting && !error) {
        foxElasticOnTxFinished();
    }

    if (txPlaybackFinalizedQso) {
        if (wasTransmitting && !error) {
            emit statusMessage(tr("Transmission complete"));
        }
        if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled) {
            m_asyncLastTxEndMs = QDateTime::currentMSecsSinceEpoch();
        }
        return;
    }

    bool const appliedDeferredAutoSeqAfterActiveTx =
        wasTransmitting && applyPendingAutoSeqTxAfterCompletedTx(finishedTx);

    // FT2 async manual QSO is edge-triggered by a double-click/map/cluster
    // selection. Keeping txEnabled latched after the audio ends makes the timer
    // retry the same TX step every FT2 slot. AutoCQ and queued callers keep the
    // old retry behavior; pre-signoff manual QSO steps re-arm only when
    // autoSequenceStep() receives a fresh partner decode and selects the next
    // TX message. TX4/TX5 keep the existing signoff/autolog path.
    // 1.0.321 — opt-in via m_ft2ManualOneShotEnabled (default OFF su fork iu8lmc).
    // OFF = comportamento pre-1.0.300 = TX1 ripete fino a maxCallerRetries
    //       (cruciale per partner deboli che non decodificano al primo periodo RX).
    // ON  = comportamento Salvatore 1.0.300 (disarm dopo TX1-TX3, re-arm da decode).
    if (m_ft2ManualOneShotEnabled
        && wasTransmitting
        && !error
        && !appliedDeferredAutoSeqAfterActiveTx
        && m_mode == QStringLiteral("FT2")
        && m_asyncTxEnabled
        && !m_autoCqRepeat
        && !m_multiAnswerMode
        && m_txEnabled
        && finishedTx >= 1
        && finishedTx <= 3
        && !m_dxCall.trimmed().isEmpty()) {
        bridgeLog(QStringLiteral("FT2 manual one-shot: TX%1 completed, disarm until partner decode")
                      .arg(finishedTx));
        setTxEnabled(false);
    }

    if (wasTransmitting
        && !error
        && !appliedDeferredAutoSeqAfterActiveTx) {
        armFt2AutoCqOneShotAfterCompletedTx(finishedTx, reason);
    }

    if (wasTransmitting) {
        if (error && reason != QStringLiteral("sound-output-error")) {
            emit errorMessage(QStringLiteral("Audio TX: playback terminato con errore | reason=%1 device=\"%2\" tx=%3 msg=\"%4\"")
                                  .arg(reason,
                                       m_audioOutputDevice.trimmed().isEmpty()
                                           ? QStringLiteral("<default output>")
                                           : m_audioOutputDevice.trimmed(),
                                       QString::number(finishedTx),
                                       m_activeTxMessage.trimmed()));
        } else if (!error) {
            emit statusMessage(tr("Transmission complete"));
        }
    }

    // 1.0.225 — guard reentry (BUG #8 agente-tx). Pre-1.0.225 questi 2
    // singleShot venivano coda CONTEMPORANEA al singleShot di
    // noteTxPlaybackFinished -> 3 chiamate consecutive, ognuna 12+ log
    // flush. Ora una sola in coda viene eseguita, le altre short-circuit.
    if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled) {
        m_asyncLastTxEndMs = QDateTime::currentMSecsSinceEpoch();
        if (!m_periodicTxCheckScheduled) {
            m_periodicTxCheckScheduled = true;
            quint64 const postTxSerial = m_ft2AsyncSmartTxSerial;
            quint64 const requestGeneration = m_txRequestGeneration;
            int const postTxFinishedTx = finishedTx;
            QTimer::singleShot(0, this, [this, postTxSerial, requestGeneration, postTxFinishedTx]() {
                m_periodicTxCheckScheduled = false;
                if (postTxSerial != m_ft2AsyncSmartTxSerial
                    || requestGeneration != m_txRequestGeneration
                    || (postTxFinishedTx >= 1
                        && postTxFinishedTx <= 6
                        && m_currentTx != postTxFinishedTx
                        && m_currentTx != 6)) {
                    return;
                }
                if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled &&
                    m_txEnabled && !m_transmitting && !m_tuning) {
                    checkAndStartPeriodicTx();
                }
            });
        }
        // 1.0.256 fix BUG #2 audit agente-tx: rimosso secondo singleShot
        // 220ms fallback. Pre-1.0.256 se il primo singleShot(0) abortiva
        // startTx (msg empty / stale partner repair fail), m_transmitting
        // restava false e il fallback 220ms rilanciava -> SECONDA TX su
        // stesso slot ("singhiozzo"). Con il guard universale
        // m_periodicTxInFlight in checkAndStartPeriodicTx il fallback e'
        // ridondante: gli altri trigger path (slot boundary, smart-Ft2-async,
        // decode-driven) gestiscono il retry naturale.
    }

    if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled && m_qsoProgress == 1 && !m_holdTxFreq) {
        int const hop = static_cast<int>(QRandomGenerator::global()->bounded(51)) - 25;
        int const newFreq = qBound(200, m_txFrequency + hop, 3000);
        if (newFreq != m_txFrequency) {
            setTxFrequency(newFreq);
            bridgeLog("FT2 async freq hop: " + QString::number(newFreq) + " Hz");
        }
    }
}
