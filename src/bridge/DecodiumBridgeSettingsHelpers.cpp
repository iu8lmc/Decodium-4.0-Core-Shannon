#include "SecureSettings.hpp"
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVariantMap>

static QString aliasedBridgeSettingKey(const QString& key)
{
    if (key == QStringLiteral("PSKRtcpip")) return QStringLiteral("PSKReporterTCPIP");
    // Greyline used two names over the lifetime of the legacy Configuration
    // dialog.  Keep both names in sync so a profile created on Linux, macOS
    // or Windows cannot silently override the Live Map toggle.
    if (key == QStringLiteral("ShowGreyline")) return QStringLiteral("MapShowGreyline");
    if (key == QStringLiteral("MapShowGreyline")) return QStringLiteral("ShowGreyline");
    if (key == QStringLiteral("RemoteWebEnabled")) return QStringLiteral("WebAppEnabled");
    if (key == QStringLiteral("RemoteHttpPort")) return QStringLiteral("WebAppHttpPort");
    if (key == QStringLiteral("RemoteWsPort")) return QStringLiteral("WebAppWsPort");
    if (key == QStringLiteral("RemoteWsBind")) return QStringLiteral("WebAppBind");
    if (key == QStringLiteral("RemoteUser")) return QStringLiteral("WebAppUser");
    if (key == QStringLiteral("RemoteToken")) return QStringLiteral("WebAppToken");
    if (key == QStringLiteral("PowerBandTXMemory")) return QStringLiteral("pwrBandTxMemory");
    if (key == QStringLiteral("PowerBandTuneMemory")) return QStringLiteral("pwrBandTuneMemory");
    if (key == QStringLiteral("ShowDXCC")) return QStringLiteral("DXCCEntity");
    if (key == QStringLiteral("ShowCountryNames")) return QStringLiteral("AlwaysShowCountryNames");
    if (key == QStringLiteral("InsertBlankLine")) return QStringLiteral("InsertBlank");
    if (key == QStringLiteral("TXMessagesToRX")) return QStringLiteral("Tx2QSO");
    if (key == QStringLiteral("LogAsRTTY")) return QStringLiteral("toRTTY");
    if (key == QStringLiteral("Log4DigitGrids")) return QStringLiteral("Log4digitGrids");
    if (key == QStringLiteral("SpecOpInComments")) return QStringLiteral("specOptoComments");
    if (key == QStringLiteral("dBReportsToComments")) return QStringLiteral("dBtoComments");
    if (key == QStringLiteral("TXFrequency")) return QStringLiteral("txFrequency");
    if (key == QStringLiteral("TXDelay")) return QStringLiteral("TxDelay");
    if (key == QStringLiteral("AllowTXQSY")) return QStringLiteral("TxQSYAllowed");
    if (key == QStringLiteral("DisableTXon73")) return QStringLiteral("73TxDisable");
    if (key == QStringLiteral("RepeatTXuntil73")) return QStringLiteral("RepeatTx");
    if (key == QStringLiteral("TXWatchdog")) return QStringLiteral("TxWatchdog");
    if (key == QStringLiteral("TuneWatchdogEnabled")) return QStringLiteral("TuneWatchdog");
    if (key == QStringLiteral("TuneWatchdogMin")) return QStringLiteral("TuneWatchdogTime");
    if (key == QStringLiteral("CWIDafter73")) return QStringLiteral("After73");
    if (key == QStringLiteral("CWIDInterval")) return QStringLiteral("IDint");
    if (key == QStringLiteral("AlternateF1F6Bindings")) return QStringLiteral("AlternateBindings");
    if (key == QStringLiteral("OrangeCallsigns")) return QStringLiteral("HighlightOrangeCallsigns");
    if (key == QStringLiteral("BlueCallsigns")) return QStringLiteral("HighlightBlueCallsigns");
    if (key == QStringLiteral("kHzWithoutk")) return QStringLiteral("kHzWithoutK");
    if (key == QStringLiteral("ForceCall1st")) return QStringLiteral("ForceCallFirst");
    if (key == QStringLiteral("EnableWaitFeatures")) return QStringLiteral("WaitFeaturesEnabled");
    if (key == QStringLiteral("EraseBandActivity")) return QStringLiteral("erase_BandActivity");
    if (key == QStringLiteral("ClearDXGrid")) return QStringLiteral("clear_DXgrid");
    if (key == QStringLiteral("ClearDXCall")) return QStringLiteral("clear_DXcall");
    if (key == QStringLiteral("RXtoTXafterQSO")) return QStringLiteral("set_RXtoTX");
    if (key == QStringLiteral("AlternateEraseButton")) return QStringLiteral("AlternateEraseButtonBehavior");
    if (key == QStringLiteral("DisableButtonColoring")) return QStringLiteral("TxWarningDisabled");
    if (key == QStringLiteral("FieldDayExchange")) return QStringLiteral("Field_Day_Exchange");
    if (key == QStringLiteral("RTTYExchange")) return QStringLiteral("RTTY_Exchange");
    if (key == QStringLiteral("ContestName")) return QStringLiteral("Contest_Name");
    if (key == QStringLiteral("IndividualContestName")) return QStringLiteral("Individual_Contest_Name");
    if (key == QStringLiteral("NCCCSprint")) return QStringLiteral("NCCC_Sprint");
    if (key == QStringLiteral("DTClampSynced")) return QStringLiteral("DTClampSynced_ms");
    if (key == QStringLiteral("DTClampUnsynced")) return QStringLiteral("DTClampUnsynced_ms");
    if (key == QStringLiteral("OTPInterval")) return QStringLiteral("OTPinterval");
    if (key == QStringLiteral("OTPURL")) return QStringLiteral("OTPUrl");
    if (key == QStringLiteral("AlertMyCall")) return QStringLiteral("alert_MyCall");
    if (key == QStringLiteral("AlertNewDXCC")) return QStringLiteral("alert_DXCC");
    if (key == QStringLiteral("AlertNewDXCCBand")) return QStringLiteral("alert_DXCCOB");
    if (key == QStringLiteral("AlertNewGrid")) return QStringLiteral("alert_Grid");
    if (key == QStringLiteral("AlertNewGridBand")) return QStringLiteral("alert_GridOB");
    if (key == QStringLiteral("AlertNewContinent")) return QStringLiteral("alert_Continent");
    if (key == QStringLiteral("AlertNewContinentBand")) return QStringLiteral("alert_ContinentOB");
    if (key == QStringLiteral("AlertNewCQZone")) return QStringLiteral("alert_CQZ");
    if (key == QStringLiteral("AlertNewCQZoneBand")) return QStringLiteral("alert_CQZOB");
    if (key == QStringLiteral("AlertNewITUZone")) return QStringLiteral("alert_ITUZ");
    if (key == QStringLiteral("AlertNewITUZoneBand")) return QStringLiteral("alert_ITUZOB");
    if (key == QStringLiteral("AlertDXCallGrid")) return QStringLiteral("alert_DXcall");
    if (key == QStringLiteral("AlertQSYMessage")) return QStringLiteral("alert_QSYmessage");
    if (key == QStringLiteral("BlacklistEnabled")) return QStringLiteral("Blacklisted");
    if (key == QStringLiteral("WhitelistEnabled")) return QStringLiteral("Whitelisted");
    if (key == QStringLiteral("AlwaysPassEnabled")) return QStringLiteral("AlwaysPass");
    if (key == QStringLiteral("FiltersWaitPounceOnly")) return QStringLiteral("FiltersForWaitAndPounceOnly");
    if (key == QStringLiteral("FiltersCallingOnly")) return QStringLiteral("FiltersForWord2");
    if (key == QStringLiteral("HideB4") || key == QStringLiteral("HideWorkedBand")) return QStringLiteral("FiltersHideWorkedBand");
    if (key == QStringLiteral("HideToday") || key == QStringLiteral("HideWorkedToday")) return QStringLiteral("FiltersHideWorkedToday");
    static QRegularExpression const alwaysPassAlias {
        QStringLiteral("^AlwaysPass([1-9]|1[0-2])$")
    };
    QRegularExpressionMatch const alwaysPassMatch = alwaysPassAlias.match(key);
    if (alwaysPassMatch.hasMatch()) {
        return QStringLiteral("Pass%1").arg(alwaysPassMatch.captured(1));
    }
    return key;
}

