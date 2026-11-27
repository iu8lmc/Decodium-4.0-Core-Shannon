#include "DecodiumBridge.h"
#include "DecodiumBridgeTraceScope.h"
#include "DecodiumBridgeTxPolicy.h"
#include "Sequencer/MessageTokenRules.hpp"
#include "DecodiumLegacyBackend.h"
#include "Audio/soundout.h"
#include "Modulator/Modulator.hpp"
#include <QAudioDevice>
#include <QAudioFormat>
#include <QBuffer>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>

using decodium::seq::messageCarries73Payload;
QString decodiumBridgeBuildTxRecordingPath(QString const&, bool);
QString decodiumBridgeTxWaveMetricsSummary(QVector<float> const&);
#define buildTxRecordingPath decodiumBridgeBuildTxRecordingPath
#define txWaveMetricsSummary decodiumBridgeTxWaveMetricsSummary
void decodiumBridgeDecodeFlowLog(const QString&);
void decodiumBridgeTxPrepareTimelineLog(const QString&);
QString decodiumBridgeTxPrepareAudioFormat(const QAudioFormat&);
qreal decodiumBridgeTxAttenuationFromSlider(double);
qreal decodiumBridgeTxGainFromSlider(double);
#define bridgeLog decodiumBridgeDecodeFlowLog
#define txTimelineLog decodiumBridgeTxPrepareTimelineLog
#define audioFormatToString decodiumBridgeTxPrepareAudioFormat
#define txAttenuationFromSlider decodiumBridgeTxAttenuationFromSlider
#define txGainFromSlider decodiumBridgeTxGainFromSlider

