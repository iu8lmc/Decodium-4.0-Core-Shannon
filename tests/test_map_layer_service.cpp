#include <QtTest>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUdpSocket>
#include <QUuid>

#include "src/services/MapIntelligenceService.h"
#include "src/services/MapBaseMapService.h"
#include "src/services/MapExternalOverlayService.h"
#include "src/services/MapLayerModel.h"
#include "src/services/MapOperationsService.h"
#include "src/services/MapPskFeedService.h"

namespace {

QByteArray field(const QByteArray& name, const QByteArray& value)
{
    return "<" + name + ":" + QByteArray::number(value.size()) + ">" + value;
}

QByteArray record(const QByteArray& call,
                  const QByteArray& grid,
                  const QByteArray& band,
                  const QByteArray& frequency,
                  const QByteArray& mode,
                  const QByteArray& submode,
                  const QByteArray& confirmationField = {},
                  const QByteArray& confirmationValue = {},
                  const QByteArray& country = {},
                  const QByteArray& continent = {},
                  const QByteArray& cqZone = {},
                  const QByteArray& ituZone = {},
                  const QByteArray& state = {},
                  const QByteArray& iota = {},
                  const QByteArray& timeOn = "120000",
                  const QByteArray& pota = {})
{
    QByteArray result = field("CALL", call)
        + field("GRIDSQUARE", grid)
        + field("QSO_DATE", "20260728")
        + field("TIME_ON", timeOn);
    if (!band.isEmpty()) {
        result += field("BAND", band);
    }
    if (!frequency.isEmpty()) {
        result += field("FREQ", frequency);
    }
    result += field("MODE", mode);
    if (!submode.isEmpty()) {
        result += field("SUBMODE", submode);
    }
    if (!confirmationField.isEmpty()) {
        result += field(confirmationField, confirmationValue);
    }
    if (!country.isEmpty()) result += field("COUNTRY", country);
    if (!continent.isEmpty()) result += field("CONT", continent);
    if (!cqZone.isEmpty()) result += field("CQZ", cqZone);
    if (!ituZone.isEmpty()) result += field("ITUZ", ituZone);
    if (!state.isEmpty()) result += field("STATE", state);
    if (!iota.isEmpty()) result += field("IOTA", iota);
    if (!pota.isEmpty()) result += field("POTA_REF", pota);
    return result + "<EOR>\n";
}

bool databaseHasIndex(const QString& databasePath, const QString& indexName)
{
    QString const connectionName =
        QStringLiteral("map_test_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool found = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                          connectionName);
        database.setDatabaseName(databasePath);
        if (database.open()) {
            QSqlQuery query(database);
            query.prepare(QStringLiteral(
                "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name=:name"));
            query.bindValue(QStringLiteral(":name"), indexName);
            found = query.exec() && query.next() && query.value(0).toInt() == 1;
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return found;
}

} // namespace

class TestMapIntelligenceService final : public QObject
{
    Q_OBJECT

private slots:
    void satelliteFieldsSurviveCanonicalQsoStore()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
        QString const adifPath = tempDir.filePath(QStringLiteral("satellite.adi"));
        QString const databasePath = tempDir.filePath(QStringLiteral("satellite.sqlite"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        file.write(field("CALL", "SAT1")
                   + field("GRIDSQUARE", "JN70aa")
                   + field("QSO_DATE", "20260728")
                   + field("TIME_ON", "120000")
                   + field("BAND", "70cm")
                   + field("MODE", "SSB")
                   + field("FREQ", "435.800")
                   + field("FREQ_RX", "435.900")
                   + field("PROP_MODE", "SAT")
                   + field("SAT_NAME", "AO-7")
                   + field("SAT_MODE", "U/V")
                   + "<EOR>\n");
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(
            !service.statistics().value(QStringLiteral("satellites")).toList().isEmpty(),
            5000);

        QString const connectionName =
            QStringLiteral("satellite_fields_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        {
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral(
                "SELECT satellite, sat_mode, freq_rx_mhz FROM map_qso LIMIT 1")));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toString(), QStringLiteral("AO-7"));
            QCOMPARE(query.value(1).toString(), QStringLiteral("U/V"));
            QCOMPARE(query.value(2).toDouble(), 435.9);
        }
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
    }

