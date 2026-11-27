#include "DecodiumBridge.h"
#include "DecodiumDxCluster.h"
#include <QDebug>

void DecodiumBridge::initialiseDecodeClusterConnections()
{
    connect(this, &DecodiumBridge::decodeListChanged, this, [this]() {
        if (usingLegacyBackendForTx()) return;
        publishRemoteActivityEntries(m_decodeList);
        replayPskReporterRecentEntries(m_decodeList, QStringLiteral("decodeListChanged"));
    });
    connect(this, &DecodiumBridge::rxDecodeListChanged, this, [this]() {
        if (usingLegacyBackendForTx()) return;
        publishRemoteActivityEntries(m_rxDecodeList);
        replayPskReporterRecentEntries(m_rxDecodeList, QStringLiteral("rxDecodeListChanged"));
    });
    connect(m_dxCluster, &DecodiumDxCluster::connectedChanged, this, [this]() {
        qDebug() << "[DxCluster] connected =" << m_dxCluster->connected();
        emit dxClusterConnectedChanged();
    });
    connect(m_dxCluster, &DecodiumDxCluster::spotsChanged, this, [this]() {
        scheduleDxClusterSpotsChanged();
    });
}
