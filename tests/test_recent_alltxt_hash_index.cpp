// RecentAllTxtHashIndex: righe recenti dei log ALL.TXT in RAM per risolvere i
// nominativi <...> (src/bridge/LegacyHashResolve.cpp).
#include "src/bridge/LegacyHashResolve.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

namespace
{
QString decodiumLine(QDateTime const& utc, int hz, QString const& message)
{
    return QStringLiteral("%1     14.074  Rx  FT8    -12  0.2 %2  %3\r\n")
        .arg(utc.toString(QStringLiteral("yyMMdd_hhmmss")))
        .arg(hz, 4)
        .arg(message);
}

QString jtdxLine(QDateTime const& utc, int hz, QString const& message)
{
    return QStringLiteral("%1  -12  0.2 %2 ~  %3\r\n")
        .arg(utc.toString(QStringLiteral("yyyyMMdd_hhmmss")))
        .arg(hz, 4)
        .arg(message);
}

void append(QString const& path, QByteArray const& bytes)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Append));
    QCOMPARE(file.write(bytes), qint64(bytes.size()));
}

QStringList messages(QVector<HashResolveAllTxtCandidate> const& candidates)
{
    QStringList out;
    for (HashResolveAllTxtCandidate const& candidate : candidates) {
        out << candidate.message;
    }
    return out;
}
}  // namespace

class TestRecentAllTxtHashIndex : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir dir_;
    QDateTime now_ {QDate(2026, 11, 26), QTime(8, 30, 15), QTimeZone::UTC};

    QString path(QString const& name) const { return dir_.filePath(name); }

