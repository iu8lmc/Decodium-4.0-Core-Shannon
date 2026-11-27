#include "DecodiumBridge.h"
#include "SatelliteTrackingService.h"
#include "Q65DopplerTracker.h"
#include <QTimer>

void DecodiumBridge::initialiseSatelliteTracking()
{
    m_satelliteTracking = new SatelliteTrackingService(this);
    m_q65Doppler = new Q65DopplerTracker(this);
    connect(m_q65Doppler, &Q65DopplerTracker::configurationChanged,
            this, &DecodiumBridge::refreshQ65Doppler);
    auto* emeTimer = new QTimer(this);
    emeTimer->setInterval(1000);
    connect(emeTimer, &QTimer::timeout, this, &DecodiumBridge::refreshQ65Doppler);
    emeTimer->start();
    if (m_satelliteTracking) {
        connect(this, &DecodiumBridge::gridChanged, this, [this]() {
            if (m_satelliteTracking) m_satelliteTracking->setObserverGrid(m_grid);
        });
        connect(this, &DecodiumBridge::frequencyChanged, this, [this]() {
            if (m_satelliteTracking && !m_satelliteDopplerApplying) {
                m_satelliteTracking->setNominalFrequencyHz(m_frequency);
            }
        });
        connect(m_satelliteTracking, &SatelliteTrackingService::dopplerFrequencyChanged,
                this, [this](double frequencyHz) {
            if (!m_satelliteTracking || !m_satelliteTracking->dopplerTracking()
                || !m_monitoring || m_transmitting || m_tuning || frequencyHz <= 0.0) {
                return;
            }
            m_satelliteDopplerApplying = true;
            setFrequency(frequencyHz);
            m_satelliteDopplerApplying = false;
        });
        m_satelliteTracking->setObserverGrid(m_grid);
        m_satelliteTracking->setNominalFrequencyHz(m_frequency);
    }
}
