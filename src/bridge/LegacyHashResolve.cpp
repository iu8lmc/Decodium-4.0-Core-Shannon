#include "LegacyHashResolve.h"
#include "DecodeMessageNormalization.h"
#include "Sequencer/MessageTokenRules.hpp"
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <utility>
using decodium::decode_message::canonicalDecodeMessage;
using decodium::decode_message::stripDecodeApAnnotation;
using decodium::seq::normalizeCallToken;
using decodium::seq::isPlaceholderCallToken;
using decodium::seq::isPlausibleDecodedCallsignToken;
QString decodiumBridgeNormalizeRemoteActivityTime(QString);
int decodiumBridgeSecondsFromUtcDisplayToken(QString);
int decodiumBridgeSignedUtcSecondDelta(int, int);

static QString cleanHashResolveMessage(QString message)
{
    QString aptype;
    message = stripDecodeApAnnotation(message.trimmed(), &aptype).simplified();
    static const QRegularExpression trailingDecodeMark {R"(\s+[?*#^]\s*$)"};
    message.remove(trailingDecodeMark);
    return message.trimmed();
}

bool parseHashResolveAllTxtLine(QString const& rawLine,
                                       HashResolveAllTxtCandidate* candidate)
{
    if (!candidate) {
        return false;
    }

    QString const line = rawLine.trimmed();
    if (line.isEmpty()) {
        return false;
    }

    static const QRegularExpression legacyPattern {
        R"(^\d{6}_(\d{6})\s+\S+\s+(Rx|Tx|Ck)\s+([A-Z0-9_+\-]+)\s+(-?\d+)\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+(.+)$)",
        QRegularExpression::CaseInsensitiveOption
    };
    QRegularExpressionMatch match = legacyPattern.match(line);
    if (match.hasMatch()) {
        if (match.captured(2).compare(QStringLiteral("Rx"), Qt::CaseInsensitive) != 0
            || match.captured(3).trimmed().toUpper() != QStringLiteral("FT8")) {
            return false;
        }

        bool freqOk = false;
        int const audioHz = match.captured(6).trimmed().toInt(&freqOk);
        QString const message = cleanHashResolveMessage(match.captured(7));
        if (!freqOk || message.isEmpty()) {
            return false;
        }
        candidate->timeToken = decodiumBridgeNormalizeRemoteActivityTime(match.captured(1));
        candidate->audioHz = audioHz;
        candidate->message = message;
        return true;
    }

    static const QRegularExpression jtStylePattern {
        R"(^\d{6,8}_(\d{6})\s+(-?\d+)\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+\S+\s+(.+)$)",
        QRegularExpression::CaseInsensitiveOption
    };
    match = jtStylePattern.match(line);
    if (!match.hasMatch()) {
        return false;
    }

    bool freqOk = false;
    int const audioHz = match.captured(4).trimmed().toInt(&freqOk);
    QString const message = cleanHashResolveMessage(match.captured(5));
    if (!freqOk || message.isEmpty()) {
        return false;
    }
    candidate->timeToken = decodiumBridgeNormalizeRemoteActivityTime(match.captured(1));
    candidate->audioHz = audioHz;
    candidate->message = message;
    return true;
}

