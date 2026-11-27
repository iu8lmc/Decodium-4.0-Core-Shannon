#include "DecodiumBridge.h"
#include "Network/MessageClient.hpp"
#include <QTimer>

void decodiumBridgeUdpRestartLog(const QString& message);

void DecodiumBridge::shutdownUdpMessageClient()
{
    if (!m_udpMessageClient && !m_udpSecondaryMessageClient && !m_udpTertiaryMessageClient) {
        return;
    }
    if (m_udpMessageClient) {
        decodiumBridgeUdpRestartLog(QStringLiteral("Stopping standalone UDP MessageClient"));
        m_udpMessageClient->disconnect(this);
        m_udpMessageClient->deleteLater();
        m_udpMessageClient = nullptr;
    }
    if (m_udpSecondaryMessageClient) {
        decodiumBridgeUdpRestartLog(QStringLiteral("Stopping secondary UDP MessageClient"));
        m_udpSecondaryMessageClient->disconnect(this);
        m_udpSecondaryMessageClient->deleteLater();
        m_udpSecondaryMessageClient = nullptr;
    }
    if (m_udpTertiaryMessageClient) {
        decodiumBridgeUdpRestartLog(QStringLiteral("Stopping tertiary UDP MessageClient"));
        m_udpTertiaryMessageClient->disconnect(this);
        m_udpTertiaryMessageClient->deleteLater();
        m_udpTertiaryMessageClient = nullptr;
    }
}

void DecodiumBridge::scheduleUdpMessageClientRestart()
{
    if (m_udpMessageClientRestartPending) {
        return;
    }

    m_udpMessageClientRestartPending = true;
    QTimer::singleShot(150, this, [this]() {
        m_udpMessageClientRestartPending = false;
        shutdownUdpMessageClient();
        initUdpMessageClient();
    });
}
















