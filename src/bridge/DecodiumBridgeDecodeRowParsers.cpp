#include "DecodiumBridge.h"
#include "DecodeMessageNormalization.h"
#include <QRegularExpression>

extern QString decodiumBridgeNormalizeRemoteActivityTime(QString);
using decodium::decode_message::stripDecodeApAnnotation;

QStringList DecodiumBridge::parseFt8Row(const QString& row) const
{
    // Actual format produced by FT2DecodeWorker::build_row() / build_rows():
    //
    //   [utcPrefix]SSSS D.D  FFFF ~ <decoded 37 chars> AA
    //
    //   utcPrefix  = right-justified 4 (HHMM) or 6 (HHMMSS) digit string
    //                prepended directly with NO separator — format_decode_utc()
    //                returns "" for async rows (nutc==0) or "HHMM"/"HHMMSS"
    //   SSSS       = SNR right-justified in 4 chars  e.g. " -12" or "  -7"
    //   D.D        = dt  right-justified in 5 chars  e.g. "  1.3"
    //   FFFF       = audio freq Hz right-justified in 5 chars e.g. " 1200"
    //   ~          = mode marker (single char, always ~ for FT2)
    //   decoded    = 37-char message field (space padded, last char may be '?')
    //   AA         = 2-char annotation: "  " (no AP) or "a1".."a9" etc.
    //
    // Because utcPrefix is prepended with no separator, splitting on whitespace
    // is unreliable when the prefix and the SNR leading spaces merge into one
    // token.  A single regex parse on the raw string is used instead.
    // The 6-digit UTC case (nutc > 9999, i.e. HHMMSS) was also missed
    // by the previous 4-char-only check.
    //
    // Returns: [time, snr, dt, df, message, aptype, quality, freq]

    if (row.trimmed().isEmpty()) return {};

    // Group 1: optional 4- or 6-digit UTC prefix
    // Group 2: SNR (signed integer, may share leading spaces with UTC token)
    // Group 3: dt  (signed float)
    // Group 4: freq (unsigned integer)
    // Group 5: marker (single non-space char, expected '~')
    // Group 6: message body (37-char field, trimmed)
    // Group 7: optional trailing annotation e.g. "a1", "a2"
    static const QRegularExpression re(
        R"(^(\d{4}|\d{6})?\s*(-?\d+)\s+(-?\d+\.\d+)\s+(\d+)\s+(\S)\s{2}(.*?)\s*(a\d+)?\s*$)");

    QRegularExpressionMatch match = re.match(row);
    if (!match.hasMatch()) return {};

    QString timeStr = decodiumBridgeNormalizeRemoteActivityTime(match.captured(1)); // "" or "HHMM" or "HHMMSS"
    QString snrStr  = match.captured(2).trimmed();
    QString dtStr   = match.captured(3).trimmed();
    QString freqStr = match.captured(4).trimmed();
    // captured(5) is the marker character ('~') — not stored separately
    QString message = match.captured(6).trimmed();
    QString aptype;
    if (!match.captured(7).isEmpty()) {
        // annotation is "a<n>"; store only the numeric suffix as aptype
        aptype = match.captured(7).mid(1);
    }
    message = stripDecodeApAnnotation(message, &aptype);

    // df = absolute audio frequency offset relative to lower band edge (nfa)
    bool freqOk = false;
    int freqHz = freqStr.toInt(&freqOk);
    QString dfStr = freqOk ? QString::number(freqHz - m_nfa) : freqStr;

    return {timeStr, snrStr, dtStr, dfStr, message, aptype, "100", freqStr};
}

