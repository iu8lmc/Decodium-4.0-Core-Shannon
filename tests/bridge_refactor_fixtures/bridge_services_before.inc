QObject * DecodiumBridge::propagationManager() const
{
    return m_propagationManager;
}

QObject * DecodiumBridge::satelliteTracking() const
{
    return m_satelliteTracking;
}

void DecodiumBridge::requestSatelliteTrackingWindow()
{
    emit satelliteTrackingWindowRequested();
}

QObject * DecodiumBridge::mapIntelligenceService() const
{
    return m_mapIntelligenceService;
}

QObject * DecodiumBridge::callsignIntelligence() const
{
    return m_callsignIntelligence;
}


