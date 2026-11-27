#include "DecodiumBridge.h"
#include "Radio.hpp"
#include <QDateTime>

void decodiumBridgeMamQueueLog(const QString&);

void DecodiumBridge::updateAutoCqPartnerLock()
{
    if (!m_autoCqRepeat || m_qsoProgress <= 1) {
        return;
    }

    QString lockedCall = m_dxCall.trimmed();
    if (lockedCall.isEmpty()) {
        return;
    }

    m_autoCqLockedCall = lockedCall;
    m_autoCqLockedGrid = m_dxGrid.trimmed();
    m_autoCqLockedNtx = (m_currentTx >= 1 && m_currentTx <= 5) ? m_currentTx : 2;
    m_autoCqLockedProgress = m_qsoProgress;
}

void DecodiumBridge::restoreAutoCqPartnerLock()
{
    if (m_manualTxHold || !m_autoCqRepeat || m_autoCqLockedCall.trimmed().isEmpty()) {
        return;
    }

    QString const lockedBase = Radio::base_callsign(m_autoCqLockedCall).trimmed().toUpper();
    if (lockedBase.isEmpty()) {
        return;
    }
    if ((m_currentTx == 6 || m_qsoProgress <= 1) && isRecentAutoCqDuplicate(lockedBase, m_frequency, m_mode)) {
        clearAutoCqPartnerLock();
        return;
    }

    QString const currentBase = Radio::base_callsign(m_dxCall).trimmed().toUpper();
    bool const currentMatches = !currentBase.isEmpty() && currentBase == lockedBase;
    bool const unexpectedCqFallback = (m_currentTx == 6) || (m_qsoProgress <= 1) || m_dxCall.trimmed().isEmpty();
    if (currentMatches && !unexpectedCqFallback) {
        return;
    }

    setDxCall(m_autoCqLockedCall);
    if (!m_autoCqLockedGrid.isEmpty()) {
        setDxGrid(m_autoCqLockedGrid);
    }

    int restoredNtx = m_autoCqLockedNtx;
    if (restoredNtx < 1 || restoredNtx > 5) {
        restoredNtx = 2;
    }
    if (!selectCurrentTxIfAllowed(restoredNtx, QStringLiteral("AutoCQ lock-restore"))) {
        clearAutoCqPartnerLock();
        return;
    }
    m_qsoProgress = qMax(2, m_autoCqLockedProgress);
    emit qsoProgressChanged();
    m_txRetryCount = 0;
    m_txWatchdogTicks = 0;
    regenerateTxMessages();
    setTxEnabled(true);
    decodiumBridgeMamQueueLog(QStringLiteral("AutoCQ lock-restore: %1 TX%2").arg(lockedBase).arg(restoredNtx));
}

bool DecodiumBridge::enqueueCallerInternal(const QString& call,
                                           int freq,
                                           int snr,
                                           bool respectRecentDuplicateGuard)
{
    QString const queuedCall = Radio::base_callsign(call).trimmed().toUpper();
    if (queuedCall.isEmpty()) {
        return false;
    }
    QString const activeBase = (m_multiAnswerMode || m_autoCqRepeat) ? activeMamCallerBase() : QString {};
    if (!activeBase.isEmpty() && queuedCall == activeBase) {
        decodiumBridgeMamQueueLog(QStringLiteral("AutoCQ queue-skip-active: %1").arg(queuedCall));
        return false;
    }
    if (respectRecentDuplicateGuard && isRecentAutoCqDuplicate(queuedCall, m_frequency, m_mode)) {
        decodiumBridgeMamQueueLog(QStringLiteral("AutoCQ queue-skip-recent: %1").arg(queuedCall));
        return false;
    }

    for (QString const& entry : m_callerQueue) {
        QStringList const parts = entry.split(' ', Qt::SkipEmptyParts);
        if (!parts.isEmpty() &&
            Radio::base_callsign(parts.first()).trimmed().compare(queuedCall, Qt::CaseInsensitive) == 0) {
            decodiumBridgeMamQueueLog(QStringLiteral("AutoCQ queue-skip-duplicate: %1").arg(queuedCall));
            return false;
        }
    }

    if (m_callerQueue.size() >= FOX_QUEUE_MAX) {
        decodiumBridgeMamQueueLog(QStringLiteral("AutoCQ queue-skip-full: %1").arg(queuedCall));
        return false;
    }

    QString entry = queuedCall;
    if (freq > 0) {
        entry += QStringLiteral(" %1").arg(freq);
        if (snr >= -99) {
            entry += QStringLiteral(" %1").arg(snr);
        }
    }
    if (m_callerQueue.isEmpty()) {
        m_callerQueueManualOrder = false;
        m_callerQueueArrivalMs.clear();
    }
    m_callerQueue.append(entry);
    if (!m_callerQueueArrivalMs.contains(queuedCall)) {
        m_callerQueueArrivalMs.insert(queuedCall, QDateTime::currentMSecsSinceEpoch());
    }
    emit callerQueueChanged();
    rankCallerQueue(QStringLiteral("enqueue"));
    return true;
}

