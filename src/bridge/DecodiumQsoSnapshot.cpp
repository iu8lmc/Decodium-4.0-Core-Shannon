#include "DecodiumBridgeQsoCacheInternal.h"
#include "AdifDocumentInternal.h"
#include "DecodiumLogging.hpp"
#include "helper_functions.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSet>
#include <algorithm>

using namespace decodium::adif_document;

static QVariantMap qsoMapFromAdifRecord(QMap<QString, QString> const& fields, QString const& myGrid)
{
    QVariantMap map;
    QString const call = fields.value(QStringLiteral("CALL")).trimmed().toUpper();
    QString const grid = fields.value(QStringLiteral("GRIDSQUARE")).trimmed().toUpper();
    QString const band = adifBandFromFields(fields);
    QString const mode = normalizeAdifModeForDisplay(fields);
    QString const dateTime = normalizedAdifDateTime(fields);
    QString const reportSent = fields.value(QStringLiteral("RST_SENT")).trimmed().toUpper();
    QString const reportReceived = fields.value(QStringLiteral("RST_RCVD")).trimmed().toUpper();
    QString const comment = fields.value(QStringLiteral("COMMENT")).trimmed();

    map.insert(QStringLiteral("call"), call);
    map.insert(QStringLiteral("grid"), grid);
    map.insert(QStringLiteral("band"), band);
    map.insert(QStringLiteral("mode"), mode);
    map.insert(QStringLiteral("dateTime"), dateTime);
    map.insert(QStringLiteral("reportSent"), reportSent);
    map.insert(QStringLiteral("reportReceived"), reportReceived);
    map.insert(QStringLiteral("comment"), comment);

    if (!myGrid.trimmed().isEmpty() && !grid.isEmpty()) {
        GeoDistanceInfo const geo = geo_distance(myGrid.trimmed().toUpper(), grid, 0.0);
        if (geo.km > 0) {
            map.insert(QStringLiteral("distanceKm"), geo.km);
        }
    }

    return map;
}

QVariantMap qsoStatsFromRows(QVariantList const& rows)
{
    QSet<QString> uniqueCalls;
    QSet<QString> uniqueGrids;
    int maxDistance = 0;
    QString farthestCall;

    for (QVariant const& rowValue : rows) {
        QVariantMap const row = rowValue.toMap();
        QString const call = row.value(QStringLiteral("call")).toString().trimmed().toUpper();
        QString const grid = row.value(QStringLiteral("grid")).toString().trimmed().toUpper();
        int const distanceKm = row.value(QStringLiteral("distanceKm")).toInt();
        if (!call.isEmpty()) {
            uniqueCalls.insert(call);
        }
        if (!grid.isEmpty()) {
            uniqueGrids.insert(grid);
        }
        if (distanceKm > maxDistance) {
            maxDistance = distanceKm;
            farthestCall = call;
        }
    }

    QVariantMap stats;
    stats.insert(QStringLiteral("totalQsos"), rows.size());
    stats.insert(QStringLiteral("uniqueCalls"), uniqueCalls.size());
    stats.insert(QStringLiteral("uniqueGrids"), uniqueGrids.size());
    stats.insert(QStringLiteral("maxDistance"), maxDistance);
    stats.insert(QStringLiteral("farthestCall"), farthestCall);
    return stats;
}

QString exportRecordFingerprint(ParsedAdifRecord const& record)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream << record.fields;
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QsoLogSnapshot buildQsoLogSnapshot(QString const& path, QString const& myGrid)
{
    QElapsedTimer timer;
    timer.start();

    QFileInfo const info(path);
    QsoLogSnapshot snapshot;
    snapshot.path = path;
    snapshot.modified = info.exists() ? info.lastModified() : QDateTime();
    snapshot.size = info.exists() ? info.size() : 0;

    ParsedAdifDocument const doc = loadAdifDocument(path);
    QList<QVariantMap> sortedRows;
    sortedRows.reserve(doc.records.size());
    for (ParsedAdifRecord const& record : doc.records) {
        auto row = qsoMapFromAdifRecord(record.fields, myGrid);
        row.insert(QStringLiteral("exportIndex"), sortedRows.size());
        row.insert(QStringLiteral("exportFingerprint"), exportRecordFingerprint(record));
        row.insert(QStringLiteral("exportLogPath"), path);
        sortedRows.append(row);
    }

    std::sort(sortedRows.begin(), sortedRows.end(), [] (QVariantMap const& lhs, QVariantMap const& rhs) {
        return lhs.value(QStringLiteral("dateTime")).toString()
             > rhs.value(QStringLiteral("dateTime")).toString();
    });

    snapshot.rows.reserve(sortedRows.size());
    for (QVariantMap const& row : sortedRows) {
        snapshot.rows.append(row);
    }
    snapshot.stats = qsoStatsFromRows(snapshot.rows);

    if (timer.elapsed() > 250) {
        DIAG_INFO(QStringLiteral("QSO log cache built: records=%1 elapsed=%2ms path=%3")
                      .arg(snapshot.rows.size())
                      .arg(timer.elapsed())
                      .arg(path));
    }
    return snapshot;
}