    void importsVuccGridsIntoCoverageAwardsHistoryAndSearch()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
        QString const adifPath = tempDir.filePath(QStringLiteral("vucc.adi"));
        QString const databasePath = tempDir.filePath(QStringLiteral("vucc.sqlite"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        file.write(field("CALL", "VUCC1")
                   + field("GRIDSQUARE", "FN20aa")
                   + field("VUCC_GRIDS", "FN20,FN21aa,FN22,INVALID,FN21aa")
                   + field("QSO_DATE", "20260728")
                   + field("TIME_ON", "120000")
                   + field("BAND", "6m")
                   + field("MODE", "FT8")
                   + field("QSL_RCVD", "Y")
                   + "<EOR>\n");
        file.write(field("CALL", "VUCC2")
                   + field("VUCC_GRIDS", "JN70,JN71aa")
                   + field("QSO_DATE", "20260728")
                   + field("TIME_ON", "120100")
                   + field("BAND", "2m")
                   + field("MODE", "FT8")
                   + "<EOR>\n");
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            service.statistics().value(QStringLiteral("grids")).toInt(), 5, 5000);

        QSet<QString> coverageGrids;
        for (QVariant const& value : service.coverageCells()) {
            coverageGrids.insert(value.toMap().value(QStringLiteral("grid")).toString());
        }
        for (QString const& grid : {QStringLiteral("FN20"), QStringLiteral("FN21"),
                                    QStringLiteral("FN22"), QStringLiteral("JN70"),
                                    QStringLiteral("JN71")}) {
            QVERIFY2(coverageGrids.contains(grid), qPrintable(grid));
        }

        auto awardByLabel = [&service](QString const& label) {
            for (QVariant const& value : service.awards()) {
                QVariantMap const award = value.toMap();
                if (award.value(QStringLiteral("label")).toString() == label) {
                    return award;
                }
            }
            return QVariantMap {};
        };
        QTRY_COMPARE_WITH_TIMEOUT(
            awardByLabel(QStringLiteral("Maidenhead"))
                .value(QStringLiteral("worked")).toInt(), 5, 5000);
        QCOMPARE(awardByLabel(QStringLiteral("Maidenhead"))
                     .value(QStringLiteral("confirmed")).toInt(), 3);

        QString vucc6mProgram;
        for (QString const& program : service.availableAwardPrograms()) {
            if (program.contains(QStringLiteral("Century Club 6m"),
                                 Qt::CaseInsensitive)) {
                vucc6mProgram = program;
                break;
            }
        }
        QVERIFY(!vucc6mProgram.isEmpty());
        service.setActiveAwardProgram(vucc6mProgram);
        QTRY_COMPARE_WITH_TIMEOUT(
            awardByLabel(vucc6mProgram).value(QStringLiteral("worked")).toInt(),
            3, 5000);

        service.selectGrid(QStringLiteral("FN21"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.gridDetailsLoading(), 5000);
        QCOMPARE(service.selectedGridQsos().size(), 1);
        QVariantMap const history = service.selectedGridQsos().first().toMap();
        QCOMPARE(history.value(QStringLiteral("grid")).toString(),
                 QStringLiteral("FN20AA"));
        QCOMPARE(history.value(QStringLiteral("matchedGrid")).toString(),
                 QStringLiteral("FN21AA"));
        QVERIFY(!history.value(QStringLiteral("matchedGridIsPrimary")).toBool());
        QVERIFY(history.value(QStringLiteral("vuccGrids")).toStringList()
                    .contains(QStringLiteral("FN22")));

        service.selectGrid(QStringLiteral("JN70"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.gridDetailsLoading(), 5000);
        QCOMPARE(service.selectedGridQsos().first().toMap()
                     .value(QStringLiteral("grid")).toString(),
                 QStringLiteral("JN70"));

        auto* operations =
            qobject_cast<MapOperationsService*>(service.operationsService());
        QVERIFY(operations);
        QTRY_COMPARE_WITH_TIMEOUT(
            operations->scorecard().value(QStringLiteral("grids")).toInt(), 5, 5000);
        operations->setLogbookSearch(QStringLiteral("JN71AA"));
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookTotal(), 1, 5000);
        QVariantMap const searchRow = operations->logbookRows().first().toMap();
        QCOMPARE(searchRow.value(QStringLiteral("grid")).toString(),
                 QStringLiteral("JN70"));
        QVERIFY(searchRow.value(QStringLiteral("vuccGrids")).toStringList()
                    .contains(QStringLiteral("JN71AA")));

        QVERIFY(databaseHasIndex(databasePath,
                                 QStringLiteral("idx_map_qso_grid_grid4")));
        QString const connectionName = QStringLiteral("vucc_grid_count_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());
            QSqlQuery count(database);
            QVERIFY(count.exec(QStringLiteral("SELECT COUNT(*) FROM map_qso_grid")));
            QVERIFY(count.next());
            QCOMPARE(count.value(0).toInt(), 5);
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }

    void persistsIndexesFiltersAndLiveRoster()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("logbook.adi"));
        QString const databasePath =
            tempDir.filePath(QStringLiteral("map-intelligence.sqlite"));
        QFile iotaCatalog(tempDir.filePath(QStringLiteral("iota_groups.json")));
        QVERIFY(iotaCatalog.open(QIODevice::WriteOnly));
        iotaCatalog.write(R"json([
            {
                "refno": "EU-005",
                "name": "Great Britain",
                "dxcc_num": "223",
                "latitude_max": "59.00",
                "latitude_min": "49.75",
                "longitude_max": "2.00",
                "longitude_min": "-8.25",
                "pc_credited": "41.2",
                "comment": ""
            },
            {
                "refno": "OC-001",
                "name": "Australia",
                "dxcc_num": "150",
                "latitude_max": "-10.00",
                "latitude_min": "-39.25",
                "longitude_max": "153.75",
                "longitude_min": "113.00",
                "pc_credited": "18.0",
                "comment": ""
            }
        ])json");
        iotaCatalog.close();
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        file.write(record("TEST1", "FN20aa", "20M", "", "FT8", "", {}, {},
                          "United States", "NA", "5", "8", "PA", "EU-005"));
        file.write(record("TEST2", "FN20bb", "20m", "", "FT8", "",
                          "LOTW_QSL_RCVD", "Y",
                          "United States", "NA", "5", "8", "NJ"));
        file.write(record("TEST3", "JN70", "40m", "", "MFSK", "FT4",
                          "QSL_RCVD", "Y",
                          "Italy", "EU", "15", "28"));
        file.write(record("TEST4", "JO21", "", "14.074", "MFSK", "FT4",
                          {}, {}, "Netherlands", "EU", "14", "27"));
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        QVERIFY(!service.coveragePushPinsEnabled());
        QVERIFY(!service.timeZoneOverlayEnabled());
        service.setCoveragePushPinsEnabled(true);
        service.setTimeZoneOverlayEnabled(true);
        service.reloadFromAdif(adifPath);
        QTRY_VERIFY_WITH_TIMEOUT(!service.loading(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 4, 5000);

        auto* layerModel = qobject_cast<MapLayerModel*>(service.layerModel());
        QVERIFY(layerModel);
        auto* baseMap = qobject_cast<MapBaseMapService*>(service.baseMapService());
        QVERIFY(baseMap);
        auto* externalOverlays =
            qobject_cast<MapExternalOverlayService*>(service.externalOverlayService());
        QVERIFY(externalOverlays);
        QCOMPARE(externalOverlays->temporalLegend().size(), 5);
        bool hasNvisStatus = false;
        for (QVariant const& statusValue : externalOverlays->providerStatus()) {
            if (statusValue.toMap().value(QStringLiteral("layerId")).toString()
                == QStringLiteral("nvis")) {
                hasNvisStatus = true;
                QCOMPARE(statusValue.toMap().value(QStringLiteral("derivedFrom"))
                             .toString(), QStringLiteral("fof2"));
            }
        }
        QVERIFY(hasNvisStatus);
        auto* pskFeed = qobject_cast<MapPskFeedService*>(service.pskFeedService());
        QVERIFY(pskFeed);
        QVERIFY(!layerModel->layerEnabled(QStringLiteral("offline")));
        QVERIFY(!baseMap->offlineMode());
        QVERIFY(!baseMap->baseMapImage().isNull());
        QVERIFY(baseMap->availableProviders().contains(
            QStringLiteral("OpenStreetMap")));
        QVERIFY(baseMap->availableProviders().contains(
            QStringLiteral("OpenTopoMap")));
        QVERIFY(baseMap->availableProviders().contains(
            QStringLiteral("GEBCO bathymetry")));
        QVERIFY(baseMap->availableStyles().contains(QStringLiteral("Day")));
        QVERIFY(baseMap->availableStyles().contains(QStringLiteral("Night")));

        layerModel->setLayerEnabled(QStringLiteral("offline"), true);
        QTRY_VERIFY_WITH_TIMEOUT(baseMap->offlineMode(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(externalOverlays->offlineMode(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(pskFeed->offlineMode(), 1000);
        QCOMPARE(baseMap->attribution(), QStringLiteral("Decodium Atlas (local)"));
        auto* offlineOperations =
            qobject_cast<MapOperationsService*>(service.operationsService());
        QVERIFY(offlineOperations);
        QTRY_VERIFY_WITH_TIMEOUT(offlineOperations->offlineMode(), 1000);

        QString const offlineRasterPath = tempDir.filePath(QStringLiteral("world.png"));
        QImage offlineRaster(128, 64, QImage::Format_ARGB32_Premultiplied);
        offlineRaster.fill(QColor(QStringLiteral("#223b58")));
        QVERIFY(offlineRaster.save(offlineRasterPath, "PNG"));
        baseMap->importOfflinePack(offlineRasterPath);
        QTRY_VERIFY_WITH_TIMEOUT(baseMap->offlinePackAvailable(), 3000);
        QVERIFY(baseMap->offlinePackStatus().contains(QStringLiteral("verify source licence")));
        QTRY_COMPARE_WITH_TIMEOUT(
            baseMap->attribution(),
            QStringLiteral("User-provided offline raster pack (verify source licence)"),
            3000);
        baseMap->clearOfflinePack();
        QTRY_VERIFY_WITH_TIMEOUT(!baseMap->offlinePackAvailable(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(baseMap->attribution(),
                                  QStringLiteral("Decodium Atlas (local)"), 3000);
        baseMap->setProvider(QStringLiteral("NASA GIBS satellite"));
        QCOMPARE(baseMap->provider(), QStringLiteral("NASA GIBS satellite"));
        QVERIFY(baseMap->offlineMode());
        QCOMPARE(baseMap->attribution(), QStringLiteral("Decodium Atlas (local)"));
        baseMap->setProvider(QStringLiteral("MapTiler satellite"));
        baseMap->setMapTilerApiKey(QString());
        QVERIFY(baseMap->apiKeyRequired());
        baseMap->setStyle(QStringLiteral("Night"));
        QTRY_VERIFY_WITH_TIMEOUT(!baseMap->baseMapImage().isNull(), 1000);
        QCOMPARE(baseMap->style(), QStringLiteral("Night"));
        baseMap->setProvider(QStringLiteral("Decodium Atlas"));
        baseMap->setStyle(QStringLiteral("Day"));

        layerModel->setLayerEnabled(QStringLiteral("offline"), false);
        QTRY_VERIFY_WITH_TIMEOUT(!baseMap->offlineMode(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(!externalOverlays->offlineMode(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(!pskFeed->offlineMode(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(!offlineOperations->offlineMode(), 1000);
        auto* operations =
            qobject_cast<MapOperationsService*>(service.operationsService());
        QVERIFY(operations);
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookTotal(), 4, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookRows().size(), 4, 5000);
        QVERIFY(operations->availableProjections().contains(
            QStringLiteral("Mercator")));
        QVERIFY(operations->availableProjections().contains(
            QStringLiteral("Miller")));
        QVERIFY(operations->availableProjections().contains(
            QStringLiteral("Azimuthal Equidistant")));
        QCOMPARE(operations->dataViewMode(), QStringLiteral("Live + Logbook"));

        operations->setMapProjection(QStringLiteral("Mercator"));
        QCOMPARE(operations->mapProjection(), QStringLiteral("Mercator"));
        operations->setDataViewMode(QStringLiteral("Logbook"));
        QCOMPARE(operations->dataViewMode(), QStringLiteral("Logbook"));
        QVERIFY(layerModel->layerEnabled(QStringLiteral("live")));
        QVERIFY(layerModel->layerEnabled(QStringLiteral("worked")));
        QVERIFY(layerModel->layerEnabled(QStringLiteral("confirmed")));
        operations->setDataViewMode(QStringLiteral("Live + Logbook"));

        MapIntelligenceService persistedService(nullptr, databasePath);
        QVERIFY(persistedService.coveragePushPinsEnabled());
        QVERIFY(persistedService.timeZoneOverlayEnabled());

        operations->setLogbookSearch(QStringLiteral("TEST2"));
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookTotal(), 1, 5000);
        QCOMPARE(operations->logbookRows().first().toMap()
                     .value(QStringLiteral("call")).toString(),
                 QStringLiteral("TEST2"));
        operations->setLogbookSearch(QString());
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookTotal(), 4, 5000);

        QTRY_VERIFY_WITH_TIMEOUT(!operations->awardProgression().isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!operations->topStatistics().isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!operations->profileStatistics().isEmpty(), 5000);
        QCOMPARE(operations->periodComparison().size(), 4);
        operations->drillDownStatistics(QStringLiteral("Band"), QStringLiteral("20m"));
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookTotal(), 3, 5000);
        QCOMPARE(operations->statisticsDrilldown(), QStringLiteral("Band: 20m"));
        operations->setLogbookBand(QStringLiteral("All"));
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookTotal(), 4, 5000);

        QString const statisticsJsonPath =
            tempDir.filePath(QStringLiteral("statistics.json"));
        QVERIFY(operations->exportStatistics(statisticsJsonPath, QStringLiteral("JSON")));
        QFile statisticsJson(statisticsJsonPath);
        QVERIFY(statisticsJson.open(QIODevice::ReadOnly));
        QJsonParseError statisticsError;
        QJsonDocument const statisticsDocument =
            QJsonDocument::fromJson(statisticsJson.readAll(), &statisticsError);
        QCOMPARE(statisticsError.error, QJsonParseError::NoError);
        QCOMPARE(statisticsDocument.object().value(QStringLiteral("type")).toString(),
                 QStringLiteral("decodium-logbook-statistics"));
        QVERIFY(statisticsDocument.object().value(QStringLiteral("awardProgression"))
                    .toArray().size() >= 1);
        QVERIFY(statisticsDocument.object().value(QStringLiteral("topStatistics"))
                    .toArray().size() >= 5);
        statisticsJson.close();

        QString const statisticsCsvPath =
            tempDir.filePath(QStringLiteral("statistics.csv"));
        QVERIFY(operations->exportStatistics(statisticsCsvPath, QStringLiteral("CSV")));
        QFile statisticsCsv(statisticsCsvPath);
        QVERIFY(statisticsCsv.open(QIODevice::ReadOnly));
        QByteArray const statisticsCsvData = statisticsCsv.readAll();
        QVERIFY(statisticsCsvData.startsWith("Section,Period/Group"));
        QVERIFY(statisticsCsvData.contains("AwardProgression"));
        statisticsCsv.close();

        QString const csvPath = tempDir.filePath(QStringLiteral("filtered.csv"));
        QVERIFY(operations->exportLogbook(csvPath, QStringLiteral("CSV")));
        QTRY_VERIFY_WITH_TIMEOUT(!operations->exportInProgress(), 5000);
        QFile csv(csvPath);
        QVERIFY(csv.open(QIODevice::ReadOnly));
        QByteArray const csvData = csv.readAll();
        QVERIFY(csvData.startsWith("Date,Time,Call,Grid"));
        QVERIFY(csvData.contains("\"TEST2\""));
        csv.close();

        QString const adifExport =
            tempDir.filePath(QStringLiteral("filtered.adi"));
        QVERIFY(operations->exportLogbook(adifExport, QStringLiteral("ADIF")));
        QTRY_VERIFY_WITH_TIMEOUT(!operations->exportInProgress(), 5000);
        QFile adif(adifExport);
        QVERIFY(adif.open(QIODevice::ReadOnly));
        QByteArray const adifData = adif.readAll();
        QVERIFY(adifData.contains("<ADIF_VER:5>3.1.4"));
        QVERIFY(adifData.contains("<CALL:5>TEST2"));
        adif.close();

        operations->saveMapPreset(QStringLiteral("Unit Test"));
        QVERIFY(operations->mapPresets().contains(QStringLiteral("Unit Test")));
        operations->setMapProjection(QStringLiteral("Miller"));
        operations->applyMapPreset(QStringLiteral("Unit Test"));
        QCOMPARE(operations->mapProjection(), QStringLiteral("Mercator"));
        operations->deleteMapPreset(QStringLiteral("Unit Test"));
        QVERIFY(!operations->mapPresets().contains(QStringLiteral("Unit Test")));

        QUdpSocket rotatorReceiver;
        QVERIFY(rotatorReceiver.bind(QHostAddress::LocalHost, 0));
        operations->setRotatorHost(QStringLiteral("127.0.0.1"));
        operations->setRotatorPort(rotatorReceiver.localPort());
        operations->setRotatorEnabled(true);
        operations->aimRotator(123.6);
        QTRY_VERIFY_WITH_TIMEOUT(rotatorReceiver.hasPendingDatagrams(), 2000);
        QByteArray rotatorPayload;
        rotatorPayload.resize(
            static_cast<int>(rotatorReceiver.pendingDatagramSize()));
        rotatorReceiver.readDatagram(rotatorPayload.data(),
                                     rotatorPayload.size());
        QCOMPARE(rotatorPayload,
                 QByteArray("<PST><AZIMUTH>124</AZIMUTH></PST>"));

        QCOMPARE(service.workedGridCount(), 3);
        QCOMPARE(service.confirmedGridCount(), 2);
        QVERIFY(service.availableBands().contains(QStringLiteral("20m")));
        QVERIFY(service.availableBands().contains(QStringLiteral("40m")));
        QVERIFY(service.availableModes().contains(QStringLiteral("FT8")));
        QVERIFY(service.availableModes().contains(QStringLiteral("FT4")));
        QVERIFY(databaseHasIndex(databasePath, QStringLiteral("idx_map_qso_band_mode")));
        QVERIFY(databaseHasIndex(databasePath, QStringLiteral("idx_map_spot_observed")));
        QVERIFY(databaseHasIndex(databasePath, QStringLiteral("idx_map_qso_grid6_status")));
        QVERIFY(databaseHasIndex(databasePath, QStringLiteral("idx_map_spot_activity_time")));

        QCOMPARE(layerModel->rowCount(), 24);
        auto hasLayer = [layerModel](QString const& id) {
            for (int row = 0; row < layerModel->rowCount(); ++row) {
                if (layerModel->data(
                        layerModel->index(row, 0),
                        MapLayerModel::LayerIdRole).toString() == id) {
                    return true;
                }
            }
            return false;
        };
        for (QString const& layerId : {
                 QStringLiteral("pota"), QStringLiteral("states"),
                 QStringLiteral("counties"), QStringLiteral("iota"),
                 QStringLiteral("wpx"), QStringLiteral("earthquakes"),
                 QStringLiteral("wildfires"), QStringLiteral("muf"),
                 QStringLiteral("fof2"), QStringLiteral("nvis"),
                 QStringLiteral("es"), QStringLiteral("aurora")}) {
            QVERIFY2(hasLayer(layerId),
                     qPrintable(QStringLiteral("Missing map layer: %1")
                                    .arg(layerId)));
        }
        QVERIFY(layerModel->layerEnabled(QStringLiteral("active")));
        QVERIFY(layerModel->layerEnabled(QStringLiteral("missing")));
        QVERIFY(layerModel->layerEnabled(QStringLiteral("psk")));
        QVERIFY(!service.awards().isEmpty());
        QVERIFY(service.availableContinents().contains(QStringLiteral("EU")));
        QVERIFY(service.availableDxcc().contains(QStringLiteral("Italy")));
        QVERIFY(service.availableSources().contains(QStringLiteral("ADIF")));
        QCOMPARE(service.gridPrecision(), 4);
        QCOMPARE(service.liveDecayMinutes(), 15);
        QVERIFY(service.splitGridEnabled());
        QCOMPARE(service.pskDisplayMode(), QStringLiteral("Overlay"));
        QCOMPARE(service.pskOpacityPercent(), 65);
        QCOMPARE(service.callLookupProvider(), QStringLiteral("QRZ"));
        QVERIFY(service.alertNewGridEnabled());
        QVERIFY(service.alertNewDxccEnabled());
        QVERIFY(service.alertCqEnabled());
        QVERIFY(!service.statistics().isEmpty());
        QCOMPARE(service.statistics().value(QStringLiteral("qso")).toInt(), 4);
        QCOMPARE(service.statistics().value(QStringLiteral("confirmed")).toInt(), 2);
        layerModel->setLayerEnabled(QStringLiteral("iota"), true);
        auto layerCount = [layerModel](QString const& id) {
            for (int row = 0; row < layerModel->rowCount(); ++row) {
                QModelIndex const index = layerModel->index(row, 0);
                if (layerModel->data(index, MapLayerModel::LayerIdRole).toString()
                    == id) {
                    return layerModel->data(index, MapLayerModel::CountRole).toInt();
                }
            }
            return -1;
        };
        QTRY_COMPARE_WITH_TIMEOUT(layerCount(QStringLiteral("iota")), 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(operations->operationalMarkers().size(), 2, 5000);
        bool hasIotaMarker = false;
        bool hasCatalogOnlyMarker = false;
        for (QVariant const& markerValue : operations->operationalMarkers()) {
            QVariantMap const marker = markerValue.toMap();
            if (marker.value(QStringLiteral("type")).toString()
                == QStringLiteral("IOTA")
                && marker.value(QStringLiteral("reference")).toString()
                       == QStringLiteral("EU-005")) {
                QVERIFY(marker.value(QStringLiteral("worked")).toBool());
                hasIotaMarker = true;
            }
            if (marker.value(QStringLiteral("type")).toString()
                    == QStringLiteral("IOTA")
                && marker.value(QStringLiteral("reference")).toString()
                       == QStringLiteral("OC-001")) {
                QVERIFY(marker.value(QStringLiteral("catalog")).toBool());
                QVERIFY(!marker.value(QStringLiteral("worked")).toBool());
                hasCatalogOnlyMarker = true;
            }
        }
        QVERIFY(hasIotaMarker);
        QVERIFY(hasCatalogOnlyMarker);

        service.setGridPrecision(6);
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 4, 5000);
        service.setGridPrecision(4);
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 3, 5000);

        service.setBandFilter(QStringLiteral("20m"));
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 2, 5000);
        QCOMPARE(service.confirmedGridCount(), 1);

        service.setModeFilter(QStringLiteral("FT8"));
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 1, 5000);
        QCOMPARE(service.confirmedGridCount(), 1);

        service.setWorkedLayerEnabled(false);
        service.setConfirmedLayerEnabled(true);
        QCOMPARE(service.coverageCells().size(), 1);
        QVERIFY(service.coverageCells().first().toMap()
                    .value(QStringLiteral("confirmed")).toBool());

        service.setBandFilter(QStringLiteral("All"));
        service.setModeFilter(QStringLiteral("All"));
        service.setContinentFilter(QStringLiteral("EU"));
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 2, 5000);
        service.setContinentFilter(QStringLiteral("All"));
        service.setDxccFilter(QStringLiteral("Italy"));
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 1, 5000);
        service.setDxccFilter(QStringLiteral("All"));
        service.setWorkedLayerEnabled(true);
        service.appendAdifRecord(record("TEST5", "KM71", "20m", "", "FT8", ""));
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 5, 5000);
        QCOMPARE(service.workedGridCount(), 4);
        QCOMPARE(service.confirmedGridCount(), 2);

        QVariantMap decode;
        decode.insert(QStringLiteral("time"), QStringLiteral("120001"));
        decode.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
        decode.insert(QStringLiteral("message"), QStringLiteral("CQ LIVE1 JN70"));
        decode.insert(QStringLiteral("fromCall"), QStringLiteral("LIVE1"));
        decode.insert(QStringLiteral("dxGrid"), QStringLiteral("JN70"));
        decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        decode.insert(QStringLiteral("db"), QStringLiteral("-17"));
        decode.insert(QStringLiteral("freq"), 1500);
        decode.insert(QStringLiteral("dxcc"), QStringLiteral("Italy"));
        decode.insert(QStringLiteral("continent"), QStringLiteral("EU"));
        decode.insert(QStringLiteral("cqZone"), 15);
        decode.insert(QStringLiteral("ituZone"), 28);
        decode.insert(QStringLiteral("isCQ"), true);
        decode.insert(QStringLiteral("distanceKm"), 1234.0);
        service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));

        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        QCOMPARE(service.activeGridCount(), 1);
        QCOMPARE(service.missingGridCount(), 0);
        QCOMPARE(service.roster().size(), 1);
        QVariantMap const liveRow = service.roster().first().toMap();
        QCOMPARE(liveRow.value(QStringLiteral("call")).toString(), QStringLiteral("LIVE1"));
        QCOMPARE(liveRow.value(QStringLiteral("grid")).toString(), QStringLiteral("JN70"));
        QCOMPARE(liveRow.value(QStringLiteral("frequencyHz")).toLongLong(), 14075500);
        QCOMPARE(liveRow.value(QStringLiteral("dxcc")).toString(), QStringLiteral("Italy"));
        QCOMPARE(liveRow.value(QStringLiteral("distanceKm")).toDouble(), 1234.0);
        bool hasLiveOpacity = false;
        for (QVariant const& cellValue : service.coverageCells()) {
            QVariantMap const cell = cellValue.toMap();
            if (cell.value(QStringLiteral("grid")).toString() == QStringLiteral("JN70")) {
                hasLiveOpacity =
                    cell.value(QStringLiteral("liveOpacity")).toDouble() > 0.0;
                QCOMPARE(cell.value(QStringLiteral("liveStatus")).toString(),
                         QStringLiteral("CQ"));
            }
        }
        QVERIFY(hasLiveOpacity);

        QVERIFY(service.availableSources().contains(QStringLiteral("ADIF")));
        QVERIFY(service.availableSources().contains(QStringLiteral("decoder")));

        service.setSourceFilter(QStringLiteral("ADIF"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 0, 5000);
        QCOMPARE(service.activeGridCount(), 0);
        QCOMPARE(service.workedGridCount(), 4);

        service.setSourceFilter(QStringLiteral("decoder"));
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 0, 5000);
        QCOMPARE(service.liveSpotCount(), 1);
        QCOMPARE(service.activeGridCount(), 1);