QStringList DecodiumBridge::parseWsprRow(const QString& row) const
{
    QString const trimmed = row.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("<DecodeFinished>")) {
        return {};
    }

    // wsprd stdout format:
    //   2256 -21 -0.3 14.097090 0 DU1MGA PK04 37
    // Returns same structure as parseFt8Row:
    //   [time, snr, dt, df, message, aptype, quality, freq]
    static const QRegularExpression re(
        R"(^(\d{4})\s+([-+]?\d+(?:\.\d+)?)\s+([-+]?\d+\.\d+)\s+(\d+\.\d+)\s+([-+]?\d+)\s+(.+?)\s*$)");
    QRegularExpressionMatch const match = re.match(trimmed);
    if (!match.hasMatch()) {
        return {};
    }

    QString const message = match.captured(6).trimmed().toUpper();
    QString const snrStr = QString::number(qRound(match.captured(2).toDouble()));
    QString const dtStr = match.captured(3).trimmed();
    QString const driftStr = match.captured(5).trimmed();

    QStringList const messageParts = message.split(QRegularExpression(QStringLiteral("\\s+")),
                                                   Qt::SkipEmptyParts);
    if (messageParts.size() < 2) {
        return {};
    }
    QString const callToken = messageParts.at(0);
    bool const hasBrackets = callToken.startsWith(QLatin1Char('<')) && callToken.endsWith(QLatin1Char('>'));
    QString const normalizedCall = hasBrackets
        ? callToken.mid(1, callToken.size() - 2)
        : callToken;
    auto const looksLikeWsprCallsign = [](QString const& token) {
        QString const t = token.trimmed().toUpper();
        if (t.size() < 3 || t.size() > 12) {
            return false;
        }
        bool hasLetter = false;
        bool hasDigit = false;
        for (QChar const& ch : t) {
            bool const ok = ch.isLetterOrNumber() || ch == QLatin1Char('/');
            if (!ok) {
                return false;
            }
            hasLetter = hasLetter || ch.isLetter();
            hasDigit = hasDigit || ch.isDigit();
        }
        return hasLetter && hasDigit;
    };
    if (!looksLikeWsprCallsign(normalizedCall)) {
        return {};
    }

    bool dbmOk = false;
    int const remoteDbm = messageParts.constLast().toInt(&dbmOk);
    if (!dbmOk || remoteDbm < 0 || remoteDbm > 60) {
        return {};
    }

    bool rfOk = false;
    double const rfMhz = match.captured(4).toDouble(&rfOk);
    int audioHz = 1500;
    if (rfOk && m_frequency > 0.0) {
        audioHz = qRound((rfMhz * 1.0e6) - m_frequency);
    }
    if (audioHz < 0 || audioHz > 5000) {
        audioHz = m_rxFrequency > 0 ? m_rxFrequency : 1500;
    }
    QString const freqStr = QString::number(audioHz);
    QString const dfStr = QString::number(audioHz - m_nfa);

    return {decodiumBridgeNormalizeRemoteActivityTime(match.captured(1)),
            snrStr,
            dtStr,
            dfStr,
            message,
            driftStr,
            QStringLiteral("100"),
            freqStr};
}

QStringList DecodiumBridge::parseJt65Row(const QString& row) const
{
    // Format from format_async_decode_line():
    //   "%04d%4d%5.1f%5d %2.2s %22.22s %3.3s"
    //   cols: 0-3(UTC) 4-7(SNR) 8-12(DT) 13-17(freq) 18(sp) 19-20(csync) 21(sp) 22-43(decoded) 44(sp) 45-47(flags)
    if (row.size() < 22) return {};

    QString timeStr = row.mid(0, 4).trimmed();
    QString snrStr  = row.mid(4, 4).trimmed();
    QString dtStr   = row.mid(8, 5).trimmed();
    QString freqStr = row.mid(13, 5).trimmed();
    QString message = (row.size() >= 44) ? row.mid(22, 22).trimmed() : row.mid(22).trimmed();

    if (snrStr.isEmpty() || freqStr.isEmpty()) return {};

    bool freqOk = false;
    int freqHz = freqStr.toInt(&freqOk);
    QString dfStr = freqOk ? QString::number(freqHz - m_nfa) : freqStr;

    // Returns same structure as parseFt8Row: [time, snr, dt, df, message, aptype, quality, freq]
    return {timeStr, snrStr, dtStr, dfStr, message, "", "100", freqStr};
}

