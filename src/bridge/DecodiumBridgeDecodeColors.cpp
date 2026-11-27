#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include "DecodeColorHelpers.h"
#include <QSettings>

namespace decodium::decode_colors {
const QStringList& decodeColorPropertyNames()
{
    static const QStringList props {
        QStringLiteral("colorTxMessage"),
        QStringLiteral("colorMyCall"),
        QStringLiteral("colorNewDxccBand"),
        QStringLiteral("colorNewDxcc"),
        QStringLiteral("colorNewContinentBand"),
        QStringLiteral("colorNewContinent"),
        QStringLiteral("colorNewCqZoneBand"),
        QStringLiteral("colorNewCqZone"),
        QStringLiteral("colorNewItuZoneBand"),
        QStringLiteral("colorNewItuZone"),
        QStringLiteral("colorNewGridBand"),
        QStringLiteral("colorNewGrid"),
        QStringLiteral("colorNewCallBand"),
        QStringLiteral("colorNewCall"),
        QStringLiteral("colorLotwUser"),
        QStringLiteral("colorCQ"),
        QStringLiteral("colorDXEntity"),
        QStringLiteral("color73"),
        QStringLiteral("colorB4"),
        QStringLiteral("colorDecodeText")
    };
    return props;
}

bool isDecodeColorProperty(const QString& prop)
{
    return decodeColorPropertyNames().contains(prop);
}

QString decodeColorEnabledSettingKey(const QString& prop)
{
    return prop + QStringLiteral("Enabled");
}

QString decodeColorBoldSettingKey(const QString& prop)
{
    return QStringLiteral("bold_%1").arg(prop);
}

bool decodeColorBoldDefault(const QString& prop)
{
    // Preserve the historical emphasis for CQ, own call, TX and "new" classes,
    // while keeping signoff and normal decodes neutral unless the user opts in.
    if (prop == QStringLiteral("colorCQ")
        || prop == QStringLiteral("colorMyCall")
        || prop == QStringLiteral("colorTxMessage")
        || prop == QStringLiteral("colorDXEntity"))
        return true;
    if (prop.startsWith(QStringLiteral("colorNew")))
        return true;
    return false;
}
}

using namespace decodium::decode_colors;

QString DecodiumBridge::decodeColorValue(const QString& prop) const
{
    if (prop == QStringLiteral("colorTxMessage")) return m_colorTxMessage;
    if (prop == QStringLiteral("colorMyCall")) return m_colorMyCall;
    if (prop == QStringLiteral("colorNewDxccBand")) return m_colorNewDxccBand;
    if (prop == QStringLiteral("colorNewDxcc")) return m_colorNewDxcc;
    if (prop == QStringLiteral("colorNewContinentBand")) return m_colorNewContinentBand;
    if (prop == QStringLiteral("colorNewContinent")) return m_colorNewContinent;
    if (prop == QStringLiteral("colorNewCqZoneBand")) return m_colorNewCqZoneBand;
    if (prop == QStringLiteral("colorNewCqZone")) return m_colorNewCqZone;
    if (prop == QStringLiteral("colorNewItuZoneBand")) return m_colorNewItuZoneBand;
    if (prop == QStringLiteral("colorNewItuZone")) return m_colorNewItuZone;
    if (prop == QStringLiteral("colorNewGridBand")) return m_colorNewGridBand;
    if (prop == QStringLiteral("colorNewGrid")) return m_colorNewGrid;
    if (prop == QStringLiteral("colorNewCallBand")) return m_colorNewCallBand;
    if (prop == QStringLiteral("colorNewCall")) return m_colorNewCall;
    if (prop == QStringLiteral("colorLotwUser")) return m_colorLotwUser;
    if (prop == QStringLiteral("colorCQ")) return m_colorCQ;
    if (prop == QStringLiteral("colorDXEntity")) return m_colorDXEntity;
    if (prop == QStringLiteral("color73")) return m_color73;
    if (prop == QStringLiteral("colorB4")) return m_colorB4;
    if (prop == QStringLiteral("colorDecodeText")) return m_colorDecodeText;
    return QString();
}

bool DecodiumBridge::decodeColorEnabled(const QString& prop) const
{
    if (!isDecodeColorProperty(prop))
        return true;
    return m_decodeColorEnabled.value(prop, true);
}

