#include "DecodiumBridge.h"
#include "BridgeConfigConstants.h"
#include "DecodiumLegacyBackend.h"
#include "Network/UdpClientId.hpp"
#include "PttTransitionPolicy.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QTimer>

bool decodiumBridgeEmbeddedLegacyRigEnabled(const QString&, bool);
bool decodiumBridgeSharedCatCanPtt(QObject const*, DecodiumTransceiverManager*, const QString&, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
bool decodiumBridgeSharedCatUsesVox(QObject const*, DecodiumTransceiverManager*, const QString&, DecodiumOmniRigManager*);
void decodiumBridgeSharedCatSetTxPtt(QObject const*, DecodiumTransceiverManager*, const QString&, bool, double, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
bool decodiumBridgeSharedCatSetTxPttAsync(QObject const*, DecodiumTransceiverManager*, const QString&, bool, double, DecodiumOmniRigManager*, DecodiumLegacyBackend*);
bool decodiumBridgeCatSuppressedForTx();
void decodiumBridgeTxPrepareTimelineLog(const QString&);
void decodiumBridgeAudioLevelLog(const QString&);
#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled
#define activeCatCanPtt decodiumBridgeSharedCatCanPtt
#define activeCatUsesVoxPtt decodiumBridgeSharedCatUsesVox
#define activeCatSetTxPtt decodiumBridgeSharedCatSetTxPtt
#define activeCatSetTxPttAsync decodiumBridgeSharedCatSetTxPttAsync
#define catSuppressedByEnvironment decodiumBridgeCatSuppressedForTx
#define txTimelineLog decodiumBridgeTxPrepareTimelineLog
#define bridgeLog decodiumBridgeAudioLevelLog

bool DecodiumBridge::ensureLegacyBackendAvailable()
{
    if (legacyBackendAvailable()) {
        return true;
    }

    bool const createdNow = (m_legacyBackend == nullptr);
    if (!m_legacyBackend) {
        if (auto * app = QCoreApplication::instance()) {
            app->setProperty("decodiumEmbeddedLegacyRigControlEnabled",
                             embeddedLegacyRigControlEnabledForBackend(m_catBackend,
                                                                       legacyTxBackendRequested()));
        }
        QStringList const udpClientIds {
            decodium::network::normalizedUdpClientId(getSetting(QStringLiteral("UDPClientId"), QStringLiteral("Decodium")).toString()),
            decodium::network::normalizedUdpClientId(getSetting(QStringLiteral("UDPSecondaryClientId"), QStringLiteral("Decodium")).toString()),
            decodium::network::normalizedUdpClientId(getSetting(QStringLiteral("UDPTertiaryClientId"), QStringLiteral("Decodium")).toString())};
        m_legacyBackend = new DecodiumLegacyBackend(this, udpClientIds);
    }

    if (!legacyBackendAvailable()) {
        bridgeLog("Legacy backend unavailable: " + (m_legacyBackend ? m_legacyBackend->failureReason()
                                                                    : QStringLiteral("unknown error")));
        return false;
    }

    applyAutoCqBurstCadenceToLegacyBackend();

    if (createdNow) {
        connect(m_legacyBackend, &DecodiumLegacyBackend::waterfallRowReady,
                this, &DecodiumBridge::onLegacyWaterfallRow);
        connect(m_legacyBackend, &DecodiumLegacyBackend::audioSamplesReady,
                this, &DecodiumBridge::onLegacyAudioSamples);
        connect(m_legacyBackend, &DecodiumLegacyBackend::adifLogged,
                this, &DecodiumBridge::mirrorLegacyLoggedAdif);
        connect(m_legacyBackend, &DecodiumLegacyBackend::warningRaised,
                this, [this](QString const& title, QString const& summary, QString const& details) {
            emit warningRaised(title, summary, details);
        });
        connect(m_legacyBackend, &DecodiumLegacyBackend::rigErrorRaised,
                this, [this](QString const& title, QString const& summary, QString const& details) {
            emit rigErrorRaised(title, summary, details);
        });
        connect(m_legacyBackend, &DecodiumLegacyBackend::preferencesRequested,
                this, [this]() {
                    emit setupSettingsRequested(0);
                });
        connect(m_legacyBackend, &DecodiumLegacyBackend::quitRequested,
                this, [this]() {
                    emit quitRequested();
                });
        connect(m_legacyBackend, &DecodiumLegacyBackend::rigPttRequested,
                this, [this](bool enabled) {
                    bridgeLog(QStringLiteral("legacyPttRequested: on=%1 backend=%2 catConnected=%3")
                                  .arg(enabled ? 1 : 0)
                                  .arg(m_catBackend)
                                  .arg(m_catConnected ? 1 : 0));
#if defined(Q_OS_MAC)
                    bool const bridgeManagedAudio = shouldUseBridgeAudioForLegacyDigitalTx();
                    if (bridgeManagedAudio) {
                        if (!enabled) {
                            cancelPendingLegacyBridgeAudioStart(QStringLiteral("legacy-ptt-off"));
                            if (m_bridgeAudioLegacyTxActive) {
                                qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
                                if (m_txPlaybackHoldUntilMs > nowMs || m_txAudioRestartPending) {
                                    bridgeLog(QStringLiteral("legacyPttRequested: ignored early legacy PTT off while bridge audio is playing hold_ms=%1 restart_pending=%2")
                                                  .arg(qMax<qint64>(0, m_txPlaybackHoldUntilMs - nowMs))
                                                  .arg(m_txAudioRestartPending ? 1 : 0));
                                    return;
                                }
                            }
                            requestLegacyPttOffOnce(QStringLiteral("legacy-ptt-off"));
                            if (m_bridgeAudioLegacyTxActive) {
                                stopBridgeAudioForLegacyDigitalTx(QStringLiteral("legacy-ptt-off"));
                            }
                            clearLegacyPttTransition(QStringLiteral("legacy-ptt-off"));
                            if (usingLegacyBackendForTx() && !useModernSpectrumFeedWithLegacy()) {
                                QTimer::singleShot(0, this, [this]() {
                                    resumeRxAudioAfterTx(QStringLiteral("legacy-ptt-off"));
                                    scheduleLegacyPcmSpectrumRearm(QStringLiteral("legacy-ptt-off"));
                                });
                            }
                            return;
                        }

                        clearLegacyPttTransition(QStringLiteral("new-legacy-ptt-on"));
                        if (!preflightLegacyBridgeTxBeforePtt(QStringLiteral("legacy-ptt-on"))) {
                            abortLegacyBridgeTxRequest(QStringLiteral("legacy-ptt-on-preflight"));
                            return;
                        }

                        auto startConfirmedBridgeAudio = [this]() -> bool {
                            syncLegacyBackendState();
                            bool const started = startBridgeAudioForLegacyDigitalTx(
                                QStringLiteral("legacy-ptt-confirmed"));
                            if (!started) {
                                bridgeLog(QStringLiteral("legacyPttRequested: bridge TX audio failed after PTT confirmation"));
                            }
                            return started;
                        };

                        bool const canPtt = activeCatCanPtt(
                            this, m_hamlibCat, m_catBackend,
                            m_omniRigCat, m_legacyBackend);
                        bool const voxPtt = activeCatUsesVoxPtt(
                            this, m_hamlibCat, m_catBackend, m_omniRigCat);
                        bool const audioOnly = voxPtt || catSuppressedByEnvironment();
                        decodium::tx::PttConfirmationMode const confirmationMode = audioOnly
                            ? decodium::tx::PttConfirmationMode::AudioActivity
                            : decodium::tx::pttConfirmationMode(activePttMethod(), canPtt);
                        beginLegacyPttTransition(QStringLiteral("legacy-ptt-on"),
                                                 confirmationMode,
                                                 startConfirmedBridgeAudio);

                        auto dispatchPttOn = [this]() {
                            QElapsedTimer pttTimer;
                            pttTimer.start();
                            QElapsedTimer syncTimer;
                            syncTimer.start();
                            syncActiveCatTxSplitFrequency(QStringLiteral("legacy-ptt-on"));
                            qint64 const syncMs = syncTimer.elapsed();
                            double const txDialHz = catSplitTxPttDialFrequencyHz();
                            QElapsedTimer catTimer;
                            catTimer.start();
                            m_pttOnDispatched = true;
                            bool const asyncPtt = activeCatSetTxPttAsync(
                                this, m_hamlibCat, m_catBackend,
                                true, txDialHz, m_omniRigCat, m_legacyBackend);
                            if (!asyncPtt) {
                                activeCatSetTxPtt(this, m_hamlibCat, m_catBackend,
                                                  true, txDialHz,
                                                  m_omniRigCat, m_legacyBackend);
                            }
                            txTimelineLog(QStringLiteral("[TX-TL] legacy_ptt_dispatch on=1 total_ms=%1 sync_ms=%2 cat_ms=%3 backend=%4 method=%5 txDialHz=%6 async=%7")
                                              .arg(pttTimer.elapsed())
                                              .arg(syncMs)
                                              .arg(catTimer.elapsed())
                                              .arg(m_catBackend, activePttMethod())
                                              .arg(txDialHz, 0, 'f', 0)
                                              .arg(asyncPtt ? 1 : 0));
                        };

                        if (audioOnly) {
                            bridgeLog(voxPtt
                                ? QStringLiteral("legacyPttRequested: VOX path, confirmation follows audio dispatch")
                                : QStringLiteral("legacyPttRequested: CAT disabled, bridge audio-only TX"));
                            return;
                        }
                        if (canPtt) {
                            dispatchPttOn();
                            return;
                        }

                        // Do not reconnect a serial/CAT backend from the TX
                        // path.  The 650 ms transition timer will cancel and
                        // requeue this message; normal CAT recovery remains
                        // independent and cannot stall the scene graph.
                        bridgeLog(QStringLiteral("legacyPttRequested: CAT PTT unavailable; waiting for asynchronous timeout"));
                        return;
                    }

                    if (!enabled) {
                        cancelPendingLegacyBridgeAudioStart(QStringLiteral("legacy-ptt-off"));
                    }
                    if (!enabled && m_bridgeAudioLegacyTxActive) {
                        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
                        if (m_txPlaybackHoldUntilMs > nowMs || m_txAudioRestartPending) {
                            bridgeLog(QStringLiteral("legacyPttRequested: ignored early legacy PTT off while bridge audio is playing hold_ms=%1 restart_pending=%2")
                                          .arg(qMax<qint64>(0, m_txPlaybackHoldUntilMs - nowMs))
                                          .arg(m_txAudioRestartPending ? 1 : 0));
                            return;
                        }
                        stopBridgeAudioForLegacyDigitalTx(QStringLiteral("legacy-ptt-off"));
                    }
                    if (enabled && shouldUseBridgeAudioForLegacyDigitalTx()
                        && !preflightLegacyBridgeTxBeforePtt(QStringLiteral("legacy-ptt-on"))) {
                        abortLegacyBridgeTxRequest(QStringLiteral("legacy-ptt-on-preflight"));
                        return;
                    }
#endif
                    if (activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
                        QElapsedTimer pttTimer;
                        pttTimer.start();
                        double txDialHz = 0.0;
                        qint64 syncMs = 0;
                        if (enabled) {
                            QElapsedTimer syncTimer;
                            syncTimer.start();
                            syncActiveCatTxSplitFrequency(QStringLiteral("legacy-ptt-on"));
                            syncMs = syncTimer.elapsed();
                            txDialHz = catSplitTxPttDialFrequencyHz();
                        }
                        QElapsedTimer catTimer;
                        catTimer.start();
                        bool const useAsyncPtt =
                            enabled && shouldUseBridgeAudioForLegacyDigitalTx();
                        bool const asyncPtt = useAsyncPtt
                            && activeCatSetTxPttAsync(this, m_hamlibCat, m_catBackend,
                                                      enabled, txDialHz, m_omniRigCat, m_legacyBackend);
                        if (!asyncPtt) {
                            activeCatSetTxPtt(this, m_hamlibCat, m_catBackend,
                                              enabled, txDialHz, m_omniRigCat, m_legacyBackend);
                        }
                        qint64 const catMs = catTimer.elapsed();
                        int const audioDelayMs = asyncPtt ? kLegacyBridgeAudioAfterAsyncPttDelayMs : 0;
                        txTimelineLog(QStringLiteral("[TX-TL] legacy_ptt reason=legacy-ptt-on on=%1 total_ms=%2 sync_ms=%3 cat_ms=%4 backend=%5 txDialHz=%6 async=%7 audio_delay_ms=%8")
                                      .arg(enabled ? 1 : 0)
                                      .arg(pttTimer.elapsed())
                                      .arg(syncMs)
                                      .arg(catMs)
                                      .arg(m_catBackend)
                                      .arg(txDialHz, 0, 'f', 0)
                                      .arg(asyncPtt ? 1 : 0)
                                      .arg(audioDelayMs));
#if defined(Q_OS_MAC)
                        if (enabled && shouldUseBridgeAudioForLegacyDigitalTx()) {
                            quint64 const audioStartSerial =
                                armPendingLegacyBridgeAudioStart(QStringLiteral("legacy-ptt-on"));
                            QTimer::singleShot(audioDelayMs, this, [this, audioStartSerial]() {
                                if (!consumePendingLegacyBridgeAudioStart(audioStartSerial,
                                                                          QStringLiteral("legacy-ptt-on"))) {
                                    return;
                                }
                                syncLegacyBackendState();
                                if (!startBridgeAudioForLegacyDigitalTx(QStringLiteral("legacy-ptt-on"))) {
                                    bridgeLog(QStringLiteral("legacyPttRequested: bridge TX audio failed, stopping legacy TX"));
                                    abortLegacyBridgeTxRequest(QStringLiteral("legacy-ptt-on-audio-failed"));
                                }
                            });
                        }
#endif
                    } else {
#if defined(Q_OS_MAC)
                        if (enabled
                            && shouldUseBridgeAudioForLegacyDigitalTx()
                            && catSuppressedByEnvironment()) {
                            bridgeLog(QStringLiteral("legacyPttRequested: CAT disabled, starting bridge audio without CAT PTT"));
                            if (!preflightLegacyBridgeTxBeforePtt(QStringLiteral("legacy-ptt-audio-only"))) {
                                abortLegacyBridgeTxRequest(QStringLiteral("legacy-ptt-audio-only-preflight"));
                                return;
                            }
                            quint64 const audioStartSerial =
                                armPendingLegacyBridgeAudioStart(QStringLiteral("legacy-ptt-audio-only"));
                            QTimer::singleShot(0, this, [this, audioStartSerial]() {
                                if (!consumePendingLegacyBridgeAudioStart(audioStartSerial,
                                                                          QStringLiteral("legacy-ptt-audio-only"))) {
                                    return;
                                }
                                syncLegacyBackendState();
                                if (!startBridgeAudioForLegacyDigitalTx(QStringLiteral("legacy-ptt-audio-only"))) {
                                    bridgeLog(QStringLiteral("legacyPttRequested: audio-only bridge TX failed, stopping legacy TX"));
                                    abortLegacyBridgeTxRequest(QStringLiteral("legacy-ptt-audio-only-failed"));
                                }
                            });
                            return;
                        }
#endif
                        bridgeLog(QStringLiteral("legacyPttRequested: active CAT unavailable, retrying connection"));
#if defined(Q_OS_MAC)
                        // Never reconnect CAT from the TX callback on macOS.
                        // The reconnect can serialize with the audio/render
                        // path and freeze the panadapter after the PTT
                        // confirmation timeout.  CAT recovery is already
                        // handled by the backend's normal reconnect path.
                        bridgeLog(QStringLiteral("legacyPttRequested: CAT unavailable; macOS TX path will not reconnect"));
                        return;
#else
                        if (enabled) {
                            retryRigConnection();
                            QTimer::singleShot(700, this, [this]() {
                                if (m_shuttingDown || !m_legacyBackend || !m_legacyBackend->transmitting()) {
                                    return;
                                }
                                if (activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
                                    bridgeLog(QStringLiteral("legacyPttRequested: applying delayed PTT after CAT reconnect"));
#if defined(Q_OS_MAC)
                                    if (shouldUseBridgeAudioForLegacyDigitalTx()
                                        && !preflightLegacyBridgeTxBeforePtt(QStringLiteral("legacy-ptt-delayed"))) {
                                        abortLegacyBridgeTxRequest(QStringLiteral("legacy-ptt-delayed-preflight"));
                                        return;
                                    }
#endif
                                    QElapsedTimer pttTimer;
                                    pttTimer.start();
                                    QElapsedTimer syncTimer;
                                    syncTimer.start();
                                    syncActiveCatTxSplitFrequency(QStringLiteral("legacy-ptt-delayed"));
                                    qint64 const syncMs = syncTimer.elapsed();
                                    double const txDialHz = catSplitTxPttDialFrequencyHz();
                                    QElapsedTimer catTimer;
                                    catTimer.start();
                                    bool const useAsyncPtt = shouldUseBridgeAudioForLegacyDigitalTx();
                                    bool const asyncPtt = useAsyncPtt
                                        && activeCatSetTxPttAsync(this, m_hamlibCat, m_catBackend,
                                                                  true, txDialHz,
                                                                  m_omniRigCat, m_legacyBackend);
                                    if (!asyncPtt) {
                                        activeCatSetTxPtt(this, m_hamlibCat, m_catBackend,
                                                          true, txDialHz,
                                                          m_omniRigCat, m_legacyBackend);
                                    }
                                    qint64 const catMs = catTimer.elapsed();
                                    int const audioDelayMs = asyncPtt ? kLegacyBridgeAudioAfterAsyncPttDelayMs : 0;
                                    txTimelineLog(QStringLiteral("[TX-TL] legacy_ptt reason=legacy-ptt-delayed on=1 total_ms=%1 sync_ms=%2 cat_ms=%3 backend=%4 txDialHz=%5 async=%6 audio_delay_ms=%7")
                                                  .arg(pttTimer.elapsed())
                                                  .arg(syncMs)
                                                  .arg(catMs)
                                                  .arg(m_catBackend)
                                                  .arg(txDialHz, 0, 'f', 0)
                                                  .arg(asyncPtt ? 1 : 0)
                                                  .arg(audioDelayMs));
#if defined(Q_OS_MAC)
                                    if (shouldUseBridgeAudioForLegacyDigitalTx()) {
                                        quint64 const audioStartSerial =
                                            armPendingLegacyBridgeAudioStart(QStringLiteral("legacy-ptt-delayed"));
                                        QTimer::singleShot(audioDelayMs, this, [this, audioStartSerial]() {
                                            if (!consumePendingLegacyBridgeAudioStart(audioStartSerial,
                                                                                      QStringLiteral("legacy-ptt-delayed"))) {
                                                return;
                                            }
                                            syncLegacyBackendState();
                                            if (!startBridgeAudioForLegacyDigitalTx(QStringLiteral("legacy-ptt-delayed"))) {
                                                bridgeLog(QStringLiteral("legacyPttRequested: delayed bridge TX audio failed, stopping legacy TX"));
                                                abortLegacyBridgeTxRequest(QStringLiteral("legacy-ptt-delayed-audio-failed"));
                                            }
                                        });
                                    }
#endif
                                } else {
                                    bridgeLog(QStringLiteral("legacyPttRequested ignored: no active CAT PTT backend after retry"));
                                }
                            });
                        }
#endif
                    }
                    if (!enabled && usingLegacyBackendForTx() && !useModernSpectrumFeedWithLegacy()) {
                        QTimer::singleShot(0, this, [this]() {
                            resumeRxAudioAfterTx(QStringLiteral("legacy-ptt-off"));
                            scheduleLegacyPcmSpectrumRearm(QStringLiteral("legacy-ptt-off"));
                        });
                    }
                });

        bridgeLog(legacyTxBackendRequested()
                      ? QStringLiteral("Legacy backend enabled for TX/Tune and special operation control")
                      : QStringLiteral("Legacy backend enabled for shared Setup/Time Sync UI"));

        m_legacyBackend->setRigControlEnabled(
            embeddedLegacyRigControlEnabledForBackend(m_catBackend, legacyTxBackendRequested()));
        if (QObject* cat = catManagerObj()) {
            syncCatSplitModeToLegacy(cat->property("splitMode").toString(),
                                     QStringLiteral("legacy-backend-init"));
        }

        // All'avvio manteniamo il device/channel audio salvato nel bridge, invece di
        // lasciare che il backend legacy riparta dal default di sistema.
        if (!m_audioInputDevice.isEmpty()) {
            m_legacyBackend->setAudioInputDeviceName(m_audioInputDevice);
        }
        if (!m_audioOutputDevice.isEmpty()) {
            m_legacyBackend->setAudioOutputDeviceName(m_audioOutputDevice);
        }
        m_legacyBackend->setAudioInputChannel(qBound(0, m_audioInputChannel, 3));
        m_legacyBackend->setAudioOutputChannel(qBound(0, m_audioOutputChannel, 3));
        m_legacyBackend->setDecodeDepthBits(legacyCompatibleDecodeDepthBits());
        m_legacyBackend->setCqOnly(false);
        bridgeLog(QStringLiteral("Legacy backend audio restored: inDev=%1 inChan=%2 outDev=%3 outChan=%4")
                      .arg(m_audioInputDevice,
                           QString::number(m_audioInputChannel),
                           m_audioOutputDevice,
                           QString::number(m_audioOutputChannel)));

        m_legacyBackend->setFt2DecodeEnabled(m_mode == QStringLiteral("FT2"));
        syncLegacyBackendDialogState();

        // Decodium3 espone un sender WSJT-X UDP quando e' il backend TX attivo.
        // Se il bridge moderno resta responsabile del QSO, il suo MessageClient
        // deve restare vivo per inoltrare LoggedADIF/QSOLogged ai log esterni.
        initUdpMessageClient();

        if (legacyTxBackendRequested() && m_monitoring && !m_transmitting && !m_tuning) {
            migrateActiveMonitoringToLegacyBackend();
        }
    }

    m_legacyBackend->setRigControlEnabled(
        embeddedLegacyRigControlEnabledForBackend(m_catBackend, legacyTxBackendRequested()));
    {
        QMutexLocker locker(&m_ft8MicroStallGuardMutex);
        m_legacyBackend->setFt8DeepThreadPenalty(m_ft8MicroStallGuard.active());
    }
    scheduleLegacyStateRefreshBurst();
#if DECODIUM_HAS_SSTV
    refreshSstvProducerTaps();
#endif

    return true;
}

#undef embeddedLegacyRigControlEnabledForBackend
#undef activeCatCanPtt
#undef activeCatUsesVoxPtt
#undef activeCatSetTxPtt
#undef activeCatSetTxPttAsync
#undef catSuppressedByEnvironment
#undef txTimelineLog
#undef bridgeLog

#define bridgeLog decodiumBridgeAudioLevelLog
void DecodiumBridge::initialiseMacLegacyStartup()
{
#if defined(Q_OS_MAC)
    auto const envFlagEnabled = [](const char* name) {
        if (!qEnvironmentVariableIsSet(name)) {
            return false;
        }
        QByteArray const value = qgetenv(name).trimmed().toLower();
        return value.isEmpty()
            || value == "1"
            || value == "true"
            || value == "yes"
            || value == "on";
    };
    bool const legacyTxOverride = envFlagEnabled("DECODIUM_MAC_LEGACY_TX_BACKEND");
    bool const forceStandaloneUdp = envFlagEnabled("DECODIUM_FORCE_STANDALONE_UDP");

    if (forceStandaloneUdp) {
        m_useLegacyTxBackend = false;
        bridgeLog(QStringLiteral("Legacy TX backend disabled by DECODIUM_FORCE_STANDALONE_UDP"));
    } else {
        m_useLegacyTxBackend = true;
        bridgeLog(legacyTxOverride
                  ? QStringLiteral("Legacy TX backend enabled by DECODIUM_MAC_LEGACY_TX_BACKEND")
                  : QStringLiteral("Legacy TX backend enabled by macOS default"));
        // The timer starts during construction, before the QML event loop.
        // If startup itself takes longer than three seconds, its callback can
        // otherwise construct the legacy window ahead of the main QML window.
        const auto initializeLegacy = [this]() {
            if (m_shuttingDown || !m_useLegacyTxBackend) {
                return;
            }
            if (!ensureLegacyBackendAvailable()) {
                m_useLegacyTxBackend = false;
                bridgeLog(QStringLiteral("Legacy RX backend unavailable: enabling the modern decoder fallback"));
                if (m_mainQmlReady || m_startupServicesStarted) {
                    ensureDecodeWorkerForMode(m_mode);
                }
            }
        };
        QTimer::singleShot(3000, this, [this, initializeLegacy]() {
            if (m_shuttingDown) {
                return;
            }
            if (m_mainQmlReady) {
                initializeLegacy();
                return;
            }
            connect(this, &DecodiumBridge::mainQmlReadyForNativeWindowing, this,
                    [this, initializeLegacy]() {
                        QTimer::singleShot(500, this, initializeLegacy);
                    }, Qt::SingleShotConnection);
        });
    }
#endif
    // Su Windows/Linux NON creare il legacy backend all'avvio — viene creato lazy
    // quando l'utente apre Settings/Setup (ensureLegacyBackendAvailable)

}
#undef bridgeLog
