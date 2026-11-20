#include "DecodiumBridge.h"
#include "DecodiumDxCluster.h"

void decodiumBridgeDxClusterLog(const QString& message);

void DecodiumBridge::connectDxCluster()
{
    connectDxCluster(QString(), 0);
}

void DecodiumBridge::connectDxCluster(const QString& host, int port)
{
    if (!m_dxCluster) return;
    QString const h = host.trimmed();
    if (!h.isEmpty()) {
        m_dxCluster->setHost(h);
    }
    if (port > 0) {
        m_dxCluster->setPort(port);
    }
    if (!m_callsign.isEmpty()) m_dxCluster->setCallsign(m_callsign);
    decodiumBridgeDxClusterLog("connectDxCluster: " + m_dxCluster->host() + ":" + QString::number(m_dxCluster->port())
              + " as " + m_callsign);
    m_dxCluster->connectCluster();
}

void DecodiumBridge::disconnectDxCluster()
{
    if (m_dxCluster) m_dxCluster->disconnectCluster();
}

void DecodiumBridge::setAutoSpotEnabled(bool enabled)
{
    setSetting(QStringLiteral("AutoSpotEnabled"), enabled);
}



