#include "DecodiumBridge.h"
#include <QClipboard>
#include <QGuiApplication>

void decodiumBridgeUiActionLog(const QString& message);

void DecodiumBridge::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void DecodiumBridge::openSetupSettings(int tabIndex)
{
    decodiumBridgeUiActionLog(QStringLiteral("openSetupSettings: requesting QML setup tab %1").arg(tabIndex));
    emit setupSettingsRequested(tabIndex);
}

void DecodiumBridge::openTimeSyncSettings()
{
    decodiumBridgeUiActionLog(QStringLiteral("openTimeSyncSettings: opening QML time-sync settings"));
    emit timeSyncSettingsRequested();
}

