#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumTransceiverManager.h"
#include "DecodiumOmniRigManager.h"
#include "DecodiumCat4OmManager.h"
#include "DecodiumBridgeTxPcm.h"
#include "DecodiumBridgeTxFormat.h"
#include "Audio/soundout.h"
#include "Modulator/Modulator.hpp"
#include <QAudioSink>
#include <QBuffer>
#include <QDateTime>
#include <QTimer>
#include <cmath>

QString decodiumBridgeAudioDeviceIdForSettings(const QAudioDevice&);
QString decodiumBridgeAudioDeviceIdSettingForLog(QString);
#define audioDeviceIdForSettings decodiumBridgeAudioDeviceIdForSettings
#define audioDeviceIdSettingForLog decodiumBridgeAudioDeviceIdSettingForLog
void decodiumBridgeDecodeFlowLog(const QString&);
void decodiumBridgeTxPrepareTimelineLog(const QString&);
void decodiumBridgeRetireAudioSink(QAudioSink*, QBuffer*, const QString&);
QString decodiumBridgeBuildTxRecordingPath(QString const&, bool);
qreal decodiumBridgeTxAttenuationFromSlider(double);
qreal decodiumBridgeTxGainFromSlider(double);
bool decodiumBridgeIsHamlibFamilyBackend(QString const&);
bool decodiumBridgeSharedCatUsesVox(QObject const*, DecodiumTransceiverManager*, const QString&, DecodiumOmniRigManager*);
bool decodiumBridgeSharedCatCanPtt(QObject const*, DecodiumTransceiverManager*, const QString&, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
void decodiumBridgeSharedActiveCatSetPtt(QObject const*, DecodiumTransceiverManager*, const QString&, bool, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
#if defined(Q_OS_MAC)
AudioDevice::Channel decodiumBridgeTxOutputChannelForFormat(const QAudioFormat&, int);
#define txOutputChannelForFormat decodiumBridgeTxOutputChannelForFormat
#endif
#define bridgeLog decodiumBridgeDecodeFlowLog
#define txTimelineLog decodiumBridgeTxPrepareTimelineLog
#define retireAudioSink decodiumBridgeRetireAudioSink
#define buildTxRecordingPath decodiumBridgeBuildTxRecordingPath
#define txAttenuationFromSlider decodiumBridgeTxAttenuationFromSlider
#define txGainFromSlider decodiumBridgeTxGainFromSlider
#define isHamlibFamilyBackend decodiumBridgeIsHamlibFamilyBackend
#define activeCatUsesVoxPtt decodiumBridgeSharedCatUsesVox
#define activeCatCanPtt decodiumBridgeSharedCatCanPtt
#define activeCatSetPtt decodiumBridgeSharedActiveCatSetPtt

static QVector<float> buildTuneWaveform(double freq, int sampleRate, int durationSeconds)
{
    int const nFrames = sampleRate * durationSeconds;
    double const twoPi = 2.0 * 3.14159265358979323846;
    double const dphi = twoPi * freq / static_cast<double>(sampleRate);

    QVector<float> wave;
    wave.resize(nFrames);
    double phi = 0.0;
    for (int i = 0; i < nFrames; ++i) {
        wave[i] = static_cast<float>(std::sin(phi));
        phi += dphi;
        if (phi > twoPi) phi -= twoPi;
    }
    return wave;
}

void DecodiumBridge::startTune()
{
    if (m_ssbTxActive || m_mode == QStringLiteral("SSB")) return;
    if (rtlSdrEnabled()) {
        const QString message = QStringLiteral("Tune blocked: RTL-SDR receiver mode is receive-only.");
        bridgeLog(message);
        emit errorMessage(message);
        return;
    }
    clearDeferredManualSyncTx(QStringLiteral("start-tune"));

    if (m_bridgeAudioTuneActive) {
        bridgeLog("startTune: bridge tune already active, stopping");
        stopTune();
        return;
    }
    if (m_tuning || m_transmitting) return;
    if (legacyTxBackendRequested() && !legacyBackendAvailable() && !ensureLegacyBackendAvailable()) {
        emit errorMessage(QStringLiteral("Backend legacy non disponibile per Fox/Hound"));
        return;
    }
    m_lastTxActivityUtc = QDateTime::currentDateTimeUtc();
    // Da qui in poi il Tune parte davvero, su tutti e tre i percorsi (backend
    // legacy, TCI, audio del bridge): il watchdog si arma una volta sola qui.
    armTuneWatchdog();

    if (m_currentTx >= 1 && m_currentTx <= 5 &&
        m_dxCall.trimmed().isEmpty() &&
        buildCurrentTxMessage().trimmed() == m_tx6.trimmed() &&
        !m_tx6.trimmed().isEmpty()) {
        bridgeLog("startTune: no DX payload available, selecting TX6/CQ");
        setCurrentTx(6);
    }

    if (usingLegacyBackendForTx()) {
#if defined(Q_OS_MAC)
        bridgeLog("startTune(mac): using bridge audio path while legacy TX backend is active");
        if (m_legacyBackend) {
            m_legacyBackend->startTune(false);
        }
#else
        bridgeLog("startTune: delegating to legacy backend");
        if (m_recordTxEnabled) {
            double const freq = m_txFrequency > 0 ? m_txFrequency : 1500.0;
            QVector<float> const wave = buildTuneWaveform(freq, 48000, 10);
            QString const txRecordPath = buildTxRecordingPath(QStringLiteral("TUNE"), true);
            saveTxRecordingAsync(txRecordPath, wave, 48000, QStringLiteral("TUNE legacy"));
        }
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->startTune(true);
        if (!m_tuning) {
            m_tuning = true;
            emit tuningChanged();
        }
        scheduleLegacyStateRefreshBurst();
        emit statusMessage("TUNE via backend legacy");
        return;
#endif
    }

    if (usingTciAudioInput()) {
        m_tuning = true;
        emit tuningChanged();
        suspendNonAudioTxWork(QStringLiteral("tune-tci"));

        bridgeLog("startTune(TCI): canPtt=" + QString::number(activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) +
                  " catConnected=" + QString::number(m_catConnected));
        syncActiveCatTxSplitFrequency(QStringLiteral("startTune"));
        bool const voxPtt = activeCatUsesVoxPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat);
        const double freq = effectiveTxAudioFrequencyHz() > 0 ? effectiveTxAudioFrequencyHz() : 1500.0;
        if (!startTciTuneAudioStream(freq)) {
            m_tuning = false;
            emit tuningChanged();
            resumeNonAudioTxWork(QStringLiteral("tune-tci-start-failed"));
            emit errorMessage(QStringLiteral("Impossibile avviare audio TUNE TCI"));
            return;
        }
        if (!voxPtt && activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
            activeCatSetPtt(this, m_hamlibCat, m_catBackend, true, m_omniRigCat, m_legacyBackend);
            bridgeLog(QStringLiteral("PTT ON via tci after TUNE audio stream arm"));
        } else if (voxPtt) {
            bridgeLog(QStringLiteral("startTune(TCI): VOX selected, no CAT/DTR/RTS PTT"));
        }
        emit statusMessage("TUNE TCI: " + QString::number(static_cast<int>(freq)) + " Hz");
        return;
    }

#if defined(Q_OS_MAC)
    {
        bool requestedDeviceFound = false;
        QAudioDevice outDev = resolveTxOutputDevice(&requestedDeviceFound);

        if (!requestedDeviceFound) {
            bridgeLog("startTune(mac): requested output device not found, fallback to default: " +
                      outDev.description() +
                      " selected_id=" + audioDeviceIdSettingForLog(audioDeviceIdForSettings(outDev)) +
                      " requested=[" + m_audioOutputDevice + "] requested_id=" +
                      audioDeviceIdSettingForLog(m_audioOutputDeviceId));
            emit statusMessage(tr("Tune audio device not found, using the default one: %1")
                           .arg(outDev.description()));
        }
        const QAudioFormat outFmt = chooseTxAudioFormat(outDev);
        const AudioDevice::Channel outChannel = txOutputChannelForFormat(outFmt, m_audioOutputChannel);
        const qreal attenuationDb = txAttenuationFromSlider(m_txOutputLevel);
        const double freq = effectiveTxAudioFrequencyHz() > 0 ? effectiveTxAudioFrequencyHz() : 1500.0;

        updateSoundOutputDevice();
        m_soundOutput->setAttenuation(attenuationDb);
        if (m_modulator && m_modulator->isActive())
            m_modulator->stop(true);

        m_tuning = true;
        m_bridgeAudioTuneActive = true;
        emit tuningChanged();
        suspendNonAudioTxWork(QStringLiteral("tune-mac"));

        bridgeLog("startTune(mac): canPtt=" + QString::number(activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) +
                  " catConnected=" + QString::number(m_catConnected));
        syncActiveCatTxSplitFrequency(QStringLiteral("startTune"));
        bool const voxPtt = activeCatUsesVoxPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat);
        if (!voxPtt && activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend))
            activeCatSetPtt(this, m_hamlibCat, m_catBackend, true, m_omniRigCat, m_legacyBackend);
        else if (voxPtt)
            bridgeLog(QStringLiteral("startTune(mac): VOX audio-only tune; no CAT/DTR/RTS command will be sent"));

        m_modulator->tune(true);
        if (m_recordTxEnabled) {
            QVector<float> const wave = buildTuneWaveform(freq, 48000, 10);
            QString const txRecordPath = buildTxRecordingPath(QStringLiteral("TUNE"), true);
            saveTxRecordingAsync(txRecordPath, wave, 48000, QStringLiteral("TUNE"));
        }
        m_modulator->start(QStringLiteral("FT8"), 79, 1920.0, freq, 0.0,
                           m_soundOutput, outChannel, false, false,
                           99.0, 15.0);

        bridgeLog("startTune(mac) modulator: freq=" + QString::number(freq) +
                  " dev=" + outDev.description() +
                  " fmt=" + audioFormatToString(outFmt) +
                  " channel=" + QString::number(static_cast<int>(outChannel)) +
                  " attn=" + QString::number(attenuationDb, 'f', 2));
        emit statusMessage("TUNE: " + QString::number(static_cast<int>(freq)) + " Hz");
        return;
    }
