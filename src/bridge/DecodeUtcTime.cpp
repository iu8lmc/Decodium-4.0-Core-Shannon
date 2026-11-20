#include <QDateTime>
#include <QString>
#include <utility>

static QString normalizeUtcDisplayToken(QString raw)
{
    QString const original = raw.trimmed();
    auto validSixDigitTime = [] (QString const& token) {
        if (token.size() != 6) {
            return false;
        }
        bool hhOk = false;
        bool mmOk = false;
        bool ssOk = false;
        int const hh = token.left(2).toInt(&hhOk);
        int const mm = token.mid(2, 2).toInt(&mmOk);
        int const ss = token.mid(4, 2).toInt(&ssOk);
        return hhOk && mmOk && ssOk
            && hh >= 0 && hh <= 23
            && mm >= 0 && mm <= 59
            && ss >= 0 && ss <= 59;
    };

    for (int i = 0; i + 7 < original.size(); ++i) {
        if (original.at(i).isDigit()
            && original.at(i + 1).isDigit()
            && original.at(i + 2) == QLatin1Char(':')
            && original.at(i + 3).isDigit()
            && original.at(i + 4).isDigit()
            && original.at(i + 5) == QLatin1Char(':')
            && original.at(i + 6).isDigit()
            && original.at(i + 7).isDigit()) {
            QString const token = original.mid(i, 2) + original.mid(i + 3, 2) + original.mid(i + 6, 2);
            if (validSixDigitTime(token)) {
                return token;
            }
        }
    }

    QString digits;
    digits.reserve(raw.size());
    for (QChar const ch : std::as_const(raw)) {
        if (ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) {
            digits.append(ch);
        }
    }
    raw = digits;
    QTime const nowUtc = QDateTime::currentDateTimeUtc().time();
    if (raw.size() >= 6) {
        if (raw.size() >= 12) {
            QString const tail = raw.right(6);
            if (validSixDigitTime(tail)) {
                return tail;
            }
        }
        return raw.left(6);
    }
    if (raw.size() == 4) {
        return raw;
    }
    if (raw.isEmpty()) {
        return nowUtc.toString(QStringLiteral("HHmmss"));
    }
    return raw;
}

QString decodiumBridgeNormalizeRemoteActivityTime(QString raw)
{
    return normalizeUtcDisplayToken(std::move(raw));
}

static int secondsFromUtcDisplayToken(QString raw)
{
    raw = normalizeUtcDisplayToken(raw);
    if (raw.size() >= 4) {
        bool hhOk = false;
        bool mmOk = false;
        int const hh = raw.left(2).toInt(&hhOk);
        int const mm = raw.mid(2, 2).toInt(&mmOk);
        if (!hhOk || !mmOk || hh < 0 || hh > 23 || mm < 0 || mm > 59) {
            return -1;
        }
        int ss = 0;
        if (raw.size() >= 6) {
            bool ssOk = false;
            ss = raw.mid(4, 2).toInt(&ssOk);
            if (!ssOk || ss < 0 || ss > 59) {
                return -1;
            }
        }
        return hh * 3600 + mm * 60 + ss;
    }
    return -1;
}

int decodiumBridgeSecondsFromUtcDisplayToken(QString raw)
{
    return secondsFromUtcDisplayToken(std::move(raw));
}

static int signedUtcSecondDelta(int earlierSeconds, int laterSeconds)
{
    if (earlierSeconds < 0 || laterSeconds < 0) {
        return 0;
    }
    int delta = laterSeconds - earlierSeconds;
    if (delta < -43200) {
        delta += 86400;
    } else if (delta > 43200) {
        delta -= 86400;
    }
    return delta;
}

int decodiumBridgeSignedUtcSecondDelta(int earlier, int later)
{
    return signedUtcSecondDelta(earlier, later);
}


static QDateTime approxUtcDateTimeForDisplayToken(QString const& utcToken)
{
    QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    QString const normalized = normalizeUtcDisplayToken(utcToken);

    QTime decodedTime;
    if (normalized.size() >= 6) {
        decodedTime = QTime::fromString(normalized.left(6), QStringLiteral("hhmmss"));
    } else if (normalized.size() == 4) {
        decodedTime = QTime::fromString(normalized, QStringLiteral("hhmm"));
    }

    if (!decodedTime.isValid()) {
        return nowUtc;
    }

    QDate decodedDate = nowUtc.date();
    if (decodedTime > nowUtc.time().addSecs(60)) {
        decodedDate = decodedDate.addDays(-1);
    }

    nowUtc.setDate(decodedDate);
    nowUtc.setTime(decodedTime);
    return nowUtc;
}

QDateTime decodiumBridgeApproxUtcDateTimeForDisplayToken(QString const& token)
{
    return approxUtcDateTimeForDisplayToken(token);
}

static qint64 decodeEntryTimestampMsecs(QString const& utcToken, qint64 fallbackMs)
{
    if (utcToken.trimmed().isEmpty()) {
        return fallbackMs;
    }

    QDateTime const approxUtc = approxUtcDateTimeForDisplayToken(utcToken);
    if (!approxUtc.isValid()) {
        return fallbackMs;
    }
    return approxUtc.toMSecsSinceEpoch();
}

qint64 decodiumBridgeDecodeEntryTimestampMsecs(QString const& token, qint64 fallback)
{
    return decodeEntryTimestampMsecs(token, fallback);
}


int utcTokenForSlotStart(qint64 slotIndex, int periodMs)
{
    if (slotIndex < 0 || periodMs <= 0) {
        return 0;
    }

    qint64 const slotStartMs = slotIndex * static_cast<qint64>(periodMs);
    QTime const slotTime = QDateTime::fromMSecsSinceEpoch(slotStartMs, Qt::UTC).time();
    if (periodMs < 60000) {
        return slotTime.hour() * 10000 + slotTime.minute() * 100 + slotTime.second();
    }
    return slotTime.hour() * 100 + slotTime.minute();
}

QString utcDisplayTokenForSlotStart(qint64 slotIndex, int periodMs)
{
    if (slotIndex < 0 || periodMs <= 0) {
        return QString();
    }

    qint64 const slotStartMs = slotIndex * static_cast<qint64>(periodMs);
    QTime const slotTime = QDateTime::fromMSecsSinceEpoch(slotStartMs, Qt::UTC).time();
    QString const fmt = periodMs < 60000 ? QStringLiteral("HHmmss")
                                         : QStringLiteral("HHmm");
    return slotTime.toString(fmt);
}

