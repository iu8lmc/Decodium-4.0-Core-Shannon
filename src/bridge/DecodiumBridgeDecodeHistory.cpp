#include "DecodiumBridge.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <utility>

extern void decodiumBridgeLogbookLog(const QString&);

// =====================================================================
// === Phase 5.3 (fork-only 1.0.268) — Decode History Query API     ===
// =====================================================================

// 1.0.270 — Cleanup retention DB: elimina decode + sessioni orfane piu' vecchie
// di N giorni + VACUUM per rilasciare spazio su disco. Eseguito UNA volta al
// boot della sessione (in startPersistenceWorker, prima del worker thread).
void DecodiumBridge::pruneDecodeHistoryOlderThan(int days)
{
    if (days <= 0) return;
    QSqlDatabase db = QSqlDatabase::database(QSqlDatabase::defaultConnection);
    if (!db.isValid() || !db.isOpen()) {
        decodiumBridgeLogbookLog(QStringLiteral("[Phase5.3] pruneDecodeHistory: default DB unavailable"));
        return;
    }
    qint64 const cutoffMs = QDateTime::currentMSecsSinceEpoch()
                            - static_cast<qint64>(days) * 24LL * 3600LL * 1000LL;

    qint64 const sizeBefore = QFileInfo(db.databaseName()).size();
    int deletedDecodes = 0;
    int deletedSessions = 0;

    // 1. DELETE decode > N giorni
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("DELETE FROM decodes WHERE ts_utc < :cutoff"));
        q.bindValue(QStringLiteral(":cutoff"), cutoffMs);
        if (!q.exec()) {
            decodiumBridgeLogbookLog(QStringLiteral("[Phase5.3] pruneDecodeHistory DELETE decodes failed: %1")
                          .arg(q.lastError().text()));
            return;
        }
        deletedDecodes = q.numRowsAffected();
    }
    // 2. DELETE sessioni orfane (nessun decode collegato)
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral(
                "DELETE FROM sessions WHERE id NOT IN "
                "(SELECT DISTINCT session_id FROM decodes)"))) {
            deletedSessions = q.numRowsAffected();
        } else {
            decodiumBridgeLogbookLog(QStringLiteral("[Phase5.3] pruneDecodeHistory DELETE sessions failed: %1")
                          .arg(q.lastError().text()));
        }
    }
    // 3. VACUUM se abbiamo cancellato qualcosa
    if (deletedDecodes > 0 || deletedSessions > 0) {
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("VACUUM"))) {
            decodiumBridgeLogbookLog(QStringLiteral("[Phase5.3] pruneDecodeHistory VACUUM failed: %1")
                          .arg(q.lastError().text()));
        }
    }
    qint64 const sizeAfter = QFileInfo(db.databaseName()).size();
    double const freedMb = (sizeBefore - sizeAfter) / (1024.0 * 1024.0);

    decodiumBridgeLogbookLog(QStringLiteral("[Phase5.3] pruneDecodeHistory(%1d): deleted %2 decodes + "
                             "%3 orphan sessions, freed %4 MB (DB %5 -> %6 MB)")
                  .arg(days)
                  .arg(deletedDecodes)
                  .arg(deletedSessions)
                  .arg(freedMb, 0, 'f', 2)
                  .arg(sizeBefore / (1024.0 * 1024.0), 0, 'f', 2)
                  .arg(sizeAfter  / (1024.0 * 1024.0), 0, 'f', 2));
}

// Apertura connessione SQLite read-only separata dal worker
QSqlDatabase DecodiumBridge::openHistoryReadDb() const
{
    static const QString kConnName = QStringLiteral("decode_history_reader");
    if (QSqlDatabase::contains(kConnName)) {
        QSqlDatabase db = QSqlDatabase::database(kConnName);
        if (db.isOpen()) return db;
    }
    QSqlDatabase defaultDb = QSqlDatabase::database(QSqlDatabase::defaultConnection);
    if (!defaultDb.isValid()) {
        return QSqlDatabase();
    }
    QString const dbPath = defaultDb.databaseName();
    if (dbPath.isEmpty()) {
        return QSqlDatabase();
    }
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnName);
    db.setDatabaseName(dbPath);
    db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    if (!db.open()) {
        decodiumBridgeLogbookLog(QStringLiteral("[Phase5.3] openHistoryReadDb open failed: %1")
                      .arg(db.lastError().text()));
        QSqlDatabase::removeDatabase(kConnName);
        return QSqlDatabase();
    }
    return db;
}

