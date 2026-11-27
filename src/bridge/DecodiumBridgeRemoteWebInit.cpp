#include "DecodiumBridge.h"
#include "Network/RemoteCommandServer.hpp"
#include "SecureSettings.hpp"
#include <QProcessEnvironment>
#include <QSettings>
#include <QHostAddress>
#include <QDateTime>

void decodiumBridgeDecodeFlowLog(const QString&);
QString decodiumBridgeResolveSecureSetting(QString const&, QString const&, QStringList);
#define bridgeLog decodiumBridgeDecodeFlowLog
#define resolveSecureSettingFromProfiles decodiumBridgeResolveSecureSetting

void DecodiumBridge::initialiseRemoteWebConsole()
{
    // Remote Web Console (webapp)
    {
        auto const env = QProcessEnvironment::systemEnvironment();
        QSettings webSettings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        // Env vars hanno priorita', fallback sui setting canonici Remote*
        // e sugli alias storici WebApp*.
        bool const webAppEnabledInSettings =
            getSetting(QStringLiteral("RemoteWebEnabled"),
                       webSettings.value(QStringLiteral("WebAppEnabled"), false)).toBool();
        auto wsPortText = env.value(QStringLiteral("FT2_REMOTE_WS_PORT"));
        if (wsPortText.isEmpty() && webAppEnabledInSettings)
            wsPortText = getSetting(QStringLiteral("RemoteWsPort"),
                                    webSettings.value(QStringLiteral("WebAppWsPort"))).toString();
        if (wsPortText.isEmpty() && webAppEnabledInSettings) {
            bool httpOk = false;
            int const httpPort = getSetting(QStringLiteral("RemoteHttpPort"),
                                            webSettings.value(QStringLiteral("WebAppHttpPort"), 19091)).toInt(&httpOk);
            wsPortText = QString::number(httpOk && httpPort > 1 ? httpPort - 1 : 19090);
        }
        if (!wsPortText.isEmpty()) {
            bool wsPortOk = false;
            auto const wsPort = wsPortText.toUInt(&wsPortOk);
            if (wsPortOk && wsPort > 0 && wsPort <= 65535u) {
                auto bindText = env.value(QStringLiteral("FT2_REMOTE_WS_BIND"));
                if (bindText.isEmpty()) bindText = getSetting(QStringLiteral("RemoteWsBind"),
                                                              webSettings.value(QStringLiteral("WebAppBind"), QStringLiteral("127.0.0.1"))).toString();
                QHostAddress bindAddress(bindText);
                auto authUser = env.value(QStringLiteral("FT2_REMOTE_WS_USER"));
                if (authUser.isEmpty()) authUser = getSetting(QStringLiteral("RemoteUser"),
                                                              webSettings.value(QStringLiteral("WebAppUser"), QStringLiteral("admin"))).toString();
                auto authToken = env.value(QStringLiteral("FT2_REMOTE_WS_TOKEN"));
                if (authToken.isEmpty()) authToken = getSetting(QStringLiteral("RemoteToken"),
                                                                webSettings.value(QStringLiteral("WebAppToken"))).toString();
                {
                    QStringList secureProfiles;
                    secureProfiles << m_callsign.trimmed().toUpper();
                    secureProfiles << readSettingFromLegacyIni(QStringLiteral("MyCall")).toString().trimmed().toUpper();
                    secureProfiles << webSettings.value(QStringLiteral("MyCall")).toString().trimmed().toUpper();
                    QSettings secureIni(legacyIniPath(), QSettings::IniFormat);
                    authToken = secure_settings::load_or_import(&secureIni,
                                                                secure_settings::service(secureProfiles.value(0)),
                                                                QStringLiteral("RemoteToken"),
                                                                authToken).trimmed();
                    if (authToken.isEmpty()) {
                        authToken = resolveSecureSettingFromProfiles(QStringLiteral("RemoteToken"),
                                                                     getSetting(QStringLiteral("RemoteToken"),
                                                                                webSettings.value(QStringLiteral("WebAppToken"))).toString(),
                                                                     secureProfiles);
                    }
                    bridgeLog(QStringLiteral("Remote Web token resolved length=%1").arg(authToken.size()));
                }
                bool const remoteExposed = (bindAddress != QHostAddress::LocalHost && bindAddress != QHostAddress(QStringLiteral("::1")));
                if (remoteExposed && authToken.size() < 12) {
                    bridgeLog("Remote Web disabled: token must be at least 12 characters on LAN/WAN bind.");
                } else {
                    m_remoteServer = new RemoteCommandServer(this);
                    m_remoteServer->setRuntimeStateProvider([this]() -> RemoteCommandServer::RuntimeState {
                        RemoteCommandServer::RuntimeState s;
                        s.mode = m_mode;
                        s.band = m_bandManager ? m_bandManager->currentBandLambda() : QString();
                        if (s.band == QStringLiteral("--")) s.band.clear();
                        s.dialFrequencyHz = static_cast<qint64>(m_frequency);
                        s.rxFrequencyHz = m_rxFrequency;
                        s.txFrequencyHz = m_txFrequency;
                        s.periodMs = qMax<qint64>(1, periodMsForMode(m_mode));
                        s.txEnabled = m_txEnabled;
                        s.autoCqEnabled = m_autoCqRepeat > 0;
                        s.autoSpotEnabled = m_autoSpotEnabled;
                        s.asyncL2Enabled = m_asyncDecodeEnabled;
                        s.dualCarrierEnabled = m_dualCarrierEnabled;
                        s.alt12Enabled = m_alt12Enabled;
                        s.manualTxEnabled = m_manualTxHold;
                        s.quickQsoEnabled = m_quickQsoEnabled;
                        s.monitoring = m_monitoring;
                        s.transmitting = m_transmitting;
                        s.myCall = m_callsign;
                        s.dxCall = m_dxCall;
                        s.nowUtcMs = QDateTime::currentMSecsSinceEpoch();
                        return s;
                    });
                    m_remoteServer->setAuthUser(authUser);
                    if (!authToken.isEmpty()) m_remoteServer->setAuthToken(authToken);

                    auto guardText = env.value(QStringLiteral("FT2_REMOTE_GUARD_PRE_MS"));
                    if (guardText.isEmpty()) guardText = webSettings.value(QStringLiteral("WebAppGuardMs"), QStringLiteral("300")).toString();
                    bool guardOk = false;
                    auto const guardMs = guardText.toInt(&guardOk);
                    if (guardOk) m_remoteServer->setGuardPreMs(guardMs);
                    auto ageText = env.value(QStringLiteral("FT2_REMOTE_MAX_AGE_MS"));
                    if (ageText.isEmpty()) ageText = webSettings.value(QStringLiteral("WebAppMaxAgeMs"), QStringLiteral("7500")).toString();
                    bool ageOk = false;
                    auto const maxAgeMs = ageText.toInt(&ageOk);
                    if (ageOk) m_remoteServer->setMaxCommandAgeMs(maxAgeMs);

                    quint16 httpPort = 0;
                    auto httpPortText = env.value(QStringLiteral("FT2_REMOTE_HTTP_PORT"));
                    if (httpPortText.isEmpty()) httpPortText = getSetting(QStringLiteral("RemoteHttpPort"),
                                                                          webSettings.value(QStringLiteral("WebAppHttpPort"))).toString();
                    if (!httpPortText.isEmpty()) {
                        bool httpOk = false;
                        auto const hp = httpPortText.toUInt(&httpOk);
                        if (httpOk && hp <= 65535u) httpPort = static_cast<quint16>(hp);
                    }

                    // Collega comandi remoti al bridge
                    connect(m_remoteServer, &RemoteCommandServer::setModeRequested, this, [this](const QString& commandId, const QString& mode) {
                        bridgeLog(QStringLiteral("Remote command %1: set_mode %2").arg(commandId, mode));
                        setMode(mode.trimmed().toUpper());
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setBandRequested, this, [this](const QString& commandId, const QString& band) {
                        bridgeLog(QStringLiteral("Remote command %1: set_band %2").arg(commandId, band));
                        applyRemoteBandChange(band);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::sendCwRequested, this, [this](const QString& commandId, const QString& text, qint64 dialFrequencyHz, int wpm) {
                        bridgeLog(QStringLiteral("Remote command %1: send_cw [%2] dial=%3 wpm=%4")
                                      .arg(commandId, text).arg(dialFrequencyHz).arg(wpm));
                        sendCwAudio(text, dialFrequencyHz, wpm);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setDialFrequencyRequested, this, [this](const QString& commandId, qint64 hz) {
                        bridgeLog(QStringLiteral("Remote command %1: set_dial_frequency %2 Hz").arg(commandId, QString::number(hz)));
                        applyRemoteDialFrequency(static_cast<double>(hz), QStringLiteral("remote-command"));
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setRxFrequencyRequested, this, [this](const QString& commandId, int f) {
                        bridgeLog(QStringLiteral("Remote command %1: set_rx_frequency %2 Hz").arg(commandId, QString::number(f)));
                        setRxFrequency(f);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setTxFrequencyRequested, this, [this](const QString& commandId, int f) {
                        bridgeLog(QStringLiteral("Remote command %1: set_tx_frequency %2 Hz").arg(commandId, QString::number(f)));
                        setTxFrequency(f);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setTxEnabledRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_tx_enabled %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setTxEnabled(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::sendTxRequested, this, [this](const QString& commandId, int slot) {
                        bridgeLog(QStringLiteral("Remote command %1: send_tx TX%2").arg(commandId, QString::number(slot)));
                        sendTx(slot);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::stopTxRequested, this, [this](const QString& commandId) {
                        bridgeLog(QStringLiteral("Remote command %1: stop_tx").arg(commandId));
                        setTxEnabled(false);
                        stopTx();
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setMonitoringRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_monitoring %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setMonitoring(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setAutoCqRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_auto_cq %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setAutoCqRepeat(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setAutoSpotRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_auto_spot %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setAutoSpotEnabled(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setAsyncL2Requested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_async_l2 %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setAsyncDecodeEnabled(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setDualCarrierRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_dual_carrier %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setDualCarrierEnabled(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setAlt12Requested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_alt_12 %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setAlt12Enabled(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setManualTxRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_manual_tx %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        if (e) {
                            engageManualTxHold(QStringLiteral("remote-web"), false);
                        } else {
                            clearManualTxHold(QStringLiteral("remote-web"));
                        }
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setQuickQsoRequested, this, [this](const QString& commandId, bool e) {
                        bridgeLog(QStringLiteral("Remote command %1: set_quick_qso %2").arg(commandId, e ? QStringLiteral("1") : QStringLiteral("0")));
                        setQuickQsoEnabled(e);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::setFt2QsoMessageCountRequested, this, [this](const QString& commandId, int count) {
                        bridgeLog(QStringLiteral("Remote command %1: set_ft2_qso_msg_count %2").arg(commandId, QString::number(count)));
                        setQuickQsoEnabled(count == 2);
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::waterfallStreamingChanged, this, [](bool enabled) {
                        bridgeLog(enabled
                                  ? QStringLiteral("Remote waterfall stream enabled")
                                  : QStringLiteral("Remote waterfall stream disabled"));
                    }, Qt::QueuedConnection);
                    connect(m_remoteServer, &RemoteCommandServer::logMessage, this, [](const QString& msg) { bridgeLog("Remote: " + msg); }, Qt::QueuedConnection);

                    if (!m_remoteServer->start(static_cast<quint16>(wsPort), bindAddress, httpPort)) {
                        bridgeLog("Remote WS disabled: failed to bind " + bindAddress.toString() + ":" + QString::number(wsPort));
                        webSettings.setValue(QStringLiteral("WebAppActive"), false);
                    } else {
                        bridgeLog("Remote Web Console running: ws://" + bindAddress.toString() + ":" + QString::number(m_remoteServer->wsPort())
                                  + "  http://" + bindAddress.toString() + ":" + QString::number(m_remoteServer->httpPort()));
                        webSettings.setValue(QStringLiteral("WebAppActive"), true);
                        clearRemoteActivityCache(false);
                        publishRemoteActivityEntries(m_decodeList);
                        publishRemoteActivityEntries(m_rxDecodeList);
                    }
                }
            }
        }
    }

}
