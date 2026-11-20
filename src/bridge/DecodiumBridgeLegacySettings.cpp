#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include <QDir>
#include <QSet>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

extern void decodiumBridgeLogbookLog(const QString&);
extern QString decodiumBridgeAliasedSettingKey(const QString&);

QString DecodiumBridge::legacyIniPath() const
{
    QString const configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QDir const configRoot(configDir.isEmpty() ? QDir::homePath() : configDir);
    // The embedded legacy MainWindow creates MultiSettings, which uses the
    // decodium4.ini file.  Using ft2.ini here made the QML UDP page update a
    // different store, while the legacy MessageClient kept the default 2237.
    // Keep one authoritative store for the embedded backend.
    return configRoot.absoluteFilePath(QStringLiteral("decodium4.ini"));
}

QString DecodiumBridge::legacyConfigGroupName() const
{
    // MultiSettings stores the active configuration name at
    // MultiSettings/CurrentName inside the INI file.
    QSettings ini(legacyIniPath(), QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("MultiSettings"));
    QString name = ini.value(QStringLiteral("CurrentName")).toString();
    ini.endGroup();
    if (name.isEmpty()) {
        name = QStringLiteral("Default");
    }
    return name;
}

bool DecodiumBridge::isLegacySyncKey(const QString& key) const
{
    static QSet<QString> const keys {
        // Keep manual GPU fallback overrides in ft2.ini in sync with Setup.
        QStringLiteral("LiveMapUseGpu"),
        QStringLiteral("UDPServer"),
        QStringLiteral("UDPServerPort"),
        QStringLiteral("UDPListenPort"),
        QStringLiteral("UDPClientId"),
        QStringLiteral("UDPSecondaryClientId"),
        QStringLiteral("UDPTertiaryClientId"),
        QStringLiteral("UDPInterface"),
        QStringLiteral("UDPTTL"),
        QStringLiteral("UDPPrimaryLoggedAdifEnabled"),
        QStringLiteral("UDPPrimarySendDecode"),
        QStringLiteral("UDPPrimarySendStatus"),
        QStringLiteral("UDPPrimarySendQso"),
        QStringLiteral("UDPPrimarySendWspr"),
        QStringLiteral("UDPSecondaryEnabled"),
        QStringLiteral("UDPSecondaryServer"),
        QStringLiteral("UDPSecondaryServerPort"),
        QStringLiteral("UDPSecondaryInterface"),
        QStringLiteral("UDPSecondaryTTL"),
        QStringLiteral("UDPSecondaryLoggedAdifEnabled"),
        QStringLiteral("UDPSecondaryQsoLoggedEnabled"),
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
        QStringLiteral("ADIFTcpEnabled"),
        QStringLiteral("ADIFTcpServer"),
        QStringLiteral("ADIFTcpPort"),
        QStringLiteral("N1MMServer"),
        QStringLiteral("N1MMServerPort"),
        QStringLiteral("BroadcastToN1MM"),
        QStringLiteral("AcceptUDPRequests"),
        QStringLiteral("udpWindowToFront"),
        QStringLiteral("udpWindowRestore"),
        QStringLiteral("RemoteWebEnabled"),
        QStringLiteral("WebAppEnabled"),
        QStringLiteral("RemoteHttpPort"),
        QStringLiteral("RemoteWsPort"),
        QStringLiteral("RemoteWsBind"),
        QStringLiteral("RemoteUser"),
        QStringLiteral("RemoteToken"),
        QStringLiteral("PromptToLog"),
        QStringLiteral("AutoLog"),
        QStringLiteral("AutoSpotEnabled"),
        QStringLiteral("ContestingOnly"),
        QStringLiteral("ZZ00"),
        QStringLiteral("LogAsRTTY"),
        QStringLiteral("toRTTY"),
        QStringLiteral("Log4DigitGrids"),
        QStringLiteral("Log4digitGrids"),
        QStringLiteral("SpecOpInComments"),
        QStringLiteral("specOptoComments"),
        QStringLiteral("dBReportsToComments"),
        QStringLiteral("dBtoComments"),
        QStringLiteral("LogComments"),
        QStringLiteral("TxDelay"),
        QStringLiteral("TXDelay"),
        QStringLiteral("TxQSYAllowed"),
        QStringLiteral("AllowTXQSY"),
        QStringLiteral("73TxDisable"),
        QStringLiteral("DisableTXon73"),
        QStringLiteral("RepeatTx"),
        QStringLiteral("RepeatTXuntil73"),
        QStringLiteral("TxWatchdog"),
        QStringLiteral("TXWatchdog"),
        QStringLiteral("TuneWatchdog"),
        QStringLiteral("TuneWatchdogEnabled"),
        QStringLiteral("TuneWatchdogTime"),
        QStringLiteral("TuneWatchdogMin"),
        QStringLiteral("After73"),
        QStringLiteral("CWIDafter73"),
        QStringLiteral("IDint"),
        QStringLiteral("CWIDInterval"),
        QStringLiteral("x2ToneSpacing"),
        QStringLiteral("x4ToneSpacing"),
        QStringLiteral("AlternateBindings"),
        QStringLiteral("AlternateF1F6Bindings"),
        QStringLiteral("Highlight73"),
        QStringLiteral("HighlightOrange"),
        QStringLiteral("HighlightBlue"),
        QStringLiteral("HighlightOrangeCallsigns"),
        QStringLiteral("HighlightBlueCallsigns"),
        QStringLiteral("OrangeCallsigns"),
        QStringLiteral("BlueCallsigns"),
        QStringLiteral("MonitorOFF"),
        QStringLiteral("MonitorLastUsed"),
        QStringLiteral("AutoAstroWindow"),
        QStringLiteral("kHzWithoutK"),
        QStringLiteral("kHzWithoutk"),
        QStringLiteral("ProgressBarRed"),
        QStringLiteral("HighDPI"),
        QStringLiteral("LargerTabWidget"),
        QStringLiteral("QuickCall"),
        QStringLiteral("ForceCallFirst"),
        QStringLiteral("ForceCall1st"),
        QStringLiteral("VHFUHF"),
        QStringLiteral("WaitFeaturesEnabled"),
        QStringLiteral("EnableWaitFeatures"),
        QStringLiteral("erase_BandActivity"),
        QStringLiteral("EraseBandActivity"),
        QStringLiteral("clear_DXgrid"),
        QStringLiteral("ClearDXGrid"),
        QStringLiteral("clear_DXcall"),
        QStringLiteral("ClearDXCall"),
        QStringLiteral("set_RXtoTX"),
        QStringLiteral("RXtoTXafterQSO"),
        QStringLiteral("AlternateEraseButtonBehavior"),
        QStringLiteral("AlternateEraseButton"),
        QStringLiteral("TxWarningDisabled"),
        QStringLiteral("DisableButtonColoring"),
        QStringLiteral("SuperFox"),
        QStringLiteral("ShowOTP"),
        QStringLiteral("SelectedActivity"),
        QStringLiteral("SpecialOpActivity"),
        QStringLiteral("Field_Day_Exchange"),
        QStringLiteral("FieldDayExchange"),
        QStringLiteral("RTTY_Exchange"),
        QStringLiteral("RTTYExchange"),
        QStringLiteral("Contest_Name"),
        QStringLiteral("ContestName"),
        QStringLiteral("Individual_Contest_Name"),
        QStringLiteral("IndividualContestName"),
        QStringLiteral("NCCC_Sprint"),
        QStringLiteral("NCCCSprint"),
        QStringLiteral("OTPEnabled"),
        QStringLiteral("OTPSeed"),
        QStringLiteral("OTPinterval"),
        QStringLiteral("OTPInterval"),
        QStringLiteral("OTPUrl"),
        QStringLiteral("OTPURL"),
        QStringLiteral("alert_Enabled"),
        QStringLiteral("alert_CQ"),
        QStringLiteral("alert_MyCall"),
        QStringLiteral("alert_DXCC"),
        QStringLiteral("alert_DXCCOB"),
        QStringLiteral("alert_Grid"),
        QStringLiteral("alert_GridOB"),
        QStringLiteral("alert_Continent"),
        QStringLiteral("alert_ContinentOB"),
        QStringLiteral("alert_CQZ"),
        QStringLiteral("alert_CQZOB"),
        QStringLiteral("alert_ITUZ"),
        QStringLiteral("alert_ITUZOB"),
        QStringLiteral("alert_DXcall"),
        QStringLiteral("alert_QSYmessage"),
        QStringLiteral("AlertMyCall"),
        QStringLiteral("AlertNewDXCC"),
        QStringLiteral("AlertNewDXCCBand"),
        QStringLiteral("AlertNewGrid"),
        QStringLiteral("AlertNewGridBand"),
        QStringLiteral("AlertNewContinent"),
        QStringLiteral("AlertNewContinentBand"),
        QStringLiteral("AlertNewCQZone"),
        QStringLiteral("AlertNewCQZoneBand"),
        QStringLiteral("AlertNewITUZone"),
        QStringLiteral("AlertNewITUZoneBand"),
        QStringLiteral("AlertDXCallGrid"),
        QStringLiteral("AlertQSYMessage"),
        QStringLiteral("Blacklisted"),
        QStringLiteral("BlacklistEnabled"),
        QStringLiteral("Whitelisted"),
        QStringLiteral("WhitelistEnabled"),
        QStringLiteral("AlwaysPass"),
        QStringLiteral("AlwaysPassEnabled"),
        QStringLiteral("FiltersForWaitAndPounceOnly"),
        QStringLiteral("FiltersWaitPounceOnly"),
        QStringLiteral("FiltersForWord2"),
        QStringLiteral("FiltersCallingOnly"),
        QStringLiteral("Font"),
        QStringLiteral("DecodedTextFont"),
        QStringLiteral("SingleDecode"),
        QStringLiteral("PSKReporter"),
        QStringLiteral("PSKReporterTCPIP"),
        QStringLiteral("PSKRtcpip"),
        QStringLiteral("ShowGreyline"),
        QStringLiteral("MapShowGreyline"),
    };
    if (keys.contains(key)) {
        return true;
    }
    static QRegularExpression const indexedLegacyKey {
        QStringLiteral("^(Blacklist|Whitelist|Pass|AlwaysPass|Territory)([1-9]|1[0-2])$")
    };
    return indexedLegacyKey.match(key).hasMatch();
}

