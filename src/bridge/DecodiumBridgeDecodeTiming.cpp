#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include <QDateTime>
#include <algorithm>
#include <cmath>

int DecodiumBridge::effectivePeriodMsForMode(const QString& mode) const
{
    QString const normalized = mode.trimmed().toUpper();
    if (normalized == QStringLiteral("MSK144")
        && m_legacyBackend
        && m_legacyBackend->available()
        && m_legacyBackend->mode().trimmed().toUpper() == normalized) {
        int const seconds = m_legacyBackend->trPeriodSeconds();
        if (seconds == 5 || seconds == 10 || seconds == 15 || seconds == 30) {
            return seconds * 1000;
        }
    }
    return periodMsForMode(normalized);
}

int DecodiumBridge::periodMilliseconds() const { return effectivePeriodMsForMode(m_mode); }
int DecodiumBridge::minimumDecodeSamplesForMode(const QString& mode) const
{
    QString const normalized = mode.trimmed().toUpper();
    if (normalized == QStringLiteral("FT8")) {
        return 144000; // 80% of a 15s, 12 kHz FT8 window.
    }
    if (normalized == QStringLiteral("FT4")) {
        return 60000;  // Leaves margin for FT4's shorter DSP window while dropping startup fragments.
    }
    if (normalized == QStringLiteral("Q65")
        || normalized.startsWith(QStringLiteral("Q65-"))) {
        // Q65's native decoder pads a short buffer for memory safety, but a
        // mode change must not turn that safety padding into a fake RF window.
        // Wait for a substantially complete period instead.
        int const targetSamples = targetDecodeSamplesForMode(normalized);
        return targetSamples > 0 ? targetSamples * 4 / 5 : 0;
    }
    if (normalized == QStringLiteral("WSPR")) {
        return 110 * SAMPLE_RATE; // WSPR needs almost the full 120s receive window.
    }
    if (normalized == QStringLiteral("FST4")
        || normalized == QStringLiteral("FST4W")
        || normalized.startsWith(QStringLiteral("FST4-"))
        || normalized.startsWith(QStringLiteral("FST4W-"))) {
        int const targetSamples = targetDecodeSamplesForMode(normalized);
        return targetSamples > 0 ? targetSamples * 4 / 5 : 0;
    }
    if (normalized == QStringLiteral("JT4")
        || normalized == QStringLiteral("JT65")
        || normalized == QStringLiteral("JT9")) {
        return 52 * SAMPLE_RATE; // Long-sync JT modes need a real 60 s accumulation.
    }
    return 0;
}

int DecodiumBridge::targetDecodeSamplesForMode(const QString& mode) const
{
    int const periodMs = effectivePeriodMsForMode(mode);
    if (periodMs <= 0) {
        return 0;
    }
    return periodMs * SAMPLE_RATE / 1000;
}

bool DecodiumBridge::isTimeSyncDecodeMode(const QString& mode) const
{
    QString const normalized = mode.trimmed().toUpper();
    return normalized == QStringLiteral("FT2")
        || normalized == QStringLiteral("FT4")
        || normalized == QStringLiteral("FT8")
        || normalized == QStringLiteral("JT4")
        || normalized == QStringLiteral("JT65")
        || normalized == QStringLiteral("JT9")
        || normalized == QStringLiteral("Q65")
        || normalized.startsWith(QStringLiteral("Q65-"))
        || normalized == QStringLiteral("FST4")
        || normalized == QStringLiteral("FST4W")
        || normalized.startsWith(QStringLiteral("FST4-"))
        || normalized.startsWith(QStringLiteral("FST4W-"))
        || normalized == QStringLiteral("WSPR");
}

