// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodiumBridge.h"
#include <QDateTime>

bool DecodiumBridge::monitoring() const { return m_monitoring; }
void DecodiumBridge::setMonitoring(bool v) {
    // IU8LMC: da quando ascoltiamo. Serve all'autodiagnosi per distinguere
    // "banda chiusa / audio scollegato" (zero decode dopo minuti) dall'impazienza
    // di chi guarda per 20 secondi. Vedi DecodiumSelfCheck.
    if (v && !m_monitoring)
        m_monitoringSince = QDateTime::currentDateTimeUtc();
    if (v) startRx(); else stopRx();
}




