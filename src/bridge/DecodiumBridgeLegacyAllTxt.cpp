#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodeMessageNormalization.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>
#include <limits>
#include <utility>

using decodium::decode_message::canonicalDecodeMessage;
QDateTime decodiumBridgeApproxUtcDateTimeForDisplayToken(QString const&);

QString DecodiumBridge::legacyAllTxtPath() const
{
    if (m_legacyBackend) {
        QString const embeddedPath = m_legacyBackend->allTxtPath().trimmed();
        if (!embeddedPath.isEmpty()) {
            return embeddedPath;
        }
    }

    QString const genericRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString const appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString const appLocal = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QStringList candidates;
    int localCandidateCount = 0;

    if (!appData.isEmpty()) {
        candidates << QDir(appData).absoluteFilePath(QStringLiteral("embedded-ft2/ALL.TXT"));
        candidates << QDir(appData).absoluteFilePath(QStringLiteral("ALL.TXT"));
        candidates << QDir(appData).absoluteFilePath(QStringLiteral("all.txt"));
        localCandidateCount = candidates.size();
    }
    if (!appLocal.isEmpty() && appLocal != appData) {
        candidates << QDir(appLocal).absoluteFilePath(QStringLiteral("embedded-ft2/ALL.TXT"));
        candidates << QDir(appLocal).absoluteFilePath(QStringLiteral("ALL.TXT"));
        candidates << QDir(appLocal).absoluteFilePath(QStringLiteral("all.txt"));
        localCandidateCount = candidates.size();
    }
    if (!genericRoot.isEmpty()) {
        candidates << QDir(genericRoot).absoluteFilePath(QStringLiteral("ft2/ALL.TXT"));
        candidates << QDir(genericRoot).absoluteFilePath(QStringLiteral("IU8LMC/ft2/ALL.TXT"));
        candidates << QDir(genericRoot).absoluteFilePath(QStringLiteral("Decodium/ALL.TXT"));
    }

    candidates.removeDuplicates();

    qint64 newestActiveMsecs = std::numeric_limits<qint64>::min();
    QList<QFileInfo> existingLocalCandidates;
    QList<QFileInfo> existingFallbackCandidates;
    for (int i = 0; i < candidates.size(); ++i) {
        QString const& candidate = candidates.at(i);
        QFileInfo const info(candidate);
        if (!info.exists() || !info.isFile()) {
            continue;
        }
        if (i < localCandidateCount) {
            existingLocalCandidates.append(info);
        } else {
            existingFallbackCandidates.append(info);
        }
        newestActiveMsecs = qMax(newestActiveMsecs,
                                 info.lastModified().toMSecsSinceEpoch());
    }

    QList<QFileInfo> const existingCandidates =
        !existingLocalCandidates.isEmpty() ? existingLocalCandidates : existingFallbackCandidates;

    if (!existingCandidates.isEmpty()) {
        QFileInfo bestInfo;
        for (QFileInfo const& info : std::as_const(existingCandidates)) {
            qint64 const modifiedMsecs = info.lastModified().toMSecsSinceEpoch();
            // Se piu file stanno venendo aggiornati praticamente insieme,
            // scegli quello piu ricco invece di quello che ha solo qualche
            // millisecondo di vantaggio ma meno decode.
            if (modifiedMsecs + 15000 < newestActiveMsecs) {
                continue;
            }
            if (!bestInfo.exists()
                || info.size() > bestInfo.size()
                || (info.size() == bestInfo.size()
                    && modifiedMsecs > bestInfo.lastModified().toMSecsSinceEpoch())) {
                bestInfo = info;
            }
        }
        if (bestInfo.exists()) {
            return bestInfo.absoluteFilePath();
        }
    }

    if (!appData.isEmpty()) {
        return QDir(appData).absoluteFilePath(QStringLiteral("ALL.TXT"));
    }
    if (!appLocal.isEmpty()) {
        return QDir(appLocal).absoluteFilePath(QStringLiteral("ALL.TXT"));
    }
    if (!genericRoot.isEmpty()) {
        return QDir(genericRoot).absoluteFilePath(QStringLiteral("ft2/ALL.TXT"));
    }
    return QStringLiteral("ALL.TXT");
}

void DecodiumBridge::appendLegacyAllTxtDecodeLine(const QVariantMap& entry) const
{
    if (entry.isEmpty() || entry.value(QStringLiteral("isTx")).toBool()) {
        return;
    }

    // Quando il backend legacy embedded è attivo, il suo MainWindow scrive già
    // ALL.TXT in formato D3. Se aggiungiamo anche le righe del decoder moderno,
    // il file privato di D4 finisce contaminato con decode extra che D3 non ha
    // e il mirror del pannello non può più coincidere 1:1 col legacy.
    if (usingLegacyBackendForTx()) {
        return;
    }

    QString const path = legacyAllTxtPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        return;
    }

    QDateTime const utcWhen = decodiumBridgeApproxUtcDateTimeForDisplayToken(entry.value(QStringLiteral("time")).toString());
    QString const modeString = m_mode.leftJustified(6, QLatin1Char(' '));
    QString const db = entry.value(QStringLiteral("db")).toString().trimmed();
    QString const dt = entry.value(QStringLiteral("dt")).toString().trimmed();
    QString const freq = entry.value(QStringLiteral("freq")).toString().trimmed();
    QString const message = canonicalDecodeMessage(entry.value(QStringLiteral("message")).toString());
    if (message.isEmpty()) {
        return;
    }

    QTextStream out(&file);
    out << QStringLiteral("%1 %2  Rx  %3%4%5%6  %7")
              .arg(utcWhen.toString(QStringLiteral("yyMMdd_hhmmss")))
              .arg(m_frequency / 1e6, 10, 'f', 3)
              .arg(modeString)
              .arg(db, 4)
              .arg(dt, 5)
              .arg(freq, 5)
              .arg(message)
        << '\n';
}

QString DecodiumBridge::logAllTxtPath() const
{
    QString const effectivePath = legacyAllTxtPath().trimmed();
    if (!effectivePath.isEmpty()) {
        return effectivePath;
    }

    QString const appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty()) {
        return QDir(appData).absoluteFilePath(QStringLiteral("ALL.TXT"));
    }

    return QStringLiteral("ALL.TXT");
}

bool DecodiumBridge::openAllTxtFolder() const
{
    QString path = logAllTxtPath().trimmed();
    if (path.isEmpty()) {
        return false;
    }

    QFileInfo const info(path);
    QDir dir = info.exists() ? info.absoluteDir() : QFileInfo(info.absolutePath()).absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    return QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
}

