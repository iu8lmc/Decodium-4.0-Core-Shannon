#include "DecodiumBridge.h"
#include <QCoreApplication>
#include <QSettings>
#include <QStringList>

void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::initialiseUiStartupSettings()
{
    // 1.0.180 — Carica preferenze UI da QSettings
    {
        QSettings s;
        m_uiQuality          = s.value(QStringLiteral("UI/Quality"), "Medium").toString();
        m_uiFramelessPopouts = s.value(QStringLiteral("UI/FramelessPopouts"), false).toBool();
        QSettings styleSettings(QCoreApplication::organizationName(), QStringLiteral("Decodium"));
        m_uiStyle = styleSettings.value(QStringLiteral("UI/Style"), "Material").toString();

        // 1.0.185 — Migrazione: alias "Default" -> "Material" + fallback per stili
        // deprecati (Imagine, Basic) o sconosciuti. Material e' baseline customizable.
        // Motivo alias: su Windows con Qt 6.11 il fallback Qt nativo risolve al
        // Windows Native style che NON permette customization di background/
        // contentItem/indicator (Decodium fa molte customizations -> warning
        // massivi + UI degradata).
        static QStringList const supportedStyles = {
            QStringLiteral("FluentWinUI3"),
            QStringLiteral("Material"),
            QStringLiteral("Universal"),
            QStringLiteral("Fusion")
        };
        if (m_uiStyle.compare(QStringLiteral("Default"), Qt::CaseInsensitive) == 0
            || !supportedStyles.contains(m_uiStyle)) {
            decodiumBridgeAudioLevelLog(QStringLiteral("[UI] Style \"%1\" migrato a \"Material\" "
                                     "(Default su Windows risolve a native non-customizable)").arg(m_uiStyle));
            m_uiStyle = QStringLiteral("Material");
            styleSettings.setValue(QStringLiteral("UI/Style"), m_uiStyle);
        }

        // 1.0.186 — Full Spectrum auto-detach + Spectrum FPS cap.
        // 1.0.201 — Default ripristinato a true: la 1.0.197 lo aveva spento
        // upstream, ma su PC modesti il Pasquale-pattern (pop-out su Window
        // separata = render thread isolato) elimina stall main-thread di
        // ~518ms cadenza costante durante drain ListView / texture upload
        // waterfall. Misurato: 1463 -> 1 stall in 9 min RX FT8.
        m_autoDetachFullSpectrum = s.value(
            QStringLiteral("UI/AutoDetachFullSpectrum"), true).toBool();
        int const fpsRaw = s.value(QStringLiteral("UI/SpectrumFpsCap"), 20).toInt();
        m_spectrumFpsCap = (fpsRaw <= 15) ? 15 : (fpsRaw >= 30) ? 30 : 20;

        decodiumBridgeAudioLevelLog(QStringLiteral("[UI] Init Quality=%1 FramelessPopouts=%2 Style=%3 "
                                 "AutoDetachFullSpectrum=%4 SpectrumFpsCap=%5")
                  .arg(m_uiQuality)
                  .arg(m_uiFramelessPopouts ? "ON" : "OFF")
                  .arg(m_uiStyle)
                  .arg(m_autoDetachFullSpectrum ? "ON" : "OFF")
                  .arg(m_spectrumFpsCap));
    }

}
