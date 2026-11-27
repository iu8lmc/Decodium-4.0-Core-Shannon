// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#include "DecodiumPropagationManager.h"
#include "SatelliteTrackingService.h"
#include "MapIntelligenceService.h"
#include "CallsignIntelligenceService.h"
#include "DecodiumCatShare.h"
#include "DecodiumSpotShare.h"
#include "DecodiumAmplifier.h"

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



QObject* DecodiumBridge::catShareObject() const
{
    return m_catShare;
}

QString DecodiumBridge::catBackendForPersistence() const
{
    return (m_catBackend == QLatin1String("decoport") && !m_catBackendBeforeDecoPort.isEmpty())
               ? m_catBackendBeforeDecoPort
               : m_catBackend;
}

QObject* DecodiumBridge::spotShareObject() const
{
    return m_spotShare;
}

QObject* DecodiumBridge::amplifierObject() const
{
    return m_amplifier;
}