void DecodiumBridge::resetTimeSyncDecodeMetrics()
{
    m_decodeStartMsBySerial.clear();
    m_decodeModeBySerial.clear();
    m_decodeUtcTokenBySerial.clear();
    m_decodeSessionBySerial.clear();
    m_ft8PendingDeepFollowups.clear();
    m_ft8DeepFollowupCooldownSlots = 0;  // P0 (1.0.403): re-abilita il deep su reset time-sync
    bool const sampleCountChanged = (m_dtLastSampleCount != 0);
    m_dtLastSampleCount = 0;
    m_totalDecodesForDt = 0;
    m_dtSmoothFactor = 0.5;

    bool const avgChanged = !qFuzzyCompare(m_avgDt + 1.0, 1.0);
    bool const latencyChanged = !qFuzzyCompare(m_decodeLatencyMs + 1.0, 1.0);

    m_avgDt = 0.0;
    m_decodeLatencyMs = 0.0;

    if (avgChanged) {
        emit avgDtChanged();
    }
    if (latencyChanged) {
        emit decodeLatencyMsChanged();
    }
    if (sampleCountChanged) {
        emit timeSyncSampleCountChanged();
    }
}

bool DecodiumBridge::shouldTrackDtSample(int snr, double dtValue, const QString& message,
                                         const QString& decodeMode) const
{
    QString const normalized = decodeMode.trimmed().toUpper();
    if (normalized != QStringLiteral("FT4") && normalized != QStringLiteral("FT8")) {
        return false;
    }

    int const snrThreshold = (normalized == QStringLiteral("FT8")) ? -15 : -16;
    double const dtLimit = (normalized == QStringLiteral("FT8")) ? 0.60 : 0.80;

    if (snr < snrThreshold) {
        return false;
    }
    if (std::abs(dtValue) >= dtLimit) {
        return false;
    }

    return !message.trimmed().endsWith(QLatin1Char('?'));
}

void DecodiumBridge::finalizeTimeSyncDecodeCycle(quint64 serial, const QString& decodeMode,
                                                 const QVector<double>& dtSamples)
{
    qint64 const startedAtMs = m_decodeStartMsBySerial.take(serial);
    m_decodeModeBySerial.remove(serial);
    m_decodeUtcTokenBySerial.remove(serial);
    m_decodeSessionBySerial.remove(serial);

    if (!isTimeSyncDecodeMode(decodeMode)) {
        return;
    }

    if (startedAtMs > 0) {
        double const latencyMs =
            static_cast<double>(QDateTime::currentMSecsSinceEpoch() - startedAtMs);
        if (!qFuzzyCompare(m_decodeLatencyMs + 1.0, latencyMs + 1.0)) {
            m_decodeLatencyMs = latencyMs;
            emit decodeLatencyMsChanged();
        }
    }

    int const nextSampleCount = dtSamples.size();
    if (m_dtLastSampleCount != nextSampleCount) {
        m_dtLastSampleCount = nextSampleCount;
        emit timeSyncSampleCountChanged();
    } else {
        m_dtLastSampleCount = nextSampleCount;
    }

    bool avgChanged = false;
    if (dtSamples.size() >= m_dtMinSamples) {
        QVector<double> sorted = dtSamples;
        std::sort(sorted.begin(), sorted.end());
        double const medianDt = sorted[sorted.size() / 2];

        if (m_totalDecodesForDt < 20) {
            m_dtSmoothFactor = 0.5;
        } else if (std::abs(m_avgDt) < 0.1) {
            m_dtSmoothFactor = 0.15;
        } else {
            m_dtSmoothFactor = 0.3;
        }

        double const nextAvg = (m_totalDecodesForDt == 0)
            ? medianDt
            : (m_dtSmoothFactor * medianDt + (1.0 - m_dtSmoothFactor) * m_avgDt);

        if (!qFuzzyCompare(m_avgDt + 1.0, nextAvg + 1.0)) {
            m_avgDt = nextAvg;
            avgChanged = true;
        }
        m_totalDecodesForDt += dtSamples.size();
    } else if (dtSamples.isEmpty() && m_totalDecodesForDt > 0) {
        double const decayedAvg = (std::abs(m_avgDt) < 0.0005) ? 0.0 : (m_avgDt * 0.9);
        if (!qFuzzyCompare(m_avgDt + 1.0, decayedAvg + 1.0)) {
            m_avgDt = decayedAvg;
            avgChanged = true;
        }
    }

    if (avgChanged) {
        emit avgDtChanged();
    }
}

