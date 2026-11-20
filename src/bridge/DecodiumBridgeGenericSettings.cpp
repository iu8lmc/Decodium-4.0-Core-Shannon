#include "DecodiumBridge.h"
#include "SpecialOperationBounds.h"
#include "DecodiumProfileSettings.h"
#include "SecureSettings.hpp"
#include "CallsignIntelligenceService.h"
#include "Network/DecodiumCloudlogLite.h"
#include "Network/DecodiumPskReporterLite.h"
#include "Network/RemoteCommandServer.hpp"
#include "DecodiumLegacyBackend.h"
#include <QFile>
#include <QSettings>
#include <QTimer>

QString decodiumBridgeAliasedSettingKey(const QString&);
bool decodiumBridgeIsHamlibFamilyBackend(const QString&);
void decodiumBridgeDecodeFlowLog(const QString&);

static QString canonicalProfileServiceSettingKey(const QString& key)
{
    if (key == QStringLiteral("CloudlogStationID")) return QStringLiteral("CloudLogStationID");
    if (key == QStringLiteral("LoTWPassword")) return QStringLiteral("Lotw_pwd");
    if (key == QStringLiteral("LoTWNonQSL")) return QStringLiteral("NonQsl");
    if (key == QStringLiteral("LoTWDaysSinceUpload")) return QStringLiteral("LotWDaysSinceLastUpload");
    return key;
}

static QString legacyProfileServiceSettingKey(const QString& canonicalKey)
{
    if (canonicalKey == QStringLiteral("CloudLogStationID")) return QStringLiteral("CloudlogStationID");
    if (canonicalKey == QStringLiteral("Lotw_pwd")) return QStringLiteral("LoTWPassword");
    if (canonicalKey == QStringLiteral("NonQsl")) return QStringLiteral("LoTWNonQSL");
    if (canonicalKey == QStringLiteral("LotWDaysSinceLastUpload")) return QStringLiteral("LoTWDaysSinceUpload");
    return canonicalKey;
}

static bool isProfileServiceCompatibilitySetting(const QString& key)
{
    QString const canonicalKey = canonicalProfileServiceSettingKey(key);
    return canonicalKey == QStringLiteral("CloudLogStationID")
        || canonicalKey == QStringLiteral("Lotw_pwd")
        || canonicalKey == QStringLiteral("NonQsl")
        || canonicalKey == QStringLiteral("LotWDaysSinceLastUpload");
}

