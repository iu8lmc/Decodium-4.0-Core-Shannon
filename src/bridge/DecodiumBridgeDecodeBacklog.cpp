#include "DecodiumBridge.h"
#include "Detector/FT8DecodeWorker.hpp"
#include "Detector/FT2DecodeWorker.hpp"
#include "Detector/FT4DecodeWorker.hpp"
#include "Detector/Q65DecodeWorker.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QMetaObject>
#include <QMutexLocker>
#include <limits>

void decodiumBridgeDecodeBacklogLog(const QString& message);
bool decodiumBridgeIsFtx77MessageMode(QString const& mode);

bool DecodiumBridge::ft8LiveDecodeBacklogActive(qint64 nowMs, int minAgeMs,
                                                quint64 ignoreSerial,
                                                QString* detail) const
{
    int pendingFt8 = 0;
    qint64 oldestAgeMs = 0;
    quint64 oldestSerial = 0;
    QString oldestKind;

    for (auto it = m_decodeStartMsBySerial.constBegin(); it != m_decodeStartMsBySerial.constEnd(); ++it) {
        quint64 const serial = it.key();
        if (serial == ignoreSerial) {
            continue;
        }
        if (m_decodeModeBySerial.value(serial).trimmed().toUpper() != QStringLiteral("FT8")) {
            continue;
        }

        ++pendingFt8;
        qint64 const ageMs = qMax<qint64>(0, nowMs - it.value());
        if (ageMs > oldestAgeMs) {
            oldestAgeMs = ageMs;
            oldestSerial = serial;
            if (m_ft8EarlyDecodeSerials.contains(serial)) {
                oldestKind = QStringLiteral("early");
            } else if (m_ft8DeepInTxSerials.contains(serial)) {
                oldestKind = QStringLiteral("list-only");
            } else if (m_ft8PendingDeepFollowups.contains(serial)) {
                oldestKind = QStringLiteral("fast-pending-deep");
            } else if (m_ft8PendingSubpassHarvest.contains(serial)) {
                oldestKind = QStringLiteral("deep-pending-harvest");
            } else {
                oldestKind = QStringLiteral("live");
            }
        }
    }

    bool const tooOld = oldestAgeMs >= minAgeMs;
    bool const tooQueued = pendingFt8 >= 5 && oldestAgeMs >= 4000;
    bool const active = tooOld || tooQueued;
    if (detail) {
        *detail = QStringLiteral("pending=%1 oldestSerial=%2 oldestAgeMs=%3 oldestKind=%4 minAgeMs=%5")
            .arg(pendingFt8)
            .arg(oldestSerial)
            .arg(oldestAgeMs)
            .arg(oldestKind)
            .arg(minAgeMs);
    }
    return active;
}

void DecodiumBridge::drainFt8LiveDecodeBacklog(quint64 keepSerial, const QString& reason)
{
    int removedMeta = 0;
    QList<quint64> const serials = m_decodeStartMsBySerial.keys();
    for (quint64 const serial : serials) {
        if (serial == keepSerial) {
            continue;
        }
        if (m_decodeModeBySerial.value(serial).trimmed().toUpper() != QStringLiteral("FT8")) {
            continue;
        }
        m_decodeStartMsBySerial.remove(serial);
        m_decodeModeBySerial.remove(serial);
        m_decodeUtcTokenBySerial.remove(serial);
        m_decodeSessionBySerial.remove(serial);
        m_ft8EarlyDecodeSerials.remove(serial);
        m_ft8DeepInTxSerials.remove(serial);
        ++removedMeta;
    }

    m_ft8PendingDeepFollowups.clear();
    m_ft8PendingSubpassHarvest.clear();
    m_ft8DeepFollowupCooldownSlots = qMax(m_ft8DeepFollowupCooldownSlots, 6);

    if (m_ft8Worker) {
        m_ft8Worker->cancelCurrentDecode();
        QCoreApplication::removePostedEvents(m_ft8Worker, QEvent::MetaCall);
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastFt8BacklogDrainLogMs >= 2000) {
        m_lastFt8BacklogDrainLogMs = nowMs;
        decodiumBridgeDecodeBacklogLog(QStringLiteral("FT8 live backlog drain: keepSerial=%1 removed=%2 cooldown=%3 %4")
                      .arg(keepSerial)
                      .arg(removedMeta)
                      .arg(m_ft8DeepFollowupCooldownSlots)
                      .arg(reason));
    }
}

