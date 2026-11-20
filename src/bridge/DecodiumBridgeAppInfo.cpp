#include "DecodiumBridge.h"
#include "DecodiumLogging.hpp"
#include <QDesktopServices>
#include <QGuiApplication>
#include <QScreen>
#include <QUrl>

QString DecodiumBridge::diagnosticLogPath() const
{
    return DecodiumLogging::diagnosticLogPath();
}

void DecodiumBridge::openDiagnosticLog() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(DecodiumLogging::diagnosticLogPath()));
}

void DecodiumBridge::openHamlibUpdatePage() const
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/Hamlib/Hamlib/releases")));
}

QVariantMap DecodiumBridge::primaryScreenAvailableGeometry() const
{
    QVariantMap geometry;

    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        auto const screens = QGuiApplication::screens();
        if (!screens.isEmpty()) {
            screen = screens.first();
        }
    }
    if (!screen) {
        return geometry;
    }

    QRect rect = screen->availableGeometry();
    if (!rect.isValid() || rect.width() <= 0 || rect.height() <= 0) {
        rect = screen->geometry();
    }
    if (!rect.isValid() || rect.width() <= 0 || rect.height() <= 0) {
        return geometry;
    }

    geometry.insert(QStringLiteral("x"), rect.x());
    geometry.insert(QStringLiteral("y"), rect.y());
    geometry.insert(QStringLiteral("width"), rect.width());
    geometry.insert(QStringLiteral("height"), rect.height());
    geometry.insert(QStringLiteral("name"), screen->name());
    return geometry;
}

QString DecodiumBridge::version() const
{
    return QStringLiteral(FORK_RELEASE_VERSION);
}

