#include "DecodiumThemeManager.h"
#include "DecodiumProfileSettings.h"

#include <QGuiApplication>
#include <QSettings>

#include <algorithm>

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

bool DecodiumThemeManager::glassTransparencySupported() const
{
    // A deployment or a user can force the deterministic opaque fallback.
    // This is useful for X11 sessions started without a compositor and for
    // remote/virtual desktops that advertise alpha windows but render them
    // as black surfaces.
    if (qEnvironmentVariableIsSet("DECODIUM_FORCE_OPAQUE_GLASS"))
        return false;

    const QString platform = QGuiApplication::platformName().toLower();
    if (platform.isEmpty() || platform == QStringLiteral("minimal")
        || platform == QStringLiteral("offscreen")
        || platform == QStringLiteral("minimalegl")) {
        return false;
    }

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    return true;
#elif defined(Q_OS_LINUX)
    // A transparent top-level QQuickWindow is not reliable across the Linux
    // compositor/driver matrix.  In particular, some Mesa/X11 and Wayland
    // combinations feed a translucent client surface back into the compositor
    // and can recurse the window image until the GPU is saturated.  This was
    // observed after opening then closing a CW or SSB pop-out: the next start
    // could display nested Decodium frames and freeze the desktop.
    //
    // Keep the Liquid Glass palette, but make its native surfaces opaque on
    // Linux by default.  Advanced users with a known-good compositor can
    // explicitly opt in; the safe default must not depend on driver probing
    // that Qt cannot provide.
    return qEnvironmentVariableIsSet("DECODIUM_ENABLE_LINUX_GLASS")
        && (platform.startsWith(QStringLiteral("wayland"))
            || platform.startsWith(QStringLiteral("xcb")));
#else
    return false;
#endif
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
    /* bgDeep         */ QColor("#D4DCE8"),   // fondo pagina
    /* bgMedium       */ QColor("#CFD8E5"),   // fasce e barre
    /* bgLight        */ QColor("#E9EEF5"),   // schede (bianco spento)
    /* primaryColor   */ QColor("#0757A8"),   // blu dei comandi attivi
    /* secondaryColor */ QColor("#005C68"),   // teal scuro, leggibile su chiaro
    /* accentColor    */ QColor("#05603F"),   // verde ad alto contrasto
    /* warningColor   */ QColor("#7C4C00"),
    /* errorColor     */ QColor("#A81D2B"),
    /* textPrimary    */ QColor("#14233B"),   // blu notte quasi nero
    /* textSecondary  */ QColor("#34466A"),   // testo secondario leggibile
    /* successColor   */ QColor("#05603F"),
    /* glassOverlay   */ QColor(233, 238, 245, 214),
    /* glassBorder    */ QColor("#A9B7CD"),   // filetti sottili
    /* borderColor    */ QColor("#9FAEC6"),
    /* borderSoft     */ QColor("#B6C3D6"),
    /* panelColor     */ QColor("#E9EEF5"),
    /* panelHeader    */ QColor("#DDE4EE"),
    /* rowMatchBg     */ QColor(5, 96, 63, 36),
    /* rowMatchBorder */ QColor("#05603F"),
    /* ledRed         */ QColor("#A81D2B"),
    /* ledBlue        */ QColor("#0757A8"),
    /* ledYellow      */ QColor("#7C4C00"),
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

// Liquid Glass uses translucent panel tokens over an opaque fallback
// gradient. This keeps the appearance stable on compositors without native
// backdrop/material support while still giving rounded/glass panels a layered
// look where the QML surface supports it.
const DecodiumThemeManager::ThemePalette DecodiumThemeManager::s_liquidGlass {
    /* bgDeep         */ QColor("#0A1020"),
    /* bgMedium       */ QColor("#14223A"),
    /* bgLight        */ QColor("#233B60"),
    /* primaryColor   */ QColor("#EAF6FF"),
    /* secondaryColor */ QColor("#A8C7E8"),
    /* accentColor    */ QColor("#8DEBFF"),
    /* warningColor   */ QColor("#FFD18A"),
    /* errorColor     */ QColor("#FF8296"),
    /* textPrimary    */ QColor("#F5FAFF"),
    /* textSecondary  */ QColor("#AFC6DF"),
    /* successColor   */ QColor("#7CF4C8"),
    /* glassOverlay   */ QColor(235, 247, 255, 38),
    /* glassBorder    */ QColor(220, 242, 255, 112),
    /* borderColor    */ QColor(220, 242, 255, 82),
    /* borderSoft     */ QColor(220, 242, 255, 42),
    /* panelColor     */ QColor(25, 43, 72, 176),
    /* panelHeader    */ QColor(38, 62, 98, 208),
    /* rowMatchBg     */ QColor(141, 235, 255, 42),
    /* rowMatchBorder */ QColor("#8DEBFF"),
    /* ledRed         */ QColor("#FF8296"),
    /* ledBlue        */ QColor("#A8C7E8"),
    /* ledYellow      */ QColor("#FFD18A"),
    /* ledMagenta     */ QColor("#FF9CCB"),
    /* accentDim      */ QColor("#4E9CB4"),
    /* accentDeep     */ QColor("#123A4A"),
    /* pileColor      */ QColor("#A8C7E8"),
    /* gridColor      */ QColor("#8DEBFF"),
    /* txColor        */ QColor("#FF9CCB"),
    /* rxColor        */ QColor("#7CF4C8"),
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
    if (stored != "Ocean Blue" && stored != "Stellar Light" && stored != "Darkcodium"
        && stored != "Liquid Glass")
        stored = startupTheme;
    m_currentTheme = stored;
    m_lookSaveTimer.setSingleShot(true);
    m_lookSaveTimer.setInterval(600);
    connect(&m_lookSaveTimer, &QTimer::timeout, this, [this]() { flushLook(); });
    loadLookForTheme();
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

QString DecodiumThemeManager::accessibleDecodeColor(const QString& prop) const
{
    if (!isLightTheme())
        return {};

    // These colours are deliberately dark, sufficiently separated by hue,
    // and readable on the light decode rows, including for common colour-
    // vision deficiencies. Keep this mapping central so the classic decode
    // window and the DX-Pedition panels cannot drift apart again.
    if (prop == QStringLiteral("colorDecodeText")) return QStringLiteral("#243746");
    if (prop == QStringLiteral("colorCQ")) return QStringLiteral("#006B52");
    if (prop == QStringLiteral("colorDXEntity")) return QStringLiteral("#8A4B00");
    if (prop == QStringLiteral("colorNewDxcc")) return QStringLiteral("#7A174E");
    if (prop == QStringLiteral("colorNewDxccBand")) return QStringLiteral("#8A2F62");
    if (prop == QStringLiteral("colorNewContinent")) return QStringLiteral("#8A1F3D");
    if (prop == QStringLiteral("colorNewContinentBand")) return QStringLiteral("#7A3150");
    if (prop == QStringLiteral("colorNewCqZone")) return QStringLiteral("#8A4700");
    if (prop == QStringLiteral("colorNewCqZoneBand")) return QStringLiteral("#7A4A16");
    if (prop == QStringLiteral("colorNewItuZone")) return QStringLiteral("#426B00");
    if (prop == QStringLiteral("colorNewItuZoneBand")) return QStringLiteral("#4C6413");
    if (prop == QStringLiteral("colorNewGrid")) return QStringLiteral("#8A3D00");
    if (prop == QStringLiteral("colorNewGridBand")) return QStringLiteral("#7A4315");
    if (prop == QStringLiteral("colorNewCall")) return QStringLiteral("#006078");
    if (prop == QStringLiteral("colorNewCallBand")) return QStringLiteral("#176073");
    if (prop == QStringLiteral("color73")) return QStringLiteral("#1456A0");
    if (prop == QStringLiteral("colorB4")) return QStringLiteral("#4A5560");
    if (prop == QStringLiteral("colorLotwUser")) return QStringLiteral("#4A207A");
    if (prop == QStringLiteral("colorTxMessage")) return QStringLiteral("#8A4B00");
    if (prop == QStringLiteral("colorMyCall")) return QStringLiteral("#9B1C1C");
    return {};
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

// Luminosita' / contrasto -----------------------------------------------------
void DecodiumThemeManager::loadLookForTheme()
{
    m_brightness = std::clamp(profiledThemeValue(
        QStringLiteral("theme/brightness/") + m_currentTheme, 0,
        QStringLiteral("Decodium")).toInt(), -100, 100);
    m_contrast = std::clamp(profiledThemeValue(
        QStringLiteral("theme/contrast/") + m_currentTheme, 0,
        QStringLiteral("Decodium")).toInt(), -100, 100);
}

void DecodiumThemeManager::setBrightness(int v)
{
    v = std::clamp(v, -100, 100);
    if (m_brightness == v) return;
    m_brightness = v;
    scheduleLookSave();
    emit paletteChanged();
}

void DecodiumThemeManager::setContrast(int v)
{
    v = std::clamp(v, -100, 100);
    if (m_contrast == v) return;
    m_contrast = v;
    scheduleLookSave();
    emit paletteChanged();
}

void DecodiumThemeManager::resetLook()
{
    if (m_brightness == 0 && m_contrast == 0) return;
    m_brightness = 0;
    m_contrast = 0;
    scheduleLookSave();
    emit paletteChanged();
}

// Il cursore QML emette decine di valori al secondo: il file INI del profilo viene
// scritto una sola volta a trascinamento finito.
void DecodiumThemeManager::scheduleLookSave()
{
    m_lookDirty = true;
    m_lookSaveTimer.start();
}

void DecodiumThemeManager::flushLook()
{
    m_lookSaveTimer.stop();
    if (!m_lookDirty) return;
    m_lookDirty = false;
    setProfiledThemeValue(QStringLiteral("theme/brightness/") + m_currentTheme, m_brightness,
                          QStringLiteral("Decodium"));
    setProfiledThemeValue(QStringLiteral("theme/contrast/") + m_currentTheme, m_contrast,
                          QStringLiteral("Decodium"));
}

double DecodiumThemeManager::rawBgDeepLightness() const
{
    QColor const c = customBg();
    return (c.isValid() ? c : currentPalette().bgDeep).lightnessF();
}

namespace
{
QColor withLightness(const QColor& c, double l)
{
    float h, s, ol, a;
    c.getHslF(&h, &s, &ol, &a);
    return QColor::fromHslF(h < 0 ? 0.0f : h, s, static_cast<float>(std::clamp(l, 0.0, 1.0)), a);
}
}

QColor DecodiumThemeManager::adjustSurface(const QColor& c) const
{
    if (!c.isValid() || (m_brightness == 0 && m_contrast == 0)) return c;
    double const k = 1.0 + m_contrast / 100.0 * 0.45;
    double const refRaw = rawBgDeepLightness();
    double const refAdj = std::clamp(refRaw + m_brightness / 100.0 * 0.20, 0.0, 1.0);
    return withLightness(c, refAdj + (c.lightnessF() - refRaw) * k);
}

QColor DecodiumThemeManager::adjustForeground(const QColor& c) const
{
    if (!c.isValid() || (m_brightness == 0 && m_contrast == 0)) return c;
    double const k = 1.0 + m_contrast / 100.0 * 0.45;
    double const refRaw = rawBgDeepLightness();
    double const refAdj = std::clamp(refRaw + m_brightness / 100.0 * 0.20, 0.0, 1.0);
    return withLightness(c, refAdj + (c.lightnessF() - refAdj) * k);
}

const DecodiumThemeManager::ThemePalette& DecodiumThemeManager::currentPalette() const
{
    if (m_currentTheme == "Stellar Light") return s_stellarLight;
    if (m_currentTheme == "Darkcodium")   return s_dxPedition;
    if (m_currentTheme == "Liquid Glass") return s_liquidGlass;
    return s_oceanBlue;
}

void DecodiumThemeManager::setCurrentTheme(const QString& name)
{
    if (m_currentTheme == name) return;
    if (name != "Ocean Blue" && name != "Stellar Light" && name != "Darkcodium"
        && name != "Liquid Glass") return;
    flushLook();
    m_currentTheme = name;
    loadLookForTheme();
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
    return adjustSurface(c.isValid() ? c : currentPalette().bgDeep);
}
QColor DecodiumThemeManager::bgMedium() const
{
    QColor const c = customBg();
    return adjustSurface(c.isValid() ? elevate(c, 0.18) : currentPalette().bgMedium);
}
QColor DecodiumThemeManager::bgLight() const
{
    QColor const c = customBg();
    return adjustSurface(c.isValid() ? elevate(c, 0.40) : currentPalette().bgLight);
}
QColor DecodiumThemeManager::primaryColor()   const { return adjustForeground(currentPalette().primaryColor); }
QColor DecodiumThemeManager::secondaryColor() const { return adjustForeground(currentPalette().secondaryColor); }
QColor DecodiumThemeManager::accentColor() const
{
    if (m_currentTheme == "Darkcodium") {
        QColor a, d, p; accentTriple(a, d, p); return adjustForeground(a);
    }
    return adjustForeground(currentPalette().accentColor);
}
QColor DecodiumThemeManager::warningColor()   const { return adjustForeground(currentPalette().warningColor); }
QColor DecodiumThemeManager::errorColor()     const { return adjustForeground(currentPalette().errorColor); }
QColor DecodiumThemeManager::textPrimary() const
{
    QColor const c = customText();
    return adjustForeground(c.isValid() ? c : currentPalette().textPrimary);
}
QColor DecodiumThemeManager::textSecondary() const
{
    QColor c = customText();
    if (c.isValid()) { c.setAlphaF(0.62); return adjustForeground(c); }
    return adjustForeground(currentPalette().textSecondary);
}
QColor DecodiumThemeManager::successColor()   const { return adjustForeground(currentPalette().successColor); }
QColor DecodiumThemeManager::glassOverlay()   const { return adjustSurface(currentPalette().glassOverlay); }
QColor DecodiumThemeManager::glassBorder()    const { return adjustSurface(currentPalette().glassBorder); }
QColor DecodiumThemeManager::borderColor()    const { return adjustSurface(currentPalette().borderColor); }
QColor DecodiumThemeManager::borderSoft()     const { return adjustSurface(currentPalette().borderSoft); }
QColor DecodiumThemeManager::panelColor() const
{
    QColor const c = customBg();
    return adjustSurface(c.isValid() ? elevate(c, 0.40) : currentPalette().panelColor);
}
QColor DecodiumThemeManager::panelHeader() const
{
    QColor const c = customBg();
    return adjustSurface(c.isValid() ? elevate(c, 0.26) : currentPalette().panelHeader);
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
