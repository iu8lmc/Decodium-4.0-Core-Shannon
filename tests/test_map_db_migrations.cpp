// Migrazioni dei dati del database della mappa (openMapDatabase): ora toccano
// solo le righe non ancora migrate. Il risultato deve restare quello delle
// istruzioni originali, che giravano su tutte le righe a ogni apertura.
//
// Il sorgente del servizio e' incluso qui per raggiungere le funzioni interne
// (openMapDatabase, runMapDataMigrations, liveSpotCapsDue).
#include "../src/services/MapIntelligenceService.cpp"

#include <QTemporaryDir>
#include <QtTest>

namespace
{
// Le istruzioni come erano prima, su tutte le righe.
QStringList const kOriginalMigrations {
    QStringLiteral("UPDATE map_spot SET grid_origin=CASE"
                   " WHEN lower(source)='decoder' AND grid<>'' THEN 'DECODED'"
                   " WHEN lower(source)='psk' AND grid<>'' THEN 'PSK'"
                   " WHEN lower(source)='oams' AND grid<>'' THEN 'OAMS'"
                   " ELSE 'UNKNOWN' END"
                   " WHERE COALESCE(grid_origin, 'UNKNOWN')='UNKNOWN'"),
    QStringLiteral("UPDATE map_qso SET grid6=upper(substr(grid,1,6))"
                   " WHERE (grid6 IS NULL OR grid6='') AND length(grid)>=6"),
    QStringLiteral("UPDATE map_spot SET grid6=upper(substr(grid,1,6))"
                   " WHERE (grid6 IS NULL OR grid6='') AND length(grid)>=6"),
    QStringLiteral("INSERT OR IGNORE INTO map_qso_grid(qso_id, grid, grid4, grid6, is_primary)"
                   " SELECT id, upper(grid), upper(grid4), nullif(upper(grid6), ''), 1"
                   " FROM map_qso WHERE COALESCE(grid4, '')<>''"
                   " AND NOT EXISTS (SELECT 1 FROM map_qso_grid existing"
                   " WHERE existing.qso_id=map_qso.id AND existing.grid=upper(map_qso.grid))"),
    QStringLiteral("UPDATE map_spot SET first_observed_ms=observed_ms, last_observed_ms=observed_ms"
                   " WHERE first_observed_ms=0 OR last_observed_ms=0"),
    QStringLiteral("UPDATE map_qso SET propagation_mode='UNKNOWN'"
                   " WHERE propagation_mode IS NULL OR trim(propagation_mode)=''"),
    QStringLiteral("UPDATE map_spot SET propagation_mode='UNKNOWN'"
                   " WHERE propagation_mode IS NULL OR trim(propagation_mode)=''"),
    QStringLiteral("UPDATE map_spot_event SET propagation_mode='UNKNOWN'"
                   " WHERE propagation_mode IS NULL OR trim(propagation_mode)=''"),
};

void exec(QSqlDatabase& db, QString const& sql)
{
    QSqlQuery query(db);
    QVERIFY2(query.exec(sql), qPrintable(sql + QStringLiteral(" -> ") + query.lastError().text()));
}

// Righe "vecchie": colonne derivate vuote o nulle, come le scriveva una
// versione precedente o come le lascia un import da completare.
void insertLegacyBatch(QSqlDatabase& db, int batch)
{
    for (int i = 0; i < 6; ++i) {
        QString const n = QStringLiteral("%1_%2").arg(batch).arg(i);
        QString const source = i % 3 == 0 ? QStringLiteral("decoder") : i % 3 == 1 ? QStringLiteral("PSK") : QStringLiteral("oams");
        QString const grid = i % 2 == 0 ? QStringLiteral("jn70ab") : (i == 3 ? QString() : QStringLiteral("JN61"));
        exec(db, QStringLiteral(
                     "INSERT INTO map_spot (unique_key, call, grid, grid6, grid_origin, observed_utc, observed_ms,"
                     " first_observed_ms, last_observed_ms, propagation_mode, source)"
                     " VALUES ('k%1', 'IU%2AB', '%3', %4, %5, '2026-11-26T20:00:00Z', %6, %7, 0, %8, '%9')")
                     .arg(n).arg(i).arg(grid)
                     .arg(i % 2 ? QStringLiteral("NULL") : QStringLiteral("''"))
                     .arg(i == 3 ? QStringLiteral("'PSK'") : QStringLiteral("'UNKNOWN'"))
                     .arg(1700000000000LL + batch * 1000 + i)
                     .arg(i % 2 ? 0 : 1700000000000LL)
                     .arg(i % 2 ? QStringLiteral("''") : QStringLiteral("'  '"))
                     .arg(source));
        exec(db, QStringLiteral(
                     "INSERT INTO map_qso (source_key, call, grid, grid4, grid6, propagation_mode)"
                     " VALUES ('q%1', 'K%2XY', '%3', '%4', '', %5)")
                     .arg(n).arg(i)
                     .arg(i % 2 ? QStringLiteral("fn42ab") : QStringLiteral("FN42"))
                     .arg(i == 5 ? QString() : QStringLiteral("fn42"))
                     .arg(i % 2 ? QStringLiteral("'  '") : QStringLiteral("''")));
        exec(db, QStringLiteral(
                     "INSERT INTO map_spot_event (spot_key, call, observed_ms, propagation_mode)"
                     " VALUES ('k%1', 'IU%2AB', %3, %4)")
                     .arg(n).arg(i).arg(1700000000000LL + batch * 1000 + i)
                     .arg(i % 2 ? QStringLiteral("''") : QStringLiteral("'ES'")));
    }
}

QString dump(QSqlDatabase& db)
{
    QString out;
    for (QString const& table : {QStringLiteral("map_spot"), QStringLiteral("map_qso"),
                                 QStringLiteral("map_qso_grid"), QStringLiteral("map_spot_event")}) {
        QSqlQuery query(db);
        QString const order = table == QStringLiteral("map_qso_grid")
            ? QStringLiteral("qso_id, grid") : QStringLiteral("id");
        if (!query.exec(QStringLiteral("SELECT * FROM %1 ORDER BY %2").arg(table, order))) {
            return query.lastError().text();
        }
        while (query.next()) {
            out += table;
            for (int c = 0; c < query.record().count(); ++c) {
                QVariant const v = query.value(c);
                out += QLatin1Char('|') + (v.isNull() ? QStringLiteral("<null>") : v.toString());
            }
            out += QLatin1Char('\n');
        }
    }
    return out;
}

qint64 mark(QSqlDatabase& db, QString const& table)
{
    QSqlQuery query(db);
    query.exec(QStringLiteral("SELECT value FROM map_meta WHERE key='data_migration_v%1_max_id_%2'")
                   .arg(kMapDataMigrationVersion).arg(table));
    return query.next() ? query.value(0).toLongLong() : -1;
}
}  // namespace

