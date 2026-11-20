#include "DecodiumBridge.h"

#if DECODIUM_HAS_SSTV
#include "Audio/soundout.h"
#include "Modulator/Modulator.hpp"
#include "src/sstv/diagnostics/SstvDiagnosticsController.h"
#include "src/sstv/integration/SstvTxCoordinator.h"
#if DECODIUM_HAS_HAMDRM
#include "src/sstv/digital/waveform/HamDrmWaveformAdapters.h"
#endif
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
bool decodiumSstvTerminalTxState(decodium::sstv::SstvTxState) noexcept;
namespace {
#if defined(Q_OS_MAC)
constexpr int kSstvOutputBufferFrames = 16'384;
#else
constexpr int kSstvOutputBufferFrames = 49'152;
#endif
}
#endif

void DecodiumBridge::initialiseSstvTx()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_sstvTxCoordinator || m_sstvTxShuttingDown) {
        return;
    }
    if (!m_soundOutput) {
        m_sstvTxState = tr("Unavailable");
        m_sstvTxError = tr("Decodium TX audio output is unavailable");
        emit sstvTxStateChanged();
        refreshSstvDiagnosticsSnapshot();
        return;
    }

    decodium::sstv::SstvTxCoordinatorConfig config;
    config.sampleRate = 48'000U;
    config.pttLeadDelayMs = static_cast<std::uint64_t>(sstvPttLeadMs());
    config.pttTailDelayMs = static_cast<std::uint64_t>(sstvPttTailMs());
    config.pttReleaseRetryMs = static_cast<std::uint64_t>(
        sstvPttReleaseRetryMs());
    config.voxPreKeyMs = static_cast<std::uint64_t>(sstvVoxPreKeyMs());
    config.voxHangMs = static_cast<std::uint64_t>(sstvVoxHangMs());
    config.voxToneFrequencyHz = sstvVoxToneFrequencyHz();
    config.voxToneLevel = sstvVoxToneLevel();
    config.voxEnvelopeEnabled = !qEnvironmentVariableIsSet(
        "DECODIUM_LAB_SSTV_SILENT_VOX");

    decodium::sstv::SstvTxCoordinatorHooks hooks;
    hooks.queryPreflight = [this]() {
        decodium::sstv::SstvTxCoordinatorPreflight result;
        const bool vox = sstvTxUsesVoxPtt();
        const bool localAudioRoute = !m_decoPortUseRemote
            && !usingTciAudioInput();
        const bool audioOnly = sstvTxAudioOnlyAllowed();
        result.audioOutputReady = m_soundOutput && localAudioRoute
            && m_sstvTxOutputPinned
            && !m_sstvTxOutputDevice.isNull()
            && (m_sstvTxOutputChannels == 1U
                || m_sstvTxOutputChannels == 2U);
        result.pttPathReady = vox || sstvTxCanControlPtt() || audioOnly;
        result.weakSignalSequencerActive = m_txEnabled || m_autoCqRepeat
            || m_deferredManualSyncTx || m_txRequested || m_pttPending
            || m_pttConfirmed;
        result.transmitAlreadyActive = m_sstvOwnsBridgeTx
            || m_transmitting || m_tuning || m_bridgeAudioLegacyTxActive
            || m_ft2LinkTxActive || m_cwTxActive || m_manualTxHold
            || m_bridgeAudioTuneActive
            || m_legacyBridgeAudioTxStartPending
            || m_txPlaybackReleasePending || m_txAudioRestartPending
            || m_txPlaybackHoldUntilMs != 0
            || m_txPlaybackHardDeadlineMs != 0
            || m_txAudioSink || m_txPcmBuffer
            || (m_modulator && m_modulator->isActive())
            || (!vox && sstvTxPttActive())
            || m_shuttingDown || QCoreApplication::closingDown();
        result.pttReleaseRequired = !vox && !audioOnly;
        result.voxAudioActivation = vox;
        if (!localAudioRoute) {
            result.detail =
                "SSTV TX currently requires Decodium's local SoundOutput route";
        } else if (!result.audioOutputReady) {
            // 1.0.588: "audio output is unavailable" e' la congiunzione di
            // quattro cose diverse, e l'operatore che la legge non puo' sapere
            // quale manchi: il dispositivo scollegato si riconnette, un
            // dispositivo mai fissato e' un difetto di sequenza nostro, e un
            // numero di canali fuori da 1/2 e' un driver che ha risposto male.
            // Dirlo qui costa una riga e risparmia un'indagine.
            if (!m_soundOutput) {
                result.detail =
                    "Decodium TX audio output is unavailable: no SoundOutput";
            } else if (!m_sstvTxOutputPinned) {
                result.detail =
                    "Decodium TX audio output is unavailable: the output device was not pinned for this session";
            } else if (m_sstvTxOutputDevice.isNull()) {
                result.detail =
                    "Decodium TX audio output is unavailable: the pinned output device is gone";
            } else {
                result.detail =
                    "Decodium TX audio output is unavailable: unsupported channel count "
                    + std::to_string(m_sstvTxOutputChannels);
            }
        } else if (!result.pttPathReady) {
            result.detail = "Decodium CAT/PTT or VOX is not ready";
        } else if (result.weakSignalSequencerActive) {
            result.detail = "the weak-signal sequencer is armed or pending";
        } else if (result.transmitAlreadyActive) {
            result.detail = "another Decodium transmission is active";
        }
        return result;
    };
    hooks.requestPttOn = [this](std::uint64_t sessionId) {
        if (m_sstvTxShuttingDown || m_shuttingDown
            || QCoreApplication::closingDown() || m_sstvOwnsBridgeTx
            || m_transmitting || m_tuning || m_txEnabled
            || m_autoCqRepeat || m_txRequested || m_pttPending
            || !m_sstvTxOutputPinned
            || m_sstvTxOutputDevice.isNull()) {
            return false;
        }
        const bool vox = sstvTxUsesVoxPtt();
        const bool audioOnly = sstvTxAudioOnlyAllowed();
        if (!vox && !sstvTxCanControlPtt() && !audioOnly) {
            return false;
        }
        if (!vox && sstvTxPttActive()) {
            return false;
        }

        // Invalidate every callback token owned by the weak-signal audio
        // sequencer before claiming the shared SoundOutput/m_transmitting
        // resources.  Entry guards in its cleanup paths then make any
        // already-queued callback a no-op for the SSTV session.
        ++m_txPlaybackSerial;
        ++m_txAudioTelemetrySerial;
        m_txPlaybackReleasePending = false;
        m_txPlaybackHoldUntilMs = 0;
        m_txPlaybackHardDeadlineMs = 0;
        m_txAudioRestartPending = false;
        clearLegacyPttTransition(QStringLiteral("sstv-tx-start"));

        m_sstvOwnsBridgeTx = true;
        m_sstvTxSessionId = static_cast<quint64>(sessionId);
        m_transmitting = true;
        emit transmittingChanged();
        applyTxAudioSchedulingBoost(QStringLiteral("sstv-tx"));
        suspendNonAudioTxWork(QStringLiteral("sstv-tx"));
        if (m_soundInput) {
            pauseRxAudioForTx(QStringLiteral("SSTV TX"));
        }
        emit sstvTxStateChanged();

        if (vox) {
            QTimer::singleShot(0, this, [this, sessionId]() {
                if (m_sstvTxCoordinator
                    && m_sstvTxSessionId == sessionId) {
                    m_sstvTxCoordinator->notifyPttConfirmed(
                        sstvTxNowMs(), sessionId);
                }
            });
        } else if (sstvTxCanControlPtt()) {
            setSstvTxPtt(true);
        } else {
            QTimer::singleShot(0, this, [this, sessionId]() {
                if (m_sstvTxCoordinator
                    && m_sstvTxSessionId == sessionId) {
                    m_sstvTxCoordinator->notifyPttConfirmed(
                        sstvTxNowMs(), sessionId);
                }
            });
        }
        return true;
    };
    hooks.requestPttOff = [this](std::uint64_t sessionId) {
        if (!sstvTxUsesVoxPtt() && sstvTxCanControlPtt()) {
            setSstvTxPtt(false);
        }
        if (!sstvTxPttActive()) {
            QTimer::singleShot(0, this, [this, sessionId]() {
                if (m_sstvTxCoordinator
                    && m_sstvTxSessionId == sessionId) {
                    m_sstvTxCoordinator->notifyPttReleased(
                        sstvTxNowMs(), sessionId);
                }
            });
        }
        return true;
    };
    hooks.startAudio = [this](
        const std::shared_ptr<decodium::sstv::SstvTxAudioDevice>& device,
        const decodium::sstv::SstvTxAudioPlan& plan) {
        if (!device || !m_soundOutput || !m_sstvOwnsBridgeTx
            || plan.sessionId != m_sstvTxSessionId
            || plan.sampleRate != 48'000U
            || !m_sstvTxOutputPinned
            || m_sstvTxOutputDevice.isNull()) {
            return false;
        }
        const unsigned channels = m_sstvTxOutputChannels;
        if (channels != plan.channelCount
            || channels != device->channelCount()) {
            return false;
        }
        m_soundOutput->setFormat(m_sstvTxOutputDevice, channels,
                                 kSstvOutputBufferFrames);
        m_sstvTxAudioDevice = device.get();
        const bool started = m_soundOutput->restartTrackedPlayback(
            device.get(), static_cast<quint64>(plan.sessionId),
            static_cast<quint64>(plan.totalFrames));
        if (!started) {
            return false;
        }
        emit statusMessage(tr("SSTV TX audio started: %1")
                               .arg(QString::fromStdString(plan.mode)));
        return true;
    };
    hooks.detachAudio = [this](
        const std::shared_ptr<decodium::sstv::SstvTxAudioDevice>& device,
        decodium::sstv::SstvTxAudioDetachReason reason) {
        if (!m_soundOutput || !device
            || (m_sstvTxAudioDevice
                && m_sstvTxAudioDevice.data() != device.get())) {
            return false;
        }
        if (reason == decodium::sstv::SstvTxAudioDetachReason::Completed) {
            if (!m_soundOutput->finishTrackedPlayback(m_sstvTxSessionId)) {
                return false;
            }
        } else {
            if (!m_soundOutput->stopTrackedPlayback(m_sstvTxSessionId)) {
                // startAudio is conservative: a hook can fail before tracked
                // playback is armed but after the coordinator marks the raw
                // device as possibly attached. Stop the owner path explicitly
                // so the shared lease can still be released synchronously.
                m_soundOutput->stop();
            }
        }
        m_sstvTxAudioDevice = nullptr;
        return true;
    };
    hooks.stateChanged = [this](
        const decodium::sstv::SstvTxCoordinatorSnapshot& snapshot) {
#if DECODIUM_HAS_HAMDRM
        if (m_hamDrmTxBackend) {
            m_hamDrmTxBackend->coordinatorStateChanged(snapshot);
        }
#endif
        const QString state = QString::fromLatin1(
            decodium::sstv::SstvTxStateMachine::stateName(
                snapshot.stateMachine.state));
        QString error;
        if (!snapshot.lastOperationDetail.empty()) {
            error = QString::fromStdString(snapshot.lastOperationDetail);
        } else if (!snapshot.stateMachine.lastErrorDetail.empty()) {
            error = QString::fromStdString(
                snapshot.stateMachine.lastErrorDetail);
        }
        const double progress = std::clamp(snapshot.progress, 0.0, 1.0);
        const bool diagnosticLifecycleChanged
            = state != m_sstvTxState || error != m_sstvTxError;
        const bool changed = state != m_sstvTxState
            || error != m_sstvTxError
            || !qFuzzyCompare(progress + 1.0, m_sstvTxProgress + 1.0);
        m_sstvTxState = state;
        m_sstvTxError = error;
        m_sstvTxProgress = progress;
        m_sstvTxSessionId = static_cast<quint64>(
            snapshot.stateMachine.currentSessionId);
        // 1.0.588: non smontare la rotta audio mentre ne stiamo montando una
        // nuova. publishState() notifica a ogni passaggio, anche quando lo
        // stato e' ancora quello TERMINALE della sessione precedente, e start()
        // /startPrepared() pubblicano prima di arrivare al proprio preflight:
        // il rilascio cancellava il dispositivo appena fissato e il preflight
        // rifiutava con "the output device was not pinned for this session".
        // Effetto per l'operatore: la prima trasmissione parte, tutte le
        // successive no, finche' non si riavvia il programma.
        if (!m_sstvTxStartInProgress
            && decodiumSstvTerminalTxState(snapshot.stateMachine.state)
            && !snapshot.audioLeaseRetained
            && (!snapshot.stateMachine.releaseRequired
                || snapshot.pttReleased)) {
            releaseSstvTxBridgeOwnership();
        }
        if (changed) {
            emit sstvTxStateChanged();
        }
        if (diagnosticLifecycleChanged && m_sstvDiagnosticsController
            && m_sstvDiagnosticsController->ready()) {
            // Do not rebuild the complete diagnostic snapshot for every
            // 20 ms playback-progress update. Lifecycle transitions remain
            // immediate on the next event-loop turn and terminal state cannot
            // leave the test-tone UI stuck in its running state.
            QTimer::singleShot(
                0, this,
                [this]() { refreshSstvDiagnosticsSnapshot(); });
        }
    };

    try {
        m_sstvTxClock.start();
        m_sstvTxCoordinator
            = std::make_unique<decodium::sstv::SstvTxCoordinator>(
                config, std::move(hooks));
        const decodium::sstv::SstvTxCoordinatorResult enabled
            = m_sstvTxCoordinator->enable(sstvTxNowMs());
        if (!enabled.accepted) {
            throw std::runtime_error(enabled.detail.empty()
                                         ? "SSTV TX enable failed"
                                         : enabled.detail);
        }
    } catch (const std::exception& exception) {
        m_sstvTxCoordinator.reset();
        m_sstvTxState = tr("Unavailable");
        m_sstvTxError = tr("Cannot initialise SSTV TX: %1")
                            .arg(QString::fromUtf8(exception.what()));
        emit sstvTxStateChanged();
        refreshSstvDiagnosticsSnapshot();
        return;
    }

    m_sstvTxTimer = new QTimer(this);
    m_sstvTxTimer->setObjectName(QStringLiteral("SSTV TX coordinator"));
    m_sstvTxTimer->setTimerType(Qt::PreciseTimer);
    m_sstvTxTimer->setInterval(20);
    connect(m_sstvTxTimer, &QTimer::timeout,
            this, &DecodiumBridge::tickSstvTx);
    connect(m_soundOutput, &SoundOutput::playbackError, this,
            [this](quint64 sessionId, const QString& detail) {
        if (!m_sstvTxCoordinator
            || m_sstvTxSessionId != sessionId) {
            return;
        }
        m_sstvTxCoordinator->notifyAudioError(
            sstvTxNowMs(), sessionId, detail.toStdString());
    }, Qt::QueuedConnection);
    connect(m_soundOutput, &SoundOutput::playbackUnderrun, this,
            [this](quint64 sessionId, const QString& detail) {
        if (!m_sstvTxCoordinator
            || m_sstvTxSessionId != sessionId) {
            return;
        }
        if (m_sstvTxUnderruns
                < std::numeric_limits<quint64>::max()) {
            ++m_sstvTxUnderruns;
        }
        m_sstvTxCoordinator->notifyAudioUnderrun(
            sstvTxNowMs(), sessionId, detail.toStdString());
    }, Qt::QueuedConnection);
    m_sstvTxTimer->start();
    emit sstvTxStateChanged();
    refreshSstvDiagnosticsSnapshot();
#endif
}