QVariantList DecodiumBridge::queryDecodeHistory(QVariantMap const& filters) const
{
    QVariantList rows;
    QSqlDatabase db = openHistoryReadDb();
    if (!db.isValid()) return rows;

    QStringList where;
    QVariantMap binds;

    QString const callsign = filters.value(QStringLiteral("callsign")).toString().trimmed().toUpper();
    if (!callsign.isEmpty()) {
        where << QStringLiteral("(UPPER(callsign_dx) LIKE :cs "
                                 "OR UPPER(callsign_de) LIKE :cs "
                                 "OR UPPER(message) LIKE :csmsg)");
        binds.insert(QStringLiteral(":cs"), QStringLiteral("%%1%%").arg(callsign));
        binds.insert(QStringLiteral(":csmsg"), QStringLiteral("%%1%%").arg(callsign));
    }
    QString const band = filters.value(QStringLiteral("band")).toString().trimmed().toUpper();
    if (!band.isEmpty()) {
        where << QStringLiteral("band = :band");
        binds.insert(QStringLiteral(":band"), band);
    }
    QString const mode = filters.value(QStringLiteral("mode")).toString().trimmed().toUpper();
    if (!mode.isEmpty()) {
        where << QStringLiteral("mode = :mode");
        binds.insert(QStringLiteral(":mode"), mode);
    }
    if (filters.contains(QStringLiteral("fromUtc"))) {
        qint64 const fromUtc = filters.value(QStringLiteral("fromUtc")).toLongLong();
        if (fromUtc > 0) {
            where << QStringLiteral("ts_utc >= :fromUtc");
            binds.insert(QStringLiteral(":fromUtc"), fromUtc);
        }
    }
    if (filters.contains(QStringLiteral("toUtc"))) {
        qint64 const toUtc = filters.value(QStringLiteral("toUtc")).toLongLong();
        if (toUtc > 0) {
            where << QStringLiteral("ts_utc <= :toUtc");
            binds.insert(QStringLiteral(":toUtc"), toUtc);
        }
    }
    if (filters.contains(QStringLiteral("sessionId"))) {
        qint64 const sid = filters.value(QStringLiteral("sessionId")).toLongLong();
        if (sid > 0) {
            where << QStringLiteral("session_id = :sid");
            binds.insert(QStringLiteral(":sid"), sid);
        }
    }

    int limit = filters.value(QStringLiteral("limit"), 500).toInt();
    limit = qBound(1, limit, 5000);
    bool const newestFirst = filters.value(QStringLiteral("newestFirst"), true).toBool();

    QString sql = QStringLiteral("SELECT id, ts_utc, band, freq_hz, mode, submode, "
                                 "callsign_dx, callsign_de, grid, snr_db, dt_s, df_hz, "
                                 "message, session_id FROM decodes");
    if (!where.isEmpty()) sql += QStringLiteral(" WHERE ") + where.join(QStringLiteral(" AND "));
    sql += QStringLiteral(" ORDER BY ts_utc ") + (newestFirst ? QStringLiteral("DESC") : QStringLiteral("ASC"));
    sql += QStringLiteral(" LIMIT :limit");

    QSqlQuery q(db);
    if (!q.prepare(sql)) {
        decodiumBridgeLogbookLog(QStringLiteral("[Phase5.3] queryDecodeHistory prepare failed: %1").arg(q.lastError().text()));
        return rows;
    }
    for (auto it = binds.constBegin(); it != binds.constEnd(); ++it) {
        q.bindValue(it.key(), it.value());
    }
    q.bindValue(QStringLiteral(":limit"), limit);
    if (!q.exec()) {
        decodiumBridgeLogbookLog(QStringLiteral("[Phase5.3] queryDecodeHistory exec failed: %1").arg(q.lastError().text()));
        return rows;
    }
    while (q.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"),          q.value(0));
        row.insert(QStringLiteral("ts_utc"),      q.value(1));
        row.insert(QStringLiteral("band"),        q.value(2));
        row.insert(QStringLiteral("freq_hz"),     q.value(3));
        row.insert(QStringLiteral("mode"),        q.value(4));
        row.insert(QStringLiteral("submode"),     q.value(5));
        row.insert(QStringLiteral("callsign_dx"), q.value(6));
        row.insert(QStringLiteral("callsign_de"), q.value(7));
        row.insert(QStringLiteral("grid"),        q.value(8));
        row.insert(QStringLiteral("snr_db"),      q.value(9));
        row.insert(QStringLiteral("dt_s"),        q.value(10));
        row.insert(QStringLiteral("df_hz"),       q.value(11));
        row.insert(QStringLiteral("message"),     q.value(12));
        row.insert(QStringLiteral("session_id"),  q.value(13));
        rows.append(row);
    }
    return rows;
}

QVariantList DecodiumBridge::queryDecodeSessions() const
{
    QVariantList rows;
    QSqlDatabase db = openHistoryReadDb();
    if (!db.isValid()) return rows;
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "SELECT s.id, s.started_utc, s.ended_utc, s.operator, s.station, s.decodium_ver, "
            "       COUNT(d.id) AS dcount "
            "FROM sessions s LEFT JOIN decodes d ON d.session_id = s.id "
            "GROUP BY s.id ORDER BY s.id DESC LIMIT 200"))) {
        decodiumBridgeLogbookLog(QStringLiteral("[Phase5.3] queryDecodeSessions failed: %1").arg(q.lastError().text()));
        return rows;
    }
    while (q.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"),           q.value(0));
        row.insert(QStringLiteral("started_utc"),  q.value(1));
        row.insert(QStringLiteral("ended_utc"),    q.value(2));
        row.insert(QStringLiteral("operator"),     q.value(3));
        row.insert(QStringLiteral("station"),      q.value(4));
        row.insert(QStringLiteral("decodium_ver"), q.value(5));
        row.insert(QStringLiteral("decode_count"), q.value(6));
        rows.append(row);
    }
    return rows;
}

