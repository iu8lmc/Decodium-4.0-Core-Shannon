#include "DecodiumBridge.h"

// === ADV AUTO-MODE ===
// Logica:
//  - Neural Sync (FT8 OSD always-on): attivo se decode rolling avg < 2/slot
//    (banda povera → costo +80% CPU giustificato dal recovery extra ~12%).
//  - Turbo Feedback (LDPC iter 30→50): attivo insieme a Neural Sync
//    (overhead aggiuntivo ~5%, può aiutare codeword borderline da OSD).
//  - Coherent Avg (Q65): attivo quando SNR Q65 medio < -22 dB.
void DecodiumBridge::setAdvAutoModeEnabled(bool v)
{
    if (m_advAutoModeEnabled != v) {
        m_advAutoModeEnabled = v;
        emit advAutoModeEnabledChanged();
        // Reset history quando cambia il mode per evitare carry-over stantio.
        m_ft8DecodeHistory.clear();
    }
}

void DecodiumBridge::recordFt8DecodeCount(int count)
{
    m_ft8DecodeHistory.append(qMax(0, count));
    while (m_ft8DecodeHistory.size() > kFt8HistoryDepth) {
        m_ft8DecodeHistory.removeFirst();
    }
}

bool DecodiumBridge::effectiveNeuralSync()
{
    bool active;
    if (m_advAutoModeEnabled) {
        // Trigger: avg decodi recenti < soglia E history popolata
        if (m_ft8DecodeHistory.size() >= 2) {
            double sum = 0.0;
            for (int v : m_ft8DecodeHistory) sum += v;
            double const avg = sum / m_ft8DecodeHistory.size();
            active = avg < 2.0;
        } else {
            // Bootstrap: prime 2 slot decode senza neural per misurare
            active = false;
        }
    } else {
        active = m_neuralSyncEnabled;
    }
    if (m_advNeuralSyncActive != active) {
        m_advNeuralSyncActive = active;
        emit advNeuralSyncActiveChanged();
    }
    return active;
}

bool DecodiumBridge::effectiveTurboFeedback()
{
    bool active;
    if (m_advAutoModeEnabled) {
        // Co-attivato con Neural Sync (overhead marginale, può recuperare bit borderline)
        active = effectiveNeuralSync();
    } else {
        active = m_turboFeedbackEnabled;
    }
    if (m_advTurboFeedbackActive != active) {
        m_advTurboFeedbackActive = active;
        emit advTurboFeedbackActiveChanged();
    }
    return active;
}

bool DecodiumBridge::effectiveCoherentAvg(double snr_db_avg)
{
    bool active;
    if (m_advAutoModeEnabled) {
        // Trigger Q65: SNR medio basso (segnali EME tipici)
        active = snr_db_avg < -22.0;
    } else {
        active = m_coherentAvgEnabled;
    }
    if (m_advCoherentAvgActive != active) {
        m_advCoherentAvgActive = active;
        emit advCoherentAvgActiveChanged();
    }
    return active;
}

