#include "LegacyHashResolve.h"
#include "DecodeMessageNormalization.h"
#include "Sequencer/MessageTokenRules.hpp"
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTimeZone>
#include <utility>
using decodium::decode_message::canonicalDecodeMessage;
using decodium::decode_message::stripDecodeApAnnotation;
using decodium::seq::normalizeCallToken;
using decodium::seq::isPlaceholderCallToken;
using decodium::seq::isPlausibleDecodedCallsignToken;
QString decodiumBridgeNormalizeRemoteActivityTime(QString);
int decodiumBridgeSecondsFromUtcDisplayToken(QString);
int decodiumBridgeSignedUtcSecondDelta(int, int);

// Data e ora UTC scritte all'inizio di una riga di ALL.TXT: yyMMdd_hhmmss
// (Decodium, WSJT-X) oppure yyyyMMdd_hhmmss (JTDX). Non valida se mancano.
static QDateTime allTxtLineUtc(QString const& line)
{
    static const QRegularExpression stampPattern {R"(^\s*(\d{6}|\d{8})_(\d{2})(\d{2})(\d{2})\b)"};
    QRegularExpressionMatch const match = stampPattern.match(line);
    if (!match.hasMatch()) {
        return {};
    }
    QString const day = match.captured(1);
    int const year = day.size() == 8 ? day.left(4).toInt() : 2000 + day.left(2).toInt();
    QDate const date(year, day.right(4).left(2).toInt(), day.right(2).toInt());
    QTime const time(match.captured(2).toInt(), match.captured(3).toInt(), match.captured(4).toInt());
    return QDateTime(date, time, QTimeZone::UTC);
}

static QByteArray recentAllTxtFingerprint(QFile& file, qint64 end)
{
    qint64 const length = qMin<qint64>(64, end);
    if (length <= 0 || !file.seek(end - length)) {
        return {};
    }
    return file.read(length);
}

QVector<HashResolveAllTxtCandidate> RecentAllTxtHashIndex::refresh(QStringList const& paths,
                                                                   QDateTime const& nowUtc,
                                                                   Stats* stats)
{
    QMutexLocker locker(&mutex_);
    Stats local;
    QDateTime const oldest = nowUtc.addSecs(-kKeepSeconds);
    QDateTime const newest = nowUtc.addSecs(kFutureToleranceSeconds);
    auto isRecent = [&](QDateTime const& when) {
        return when.isValid() && when >= oldest && when <= newest;
    };

    QSet<QString> wanted;
    for (QString const& path : paths) {
        wanted.insert(path);
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            files_.remove(path);
            continue;
        }
        ++local.files;
        qint64 const size = file.size();
        File& state = files_[path];
        bool const appended = state.valid && size >= state.committed
                              && size - state.committed <= kMaxAppendBytes
                              && recentAllTxtFingerprint(file, state.committed) == state.fingerprint;
        if (!appended) {
            // Si riparte dalla coda: corta se il file non e' stato toccato di
            // recente (le sue righe sarebbero comunque troppo vecchie).
            state = File {};
            state.valid = true;
            ++local.restarts;
            QDateTime const modified = QFileInfo(path).lastModified().toUTC();
            qint64 const tail = isRecent(modified) || modified > newest
                ? kInitialTailBytes : kOldFileTailBytes;
            if (size > tail) {
                qint64 const start = size - tail;
                if (!file.seek(start)) {
                    files_.remove(path);
                    continue;
                }
                state.committed = start + file.readLine().size();
            }
        }
        if (!file.seek(state.committed)) {
            files_.remove(path);
            continue;
        }
        QByteArray const fresh = file.read(qMax<qint64>(0, size - state.committed));
        local.readBytes += fresh.size();
        qsizetype const lastNewline = fresh.lastIndexOf('\n');
        if (lastNewline >= 0) {
            QString const source = QFileInfo(path).fileName();
            QByteArray const complete = fresh.left(lastNewline + 1);
            for (QByteArray const& raw : complete.split('\n')) {
                QString const line = QString::fromUtf8(raw).trimmed();
                if (line.isEmpty()) {
                    continue;
                }
                QDateTime const when = allTxtLineUtc(line);
                if (!isRecent(when)) {
                    continue;
                }
                Entry entry;
                entry.when = when;
                if (!parseHashResolveAllTxtLine(line, &entry.candidate)
                    || entry.candidate.message.contains(QStringLiteral("<...>"))) {
                    continue;
                }
                entry.candidate.source = source;
                state.entries.append(entry);
            }
            state.committed += lastNewline + 1;
        }
        state.fingerprint = recentAllTxtFingerprint(file, state.committed);
        state.entries.removeIf([&](Entry const& entry) { return entry.when < oldest; });
    }

    for (auto it = files_.begin(); it != files_.end();) {
        if (wanted.contains(it.key())) {
            ++it;
        } else {
            it = files_.erase(it);
        }
    }

    QVector<HashResolveAllTxtCandidate> candidates;
    for (QString const& path : paths) {
        auto const it = files_.constFind(path);
        if (it == files_.constEnd()) {
            continue;
        }
        for (Entry const& entry : it->entries) {
            candidates.append(entry.candidate);
        }
    }
    local.candidates = candidates.size();
    if (stats) {
        *stats = local;
    }
    return candidates;
}

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
