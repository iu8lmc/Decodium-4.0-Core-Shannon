#include "DecodiumBridge.h"
#include "DecodiumBridgeTraceScope.h"
#include "DecodiumAudioSink.h"
#include "Audio/soundin.h"
#include "Audio/AudioDevice.hpp"
#include <QDateTime>
#include <QThread>
#include <QTimer>
#if DECODIUM_HAS_SSTV
#include "src/sstv/integration/SstvRxRuntime.h"
#endif

void decodiumBridgeAudioLevelLog(const QString&);
bool decodiumBridgeIsFt2LinkApplicationMode(QString);
QString decodiumBridgeAudioDeviceIdSettingForLog(QString);
QString decodiumBridgeAudioDeviceIdForSettings(const QAudioDevice&);
QString decodiumCacheDeviceId(const QAudioDevice&);
float decodiumBridgeRxInputGainFromLevel(double);
#ifdef Q_OS_LINUX
QString decodiumBridgeLinuxPulseSourceNameFromDisplay(QString);
bool decodiumBridgeLinuxPulseSourceIsMonitor(const QString&);
QString decodiumBridgeLinuxMatchingPulseSinkForSource(const QString&);
const char* decodiumCacheSinkPrefix();
#define linuxPulseSourceNameFromDisplay decodiumBridgeLinuxPulseSourceNameFromDisplay
#define linuxPulseSourceIsMonitor decodiumBridgeLinuxPulseSourceIsMonitor
#define linuxMatchingPulseSinkForSource decodiumBridgeLinuxMatchingPulseSinkForSource
#define kLinuxPulseSinkPrefix decodiumCacheSinkPrefix()
#endif
#define bridgeLog decodiumBridgeAudioLevelLog
#define isFt2LinkApplicationMode decodiumBridgeIsFt2LinkApplicationMode
#define audioDeviceIdSettingForLog decodiumBridgeAudioDeviceIdSettingForLog
#define audioDeviceIdForSettings decodiumBridgeAudioDeviceIdForSettings
#define audioDeviceIdForLog decodiumCacheDeviceId
#define rxInputGainFromLevel decodiumBridgeRxInputGainFromLevel

