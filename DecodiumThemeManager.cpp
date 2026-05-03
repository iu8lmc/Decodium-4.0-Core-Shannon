#include "DecodiumThemeManager.h"
#include <QSettings>

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
    /* panelColor     */ QColor("#1E2D42"),
    /* rowMatchBg     */ QColor(0, 255, 136, 38),
    /* rowMatchBorder */ QColor("#00FF88"),
    /* ledRed         */ QColor("#FF5F56"),
    /* ledBlue        */ QColor("#4A90E2"),
    /* ledYellow      */ QColor("#FFD700"),
    /* ledMagenta     */ QColor("#FF00FF"),
    /* isLight        */ false
};

const DecodiumThemeManager::ThemePalette DecodiumThemeManager::s_stellarLight {
    /* bgDeep         */ QColor("#EEF3F7"),
    /* bgMedium       */ QColor("#E4ECF3"),
    /* bgLight        */ QColor("#FFFFFF"),
    /* primaryColor   */ QColor("#6E9DD1"),
    /* secondaryColor */ QColor("#67B7C9"),
    /* accentColor    */ QColor("#5EAE82"),
    /* warningColor   */ QColor("#D9B154"),
    /* errorColor     */ QColor("#D86A6A"),
    /* textPrimary    */ QColor("#0F2235"),
    /* textSecondary  */ QColor("#6B829A"),
    /* successColor   */ QColor("#5EAE82"),
    /* glassOverlay   */ QColor(255, 255, 255, 180),
    /* glassBorder    */ QColor(201, 214, 226, 200),
    /* borderColor    */ QColor("#C9D6E2"),
    /* panelColor     */ QColor("#FFFFFF"),
    /* rowMatchBg     */ QColor("#D7EFDF"),
    /* rowMatchBorder */ QColor("#5CAE7F"),
    /* ledRed         */ QColor("#D86A6A"),
    /* ledBlue        */ QColor("#6E9DD1"),
    /* ledYellow      */ QColor("#D9B154"),
    /* ledMagenta     */ QColor("#9C8AC9"),
    /* isLight        */ true
};

DecodiumThemeManager::DecodiumThemeManager(QObject* parent)
    : QObject(parent)
{
    QSettings s("Decodium", "Decodium");
    QString const stored = s.value("theme/current", "Ocean Blue").toString();
    if (stored == "Stellar Light" || stored == "Ocean Blue") {
        m_currentTheme = stored;
    } else {
        m_currentTheme = "Ocean Blue";
    }
}

const DecodiumThemeManager::ThemePalette& DecodiumThemeManager::currentPalette() const
{
    if (m_currentTheme == "Stellar Light") return s_stellarLight;
    return s_oceanBlue;
}

void DecodiumThemeManager::setCurrentTheme(const QString& name)
{
    if (m_currentTheme == name) return;
    if (name != "Ocean Blue" && name != "Stellar Light") return;
    m_currentTheme = name;
    QSettings("Decodium", "Decodium").setValue("theme/current", name);
    emit currentThemeChanged();
    emit paletteChanged();
}

bool   DecodiumThemeManager::isLightTheme()   const { return currentPalette().isLight; }
QColor DecodiumThemeManager::bgDeep()         const { return currentPalette().bgDeep; }
QColor DecodiumThemeManager::bgMedium()       const { return currentPalette().bgMedium; }
QColor DecodiumThemeManager::bgLight()        const { return currentPalette().bgLight; }
QColor DecodiumThemeManager::primaryColor()   const { return currentPalette().primaryColor; }
QColor DecodiumThemeManager::secondaryColor() const { return currentPalette().secondaryColor; }
QColor DecodiumThemeManager::accentColor()    const { return currentPalette().accentColor; }
QColor DecodiumThemeManager::warningColor()   const { return currentPalette().warningColor; }
QColor DecodiumThemeManager::errorColor()     const { return currentPalette().errorColor; }
QColor DecodiumThemeManager::textPrimary()    const { return currentPalette().textPrimary; }
QColor DecodiumThemeManager::textSecondary()  const { return currentPalette().textSecondary; }
QColor DecodiumThemeManager::successColor()   const { return currentPalette().successColor; }
QColor DecodiumThemeManager::glassOverlay()   const { return currentPalette().glassOverlay; }
QColor DecodiumThemeManager::glassBorder()    const { return currentPalette().glassBorder; }
QColor DecodiumThemeManager::borderColor()    const { return currentPalette().borderColor; }
QColor DecodiumThemeManager::panelColor()     const { return currentPalette().panelColor; }
QColor DecodiumThemeManager::rowMatchBg()     const { return currentPalette().rowMatchBg; }
QColor DecodiumThemeManager::rowMatchBorder() const { return currentPalette().rowMatchBorder; }
QColor DecodiumThemeManager::ledRed()         const { return currentPalette().ledRed; }
QColor DecodiumThemeManager::ledBlue()        const { return currentPalette().ledBlue; }
QColor DecodiumThemeManager::ledYellow()      const { return currentPalette().ledYellow; }
QColor DecodiumThemeManager::ledMagenta()     const { return currentPalette().ledMagenta; }
