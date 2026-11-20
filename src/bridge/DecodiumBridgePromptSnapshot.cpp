#include "DecodiumBridge.h"
#include <QDateTime>

void DecodiumBridge::capturePromptLogSnapshot(const QVariantMap& preview)
{
    m_promptLogSnapshotValid = true;
    m_promptLogCall = preview.value(QStringLiteral("call")).toString().trimmed();
    m_promptLogGrid = preview.value(QStringLiteral("grid")).toString().trimmed();
    m_promptLogRptSent = preview.value(QStringLiteral("sent")).toString().trimmed();
    m_promptLogRptRcvd = preview.value(QStringLiteral("rcvd")).toString().trimmed();
    m_promptLogMode = preview.value(QStringLiteral("mode"), m_mode).toString().trimmed();
    m_promptLogDialFreq = preview.value(QStringLiteral("freq"), m_frequency).toDouble();
    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    if (m_pendingAutoLogValid && m_pendingAutoLogOn.isValid()) {
        m_promptLogOn = m_pendingAutoLogOn;
        m_promptLogOff = m_pendingAutoLogOff.isValid() ? m_pendingAutoLogOff : nowUtc;
    } else if (m_lateAutoLogValid && m_lateAutoLogOn.isValid()) {
        m_promptLogOn = m_lateAutoLogOn;
        m_promptLogOff = m_lateAutoLogOff.isValid() ? m_lateAutoLogOff : nowUtc;
    } else {
        m_promptLogOn = effectiveQsoLogTimeOnUtc();
        if (!m_promptLogOn.isValid()) {
            m_promptLogOn = nowUtc;
        }
        m_promptLogOff = nowUtc;
    }
    if (m_promptLogOff.isValid() && m_promptLogOn.isValid() && m_promptLogOff < m_promptLogOn) {
        m_promptLogOff = m_promptLogOn;
    }
}

void DecodiumBridge::clearPromptLogSnapshot()
{
    m_promptLogSnapshotValid = false;
    m_promptLogCall.clear();
    m_promptLogGrid.clear();
    m_promptLogRptSent.clear();
    m_promptLogRptRcvd.clear();
    m_promptLogMode.clear();
    m_promptLogComment.clear();
    m_promptLogCommentOverrideValid = false;
    m_promptLogOn = QDateTime {};
    m_promptLogOff = QDateTime {};
    m_promptLogDialFreq = 0.0;
}

