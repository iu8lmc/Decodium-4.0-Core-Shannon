#include "DecodiumBridge.h"
#include <QAudioSink>
#include <QBuffer>
#include <QDateTime>
#include <QPointer>
#include <QTimer>

void decodiumBridgeTxAudioTelemetryLog(const QString&);

void DecodiumBridge::resetTxAudioTelemetry(quint64 txSerial, qint64 audioStartWallMs, qint64 expectedUs,
                                           int slotElapsedMs, int leadInMs, int payloadDelayMs,
                                           qint64 pcmBytes, qsizetype sinkBufferSize,
                                           double wavePeak, double effectiveGain)
{
    m_txAudioTelemetrySerial = txSerial;
    m_txAudioTelemetryStartWallMs = audioStartWallMs;
    m_txAudioTelemetryLastWallMs = audioStartWallMs;
    m_txAudioTelemetryLastProcessedUs = 0;
    m_txAudioTelemetryExpectedUs = expectedUs;
    m_txAudioTelemetryMaxLagMs = 0;
    m_txAudioTelemetryMaxStallMs = 0;
    m_txAudioTelemetryLastWarningMs = 0;
    m_txAudioTelemetrySlotElapsedMs = slotElapsedMs;
    m_txAudioTelemetryLeadInMs = leadInMs;
    m_txAudioTelemetryPayloadDelayMs = payloadDelayMs;
    m_txAudioTelemetryPcmBytes = pcmBytes;
    m_txAudioTelemetrySinkBufferSize = sinkBufferSize;
    m_txAudioTelemetryWavePeak = wavePeak;
    m_txAudioTelemetryGain = effectiveGain;
    m_txAudioTelemetryStallCount = 0;
    m_txAudioTelemetryErrorCount = 0;
    m_txAudioTelemetryIdleCount = 0;
    m_txAudioTelemetryUnderrunCount = 0;
}

void DecodiumBridge::scheduleTxAudioTelemetryProbe(quint64 txSerial)
{
    if (!m_txAudioSink || txSerial != m_txPlaybackSerial) {
        return;
    }

    QPointer<QAudioSink> const sinkGuard(m_txAudioSink);
    QPointer<QBuffer> const bufferGuard(m_txPcmBuffer);
    QTimer::singleShot(250, this, [this, txSerial, sinkGuard, bufferGuard]() {
        if (!m_transmitting
            || m_tuning
            || txSerial != m_txPlaybackSerial
            || txSerial != m_txAudioTelemetrySerial
            || !sinkGuard
            || sinkGuard != m_txAudioSink) {
            return;
        }

        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        qint64 const wallMs = qMax<qint64>(0, nowMs - m_txAudioTelemetryStartWallMs);
        qint64 const processedUs = sinkGuard->processedUSecs();
        qint64 const processedMs = qMax<qint64>(0, processedUs / 1000);
        qint64 const lagMs = wallMs - processedMs;
        m_txAudioTelemetryMaxLagMs = qMax(m_txAudioTelemetryMaxLagMs, lagMs);

        qint64 const deltaWallMs = m_txAudioTelemetryLastWallMs > 0
            ? qMax<qint64>(0, nowMs - m_txAudioTelemetryLastWallMs)
            : qint64(0);
        qint64 const deltaProcessedMs =
            qMax<qint64>(0, (processedUs - m_txAudioTelemetryLastProcessedUs) / 1000);
        qint64 const stallMs = qMax<qint64>(0, deltaWallMs - deltaProcessedMs);

        QAudio::State const state = sinkGuard->state();
        QAudio::Error const err = sinkGuard->error();
        bool const bufferIncomplete = bufferGuard && !bufferGuard->atEnd();
        bool const inPlaybackWindow =
            wallMs > 650
            && wallMs < (m_txAudioTelemetryExpectedUs / 1000) + 750;
        bool const processedStalled =
            inPlaybackWindow
            && bufferIncomplete
            && state == QAudio::ActiveState
            && deltaWallMs >= 220
            && deltaProcessedMs < qMin<qint64>(qint64(80), deltaWallMs / 3);
        bool const sinkBadState =
            inPlaybackWindow
            && bufferIncomplete
            && (state == QAudio::IdleState || state == QAudio::StoppedState);
        bool const sinkError = err != QAudio::NoError;

        if (processedStalled) {
            ++m_txAudioTelemetryStallCount;
            m_txAudioTelemetryMaxStallMs = qMax(m_txAudioTelemetryMaxStallMs, stallMs);
        }
        if (sinkError) {
            ++m_txAudioTelemetryErrorCount;
        }
        if (sinkBadState) {
            ++m_txAudioTelemetryIdleCount;
        }
        if (err == QAudio::UnderrunError) {
            ++m_txAudioTelemetryUnderrunCount;
        }

        bool const shouldLog =
            processedStalled
            || sinkBadState
            || sinkError
            || (inPlaybackWindow && lagMs > 900);
        if (shouldLog && nowMs - m_txAudioTelemetryLastWarningMs > 700) {
            m_txAudioTelemetryLastWarningMs = nowMs;
            decodiumBridgeTxAudioTelemetryLog(QStringLiteral("TX audio telemetry warning: serial=%1 mode=%2 TX=%3 state=%4 err=%5 wall=%6ms processed=%7ms dWall=%8ms dProcessed=%9ms lag=%10ms stall=%11ms bytesFree=%12 bufferSize=%13 pcm=%14/%15 slotElapsed=%16ms leadIn=%17ms payloadDelay=%18ms peak=%19 gain=%20 cpuPressure=%21 lowCpu=%22 msg=[%23]")
                          .arg(txSerial)
                          .arg(m_mode)
                          .arg(m_activeTxNumber)
                          .arg(static_cast<int>(state))
                          .arg(static_cast<int>(err))
                          .arg(wallMs)
                          .arg(processedMs)
                          .arg(deltaWallMs)
                          .arg(deltaProcessedMs)
                          .arg(lagMs)
                          .arg(stallMs)
                          .arg(static_cast<qlonglong>(sinkGuard->bytesFree()))
                          .arg(static_cast<qlonglong>(sinkGuard->bufferSize()))
                          .arg(bufferGuard ? bufferGuard->pos() : -1)
                          .arg(bufferGuard ? bufferGuard->size() : -1)
                          .arg(m_txAudioTelemetrySlotElapsedMs)
                          .arg(m_txAudioTelemetryLeadInMs)
                          .arg(m_txAudioTelemetryPayloadDelayMs)
                          .arg(m_txAudioTelemetryWavePeak, 0, 'f', 6)
                          .arg(m_txAudioTelemetryGain, 0, 'f', 4)
                          .arg(cpuPressureActive() ? 1 : 0)
                          .arg(m_lowCpuModeEnabled ? 1 : 0)
                          .arg(m_activeTxMessage));
        }

        m_txAudioTelemetryLastWallMs = nowMs;
        m_txAudioTelemetryLastProcessedUs = processedUs;

        bool const keepProbing =
            m_transmitting
            && !m_tuning
            && txSerial == m_txPlaybackSerial
            && wallMs < (m_txAudioTelemetryExpectedUs / 1000) + 2500;
        if (keepProbing) {
            scheduleTxAudioTelemetryProbe(txSerial);
        }
    });
}

