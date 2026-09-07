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
    Q_PROPERTY(QColor borderSoft     READ borderSoft     NOTIFY paletteChanged)
    Q_PROPERTY(QColor panelColor     READ panelColor     NOTIFY paletteChanged)
    Q_PROPERTY(QColor panelHeader    READ panelHeader    NOTIFY paletteChanged)
    Q_PROPERTY(QColor rowMatchBg     READ rowMatchBg     NOTIFY paletteChanged)
    Q_PROPERTY(QColor rowMatchBorder READ rowMatchBorder NOTIFY paletteChanged)
    Q_PROPERTY(QColor ledRed         READ ledRed         NOTIFY paletteChanged)
    Q_PROPERTY(QColor ledBlue        READ ledBlue        NOTIFY paletteChanged)
    Q_PROPERTY(QColor ledYellow      READ ledYellow      NOTIFY paletteChanged)
    Q_PROPERTY(QColor ledMagenta     READ ledMagenta     NOTIFY paletteChanged)
    // DX-Pedition Fase 1 — token aggiuntivi (NOTIFY paletteChanged così seguono accent/tema)
    Q_PROPERTY(QColor accentDim      READ accentDim      NOTIFY paletteChanged)
    Q_PROPERTY(QColor accentDeep     READ accentDeep     NOTIFY paletteChanged)
    Q_PROPERTY(QColor pileColor      READ pileColor      NOTIFY paletteChanged)
    Q_PROPERTY(QColor gridColor      READ gridColor      NOTIFY paletteChanged)
    Q_PROPERTY(QColor txColor        READ txColor        NOTIFY paletteChanged)
    Q_PROPERTY(QColor rxColor        READ rxColor        NOTIFY paletteChanged)
    Q_PROPERTY(QStringList availableThemes READ availableThemes CONSTANT)
    Q_PROPERTY(QString currentTheme  READ currentTheme   WRITE setCurrentTheme NOTIFY currentThemeChanged)
    Q_PROPERTY(bool   isLightTheme   READ isLightTheme   NOTIFY paletteChanged)
    // 1.0.305 (#6) — colori UI personalizzati (sfondo + testo) come in v3, sopra il tema.
    Q_PROPERTY(bool    customColorsEnabled READ customColorsEnabled WRITE setCustomColorsEnabled NOTIFY paletteChanged)
    Q_PROPERTY(QString customBgColor       READ customBgColor       WRITE setCustomBgColor       NOTIFY paletteChanged)
    Q_PROPERTY(QString customTextColor     READ customTextColor     WRITE setCustomTextColor     NOTIFY paletteChanged)
    // DX-Pedition Fase 1 — accent swappabile (solo tema "Darkcodium") + densità (metrica)
    Q_PROPERTY(QString accentVariant       READ accentVariant       WRITE setAccentVariant       NOTIFY paletteChanged)
    Q_PROPERTY(QString density             READ density             WRITE setDensity             NOTIFY densityChanged)

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
    QColor borderSoft()     const;
    QColor panelColor()     const;
    QColor panelHeader()    const;
    QColor rowMatchBg()     const;
    QColor rowMatchBorder() const;
    QColor ledRed()         const;
    QColor ledBlue()        const;
    QColor ledYellow()      const;
    QColor ledMagenta()     const;
    // DX-Pedition Fase 1 — token aggiuntivi
    QColor accentDim()      const;
    QColor accentDeep()     const;
    QColor pileColor()      const;
    QColor gridColor()      const;
    QColor txColor()        const;
    QColor rxColor()        const;

    QStringList availableThemes() const { return {"Ocean Blue", "Stellar Light", "Darkcodium"}; }
    QString currentTheme() const { return m_currentTheme; }
    void setCurrentTheme(const QString& name);
    bool isLightTheme() const;

    // 1.0.305 (#6) — override colori UI (sfondo+testo). Stringhe hex "#RRGGBB" (vuote = tema).
    bool    customColorsEnabled() const { return m_customColorsEnabled; }
    void    setCustomColorsEnabled(bool v);
    QString customBgColor()   const { return m_customBgColor; }
    void    setCustomBgColor(const QString& hex);
    QString customTextColor() const { return m_customTextColor; }
    void    setCustomTextColor(const QString& hex);

    // DX-Pedition Fase 1 — accent variant (phosphor/cyan/amber/red) + densità
    QString accentVariant() const { return m_accentVariant; }
    void    setAccentVariant(const QString& name);
    QString density() const { return m_density; }
    void    setDensity(const QString& name);
    // Helper metrica densità (compact/regular/comfy) — non colori.
    Q_INVOKABLE int densityRowHeight()   const;
    Q_INVOKABLE int densityFontSize()    const;
    Q_INVOKABLE int densityPanelHeight() const;

public slots:
    Q_INVOKABLE void applyThemeByName(const QString& name) { setCurrentTheme(name); }

signals:
    void currentThemeChanged();
    void paletteChanged();
    void densityChanged();

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
        QColor borderSoft;
        QColor panelColor;
        QColor panelHeader;
        QColor rowMatchBg;
        QColor rowMatchBorder;
        QColor ledRed;
        QColor ledBlue;
        QColor ledYellow;
        QColor ledMagenta;
        // DX-Pedition Fase 1 — token extra (gli altri temi lasciano invalidi → fallback derivati)
        QColor accentDim;
        QColor accentDeep;
        QColor pileColor;
        QColor gridColor;
        QColor txColor;
        QColor rxColor;
        bool   isLight;
    };

    static const ThemePalette s_oceanBlue;
    static const ThemePalette s_stellarLight;
    static const ThemePalette s_dxPedition;

    const ThemePalette& currentPalette() const;
    // DX-Pedition Fase 1 — tripletta accent del variant scelto (solo tema DX-Pedition)
    void accentTriple(QColor& accent, QColor& dim, QColor& deep) const;

    // 1.0.305 (#6) — colori custom: override valido solo se enabled + hex parsabile.
    QColor customBg() const;    // QColor(m_customBgColor) se enabled+valido, altrimenti invalido
    QColor customText() const;
    static QColor elevate(const QColor& base, double factor);  // sfumatura lightness-aware per i livelli bg

    QString m_currentTheme {"Ocean Blue"};
    bool    m_customColorsEnabled {false};
    QString m_customBgColor;    // "#RRGGBB" o vuoto
    QString m_customTextColor;
    // DX-Pedition Fase 1
    QString m_accentVariant {"phosphor"};   // phosphor/cyan/amber/red — usato solo dal tema DX-Pedition
    QString m_density {"regular"};           // compact/regular/comfy — metrica, non colore
};
