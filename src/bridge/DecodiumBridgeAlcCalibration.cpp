#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include "AlcCalibrationMath.h"
#include <QDateTime>
#include <QSettings>
#include <QTimer>

void decodiumBridgeDecodeFlowLog(const QString&);

void DecodiumBridge::setAlcTarget(int v)
{
    int const clamped = qBound(5, v, 60);
    if (m_alcTarget == clamped)
        return;
    m_alcTarget = clamped;
    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(s);
    s.setValue(QStringLiteral("AlcTarget"), m_alcTarget);
    emit alcTargetChanged();
}

void DecodiumBridge::startAlcCalibration()
{
    if (m_alcCalibrating)
        return;

    // Backend guard: solo hamlib connesso legge l'ALC
    if (m_catBackend != QStringLiteral("hamlib") || !m_hamlibCat || !m_hamlibCat->connected()) {
        m_alcCalStatus = QStringLiteral("ALC not available (Hamlib CAT must be connected)");
        emit alcCalibrationStatusChanged();
        return;
    }

    // Guard active RF only. ManualTxHold is a latched "do not resume normal TX"
    // state after HALT; an explicit one-shot ALC tune carrier may run while it
    // remains set, and the hold continues to block normal auto-TX afterwards.
    if (m_transmitting || m_tuning) {
        m_alcCalStatus = QStringLiteral("Cannot calibrate while transmitting/tuning");
        emit alcCalibrationStatusChanged();
        return;
    }
    if (m_manualTxHold) {
        decodiumBridgeDecodeFlowLog(QStringLiteral("[ALC] calibration allowed while manualTxHold is engaged"));
    }

    m_alcCalStartLevel  = m_txOutputLevel;
    m_alcEma            = 0.0;
    m_alcConvergeCount  = 0;
    m_alcStuckCount     = 0;   // 1.0.325
    m_alcCalStartMs     = QDateTime::currentMSecsSinceEpoch();

    m_alcCalibrating = true;
    emit alcCalibratingChanged();
    m_alcCalStatus = QStringLiteral("Calibrating...");
    emit alcCalibrationStatusChanged();

    if (!m_alcCalTimer) {
        m_alcCalTimer = new QTimer(this);
        m_alcCalTimer->setInterval(500);
        connect(m_alcCalTimer, &QTimer::timeout, this, &DecodiumBridge::onAlcCalibrationTick);
    }
    m_alcCalTimer->start();

    startTune();
    decodiumBridgeDecodeFlowLog(QStringLiteral("[ALC] calibration start target=%1 startLevel=%2")
                  .arg(m_alcTarget)
                  .arg(m_alcCalStartLevel, 0, 'f', 1));
}

