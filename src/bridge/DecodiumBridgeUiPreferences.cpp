#include "DecodiumBridge.h"
#include <QCoreApplication>
#include <QSettings>

extern void decodiumBridgeLogbookLog(const QString&);

QString DecodiumBridge::uiQuality() const
{
    if (m_themeManager && m_themeManager->isFlatTheme()) {
        return QStringLiteral("Low");
    }
    return m_uiQuality;
}

// 1.0.180 — UI Revolution: setter per uiQuality (Low/Medium/High).
// Persistito su QSettings "UI/Quality". Restart richiesto per alcune
// componenti che leggono il valore solo a init (es. ShaderEffect chain).
void DecodiumBridge::setUiQuality(QString const& v)
{
    QString const norm = v.trimmed();
    if (norm != QStringLiteral("Low") && norm != QStringLiteral("Medium")
        && norm != QStringLiteral("High")) return;
    if (m_uiQuality == norm) return;
    m_uiQuality = norm;
    QSettings().setValue(QStringLiteral("UI/Quality"), norm);
    emit uiQualityChanged();
    decodiumBridgeLogbookLog(QStringLiteral("[UI] Quality = %1").arg(norm));
}

// 1.0.186 — Full Spectrum auto-detach: pop-out opzionale di period1 per
// isolare il scene-graph dei ListView decode dal Main (Pasquale-pattern).
// Default OFF al primo avvio. Persistito su QSettings.
void DecodiumBridge::setAutoDetachFullSpectrum(bool v)
{
    if (m_autoDetachFullSpectrum == v) return;
    m_autoDetachFullSpectrum = v;
    QSettings settings;
    settings.setValue(QStringLiteral("UI/AutoDetachFullSpectrum"), v);
    settings.sync();
    emit autoDetachFullSpectrumChanged();
    decodiumBridgeLogbookLog(QStringLiteral("[UI] AutoDetachFullSpectrum = %1").arg(v ? "ON" : "OFF"));
}

// 1.0.186 — Cap FPS panadapter (15/20/30). Default 20 quando integrato,
// usabile a 30 quando detached (Window separata con render thread isolato).
// Persistito su QSettings; letto da m_minPanadapterIntervalMs path.
void DecodiumBridge::setSpectrumFpsCap(int v)
{
    int const clamped = (v <= 15) ? 15 : (v >= 30) ? 30 : 20;
    if (m_spectrumFpsCap == clamped) return;
    m_spectrumFpsCap = clamped;
    QSettings().setValue(QStringLiteral("UI/SpectrumFpsCap"), clamped);
    emit spectrumFpsCapChanged();
    decodiumBridgeLogbookLog(QStringLiteral("[UI] SpectrumFpsCap = %1 fps (interval=%2 ms)")
                  .arg(clamped).arg(1000 / clamped));
}

// 1.0.180 — UI Revolution: setter per uiFramelessPopouts (Spectrum/Waterfall
// in modalita' detached senza decorazione frame OS). Persistito su QSettings.
void DecodiumBridge::setUiFramelessPopouts(bool v)
{
    if (m_uiFramelessPopouts == v) return;
    m_uiFramelessPopouts = v;
    QSettings settings;
    settings.setValue(QStringLiteral("UI/FramelessPopouts"), v);
    settings.sync();
    emit uiFramelessPopoutsChanged();
    decodiumBridgeLogbookLog(QStringLiteral("[UI] FramelessPopouts = %1").arg(v ? "ON" : "OFF"));
}

// 1.0.180 — UI Revolution: setter per uiStyle (selettore stile QML).
// Persistito su QSettings "UI/Style"; richiede restart per applicare il
// QQuickStyle::setStyle a freddo all'avvio successivo.
void DecodiumBridge::setUiStyle(QString const& v)
{
    QString const norm = v.trimmed();
    // 1.0.185 — Whitelist stili ridotta a 5. Imagine rimosso (binding loop
    // ScrollBar/Button + asset webp mancanti); Basic rimosso (problemi
    // cosmetici minori). Migrazione automatica nel costruttore.
    // 1.0.185 — "Default" rimosso dalla whitelist del setter: e' un alias gestito
    // alla lettura (vedi costruttore) -> il setter non deve accettarlo come valore.
    static QStringList const valid = { "FluentWinUI3", "Material", "Universal", "Fusion" };
    if (!valid.contains(norm)) return;
    if (m_uiStyle == norm) return;
    m_uiStyle = norm;
    QSettings(QCoreApplication::organizationName(), QStringLiteral("Decodium")).setValue(QStringLiteral("UI/Style"), norm);
    emit uiStyleChanged();
    decodiumBridgeLogbookLog(QStringLiteral("[UI] Style = %1 (restart richiesto)").arg(norm));
}