class TestMapDbMigrations : public QObject
{
    Q_OBJECT

private slots:
    // Tre giri di righe vecchie con aperture in mezzo: il database aperto con
    // le migrazioni incrementali deve finire uguale, colonna per colonna, a
    // uno su cui dopo ogni giro girano le istruzioni originali su tutto.
    void incrementalMatchesFullMigration()
    {
        QTemporaryDir dir;
        QString const incremental = dir.filePath(QStringLiteral("incremental.sqlite"));
        QString const full = dir.filePath(QStringLiteral("full.sqlite"));
        for (QString const& path : {incremental, full}) {
            std::unique_ptr<ScopedSqliteConnection> connection;
            QString error;
            QVERIFY2(openMapDatabase(path, &connection, &error), qPrintable(error));
        }
        for (int batch = 0; batch < 3; ++batch) {
            {
                std::unique_ptr<ScopedSqliteConnection> connection;
                QString error;
                QVERIFY2(openMapDatabase(incremental, &connection, &error), qPrintable(error));
                insertLegacyBatch(connection->database(), batch);
            }
            {
                std::unique_ptr<ScopedSqliteConnection> connection;
                QString error;
                QVERIFY2(openMapDatabase(incremental, &connection, &error), qPrintable(error));
            }
            ScopedSqliteConnection reference(full);
            QVERIFY(reference.database().open());
            insertLegacyBatch(reference.database(), batch);
            for (QString const& sql : kOriginalMigrations) {
                exec(reference.database(), sql);
            }
        }
        ScopedSqliteConnection a(incremental);
        ScopedSqliteConnection b(full);
        QVERIFY(a.database().open());
        QVERIFY(b.database().open());
        QString const dumpA = dump(a.database());
        QCOMPARE(dumpA, dump(b.database()));
        QVERIFY(dumpA.contains(QStringLiteral("DECODED")));
        QVERIFY(dumpA.contains(QStringLiteral("|OAMS|")) || dumpA.contains(QStringLiteral("OAMS")));
        QVERIFY(dumpA.contains(QStringLiteral("JN70AB")));
        QCOMPARE(mark(a.database(), QStringLiteral("map_spot")), 18);
        QCOMPARE(mark(a.database(), QStringLiteral("map_qso")), 18);
        QCOMPARE(mark(a.database(), QStringLiteral("map_spot_event")), 18);
    }

