// -*- Mode: C++ -*-
#include "DecoSyncSelfCal.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    double medianOf(QVector<double> values)
    {
        if (values.isEmpty()) return 0.0;
        std::sort(values.begin(), values.end());
        int const n = values.size();
        if ((n % 2) == 0) {
            return (values[n / 2 - 1] + values[n / 2]) / 2.0;
        }
        return values[n / 2];
    }

    // Median Absolute Deviation come stima robust della dispersione.
    double madOf(QVector<double> const& values, double med)
    {
        if (values.isEmpty()) return 0.0;
        QVector<double> dev;
        dev.reserve(values.size());
        for (double v : values) dev.append(std::abs(v - med));
        return medianOf(dev);
    }
}

DecoSyncSelfCal::DecoSyncSelfCal(QObject* parent)
    : QObject(parent)
{
    m_flushTimer.setSingleShot(true);
    connect(&m_flushTimer, &QTimer::timeout, this, &DecoSyncSelfCal::onFlushTimeout);
}

void DecoSyncSelfCal::setPeriodMs(int ms)
{
    if (ms < 500) ms = 500;
    if (ms > 60000) ms = 60000;
    m_periodMs = ms;
}

void DecoSyncSelfCal::setEnabled(bool on)
{
    if (m_enabled == on) return;
    m_enabled = on;
    if (!m_enabled) {
        m_pending.clear();
        m_flushTimer.stop();
    }
}

void DecoSyncSelfCal::addDecodeDt(double dtSec, int snrDb)
{
    if (!m_enabled) return;
    if (!std::isfinite(dtSec)) return;
    if (snrDb < m_snrThresholdDb) return;     // decode marginale
    if (std::abs(dtSec) > kMaxDtSec) return;  // decoder out-of-lock

    m_pending.append(dtSec);

    // Riarma il flush timer: chiude la finestra dopo periodMs*1.5 di silenzio
    // (margine per non chiudere prematuramente se i decode arrivano tutti
    // verso la fine del period).
    int const flushMs = static_cast<int>(m_periodMs * 1.5);
    if (!m_flushTimer.isActive() || m_flushTimer.remainingTime() < flushMs) {
        m_flushTimer.start(flushMs);
    }

    // Se gia' superiamo il min, emettiamo subito senza aspettare il flush.
    // Questo accelera la convergenza quando arrivano molti decode in un slot.
    if (m_pending.size() >= m_minSamples * 2) {
        emitIfReady();
    }
}

void DecoSyncSelfCal::onFlushTimeout()
{
    emitIfReady();
}

void DecoSyncSelfCal::emitIfReady()
{
    if (m_pending.size() < m_minSamples) {
        // Non abbastanza dati: lascia il buffer accumulare ancora.
        return;
    }
    QVector<double> snapshot = m_pending;
    m_pending.clear();

    double const medDt = medianOf(snapshot);
    double const madDt = madOf(snapshot, medDt);
    // Convertiamo dt (s) in correzione offset (ms). Convenzione:
    //   dt positivo = signal in ritardo da MY boundary
    //   = my_clock e' in anticipo
    //   = offset_correction = -median(dt) * 1000
    double const offsetMs = -medDt * 1000.0;

    // Varianza: MAD scaled (sigma ≈ 1.4826 * MAD) elevato a 2 + jitter floor.
    // Decoder dt accuracy tipica ~150ms ⇒ floor 150^2 = 22500 ms^2.
    double const sigmaMad = 1.4826 * madDt * 1000.0;
    double variance = sigmaMad * sigmaMad / snapshot.size();
    variance += 22500.0;  // floor: jitter intrinseco del decoder

    m_lastOffsetMs = offsetMs;
    m_lastVarianceMs2 = variance;
    emit offsetEstimateUpdated(offsetMs, variance, snapshot.size());
}
