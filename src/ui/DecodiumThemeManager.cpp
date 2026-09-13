#include "DecodiumThemeManager.h"
#include "DecodiumProfileSettings.h"

#include <QSettings>

namespace
{
QVariant profiledThemeValue(const QString& key,
                            const QVariant& defaultValue,
                            const QString& legacyApplication)
{
    QSettings profile(QSettings::IniFormat, QSettings::UserScope,
                      QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    bool const hasActiveProfile = decodium::beginActiveSettingsProfile(profile);
    if (hasActiveProfile && profile.contains(key)) {
        return profile.value(key, defaultValue);
    }

    QSettings legacy(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("Decodium"), legacyApplication);
    QVariant const value = legacy.value(key, defaultValue);
    if (hasActiveProfile) {
        profile.setValue(key, value);
        profile.sync();
    }
    return value;
}

void setProfiledThemeValue(const QString& key,
                           const QVariant& value,
                           const QString& legacyApplication)
{
    QSettings profile(QSettings::IniFormat, QSettings::UserScope,
                      QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    if (decodium::beginActiveSettingsProfile(profile)) {
        profile.setValue(key, value);
        profile.sync();
        return;
    }

    QSettings legacy(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("Decodium"), legacyApplication);
    legacy.setValue(key, value);
    legacy.sync();
}
}

const DecodiumThemeManager::ThemePalette DecodiumThemeManager::s_oceanBlue {
    /* bgDeep         */ QColor("#0A0F1A"),
    /* bgMedium       */ QColor("#111827"),
    /* bgLight        */ QColor("#1E2D42"),
    /* primaryColor   */ QColor("#4A90E2"),
    /* secondaryColor */ QColor("#00D4FF"),
    /* accentColor    */ QColor("#00FF88"),
    /* warningColor   */ QColor("#FF8C00"),
    /* errorColor     */ QColor("#FF5F56"),
    /* textPrimary    */ QColor("#E8F4FD"),
    /* textSecondary  */ QColor("#89B4D0"),
    /* successColor   */ QColor("#4CAF50"),
    /* glassOverlay   */ QColor(26, 58, 92, 64),
    /* glassBorder    */ QColor(74, 144, 226, 80),
    /* borderColor    */ QColor(74, 144, 226, 80),
    /* borderSoft     */ QColor(74, 144, 226, 40),
    /* panelColor     */ QColor("#1E2D42"),
    /* panelHeader    */ QColor("#283C57"),
    /* rowMatchBg     */ QColor(0, 255, 136, 38),
    /* rowMatchBorder */ QColor("#00FF88"),
    /* ledRed         */ QColor("#FF5F56"),
    /* ledBlue        */ QColor("#4A90E2"),
    /* ledYellow      */ QColor("#FFD700"),
    /* ledMagenta     */ QColor("#FF00FF"),
    /* accentDim      */ QColor(),
    /* accentDeep     */ QColor(),
    /* pileColor      */ QColor(),
    /* gridColor      */ QColor(),
    /* txColor        */ QColor(),
    /* rxColor        */ QColor(),
    /* isLight        */ false
};

// Tema chiaro rifatto 1.0.526: schede bianche su fondo chiaro freddo, bordi
// sottili e testo ardesia. L'identita' resta pero' nostra: l'accento e' un
// verde-teal (non il blu/ciano altrui) e il fondo pagina e' appena piu'
// grigio, cosi' le schede bianche si staccano invece di confondersi.
const DecodiumThemeManager::ThemePalette DecodiumThemeManager::s_stellarLight {
    /* bgDeep         */ QColor("#EDF2F7"),   // fondo pagina
    /* bgMedium       */ QColor("#E1E9F1"),   // fasce e barre
    /* bgLight        */ QColor("#FFFFFF"),   // schede
    /* primaryColor   */ QColor("#1F76D2"),   // blu dei comandi attivi
    /* secondaryColor */ QColor("#0E9AAE"),   // teal di supporto
    /* accentColor    */ QColor("#0E8C6A"),   // verde-teal: la nostra firma
    /* warningColor   */ QColor("#B5741A"),
    /* errorColor     */ QColor("#CE4038"),
    /* textPrimary    */ QColor("#0E1A22"),   // ardesia quasi nera
    /* textSecondary  */ QColor("#5C6E7E"),
    /* successColor   */ QColor("#0E8C6A"),
    /* glassOverlay   */ QColor(255, 255, 255, 214),
    /* glassBorder    */ QColor("#CBD8E3"),   // filetti sottili
    /* borderColor    */ QColor("#C3D2DF"),
    /* borderSoft     */ QColor("#DFE8F0"),
    /* panelColor     */ QColor("#FFFFFF"),
    /* panelHeader    */ QColor("#EAF1F7"),
    /* rowMatchBg     */ QColor(14, 140, 106, 36),
    /* rowMatchBorder */ QColor("#0E8C6A"),
    /* ledRed         */ QColor("#CE4038"),
    /* ledBlue        */ QColor("#1F76D2"),
    /* ledYellow      */ QColor("#B5741A"),
    /* ledMagenta     */ QColor("#9B5FBF"),
    /* accentDim      */ QColor(),
    /* accentDeep     */ QColor(),
    /* pileColor      */ QColor(),
    /* gridColor      */ QColor(),
    /* txColor        */ QColor(),
    /* rxColor        */ QColor(),
    /* isLight        */ true
};

// DX-Pedition Fase 1 — tema phosphor dark opt-in. accent qui = default phosphor;
// il variant attivo (cyan/amber/red) viene risolto a runtime in accentTriple().
const DecodiumThemeManager::ThemePalette DecodiumThemeManager::s_dxPedition {
    /* bgDeep         */ QColor("#050706"),
    /* bgMedium       */ QColor("#0d1310"),  // panel
    /* bgLight        */ QColor("#182019"),  // line
    /* primaryColor   */ QColor("#19ff88"),  // accent phosphor
    /* secondaryColor */ QColor("#66e6ff"),  // pile
    /* accentColor    */ QColor("#19ff88"),  // accent phosphor (override via variant)
    /* warningColor   */ QColor("#ffb84a"),  // warn
    /* errorColor     */ QColor("#ff5466"),  // hot
    /* textPrimary    */ QColor("#d6dcd8"),  // txt
    /* textSecondary  */ QColor("#6c7872"),  // txt-dim
    /* successColor   */ QColor("#19ff88"),  // rx == phosphor
    /* glassOverlay   */ QColor(13, 19, 16, 160),     // panel @ ~63%
    /* glassBorder    */ QColor(31, 42, 34, 200),     // line-2
    /* borderColor    */ QColor("#182019"),  // line
    /* borderSoft     */ QColor("#1f2a22"),  // line-2
    /* panelColor     */ QColor("#0d1310"),  // panel
    /* panelHeader    */ QColor("#0a0e0c"),  // bg-2
    /* rowMatchBg     */ QColor(25, 255, 136, 28),    // accent soft
    /* rowMatchBorder */ QColor("#19ff88"),  // accent
    /* ledRed         */ QColor("#ff5466"),  // hot
    /* ledBlue        */ QColor("#66e6ff"),  // pile
    /* ledYellow      */ QColor("#ffb84a"),  // warn
    /* ledMagenta     */ QColor("#ff7a5c"),  // tx
    /* accentDim      */ QColor("#0fa55a"),  // accent-dim phosphor (override via variant)
    /* accentDeep     */ QColor("#052d1a"),  // accent-deep phosphor (override via variant)
    /* pileColor      */ QColor("#66e6ff"),
    /* gridColor      */ QColor("#00d4b4"),
    /* txColor        */ QColor("#ff7a5c"),
    /* rxColor        */ QColor("#19ff88"),
    /* isLight        */ false
};

DecodiumThemeManager::DecodiumThemeManager(QObject* parent)
    : QObject(parent)
{
    QString const startupTheme = QStringLiteral("Ocean Blue");
    // One-shot migration: from 1.0.70 the dark Ocean Blue theme is the
    // canonical default. Reset any persisted choice once so the upgrade
    // lands on dark; the user can re-select Stellar Light afterwards.
    if (!profiledThemeValue(QStringLiteral("theme/migrated_v2"), false,
                            QStringLiteral("Decodium")).toBool()) {
        setProfiledThemeValue(QStringLiteral("theme/current"), startupTheme,
                              QStringLiteral("Decodium"));
        // Stellar Light forces palette index 11 (pastel light) on the
        // panadapter. Reset any residual 11 so the dark default lands on
        // a sensible spectrum palette instead of an all-white waterfall.
        // uiPaletteIndex is persisted by DecodiumBridge under the
        // "Decodium3" store, not the same one used for theme/current.
        if (profiledThemeValue(QStringLiteral("uiPaletteIndex"), 0,
                               QStringLiteral("Decodium3")).toInt() == 11) {
            setProfiledThemeValue(QStringLiteral("uiPaletteIndex"), 0,
                                  QStringLiteral("Decodium3"));
        }
        setProfiledThemeValue(QStringLiteral("theme/migrated_v2"), true,
                              QStringLiteral("Decodium"));
    }
    // 1.0.342 — tema persistente: rimosso il guard che forzava 'Ocean Blue' ad
    // ogni avvio (rendeva DX-Pedition non selezionabile in modo permanente). Ora
    // ripristina il tema salvato dall'utente, validandolo contro i temi noti.
    QString stored = profiledThemeValue(QStringLiteral("theme/current"), startupTheme,
                                        QStringLiteral("Decodium")).toString().trimmed();
    // 1.0.344 — rename tema DX-Pedition -> Darkcodium: migra il valore salvato.
    if (stored == "DX-Pedition") {
        stored = "Darkcodium";
        setProfiledThemeValue(QStringLiteral("theme/current"), stored,
                              QStringLiteral("Decodium"));
    }
    if (stored != "Ocean Blue" && stored != "Stellar Light" && stored != "Darkcodium")
        stored = startupTheme;
    m_currentTheme = stored;
    // DX-Pedition Fase 1 — accent variant + densità (store Decodium3 esplicito, opt-in)
    {
        QString const av =
            profiledThemeValue(QStringLiteral("ThemeAccentVariant"), QStringLiteral("phosphor"),
                               QStringLiteral("Decodium3")).toString();
        if (av == "phosphor" || av == "cyan" || av == "amber" || av == "red")
            m_accentVariant = av;
        QString const dn =
            profiledThemeValue(QStringLiteral("ThemeDensity"), QStringLiteral("regular"),
                               QStringLiteral("Decodium3")).toString();
        if (dn == "compact" || dn == "regular" || dn == "comfy")
            m_density = dn;
    }
    // 1.0.305 (#6) — colori UI personalizzati (sfondo+testo), opt-in default OFF
    m_customColorsEnabled =
        profiledThemeValue(QStringLiteral("theme/customEnabled"), false,
                           QStringLiteral("Decodium")).toBool();
    m_customBgColor =
        profiledThemeValue(QStringLiteral("theme/customBg"), QString(),
                           QStringLiteral("Decodium")).toString();
    m_customTextColor =
        profiledThemeValue(QStringLiteral("theme/customText"), QString(),
                           QStringLiteral("Decodium")).toString();
}

// 1.0.305 (#6) — override colori UI ----------------------------------------
QColor DecodiumThemeManager::customBg() const
{
    if (!m_customColorsEnabled) return QColor();
    QColor c(m_customBgColor);
    return c.isValid() ? c : QColor();
}

QColor DecodiumThemeManager::customText() const
{
    if (!m_customColorsEnabled) return QColor();
    QColor c(m_customTextColor);
    return c.isValid() ? c : QColor();
}

// Sfumatura "elevazione" per i livelli di sfondo: su uno sfondo scuro schiarisce,
// su uno chiaro scurisce leggermente → i pannelli restano leggibili su qualsiasi base.
QColor DecodiumThemeManager::elevate(const QColor& base, double factor)
{
    if (!base.isValid()) return base;
    return base.lightnessF() < 0.5
        ? base.lighter(static_cast<int>(100 + factor * 100))
        : base.darker(static_cast<int>(100 + factor * 45));
}

void DecodiumThemeManager::setCustomColorsEnabled(bool v)
{
    if (m_customColorsEnabled == v) return;
    m_customColorsEnabled = v;
    setProfiledThemeValue(QStringLiteral("theme/customEnabled"), v,
                          QStringLiteral("Decodium"));
    emit paletteChanged();
}

void DecodiumThemeManager::setCustomBgColor(const QString& hex)
{
    QString const h = hex.trimmed();
    if (m_customBgColor == h) return;
    m_customBgColor = h;
    setProfiledThemeValue(QStringLiteral("theme/customBg"), h,
                          QStringLiteral("Decodium"));
    if (m_customColorsEnabled) emit paletteChanged();
}

void DecodiumThemeManager::setCustomTextColor(const QString& hex)
{
    QString const h = hex.trimmed();
    if (m_customTextColor == h) return;
    m_customTextColor = h;
    setProfiledThemeValue(QStringLiteral("theme/customText"), h,
                          QStringLiteral("Decodium"));
    if (m_customColorsEnabled) emit paletteChanged();
}

const DecodiumThemeManager::ThemePalette& DecodiumThemeManager::currentPalette() const
{
    if (m_currentTheme == "Stellar Light") return s_stellarLight;
    if (m_currentTheme == "Darkcodium")   return s_dxPedition;
    return s_oceanBlue;
}

void DecodiumThemeManager::setCurrentTheme(const QString& name)
{
    if (m_currentTheme == name) return;
    if (name != "Ocean Blue" && name != "Stellar Light" && name != "Darkcodium") return;
    m_currentTheme = name;
    setProfiledThemeValue(QStringLiteral("theme/current"), name,
                          QStringLiteral("Decodium"));
    emit currentThemeChanged();
    emit paletteChanged();
}

// DX-Pedition Fase 1 — accent swappabile + densità ---------------------------
void DecodiumThemeManager::accentTriple(QColor& accent, QColor& dim, QColor& deep) const
{
    // Default = phosphor (anche se per qualche motivo il variant è ignoto).
    if (m_accentVariant == "cyan") {
        accent = QColor("#66e6ff"); dim = QColor("#1b9fcc"); deep = QColor("#04222d");
    } else if (m_accentVariant == "amber") {
        accent = QColor("#ffb820"); dim = QColor("#a06d10"); deep = QColor("#2e1d04");
    } else if (m_accentVariant == "red") {
        accent = QColor("#ff5466"); dim = QColor("#a82c3a"); deep = QColor("#2e090f");
    } else { // phosphor
        accent = QColor("#19ff88"); dim = QColor("#0fa55a"); deep = QColor("#052d1a");
    }
}

void DecodiumThemeManager::setAccentVariant(const QString& name)
{
    QString const n = name.trimmed();
    if (n != "phosphor" && n != "cyan" && n != "amber" && n != "red") return;
    if (m_accentVariant == n) return;
    m_accentVariant = n;
    setProfiledThemeValue(QStringLiteral("ThemeAccentVariant"), n,
                          QStringLiteral("Decodium3"));
    // Influisce sui colori solo quando il tema DX-Pedition è attivo, ma emettiamo
    // comunque: i binding QML restano corretti e l'effetto è nullo sugli altri temi.
    emit paletteChanged();
}

void DecodiumThemeManager::setDensity(const QString& name)
{
    QString const n = name.trimmed();
    if (n != "compact" && n != "regular" && n != "comfy") return;
    if (m_density == n) return;
    m_density = n;
    setProfiledThemeValue(QStringLiteral("ThemeDensity"), n,
                          QStringLiteral("Decodium3"));
    emit densityChanged();
}

int DecodiumThemeManager::densityRowHeight() const
{
    // "Row pad" → altezza riga effettiva indicativa (compact/regular/comfy).
    if (m_density == "compact") return 22;
    if (m_density == "comfy")   return 30;
    return 26; // regular
}

int DecodiumThemeManager::densityFontSize() const
{
    if (m_density == "compact") return 11;
    if (m_density == "comfy")   return 13;
    return 12; // regular
}

int DecodiumThemeManager::densityPanelHeight() const
{
    if (m_density == "compact") return 26;
    if (m_density == "comfy")   return 38;
    return 30; // regular
}

bool DecodiumThemeManager::isLightTheme() const
{
    QColor const c = customBg();
    if (c.isValid()) return c.lightnessF() >= 0.5;
    return currentPalette().isLight;
}
QColor DecodiumThemeManager::bgDeep() const
{
    QColor const c = customBg();
    return c.isValid() ? c : currentPalette().bgDeep;
}
QColor DecodiumThemeManager::bgMedium() const
{
    QColor const c = customBg();
    return c.isValid() ? elevate(c, 0.18) : currentPalette().bgMedium;
}
QColor DecodiumThemeManager::bgLight() const
{
    QColor const c = customBg();
    return c.isValid() ? elevate(c, 0.40) : currentPalette().bgLight;
}
QColor DecodiumThemeManager::primaryColor()   const { return currentPalette().primaryColor; }
QColor DecodiumThemeManager::secondaryColor() const { return currentPalette().secondaryColor; }
QColor DecodiumThemeManager::accentColor() const
{
    if (m_currentTheme == "Darkcodium") {
        QColor a, d, p; accentTriple(a, d, p); return a;
    }
    return currentPalette().accentColor;
}
QColor DecodiumThemeManager::warningColor()   const { return currentPalette().warningColor; }
QColor DecodiumThemeManager::errorColor()     const { return currentPalette().errorColor; }
QColor DecodiumThemeManager::textPrimary() const
{
    QColor const c = customText();
    return c.isValid() ? c : currentPalette().textPrimary;
}
QColor DecodiumThemeManager::textSecondary() const
{
    QColor c = customText();
    if (c.isValid()) { c.setAlphaF(0.62); return c; }
    return currentPalette().textSecondary;
}
QColor DecodiumThemeManager::successColor()   const { return currentPalette().successColor; }
QColor DecodiumThemeManager::glassOverlay()   const { return currentPalette().glassOverlay; }
QColor DecodiumThemeManager::glassBorder()    const { return currentPalette().glassBorder; }
QColor DecodiumThemeManager::borderColor()    const { return currentPalette().borderColor; }
QColor DecodiumThemeManager::borderSoft()     const { return currentPalette().borderSoft; }
QColor DecodiumThemeManager::panelColor() const
{
    QColor const c = customBg();
    return c.isValid() ? elevate(c, 0.40) : currentPalette().panelColor;
}
QColor DecodiumThemeManager::panelHeader() const
{
    QColor const c = customBg();
    return c.isValid() ? elevate(c, 0.26) : currentPalette().panelHeader;
}
QColor DecodiumThemeManager::rowMatchBg()     const { return currentPalette().rowMatchBg; }
QColor DecodiumThemeManager::rowMatchBorder() const { return currentPalette().rowMatchBorder; }
QColor DecodiumThemeManager::ledRed()         const { return currentPalette().ledRed; }
QColor DecodiumThemeManager::ledBlue()        const { return currentPalette().ledBlue; }
QColor DecodiumThemeManager::ledYellow()      const { return currentPalette().ledYellow; }
QColor DecodiumThemeManager::ledMagenta()     const { return currentPalette().ledMagenta; }

// DX-Pedition Fase 1 — token extra. Per il tema DX-Pedition usano i token design
// (accentDim/Deep seguono il variant); per Ocean Blue/Stellar derivano fallback
// ragionevoli dai getter esistenti, così quei temi restano invariati e validi.
QColor DecodiumThemeManager::accentDim() const
{
    if (m_currentTheme == "Darkcodium") {
        QColor a, d, p; accentTriple(a, d, p); return d;
    }
    return accentColor().darker(160);
}
QColor DecodiumThemeManager::accentDeep() const
{
    if (m_currentTheme == "Darkcodium") {
        QColor a, d, p; accentTriple(a, d, p); return p;
    }
    return accentColor().darker(420);
}
QColor DecodiumThemeManager::pileColor() const
{
    QColor const c = currentPalette().pileColor;
    return c.isValid() ? c : currentPalette().secondaryColor;
}
QColor DecodiumThemeManager::gridColor() const
{
    QColor const c = currentPalette().gridColor;
    return c.isValid() ? c : currentPalette().successColor;
}
QColor DecodiumThemeManager::txColor() const
{
    QColor const c = currentPalette().txColor;
    return c.isValid() ? c : currentPalette().warningColor;
}
QColor DecodiumThemeManager::rxColor() const
{
    QColor const c = currentPalette().rxColor;
    return c.isValid() ? c : currentPalette().successColor;
}
