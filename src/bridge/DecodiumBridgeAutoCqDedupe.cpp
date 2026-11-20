#include "DecodiumBridge.h"
#include "Radio.hpp"
#include <QDateTime>

extern bool decodiumBridge_isWithinRecentDuplicateWindow(const QDateTime&, const QDateTime&);
extern bool decodiumBridge_isWithinRecentAutoCqPartnerWindow(const QDateTime&, const QDateTime&);

bool DecodiumBridge::isRecentAutoCqDuplicate(const QString& call, double freqHz, const QString& mode) const
{
    QString const dedupeCall = Radio::base_callsign(call).trimmed().toUpper();
    if (dedupeCall.isEmpty()) {
        return false;
    }

    QString const dedupeBand = autoCqBandKeyForFrequency(freqHz).trimmed().toUpper();
    QString const dedupeMode = (mode.isEmpty() ? m_mode : mode).trimmed().toUpper();
    if (dedupeBand.isEmpty() || dedupeBand == QStringLiteral("--") || dedupeMode.isEmpty()) {
        return false;
    }

    QString const dedupeKey = QStringLiteral("%1|%2|%3").arg(dedupeCall, dedupeBand, dedupeMode);
    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();

    auto const abandonedIt = m_recentAutoCqAbandonedUtcByKey.constFind(dedupeKey);
    if (abandonedIt != m_recentAutoCqAbandonedUtcByKey.constEnd()) {
        return decodiumBridge_isWithinRecentDuplicateWindow(abandonedIt.value(), nowUtc);
    }

    auto const workedIt = m_recentAutoCqWorkedUtcByKey.constFind(dedupeKey);
    if (workedIt != m_recentAutoCqWorkedUtcByKey.constEnd()) {
        return decodiumBridge_isWithinRecentAutoCqPartnerWindow(workedIt.value(), nowUtc);
    }

    auto const loggedIt = m_recentQsoLogUtcByKey.constFind(dedupeKey);
    if (loggedIt == m_recentQsoLogUtcByKey.constEnd()) {
        return false;
    }

    return decodiumBridge_isWithinRecentAutoCqPartnerWindow(loggedIt.value(), nowUtc);
}

bool DecodiumBridge::isRecentAutoCqWorkedOrLoggedDuplicate(const QString& call,
                                                           double freqHz,
                                                           const QString& mode) const
{
    QString const dedupeCall = Radio::base_callsign(call).trimmed().toUpper();
    if (dedupeCall.isEmpty()) {
        return false;
    }

    QString const dedupeBand = autoCqBandKeyForFrequency(freqHz).trimmed().toUpper();
    QString const dedupeMode = (mode.isEmpty() ? m_mode : mode).trimmed().toUpper();
    if (dedupeBand.isEmpty() || dedupeBand == QStringLiteral("--") || dedupeMode.isEmpty()) {
        return false;
    }

    QString const dedupeKey = QStringLiteral("%1|%2|%3").arg(dedupeCall, dedupeBand, dedupeMode);
    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();

    auto const workedIt = m_recentAutoCqWorkedUtcByKey.constFind(dedupeKey);
    if (workedIt != m_recentAutoCqWorkedUtcByKey.constEnd()) {
        return decodiumBridge_isWithinRecentAutoCqPartnerWindow(workedIt.value(), nowUtc);
    }

    auto const loggedIt = m_recentQsoLogUtcByKey.constFind(dedupeKey);
    if (loggedIt == m_recentQsoLogUtcByKey.constEnd()) {
        return false;
    }

    return decodiumBridge_isWithinRecentAutoCqPartnerWindow(loggedIt.value(), nowUtc);
}

void DecodiumBridge::touchRecentAutoCqWorked(const QString& call, double freqHz, const QString& mode)
{
    QString const dedupeCall = Radio::base_callsign(call).trimmed().toUpper();
    QString const dedupeBand = autoCqBandKeyForFrequency(freqHz).trimmed().toUpper();
    QString const dedupeMode = (mode.isEmpty() ? m_mode : mode).trimmed().toUpper();
    if (dedupeCall.isEmpty() || dedupeBand.isEmpty() || dedupeBand == QStringLiteral("--")
        || dedupeMode.isEmpty()) {
        return;
    }
    QString const dedupeKey = QStringLiteral("%1|%2|%3").arg(dedupeCall, dedupeBand, dedupeMode);
    auto const it = m_recentAutoCqWorkedUtcByKey.find(dedupeKey);
    if (it != m_recentAutoCqWorkedUtcByKey.end()) {
        it.value() = QDateTime::currentDateTimeUtc();
    }
}

