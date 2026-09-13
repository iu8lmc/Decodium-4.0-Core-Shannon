#include "MapOperationsService.h"

#include "MapLayerModel.h"
#include "RotatorService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QtConcurrent>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kNetworkTimeoutMs = 20000;
constexpr int kMaxPotaSpots = 1500;
constexpr int kMaxGeoFeatures = 5000;
constexpr int kMaxGeoPointsPerRing = 1200;
constexpr qint64 kIotaCacheMaxAgeSeconds = 30LL * 24LL * 60LL * 60LL;
constexpr auto kIotaCatalogUrl =
    "https://www.iota-world.org/islands-on-the-air/downloads/"
    "download-file.html?path=groups.json";

class ScopedMapDatabase
{
public:
    explicit ScopedMapDatabase(const QString& path)
        : m_name(QStringLiteral("map_operations_%1")
                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
    {
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
        m_database.setDatabaseName(path);
    }

    ~ScopedMapDatabase()
    {
        if (m_database.isValid()) {
            m_database.close();
        }
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_name);
    }

    QSqlDatabase& database() { return m_database; }

private:
    QString m_name;
    QSqlDatabase m_database;
};

QString normalizedChoice(QString value, const QStringList& choices,
                         const QString& fallback)
{
    value = value.trimmed();
    for (QString const& choice : choices) {
        if (choice.compare(value, Qt::CaseInsensitive) == 0) {
            return choice;
        }
    }
    return fallback;
}

qint64 periodStartEpoch(const QString& period)
{
    QDateTime now = QDateTime::currentDateTimeUtc();
    if (period.compare(QStringLiteral("24 hours"), Qt::CaseInsensitive) == 0) {
        return now.addDays(-1).toSecsSinceEpoch();
    }
    if (period.compare(QStringLiteral("7 days"), Qt::CaseInsensitive) == 0) {
        return now.addDays(-7).toSecsSinceEpoch();
    }
    if (period.compare(QStringLiteral("30 days"), Qt::CaseInsensitive) == 0) {
        return now.addDays(-30).toSecsSinceEpoch();
    }
    if (period.compare(QStringLiteral("1 year"), Qt::CaseInsensitive) == 0) {
        return now.addYears(-1).toSecsSinceEpoch();
    }
    return 0;
}

QString csvQuoted(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(value);
}

QPointF maidenheadCenter(QString grid)
{
    grid = grid.trimmed().toUpper();
    if (grid.size() < 4
        || grid.at(0) < QLatin1Char('A') || grid.at(0) > QLatin1Char('R')
        || grid.at(1) < QLatin1Char('A') || grid.at(1) > QLatin1Char('R')
        || !grid.at(2).isDigit() || !grid.at(3).isDigit()) {
        return {};
    }

    double lon = -180.0 + (grid.at(0).unicode() - QLatin1Char('A').unicode()) * 20.0
        + grid.at(2).digitValue() * 2.0;
    double lat = -90.0 + (grid.at(1).unicode() - QLatin1Char('A').unicode()) * 10.0
        + grid.at(3).digitValue();
    double lonSpan = 2.0;
    double latSpan = 1.0;
    if (grid.size() >= 6
        && grid.at(4) >= QLatin1Char('A') && grid.at(4) <= QLatin1Char('X')
        && grid.at(5) >= QLatin1Char('A') && grid.at(5) <= QLatin1Char('X')) {
        lonSpan = 2.0 / 24.0;
        latSpan = 1.0 / 24.0;
        lon += (grid.at(4).unicode() - QLatin1Char('A').unicode()) * lonSpan;
        lat += (grid.at(5).unicode() - QLatin1Char('A').unicode()) * latSpan;
    }
    return QPointF(lon + lonSpan / 2.0, lat + latSpan / 2.0);
}

double initialBearing(double latitude, double longitude,
                      double targetLatitude, double targetLongitude)
{
    double const lat1 = qDegreesToRadians(latitude);
    double const lat2 = qDegreesToRadians(targetLatitude);
    double const deltaLon = qDegreesToRadians(targetLongitude - longitude);
    double const y = qSin(deltaLon) * qCos(lat2);
    double const x = qCos(lat1) * qSin(lat2)
        - qSin(lat1) * qCos(lat2) * qCos(deltaLon);
    double bearing = qRadiansToDegrees(qAtan2(y, x));
    if (bearing < 0.0) {
        bearing += 360.0;
    }
    return bearing;
}

QString sortableColumn(const QString& value)
{
    QString const normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("call")) return QStringLiteral("call");
    if (normalized == QStringLiteral("band")) return QStringLiteral("band");
    if (normalized == QStringLiteral("mode")) return QStringLiteral("mode");
    if (normalized == QStringLiteral("dxcc")) return QStringLiteral("dxcc");
    if (normalized == QStringLiteral("grid")) return QStringLiteral("grid");
    if (normalized == QStringLiteral("frequency")) return QStringLiteral("frequency_mhz");
    if (normalized == QStringLiteral("status")) return QStringLiteral("confirmed");
    return QStringLiteral("qso_epoch");
}

struct SqlFilter {
    QString where;
    QVariantList binds;
};

SqlFilter buildFilter(const QString& search, const QString& band,
                      const QString& mode, const QString& period)
{
    QStringList clauses {QStringLiteral("1=1")};
    QVariantList binds;
    if (!band.trimmed().isEmpty()
        && band.compare(QStringLiteral("All"), Qt::CaseInsensitive) != 0) {
        clauses << QStringLiteral("lower(band)=lower(?)");
        binds << band.trimmed();
    }
    if (!mode.trimmed().isEmpty()
        && mode.compare(QStringLiteral("All"), Qt::CaseInsensitive) != 0) {
        clauses << QStringLiteral("upper(mode)=upper(?)");
        binds << mode.trimmed();
    }
    qint64 const start = periodStartEpoch(period);
    if (start > 0) {
        clauses << QStringLiteral("qso_epoch>=?");
        binds << start;
    }
    QString const text = search.trimmed();
    if (!text.isEmpty()) {
        clauses << QStringLiteral(
            "(call LIKE ? OR operator_call LIKE ? OR grid LIKE ? OR dxcc LIKE ? OR state LIKE ?"
            " OR pota_ref LIKE ? OR iota LIKE ? OR wpx LIKE ?"
            " OR EXISTS(SELECT 1 FROM map_qso_grid qg"
            "           WHERE qg.qso_id=map_qso.id AND qg.grid LIKE ?))");
        QString const pattern = QStringLiteral("%%1%").arg(text);
        for (int i = 0; i < 9; ++i) {
            binds << pattern;
        }
    }
    return {clauses.join(QStringLiteral(" AND ")), binds};
}

bool prepareAndBind(QSqlQuery* query, const QString& sql,
                    const QVariantList& binds, QString* error)
{
    if (!query->prepare(sql)) {
        if (error) *error = query->lastError().text();
        return false;
    }
    for (QVariant const& bind : binds) {
        query->addBindValue(bind);
    }
    if (!query->exec()) {
        if (error) *error = query->lastError().text();
        return false;
    }
    return true;
}

QVariantMap rowToMap(QSqlQuery& query)
{
    QVariantMap row;
    row.insert(QStringLiteral("sourceKey"), query.value(0).toString());
    row.insert(QStringLiteral("call"), query.value(1).toString());
    row.insert(QStringLiteral("grid"), query.value(2).toString());
    row.insert(QStringLiteral("band"), query.value(3).toString());
    row.insert(QStringLiteral("mode"), query.value(4).toString());
    row.insert(QStringLiteral("date"), query.value(5).toString());
    row.insert(QStringLiteral("time"), query.value(6).toString());
    row.insert(QStringLiteral("epoch"), query.value(7).toLongLong());
    row.insert(QStringLiteral("frequencyMhz"), query.value(8).toDouble());
    row.insert(QStringLiteral("satellite"), query.value(9).toString());
    row.insert(QStringLiteral("satMode"), query.value(10).toString());
    row.insert(QStringLiteral("frequencyRxMhz"), query.value(11).toDouble());
    row.insert(QStringLiteral("confirmed"), query.value(12).toBool());
    row.insert(QStringLiteral("dxcc"), query.value(13).toString());
    row.insert(QStringLiteral("continent"), query.value(14).toString());
    row.insert(QStringLiteral("state"), query.value(15).toString());
    row.insert(QStringLiteral("pota"), query.value(16).toString());
    row.insert(QStringLiteral("iota"), query.value(17).toString());
    row.insert(QStringLiteral("wpx"), query.value(18).toString());
    row.insert(QStringLiteral("source"), query.value(19).toString());
    row.insert(QStringLiteral("vuccGrids"), query.value(20).toString()
                                                  .split(QStringLiteral(", "),
                                                         Qt::SkipEmptyParts));
    return row;
}

QVariantList parseRings(const QJsonValue& coordinates)
{
    QVariantList rings;
    QJsonArray const outer = coordinates.toArray();
    for (QJsonValue const& ringValue : outer) {
        QJsonArray const rawRing = ringValue.toArray();
        if (rawRing.size() < 2) {
            continue;
        }
        int const stride = qMax(1, rawRing.size() / kMaxGeoPointsPerRing);
        QVariantList ring;
        ring.reserve(rawRing.size() / stride + 1);
        for (int index = 0; index < rawRing.size(); index += stride) {
            QJsonArray const point = rawRing.at(index).toArray();
            if (point.size() < 2) {
                continue;
            }
            // QVariantList << QVariantList concatenates both lists.  Keep the
            // GeoJSON coordinate as one nested QVariant so renderers receive
            // [longitude, latitude] points instead of a flattened number list.
            ring.append(QVariant::fromValue(QVariantList {
                point.at(0).toDouble(), point.at(1).toDouble()}));
        }
        if (ring.size() >= 2) {
            rings << QVariant::fromValue(ring);
        }
    }
    return rings;
}

QVariantMap presetMap(const QString& projection, const QString& dataView,
                      const QStringList& enabledLayers)
{
    return {
        {QStringLiteral("projection"), projection},
        {QStringLiteral("dataView"), dataView},
        {QStringLiteral("layers"), enabledLayers}
    };
}

} // namespace

MapOperationsService::MapOperationsService(const QString& databasePath,
                                           MapLayerModel* layerModel,
                                           QObject* parent)
    : QObject(parent)
    , m_databasePath(databasePath)
    , m_layerModel(layerModel)
    , m_network(new QNetworkAccessManager(this))
    , m_potaExpiryTimer(new QTimer(this))
    , m_rotatorService(new RotatorService(this))
{
    connect(m_rotatorService, &RotatorService::feedbackChanged,
            this, &MapOperationsService::rotatorFeedbackChanged);
    connect(m_rotatorService, &RotatorService::targetChanged,
            this, &MapOperationsService::rotatorTargetChanged);
    connect(m_rotatorService, &RotatorService::trackingChanged,
            this, &MapOperationsService::rotatorTrackingChanged);
    connect(m_rotatorService, &RotatorService::safetyChanged,
            this, &MapOperationsService::rotatorSafetyChanged);
    connect(m_rotatorService, &RotatorService::statusChanged, this, [this]() {
        if (m_rotatorService) setRotatorStatus(m_rotatorService->status());
    });
    m_potaExpiryTimer->setInterval(30000);
    connect(m_potaExpiryTimer, &QTimer::timeout, this,
            &MapOperationsService::pruneExpiredPotaSpots);
    m_potaExpiryTimer->start();

    loadSettings();
    loadMapPresets();

    if (m_layerModel) {
        connect(m_layerModel, &MapLayerModel::layerToggled, this,
                [this](QString const& id, bool enabled) {
            if (id == QStringLiteral("pota")) {
                if (enabled) {
                    refreshPota();
                } else {
                    clearSelectedPotaPark();
                    rebuildOperationalMarkers();
                }
            } else if (id == QStringLiteral("states")
                       || id == QStringLiteral("counties")) {
                // Rebuild for both transitions.  The feature cache remains in
                // memory, but disabled boundaries must leave the renderer now.
                refreshGeographicFeatures();
            } else if (id == QStringLiteral("iota")
                       || id == QStringLiteral("wpx")) {
                if (enabled && id == QStringLiteral("iota")) {
                    ensureIotaCatalog();
                }
                rebuildOperationalMarkers();
            }
        });
    }

    refreshLogbook();
    if (m_layerModel && m_layerModel->layerEnabled(QStringLiteral("pota"))) {
        refreshPota();
    }
    if (m_layerModel
        && (m_layerModel->layerEnabled(QStringLiteral("states"))
            || m_layerModel->layerEnabled(QStringLiteral("counties")))) {
        refreshGeographicFeatures();
    }
    if (m_layerModel && m_layerModel->layerEnabled(QStringLiteral("iota"))) {
        ensureIotaCatalog();
    }
}

MapOperationsService::~MapOperationsService()
{
    ++m_logbookGeneration;
    ++m_geoGeneration;
    ++m_iotaGeneration;
}

QStringList MapOperationsService::availableProjections() const
{
    return {
        QStringLiteral("Equirectangular"),
        QStringLiteral("Mercator"),
        QStringLiteral("Miller"),
        QStringLiteral("Azimuthal Equidistant")
    };
}

QStringList MapOperationsService::availableDataViews() const
{
    return {
        QStringLiteral("Live"),
        QStringLiteral("Logbook"),
        QStringLiteral("Live + Logbook")
    };
}

QStringList MapOperationsService::rotatorProtocols() const
{
    return m_rotatorService ? m_rotatorService->protocols()
                            : QStringList {QStringLiteral("PSTRotator"),
                                            QStringLiteral("CatRotator"),
                                            QStringLiteral("Hamlib rotctld")};
}

QString MapOperationsService::rotatorTransport() const
{
    return m_rotatorService ? m_rotatorService->transport()
                            : QStringLiteral("UDP");
}

bool MapOperationsService::rotatorFeedbackAvailable() const
{
    return m_rotatorService && m_rotatorService->feedbackAvailable();
}

qint64 MapOperationsService::rotatorLastFeedbackMs() const
{
    return m_rotatorService ? m_rotatorService->lastFeedbackMs() : 0;
}

double MapOperationsService::rotatorCurrentAzimuth() const
{
    return m_rotatorService ? m_rotatorService->currentAzimuth() : 0.0;
}

double MapOperationsService::rotatorCurrentElevation() const
{
    return m_rotatorService ? m_rotatorService->currentElevation() : 0.0;
}

double MapOperationsService::rotatorTargetAzimuth() const
{
    return m_rotatorService ? m_rotatorService->targetAzimuth() : 0.0;
}

double MapOperationsService::rotatorTargetElevation() const
{
    return m_rotatorService ? m_rotatorService->targetElevation() : 0.0;
}

bool MapOperationsService::rotatorTracking() const
{
    return m_rotatorService && m_rotatorService->tracking();
}

int MapOperationsService::rotatorTrackingIntervalMs() const
{
    return m_rotatorService ? m_rotatorService->trackingIntervalMs()
                            : m_rotatorTrackingIntervalMs;
}

bool MapOperationsService::rotatorSafetyEnabled() const
{
    return m_rotatorService ? m_rotatorService->safetyEnabled()
                            : m_rotatorSafetyEnabled;
}

double MapOperationsService::rotatorMinAzimuth() const
{
    return m_rotatorService ? m_rotatorService->minAzimuth() : m_rotatorMinAzimuth;
}

