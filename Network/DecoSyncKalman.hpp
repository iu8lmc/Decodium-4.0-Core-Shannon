// -*- Mode: C++ -*-
//
// DecoSyncTime fase 3 — Kalman filter 2D su offset + drift rate.
//
// Modello di stato:
//   x = [offset_ms, drift_ms_per_sec]^T
//
// Transizione (intervallo dt secondi):
//   F = [[1, dt],
//        [0,  1]]
//   x_next = F * x
//   P_next = F * P * F^T + Q
//
// Q = process noise (drift cambia lentamente):
//   Q = [[q_offset*dt,        0          ],
//        [        0,    q_drift*dt   ]]
//
// Measurement: z = offset_observed (ms)
//   H = [1, 0]
//   R = measurement variance (basata su RTT del sample)
//
// Vantaggi rispetto a usare l'offset NTP raw:
// - Compensa drift sistematico del PC tra i sync (es. crystal skew)
// - Filtra outliers via Kalman gain (sample con RTT alto pesano meno)
// - Predicts offset corretto in qualsiasi istante via predict-only tick

#ifndef DECO_SYNC_KALMAN_HPP__
#define DECO_SYNC_KALMAN_HPP__

#include <array>

class DecoSyncKalman
{
public:
    DecoSyncKalman();

    // Re-inizializza lo stato. Usato all'avvio o quando offset salta troppo.
    void reset(double offsetMs = 0.0);

    // Propagation step: avanza lo stato di dt secondi senza nuova osservazione.
    // Va chiamato a frequenza fissa (tipicamente 1 Hz) anche senza sample.
    void predict(double dtSec);

    // Update step: incorpora una nuova osservazione di offset (ms) con la
    // sua varianza stimata (ms^2). La varianza viene tipicamente derivata
    // dal RTT del sample (rtt/2 e' l'incertezza di base sull'half-roundtrip).
    void update(double measuredOffsetMs, double measurementVarianceMs2);

    // Stato stimato corrente.
    double offsetMs()         const { return m_x[0]; }
    double driftMsPerSec()    const { return m_x[1]; }
    double offsetVarianceMs2()const { return m_P[0][0]; }
    double driftVarianceMs2() const { return m_P[1][1]; }
    bool   hasLock()          const { return m_locked; }

    // Tuning runtime
    void setProcessNoiseOffset(double q) { m_qOffsetPerSec = q; }
    void setProcessNoiseDrift(double q)  { m_qDriftPerSec = q; }

private:
    // x e P
    std::array<double, 2> m_x {{0.0, 0.0}};
    std::array<std::array<double, 2>, 2> m_P {{
        {{1.0e9, 0.0}},  // initial offset variance: very high (no lock)
        {{0.0,   1.0e3}} // initial drift variance
    }};
    // Process noise (ms^2 per second integrato)
    // Defaults conservativi: PC crystal drift ~1-10 ppm = 1-10 ms/100s.
    double m_qOffsetPerSec {0.01};   // 0.1 ms/sqrt(s) — small per offset
    double m_qDriftPerSec  {0.00001};// drift change very slow
    bool   m_locked {false};
};

#endif  // DECO_SYNC_KALMAN_HPP__