QString decodiumBridgeAliasedSettingKey(const QString& key) { return aliasedBridgeSettingKey(key); }


static QString resolveSecureSettingFromProfiles(QString const& settingKey,
                                                QString const& rawValue,
                                                QStringList profiles)
{
    QString plain = rawValue.trimmed();
    if (secure_settings::is_placeholder(plain)) {
        plain.clear();
    }

    profiles.removeAll(QString {});
    profiles.removeDuplicates();
    if (!profiles.contains(QStringLiteral("DEFAULT"), Qt::CaseInsensitive)) {
        profiles << QStringLiteral("DEFAULT");
    }

    auto const& backend = secure_settings::default_backend();
    for (QString profile : profiles) {
        profile = profile.trimmed().toUpper();
        if (profile.isEmpty()) {
            continue;
        }

        auto const lookup = backend.lookup(secure_settings::service(profile), settingKey);
        if (lookup.found) {
            return lookup.value.trimmed();
        }
    }

    return plain;
}

QString decodiumBridgeResolveSecureSetting(QString const& key, QString const& raw, QStringList profiles)
{ return resolveSecureSettingFromProfiles(key, raw, profiles); }

static QString ft2LinkSmtpConfigString(QVariantMap const& config,
                                       QString const& key,
                                       QString const& fallback = QString())
{
    return config.value(key, fallback).toString().trimmed();
}

static QString ft2LinkSmtpSecretKey(QVariantMap const& config)
{
    QString host = ft2LinkSmtpConfigString(config, QStringLiteral("host")).toLower();
    QString user = ft2LinkSmtpConfigString(config, QStringLiteral("username")).toLower();
    if (host.isEmpty()) {
        host = QStringLiteral("default-host");
    }
    if (user.isEmpty()) {
        user = QStringLiteral("default-user");
    }
    host.replace(QRegularExpression(QStringLiteral(R"([^a-z0-9._-])")), QStringLiteral("_"));
    user.replace(QRegularExpression(QStringLiteral(R"([^a-z0-9._@+-])")), QStringLiteral("_"));
    return QStringLiteral("ft2LinkSmtpPassword/%1/%2").arg(host, user);
}

QString decodiumBridgeFt2SmtpSecretKey(const QVariantMap& config)
{
    return ft2LinkSmtpSecretKey(config);
}
QString decodiumBridgeFt2SmtpConfigString(const QVariantMap& config, const QString& key,
                                          const QString& fallback)
{
    return ft2LinkSmtpConfigString(config, key, fallback);
}