double MapOperationsService::rotatorMaxAzimuth() const
{
    return m_rotatorService ? m_rotatorService->maxAzimuth() : m_rotatorMaxAzimuth;
}

double MapOperationsService::rotatorMinElevation() const
{
    return m_rotatorService ? m_rotatorService->minElevation() : m_rotatorMinElevation;
}

double MapOperationsService::rotatorMaxElevation() const
{
    return m_rotatorService ? m_rotatorService->maxElevation() : m_rotatorMaxElevation;
}

bool MapOperationsService::rotatorParkOnStop() const
{
    return m_rotatorService ? m_rotatorService->parkOnStop() : m_rotatorParkOnStop;
}

double MapOperationsService::rotatorParkAzimuth() const
{
    return m_rotatorService ? m_rotatorService->parkAzimuth() : m_rotatorParkAzimuth;
}

double MapOperationsService::rotatorParkElevation() const
{
    return m_rotatorService ? m_rotatorService->parkElevation() : m_rotatorParkElevation;
}

void MapOperationsService::loadSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("MapOperations"));
    m_mapProjection = normalizedChoice(
        settings.value(QStringLiteral("Projection"),
                       QStringLiteral("Equirectangular")).toString(),
        availableProjections(), QStringLiteral("Equirectangular"));
    m_dataViewMode = normalizedChoice(
        settings.value(QStringLiteral("DataView"),
                       QStringLiteral("Live + Logbook")).toString(),
        availableDataViews(), QStringLiteral("Live + Logbook"));
    m_activeMapPreset =
        settings.value(QStringLiteral("ActivePreset"),
                       QStringLiteral("Operational")).toString();
    m_logbookBand =
        settings.value(QStringLiteral("LogbookBand"), QStringLiteral("All")).toString();
    m_logbookMode =
        settings.value(QStringLiteral("LogbookMode"), QStringLiteral("All")).toString();
    m_logbookSearch =
        settings.value(QStringLiteral("LogbookSearch"), QString()).toString().left(80);
    m_logbookPeriod =
        settings.value(QStringLiteral("LogbookPeriod"),
                       QStringLiteral("All time")).toString();
    m_logbookSort =
        settings.value(QStringLiteral("LogbookSort"), QStringLiteral("Date")).toString();
    m_logbookSortDescending =
        settings.value(QStringLiteral("LogbookSortDescending"), true).toBool();
    m_logbookLimit =
        qBound(50, settings.value(QStringLiteral("LogbookLimit"), 500).toInt(), 5000);
    m_rotatorHost =
        settings.value(QStringLiteral("RotatorHost"),
                       QStringLiteral("127.0.0.1")).toString().trimmed();
    m_rotatorProtocol = settings.value(QStringLiteral("RotatorProtocol"),
                                       QStringLiteral("PSTRotator")).toString();
    m_rotatorPort =
        qBound(1, settings.value(QStringLiteral("RotatorPort"),
                                 RotatorService::defaultPortForProtocol(m_rotatorProtocol)).toInt(),
               65535);
    m_rotatorEnabled =
        settings.value(QStringLiteral("RotatorEnabled"), false).toBool();
    m_rotatorTrackingIntervalMs = qBound(
        250, settings.value(QStringLiteral("RotatorTrackingIntervalMs"), 1000).toInt(), 10000);
    m_rotatorSafetyEnabled = settings.value(QStringLiteral("RotatorSafetyEnabled"), true).toBool();
    m_rotatorMinAzimuth = settings.value(QStringLiteral("RotatorMinAzimuth"), 0.0).toDouble();
    m_rotatorMaxAzimuth = settings.value(QStringLiteral("RotatorMaxAzimuth"), 360.0).toDouble();
    m_rotatorMinElevation = settings.value(QStringLiteral("RotatorMinElevation"), 0.0).toDouble();
    m_rotatorMaxElevation = settings.value(QStringLiteral("RotatorMaxElevation"), 180.0).toDouble();
    m_rotatorParkOnStop = settings.value(QStringLiteral("RotatorParkOnStop"), false).toBool();
    m_rotatorParkAzimuth = settings.value(QStringLiteral("RotatorParkAzimuth"), 0.0).toDouble();
    m_rotatorParkElevation = settings.value(QStringLiteral("RotatorParkElevation"), 0.0).toDouble();
    settings.endGroup();
    if (m_rotatorService) {
        m_rotatorService->setProtocol(m_rotatorProtocol);
        m_rotatorProtocol = m_rotatorService->protocol();
        m_rotatorService->setHost(m_rotatorHost);
        m_rotatorService->setPort(m_rotatorPort);
        m_rotatorService->setTrackingIntervalMs(m_rotatorTrackingIntervalMs);
        m_rotatorService->setSafetyEnabled(m_rotatorSafetyEnabled);
        m_rotatorService->setMinAzimuth(m_rotatorMinAzimuth);
        m_rotatorService->setMaxAzimuth(m_rotatorMaxAzimuth);
        m_rotatorService->setMinElevation(m_rotatorMinElevation);
        m_rotatorService->setMaxElevation(m_rotatorMaxElevation);
        m_rotatorService->setParkOnStop(m_rotatorParkOnStop);
        m_rotatorService->setParkAzimuth(m_rotatorParkAzimuth);
        m_rotatorService->setParkElevation(m_rotatorParkElevation);
        m_rotatorService->setEnabled(m_rotatorEnabled);
    }
    m_rotatorStatus = m_rotatorService ? m_rotatorService->status()
                                       : QStringLiteral("Rotator disabled");
}

void MapOperationsService::saveSetting(const QString& key,
                                       const QVariant& value) const
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("MapOperations"));
    settings.setValue(key, value);
    settings.endGroup();
}

void MapOperationsService::loadMapPresets()
{
    m_mapPresets = {
        QStringLiteral("Operational"), QStringLiteral("Logbook"),
        QStringLiteral("Parks"), QStringLiteral("Awards"),
        QStringLiteral("Propagation"), QStringLiteral("Minimal")
    };
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("MapPresets"));
    for (QString const& name : settings.childGroups()) {
        if (!m_mapPresets.contains(name, Qt::CaseInsensitive)) {
            m_mapPresets << name;
        }
    }
    settings.endGroup();
    m_mapPresets.sort(Qt::CaseInsensitive);
    emit mapPresetsChanged();
}

void MapOperationsService::setMapProjection(const QString& projection)
{
    QString const normalized = normalizedChoice(
        projection, availableProjections(), QStringLiteral("Equirectangular"));
    if (m_mapProjection == normalized) {
        return;
    }
    m_mapProjection = normalized;
    saveSetting(QStringLiteral("Projection"), normalized);
    m_activeMapPreset.clear();
    emit mapProjectionChanged();
    emit activeMapPresetChanged();
}

void MapOperationsService::setDataViewMode(const QString& mode)
{
    QString const normalized = normalizedChoice(
        mode, availableDataViews(), QStringLiteral("Live + Logbook"));
    if (m_dataViewMode == normalized) {
        return;
    }
    m_dataViewMode = normalized;
    // Data view controls the operations panel only. Layer choices belong to
    // the map and must remain independent and persistent across restarts.
    saveSetting(QStringLiteral("DataView"), normalized);
    emit dataViewModeChanged();
}

void MapOperationsService::setLogbookSearch(const QString& value)
{
    QString const normalized = value.trimmed().left(80);
    if (m_logbookSearch == normalized) return;
    m_logbookSearch = normalized;
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setLogbookBand(const QString& value)
{
    QString const normalized = value.trimmed().isEmpty()
        ? QStringLiteral("All") : value.trimmed();
    if (m_logbookBand == normalized) return;
    m_logbookBand = normalized;
    saveSetting(QStringLiteral("LogbookBand"), normalized);
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setLogbookMode(const QString& value)
{
    QString const normalized = value.trimmed().isEmpty()
        ? QStringLiteral("All") : value.trimmed();
    if (m_logbookMode == normalized) return;
    m_logbookMode = normalized;
    saveSetting(QStringLiteral("LogbookMode"), normalized);
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setLogbookPeriod(const QString& value)
{
    QString const normalized = value.trimmed().isEmpty()
        ? QStringLiteral("All time") : value.trimmed();
    if (m_logbookPeriod == normalized) return;
    m_logbookPeriod = normalized;
    saveSetting(QStringLiteral("LogbookPeriod"), normalized);
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setLogbookSort(const QString& value)
{
    QString const normalized = value.trimmed().isEmpty()
        ? QStringLiteral("Date") : value.trimmed();
    if (m_logbookSort == normalized) return;
    m_logbookSort = normalized;
    saveSetting(QStringLiteral("LogbookSort"), normalized);
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setLogbookSortDescending(bool descending)
{
    if (m_logbookSortDescending == descending) return;
    m_logbookSortDescending = descending;
    saveSetting(QStringLiteral("LogbookSortDescending"), descending);
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setLogbookLimit(int limit)
{
    int const bounded = qBound(50, limit, 5000);
    if (m_logbookLimit == bounded) return;
    m_logbookLimit = bounded;
    saveSetting(QStringLiteral("LogbookLimit"), bounded);
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setRotatorHost(const QString& host)
{
    QString const normalized = host.trimmed().left(255);
    if (normalized.isEmpty() || m_rotatorHost == normalized) return;
    m_rotatorHost = normalized;
    if (m_rotatorService) m_rotatorService->setHost(normalized);
    saveSetting(QStringLiteral("RotatorHost"), normalized);
    emit rotatorSettingsChanged();
}

void MapOperationsService::setRotatorPort(int port)
{
    int const bounded = qBound(1, port, 65535);
    if (m_rotatorPort == bounded) return;
    m_rotatorPort = bounded;
    if (m_rotatorService) m_rotatorService->setPort(bounded);
    saveSetting(QStringLiteral("RotatorPort"), bounded);
    emit rotatorSettingsChanged();
}

void MapOperationsService::setRotatorEnabled(bool enabled)
{
    if (m_rotatorEnabled == enabled) return;
    m_rotatorEnabled = enabled;
    if (m_rotatorService) m_rotatorService->setEnabled(enabled);
    saveSetting(QStringLiteral("RotatorEnabled"), enabled);
    emit rotatorSettingsChanged();
}

void MapOperationsService::setRotatorProtocol(const QString& protocol)
{
    if (!m_rotatorService) return;
    bool const followedDefaultPort =
        m_rotatorPort == RotatorService::defaultPortForProtocol(m_rotatorProtocol);
    m_rotatorService->setProtocol(protocol);
    m_rotatorProtocol = m_rotatorService->protocol();
    if (followedDefaultPort) {
        int const recommendedPort =
            RotatorService::defaultPortForProtocol(m_rotatorProtocol);
        if (recommendedPort != m_rotatorPort)
            setRotatorPort(recommendedPort);
    }
    saveSetting(QStringLiteral("RotatorProtocol"), m_rotatorProtocol);
    emit rotatorSettingsChanged();
}

void MapOperationsService::setRotatorTrackingIntervalMs(int intervalMs)
{
    if (!m_rotatorService) return;
    m_rotatorService->setTrackingIntervalMs(intervalMs);
    m_rotatorTrackingIntervalMs = m_rotatorService->trackingIntervalMs();
    saveSetting(QStringLiteral("RotatorTrackingIntervalMs"), m_rotatorTrackingIntervalMs);
    emit rotatorSettingsChanged();
}

void MapOperationsService::setRotatorSafetyEnabled(bool enabled)
{
    if (!m_rotatorService) return;
    m_rotatorService->setSafetyEnabled(enabled);
    m_rotatorSafetyEnabled = m_rotatorService->safetyEnabled();
    saveSetting(QStringLiteral("RotatorSafetyEnabled"), m_rotatorSafetyEnabled);
    emit rotatorSafetyChanged();
}

void MapOperationsService::setRotatorMinAzimuth(double value)
{
    if (!m_rotatorService) return;
    m_rotatorService->setMinAzimuth(value);
    m_rotatorMinAzimuth = m_rotatorService->minAzimuth();
    saveSetting(QStringLiteral("RotatorMinAzimuth"), m_rotatorMinAzimuth);
    emit rotatorSafetyChanged();
}

void MapOperationsService::setRotatorMaxAzimuth(double value)
{
    if (!m_rotatorService) return;
    m_rotatorService->setMaxAzimuth(value);
    m_rotatorMaxAzimuth = m_rotatorService->maxAzimuth();
    saveSetting(QStringLiteral("RotatorMaxAzimuth"), m_rotatorMaxAzimuth);
    emit rotatorSafetyChanged();
}

void MapOperationsService::setRotatorMinElevation(double value)
{
    if (!m_rotatorService) return;
    m_rotatorService->setMinElevation(value);
    m_rotatorMinElevation = m_rotatorService->minElevation();
    saveSetting(QStringLiteral("RotatorMinElevation"), m_rotatorMinElevation);
    emit rotatorSafetyChanged();
}

void MapOperationsService::setRotatorMaxElevation(double value)
{
    if (!m_rotatorService) return;
    m_rotatorService->setMaxElevation(value);
    m_rotatorMaxElevation = m_rotatorService->maxElevation();
    saveSetting(QStringLiteral("RotatorMaxElevation"), m_rotatorMaxElevation);
    emit rotatorSafetyChanged();
}

void MapOperationsService::setRotatorParkOnStop(bool enabled)
{
    if (!m_rotatorService) return;
    m_rotatorService->setParkOnStop(enabled);
    m_rotatorParkOnStop = m_rotatorService->parkOnStop();
    saveSetting(QStringLiteral("RotatorParkOnStop"), m_rotatorParkOnStop);
    emit rotatorSafetyChanged();
}

void MapOperationsService::setRotatorParkAzimuth(double value)
{
    if (!m_rotatorService) return;
    m_rotatorService->setParkAzimuth(value);
    m_rotatorParkAzimuth = m_rotatorService->parkAzimuth();
    saveSetting(QStringLiteral("RotatorParkAzimuth"), m_rotatorParkAzimuth);
    emit rotatorSafetyChanged();
}

void MapOperationsService::setRotatorParkElevation(double value)
{
    if (!m_rotatorService) return;
    m_rotatorService->setParkElevation(value);
    m_rotatorParkElevation = m_rotatorService->parkElevation();
    saveSetting(QStringLiteral("RotatorParkElevation"), m_rotatorParkElevation);
    emit rotatorSafetyChanged();
}

void MapOperationsService::setOperatorCall(const QString& call)
{
    QString normalized = call.trimmed().toUpper();
    if (m_operatorCall == normalized) {
        return;
    }
    m_operatorCall = normalized;
    emit operatorCallChanged();

    if (m_potaSpots.isEmpty()) {
        return;
    }
    QVariantList updatedSpots;
    QVariantList updatedMarkers;
    updatedSpots.reserve(m_potaSpots.size());
    updatedMarkers.reserve(m_potaSpots.size());
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    for (QVariant const& value : std::as_const(m_potaSpots)) {
        QVariantMap spot = value.toMap();
        QVariantMap const state = potaSpotState(spot, m_operatorCall, nowMs);
        if (!state.value(QStringLiteral("valid")).toBool()) {
            continue;
        }
        for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
            spot.insert(it.key(), it.value());
        }
        updatedSpots << spot;
        QVariantMap marker = markerFromPotaSpot(spot);
        for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
            marker.insert(it.key(), it.value());
        }
        updatedMarkers << marker;
    }
    if (m_potaSpots != updatedSpots || m_potaMarkers != updatedMarkers) {
        m_potaSpots = updatedSpots;
        m_potaMarkers = updatedMarkers;
        if (m_layerModel) {
            m_layerModel->setCount(QStringLiteral("pota"), m_potaSpots.size());
        }
        rebuildOperationalMarkers();
        emit potaSpotsChanged();
    }
}

void MapOperationsService::setOfflineMode(bool offline)
{
    if (m_offlineMode == offline) {
        return;
    }

    m_offlineMode = offline;
    if (offline) {
        ++m_geoGeneration;
        ++m_iotaGeneration;
        for (QNetworkReply* reply : m_network->findChildren<QNetworkReply*>()) {
            if (reply) {
                reply->abort();
            }
        }
        m_geoPendingLayers.clear();
        m_iotaLoading = false;
        setPotaLoading(false);
        setGeographicLoading(false);
        setStatusMessage(QStringLiteral("Offline: POTA, IOTA e confini usano solo la cache locale"));
        refreshGeographicFeatures();
        rebuildOperationalMarkers();
    } else {
        setStatusMessage(QStringLiteral("Online: aggiornamento dei dati mappa abilitato"));
        if (m_layerModel && m_layerModel->layerEnabled(QStringLiteral("pota"))) {
            refreshPota();
        }
        if (m_layerModel
            && (m_layerModel->layerEnabled(QStringLiteral("states"))
                || m_layerModel->layerEnabled(QStringLiteral("counties")))) {
            refreshGeographicFeatures();
        }
        if (m_layerModel && m_layerModel->layerEnabled(QStringLiteral("iota"))) {
            ensureIotaCatalog();
        }
    }
    emit offlineModeChanged();
}

void MapOperationsService::refreshPota()
{
    if (m_offlineMode || m_potaLoading) {
        return;
    }
    setPotaLoading(true);
    QNetworkRequest request(QUrl(QStringLiteral(
        "https://api.pota.app/spot/activator")));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Decodium4 Map Intelligence"));
    request.setTransferTimeout(kNetworkTimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply] { handlePotaReply(reply); });
}

void MapOperationsService::handlePotaReply(QNetworkReply* reply)
{
    if (m_offlineMode) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
        setPotaLoading(false);
        return;
    }
    QByteArray const bytes = reply->readAll();
    QString const networkError = reply->error() == QNetworkReply::NoError
        ? QString() : reply->errorString();
    reply->deleteLater();
    setPotaLoading(false);
    if (!networkError.isEmpty()) {
        setStatusMessage(QStringLiteral("POTA update failed: %1").arg(networkError));
        return;
    }

    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        setStatusMessage(QStringLiteral("POTA returned invalid data"));
        return;
    }

    QVariantList spots;
    QVariantList markers;
    QJsonArray const values = document.array();
    spots.reserve(qMin(values.size(), kMaxPotaSpots));
    markers.reserve(qMin(values.size(), kMaxPotaSpots));
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    for (QJsonValue const& value : values) {
        if (spots.size() >= kMaxPotaSpots || !value.isObject()) break;
        QVariantMap spot = value.toObject().toVariantMap();
        QString const reference = spot.value(QStringLiteral("reference")).toString().toUpper();
        bool latitudeOk = false;
        bool longitudeOk = false;
        double const latitude =
            spot.value(QStringLiteral("latitude")).toDouble(&latitudeOk);
        double const longitude =
            spot.value(QStringLiteral("longitude")).toDouble(&longitudeOk);
        if (reference.isEmpty() || !latitudeOk || !longitudeOk
            || qAbs(latitude) > 90.0 || qAbs(longitude) > 180.0) {
            continue;
        }
        spot.insert(QStringLiteral("reference"), reference);
        spot.insert(QStringLiteral("parkName"),
                    spot.value(QStringLiteral("name")).toString());
        QVariantMap const state = potaSpotState(spot, m_operatorCall, nowMs);
        if (!state.value(QStringLiteral("valid")).toBool()) {
            continue;
        }
        for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
            spot.insert(it.key(), it.value());
        }
        QVariantMap marker = markerFromPotaSpot(spot);
        for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
            marker.insert(it.key(), it.value());
        }
        spots << spot;
        markers << marker;
    }
    m_potaSpots = spots;
    m_potaMarkers = markers;
    if (m_layerModel) {
        m_layerModel->setCount(QStringLiteral("pota"), spots.size());
    }
    rebuildOperationalMarkers();
    emit potaSpotsChanged();
    setStatusMessage(QStringLiteral("POTA: %1 active parks").arg(spots.size()));
}

