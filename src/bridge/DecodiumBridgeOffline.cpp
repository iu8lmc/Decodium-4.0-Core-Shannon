#include "DecodiumBridge.h"
#include "MapIntelligenceService.h"
#include <QDebug>

bool DecodiumBridge::offlineMode() const
{
    return m_mapIntelligenceService && m_mapIntelligenceService->offlineMode();
}

void DecodiumBridge::resumeOnlineServices()
{
    if (!m_mapIntelligenceService) {
        emit errorMessage(tr("Online services are not available yet."));
        return;
    }

    m_mapIntelligenceService->setNetworkOfflineMode(false);
    if (m_mapIntelligenceService->offlineMode()) {
        emit errorMessage(tr("Could not resume Decodium online services."));
        return;
    }

    qInfo().noquote() << "[NETWORK] Online services resumed by user";
    emit statusMessage(tr("Online services resumed. DX Cluster and update checks can connect again."));
}