#endif

    m_tuning = true;
    m_bridgeAudioTuneActive = false;
    emit tuningChanged();
    suspendNonAudioTxWork(QStringLiteral("tune"));

    bridgeLog("startTune: canPtt=" + QString::number(activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) +
              " catConnected=" + QString::number(m_catConnected));
    syncActiveCatTxSplitFrequency(QStringLiteral("startTune"));
    bool const voxPtt = activeCatUsesVoxPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat);
    if (!voxPtt && activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend))
        activeCatSetPtt(this, m_hamlibCat, m_catBackend, true, m_omniRigCat, m_legacyBackend);
    else if (voxPtt)
        bridgeLog(QStringLiteral("startTune: VOX audio-only tune; no CAT/DTR/RTS command will be sent"));

    // Timer di loop per rigenerare il tono ogni 10 secondi
    if (!m_tuneTimer) {
        m_tuneTimer = new QTimer(this);
        m_tuneTimer->setInterval(9800);
        connect(m_tuneTimer, &QTimer::timeout, this, [this]() {
            if (m_tuning && !launchTuneAudio()) {
                bridgeLog("startTune: periodic relaunch failed, stopping tune");
                stopTune();
            }
        });
    }
    m_tuneTimer->start();

    const double freq = effectiveTxAudioFrequencyHz() > 0 ? effectiveTxAudioFrequencyHz() : 1500.0;

    if (!launchTuneAudio()) {
        if (m_tuneTimer) m_tuneTimer->stop();
        disarmTuneWatchdog(QStringLiteral("tune-start-failed"));
        if (activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend))
            activeCatSetPtt(this, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
        m_tuning = false;
        emit tuningChanged();
        resumeNonAudioTxWork(QStringLiteral("tune-start-failed"));
        emit errorMessage(QStringLiteral(
            "Audio TUNE: impossibile avviare il tono | deviceRichiesto=\"%1\" "
            "freq=%2 Hz livelloTX=%3 canale=%4")
            .arg(m_audioOutputDevice.trimmed().isEmpty() ? QStringLiteral("<default output>")
                                                         : m_audioOutputDevice.trimmed(),
                 QString::number(freq, 'f', 1),
                 QString::number(m_txOutputLevel, 'f', 1),
                 QString::number(m_audioOutputChannel)));
        return;
    }

    bridgeLog("startTune push: freq=" + QString::number(freq) + " dev=" + m_audioOutputDevice);
    emit statusMessage("TUNE: " + QString::number(static_cast<int>(freq)) + " Hz");
}