bool DecodiumBridge::ft4LiveDecodeBacklogActive(qint64 nowMs, int minAgeMs,
                                                quint64 ignoreSerial,
                                                QString* detail) const
{
    int pendingFt4 = 0;
    qint64 oldestAgeMs = 0;
    quint64 oldestSerial = 0;

    for (auto it = m_decodeStartMsBySerial.constBegin(); it != m_decodeStartMsBySerial.constEnd(); ++it) {
        quint64 const serial = it.key();
        if (serial == ignoreSerial) {
            continue;
        }
        if (m_decodeModeBySerial.value(serial).trimmed().toUpper() != QStringLiteral("FT4")) {
            continue;
        }

        ++pendingFt4;
        qint64 const ageMs = qMax<qint64>(0, nowMs - it.value());
        if (ageMs > oldestAgeMs) {
            oldestAgeMs = ageMs;
            oldestSerial = serial;
        }
    }

    bool const tooOld = oldestAgeMs >= minAgeMs;
    bool const tooQueued = pendingFt4 >= 2 && oldestAgeMs >= 3500;
    bool const active = tooOld || tooQueued;
    if (detail) {
        *detail = QStringLiteral("pending=%1 oldestSerial=%2 oldestAgeMs=%3 minAgeMs=%4")
            .arg(pendingFt4)
            .arg(oldestSerial)
            .arg(oldestAgeMs)
            .arg(minAgeMs);
    }
    return active;
}

void DecodiumBridge::drainFt4LiveDecodeBacklog(quint64 keepSerial, const QString& reason)
{
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    static constexpr qint64 kFt4PreserveRunningDecodeMaxAgeMs = 9000;
    quint64 preserveRunningSerial = 0;
    qint64 preserveRunningAgeMs = -1;

    for (auto it = m_decodeStartMsBySerial.constBegin(); it != m_decodeStartMsBySerial.constEnd(); ++it) {
        quint64 const serial = it.key();
        if (serial == keepSerial) {
            continue;
        }
        if (m_decodeModeBySerial.value(serial).trimmed().toUpper() != QStringLiteral("FT4")) {
            continue;
        }
        qint64 const ageMs = qMax<qint64>(0, nowMs - it.value());
        if (ageMs <= kFt4PreserveRunningDecodeMaxAgeMs
            && (preserveRunningSerial == 0 || ageMs > preserveRunningAgeMs)) {
            preserveRunningSerial = serial;
            preserveRunningAgeMs = ageMs;
        }
    }

    int removedMeta = 0;
    QList<quint64> const serials = m_decodeStartMsBySerial.keys();
    for (quint64 const serial : serials) {
        if (serial == keepSerial) {
            continue;
        }
        if (serial == preserveRunningSerial) {
            continue;
        }
        if (m_decodeModeBySerial.value(serial).trimmed().toUpper() != QStringLiteral("FT4")) {
            continue;
        }
        m_decodeStartMsBySerial.remove(serial);
        m_decodeModeBySerial.remove(serial);
        m_decodeUtcTokenBySerial.remove(serial);
        m_decodeSessionBySerial.remove(serial);
        ++removedMeta;
    }

    if (m_ft4Worker) {
        QCoreApplication::removePostedEvents(m_ft4Worker, QEvent::MetaCall);
    }

    activateFt4AdaptiveCpuLimit(nowMs, QStringLiteral("backlog drain %1").arg(reason), qint64 {3000});

    if (nowMs - m_lastFt4BacklogDrainLogMs >= 2000) {
        m_lastFt4BacklogDrainLogMs = nowMs;
        decodiumBridgeDecodeBacklogLog(QStringLiteral("FT4 live backlog drain: keepSerial=%1 removed=%2 preserveSerial=%3 preserveAgeMs=%4 %5")
                      .arg(keepSerial)
                      .arg(removedMeta)
                      .arg(preserveRunningSerial)
                      .arg(preserveRunningAgeMs)
                      .arg(reason));
    }
}

void DecodiumBridge::resetEarlyDecodeSchedule()
{
    m_ft8EarlyDecodeSlot = -1;
    m_ft8EarlyDecode41Sent = false;
    m_ft8EarlyDecode47Sent = false;
    m_ft8PendingDeepFollowups.clear();
    m_ft8DeepFollowupCooldownSlots = 0;  // P0 (1.0.403): re-abilita il deep su nuova sessione RX
    m_ft4EarlyDecodeSlot = -1;
    m_ft4EarlyDecodeSent = false;

    if (m_ft8Worker) {
        auto *worker = m_ft8Worker;
        QMetaObject::invokeMethod(worker, [worker]() {
            worker->resetDecoderState();
        }, Qt::QueuedConnection);
    }
}

