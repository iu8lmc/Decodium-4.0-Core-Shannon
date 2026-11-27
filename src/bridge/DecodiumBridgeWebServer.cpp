#include "DecodiumBridge.h"
#include "Web/DecodiumWebServer.hpp"

#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>

void decodiumBridgeWebServerLog(const QString& message);

namespace
{
QString generateWebServerAccessToken()
{
    QString token;
    token.reserve(64);
    for (int i = 0; i < 4; ++i) {
        token += QStringLiteral("%1")
                     .arg(QRandomGenerator::global()->generate64(), 16, 16, QLatin1Char('0'));
    }
    return token;
}

bool webServerAccessTokenLooksUsable(QString const& token)
{
    static QRegularExpression const safeTokenRe(QStringLiteral("^[A-Za-z0-9._~-]{24,}$"));
    return safeTokenRe.match(token).hasMatch();
}
}

bool DecodiumBridge::startWebServer(int port)
{
    if (!m_webServer) {
        m_webServer = new DecodiumWebServer(this, this);
        connect(m_webServer, &DecodiumWebServer::errorOccurred, this,
            [](QString const& msg) { decodiumBridgeWebServerLog(QStringLiteral("WebServer: ") + msg); });
        connect(m_webServer, &DecodiumWebServer::clientCountChanged, this,
            [](int n) { decodiumBridgeWebServerLog(QStringLiteral("WebServer clients=%1").arg(n)); });
        // 1.0.168 fase 2: push real-time via WebSocket.
        connect(this, &DecodiumBridge::decodeListChanged, m_webServer,
                &DecodiumWebServer::broadcastDecodesUpdate);
        connect(this, &DecodiumBridge::modeChanged, m_webServer,
                &DecodiumWebServer::broadcastStateUpdate);
        connect(this, &DecodiumBridge::rxFrequencyChanged, m_webServer,
                &DecodiumWebServer::broadcastStateUpdate);
        connect(this, &DecodiumBridge::transmittingChanged, m_webServer,
                &DecodiumWebServer::broadcastStateUpdate);
        connect(this, &DecodiumBridge::ntpSyncedChanged, m_webServer,
                &DecodiumWebServer::broadcastStateUpdate);
        connect(this, &DecodiumBridge::dxCallChanged, m_webServer,
                &DecodiumWebServer::broadcastStateUpdate);
    }
    QSettings webSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    QString token = webSettings.value(QStringLiteral("WebServerAccessToken")).toString().trimmed();
    if (!webServerAccessTokenLooksUsable(token)) {
        token = generateWebServerAccessToken();
        webSettings.setValue(QStringLiteral("WebServerAccessToken"), token);
        webSettings.sync();
    }

    bool const ok = m_webServer->start(static_cast<quint16>(qBound(1024, port, 65535)), token);
    if (ok) {
        QString const url = m_webServer->accessUrl();
        decodiumBridgeWebServerLog(QStringLiteral("WebServer started at %1 (token protected)")
                      .arg(url.section(QStringLiteral("?token="), 0, 0)));
    }
    return ok;
}

void DecodiumBridge::stopWebServer()
{
    if (m_webServer) {
        m_webServer->stop();
        decodiumBridgeWebServerLog(QStringLiteral("WebServer stopped"));
    }
}

bool DecodiumBridge::webServerRunning() const
{
    return m_webServer && m_webServer->isRunning();
}

QString DecodiumBridge::webServerUrl() const
{
    return m_webServer ? m_webServer->accessUrl() : QString();
}

QString DecodiumBridge::webServerQrUrl() const
{
    return m_webServer ? m_webServer->qrUrl() : QString();
}