private slots:
    void keepsOnlyRecentLines()
    {
        QString const log = path(QStringLiteral("recent_ALL.TXT"));
        QByteArray bytes;
        // Stessa ora di ieri: prima entrava fra i candidati e poteva dare una
        // risoluzione sbagliata o ambigua.
        bytes += decodiumLine(now_.addDays(-1), 1500, QStringLiteral("IU8LMC W1OLD -12")).toLatin1();
        bytes += decodiumLine(now_.addSecs(-20 * 60), 1500, QStringLiteral("IU8LMC W2OLD -12")).toLatin1();
        bytes += decodiumLine(now_.addSecs(-60), 1400, QStringLiteral("CQ K1ABC FN42")).toLatin1();
        bytes += decodiumLine(now_, 1500, QStringLiteral("IU8LMC W9XYZ -12")).toLatin1();
        bytes += decodiumLine(now_, 1600, QStringLiteral("IU8LMC <...> -10")).toLatin1();
        append(log, bytes);

        RecentAllTxtHashIndex index;
        RecentAllTxtHashIndex::Stats stats;
        auto const candidates = index.refresh({log}, now_, &stats);
        QCOMPARE(messages(candidates), (QStringList {QStringLiteral("CQ K1ABC FN42"),
                                                     QStringLiteral("IU8LMC W9XYZ -12")}));
        QCOMPARE(candidates.first().source, QStringLiteral("recent_ALL.TXT"));
        QCOMPARE(stats.restarts, 1);

        QString source;
        QString const resolved = resolveHashMessageFromCandidates(QStringLiteral("083015"), 1502,
                                                                  QStringLiteral("IU8LMC <...> -12"),
                                                                  candidates, &source);
        QCOMPARE(resolved, QStringLiteral("IU8LMC W9XYZ -12"));
        QCOMPARE(source, QStringLiteral("recent_ALL.TXT"));

        // Passati 20 minuti le righe escono dall'indice.
        QVERIFY(index.refresh({log}, now_.addSecs(20 * 60)).isEmpty());
    }

    void readsOnlyAppendedBytesAndWaitsForWholeLines()
    {
        QString const log = path(QStringLiteral("growing_ALL.TXT"));
        QByteArray old;
        while (old.size() < 300 * 1024) {
            old += decodiumLine(now_.addDays(-3), 1000, QStringLiteral("CQ W0OLD EN10")).toLatin1();
        }
        append(log, old);

        RecentAllTxtHashIndex index;
        RecentAllTxtHashIndex::Stats stats;
        QVERIFY(index.refresh({log}, now_, &stats).isEmpty());
        QVERIFY(stats.readBytes <= RecentAllTxtHashIndex::kInitialTailBytes);

        QByteArray const line = jtdxLine(now_, 1234, QStringLiteral("K1ABC W9XYZ R-05")).toLatin1();
        append(log, line.left(20));
        QVERIFY(index.refresh({log}, now_, &stats).isEmpty());  // riga a meta'
        QCOMPARE(stats.readBytes, qint64(20));
        QCOMPARE(stats.restarts, 0);

        append(log, line.mid(20));
        auto const candidates = index.refresh({log}, now_, &stats);
        QCOMPARE(messages(candidates), QStringList {QStringLiteral("K1ABC W9XYZ R-05")});
        QCOMPARE(candidates.first().audioHz, 1234);
        QCOMPARE(stats.readBytes, qint64(line.size()));
        QCOMPARE(stats.restarts, 0);
    }

    void restartsAfterEraseOrRewrite()
    {
        QString const log = path(QStringLiteral("erase_ALL.TXT"));
        append(log, decodiumLine(now_, 1500, QStringLiteral("IU8LMC W9XYZ -12")).toLatin1());
        RecentAllTxtHashIndex index;
        RecentAllTxtHashIndex::Stats stats;
        QCOMPARE(index.refresh({log}, now_).size(), 1);

        // Erase ALL.TXT, poi una riga nuova.
        {
            QFile file(log);
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        }
        append(log, decodiumLine(now_, 900, QStringLiteral("CQ K2NEW FN20")).toLatin1());
        QCOMPARE(messages(index.refresh({log}, now_, &stats)), QStringList {QStringLiteral("CQ K2NEW FN20")});
        QCOMPARE(stats.restarts, 1);

        // Riscritto sul posto, piu' lungo: la dimensione non cala ma il
        // contenuto gia' letto cambia.
        QByteArray rewritten;
        rewritten += decodiumLine(now_, 700, QStringLiteral("CQ K3REW FN31")).toLatin1();
        rewritten += decodiumLine(now_, 800, QStringLiteral("CQ K4REW FN31")).toLatin1();
        {
            QFile file(log);
            QVERIFY(file.open(QIODevice::ReadWrite));
            QVERIFY(file.seek(0));
            QCOMPARE(file.write(rewritten), qint64(rewritten.size()));
        }
        QCOMPARE(messages(index.refresh({log}, now_, &stats)),
                 (QStringList {QStringLiteral("CQ K3REW FN31"), QStringLiteral("CQ K4REW FN31")}));
        QCOMPARE(stats.restarts, 1);

        // Il file sparisce: niente candidati, niente errori.
        QVERIFY(QFile::remove(log));
        QVERIFY(index.refresh({log}, now_).isEmpty());
    }

    void oldFileReadsShortTail()
    {
        QString const log = path(QStringLiteral("202609_ALL.TXT"));
        QByteArray old;
        while (old.size() < 2 * 1024 * 1024) {
            old += jtdxLine(now_.addDays(-60), 1500, QStringLiteral("CQ W0OLD EN10")).toLatin1();
        }
        append(log, old);
        {
            QFile file(log);
            QVERIFY(file.open(QIODevice::ReadWrite));
            QVERIFY(file.setFileTime(now_.addDays(-60), QFileDevice::FileModificationTime));
        }
        RecentAllTxtHashIndex index;
        RecentAllTxtHashIndex::Stats stats;
        QVERIFY(index.refresh({log}, now_, &stats).isEmpty());
        QVERIFY(stats.readBytes <= RecentAllTxtHashIndex::kOldFileTailBytes);

        // Se poi qualcuno torna a scriverci, le righe nuove entrano.
        append(log, jtdxLine(now_, 1500, QStringLiteral("CQ K5NEW EM12")).toLatin1());
        QCOMPARE(messages(index.refresh({log}, now_, &stats)), QStringList {QStringLiteral("CQ K5NEW EM12")});
        QCOMPARE(stats.restarts, 0);
    }

    void candidatesFollowPathOrder()
    {
        QString const a = path(QStringLiteral("a_ALL.TXT"));
        QString const b = path(QStringLiteral("b_ALL.TXT"));
        append(a, decodiumLine(now_, 1500, QStringLiteral("CQ K1AAA FN42")).toLatin1());
        append(b, jtdxLine(now_, 1500, QStringLiteral("CQ K1BBB FN42")).toLatin1());
        RecentAllTxtHashIndex index;
        QCOMPARE(messages(index.refresh({b, a}, now_)),
                 (QStringList {QStringLiteral("CQ K1BBB FN42"), QStringLiteral("CQ K1AAA FN42")}));
        // Un log tolto dalla lista lascia la RAM.
        QCOMPARE(messages(index.refresh({a}, now_)), QStringList {QStringLiteral("CQ K1AAA FN42")});
    }

    // Misura su log veri (solo lettura): DECODIUM_RECENT_INDEX_BENCH_PATHS con
    // i percorsi separati da ';'.
    void benchRealLogs()
    {
        QString const value = qEnvironmentVariable("DECODIUM_RECENT_INDEX_BENCH_PATHS");
        if (value.isEmpty()) {
            QSKIP("DECODIUM_RECENT_INDEX_BENCH_PATHS non impostata");
        }
        QStringList const paths = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        RecentAllTxtHashIndex index;
        for (int round = 0; round < 3; ++round) {
            RecentAllTxtHashIndex::Stats stats;
            QElapsedTimer timer;
            timer.start();
            index.refresh(paths, QDateTime::currentDateTimeUtc(), &stats);
            qInfo().noquote() << "round" << round << "files=" << stats.files
                              << "restarts=" << stats.restarts << "read_bytes=" << stats.readBytes
                              << "candidates=" << stats.candidates
                              << "elapsed_us=" << timer.nsecsElapsed() / 1000;
        }
    }
};

QTEST_GUILESS_MAIN (TestRecentAllTxtHashIndex)
#include "test_recent_alltxt_hash_index.moc"
