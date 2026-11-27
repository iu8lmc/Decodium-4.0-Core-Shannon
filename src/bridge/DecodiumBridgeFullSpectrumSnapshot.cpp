#include "DecodiumBridge.h"
#include <QDateTime>
#include <QTimer>
#include <utility>

void DecodiumBridge::scheduleFullSpectrumModelRefresh()
{
    if (!m_fullSpectrumSnapshotPending) return;
    if (!m_fullSpectrumRefreshTimer) {
        m_fullSpectrumRefreshTimer = new QTimer(this);
        m_fullSpectrumRefreshTimer->setSingleShot(true);
        connect(m_fullSpectrumRefreshTimer, &QTimer::timeout,
                this, &DecodiumBridge::applyPendingFullSpectrumSnapshot);
    }
    qint64 const remainingMs =
        qMax<qint64>(0, m_decodeUiBurstUntilMs
                           - QDateTime::currentMSecsSinceEpoch());
    m_fullSpectrumRefreshTimer->start(
        static_cast<int>(qMax<qint64>(80, remainingMs + 80)));
}

void DecodiumBridge::applyPendingFullSpectrumSnapshot()
{
    if (!m_fullSpectrumSnapshotPending || !m_fullSpectrumModel) return;
    if (decodeUiBurstActive()) {
        scheduleFullSpectrumModelRefresh();
        return;
    }

    DecodeListModel::PreparedSnapshot target;
    target.entries = std::move(m_pendingFullSpectrumModelEntries);
    target.keys = std::move(m_pendingFullSpectrumModelKeys);
    m_pendingFullSpectrumModelEntries.clear();
    m_pendingFullSpectrumModelKeys.clear();
    m_fullSpectrumSnapshotPending = false;
    m_fullSpectrumModel->setEntriesBudgeted(
        std::move(target), qMax(12, decodeUiRowsPerCycle() / 2));
}

