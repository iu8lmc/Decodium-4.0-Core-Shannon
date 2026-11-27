#include "DecodiumBridge.h"
#include "DecodeUtcTime.h"
#include "Detector/FT4DecodeWorker.hpp"
#include <QDateTime>
#include <QMetaObject>
#include <QMutexLocker>

void decodiumBridgeAudioLevelLog(const QString&);
#define bridgeLog decodiumBridgeAudioLevelLog

void DecodiumBridge::queueFt4DecodeRequest(const QVector<short>& audioSnapshot, quint64 serial,
                                           int nutc, qint64 slotIndexForUtc, int decodeDepth,
                                           int decodeQsoProgress, int cqHint)
{
    if (!m_ft4Worker) {
        return;
    }

    decodium::ft4::DecodeRequest req;
    req.serial = serial;
    req.audio = audioSnapshot;
    req.nutc = nutc;
    req.nqsoprogress = decodeQsoProgress;
    req.nfqso = qBound(m_nfa, m_rxFrequency, m_nfb);
    req.nfa = m_nfa;
    req.nfb = m_nfb;
    int const requestedDepth = (m_transmitting || m_tuning) ? qMin(decodeDepth, 2) : decodeDepth;
    req.ndepth = effectiveFt4DecodeDepth(requestedDepth);
    req.threadCount = effectiveFt4ThreadLimit();
    req.ncontest = m_ncontest;
    req.lapcqonly = cqHint;
    req.mycall = m_callsign.toLocal8Bit();
    req.hiscall = m_dxCall.toLocal8Bit();

    if (!(m_transmitting || m_tuning) && req.ndepth >= 3) {
        m_deepDecodeVisualSerial = serial;
        m_deepDecodeVisualThrottleUntilMs = qMax(
            m_deepDecodeVisualThrottleUntilMs,
            QDateTime::currentMSecsSinceEpoch() + qint64 {7000});
    }

    bridgeLog("FT4 decode dispatch: serial=" + QString::number(serial) +
              " samples=" + QString::number(audioSnapshot.size()) +
              " slot=" + QString::number(slotIndexForUtc) +
              " depth=" + QString::number(req.ndepth) +
              (req.ndepth != decodeDepth ? " requestedDepth=" + QString::number(decodeDepth) : QString()) +
              " threads=" + QString::number(req.threadCount) +
              (ft4AdaptiveCpuLimitActive() ? " adaptive=1" : QString()));

    auto* worker = m_ft4Worker;
    worker->markLatestDecodeSerial(serial);
    QMetaObject::invokeMethod(worker, [worker, req]() {
        worker->decode(req);
    }, Qt::QueuedConnection);
}

void DecodiumBridge::maybeDispatchFt4EarlyDecode(qint64 utcSlot, int msInSlot, int periodMs)
{
    if (m_mode != QStringLiteral("FT4")
        || periodMs != 7500
        || !m_monitoring
        || usingLegacyBackendForTx()
        || m_transmitting
        || m_tuning
        || m_rxAudioSuspendedForTx) {
        return;
    }

    if (m_ft4EarlyDecodeSlot != utcSlot) {
        m_ft4EarlyDecodeSlot = utcSlot;
        m_ft4EarlyDecodeSent = false;
    }
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    bool const latencyGuard = ft4LatencyGuardActive(nowMs);
    int const earlyThresholdMs = latencyGuard ? 5200 : 6250;
    if (m_ft4EarlyDecodeSent || msInSlot < earlyThresholdMs) {
        return;
    }

    if (!latencyGuard && ft4AdaptiveCpuLimitActive(nowMs)) {
        m_ft4EarlyDecodeSent = true;
        if (nowMs - m_lastEarlyDecodeSkipLogMs > 2000) {
            m_lastEarlyDecodeSkipLogMs = nowMs;
            bridgeLog(QStringLiteral("FT4 early decode skipped: adaptive CPU limit"));
        }
        return;
    }

    QString ft4BacklogDetail;
    if (ft4LiveDecodeBacklogActive(nowMs, 5000, 0, &ft4BacklogDetail)) {
        m_ft4EarlyDecodeSent = true;
        activateFt4AdaptiveCpuLimit(nowMs, QStringLiteral("early-skip %1").arg(ft4BacklogDetail), qint64 {3000});
        if (nowMs - m_lastEarlyDecodeSkipLogMs > 2000) {
            m_lastEarlyDecodeSkipLogMs = nowMs;
            bridgeLog(QStringLiteral("FT4 early decode skipped: backlog %1").arg(ft4BacklogDetail));
        }
        return;
    }

    if (cpuPressureActive() || m_lowCpuModeEnabled) {
        m_ft4EarlyDecodeSent = true;
        if (nowMs - m_lastEarlyDecodeSkipLogMs > 2000) {
            m_lastEarlyDecodeSkipLogMs = nowMs;
            bridgeLog(m_lowCpuModeEnabled
                      ? QStringLiteral("FT4 early decode skipped: Low CPU mode")
                      : QStringLiteral("FT4 early decode skipped: CPU pressure"));
        }
        return;
    }

    QVector<short> audioSnapshot;
    {
        QMutexLocker locker(&m_audioBufferMutex);
        audioSnapshot = m_audioBuffer;
    }

    int const minSamples = minimumDecodeSamplesForMode(QStringLiteral("FT4"));
    if (audioSnapshot.size() < minSamples) {
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - m_lastEarlyDecodeSkipLogMs > 1500) {
            m_lastEarlyDecodeSkipLogMs = nowMs;
            bridgeLog("FT4 early decode skipped: samples=" + QString::number(audioSnapshot.size()) +
                      " min=" + QString::number(minSamples));
        }
        return;
    }

    quint64 const serial = ++m_decodeSerial;
    int nutc = utcTokenForSlotStart(utcSlot, periodMs);
    QString utcToken = utcDisplayTokenForSlotStart(utcSlot, periodMs);
    if (nutc <= 0) {
        utcToken = QDateTime::currentDateTimeUtc().toString(QStringLiteral("hhmmss"));
        nutc = utcToken.toInt();
    }

    m_decodeStartMsBySerial.insert(serial, nowMs);
    m_decodeModeBySerial.insert(serial, QStringLiteral("FT4"));
    m_decodeUtcTokenBySerial.insert(serial, utcToken);
    m_decodeSessionBySerial.insert(serial, m_decodeSessionId);

    queueFt4DecodeRequest(audioSnapshot, serial, nutc, utcSlot, qMin(effectiveDecodeDepth(), 2),
                          legacyDecodeQsoProgress(), legacyDecodeCqHint());
    m_ft4EarlyDecodeSent = true;
}

#undef bridgeLog
