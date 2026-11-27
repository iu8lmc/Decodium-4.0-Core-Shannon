#include "DecodiumBridge.h"
#include "DecodiumBridgeDecodeUiFlags.h"
#include <QDateTime>
#include <QThread>
#include <QTimer>

void DecodiumBridge::rebuildBandActivityModel()
{
    scheduleDecodeUiRefresh(kDecodeUiRefreshBand
                            | kDecodeUiRefreshFullSpectrum);
}

void DecodiumBridge::rebuildRxDecodeModel()
{
    scheduleDecodeUiRefresh(kDecodeUiRefreshRx);
}

bool DecodiumBridge::decodeUiBurstActive() const
{
    return QDateTime::currentMSecsSinceEpoch() < m_decodeUiBurstUntilMs;
}

int DecodiumBridge::decodeUiRowsPerCycle() const
{
    int const cores = qMax(1, QThread::idealThreadCount());
    int budget = 48;
    if (cores <= 4) {
        budget = 16;
    } else if (cores <= 8) {
        budget = 24;
    } else if (cores <= 12) {
        budget = 32;
    }
    if (uiQuality().compare(QStringLiteral("Low"), Qt::CaseInsensitive) == 0) {
        budget = qMin(budget, 20);
    }
    if (isUiStallActive(90, 2500)) {
        budget = qMin(budget, 16);
    }
    return budget;
}

void DecodiumBridge::invalidateDecodeUiPredicateCaches()
{
    m_decodeUiGhostCache.clear();
    m_decodeUiRxMembershipCache.clear();
}

void DecodiumBridge::scheduleDecodeUiRefresh(int flags, int delayMs)
{
    if (flags == 0) return;
    m_decodeUiPendingFlags |= flags;
    ++m_decodeUiSnapshotGeneration;

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    // Keep map/full-spectrum work outside the short decode delivery window.
    m_decodeUiBurstUntilMs = qMax(m_decodeUiBurstUntilMs, nowMs + 650);

    if (!m_decodeUiRefreshTimer) {
        m_decodeUiRefreshTimer = new QTimer(this);
        m_decodeUiRefreshTimer->setSingleShot(true);
        connect(m_decodeUiRefreshTimer, &QTimer::timeout,
                this, &DecodiumBridge::startDecodeUiSnapshotRefresh);
    }
    if (!m_decodeUiSnapshotInFlight) {
        // Restarting this short quiet-window timer coalesces Band Activity and
        // Signal RX notifications into one worker snapshot.
        m_decodeUiRefreshTimer->start(qMax(0, delayMs));
    }
}