void DecodiumBridge::resetFtxDecodeWorkersForModeChange(const QString& previousMode,
                                                        const QString& nextMode)
{
    auto isQ65Mode = [] (const QString& mode) {
        QString const normalized = mode.trimmed().toUpper();
        return normalized == QStringLiteral("Q65")
            || normalized.startsWith(QStringLiteral("Q65-"));
    };
    bool const involvesFtx = decodiumBridgeIsFtx77MessageMode(previousMode)
        || decodiumBridgeIsFtx77MessageMode(nextMode);
    bool const involvesQ65 = isQ65Mode(previousMode) || isQ65Mode(nextMode);
    if (!involvesFtx && !involvesQ65) {
        return;
    }

    // Stop hash-log work belonging to the old mode without terminating the
    // reusable worker or discarding hashes already applied to the decoder.
    if (previousMode == QStringLiteral("FT8")) decodium::ft8::setHashSeedWorkEnabled(false);
    if (previousMode == QStringLiteral("FT4")) decodium::ft4::setHashSeedWorkEnabled(false);
    if (nextMode == QStringLiteral("FT8")) decodium::ft8::setHashSeedWorkEnabled(true);
    if (nextMode == QStringLiteral("FT4")) decodium::ft4::setHashSeedWorkEnabled(true);

    decodiumBridgeDecodeBacklogLog(QStringLiteral("mode-change decoder worker reset: %1 -> %2").arg(previousMode, nextMode));

    m_asyncDecodePending = false;
    m_ft8EarlyDecodeSerials.clear();
    m_ft8DeepInTxSerials.clear();
    m_ft8PendingDeepFollowups.clear();
    m_ft8PendingSubpassHarvest.clear();
    m_ft8DeepFollowupCooldownSlots = 0;  // P0 (1.0.403): re-abilita il deep su cambio modo
    m_decodeStartMsBySerial.clear();
    m_decodeModeBySerial.clear();
    m_decodeUtcTokenBySerial.clear();
    m_decodeSessionBySerial.clear();
    if (m_decoding) {
        m_decoding = false;
        emit decodingChanged();
    }

    auto removeQueuedDecodeCalls = [](QObject* worker) {
        if (worker) {
            QCoreApplication::removePostedEvents(worker, QEvent::MetaCall);
        }
    };

    removeQueuedDecodeCalls(m_ft8Worker);
    removeQueuedDecodeCalls(m_ft2Worker);
    removeQueuedDecodeCalls(m_ft4Worker);
    if (involvesQ65) {
        removeQueuedDecodeCalls(m_q65Worker);
    }

    quint64 const staleDecodeSerial = std::numeric_limits<quint64>::max();
    if (m_ft8Worker) {
        m_ft8Worker->markLatestDecodeSerial(staleDecodeSerial);
        m_ft8Worker->cancelCurrentDecode();
        auto* worker = m_ft8Worker;
        QMetaObject::invokeMethod(worker, [worker]() {
            worker->resetDecoderState();
        }, Qt::QueuedConnection);
    }
    if (m_ft2Worker) {
        m_ft2Worker->markLatestDecodeSerial(staleDecodeSerial);
        m_ft2Worker->cancelCurrentDecode();
    }
    if (m_ft4Worker) {
        m_ft4Worker->markLatestDecodeSerial(staleDecodeSerial);
    }
}

void DecodiumBridge::resetRxPeriodAccumulation(bool reserveAudioBuffer)
{
    m_periodTicks = 0;
    m_lastPeriodSlot = -1;
    m_periodProgress = 0;
    emit periodProgressChanged();
    resetEarlyDecodeSchedule();
    m_pendingTimeSyncDecodeAudio.clear();
    m_pendingTimeSyncDecodeSlot = -1;
    m_pendingTimeSyncDecodeMode.clear();
    m_pendingTimeSyncDecodeActive = false;
    m_forcedDecodeAudioSnapshot.clear();
    m_forcedDecodeAudioSnapshotActive = false;

    QMutexLocker locker(&m_audioBufferMutex);
    m_audioBuffer.clear();
    if (reserveAudioBuffer) {
        m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
    }
}

