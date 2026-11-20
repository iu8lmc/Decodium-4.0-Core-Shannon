#include "DecodiumBridge.h"
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>
#include <algorithm>
#include <cmath>

void decodiumBridgeAudioLevelLog(const QString&);
#define bridgeLog decodiumBridgeAudioLevelLog

void DecodiumBridge::sendCwAudio(const QString& text, qint64 dialFrequencyHz, int wpm)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "sendCwAudio", Qt::QueuedConnection,
                                  Q_ARG(QString, text),
                                  Q_ARG(qint64, dialFrequencyHz),
                                  Q_ARG(int, wpm));
        return;
    }

    QString const msg = text.trimmed();
    if (msg.isEmpty()) {
        emit statusMessage(QStringLiteral("CW: testo vuoto"));
        return;
    }
    int w = wpm;
    if (w < 5)  w = 5;
    if (w > 60) w = 60;
    if (m_transmitting || m_tuning) {
        emit statusMessage(QStringLiteral("CW: TX o Tune gia' attivo"));
        return;
    }

    // Arma lo stato CW: startTx() e ensureTxAudioPrepared() lo useranno per
    // generare/trasmettere la waveform CW invece del payload FT.
    m_pendingCwText = msg;
    m_cwWpm = w;
    m_cwTxActive = true;
    armExplicitManualTxRequest(m_currentTx, QStringLiteral("CW audio"));

    // QSY opzionale: dial = freq_richiesta - sidetone, cosi' in USB il tono
    // sidetone emerge esattamente sulla frequenza richiesta.
    if (dialFrequencyHz > 0) {
        double dial = static_cast<double>(dialFrequencyHz - m_cwSidetoneHz);
        if (dial < 1.0) dial = static_cast<double>(dialFrequencyHz);
        setFrequency(dial);
    }

    bridgeLog(QStringLiteral("sendCwAudio: text=[%1] wpm=%2 dial=%3 sidetone=%4")
                  .arg(msg).arg(w).arg(dialFrequencyHz).arg(m_cwSidetoneHz));

    startTx();

    if (!m_transmitting) {
        // startTx() ha abortito (es. backend non pronto): non lasciare lo
        // stato CW armato, altrimenti il prossimo TX FT verrebbe deviato.
        m_cwTxActive = false;
        m_pendingCwText.clear();
        emit statusMessage(QStringLiteral("CW: avvio TX non riuscito"));
    }
}

// ============================================================
// Telemetria stazione+meteo (FT8/FT4 tipo 0.5, opt-in) — vedi
// src/radio/StationTelemetryCodec.hpp per il layout dei 71 bit.
// ============================================================