void DecodiumBridge::setB4Strikethrough(bool enabled)
{
    if (m_b4Strikethrough == enabled)
        return;

    m_b4Strikethrough = enabled;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("b4Strikethrough"), enabled);
    settings.sync();
    syncSettingToLegacyIni(QStringLiteral("b4Strikethrough"), enabled);

    emit b4StrikethroughChanged();
    emit settingValueChanged(QStringLiteral("b4Strikethrough"), enabled);
}

void DecodiumBridge::setDecodeColorEnabled(const QString& prop, bool enabled)
{
    if (!isDecodeColorProperty(prop))
        return;
    if (m_decodeColorEnabled.value(prop, true) == enabled)
        return;

    m_decodeColorEnabled.insert(prop, enabled);
    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(s);
    const QString key = decodeColorEnabledSettingKey(prop);
    s.setValue(key, enabled);
    s.sync();
    syncSettingToLegacyIni(key, enabled);
    emit decodeColorEnabledChanged(prop, enabled);
    emit settingValueChanged(key, enabled);
}

bool DecodiumBridge::decodeColorBold(const QString& prop) const
{
    if (!isDecodeColorProperty(prop))
        return false;
    return m_decodeColorBold.value(prop, decodeColorBoldDefault(prop));
}

void DecodiumBridge::setDecodeColorBold(const QString& prop, bool bold)
{
    if (!isDecodeColorProperty(prop))
        return;
    if (m_decodeColorBold.value(prop, decodeColorBoldDefault(prop)) == bold)
        return;

    m_decodeColorBold.insert(prop, bold);
    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(s);
    const QString key = decodeColorBoldSettingKey(prop);
    s.setValue(key, bold);
    s.sync();
    syncSettingToLegacyIni(key, bold);
    emit decodeColorBoldChanged(prop, bold);
    emit settingValueChanged(key, bold);
}

QString DecodiumBridge::effectiveDecodeColor(const QString& prop) const
{
    if (!isDecodeColorProperty(prop))
        return QString();
    return decodeColorEnabled(prop) ? decodeColorValue(prop) : decodeColorFallback();
}

QString DecodiumBridge::decodeHighlightBg(const QVariantMap& entry) const
{
    auto enabledColor = [this](const QString& prop) -> QString {
        return decodeColorEnabled(prop) ? decodeColorValue(prop) : QString();
    };

    if (entry.value(QStringLiteral("isTx")).toBool())                return enabledColor(QStringLiteral("colorTxMessage"));
    if (entry.value(QStringLiteral("isMyCall")).toBool())            return enabledColor(QStringLiteral("colorMyCall"));

    if (entry.value(QStringLiteral("isCQ")).toBool())                return enabledColor(QStringLiteral("colorCQ"));

    if (entry.value(QStringLiteral("dxIsNewDxccBand")).toBool())     return enabledColor(QStringLiteral("colorNewDxccBand"));
    if (entry.value(QStringLiteral("dxIsNewDxcc")).toBool())         return enabledColor(QStringLiteral("colorNewDxcc"));
    if (entry.value(QStringLiteral("dxIsNewContinentBand")).toBool())return enabledColor(QStringLiteral("colorNewContinentBand"));
    if (entry.value(QStringLiteral("dxIsNewContinent")).toBool())    return enabledColor(QStringLiteral("colorNewContinent"));
    if (entry.value(QStringLiteral("dxIsNewCqZoneBand")).toBool())   return enabledColor(QStringLiteral("colorNewCqZoneBand"));
    if (entry.value(QStringLiteral("dxIsNewCqZone")).toBool())       return enabledColor(QStringLiteral("colorNewCqZone"));
    if (entry.value(QStringLiteral("dxIsNewItuZoneBand")).toBool())  return enabledColor(QStringLiteral("colorNewItuZoneBand"));
    if (entry.value(QStringLiteral("dxIsNewItuZone")).toBool())      return enabledColor(QStringLiteral("colorNewItuZone"));
    if (entry.value(QStringLiteral("dxIsNewGridBand")).toBool())     return enabledColor(QStringLiteral("colorNewGridBand"));
    if (entry.value(QStringLiteral("dxIsNewGrid")).toBool())         return enabledColor(QStringLiteral("colorNewGrid"));
    if (entry.value(QStringLiteral("dxIsNewCallBand")).toBool())     return enabledColor(QStringLiteral("colorNewCallBand"));
    if (entry.value(QStringLiteral("dxIsNewCall")).toBool())         return enabledColor(QStringLiteral("colorNewCall"));

    return QString();
}

