#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStandardPaths>
#include <QStringList>

static bool qsoJournalWanted(const QString& msg)
{
    static QStringList const keys {
        QStringLiteral("autoSeq"), QStringLiteral("checkAndStartPeriodicTx"),
        QStringLiteral("finishAutoSequenceQso"), QStringLiteral("processDecodeDoubleClick"),
        QStringLiteral("FT2 AutoCQ"), QStringLiteral("UDP reply accepted"),
        QStringLiteral("Wait & Pounce"), QStringLiteral("[CALL]"), QStringLiteral("[STATE"),
        QStringLiteral("smartFt2Tx"), QStringLiteral("FT2 async abort"),
        QStringLiteral("manualTxHold"), QStringLiteral("retry limit"),
        QStringLiteral("logQso"), QStringLiteral("[FT2-ATTESO]"),
        QStringLiteral("advanceQsoState"), QStringLiteral("caller queue"),
        QStringLiteral("processNextInQueue"), QStringLiteral("signoff"),
        QStringLiteral("QSO complet"), QStringLiteral("partner left"),
        QStringLiteral("MAM "),
    };
    for (QString const& key : keys) {
        if (msg.contains(key)) return true;
    }
    return false;
}

static void qsoJournal(const QString& msg)
{
    static bool const enabled =
        qEnvironmentVariable("DECODIUM_QSO_JOURNAL").trimmed() != QStringLiteral("0");
    if (!enabled) return;
    static QMutex mutex;
    QMutexLocker locker {&mutex};
    static QString dirPath;
    static bool cleaned = false;
    if (dirPath.isEmpty()) {
        dirPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            + QStringLiteral("/qso_journal");
        QDir().mkpath(dirPath);
    }
    QDateTime const now = QDateTime::currentDateTimeUtc();
    if (!cleaned) {
        cleaned = true;
        QDir dir {dirPath};
        QDate const limit = now.date().addDays(-30);
        for (QFileInfo const& info : dir.entryInfoList({QStringLiteral("sequencer_*.log")}, QDir::Files)) {
            QDate const day = QDate::fromString(info.completeBaseName().mid(10), QStringLiteral("yyyy-MM-dd"));
            if (day.isValid() && day < limit) QFile::remove(info.absoluteFilePath());
        }
    }
    QFile file {dirPath + QStringLiteral("/sequencer_") + now.toString(QStringLiteral("yyyy-MM-dd"))
                + QStringLiteral(".log")};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
    QByteArray const line = (now.toString(QStringLiteral("HH:mm:ss.zzz")) + QLatin1Char(' ')
                             + QString(msg).replace(QLatin1Char('\n'), QLatin1Char(' '))
                             + QLatin1Char('\n')).toUtf8();
    file.write(line);
}

void decodiumBridgeQsoJournal(const QString& msg) { qsoJournal(msg); }

bool decodiumBridgeQsoJournalWanted(const QString& msg) { return qsoJournalWanted(msg); }