void DecodiumBridge::startAudioCapture(bool watchdogRecovery)
{
    MainThreadTraceScope trace(QStringLiteral("start_audio_capture"),
                               QStringLiteral("requested_in=[%1] mode=%2 legacy=%3")
                                   .arg(m_audioInputDevice, m_mode)
                                   .arg(usingLegacyBackendForRx() ? 1 : 0),
                               25);
    bridgeLog("startAudioCapture() called");
    // Con la radio remota in uso la scheda locale non si riapre, da nessuna
    // strada: non dal watchdog, non dal riaggancio dopo un cambio banda. Sono
    // gli automatismi che tengono viva la ricezione quando l'audio manca, e
    // qui l'audio non manca — arriva dalla rete. Riaprirla vorrebbe dire due
    // sorgenti nello stesso buffer.
    if (m_decoPortUseRemote) {
#if DECODIUM_HAS_SSTV
        auto const source = decodium::sstv::SstvAudioSourceKind::DecoPort;
        selectSstvRxSource(source, currentSstvAudioStreamId(source));
#endif
        // Il sink pero' serve lo stesso: e' il punto in cui l'audio della radio
        // remota entra nel decoder, e senza non entra da nessuna parte.
        ensureAudioSink();
        bridgeLog(QStringLiteral("startAudioCapture skipped: the remote radio is the source"));
        return;
    }
    qint64 const captureRequestMs = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kFt2LinkPostTxAckGuardMs = 15000;
    if (isFt2LinkApplicationMode(m_mode)
        && (m_transmitting
            || m_ft2LinkTxActive
            || (m_soundInput
                && m_ft2LinkPostTxAckGuardUntilMs > 0
                && captureRequestMs < m_ft2LinkPostTxAckGuardUntilMs)
            || (m_lastTxEndMs > 0
                && captureRequestMs - m_lastTxEndMs < kFt2LinkPostTxAckGuardMs))) {
        bridgeLog(QStringLiteral(
            "startAudioCapture skipped: FT2-Link post-TX ACK guard tx=%1 ft2Tx=%2 sinceTxEnd=%3ms guardLeft=%4ms")
                      .arg(m_transmitting ? 1 : 0)
                      .arg(m_ft2LinkTxActive ? 1 : 0)
                      .arg(m_lastTxEndMs > 0 ? captureRequestMs - m_lastTxEndMs : -1)
                      .arg(qMax<qint64>(0, m_ft2LinkPostTxAckGuardUntilMs - captureRequestMs)));
        m_audioWatchdogIgnoreUntilMs = qMax(m_audioWatchdogIgnoreUntilMs,
                                            captureRequestMs + kFt2LinkPostTxAckGuardMs);
        return;
    }
    if (rtlSdrEnabled()) {
        // 1.0.537 iu8lmc - la modalita' RTL-SDR vale per le chiavette basate
        // su RTL2832U. Una ColibriNANO o un altro SDR pilotato da software
        // esterno non si apre di li' ("opening device 0 failed") e, poiche'
        // questo ramo usciva comunque, restava senza alcuna sorgente audio.
        // Se la ricerca USB e' finita senza trovare nulla e l'audio TCI e'
        // configurato, si prosegue invece di lasciare il ricevitore muto.
        if (!m_rtlSdrDiscoveryPending && m_rtlSdrDevices.isEmpty() && usingTciAudioInput()) {
            bridgeLog(QStringLiteral(
                "startAudioCapture: nessuna chiavetta RTL-SDR presente, si usa l'audio TCI"));
        } else {
            bridgeLog(QStringLiteral("startAudioCapture: using RTL-SDR RX input"));
#if DECODIUM_HAS_SSTV
            auto const source = decodium::sstv::SstvAudioSourceKind::RtlSdr;
            selectSstvRxSource(source, currentSstvAudioStreamId(source));
#endif
            startRtlSdrCapture();
            return;
        }
    }
    if (usingLegacyBackendForRx()
        && !useModernSpectrumFeedWithLegacy()
        && !nativeSstvRxForcesDedicatedAudioCapture()) {
#if DECODIUM_HAS_SSTV
        auto const source = decodium::sstv::SstvAudioSourceKind::LegacyBackend;
        selectSstvRxSource(source, currentSstvAudioStreamId(source));
#endif
        bridgeLog(QStringLiteral("startAudioCapture skipped: legacy backend owns RX audio/panadapter"));
        if (m_monitoring) {
            rearmLegacyPcmSpectrumFeed(QStringLiteral("blocked modern capture start"));
        }
        return;
    }
    if (usingLegacyBackendForRx()
        && useModernSpectrumFeedWithLegacy()
        && !useDedicatedModernAudioCaptureWithLegacy()
        && !nativeSstvRxForcesDedicatedAudioCapture()) {
#if DECODIUM_HAS_SSTV
        auto const source = decodium::sstv::SstvAudioSourceKind::LegacyBackend;
        selectSstvRxSource(source, currentSstvAudioStreamId(source));
#endif
        bridgeLog(QStringLiteral("startAudioCapture skipped: legacy PCM tap is the single RX source for Direct Visual"));
        m_legacyPcmSpectrumFeed = true;
        if (m_soundInput || m_tciAudioCaptureActive) {
            stopAudioCapture();
        }
        return;
    }
    if (usingTciAudioInput()) {
#if DECODIUM_HAS_SSTV
        auto const source = decodium::sstv::SstvAudioSourceKind::Tci;
        selectSstvRxSource(source, currentSstvAudioStreamId(source), true);
#endif
        bridgeLog(QStringLiteral("startAudioCapture: using TCI audio stream"));
        startTciAudioCapture();
        return;
    }

    // Qt6: use the debounced audio-device cache here; RX start stays off the
    // expensive QMediaDevices::audioInputs() hot path.
#ifdef Q_OS_LINUX
    const QString pulseSourceName = linuxPulseSourceNameFromDisplay(m_audioInputDevice);
#endif
    bool requestedDeviceFound = false;
    QString inputMatchReason;
    QAudioDevice selectedDevice =
        resolveRxInputDevice(m_audioInputDevice, m_audioInputDeviceId,
                             &requestedDeviceFound, &inputMatchReason);
    QString const selectedDeviceId = audioDeviceIdForSettings(selectedDevice);
    bridgeLog(QStringLiteral("startAudioCapture: RX device resolve saved=[%1] saved_id=%2 match=%3 found=%4 selected=[%5] selected_id=%6 default=[%7] default_id=%8 input_count=%9")
                  .arg(m_audioInputDevice,
                       audioDeviceIdSettingForLog(m_audioInputDeviceId),
                       inputMatchReason,
                       requestedDeviceFound ? QStringLiteral("1") : QStringLiteral("0"),
                       selectedDevice.description(),
                       audioDeviceIdSettingForLog(selectedDeviceId),
                       cachedDefaultAudioInput(QStringLiteral("startAudioCapture log default"), false).description(),
                       audioDeviceIdForLog(cachedDefaultAudioInput(QStringLiteral("startAudioCapture log default"), false)))
                  .arg(cachedAudioInputs(QStringLiteral("startAudioCapture log input count"), false).size()));
    if (requestedDeviceFound) {
        rememberAudioInputDeviceIdentity(selectedDevice,
                                         QStringLiteral("startAudioCapture %1").arg(inputMatchReason),
                                         !m_audioInputDevice.trimmed().isEmpty());
    } else if (!m_audioInputDevice.trimmed().isEmpty()) {
        QString const warning = inputMatchReason.startsWith(QStringLiteral("ambiguous"))
            ? QStringLiteral("Audio input non univoco, uso default: %1")
            : QStringLiteral("Audio input non trovato, uso default: %1");
        bridgeLog(QStringLiteral("startAudioCapture: requested input unresolved; fallback default selected=[%1] selected_id=%2 requested=[%3] requested_id=%4 reason=%5")
                      .arg(selectedDevice.description(),
                           audioDeviceIdSettingForLog(selectedDeviceId),
                           m_audioInputDevice,
                           audioDeviceIdSettingForLog(m_audioInputDeviceId),
                           inputMatchReason));
        emit statusMessage(warning.arg(selectedDevice.description()));
    }

    ensureAudioSink();

    // Create SoundInput (once).
    if (!m_soundInput) {
        if (!m_soundInputThread) {
            m_soundInputThread = new QThread(this);
            m_soundInputThread->setObjectName(QStringLiteral("SoundInputThread"));
            m_soundInputThread->start(QThread::HighPriority);
        }
        m_soundInput = new SoundInput();
        m_soundInput->moveToThread(m_soundInputThread);
        connect(m_soundInputThread, &QThread::finished,
                m_soundInput, &QObject::deleteLater);
        connect(m_soundInput, &SoundInput::error, this, [this](const QString& msg) {
            QString text = msg.trimmed();
            if (!text.startsWith(QStringLiteral("Audio "), Qt::CaseInsensitive)) {
                text = QStringLiteral("Audio RX: ") + text;
            }
            emit errorMessage(text);
        });
        connect(m_soundInput, &SoundInput::recoverableError, this, [this](const QString& msg) {
            QString text = msg.trimmed();
            if (!text.startsWith(QStringLiteral("Audio "), Qt::CaseInsensitive)) {
                text = QStringLiteral("Audio RX: ") + text;
            }
            bridgeLog(QStringLiteral("Recoverable audio input failure: %1").arg(text));
            emit statusMessage(QStringLiteral("Audio RX temporaneamente interrotto; recupero automatico in corso"));

            if (m_rxAudioIoRecoveryQueued || !m_monitoring) {
                return;
            }
            if (m_transmitting || m_tuning || m_rxAudioKeptOpenForTx) {
                // resumeRxAudioAfterTx() already verifies fresh PCM and starts
                // the watchdog if the kept-open stream did not recover.
                bridgeLog(QStringLiteral("Audio RX I/O recovery deferred until TX ends"));
                return;
            }

            m_rxAudioIoRecoveryQueued = true;
            QTimer::singleShot(0, this, [this]() {
                if (!m_monitoring || m_transmitting || m_tuning) {
                    m_rxAudioIoRecoveryQueued = false;
                    return;
                }
                restartAudioCaptureFromWatchdog(QStringLiteral("Qt audio input I/O error"));
                // Keep duplicate stateChanged/checkStream notifications from
                // restarting the same endpoint repeatedly while the existing
                // 3.5-second recovery probe is still evaluating fresh PCM.
                QTimer::singleShot(4500, this, [this]() {
                    m_rxAudioIoRecoveryQueued = false;
                });
            });
        });
        connect(m_soundInput, &SoundInput::status, this, [this](const QString& msg) {
            QString text = msg.trimmed();
            if (!text.startsWith(QStringLiteral("Audio "), Qt::CaseInsensitive)) {
                text = QStringLiteral("Audio RX: ") + text;
            }
            emit statusMessage(text);
        });
    }

    // AudioDevice::Channel is intentionally mapped to the QML combobox index:
    // 0=Mono, 1=Left, 2=Right, 3=Both.
    AudioDevice::Channel channel =
        static_cast<AudioDevice::Channel>(qBound(0, m_audioInputChannel, 3));

    // downSampleFactor: SoundInput requests sample rate = 12000 * factor from
    // the driver.  Use 1 if the device natively supports 12 kHz; use 4 for
    // the common 48 kHz USB codec (e.g. Kenwood TS-590S USB Audio CODEC).
    // Windows Qt Multimedia can drop RX samples when the UI/graphics thread
    // stalls and the input buffer is only a few milliseconds deep.  JTDX/WSJT-X
    // use a much roomier audio queue; keep Decodium's RX buffer similarly
    // forgiving so FT4/FT8 final windows keep their full 12 kHz sample count.
    const unsigned downSampleFactor = 4; // assumes 48 kHz source
    // 1.0.165 — Salvatore in 1.0.158 ha alzato rxFramesPerBuffer da 4096 a
    // 32768. Su Windows+WASAPI questo causa "RX sordo dopo TX": Qt6 dopo
    // suspend()/resume() su uno stream con buffer >~256ms non riavvia i
    // pull callback, lo stream resta in IdleState permanente. Risultato:
    // niente sample arrivano al sink → Full Spectrum nero + decoder muto.
    // Rolling back a 8192 (~170ms) — buffer sufficiente per FT4/FT8 final
    // windows ma sotto la soglia di breakage di Qt-WASAPI.
    const int rxFramesPerBuffer = 8192; // ~170 ms at 48 kHz, before decimation.
    QString const startKey = QStringLiteral("%1|%2|%3|%4|%5")
        .arg(selectedDevice.description())
        .arg(selectedDeviceId)
        .arg(downSampleFactor)
        .arg(static_cast<int>(channel))
        .arg(rxFramesPerBuffer);
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (watchdogRecovery && m_soundInput) {
        // Keep the QAudioSource object alive for a watchdog recovery.  On
        // Qt 6.11 + PipeWire, a stop/delete/create cycle can leave a posted
        // QtMultimedia event aimed at a retired backend object and abort with
        // "pure virtual method called" on the main thread.
        bridgeLog(QStringLiteral("SoundInput::restart non-destructive watchdog recovery device=%1 id=%2")
                      .arg(selectedDevice.description(),
                           audioDeviceIdSettingForLog(selectedDeviceId)));
        m_lastAudioCaptureStartKey = startKey;
        m_lastAudioCaptureStartMs = nowMs;
        m_activeRxInputDeviceName = selectedDevice.description().trimmed();
        m_activeRxInputDeviceId = selectedDeviceId;
        emit sstvRxSourceDeviceChanged();
#if DECODIUM_HAS_SSTV
        auto sstvSource = decodium::sstv::SstvAudioSourceKind::LocalSoundCard;
#ifdef Q_OS_LINUX
        if (linuxPulseSourceIsMonitor(pulseSourceName)) {
            sstvSource = decodium::sstv::SstvAudioSourceKind::WebSdr;
        }
#endif
        selectSstvRxSource(sstvSource, currentSstvAudioStreamId(sstvSource), true);
#endif
        m_rxAudioStartupStartMs = nowMs;
        m_pendingRxAudioStartupHealthLog = true;
        m_audioUnhealthyStartMs = 0;
        m_audioWatchdogIgnoreUntilMs = nowMs + 5000;
        if (m_audioSink) {
            m_audioSink->resetDecimationPhase();
        }
        m_soundInput->restart(selectedDevice, rxFramesPerBuffer, m_audioSink,
                              downSampleFactor, channel);
        m_soundInput->setInputGain(rxInputGainFromLevel(m_rxInputLevel));
        trace.addDetail(QStringLiteral("watchdog_safe_recovery=1 selected=[%1] selected_id=%2")
                            .arg(selectedDevice.description(),
                                 audioDeviceIdSettingForLog(selectedDeviceId)));
        emit statusMessage(QStringLiteral("Audio RX recovery in corso: ") + selectedDevice.description());
        return;
    }
    bool const recentAudioStart =
        m_soundInput
        && m_lastAudioCaptureStartMs > 0
        && nowMs - m_lastAudioCaptureStartMs < 5000;
    if (recentAudioStart && m_lastAudioCaptureStartKey == startKey) {
        bridgeLog(QStringLiteral("startAudioCapture skipped: identical stream start already pending age=%1ms")
                      .arg(nowMs - m_lastAudioCaptureStartMs));
        m_soundInput->setInputGain(rxInputGainFromLevel(m_rxInputLevel));
        return;
    }
    if (!recentAudioStart
        && m_soundInput
        && m_soundInput->isActiveFor(selectedDevice, downSampleFactor, channel)) {
        m_soundInput->setInputGain(rxInputGainFromLevel(m_rxInputLevel));
        return;
    }

    bridgeLog("SoundInput::start device=" + selectedDevice.description() +
              " id=" + audioDeviceIdSettingForLog(selectedDeviceId) +
              " channel=" + QString::number((int)channel) +
              " dsf=" + QString::number(downSampleFactor) +
              " frames=" + QString::number(rxFramesPerBuffer));
    m_lastAudioCaptureStartKey = startKey;
    m_lastAudioCaptureStartMs = nowMs;
    m_activeRxInputDeviceName = selectedDevice.description().trimmed();
    m_activeRxInputDeviceId = selectedDeviceId;
    emit sstvRxSourceDeviceChanged();
#if DECODIUM_HAS_SSTV
    auto sstvSource = decodium::sstv::SstvAudioSourceKind::LocalSoundCard;
#ifdef Q_OS_LINUX
    if (linuxPulseSourceIsMonitor(pulseSourceName)) {
        sstvSource = decodium::sstv::SstvAudioSourceKind::WebSdr;
    }
#endif
    selectSstvRxSource(sstvSource, currentSstvAudioStreamId(sstvSource), true);
#endif
    m_rxAudioStartupStartMs = nowMs;
    m_pendingRxAudioStartupHealthLog = true;
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = QDateTime::currentMSecsSinceEpoch() + 5000;
    if (m_audioSink) {
        m_audioSink->resetDecimationPhase();
    }
    m_soundInput->start(selectedDevice, rxFramesPerBuffer, m_audioSink, downSampleFactor, channel);
    m_soundInput->setInputGain(rxInputGainFromLevel(m_rxInputLevel));
    trace.addDetail(QStringLiteral("selected=[%1] selected_id=%2 frames=%3 channel=%4")
                        .arg(selectedDevice.description(),
                             audioDeviceIdSettingForLog(selectedDeviceId))
                        .arg(rxFramesPerBuffer)
                        .arg(static_cast<int>(channel)));

    emit statusMessage(tr("Audio capture started: %1").arg(selectedDevice.description()));

    // Auto-match: se l'output TX non è configurato, cerca un device di output
    // che appartiene alla stessa scheda audio dell'input (es. "USB Audio CODEC")
    if (m_audioOutputDevice.isEmpty()) {
#ifdef Q_OS_LINUX
        const QString matchingPulseSink = linuxMatchingPulseSinkForSource(pulseSourceName);
        if (!matchingPulseSink.isEmpty()) {
            const QString displayName = QString::fromLatin1(kLinuxPulseSinkPrefix) + matchingPulseSink;
            setAudioOutputDevice(displayName);
            bridgeLog(QStringLiteral("auto-matched Linux Pulse/PipeWire TX output sink: %1 (from source: %2)")
                          .arg(matchingPulseSink, pulseSourceName));
        }
#endif
    }
    if (m_audioOutputDevice.isEmpty()) {
        // Estrai la parte identificativa dal nome input (es. "USB Audio CODEC" da "Microphone (USB Audio CODEC )")
        QString inputDesc = selectedDevice.description();
        // Cerca tra parentesi: "Microphone (USB Audio CODEC )" → "USB Audio CODEC"
        int paren1 = inputDesc.indexOf('(');
        int paren2 = inputDesc.lastIndexOf(')');
        QString deviceId;
        if (paren1 >= 0 && paren2 > paren1)
            deviceId = inputDesc.mid(paren1 + 1, paren2 - paren1 - 1).trimmed();

        if (!deviceId.isEmpty()) {
            for (const QAudioDevice& d :
                 cachedAudioOutputs(QStringLiteral("startAudioCapture auto-match output"), false)) {
                if (d.description().contains(deviceId, Qt::CaseInsensitive)) {
                    setAudioOutputDevice(d.description());
                    bridgeLog("auto-matched TX output device: " + d.description() +
                              " (from input: " + inputDesc + ")");
                    break;
                }
            }
        }
    }

    bridgeLog("startAudioCapture() done");
}