QStringList DecodiumBridge::queryDecodeBands() const
{
    QStringList bands;
    QSqlDatabase db = openHistoryReadDb();
    if (!db.isValid()) return bands;
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT DISTINCT band FROM decodes WHERE band != '' ORDER BY band"))) {
        while (q.next()) bands << q.value(0).toString();
    }
    return bands;
}

QStringList DecodiumBridge::queryDecodeModes() const
{
    QStringList modes;
    QSqlDatabase db = openHistoryReadDb();
    if (!db.isValid()) return modes;
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT DISTINCT mode FROM decodes WHERE mode != '' ORDER BY mode"))) {
        while (q.next()) modes << q.value(0).toString();
    }
    return modes;
}

QVariantMap DecodiumBridge::queryDecodeStats() const
{
    QVariantMap stats;
    QSqlDatabase db = openHistoryReadDb();
    if (!db.isValid()) return stats;
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT COUNT(*), MIN(ts_utc), MAX(ts_utc) FROM decodes"))) {
        if (q.next()) {
            stats.insert(QStringLiteral("total"),      q.value(0).toLongLong());
            stats.insert(QStringLiteral("first_utc"),  q.value(1));
            stats.insert(QStringLiteral("last_utc"),   q.value(2));
        }
    }
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM sessions"))) {
        if (q.next()) stats.insert(QStringLiteral("session_count"), q.value(0).toLongLong());
    }
    return stats;
}

bool DecodiumBridge::exportAdifFromHistory(QVariantMap const& filters, QString const& path)
{
    QVariantList rows = queryDecodeHistory(filters);
    QString cleanPath = path;
    if (cleanPath.startsWith(QStringLiteral("file:///"))) cleanPath.remove(0, 8);
    else if (cleanPath.startsWith(QStringLiteral("file://"))) cleanPath.remove(0, 7);
    QFile f(cleanPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        decodiumBridgeLogbookLog(QStringLiteral("[Phase5.3] exportAdifFromHistory open failed: %1").arg(cleanPath));
        return false;
    }
    QTextStream out(&f);
    out << "Decodium decode history export\n";
    out << "<ADIF_VER:5>3.1.4\n";
    out << "<PROGRAMID:8>Decodium\n";
    out << "<PROGRAMVERSION:" << QString::fromLatin1(FORK_RELEASE_VERSION).size()
        << ">" << QString::fromLatin1(FORK_RELEASE_VERSION) << "\n";
    out << "<EOH>\n";
    auto adifField = [&out](QString const& name, QString const& value) {
        if (value.isEmpty()) return;
        out << QStringLiteral("<%1:%2>%3 ").arg(name).arg(value.size()).arg(value);
    };
    int written = 0;
    for (QVariant const& r : std::as_const(rows)) {
        QVariantMap const e = r.toMap();
        qint64 const ts = e.value(QStringLiteral("ts_utc")).toLongLong();
        QDateTime const dt = QDateTime::fromMSecsSinceEpoch(ts, Qt::UTC);
        QString const dxCall = e.value(QStringLiteral("callsign_dx")).toString().trimmed();
        if (dxCall.isEmpty()) continue;
        adifField(QStringLiteral("CALL"),    dxCall);
        adifField(QStringLiteral("QSO_DATE"), dt.toString(QStringLiteral("yyyyMMdd")));
        adifField(QStringLiteral("TIME_ON"),  dt.toString(QStringLiteral("hhmmss")));
        adifField(QStringLiteral("BAND"),    e.value(QStringLiteral("band")).toString().toLower());
        double const freqMhz = e.value(QStringLiteral("freq_hz")).toDouble() / 1e6;
        adifField(QStringLiteral("FREQ"),    QString::number(freqMhz, 'f', 6));
        adifField(QStringLiteral("MODE"),    e.value(QStringLiteral("mode")).toString());
        adifField(QStringLiteral("GRIDSQUARE"), e.value(QStringLiteral("grid")).toString());
        QString const snr = e.value(QStringLiteral("snr_db")).toString();
        adifField(QStringLiteral("APP_DECODIUM_SNR"), snr);
        adifField(QStringLiteral("COMMENT"), e.value(QStringLiteral("message")).toString());
        out << "<EOR>\n";
        ++written;
    }
    f.close();
    decodiumBridgeLogbookLog(QStringLiteral("[Phase5.3] exportAdifFromHistory: %1 records -> %2").arg(written).arg(cleanPath));
    return written > 0;
}

