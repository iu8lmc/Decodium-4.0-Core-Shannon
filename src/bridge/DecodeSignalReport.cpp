#include "DecodeSignalReport.h"

int decodeSnrOrDefault(QString const& text, int fallback)
{
    bool ok = false;
    int const parsed = text.trimmed().toInt(&ok);
    return ok ? qBound(-50, parsed, 49) : fallback;
}

QString formatSignalReport(int snr)
{
    snr = qBound(-50, snr, 49);
    return QStringLiteral("%1%2")
        .arg(snr >= 0 ? QStringLiteral("+") : QStringLiteral("-"))
        .arg(qAbs(snr), 2, 10, QLatin1Char('0'));
}

QString normalizeSignalReportText(QString const& text)
{
    QString token = text.trimmed().toUpper();
    if (token.isEmpty()) {
        return {};
    }
    if (token.startsWith(QLatin1Char('R')) && token.size() > 1) {
        token.remove(0, 1);
    }

    bool ok = false;
    int const snr = token.toInt(&ok);
    return ok ? formatSignalReport(snr) : QStringLiteral("-10");
}