        service.setBandFilter(QStringLiteral("40m"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 0, 5000);
        service.setBandFilter(QStringLiteral("20m"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        service.setModeFilter(QStringLiteral("FT4"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 0, 5000);
        service.setModeFilter(QStringLiteral("FT8"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        service.setBandFilter(QStringLiteral("All"));
        service.setModeFilter(QStringLiteral("All"));
        service.setSourceFilter(QStringLiteral("All"));
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 4, 5000);
        QCOMPARE(service.liveSpotCount(), 1);

        // Active-grid coverage must remain drawable when the generic live
        // marker layer is hidden. The two controls represent independent
        // visual layers in the map UI.
        service.setLiveLayerEnabled(false);
        service.setActiveLayerEnabled(true);
        QVariantMap activeCoverage;
        for (QVariant const& cellValue : service.coverageCells()) {
            QVariantMap const cell = cellValue.toMap();
            if (cell.value(QStringLiteral("grid")).toString() == QStringLiteral("JN70")) {
                activeCoverage = cell;
                break;
            }
        }
        QVERIFY(!activeCoverage.isEmpty());
        QVERIFY(activeCoverage.value(QStringLiteral("active")).toBool());
        service.setLiveLayerEnabled(true);

        service.setCqOnly(true);
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        service.setRosterSort(QStringLiteral("Distance"));
        service.setRosterSortDescending(false);
        QTRY_COMPARE_WITH_TIMEOUT(service.roster().size(), 1, 5000);
        service.setRosterTextMode(QStringLiteral("Only"));
        service.setRosterTextFilter(QStringLiteral("LIVE1"));
        QTRY_COMPARE_WITH_TIMEOUT(service.roster().size(), 1, 5000);
        service.setRosterTextFilter(QStringLiteral("NO-MATCH"));
        QTRY_COMPARE_WITH_TIMEOUT(service.roster().size(), 0, 5000);
        service.setRosterTextMode(QStringLiteral("No filter"));
        QTRY_COMPARE_WITH_TIMEOUT(service.roster().size(), 1, 5000);

        QVariantMap psk;
        psk.insert(QStringLiteral("call"), QStringLiteral("PSK1"));
        psk.insert(QStringLiteral("grid"), QStringLiteral("KM72"));
        psk.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        psk.insert(QStringLiteral("frequency"), 14076000);
        psk.insert(QStringLiteral("snr"), -21);
        psk.insert(QStringLiteral("country"), QStringLiteral("Israel"));
        psk.insert(QStringLiteral("continent"), QStringLiteral("AS"));
        psk.insert(QStringLiteral("cqZone"), 20);
        psk.insert(QStringLiteral("ituZone"), 39);
        psk.insert(QStringLiteral("distanceKm"), 2100.0);
        service.ingestPskSpots({psk}, QStringLiteral("HOME"), QStringLiteral("JM75"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        service.setCqOnly(false);
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 2, 5000);
        QVERIFY(service.availableSources().contains(QStringLiteral("psk")));
        QCOMPARE(service.activeGridCount(), 2);
        QCOMPARE(service.missingGridCount(), 1);
        QVERIFY(service.unreadAlertCount() > 0);
        QVERIFY(!service.alerts().isEmpty());
        service.setPskOpacityPercent(40);
        service.setPskDisplayMode(QStringLiteral("Replace"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.activeGridCount(), 1, 5000);
        QVariantMap pskCoverage;
        for (QVariant const& value : service.coverageCells()) {
            QVariantMap const cell = value.toMap();
            if (cell.value(QStringLiteral("grid")).toString()
                == QStringLiteral("KM72")) {
                pskCoverage = cell;
                break;
            }
        }
        QVERIFY(!pskCoverage.isEmpty());
        QCOMPARE(pskCoverage.value(QStringLiteral("liveStatus")).toString(),
                 QStringLiteral("PSK"));
        QVERIFY(pskCoverage.value(QStringLiteral("liveOpacity")).toDouble() <= 0.4);
        service.setPskDisplayMode(QStringLiteral("Overlay"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 2, 5000);
        service.setCallLookupProvider(QStringLiteral("HamQTH"));
        QCOMPARE(service.callLookupProvider(), QStringLiteral("HamQTH"));

        service.selectGrid(QStringLiteral("JN70"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.gridDetailsLoading(), 5000);
        QCOMPARE(service.selectedGrid(), QStringLiteral("JN70"));
        QCOMPARE(service.selectedGridSummary()
                     .value(QStringLiteral("workedCount")).toInt(), 1);
        QCOMPARE(service.selectedGridSummary()
                     .value(QStringLiteral("confirmedCount")).toInt(), 1);
        QCOMPARE(service.selectedGridSummary()
                     .value(QStringLiteral("activeCount")).toInt(), 1);
        QCOMPARE(service.selectedGridLive().size(), 1);
        QCOMPARE(service.selectedGridLive().first().toMap()
                     .value(QStringLiteral("call")).toString(),
                 QStringLiteral("LIVE1"));
        QCOMPARE(service.selectedGridLive().first().toMap()
                     .value(QStringLiteral("activityType")).toString(),
                 QStringLiteral("CQ"));
        QCOMPARE(service.selectedGridLive().first().toMap()
                     .value(QStringLiteral("gridEvidence")).toString(),
                 QStringLiteral("TX locator in decoded message"));
        QCOMPARE(service.selectedGridQsos().size(), 1);
        QCOMPARE(service.selectedGridQsos().first().toMap()
                     .value(QStringLiteral("call")).toString(),
                 QStringLiteral("TEST3"));

        QVariantMap repeatedGridDecode;
        repeatedGridDecode.insert(QStringLiteral("time"), QStringLiteral("120002"));
        repeatedGridDecode.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
        repeatedGridDecode.insert(QStringLiteral("message"),
                                  QStringLiteral("R1EMOTE TESTER KN37"));
        repeatedGridDecode.insert(QStringLiteral("fromCall"), QStringLiteral("R1EMOTE"));
        repeatedGridDecode.insert(QStringLiteral("dxGrid"), QStringLiteral("KN37"));
        repeatedGridDecode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        repeatedGridDecode.insert(QStringLiteral("db"), QStringLiteral("-10"));
        repeatedGridDecode.insert(QStringLiteral("freq"), 1600);
        service.ingestDecodeEntry(repeatedGridDecode, 14074000, QStringLiteral("20m"));
        repeatedGridDecode.insert(QStringLiteral("time"), QStringLiteral("120003"));
        repeatedGridDecode.insert(QStringLiteral("timestamp"),
                                  QDateTime::currentMSecsSinceEpoch() + 1000);
        service.ingestDecodeEntry(repeatedGridDecode, 14074000, QStringLiteral("20m"));

        QVariantMap staleGridDecode;
        staleGridDecode.insert(QStringLiteral("time"), QStringLiteral("120004"));
        staleGridDecode.insert(QStringLiteral("timestamp"),
                               QDateTime::currentMSecsSinceEpoch() + 2000);
        staleGridDecode.insert(QStringLiteral("message"),
                               QStringLiteral("N0GRID TESTER -10"));
        staleGridDecode.insert(QStringLiteral("fromCall"), QStringLiteral("N0GRID"));
        staleGridDecode.insert(QStringLiteral("dxGrid"), QStringLiteral("KN37"));
        staleGridDecode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        staleGridDecode.insert(QStringLiteral("db"), QStringLiteral("-10"));
        staleGridDecode.insert(QStringLiteral("freq"), 1700);
        service.ingestDecodeEntry(staleGridDecode, 14074000, QStringLiteral("20m"));

        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 5, 5000);
        service.selectGrid(QStringLiteral("KN37"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.gridDetailsLoading(), 5000);
        QCOMPARE(service.selectedGridSummary()
                     .value(QStringLiteral("activeCount")).toInt(), 1);
        QCOMPARE(service.selectedGridLive().size(), 1);
        QCOMPARE(service.selectedGridLive().first().toMap()
                     .value(QStringLiteral("call")).toString(),
                 QStringLiteral("R1EMOTE"));

        service.markAlertsRead();
        QTRY_COMPARE_WITH_TIMEOUT(service.unreadAlertCount(), 0, 5000);
        service.clearAlerts();
        QTRY_VERIFY_WITH_TIMEOUT(service.alerts().isEmpty(), 5000);

        service.setAlertNewGridEnabled(false);
        service.setAlertNewDxccEnabled(false);
        service.setAlertCqEnabled(false);
        service.setAlertCallPattern(QStringLiteral("WATCH*"));
        decode.insert(QStringLiteral("timestamp"),
                      QDateTime::currentMSecsSinceEpoch() + 1000);
        decode.insert(QStringLiteral("message"), QStringLiteral("WATCHME LIVE"));
        decode.insert(QStringLiteral("fromCall"), QStringLiteral("WATCHME"));
        decode.insert(QStringLiteral("dxGrid"), QStringLiteral("IO91"));
        service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));
        QTRY_COMPARE_WITH_TIMEOUT(service.unreadAlertCount(), 1, 5000);
        QCOMPARE(service.alerts().first().toMap()
                     .value(QStringLiteral("type")).toString(),
                 QStringLiteral("call_watch"));

        service.clearLiveSpots();
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 0, 5000);
        QVERIFY(service.roster().isEmpty());
    }

    void attributesDirectedDecodeToTransmittingStation()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const databasePath =
            tempDir.filePath(QStringLiteral("directed-message-map.sqlite"));
        MapIntelligenceService service(nullptr, databasePath);

        QVariantMap decode;
        decode.insert(QStringLiteral("time"), QStringLiteral("075255"));
        decode.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
        decode.insert(QStringLiteral("message"), QStringLiteral("WA1BXY UA3GIE KO92"));
        decode.insert(QStringLiteral("fromCall"), QStringLiteral("WA1BXY"));
        decode.insert(QStringLiteral("dxGrid"), QStringLiteral("KO92"));
        decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        decode.insert(QStringLiteral("db"), QStringLiteral("-11"));
        decode.insert(QStringLiteral("freq"), 1500);
        decode.insert(QStringLiteral("dxcc"), QStringLiteral("United States"));
        decode.insert(QStringLiteral("continent"), QStringLiteral("NA"));
        decode.insert(QStringLiteral("cqZone"), 5);
        decode.insert(QStringLiteral("ituZone"), 8);

        service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));

        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.roster().size(), 1, 5000);
        QVariantMap const row = service.roster().first().toMap();
        QCOMPARE(row.value(QStringLiteral("call")).toString(),
                 QStringLiteral("UA3GIE"));
        QCOMPARE(row.value(QStringLiteral("targetCall")).toString(),
                 QStringLiteral("WA1BXY"));
        QCOMPARE(row.value(QStringLiteral("grid")).toString(),
                 QStringLiteral("KO92"));
        QVERIFY(row.value(QStringLiteral("dxcc")).toString()
                    != QStringLiteral("United States"));
        QVERIFY(row.value(QStringLiteral("continent")).toString()
                    != QStringLiteral("NA"));
    }

    void classifiesRosterGridOriginAndFineSpotAge()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
        MapIntelligenceService service(
            nullptr, tempDir.filePath(QStringLiteral("grid-origin.sqlite")));

        QStringList const spotAges = service.availableSpotAgeFilters();
        QCOMPARE(spotAges.first(), QStringLiteral("5 min"));
        QCOMPARE(spotAges.at(1), QStringLiteral("10 min"));
        QCOMPARE(spotAges.at(11), QStringLiteral("60 min"));
        QVERIFY(spotAges.contains(QStringLiteral("35 min")));
        QVERIFY(spotAges.contains(QStringLiteral("All retained")));
        service.setSpotAgeFilter(QStringLiteral("35 min"));
        QCOMPARE(service.spotAgeFilter(), QStringLiteral("35 min"));
        service.setSpotAgeFilter(QStringLiteral("1 hour"));
        QCOMPARE(service.spotAgeFilter(), QStringLiteral("60 min"));
        QVERIFY(service.availableRosterColumns().contains(QStringLiteral("Grid source")));

        auto ingest = [&service](const QString& call,
                                 const QString& grid,
                                 const QString& source,
                                 const QString& gridOrigin) {
            QVariantMap entry;
            entry.insert(QStringLiteral("time"), QStringLiteral("120000"));
            entry.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
            entry.insert(QStringLiteral("message"),
                         source == QStringLiteral("decoder")
                             ? QStringLiteral("CQ %1 %2").arg(call, grid)
                             : QStringLiteral("Grid information for %1").arg(call));
            entry.insert(QStringLiteral("fromCall"), call);
            entry.insert(QStringLiteral("dxGrid"), grid);
            entry.insert(QStringLiteral("source"), source);
            entry.insert(QStringLiteral("gridOrigin"), gridOrigin);
            entry.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
            entry.insert(QStringLiteral("db"), QStringLiteral("-14"));
            entry.insert(QStringLiteral("freq"), 1500);
            service.ingestDecodeEntry(entry, 14074000, QStringLiteral("20m"));
        };

        ingest(QStringLiteral("DECODED1"), QStringLiteral("JN70"),
               QStringLiteral("decoder"), QStringLiteral("Decoded on-air"));
        ingest(QStringLiteral("LOOKUP1"), QStringLiteral("IO91"),
               QStringLiteral("lookup"), QStringLiteral("Lookup"));
        ingest(QStringLiteral("OAMS1"), QStringLiteral("KN12"),
               QStringLiteral("oams"), QStringLiteral("OAMS"));

        auto rosterRow = [&service](const QString& call) {
            for (QVariant const& value : service.roster()) {
                QVariantMap const row = value.toMap();
                if (row.value(QStringLiteral("call")).toString() == call) return row;
            }
            return QVariantMap {};
        };
        QTRY_VERIFY_WITH_TIMEOUT(!rosterRow(QStringLiteral("OAMS1")).isEmpty(), 5000);

        QVariantMap const decoded = rosterRow(QStringLiteral("DECODED1"));
        QCOMPARE(decoded.value(QStringLiteral("gridOrigin")).toString(),
                 QStringLiteral("Decoded on-air"));
        QCOMPARE(decoded.value(QStringLiteral("gridReliability")).toString(),
                 QStringLiteral("Verified"));
        QCOMPARE(decoded.value(QStringLiteral("gridMarker")).toString(),
                 QStringLiteral("✓"));

        QVariantMap const lookup = rosterRow(QStringLiteral("LOOKUP1"));
        QCOMPARE(lookup.value(QStringLiteral("gridOrigin")).toString(),
                 QStringLiteral("Lookup estimate"));
        QCOMPARE(lookup.value(QStringLiteral("gridReliability")).toString(),
                 QStringLiteral("Estimated"));
        QCOMPARE(lookup.value(QStringLiteral("gridMarker")).toString(),
                 QStringLiteral("?"));

        QVariantMap const oams = rosterRow(QStringLiteral("OAMS1"));
        QCOMPARE(oams.value(QStringLiteral("gridOrigin")).toString(),
                 QStringLiteral("OAMS"));
        QCOMPARE(oams.value(QStringLiteral("gridReliability")).toString(),
                 QStringLiteral("Corroborated"));
        QCOMPARE(oams.value(QStringLiteral("gridMarker")).toString(),
                 QStringLiteral("◇"));

        QVariantMap psk;
        psk.insert(QStringLiteral("call"), QStringLiteral("PSK1"));
        psk.insert(QStringLiteral("grid"), QStringLiteral("JO21"));
        psk.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        psk.insert(QStringLiteral("freq"), 14076000);
        service.setRosterStationCall(QStringLiteral("HOME"));
        service.setRosterSpottedMeOnly(true);
        service.ingestPskSpots({psk}, QStringLiteral("HOME"), QStringLiteral("JN70"));
        QTRY_VERIFY_WITH_TIMEOUT(!rosterRow(QStringLiteral("PSK1")).isEmpty(), 5000);
        QVariantMap const pskRow = rosterRow(QStringLiteral("PSK1"));
        QCOMPARE(pskRow.value(QStringLiteral("gridOrigin")).toString(),
                 QStringLiteral("PSK Reporter"));
        QCOMPARE(pskRow.value(QStringLiteral("gridReliability")).toString(),
                 QStringLiteral("Corroborated"));
        QCOMPARE(pskRow.value(QStringLiteral("gridMarker")).toString(),
                 QStringLiteral("◇"));
    }