QStringList hashResolveLogCandidatePaths(QString const& primaryAllTxtPath)
{
    QStringList paths;
    QSet<QString> seen;
    auto addPath = [&](QString const& path) {
        QString const cleaned = path.trimmed();
        if (cleaned.isEmpty()) {
            return;
        }
        QString const absolute = QFileInfo(cleaned).absoluteFilePath();
        if (seen.contains(absolute)) {
            return;
        }
        seen.insert(absolute);
        paths.append(absolute);
    };

    addPath(primaryAllTxtPath);

    QString const appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString const appLocal = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString const generic = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString const homeSupport = QDir(QDir::homePath()).absoluteFilePath(
        QStringLiteral("Library/Application Support"));

    for (QString const& root : {appData, appLocal}) {
        if (root.isEmpty()) {
            continue;
        }
        QDir const dir(root);
        addPath(dir.absoluteFilePath(QStringLiteral("embedded-ft2/ALL.TXT")));
        addPath(dir.absoluteFilePath(QStringLiteral("ALL.TXT")));
        addPath(dir.absoluteFilePath(QStringLiteral("all.txt")));
    }

    QStringList monthFiles;
    QDate const currentMonth = QDate::currentDate();
    for (int i = 0; i < 2; ++i) {
        monthFiles.append(currentMonth.addMonths(-i).toString(QStringLiteral("yyyyMM"))
                          + QStringLiteral("_ALL.TXT"));
    }
    auto addLogDirectory = [&](QString const& path) {
        QDir const dir(path);
        if (!dir.exists()) {
            return;
        }
        static QStringList const filters {
            QStringLiteral("ALL.TXT"),
            QStringLiteral("all.txt"),
            QStringLiteral("*_ALL.TXT")
        };
        for (QString const& name : dir.entryList(filters, QDir::Files, QDir::Name)) {
            addPath(dir.absoluteFilePath(name));
        }
        for (QString const& monthFile : std::as_const(monthFiles)) {
            addPath(dir.absoluteFilePath(monthFile));
        }
    };
    auto addExternalEntry = [&](QString const& path) {
        QString const cleaned = path.trimmed();
        if (cleaned.isEmpty()) {
            return;
        }
        QFileInfo const info(cleaned);
        if (info.isDir()) {
            QDir const dir(info.absoluteFilePath());
            addLogDirectory(dir.absolutePath());
            addLogDirectory(dir.absoluteFilePath(QStringLiteral("JTDX")));
            addLogDirectory(dir.absoluteFilePath(QStringLiteral("WSJT-X")));
            return;
        }
        addPath(info.absoluteFilePath());
    };

    for (QString const& root : {generic, homeSupport}) {
        if (root.isEmpty()) {
            continue;
        }
        QDir const dir(root);
        for (QString const& monthFile : std::as_const(monthFiles)) {
            addPath(dir.absoluteFilePath(QStringLiteral("JTDX/") + monthFile));
        }
        addPath(dir.absoluteFilePath(QStringLiteral("JTDX/ALL.TXT")));
        addPath(dir.absoluteFilePath(QStringLiteral("WSJT-X/ALL.TXT")));
        addPath(dir.absoluteFilePath(QStringLiteral("Decodium/ALL.TXT")));
        addPath(dir.absoluteFilePath(QStringLiteral("ft2/ALL.TXT")));
        addPath(dir.absoluteFilePath(QStringLiteral("IU8LMC/ft2/ALL.TXT")));
    }
    for (char const* name : {"DECODIUM_HASH_SEED_PATHS", "DECODIUM_EXTERNAL_LOG_DIRS"}) {
        QString const value = qEnvironmentVariable(name).trimmed();
        if (value.isEmpty()) {
            continue;
        }
        for (QString const& entry : value.split(QDir::listSeparator(), Qt::SkipEmptyParts)) {
            addExternalEntry(entry);
        }
    }

    return paths;
}

static QStringList hashResolveMessageTokens(QString const& message)
{
    return canonicalDecodeMessage(message).toUpper().simplified().split(
        QLatin1Char(' '), Qt::SkipEmptyParts);
}

static bool hashResolveMessagesCompatible(QString const& hashedMessage,
                                          QString const& resolvedMessage)
{
    if (!hashedMessage.contains(QStringLiteral("<...>"))
        || resolvedMessage.contains(QStringLiteral("<...>"))) {
        return false;
    }

    QStringList const hashedTokens = hashResolveMessageTokens(hashedMessage);
    QStringList const resolvedTokens = hashResolveMessageTokens(resolvedMessage);
    if (hashedTokens.size() != resolvedTokens.size() || hashedTokens.isEmpty()) {
        return false;
    }

    int placeholderCount = 0;
    for (int i = 0; i < hashedTokens.size(); ++i) {
        QString const hashedToken = hashedTokens.at(i).trimmed().toUpper();
        QString const resolvedToken = resolvedTokens.at(i).trimmed().toUpper();
        if (isPlaceholderCallToken(hashedToken)) {
            ++placeholderCount;
            if (!isPlausibleDecodedCallsignToken(resolvedToken)) {
                return false;
            }
            continue;
        }
        if (normalizeCallToken(hashedToken).toUpper()
            != normalizeCallToken(resolvedToken).toUpper()) {
            return false;
        }
    }
    return placeholderCount > 0;
}

QString resolveHashMessageFromCandidates(QString const& timeToken,
                                                int targetAudioHz,
                                                QString const& hashedMessage,
                                                QVector<HashResolveAllTxtCandidate> const& candidates,
                                                QString* sourceOut)
{
    int const targetSeconds = decodiumBridgeSecondsFromUtcDisplayToken(timeToken);
    if (targetSeconds < 0 || targetAudioHz <= 0) {
        return hashedMessage;
    }

    QString resolved;
    QString resolvedSource;
    for (HashResolveAllTxtCandidate const& candidate : candidates) {
        int const candidateSeconds = decodiumBridgeSecondsFromUtcDisplayToken(candidate.timeToken);
        if (candidateSeconds < 0) {
            continue;
        }
        if (qAbs(decodiumBridgeSignedUtcSecondDelta(candidateSeconds, targetSeconds)) > 2) {
            continue;
        }
        if (qAbs(candidate.audioHz - targetAudioHz) > 5) {
            continue;
        }
        if (!hashResolveMessagesCompatible(hashedMessage, candidate.message)) {
            continue;
        }

        QString const candidateMessage = canonicalDecodeMessage(candidate.message);
        if (resolved.isEmpty()) {
            resolved = candidateMessage;
            resolvedSource = candidate.source;
            continue;
        }
        if (resolved.compare(candidateMessage, Qt::CaseInsensitive) != 0) {
            return hashedMessage;
        }
    }

    if (!resolved.isEmpty()) {
        if (sourceOut) {
            *sourceOut = resolvedSource;
        }
        return resolved;
    }
    return hashedMessage;
}