void MapOperationsService::selectPotaPark(const QString& reference)
{
    QString const normalized = reference.trimmed().toUpper();
    if (normalized.isEmpty()) return;

    for (QVariant const& value : std::as_const(m_potaSpots)) {
        QVariantMap spot = value.toMap();
        if (spot.value(QStringLiteral("reference")).toString() == normalized) {
            m_selectedPotaPark = spot;
            emit selectedPotaParkChanged();
            break;
        }
    }

    if (m_offlineMode) {
        setStatusMessage(QStringLiteral("Offline: dettagli POTA remoti non disponibili; uso lo spot in cache"));
        return;
    }

    QNetworkRequest request(
        QUrl(QStringLiteral("https://api.pota.app/park/%1").arg(normalized)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Decodium4 Map Intelligence"));
    request.setTransferTimeout(kNetworkTimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply] { handlePotaParkReply(reply); });
}

void MapOperationsService::handlePotaParkReply(QNetworkReply* reply)
{
    if (m_offlineMode) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
        return;
    }
    QByteArray const bytes = reply->readAll();
    bool const ok = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();
    if (!ok) return;
    QJsonDocument const document = QJsonDocument::fromJson(bytes);
    if (!document.isObject()) return;
    QVariantMap details = document.object().toVariantMap();
    for (auto it = m_selectedPotaPark.constBegin();
         it != m_selectedPotaPark.constEnd(); ++it) {
        if (!details.contains(it.key())) details.insert(it.key(), it.value());
    }
    m_selectedPotaPark = details;
    emit selectedPotaParkChanged();
}

void MapOperationsService::clearSelectedPotaPark()
{
    if (m_selectedPotaPark.isEmpty()) return;
    m_selectedPotaPark.clear();
    emit selectedPotaParkChanged();
}

void MapOperationsService::pruneExpiredPotaSpots()
{
    if (m_potaSpots.isEmpty()) {
        return;
    }

    QVariantList activeSpots;
    QVariantList activeMarkers;
    activeSpots.reserve(m_potaSpots.size());
    activeMarkers.reserve(m_potaSpots.size());
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    for (QVariant const& value : std::as_const(m_potaSpots)) {
        QVariantMap spot = value.toMap();
        QVariantMap const state = potaSpotState(spot, m_operatorCall, nowMs);
        if (!state.value(QStringLiteral("valid")).toBool()) {
            continue;
        }
        for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
            spot.insert(it.key(), it.value());
        }
        QVariantMap marker = markerFromPotaSpot(spot);
        for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
            marker.insert(it.key(), it.value());
        }
        activeSpots << spot;
        activeMarkers << marker;
    }
    if (activeSpots == m_potaSpots && activeMarkers == m_potaMarkers) {
        return;
    }
    m_potaSpots = activeSpots;
    m_potaMarkers = activeMarkers;
    if (m_layerModel) {
        m_layerModel->setCount(QStringLiteral("pota"), m_potaSpots.size());
    }
    rebuildOperationalMarkers();
    emit potaSpotsChanged();
}

QVariantMap MapOperationsService::preparePotaAction(
    const QVariantMap& spot, const QString& operatorCall) const
{
    QString const effectiveOperator = operatorCall.trimmed().isEmpty()
        ? m_operatorCall : operatorCall.trimmed().toUpper();
    QVariantMap action = potaSpotState(
        spot, effectiveOperator, QDateTime::currentMSecsSinceEpoch());
    QString const grid = spot.value(QStringLiteral("grid6")).toString().trimmed()
        .toUpper();
    QString const mode = spot.value(QStringLiteral("mode")).toString().trimmed()
        .toUpper();
    QString const activator = spot.value(QStringLiteral("activator"))
        .toString().trimmed().toUpper();
    double frequency = spot.value(QStringLiteral("frequency")).toDouble();
    if (frequency > 0.0) {
        // POTA publishes kHz (e.g. 14074.0).  Accept MHz as well for
        // imported/cached entries whose value is below 1000.
        frequency = frequency < 1000.0 ? frequency * 1000000.0
                                       : frequency * 1000.0;
    }
    bool const digital = mode == QStringLiteral("FT8")
        || mode == QStringLiteral("FT4")
        || mode == QStringLiteral("JT9")
        || mode == QStringLiteral("JT65")
        || mode == QStringLiteral("Q65")
        || mode == QStringLiteral("MSK144")
        || mode == QStringLiteral("FST4")
        || mode == QStringLiteral("FST4W")
        || mode == QStringLiteral("JS8");
    action.insert(QStringLiteral("targetCall"),
                  action.value(QStringLiteral("role")).toString()
                          == QStringLiteral("HUNTER")
                      ? activator : QString());
    action.insert(QStringLiteral("targetGrid"), grid);
    action.insert(QStringLiteral("frequencyHz"), frequency);
    action.insert(QStringLiteral("mode"), mode);
    action.insert(QStringLiteral("digitalMode"), digital);
    action.insert(QStringLiteral("messageReady"),
                  action.value(QStringLiteral("valid")).toBool()
                  && !activator.isEmpty()
                  && digital
                  && action.value(QStringLiteral("role")).toString()
                         == QStringLiteral("HUNTER"));
    if (!action.value(QStringLiteral("valid")).toBool()) {
        action.insert(QStringLiteral("reason"),
                      action.value(QStringLiteral("expired")).toBool()
                          ? QStringLiteral("Spot scaduto")
                          : QStringLiteral("Spot non valido"));
    } else if (activator.isEmpty()) {
        action.insert(QStringLiteral("reason"), QStringLiteral("Attivatore non disponibile"));
    } else if (action.value(QStringLiteral("role")).toString()
                   == QStringLiteral("ACTIVATOR")) {
        action.insert(QStringLiteral("reason"),
                      QStringLiteral("Sei l'attivatore di questo parco"));
    } else if (!digital) {
        action.insert(QStringLiteral("reason"),
                      QStringLiteral("Spot %1: nessun messaggio WSJT-X").arg(mode));
    } else {
        action.insert(QStringLiteral("reason"),
                      QStringLiteral("Hunter: messaggio WSJT-X pronto"));
    }
    return action;
}

QVariantMap MapOperationsService::potaSpotState(const QVariantMap& spot,
                                                 const QString& operatorCall,
                                                 qint64 nowMs)
{
    auto canonicalCall = [](QString value) {
        value = value.trimmed().toUpper();
        QStringList const parts = value.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            return value;
        }
        QString longest = parts.constFirst();
        for (QString const& part : parts) {
            if (part.size() > longest.size()) {
                longest = part;
            }
        }
        return longest;
    };

    QString const activator = spot.value(QStringLiteral("activator"))
        .toString().trimmed().toUpper();
    QString const localCall = operatorCall.trimmed().toUpper();
    bool const isActivator = !activator.isEmpty() && !localCall.isEmpty()
        && (activator.compare(localCall, Qt::CaseInsensitive) == 0
            || canonicalCall(activator).compare(canonicalCall(localCall),
                                                Qt::CaseInsensitive) == 0);

    QString const spotTimeText = spot.value(QStringLiteral("spotTime"))
        .toString().trimmed();
    bool const hasExplicitTimeZone = spotTimeText.endsWith(QLatin1Char('Z'))
        || spotTimeText.contains(QLatin1Char('+'))
        || spotTimeText.contains(QRegularExpression(QStringLiteral("-\\d\\d:\\d\\d$")));
    QString const parseSpotTimeText = hasExplicitTimeZone
        ? spotTimeText : spotTimeText + QLatin1Char('Z');
    QDateTime spotTime = QDateTime::fromString(parseSpotTimeText,
                                               Qt::ISODateWithMs);
    if (!spotTime.isValid()) {
        spotTime = QDateTime::fromString(parseSpotTimeText, Qt::ISODate);
    }
    qint64 const spotTimeMs = spotTime.isValid()
        ? spotTime.toUTC().toMSecsSinceEpoch() : 0;
    bool expireOk = false;
    qint64 const expireSeconds = spot.value(QStringLiteral("expire"))
        .toLongLong(&expireOk);
    bool const hasExpiry = expireOk && spotTimeMs > 0;
    qint64 const expiresAtMs = hasExpiry
        ? spotTimeMs + expireSeconds * 1000LL : 0;
    bool invalid = spot.value(QStringLiteral("invalid")).toBool();
    QString const invalidText = spot.value(QStringLiteral("invalid"))
        .toString().trimmed().toLower();
    if (invalidText == QStringLiteral("true")
        || invalidText == QStringLiteral("yes")) {
        invalid = true;
    }
    bool const expired = hasExpiry && nowMs >= expiresAtMs;
    bool const valid = !invalid && !expired;
    qint64 const remainingSeconds = hasExpiry
        ? qMax<qint64>(0, (expiresAtMs - nowMs + 999) / 1000) : 0;
    qint64 const ageSeconds = spotTimeMs > 0
        ? qMax<qint64>(0, (nowMs - spotTimeMs) / 1000) : 0;

    return {
        {QStringLiteral("role"), isActivator
             ? QStringLiteral("ACTIVATOR") : QStringLiteral("HUNTER")},
        {QStringLiteral("spotValid"), valid},
        {QStringLiteral("valid"), valid},
        {QStringLiteral("invalid"), invalid},
        {QStringLiteral("expired"), expired},
        {QStringLiteral("spotTimeMs"), spotTimeMs},
        {QStringLiteral("expiresAtMs"), expiresAtMs},
        {QStringLiteral("remainingSeconds"), remainingSeconds},
        {QStringLiteral("spotAgeSeconds"), ageSeconds},
        {QStringLiteral("expirySeconds"), expireSeconds}
    };
}

QVariantMap MapOperationsService::markerFromPotaSpot(const QVariantMap& spot)
{
    QString const reference =
        spot.value(QStringLiteral("reference")).toString().toUpper();
    QString const activator =
        spot.value(QStringLiteral("activator")).toString().toUpper();
    QVariantMap marker {
        {QStringLiteral("id"), QStringLiteral("pota:%1:%2").arg(reference, activator)},
        {QStringLiteral("type"), QStringLiteral("POTA")},
        {QStringLiteral("reference"), reference},
        {QStringLiteral("call"), activator},
        {QStringLiteral("label"), reference},
        {QStringLiteral("latitude"), spot.value(QStringLiteral("latitude"))},
        {QStringLiteral("longitude"), spot.value(QStringLiteral("longitude"))},
        {QStringLiteral("grid"), spot.value(QStringLiteral("grid6"))},
        {QStringLiteral("name"), spot.value(QStringLiteral("parkName"))},
        {QStringLiteral("frequency"), spot.value(QStringLiteral("frequency"))},
        {QStringLiteral("mode"), spot.value(QStringLiteral("mode"))},
        {QStringLiteral("comments"), spot.value(QStringLiteral("comments"))},
        {QStringLiteral("activator"), activator},
        {QStringLiteral("spotter"), spot.value(QStringLiteral("spotter"))},
        {QStringLiteral("source"), spot.value(QStringLiteral("source"))},
        {QStringLiteral("color"), QStringLiteral("#74d66a")}
    };
    QVariantMap const state = potaSpotState(
        spot, QString(), QDateTime::currentMSecsSinceEpoch());
    for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
        marker.insert(it.key(), it.value());
    }
    return marker;
}