bool DecodiumBridge::startBridgeAudioForLegacyDigitalTx(const QString& reason)
{
#if defined(Q_OS_MAC)
    MainThreadTraceScope trace(QStringLiteral("legacy_bridge_audio_start_main"),
                               QStringLiteral("reason=%1 mode=%2 tx=%3 active=%4")
                                   .arg(reason, m_mode)
                                   .arg(m_transmitting ? 1 : 0)
                                   .arg(m_bridgeAudioLegacyTxActive ? 1 : 0),
                               25);
    QElapsedTimer totalTimer;
    totalTimer.start();
    qint64 msgMs = 0;
    qint64 ensureMs = 0;
    qint64 outputMs = 0;
    qint64 modStopMs = 0;
    qint64 syncMs = 0;
    qint64 alignMs = 0;
    qint64 bufferMs = 0;
    qint64 scheduleMs = 0;

    if (!shouldUseBridgeAudioForLegacyDigitalTx()) {
        bridgeLog(QStringLiteral("legacyBridgeTxAudio start skipped (%1): mode=%2 legacy=%3")
                      .arg(reason, m_mode)
                      .arg(usingLegacyBackendForTx() ? 1 : 0));
        return false;
    }
    if (!m_pttConfirmed) {
        bridgeLog(QStringLiteral("legacyBridgeTxAudio start blocked (%1): PTT is not confirmed")
                      .arg(reason));
        return false;
    }
    if (m_bridgeAudioLegacyTxActive) {
        bridgeLog(QStringLiteral("legacyBridgeTxAudio already active (%1)").arg(reason));
        return true;
    }
    if (m_bridgeAudioTuneActive) {
        bridgeLog(QStringLiteral("legacyBridgeTxAudio: stopping bridge tune first (%1)").arg(reason));
        stopTune();
    }
    if (legacyBridgeAudioTxInFlight()) {
        bridgeLog(QStringLiteral("legacyBridgeTxAudio duplicate start ignored (%1): tx=%2 tune=%3 pcm=%4 sink=%5")
                      .arg(reason)
                      .arg(m_transmitting ? 1 : 0)
                      .arg(m_tuning ? 1 : 0)
                      .arg(m_txPcmBuffer ? 1 : 0)
                      .arg(m_txAudioSink ? 1 : 0));
        return true;
    }
    if (m_tuning) {
        bridgeLog(QStringLiteral("legacyBridgeTxAudio blocked by active Tune (%1)").arg(reason));
        return false;
    }
    if (!m_modulator || !m_soundOutput) {
        bridgeLog(QStringLiteral("legacyBridgeTxAudio unavailable: modulator=%1 soundOutput=%2")
                      .arg(m_modulator ? 1 : 0)
                      .arg(m_soundOutput ? 1 : 0));
        return false;
    }
    if (!checkSwrAllowsTransmission(QStringLiteral("legacyBridgeTxAudio"))) {
        return false;
    }

    QElapsedTimer phaseTimer;
    phaseTimer.start();
    QString msg = currentBridgeTxRepresentativeMessage();
    forceRecentRogerReportSignoffIfNeeded(msg, QStringLiteral("legacyBridgeTxAudio:%1").arg(reason));
    msgMs = phaseTimer.elapsed();
    if (msg.trimmed().isEmpty()) {
        bridgeLog(QStringLiteral("legacyBridgeTxAudio: empty TX message"));
        emit errorMessage(QStringLiteral("Nessun messaggio TX selezionato"));
        return false;
    }
    if (!repairOrRejectStalePartnerTxMessage(msg, QStringLiteral("legacyBridgeTxAudio:%1").arg(reason))) {
        emit statusMessage(QStringLiteral("TX bloccato: messaggio non coerente con il QSO attivo"));
        return false;
    }

    QVector<float> wave;
    QByteArray preparedPcm;
    QAudioFormat preparedFmt;
    QAudioDevice preparedDev;
    QString prepareError;
    int const txAudioFrequency = effectiveTxAudioFrequencyHz();
    phaseTimer.restart();
    if (!ensureTxAudioPrepared(msg, txAudioFrequency, true,
                               &wave, &preparedPcm, &preparedFmt, &preparedDev,
                               &prepareError)) {
        ensureMs = phaseTimer.elapsed();
        QString const errorText = prepareError.isEmpty()
            ? QStringLiteral("Audio TX: generazione waveform fallita")
            : prepareError;
        bridgeLog(QStringLiteral("legacyBridgeTxAudio prepare failed (%1): %2 msg=[%3]")
                      .arg(reason, errorText, msg.trimmed()));
        emit errorMessage(errorText.startsWith(QStringLiteral("Audio "), Qt::CaseInsensitive)
                          ? errorText
                          : QStringLiteral("Audio TX: %1 | modo=%2 freqAudio=%3 Hz msg=\"%4\"")
                                .arg(errorText, m_mode, QString::number(txAudioFrequency), msg.trimmed()));
        return false;
    }
    ensureMs = phaseTimer.elapsed();
    if (wave.isEmpty()) {
        bridgeLog(QStringLiteral("legacyBridgeTxAudio refused empty wave (%1) msg=[%2]")
                      .arg(reason, msg.trimmed()));
        emit errorMessage(QStringLiteral("Audio TX: waveform vuota dopo generazione | modo=%1 freqAudio=%2 Hz msg=\"%3\"")
                              .arg(m_mode, QString::number(txAudioFrequency), msg.trimmed()));
        return false;
    }

    QAudioDevice const outDev = preparedDev;
    QAudioFormat const outFmt = preparedFmt;
    qreal const attenuationDb = txAttenuationFromSlider(m_txOutputLevel);
    QAudioFormat const pcmFmt = outFmt;
    QByteArray pcm = std::move(preparedPcm);
    if (pcm.isEmpty()) {
        bridgeLog(QStringLiteral("legacyBridgeTxAudio PCM build failed (%1): fmt=%2 msg=[%3]")
                      .arg(reason, audioFormatToString(pcmFmt), msg.trimmed()));
        emit errorMessage(QStringLiteral("Audio TX: impossibile costruire PCM per il device selezionato | device=\"%1\" formato=%2 canale=%3 msg=\"%4\"")
                              .arg(outDev.description(),
                                   audioFormatToString(pcmFmt),
                                   QString::number(m_audioOutputChannel),
                                   msg.trimmed()));
        return false;
    }
    int syncInitialSilenceMs = 0;
    int syncSlotElapsedMs = -1;
    bool const syncAlignedPayload = shouldAlignTxAudioToCurrentSyncSlot();
    phaseTimer.restart();
    if (syncAlignedPayload && pcmFmt.isValid() && pcmFmt.sampleRate() > 0) {
        int const syncPeriodMs = effectivePeriodMsForMode(m_mode);
        if (syncPeriodMs > 0) {
            syncSlotElapsedMs =
                static_cast<int>(correctedUtcEpochMs() % static_cast<qint64>(syncPeriodMs));
            int const nominalLeadInMs = txSyncLeadInMsForMode(m_mode);
            syncInitialSilenceMs = syncSlotElapsedMs < nominalLeadInMs
                ? nominalLeadInMs - syncSlotElapsedMs
                : 0;
            if (syncInitialSilenceMs > 0) {
                int const leadInFrames =
                    qMax(0, qRound(static_cast<double>(pcmFmt.sampleRate()) * syncInitialSilenceMs / 1000.0));
                qsizetype const leadInBytes = pcmFmt.bytesForFrames(leadInFrames);
                if (leadInBytes > 0 && leadInBytes <= std::numeric_limits<int>::max()) {
                    pcm.prepend(QByteArray(static_cast<int>(leadInBytes), '\0'));
                }
            }
        }
    }
    alignMs = phaseTimer.elapsed();
    phaseTimer.restart();
    m_soundOutput->setFormat(outDev, static_cast<unsigned>(qMax(1, outFmt.channelCount())), 16384);
    m_soundOutput->setAttenuation(attenuationDb);
    outputMs = phaseTimer.elapsed();
    if (m_modulator->isActive()) {
        phaseTimer.restart();
        m_modulator->stop(true);
        modStopMs = phaseTimer.elapsed();
    }

    if (m_currentTx == m_lastNtx) {
        ++m_txRetryCount;
    } else {
        m_txRetryCount = 1;
        m_lastNtx = m_currentTx;
    }

    bool const txCarries73Payload =
        (m_currentTx == 5) || messageCarries73Payload(msg);
    bool const ft2DeferredSignoff =
        txCarries73Payload
        && (m_qsoProgress > 1)
        && (m_mode == QStringLiteral("FT2"))
        && (!m_quickQsoEnabled || !m_quickPeerSignaled)
        && !m_logAfterOwn73;
    bool const autoCqDeferredSignoff =
        m_autoCqRepeat
        && txCarries73Payload
        && (m_qsoProgress > 1)
        && (m_mode == QStringLiteral("FT4") || m_mode == QStringLiteral("FT8"))
        && !m_logAfterOwn73;
    if (ft2DeferredSignoff || autoCqDeferredSignoff) {
        m_ft2DeferredLogPending = true;
        capturePendingAutoLogSnapshot();
    }

    m_lastTransmittedMessage = msg.trimmed();
    m_lastTxActivityUtc = QDateTime::currentDateTimeUtc();
    m_activeTxNumber = m_currentTx;
    m_activeTxMessage = msg.trimmed();
    // The embedded backend owns its own PTT-edge cadence accounting.
    m_activeTxWasPureAutoCqCq = false;
    phaseTimer.restart();
    syncActiveCatTxSplitFrequency(QStringLiteral("legacyBridgeTxAudio"));
    syncMs = phaseTimer.elapsed();

    // Il blocco anti-duplicato dello sgancio vale per UNA trasmissione.
    // Azzerarlo nella sola beginLegacyPttTransition, che e' compilata
    // soltanto su macOS, lasciava la radio in aria dalla seconda
    // trasmissione in poi: ogni PTT OFF successivo veniva scartato come
    // doppione e nessuno comandava piu' lo sgancio.
    m_pttOffSentForTransition = false;

    if (!m_transmitting) {
        m_transmitting = true;
        emit transmittingChanged();
    }
    if (m_mode == QStringLiteral("FT8") && !m_tuning) {
        m_lastFt8MessageTxMs = QDateTime::currentMSecsSinceEpoch();
    }
    m_bridgeAudioLegacyTxActive = true;
    suspendNonAudioTxWork(QStringLiteral("legacy-bridge-tx"));

    if (m_soundInput
        && !m_ft2LinkTxActive
        && !(m_mode == QStringLiteral("FT2") && m_asyncTxEnabled)) {
        pauseRxAudioForTx(QStringLiteral("legacy bridge TX"));
    }

    appendTxDecodeEntry(msg);

    if (m_recordTxEnabled) {
        QString const txRecordPath = buildTxRecordingPath(m_mode, false);
        saveTxRecordingAsync(txRecordPath, wave, 48000, QStringLiteral("TX legacy bridge"));
    }

    qint64 const txPayloadMs = qMax<qint64>(
        1, qRound64(static_cast<double>(wave.size()) * 1000.0 / 48000.0));
    qint64 const txPlaybackMs = txPayloadMs + qMax<qint64>(0, syncInitialSilenceMs);
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    m_txPlaybackHoldUntilMs = nowMs + txPlaybackMs + 300;
    m_txPlaybackHardDeadlineMs = nowMs + txPlaybackMs + 5000;
    m_txPlaybackReleasePending = false;
    m_txAudioRestartPending = false;

    phaseTimer.restart();
    if (m_txPcmBuffer) {
        m_txPcmBuffer->close();
        delete m_txPcmBuffer;
        m_txPcmBuffer = nullptr;
    }
    m_txPcmData = std::move(pcm);
    m_txPcmBuffer = new QBuffer(this);
    m_txPcmBuffer->setData(m_txPcmData);
    m_txPcmBuffer->open(QIODevice::ReadOnly);
    m_txPcmBuffer->seek(0);
    bufferMs = phaseTimer.elapsed();
    QPointer<QBuffer> const bufferGuard(m_txPcmBuffer);
    bridgeLog(QStringLiteral("TX legacy bridge payload reason=%1 msg=[%2] waveSamples=%3 pcmBytes=%4 bufferSize=%5")
                  .arg(reason,
                       msg.trimmed())
                  .arg(wave.size())
                  .arg(m_txPcmData.size())
                  .arg(m_txPcmBuffer->size()));
    phaseTimer.restart();
    m_txAudioRestartPending = true;
    QPointer<SoundOutput> const soundOutputGuard(m_soundOutput);
    qint64 const audioLaunchScheduledAtMs = QDateTime::currentMSecsSinceEpoch();
    QString const launchReason = reason;
    QString const launchMessage = msg.trimmed();
    QTimer::singleShot(0, this, [this, bufferGuard, soundOutputGuard, audioLaunchScheduledAtMs,
                                 txPlaybackMs, launchReason, launchMessage]() {
        QElapsedTimer launchTimer;
        launchTimer.start();
        qint64 const queueDelayMs = QDateTime::currentMSecsSinceEpoch() - audioLaunchScheduledAtMs;
        if (!m_bridgeAudioLegacyTxActive || !m_transmitting || bufferGuard != m_txPcmBuffer || !soundOutputGuard) {
            m_txAudioRestartPending = false;
            txTimelineLog(QStringLiteral("[TX-TL] legacy_bridge_audio_launch skipped reason=%1 queue_delay_ms=%2 active=%3 transmitting=%4 buffer_ok=%5 sound_ok=%6 msg=[%7]")
                          .arg(launchReason)
                          .arg(queueDelayMs)
                          .arg(m_bridgeAudioLegacyTxActive ? 1 : 0)
                          .arg(m_transmitting ? 1 : 0)
                          .arg(bufferGuard == m_txPcmBuffer ? 1 : 0)
                          .arg(soundOutputGuard ? 1 : 0)
                          .arg(launchMessage));
            return;
        }

        soundOutputGuard->restart(m_txPcmBuffer);
        qint64 const restartMs = launchTimer.elapsed();
        m_txAudioRestartPending = false;
        txTimelineLog(QStringLiteral("[TX-TL] legacy_bridge_audio_launch reason=%1 queue_delay_ms=%2 restart_ms=%3 bufPos=%4/%5 msg=[%6]")
                      .arg(launchReason)
                      .arg(queueDelayMs)
                      .arg(restartMs)
                      .arg(m_txPcmBuffer ? m_txPcmBuffer->pos() : -1)
                      .arg(m_txPcmBuffer ? m_txPcmBuffer->size() : -1)
                      .arg(launchMessage));

        QTimer::singleShot(txPlaybackMs + 350, this, [this, bufferGuard, txPlaybackMs]() {
            if (!m_bridgeAudioLegacyTxActive || !m_transmitting || bufferGuard != m_txPcmBuffer) {
                return;
            }
            bridgeLog(QStringLiteral("legacyBridgeTxAudio completion timer: playback_ms=%1 bufPos=%2/%3")
                          .arg(txPlaybackMs)
                          .arg(m_txPcmBuffer ? m_txPcmBuffer->pos() : -1)
                          .arg(m_txPcmBuffer ? m_txPcmBuffer->size() : -1));
            finishModulatorIdlePlayback(QStringLiteral("legacy-bridge-pcm"));
        });
    });
    scheduleMs = phaseTimer.elapsed();

    bridgeLog(QStringLiteral("legacyBridgeTxAudio start: reason=%1 mode=%2 msg=[%3] dev=%4 fmt=%5 pcm_fmt=%6 channels=%7 attn=%8 gain=%9 payload=%10 payload_ms=%11 initial_silence_ms=%12 slot_elapsed_ms=%13 align_slot=%14 pcm_bytes=%15 hold_ms=%16")
                  .arg(reason, m_mode, msg.trimmed(), outDev.description(),
                       audioFormatToString(outFmt), audioFormatToString(pcmFmt))
                  .arg(pcmFmt.channelCount())
                  .arg(attenuationDb, 0, 'f', 2)
                  .arg(txGainFromSlider(m_txOutputLevel), 0, 'f', 3)
                  .arg(txWaveMetricsSummary(wave))
                  .arg(txPayloadMs)
                  .arg(syncInitialSilenceMs)
                  .arg(syncSlotElapsedMs)
                  .arg(syncAlignedPayload ? 1 : 0)
                  .arg(m_txPcmData.size())
                  .arg(txPlaybackMs + 300));
    txTimelineLog(QStringLiteral("[TX-TL] legacy_bridge_audio_prepare reason=%1 total_ms=%2 msg_ms=%3 ensure_ms=%4 align_ms=%5 output_ms=%6 mod_stop_ms=%7 sync_ms=%8 buffer_ms=%9 schedule_ms=%10 wave_samples=%11 pcm_bytes=%12 initial_silence_ms=%13 slot_elapsed_ms=%14 msg=[%15]")
                  .arg(reason)
                  .arg(totalTimer.elapsed())
                  .arg(msgMs)
                  .arg(ensureMs)
                  .arg(alignMs)
                  .arg(outputMs)
                  .arg(modStopMs)
                  .arg(syncMs)
                  .arg(bufferMs)
                  .arg(scheduleMs)
                  .arg(wave.size())
                  .arg(m_txPcmData.size())
                  .arg(syncInitialSilenceMs)
                  .arg(syncSlotElapsedMs)
                  .arg(msg.trimmed()));
    trace.addDetail(QStringLiteral("msg=[%1] pcm_bytes=%2")
                        .arg(msg.trimmed())
                        .arg(m_txPcmData.size()));
    emit statusMessage(QStringLiteral("TX: ") + msg.trimmed());
    return true;
#else
    Q_UNUSED(reason)
    return false;
#endif
}

