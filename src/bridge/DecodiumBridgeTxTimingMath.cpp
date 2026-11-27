#include "DecodiumBridgeTxTimingMath.h"

int q65PeriodSecondsFromModeLabel(QString const& mode)
{
    QString const normalized = mode.trimmed().toUpper();
    if (!normalized.startsWith(QStringLiteral("Q65-"))) {
        return 60;
    }

    int end = 4;
    while (end < normalized.size() && normalized.at(end).isDigit()) {
        ++end;
    }
    bool ok = false;
    int const periodSeconds = normalized.mid(4, end - 4).toInt(&ok);
    if (ok && (periodSeconds == 15 || periodSeconds == 30 || periodSeconds == 60
               || periodSeconds == 120 || periodSeconds == 300)) {
        return periodSeconds;
    }
    return 60;
}

int q65SubmodeIndexFromModeLabel(QString const& mode)
{
    QString const normalized = mode.trimmed().toUpper();
    if (!normalized.startsWith(QStringLiteral("Q65-"))) {
        return 0;
    }

    int end = 4;
    while (end < normalized.size() && normalized.at(end).isDigit()) {
        ++end;
    }
    if (end >= normalized.size()) {
        return 0;
    }

    QChar const submode = normalized.at(end);
    if (submode < QLatin1Char('A') || submode > QLatin1Char('E')) {
        return 0;
    }
    return submode.unicode() - QLatin1Char('A').unicode();
}

int q65SamplesPerSymbolAt48k(QString const& mode)
{
    switch (q65PeriodSecondsFromModeLabel(mode)) {
    case 15:  return 4 * 1800;
    case 30:  return 4 * 3600;
    case 120: return 4 * 16000;
    case 300: return 4 * 41472;
    case 60:
    default:  return 4 * 7200;
    }
}

int fst4PeriodSecondsForMode(QString const& mode, double fallbackSeconds)
{
    QString const normalized = mode.trimmed().toUpper();
    int dashPos = normalized.indexOf(QLatin1Char('-'));
    if (dashPos >= 0) {
        bool ok = false;
        int const parsed = normalized.mid(dashPos + 1).toInt(&ok);
        if (ok && (parsed == 15 || parsed == 30 || parsed == 60
                   || parsed == 120 || parsed == 300 || parsed == 900
                   || parsed == 1800)) {
            return parsed;
        }
    }

    int const fallback = qRound(fallbackSeconds);
    if (fallback == 15 || fallback == 30 || fallback == 60
        || fallback == 120 || fallback == 300 || fallback == 900
        || fallback == 1800) {
        return fallback;
    }

    return normalized.startsWith(QStringLiteral("FST4W")) ? 120 : 60;
}

int fst4SamplesPerSymbolAt12k(int trPeriodSeconds)
{
    switch (trPeriodSeconds) {
    case 15:   return 720;
    case 30:   return 1680;
    case 60:   return 3888;
    case 120:  return 8200;
    case 300:  return 21504;
    case 900:  return 66560;
    case 1800: return 134400;
    default:   return 3888;
    }
}

int fst4SamplesPerSymbolAt48k(QString const& mode, double fallbackSeconds)
{
    return 4 * fst4SamplesPerSymbolAt12k(fst4PeriodSecondsForMode(mode, fallbackSeconds));
}

int decodiumBridgeFst4SamplesPerSymbol(QString const& mode, double period) { return fst4SamplesPerSymbolAt48k(mode, period); }
int decodiumBridgeQ65SamplesPerSymbol(QString const& mode) { return q65SamplesPerSymbolAt48k(mode); }
int decodiumBridgeQ65SubmodeIndex(QString const& mode) { return q65SubmodeIndexFromModeLabel(mode); }

