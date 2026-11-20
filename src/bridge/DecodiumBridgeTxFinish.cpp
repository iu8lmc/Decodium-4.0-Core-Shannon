#include "DecodiumBridge.h"
#include "Sequencer/MessageTokenRules.hpp"
#include <QTimer>
#include <QBuffer>
#include <QDateTime>

using decodium::seq::messageCarries73Payload;
void decodiumBridgeTxFinishLog(const QString&);

bool DecodiumBridge::noteTxPlaybackFinished(const QString& reason, bool error)
{
    int const finishedTx = m_activeTxNumber;
    QString const finishedMessage = m_activeTxMessage;
    bool const finishedPureAutoCqCq = m_activeTxWasPureAutoCqCq;
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();
    m_activeTxWasPureAutoCqCq = false;

    if (finishedTx <= 0) {
        return false;
    }

    // Count a burst only after the app-owned audio playback completed.  The
    // pure-CQ marker was captured at TX start so a caller decoded meanwhile
    // cannot turn this CQ into a different operation retroactively.
    noteAutoCqBurstCqCompleted(finishedPureAutoCqCq, reason, error);

    if (error) {
        decodiumBridgeTxFinishLog(QStringLiteral("tx-playback-finished: TX%1 error, not counting signoff (%2)")
                      .arg(finishedTx)
                      .arg(reason));
        return false;
    }

    bool const signoffPayload =
        (finishedTx == 5) || messageCarries73Payload(finishedMessage);
    if (!signoffPayload) {
        return false;
    }

    ++m_nTx73;
    decodiumBridgeTxFinishLog(QStringLiteral("tx-playback-finished: completed signoff TX%1 count=%2 reason=%3 msg=[%4]")
                  .arg(finishedTx)
                  .arg(m_nTx73)
                  .arg(reason, finishedMessage));

    QString const normalizedMode = m_mode.trimmed().toUpper();
    if (finishedTx == 5
        && (normalizedMode == QStringLiteral("FT2")
            || normalizedMode == QStringLiteral("FT4")
            || normalizedMode == QStringLiteral("FT8"))) {
        m_logAfterOwn73 = true;
        m_ft2DeferredLogPending = false;
        m_lastNtx = 5;
        m_lastTransmittedMessage = finishedMessage;
        finishAutoSequenceQsoFromCompletedSignoff(
            QStringLiteral("tx-playback-finished: completed final TX5 73 once -> QSO complete"),
            true);
        return true;
    }

    // The embedded legacy FT4/FT8 sequencer can continue to request its TX4
    // RR73 after bridge state has already scheduled the normal completion
    // check.  That path used to bypass the caller-retry limit entirely: the
    // playback counter visibly reached 11 for YC2ULK with MaxCallerRetries=3.
    // Enforce the same definition used by the normal retry path here: one
    // first TX plus at most MaxCallerRetries repeats.  A received report has
    // already completed the exchange, so at the limit we log and release the
    // partner instead of transmitting RR73 indefinitely.
    if (finishedTx == 4
        && (normalizedMode == QStringLiteral("FT2")
            || normalizedMode == QStringLiteral("FT4")
            || normalizedMode == QStringLiteral("FT8"))
        && m_autoCqRepeat
        && m_nTx73 > m_maxCallerRetries) {
        capturePendingAutoLogSnapshot();
        m_ft2DeferredLogPending = false;
        decodiumBridgeTxFinishLog(QStringLiteral("tx-playback-finished: TX4 RR73 retry cap reached %1/%2 -> log and release partner")
                      .arg(m_nTx73)
                      .arg(m_maxCallerRetries + 1));
        finishAutoSequenceQsoFromCompletedSignoff(
            QStringLiteral("tx-playback-finished: TX4 RR73 retry cap reached -> QSO complete"),
            true);
        return true;
    }

    // 1.0.225 — guard reentry (BUG #8 agente-tx). Senza guard, al fine TX
    // FT2 venivano scheduate 3 singleShot in coda verso
    // checkAndStartPeriodicTx (qui + 2 da completeTxPlayback) -> 3
    // chiamate consecutive, ognuna 12+ bridgeLog flush.
    if (!m_periodicTxCheckScheduled) {
        m_periodicTxCheckScheduled = true;
        quint64 const requestGeneration = m_txRequestGeneration;
        QTimer::singleShot(0, this, [this, requestGeneration]() {
            m_periodicTxCheckScheduled = false;
            if (requestGeneration == m_txRequestGeneration
                && !m_transmitting && !m_tuning) {
                checkAndStartPeriodicTx();
            }
        });
    }
    return false;
}


