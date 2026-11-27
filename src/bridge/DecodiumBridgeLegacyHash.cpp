#include "DecodiumBridge.h"
#include "LegacyHashResolve.h"
#include "LegacyDecodeText.h"
#include <QElapsedTimer>
#include <QFileInfo>

int decodiumBridgeSecondsFromUtcDisplayToken(QString);
QString decodiumBridgeNormalizeRemoteActivityTime(QString);
void decodiumBridgeDecodeFlowLog(const QString&);

QString DecodiumBridge::resolveFt8HashPlaceholdersFromRecentAllTxt(const QString& timeToken,
                                                                   const QString& audioFreq,
                                                                   const QString& message,
                                                                   const QString& mode,
                                                                   QString* sourceOut) const
{
    if (sourceOut) {
        sourceOut->clear();
    }

    QString const normalizedMode = mode.trimmed().toUpper();
    if (normalizedMode != QStringLiteral("FT8")
        || !message.contains(QStringLiteral("<...>"))) {
        return message;
    }

    bool freqOk = false;
    int const targetAudioHz = audioFreq.trimmed().toInt(&freqOk);
    if (!freqOk || targetAudioHz <= 0
        || decodiumBridgeSecondsFromUtcDisplayToken(timeToken) < 0) {
        return message;
    }

    static constexpr qint64 kHashResolveTailBytes = 8 * 1024 * 1024;
    static constexpr int kHashResolveMaxLinesPerFile = 50000;

    QElapsedTimer timer;
    timer.start();
    QVector<HashResolveAllTxtCandidate> candidates;
    QStringList const paths = hashResolveLogCandidatePaths(legacyAllTxtPath());
    for (QString const& path : paths) {
        QFileInfo const info(path);
        if (!info.exists() || !info.isFile()) {
            continue;
        }

        QStringList const lines = readTextTailLines(info.absoluteFilePath(),
                                                    kHashResolveTailBytes,
                                                    kHashResolveMaxLinesPerFile);
        for (QString const& line : lines) {
            HashResolveAllTxtCandidate candidate;
            if (!parseHashResolveAllTxtLine(line, &candidate)
                || candidate.message.contains(QStringLiteral("<...>"))) {
                continue;
            }
            candidate.source = info.fileName();
            candidates.append(candidate);
        }
    }

    QString source;
    QString const resolved = resolveHashMessageFromCandidates(decodiumBridgeNormalizeRemoteActivityTime(timeToken),
                                                             targetAudioHz,
                                                             message,
                                                             candidates,
                                                             &source);
    if (resolved.compare(message, Qt::CaseInsensitive) != 0) {
        if (sourceOut) {
            *sourceOut = source;
        }
        decodiumBridgeDecodeFlowLog(QStringLiteral("[FT8-HASH] resolved from ALL.TXT time=%1 hz=%2 '%3' -> '%4' source=%5 candidates=%6 elapsed_ms=%7")
                      .arg(decodiumBridgeNormalizeRemoteActivityTime(timeToken),
                           QString::number(targetAudioHz),
                           message,
                           resolved,
                           source.isEmpty() ? QStringLiteral("<unknown>") : source)
                      .arg(candidates.size())
                      .arg(timer.elapsed()));
        return resolved;
    }

    return message;
}