bool DecodiumBridge::launchTuneAudio()
{
    const double freq    = effectiveTxAudioFrequencyHz() > 0 ? effectiveTxAudioFrequencyHz() : 1500.0;
    // Genera 10 secondi di tono sinusoidale a 48 kHz, poi converte nel formato
    // realmente supportato dal device di uscita.
    QVector<float> const wave = buildTuneWaveform(freq, 48000, 10);

    // Pulizia sink precedente
    if (m_txAudioSink || m_txPcmBuffer) {
        retireAudioSink(m_txAudioSink, m_txPcmBuffer, QStringLiteral("launchTune-cleanup"));
        m_txAudioSink = nullptr;
        m_txPcmBuffer = nullptr;
    }

    bool requestedDeviceFound = false;
    m_cachedTxOutputDeviceValid = false;
    QAudioDevice outDev = resolveTxOutputDevice(&requestedDeviceFound);

    if (!requestedDeviceFound) {
        bridgeLog("launchTuneAudio: requested output device not found, fallback to default: " +
                  outDev.description() +
                  " selected_id=" + audioDeviceIdSettingForLog(audioDeviceIdForSettings(outDev)) +
                  " requested=[" + m_audioOutputDevice + "] requested_id=" +
                  audioDeviceIdSettingForLog(m_audioOutputDeviceId));
    }
    const QAudioFormat outFmt = chooseTxAudioFormat(outDev);
    m_txPcmData = buildTxPcmBuffer(wave, outFmt, boundedAudioChannel(m_audioOutputChannel));
    if (m_txPcmData.isEmpty()) {
        bridgeLog("launchTuneAudio: unable to build PCM buffer for " + audioFormatToString(outFmt));
        return false;
    }
    if (m_recordTxEnabled) {
        QString const txRecordPath = buildTxRecordingPath(QStringLiteral("TUNE"), true);
        saveTxRecordingAsync(txRecordPath, wave, 48000, QStringLiteral("TUNE"));
    }

    m_txPcmBuffer = new QBuffer(this);
    m_txPcmBuffer->setData(m_txPcmData);
    m_txPcmBuffer->open(QIODevice::ReadOnly);
    m_txPcmBuffer->seek(0);

    // 1.0.220 — [TX-TL] timeline diagnostic anche per il path Tune.
#if defined(Q_OS_WIN)
    {
        if (!m_soundOutput) {
            bridgeLog(QStringLiteral("launchTuneAudio: SoundOutput path unavailable"));
            return false;
        }
        qreal const attenuationDb = txAttenuationFromSlider(m_txOutputLevel);
        qsizetype const tuneBufferBytes =
            txAudioBufferBytesForMode(outFmt, QStringLiteral("TUNE"), true,
                                      m_lowCpuModeEnabled || cpuPressureActive());
        m_bridgeAudioTuneActive = true;
        m_soundOutput->setFormat(outDev, static_cast<unsigned>(qMax(1, outFmt.channelCount())), 49152);
        m_soundOutput->setAttenuation(attenuationDb);
        m_soundOutput->restart(m_txPcmBuffer);
        txTimelineLog(QStringLiteral("[TX-TL] tune_sound_output_launch dev=%1 fmt=%2 configured_buf=%3 channel=%4 tx_level=%5 gain=%6 pcm_bytes=%7")
                          .arg(outDev.description(),
                               audioFormatToString(outFmt))
                          .arg(static_cast<qlonglong>(tuneBufferBytes))
                          .arg(m_audioOutputChannel)
                          .arg(m_txOutputLevel, 0, 'f', 1)
                          .arg(txGainFromSlider(m_txOutputLevel), 0, 'f', 4)
                          .arg(m_txPcmData.size()));
        QTimer::singleShot(2000, this, [this]() {
            if (m_tuning && m_txPcmBuffer) {
                txTimelineLog(QStringLiteral("[TX-TL] tune_sound_output_2s pos=%1/%2")
                                  .arg(m_txPcmBuffer->pos())
                                  .arg(m_txPcmBuffer->size()));
            }
        });
        return true;
    }
#endif

    qint64 const tuneSinkCreateStartMs = QDateTime::currentMSecsSinceEpoch();
    m_txAudioSink = new QAudioSink(outDev, outFmt, this);
    qint64 const tuneSinkCreateEndMs = QDateTime::currentMSecsSinceEpoch();
    txTimelineLog(QStringLiteral("[TX-TL] tune_sink_create dt=%1ms dev=%2")
                  .arg(tuneSinkCreateEndMs - tuneSinkCreateStartMs)
                  .arg(outDev.description().isEmpty()
                       ? QStringLiteral("<default>")
                       : outDev.description()));
    m_txAudioSink->setVolume(static_cast<float>(txGainFromSlider(m_txOutputLevel)));
    qsizetype const tuneBufferBytes =
        txAudioBufferBytesForMode(outFmt, QStringLiteral("TUNE"), true,
                                  m_lowCpuModeEnabled || cpuPressureActive());
    if (tuneBufferBytes > 0) {
        m_txAudioSink->setBufferSize(tuneBufferBytes);
    }
    bridgeLog(QStringLiteral("TUNE audio sink: %1 buffer=%2 channel=%3")
                  .arg(audioFormatToString(outFmt))
                  .arg(tuneBufferBytes)
                  .arg(m_audioOutputChannel));
    QString const outDevDescription = outDev.description().isEmpty()
        ? QStringLiteral("<default output>")
        : outDev.description();
    QString const outFmtDescription = audioFormatToString(outFmt);
    QPointer<QAudioSink> const tuneSinkGuard(m_txAudioSink);
    connect(m_txAudioSink, &QAudioSink::stateChanged, this,
            [this, tuneSinkGuard, outDevDescription, outFmtDescription, freq](QAudio::State st) {
        if (st == QAudio::StoppedState && tuneSinkGuard && tuneSinkGuard == m_txAudioSink &&
            tuneSinkGuard->error() != QAudio::NoError && m_tuning) {
            QAudio::Error const sinkError = tuneSinkGuard->error();
            QAudio::State const sinkState = tuneSinkGuard->state();
            bridgeLog(QStringLiteral("TUNE audio stopped with error, dropping PTT: %1 state=%2")
                          .arg(audioErrorToString(sinkError), audioStateToString(sinkState)));
            emit errorMessage(QStringLiteral(
                "Audio TUNE: sink fermato con errore | device=\"%1\" formato=%2 "
                "state=%3 qt-error=%4 freq=%5 Hz livelloTX=%6")
                .arg(outDevDescription,
                     outFmtDescription,
                     audioStateToString(sinkState),
                     audioErrorToString(sinkError),
                     QString::number(freq, 'f', 1),
                     QString::number(m_txOutputLevel, 'f', 1)));
            QMetaObject::invokeMethod(this, [this]() {
                stopTune();
            }, Qt::QueuedConnection);
        }
    }, Qt::QueuedConnection);
    m_txAudioSink->start(m_txPcmBuffer);
    bridgeLog("launchTuneAudio: sink_state=" + QString::number((int)m_txAudioSink->state()) +
              " err=" + QString::number((int)m_txAudioSink->error()) +
              " dev=" + outDev.description() +
              " fmt=" + audioFormatToString(outFmt) +
              " buf=" + QString::number(m_txAudioSink->bufferSize()) +
              " out_channel=" + QString::number(m_audioOutputChannel) +
              " tx_level=" + QString::number(m_txOutputLevel, 'f', 1) +
              " gain=" + QString::number(txGainFromSlider(m_txOutputLevel), 'f', 4) +
              " pcm_bytes=" + QString::number(m_txPcmData.size()));
    // Verifica dopo 2s se processedUSecs avanza (conferma audio fluisce)
    QTimer::singleShot(2000, this, [this]() {
        if (m_txAudioSink && m_tuning)
            bridgeLog("tuneAudio 2s check: processed=" + QString::number(m_txAudioSink->processedUSecs()) +
                      "us state=" + QString::number((int)m_txAudioSink->state()));
    });
    return true;
}