QVariant DecodiumBridge::getSetting(const QString& key, const QVariant& defaultValue) const
{
    // Startup reads this before a configuration/profile can be selected.
    if (key == QStringLiteral("uiScaleFactor")) {
        QSettings root(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        return root.value(key, defaultValue);
    }
    QVariant effectiveDefault = defaultValue;
    if (!effectiveDefault.isValid()
        && (key == QStringLiteral("ShowDXCC") || key == QStringLiteral("DXCCEntity"))) {
        effectiveDefault = true;
    }

    if (isProfileServiceCompatibilitySetting(key)) {
        QString const canonicalKey = canonicalProfileServiceSettingKey(key);
        QString const legacyKey = legacyProfileServiceSettingKey(canonicalKey);
        QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(s);

        QVariant storedValue = s.value(canonicalKey);
        if (!storedValue.isValid() && legacyKey != canonicalKey) {
            storedValue = s.value(legacyKey);
            if (storedValue.isValid()) {
                s.setValue(canonicalKey, storedValue);
            }
        }

        if (canonicalKey == QStringLiteral("Lotw_pwd")) {
            QString const password =
                secure_settings::load_or_import(&s,
                                                secure_settings::service(
                                                    m_callsign.trimmed().toUpper()),
                                                canonicalKey,
                                                storedValue.toString()).trimmed();
            if (legacyKey != canonicalKey) {
                s.remove(legacyKey);
            }
            s.sync();
            return password.isEmpty() ? effectiveDefault : QVariant(password);
        }

        if (legacyKey != canonicalKey && s.contains(legacyKey)) {
            s.remove(legacyKey);
        }
        s.sync();
        return storedValue.isValid() ? storedValue : effectiveDefault;
    }

    auto readBridgeSettingWithAlias = [](QSettings& settings, const QString& settingKey) -> QVariant {
        QVariant v = settings.value(settingKey);
        if (!v.isValid()) {
            QString const legacyKey = decodiumBridgeAliasedSettingKey(settingKey);
            if (legacyKey != settingKey) {
                v = settings.value(legacyKey);
            }
        }
        if (!v.isValid() && settingKey == QStringLiteral("PSKReporterTCPIP")) {
            v = settings.value(QStringLiteral("PSKRtcpip"));
        } else if (!v.isValid() && settingKey == QStringLiteral("PSKRtcpip")) {
            v = settings.value(QStringLiteral("PSKReporterTCPIP"));
        }
        return v;
    };

    if (key == QStringLiteral("PSKReporterTCPIP") || key == QStringLiteral("PSKRtcpip")) {
        // This switch belongs to the D4 reporting page.  Prefer QSettings so a
        // stale decodium4.ini legacy value cannot re-enable TCP/IP at startup.
        QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(s);
        QVariant value = readBridgeSettingWithAlias(s, key);
        if (!value.isValid()) {
            value = readSettingFromLegacyIni(key);
            if (!value.isValid()) {
                QString const legacyKey = decodiumBridgeAliasedSettingKey(key);
                if (legacyKey != key) {
                    value = readSettingFromLegacyIni(legacyKey);
                }
            }
        }
        return value.isValid() ? value : effectiveDefault;
    }

    if (key == QStringLiteral("PromptToLog") || key == QStringLiteral("AutoLog")) {
        auto readCanonicalSetting = [this](const QString& settingKey) -> QVariant {
            if (isLegacySyncKey(settingKey)) {
                QVariant v = readSettingFromLegacyIni(settingKey);
                if (v.isValid()) {
                    return v;
                }
                QString const legacyKey = decodiumBridgeAliasedSettingKey(settingKey);
                if (legacyKey != settingKey) {
                    v = readSettingFromLegacyIni(legacyKey);
                    if (v.isValid()) {
                        return v;
                    }
                }
            }

            QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
            decodium::beginActiveSettingsProfile(s);
            QVariant v = s.value(settingKey);
            if (!v.isValid()) {
                QString const legacyKey = decodiumBridgeAliasedSettingKey(settingKey);
                if (legacyKey != settingKey) {
                    v = s.value(legacyKey);
                }
            }
            return v;
        };

        QVariant const promptToLog = readCanonicalSetting(QStringLiteral("PromptToLog"));
        QVariant const autoLog = readCanonicalSetting(QStringLiteral("AutoLog"));
        bool const hasPromptToLog = promptToLog.isValid();
        bool const hasAutoLog = autoLog.isValid();
        bool promptEnabled = hasPromptToLog ? promptToLog.toBool() : false;
        bool autoLogEnabled = hasAutoLog ? autoLog.toBool() : !promptEnabled;
        if (promptEnabled && autoLogEnabled) {
            if (hasPromptToLog && !hasAutoLog) {
                autoLogEnabled = false;
            } else {
                promptEnabled = false;
            }
        } else if (!promptEnabled && !autoLogEnabled) {
            autoLogEnabled = true;
        }
        return key == QStringLiteral("PromptToLog") ? promptEnabled : autoLogEnabled;
    }

    // For settings owned by the legacy Configuration dialog, read the legacy
    // INI first because that is the canonical source used by Configuration.
    if (isLegacySyncKey(key)) {
        QVariant v = readSettingFromLegacyIni(key);
        if (v.isValid()) {
            return v;
        }
        QString const legacyKey = decodiumBridgeAliasedSettingKey(key);
        if (legacyKey != key) {
            v = readSettingFromLegacyIni(legacyKey);
            if (v.isValid()) {
                return v;
            }
        }
    }

    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    QVariant value = readBridgeSettingWithAlias(s, key);
    if (!value.isValid() && key == QStringLiteral("RemoteHttpPort")) {
        value = s.value(QStringLiteral("WebAppHttpPort"));
    }
    return value.isValid() ? value : effectiveDefault;
}

void DecodiumBridge::setSetting(const QString& key, const QVariant& value)
{
    if (key == QStringLiteral("uiScaleFactor")) {
        QSettings root(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        root.setValue(key, value);
        root.sync();
        emit settingValueChanged(key, value);
        return;
    }
    if (key == QStringLiteral("b4Strikethrough")) {
        setB4Strikethrough(value.toBool());
        return;
    }

    if (key == QStringLiteral("PskReporterTimeSpanMinutes")) {
        setPskReporterTimeSpanMinutes(value.toInt());
        return;
    }

    if (key == QStringLiteral("PskMapSpotWindowMinutes")) {
        setPskMapSpotWindowMinutes(value.toInt());
        return;
    }

    bool const isCatTelemetrySetting =
        key == QStringLiteral("PWRandSWR") || key == QStringLiteral("CheckSWR");
    bool const catTelemetrySettingChanged =
        isCatTelemetrySetting && getSetting(key, false).toBool() != value.toBool();

    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    if (isProfileServiceCompatibilitySetting(key)) {
        QString const canonicalKey = canonicalProfileServiceSettingKey(key);
        QString const legacyKey = legacyProfileServiceSettingKey(canonicalKey);
        QVariant storedValue = value;
        if (canonicalKey == QStringLiteral("Lotw_pwd")) {
            storedValue =
                secure_settings::value_for_write(
                    secure_settings::service(m_callsign.trimmed().toUpper()),
                    canonicalKey,
                    value.toString());
        }
        s.setValue(canonicalKey, storedValue);
        if (legacyKey != canonicalKey) {
            s.remove(legacyKey);
        }
        s.sync();

        if (canonicalKey == QStringLiteral("CloudLogStationID") && m_cloudlog) {
            m_cloudlog->setStationId(qBound(0, value.toInt(), 999));
        }
        if (canonicalKey == QStringLiteral("Lotw_pwd") && m_callsignIntelligence) {
            m_callsignIntelligence->setLotwPassword(value.toString());
        }
        emit settingValueChanged(canonicalKey, value);
        if (legacyKey != canonicalKey) {
            emit settingValueChanged(legacyKey, value);
        }
        return;
    }

    if (key == QStringLiteral("WorldMapDisplayed")) {
        m_worldMapDisplayed = value.toBool();
        if (m_worldMapDisplayed && worldMapConsumerReady()) {
            m_worldMapFullReplayDeferred = true;
            scheduleDeferredWorldMapFeedFlush(0);
        }
    }
    if (key == QStringLiteral("PromptToLog") || key == QStringLiteral("AutoLog")) {
        bool promptMode = false;
        if (key == QStringLiteral("PromptToLog")) {
            promptMode = value.toBool();
        } else {
            promptMode = !value.toBool();
        }

        bool const autoLogMode = !promptMode;
        s.setValue(QStringLiteral("PromptToLog"), promptMode);
        s.setValue(QStringLiteral("AutoLog"), autoLogMode);
        s.sync();

        syncSettingToLegacyIni(QStringLiteral("PromptToLog"), promptMode);
        syncSettingToLegacyIni(QStringLiteral("AutoLog"), autoLogMode);

        emit settingValueChanged(QStringLiteral("PromptToLog"), promptMode);
        emit settingValueChanged(QStringLiteral("AutoLog"), autoLogMode);
        return;
    }

    QString exclusiveLoggingKey;
    if (value.toBool()) {
        if (key == QStringLiteral("PromptToLog")) {
            exclusiveLoggingKey = QStringLiteral("AutoLog");
        } else if (key == QStringLiteral("AutoLog")) {
            exclusiveLoggingKey = QStringLiteral("PromptToLog");
        }
    }

    QString const legacyKey = decodiumBridgeAliasedSettingKey(key);
    if (key == QStringLiteral("MonitorOFF")) {
        s.setValue(QStringLiteral("MonitorOFF"), false);
        if (legacyKey != key) {
            s.setValue(legacyKey, false);
        }
        s.sync();
        syncSettingToLegacyIni(QStringLiteral("MonitorOFF"), false);
        if (!m_autoStartMonitorOnStartup) {
            m_autoStartMonitorOnStartup = true;
            emit autoStartMonitorOnStartupChanged();
        }
        decodiumBridgeDecodeFlowLog(QStringLiteral("setSetting: MonitorOFF forced to 0 autoStartMonitorOnStartup=1"));
        emit settingValueChanged(QStringLiteral("MonitorOFF"), false);
        if (legacyKey != key) {
            emit settingValueChanged(legacyKey, false);
        }
        return;
    }

    if (key == QStringLiteral("LowCpuMode")
        || key == QStringLiteral("lowCpuModeEnabled")) {
        setLowCpuModeEnabled(value.toBool());
        return;
    }

    if (key == QStringLiteral("TxWatchdog") || key == QStringLiteral("TXWatchdog")) {
        int const minutes = qBound(0, value.toInt(), 999);
        setTxWatchdogTime(minutes);
        setTxWatchdogMode(minutes > 0 ? 1 : 0);
        return;
    }
    if (key == QStringLiteral("txWatchdogMode")) {
        setTxWatchdogMode(value.toInt());
        return;
    }
    if (key == QStringLiteral("txWatchdogTime")) {
        setTxWatchdogTime(value.toInt());
        return;
    }
    if (key == QStringLiteral("txWatchdogCount")) {
        setTxWatchdogCount(value.toInt());
        return;
    }
    if (key == QStringLiteral("VHFUHF") || key == QStringLiteral("vhfUhfFeatures")) {
        setVhfUhfFeatures(value.toBool());
        return;
    }

    s.setValue(key, value);
    if (legacyKey != key) {
        s.setValue(legacyKey, value);
    }
    if (key == QStringLiteral("PSKReporterTCPIP") || key == QStringLiteral("PSKRtcpip")) {
        s.setValue(QStringLiteral("PSKReporterTCPIP"), value);
        s.setValue(QStringLiteral("PSKRtcpip"), value);
        syncSettingToLegacyIni(QStringLiteral("PSKReporterTCPIP"), value);
        syncSettingToLegacyIni(QStringLiteral("PSKRtcpip"), value);
    }
    if (key == QStringLiteral("RemoteWebEnabled") || key == QStringLiteral("WebAppEnabled")) {
        s.setValue(QStringLiteral("RemoteWebEnabled"), value);
        s.setValue(QStringLiteral("WebAppEnabled"), value);
        s.setValue(QStringLiteral("WebAppActive"), value.toBool() && m_remoteServer && m_remoteServer->isRunning());
    }
    if (!exclusiveLoggingKey.isEmpty()) {
        s.setValue(exclusiveLoggingKey, false);
        QString const exclusiveLegacyKey = decodiumBridgeAliasedSettingKey(exclusiveLoggingKey);
        if (exclusiveLegacyKey != exclusiveLoggingKey) {
            s.setValue(exclusiveLegacyKey, false);
        }
    }
    if (key == QStringLiteral("RemoteHttpPort")) {
        bool ok = false;
        int httpPort = value.toInt(&ok);
        if (ok && httpPort > 1) {
            int wsPort = httpPort - 1;
            s.setValue(QStringLiteral("RemoteWsPort"), wsPort);
            s.setValue(QStringLiteral("WebAppWsPort"), wsPort);
        }
    }
    if (key == QStringLiteral("CheckSWR") && value.toBool()) {
        s.setValue(QStringLiteral("PWRandSWR"), true);
    }
    if (legacyKey == QStringLiteral("HighDPI")) {
        if (value.toBool()) {
            QFile::remove(QStringLiteral("DisableHighDpiScaling"));
        } else {
            QFile f(QStringLiteral("DisableHighDpiScaling"));
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&f);
                out << QStringLiteral("DisableHighDpiScaling=\"true\"");
            }
        }
    }
    s.sync();
    if (key == QStringLiteral("alertSoundsEnabled") || key == QStringLiteral("alert_Enabled")) {
        bool const enabled = value.toBool();
        if (m_alertSoundsEnabled != enabled) {
            m_alertSoundsEnabled = enabled;
            emit alertSoundsEnabledChanged();
        }
    } else if (key == QStringLiteral("alertOnCq") || key == QStringLiteral("alert_CQ")) {
        bool const enabled = value.toBool();
        if (m_alertOnCq != enabled) {
            m_alertOnCq = enabled;
            emit alertOnCqChanged();
        }
    } else if (key == QStringLiteral("alertOnMyCall") || key == QStringLiteral("alert_MyCall")) {
        bool const enabled = value.toBool();
        if (m_alertOnMyCall != enabled) {
            m_alertOnMyCall = enabled;
            emit alertOnMyCallChanged();
        }
    } else if (key == QStringLiteral("AutoSpotEnabled")) {
        bool const enabled = value.toBool();
        if (m_autoSpotEnabled != enabled) {
            m_autoSpotEnabled = enabled;
            emit autoSpotEnabledChanged();
        }
        if (legacyBackendAvailable()) {
            m_legacyBackend->setAutoSpotEnabled(enabled);
        }
    } else if (key == QStringLiteral("SingleDecode")) {
        bool const enabled = value.toBool();
        if (m_singleDecode != enabled) {
            m_singleDecode = enabled;
            emit singleDecodeChanged();
        }
    } else if (key == QStringLiteral("PSKReporterTCPIP") || key == QStringLiteral("PSKRtcpip")) {
        bool const tcp = value.toBool();
        if (m_pskReporter) {
            m_pskReporter->setUseTcpIp(tcp);
        }
        emit statusMessage(tcp
                           ? QStringLiteral("PSK Reporter: transport TCP/IP")
                           : QStringLiteral("PSK Reporter: transport UDP/IP"));
    } else if (key == QStringLiteral("WaitPounceActive")) {
        bool const active = value.toBool();
        if (m_waitPounceActive != active) {
            m_waitPounceActive = active;
            if (!m_waitPounceActive) {
                m_lastWaitPounceKey.clear();
                m_lastWaitPounceMs = 0;
            } else if (!m_autoSeq) {
                setAutoSeq(true);
            }
            emit waitPounceActiveChanged();
        }
    } else if (key == QStringLiteral("DirectVisualAudioCaptureUnsafe")) {
        bool const directVisual = value.toBool();
        if (m_directVisualAudioCaptureUnsafe != directVisual) {
            m_directVisualAudioCaptureUnsafe = directVisual;
            applyDirectVisualAudioCaptureMode(QStringLiteral("settings"));
        }
    } else if (key == QStringLiteral("OpenGlGpuPanadapterFft")) {
        bool const enabled = value.toBool();
#ifdef Q_OS_LINUX
        if (m_openGlGpuPanadapterFftEnabled.load(std::memory_order_relaxed) != enabled)
            emit statusMessage(enabled
                ? QStringLiteral("OpenGL GPU panadapter FFT will be enabled after restarting Decodium")
                : QStringLiteral("OpenGL GPU panadapter FFT will be disabled after restarting Decodium"));
#else
        Q_UNUSED(enabled);
#endif
    } else if (key == QStringLiteral("CloudLogStationID")) {
        if (m_cloudlog) {
            m_cloudlog->setStationId(qBound(0, value.toInt(), 999));
        }
    }
    emit settingValueChanged(key, value);
    if (legacyKey != key) {
        emit settingValueChanged(legacyKey, value);
    }
    if (!exclusiveLoggingKey.isEmpty()) {
        emit settingValueChanged(exclusiveLoggingKey, false);
        QString const exclusiveLegacyKey = decodiumBridgeAliasedSettingKey(exclusiveLoggingKey);
        if (exclusiveLegacyKey != exclusiveLoggingKey) {
            emit settingValueChanged(exclusiveLegacyKey, false);
        }
    }
    if (key == QStringLiteral("CheckSWR") && value.toBool()) {
        emit settingValueChanged(QStringLiteral("PWRandSWR"), true);
        enforceSwrTransmissionLimit(QStringLiteral("setting-change"));
    }
    // Aggiorna il timer spectrum se cambia l'intervallo
    if (key == "spectrumInterval" && m_spectrumTimer) {
        applyLowCpuRuntimeProfile(QStringLiteral("spectrum-interval"));
    }
    if (key == QStringLiteral("NTPEnabled")
        || key == QStringLiteral("NTPCustomServer")
        || key == QStringLiteral("NTPOffset_ms")
        || key == QStringLiteral("DecoSyncSelfCalEnabled")) {
        applyNtpSettings();

        if (key == QStringLiteral("NTPEnabled")) {
            bool const enabled = value.toBool();
            emit statusMessage(enabled
                               ? QStringLiteral("NTP abilitato: sincronizzazione in corso...")
                               : QStringLiteral("NTP disabilitato"));
        } else if (key == QStringLiteral("NTPCustomServer")) {
            QString const server = value.toString().trimmed();
            emit statusMessage(server.isEmpty()
                               ? QStringLiteral("NTP: uso server pubblici automatici")
                               : QStringLiteral("NTP: server impostato su %1").arg(server));
        } else if (key == QStringLiteral("DecoSyncSelfCalEnabled")) {
            emit statusMessage(value.toBool()
                               ? QStringLiteral("Time sync RF self-calibration enabled")
                               : QStringLiteral("Time sync RF self-calibration disabled"));
        }
    }

    if (key == QStringLiteral("RtlSdrEnabled")) {
        m_rtlSdrEnabledCache.invalidate();
    }
    if (key.startsWith(QStringLiteral("RtlSdr"))) {
        // Coalesce QML field edits onto the next event-loop pass.  Changing a
        // frequency or gain never performs USB I/O synchronously in the UI.
        QTimer::singleShot(0, this, [this, key]() {
            applyRtlSdrSettings(QStringLiteral("setting %1").arg(key));
        });
    }

    // Sync legacy-owned keys to the legacy INI file so that the embedded
    // MainWindow / Configuration / MessageClient can see them.
    if (isLegacySyncKey(key)
        && key != QStringLiteral("PSKReporterTCPIP")
        && key != QStringLiteral("PSKRtcpip")) {
        syncSettingToLegacyIni(key, value);
        if (legacyKey != key) {
            syncSettingToLegacyIni(legacyKey, value);
        }
    }
    if (key == QStringLiteral("SelectedActivity")) {
        int const activity = qBound(kSpecialOpNone, value.toInt(), kSpecialOpMax);
        if (activity != m_specialOperationActivity
            || m_deferredSpecialOperationActivity >= kSpecialOpNone) {
            setSpecialOperationActivity(activity);
        }
    } else if (key == QStringLiteral("SpecialOpActivity")) {
        if (!value.toBool()
            && (m_specialOperationActivity != kSpecialOpNone
                || m_deferredSpecialOperationActivity > kSpecialOpNone)) {
            setSpecialOperationActivity(kSpecialOpNone);
        }
    } else if (key == QStringLiteral("SuperFox") && legacyBackendAvailable()) {
        syncSpecialOperationToLegacyBackend();
    }
    if (!exclusiveLoggingKey.isEmpty()) {
        syncSettingToLegacyIni(exclusiveLoggingKey, false);
        QString const exclusiveLegacyKey = decodiumBridgeAliasedSettingKey(exclusiveLoggingKey);
        if (exclusiveLegacyKey != exclusiveLoggingKey) {
            syncSettingToLegacyIni(exclusiveLegacyKey, false);
        }
    }

    static QSet<QString> const udpRuntimeKeys {
        QStringLiteral("UDPServer"),
        QStringLiteral("UDPServerPort"),
        QStringLiteral("UDPListenPort"),
        QStringLiteral("UDPClientId"),
        QStringLiteral("UDPSecondaryClientId"),
        QStringLiteral("UDPTertiaryClientId"),
        QStringLiteral("UDPPrimarySendDecode"),
        QStringLiteral("UDPPrimarySendStatus"),
        QStringLiteral("UDPPrimarySendQso"),
        QStringLiteral("UDPPrimarySendWspr"),
        QStringLiteral("UDPTTL"),
        QStringLiteral("AcceptUDPRequests"),
        QStringLiteral("UDPSecondaryEnabled"),
        QStringLiteral("UDPSecondaryServer"),
        QStringLiteral("UDPSecondaryServerPort"),
        QStringLiteral("UDPSecondaryInterface"),
        QStringLiteral("UDPSecondaryTTL"),
        QStringLiteral("UDPSecondaryQsoLoggedEnabled"),
        QStringLiteral("UDPSecondaryLoggedAdifEnabled"),
        QStringLiteral("UDPSecondarySendDecode"),
        QStringLiteral("UDPSecondarySendStatus"),
        QStringLiteral("UDPSecondarySendQso"),
        QStringLiteral("UDPSecondarySendWspr"),
        QStringLiteral("UDPTertiaryEnabled"),
        QStringLiteral("UDPTertiaryServer"),
        QStringLiteral("UDPTertiaryServerPort"),
        QStringLiteral("UDPTertiaryInterface"),
        QStringLiteral("UDPTertiaryTTL"),
        QStringLiteral("UDPTertiaryLoggedAdifEnabled"),
        QStringLiteral("UDPTertiarySendDecode"),
        QStringLiteral("UDPTertiarySendStatus"),
        QStringLiteral("UDPTertiarySendQso"),
        QStringLiteral("UDPTertiarySendWspr"),
    };
    if (udpRuntimeKeys.contains(key)) {
        scheduleUdpMessageClientRestart();
    }

    if (key == QStringLiteral("CATMode")) {
        applyConfiguredCatRigMode(QStringLiteral("setting-change"));
    }
    if (key == QStringLiteral("TXAudioSource")
        && decodiumBridgeIsHamlibFamilyBackend(m_catBackend)
        && m_hamlibCat
        && m_hamlibCat->connected()) {
        emit statusMessage(QStringLiteral("TX Audio Src: riconnessione CAT per applicare la sorgente audio"));
        QTimer::singleShot(0, this, [this]() { retryRigConnection(); });
    }
    if (catTelemetrySettingChanged
        && decodiumBridgeIsHamlibFamilyBackend(m_catBackend)
        && m_hamlibCat
        && m_hamlibCat->connected()
        && !m_catTelemetryReconnectPending) {
        m_catTelemetryReconnectPending = true;
        emit statusMessage(QStringLiteral("PWR/SWR: riconnessione CAT per applicare il polling"));
        QTimer::singleShot(0, this, [this]() {
            m_catTelemetryReconnectPending = false;
            if (m_shuttingDown
                || !decodiumBridgeIsHamlibFamilyBackend(m_catBackend)
                || !m_hamlibCat
                || !m_hamlibCat->connected()) {
                return;
            }
            retryRigConnection();
        });
    }
}
