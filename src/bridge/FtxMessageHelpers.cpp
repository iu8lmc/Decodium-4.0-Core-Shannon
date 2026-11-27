#include "FtxMessageHelpers.h"
#include "DecodeSignalReport.h"
#include "Sequencer/MessageTokenRules.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include <QStringList>

using decodium::seq::normalizeCallToken;

bool isFtx77MessageMode(QString const& mode)
{
    QString const upper = mode.trimmed().toUpper();
    return upper == QStringLiteral("FT8")
        || upper == QStringLiteral("FT4")
        || upper == QStringLiteral("FT2");
}

static QString normalizedTxCallToken(QString const& call)
{
    return normalizeCallToken(call).trimmed().toUpper();
}

static QString ftxHashedCallToken(QString const& call)
{
    return decodium::txmsg::bracketHashCall(normalizedTxCallToken(call));
}

QString standardFtxQsoMessage(int txNumber,
                                     QString const& dxCall,
                                     QString const& myCall,
                                     QString const& grid4,
                                     QString const& reportSent,
                                     bool sendRR73,
                                     bool quickQso,
                                     QString const& mode)
{
    QString const dx = normalizedTxCallToken(dxCall);
    QString const my = normalizedTxCallToken(myCall);
    QString const grid = grid4.left(4).trimmed().toUpper();
    QString const report = normalizeSignalReportText(reportSent.isEmpty()
        ? QStringLiteral("-10")
        : reportSent);
    QString const endMessage = sendRR73 ? QStringLiteral("RR73") : QStringLiteral("RRR");

    if (dx.isEmpty() || my.isEmpty()) {
        return {};
    }

    auto const normalMessage = [&]() -> QString {
        switch (txNumber) {
        case 1: return dx + QLatin1Char(' ') + my
                    + (grid.isEmpty() ? QString() : QStringLiteral(" ") + grid);
        case 2: return dx + QLatin1Char(' ') + my + QLatin1Char(' ') + report;
        case 3: return dx + QLatin1Char(' ') + my + QStringLiteral(" R") + report
                    + (quickQso ? QStringLiteral(" TU") : QString());
        case 4: return dx + QLatin1Char(' ') + my + QLatin1Char(' ') + endMessage;
        case 5: return dx + QLatin1Char(' ') + my + QStringLiteral(" 73");
        default: return {};
        }
    };

    if (!isFtx77MessageMode(mode)) {
        return normalMessage();
    }

    bool const myStandard = decodium::txmsg::isStandardFtxCall(my);
    bool const dxStandard = decodium::txmsg::isStandardFtxCall(dx);
    if (myStandard && dxStandard) {
        return normalMessage();
    }

    QString const myHash = ftxHashedCallToken(my);
    QString const dxHash = ftxHashedCallToken(dx);
    auto const withGrid = [&](QString const& base) {
        return grid.isEmpty() ? base : base + QLatin1Char(' ') + grid;
    };
    auto const withQuick = [&](QString const& base) {
        return quickQso ? base + QStringLiteral(" TU") : base;
    };

    if (!myStandard && dxStandard) {
        switch (txNumber) {
        case 1:
            // Type-4 compound exchange for a non-standard local callsign. This
            // follows the FT compound sequence: start with the peer hash plus
            // our full call, then keep reports addressed to the peer.
            return dxHash + QLatin1Char(' ') + my;
        case 2:
            // Report from a non-standard local call must still be addressed to
            // the peer. "<MYHASH> DX -15" is valid type-4 text, but peers read
            // it as their own transmission and keep resending their grid.
            return dx + QLatin1Char(' ') + myHash + QLatin1Char(' ') + report;
        case 3: return withQuick(dx + QLatin1Char(' ') + myHash + QStringLiteral(" R") + report);
        case 4: return dxHash + QLatin1Char(' ') + my + QLatin1Char(' ') + endMessage;
        case 5: return dxHash + QLatin1Char(' ') + my + QStringLiteral(" 73");
        default: return {};
        }
    }

    if (myStandard && !dxStandard) {
        switch (txNumber) {
        case 1: return withGrid(dxHash + QLatin1Char(' ') + my);
        case 2: return dxHash + QLatin1Char(' ') + my + QLatin1Char(' ') + report;
        case 3: return withQuick(dxHash + QLatin1Char(' ') + my + QStringLiteral(" R") + report);
        case 4: return dxHash + QLatin1Char(' ') + my + QLatin1Char(' ') + endMessage;
        case 5: return dxHash + QLatin1Char(' ') + my + QStringLiteral(" 73");
        default: return {};
        }
    }

    switch (txNumber) {
    case 1: return withGrid(myHash + QLatin1Char(' ') + dxHash);
    case 2: return myHash + QLatin1Char(' ') + dxHash + QLatin1Char(' ') + report;
    case 3: return withQuick(dxHash + QLatin1Char(' ') + myHash + QStringLiteral(" R") + report);
    case 4: return my + QLatin1Char(' ') + dxHash + QLatin1Char(' ') + endMessage;
    case 5: return my + QLatin1Char(' ') + dxHash + QStringLiteral(" 73");
    default: return {};
    }
}

QString decodiumBridgeStandardFtxQso(int n, QString const& dx, QString const& own,
                                      QString const& grid, QString const& report,
                                      bool rr73, bool quick, QString const& mode)
{
    return standardFtxQsoMessage(n, dx, own, grid, report, rr73, quick, mode);
}

QString standardFtxCqMessage(QString const& myCall, QString const& grid4, QString const& mode)
{
    QString const my = normalizedTxCallToken(myCall);
    QString const grid = grid4.left(4).trimmed().toUpper();
    if (my.isEmpty()) {
        return {};
    }
    if (isFtx77MessageMode(mode) && !decodium::txmsg::isStandardFtxCall(my)) {
        return QStringLiteral("CQ ") + my;
    }
    return QStringLiteral("CQ ") + my + (grid.isEmpty() ? QString() : QStringLiteral(" ") + grid);
}


QString customCqPrefixFromMessage(QString const& message, QString const& myCall)
{
    QStringList const tokens = message.trimmed().toUpper().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QString const call = normalizedTxCallToken(myCall);
    if (tokens.size() < 3 || tokens.first() != QStringLiteral("CQ") || call.isEmpty()) {
        return {};
    }

    int const callIndex = tokens.indexOf(call);
    if (callIndex <= 1) {
        return {};
    }
    return tokens.mid(0, callIndex).join(QLatin1Char(' '));
}
QString decodiumBridgeCustomCqPrefix(const QString& message, const QString& call)
{
    return customCqPrefixFromMessage(message, call);
}
QString decodiumBridgeStandardFtxCq(const QString& call, const QString& grid, const QString& mode)
{
    return standardFtxCqMessage(call, grid, mode);
}


QString ftxCqMessageWithCustomPrefix(QString const& standardMessage, QString const& customPrefix)
{
    QStringList const standardTokens = standardMessage.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList const prefixTokens = customPrefix.trimmed().toUpper().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (standardTokens.size() < 2 || standardTokens.first() != QStringLiteral("CQ")
        || prefixTokens.size() < 2 || prefixTokens.first() != QStringLiteral("CQ")) {
        return standardMessage;
    }
    return prefixTokens.join(QLatin1Char(' '))
        + QLatin1Char(' ')
        + standardTokens.mid(1).join(QLatin1Char(' '));
}

QString decodiumBridgeFtxCqWithPrefix(QString const& message, QString const& prefix)
{
    return ftxCqMessageWithCustomPrefix(message, prefix);
}

