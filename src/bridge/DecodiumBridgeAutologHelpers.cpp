#include "Sequencer/MessageTokenRules.hpp"
#include "Radio.hpp"
#include <QDateTime>
#include <QHash>
#include <QStringList>

using decodium::seq::normalizeCallToken;
using decodium::seq::isPlaceholderCallToken;

static QString inferPartnerFromDirectedMessage(QString const& message,
                                               QString const& myCall,
                                               QString const& myBase)
{
    QStringList const words = message.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.size() < 2) {
        return {};
    }

    QString const first = normalizeCallToken(words.at(0));
    QString const second = normalizeCallToken(words.at(1));
    QString const third = words.size() > 2 ? normalizeCallToken(words.at(2)) : QString {};
    if (first.isEmpty() || second.isEmpty()) {
        return {};
    }

    auto const isMyCall = [&](QString const& token) {
        return token.compare(myCall, Qt::CaseInsensitive) == 0
            || token.compare(myBase, Qt::CaseInsensitive) == 0;
    };
    auto const looksLikePartnerCall = [](QString const& token) {
        QString const upper = token.trimmed().toUpper();
        if (isPlaceholderCallToken(upper)) {
            return false;
        }
        if (upper == QStringLiteral("CQ")
            || upper == QStringLiteral("QRZ")
            || upper == QStringLiteral("DE")
            || upper == QStringLiteral("TU")
            || upper == QStringLiteral("TU;")
            || upper == QStringLiteral("73")
            || upper == QStringLiteral("RR73")
            || upper == QStringLiteral("RRR")) {
            return false;
        }
        QString const base = Radio::base_callsign(upper).trimmed().toUpper();
        return !base.isEmpty() && Radio::is_callsign(base);
    };

    if (isMyCall(first) && looksLikePartnerCall(second)) return second;
    if (isMyCall(second) && looksLikePartnerCall(first)) return first;
    if (first.compare(QStringLiteral("DE"), Qt::CaseInsensitive) == 0
        && isMyCall(second)
        && looksLikePartnerCall(third)) {
        return third;
    }

    return {};
}

namespace
{
constexpr int kRecentDuplicateLogWindowSeconds {90};
// A completed QSO keeps its partner out of AutoCQ/MAM for longer than the log window: a station
// that did not hear our RR73 goes on calling for minutes, and at 90 s it was reopened and
// answered again (Z32AW, 27/10/2026: RR73 sent 4 more times; EA4HUK: 4 TX2). The log
// de-duplication window stays at 90 s.
constexpr int kRecentAutoCqPartnerWindowSeconds {600};  // 1.0.732: 10 minutes, as in the pile-up
// 1.0.732: MAM queue and late-report recovery: a station worked or logged within 10 minutes is not reopened

constexpr int kLateAutoLogGraceWindowSeconds {45};
// The short in-memory QSO cooldown only breaks immediate 73/RR73 loops.  It
// must never become a session-long "worked before" list, especially when the
// native MAM sequencer is driving its own dispatch path.
constexpr qint64 kQsoCooldownWindowMs {30 * 1000};

bool isWithinRecentDuplicateWindow(QDateTime const& seenAt, QDateTime const& nowUtc)
{
    qint64 const deltaSec = seenAt.secsTo(nowUtc);
    return deltaSec >= 0 && deltaSec <= kRecentDuplicateLogWindowSeconds;
}

bool isWithinRecentAutoCqPartnerWindow(QDateTime const& seenAt, QDateTime const& nowUtc)
{
    qint64 const deltaSec = seenAt.secsTo(nowUtc);
    return deltaSec >= 0 && deltaSec <= kRecentAutoCqPartnerWindowSeconds;
}

void pruneRecentDuplicateCache(QHash<QString, QDateTime>& cache, QDateTime const& nowUtc)
{
    if (cache.size() <= 512) {
        return;
    }

    for (auto it = cache.begin(); it != cache.end(); ) {
        if (it.value().secsTo(nowUtc) > 3 * kRecentAutoCqPartnerWindowSeconds) {
            it = cache.erase(it);
        } else {
            ++it;
        }
    }
}

/* step A3: spostata in Sequencer/MessageTokenRules.cpp */
}

/* step A3: spostata in Sequencer/MessageTokenRules.cpp */

QString decodiumBridgeInferPartnerForAutolog(const QString& message, const QString& call, const QString& base)
{
    return inferPartnerFromDirectedMessage(message, call, base);
}
int decodiumBridgeLateAutoLogGraceSeconds() { return kLateAutoLogGraceWindowSeconds; }

qint64 decodiumBridgeQsoCooldownWindowMs() { return kQsoCooldownWindowMs; }
void decodiumBridgePruneDuplicateCache(QHash<QString, QDateTime>& cache, const QDateTime& now) { pruneRecentDuplicateCache(cache, now); }
int decodiumBridgeRecentAutoCqPartnerWindowSeconds() { return kRecentAutoCqPartnerWindowSeconds; }
bool decodiumBridge_isWithinRecentDuplicateWindow(const QDateTime& seen, const QDateTime& now) { return isWithinRecentDuplicateWindow(seen, now); }
bool decodiumBridge_isWithinRecentAutoCqPartnerWindow(const QDateTime& seen, const QDateTime& now) { return isWithinRecentAutoCqPartnerWindow(seen, now); }