QString MapOperationsService::iotaCachePath() const
{
    QFileInfo const databaseInfo(m_databasePath);
    return databaseInfo.dir().filePath(QStringLiteral("iota_groups.json"));
}

void MapOperationsService::ensureIotaCatalog()
{
    if (m_iotaLoading || !m_iotaCatalogMarkers.isEmpty()) {
        return;
    }

    QString const cachePath = iotaCachePath();
    QFile cache(cachePath);
    if (cache.open(QIODevice::ReadOnly)) {
        QByteArray const data = cache.readAll();
        QFileInfo const cacheInfo(cachePath);
        qint64 const ageSeconds =
            cacheInfo.lastModified().toUTC().secsTo(QDateTime::currentDateTimeUtc());
        bool const stale = ageSeconds < 0
            || ageSeconds > kIotaCacheMaxAgeSeconds;
        parseIotaCatalog(data, false, stale);
        return;
    }

    requestIotaCatalog();
}

void MapOperationsService::refreshIotaCatalog()
{
    if (m_offlineMode) {
        ensureIotaCatalog();
        setStatusMessage(QStringLiteral("Offline: catalogo IOTA locale"));
        return;
    }
    requestIotaCatalog();
}

void MapOperationsService::requestIotaCatalog()
{
    if (m_offlineMode || m_iotaLoading) {
        return;
    }
    m_iotaLoading = true;
    QNetworkRequest request(QUrl(QString::fromLatin1(kIotaCatalogUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Decodium4 Map Intelligence"));
    request.setTransferTimeout(kNetworkTimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply] { handleIotaCatalogReply(reply); });
}

void MapOperationsService::handleIotaCatalogReply(QNetworkReply* reply)
{
    if (m_offlineMode) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
        m_iotaLoading = false;
        return;
    }
    QByteArray const bytes = reply->readAll();
    QString const networkError = reply->error() == QNetworkReply::NoError
        ? QString() : reply->errorString();
    reply->deleteLater();
    if (!networkError.isEmpty()) {
        m_iotaLoading = false;
        setStatusMessage(
            m_iotaCatalogMarkers.isEmpty()
                ? QStringLiteral("IOTA catalog update failed: %1")
                      .arg(networkError)
                : QStringLiteral("IOTA catalog update failed; cached groups remain available"));
        return;
    }
    parseIotaCatalog(bytes, true, false);
}

void MapOperationsService::parseIotaCatalog(const QByteArray& data,
                                            bool persist,
                                            bool refreshAfterParse)
{
    m_iotaLoading = true;
    quint64 const generation = ++m_iotaGeneration;
    auto* watcher = new QFutureWatcher<IotaSnapshot>(this);
    connect(watcher, &QFutureWatcher<IotaSnapshot>::finished, this,
            [this, watcher, generation, data, persist, refreshAfterParse] {
        IotaSnapshot snapshot = watcher->result();
        watcher->deleteLater();
        if (generation != m_iotaGeneration.load()) {
            return;
        }
        m_iotaLoading = false;
        if (!snapshot.error.isEmpty()) {
            setStatusMessage(snapshot.error);
            if (!persist && m_iotaCatalogMarkers.isEmpty()) {
                requestIotaCatalog();
            }
            return;
        }

        m_iotaCatalogMarkers = snapshot.markers;
        if (persist) {
            QString const cachePath = iotaCachePath();
            QDir().mkpath(QFileInfo(cachePath).absolutePath());
            QSaveFile cache(cachePath);
            if (cache.open(QIODevice::WriteOnly)) {
                cache.write(data);
                cache.commit();
            }
        }
        rebuildOperationalMarkers();
        setStatusMessage(QStringLiteral("IOTA: %1 catalog groups")
                             .arg(m_iotaCatalogMarkers.size()));
        if (refreshAfterParse) {
            requestIotaCatalog();
        }
    });
    watcher->setFuture(QtConcurrent::run(
        [data] { return parseIotaCatalogData(data); }));
}

MapOperationsService::IotaSnapshot
MapOperationsService::parseIotaCatalogData(const QByteArray& data)
{
    IotaSnapshot snapshot;
    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        snapshot.error = QStringLiteral("IOTA catalog contains invalid JSON");
        return snapshot;
    }

    static QRegularExpression const referencePattern(
        QStringLiteral("^[A-Z]{2}-\\d{3}$"));
    QJsonArray const groups = document.array();
    snapshot.markers.reserve(groups.size());
    for (QJsonValue const& value : groups) {
        if (!value.isObject()) {
            continue;
        }
        QJsonObject const group = value.toObject();
        QString const reference =
            group.value(QStringLiteral("refno")).toString().trimmed().toUpper();
        if (!referencePattern.match(reference).hasMatch()) {
            continue;
        }

        bool latitudeMinOk = false;
        bool latitudeMaxOk = false;
        bool longitudeMinOk = false;
        bool longitudeMaxOk = false;
        double const latitudeMin =
            group.value(QStringLiteral("latitude_min")).toVariant()
                .toString().toDouble(&latitudeMinOk);
        double const latitudeMax =
            group.value(QStringLiteral("latitude_max")).toVariant()
                .toString().toDouble(&latitudeMaxOk);
        double const longitudeMin =
            group.value(QStringLiteral("longitude_min")).toVariant()
                .toString().toDouble(&longitudeMinOk);
        double const longitudeMax =
            group.value(QStringLiteral("longitude_max")).toVariant()
                .toString().toDouble(&longitudeMaxOk);
        if (!latitudeMinOk || !latitudeMaxOk || !longitudeMinOk
            || !longitudeMaxOk
            || qAbs(latitudeMin) > 90.0 || qAbs(latitudeMax) > 90.0
            || qAbs(longitudeMin) > 180.0 || qAbs(longitudeMax) > 180.0) {
            continue;
        }

        double longitudeA = longitudeMin;
        double longitudeB = longitudeMax;
        if (qAbs(longitudeA - longitudeB) > 180.0) {
            if (longitudeA < 0.0) longitudeA += 360.0;
            if (longitudeB < 0.0) longitudeB += 360.0;
        }
        double longitude = 0.5 * (longitudeA + longitudeB);
        if (longitude > 180.0) longitude -= 360.0;
        double const latitude = 0.5 * (latitudeMin + latitudeMax);
        QString const name =
            group.value(QStringLiteral("name")).toString().trimmed();
        QString const comment =
            group.value(QStringLiteral("comment")).toString().trimmed();

        snapshot.markers << QVariantMap {
            {QStringLiteral("id"),
             QStringLiteral("iota-catalog:%1").arg(reference)},
            {QStringLiteral("type"), QStringLiteral("IOTA")},
            {QStringLiteral("reference"), reference},
            {QStringLiteral("label"), reference},
            {QStringLiteral("name"), name},
            {QStringLiteral("latitude"), latitude},
            {QStringLiteral("longitude"), longitude},
            {QStringLiteral("latitudeMin"), latitudeMin},
            {QStringLiteral("latitudeMax"), latitudeMax},
            {QStringLiteral("longitudeMin"), longitudeMin},
            {QStringLiteral("longitudeMax"), longitudeMax},
            {QStringLiteral("dxcc"),
             group.value(QStringLiteral("dxcc_num")).toString()},
            {QStringLiteral("creditedPercent"),
             group.value(QStringLiteral("pc_credited")).toString()},
            {QStringLiteral("comments"),
             comment.isEmpty()
                 ? QStringLiteral("Official IOTA Directory catalog group")
                 : comment},
            {QStringLiteral("source"), QStringLiteral("IOTA Directory")},
            {QStringLiteral("catalog"), true},
            {QStringLiteral("worked"), false},
            {QStringLiteral("confirmed"), false},
            {QStringLiteral("color"), QStringLiteral("#44d7e8")}
        };
    }
    if (snapshot.markers.isEmpty()) {
        snapshot.error = QStringLiteral("IOTA catalog contains no usable groups");
    }
    return snapshot;
}

void MapOperationsService::refreshGeographicFeatures()
{
    if (!m_layerModel) return;
    // Invalidate a reply for a layer that was turned off while its network
    // request or GeoJSON parsing task was still in flight.
    for (QString const& layerId : {QStringLiteral("states"),
                                  QStringLiteral("counties")}) {
        if (!m_layerModel->layerEnabled(layerId)) {
            m_geoLayerGeneration.insert(layerId, ++m_geoGeneration);
        }
    }
    if (m_offlineMode) {
        m_geographicFeatures.clear();
        if (m_layerModel->layerEnabled(QStringLiteral("states"))) {
            m_geographicFeatures.append(m_stateFeatures);
        }
        if (m_layerModel->layerEnabled(QStringLiteral("counties"))) {
            m_geographicFeatures.append(m_countyFeatures);
        }
        emit geographicFeaturesChanged();
        return;
    }
    bool requested = false;
    if (m_layerModel->layerEnabled(QStringLiteral("states"))
        && m_stateFeatures.isEmpty()) {
        QUrl url(QStringLiteral(
            "https://tigerweb.geo.census.gov/arcgis/rest/services/"
            "TIGERweb/State_County/MapServer/15/query"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("where"), QStringLiteral("1=1"));
        query.addQueryItem(QStringLiteral("outFields"),
                           QStringLiteral("STATE,STUSAB,BASENAME,NAME"));
        query.addQueryItem(QStringLiteral("returnGeometry"), QStringLiteral("true"));
        query.addQueryItem(QStringLiteral("maxAllowableOffset"), QStringLiteral("0.04"));
        query.addQueryItem(QStringLiteral("geometryPrecision"), QStringLiteral("3"));
        query.addQueryItem(QStringLiteral("outSR"), QStringLiteral("4326"));
        query.addQueryItem(QStringLiteral("f"), QStringLiteral("geojson"));
        url.setQuery(query);
        requestGeoLayer(QStringLiteral("states"), url);
        requested = true;
    }
    if (m_layerModel->layerEnabled(QStringLiteral("counties"))
        && m_countyFeatures.isEmpty()) {
        QUrl url(QStringLiteral(
            "https://tigerweb.geo.census.gov/arcgis/rest/services/"
            "TIGERweb/State_County/MapServer/13/query"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("where"), QStringLiteral("1=1"));
        query.addQueryItem(QStringLiteral("outFields"),
                           QStringLiteral("STATE,COUNTY,BASENAME,NAME"));
        query.addQueryItem(QStringLiteral("returnGeometry"), QStringLiteral("true"));
        query.addQueryItem(QStringLiteral("maxAllowableOffset"), QStringLiteral("0.03"));
        query.addQueryItem(QStringLiteral("geometryPrecision"), QStringLiteral("3"));
        query.addQueryItem(QStringLiteral("outSR"), QStringLiteral("4326"));
        query.addQueryItem(QStringLiteral("f"), QStringLiteral("geojson"));
        url.setQuery(query);
        requestGeoLayer(QStringLiteral("counties"), url);
        requested = true;
    }
    if (!requested) {
        m_geographicFeatures.clear();
        if (m_layerModel->layerEnabled(QStringLiteral("states"))) {
            m_geographicFeatures.append(m_stateFeatures);
        }
        if (m_layerModel->layerEnabled(QStringLiteral("counties"))) {
            m_geographicFeatures.append(m_countyFeatures);
        }
        emit geographicFeaturesChanged();
    }
}

void MapOperationsService::requestGeoLayer(const QString& layerId,
                                           const QUrl& url)
{
    if (m_offlineMode || m_geoPendingLayers.contains(layerId)) {
        return;
    }
    quint64 const generation = ++m_geoGeneration;
    m_geoLayerGeneration.insert(layerId, generation);
    m_geoPendingLayers.insert(layerId);
    setGeographicLoading(true);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Decodium4 Map Intelligence"));
    request.setTransferTimeout(kNetworkTimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, layerId, generation, reply] {
                handleGeoReply(layerId, generation, reply);
            });
}

void MapOperationsService::handleGeoReply(const QString& layerId,
                                          quint64 generation,
                                          QNetworkReply* reply)
{
    if (m_offlineMode) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
        m_geoPendingLayers.remove(layerId);
        setGeographicLoading(!m_geoPendingLayers.isEmpty());
        return;
    }
    QByteArray const bytes = reply->readAll();
    QString const error = reply->error() == QNetworkReply::NoError
        ? QString() : reply->errorString();
    reply->deleteLater();
    if (!error.isEmpty()) {
        m_geoPendingLayers.remove(layerId);
        setGeographicLoading(!m_geoPendingLayers.isEmpty());
        setStatusMessage(QStringLiteral("%1 boundaries failed: %2")
                             .arg(layerId, error));
        return;
    }

    auto* watcher = new QFutureWatcher<GeoSnapshot>(this);
    connect(watcher, &QFutureWatcher<GeoSnapshot>::finished, this,
            [this, watcher, generation, layerId] {
        GeoSnapshot snapshot = watcher->result();
        watcher->deleteLater();
        m_geoPendingLayers.remove(layerId);
        setGeographicLoading(!m_geoPendingLayers.isEmpty());
        bool const stale = m_geoLayerGeneration.value(layerId) != generation;
        if (stale) {
            if (m_layerModel && m_layerModel->layerEnabled(layerId)) {
                refreshGeographicFeatures();
            }
            return;
        }
        if (!snapshot.error.isEmpty()) {
            setStatusMessage(snapshot.error);
            return;
        }
        if (layerId == QStringLiteral("states")) {
            m_stateFeatures = snapshot.features;
        } else {
            m_countyFeatures = snapshot.features;
        }
        if (m_layerModel) {
            m_layerModel->setCount(layerId, snapshot.features.size());
        }
        m_geographicFeatures.clear();
        if (m_layerModel && m_layerModel->layerEnabled(QStringLiteral("states"))) {
            m_geographicFeatures.append(m_stateFeatures);
        }
        if (m_layerModel && m_layerModel->layerEnabled(QStringLiteral("counties"))) {
            m_geographicFeatures.append(m_countyFeatures);
        }
        emit geographicFeaturesChanged();
        setStatusMessage(QStringLiteral("%1: %2 boundaries")
                             .arg(layerId).arg(snapshot.features.size()));
    });
    watcher->setFuture(QtConcurrent::run(
        [bytes, layerId] { return parseGeoJson(bytes, layerId); }));
}