    void migratesLegacySpotGridOrigin()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
        QString const databasePath = tempDir.filePath(QStringLiteral("legacy-grid.sqlite"));
        QString const connectionName = QStringLiteral("legacy_grid_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        {
            QSqlDatabase database =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral(
                "CREATE TABLE map_spot("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, unique_key TEXT NOT NULL UNIQUE,"
                "call TEXT NOT NULL, grid TEXT, grid4 TEXT, band TEXT, mode TEXT,"
                "message TEXT, observed_utc TEXT NOT NULL, observed_ms INTEGER NOT NULL,"
                "frequency_hz INTEGER, snr INTEGER, source TEXT,"
                "hits INTEGER NOT NULL DEFAULT 1)")));
            query.prepare(QStringLiteral(
                "INSERT INTO map_spot(unique_key,call,grid,grid4,band,mode,message,"
                "observed_utc,observed_ms,frequency_hz,snr,source)"
                " VALUES(?,?,?,?,?,?,?,?,?,?,?,?)"));
            query.addBindValue(QStringLiteral("legacy-grid-1"));
            query.addBindValue(QStringLiteral("LEGACY1"));
            query.addBindValue(QStringLiteral("JN70"));
            query.addBindValue(QStringLiteral("JN70"));
            query.addBindValue(QStringLiteral("20m"));
            query.addBindValue(QStringLiteral("FT8"));
            query.addBindValue(QStringLiteral("CQ LEGACY1 JN70"));
            query.addBindValue(QStringLiteral("2026-08-06T12:00:00Z"));
            query.addBindValue(QDateTime::currentMSecsSinceEpoch());
            query.addBindValue(14075500);
            query.addBindValue(-15);
            query.addBindValue(QStringLiteral("decoder"));
            QVERIFY(query.exec());
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        MapIntelligenceService service(nullptr, databasePath);
        QTRY_COMPARE_WITH_TIMEOUT(service.roster().size(), 1, 5000);
        QVariantMap const row = service.roster().first().toMap();
        QCOMPARE(row.value(QStringLiteral("call")).toString(), QStringLiteral("LEGACY1"));
        QCOMPARE(row.value(QStringLiteral("gridOrigin")).toString(),
                 QStringLiteral("Decoded on-air"));
        QCOMPARE(row.value(QStringLiteral("gridReliability")).toString(),
                 QStringLiteral("Verified"));
    }

    void repairsPersistedDirectedDecodeAttribution()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
        QString const databasePath =
            tempDir.filePath(QStringLiteral("persisted-attribution-map.sqlite"));

        {
            MapIntelligenceService service(nullptr, databasePath);
            QVariantMap decode;
            decode.insert(QStringLiteral("time"), QStringLiteral("075255"));
            decode.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
            decode.insert(QStringLiteral("message"), QStringLiteral("WA1BXY UA3GIE KO92"));
            decode.insert(QStringLiteral("fromCall"), QStringLiteral("WA1BXY"));
            decode.insert(QStringLiteral("dxGrid"), QStringLiteral("KO92"));
            decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
            decode.insert(QStringLiteral("db"), QStringLiteral("-11"));
            decode.insert(QStringLiteral("freq"), 1500);
            service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));
            QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        }

        QString const connectionName =
            QStringLiteral("map_attribution_test_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        {
            QSqlDatabase database =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral(
                "UPDATE map_spot SET call='WA1BXY', target_call='',"
                " dxcc='United States', continent='NA', cq_zone=5, itu_zone=8,"
                " state='RI'")));
            QVERIFY(query.exec(QStringLiteral(
                "UPDATE map_spot_event SET call='WA1BXY'")));
            QVERIFY(query.exec(QStringLiteral(
                "DELETE FROM map_meta"
                " WHERE key='decoder_sender_attribution_version'")));
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        {
            MapIntelligenceService service(nullptr, databasePath);
            QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        }

        QString const verifyConnectionName =
            QStringLiteral("map_attribution_verify_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        {
            QSqlDatabase database =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                          verifyConnectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral(
                "SELECT call, target_call, dxcc, continent, state FROM map_spot")));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toString(), QStringLiteral("UA3GIE"));
            QCOMPARE(query.value(1).toString(), QStringLiteral("WA1BXY"));
            QVERIFY(query.value(2).toString() != QStringLiteral("United States"));
            QVERIFY(query.value(3).toString() != QStringLiteral("NA"));
            QVERIFY(query.value(4).toString().isEmpty());
            QVERIFY(query.exec(QStringLiteral("SELECT call FROM map_spot_event")));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toString(), QStringLiteral("UA3GIE"));
            database.close();
        }
        QSqlDatabase::removeDatabase(verifyConnectionName);
    }

    void exportsAllFilteredRowsBeyondVisibleLimit()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("large-logbook.adi"));
        QString const databasePath =
            tempDir.filePath(QStringLiteral("large-map-intelligence.sqlite"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        for (int index = 0; index < 75; ++index) {
            QByteArray const call =
                QByteArray("EX") + QByteArray::number(index).rightJustified(4, '0');
            file.write(record(call, "JN70", "20m", "", "FT8", ""));
        }
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.reloadFromAdif(adifPath);
        QTRY_VERIFY_WITH_TIMEOUT(!service.loading(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 75, 5000);

        auto* operations =
            qobject_cast<MapOperationsService*>(service.operationsService());
        QVERIFY(operations);
        operations->setLogbookLimit(50);
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookTotal(), 75, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookRows().size(), 50, 5000);

        QString const csvPath = tempDir.filePath(QStringLiteral("all-filtered.csv"));
        QVERIFY(operations->exportLogbook(csvPath, QStringLiteral("CSV")));
        QTRY_VERIFY_WITH_TIMEOUT(!operations->exportInProgress(), 5000);
        QFile csv(csvPath);
        QVERIFY(csv.open(QIODevice::ReadOnly));
        QByteArray const data = csv.readAll();
        QCOMPARE(data.count('\n'), 76);
        QVERIFY(data.contains("\"EX0000\""));
        QVERIFY(data.contains("\"EX0074\""));
    }

    void completesPotaActionValidityAndBandModeStatus()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("pota.adi"));
        QString const databasePath =
            tempDir.filePath(QStringLiteral("pota-intelligence.sqlite"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        file.write(record("POTA1", "FN20", "20m", "14.074", "FT8", "",
                          {}, {}, "United States", "NA", {}, {}, {}, {},
                          "120000", "US-0001"));
        file.write(record("POTA2", "FN21", "40m", "7.074", "FT4", "",
                          "LOTW_QSL_RCVD", "Y", "United States", "NA", {}, {}, {}, {},
                          "120001", "US-0001"));
        file.write(record("IOTA1", "JN70", "20m", "14.074", "FT8", "",
                          "QSL_RCVD", "Y", "Italy", "EU", {}, {}, {}, "EU-005",
                          "120002"));
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 3, 5000);
        auto* operations =
            qobject_cast<MapOperationsService*>(service.operationsService());
        QVERIFY(operations);
        auto* layers = qobject_cast<MapLayerModel*>(service.layerModel());
        QVERIFY(layers);
        layers->setLayerEnabled(QStringLiteral("pota"), true);
        layers->setLayerEnabled(QStringLiteral("iota"), true);
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookTotal(), 3, 5000);

        QDateTime const now = QDateTime::currentDateTimeUtc();
        QVariantMap liveSpot {
            {QStringLiteral("reference"), QStringLiteral("US-0001")},
            {QStringLiteral("activator"), QStringLiteral("N0CALL/P")},
            {QStringLiteral("grid6"), QStringLiteral("FN20aa")},
            {QStringLiteral("frequency"), QStringLiteral("14074.0")},
            {QStringLiteral("mode"), QStringLiteral("FT8")},
            {QStringLiteral("spotTime"), now.toString(Qt::ISODate)},
            {QStringLiteral("expire"), 120},
            {QStringLiteral("invalid"), false}
        };
        operations->setOperatorCall(QStringLiteral("N0CALL"));
        QVariantMap const activatorAction = operations->preparePotaAction(
            liveSpot, QStringLiteral("N0CALL"));
        QCOMPARE(activatorAction.value(QStringLiteral("role")).toString(),
                 QStringLiteral("ACTIVATOR"));
        QVERIFY(activatorAction.value(QStringLiteral("valid")).toBool());
        QVERIFY(activatorAction.value(QStringLiteral("targetCall")).toString().isEmpty());

        QVariantMap const hunterAction = operations->preparePotaAction(
            liveSpot, QStringLiteral("W1AW"));
        QCOMPARE(hunterAction.value(QStringLiteral("role")).toString(),
                 QStringLiteral("HUNTER"));
        QCOMPARE(hunterAction.value(QStringLiteral("targetCall")).toString(),
                 QStringLiteral("N0CALL/P"));
        QCOMPARE(hunterAction.value(QStringLiteral("targetGrid")).toString(),
                 QStringLiteral("FN20AA"));
        QCOMPARE(hunterAction.value(QStringLiteral("frequencyHz")).toDouble(),
                 14074000.0);
        QVERIFY(hunterAction.value(QStringLiteral("messageReady")).toBool());
        QVERIFY(hunterAction.value(QStringLiteral("remainingSeconds")).toLongLong() > 0);

        QVariantMap expiredSpot = liveSpot;
        expiredSpot.insert(QStringLiteral("spotTime"),
                           now.addSecs(-180).toString(Qt::ISODate));
        expiredSpot.insert(QStringLiteral("expire"), 60);
        QVariantMap const expiredAction = operations->preparePotaAction(
            expiredSpot, QStringLiteral("W1AW"));
        QVERIFY(!expiredAction.value(QStringLiteral("valid")).toBool());
        QVERIFY(expiredAction.value(QStringLiteral("expired")).toBool());
        QVERIFY(!expiredAction.value(QStringLiteral("messageReady")).toBool());

        QTRY_VERIFY_WITH_TIMEOUT(!operations->operationalMarkers().isEmpty(), 5000);
        QVariantMap potaMarker;
        QVariantMap iotaMarker;
        for (QVariant const& value : operations->operationalMarkers()) {
            QVariantMap const marker = value.toMap();
            QString const type = marker.value(QStringLiteral("type")).toString();
            if (type.compare(QStringLiteral("POTA"), Qt::CaseInsensitive) == 0
                && marker.value(QStringLiteral("reference")).toString()
                       == QStringLiteral("US-0001")) {
                potaMarker = marker;
            }
            if (type.compare(QStringLiteral("IOTA"), Qt::CaseInsensitive) == 0
                && marker.value(QStringLiteral("reference")).toString()
                       == QStringLiteral("EU-005")) {
                iotaMarker = marker;
            }
        }
        QVERIFY(!potaMarker.isEmpty());
        QVERIFY(potaMarker.value(QStringLiteral("worked")).toBool());
        QVERIFY(potaMarker.value(QStringLiteral("workedBands")).toList()
                    .contains(QStringLiteral("20M")));
        QVERIFY(potaMarker.value(QStringLiteral("workedBands")).toList()
                    .contains(QStringLiteral("40M")));
        QVERIFY(potaMarker.value(QStringLiteral("confirmedBands")).toList()
                    .contains(QStringLiteral("40M")));
        QVERIFY(potaMarker.value(QStringLiteral("workedModes")).toList()
                    .contains(QStringLiteral("FT8")));
        QVERIFY(potaMarker.value(QStringLiteral("workedModes")).toList()
                    .contains(QStringLiteral("FT4")));
        bool confirmedBandMode = false;
        for (QVariant const& value : potaMarker.value(QStringLiteral("workedBandModes"))
                                        .toList()) {
            QVariantMap const bandMode = value.toMap();
            if (bandMode.value(QStringLiteral("band")).toString() == QStringLiteral("40M")
                && bandMode.value(QStringLiteral("mode")).toString() == QStringLiteral("FT4")) {
                confirmedBandMode = bandMode.value(QStringLiteral("confirmed")).toBool();
            }
        }
        QVERIFY(confirmedBandMode);

        QVERIFY(!iotaMarker.isEmpty());
        QVERIFY(iotaMarker.value(QStringLiteral("worked")).toBool());
        QVERIFY(iotaMarker.value(QStringLiteral("confirmed")).toBool());
        QVERIFY(iotaMarker.value(QStringLiteral("workedBandModes")).toList().size() >= 1);
    }

    void buildsIndependentOperationalRoster()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("roster.adi"));
        QString const databasePath =
            tempDir.filePath(QStringLiteral("roster-intelligence.sqlite"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        file.write(record("DONE1", "FN20", "20m", "", "FT8", "",
                          "LOTW_QSL_RCVD", "Y", "United States", "NA",
                          {}, {}, "PA"));
        file.write(record("PENDING1", "JN70", "20m", "", "FT8", "",
                          {}, {}, "Italy", "EU"));
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 2, 5000);
        QVERIFY(service.availableAwardPrograms().contains(QStringLiteral("WAC")));
        QVERIFY(service.availableAwardPrograms().contains(QStringLiteral("US48")));
        auto awardByLabel = [&service](QString const& label) {
            for (QVariant const& value : service.awards()) {
                QVariantMap const award = value.toMap();
                if (award.value(QStringLiteral("label")).toString() == label) {
                    return award;
                }
            }
            return QVariantMap {};
        };
        QCOMPARE(awardByLabel(QStringLiteral("WAC"))
                     .value(QStringLiteral("worked")).toInt(), 2);
        QCOMPARE(awardByLabel(QStringLiteral("US48"))
                     .value(QStringLiteral("worked")).toInt(), 1);

        auto ingest = [&service](QString const& call,
                                 QString const& grid,
                                 QString const& dxcc,
                                 QString const& continent,
                                 int snr,
                                 qint64 timestamp) {
            QVariantMap decode;
            decode.insert(QStringLiteral("time"), QString::number(timestamp));
            decode.insert(QStringLiteral("timestamp"), timestamp);
            decode.insert(QStringLiteral("message"),
                          QStringLiteral("CQ %1 %2").arg(call, grid));
            decode.insert(QStringLiteral("fromCall"), call);
            decode.insert(QStringLiteral("dxGrid"), grid);
            decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
            decode.insert(QStringLiteral("db"), snr);
            decode.insert(QStringLiteral("freq"), 1500);
            decode.insert(QStringLiteral("dxcc"), dxcc);
            decode.insert(QStringLiteral("continent"), continent);
            decode.insert(QStringLiteral("isCQ"), true);
            service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));
        };

        qint64 const now = QDateTime::currentMSecsSinceEpoch();
        ingest(QStringLiteral("DONE1"), QStringLiteral("FN20"),
               QStringLiteral("United States"), QStringLiteral("NA"), -12, now);
        ingest(QStringLiteral("PENDING1"), QStringLiteral("JN70"),
               QStringLiteral("Italy"), QStringLiteral("EU"), -15, now + 1);
        ingest(QStringLiteral("NEW1"), QStringLiteral("KM72"),
               QStringLiteral("Israel"), QStringLiteral("AS"), -18, now + 2);

        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 3, 5000);
        QCOMPARE(service.rosterNewCount(), 1);
        QCOMPARE(service.rosterUnconfirmedCount(), 1);
        QCOMPARE(service.rosterWantedCount(), 2);

        auto findCall = [&service](QString const& call) {
            for (QVariant const& value : service.roster()) {
                QVariantMap const row = value.toMap();
                if (row.value(QStringLiteral("call")).toString() == call) return row;
            }
            return QVariantMap {};
        };
        QCOMPARE(findCall(QStringLiteral("DONE1")).value(QStringLiteral("status")).toString(),
                 QStringLiteral("CONFIRMED"));
        QCOMPARE(findCall(QStringLiteral("PENDING1")).value(QStringLiteral("status")).toString(),
                 QStringLiteral("UNCONFIRMED"));
        QCOMPARE(findCall(QStringLiteral("NEW1")).value(QStringLiteral("status")).toString(),
                 QStringLiteral("NEW"));

        // A newer decode replaces the previous row for the same station.
        ingest(QStringLiteral("NEW1"), QStringLiteral("KM72"),
               QStringLiteral("Israel"), QStringLiteral("AS"), -5, now + 3);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 3, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            findCall(QStringLiteral("NEW1")).value(QStringLiteral("snr")).toInt(), -5, 5000);

        service.setRosterStatusFilter(QStringLiteral("New"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 1, 5000);
        QCOMPARE(service.roster().first().toMap().value(QStringLiteral("call")).toString(),
                 QStringLiteral("NEW1"));
        service.setRosterStatusFilter(QStringLiteral("Unconfirmed"));
        QTRY_COMPARE_WITH_TIMEOUT(
            service.roster().first().toMap().value(QStringLiteral("call")).toString(),
            QStringLiteral("PENDING1"), 5000);
        QCOMPARE(service.rosterCount(), 1);
        service.setRosterStatusFilter(QStringLiteral("Wanted"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 2, 5000);
        service.setRosterStatusFilter(QStringLiteral("All"));

        // Map filters must not hide active stations from the operational roster.
        service.setContinentFilter(QStringLiteral("EU"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        QCOMPARE(service.rosterCount(), 3);

        QVariantMap psk;
        psk.insert(QStringLiteral("call"), QStringLiteral("PSK-RX"));
        psk.insert(QStringLiteral("grid"), QStringLiteral("JO21"));
        psk.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        psk.insert(QStringLiteral("freq"), 14076000);
        psk.insert(QStringLiteral("continent"), QStringLiteral("EU"));
        service.ingestPskSpots({psk}, QStringLiteral("HOME"), QStringLiteral("JM75"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 2, 5000);
        QCOMPARE(service.rosterCount(), 3);

        service.setRosterCallWatched(QStringLiteral("NEW1"), true);
        QTRY_VERIFY_WITH_TIMEOUT(
            findCall(QStringLiteral("NEW1")).value(QStringLiteral("watched")).toBool(),
            5000);
        QCOMPARE(service.roster().first().toMap().value(QStringLiteral("call")).toString(),
                 QStringLiteral("NEW1"));
        service.setRosterStatusFilter(QStringLiteral("Watched"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 1, 5000);
        QCOMPARE(service.roster().first().toMap().value(QStringLiteral("call")).toString(),
                 QStringLiteral("NEW1"));

        service.setRosterStatusFilter(QStringLiteral("All"));
        service.setRosterCallIgnored(QStringLiteral("NEW1"), true);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 2, 5000);
        QVERIFY(findCall(QStringLiteral("NEW1")).isEmpty());

        MapIntelligenceService persisted(nullptr, databasePath);
        QTRY_COMPARE_WITH_TIMEOUT(persisted.rosterCount(), 2, 5000);
        service.clearRosterPreferences();
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 3, 5000);
        persisted.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(persisted.rosterCount(), 3, 5000);
    }

    void keepsAllTimeAwardProgressWithTemporaryMapPeriod()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("historical.adi"));
        QString const databasePath =
            tempDir.filePath(QStringLiteral("historical-intelligence.sqlite"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        file.write(field("CALL", "OLD1")
                   + field("GRIDSQUARE", "JN70")
                   + field("BAND", "20m")
                   + field("MODE", "FT8")
                   + field("QSO_DATE", "20260101")
                   + field("TIME_ON", "120000")
                   + field("COUNTRY", "Italy")
                   + field("CONT", "EU")
                   + "<EOR>\n");
        file.write(field("CALL", "OLD2")
                   + field("GRIDSQUARE", "KM18")
                   + field("BAND", "20m")
                   + field("MODE", "FT8")
                   + field("QSO_DATE", "20260102")
                   + field("TIME_ON", "120000")
                   + field("COUNTRY", "Greece")
                   + field("CONT", "EU")
                   + field("LOTW_QSL_RCVD", "Y")
                   + "<EOR>\n");
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 2, 5000);
        QCOMPARE(service.statistics().value(QStringLiteral("qso")).toInt(), 2);
        QCOMPARE(service.statistics().value(QStringLiteral("totalQso")).toInt(), 2);

        service.setPeriodFilter(QStringLiteral("7 days"));
        QTRY_COMPARE_WITH_TIMEOUT(
            service.statistics().value(QStringLiteral("qso")).toInt(), 0, 5000);
        QCOMPARE(service.statistics().value(QStringLiteral("totalQso")).toInt(), 2);
        QCOMPARE(service.statistics().value(QStringLiteral("totalConfirmed")).toInt(), 1);
        QCOMPARE(service.statistics().value(QStringLiteral("period")).toString(),
                 QStringLiteral("7 days"));

        auto awardByLabel = [&service](QString const& label) {
            for (QVariant const& value : service.awards()) {
                QVariantMap const award = value.toMap();
                if (award.value(QStringLiteral("label")).toString() == label) {
                    return award;
                }
            }
            return QVariantMap {};
        };
        QTRY_COMPARE_WITH_TIMEOUT(
            awardByLabel(QStringLiteral("DXCC"))
                .value(QStringLiteral("worked")).toInt(),
            2, 5000);
        QCOMPARE(awardByLabel(QStringLiteral("DXCC"))
                     .value(QStringLiteral("confirmed")).toInt(),
                 1);
        QVERIFY(awardByLabel(QStringLiteral("DXCC"))
                    .value(QStringLiteral("scope")).toString()
                    .endsWith(QStringLiteral("All time")));

        service.setPeriodFilter(QStringLiteral("All time"));
        QTRY_COMPARE_WITH_TIMEOUT(
            service.statistics().value(QStringLiteral("qso")).toInt(), 2, 5000);
    }

    void enrichesSparseDecodiumAdifForRosterAndAwards()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("decodium-log.adi"));
        QString const databasePath =
            tempDir.filePath(QStringLiteral("decodium-map-intelligence.sqlite"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        // Older or imported ADI records may omit COUNTRY, CONT, CQZ and ITUZ.
        // The map must still derive those values from cty.dat.
        file.write(field("CALL", "SV1ABC")
                   + field("GRIDSQUARE", "KM18")
                   + field("BAND", "20m")
                   + field("MODE", "FT8")
                   + field("QSO_DATE", "20260728")
                   + field("TIME_ON", "120000")
                   + "<EOR>\n");
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.setRosterStationCall(QStringLiteral("9H1ABC"));
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(service.availableDxcc().contains(
                                      QStringLiteral("Greece"), Qt::CaseInsensitive),
                                  5000);

        auto awardByLabel = [&service](QString const& label) {
            for (QVariant const& value : service.awards()) {
                QVariantMap const award = value.toMap();
                if (award.value(QStringLiteral("label")).toString() == label) {
                    return award;
                }
            }
            return QVariantMap {};
        };
        QTRY_COMPARE_WITH_TIMEOUT(
            awardByLabel(QStringLiteral("DXCC")).value(QStringLiteral("worked")).toInt(),
            1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            awardByLabel(QStringLiteral("Maidenhead")).value(QStringLiteral("worked")).toInt(),
            1, 5000);
        QVERIFY(awardByLabel(QStringLiteral("DXCC"))
                    .value(QStringLiteral("scope")).toString()
                    .contains(QStringLiteral("9H1ABC")));

        QVariantMap decode;
        decode.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
        decode.insert(QStringLiteral("message"), QStringLiteral("CQ SV2TEST KM18"));
        decode.insert(QStringLiteral("fromCall"), QStringLiteral("SV2TEST"));
        decode.insert(QStringLiteral("dxGrid"), QStringLiteral("KM18"));
        decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        decode.insert(QStringLiteral("db"), -8);
        decode.insert(QStringLiteral("freq"), 1500);
        decode.insert(QStringLiteral("dxcc"), QStringLiteral("Greece"));
        decode.insert(QStringLiteral("continent"), QStringLiteral("EU"));
        decode.insert(QStringLiteral("isCQ"), true);
        service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));

        auto findRoster = [&service](QString const& call) {
            for (QVariant const& value : service.roster()) {
                QVariantMap const row = value.toMap();
                if (row.value(QStringLiteral("call")).toString() == call) {
                    return row;
                }
            }
            return QVariantMap {};
        };
        QTRY_VERIFY_WITH_TIMEOUT(!findRoster(QStringLiteral("SV2TEST")).isEmpty(), 5000);
        QVariantMap const rosterRow = findRoster(QStringLiteral("SV2TEST"));
        QVERIFY(rosterRow.value(QStringLiteral("dxccWorked")).toBool());
        QVERIFY(!rosterRow.value(QStringLiteral("huntReason")).toString()
                     .contains(QStringLiteral("New DXCC"), Qt::CaseInsensitive));
    }

    void appliesAwardRulesAndDetailedIgnoreLists()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("awards.adi"));
        QString const databasePath =
            tempDir.filePath(QStringLiteral("awards-intelligence.sqlite"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        file.write(record("USWORK", "FN20", "20m", "", "FT8", "",
                          "LOTW_QSL_RCVD", "Y",
                          "United States", "NA", "5", "8", "PA"));
        file.write(record("ITWORK", "JN70", "20m", "", "FT8", "",
                          {}, {}, "Italy", "EU", "15", "28"));
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 2, 5000);

        auto ingest = [&service](QString const& call,
                                 QString const& grid,
                                 QString const& dxcc,
                                 QString const& continent,
                                 int cqZone,
                                 int ituZone,
                                 QString const& state) {
            QVariantMap decode;
            decode.insert(QStringLiteral("timestamp"),
                          QDateTime::currentMSecsSinceEpoch());
            decode.insert(QStringLiteral("time"), QStringLiteral("120000"));
            decode.insert(QStringLiteral("message"),
                          QStringLiteral("CQ %1 %2").arg(call, grid));
            decode.insert(QStringLiteral("fromCall"), call);
            decode.insert(QStringLiteral("dxGrid"), grid);
            decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
            decode.insert(QStringLiteral("db"), -12);
            decode.insert(QStringLiteral("freq"), 1500);
            decode.insert(QStringLiteral("dxcc"), dxcc);
            decode.insert(QStringLiteral("continent"), continent);
            decode.insert(QStringLiteral("cqZone"), cqZone);
            decode.insert(QStringLiteral("ituZone"), ituZone);
            decode.insert(QStringLiteral("state"), state);
            decode.insert(QStringLiteral("isCQ"), true);
            service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));
        };

        ingest(QStringLiteral("LIVEIT"), QStringLiteral("JN70"),
               QStringLiteral("Italy"), QStringLiteral("EU"), 15, 28, {});
        ingest(QStringLiteral("LIVEIL"), QStringLiteral("KM72"),
               QStringLiteral("Israel"), QStringLiteral("AS"), 20, 39, {});
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 2, 5000);

        service.setActiveAwardProgram(QStringLiteral("WAZ"));
        service.setAwardGoal(QStringLiteral("Confirmed"));
        service.setRosterStatusFilter(QStringLiteral("Award"));
        QTRY_VERIFY_WITH_TIMEOUT(
            !service.roster().isEmpty()
                && service.roster().first().toMap()
                       .value(QStringLiteral("awardProgram")).toString()
                       == QStringLiteral("WAZ")
                && service.roster().first().toMap()
                       .value(QStringLiteral("awardWanted")).toBool(),
            5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 2, 5000);
        for (QVariant const& value : service.roster()) {
            QVERIFY(value.toMap().value(QStringLiteral("awardWanted")).toBool());
            QCOMPARE(value.toMap().value(QStringLiteral("awardProgram")).toString(),
                     QStringLiteral("WAZ"));
        }

        service.setAwardGoal(QStringLiteral("Worked"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 1, 5000);
        QCOMPARE(service.roster().first().toMap()
                     .value(QStringLiteral("call")).toString(),
                 QStringLiteral("LIVEIL"));
        service.setRosterStatusFilter(QStringLiteral("All"));

        service.setRosterDxccIgnored(QStringLiteral("Israel"), true);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterPreferenceCount(), 1, 5000);
        QCOMPARE(service.rosterPreferences().first().toMap()
                     .value(QStringLiteral("type")).toString(),
                 QStringLiteral("DXCC"));

        service.setRosterCallWatched(QStringLiteral("LIVEIT"), true);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterPreferenceCount(), 2, 5000);
        service.removeRosterPreference(QStringLiteral("WATCH"),
                                       QStringLiteral("LIVEIT"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterPreferenceCount(), 1, 5000);

        service.setRosterCallIgnored(QStringLiteral("LIVEIT"), true);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 0, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterPreferenceCount(), 2, 5000);
        service.removeRosterPreference(QStringLiteral("CALL"),
                                       QStringLiteral("LIVEIT"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 1, 5000);

        MapIntelligenceService persisted(nullptr, databasePath);
        persisted.setRosterStatusFilter(QStringLiteral("All"));
        QTRY_COMPARE_WITH_TIMEOUT(persisted.rosterCount(), 1, 5000);
        persisted.removeRosterPreference(QStringLiteral("DXCC"),
                                         QStringLiteral("Israel"));
        QTRY_COMPARE_WITH_TIMEOUT(persisted.rosterCount(), 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(persisted.rosterPreferenceCount(), 0, 5000);
    }

    void completesRosterFiltersScopesMatricesAndRr73()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("roster.adi"));
        QString const databasePath = tempDir.filePath(QStringLiteral("roster.sqlite"));
        QByteArray adif = QByteArrayLiteral("Decodium ADIF\n<EOH>\n");
        adif += record("LOTW1", "FN25", "20m", "14.074", "FT8", "",
                        "LOTW_QSL_RCVD", "Y", "Canada", "NA", "2", "4", "ON");
        adif += record("EQSL1", "FN26", "20m", "14.074", "FT8", "",
                        "EQSL_QSL_RCVD", "Y", "Canada", "NA", "2", "4", "ON");
        adif += record("OQRS1", "FN27", "20m", "14.074", "FT8", "",
                        "OQRS", "Y", "Canada", "NA", "2", "4", "ON");
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(adif) == adif.size());
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.setRosterStationCall(QStringLiteral("WA1BXY"));
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 3, 5000);

        qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
        auto ingest = [&service, &timestamp](const QString& call,
                                               const QString& message,
                                               const QString& grid,
                                               const QString& band,
                                               const QString& mode,
                                               const QString& dxcc,
                                               const QString& continent,
                                               const QString& state,
                                               int snr,
                                               double dt,
                                               bool isCq,
                                               const QString& pota = {},
                                               const QString& county = {},
                                               int cqZone = 0,
                                               int ituZone = 0) {
            QVariantMap decode;
            decode.insert(QStringLiteral("time"), QString::number(++timestamp));
            decode.insert(QStringLiteral("timestamp"), timestamp);
            decode.insert(QStringLiteral("message"), message);
            decode.insert(QStringLiteral("fromCall"), call);
            decode.insert(QStringLiteral("dxGrid"), grid);
            decode.insert(QStringLiteral("mode"), mode);
            decode.insert(QStringLiteral("db"), snr);
            decode.insert(QStringLiteral("dt"), dt);
            decode.insert(QStringLiteral("freq"), 1500);
            decode.insert(QStringLiteral("dxcc"), dxcc);
            decode.insert(QStringLiteral("continent"), continent);
            decode.insert(QStringLiteral("state"), state);
            if (cqZone > 0) decode.insert(QStringLiteral("cqZone"), cqZone);
            if (ituZone > 0) decode.insert(QStringLiteral("ituZone"), ituZone);
            decode.insert(QStringLiteral("isCQ"), isCq);
            if (!pota.isEmpty()) decode.insert(QStringLiteral("pota"), pota);
            if (!county.isEmpty()) decode.insert(QStringLiteral("county"), county);
            service.ingestDecodeEntry(decode, 14074000,
                                      band.isEmpty() ? QStringLiteral("20m") : band);
        };

        ingest(QStringLiteral("WANTED1"), QStringLiteral("CQ WANTED1 FN20"),
               QStringLiteral("FN20"), QStringLiteral("20m"), QStringLiteral("FT8"),
               QStringLiteral("Brazil"), QStringLiteral("SA"), QStringLiteral("SP"),
               -10, 0.20, true, QStringLiteral("K-1234"), QStringLiteral("SP-001"), 5, 8);
        ingest(QStringLiteral("LOTW1"), QStringLiteral("CQ LOTW1 FN21"),
               QStringLiteral("FN21"), QStringLiteral("20m"), QStringLiteral("FT8"),
               QStringLiteral("United States"), QStringLiteral("NA"), QStringLiteral("PA"),
               -10, 0.20, true);
        ingest(QStringLiteral("EQSL1"), QStringLiteral("CQ EQSL1 FN36"),
               QStringLiteral("FN36"), QStringLiteral("20m"), QStringLiteral("FT8"),
               QStringLiteral("Canada"), QStringLiteral("NA"), QStringLiteral("ON"),
               -10, 0.20, true);
        ingest(QStringLiteral("OQRS1"), QStringLiteral("CQ OQRS1 FN37"),
               QStringLiteral("FN37"), QStringLiteral("20m"), QStringLiteral("FT8"),
               QStringLiteral("Canada"), QStringLiteral("NA"), QStringLiteral("ON"),
               -10, 0.20, true);
        ingest(QStringLiteral("HIGHDT"), QStringLiteral("CQ HIGHDT FN22"),
               QStringLiteral("FN22"), QStringLiteral("20m"), QStringLiteral("FT8"),
               QStringLiteral("United States"), QStringLiteral("NA"), QStringLiteral("PA"),
               -10, 0.90, true);
        ingest(QStringLiteral("LOWSNR"), QStringLiteral("CQ LOWSNR FN23"),
               QStringLiteral("FN23"), QStringLiteral("20m"), QStringLiteral("FT8"),
               QStringLiteral("United States"), QStringLiteral("NA"), QStringLiteral("PA"),
               -30, 0.10, true);
        ingest(QStringLiteral("CW1"), QStringLiteral("CQ CW1 FN24"),
               QStringLiteral("FN24"), QStringLiteral("20m"), QStringLiteral("CW"),
               QStringLiteral("United States"), QStringLiteral("NA"), QStringLiteral("PA"),
               -10, 0.10, true);
        ingest(QStringLiteral("ITALY1"), QStringLiteral("CQ ITALY1 JN70"),
               QStringLiteral("JN70"), QStringLiteral("40m"), QStringLiteral("FT8"),
               QStringLiteral("Italy"), QStringLiteral("EU"), QString(),
               -10, 0.10, true);
        ingest(QStringLiteral("RR73CALL"), QStringLiteral("RR73CALL RR73"),
               QString(), QStringLiteral("20m"), QStringLiteral("FT8"),
               QStringLiteral("United States"), QStringLiteral("NA"), QStringLiteral("PA"),
               -10, 0.10, false);

        auto rosterRow = [&service](const QString& call) {
            for (QVariant const& value : service.roster()) {
                QVariantMap const row = value.toMap();
                if (row.value(QStringLiteral("call")).toString()
                        .compare(call, Qt::CaseInsensitive) == 0) {
                    return row;
                }
            }
            return QVariantMap();
        };
        QTRY_VERIFY_WITH_TIMEOUT(!rosterRow(QStringLiteral("WANTED1")).isEmpty(), 5000);
        QVariantMap const wanted = rosterRow(QStringLiteral("WANTED1"));
        QString const reason = wanted.value(QStringLiteral("huntReason")).toString();
        QVERIFY(reason.contains(QStringLiteral("New call: WANTED1")));
        QVERIFY(reason.contains(QStringLiteral("New grid: FN20")));
        QVERIFY(reason.contains(QStringLiteral("New DXCC: Brazil")));
        QVERIFY(reason.contains(QStringLiteral("New WPX:")));
        QVERIFY(reason.contains(QStringLiteral("New POTA: K-1234")));
        QVERIFY(reason.contains(QStringLiteral("New CQ zone: 5")));
        QVERIFY(reason.contains(QStringLiteral("New ITU zone: 0"))
                || reason.contains(QStringLiteral("New ITU zone: 8")));
        QVERIFY(reason.contains(QStringLiteral("New state: SP")));
        QVERIFY(reason.contains(QStringLiteral("New county: SP-001")));
        QVERIFY(reason.contains(QStringLiteral("New continent: SA")));
        QCOMPARE(wanted.value(QStringLiteral("dt")).toDouble(), 0.20);

        service.setRosterWantedTypes({QStringLiteral("POTA")});
        QTRY_VERIFY_WITH_TIMEOUT(
            !rosterRow(QStringLiteral("WANTED1"))
                 .value(QStringLiteral("huntReason")).toString()
                 .contains(QStringLiteral("New grid:")), 5000);
        QVERIFY(!rosterRow(QStringLiteral("WANTED1"))
                     .value(QStringLiteral("huntReason")).toString()
                     .contains(QStringLiteral("New grid:")));
        service.setRosterRule(QStringLiteral("POTA"), QStringLiteral("K-1234"),
                              QStringLiteral("WANTED"));
        service.setRosterRule(QStringLiteral("STATE"), QStringLiteral("SP"),
                              QStringLiteral("IGNORE"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterWantedMatrix().size(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterExceptionMatrix().size(), 1, 5000);
        service.removeRosterRule(QStringLiteral("STATE"), QStringLiteral("SP"));
        service.setRosterWantedTypes(service.availableRosterWantedTypes());

        service.setRosterMinSnr(-15);
        service.setRosterMinSnrEnabled(true);
        service.setRosterMaxDt(0.30);
        service.setRosterMaxDtEnabled(true);
        QTRY_VERIFY_WITH_TIMEOUT(rosterRow(QStringLiteral("LOWSNR")).isEmpty(), 5000);
        QVERIFY(rosterRow(QStringLiteral("LOWSNR")).isEmpty());
        QVERIFY(rosterRow(QStringLiteral("HIGHDT")).isEmpty());
        service.setRosterMinSnrEnabled(false);
        service.setRosterMaxDtEnabled(false);

        service.setRosterUsesLoTW(true);
        QTRY_VERIFY_WITH_TIMEOUT(!rosterRow(QStringLiteral("LOTW1")).isEmpty()
                                 && rosterRow(QStringLiteral("WANTED1")).isEmpty(),
                                 5000);
        QVERIFY(!rosterRow(QStringLiteral("LOTW1")).isEmpty());
        QVERIFY(rosterRow(QStringLiteral("WANTED1")).isEmpty());
        service.setRosterUsesLoTW(false);
        service.setRosterUsesEQSL(true);
        QTRY_VERIFY_WITH_TIMEOUT(!rosterRow(QStringLiteral("EQSL1")).isEmpty()
                                 && rosterRow(QStringLiteral("LOTW1")).isEmpty()
                                 && rosterRow(QStringLiteral("OQRS1")).isEmpty(),
                                 5000);
        service.setRosterUsesEQSL(false);
        service.setRosterUsesOQRS(true);
        QTRY_VERIFY_WITH_TIMEOUT(!rosterRow(QStringLiteral("OQRS1")).isEmpty()
                                 && rosterRow(QStringLiteral("LOTW1")).isEmpty()
                                 && rosterRow(QStringLiteral("EQSL1")).isEmpty(),
                                 5000);
        service.setRosterUsesOQRS(false);

        QVariantMap spotted;
        spotted.insert(QStringLiteral("call"), QStringLiteral("SPOTME"));
        spotted.insert(QStringLiteral("grid"), QStringLiteral("FN30"));
        spotted.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        spotted.insert(QStringLiteral("freq"), 14074000);
        service.ingestPskSpots({spotted}, QStringLiteral("WA1BXY"),
                               QStringLiteral("FN20"));
        service.setRosterSpottedMeOnly(true);
        QTRY_VERIFY_WITH_TIMEOUT(!rosterRow(QStringLiteral("SPOTME")).isEmpty()
                                 && rosterRow(QStringLiteral("WANTED1")).isEmpty(),
                                 5000);
        service.setRosterSpottedMeOnly(false);

        service.setRosterScope(QStringLiteral("Current band"));
        service.setBandFilter(QStringLiteral("20m"));
        QTRY_VERIFY_WITH_TIMEOUT(rosterRow(QStringLiteral("ITALY1")).isEmpty(), 5000);
        service.setRosterScope(QStringLiteral("Current mode"));
        service.setModeFilter(QStringLiteral("FT8"));
        QTRY_VERIFY_WITH_TIMEOUT(rosterRow(QStringLiteral("CW1")).isEmpty(), 5000);
        service.setRosterScope(QStringLiteral("Digital modes"));
        service.setBandFilter(QStringLiteral("All"));
        QVERIFY(rosterRow(QStringLiteral("CW1")).isEmpty());
        service.setRosterScope(QStringLiteral("All bands"));

        service.setRosterCqOnly(true);
        service.setRosterTreatRr73AsCq(false);
        QTRY_VERIFY_WITH_TIMEOUT(rosterRow(QStringLiteral("RR73CALL")).isEmpty(), 5000);
        service.setRosterTreatRr73AsCq(true);
        QTRY_VERIFY_WITH_TIMEOUT(!rosterRow(QStringLiteral("RR73CALL")).isEmpty(), 5000);
        QVERIFY(rosterRow(QStringLiteral("RR73CALL"))
                    .value(QStringLiteral("isCQ")).toBool());
        service.setRosterCqOnly(false);

        service.setRosterDxccScope(QStringLiteral("Same DXCC"));
        QTRY_VERIFY_WITH_TIMEOUT(rosterRow(QStringLiteral("ITALY1")).isEmpty(), 5000);
        service.setRosterDxccScope(QStringLiteral("Other DXCC"));
        QTRY_VERIFY_WITH_TIMEOUT(!rosterRow(QStringLiteral("ITALY1")).isEmpty(), 5000);
        service.setRosterDxccScope(QStringLiteral("All"));

        service.setActiveAwardProgram(QStringLiteral("DXCC"));
        service.setAwardGoal(QStringLiteral("Worked"));
        service.setRosterScope(QStringLiteral("Award selected"));
        QTRY_VERIFY_WITH_TIMEOUT(
            rosterRow(QStringLiteral("WANTED1"))
                .value(QStringLiteral("awardProgram")).toString()
                .compare(QStringLiteral("DXCC"), Qt::CaseInsensitive) == 0,
            5000);
        QVERIFY(!rosterRow(QStringLiteral("ITALY1")).isEmpty());
        QVERIFY(!rosterRow(QStringLiteral("WANTED1")).isEmpty());
        QVERIFY(rosterRow(QStringLiteral("WANTED1"))
                    .value(QStringLiteral("awardWanted")).toBool());
    }

    void ingestsPskMqttIntoAnalyticsAndOperationalRoster()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           tempDir.path());

        MapIntelligenceService service(
            nullptr, tempDir.filePath(QStringLiteral("psk-intelligence.sqlite")));
        auto* feed = qobject_cast<MapPskFeedService*>(service.pskFeedService());
        QVERIFY(feed);
        feed->configureStation(QStringLiteral("HOME"), QStringLiteral("JM75"));

        QVariantMap localDecode;
        localDecode.insert(QStringLiteral("timestamp"),
                           QDateTime::currentMSecsSinceEpoch());
        localDecode.insert(QStringLiteral("time"), QStringLiteral("120000"));
        localDecode.insert(QStringLiteral("message"), QStringLiteral("CQ MQTT1 JN70"));
        localDecode.insert(QStringLiteral("fromCall"), QStringLiteral("MQTT1"));
        localDecode.insert(QStringLiteral("dxGrid"), QStringLiteral("JN70"));
        localDecode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        localDecode.insert(QStringLiteral("db"), -9);
        localDecode.insert(QStringLiteral("freq"), 1500);
        localDecode.insert(QStringLiteral("isCQ"), true);
        service.ingestDecodeEntry(localDecode, 14074000, QStringLiteral("20m"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);

        QByteArray const payload = QByteArrayLiteral(
            R"json([{"rc":"MQTT1","rl":"JN70","rp":-7,"f":14074000,
                    "b":"20m","md":"FT8","t":)json")
            + QByteArray::number(QDateTime::currentSecsSinceEpoch())
            + QByteArrayLiteral("}]");
        QVERIFY(feed->injectPayloadForTest(payload));
        QTRY_VERIFY_WITH_TIMEOUT(feed->receivedCount() >= 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(service.liveSpotCount() >= 2, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!service.spotHeatmap().isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!service.spotTimeline().isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!service.spotPaths().isEmpty(), 5000);

        service.setSpotCorrelationFilter(QStringLiteral("Correlated"));
        QTRY_VERIFY_WITH_TIMEOUT(service.liveSpotCount() >= 1, 5000);
        service.setSpotCorrelationFilter(QStringLiteral("All"));

        auto corroboratedRosterRow = [&service]() {
            for (QVariant const& value : service.roster()) {
                QVariantMap const row = value.toMap();
                if (row.value(QStringLiteral("call")).toString()
                    == QStringLiteral("MQTT1")) {
                    return row;
                }
            }
            return QVariantMap {};
        };
        QTRY_VERIFY_WITH_TIMEOUT(
            corroboratedRosterRow().value(QStringLiteral("sourceCount")).toInt() >= 2,
            5000);
        QVERIFY(corroboratedRosterRow().value(QStringLiteral("sourceSummary"))
                    .toString().contains(QStringLiteral("Local decode")));
        QCOMPARE(corroboratedRosterRow().value(QStringLiteral("corroborationLevel"))
                     .toString(), QStringLiteral("Corroborated"));

        service.setRosterRule(QStringLiteral("CALL"), QStringLiteral("MQTT1"),
                              QStringLiteral("WATCH"), QStringLiteral("20m"),
                              QStringLiteral("FT8"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterRules().size(), 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!service.roster().isEmpty()
                                 && service.roster().first().toMap()
                                        .value(QStringLiteral("watched")).toBool(),
                                 5000);

        QStringList const visibleColumns {
            QStringLiteral("Grid"), QStringLiteral("POTA"),
            QStringLiteral("LoTW age")
        };
        service.setRosterVisibleColumns(visibleColumns);
        QCOMPARE(service.rosterVisibleColumns(), visibleColumns);
        QVERIFY(service.availableAwardPrograms().size() >= 328);
        QVERIFY(std::any_of(service.availableAwardPrograms().cbegin(),
                            service.availableAwardPrograms().cend(),
                            [](const QString& label) {
                                return label.contains(QStringLiteral("FT8DMC:"),
                                                      Qt::CaseInsensitive);
                            }));
    }

    void classifiesAndFiltersPropagationData()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        auto propagationQso = [](const QByteArray& call,
                                 const QByteArray& grid,
                                 const QByteArray& propagation,
                                 const QByteArray& time,
                                 bool confirmed) {
            QByteArray result = field("CALL", call)
                + field("GRIDSQUARE", grid)
                + field("QSO_DATE", "20260728")
                + field("TIME_ON", time)
                + field("BAND", "20m")
                + field("MODE", "FT8");
            if (!propagation.isEmpty()) {
                result += field("PROP_MODE", propagation);
            }
            if (confirmed) {
                result += field("QSL_RCVD", "Y");
            }
            return result + "<EOR>\n";
        };

        QString const adifPath = tempDir.filePath(QStringLiteral("propagation.adi"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        file.write(propagationQso("ES1TEST", "JN58aa", "ES", "120000", true));
        file.write(propagationQso("EME1TEST", "JN48bb", "EME", "120100", false));
        file.write(propagationQso("MS1TEST", "JO21cc", "Meteor Scatter", "120200", true));
        file.write(propagationQso("UNKNOWN1", "FN20dd", {}, "120300", false));
        file.close();

        MapIntelligenceService service(
            nullptr, tempDir.filePath(QStringLiteral("propagation.sqlite")));
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 4, 5000);
        QStringList const expectedPropagationModes {
            QStringLiteral("MIXED"), QStringLiteral("UNKNOWN"),
            QStringLiteral("AS"), QStringLiteral("AUE"), QStringLiteral("AUR"),
            QStringLiteral("BS"), QStringLiteral("ECH"), QStringLiteral("EME"),
            QStringLiteral("ES"), QStringLiteral("F2"), QStringLiteral("FAI"),
            QStringLiteral("INTERNET"), QStringLiteral("ION"), QStringLiteral("IRL"),
            QStringLiteral("MS"), QStringLiteral("RPT"), QStringLiteral("RS"),
            QStringLiteral("SAT"), QStringLiteral("TEP"), QStringLiteral("TR")
        };
        QCOMPARE(service.availablePropagationModes(), expectedPropagationModes);
        QTRY_COMPARE_WITH_TIMEOUT(service.propagationStatistics().size(), 20, 5000);
        QVERIFY(std::any_of(service.availablePropagationTypes().cbegin(),
                            service.availablePropagationTypes().cend(),
                            [](const QVariant& value) {
                                return value.toMap().value(QStringLiteral("label"))
                                           == QStringLiteral("Meteor Scatter");
                            }));

        auto rowFor = [&service](const QString& code) {
            for (QVariant const& value : service.propagationStatistics()) {
                QVariantMap const row = value.toMap();
                if (row.value(QStringLiteral("code")).toString() == code) {
                    return row;
                }
            }
            return QVariantMap();
        };
        QTRY_COMPARE_WITH_TIMEOUT(rowFor(QStringLiteral("ES"))
                                      .value(QStringLiteral("qso")).toInt(),
                                  1, 5000);
        QCOMPARE(rowFor(QStringLiteral("EME"))
                     .value(QStringLiteral("qso")).toInt(), 1);
        QCOMPARE(rowFor(QStringLiteral("MS"))
                     .value(QStringLiteral("qso")).toInt(), 1);
        QCOMPARE(rowFor(QStringLiteral("UNKNOWN"))
                     .value(QStringLiteral("qso")).toInt(), 1);
        QCOMPARE(service.propagationSummary().value(QStringLiteral("classified")).toInt(), 3);
        QCOMPARE(service.propagationSummary().value(QStringLiteral("unknown")).toInt(), 1);

        service.setPropagationFilter(QStringLiteral("MS"));
        QTRY_COMPARE_WITH_TIMEOUT(service.statistics().value(QStringLiteral("qso")).toInt(),
                                  1, 5000);
        QCOMPARE(service.propagationSummary().value(QStringLiteral("filter")).toString(),
                 QStringLiteral("MS"));
        QCOMPARE(rowFor(QStringLiteral("MS"))
                     .value(QStringLiteral("confirmed")).toInt(), 1);
        QCOMPARE(rowFor(QStringLiteral("ES"))
                     .value(QStringLiteral("qso")).toInt(), 0);

        service.setPropagationFilter(QStringLiteral("MIXED"));
        QTRY_COMPARE_WITH_TIMEOUT(service.statistics().value(QStringLiteral("qso")).toInt(),
                                  4, 5000);

        QVariantList const liveRows {
            QVariantMap {
                {QStringLiteral("call"), QStringLiteral("LIVE-ES")},
                {QStringLiteral("grid"), QStringLiteral("JN58aa")},
                {QStringLiteral("band"), QStringLiteral("20m")},
                {QStringLiteral("mode"), QStringLiteral("FT8")},
                {QStringLiteral("propMode"), QStringLiteral("ES")},
                {QStringLiteral("source"), QStringLiteral("psk")},
                {QStringLiteral("direction"), QStringLiteral("RX")},
                {QStringLiteral("freq"), 14074000},
                {QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch()}
            },
            QVariantMap {
                {QStringLiteral("call"), QStringLiteral("LIVE-MS")},
                {QStringLiteral("grid"), QStringLiteral("JO21cc")},
                {QStringLiteral("band"), QStringLiteral("20m")},
                {QStringLiteral("mode"), QStringLiteral("FT8")},
                {QStringLiteral("propMode"), QStringLiteral("MS")},
                {QStringLiteral("source"), QStringLiteral("psk")},
                {QStringLiteral("direction"), QStringLiteral("RX")},
                {QStringLiteral("freq"), 14075000},
                {QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch()}
            }
        };
        service.ingestPskSpots(liveRows, QStringLiteral("9H1ABC"), QStringLiteral("JM75fv"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 2, 5000);
        auto heatmapHasGrid = [&service](const QString& grid) {
            for (QVariant const& value : service.spotHeatmap()) {
                if (value.toMap().value(QStringLiteral("grid")).toString()
                        .compare(grid, Qt::CaseInsensitive) == 0) {
                    return true;
                }
            }
            return false;
        };
        QTRY_VERIFY_WITH_TIMEOUT(heatmapHasGrid(QStringLiteral("JN58")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(heatmapHasGrid(QStringLiteral("JO21")), 5000);
        service.setPropagationFilter(QStringLiteral("ES"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(heatmapHasGrid(QStringLiteral("JN58")), 5000);
        QVERIFY(!heatmapHasGrid(QStringLiteral("JO21")));
        service.setPropagationFilter(QStringLiteral("MIXED"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 2, 5000);
    }

    void executesAwardsAgainstRealAdifAndShowsMissingEntities()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const adifPath = QFINDTESTDATA("data/award-real.adi");
        QVERIFY2(!adifPath.isEmpty(), "real ADIF award fixture not found");
        QString const databasePath = tempDir.filePath(QStringLiteral("real-awards.sqlite"));
        MapIntelligenceService service(nullptr, databasePath);
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 5, 5000);
        QCOMPARE(service.statistics().value(QStringLiteral("qso")).toInt(), 5);
        QCOMPARE(service.sourcePath(), QFileInfo(adifPath).absoluteFilePath());

        QString const bassa = QStringLiteral("FT8DMC: BASSA - Basotho Stations");
        QVERIFY(service.availableAwardPrograms().contains(bassa));
        service.setActiveAwardProgram(bassa);
        service.setAwardGoal(QStringLiteral("Worked"));
        service.setAwardConfirmation(QStringLiteral("LoTW"));
        service.setAwardCallsign(QStringLiteral("9H1ABC"));
        service.setAwardFromDate(QStringLiteral("2026-01-01"));
        service.setAwardToDate(QStringLiteral("2026-01-03"));
        service.setBandFilter(QStringLiteral("20m"));
        service.setModeFilter(QStringLiteral("FT8"));

        auto awardByLabel = [&service](const QString& label) {
            for (QVariant const& value : service.awards()) {
                QVariantMap const award = value.toMap();
                if (award.value(QStringLiteral("label")).toString()
                        .compare(label, Qt::CaseInsensitive) == 0) {
                    return award;
                }
            }
            return QVariantMap();
        };
        QTRY_COMPARE_WITH_TIMEOUT(
            awardByLabel(bassa).value(QStringLiteral("worked")).toInt(), 2, 5000);
        QCOMPARE(awardByLabel(bassa).value(QStringLiteral("confirmed")).toInt(), 1);
        QVERIFY(awardByLabel(bassa).value(QStringLiteral("workedEntities"))
                    .toStringList().contains(QStringLiteral("7P8ABC")));
        QVERIFY(awardByLabel(bassa).value(QStringLiteral("scope"))
                    .toString().contains(QStringLiteral("9H1ABC")));

        QVariantMap decode;
        decode.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
        decode.insert(QStringLiteral("message"), QStringLiteral("CQ 7P8XYZ KG30DZ"));
        decode.insert(QStringLiteral("fromCall"), QStringLiteral("7P8XYZ"));
        decode.insert(QStringLiteral("dxGrid"), QStringLiteral("KG30DZ"));
        decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        decode.insert(QStringLiteral("dxcc"), QStringLiteral("Lesotho"));
        decode.insert(QStringLiteral("dxccNumber"), 432);
        decode.insert(QStringLiteral("continent"), QStringLiteral("AF"));
        decode.insert(QStringLiteral("isCQ"), true);
        service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.awardMissing().isEmpty(), 5000);
        QVariantMap const missing = service.awardMissing().first().toMap();
        QCOMPARE(missing.value(QStringLiteral("call")).toString(), QStringLiteral("7P8XYZ"));
        QVERIFY(missing.value(QStringLiteral("reason")).toString().contains("Missing"));

        QVERIFY(service.availableAwardPrograms().contains(
            QStringLiteral("FT8DMC: OHCA - One Hundred Countries")));
        service.setActiveAwardProgram(QStringLiteral("FT8DMC: OHCA - One Hundred Countries"));
        QTRY_VERIFY_WITH_TIMEOUT(service.availableAwardEndorsements().contains(
                                     QStringLiteral("20m")), 5000);
        service.setAwardEndorsement(QStringLiteral("20m"));
        QCOMPARE(service.awardEndorsement(), QStringLiteral("20m"));
        service.setAwardCallsign(QStringLiteral("OTHER"));
        QTRY_COMPARE_WITH_TIMEOUT(
            awardByLabel(QStringLiteral("FT8DMC: OHCA - One Hundred Countries"))
                .value(QStringLiteral("worked")).toInt(), 0, 5000);
    }

    void convertsWebMercatorAndRendersTropo()
    {
        QImage mercator(16, 16, QImage::Format_ARGB32);
        mercator.fill(QColor(20, 80, 160, 220));
        QImage const converted =
            MapExternalOverlayService::webMercatorToEquirectangular(
                mercator, QSize(64, 32));
        QCOMPARE(converted.size(), QSize(64, 32));
        QVERIFY(qAlpha(converted.pixel(32, 16)) > 0);
        QCOMPARE(qAlpha(converted.pixel(32, 0)), 0);

        QByteArray const tropo = R"json({
            "update_calls": [{
                "call": "TEST-1",
                "lat": 0.785398,
                "lon": 0.174533,
                "ts": 4102444800,
            "spokes": [
                0.0, 0.04,
                1.570796, 0.06,
                3.141593, 0.05,
                4.712389, 0.04
            ]
            }],
            "cloud_list": [{
                "id": "es-test",
                "perim": [
                    {"lat": 0.0, "lon": 0.0},
                    {"lat": 0.0, "lon": 0.25},
                    {"lat": 0.20, "lon": 0.125}
                ]
            }]
        })json";
        int featureCount = 0;
        QString error;
        QImage const tropoImage =
            MapExternalOverlayService::renderTropoPayload(
                tropo, &featureCount, &error, QSize(128, 64));
        QCOMPARE(featureCount, 2);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!tropoImage.isNull());

        bool hasVisiblePixel = false;
        for (int y = 0; y < tropoImage.height() && !hasVisiblePixel; ++y) {
            QRgb const* line =
                reinterpret_cast<QRgb const*>(tropoImage.constScanLine(y));
            for (int x = 0; x < tropoImage.width(); ++x) {
                if (qAlpha(line[x]) > 0) {
                    hasVisiblePixel = true;
                    break;
                }
            }
        }
        QVERIFY(hasVisiblePixel);
        QColor const esPixel = tropoImage.pixelColor(66, 30);
        QVERIFY(esPixel.alpha() > 0);
        QVERIFY(esPixel.blue() > esPixel.red());
        QVERIFY(esPixel.red() > esPixel.green());

        QImage const moonImage =
            MapExternalOverlayService::renderMoonOverlay(
                35.9, 14.5, 128.0, 34.0, 384400.0, 63.0,
                QSize(256, 128));
        QCOMPARE(moonImage.size(), QSize(256, 128));
        bool hasMoonPixel = false;
        for (int y = 0; y < moonImage.height() && !hasMoonPixel; ++y) {
            QRgb const* line =
                reinterpret_cast<QRgb const*>(moonImage.constScanLine(y));
            for (int x = 0; x < moonImage.width(); ++x) {
                if (qAlpha(line[x]) > 0) {
                    hasMoonPixel = true;
                    break;
                }
            }
        }
        QVERIFY(hasMoonPixel);

        QTemporaryDir moonCache;
        QVERIFY(moonCache.isValid());
        MapLayerModel moonLayers;
        MapExternalOverlayService moonService(
            &moonLayers, nullptr, moonCache.path());
        moonLayers.setLayerEnabled(QStringLiteral("moon"), true);
        moonService.updateMoonForStation(35.9, 14.5);
        QVERIFY(moonService.moonDataAvailable());
        int moonLayerCount = -1;
        for (int row = 0; row < moonLayers.rowCount(); ++row) {
            QModelIndex const index = moonLayers.index(row, 0);
            if (moonLayers.data(index, MapLayerModel::LayerIdRole).toString()
                == QStringLiteral("moon")) {
                moonLayerCount = moonLayers.data(index, MapLayerModel::CountRole).toInt();
                break;
            }
        }
        QCOMPARE(moonLayerCount, 1);
        QVERIFY(moonService.moonAzimuth() >= 0.0);
        QVERIFY(moonService.moonAzimuth() < 360.0);
        QVERIFY(moonService.moonElevation() >= -90.0);
        QVERIFY(moonService.moonElevation() <= 90.0);
        QVERIFY(moonService.moonDistanceKm() > 300000.0);
        QVERIFY(moonService.moonDistanceKm() < 450000.0);
        QVERIFY(moonService.moonIllumination() >= 0.0);
        QVERIFY(moonService.moonIllumination() <= 100.0);
        QVERIFY(moonService.moonSublunarLatitude() >= -90.0);
        QVERIFY(moonService.moonSublunarLatitude() <= 90.0);
        QVERIFY(moonService.moonSublunarLongitude() >= -180.0);
        QVERIFY(moonService.moonSublunarLongitude() <= 180.0);
        QTRY_VERIFY_WITH_TIMEOUT(moonService.hasOverlay(), 5000);

        // The startup path restores layer preferences before the overlay
        // service is constructed.  A persisted Moon layer must refresh too.
        MapLayerModel persistedMoonLayers;
        persistedMoonLayers.setLayerEnabled(QStringLiteral("moon"), true);
        MapExternalOverlayService persistedMoonService(
            &persistedMoonLayers, nullptr, moonCache.path());
        persistedMoonService.updateMoonForStation(35.9, 14.5);
        QVERIFY(persistedMoonService.moonDataAvailable());

        QByteArray const earthquakes = R"json({
            "type": "FeatureCollection",
            "features": [{
                "type": "Feature",
                "properties": {"mag": 5.4, "title": "Test quake"},
                "geometry": {"type": "Point", "coordinates": [14.5, 35.9, 10]}
            }]
        })json";
        featureCount = 0;
        error.clear();
        QImage const earthquakeImage =
            MapExternalOverlayService::renderEarthquakePayload(
                earthquakes, &featureCount, &error, QSize(128, 64));
        QCOMPARE(featureCount, 1);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!earthquakeImage.isNull());

        QByteArray const wildfires = R"json({
            "events": [{
                "id": "WF-TEST",
                "title": "Test wildfire",
                "geometry": [
                    {"date": "2026-07-28T00:00:00Z",
                     "type": "Point", "coordinates": [-120.5, 38.2]}
                ]
            }]
        })json";
        featureCount = 0;
        error.clear();
        QImage const wildfireImage =
            MapExternalOverlayService::renderWildfirePayload(
                wildfires, &featureCount, &error, QSize(128, 64));
        QCOMPARE(featureCount, 1);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!wildfireImage.isNull());
    }

    void exposesTemporalForecastMetadataFromCache()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QString const cachePath = tempDir.filePath(QStringLiteral("cache"));
        QDir().mkpath(cachePath);

        QImage cachedImage(16, 8, QImage::Format_ARGB32);
        cachedImage.fill(QColor(120, 80, 220, 180));
        QString const mufPath = QDir(cachePath).filePath(QStringLiteral("muf.img"));
        QVERIFY(cachedImage.save(mufPath, "PNG"));
        QFile cacheFile(mufPath);
        QVERIFY(cacheFile.open(QIODevice::ReadOnly));
        QVERIFY(cacheFile.setFileTime(
            QDateTime::currentDateTimeUtc().addSecs(-3600),
            QFileDevice::FileModificationTime));
        cacheFile.close();

        MapLayerModel layers;
        MapExternalOverlayService overlays(&layers, nullptr, cachePath);
        overlays.setOfflineMode(true);
        layers.setLayerEnabled(QStringLiteral("muf"), true);

        auto mufStatus = [&overlays] {
            for (QVariant const& value : overlays.providerStatus()) {
                QVariantMap const row = value.toMap();
                if (row.value(QStringLiteral("layerId")).toString()
                    == QStringLiteral("muf")) {
                    return row;
                }
            }
            return QVariantMap {};
        };
        QTRY_VERIFY_WITH_TIMEOUT(
            mufStatus().value(QStringLiteral("available")).toBool(), 5000);
        QVariantMap const status = mufStatus();
        QVERIFY(status.value(QStringLiteral("validUntilMs")).toLongLong() > 0);
        QVERIFY(status.value(QStringLiteral("ageSeconds")).toLongLong() >= 3500);
        QVERIFY(status.value(QStringLiteral("stale")).toBool());
        QCOMPARE(status.value(QStringLiteral("state")).toString(),
                 QStringLiteral("offline-cache"));
        QVERIFY(status.value(QStringLiteral("decayOpacity")).toDouble() < 1.0);
    }

    void persistsLiveMapLayersAcrossRestarts()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        {
            QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                               QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
            settings.clear();
            settings.beginGroup(QStringLiteral("LiveMapLayers"));
            // Simulate the old data-view side effect that saved Live as off.
            settings.setValue(QStringLiteral("Live"), false);
            settings.endGroup();
            settings.sync();
        }

        QString const databasePath =
            tempDir.filePath(QStringLiteral("map-intelligence.sqlite"));
        {
            MapIntelligenceService service(nullptr, databasePath);
            auto* layerModel = qobject_cast<MapLayerModel*>(service.layerModel());
            QVERIFY(layerModel);
            QVERIFY(layerModel->layerEnabled(QStringLiteral("live")));

            layerModel->setLayerEnabled(QStringLiteral("live"), false);
            layerModel->setLayerEnabled(QStringLiteral("moon"), true);
        }

        {
            MapIntelligenceService restored(nullptr, databasePath);
            auto* layerModel = qobject_cast<MapLayerModel*>(restored.layerModel());
            QVERIFY(layerModel);
            QVERIFY(!layerModel->layerEnabled(QStringLiteral("live")));
            QVERIFY(layerModel->layerEnabled(QStringLiteral("moon")));
        }
    }

    void replacesPskHeardBySnapshotWithoutClearingMqttFeed()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        MapIntelligenceService service(
            nullptr, tempDir.filePath(QStringLiteral("map-intelligence.sqlite")));
        auto* layerModel = qobject_cast<MapLayerModel*>(service.layerModel());
        QVERIFY(layerModel);

        auto layerCount = [layerModel](const QString& layerId) {
            for (int row = 0; row < layerModel->rowCount(); ++row) {
                QModelIndex const index = layerModel->index(row, 0);
                if (layerModel->data(index, MapLayerModel::LayerIdRole).toString() == layerId) {
                    return layerModel->data(index, MapLayerModel::CountRole).toInt();
                }
            }
            return -1;
        };

        QVariantMap mqtt;
        mqtt.insert(QStringLiteral("call"), QStringLiteral("MQTT-RX"));
        mqtt.insert(QStringLiteral("grid"), QStringLiteral("JO21"));
        mqtt.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        mqtt.insert(QStringLiteral("freq"), 14074000);
        mqtt.insert(QStringLiteral("provider"), QStringLiteral("PSK Reporter MQTT"));
        service.ingestPskSpots({mqtt}, QStringLiteral("HOME"), QStringLiteral("JM75"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);

        QVariantMap heardBy;
        heardBy.insert(QStringLiteral("call"), QStringLiteral("HTTP-RX"));
        heardBy.insert(QStringLiteral("grid"), QStringLiteral("JN58"));
        heardBy.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        heardBy.insert(QStringLiteral("freq"), 14074000);
        service.replacePskHeardBySpots(
            {heardBy}, QStringLiteral("HOME"), QStringLiteral("JM75"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(layerCount(QStringLiteral("psk")), 2, 5000);

        // An empty HTTP reply is authoritative: it clears cached heard-by
        // listeners, but not the independent continuous MQTT feed.
        service.replacePskHeardBySpots({}, QStringLiteral("HOME"), QStringLiteral("JM75"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(layerCount(QStringLiteral("psk")), 1, 5000);
        QCOMPARE(service.coverageCells().size(), 1);
        QCOMPARE(service.coverageCells().first().toMap()
                     .value(QStringLiteral("grid")).toString(),
                 QStringLiteral("JO21"));
    }

    void aggregatesOperationalBandActivity()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const databasePath =
            tempDir.filePath(QStringLiteral("band-activity.sqlite"));
        MapIntelligenceService service(nullptr, databasePath);
        service.setBandActivityWindowHours(1);
        QCOMPARE(service.bandActivityWindowHours(), 1);

        qint64 const now = QDateTime::currentMSecsSinceEpoch();
        auto ingestLocal = [&service, now](QString const& call,
                                           QString const& grid,
                                           QString const& band,
                                           qint64 dialFrequencyHz,
                                           int offsetHz,
                                           int snr,
                                           bool isTx,
                                           int ageSeconds) {
            QVariantMap decode;
            decode.insert(QStringLiteral("timestamp"),
                          now - static_cast<qint64>(ageSeconds) * 1000);
            decode.insert(QStringLiteral("time"), QStringLiteral("120000"));
            decode.insert(QStringLiteral("message"),
                          QStringLiteral("CQ %1 %2").arg(call, grid));
            decode.insert(QStringLiteral("fromCall"), call);
            decode.insert(QStringLiteral("dxGrid"), grid);
            decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
            decode.insert(QStringLiteral("db"), snr);
            decode.insert(QStringLiteral("freq"), offsetHz);
            decode.insert(QStringLiteral("isCQ"), true);
            decode.insert(QStringLiteral("isTx"), isTx);
            service.ingestDecodeEntry(decode, dialFrequencyHz, band);
        };

        ingestLocal(QStringLiteral("RX20A"), QStringLiteral("JN70"),
                    QStringLiteral("20m"), 14074000, 1450, -7, false, 35);
        ingestLocal(QStringLiteral("RX20B"), QStringLiteral("JO21"),
                    QStringLiteral("20m"), 14074000, 1550, -12, false, 20);
        ingestLocal(QStringLiteral("HOME"), QStringLiteral("JM75"),
                    QStringLiteral("20m"), 14074000, 1500, 0, true, 10);
        ingestLocal(QStringLiteral("RX40A"), QStringLiteral("IO91"),
                    QStringLiteral("40m"), 7074000, 1500, -18, false, 50);

        QTRY_COMPARE_WITH_TIMEOUT(
            service.bandActivitySummary().value(QStringLiteral("localRx")).toInt(),
            3, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            service.bandActivitySummary().value(QStringLiteral("localTx")).toInt(),
            1, 5000);

        QVariantList pskRows;
        auto pskRow = [now](QString const& call,
                            QString const& grid,
                            QString const& band,
                            qint64 frequencyHz,
                            QString const& direction,
                            int ageSeconds) {
            QVariantMap row;
            row.insert(QStringLiteral("call"), call);
            row.insert(QStringLiteral("grid"), grid);
            row.insert(QStringLiteral("band"), band);
            row.insert(QStringLiteral("freq"), frequencyHz);
            row.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
            row.insert(QStringLiteral("snr"), -10);
            row.insert(QStringLiteral("direction"), direction);
            row.insert(QStringLiteral("source"), QStringLiteral("psk"));
            row.insert(QStringLiteral("provider"), QStringLiteral("PSK Reporter"));
            row.insert(QStringLiteral("timestamp"),
                       now - static_cast<qint64>(ageSeconds) * 1000);
            return row;
        };
        pskRows.append(pskRow(QStringLiteral("PSK20A"), QStringLiteral("JN58"),
                              QStringLiteral("20m"), 14074000,
                              QStringLiteral("TX"), 15));
        pskRows.append(pskRow(QStringLiteral("PSK20B"), QStringLiteral("FN20"),
                              QStringLiteral("20m"), 14074000,
                              QStringLiteral("TX"), 5));
        pskRows.append(pskRow(QStringLiteral("PSK40A"), QStringLiteral("KO85"),
                              QStringLiteral("40m"), 7074000,
                              QStringLiteral("RX"), 25));
        service.ingestPskSpots(pskRows, QStringLiteral("HOME"),
                               QStringLiteral("JM75"));

        QTRY_COMPARE_WITH_TIMEOUT(
            service.bandActivitySummary().value(QStringLiteral("pskTx")).toInt(),
            2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            service.bandActivitySummary().value(QStringLiteral("pskRx")).toInt(),
            1, 5000);
        QCOMPARE(service.bandActivitySummary()
                     .value(QStringLiteral("windowHours")).toInt(),
                 1);
        QCOMPARE(service.bandActivitySummary()
                     .value(QStringLiteral("bandCount")).toInt(),
                 2);
        QCOMPARE(service.bandActivitySummary()
                     .value(QStringLiteral("bestBand")).toString(),
                 QStringLiteral("20m"));
        QVERIFY(service.bandActivitySummary()
                    .value(QStringLiteral("bestScore")).toInt() > 0);
        QCOMPARE(service.bandActivity().size(), 2);
        QVERIFY(!service.bandActivityTimeline().isEmpty());

        auto metricForBand = [&service](QString const& band) {
            for (QVariant const& value : service.bandActivity()) {
                QVariantMap const row = value.toMap();
                if (row.value(QStringLiteral("band")).toString() == band) {
                    return row;
                }
            }
            return QVariantMap {};
        };
        QVariantMap const twenty = metricForBand(QStringLiteral("20m"));
        QVERIFY(!twenty.isEmpty());
        QCOMPARE(twenty.value(QStringLiteral("rank")).toInt(), 1);
        QVERIFY(twenty.value(QStringLiteral("best")).toBool());
        QCOMPARE(twenty.value(QStringLiteral("localRx")).toInt(), 2);
        QCOMPARE(twenty.value(QStringLiteral("localTx")).toInt(), 1);
        QCOMPARE(twenty.value(QStringLiteral("pskRx")).toInt(), 0);
        QCOMPARE(twenty.value(QStringLiteral("pskTx")).toInt(), 2);
        QCOMPARE(twenty.value(QStringLiteral("uniqueCalls")).toInt(), 2);

        QVariantMap const forty = metricForBand(QStringLiteral("40m"));
        QVERIFY(!forty.isEmpty());
        QCOMPARE(forty.value(QStringLiteral("localRx")).toInt(), 1);
        QCOMPARE(forty.value(QStringLiteral("pskRx")).toInt(), 1);

        QVERIFY(databaseHasIndex(
            databasePath, QStringLiteral("idx_map_spot_event_band_window")));

        service.setBandActivityWindowHours(6);
        QCOMPARE(service.bandActivityWindowHours(), 6);
        service.setBandActivityWindowHours(12);
        QCOMPARE(service.bandActivityWindowHours(), 12);
        service.setBandActivityWindowHours(24);
        QCOMPARE(service.bandActivityWindowHours(), 24);
        service.setBandActivityWindowHours(5);
        QCOMPARE(service.bandActivityWindowHours(), 24);

        MapIntelligenceService persisted(nullptr, databasePath);
        QCOMPARE(persisted.bandActivityWindowHours(), 24);
    }

    void roundTripsMapConfigurationBundle()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        const QString sourceDatabase = tempDir.filePath(QStringLiteral("source.sqlite"));
        const QString targetDatabase = tempDir.filePath(QStringLiteral("target.sqlite"));
        MapIntelligenceService source(nullptr, sourceDatabase);
        auto* sourceLayers = qobject_cast<MapLayerModel*>(source.layerModel());
        QVERIFY(sourceLayers);
        sourceLayers->setLayerStyle(QStringLiteral("live"), QStringLiteral("#123456"),
                                    0.42, 2.5, 30);
        source.setSourceDecayMinutes({
            {QStringLiteral("decoder"), 25},
            {QStringLiteral("psk"), 120},
            {QStringLiteral("oams"), 45}
        });
        source.setRosterCallWatched(QStringLiteral("W8TEST"), true);
        source.setRosterRule(QStringLiteral("DXCC"), QStringLiteral("Italy"),
                             QStringLiteral("WANTED"), QStringLiteral("20m"),
                             QStringLiteral("FT8"));
        QTRY_VERIFY_WITH_TIMEOUT(source.rosterPreferenceCount() >= 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!source.rosterRules().isEmpty(), 5000);

        QString const bundlePath = tempDir.filePath(QStringLiteral("map-config.json"));
        QVERIFY(source.exportMapConfiguration(bundlePath, {
            {QStringLiteral("centerLongitude"), 12.5},
            {QStringLiteral("centerLatitude"), 41.9},
            {QStringLiteral("spanLongitude"), 28.0},
            {QStringLiteral("spanLatitude"), 18.0},
            {QStringLiteral("locked"), true}
        }));
        QFile bundle(bundlePath);
        QVERIFY(bundle.open(QIODevice::ReadOnly));
        QJsonParseError parseError;
        QJsonDocument const document = QJsonDocument::fromJson(bundle.readAll(), &parseError);
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        QVERIFY(document.object().value(QStringLiteral("layers")).toArray().size() >= 24);
        QVERIFY(document.object().value(QStringLiteral("presetNames")).toArray().size() >= 6);
        QVERIFY(document.object().value(QStringLiteral("presets")).isObject());
        QVERIFY(document.object().value(QStringLiteral("roster")).toObject()
                    .value(QStringLiteral("rules")).toArray().size() >= 1);
        QCOMPARE(document.object().value(QStringLiteral("viewport")).toObject()
                     .value(QStringLiteral("centerLongitude")).toDouble(), 12.5);

        MapIntelligenceService target(nullptr, targetDatabase);
        QVariantMap const viewport = target.importMapConfiguration(bundlePath);
        QCOMPARE(viewport.value(QStringLiteral("centerLongitude")).toDouble(), 12.5);
        auto* targetLayers = qobject_cast<MapLayerModel*>(target.layerModel());
        QVERIFY(targetLayers);
        QVariantMap const liveStyle = targetLayers->layerStyle(QStringLiteral("live"));
        QCOMPARE(liveStyle.value(QStringLiteral("color")).toString(), QStringLiteral("#123456"));
        QCOMPARE(liveStyle.value(QStringLiteral("labelDensity")).toInt(), 30);
        QCOMPARE(target.sourceDecayMinutes().value(QStringLiteral("decoder")).toInt(), 25);
        QCOMPARE(target.sourceDecayMinutes().value(QStringLiteral("psk")).toInt(), 120);
        QVERIFY(target.temporalLegend().size() >= 3);
        QTRY_VERIFY_WITH_TIMEOUT(target.rosterPreferenceCount() >= 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!target.rosterRules().isEmpty(), 5000);
    }
};

QTEST_MAIN(TestMapIntelligenceService)
#include "test_map_layer_service.moc"