    // Le righe gia' migrate non vengono piu' rilette ne' riscritte.
    void migratedRowsAreNotRevisited()
    {
        QTemporaryDir dir;
        QString const path = dir.filePath(QStringLiteral("map.sqlite"));
        {
            std::unique_ptr<ScopedSqliteConnection> connection;
            QString error;
            QVERIFY2(openMapDatabase(path, &connection, &error), qPrintable(error));
            insertLegacyBatch(connection->database(), 0);
        }
        {
            std::unique_ptr<ScopedSqliteConnection> connection;
            QString error;
            QVERIFY2(openMapDatabase(path, &connection, &error), qPrintable(error));
            exec(connection->database(), QStringLiteral("UPDATE map_spot SET propagation_mode='' WHERE id=1"));
        }
        std::unique_ptr<ScopedSqliteConnection> connection;
        QString error;
        QVERIFY2(openMapDatabase(path, &connection, &error), qPrintable(error));
        QSqlQuery query(connection->database());
        QVERIFY(query.exec(QStringLiteral("SELECT propagation_mode FROM map_spot WHERE id=1")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QString());
    }

    // Un database senza segni (versione precedente) viene migrato tutto.
    void unmarkedDatabaseIsMigratedCompletely()
    {
        QTemporaryDir dir;
        QString const path = dir.filePath(QStringLiteral("map.sqlite"));
        {
            std::unique_ptr<ScopedSqliteConnection> connection;
            QString error;
            QVERIFY2(openMapDatabase(path, &connection, &error), qPrintable(error));
            insertLegacyBatch(connection->database(), 0);
            exec(connection->database(), QStringLiteral("DELETE FROM map_meta WHERE key LIKE 'data_migration_%'"));
        }
        std::unique_ptr<ScopedSqliteConnection> connection;
        QString error;
        QVERIFY2(openMapDatabase(path, &connection, &error), qPrintable(error));
        QSqlQuery query(connection->database());
        QVERIFY(query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM map_spot WHERE trim(propagation_mode)=''"
            " OR first_observed_ms=0 OR (grid_origin='UNKNOWN' AND grid<>'')")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
    }

    void capsRunAtMostOncePerMinute()
    {
        QString const path = QStringLiteral("caps-test-path");
        QVERIFY(liveSpotCapsDue(path));
        QVERIFY(!liveSpotCapsDue(path));
        QVERIFY(liveSpotCapsDue(path + QStringLiteral("-other")));
    }
};

QTEST_GUILESS_MAIN (TestMapDbMigrations)
#include "test_map_db_migrations.moc"
