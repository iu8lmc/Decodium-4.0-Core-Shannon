#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumProfileSettings.h"
#include "DecodiumDxCluster.h"
#include "DecodiumDiagnostics.h"
#include "Network/DecodiumPskReporterLite.h"
#include "UsStateDataManager.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

bool decodiumBridgeCatSuppressedForTx();
bool decodiumBridgeLegacyOwnsRigControl(DecodiumLegacyBackend*);
bool decodiumBridgeLegacyRigControlFallback(DecodiumLegacyBackend*, const QString&);
void decodiumBridgeAudioLevelLog(const QString&);
#define catSuppressedByEnvironment decodiumBridgeCatSuppressedForTx
#define legacyOwnsRigControl decodiumBridgeLegacyOwnsRigControl
#define useLegacyRigControlFallback decodiumBridgeLegacyRigControlFallback
#define beginConfiguredBridgeSettingsGroup decodium::beginActiveSettingsProfile
#define bridgeLog decodiumBridgeAudioLevelLog

void DecodiumBridge::runPostQmlStartupServices()
{
    if (m_startupServicesStarted) {
        return;
    }
    m_startupServicesStarted = true;
    bridgeLog(QStringLiteral("startup services: starting after Main.qml ready"));

    // FT2 Log Bridge: se c'e' una copia configurata e non gira gia',
    // parte insieme a Decodium (ricerca e avvio fuori dal thread UI).
    QTimer::singleShot(1500, this, [this]() {
        if (!m_shuttingDown) {
            launchFt2LogBridge(true);
        }
    });

    if (!legacyBackendRequestedForRx()) {
        ensureDecodeWorkerForMode(m_mode);
    } else {
        bridgeLog(QStringLiteral("decoder worker startup delegated to legacy RX backend: mode=%1 state=%2")
                      .arg(m_mode,
                           legacyBackendAvailable() ? QStringLiteral("ready")
                                                    : QStringLiteral("pending")));
    }

    // These tasks do file I/O and parsing on the global low-priority pool.
    // Starting them after the first QML frame keeps startup interactive while
    // still completing enrichment before the first normal decode period.
    loadDxccLookupAsync();
    QTimer::singleShot(900, this, [this]() {
        if (!m_shuttingDown) {
            loadWorldMapCall3Cache();
        }
    });

    scheduleAudioDeviceRefresh(1800, false);
    QTimer::singleShot(8000, this, [this]() {
        if (!m_shuttingDown && !m_audioDeviceCacheValid) {
            scheduleAudioDeviceRefresh(0, false);
        }
    });

    QTimer::singleShot(2200, this, [this]() {
        if (m_shuttingDown) {
            return;
        }
#if defined(Q_OS_MAC)
        if (m_useLegacyTxBackend && ensureLegacyBackendAvailable()) {
            bridgeLog(QStringLiteral("UDP WSJT-X delegated to legacy backend"));
            initUdpMessageClient();
            return;
        }
#endif
        initUdpMessageClient();
    });

    QTimer::singleShot(1500, this, [this]() {
        if (m_shuttingDown || !m_pskReporter) {
            return;
        }
        refreshPskReporterLocalStation();
        m_pskReporter->setEnabled(m_pskReporterEnabled);
        bridgeLog(QStringLiteral("PSK Reporter startup stage: enabled=%1")
                      .arg(m_pskReporterEnabled ? 1 : 0));
    });

    QTimer::singleShot(2500, this, [this]() {
        if (m_shuttingDown) {
            return;
        }
        if (m_showUsState && m_usStateData) {
            m_usStateData->ensureLoadedAsync();
        }
    });

    QTimer::singleShot(800, this, [this]() {
        if (m_shuttingDown) {
            return;
        }
        updateLotwUsers();
    });

    QTimer::singleShot(3000, this, [this]() {
        if (m_shuttingDown || !m_dxCluster || m_callsign.isEmpty()) {
            return;
        }
        m_dxCluster->setCallsign(m_callsign);
        QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        beginConfiguredBridgeSettingsGroup(s);
        bool autoConnect = s.value(QStringLiteral("DXCluster/autoConnect"), false).toBool();
        if (!autoConnect) {
            autoConnect = s.value(QStringLiteral("DXClusterAutoConnect"), false).toBool();
        }
        if (autoConnect) {
            bridgeLog(QStringLiteral("DX Cluster auto-connect: %1:%2")
                          .arg(m_dxCluster->host())
                          .arg(m_dxCluster->port()));
            m_dxCluster->connectCluster();
        }
    });

    bool const autoConn = (m_catBackend == QStringLiteral("cat4om")) ? m_cat4OmCat->catAutoConnect()
                        : (m_catBackend == QStringLiteral("omnirig")) ? m_omniRigCat->catAutoConnect()
                                                                      : m_hamlibCat->catAutoConnect();
    // Auto Connect is the explicit preference; lastSuccessfulCatConnected
    // preserves the common "close while connected, reopen connected" workflow.
    QSettings catLastSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(catLastSettings);
    bool const lastSuccess = catLastSettings.value(QStringLiteral("lastSuccessfulCatConnected"), false).toBool();
    QString const lastBackend = catLastSettings.value(QStringLiteral("lastSuccessfulCatBackend")).toString();
    bool const sameBackend = (lastBackend == m_catBackend);
    bool const retryLastSuccessfulCat = lastSuccess && sameBackend;
    bool const startupCatSuppressed = catSuppressedByEnvironment();
    bool const startupCatRequested = !startupCatSuppressed && (autoConn || retryLastSuccessfulCat);
    bridgeLog(QStringLiteral("CAT[%1] autoConnect=%2 retryLastSuccessful=%3 suppressedByEnv=%4")
                  .arg(m_catBackend)
                  .arg(autoConn ? 1 : 0)
                  .arg(retryLastSuccessfulCat ? 1 : 0)
                  .arg(startupCatSuppressed ? 1 : 0));
    if (startupCatSuppressed && (autoConn || retryLastSuccessfulCat)) {
        bridgeLog(QStringLiteral("CAT startup auto-connect suppressed by environment"));
    }

    if ((legacyOwnsRigControl(m_legacyBackend) || useLegacyRigControlFallback(m_legacyBackend, m_catBackend)) && startupCatRequested) {
        bridgeLog(QStringLiteral("CAT auto-connect delegated to legacy backend"));
        QTimer::singleShot(250, this, [this]() {
            if (!m_legacyBackend || !m_legacyBackend->available()) {
                return;
            }
            m_legacyBackend->retryRigConnection();
            syncLegacyBackendTxState();
            scheduleLegacyStateRefreshBurst();
        });
    } else if (startupCatRequested) {
        m_startupCatRetryCount = 0;
        // Windows can expose COM ports late after USB/driver startup. HRD can
        // also hold the first network probe until its 75s watchdog expires, so
        // keep at least one retry beyond that window.
        static constexpr int kStartupCatRetryDelaysMs[] = {
            1000, 3500, 7000, 12000, 20000, 35000, 60000, 90000
        };
        constexpr int kStartupCatRetryAttemptCount =
            int(sizeof(kStartupCatRetryDelaysMs) / sizeof(kStartupCatRetryDelaysMs[0]));
        int attempt = 0;
        for (int const delayMs : kStartupCatRetryDelaysMs) {
            ++attempt;
            QTimer::singleShot(delayMs, this, [this, attempt, delayMs]() {
                if (m_shuttingDown || m_catConnected) return;
                if (m_startupCatRetryCount >= attempt) return;
                m_startupCatRetryCount = attempt;
                bridgeLog(QStringLiteral("CAT startup reconnect: attempt %1/%2 on %3 delay=%4ms")
                              .arg(attempt)
                              .arg(kStartupCatRetryAttemptCount)
                              .arg(m_catBackend)
                              .arg(delayMs));
                retryRigConnection();
            });
        }
    }

    if (m_autoStartMonitorOnStartup && m_mode != QStringLiteral("Echo")) {
        int const startupMonitorDelayMs = usingLegacyBackendForTx() ? 2800 : 1200;
        QTimer::singleShot(startupMonitorDelayMs, this, [this]() {
            if (QStandardPaths::isTestModeEnabled()
                && QCoreApplication::instance()
                && QCoreApplication::instance()->property("decodiumLabNoMonitor").toBool()) {
                qInfo() << "[LAB] startup auto-monitor suppressed by --lab-no-monitor";
                return;
            }
            if (!m_autoStartMonitorOnStartup || m_mode == QStringLiteral("Echo")
                || m_monitoring || m_transmitting || m_tuning) {
                bridgeLog(QStringLiteral("startup auto-monitor: skipped autoStart=%1 mode=%2 monitoring=%3 tx=%4 tune=%5")
                              .arg(m_autoStartMonitorOnStartup ? 1 : 0)
                              .arg(m_mode)
                              .arg(m_monitoring ? 1 : 0)
                              .arg(m_transmitting ? 1 : 0)
                              .arg(m_tuning ? 1 : 0));
                return;
            }
            bridgeLog(QStringLiteral("startup auto-monitor: refreshing audio devices before monitor"));
            refreshAudioDevicesInternal(QStringLiteral("startup auto-monitor preflight"),
                                        true,
                                        false);
            bridgeLog(QStringLiteral("startup auto-monitor: enabling monitor after Main.qml ready"));
            setMonitoring(true);
        });
    } else {
        bridgeLog(QStringLiteral("startup auto-monitor: disabled autoStart=%1 mode=%2")
                      .arg(m_autoStartMonitorOnStartup ? 1 : 0)
                      .arg(m_mode));
    }

    // A suspend/resume cycle can invalidate CoreAudio/PipeWire handles and
    // leave a USB CAT endpoint temporarily absent.  Treat resume as a
    // lifecycle boundary: refresh audio after the device graph settles, then
    // retry CAT only when the previous session was known-good.  This does not
    // override an explicit user disconnect.
    if (auto *guiApp = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        connect(guiApp, &QGuiApplication::applicationStateChanged, this,
                [this](Qt::ApplicationState state) {
            if (m_shuttingDown) return;
            if (state == Qt::ApplicationSuspended) {
                bridgeLog(QStringLiteral("lifecycle: application suspended; preserving radio/audio state"));
                if (m_diagnostics) {
                    m_diagnostics->setAudioState(QStringLiteral("Suspended; audio devices will be refreshed on resume"));
                }
                return;
            }
            if (state != Qt::ApplicationActive) return;

            bridgeLog(QStringLiteral("lifecycle: application resumed; scheduling audio/CAT recovery"));
            scheduleAudioDeviceRefresh(1200, true);
            QTimer::singleShot(1800, this, [this]() {
                if (m_shuttingDown || m_catConnected || m_transmitting || m_tuning
                    || !m_lastSuccessfulCatConnected) {
                    return;
                }
                bridgeLog(QStringLiteral("lifecycle: CAT recovery retry after resume"));
                retryRigConnection();
            });
        });
    }
}

#undef catSuppressedByEnvironment
#undef legacyOwnsRigControl
#undef useLegacyRigControlFallback
#undef beginConfiguredBridgeSettingsGroup
#undef bridgeLog