void DecodiumBridge::syncSettingToLegacyIni(const QString& key, const QVariant& value)
{
    QString const iniPath = legacyIniPath();
    if (iniPath.isEmpty()) return;

    QSettings ini(iniPath, QSettings::IniFormat);
    ini.setValue(key, value);
    QStringList groups {legacyConfigGroupName(), QStringLiteral("Configuration"), QStringLiteral("Default")};
    groups.removeDuplicates();
    for (QString const& group : groups) {
        if (group.isEmpty()) {
            continue;
        }
        ini.beginGroup(group);
        ini.setValue(key, value);
        ini.endGroup();
    }
    // Station/transverter edits are immediately read back by the QML page.
    // Flush here so a write error cannot be reported as a successful save.
    ini.sync();
    if (ini.status() != QSettings::NoError) {
        decodiumBridgeLogbookLog(QStringLiteral("Legacy setting write failed: key=%1 path=%2 status=%3")
                      .arg(key, iniPath)
                      .arg(static_cast<int>(ini.status())));
    }
}

QVariant DecodiumBridge::readSettingFromLegacyIni(const QString& key) const
{
    // The embedded legacy backend has one current/root configuration.  A
    // value explicitly saved for this dashboard profile must not be shadowed
    // by another instance's legacy root value.
    if (!decodium::activeSettingsProfileName().isEmpty()) {
        QSettings profile(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(profile);
        if (profile.contains(key)) return profile.value(key);
        const QString alias = decodiumBridgeAliasedSettingKey(key);
        if (alias != key && profile.contains(alias)) return profile.value(alias);
    }
    QString const iniPath = legacyIniPath();
    if (iniPath.isEmpty()) return {};

    QSettings ini(iniPath, QSettings::IniFormat);
    QVariant value = ini.value(key);
    if (value.isValid()) {
        return value;
    }

    QStringList groups {legacyConfigGroupName(), QStringLiteral("Configuration"), QStringLiteral("Default")};
    groups.removeDuplicates();
    for (QString const& group : groups) {
        if (group.isEmpty()) {
            continue;
        }
        ini.beginGroup(group);
        value = ini.value(key);
        ini.endGroup();
        if (value.isValid()) {
            return value;
        }
    }

    return {};
}

