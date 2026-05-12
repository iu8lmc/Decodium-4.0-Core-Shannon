// -*- Mode: C++ -*-
#include "DecoSyncKalman.hpp"

#include <algorithm>
#include <cmath>

DecoSyncKalman::DecoSyncKalman() = default;

void DecoSyncKalman::reset(double offsetMs)
{
    m_x[0] = offsetMs;
    m_x[1] = 0.0;
    m_P[0][0] = 1.0e9;   // alto: non abbiamo lock confidence
    m_P[0][1] = 0.0;
    m_P[1][0] = 0.0;
    m_P[1][1] = 1.0e3;
    m_locked = false;
}

void DecoSyncKalman::predict(double dtSec)
{
    if (!std::isfinite(dtSec) || dtSec <= 0.0) return;

    // x_next = F * x = [offset + drift*dt, drift]
    double const off  = m_x[0];
    double const drf  = m_x[1];
    m_x[0] = off + drf * dtSec;
    m_x[1] = drf;

    // F = [[1, dt], [0, 1]]
    // P_next = F * P * F^T + Q
    //
    // F*P = [[P00 + dt*P10, P01 + dt*P11],
    //        [        P10,         P11  ]]
    //
    // (F*P)*F^T = [[P00 + dt*(P01+P10) + dt^2*P11, P01 + dt*P11],
    //              [        P10 + dt*P11,                 P11  ]]
    double const P00 = m_P[0][0];
    double const P01 = m_P[0][1];
    double const P10 = m_P[1][0];
    double const P11 = m_P[1][1];

    double const newP00 = P00 + dtSec * (P01 + P10) + dtSec * dtSec * P11;
    double const newP01 = P01 + dtSec * P11;
    double const newP10 = P10 + dtSec * P11;
    double const newP11 = P11;

    // Process noise: Q = diag(q_offset*dt, q_drift*dt)
    m_P[0][0] = newP00 + m_qOffsetPerSec * dtSec;
    m_P[0][1] = newP01;
    m_P[1][0] = newP10;
    m_P[1][1] = newP11 + m_qDriftPerSec * dtSec;
}

void DecoSyncKalman::update(double measuredOffsetMs, double measurementVarianceMs2)
{
    if (!std::isfinite(measuredOffsetMs)) return;
    double const R = std::max(0.25, measurementVarianceMs2);  // floor 0.25 ms^2

    // H = [1, 0]; innovation y = z - x[0]
    double const y = measuredOffsetMs - m_x[0];

    // S = H * P * H^T + R = P[0][0] + R
    double const S = m_P[0][0] + R;
    if (S <= 0.0) return;

    // K = P * H^T / S = [P[0][0]/S, P[1][0]/S]^T
    double const K0 = m_P[0][0] / S;
    double const K1 = m_P[1][0] / S;

    // x_new = x + K*y
    m_x[0] += K0 * y;
    m_x[1] += K1 * y;

    // P_new = (I - K*H) * P_old
    // I - K*H = [[1-K0, 0],
    //            [-K1, 1]]
    //
    // (I-KH)*P = [[(1-K0)*P00, (1-K0)*P01],
    //             [-K1*P00 + P10, -K1*P01 + P11]]
    double const P00 = m_P[0][0];
    double const P01 = m_P[0][1];
    double const P10 = m_P[1][0];
    double const P11 = m_P[1][1];

    m_P[0][0] = (1.0 - K0) * P00;
    m_P[0][1] = (1.0 - K0) * P01;
    m_P[1][0] = P10 - K1 * P00;
    m_P[1][1] = P11 - K1 * P01;

    m_locked = true;
}
