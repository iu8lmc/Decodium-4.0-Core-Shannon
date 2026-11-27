#include "DecodiumBridge.h"
#include <QDateTime>
#include <QRegularExpression>

void DecodiumBridge::setNextLogClusterSpotEnabled(bool enabled)
{
    m_nextLogClusterSpotOverrideValid = true;
    m_nextLogClusterSpotEnabled = enabled;
}

void DecodiumBridge::setNextLogComment(const QString& comment)
{
    m_promptLogComment = comment.trimmed();
    m_promptLogCommentOverrideValid = true;
    setSetting(QStringLiteral("LogComments"), m_promptLogComment);
}

// 1.0.302 — locator editabile nel dialog di conferma log QSO. Sovrascrive il grid
// dello snapshot prompt (m_promptLogGrid), che logQsoNow usa al commit quando
// m_promptLogSnapshotValid. Permette di inserire/correggere il locator a mano
// quando il partner non l'ha trasmesso (feedback tester 2026-05).
void DecodiumBridge::setNextLogGrid(const QString& grid)
{
    m_promptLogGrid = grid.trimmed().toUpper();
}

QString decodiumBridgeLogPromptDateTimeText(const QDateTime& utc)
{
    QDateTime const effective = utc.isValid()
        ? utc.toUTC()
        : QDateTime::currentDateTimeUtc();
    return effective.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

static QDateTime parseLogPromptDateTime(const QString& text, const QDateTime& fallbackUtc)
{
    QDateTime const fallback = fallbackUtc.isValid()
        ? fallbackUtc.toUTC()
        : QDateTime::currentDateTimeUtc();
    QString clean = text.trimmed();
    if (clean.isEmpty()) {
        return fallback;
    }

    clean.replace(QRegularExpression(QStringLiteral("\\s*(UTC|Z)\\s*$"),
                                     QRegularExpression::CaseInsensitiveOption),
                  QString());
    clean.replace(QChar('T'), QChar(' '));
    clean = clean.simplified();

    QStringList const dateTimeFormats {
        QStringLiteral("yyyy-MM-dd HH:mm:ss"),
        QStringLiteral("yyyy/MM/dd HH:mm:ss"),
        QStringLiteral("yyyyMMdd HHmmss"),
        QStringLiteral("yyyyMMddHHmmss")
    };
    for (const QString& format : dateTimeFormats) {
        QDateTime parsed = QDateTime::fromString(clean, format);
        if (parsed.isValid()) {
            parsed.setTimeSpec(Qt::UTC);
            return parsed.toUTC();
        }
    }

    QString const digits = QString(clean).remove(QRegularExpression(QStringLiteral("[^0-9]")));
    QStringList const timeFormats {
        QStringLiteral("HH:mm:ss"),
        QStringLiteral("HHmmss"),
        QStringLiteral("HH:mm"),
        QStringLiteral("HHmm")
    };
    for (const QString& format : timeFormats) {
        QTime parsed = QTime::fromString(format == QStringLiteral("HHmmss") || format == QStringLiteral("HHmm")
                                             ? digits
                                             : clean,
                                         format);
        if (parsed.isValid()) {
            return QDateTime(fallback.date(), parsed, Qt::UTC);
        }
    }

    return fallback;
}

void DecodiumBridge::setNextLogTimes(const QString& timeOnUtc, const QString& timeOffUtc)
{
    QDateTime const fallbackOn = m_promptLogOn.isValid()
        ? m_promptLogOn
        : (m_pendingAutoLogOn.isValid()
               ? m_pendingAutoLogOn
               : QDateTime::currentDateTimeUtc());
    QDateTime const fallbackOff = m_promptLogOff.isValid()
        ? m_promptLogOff
        : (m_pendingAutoLogOff.isValid() ? m_pendingAutoLogOff : fallbackOn);

    m_promptLogOn = parseLogPromptDateTime(timeOnUtc, fallbackOn);
    m_promptLogOff = parseLogPromptDateTime(timeOffUtc, fallbackOff);
    if (m_promptLogOff.isValid() && m_promptLogOn.isValid() && m_promptLogOff < m_promptLogOn) {
        m_promptLogOff = m_promptLogOn;
    }
}

void DecodiumBridge::clearNextLogClusterSpotOverride()
{
    m_nextLogClusterSpotOverrideValid = false;
    m_nextLogClusterSpotEnabled = false;
}

