    m_mapIntelligenceService = new MapIntelligenceService(this);
    if (m_mapIntelligenceService) {
        auto applyOfflineMode = [this]() {
            if (!m_mapIntelligenceService) return;
            const bool offline = m_mapIntelligenceService->offlineMode();
            if (offline) {
                for (QNetworkAccessManager* manager :
                     findChildren<QNetworkAccessManager*>()) {
                    if (!manager) continue;
                    for (QNetworkReply* reply : manager->findChildren<QNetworkReply*>()) {
                        if (reply) reply->abort();
                    }
                }
            }
            if (m_propagationManager) m_propagationManager->setOfflineMode(offline);
            if (m_satelliteTracking) m_satelliteTracking->setOfflineMode(offline);
            if (m_callsignIntelligence) m_callsignIntelligence->setOfflineMode(offline);
            if (m_dxCluster) m_dxCluster->setOfflineMode(offline);
        };
        connect(m_mapIntelligenceService, &MapIntelligenceService::offlineModeChanged,
                this, applyOfflineMode);
        connect(m_mapIntelligenceService, &MapIntelligenceService::offlineModeChanged,
                this, &DecodiumBridge::offlineModeChanged);
        applyOfflineMode();
        auto* const mapLayers = qobject_cast<MapLayerModel*>(
            m_mapIntelligenceService->layerModel());
        if (mapLayers) {
            connect(mapLayers, &MapLayerModel::layerToggled, this,
                    [this](const QString& layerId, bool enabled) {
                if (layerId == QStringLiteral("moon") && enabled) {
                    refreshMapMoonOverlay();
                }
            });
        }
    }
