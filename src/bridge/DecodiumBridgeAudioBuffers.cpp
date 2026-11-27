#include "DecodiumBridge.h"
#include "MamTxPayloadPolicy.h"
#include <QMutexLocker>
#include <QTimer>

void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::invalidateTxAudioCache()
{
    m_txAudioCache = TxAudioCache {};
}

void DecodiumBridge::clearMamPendingTxPayload(const QString& reason)
{
    const int messageCount = m_mamMessages.size();
    const int frequencyCount = m_mamF0sHz.size();
    const bool hadPayload = decodium::mamtx::clearPendingPayload(m_mamMessages,
                                                                   m_mamF0sHz);
    // A MAM composite can have the same representative message, frequency and
    // period as mono TX. Always invalidate it, even if the vectors were
    // already empty, so the next manual transmission is regenerated safely.
    m_mamGains.clear();
    invalidateTxAudioCache();
    if (hadPayload) {
        emit mamTxStreamsChanged();
        decodiumBridgeAudioLevelLog(QStringLiteral("MAM pending TX payload cleared (%1): messages=%2 frequencies=%3")
                      .arg(reason.trimmed().isEmpty() ? QStringLiteral("unspecified")
                                                      : reason.trimmed())
                      .arg(messageCount)
                      .arg(frequencyCount));
    }
}

void DecodiumBridge::scheduleIdleAudioBufferRelease(int delayMs)
{
    if (!m_audioBufferReleaseTimer) {
        return;
    }

    int const boundedDelayMs = qBound(5000, delayMs, 300000);
    m_audioBufferReleaseTimer->start(boundedDelayMs);
}

void DecodiumBridge::releaseIdleAudioBuffers()
{
    bool const txPathActive = m_transmitting
        || m_tuning
        || m_bridgeAudioLegacyTxActive
        || m_ft2LinkTxActive
        || m_txPcmBuffer != nullptr;
    if (txPathActive) {
        scheduleIdleAudioBufferRelease(30000);
        return;
    }

    qint64 releasedCapacityBytes = 0;
    auto accountShortVector = [&releasedCapacityBytes](QVector<short> const& buffer) {
        releasedCapacityBytes += static_cast<qint64>(buffer.capacity())
            * static_cast<qint64>(sizeof(short));
    };
    auto accountFloatVector = [&releasedCapacityBytes](QVector<float> const& buffer) {
        releasedCapacityBytes += static_cast<qint64>(buffer.capacity())
            * static_cast<qint64>(sizeof(float));
    };

    accountFloatVector(m_txAudioCache.wave);
    releasedCapacityBytes += m_txAudioCache.pcm.capacity();
    invalidateTxAudioCache();

    releasedCapacityBytes += m_txPcmData.capacity();
    m_txPcmData.clear();
    m_txPcmData.squeeze();

    if (m_pendingFt2LinkText.isEmpty() && m_pendingFt2LinkPlan.isEmpty()) {
        accountFloatVector(m_pendingFt2LinkWave);
        m_pendingFt2LinkWave.clear();
        m_pendingFt2LinkWave.squeeze();
    }

    bool const rxPathIdle = !m_monitoring && !m_monitorRequested;
    if (rxPathIdle) {
        {
            QMutexLocker locker(&m_audioBufferMutex);
            accountShortVector(m_audioBuffer);
            m_audioBuffer.clear();
            m_audioBuffer.squeeze();
        }
        if (!m_pendingTimeSyncDecodeActive) {
            accountShortVector(m_pendingTimeSyncDecodeAudio);
            m_pendingTimeSyncDecodeAudio.clear();
            m_pendingTimeSyncDecodeAudio.squeeze();
        }
        if (!m_forcedDecodeAudioSnapshotActive) {
            accountShortVector(m_forcedDecodeAudioSnapshot);
            m_forcedDecodeAudioSnapshot.clear();
            m_forcedDecodeAudioSnapshot.squeeze();
        }
        if (!m_ft2LinkModernRxDrainScheduled) {
            accountShortVector(m_ft2LinkModernRxPending);
            m_ft2LinkModernRxPending.clear();
            m_ft2LinkModernRxPending.squeeze();
        }
        if (!m_ft2LinkLegacyRxDrainScheduled) {
            accountShortVector(m_ft2LinkLegacyRxPending);
            m_ft2LinkLegacyRxPending.clear();
            m_ft2LinkLegacyRxPending.squeeze();
        }
        accountShortVector(m_spectrumBuf);
        m_spectrumBuf.clear();
        m_spectrumBuf.squeeze();
        accountFloatVector(m_lastPanadapterData);
        m_lastPanadapterData.clear();
        m_lastPanadapterData.squeeze();
    }

    if (releasedCapacityBytes >= 64 * 1024) {
        decodiumBridgeAudioLevelLog(QStringLiteral("[AUDIO-MEM] idle buffers released capacity_kib=%1 rx_idle=%2")
                      .arg(releasedCapacityBytes / 1024)
                      .arg(rxPathIdle ? 1 : 0));
    }
}

