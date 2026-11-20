#include "DecodiumBridge.h"

float decodiumBridgeRxInputGainFromLevel(double);
double decodiumBridgeRxInputLevelFromGain(double);
double decodiumBridgeAutoRxInputMaxLevel();
void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::updateAutoRxInputLevel(double rms,
                                            double peak,
                                            int clippedSamples,
                                            int samples,
                                            qint64 now)
{
    if (!m_autoRxInputLevel || samples < 64) {
        m_autoRxLevelLowStartMs = 0;
        return;
    }
    if (now < m_autoRxLevelManualHoldUntilMs) {
        m_autoRxLevelLowStartMs = 0;
        return;
    }

    constexpr double kTargetPeak = 0.70;     // circa -3 dBFS: alto, ma con margine dal clipping.
    constexpr double kTargetRms = 0.045;     // circa -27 dBFS RMS, evita input inutilmente fiacchi.
    constexpr double kHotPeak = 0.93;        // riduci solo quando il picco e' davvero vicino al limite.
    constexpr double kClipPeak = 0.985;
    constexpr double kLowPeak = 0.42;        // sotto ~-7.5 dBFS si puo' salire se il segnale e' utile.
    constexpr double kLowRms = 0.030;
    constexpr double kUsefulPeak = 0.012;    // circa -38 dBFS.
    constexpr double kUsefulRms = 0.0008;
    constexpr qint64 kFastCooldownMs = 600;
    constexpr qint64 kSlowCooldownMs = 2000;
    constexpr qint64 kLowHoldMs = 3000;
    constexpr double kMinGain = 0.05;
    const double kMaxGain = decodiumBridgeRxInputGainFromLevel(decodiumBridgeAutoRxInputMaxLevel());

    const double clippedRatio = samples > 0
        ? static_cast<double>(clippedSamples) / static_cast<double>(samples)
        : 0.0;
    const bool clipped = clippedSamples >= 2 || clippedRatio >= 0.001 || peak >= kClipPeak;
    const bool hot = peak >= kHotPeak;
    const double currentGain = decodiumBridgeRxInputGainFromLevel(m_rxInputLevel);
    if (m_rxInputLevel > decodiumBridgeAutoRxInputMaxLevel()) {
        applyRxInputLevel(decodiumBridgeAutoRxInputMaxLevel(), true);
        m_autoRxLevelLastAdjustMs = now;
        m_autoRxLevelLowStartMs = 0;
        decodiumBridgeAudioLevelLog(QStringLiteral("Auto RX level capped: level=%1 peak=%2 rms=%3")
                      .arg(m_rxInputLevel, 0, 'f', 1)
                      .arg(peak, 0, 'g', 4)
                      .arg(rms, 0, 'g', 4));
        return;
    }

    if ((clipped || hot) && peak > 1e-6) {
        if (now - m_autoRxLevelLastAdjustMs < kFastCooldownMs) {
            return;
        }

        const double targetGain = qBound(kMinGain, currentGain * (kTargetPeak / peak), kMaxGain);
        const double newLevel = decodiumBridgeRxInputLevelFromGain(targetGain);
        if (newLevel < m_rxInputLevel - 0.5) {
            applyRxInputLevel(newLevel, true);
            m_autoRxLevelLastAdjustMs = now;
            m_autoRxLevelLowStartMs = 0;
            decodiumBridgeAudioLevelLog(QStringLiteral("Auto RX level down: level=%1 peak=%2 rms=%3 clipped=%4/%5")
                          .arg(m_rxInputLevel, 0, 'f', 1)
                          .arg(peak, 0, 'g', 4)
                          .arg(rms, 0, 'g', 4)
                          .arg(clippedSamples)
                          .arg(samples));
        }
        return;
    }

    const bool usefulLowSignal =
        peak >= kUsefulPeak
        && rms >= kUsefulRms
        && (peak < kLowPeak || rms < kLowRms);
    if (!usefulLowSignal || m_rxInputLevel >= decodiumBridgeAutoRxInputMaxLevel() - 0.5) {
        m_autoRxLevelLowStartMs = 0;
        return;
    }

    if (m_autoRxLevelLowStartMs == 0) {
        m_autoRxLevelLowStartMs = now;
        return;
    }
    if (now - m_autoRxLevelLowStartMs < kLowHoldMs
        || now - m_autoRxLevelLastAdjustMs < kSlowCooldownMs) {
        return;
    }

    double idealGain = currentGain * (kTargetPeak / qMax(peak, 1e-6));
    if (rms > 1e-9) {
        idealGain = qMin(idealGain, currentGain * (kTargetRms / rms));
    }
    const double targetGain = qBound(kMinGain,
                                     qMin(currentGain * 1.12, idealGain),
                                     kMaxGain);
    const double newLevel = qMin(decodiumBridgeAutoRxInputMaxLevel(), decodiumBridgeRxInputLevelFromGain(targetGain));
    if (newLevel > m_rxInputLevel + 0.5) {
        applyRxInputLevel(newLevel, true);
        m_autoRxLevelLastAdjustMs = now;
        decodiumBridgeAudioLevelLog(QStringLiteral("Auto RX level up: level=%1 peak=%2 rms=%3")
                      .arg(m_rxInputLevel, 0, 'f', 1)
                      .arg(peak, 0, 'g', 4)
                      .arg(rms, 0, 'g', 4));
    }
}

