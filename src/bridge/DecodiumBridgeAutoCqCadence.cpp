#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "Sequencer/AutoCqCallPolicy.hpp"
#include <QCoreApplication>
#include <QTimer>
#include <limits>

extern void decodiumBridgeAutoCqLog(const QString&);

bool DecodiumBridge::autoCqCanRestartTx6() const
{
    return m_autoCqRepeat
        && !m_tx6.trimmed().isEmpty()
        && !isTxDisabled(6);
}

bool DecodiumBridge::autoCqBurstCadenceEnabled() const
{
    return m_autoCqRepeat
        && m_autoCqBurstCalls > 0
        && m_autoCqListenCycles > 0;
}

bool DecodiumBridge::isAutoCqBurstPureCq(int txNumber, const QString& message) const
{
    bool const partnerActive = !m_dxCall.trimmed().isEmpty()
        || !m_autoCqLockedCall.trimmed().isEmpty()
        || m_pendingAutoSeqTxAfterActiveTx > 0;
    return decodium::isAutoCqCall(m_autoCqRepeat && !m_tuning, txNumber,
                                 m_qsoProgress <= 1, partnerActive, message);
}

void DecodiumBridge::resetAutoCqBurstCadence(const QString& reason)
{
    bool const hadCadenceState = m_autoCqBurstCompletedCalls > 0
        || m_autoCqBurstListenUntilMs > 0
        || m_activeTxWasPureAutoCqCq;
    ++m_autoCqBurstSerial;
    m_autoCqBurstCompletedCalls = 0;
    m_autoCqBurstListenUntilMs = 0;
    m_autoCqGenericListenUntilMs = 0;
    m_autoCqBurstListenMode.clear();
    // A CQ that was in flight belongs to the preceding cadence session.  Do
    // not let its completion start a new pause after a configuration, mode or
    // caller transition reset.
    m_activeTxWasPureAutoCqCq = false;
    if (hadCadenceState) {
        decodiumBridgeAutoCqLog(QStringLiteral("AutoCQ burst: cadence reset (%1)").arg(reason));
    }
}

bool DecodiumBridge::autoCqBurstListening()
{
    if (m_autoCqRepeat && decodium::autoCqPausePending(
            correctedUtcEpochMs(), m_autoCqGenericListenUntilMs, 0))
        return true;
    if (!autoCqBurstCadenceEnabled() || m_autoCqBurstListenUntilMs <= 0) {
        return false;
    }

    if (m_autoCqBurstListenMode != m_mode) {
        resetAutoCqBurstCadence(QStringLiteral("mode changed while listening"));
        return false;
    }

    qint64 const nowMs = correctedUtcEpochMs();
    if (nowMs < m_autoCqBurstListenUntilMs) {
        return true;
    }

    m_autoCqBurstListenUntilMs = 0;
    m_autoCqBurstListenMode.clear();
    decodiumBridgeAutoCqLog(QStringLiteral("AutoCQ burst: listening interval complete; CQ cadence resumes"));
    return false;
}

void DecodiumBridge::applyAutoCqBurstCadenceToLegacyBackend()
{
    if (m_legacyBackend) {
        m_legacyBackend->setAutoCqBurstCadence(m_autoCqBurstCalls,
                                                m_autoCqListenCycles,
                                                m_autoCqMaxCycles, m_autoCqPauseSec);
    }
}

void DecodiumBridge::noteAutoCqBurstCqCompleted(bool completedPureAutoCqCq,
                                                  const QString& reason,
                                                  bool error)
{
    if (error || !completedPureAutoCqCq || !m_autoCqRepeat) return;
    bool const limitReached = decodium::completeAutoCq(true, false,
        m_autoCqMaxCycles, m_autoCqCycleCount);
    decodiumBridgeAutoCqLog(QStringLiteral("AutoCQ: completed call %1 / %2 (%3)")
        .arg(m_autoCqCycleCount).arg(m_autoCqMaxCycles).arg(reason));
    if (limitReached) {
        decodiumBridgeAutoCqLog(QStringLiteral("AutoCQ: completed-call limit reached; stopping"));
        setAutoCqRepeat(false);
        setTxEnabled(false);
        return;
    }
    if (m_autoCqPauseSec > 0) {
        m_autoCqGenericListenUntilMs = correctedUtcEpochMs() + qint64(m_autoCqPauseSec) * 1000;
        decodiumBridgeAutoCqLog(QStringLiteral("AutoCQ: generic listening pause %1s after completed CQ")
                      .arg(m_autoCqPauseSec));
    }
    if (!autoCqBurstCadenceEnabled()) return;

    ++m_autoCqBurstCompletedCalls;
    if (m_autoCqBurstCompletedCalls < m_autoCqBurstCalls) {
        decodiumBridgeAutoCqLog(QStringLiteral("AutoCQ burst: completed CQ %1/%2 (%3)")
                      .arg(m_autoCqBurstCompletedCalls)
                      .arg(m_autoCqBurstCalls)
                      .arg(reason));
        return;
    }

    m_autoCqBurstCompletedCalls = 0;
    QString const modeSnapshot = m_mode;
    qint64 const periodMs = qMax<qint64>(1, effectivePeriodMsForMode(modeSnapshot));
    qint64 const pauseMs = static_cast<qint64>(m_autoCqListenCycles) * periodMs;
    if (pauseMs <= 0) {
        return;
    }

    m_autoCqBurstListenMode = modeSnapshot;
    m_autoCqBurstListenUntilMs = correctedUtcEpochMs() + pauseMs;
    quint64 const serial = ++m_autoCqBurstSerial;
    decodiumBridgeAutoCqLog(QStringLiteral("AutoCQ burst: completed %1 CQs; listening for %2 cycle(s) / %3 ms (%4)")
                  .arg(m_autoCqBurstCalls)
                  .arg(m_autoCqListenCycles)
                  .arg(pauseMs)
                  .arg(reason));

    // The period timer normally reaches the dispatch path itself.  This wake
    // up covers an idle scheduler without bypassing the normal parity and
    // late-slot checks.
    int const wakeDelayMs = static_cast<int>(qMin<qint64>(
        static_cast<qint64>(std::numeric_limits<int>::max()), pauseMs + 25));
    quint64 const requestGeneration = m_txRequestGeneration;
    QTimer::singleShot(wakeDelayMs, this, [this, serial, modeSnapshot, requestGeneration]() {
        if (serial != m_autoCqBurstSerial
            || requestGeneration != m_txRequestGeneration
            || !m_autoCqRepeat
            || m_mode != modeSnapshot
            || m_shuttingDown
            || QCoreApplication::closingDown()) {
            return;
        }
        if (autoCqBurstListening() || m_manualTxHold || m_transmitting || m_tuning) {
            return;
        }
        checkAndStartPeriodicTx();
    });
}