void DecodiumBridge::stopTune()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, &DecodiumBridge::stopTune, Qt::QueuedConnection);
        return;
    }

    if (sstvTxActive()) {
        bridgeLog(QStringLiteral(
            "stopTune ignored while native SSTV owns TX"));
        return;
    }

    disarmTuneWatchdog(QStringLiteral("stopTune"));

    bool const catReportsPttActive =
        (m_catBackend == QStringLiteral("cat4om") && m_cat4OmCat && m_cat4OmCat->pttActive())
        || (isHamlibFamilyBackend(m_catBackend) && m_hamlibCat && m_hamlibCat->pttActive())
        || (m_catBackend == QStringLiteral("omnirig") && m_omniRigCat && m_omniRigCat->pttActive());
    bool const pttReleaseNeeded =
        m_tuning
        || m_bridgeAudioTuneActive
        || m_txAudioSink
        || m_txPcmBuffer
        || (m_modulator && m_modulator->isActive())
        || catReportsPttActive;

    if (usingLegacyBackendForTx()) {
#if defined(Q_OS_MAC)
        if (pttReleaseNeeded || (m_legacyBackend && m_legacyBackend->tuning())) {
            bridgeLog("stopTune(mac): stopping bridge audio path while legacy TX backend is active");
        } else {
            return;
        }
#else
        if (pttReleaseNeeded || (m_legacyBackend && m_legacyBackend->tuning())) {
            bridgeLog("stopTune: delegating to legacy backend");
            m_legacyBackend->startTune(false);
            if (m_tuning) {
                m_tuning = false;
                emit tuningChanged();
                emit statusMessage(tr("Tune finished"));
            }
            scheduleLegacyStateRefreshBurst();
        }
        return;
#endif
    }

    m_txPlaybackHoldUntilMs = 0;
    m_txPlaybackHardDeadlineMs = 0;
    m_txPlaybackReleasePending = false;
    m_txAudioRestartPending = false;

