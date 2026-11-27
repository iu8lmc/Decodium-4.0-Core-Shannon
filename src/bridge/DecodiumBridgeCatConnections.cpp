#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumSubManagers.h"
#include "DecodiumProfileSettings.h"
#include "Radio.hpp"
#include "Q65DopplerTracker.h"
#include <QDateTime>
#include <QScopeGuard>
#include <QSettings>
#include <QTimer>
#include <cmath>
#include <type_traits>

void decodiumBridgeDecodeFlowLog(const QString&);
bool decodiumBridgeLegacyRigControlFallback(DecodiumLegacyBackend*, const QString&);
bool decodiumBridgeIsNetworkRigctlCat(const QString&, const QObject*);
bool decodiumBridgePttSignalMatches(const QString&, const QString&);
QString decodiumBridgeCanonicalApplicationDecodeMode(QString);
#define bridgeLog decodiumBridgeDecodeFlowLog
#define useLegacyRigControlFallback decodiumBridgeLegacyRigControlFallback
#define isNetworkRigctlCat decodiumBridgeIsNetworkRigctlCat
#define catSignalMatchesBackend decodiumBridgePttSignalMatches
#define canonicalApplicationDecodeMode decodiumBridgeCanonicalApplicationDecodeMode

void DecodiumBridge::initialiseCatConnections()
{
    // Helper: legge una property QObject* in modo generico
    auto catProp = [this](const char* prop) -> QVariant {
        auto* obj = catManagerObj();
        return obj ? obj->property(prop) : QVariant{};
    };

    // BandManager → aggiorna frequenza sul radio quando si cambia banda
    connect(m_bandManager, &BandManager::bandFrequencyRequested, this, [this](double freq) {
        bool const monitorShouldStayActive = m_monitoring || m_monitorRequested;
        quint64 const monitorSessionId = m_periodTimerSessionId;
        double const previousFrequency = m_frequency;
        // requestRigFrequencyFromBridge() records the current dial as the
        // previous CAT value before it publishes the requested one.  Do not
        // call setFrequency() first: doing so made previous==target, so a
        // delayed CAT poll containing the old mode's dial was mistaken for a
        // manual retune and could undo FT4 -> FT8 (or the reverse).
        requestRigFrequencyFromBridge(freq, QStringLiteral("band/mode"));
        if (m_monitoring) {
            {
                QMutexLocker locker(&m_audioBufferMutex);
                m_audioBuffer.clear();
            }
            m_periodTicks = 0;
            m_lastPeriodSlot = -1;  // next onPeriodTimer tick re-anchors UTC slot
        }
        scheduleMonitorRecovery(QStringLiteral("band QSY %1 -> %2 Hz")
                                    .arg(QString::number(previousFrequency, 'f', 0),
                                         QString::number(freq, 'f', 0)),
                                monitorSessionId,
                                monitorShouldStayActive);
    });

    // Helper: collega i segnali di un manager alle property del bridge, con guard backend
    auto connectCatSignals = [this, catProp](auto* mgr, const QString& backend) {
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::frequencyChanged, this, [this, backend, catProp]() {
            if (m_shuttingDown) return;
            if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && backend == QStringLiteral("hamlib")) return;
            if (!catSignalMatchesBackend(m_catBackend, backend)) return;
            double const rigFrequency = catProp("frequency").toDouble();
            QString resolvedBand;
            Radio::FrequencyDelta resolvedOffsetHz = 0;
            double const f = logicalFrequencyFromCatDial(rigFrequency,
                                                         &resolvedBand,
                                                         &resolvedOffsetHz);
            if (m_q65Doppler && backend == QStringLiteral("hamlib") && f > 0.0
                && m_q65Doppler->consumeFrequencyReport(f, QDateTime::currentMSecsSinceEpoch()))
                return; // tracking CAT dial must not become the nominal/logged dial
            if (f > 0.0 && shouldIgnoreCatFrequencyDuringLocalQsy(f, backend)) {
                return;
            }
            if (f > 0.0) {
                m_currentCatLogicalBand = resolvedBand;
                m_currentCatStationOffsetHz = resolvedOffsetHz;
            }
            if (f > 0 && m_frequency != f) {
                bool const monitorShouldStayActive = m_monitoring || m_monitorRequested;
                quint64 const monitorSessionId = m_periodTimerSessionId;
                double const previousFrequency = m_frequency;
                // Non sovrascrivere la frequenza se l'abbiamo appena impostata noi
                // (il radio riporta la frequenza FT8 anche se abbiamo chiesto FT2)
                double diff = std::abs(f - m_frequency);
                if (diff > 50000.0) {
                    // Cambio banda dal radio — accetta
                    m_frequency = f; emit frequencyChanged();
                    m_bandManager->updateFromFrequency(f);
                    clearDecodeWindowsForBandChange(previousFrequency, f, QStringLiteral("rig-frequency"));
                    syncActiveCatTxSplitFrequency(QStringLiteral("rig-frequency"));
                    scheduleMonitorRecovery(QStringLiteral("rig QSY %1 -> %2 Hz")
                                                .arg(QString::number(previousFrequency, 'f', 0),
                                                     QString::number(f, 'f', 0)),
                                            monitorSessionId,
                                            monitorShouldStayActive);
                } else if (diff > 1.0) {
                    // Piccola variazione — aggiorna solo la frequenza, non la banda
                    m_frequency = f; emit frequencyChanged();
                    syncActiveCatTxSplitFrequency(QStringLiteral("rig-frequency"));
                }
            } else if (f > 0.0 && m_bandManager) {
                m_bandManager->updateFromFrequency(f);
            }
            if (f > 0.0) {
                maybeApplyStartupModeFromRigFrequency(f, true);
            }
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::modeChanged, this, [this, backend, catProp]() {
            if (m_shuttingDown) return;
            if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && backend == QStringLiteral("hamlib")) return;
            if (!catSignalMatchesBackend(m_catBackend, backend)) return;
            QString m = catProp("mode").toString();
            if (m_catMode != m) { m_catMode = m; emit catModeChanged(); }
            if (m_mode == QStringLiteral("SSB")) {
                QString const upper = m.trimmed().toUpper();
                QString ssbMode;
                if (upper == QStringLiteral("LSB") || upper == QStringLiteral("DIGL")
                    || upper == QStringLiteral("DATA-L") || upper == QStringLiteral("PKTLSB")) {
                    ssbMode = upper == QStringLiteral("LSB") ? QStringLiteral("LSB") : QStringLiteral("DIGL");
                } else if (upper == QStringLiteral("USB") || upper == QStringLiteral("DIGU")
                           || upper == QStringLiteral("DATA-U") || upper == QStringLiteral("PKTUSB")) {
                    ssbMode = upper == QStringLiteral("USB") ? QStringLiteral("USB") : QStringLiteral("DIGU");
                }
                if (!ssbMode.isEmpty() && m_ssbRadioMode != ssbMode) {
                    m_ssbRadioMode = ssbMode;
                    emit ssbStateChanged();
                }
            }
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::splitModeChanged, this, [this, backend, catProp]() {
            if (m_shuttingDown) return;
            if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && backend == QStringLiteral("hamlib")) return;
            if (!catSignalMatchesBackend(m_catBackend, backend)) return;
            QString const splitMode = catProp("splitMode").toString();
            syncCatSplitModeToLegacy(splitMode, QStringLiteral("cat-manager"));
            syncActiveCatTxSplitFrequency(QStringLiteral("split-mode-change"));
            emit displayFrequencyChanged();
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::connectedChanged, this, [this, backend, catProp]() {
            if (m_shuttingDown) {
                bridgeLog("CAT[" + backend + "] connectedChanged ignored during shutdown");
                return;
            }
            if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && backend == QStringLiteral("hamlib")) return;
            if (!catSignalMatchesBackend(m_catBackend, backend)) return;
            bool c = catProp("connected").toBool();
            if (!c && m_q65Doppler) m_q65Doppler->disconnected();
            bridgeLog("CAT[" + backend + "] connectedChanged: " + QString::number(c));
            if (m_diagnostics) {
                const QString rig = c
                    ? QStringLiteral("Connected · %1 · backend=%2")
                          .arg(catProp("rigName").toString(), backend)
                    : QStringLiteral("Disconnected · backend=%1").arg(backend);
                m_diagnostics->setRigState(rig);
            }
            if (m_catConnected != c) {
                if (!c) m_rttyRigModeState.clear();
                m_catConnected = c;
                m_catRigName = c ? catProp("rigName").toString() : QString();
                emit catConnectedChanged();
                emit catRigNameChanged();
                refreshPskReporterLocalStation();
                if (!c && m_tciAudioCaptureActive) {
                    // TCI audio is a network stream, not a local sound card.
                    // When CAT drops, stop it immediately and reopen the
                    // selected local input so RX remains observable to
                    // PulseAudio/PipeWire and the decoder does not stay on an
                    // empty TCI buffer.
                    bridgeLog(QStringLiteral("CAT[%1] disconnected: falling back from TCI audio to local RX input")
                                  .arg(backend));
                    stopAudioCapture();
                    if (m_monitoring) {
                        QTimer::singleShot(0, this, [this]() {
                            if (m_monitoring && !usingTciAudioInput()
                                && !m_transmitting && !m_tuning) {
                                startAudioCapture();
                            }
                        });
                    }
                }
                bool autoStart = catProp("audioAutoStart").toBool();
                if (c && autoStart && !m_monitoring)
                    QTimer::singleShot(300, this, [this]() { setMonitoring(true); });
                if (!c && (m_transmitting || m_tuning))
                    QTimer::singleShot(0, this, [this]() { halt(); });
            }

            bool const networkRigctlStartup = c
                && isNetworkRigctlCat(m_catBackend, catManagerObj());
            if (networkRigctlStartup) {
                m_networkRigctlStartupReadOnly = true;
                quint64 const startupSerial = ++m_networkRigctlStartupSerial;
                bridgeLog(QStringLiteral(
                    "CAT NET rigctl startup: read-only state adoption; mode/VFO/split writes deferred"));
                QTimer::singleShot(750, this, [this, startupSerial]() {
                    if (startupSerial != m_networkRigctlStartupSerial)
                        return;
                    m_networkRigctlStartupReadOnly = false;
                    if (m_shuttingDown || !m_catConnected
                        || !isNetworkRigctlCat(m_catBackend, m_hamlibCat)) {
                        return;
                    }
                    bridgeLog(QStringLiteral(
                        "CAT NET rigctl startup: state adopted; idle split replay remains suppressed"));
                    // Once the first rig state is known, a mode write is made
                    // only if the operator's configured mode truly differs.
                    // Fake/emulated split is intentionally prepared later by
                    // the TX path, not while an idle shared CAT connects.
                    if (!m_startupModeAutoPending)
                        applyConfiguredCatRigMode(QStringLiteral("network-rigctl-state-ready"));
                });
            } else {
                m_networkRigctlStartupReadOnly = false;
                ++m_networkRigctlStartupSerial;
            }
            if (c) {
                ++m_omniRigReconnectSerial;
                m_omniRigReconnectInProgress = false;
                m_lastSuccessfulCatConnected = true;
                m_lastSuccessfulCatBackend = m_catBackend;
                if (!m_lastCatError.isEmpty()) {
                    m_lastCatError.clear();
                    emit lastCatErrorChanged();
                }
                QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
                decodium::beginActiveSettingsProfile(s);
                s.setValue("lastSuccessfulCatConnected", true);
                s.setValue("lastSuccessfulCatBackend", m_catBackend);
            } else if (!m_shuttingDown) {
                m_lastSuccessfulCatConnected = false;
                m_lastSuccessfulCatBackend = m_catBackend;
                QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
                decodium::beginActiveSettingsProfile(s);
                s.setValue("lastSuccessfulCatConnected", false);
                s.setValue("lastSuccessfulCatBackend", m_catBackend);
                // 1.0.303 — auto-reconnect runtime OmniRig (feedback tester "OmniRig si
                // disconnette dopo inattività"). Distinguo un DROP (la disconnessione da
                // "rig non risponde" lascia m_lastCatError valorizzato) da una disconnessione
                // VOLONTARIA (utente preme Disconnetti → nessun errore → niente reconnect).
                // Burst di tentativi (riusa retryRigConnection), gated off durante TX/tune
                // (non toccare PTT/DTR) e a bassa cadenza (anti-stall, vedi connectRig blocking).
                if (backend == QStringLiteral("omnirig") && !m_lastCatError.isEmpty()) {
                    quint64 const reconnectSerial = ++m_omniRigReconnectSerial;
                    static constexpr int kOmniReconnectDelaysMs[] = {3000, 8000, 16000, 30000};
                    for (int const delayMs : kOmniReconnectDelaysMs) {
                        QTimer::singleShot(delayMs, this, [this, delayMs, reconnectSerial]() {
                            if (reconnectSerial != m_omniRigReconnectSerial) return;
                            if (m_shuttingDown || m_catConnected) return;
                            if (m_transmitting || m_tuning) return;
                            if (m_catBackend != QStringLiteral("omnirig")) return;
                            if (m_omniRigReconnectInProgress) {
                                bridgeLog(QStringLiteral("OmniRig auto-reconnect runtime: skip overlapping attempt (delay=%1ms)").arg(delayMs));
                                return;
                            }
                            m_omniRigReconnectInProgress = true;
                            auto reconnectGuard = qScopeGuard([this]() { m_omniRigReconnectInProgress = false; });
                            bridgeLog(QStringLiteral("OmniRig auto-reconnect runtime: tentativo (delay=%1ms)").arg(delayMs));
                            retryRigConnection();
                        });
                    }
                }
            }

            if (c) {
                auto pushTciStartupFrequency = [this]() {
                    if (m_catBackend != QStringLiteral("tci")
                        || !m_hamlibCat
                        || !m_hamlibCat->connected()
                        || m_frequency <= 0.0) {
                        return;
                    }

                    double const targetHz = m_frequency;
                    bridgeLog(QStringLiteral("TCI startup CAT sync: pushing Decodium frequency %1 Hz to SDR")
                                  .arg(QString::number(targetHz, 'f', 0)));
                    requestRigFrequencyFromBridge(targetHz, QStringLiteral("tci-startup-frequency"));
                };

                if (m_catBackend == QStringLiteral("tci")) {
                    pushTciStartupFrequency();
                    static constexpr int kTciStartupFrequencyPushRetryMs[] = {150, 450, 1000, 2000};
                    for (int const delayMs : kTciStartupFrequencyPushRetryMs) {
                        QTimer::singleShot(delayMs, this, pushTciStartupFrequency);
                    }
                }

                if (backend == QStringLiteral("hamlib")
                    && m_catBackend == QStringLiteral("tci")
                    && m_monitoring
                    && usingTciAudioInput()) {
                    QTimer::singleShot(150, this, [this]() {
                        if (m_catBackend == QStringLiteral("tci") && m_monitoring && usingTciAudioInput()) {
                            // A disconnected TCI profile may have fallen back
                            // to the local QAudioSource while CAT was down.
                            // Release it before handing RX back to the TCI
                            // stream, otherwise both paths remain armed.
                            if (m_soundInput) {
                                stopAudioCapture();
                            }
                            startTciAudioCapture();
                        }
                    });
                }
                // Releasing PTT only inside rig_open() is not sufficient for
                // rigs left in CW: restoring the configured CAT mode below
                // can briefly reassert the previous key/PTT state. Keep the
                // connection boundary RX-only before and after that restore.
                auto releaseStartupPtt = [this]() {
                    if (m_hamlibCat && m_hamlibCat->connected()) {
                        m_hamlibCat->setRigPtt(false);
                    }
                };
                releaseStartupPtt();
                // During startup the rig frequency may still determine the
                // application mode. Do not first apply the generic DATA-U
                // CAT mode and then switch to CW/SSB a moment later.
                if (!m_startupModeAutoPending)
                    applyConfiguredCatRigMode(QStringLiteral("connect"));
                releaseStartupPtt();
                QTimer::singleShot(300, this, [this]() {
                    if (!m_shuttingDown && m_hamlibCat && m_hamlibCat->connected()) {
                        m_hamlibCat->setRigPtt(false);
                    }
                });
                syncActiveCatTxSplitFrequency(QStringLiteral("connect"));
                auto applyCatStartupFrequency = [this, backend, catProp]() {
                    if (!catSignalMatchesBackend(m_catBackend, backend)) {
                        return;
                    }
                    // A delayed startup retry must not adopt the Doppler-shifted
                    // dial as a new nominal frequency after tracking is armed.
                    if (m_q65Doppler && (m_q65Doppler->enabled() || m_q65Doppler->applied())) {
                        return;
                    }
                    if (!catProp("connected").toBool()) {
                        return;
                    }
                    double const physicalRigFrequency = catProp("frequency").toDouble();
                    QString resolvedBand;
                    Radio::FrequencyDelta resolvedOffsetHz = 0;
                    double const rigFrequency = logicalFrequencyFromCatDial(physicalRigFrequency,
                                                                            &resolvedBand,
                                                                            &resolvedOffsetHz);
                    if (rigFrequency <= 0.0) {
                        return;
                    }
                    if (shouldIgnoreCatFrequencyDuringLocalQsy(rigFrequency, backend)) {
                        return;
                    }
                    bool const differsFromBridge =
                        !qFuzzyCompare(m_frequency + 1.0, rigFrequency + 1.0);
                    m_currentCatLogicalBand = resolvedBand;
                    m_currentCatStationOffsetHz = resolvedOffsetHz;
                    if (differsFromBridge || m_startupModeAutoPending) {
                        bridgeLog(QStringLiteral("startup CAT sync: using logical frequency %1 Hz "
                                                 "from %2 (rig=%3 Hz offset=%4 Hz)")
                                      .arg(QString::number(rigFrequency, 'f', 0),
                                           backend,
                                           QString::number(physicalRigFrequency, 'f', 0),
                                           QString::number(resolvedOffsetHz)));
                        setFrequency(rigFrequency);
                        maybeApplyStartupModeFromRigFrequency(rigFrequency, true);
                        syncActiveCatTxSplitFrequency(QStringLiteral("startup-frequency"));
                    } else if (m_bandManager) {
                        m_bandManager->updateFromFrequency(rigFrequency);
                    }
                };

                applyCatStartupFrequency();
                static constexpr int kStartupCatSyncRetryMs[] = {75, 200, 500};
                for (int const delayMs : kStartupCatSyncRetryMs) {
                    QTimer::singleShot(delayMs, this, applyCatStartupFrequency);
                }
                QTimer::singleShot(250, this, [this]() {
                    if (!m_startupModeAutoPending)
                        applyConfiguredCatRigMode(QStringLiteral("connect-retry"));
                    syncActiveCatTxSplitFrequency(QStringLiteral("connect-retry"));
                });
                if (m_catBackend == QStringLiteral("tci") && !canonicalApplicationDecodeMode(m_mode).isEmpty()) {
                    QTimer::singleShot(150, this, [this]() {
                        if (m_catBackend == QStringLiteral("tci") && m_hamlibCat && m_hamlibCat->connected()) {
                            if (configuredCatRigMode().isEmpty()) {
                                m_hamlibCat->setRigMode(m_mode);
                            } else {
                                applyConfiguredCatRigMode(QStringLiteral("tci-connect"));
                            }
                            syncActiveCatTxSplitFrequency(QStringLiteral("tci-connect"));
                        }
                    });
                }
            }
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::pttActiveChanged,
                this, [this, backend, catProp]() {
            if (m_shuttingDown || !catSignalMatchesBackend(m_catBackend, backend)) return;
            handleActivePttFeedback(catProp("pttActive").toBool(), backend);
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::errorOccurred, this, [this, backend](const QString& msg) {
            if (m_shuttingDown) return;
            if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && backend == QStringLiteral("hamlib")) return;
            if (!catSignalMatchesBackend(m_catBackend, backend)) return;
            if (m_suppressCatErrors) { bridgeLog("CAT[" + backend + "] error suppressed: " + msg); return; }
            bridgeLog("CAT[" + backend + "] error: " + msg);
            if (m_lastCatError != msg) {
                m_lastCatError = msg;
                emit lastCatErrorChanged();
            }
            emit errorMessage(msg);
        });
        connect(mgr, &std::remove_pointer_t<decltype(mgr)>::statusUpdate, this, [this, backend](const QString& msg) {
            if (m_shuttingDown) return;
            if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && backend == QStringLiteral("hamlib")) return;
            if (!catSignalMatchesBackend(m_catBackend, backend)) return;
            bridgeLog("CAT[" + backend + "] status: " + msg);
            emit statusMessage(msg);
        });
    };
    connectCatSignals(m_cat4OmCat, "cat4om");
    connectCatSignals(m_omniRigCat, "omnirig");
    connectCatSignals(m_hamlibCat, "hamlib");
}
