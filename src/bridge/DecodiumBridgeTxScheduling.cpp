#include "DecodiumBridge.h"
#include <QTimer>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::suspendNonAudioTxWork(const QString& reason)
{
    // The spectrum timer is also the producer for the panadapter and
    // waterfall. Stopping it during legacy TX leaves the UI presenting a
    // frozen frame for the whole PTT/audio tail (and longer after a delayed
    // PTT confirmation). Keep visual acquisition alive; TX-specific audio
    // priority is handled separately by applyTxAudioSchedulingBoost().
    Q_UNUSED(reason);
    m_spectrumTimerPausedForTx = false;
}

void DecodiumBridge::resumeNonAudioTxWork(const QString& reason)
{
    if (!m_spectrumTimerPausedForTx) {
        return;
    }
    m_spectrumTimerPausedForTx = false;
    if (m_spectrumTimer && m_monitoring && !m_transmitting && !m_tuning) {
        m_spectrumTimer->start();
        decodiumBridgeAudioLevelLog(QStringLiteral("TX workload: spectrum timer resumed (%1)").arg(reason));
    }
}

void DecodiumBridge::applyTxAudioSchedulingBoost(const QString& reason)
{
#if defined(Q_OS_WIN)
    if (m_txAudioPriorityBoosted) {
        return;
    }

    HANDLE const process = GetCurrentProcess();
    DWORD const previousClass = GetPriorityClass(process);
    if (previousClass == 0) {
        decodiumBridgeAudioLevelLog(QStringLiteral("TX audio priority boost unavailable (%1): GetPriorityClass error=%2")
                      .arg(reason)
                      .arg(static_cast<qulonglong>(GetLastError())));
        return;
    }

    DWORD const targetClass =
        (m_lowCpuModeEnabled || cpuPressureActive())
            ? HIGH_PRIORITY_CLASS
            : ABOVE_NORMAL_PRIORITY_CLASS;

    if (previousClass == REALTIME_PRIORITY_CLASS ||
        previousClass == HIGH_PRIORITY_CLASS ||
        previousClass == targetClass) {
        return;
    }

    if (SetPriorityClass(process, targetClass)) {
        m_txAudioPreviousPriorityClass = static_cast<quint32>(previousClass);
        m_txAudioPriorityBoosted = true;
        decodiumBridgeAudioLevelLog(QStringLiteral("TX audio priority boost: %1 -> %2 (%3)")
                      .arg(static_cast<qulonglong>(previousClass))
                      .arg(static_cast<qulonglong>(targetClass))
                      .arg(reason));
    } else {
        decodiumBridgeAudioLevelLog(QStringLiteral("TX audio priority boost failed (%1): error=%2")
                      .arg(reason)
                      .arg(static_cast<qulonglong>(GetLastError())));
    }
#else
    Q_UNUSED(reason)
#endif
}

void DecodiumBridge::restoreTxAudioSchedulingBoost(const QString& reason)
{
#if defined(Q_OS_WIN)
    if (!m_txAudioPriorityBoosted || m_txAudioPreviousPriorityClass == 0) {
        return;
    }

    DWORD const previousClass = static_cast<DWORD>(m_txAudioPreviousPriorityClass);
    if (SetPriorityClass(GetCurrentProcess(), previousClass)) {
        decodiumBridgeAudioLevelLog(QStringLiteral("TX audio priority restored: %1 (%2)")
                      .arg(static_cast<qulonglong>(previousClass))
                      .arg(reason));
        m_txAudioPriorityBoosted = false;
        m_txAudioPreviousPriorityClass = 0;
    } else {
        decodiumBridgeAudioLevelLog(QStringLiteral("TX audio priority restore failed (%1): error=%2")
                      .arg(reason)
                      .arg(static_cast<qulonglong>(GetLastError())));
    }
#else
    Q_UNUSED(reason)
#endif
}