MapOperationsService::GeoSnapshot
MapOperationsService::parseGeoJson(const QByteArray& data,
                                   const QString& layerId)
{
    GeoSnapshot snapshot;
    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        snapshot.error = QStringLiteral("%1 boundaries contain invalid GeoJSON")
                             .arg(layerId);
        return snapshot;
    }
    QJsonArray const features =
        document.object().value(QStringLiteral("features")).toArray();
    for (QJsonValue const& value : features) {
        if (snapshot.features.size() >= kMaxGeoFeatures || !value.isObject()) break;
        QJsonObject const feature = value.toObject();
        QJsonObject const geometry =
            feature.value(QStringLiteral("geometry")).toObject();
        QJsonObject const properties =
            feature.value(QStringLiteral("properties")).toObject();
        QString const geometryType =
            geometry.value(QStringLiteral("type")).toString();
        QVariantList polygonRings;
        if (geometryType == QStringLiteral("Polygon")) {
            QVariantList const rings =
                parseRings(geometry.value(QStringLiteral("coordinates")));
            if (!rings.isEmpty()) polygonRings << QVariant::fromValue(rings);
        } else if (geometryType == QStringLiteral("MultiPolygon")) {
            for (QJsonValue const& polygon :
                 geometry.value(QStringLiteral("coordinates")).toArray()) {
                QVariantList const rings = parseRings(polygon);
                if (!rings.isEmpty()) polygonRings << QVariant::fromValue(rings);
            }
        }
        if (polygonRings.isEmpty()) continue;
        QString const state = properties.value(QStringLiteral("STUSAB")).toString();
        QString const county = properties.value(QStringLiteral("BASENAME")).toString();
        QString const label = layerId == QStringLiteral("states")
            ? (!state.isEmpty() ? state : properties.value(QStringLiteral("NAME")).toString())
            : QStringLiteral("%1, %2").arg(county, state);
        QVariantMap row {
            {QStringLiteral("id"),
             QStringLiteral("%1:%2:%3")
                 .arg(layerId,
                      properties.value(QStringLiteral("STATE")).toVariant().toString(),
                      properties.value(QStringLiteral("COUNTY")).toVariant().toString())},
            {QStringLiteral("type"), layerId},
            {QStringLiteral("label"), label},
            {QStringLiteral("state"), state},
            {QStringLiteral("county"), county},
            {QStringLiteral("polygons"), polygonRings},
            {QStringLiteral("color"),
             layerId == QStringLiteral("states")
                 ? QStringLiteral("#58b8d6") : QStringLiteral("#7c91a8")}
        };
        snapshot.features << row;
    }
    return snapshot;
}

void MapOperationsService::refreshLogbook()
{
    quint64 const generation = ++m_logbookGeneration;
    setLogbookLoading(true);
    QString const databasePath = m_databasePath;
    QString const search = m_logbookSearch;
    QString const band = m_logbookBand;
    QString const mode = m_logbookMode;
    QString const period = m_logbookPeriod;
    QString const sort = m_logbookSort;
    bool const descending = m_logbookSortDescending;
    int const limit = m_logbookLimit;

    auto* watcher = new QFutureWatcher<LogbookSnapshot>(this);
    connect(watcher, &QFutureWatcher<LogbookSnapshot>::finished, this,
            [this, watcher, generation] {
        LogbookSnapshot snapshot = watcher->result();
        watcher->deleteLater();
        if (generation != m_logbookGeneration.load()) return;
        setLogbookLoading(false);
        if (!snapshot.error.isEmpty()) {
            setStatusMessage(QStringLiteral("Logbook: %1").arg(snapshot.error));
            return;
        }
        m_logbookRows = snapshot.rows;
        m_logbookTotal = snapshot.total;
        m_scorecard = snapshot.scorecard;
        m_chartData = snapshot.chartData;
        m_comparison = snapshot.comparison;
        m_awardProgression = snapshot.awardProgression;
        m_topStatistics = snapshot.topStatistics;
        m_periodComparison = snapshot.periodComparison;
        m_profileStatistics = snapshot.profileStatistics;
        m_databaseMarkers = snapshot.markers;
        rebuildOperationalMarkers();
        emit logbookChanged();
        emit statisticsChanged();
    });
    watcher->setFuture(QtConcurrent::run(
        [databasePath, search, band, mode, period, sort, descending, limit] {
        return queryLogbookDatabase(databasePath, search, band, mode, period,
                                    sort, descending, limit);
    }));
}