bool DecodiumBridge::transmitFt2LinkAudio(const QString& text,
                                          const QVector<float>& wave,
                                          const QVariantMap& plan)
{
    if (QThread::currentThread() != thread()) {
        QString const queuedText = text;
        QVector<float> const queuedWave = wave;
        QVariantMap const queuedPlan = plan;
        QMetaObject::invokeMethod(this, [this, queuedText, queuedWave, queuedPlan]() {
            transmitFt2LinkAudio(queuedText, queuedWave, queuedPlan);
        }, Qt::QueuedConnection);
        return true;
    }

    QString const msg = text.trimmed().isEmpty()
        ? QStringLiteral("FT2-Link DATA")
        : text.trimmed();
    if (wave.isEmpty()) {
        emit errorMessage(QStringLiteral("FT2-Link: piano TX radio vuoto"));
        return false;
    }
    if (m_transmitting || m_tuning) {
        emit statusMessage(QStringLiteral("FT2-Link: TX o Tune gia' attivo"));
        return false;
    }

    double peak = 0.0;
    for (float sample : wave) {
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
    }
    if (peak <= 1.0e-7) {
        emit errorMessage(QStringLiteral("FT2-Link: piano TX radio silenzioso"));
        return false;
    }

    m_pendingFt2LinkText = msg;
    m_pendingFt2LinkWave = wave;
    m_pendingFt2LinkPlan = plan;
    m_ft2LinkTxActive = true;

    // Satellite half-duplex has an explicit CAT VFO/Ptt sequence.  Do not
    // fall through to the normal startTx path until RX and TX VFOs have been
    // programmed and have observed the configured settle guard.
    if (ft2LinkSatelliteHalfDuplexRequested()) {
        if (!beginFt2LinkSatelliteHalfDuplexTx()) {
            m_ft2LinkTxActive = false;
            m_pendingFt2LinkText.clear();
            m_pendingFt2LinkWave.clear();
            m_pendingFt2LinkPlan.clear();
            emit ft2LinkTxFinished();
            return false;
        }
        return true;
    }

    int rxBufferedBeforeTx = 0;
    {
        QMutexLocker locker(&m_audioBufferMutex);
        rxBufferedBeforeTx = m_audioBuffer.size();
    }
    QString txPlanLog = QStringLiteral(
        "FT2-Link radio TX requested: msg=[%1] samples=%2 peak=%3")
        .arg(msg)
        .arg(wave.size())
        .arg(peak, 0, 'f', 6);
    txPlanLog += QStringLiteral(
        " kind=%1 profile=%2 rate=%3 bursts=%4 frames=%5 duration=%6s")
        .arg(plan.value(QStringLiteral("kind")).toString(),
             plan.value(QStringLiteral("profileName")).toString(),
             plan.value(QStringLiteral("w2300RateModeName")).toString(),
             plan.value(QStringLiteral("burstCount")).toString(),
             plan.value(QStringLiteral("frameCount")).toString(),
             QString::number(plan.value(QStringLiteral("audioSeconds")).toDouble(), 'f', 3));
    txPlanLog += QStringLiteral(
        " centerHz=%1 lowHz=%2 highHz=%3 tones=[%4] carriers=[%5] nominalWidthHz=%6")
        .arg(QString::number(plan.value(QStringLiteral("audioCenterHz")).toDouble(), 'f', 1),
             QString::number(plan.value(QStringLiteral("audioLowHz")).toDouble(), 'f', 1),
             QString::number(plan.value(QStringLiteral("audioHighHz")).toDouble(), 'f', 1),
             plan.value(QStringLiteral("audioToneHz")).toString(),
             plan.value(QStringLiteral("audioCarrierHz")).toString(),
             QString::number(plan.value(QStringLiteral("audioNominalWidthHz")).toDouble(), 'f', 1));
    txPlanLog += QStringLiteral(
        " queued=%1 lbt=%2 plan=[%3] mode=%4 monitor=%5 soundInput=%6 rxSuspended=%7 rxBuffered=%8 tx=%9 tune=%10")
        .arg(plan.value(QStringLiteral("queued")).toBool() ? 1 : 0)
        .arg(plan.value(QStringLiteral("lbtDeferred")).toBool() ? 1 : 0)
        .arg(plan.value(QStringLiteral("audioPlan")).toString())
        .arg(m_mode)
        .arg(m_monitoring ? QStringLiteral("on") : QStringLiteral("off"))
        .arg(m_soundInput ? QStringLiteral("alive") : QStringLiteral("null"))
        .arg(m_rxAudioSuspendedForTx ? QStringLiteral("yes") : QStringLiteral("no"))
        .arg(rxBufferedBeforeTx)
        .arg(m_transmitting ? 1 : 0)
        .arg(m_tuning ? 1 : 0);
    bridgeLog(txPlanLog);

    armExplicitManualTxRequest(m_currentTx, QStringLiteral("FT2-Link audio"));
    startTx();

    if (!m_transmitting) {
        m_ft2LinkTxActive = false;
        m_pendingFt2LinkText.clear();
        m_pendingFt2LinkWave.clear();
        m_pendingFt2LinkPlan.clear();
        emit errorMessage(QStringLiteral("FT2-Link: avvio TX radio non riuscito"));
        emit ft2LinkTxFinished();
        return false;
    }

    return true;
}

#undef bridgeLog
