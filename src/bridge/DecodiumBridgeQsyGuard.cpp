#include "DecodiumBridge.h"
#include <QDateTime>
#include <algorithm>
#include <cmath>

void decodiumBridgeAudioLevelLog(const QString&);

bool DecodiumBridge::shouldIgnoreCatFrequencyDuringLocalQsy(double hz, const QString& backend)
{
    if (hz <= 0.0 || m_localCatFrequencyTargetHz <= 0.0) {
        return false;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_localCatFrequencyGuardUntilMs <= 0 || nowMs > m_localCatFrequencyGuardUntilMs) {
        return false;
    }

    if (std::abs(hz - m_localCatFrequencyTargetHz) <= 1.0) {
        m_localCatFrequencyGuardUntilMs = 0;
        m_localCatFrequencyGuardMaxMs = 0;
        m_localCatFrequencyTargetHz = 0.0;
        m_localCatFrequencyPreviousHz = 0.0;
        return false;
    }

    bool const looksLikePreviousDial =
        m_localCatFrequencyPreviousHz > 0.0
        && std::abs(hz - m_localCatFrequencyPreviousHz) <= 25.0;
    bool const looksLikeManualDialChange =
        !looksLikePreviousDial
        && std::abs(hz - m_localCatFrequencyTargetHz) > 100.0;
    if (looksLikeManualDialChange) {
        decodiumBridgeAudioLevelLog(QStringLiteral("CAT[%1] manual rig frequency accepted during local QSY guard: rig=%2 target=%3 previous=%4")
                      .arg(backend,
                           QString::number(hz, 'f', 0),
                           QString::number(m_localCatFrequencyTargetHz, 'f', 0),
                           QString::number(m_localCatFrequencyPreviousHz, 'f', 0)));
        m_localCatFrequencyGuardUntilMs = 0;
        m_localCatFrequencyGuardMaxMs = 0;
        m_localCatFrequencyTargetHz = 0.0;
        m_localCatFrequencyPreviousHz = 0.0;
        return false;
    }

    if (nowMs - m_lastIgnoredCatFrequencyLogMs >= 500) {
        decodiumBridgeAudioLevelLog(QStringLiteral("CAT[%1] stale frequency ignored after local QSY: rig=%2 target=%3 previous=%4 guard_ms=%5")
                      .arg(backend,
                           QString::number(hz, 'f', 0),
                           QString::number(m_localCatFrequencyTargetHz, 'f', 0),
                           QString::number(m_localCatFrequencyPreviousHz, 'f', 0),
                           QString::number(m_localCatFrequencyGuardUntilMs - nowMs)));
        m_lastIgnoredCatFrequencyLogMs = nowMs;
    }
    // 1.0.362 — ri-arma il guard finche' il rig continua a riportare una freq != target
    // (FT-991 lento): senza questo, dopo gli 8s iniziali una poll stale FT8 passava e la
    // freq tornava a FT8 in FT2. Bounded da m_localCatFrequencyGuardMaxMs (~20s totali).
    if (m_localCatFrequencyGuardMaxMs > 0 && nowMs < m_localCatFrequencyGuardMaxMs) {
        m_localCatFrequencyGuardUntilMs = std::min<qint64>(nowMs + 3000, m_localCatFrequencyGuardMaxMs);
    }
    return true;
}

bool DecodiumBridge::shouldIgnoreLegacyAudioFrequencyDuringLocalQsy(int hz, bool tx)
{
    int const targetHz = tx ? m_localTxFrequencyTargetHz : m_localRxFrequencyTargetHz;
    qint64& guardUntilMs = tx ? m_localTxFrequencyGuardUntilMs : m_localRxFrequencyGuardUntilMs;
    int& targetRef = tx ? m_localTxFrequencyTargetHz : m_localRxFrequencyTargetHz;

    if (targetHz < 0 || guardUntilMs <= 0) {
        return false;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs > guardUntilMs) {
        guardUntilMs = 0;
        targetRef = 0;
        return false;
    }

    if (hz == targetHz) {
        guardUntilMs = 0;
        targetRef = 0;
        return false;
    }

    return true;
}