MapOperationsService::LogbookSnapshot
MapOperationsService::queryLogbookDatabase(
    const QString& databasePath, const QString& search,
    const QString& band, const QString& mode, const QString& period,
    const QString& sort, bool descending, int limit)
{
    LogbookSnapshot snapshot;
    ScopedMapDatabase connection(databasePath);
    QSqlDatabase& db = connection.database();
    if (!db.open()) {
        snapshot.error = db.lastError().text();
        return snapshot;
    }
    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA busy_timeout=3000"));

    SqlFilter const filter = buildFilter(search, band, mode, period);
    QSqlQuery countQuery(db);
    if (!prepareAndBind(&countQuery,
                        QStringLiteral("SELECT count(*) FROM map_qso WHERE %1")
                            .arg(filter.where),
                        filter.binds, &snapshot.error)) {
        return snapshot;
    }
    if (countQuery.next()) snapshot.total = countQuery.value(0).toInt();

    QString const selectSql = QStringLiteral(
        "SELECT source_key,call,grid,band,mode,qso_date,time_on,qso_epoch,"
        " frequency_mhz,satellite,sat_mode,freq_rx_mhz,confirmed,dxcc,continent,state,pota_ref,iota,wpx,source,"
        " COALESCE((SELECT GROUP_CONCAT(g.grid, ', ') FROM map_qso_grid g"
        "           WHERE g.qso_id=map_qso.id AND g.is_primary=0), '')"
        " FROM map_qso WHERE %1 ORDER BY %2 %3 LIMIT %4")
        .arg(filter.where, sortableColumn(sort),
             descending ? QStringLiteral("DESC") : QStringLiteral("ASC"))
        .arg(qBound(50, limit, 5000));
    QSqlQuery rowQuery(db);
    if (!prepareAndBind(&rowQuery, selectSql, filter.binds, &snapshot.error)) {
        return snapshot;
    }
    while (rowQuery.next()) {
        snapshot.rows << rowToMap(rowQuery);
    }

    QSqlQuery scoreQuery(db);
    QString const scoreSql = QStringLiteral(
        "SELECT count(*),sum(CASE WHEN confirmed<>0 THEN 1 ELSE 0 END),"
        " count(DISTINCT upper(call)),count(DISTINCT upper(dxcc)),"
        " count(DISTINCT upper(grid4)),"
        " count(DISTINCT CASE WHEN pota_ref<>'' THEN upper(pota_ref) END),"
        " count(DISTINCT CASE WHEN iota<>'' THEN upper(iota) END),"
        " count(DISTINCT CASE WHEN wpx<>'' THEN upper(wpx) END),"
        " count(DISTINCT CASE WHEN satellite<>'' THEN upper(satellite) END)"
        " FROM map_qso WHERE %1").arg(filter.where);
    if (prepareAndBind(&scoreQuery, scoreSql, filter.binds, nullptr)
        && scoreQuery.next()) {
        snapshot.scorecard = {
            {QStringLiteral("qsos"), scoreQuery.value(0).toInt()},
            {QStringLiteral("confirmed"), scoreQuery.value(1).toInt()},
            {QStringLiteral("calls"), scoreQuery.value(2).toInt()},
            {QStringLiteral("dxcc"), scoreQuery.value(3).toInt()},
            {QStringLiteral("grids"), scoreQuery.value(4).toInt()},
            {QStringLiteral("pota"), scoreQuery.value(5).toInt()},
            {QStringLiteral("iota"), scoreQuery.value(6).toInt()},
            {QStringLiteral("wpx"), scoreQuery.value(7).toInt()},
            {QStringLiteral("satellites"), scoreQuery.value(8).toInt()}
        };
    }
    QSqlQuery gridScoreQuery(db);
    if (prepareAndBind(&gridScoreQuery,
                       QStringLiteral(
                           "SELECT COUNT(DISTINCT upper(grid4)) FROM map_qso_grid"
                           " WHERE qso_id IN (SELECT id FROM map_qso WHERE %1)")
                           .arg(filter.where),
                       filter.binds, nullptr)
        && gridScoreQuery.next()) {
        snapshot.scorecard.insert(QStringLiteral("grids"),
                                  gridScoreQuery.value(0).toInt());
    }

    auto appendChart = [&](QString const& group, QString const& column) {
        QSqlQuery chartQuery(db);
        QString const sql = QStringLiteral(
            "SELECT coalesce(nullif(%1,''),'Unknown'),count(*),"
            " sum(CASE WHEN confirmed<>0 THEN 1 ELSE 0 END)"
            " FROM map_qso WHERE %2 GROUP BY 1 ORDER BY count(*) DESC LIMIT 30")
            .arg(column, filter.where);
        if (!prepareAndBind(&chartQuery, sql, filter.binds, nullptr)) return;
        while (chartQuery.next()) {
            snapshot.chartData << QVariantMap {
                {QStringLiteral("group"), group},
                {QStringLiteral("label"), chartQuery.value(0).toString()},
                {QStringLiteral("worked"), chartQuery.value(1).toInt()},
                {QStringLiteral("confirmed"), chartQuery.value(2).toInt()}
            };
        }
    };
    appendChart(QStringLiteral("Band"), QStringLiteral("band"));
    appendChart(QStringLiteral("Mode"), QStringLiteral("mode"));
    appendChart(QStringLiteral("Continent"), QStringLiteral("continent"));
    appendChart(QStringLiteral("DXCC"), QStringLiteral("dxcc"));
    appendChart(QStringLiteral("WPX"), QStringLiteral("wpx"));
    appendChart(QStringLiteral("Grid"), QStringLiteral("grid4"));
    appendChart(QStringLiteral("Satellite"), QStringLiteral("satellite"));

    auto appendTopStatistics = [&](const QString& group, const QString& column) {
        QSqlQuery topQuery(db);
        QString const sql = QStringLiteral(
            "SELECT coalesce(nullif(%1,''),'Unknown'), count(*),"
            " sum(CASE WHEN confirmed<>0 THEN 1 ELSE 0 END),"
            " count(DISTINCT nullif(upper(call),'')), min(qso_epoch), max(qso_epoch)"
            " FROM map_qso WHERE %2 GROUP BY 1"
            " ORDER BY count(*) DESC, 1 LIMIT 20")
            .arg(column, filter.where);
        if (!prepareAndBind(&topQuery, sql, filter.binds, nullptr)) return;
        int rank = 0;
        while (topQuery.next()) {
            snapshot.topStatistics << QVariantMap {
                {QStringLiteral("group"), group},
                {QStringLiteral("label"), topQuery.value(0).toString()},
                {QStringLiteral("worked"), topQuery.value(1).toInt()},
                {QStringLiteral("confirmed"), topQuery.value(2).toInt()},
                {QStringLiteral("calls"), topQuery.value(3).toInt()},
                {QStringLiteral("firstEpoch"), topQuery.value(4).toLongLong()},
                {QStringLiteral("lastEpoch"), topQuery.value(5).toLongLong()},
                {QStringLiteral("rank"), ++rank}
            };
        }
    };
    appendTopStatistics(QStringLiteral("Band"), QStringLiteral("band"));
    appendTopStatistics(QStringLiteral("Mode"), QStringLiteral("mode"));
    appendTopStatistics(QStringLiteral("DXCC"), QStringLiteral("dxcc"));
    appendTopStatistics(QStringLiteral("WPX"), QStringLiteral("wpx"));
    appendTopStatistics(QStringLiteral("Grid"), QStringLiteral("grid4"));
    appendTopStatistics(QStringLiteral("Callsign"), QStringLiteral("call"));
    appendTopStatistics(QStringLiteral("Satellite"), QStringLiteral("satellite"));

    struct ProgressBucket {
        int qsos {0};
        int confirmed {0};
        QSet<QString> calls;
        QSet<QString> confirmedCalls;
        QSet<QString> dxcc;
        QSet<QString> confirmedDxcc;
        QSet<QString> grids;
        QSet<QString> confirmedGrids;
        QSet<QString> wpx;
        QSet<QString> confirmedWpx;
        QSet<QString> pota;
        QSet<QString> confirmedPota;
        QSet<QString> iota;
        QSet<QString> confirmedIota;
        QSet<QString> cqZones;
        QSet<QString> confirmedCqZones;
        QSet<QString> ituZones;
        QSet<QString> confirmedItuZones;
        QSet<QString> states;
        QSet<QString> confirmedStates;
        QSet<QString> continents;
        QSet<QString> confirmedContinents;
    };

    QMap<qint64, ProgressBucket> progressBuckets;
    QSqlQuery progressionQuery(db);
    QList<QVariantList> progressionRows;
    QString const progressionSql = QStringLiteral(
        "SELECT qso_epoch, confirmed, call, dxcc, grid4, wpx, pota_ref, iota,"
        " cq_zone, itu_zone, state, continent"
        " FROM map_qso WHERE %1 AND qso_epoch>0 ORDER BY qso_epoch ASC")
        .arg(filter.where);
    if (prepareAndBind(&progressionQuery, progressionSql, filter.binds, nullptr)) {
        qint64 firstEpoch = 0;
        qint64 lastEpoch = 0;
        while (progressionQuery.next()) {
            qint64 const epoch = progressionQuery.value(0).toLongLong();
            if (firstEpoch == 0) firstEpoch = epoch;
            lastEpoch = epoch;
            QVariantList values;
            for (int column = 0; column < 12; ++column) {
                values.append(progressionQuery.value(column));
            }
            progressionRows.append(std::move(values));
        }
        qint64 const span = qMax<qint64>(0, lastEpoch - firstEpoch);
        qint64 const bucketSeconds = span <= 90LL * 86400
            ? 86400 : (span <= 3LL * 365 * 86400 ? 30LL * 86400 : 365LL * 86400);

        for (QVariantList const& values : std::as_const(progressionRows)) {
                qint64 const epoch = values.value(0).toLongLong();
                qint64 const bucket = (epoch / bucketSeconds) * bucketSeconds;
                ProgressBucket& row = progressBuckets[bucket];
                bool const confirmed = values.value(1).toBool();
                ++row.qsos;
                if (confirmed) ++row.confirmed;
                auto add = [](QSet<QString>& set, const QVariant& value) {
                    QString const normalized = value.toString().trimmed().toUpper();
                    if (!normalized.isEmpty() && normalized != QStringLiteral("UNKNOWN")) {
                        set.insert(normalized);
                    }
                };
                add(row.calls, values.value(2));
                add(row.dxcc, values.value(3));
                add(row.grids, values.value(4));
                add(row.wpx, values.value(5));
                add(row.pota, values.value(6));
                add(row.iota, values.value(7));
                add(row.cqZones, values.value(8));
                add(row.ituZones, values.value(9));
                add(row.states, values.value(10));
                add(row.continents, values.value(11));
                if (confirmed) {
                    add(row.confirmedCalls, values.value(2));
                    add(row.confirmedDxcc, values.value(3));
                    add(row.confirmedGrids, values.value(4));
                    add(row.confirmedWpx, values.value(5));
                    add(row.confirmedPota, values.value(6));
                    add(row.confirmedIota, values.value(7));
                    add(row.confirmedCqZones, values.value(8));
                    add(row.confirmedItuZones, values.value(9));
                    add(row.confirmedStates, values.value(10));
                    add(row.confirmedContinents, values.value(11));
                }
        }

        ProgressBucket cumulative;
        for (auto it = progressBuckets.constBegin(); it != progressBuckets.constEnd(); ++it) {
            ProgressBucket const& bucket = it.value();
            cumulative.qsos += bucket.qsos;
            cumulative.confirmed += bucket.confirmed;
            cumulative.calls.unite(bucket.calls);
            cumulative.confirmedCalls.unite(bucket.confirmedCalls);
            cumulative.dxcc.unite(bucket.dxcc);
            cumulative.confirmedDxcc.unite(bucket.confirmedDxcc);
            cumulative.grids.unite(bucket.grids);
            cumulative.confirmedGrids.unite(bucket.confirmedGrids);
            cumulative.wpx.unite(bucket.wpx);
            cumulative.confirmedWpx.unite(bucket.confirmedWpx);
            cumulative.pota.unite(bucket.pota);
            cumulative.confirmedPota.unite(bucket.confirmedPota);
            cumulative.iota.unite(bucket.iota);
            cumulative.confirmedIota.unite(bucket.confirmedIota);
            cumulative.cqZones.unite(bucket.cqZones);
            cumulative.confirmedCqZones.unite(bucket.confirmedCqZones);
            cumulative.ituZones.unite(bucket.ituZones);
            cumulative.confirmedItuZones.unite(bucket.confirmedItuZones);
            cumulative.states.unite(bucket.states);
            cumulative.confirmedStates.unite(bucket.confirmedStates);
            cumulative.continents.unite(bucket.continents);
            cumulative.confirmedContinents.unite(bucket.confirmedContinents);

            QDateTime const date = QDateTime::fromSecsSinceEpoch(it.key(), QTimeZone::UTC);
            QString label;
            if (bucketSeconds == 86400) {
                label = date.toString(QStringLiteral("yyyy-MM-dd"));
            } else if (bucketSeconds == 30LL * 86400) {
                label = date.toString(QStringLiteral("yyyy-MM"));
            } else {
                label = date.toString(QStringLiteral("yyyy"));
            }
            snapshot.awardProgression << QVariantMap {
                {QStringLiteral("bucketEpoch"), it.key()},
                {QStringLiteral("period"), label},
                {QStringLiteral("bucketSeconds"), bucketSeconds},
                {QStringLiteral("qsos"), bucket.qsos},
                {QStringLiteral("confirmed"), bucket.confirmed},
                {QStringLiteral("callsWorked"), cumulative.calls.size()},
                {QStringLiteral("callsConfirmed"), cumulative.confirmedCalls.size()},
                {QStringLiteral("dxccWorked"), cumulative.dxcc.size()},
                {QStringLiteral("dxccConfirmed"), cumulative.confirmedDxcc.size()},
                {QStringLiteral("gridWorked"), cumulative.grids.size()},
                {QStringLiteral("gridConfirmed"), cumulative.confirmedGrids.size()},
                {QStringLiteral("wpxWorked"), cumulative.wpx.size()},
                {QStringLiteral("wpxConfirmed"), cumulative.confirmedWpx.size()},
                {QStringLiteral("potaWorked"), cumulative.pota.size()},
                {QStringLiteral("potaConfirmed"), cumulative.confirmedPota.size()},
                {QStringLiteral("iotaWorked"), cumulative.iota.size()},
                {QStringLiteral("iotaConfirmed"), cumulative.confirmedIota.size()},
                {QStringLiteral("cqZonesWorked"), cumulative.cqZones.size()},
                {QStringLiteral("cqZonesConfirmed"), cumulative.confirmedCqZones.size()},
                {QStringLiteral("ituZonesWorked"), cumulative.ituZones.size()},
                {QStringLiteral("ituZonesConfirmed"), cumulative.confirmedItuZones.size()},
                {QStringLiteral("statesWorked"), cumulative.states.size()},
                {QStringLiteral("statesConfirmed"), cumulative.confirmedStates.size()},
                {QStringLiteral("continentsWorked"), cumulative.continents.size()},
                {QStringLiteral("continentsConfirmed"), cumulative.confirmedContinents.size()}
            };
        }
    }

    auto metricsForWindow = [&](const SqlFilter& baseFilter,
                                qint64 start, qint64 end) {
        QVariantMap result;
        QSqlQuery metricQuery(db);
        QString sql = QStringLiteral(
            "SELECT count(*), coalesce(sum(confirmed),0),"
            " count(DISTINCT nullif(upper(call),'')),"
            " count(DISTINCT nullif(upper(dxcc),'')),"
            " count(DISTINCT nullif(upper(grid4),'')),"
            " count(DISTINCT nullif(upper(wpx),''))"
            " FROM map_qso WHERE %1 AND qso_epoch>=?")
            .arg(baseFilter.where);
        if (end > 0) sql += QStringLiteral(" AND qso_epoch<?");
        if (!metricQuery.prepare(sql)) return result;
        for (QVariant const& bind : baseFilter.binds) metricQuery.addBindValue(bind);
        metricQuery.addBindValue(start);
        if (end > 0) metricQuery.addBindValue(end);
        if (metricQuery.exec() && metricQuery.next()) {
            result.insert(QStringLiteral("qsos"), metricQuery.value(0).toInt());
            result.insert(QStringLiteral("confirmed"), metricQuery.value(1).toInt());
            result.insert(QStringLiteral("calls"), metricQuery.value(2).toInt());
            result.insert(QStringLiteral("dxcc"), metricQuery.value(3).toInt());
            result.insert(QStringLiteral("grids"), metricQuery.value(4).toInt());
            result.insert(QStringLiteral("wpx"), metricQuery.value(5).toInt());
        }
        return result;
    };

    SqlFilter const comparisonFilter = buildFilter(search, band, mode,
                                                    QStringLiteral("All time"));
    qint64 const comparisonNow = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    const QList<QPair<QString, qint64>> comparisonWindows {
        {QStringLiteral("24 hours"), 86400},
        {QStringLiteral("7 days"), 7LL * 86400},
        {QStringLiteral("30 days"), 30LL * 86400},
        {QStringLiteral("1 year"), 365LL * 86400}
    };
    for (auto const& window : comparisonWindows) {
        QVariantMap const current = metricsForWindow(
            comparisonFilter, comparisonNow - window.second, 0);
        QVariantMap const previous = metricsForWindow(
            comparisonFilter, comparisonNow - 2 * window.second,
            comparisonNow - window.second);
        auto delta = [&current, &previous](const QString& key) {
            return current.value(key).toInt() - previous.value(key).toInt();
        };
        QVariantMap row {
            {QStringLiteral("period"), window.first},
            {QStringLiteral("windowSeconds"), window.second},
            {QStringLiteral("currentQsos"), current.value(QStringLiteral("qsos"))},
            {QStringLiteral("previousQsos"), previous.value(QStringLiteral("qsos"))},
            {QStringLiteral("qsoDelta"), delta(QStringLiteral("qsos"))},
            {QStringLiteral("currentConfirmed"), current.value(QStringLiteral("confirmed"))},
            {QStringLiteral("previousConfirmed"), previous.value(QStringLiteral("confirmed"))},
            {QStringLiteral("confirmedDelta"), delta(QStringLiteral("confirmed"))},
            {QStringLiteral("currentCalls"), current.value(QStringLiteral("calls"))},
            {QStringLiteral("previousCalls"), previous.value(QStringLiteral("calls"))},
            {QStringLiteral("callDelta"), delta(QStringLiteral("calls"))},
            {QStringLiteral("currentDxcc"), current.value(QStringLiteral("dxcc"))},
            {QStringLiteral("previousDxcc"), previous.value(QStringLiteral("dxcc"))},
            {QStringLiteral("dxccDelta"), delta(QStringLiteral("dxcc"))},
            {QStringLiteral("currentGrids"), current.value(QStringLiteral("grids"))},
            {QStringLiteral("previousGrids"), previous.value(QStringLiteral("grids"))},
            {QStringLiteral("gridDelta"), delta(QStringLiteral("grids"))},
            {QStringLiteral("currentWpx"), current.value(QStringLiteral("wpx"))},
            {QStringLiteral("previousWpx"), previous.value(QStringLiteral("wpx"))},
            {QStringLiteral("wpxDelta"), delta(QStringLiteral("wpx"))}
        };
        snapshot.periodComparison.append(row);
        if (window.first == QStringLiteral("30 days")) {
            snapshot.comparison = {
                {QStringLiteral("period"), window.first},
                {QStringLiteral("currentQsos"), current.value(QStringLiteral("qsos"))},
                {QStringLiteral("previousQsos"), previous.value(QStringLiteral("qsos"))},
                {QStringLiteral("qsoDelta"), delta(QStringLiteral("qsos"))},
                {QStringLiteral("currentCalls"), current.value(QStringLiteral("calls"))},
                {QStringLiteral("previousCalls"), previous.value(QStringLiteral("calls"))},
                {QStringLiteral("callDelta"), delta(QStringLiteral("calls"))}
            };
        }
    }

    {
        QSqlQuery profiles(db);
        QString const sql = QStringLiteral(
            "SELECT coalesce(nullif(upper(operator_call),''),'UNKNOWN'), count(*),"
            " coalesce(sum(confirmed),0), count(DISTINCT nullif(upper(call),'')),"
            " count(DISTINCT nullif(upper(dxcc),'')), count(DISTINCT nullif(upper(grid4),'')),"
            " min(qso_epoch), max(qso_epoch) FROM map_qso WHERE %1"
            " GROUP BY 1 ORDER BY count(*) DESC, 1 LIMIT 50").arg(filter.where);
        if (prepareAndBind(&profiles, sql, filter.binds, nullptr)) {
            int rank = 0;
            while (profiles.next()) {
                snapshot.profileStatistics << QVariantMap {
                    {QStringLiteral("profile"), profiles.value(0).toString()},
                    {QStringLiteral("callsign"), profiles.value(0).toString()},
                    {QStringLiteral("qsos"), profiles.value(1).toInt()},
                    {QStringLiteral("confirmed"), profiles.value(2).toInt()},
                    {QStringLiteral("calls"), profiles.value(3).toInt()},
                    {QStringLiteral("dxcc"), profiles.value(4).toInt()},
                    {QStringLiteral("grids"), profiles.value(5).toInt()},
                    {QStringLiteral("firstEpoch"), profiles.value(6).toLongLong()},
                    {QStringLiteral("lastEpoch"), profiles.value(7).toLongLong()},
                    {QStringLiteral("rank"), ++rank}
                };
            }
        }
    }

    QHash<QString, QVariantMap> markerDetails;
    QSqlQuery markerDetailQuery(db);
    QString const markerDetailSql = QStringLiteral(
        "SELECT coalesce(nullif(trim(pota_ref),''),"
        " nullif(trim(iota),''),nullif(trim(wpx),'')),"
        " CASE WHEN trim(pota_ref)<>'' THEN 'POTA'"
        "      WHEN trim(iota)<>'' THEN 'IOTA' ELSE 'WPX' END,"
        " coalesce(nullif(upper(trim(band)),''),'ALL'),"
        " coalesce(nullif(upper(trim(mode)),''),'ALL'),"
        " sum(confirmed),count(*) FROM map_qso WHERE %1"
        " AND (trim(pota_ref)<>'' OR trim(iota)<>'' OR trim(wpx)<>'')"
        " GROUP BY 1,2,3,4").arg(filter.where);
    if (prepareAndBind(&markerDetailQuery, markerDetailSql, filter.binds, nullptr)) {
        auto appendUnique = [](QVariantList& values, const QString& value) {
            for (QVariant const& existing : values) {
                if (existing.toString().compare(value, Qt::CaseInsensitive) == 0) {
                    return;
                }
            }
            values << value;
        };
        while (markerDetailQuery.next()) {
            QString const reference = markerDetailQuery.value(0).toString().trimmed();
            QString const type = markerDetailQuery.value(1).toString().toUpper();
            if (reference.isEmpty() || type.isEmpty()) {
                continue;
            }
            QString const key = type + QLatin1Char('\x1f') + reference.toUpper();
            QVariantMap detail = markerDetails.value(key);
            QVariantList workedBands = detail.value(QStringLiteral("workedBands")).toList();
            QVariantList confirmedBands = detail.value(QStringLiteral("confirmedBands")).toList();
            QVariantList workedModes = detail.value(QStringLiteral("workedModes")).toList();
            QVariantList confirmedModes = detail.value(QStringLiteral("confirmedModes")).toList();
            QVariantList bandModes = detail.value(QStringLiteral("workedBandModes")).toList();
            QString const band = markerDetailQuery.value(2).toString().toUpper();
            QString const mode = markerDetailQuery.value(3).toString().toUpper();
            int const confirmedCount = markerDetailQuery.value(4).toInt();
            int const qsoCount = markerDetailQuery.value(5).toInt();
            appendUnique(workedBands, band);
            appendUnique(workedModes, mode);
            if (confirmedCount > 0) {
                appendUnique(confirmedBands, band);
                appendUnique(confirmedModes, mode);
            }
            bandModes << QVariantMap {
                {QStringLiteral("band"), band},
                {QStringLiteral("mode"), mode},
                {QStringLiteral("worked"), qsoCount > 0},
                {QStringLiteral("confirmed"), confirmedCount > 0},
                {QStringLiteral("qsos"), qsoCount},
                {QStringLiteral("confirmedQsos"), confirmedCount}
            };
            detail.insert(QStringLiteral("workedBands"), workedBands);
            detail.insert(QStringLiteral("confirmedBands"), confirmedBands);
            detail.insert(QStringLiteral("workedModes"), workedModes);
            detail.insert(QStringLiteral("confirmedModes"), confirmedModes);
            detail.insert(QStringLiteral("workedBandModes"), bandModes);
            detail.insert(QStringLiteral("workedCount"),
                          detail.value(QStringLiteral("workedCount")).toInt() + qsoCount);
            detail.insert(QStringLiteral("confirmedCount"),
                          detail.value(QStringLiteral("confirmedCount")).toInt()
                              + confirmedCount);
            markerDetails.insert(key, detail);
        }
    }

    QSqlQuery markerQuery(db);
    QString const markerSql = QStringLiteral(
        "SELECT call,grid,"
        " coalesce(nullif(pota_ref,''),nullif(iota,''),nullif(wpx,'')),"
        " CASE WHEN pota_ref<>'' THEN 'POTA'"
        "      WHEN iota<>'' THEN 'IOTA' ELSE 'WPX' END,"
        " max(qso_epoch),max(confirmed)"
        " FROM map_qso WHERE %1"
        " AND (pota_ref<>'' OR iota<>'' OR wpx<>'')"
        " GROUP BY 3,4 LIMIT 3000").arg(filter.where);
    if (prepareAndBind(&markerQuery, markerSql, filter.binds, nullptr)) {
        while (markerQuery.next()) {
            QString const grid = markerQuery.value(1).toString();
            QPointF const center = maidenheadCenter(grid);
            QString const reference = markerQuery.value(2).toString();
            QString const type = markerQuery.value(3).toString();
            QVariantMap marker {
                {QStringLiteral("id"),
                 QStringLiteral("%1:%2").arg(type.toLower(), reference)},
                {QStringLiteral("type"), type},
                {QStringLiteral("reference"), reference},
                {QStringLiteral("call"), markerQuery.value(0).toString()},
                {QStringLiteral("grid"), grid},
                {QStringLiteral("label"), reference},
                {QStringLiteral("confirmed"), markerQuery.value(5).toBool()},
                {QStringLiteral("color"),
                 type == QStringLiteral("IOTA") ? QStringLiteral("#44d7e8")
                 : type == QStringLiteral("WPX") ? QStringLiteral("#f0b94d")
                                                  : QStringLiteral("#74d66a")}
            };
            if (!center.isNull()) {
                marker.insert(QStringLiteral("longitude"), center.x());
                marker.insert(QStringLiteral("latitude"), center.y());
            }
            QString const detailKey = type.toUpper() + QLatin1Char('\x1f')
                + reference.toUpper();
            QVariantMap const detail = markerDetails.value(detailKey);
            for (auto it = detail.constBegin(); it != detail.constEnd(); ++it) {
                marker.insert(it.key(), it.value());
            }
            marker.insert(QStringLiteral("worked"), true);
            snapshot.markers << marker;
        }
    }
    return snapshot;
}