void DecodiumBridge::stopBridgeAudioForLegacyDigitalTx(const QString& reason)
{
#if defined(Q_OS_MAC)
    cancelPendingLegacyBridgeAudioStart(QStringLiteral("stop:%1").arg(reason));
    MainThreadTraceScope trace(QStringLiteral("legacy_bridge_audio_stop_main"),
                               QStringLiteral("reason=%1 active=%2 tx=%3")
                                   .arg(reason)
                                   .arg(m_bridgeAudioLegacyTxActive ? 1 : 0)
                                   .arg(m_transmitting ? 1 : 0),
                               25);
    if (!m_bridgeAudioLegacyTxActive
        && !(m_transmitting && m_modulator && m_modulator->isActive())) {
        return;
    }
    bridgeLog(QStringLiteral("legacyBridgeTxAudio stop: reason=%1 active=%2 mod=%3")
                  .arg(reason)
                  .arg(m_bridgeAudioLegacyTxActive ? 1 : 0)
                  .arg((m_modulator && m_modulator->isActive()) ? 1 : 0));
    m_bridgeAudioLegacyTxActive = false;
    m_txPlaybackHoldUntilMs = 0;
    m_txPlaybackHardDeadlineMs = 0;
    m_txPlaybackReleasePending = false;
    m_txAudioRestartPending = false;
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();
    if (m_modulator && m_modulator->isActive()) {
        m_modulator->stop(true);
    }
    if (m_soundOutput) {
        m_soundOutput->finishPlayback();
    }
    if (m_txPcmBuffer) {
        m_txPcmBuffer->close();
        delete m_txPcmBuffer;
        m_txPcmBuffer = nullptr;
    }
    m_txPcmData.clear();
    if (m_transmitting) {
        m_transmitting = false;
        emit transmittingChanged();
        emit statusMessage(QStringLiteral("TX fermato"));
    }
    resumeRxAudioAfterTx(reason);
    resumeNonAudioTxWork(reason);
#else
    Q_UNUSED(reason)
#endif
}
