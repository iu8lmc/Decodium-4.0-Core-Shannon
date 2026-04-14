#pragma once
#include <QObject>
#include <QColor>
#include <QString>
#include <QStringList>

// Provides theme colors to the QML frontend.
// Dark glassmorphism palette matching the Decodium 2.5 design.
class DecodiumThemeManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QColor bgDeep       READ bgDeep       CONSTANT)
    Q_PROPERTY(QColor bgMedium     READ bgMedium     CONSTANT)
    Q_PROPERTY(QColor bgLight      READ bgLight      CONSTANT)
    Q_PROPERTY(QColor primaryColor READ primaryColor CONSTANT)
    Q_PROPERTY(QColor secondaryColor READ secondaryColor CONSTANT)
    Q_PROPERTY(QColor accentColor  READ accentColor  CONSTANT)
    Q_PROPERTY(QColor warningColor READ warningColor CONSTANT)
    Q_PROPERTY(QColor errorColor   READ errorColor   CONSTANT)
    Q_PROPERTY(QColor textPrimary  READ textPrimary  CONSTANT)
    Q_PROPERTY(QColor textSecondary READ textSecondary CONSTANT)
    Q_PROPERTY(QColor successColor READ successColor CONSTANT)
    Q_PROPERTY(QColor glassOverlay READ glassOverlay CONSTANT)
    Q_PROPERTY(QColor glassBorder  READ glassBorder  CONSTANT)
    Q_PROPERTY(QStringList availableThemes READ availableThemes CONSTANT)
    Q_PROPERTY(QString currentTheme READ currentTheme WRITE setCurrentTheme NOTIFY currentThemeChanged)
    Q_PROPERTY(bool   isLightTheme READ isLightTheme CONSTANT)

public:
    explicit DecodiumThemeManager(QObject* parent = nullptr) : QObject(parent) {}

    QColor bgDeep()         const { return QColor("#0A0F1A"); }
    QColor bgMedium()       const { return QColor("#111827"); }
    QColor bgLight()        const { return QColor("#1E2D42"); }
    QColor primaryColor()   const { return QColor("#4A90E2"); }
    QColor secondaryColor() const { return QColor("#00D4FF"); }
    QColor accentColor()    const { return QColor("#00FF88"); }
    QColor warningColor()   const { return QColor("#FF8C00"); }
    QColor errorColor()     const { return QColor("#FF5F56"); }
    QColor textPrimary()    const { return QColor("#E8F4FD"); }
    QColor textSecondary()  const { return QColor("#89B4D0"); }
    QColor successColor()   const { return QColor("#4CAF50"); }
    QColor glassOverlay()   const { return QColor(26, 58, 92, 64);  } // #1A3A5C40
    QColor glassBorder()    const { return QColor(74, 144, 226, 80); } // #4A90E250
    QStringList availableThemes() const { return {"Ocean Blue", "Stellar Light", "Emerald Night"}; }
    QString currentTheme() const { return m_currentTheme; }
    void setCurrentTheme(const QString& name) {
        if (m_currentTheme == name) return;
        m_currentTheme = name;
        m_isLight = (name == "Stellar Light");
        emit currentThemeChanged();
    }
    bool   isLightTheme()   const { return m_isLight; }

public slots:
    Q_INVOKABLE void applyThemeByName(const QString& name) {
        setCurrentTheme(name);
    }

signals:
    void currentThemeChanged();

private:
    bool m_isLight {false};
    QString m_currentTheme {"Ocean Blue"};
};
