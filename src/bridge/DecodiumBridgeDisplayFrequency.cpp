#include "DecodiumBridge.h"

double DecodiumBridge::frequency() const { return m_frequency; }
double DecodiumBridge::displayFrequency() const
{
    if (m_transmitting || m_tuning) {
        if (m_transmitting && ft2LinkSatelliteHalfDuplexOperationActive()
            && m_ft2LinkSatelliteHalfDuplexTxDialHz > 0) {
            return static_cast<double>(m_ft2LinkSatelliteHalfDuplexTxDialHz);
        }
        double const txDialHz = catSplitTxDialFrequencyHz();
        if (txDialHz > 0.0) {
            return txDialHz;
        }
    }
    return m_frequency;
}