void MapOperationsService::rebuildOperationalMarkers()
{
    QVariantList markers;
    QHash<QString, QVariantMap> loggedByKey;
    for (QVariant const& value : std::as_const(m_databaseMarkers)) {
        QVariantMap const marker = value.toMap();
        QString const type = marker.value(QStringLiteral("type")).toString().trimmed().toUpper();
        QString const reference = marker.value(QStringLiteral("reference"))
            .toString().trimmed().toUpper();
        if (type.isEmpty() || reference.isEmpty()) {
            continue;
        }
        loggedByKey.insert(type + QLatin1Char('\x1f') + reference, marker);
    }

    auto mergeLoggedStatus = [](QVariantMap& marker, const QVariantMap& logged) {
        for (QString const& key : {
                 QStringLiteral("call"), QStringLiteral("grid"),
                 QStringLiteral("worked"), QStringLiteral("confirmed"),
                 QStringLiteral("workedBands"), QStringLiteral("confirmedBands"),
                 QStringLiteral("workedModes"), QStringLiteral("confirmedModes"),
                 QStringLiteral("workedBandModes"), QStringLiteral("workedCount"),
                 QStringLiteral("confirmedCount")}) {
            if (logged.contains(key)) {
                marker.insert(key, logged.value(key));
            }
        }
        marker.insert(QStringLiteral("worked"), true);
    };

    bool const potaEnabled =
        m_layerModel && m_layerModel->layerEnabled(QStringLiteral("pota"));
    QSet<QString> livePotaKeys;
    if (potaEnabled) {
        for (QVariant const& value : std::as_const(m_potaMarkers)) {
            QVariantMap marker = value.toMap();
            QString const reference = marker.value(QStringLiteral("reference"))
                .toString().trimmed().toUpper();
            QString const key = QStringLiteral("POTA") + QLatin1Char('\x1f') + reference;
            auto const logged = loggedByKey.constFind(key);
            if (logged != loggedByKey.constEnd()) {
                mergeLoggedStatus(marker, logged.value());
            }
            livePotaKeys.insert(key);
            markers << marker;
        }
    }

    QHash<QString, QVariantMap> loggedIota;
    for (auto it = loggedByKey.constBegin(); it != loggedByKey.constEnd(); ++it) {
        if (it.key().startsWith(QStringLiteral("IOTA") + QLatin1Char('\x1f'))) {
            loggedIota.insert(it.key().section(QLatin1Char('\x1f'), 1), it.value());
        }
    }
    bool const iotaEnabled =
        m_layerModel && m_layerModel->layerEnabled(QStringLiteral("iota"));
    if (iotaEnabled && !m_iotaCatalogMarkers.isEmpty()) {
        for (QVariant const& value : std::as_const(m_iotaCatalogMarkers)) {
            QVariantMap marker = value.toMap();
            QString const reference =
                marker.value(QStringLiteral("reference")).toString().toUpper();
            auto const logged = loggedIota.constFind(reference);
            if (logged != loggedIota.constEnd()) {
                QVariantMap const qsoMarker = logged.value();
                mergeLoggedStatus(marker, qsoMarker);
                marker.insert(
                    QStringLiteral("comments"),
                    marker.value(QStringLiteral("confirmed")).toBool()
                        ? QStringLiteral("Worked and confirmed in the imported ADIF log")
                        : QStringLiteral("Worked in the imported ADIF log"));
            }
            markers << marker;
        }
    }

    for (QVariant const& value : std::as_const(m_databaseMarkers)) {
        QVariantMap const marker = value.toMap();
        QString const type = marker.value(QStringLiteral("type")).toString().toLower();
        if (type == QStringLiteral("iota")
            && iotaEnabled && !m_iotaCatalogMarkers.isEmpty()) {
            continue;
        }
        QString const reference = marker.value(QStringLiteral("reference"))
            .toString().trimmed().toUpper();
        QString const markerKey = type.toUpper() + QLatin1Char('\x1f') + reference;
        if (type == QStringLiteral("pota") && livePotaKeys.contains(markerKey)) {
            continue;
        }
        if (!m_layerModel || m_layerModel->layerEnabled(type)) {
            markers << marker;
        }
    }
    if (m_layerModel) {
        int iotaCount = 0;
        int wpxCount = 0;
        for (QVariant const& value : std::as_const(m_databaseMarkers)) {
            QString const type =
                value.toMap().value(QStringLiteral("type")).toString().toUpper();
            if (type == QStringLiteral("IOTA")) ++iotaCount;
            if (type == QStringLiteral("WPX")) ++wpxCount;
        }
        m_layerModel->setCount(
            QStringLiteral("iota"),
            m_iotaCatalogMarkers.isEmpty()
                ? iotaCount : m_iotaCatalogMarkers.size());
        m_layerModel->setCount(QStringLiteral("wpx"), wpxCount);
    }
    if (m_operationalMarkers == markers) return;
    m_operationalMarkers = markers;
    emit operationalMarkersChanged();
}

QString MapOperationsService::normalizedLocalPath(const QString& path)
{
    QUrl const url(path);
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    return path;
}

bool MapOperationsService::exportLogbook(const QString& path,
                                         const QString& format)
{
    QString const localPath = normalizedLocalPath(path).trimmed();
    if (localPath.isEmpty()) {
        setStatusMessage(QStringLiteral("Choose an export file"));
        return false;
    }
    if (m_exportInProgress) {
        setStatusMessage(QStringLiteral("A logbook export is already running"));
        return false;
    }

    QString const databasePath = m_databasePath;
    QString const search = m_logbookSearch;
    QString const band = m_logbookBand;
    QString const mode = m_logbookMode;
    QString const period = m_logbookPeriod;
    QString const sort = m_logbookSort;
    bool const descending = m_logbookSortDescending;
    setExportInProgress(true);
    setStatusMessage(QStringLiteral("Exporting the filtered logbook..."));

    auto* watcher = new QFutureWatcher<ExportResult>(this);
    connect(watcher, &QFutureWatcher<ExportResult>::finished, this,
            [this, watcher] {
        ExportResult const result = watcher->result();
        watcher->deleteLater();
        setExportInProgress(false);
        if (!result.error.isEmpty()) {
            setStatusMessage(QStringLiteral("Logbook export: %1")
                                 .arg(result.error));
            return;
        }
        m_lastExportPath = result.path;
        emit lastExportPathChanged();
        setStatusMessage(QStringLiteral("Exported %1 logbook rows")
                             .arg(result.rows));
    });
    watcher->setFuture(QtConcurrent::run(
        [databasePath, localPath, format, search, band, mode, period, sort,
         descending] {
        return exportLogbookDatabase(databasePath, localPath, format,
                                     search, band, mode, period, sort,
                                     descending);
    }));
    return true;
}

bool MapOperationsService::exportStatistics(const QString& path,
                                            const QString& format)
{
    QString const localPath = normalizedLocalPath(path).trimmed();
    if (localPath.isEmpty()) {
        setStatusMessage(QStringLiteral("Choose a statistics export file"));
        return false;
    }
    QFileInfo const info(localPath);
    if (!QDir().mkpath(info.absolutePath())) {
        setStatusMessage(QStringLiteral("Cannot create statistics export directory"));
        return false;
    }

    QSaveFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatusMessage(QStringLiteral("Statistics export: %1").arg(file.errorString()));
        return false;
    }

    bool const json = format.compare(QStringLiteral("JSON"), Qt::CaseInsensitive) == 0
        || info.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0;
    if (json) {
        QJsonObject report {
            {QStringLiteral("type"), QStringLiteral("decodium-logbook-statistics")},
            {QStringLiteral("version"), 1},
            {QStringLiteral("generatedAt"),
             QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
            {QStringLiteral("filters"), QJsonObject {
                {QStringLiteral("search"), m_logbookSearch},
                {QStringLiteral("band"), m_logbookBand},
                {QStringLiteral("mode"), m_logbookMode},
                {QStringLiteral("period"), m_logbookPeriod},
                {QStringLiteral("drilldown"), m_statisticsDrilldown}
            }},
            {QStringLiteral("scorecard"), QJsonObject::fromVariantMap(m_scorecard)},
            {QStringLiteral("comparison"), QJsonObject::fromVariantMap(m_comparison)},
            {QStringLiteral("periodComparison"), QJsonValue::fromVariant(m_periodComparison)},
            {QStringLiteral("awardProgression"), QJsonValue::fromVariant(m_awardProgression)},
            {QStringLiteral("topStatistics"), QJsonValue::fromVariant(m_topStatistics)},
            {QStringLiteral("profileStatistics"), QJsonValue::fromVariant(m_profileStatistics)},
            {QStringLiteral("logbookRows"), QJsonValue::fromVariant(m_logbookRows)}
        };
        file.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    } else {
        QTextStream stream(&file);
        stream << "Section,Period/Group,Label,QSO,Confirmed,Calls,DXCC,Grids,WPX\n";
        for (QVariant const& value : std::as_const(m_periodComparison)) {
            QVariantMap const row = value.toMap();
            stream << "Period," << csvQuoted(row.value(QStringLiteral("period")).toString())
                   << ",Current," << row.value(QStringLiteral("currentQsos")).toInt()
                   << "," << row.value(QStringLiteral("currentConfirmed")).toInt()
                   << "," << row.value(QStringLiteral("currentCalls")).toInt()
                   << "," << row.value(QStringLiteral("currentDxcc")).toInt()
                   << "," << row.value(QStringLiteral("currentGrids")).toInt()
                   << "," << row.value(QStringLiteral("currentWpx")).toInt() << "\n";
            stream << "Period," << csvQuoted(row.value(QStringLiteral("period")).toString())
                   << ",Previous," << row.value(QStringLiteral("previousQsos")).toInt()
                   << "," << row.value(QStringLiteral("previousConfirmed")).toInt()
                   << "," << row.value(QStringLiteral("previousCalls")).toInt()
                   << "," << row.value(QStringLiteral("previousDxcc")).toInt()
                   << "," << row.value(QStringLiteral("previousGrids")).toInt()
                   << "," << row.value(QStringLiteral("previousWpx")).toInt() << "\n";
        }
        for (QVariant const& value : std::as_const(m_topStatistics)) {
            QVariantMap const row = value.toMap();
            stream << "Top," << csvQuoted(row.value(QStringLiteral("group")).toString())
                   << "," << csvQuoted(row.value(QStringLiteral("label")).toString())
                   << "," << row.value(QStringLiteral("worked")).toInt()
                   << "," << row.value(QStringLiteral("confirmed")).toInt()
                   << "," << row.value(QStringLiteral("calls")).toInt() << "\n";
        }
        for (QVariant const& value : std::as_const(m_awardProgression)) {
            QVariantMap const row = value.toMap();
            stream << "AwardProgression," << csvQuoted(row.value(QStringLiteral("period")).toString())
                   << ",Cumulative," << row.value(QStringLiteral("qsos")).toInt()
                   << "," << row.value(QStringLiteral("confirmed")).toInt()
                   << "," << row.value(QStringLiteral("callsWorked")).toInt()
                   << "," << row.value(QStringLiteral("dxccWorked")).toInt()
                   << "," << row.value(QStringLiteral("gridWorked")).toInt()
                   << "," << row.value(QStringLiteral("wpxWorked")).toInt() << "\n";
        }
        for (QVariant const& value : std::as_const(m_profileStatistics)) {
            QVariantMap const row = value.toMap();
            stream << "Profile,," << csvQuoted(row.value(QStringLiteral("profile")).toString())
                   << "," << row.value(QStringLiteral("qsos")).toInt()
                   << "," << row.value(QStringLiteral("confirmed")).toInt()
                   << "," << row.value(QStringLiteral("calls")).toInt()
                   << "," << row.value(QStringLiteral("dxcc")).toInt()
                   << "," << row.value(QStringLiteral("grids")).toInt() << "\n";
        }
    }

    if (!file.commit()) {
        setStatusMessage(QStringLiteral("Statistics export: %1").arg(file.errorString()));
        return false;
    }
    m_lastExportPath = localPath;
    emit lastExportPathChanged();
    setStatusMessage(QStringLiteral("Exported logbook statistics"));
    return true;
}

void MapOperationsService::drillDownStatistics(const QString& dimension,
                                                const QString& value)
{
    QString const normalizedDimension = dimension.trimmed();
    QString const normalizedValue = value.trimmed().left(120);
    if (normalizedValue.isEmpty()) return;

    m_statisticsDrilldown = QStringLiteral("%1: %2")
        .arg(normalizedDimension, normalizedValue);
    if (normalizedDimension.compare(QStringLiteral("Band"), Qt::CaseInsensitive) == 0) {
        m_logbookBand = normalizedValue;
        m_logbookSearch.clear();
    } else if (normalizedDimension.compare(QStringLiteral("Mode"), Qt::CaseInsensitive) == 0) {
        m_logbookMode = normalizedValue;
        m_logbookSearch.clear();
    } else {
        m_logbookSearch = normalizedValue;
    }
    saveSetting(QStringLiteral("LogbookBand"), m_logbookBand);
    saveSetting(QStringLiteral("LogbookMode"), m_logbookMode);
    saveSetting(QStringLiteral("LogbookSearch"), m_logbookSearch);
    emit logbookFiltersChanged();
    refreshLogbook();
}

