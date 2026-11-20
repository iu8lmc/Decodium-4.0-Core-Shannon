#include "DecodiumBridge.h"
#include "src/rtl/RtlSdrInput.h"
#include <QDateTime>

void decodiumBridgeAudioLevelLog(const QString&);
QString decodiumBridgeAudioDeviceIdSettingForLog(QString);
bool decodiumBridgeIsFt2LinkApplicationMode(QString);

void DecodiumBridge::handleAudioHealth(double rms,
                                       double peak,
                                       int dynamicRange,
                                       int clippedSamples,
                                       int samples)
{
    const bool captureActive = (m_soundInput != nullptr)
        || m_tciAudioCaptureActive
        || (m_rtlSdrInput && m_rtlSdrInput->isRunning());
    if (!m_monitoring || !captureActive || samples <= 0) {
        m_audioUnhealthyStartMs = 0;
        m_audioOverdriveStartMs = 0;
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_lastAudioHealthMs = now;
    m_lastAudioHealthRms = rms;
    m_lastAudioHealthPeak = peak;
    m_lastAudioHealthClippedSamples = clippedSamples;
    m_lastAudioHealthSamples = samples;

    if (m_pendingRxAudioStartupHealthLog) {
        qint64 const startupAgeMs = m_rxAudioStartupStartMs > 0
            ? now - m_rxAudioStartupStartMs
            : 0;
        if (startupAgeMs >= 1000) {
            decodiumBridgeAudioLevelLog(QStringLiteral("Audio RX startup health: selected=[%1] id=%2 age_ms=%3 rms=%4 peak=%5 range=%6 clipped=%7/%8")
                          .arg(m_activeRxInputDeviceName.isEmpty()
                                   ? QStringLiteral("<unknown>")
                                   : m_activeRxInputDeviceName,
                               decodiumBridgeAudioDeviceIdSettingForLog(m_activeRxInputDeviceId),
                               QString::number(startupAgeMs),
                               QString::number(rms, 'g', 4),
                               QString::number(peak, 'g', 4),
                               QString::number(dynamicRange),
                               QString::number(clippedSamples),
                               QString::number(samples)));
            m_pendingRxAudioStartupHealthLog = false;
        }
    }

    if (now < m_audioWatchdogIgnoreUntilMs
        || m_transmitting
        || m_tuning
        || m_rxAudioSuspendedForTx) {
        m_audioUnhealthyStartMs = 0;
        m_audioOverdriveStartMs = 0;
        return;
    }

    constexpr int kFlatDynamicRange = 16;
    constexpr double kSilentRms = 0.00003;
    constexpr double kSilentPeak = 0.0001;
    constexpr int kClippedPercent = 35;
    constexpr double kClippedRms = 0.30;
    constexpr double kSquareMinRms = 0.08;
    constexpr double kSquareMinPeak = 0.08;
    constexpr double kSquareMaxCrestFactor = 1.12;
    constexpr double kOverdrivePeak = 0.98;
    constexpr double kOverdriveRms = 0.35;
    constexpr qint64 kOverdriveWarnMs = 3000;
    constexpr qint64 kOverdriveLogCooldownMs = 10000;
    constexpr qint64 kUnhealthyMs = 8000;
    constexpr qint64 kLogCooldownMs = 10000;

    const double crestFactor = rms > 1e-9 ? peak / rms : 999.0;
    updateAutoRxInputLevel(rms, peak, clippedSamples, samples, now);
    const bool flatBlock =
        dynamicRange <= kFlatDynamicRange
        || (rms <= kSilentRms && peak <= kSilentPeak);
    const bool clippedBlock =
        samples >= 64
        && clippedSamples * 100 >= samples * kClippedPercent
        && rms >= kClippedRms;
    const bool squareLikeBlock =
        samples >= 64
        && rms >= kSquareMinRms
        && peak >= kSquareMinPeak
        && crestFactor <= kSquareMaxCrestFactor;
    const bool overdrivenBlock =
        samples >= 64
        && peak >= kOverdrivePeak
        && rms >= kOverdriveRms;

    QString const normalizedMode = m_mode.trimmed().toUpper();

    // FT2-Link uses a streaming decoder and can legitimately sit on a quiet or
    // digitally silent input while waiting for a burst. Treating that as a dead
    // RX path causes repeated capture restarts, especially with virtual cables
    // during lab tests.
    if (decodiumBridgeIsFt2LinkApplicationMode(m_mode)
        && flatBlock
        && !clippedBlock
        && !squareLikeBlock
        && !overdrivenBlock) {
        m_audioUnhealthyStartMs = 0;
        return;
    }

    // Long synchronized weak-signal modes need an uninterrupted receive window.
    // A quiet BlackHole/USB input before or after a burst is normal, and a
    // watchdog restart here discards the accumulation that JT4, JT65, JT9,
    // Q65, FST4/FST4W and WSPR need for decode.
    bool const longSyncMode =
        normalizedMode == QStringLiteral("JT4")
        || normalizedMode == QStringLiteral("JT65")
        || normalizedMode == QStringLiteral("JT9")
        || normalizedMode == QStringLiteral("WSPR")
        || normalizedMode == QStringLiteral("Q65")
        || normalizedMode.startsWith(QStringLiteral("Q65-"))
        || normalizedMode == QStringLiteral("FST4")
        || normalizedMode == QStringLiteral("FST4W")
        || normalizedMode.startsWith(QStringLiteral("FST4-"))
        || normalizedMode.startsWith(QStringLiteral("FST4W-"));
    if (longSyncMode
        && flatBlock
        && !clippedBlock
        && !squareLikeBlock
        && !overdrivenBlock) {
        m_audioUnhealthyStartMs = 0;
        return;
    }

    if (overdrivenBlock) {
        if (m_audioOverdriveStartMs == 0) {
            m_audioOverdriveStartMs = now;
        }
        const qint64 overdriveForMs = now - m_audioOverdriveStartMs;
        if (overdriveForMs >= kOverdriveWarnMs
            && now - m_lastAudioOverdriveWarningMs >= kOverdriveLogCooldownMs) {
            decodiumBridgeAudioLevelLog(QStringLiteral("Audio warning: overdriven RX audio rms=%1 peak=%2 crest=%3 range=%4 clipped=%5/%6 rxLevel=%7 overdrive_ms=%8")
                          .arg(rms, 0, 'g', 4)
                          .arg(peak, 0, 'g', 4)
                          .arg(crestFactor, 0, 'g', 4)
                          .arg(dynamicRange)
                          .arg(clippedSamples)
                          .arg(samples)
                          .arg(m_rxInputLevel, 0, 'f', 1)
                          .arg(overdriveForMs));
            emit statusMessage(QStringLiteral("Audio RX troppo alto/clippato: abbassa livello Windows/rig o RX Input"));
            m_lastAudioOverdriveWarningMs = now;
        }
    } else {
        m_audioOverdriveStartMs = 0;
    }

    if (!flatBlock && !clippedBlock && !squareLikeBlock) {
        m_audioUnhealthyStartMs = 0;
        return;
    }

    if (m_audioUnhealthyStartMs == 0) {
        m_audioUnhealthyStartMs = now;
    }

    const qint64 unhealthyForMs = now - m_audioUnhealthyStartMs;
    if (unhealthyForMs < kUnhealthyMs) {
        return;
    }

    const QString kind =
        squareLikeBlock ? QStringLiteral("square-like")
                        : clippedBlock ? QStringLiteral("clipped")
                                       : QStringLiteral("flat");
    // This callback is proof that capture is alive.  A silent, clipped or
    // square-like PCM stream may need user intervention, but tearing down a
    // healthy QAudioSource cannot repair its content and is unsafe on Qt 6.11
    // PipeWire: immediate source recreation can deliver a stale Multimedia
    // event to a retired backend object.  Keep the Auto-CQ health gate armed,
    // report the condition, and reserve capture recovery for the no-callback
    // watchdog paths (waterfall/post-TX recovery).
    if (now - m_lastAudioWatchdogLogMs >= kLogCooldownMs) {
        decodiumBridgeAudioLevelLog(QStringLiteral("Audio watchdog: RX stream still delivers %1 samples; capture retained rms=%2 peak=%3 crest=%4 range=%5 clipped=%6/%7 unhealthy_ms=%8")
                      .arg(kind)
                      .arg(rms, 0, 'g', 4)
                      .arg(peak, 0, 'g', 4)
                      .arg(crestFactor, 0, 'g', 4)
                      .arg(dynamicRange)
                      .arg(clippedSamples)
                      .arg(samples)
                      .arg(unhealthyForMs));
        m_lastAudioWatchdogLogMs = now;
    }
}

