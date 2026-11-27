#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumProfileSettings.h"
#include <QSettings>

void decodiumBridgeDecodeFlowLog(const QString&);

// 1.0.226 — Q_INVOKABLE wrapper per uso da QML fallback. Restituisce true
// SOLO se m_hideGhostDecodes attivo E looksLikeGhostDecode positivo.
// Cosi' il QML chiama un singolo metodo invece di duplicare la logica.
bool DecodiumBridge::entryLooksLikeGhost(const QVariantMap& entry) const
{
    if (!m_hideGhostDecodes) return false;
    return looksLikeGhostDecode(entry);
}

void DecodiumBridge::setHideGhostDecodes(bool v)
{
    if (m_hideGhostDecodes == v) return;
    m_hideGhostDecodes = v;
    decodiumBridgeDecodeFlowLog(QStringLiteral("setHideGhostDecodes: %1").arg(v ? "ON" : "OFF"));
    saveSettingsAsync();
    invalidateDecodeUiPredicateCaches();
    invalidateLegacyDecodeModelDeltaFastPath();
    // Rigenera i model con la nuova policy di filter.
    rebuildBandActivityModel();
    rebuildRxDecodeModel();
}

void DecodiumBridge::setFiltersBypassed(bool v)
{
    if (m_filtersBypassed == v) return;
    m_filtersBypassed = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("filtersBypassed"), m_filtersBypassed);
    settings.setValue(QStringLiteral("FiltersBypassed"), m_filtersBypassed);
    settings.sync();
    decodiumBridgeDecodeFlowLog(QStringLiteral("Decode filters bypass: %1").arg(m_filtersBypassed ? "ON" : "OFF"));
    invalidateDecodeUiPredicateCaches();
    invalidateLegacyDecodeModelDeltaFastPath();
    emit filtersBypassedChanged();
    emit settingValueChanged(QStringLiteral("filtersBypassed"), m_filtersBypassed);
    emit settingValueChanged(QStringLiteral("FiltersBypassed"), m_filtersBypassed);

    if (legacyBackendAvailable()) {
        // CQ Only is a Decodium4 presentation filter. Do not pass it down to
        // the hidden legacy decoder, otherwise ALL.TXT loses non-CQ reports.
        m_legacyBackend->setCqOnly(false);
    }
    if (usingLegacyBackendForTx()) {
        m_legacyBandActivityRevision = -1;
        m_legacyRxFrequencyRevision = -1;
        m_legacyAllTxtRevisionKey.clear();
        syncLegacyBackendDecodeList();
    }
    rebuildBandActivityModel();
    rebuildRxDecodeModel();
    emit statusMessage(m_filtersBypassed
        ? QStringLiteral("Decode filters bypassed")
        : QStringLiteral("Decode filters active"));
}