#if defined(Q_OS_MAC)
    if (m_tuneTimer) m_tuneTimer->stop();
    if (m_txAudioSink || m_txPcmBuffer) {
        retireAudioSink(m_txAudioSink, m_txPcmBuffer, QStringLiteral("stopTune-mac"));
        m_txAudioSink = nullptr;
        m_txPcmBuffer = nullptr;
    }
    if (m_modulator)
        m_modulator->tune(false);
    if (m_soundOutput)
        m_soundOutput->stop();
    m_bridgeAudioTuneActive = false;
    m_txPcmData.clear();
    if (pttReleaseNeeded
        && activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
        activeCatSetPtt(this, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
    }
    if (m_tuning) {
        m_tuning = false;
        emit tuningChanged();
        emit statusMessage(tr("Tune finished"));
    }
    resumeNonAudioTxWork(QStringLiteral("stopTune"));
    return;
#endif

    if (!m_tuning) return;
    m_bridgeAudioTuneActive = false;
    if (m_tuneTimer) m_tuneTimer->stop();
    if (m_txAudioSink || m_txPcmBuffer) {
#if defined(Q_OS_WIN)
        if (m_soundOutput && !m_txAudioSink) {
            m_soundOutput->stop();
        }
#endif
        retireAudioSink(m_txAudioSink, m_txPcmBuffer, QStringLiteral("stopTune"));
        m_txAudioSink = nullptr;
        m_txPcmBuffer = nullptr;
    }
    m_txPcmData.clear();
    if (pttReleaseNeeded
        && activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
        activeCatSetPtt(this, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
    }
    if (usingTciAudioInput()) stopTciTxAudioStream(true);
    m_tuning = false;
    emit tuningChanged();
    emit statusMessage(tr("Tune finished"));
    resumeNonAudioTxWork(QStringLiteral("stopTune"));
}