void DecodiumBridge::onAlcCalibrationTick()
{
    if (!m_alcCalibrating) return; // 1.0.326 — A3: guard re-entry (tick accodato dopo finish/cancel)

    qint64 const now = QDateTime::currentMSecsSinceEpoch();

    // Timeout assoluto 12s (ridotto da 20s in 1.0.325 — saturazione viene rilevata in ~1.5s)
    if (now - m_alcCalStartMs > 12000) {
        finishAlcCalibration(false, QStringLiteral("timeout"));
        return;
    }

    if (!m_rigAlcValid) {
        if ((now - m_alcCalStartMs) > 2500) {
            finishAlcCalibration(false, QStringLiteral("ALC not reported by Hamlib"));
        } else {
            decodiumBridgeDecodeFlowLog(QStringLiteral("[ALC] tick: waiting for valid Hamlib ALC"));
        }
        return;
    }

    double const alc = m_rigAlc;

    // EMA smussante (alpha=0.4 nuovo dato)
    m_alcEma = (m_alcEma <= 0.0) ? alc : (0.6 * m_alcEma + 0.4 * alc);

    double const err = m_alcEma - static_cast<double>(m_alcTarget);

    // Deadband ±5
    if (qAbs(err) <= 5.0) {
        m_alcConvergeCount++;
        if (m_alcConvergeCount >= 2) {
            finishAlcCalibration(true, QString());
        }
        return;
    }

    m_alcConvergeCount = 0;

    double const oldLevel = m_txOutputLevel;
    double const newLevel = decodium::alc::adjustedAttenuation(oldLevel, err);

    decodiumBridgeDecodeFlowLog(QStringLiteral("[ALC] tick alc=%1 ema=%2 target=%3 err=%4 level=%5->%6")
                  .arg(alc,     0, 'f', 1)
                  .arg(m_alcEma, 0, 'f', 1)
                  .arg(m_alcTarget)
                  .arg(err,     0, 'f', 1)
                  .arg(oldLevel, 0, 'f', 1)
                  .arg(newLevel, 0, 'f', 1));

    // Rilevamento saturazione: il clamp ha bloccato newLevel = oldLevel
    // ma siamo fuori deadband → drive fisicamente al limite, non convergerà mai.
    decodium::alc::DriveLimit const limit =
        decodium::alc::limitReached(oldLevel, newLevel, err);
    if (limit != decodium::alc::DriveLimit::None) {
        m_alcStuckCount++;
        if (m_alcStuckCount >= 3) {
            finishAlcCalibration(
                false,
                limit == decodium::alc::DriveLimit::MaxDrive
                    ? QStringLiteral("max-drive")
                    : QStringLiteral("min-drive"));
            return;
        }
    } else {
        m_alcStuckCount = 0;   // correzione reale applicata, reset contatore
    }

    setTxOutputLevel(newLevel);
}

void DecodiumBridge::finishAlcCalibration(bool success, const QString& reason)
{
    if (m_alcCalTimer)
        m_alcCalTimer->stop();

    stopTune();

    m_alcCalibrating = false;
    emit alcCalibratingChanged();

    if (success) {
        double const finalLevel = m_txOutputLevel;
        double const finalAlc   = m_alcEma;
        m_alcCalStatus = QStringLiteral("Calibration done: level %1 (ALC %2)")
                             .arg(qRound(finalLevel))
                             .arg(qRound(finalAlc));
        // Persisti il livello TX nello store canonico (setTxOutputLevel non lo fa da solo)
        QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        decodium::beginActiveSettingsProfile(s);
        s.setValue(QStringLiteral("txOutputLevel"), finalLevel);
        decodiumBridgeDecodeFlowLog(QStringLiteral("[ALC] calibration OK level=%1 alc=%2")
                      .arg(finalLevel, 0, 'f', 1)
                      .arg(finalAlc,   0, 'f', 1));
    } else {
        setTxOutputLevel(m_alcCalStartLevel);
        // 1.0.325 — messaggi dettagliati per saturazione fisica
        if (reason == QStringLiteral("max-drive")) {
            m_alcCalStatus = QString(QStringLiteral(
                "Drive al massimo ma ALC solo %1 (target %2). "
                "Alza il MIC/USB GAIN sulla radio, oppure abbassa il target."))
                    .arg(qRound(m_alcEma))
                    .arg(m_alcTarget);
        } else if (reason == QStringLiteral("min-drive")) {
            m_alcCalStatus = QString(QStringLiteral(
                "Drive al minimo ma ALC ancora %1 (target %2). "
                "Abbassa il MIC/USB GAIN sulla radio, oppure alza il target."))
                    .arg(qRound(m_alcEma))
                    .arg(m_alcTarget);
        } else {
            m_alcCalStatus = QStringLiteral("Calibration ") + reason
                             + QStringLiteral(" — level restored");
        }
        decodiumBridgeDecodeFlowLog(QStringLiteral("[ALC] calibration aborted: ") + reason);
    }

    emit alcCalibrationStatusChanged();
}

void DecodiumBridge::cancelAlcCalibration()
{
    if (m_alcCalibrating)
        finishAlcCalibration(false, QStringLiteral("cancelled"));
}
