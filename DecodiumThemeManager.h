#pragma once
#include <QObject>
#include <QColor>
#include <QString>
#include <QStringList>

class DecodiumThemeManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QColor bgDeep         READ bgDeep         NOTIFY paletteChanged)
    Q_PROPERTY(QColor bgMedium       READ bgMedium       NOTIFY paletteChanged)
    Q_PROPERTY(QColor bgLight        READ bgLight        NOTIFY paletteChanged)
    Q_PROPERTY(QColor primaryColor   READ primaryColor   NOTIFY paletteChanged)
    Q_PROPERTY(QColor secondaryColor READ secondaryColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor accentColor    READ accentColor    NOTIFY paletteChanged)
    Q_PROPERTY(QColor warningColor   READ warningColor   NOTIFY paletteChanged)
    Q_PROPERTY(QColor errorColor     READ errorColor     NOTIFY paletteChanged)
    Q_PROPERTY(QColor textPrimary    READ textPrimary    NOTIFY paletteChanged)
    Q_PROPERTY(QColor textSecondary  READ textSecondary  NOTIFY paletteChanged)
    Q_PROPERTY(QColor successColor   READ successColor   NOTIFY paletteChanged)
    Q_PROPERTY(QColor glassOverlay   READ glassOverlay   NOTIFY paletteChanged)
    Q_PROPERTY(QColor glassBorder    READ glassBorder    NOTIFY paletteChanged)
    Q_PROPERTY(QColor borderColor    READ borderColor    NOTIFY paletteChanged)
    Q_PROPERTY(QColor panelColor     READ panelColor     NOTIFY paletteChanged)
    Q_PROPERTY(QColor rowMatchBg     READ rowMatchBg     NOTIFY paletteChanged)
    Q_PROPERTY(QColor rowMatchBorder READ rowMatchBorder NOTIFY paletteChanged)
    Q_PROPERTY(QColor ledRed         READ ledRed         NOTIFY paletteChanged)
    Q_PROPERTY(QColor ledBlue        READ ledBlue        NOTIFY paletteChanged)
    Q_PROPERTY(QColor ledYellow      READ ledYellow      NOTIFY paletteChanged)
    Q_PROPERTY(QColor ledMagenta     READ ledMagenta     NOTIFY paletteChanged)
    Q_PROPERTY(QStringList availableThemes READ availableThemes CONSTANT)
    Q_PROPERTY(QString currentTheme  READ currentTheme   WRITE setCurrentTheme NOTIFY currentThemeChanged)
    Q_PROPERTY(bool   isLightTheme   READ isLightTheme   NOTIFY paletteChanged)

public:
    explicit DecodiumThemeManager(QObject* parent = nullptr);

    QColor bgDeep()         const;
    QColor bgMedium()       const;
    QColor bgLight()        const;
    QColor primaryColor()   const;
    QColor secondaryColor() const;
    QColor accentColor()    const;
    QColor warningColor()   const;
    QColor errorColor()     const;
    QColor textPrimary()    const;
    QColor textSecondary()  const;
    QColor successColor()   const;
    QColor glassOverlay()   const;
    QColor glassBorder()    const;
    QColor borderColor()    const;
    QColor panelColor()     const;
    QColor rowMatchBg()     const;
    QColor rowMatchBorder() const;
    QColor ledRed()         const;
    QColor ledBlue()        const;
    QColor ledYellow()      const;
    QColor ledMagenta()     const;

    QStringList availableThemes() const { return {"Ocean Blue", "Stellar Light"}; }
    QString currentTheme() const { return m_currentTheme; }
    void setCurrentTheme(const QString& name);
    bool isLightTheme() const;

public slots:
    Q_INVOKABLE void applyThemeByName(const QString& name) { setCurrentTheme(name); }

signals:
    void currentThemeChanged();
    void paletteChanged();

private:
    struct ThemePalette {
        QColor bgDeep;
        QColor bgMedium;
        QColor bgLight;
        QColor primaryColor;
        QColor secondaryColor;
        QColor accentColor;
        QColor warningColor;
        QColor errorColor;
        QColor textPrimary;
        QColor textSecondary;
        QColor successColor;
        QColor glassOverlay;
        QColor glassBorder;
        QColor borderColor;
        QColor panelColor;
        QColor rowMatchBg;
        QColor rowMatchBorder;
        QColor ledRed;
        QColor ledBlue;
        QColor ledYellow;
        QColor ledMagenta;
        bool   isLight;
    };

    static const ThemePalette s_oceanBlue;
    static const ThemePalette s_stellarLight;

    const ThemePalette& currentPalette() const;

    QString m_currentTheme {"Ocean Blue"};
};