MapOperationsService::ExportResult
MapOperationsService::exportLogbookDatabase(
    const QString& databasePath, const QString& path,
    const QString& format, const QString& search,
    const QString& band, const QString& mode, const QString& period,
    const QString& sort, bool descending)
{
    ExportResult result;
    result.path = path;
    QFileInfo const info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        result.error = QStringLiteral("Cannot create export directory");
        return result;
    }

    ScopedMapDatabase connection(databasePath);
    QSqlDatabase& db = connection.database();
    if (!db.open()) {
        result.error = db.lastError().text();
        return result;
    }
    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA busy_timeout=3000"));
    SqlFilter const filter = buildFilter(search, band, mode, period);
    QString const selectSql = QStringLiteral(
        "SELECT source_key,call,grid,band,mode,qso_date,time_on,qso_epoch,"
        " frequency_mhz,satellite,sat_mode,freq_rx_mhz,confirmed,dxcc,continent,state,pota_ref,iota,wpx,source,"
        " COALESCE((SELECT GROUP_CONCAT(g.grid, ', ') FROM map_qso_grid g"
        "           WHERE g.qso_id=map_qso.id AND g.is_primary=0), '')"
        " FROM map_qso WHERE %1 ORDER BY %2 %3")
        .arg(filter.where, sortableColumn(sort),
             descending ? QStringLiteral("DESC") : QStringLiteral("ASC"));
    QSqlQuery query(db);
    if (!prepareAndBind(&query, selectSql, filter.binds, &result.error)) {
        return result;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        result.error = file.errorString();
        return result;
    }
    QTextStream stream(&file);
    bool const adif =
        format.compare(QStringLiteral("ADIF"), Qt::CaseInsensitive) == 0
        || info.suffix().compare(QStringLiteral("adi"), Qt::CaseInsensitive) == 0
        || info.suffix().compare(QStringLiteral("adif"), Qt::CaseInsensitive) == 0;
    if (adif) {
        stream << "<ADIF_VER:5>3.1.4 <PROGRAMID:9>Decodium4 <EOH>\n";
    } else {
        stream << "Date,Time,Call,Grid,VUCC Grids,Band,Mode,Frequency MHz,Receive Frequency MHz,Satellite,Sat Mode,Confirmed,DXCC,"
                  "Continent,State,POTA,IOTA,WPX,Source\n";
    }

    while (query.next()) {
        QVariantMap const row = rowToMap(query);
        if (adif) {
            auto field = [&](QString const& key, QString const& name) {
                QString const text = key == QStringLiteral("vuccGrids")
                    ? row.value(key).toStringList().join(QLatin1Char(','))
                    : row.value(key).toString();
                if (!text.isEmpty()) {
                    stream << '<' << name << ':' << text.toUtf8().size()
                           << '>' << text << ' ';
                }
            };
            field(QStringLiteral("call"), QStringLiteral("CALL"));
            field(QStringLiteral("grid"), QStringLiteral("GRIDSQUARE"));
            field(QStringLiteral("vuccGrids"), QStringLiteral("VUCC_GRIDS"));
            field(QStringLiteral("band"), QStringLiteral("BAND"));
            field(QStringLiteral("mode"), QStringLiteral("MODE"));
            field(QStringLiteral("frequencyMhz"), QStringLiteral("FREQ"));
            field(QStringLiteral("frequencyRxMhz"), QStringLiteral("FREQ_RX"));
            field(QStringLiteral("date"), QStringLiteral("QSO_DATE"));
            field(QStringLiteral("time"), QStringLiteral("TIME_ON"));
            field(QStringLiteral("satellite"), QStringLiteral("SAT_NAME"));
            field(QStringLiteral("satMode"), QStringLiteral("SAT_MODE"));
            field(QStringLiteral("pota"), QStringLiteral("POTA_REF"));
            field(QStringLiteral("iota"), QStringLiteral("IOTA"));
            stream << "<EOR>\n";
        } else {
            QStringList values {
                row.value(QStringLiteral("date")).toString(),
                row.value(QStringLiteral("time")).toString(),
                row.value(QStringLiteral("call")).toString(),
                row.value(QStringLiteral("grid")).toString(),
                row.value(QStringLiteral("vuccGrids")).toStringList()
                    .join(QLatin1Char(',')),
                row.value(QStringLiteral("band")).toString(),
                row.value(QStringLiteral("mode")).toString(),
                QString::number(
                    row.value(QStringLiteral("frequencyMhz")).toDouble(),
                    'f', 6),
                QString::number(
                    row.value(QStringLiteral("frequencyRxMhz")).toDouble(),
                    'f', 6),
                row.value(QStringLiteral("satellite")).toString(),
                row.value(QStringLiteral("satMode")).toString(),
                row.value(QStringLiteral("confirmed")).toBool()
                    ? QStringLiteral("Y") : QStringLiteral("N"),
                row.value(QStringLiteral("dxcc")).toString(),
                row.value(QStringLiteral("continent")).toString(),
                row.value(QStringLiteral("state")).toString(),
                row.value(QStringLiteral("pota")).toString(),
                row.value(QStringLiteral("iota")).toString(),
                row.value(QStringLiteral("wpx")).toString(),
                row.value(QStringLiteral("source")).toString()
            };
            for (QString& value : values) value = csvQuoted(value);
            stream << values.join(QLatin1Char(',')) << '\n';
        }
        ++result.rows;
    }

    if (!file.commit()) {
        result.error = file.errorString();
    }
    return result;
}

void MapOperationsService::cycleDataView()
{
    QStringList const values = availableDataViews();
    int const index = values.indexOf(m_dataViewMode);
    setDataViewMode(values.at((index + 1) % values.size()));
}

void MapOperationsService::applyMapPreset(const QString& name)
{
    QString normalized = name.trimmed();
    if (normalized.isEmpty() || !m_layerModel) return;

    QVariantMap preset;
    if (normalized.compare(QStringLiteral("Operational"), Qt::CaseInsensitive) == 0) {
        preset = presetMap(QStringLiteral("Equirectangular"),
                           QStringLiteral("Live + Logbook"),
                           {QStringLiteral("live"), QStringLiteral("active"),
                            QStringLiteral("psk"), QStringLiteral("offline")});
        normalized = QStringLiteral("Operational");
    } else if (normalized.compare(QStringLiteral("Logbook"), Qt::CaseInsensitive) == 0) {
        preset = presetMap(QStringLiteral("Equirectangular"),
                           QStringLiteral("Logbook"),
                           {QStringLiteral("worked"), QStringLiteral("confirmed"),
                            QStringLiteral("states"), QStringLiteral("offline")});
        normalized = QStringLiteral("Logbook");
    } else if (normalized.compare(QStringLiteral("Parks"), Qt::CaseInsensitive) == 0) {
        preset = presetMap(QStringLiteral("Mercator"),
                           QStringLiteral("Live + Logbook"),
                           {QStringLiteral("live"), QStringLiteral("pota"),
                            QStringLiteral("states"), QStringLiteral("offline")});
        normalized = QStringLiteral("Parks");
    } else if (normalized.compare(QStringLiteral("Awards"), Qt::CaseInsensitive) == 0) {
        preset = presetMap(QStringLiteral("Miller"),
                           QStringLiteral("Logbook"),
                           {QStringLiteral("worked"), QStringLiteral("confirmed"),
                            QStringLiteral("iota"), QStringLiteral("wpx"),
                            QStringLiteral("offline")});
        normalized = QStringLiteral("Awards");
    } else if (normalized.compare(QStringLiteral("Propagation"), Qt::CaseInsensitive) == 0) {
        preset = presetMap(QStringLiteral("Equirectangular"),
                           QStringLiteral("Live"),
                           {QStringLiteral("live"), QStringLiteral("propagation"),
                            QStringLiteral("muf"), QStringLiteral("fof2"),
                            QStringLiteral("aurora"), QStringLiteral("offline")});
        normalized = QStringLiteral("Propagation");
    } else if (normalized.compare(QStringLiteral("Minimal"), Qt::CaseInsensitive) == 0) {
        preset = presetMap(QStringLiteral("Equirectangular"),
                           QStringLiteral("Live"),
                           {QStringLiteral("live"), QStringLiteral("offline")});
        normalized = QStringLiteral("Minimal");
    } else {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        settings.beginGroup(QStringLiteral("MapPresets"));
        settings.beginGroup(normalized);
        preset = presetMap(
            settings.value(QStringLiteral("Projection"),
                           QStringLiteral("Equirectangular")).toString(),
            settings.value(QStringLiteral("DataView"),
                           QStringLiteral("Live + Logbook")).toString(),
            settings.value(QStringLiteral("Layers")).toStringList());
        QVariantMap const styles = settings.value(QStringLiteral("Styles")).toMap();
        for (auto it = styles.constBegin(); it != styles.constEnd(); ++it) {
            QVariantMap const style = it.value().toMap();
            m_layerModel->setLayerStyle(
                it.key(), style.value(QStringLiteral("color")).toString(),
                style.value(QStringLiteral("opacity"), 1.0).toDouble(),
                style.value(QStringLiteral("thickness"), 1.0).toDouble(),
                style.value(QStringLiteral("labelDensity"), 100).toInt());
        }
        settings.endGroup();
        settings.endGroup();
    }
    QStringList const enabled = preset.value(QStringLiteral("layers")).toStringList();
    for (int row = 0; row < m_layerModel->rowCount(); ++row) {
        QModelIndex const index = m_layerModel->index(row, 0);
        QString const id =
            m_layerModel->data(index, MapLayerModel::LayerIdRole).toString();
        m_layerModel->setLayerEnabled(id, enabled.contains(id));
    }
    setMapProjection(preset.value(QStringLiteral("projection")).toString());
    setDataViewMode(preset.value(QStringLiteral("dataView")).toString());
    m_activeMapPreset = normalized;
    saveSetting(QStringLiteral("ActivePreset"), normalized);
    emit activeMapPresetChanged();
    refreshPota();
    refreshGeographicFeatures();
}

void MapOperationsService::saveMapPreset(const QString& name)
{
    QString const normalized = name.trimmed().left(48);
    if (normalized.isEmpty() || !m_layerModel) return;
    QStringList enabled;
    for (int row = 0; row < m_layerModel->rowCount(); ++row) {
        QModelIndex const index = m_layerModel->index(row, 0);
        if (m_layerModel->data(index, MapLayerModel::LayerEnabledRole).toBool()) {
            enabled << m_layerModel->data(index, MapLayerModel::LayerIdRole).toString();
        }
    }
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("MapPresets"));
    settings.beginGroup(normalized);
    settings.setValue(QStringLiteral("Projection"), m_mapProjection);
    settings.setValue(QStringLiteral("DataView"), m_dataViewMode);
    settings.setValue(QStringLiteral("Layers"), enabled);
    settings.setValue(QStringLiteral("Styles"), m_layerModel->allLayerStyles());
    settings.endGroup();
    settings.endGroup();
    m_activeMapPreset = normalized;
    saveSetting(QStringLiteral("ActivePreset"), normalized);
    loadMapPresets();
    emit activeMapPresetChanged();
}

void MapOperationsService::deleteMapPreset(const QString& name)
{
    static const QStringList builtIns {
        QStringLiteral("Operational"), QStringLiteral("Logbook"),
        QStringLiteral("Parks"), QStringLiteral("Awards"),
        QStringLiteral("Propagation"), QStringLiteral("Minimal")
    };
    QString const normalized = name.trimmed();
    if (builtIns.contains(normalized, Qt::CaseInsensitive)) {
        setStatusMessage(QStringLiteral("Built-in presets cannot be deleted"));
        return;
    }
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("MapPresets"));
    settings.remove(normalized);
    settings.endGroup();
    if (m_activeMapPreset == normalized) {
        m_activeMapPreset.clear();
        emit activeMapPresetChanged();
    }
    loadMapPresets();
}

void MapOperationsService::aimRotator(double azimuth)
{
    if (!m_rotatorService || !qIsFinite(azimuth)) return;
    int const heading = qRound(std::fmod(azimuth + 360.0, 360.0));
    m_rotatorService->commandTarget(heading, 0.0, false);
}

void MapOperationsService::aimRotatorWithElevation(double azimuth, double elevation)
{
    if (m_rotatorService) m_rotatorService->commandTarget(azimuth, elevation, true);
}

void MapOperationsService::aimRotatorAt(double latitude, double longitude,
                                        double homeLatitude, double homeLongitude)
{
    aimRotator(initialBearing(homeLatitude, homeLongitude,
                              latitude, longitude));
}

void MapOperationsService::aimRotatorAtWithElevation(double latitude, double longitude,
                                                     double elevation,
                                                     double homeLatitude,
                                                     double homeLongitude)
{
    aimRotatorWithElevation(initialBearing(homeLatitude, homeLongitude,
                                           latitude, longitude), elevation);
}

void MapOperationsService::trackRotatorAt(double latitude, double longitude,
                                          double homeLatitude, double homeLongitude)
{
    if (m_rotatorService) {
        m_rotatorService->trackTarget(initialBearing(homeLatitude, homeLongitude,
                                                     latitude, longitude),
                                      0.0, false);
    }
}

void MapOperationsService::trackRotatorAtWithElevation(double latitude, double longitude,
                                                       double elevation,
                                                       double homeLatitude,
                                                       double homeLongitude)
{
    if (m_rotatorService) {
        m_rotatorService->trackTarget(initialBearing(homeLatitude, homeLongitude,
                                                     latitude, longitude),
                                      elevation, true);
    }
}

void MapOperationsService::trackRotator(double azimuth, double elevation,
                                        bool hasElevation)
{
    if (m_rotatorService) m_rotatorService->trackTarget(azimuth, elevation, hasElevation);
}

void MapOperationsService::stopRotator()
{
    if (m_rotatorService) m_rotatorService->emergencyStop();
}

void MapOperationsService::parkRotator()
{
    if (m_rotatorService) m_rotatorService->park();
}

QString MapOperationsService::reserveScreenshotPath()
{
    QString root = QStandardPaths::writableLocation(
        QStandardPaths::PicturesLocation);
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation);
    }
    QDir directory(root);
    directory.mkpath(QStringLiteral("Decodium"));
    QString const path = directory.absoluteFilePath(
        QStringLiteral("Decodium-Map-%1.png")
            .arg(QDateTime::currentDateTimeUtc().toString(
                QStringLiteral("yyyyMMdd-HHmmss"))));
    emit screenshotPathReserved(path);
    return path;
}

QString MapOperationsService::reserveLogbookExportPath(const QString& format)
{
    QString root = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(
            QStandardPaths::HomeLocation);
    }
    QDir directory(root);
    directory.mkpath(QStringLiteral("Decodium"));
    bool const adif =
        format.compare(QStringLiteral("ADIF"), Qt::CaseInsensitive) == 0;
    return directory.absoluteFilePath(
        QStringLiteral("Decodium-Logbook-%1.%2")
            .arg(QDateTime::currentDateTimeUtc().toString(
                     QStringLiteral("yyyyMMdd-HHmmss")),
                 adif ? QStringLiteral("adi") : QStringLiteral("csv")));
}

QString MapOperationsService::reserveStatisticsExportPath(const QString& format)
{
    QString root = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
    if (root.isEmpty()) root = QStandardPaths::writableLocation(
        QStandardPaths::HomeLocation);
    QDir directory(root);
    directory.mkpath(QStringLiteral("Decodium"));
    bool const json = format.compare(QStringLiteral("JSON"), Qt::CaseInsensitive) == 0;
    return directory.absoluteFilePath(
        QStringLiteral("Decodium-Logbook-Statistics-%1.%2")
            .arg(QDateTime::currentDateTimeUtc().toString(
                     QStringLiteral("yyyyMMdd-HHmmss")),
                 json ? QStringLiteral("json") : QStringLiteral("csv")));
}

void MapOperationsService::setLogbookLoading(bool loading)
{
    if (m_logbookLoading == loading) return;
    m_logbookLoading = loading;
    emit logbookLoadingChanged();
}

void MapOperationsService::setGeographicLoading(bool loading)
{
    if (m_geographicLoading == loading) return;
    m_geographicLoading = loading;
    emit geographicLoadingChanged();
}

void MapOperationsService::setPotaLoading(bool loading)
{
    if (m_potaLoading == loading) return;
    m_potaLoading = loading;
    emit potaLoadingChanged();
}

void MapOperationsService::setExportInProgress(bool inProgress)
{
    if (m_exportInProgress == inProgress) return;
    m_exportInProgress = inProgress;
    emit exportInProgressChanged();
}

void MapOperationsService::setStatusMessage(const QString& message)
{
    if (m_statusMessage == message) return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void MapOperationsService::setRotatorStatus(const QString& message)
{
    if (m_rotatorStatus == message) return;
    m_rotatorStatus = message;
    emit rotatorStatusChanged();
}
