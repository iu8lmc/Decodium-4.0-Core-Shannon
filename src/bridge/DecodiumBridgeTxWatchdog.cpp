#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumProfileSettings.h"
#include <QSettings>

void DecodiumBridge::persistTxWatchdogSettings()
{
    int const legacyMinutes = (m_txWatchdogMode == 1 && m_txWatchdogTime > 0)
        ? m_txWatchdogTime
        : 0;

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("txWatchdogMode"), m_txWatchdogMode);
    settings.setValue(QStringLiteral("txWatchdogTime"), m_txWatchdogTime);
    settings.setValue(QStringLiteral("txWatchdogCount"), m_txWatchdogCount);
    settings.setValue(QStringLiteral("TxWatchdog"), legacyMinutes);
    settings.setValue(QStringLiteral("TXWatchdog"), legacyMinutes);
    settings.sync();

    syncSettingToLegacyIni(QStringLiteral("TxWatchdog"), legacyMinutes);
    syncSettingToLegacyIni(QStringLiteral("TXWatchdog"), legacyMinutes);
    if (legacyBackendAvailable()) {
        m_legacyBackend->setTxWatchdogMinutes(legacyMinutes);
    }
}

bool DecodiumBridge::txWatchdogLimitConfigured() const
{
    return (m_txWatchdogMode == 1 && m_txWatchdogTime > 0)
        || (m_txWatchdogMode == 2 && m_txWatchdogCount > 0);
}

void DecodiumBridge::setTxWatchdogMode(int v)
{
    v = qBound(0, v, 2);
    if (m_txWatchdogMode == v) {
        persistTxWatchdogSettings();
        return;
    }

    m_txWatchdogMode = v;
    m_txWatchdogTicks = 0;
    m_txWatchdogActiveSinceMs = 0;
    m_txWatchdogLastProgressLogMs = 0;
    m_txWatchdogElapsedActive = false;
    m_txWatchdogElapsed.invalidate();
    m_autoCQPeriodsMissed = 0;
    persistTxWatchdogSettings();

    emit txWatchdogModeChanged();
    emit settingValueChanged(QStringLiteral("txWatchdogMode"), m_txWatchdogMode);
    emit settingValueChanged(QStringLiteral("TxWatchdog"),
                             (m_txWatchdogMode == 1 && m_txWatchdogTime > 0) ? m_txWatchdogTime : 0);
    emit settingValueChanged(QStringLiteral("TXWatchdog"),
                             (m_txWatchdogMode == 1 && m_txWatchdogTime > 0) ? m_txWatchdogTime : 0);
}

void DecodiumBridge::setTxWatchdogTime(int v)
{
    v = qBound(0, v, 999);
    if (m_txWatchdogTime == v) {
        persistTxWatchdogSettings();
        return;
    }

    m_txWatchdogTime = v;
    m_txWatchdogTicks = 0;
    m_txWatchdogActiveSinceMs = 0;
    m_txWatchdogLastProgressLogMs = 0;
    m_txWatchdogElapsedActive = false;
    m_txWatchdogElapsed.invalidate();
    m_autoCQPeriodsMissed = 0;
    persistTxWatchdogSettings();

    emit txWatchdogTimeChanged();
    emit settingValueChanged(QStringLiteral("txWatchdogTime"), m_txWatchdogTime);
    emit settingValueChanged(QStringLiteral("TxWatchdog"),
                             (m_txWatchdogMode == 1 && m_txWatchdogTime > 0) ? m_txWatchdogTime : 0);
    emit settingValueChanged(QStringLiteral("TXWatchdog"),
                             (m_txWatchdogMode == 1 && m_txWatchdogTime > 0) ? m_txWatchdogTime : 0);
}

void DecodiumBridge::setTxWatchdogCount(int v)
{
    v = qBound(1, v, 50);
    if (m_txWatchdogCount == v) {
        persistTxWatchdogSettings();
        return;
    }

    m_txWatchdogCount = v;
    m_txWatchdogTicks = 0;
    m_txWatchdogActiveSinceMs = 0;
    m_txWatchdogLastProgressLogMs = 0;
    m_txWatchdogElapsedActive = false;
    m_txWatchdogElapsed.invalidate();
    m_autoCQPeriodsMissed = 0;
    persistTxWatchdogSettings();

    emit txWatchdogCountChanged();
    emit settingValueChanged(QStringLiteral("txWatchdogCount"), m_txWatchdogCount);
}