void DecodiumBridge::scheduleSyncTxBoundaryStop(const QString& reason, quint64 txSerial)
{
    if (!shouldAlignTxAudioToCurrentSyncSlot()) {
        return;
    }
    int const periodMs = effectivePeriodMsForMode(m_mode);
    if (periodMs <= 0) {
        return;
    }

    qint64 const nowMs = correctedUtcEpochMs();
    int const elapsedMs = static_cast<int>(nowMs % static_cast<qint64>(periodMs));
    int delayMs = periodMs - elapsedMs + 40;
    if (delayMs <= 40 || delayMs > periodMs + 40) {
        delayMs = periodMs + 40;
    }

    QString const modeSnapshot = m_mode;
    decodiumBridgeTxFinishLog(QStringLiteral("sync TX boundary stop armed: reason=%1 mode=%2 elapsed=%3ms delay=%4ms serial=%5")
                  .arg(reason, modeSnapshot)
                  .arg(elapsedMs)
                  .arg(delayMs)
                  .arg(txSerial));

    QTimer::singleShot(delayMs, this, [this, txSerial, modeSnapshot, reason]() {
        if (!m_transmitting
            || m_tuning
            || m_txPlaybackSerial != txSerial
            || m_mode != modeSnapshot
            || !shouldAlignTxAudioToCurrentSyncSlot()) {
            return;
        }

        decodiumBridgeTxFinishLog(QStringLiteral("sync TX boundary stop: reason=%1 mode=%2 serial=%3")
                      .arg(reason, modeSnapshot)
                      .arg(txSerial));
        m_txPlaybackHoldUntilMs = 0;
        m_txPlaybackHardDeadlineMs = 0;
        if (m_txPcmBuffer && m_txPcmBuffer->isOpen()) {
            m_txPcmBuffer->seek(m_txPcmBuffer->size());
        }
        completeTxPlayback(QStringLiteral("slot-boundary"));
    });
}

bool DecodiumBridge::delayTxFinishUntilPcmConsumed(const QString& context,
                                                   const QString& reason,
                                                   bool useCompletePlayback)
{
    if (!m_transmitting || !m_txPcmBuffer || m_txPcmBuffer->atEnd()) {
        return false;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 const hardDeadline = m_txPlaybackHardDeadlineMs;
    if (hardDeadline <= 0 || nowMs < hardDeadline) {
        qint64 const delayMs = hardDeadline > 0
            ? qBound<qint64>(qint64(50), hardDeadline - nowMs, qint64(250))
            : qint64(250);
        if (!m_txPlaybackReleasePending) {
            m_txPlaybackReleasePending = true;
            decodiumBridgeTxFinishLog(QStringLiteral("%1 delayed: TX PCM not drained reason=%2 pos=%3/%4 delay_ms=%5 ft2Link=%6 bridgeLegacy=%7")
                          .arg(context, reason)
                          .arg(m_txPcmBuffer->pos())
                          .arg(m_txPcmBuffer->size())
                          .arg(delayMs)
                          .arg(m_ft2LinkTxActive ? 1 : 0)
                          .arg(m_bridgeAudioLegacyTxActive ? 1 : 0));
            QTimer::singleShot(delayMs, this, [this, reason, useCompletePlayback]() {
                m_txPlaybackReleasePending = false;
                if (useCompletePlayback) {
                    completeTxPlayback(reason + QStringLiteral("-buffer"));
                } else {
                    finishModulatorIdlePlayback(reason + QStringLiteral("-buffer"));
                }
            });
        }
        return true;
    }

    decodiumBridgeTxFinishLog(QStringLiteral("%1 forcing incomplete TX PCM after hard deadline: reason=%2 pos=%3/%4 ft2Link=%5 bridgeLegacy=%6")
                  .arg(context, reason)
                  .arg(m_txPcmBuffer->pos())
                  .arg(m_txPcmBuffer->size())
                  .arg(m_ft2LinkTxActive ? 1 : 0)
                  .arg(m_bridgeAudioLegacyTxActive ? 1 : 0));
    return false;
}