QString DecodiumBridge::decodeColorBgValue(const QString& prop) const
{
    return m_decodeColorBg.value(prop, QString());
}

bool DecodiumBridge::decodeColorBgEnabled(const QString& prop) const
{
    return m_decodeColorBgEnabled.value(prop, false);
}

void DecodiumBridge::setDecodeColorBg(const QString& prop, const QString& hex)
{
    if (!isDecodeColorProperty(prop)) return;
    if (m_decodeColorBg.value(prop) == hex) return;
    m_decodeColorBg.insert(prop, hex);
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("bg_%1").arg(prop), hex);
    emit decodeColorBgChanged();
}

void DecodiumBridge::setDecodeColorBgEnabled(const QString& prop, bool enabled)
{
    if (!isDecodeColorProperty(prop)) return;
    if (m_decodeColorBgEnabled.value(prop, false) == enabled) return;
    m_decodeColorBgEnabled.insert(prop, enabled);
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("bgEnabled_%1").arg(prop), enabled);
    emit decodeColorBgChanged();
}

// 1.0.416 — sfondo riga SCELTO dall'utente per la categoria che matcha l'entry.
// Stessa priorita' di decodeHighlightBg, piu' B4 / DX Entity / 73 (signoff).
// Una riga puo' rientrare in piu' categorie (es. CQ + nominativo nuovo + LoTW):
// vince la PRIMA, in ordine di priorita', che ha uno sfondo acceso e un colore.
// Prima vinceva la prima categoria in assoluto anche senza sfondo, cosi' uno
// sfondo acceso solo per LoTW non compariva quasi mai (le stazioni in CQ o mai
// lavorate si fermavano prima). Ritorna "" se nessuna categoria ha lo sfondo.
QString DecodiumBridge::decodeHighlightUserBg(const QVariantMap& entry) const
{
    auto bg = [this](const QString& prop) -> QString {
        return m_decodeColorBgEnabled.value(prop, false) ? m_decodeColorBg.value(prop, QString()) : QString();
    };
    auto flag = [&entry](const char* key) -> bool {
        return entry.value(QString::fromLatin1(key)).toBool();
    };
    QString picked;
    auto pick = [&](bool matches, const char* prop) -> bool {
        if (matches && picked.isEmpty())
            picked = bg(QString::fromLatin1(prop));
        return !picked.isEmpty();
    };

    if (pick(flag("isTx"), "colorTxMessage")) return picked;
    if (pick(flag("isMyCall"), "colorMyCall")) return picked;
    {
        const QString msg = entry.value(QStringLiteral("message")).toString().toUpper();
        const bool signoff = msg.endsWith(QStringLiteral(" 73"))
            || msg.contains(QStringLiteral("RR73"))
            || msg.endsWith(QStringLiteral("RRR"));
        if (pick(signoff, "color73")) return picked;
    }
    if (pick(flag("isCQ"), "colorCQ")) return picked;

    if (pick(flag("dxIsNewDxccBand"), "colorNewDxccBand")) return picked;
    if (pick(flag("dxIsNewDxcc"), "colorNewDxcc")) return picked;
    if (pick(flag("dxIsNewContinentBand"), "colorNewContinentBand")) return picked;
    if (pick(flag("dxIsNewContinent"), "colorNewContinent")) return picked;
    if (pick(flag("dxIsNewCqZoneBand"), "colorNewCqZoneBand")) return picked;
    if (pick(flag("dxIsNewCqZone"), "colorNewCqZone")) return picked;
    if (pick(flag("dxIsNewItuZoneBand"), "colorNewItuZoneBand")) return picked;
    if (pick(flag("dxIsNewItuZone"), "colorNewItuZone")) return picked;
    if (pick(flag("dxIsNewGridBand"), "colorNewGridBand")) return picked;
    if (pick(flag("dxIsNewGrid"), "colorNewGrid")) return picked;
    if (pick(flag("dxIsNewCallBand"), "colorNewCallBand")) return picked;
    if (pick(flag("dxIsNewCall"), "colorNewCall")) return picked;
    if (pick(flag("isB4") || flag("dxIsWorked"), "colorB4")) return picked;
    if (pick(flag("isLotw"), "colorLotwUser")) return picked;
    const bool dxEntity = entry.value(QStringLiteral("dxCountry")).toString().size() > 0
        || flag("dxIsMostWanted")
        || flag("dxIsNewCountry")
        || flag("dxIsNewBand");
    if (pick(dxEntity, "colorDXEntity")) return picked;
    return QString();
}

