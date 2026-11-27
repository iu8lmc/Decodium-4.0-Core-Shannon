#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "Radio.hpp"
#include "models/Bands.hpp"
#include <cmath>

void DecodiumBridge::setFrequency(double v) {
    // Con la radio remota in uso la manopola dell'applicazione e' la sua: quello
    // che l'operatore cambia qui deve arrivare li'. Il flag evita il rimbalzo,
    // perche' la stessa frequenza torna indietro nel contesto successivo.
    if (m_decoPortUseRemote && !m_decoPortApplyingRemote && v > 0.0) {
        if (auto* link = remoteLink()) {
            if (link->isLinked() && std::abs(link->frequencyHz() - v) >= 0.5)
                link->tune(v);
        }
    }
    bool const monitorShouldStayActive = m_monitoring || m_monitorRequested;
    quint64 const monitorSessionId = m_periodTimerSessionId;
    bool frequencyChangedForRecovery = false;
    double oldFreq = m_frequency;
    bool const legacyDialNeedsSync =
        legacyBackendAvailable()
        && (m_legacyBackend->dialFrequency() <= 0.0
            || std::abs(m_legacyBackend->dialFrequency() - v) > 1.0);
    if (!qFuzzyCompare(m_frequency + 1.0, v + 1.0)) {
        m_frequency = v;
        frequencyChangedForRecovery = true;
        emit frequencyChanged();
        // Pulisci le finestre decode quando cambia banda (differenza > 100kHz)
        if (std::abs(v - oldFreq) > 100000.0) {
            clearDecodeWindowsForBandChange(oldFreq, v, QStringLiteral("set-frequency"));
        }
    }
    if (v > 0.0) {
        Bands bands;
        m_currentCatLogicalBand = bands.find(static_cast<Radio::Frequency>(std::llround(v)));
        m_currentCatStationOffsetHz =
            static_cast<Radio::FrequencyDelta>(std::llround(stationOffsetForFrequencyHz(v)));
    }
    // A legacy mode switch can retune its own band selector while the bridge
    // already holds the requested dial frequency. Reassert the dial in that
    // case as well, otherwise UI and decoder can silently use different bands.
    if (legacyBackendAvailable()
        && (frequencyChangedForRecovery || legacyDialNeedsSync)) {
        m_legacyBackend->setDialFrequency(v);
    }
    if (m_bandManager) {
        m_bandManager->updateFromFrequency(v);
    }
    // RTL-SDR owns its own asynchronous retune/reopen cycle.  Sending it
    // through the generic audio-capture recovery path would create a second
    // restart while the user is dragging the RF dial.
    if (frequencyChangedForRecovery && std::abs(v - oldFreq) > 50000.0
        && !rtlSdrEnabled()) {
        scheduleMonitorRecovery(QStringLiteral("frequency QSY %1 -> %2 Hz")
                                    .arg(QString::number(oldFreq, 'f', 0),
                                         QString::number(v, 'f', 0)),
                                monitorSessionId,
                                monitorShouldStayActive);
    }
    if (frequencyChangedForRecovery
        && rtlSdrEnabled()
        && getSetting(QStringLiteral("RtlSdrFollowDial"), true).toBool()
        && m_monitoring) {
        scheduleRtlSdrRetune();
    }
}