void DecodiumBridge::logTxAudioTelemetrySummary(const QString& reason)
{
    if (m_txAudioTelemetrySerial == 0) {
        return;
    }

    qint64 processedUs = -1;
    int state = -1;
    int err = -1;
    qint64 bytesFree = -1;
    qsizetype sinkBufferSize = -1;
    if (m_txAudioSink) {
        processedUs = m_txAudioSink->processedUSecs();
        state = static_cast<int>(m_txAudioSink->state());
        err = static_cast<int>(m_txAudioSink->error());
        bytesFree = m_txAudioSink->bytesFree();
        sinkBufferSize = m_txAudioSink->bufferSize();
    }

    decodiumBridgeTxAudioTelemetryLog(QStringLiteral("TX audio telemetry summary: serial=%1 reason=%2 mode=%3 TX=%4 processed=%5ms expected=%6ms maxLag=%7ms maxStall=%8ms stalls=%9 errors=%10 idle=%11 underrun=%12 state=%13 err=%14 bytesFree=%15 bufferSize=%16 configuredBuffer=%17 pcm=%18/%19 pcmBytes=%20 slotElapsed=%21ms leadIn=%22ms payloadDelay=%23ms peak=%24 gain=%25 msg=[%26]")
                  .arg(m_txAudioTelemetrySerial)
                  .arg(reason)
                  .arg(m_mode)
                  .arg(m_activeTxNumber)
                  .arg(processedUs >= 0 ? processedUs / 1000 : -1)
                  .arg(m_txAudioTelemetryExpectedUs / 1000)
                  .arg(m_txAudioTelemetryMaxLagMs)
                  .arg(m_txAudioTelemetryMaxStallMs)
                  .arg(m_txAudioTelemetryStallCount)
                  .arg(m_txAudioTelemetryErrorCount)
                  .arg(m_txAudioTelemetryIdleCount)
                  .arg(m_txAudioTelemetryUnderrunCount)
                  .arg(state)
                  .arg(err)
                  .arg(static_cast<qlonglong>(bytesFree))
                  .arg(static_cast<qlonglong>(sinkBufferSize))
                  .arg(static_cast<qlonglong>(m_txAudioTelemetrySinkBufferSize))
                  .arg(m_txPcmBuffer ? m_txPcmBuffer->pos() : -1)
                  .arg(m_txPcmBuffer ? m_txPcmBuffer->size() : -1)
                  .arg(m_txAudioTelemetryPcmBytes)
                  .arg(m_txAudioTelemetrySlotElapsedMs)
                  .arg(m_txAudioTelemetryLeadInMs)
                  .arg(m_txAudioTelemetryPayloadDelayMs)
                  .arg(m_txAudioTelemetryWavePeak, 0, 'f', 6)
                  .arg(m_txAudioTelemetryGain, 0, 'f', 4)
                  .arg(m_activeTxMessage));

    m_txAudioTelemetrySerial = 0;
}

