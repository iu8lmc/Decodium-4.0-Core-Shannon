#include "MapExternalOverlayService.h"

#include "MapLayerModel.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QRunnable>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {

constexpr int kNetworkTimeoutMs = 15000;
constexpr int kMaxImagePayloadBytes = 20 * 1024 * 1024;
constexpr int kMaxJsonPayloadBytes = 8 * 1024 * 1024;
constexpr double kMercatorLatitudeLimit = 85.0511287798066;
constexpr double kEarthRadiusKm = 6371.0088;
constexpr double kSynodicMonthDays = 29.53059;
constexpr double kReferenceNewMoonJulianDate = 2460320.998;

struct MoonEphemeris {
    double azimuthDegrees {0.0};
    double elevationDegrees {0.0};
    double distanceKm {0.0};
    double illuminationPercent {0.0};
    bool valid {false};
};

double normalizedDegrees(double value)
{
    value = std::fmod(value, 360.0);
    return value < 0.0 ? value + 360.0 : value;
}

MoonEphemeris moonEphemeris(double stationLatitude,
                            double stationLongitude,
                            const QDateTime& timestamp)
{
    MoonEphemeris result;
    if (!std::isfinite(stationLatitude)
        || !std::isfinite(stationLongitude)
        || stationLatitude < -90.0
        || stationLatitude > 90.0
        || stationLongitude < -180.0
        || stationLongitude > 180.0
        || !timestamp.isValid()) {
        return result;
    }

    double const julianDate =
        2440587.5
        + timestamp.toUTC().toMSecsSinceEpoch() / 86400000.0;
    double const centuries = (julianDate - 2451545.0) / 36525.0;

    double const meanLongitude =
        normalizedDegrees(218.3164477 + 481267.88123421 * centuries);
    double const moonAnomaly = qDegreesToRadians(
        normalizedDegrees(134.9633964 + 477198.8675055 * centuries));
    double const sunAnomaly = qDegreesToRadians(
        normalizedDegrees(357.5291092 + 35999.0502909 * centuries));
    double const elongation = qDegreesToRadians(
        normalizedDegrees(297.8501921 + 445267.1114034 * centuries));
    double const latitudeArgument = qDegreesToRadians(
        normalizedDegrees(93.2720950 + 483202.0175233 * centuries));

    double const eclipticLongitude = qDegreesToRadians(
        meanLongitude
        + 6.289 * std::sin(moonAnomaly)
        - 1.274 * std::sin(2.0 * elongation - moonAnomaly)
        + 0.658 * std::sin(2.0 * elongation)
        + 0.214 * std::sin(2.0 * moonAnomaly)
        - 0.186 * std::sin(sunAnomaly));
    double const eclipticLatitude = qDegreesToRadians(
        5.128 * std::sin(latitudeArgument)
        + 0.281 * std::sin(moonAnomaly + latitudeArgument)
        + 0.078 * std::sin(2.0 * elongation - latitudeArgument));
    double const obliquity =
        qDegreesToRadians(23.439291 - 0.0130042 * centuries);

    double const rightAscension = std::atan2(
        std::sin(eclipticLongitude) * std::cos(obliquity)
            - std::tan(eclipticLatitude) * std::sin(obliquity),
        std::cos(eclipticLongitude));
    double const declination = std::asin(
        std::sin(eclipticLatitude) * std::cos(obliquity)
        + std::cos(eclipticLatitude) * std::sin(obliquity)
            * std::sin(eclipticLongitude));

    double const siderealDegrees = normalizedDegrees(
        280.46061837 + 360.98564736629 * (julianDate - 2451545.0));
    double const hourAngle = qDegreesToRadians(normalizedDegrees(
        siderealDegrees + stationLongitude
        - qRadiansToDegrees(rightAscension)));
    double const stationLatitudeRadians =
        qDegreesToRadians(stationLatitude);
    double const sinAltitude =
        std::sin(stationLatitudeRadians) * std::sin(declination)
        + std::cos(stationLatitudeRadians) * std::cos(declination)
            * std::cos(hourAngle);
    double const altitude =
        std::asin(qBound(-1.0, sinAltitude, 1.0));
    double const cosAltitude = std::cos(altitude);
    if (std::abs(cosAltitude) < 1.0e-12) {
        return result;
    }
    double const cosAzimuth = qBound(
        -1.0,
        (std::sin(declination)
         - std::sin(stationLatitudeRadians) * sinAltitude)
            / (std::cos(stationLatitudeRadians) * cosAltitude),
        1.0);
    double azimuth = qRadiansToDegrees(std::acos(cosAzimuth));
    if (std::sin(hourAngle) > 0.0) {
        azimuth = 360.0 - azimuth;
    }

    double const distanceKm =
        385000.56
        - 20905.355 * std::cos(moonAnomaly)
        - 3699.111 * std::cos(2.0 * elongation - moonAnomaly)
        - 2955.968 * std::cos(2.0 * elongation)
        - 569.925 * std::cos(2.0 * moonAnomaly);
    double phaseDays =
        std::fmod(julianDate - kReferenceNewMoonJulianDate,
                  kSynodicMonthDays);
    if (phaseDays < 0.0) {
        phaseDays += kSynodicMonthDays;
    }
    double const phaseRadians =
        phaseDays / kSynodicMonthDays * 2.0 * qAcos(-1.0);

    result.azimuthDegrees = normalizedDegrees(azimuth);
    result.elevationDegrees = qRadiansToDegrees(altitude);
    result.distanceKm = distanceKm;
    result.illuminationPercent =
        (1.0 - std::cos(phaseRadians)) * 50.0;
    result.valid =
        std::isfinite(result.azimuthDegrees)
        && std::isfinite(result.elevationDegrees)
        && std::isfinite(result.distanceKm)
        && std::isfinite(result.illuminationPercent)
        && result.distanceKm > kEarthRadiusKm;
    return result;
}

QString wmsUrl(const QString& baseUrl,
               const QString& layer,
               const QString& style)
{
    return baseUrl
        + QStringLiteral("?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap")
        + QStringLiteral("&LAYERS=") + layer
        + QStringLiteral("&STYLES=") + style
        + QStringLiteral("&SRS=EPSG:4326&BBOX=-180,-90,180,90")
        + QStringLiteral("&WIDTH=1024&HEIGHT=512")
        + QStringLiteral("&FORMAT=image/png&TRANSPARENT=TRUE");
}

QString mercatorWmsUrl(const QString& baseUrl,
                       const QString& layer,
                       const QString& style)
{
    return baseUrl
        + QStringLiteral("?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap")
        + QStringLiteral("&LAYERS=") + layer
        + QStringLiteral("&STYLES=") + style
        + QStringLiteral("&SRS=EPSG:3857")
        + QStringLiteral("&BBOX=-20037508.342789244,-20037508.342789244,"
                         "20037508.342789244,20037508.342789244")
        + QStringLiteral("&WIDTH=512&HEIGHT=512")
        + QStringLiteral("&FORMAT=image/png&TRANSPARENT=TRUE");
}

double wrappedLongitudeDegrees(double value)
{
    while (value < -180.0) value += 360.0;
    while (value >= 180.0) value -= 360.0;
    return value;
}

QPointF destinationPoint(double latitudeRadians,
                         double longitudeRadians,
                         double bearingRadians,
                         double angularDistance)
{
    double const sinLat = std::sin(latitudeRadians);
    double const cosLat = std::cos(latitudeRadians);
    double const sinDistance = std::sin(angularDistance);
    double const cosDistance = std::cos(angularDistance);
    double const latitude = std::asin(
        sinLat * cosDistance
        + cosLat * sinDistance * std::cos(bearingRadians));
    double const longitude = longitudeRadians + std::atan2(
        std::sin(bearingRadians) * sinDistance * cosLat,
        cosDistance - sinLat * std::sin(latitude));
    return QPointF(wrappedLongitudeDegrees(qRadiansToDegrees(longitude)),
                   qRadiansToDegrees(latitude));
}

QPointF moonSublunarPoint(double stationLatitude,
                         double stationLongitude,
                         double azimuthDegrees,
                         double elevationDegrees,
                         double distanceKm)
{
    double const latitude = qDegreesToRadians(stationLatitude);
    double const longitude = qDegreesToRadians(stationLongitude);
    double const azimuth = qDegreesToRadians(azimuthDegrees);
    double const elevation = qDegreesToRadians(elevationDegrees);
    double const cosLatitude = std::cos(latitude);
    double const sinLatitude = std::sin(latitude);
    double const cosLongitude = std::cos(longitude);
    double const sinLongitude = std::sin(longitude);
    double const cosElevation = std::cos(elevation);

    double const upX = cosLatitude * cosLongitude;
    double const upY = cosLatitude * sinLongitude;
    double const upZ = sinLatitude;
    double const eastX = -sinLongitude;
    double const eastY = cosLongitude;
    double const northX = -sinLatitude * cosLongitude;
    double const northY = -sinLatitude * sinLongitude;
    double const northZ = cosLatitude;
    double const lineOfSightX =
        cosElevation * std::sin(azimuth) * eastX
        + cosElevation * std::cos(azimuth) * northX
        + std::sin(elevation) * upX;
    double const lineOfSightY =
        cosElevation * std::sin(azimuth) * eastY
        + cosElevation * std::cos(azimuth) * northY
        + std::sin(elevation) * upY;
    double const lineOfSightZ =
        cosElevation * std::cos(azimuth) * northZ
        + std::sin(elevation) * upZ;

    double const moonX = kEarthRadiusKm * upX + distanceKm * lineOfSightX;
    double const moonY = kEarthRadiusKm * upY + distanceKm * lineOfSightY;
    double const moonZ = kEarthRadiusKm * upZ + distanceKm * lineOfSightZ;
    double const moonNorm =
        std::sqrt(moonX * moonX + moonY * moonY + moonZ * moonZ);
    if (!std::isfinite(moonNorm) || moonNorm <= kEarthRadiusKm) {
        double const invalid = std::numeric_limits<double>::quiet_NaN();
        return QPointF(invalid, invalid);
    }

    return QPointF(
        wrappedLongitudeDegrees(qRadiansToDegrees(std::atan2(moonY, moonX))),
        qRadiansToDegrees(
            std::asin(qBound(-1.0, moonZ / moonNorm, 1.0))));
}

QPointF mapPoint(const QPointF& lonLat, const QSize& size)
{
    return QPointF((lonLat.x() + 180.0) / 360.0 * size.width(),
                   (90.0 - lonLat.y()) / 180.0 * size.height());
}

QColor tropoColor(double maxDistanceKm, qint64 ageSeconds)
{
    int red = 0;
    int green = 0;
    if (maxDistanceKm > 250.0) {
        red = 255;
        green = qBound(0, qRound(255.0 - (maxDistanceKm - 250.0) * 255.0 / 500.0), 255);
    } else {
        green = 255;
        red = qBound(0, qRound(maxDistanceKm * 255.0 / 250.0), 255);
    }
    double const ageFactor = 1.0 - qBound(0.0, ageSeconds / 3600.0, 1.0);
    return QColor(red, green, 0, qBound(24, qRound(118.0 * ageFactor), 118));
}

QString providerCacheSuffix(const QString& layerId)
{
    return layerId == QStringLiteral("tropo")
            || layerId == QStringLiteral("earthquakes")
            || layerId == QStringLiteral("wildfires")
        ? QStringLiteral(".json")
        : QStringLiteral(".img");
}

bool isJsonProvider(const QString& layerId)
{
    return layerId == QStringLiteral("tropo")
        || layerId == QStringLiteral("earthquakes")
        || layerId == QStringLiteral("wildfires");
}

} // namespace

MapExternalOverlayService::MapExternalOverlayService(MapLayerModel* layerModel,
                                                     QObject* parent,
                                                     const QString& cachePath)
    : QObject(parent)
    , m_layerModel(layerModel)
    , m_network(new QNetworkAccessManager(this))
    , m_cachePath(cachePath.trimmed().isEmpty()
          ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                .absoluteFilePath(QStringLiteral("map-overlays"))
          : QFileInfo(cachePath).absoluteFilePath())
{
    m_workerPool.setMaxThreadCount(1);
    m_workerPool.setExpiryTimeout(30000);
    m_workerPool.setThreadPriority(QThread::LowPriority);
    QDir().mkpath(m_cachePath);

    auto addProvider = [this](const QString& id,
                              const QString& label,
                              const QString& attribution,
                              int refreshSeconds,
                              int validitySeconds,
                              int opacityPercent,
                              bool derived) {
        Provider provider;
        provider.id = id;
        provider.label = label;
        provider.attribution = attribution;
        provider.refreshSeconds = refreshSeconds;
        provider.validitySeconds = validitySeconds;
        provider.opacityPercent = opacityPercent;
        provider.derived = derived;
        provider.enabled = m_layerModel && m_layerModel->layerEnabled(id);
        provider.timer = new QTimer(this);
        provider.timer->setSingleShot(true);
        connect(provider.timer, &QTimer::timeout, this, [this, id] {
            refreshLayer(id);
        });
        m_providers.insert(id, provider);
    };

    addProvider(QStringLiteral("radar"), QStringLiteral("RADAR WORLD"),
                QStringLiteral("RainViewer (global)"), 300, 900, 72, false);
    addProvider(QStringLiteral("muf"), QStringLiteral("MUF"),
                QStringLiteral("KC2G"), 900, 1800, 52, false);
    addProvider(QStringLiteral("fof2"), QStringLiteral("foF2"),
                QStringLiteral("KC2G"), 900, 1800, 52, false);
    addProvider(QStringLiteral("nvis"), QStringLiteral("NVIS"),
                QStringLiteral("KC2G · foF2-derived"), 900, 1800, 44, true);
    addProvider(QStringLiteral("es"), QStringLiteral("Sporadic-E"),
                QStringLiteral("PROPquest"), 3600, 10800, 58, false);
    addProvider(QStringLiteral("aurora"), QStringLiteral("AURORA"),
                QStringLiteral("NOAA SWPC"), 360, 900, 62, false);
    addProvider(QStringLiteral("tropo"), QStringLiteral("TROPO"),
                QStringLiteral("DXView"), 60, 300, 58, false);
    addProvider(QStringLiteral("lightning"), QStringLiteral("LIGHTNING"),
                QStringLiteral("NOAA nowCOAST"), 900, 1800, 90, false);
    addProvider(QStringLiteral("earthquakes"), QStringLiteral("EARTHQUAKES"),
                QStringLiteral("USGS Earthquake Hazards Program"), 300, 900, 94, false);
    addProvider(QStringLiteral("wildfires"), QStringLiteral("WILDFIRES"),
                QStringLiteral("NASA EONET"), 900, 1800, 92, false);
    m_providers[QStringLiteral("radar")].attributionUrl =
        QStringLiteral("https://www.rainviewer.com/");
    m_providers[QStringLiteral("muf")].attributionUrl =
        QStringLiteral("https://prop.kc2g.com/");
    m_providers[QStringLiteral("fof2")].attributionUrl =
        QStringLiteral("https://prop.kc2g.com/");
    m_providers[QStringLiteral("nvis")].attributionUrl =
        QStringLiteral("https://prop.kc2g.com/");
    m_providers[QStringLiteral("es")].attributionUrl =
        QStringLiteral("https://www.propquest.co.uk/");
    m_providers[QStringLiteral("aurora")].attributionUrl =
        QStringLiteral("https://www.swpc.noaa.gov/");
    m_providers[QStringLiteral("earthquakes")].attributionUrl =
        QStringLiteral(
            "https://earthquake.usgs.gov/earthquakes/feed/v1.0/geojson.php");
    m_providers[QStringLiteral("wildfires")].attributionUrl =
        QStringLiteral("https://eonet.gsfc.nasa.gov/docs/v3");

    m_moonEnabled = m_layerModel
        && m_layerModel->layerEnabled(QStringLiteral("moon"));
    if (m_layerModel) {
        connect(m_layerModel, &MapLayerModel::layerToggled,
                this, &MapExternalOverlayService::setLayerEnabled);
        connect(m_layerModel, &MapLayerModel::layerStyleChanged,
                this, &MapExternalOverlayService::setLayerStyle);
        for (auto it = m_providers.begin(); it != m_providers.end(); ++it) {
            setLayerStyle(it.key());
        }
    }

    for (auto it = m_providers.begin(); it != m_providers.end(); ++it) {
        if (it->enabled) {
            requestProvider(it.key());
            loadCache(it.key());
        }
    }
    m_ageTimer.setInterval(30000);
    connect(&m_ageTimer, &QTimer::timeout,
            this, &MapExternalOverlayService::refreshProviderAges);
    m_ageTimer.start();
    updateProviderStatus();
}

MapExternalOverlayService::~MapExternalOverlayService()
{
    m_workerPool.clear();
    m_workerPool.waitForDone(5000);
}

QVariantList MapExternalOverlayService::providerStatus() const
{
    static const QStringList order {
        QStringLiteral("radar"), QStringLiteral("lightning"),
        QStringLiteral("muf"), QStringLiteral("fof2"),
        QStringLiteral("nvis"), QStringLiteral("es"), QStringLiteral("aurora"),
        QStringLiteral("tropo"), QStringLiteral("earthquakes"),
        QStringLiteral("wildfires")
    };
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    QVariantList rows;
    rows.reserve(order.size());
    for (QString const& id : order) {
        auto const it = m_providers.constFind(id);
        if (it == m_providers.cend()) {
            continue;
        }
        Provider const& provider = it.value();
        QVariantMap row;
        row.insert(QStringLiteral("layerId"), provider.id);
        row.insert(QStringLiteral("id"), provider.id);
        row.insert(QStringLiteral("label"), provider.label);
        row.insert(QStringLiteral("enabled"), provider.enabled);
        row.insert(QStringLiteral("loading"), provider.loading);
        row.insert(QStringLiteral("available"), m_providerImages.contains(provider.id));
        row.insert(QStringLiteral("count"), provider.itemCount);
        row.insert(QStringLiteral("itemCount"), provider.itemCount);
        row.insert(QStringLiteral("updatedMs"), provider.updatedMs);
        row.insert(QStringLiteral("validFromMs"), provider.updatedMs);
        row.insert(QStringLiteral("validUntilMs"), provider.validUntilMs);
        qint64 const ageSeconds = provider.updatedMs > 0
            ? qMax<qint64>(0, (nowMs - provider.updatedMs) / 1000)
            : -1;
        double freshness = 0.0;
        if (ageSeconds >= 0 && provider.validitySeconds > 0) {
            freshness = qBound(0.0,
                               1.0 - static_cast<double>(ageSeconds)
                                   / provider.validitySeconds,
                               1.0);
        }
        bool const stale = provider.updatedMs > 0
            && (provider.validUntilMs <= 0 || nowMs >= provider.validUntilMs);
        QString state = QStringLiteral("waiting");
        if (!provider.enabled) {
            state = QStringLiteral("disabled");
        } else if (m_offlineMode) {
            state = provider.updatedMs > 0
                ? QStringLiteral("offline-cache")
                : QStringLiteral("offline");
        } else if (provider.loading) {
            state = QStringLiteral("loading");
        } else if (!provider.error.isEmpty() && !provider.updatedMs) {
            state = QStringLiteral("error");
        } else if (!provider.updatedMs) {
            state = QStringLiteral("waiting");
        } else if (stale) {
            state = QStringLiteral("stale");
        } else {
            state = QStringLiteral("current");
        }
        QString stateText = state;
        if (state == QStringLiteral("current")) {
            stateText = tr("current");
        } else if (state == QStringLiteral("stale")) {
            stateText = tr("stale");
        } else if (state == QStringLiteral("offline-cache")) {
            stateText = tr("offline cache");
        } else if (state == QStringLiteral("loading")) {
            stateText = tr("loading");
        }
        row.insert(QStringLiteral("ageSeconds"), ageSeconds);
        row.insert(QStringLiteral("validitySeconds"), provider.validitySeconds);
        row.insert(QStringLiteral("freshness"), freshness);
        row.insert(QStringLiteral("stale"), stale);
        row.insert(QStringLiteral("state"), state);
        row.insert(QStringLiteral("stateText"), stateText);
        row.insert(QStringLiteral("updatedText"), provider.updatedMs > 0
            ? QDateTime::fromMSecsSinceEpoch(provider.updatedMs, QTimeZone::UTC)
                  .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss 'UTC'"))
            : QString());
        row.insert(QStringLiteral("validUntilText"), provider.validUntilMs > 0
            ? QDateTime::fromMSecsSinceEpoch(provider.validUntilMs, QTimeZone::UTC)
                  .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss 'UTC'"))
            : QString());
        row.insert(QStringLiteral("derived"), provider.derived);
        row.insert(QStringLiteral("derivedFrom"), provider.derived
            ? QStringLiteral("fof2") : QString());
        row.insert(QStringLiteral("decayOpacity"), provider.updatedMs > 0
            ? qMax(0.12, freshness) : 0.0);
        row.insert(QStringLiteral("error"), provider.error);
        row.insert(QStringLiteral("attribution"), provider.attribution);
        row.insert(QStringLiteral("attributionUrl"), provider.attributionUrl);
        row.insert(QStringLiteral("sourceUrl"), provider.sourceUrl);
        rows.append(row);
    }
    return rows;
}

QVariantList MapExternalOverlayService::temporalLegend() const
{
    QVariantList rows;
    QVariantList const statuses = providerStatus();
    for (QVariant const& value : statuses) {
        QVariantMap const row = value.toMap();
        QString const id = row.value(QStringLiteral("layerId")).toString();
        if (id == QStringLiteral("muf")
            || id == QStringLiteral("fof2")
            || id == QStringLiteral("nvis")
            || id == QStringLiteral("es")
            || id == QStringLiteral("aurora")) {
            rows.append(value);
        }
    }
    return rows;
}

void MapExternalOverlayService::refreshAll()
{
    if (m_offlineMode) {
        return;
    }
    for (auto it = m_providers.cbegin(); it != m_providers.cend(); ++it) {
        if (it->enabled) {
            requestProvider(it.key());
        }
    }
}

void MapExternalOverlayService::refreshProviderAges()
{
    // Age is part of the rendered semantics, not only a diagnostic field: a
    // cached forecast must visibly decay while it waits for the next update.
    rebuildComposite();
    updateProviderStatus();
}

void MapExternalOverlayService::refreshLayer(const QString& layerId)
{
    if (m_offlineMode) {
        return;
    }
    QString const id = normalizedLayerId(layerId);
    auto const it = m_providers.constFind(id);
    if (it == m_providers.cend() || !it->enabled) {
        return;
    }
    requestProvider(id);
}

void MapExternalOverlayService::setOfflineMode(bool offline)
{
    if (m_offlineMode == offline) {
        return;
    }

    m_offlineMode = offline;
    if (m_offlineMode) {
        for (auto it = m_providers.begin(); it != m_providers.end(); ++it) {
            ++it->generation;
            it->timer->stop();
            setProviderLoading(it.value(), false);
            it->error = QStringLiteral("Offline mode");
        }
        for (QNetworkReply* reply : findChildren<QNetworkReply*>()) {
            reply->abort();
        }
        // Keep the last accepted raster visible and mark it as an offline
        // cache entry.  Offline mode stops network activity; it must not
        // destroy the forecast cache that the temporal legend describes.
        if (!m_earthquakeFeatures.isEmpty()) {
            m_earthquakeFeatures.clear();
            emit earthquakeFeaturesChanged();
        }
        if (m_layerModel) {
            for (auto it = m_providers.cbegin(); it != m_providers.cend(); ++it) {
                m_layerModel->setCount(it.key(), 0);
            }
        }
        rebuildComposite();
    } else {
        for (auto it = m_providers.begin(); it != m_providers.end(); ++it) {
            it->error.clear();
            if (it->enabled) {
                loadCache(it.key());
                requestProvider(it.key());
            }
        }
    }
    updateProviderStatus();
    emit offlineModeChanged();
}

QString MapExternalOverlayService::normalizedLayerId(const QString& value)
{
    return value.trimmed().toLower();
}

void MapExternalOverlayService::setLayerEnabled(const QString& layerId, bool enabled)
{
    QString const id = normalizedLayerId(layerId);
    if (id == QStringLiteral("moon")) {
        if (m_moonEnabled == enabled) {
            return;
        }
        m_moonEnabled = enabled;
        ++m_moonGeneration;
        if (!enabled) {
            bool const hadMoonData = m_moonDataAvailable;
            m_moonDataAvailable = false;
            m_providerImages.remove(id);
            if (m_layerModel) {
                m_layerModel->setCount(id, 0);
            }
            if (hadMoonData) {
                emit moonDataChanged();
            }
            rebuildComposite();
        }
        return;
    }

    auto it = m_providers.find(id);
    if (it == m_providers.end() || it->enabled == enabled) {
        return;
    }

    it->enabled = enabled;
    ++it->generation;
    if (!enabled) {
        it->timer->stop();
        setProviderLoading(it.value(), false);
        m_providerImages.remove(id);
        if (id == QStringLiteral("earthquakes") && !m_earthquakeFeatures.isEmpty()) {
            m_earthquakeFeatures.clear();
            emit earthquakeFeaturesChanged();
        }
        it->itemCount = 0;
        if (m_layerModel) {
            m_layerModel->setCount(id, 0);
        }
        rebuildComposite();
        updateProviderStatus();
        return;
    }

    loadCache(id);
    if (!m_offlineMode) {
        requestProvider(id);
    }
}

void MapExternalOverlayService::setLayerStyle(const QString& layerId)
{
    if (!m_layerModel) {
        return;
    }
    auto it = m_providers.find(layerId.trimmed().toLower());
    if (it == m_providers.end()) {
        return;
    }
    QVariantMap const style = m_layerModel->layerStyle(layerId);
    it->opacityPercent = qBound(5,
        qRound(style.value(QStringLiteral("opacity"), 1.0).toDouble() * 100.0), 100);
    rebuildComposite();
}

QString MapExternalOverlayService::providerUrl(const QString& layerId,
                                               int fallbackOffset)
{
    QString const id = normalizedLayerId(layerId);
    if (id == QStringLiteral("radar")) {
        if (fallbackOffset == 0) {
            return QStringLiteral(
                "https://api.rainviewer.com/public/weather-maps.json");
        }
        return mercatorWmsUrl(
            QStringLiteral("https://mapservices.weather.noaa.gov/eventdriven/services/"
                           "radar/radar_base_reflectivity/MapServer/WMSServer"),
            QStringLiteral("1"), QString());
    }
    if (id == QStringLiteral("lightning")) {
        return wmsUrl(
            QStringLiteral("https://nowcoast.noaa.gov/geoserver/observations/"
                           "lightning_detection/ows"),
            QStringLiteral("ldn_lightning_strike_density"),
            QStringLiteral("lightning_density"));
    }
    if (id == QStringLiteral("muf")) {
        qint64 const bucket = QDateTime::currentSecsSinceEpoch() / 900;
        return QStringLiteral("https://tagloomis.com/pred/muf/img/muf.png?v=%1")
            .arg(bucket);
    }
    if (id == QStringLiteral("fof2")) {
        qint64 const bucket = QDateTime::currentSecsSinceEpoch() / 900;
        return QStringLiteral("https://tagloomis.com/pred/muf/img/fof2.png?v=%1")
            .arg(bucket);
    }
    if (id == QStringLiteral("nvis")) {
        // The critical frequency (foF2) is the operational input used to
        // select an NVIS working frequency.  The source does not publish a
        // separate NVIS raster, so keep a first-class NVIS layer while
        // explicitly deriving it from the same current foF2 forecast.
        qint64 const bucket = QDateTime::currentSecsSinceEpoch() / 900;
        return QStringLiteral("https://tagloomis.com/pred/muf/img/fof2.png?v=%1")
            .arg(bucket);
    }
    if (id == QStringLiteral("aurora")) {
        qint64 const bucket = QDateTime::currentSecsSinceEpoch() / 360;
        return QStringLiteral("https://tagloomis.com/pred/auf/img/auf.png?v=%1")
            .arg(bucket);
    }
    if (id == QStringLiteral("es")) {
        qint64 const epochHour =
            (QDateTime::currentSecsSinceEpoch() / 3600 - fallbackOffset) * 3600;
        return QStringLiteral("https://tagloomis.com/pred/epi/img/%1.jpg")
            .arg(epochHour);
    }
    if (id == QStringLiteral("tropo")) {
        return QStringLiteral(
            "https://vhf.dxview.org/map/refresh?"
            "band=50&alert_id=0&size=2.0000e-4&squares=AARR");
    }
    if (id == QStringLiteral("earthquakes")) {
        return QStringLiteral(
            "https://earthquake.usgs.gov/earthquakes/feed/v1.0/"
            "summary/2.5_day.geojson");
    }
    if (id == QStringLiteral("wildfires")) {
        return QStringLiteral(
            "https://eonet.gsfc.nasa.gov/api/v3/events?"
            "category=wildfires&status=open&limit=250");
    }
    return {};
}

void MapExternalOverlayService::requestProvider(const QString& layerId,
                                                int fallbackOffset)
{
    if (m_offlineMode) {
        return;
    }
    auto it = m_providers.find(normalizedLayerId(layerId));
    if (it == m_providers.end() || !it->enabled || it->loading) {
        return;
    }

    QString const urlText = providerUrl(it->id, fallbackOffset);
    if (urlText.isEmpty()) {
        it->error = QStringLiteral("No data source configured");
        updateProviderStatus();
        return;
    }

    ++it->generation;
    int const generation = it->generation;
    it->sourceUrl = urlText;
    if (it->id == QStringLiteral("radar")) {
        it->attribution = fallbackOffset == 0
            ? QStringLiteral("RainViewer (global)")
            : QStringLiteral("NOAA/NWS (USA fallback)");
    }
    it->error.clear();
    setProviderLoading(it.value(), true);
    updateProviderStatus();

    QNetworkRequest request {QUrl(urlText)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Decodium/4 MapOverlay"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kNetworkTimeoutMs);
    QNetworkReply* reply = m_network->get(request);
    reply->setProperty("layerId", it->id);
    reply->setProperty("generation", generation);
    reply->setProperty("fallbackOffset", fallbackOffset);
    reply->setProperty("rainViewerMetadata",
                       it->id == QStringLiteral("radar")
                           && fallbackOffset == 0);
    reply->setProperty("rainViewerTile", false);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleReply(reply);
    });
}

void MapExternalOverlayService::requestRainViewerTile(
    const QString& layerId,
    int generation,
    const QByteArray& metadata)
{
    if (m_offlineMode) {
        return;
    }
    auto failToNoaa = [this, layerId](const QString& error) {
        auto it = m_providers.find(layerId);
        if (it == m_providers.end()) {
            return;
        }
        setProviderLoading(it.value(), false);
        it->error = error;
        updateProviderStatus();
        QTimer::singleShot(0, this, [this, layerId] {
            requestProvider(layerId, 1);
        });
    };

    QJsonParseError parseError;
    QJsonDocument const document =
        QJsonDocument::fromJson(metadata, &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        failToNoaa(QStringLiteral("RainViewer metadata: %1")
                       .arg(parseError.errorString()));
        return;
    }

    QJsonObject const root = document.object();
    QUrl const host(root.value(QStringLiteral("host")).toString());
    QJsonArray const frames =
        root.value(QStringLiteral("radar")).toObject()
            .value(QStringLiteral("past")).toArray();
    QString const framePath = frames.isEmpty()
        ? QString()
        : frames.at(frames.size() - 1).toObject()
              .value(QStringLiteral("path")).toString();
    if (!host.isValid()
        || host.scheme() != QStringLiteral("https")
        || !host.host().endsWith(QStringLiteral("rainviewer.com"))
        || framePath.isEmpty()
        || !framePath.startsWith(QLatin1Char('/'))) {
        failToNoaa(QStringLiteral("RainViewer returned invalid tile metadata"));
        return;
    }

    QString const tileUrl =
        host.toString(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment)
        + framePath
        + QStringLiteral("/512/0/0/0/2/1_1.png");
    auto it = m_providers.find(layerId);
    if (it == m_providers.end()
        || !it->enabled
        || it->generation != generation) {
        return;
    }
    it->sourceUrl = tileUrl;
    it->attribution = QStringLiteral("RainViewer (global)");
    updateProviderStatus();

    QNetworkRequest request {QUrl(tileUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Decodium/4 MapOverlay"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kNetworkTimeoutMs);
    QNetworkReply* reply = m_network->get(request);
    reply->setProperty("layerId", layerId);
    reply->setProperty("generation", generation);
    reply->setProperty("fallbackOffset", 0);
    reply->setProperty("rainViewerMetadata", false);
    reply->setProperty("rainViewerTile", true);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleReply(reply);
    });
}

void MapExternalOverlayService::handleReply(QNetworkReply* reply)
{
    QString const id = reply->property("layerId").toString();
    int const generation = reply->property("generation").toInt();
    int const fallbackOffset = reply->property("fallbackOffset").toInt();
    bool const rainViewerMetadata =
        reply->property("rainViewerMetadata").toBool();
    bool const rainViewerTile =
        reply->property("rainViewerTile").toBool();
    int const httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray payload;
    if (reply->error() == QNetworkReply::NoError) {
        payload = reply->readAll();
    }
    QString error = reply->error() == QNetworkReply::NoError
        ? QString()
        : reply->errorString();
    reply->deleteLater();

    if (m_offlineMode) {
        return;
    }

    auto it = m_providers.find(id);
    if (it == m_providers.end() || generation != it->generation) {
        return;
    }

    if (rainViewerMetadata) {
        if (!payload.isEmpty()) {
            requestRainViewerTile(id, generation, payload);
            return;
        }
        setProviderLoading(it.value(), false);
        it->error = error.isEmpty()
            ? QStringLiteral("RainViewer metadata unavailable")
            : error;
        updateProviderStatus();
        QTimer::singleShot(0, this, [this, id] {
            requestProvider(id, 1);
        });
        return;
    }

    setProviderLoading(it.value(), false);

    if (payload.isEmpty()) {
        if (id == QStringLiteral("radar") && rainViewerTile) {
            it->error = error.isEmpty()
                ? QStringLiteral("RainViewer tile unavailable")
                : error;
            updateProviderStatus();
            QTimer::singleShot(0, this, [this, id] {
                requestProvider(id, 1);
            });
            return;
        }
        if (id == QStringLiteral("es")
            && fallbackOffset < 4
            && (httpStatus == 404 || httpStatus == 0)) {
            QTimer::singleShot(0, this, [this, id, fallbackOffset] {
                requestProvider(id, fallbackOffset + 1);
            });
            return;
        }
        it->error = error.isEmpty()
            ? QStringLiteral("Empty response")
            : error;
        if (!m_providerImages.contains(id)) {
            loadCache(id);
        }
        it->timer->start(qMax(60, it->refreshSeconds / 3) * 1000);
        updateProviderStatus();
        return;
    }

    int const maxBytes = isJsonProvider(id)
        ? kMaxJsonPayloadBytes : kMaxImagePayloadBytes;
    if (payload.size() > maxBytes) {
        it->error = QStringLiteral("Response exceeds safety limit");
        it->timer->start(qMax(60, it->refreshSeconds / 3) * 1000);
        updateProviderStatus();
        return;
    }

    saveCache(id, payload);
    processPayloadAsync(id, payload, generation, false);
}

MapExternalOverlayService::ProcessedPayload
MapExternalOverlayService::processPayload(const QString& layerId,
                                          const QByteArray& payload)
{
    ProcessedPayload result;
    if (layerId == QStringLiteral("tropo")) {
        result.image = renderTropoPayload(payload, &result.itemCount, &result.error);
        return result;
    }
    if (layerId == QStringLiteral("earthquakes")) {
        result.image =
            renderEarthquakePayload(payload, &result.itemCount, &result.error);
        if (!result.image.isNull()) {
            QString featureError;
            result.features = parseEarthquakeFeatures(payload, &featureError);
            if (result.features.isEmpty() && !featureError.isEmpty()) {
                result.error = featureError;
            }
        }
        return result;
    }
    if (layerId == QStringLiteral("wildfires")) {
        result.image =
            renderWildfirePayload(payload, &result.itemCount, &result.error);
        return result;
    }

    QImage source;
    if (!source.loadFromData(payload)) {
        result.error = QStringLiteral("Unsupported or corrupt image");
        return result;
    }

    if (layerId == QStringLiteral("muf")
        || layerId == QStringLiteral("fof2")
        || layerId == QStringLiteral("nvis")
        || layerId == QStringLiteral("es")
        || layerId == QStringLiteral("aurora")
        || layerId == QStringLiteral("radar")) {
        result.image = webMercatorToEquirectangular(source);
    } else {
        result.image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied)
                           .scaled(QSize(1024, 512), Qt::IgnoreAspectRatio,
                                   Qt::SmoothTransformation);
    }
    if (result.image.isNull()) {
        result.error = QStringLiteral("Image conversion failed");
    } else {
        result.itemCount = 1;
    }
    return result;
}

void MapExternalOverlayService::processPayloadAsync(const QString& layerId,
                                                    const QByteArray& payload,
                                                    int generation,
                                                    bool fromCache)
{
    QPointer<MapExternalOverlayService> guard(this);
    m_workerPool.start(QRunnable::create(
        [guard, layerId, payload, generation, fromCache] {
            ProcessedPayload result = processPayload(layerId, payload);
            if (!guard) {
                return;
            }
            QMetaObject::invokeMethod(
                guard.data(),
                [guard, layerId, generation,
                 result = std::move(result), fromCache]() mutable {
                    if (guard) {
                        guard->applyProcessedPayload(
                            layerId, generation, std::move(result), fromCache);
                    }
                },
                Qt::QueuedConnection);
        }));
}

void MapExternalOverlayService::applyProcessedPayload(
    const QString& layerId,
    int generation,
    ProcessedPayload result,
    bool fromCache)
{
    auto it = m_providers.find(layerId);
    if (it == m_providers.end()
        || !it->enabled
        || (m_offlineMode && !fromCache)
        || generation != it->generation) {
        return;
    }
    if (fromCache && it->appliedGeneration == generation) {
        return;
    }

    if (!result.image.isNull()) {
        m_providerImages.insert(layerId, std::move(result.image));
        if (layerId == QStringLiteral("earthquakes")
            && m_earthquakeFeatures != result.features) {
            m_earthquakeFeatures = std::move(result.features);
            emit earthquakeFeaturesChanged();
        }
        it->itemCount = result.itemCount;
        it->updatedMs = fromCache
            ? QFileInfo(cacheFilePath(layerId)).lastModified().toMSecsSinceEpoch()
            : QDateTime::currentMSecsSinceEpoch();
        it->validUntilMs = it->updatedMs
            + static_cast<qint64>(it->validitySeconds) * 1000;
        it->appliedGeneration = generation;
        it->error.clear();
        if (m_layerModel) {
            m_layerModel->setCount(layerId, qMax(1, it->itemCount));
        }
        rebuildComposite();
    } else {
        it->error = result.error.isEmpty()
            ? QStringLiteral("Data processing failed")
            : result.error;
    }

    it->timer->start(it->refreshSeconds * 1000);
    updateProviderStatus();
}

void MapExternalOverlayService::loadCache(const QString& layerId)
{
    QFile file(cacheFilePath(layerId));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QByteArray const payload = file.read(
        isJsonProvider(layerId)
            ? kMaxJsonPayloadBytes + 1
            : kMaxImagePayloadBytes + 1);
    if (payload.isEmpty()
        || payload.size() > (isJsonProvider(layerId)
                                 ? kMaxJsonPayloadBytes
                                 : kMaxImagePayloadBytes)) {
        return;
    }
    auto it = m_providers.find(layerId);
    if (it != m_providers.end()) {
        processPayloadAsync(layerId, payload, it->generation, true);
    }
}

void MapExternalOverlayService::saveCache(const QString& layerId,
                                          const QByteArray& payload) const
{
    QSaveFile file(cacheFilePath(layerId));
    if (file.open(QIODevice::WriteOnly)
        && file.write(payload) == payload.size()) {
        file.commit();
    }
}

QString MapExternalOverlayService::cacheFilePath(const QString& layerId) const
{
    return QDir(m_cachePath).absoluteFilePath(
        layerId + providerCacheSuffix(layerId));
}

void MapExternalOverlayService::rebuildComposite()
{
    static const QStringList drawOrder {
        QStringLiteral("moon"),
        QStringLiteral("muf"), QStringLiteral("fof2"), QStringLiteral("nvis"),
        QStringLiteral("es"), QStringLiteral("aurora"),
        QStringLiteral("tropo"), QStringLiteral("radar"),
        QStringLiteral("lightning"), QStringLiteral("wildfires"),
        QStringLiteral("earthquakes")
    };

    QImage composite;
    QPainter painter;
    for (QString const& id : drawOrder) {
        auto const image = m_providerImages.constFind(id);
        if (image == m_providerImages.cend() || image->isNull()) {
            continue;
        }
        int opacityPercent = 100;
        if (id == QStringLiteral("moon")) {
            if (!m_moonEnabled) {
                continue;
            }
            opacityPercent = 78;
        } else {
            auto const provider = m_providers.constFind(id);
            if (provider == m_providers.cend() || !provider->enabled) {
                continue;
            }
            double decay = 1.0;
            if (provider->updatedMs > 0 && provider->validitySeconds > 0) {
                qint64 const ageSeconds = qMax<qint64>(
                    0,
                    (QDateTime::currentMSecsSinceEpoch() - provider->updatedMs)
                        / 1000);
                decay = qMax(0.12,
                             1.0 - static_cast<double>(ageSeconds)
                                 / provider->validitySeconds);
            }
            opacityPercent = qRound(provider->opacityPercent * decay);
        }
        if (composite.isNull()) {
            composite = QImage(1024, 512, QImage::Format_ARGB32_Premultiplied);
            composite.fill(Qt::transparent);
            painter.begin(&composite);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        }
        painter.setOpacity(opacityPercent / 100.0);
        painter.drawImage(composite.rect(), image.value());
    }
    if (painter.isActive()) {
        painter.end();
    }

    if (composite.cacheKey() == m_overlayImage.cacheKey()
        && composite.size() == m_overlayImage.size()) {
        return;
    }
    m_overlayImage = std::move(composite);
    emit overlayImageChanged();
}

void MapExternalOverlayService::setMoonData(
    bool enabled,
    double stationLatitude,
    double stationLongitude,
    double azimuthDegrees,
    double elevationDegrees,
    double distanceKm,
    double illuminationPercent)
{
    bool const valid =
        enabled && m_moonEnabled
        && std::isfinite(stationLatitude)
        && std::isfinite(stationLongitude)
        && std::isfinite(azimuthDegrees)
        && std::isfinite(elevationDegrees)
        && std::isfinite(distanceKm)
        && std::isfinite(illuminationPercent)
        && stationLatitude >= -90.0
        && stationLatitude <= 90.0
        && stationLongitude >= -180.0
        && stationLongitude <= 180.0
        && distanceKm > kEarthRadiusKm;
    QPointF const sublunar = valid
        ? moonSublunarPoint(stationLatitude, stationLongitude,
                            azimuthDegrees, elevationDegrees, distanceKm)
        : QPointF();
    bool const sublunarValid =
        valid && std::isfinite(sublunar.x()) && std::isfinite(sublunar.y());
    if (!sublunarValid) {
        ++m_moonGeneration;
        bool const hadMoonData = m_moonDataAvailable;
        m_moonDataAvailable = false;
        m_moonSublunarLatitude = 0.0;
        m_moonSublunarLongitude = 0.0;
        if (m_layerModel) {
            m_layerModel->setCount(QStringLiteral("moon"), 0);
        }
        if (m_providerImages.remove(QStringLiteral("moon")) > 0) {
            rebuildComposite();
        }
        if (hadMoonData) {
            emit moonDataChanged();
        }
        return;
    }

    bool const stateChanged =
        !m_moonDataAvailable
        || !qFuzzyCompare(m_moonAzimuth, azimuthDegrees)
        || !qFuzzyCompare(m_moonElevation, elevationDegrees)
        || !qFuzzyCompare(m_moonDistanceKm, distanceKm)
        || !qFuzzyCompare(m_moonIllumination, illuminationPercent)
        || !qFuzzyCompare(m_moonSublunarLongitude, sublunar.x())
        || !qFuzzyCompare(m_moonSublunarLatitude, sublunar.y());
    m_moonDataAvailable = true;
    m_moonAzimuth = azimuthDegrees;
    m_moonElevation = elevationDegrees;
    m_moonDistanceKm = distanceKm;
    m_moonIllumination = qBound(0.0, illuminationPercent, 100.0);
    m_moonSublunarLongitude = sublunar.x();
    m_moonSublunarLatitude = sublunar.y();
    // The vector Moon marker and station data are already valid here.  Do not
    // report a false zero while the optional raster visibility overlay waits
    // behind another external-overlay render job.
    if (m_layerModel) {
        m_layerModel->setCount(QStringLiteral("moon"), 1);
    }
    if (stateChanged) {
        emit moonDataChanged();
    }

    int const generation = ++m_moonGeneration;
    QPointer<MapExternalOverlayService> guard(this);
    m_workerPool.start(QRunnable::create(
        [guard, generation, stationLatitude, stationLongitude,
         azimuthDegrees, elevationDegrees, distanceKm,
         illuminationPercent] {
            QImage image = renderMoonOverlay(
                stationLatitude, stationLongitude,
                azimuthDegrees, elevationDegrees,
                distanceKm, illuminationPercent);
            if (!guard) {
                return;
            }
            QMetaObject::invokeMethod(
                guard.data(),
                [guard, generation, image = std::move(image)]() mutable {
                    if (!guard
                        || !guard->m_moonEnabled
                        || guard->m_moonGeneration != generation) {
                        return;
                    }
                    if (image.isNull()) {
                        bool const hadMoonData = guard->m_moonDataAvailable;
                        guard->m_moonDataAvailable = false;
                        guard->m_providerImages.remove(QStringLiteral("moon"));
                        if (guard->m_layerModel) {
                            guard->m_layerModel->setCount(
                                QStringLiteral("moon"), 0);
                        }
                        if (hadMoonData) {
                            emit guard->moonDataChanged();
                        }
                    } else {
                        guard->m_providerImages.insert(
                            QStringLiteral("moon"), std::move(image));
                        if (guard->m_layerModel) {
                            guard->m_layerModel->setCount(
                                QStringLiteral("moon"), 1);
                        }
                    }
                    guard->rebuildComposite();
                },
                Qt::QueuedConnection);
        }));
}

void MapExternalOverlayService::updateMoonForStation(
    double stationLatitude,
    double stationLongitude)
{
    // The service can be created after persisted layer preferences have been
    // restored.  Reconcile with the model before deciding whether to refresh.
    if (m_layerModel) {
        bool const enabled = m_layerModel->layerEnabled(QStringLiteral("moon"));
        if (enabled != m_moonEnabled) {
            setLayerEnabled(QStringLiteral("moon"), enabled);
        }
    }
    if (!m_moonEnabled) {
        setMoonData(false, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        return;
    }
    MoonEphemeris const ephemeris =
        moonEphemeris(stationLatitude, stationLongitude,
                      QDateTime::currentDateTimeUtc());
    if (!ephemeris.valid) {
        setMoonData(false, stationLatitude, stationLongitude,
                    0.0, 0.0, 0.0, 0.0);
        return;
    }
    setMoonData(true, stationLatitude, stationLongitude,
                ephemeris.azimuthDegrees,
                ephemeris.elevationDegrees,
                ephemeris.distanceKm,
                ephemeris.illuminationPercent);
}

void MapExternalOverlayService::updateProviderStatus()
{
    emit providerStatusChanged();
}

void MapExternalOverlayService::setProviderLoading(Provider& provider,
                                                   bool loading)
{
    if (provider.loading == loading) {
        return;
    }
    bool const wasLoading = m_loadingCount > 0;
    provider.loading = loading;
    m_loadingCount += loading ? 1 : -1;
    m_loadingCount = qMax(0, m_loadingCount);
    if (wasLoading != (m_loadingCount > 0)) {
        emit loadingChanged();
    }
}

QImage MapExternalOverlayService::webMercatorToEquirectangular(
    const QImage& source,
    const QSize& outputSize)
{
    if (source.isNull() || outputSize.isEmpty()) {
        return {};
    }
    QImage const input = source.convertToFormat(QImage::Format_ARGB32);
    QImage output(outputSize, QImage::Format_ARGB32_Premultiplied);
    output.fill(Qt::transparent);

    for (int y = 0; y < output.height(); ++y) {
        double const latitude =
            90.0 - (y + 0.5) * 180.0 / output.height();
        if (std::abs(latitude) > kMercatorLatitudeLimit) {
            continue;
        }
        double const latitudeRadians = qDegreesToRadians(latitude);
        double const mercatorY =
            (1.0 - std::log(std::tan(M_PI_4 + latitudeRadians / 2.0)) / M_PI)
            / 2.0;
        int const sourceY = qBound(
            0, qRound(mercatorY * (input.height() - 1)), input.height() - 1);
        QRgb* destination = reinterpret_cast<QRgb*>(output.scanLine(y));
        QRgb const* sourceLine =
            reinterpret_cast<QRgb const*>(input.constScanLine(sourceY));
        for (int x = 0; x < output.width(); ++x) {
            int const sourceX = qBound(
                0,
                qRound((x + 0.5) * input.width() / output.width() - 0.5),
                input.width() - 1);
            destination[x] = sourceLine[sourceX];
        }
    }
    return output;
}

QImage MapExternalOverlayService::renderMoonOverlay(
    double stationLatitude,
    double stationLongitude,
    double azimuthDegrees,
    double elevationDegrees,
    double distanceKm,
    double illuminationPercent,
    const QSize& outputSize)
{
    if (outputSize.isEmpty()
        || !std::isfinite(stationLatitude)
        || !std::isfinite(stationLongitude)
        || !std::isfinite(azimuthDegrees)
        || !std::isfinite(elevationDegrees)
        || !std::isfinite(distanceKm)
        || stationLatitude < -90.0
        || stationLatitude > 90.0
        || distanceKm <= kEarthRadiusKm) {
        return {};
    }

    double const latitude = qDegreesToRadians(stationLatitude);
    double const longitude = qDegreesToRadians(stationLongitude);
    double const azimuth = qDegreesToRadians(azimuthDegrees);
    double const elevation = qDegreesToRadians(elevationDegrees);

    double const cosLatitude = std::cos(latitude);
    double const sinLatitude = std::sin(latitude);
    double const cosLongitude = std::cos(longitude);
    double const sinLongitude = std::sin(longitude);
    double const cosElevation = std::cos(elevation);

    double const upX = cosLatitude * cosLongitude;
    double const upY = cosLatitude * sinLongitude;
    double const upZ = sinLatitude;
    double const eastX = -sinLongitude;
    double const eastY = cosLongitude;
    double const northX = -sinLatitude * cosLongitude;
    double const northY = -sinLatitude * sinLongitude;
    double const northZ = cosLatitude;

    double const losX =
        cosElevation * std::sin(azimuth) * eastX
        + cosElevation * std::cos(azimuth) * northX
        + std::sin(elevation) * upX;
    double const losY =
        cosElevation * std::sin(azimuth) * eastY
        + cosElevation * std::cos(azimuth) * northY
        + std::sin(elevation) * upY;
    double const losZ =
        cosElevation * std::cos(azimuth) * northZ
        + std::sin(elevation) * upZ;

    double const moonX = kEarthRadiusKm * upX + distanceKm * losX;
    double const moonY = kEarthRadiusKm * upY + distanceKm * losY;
    double const moonZ = kEarthRadiusKm * upZ + distanceKm * losZ;
    double const moonNorm =
        std::sqrt(moonX * moonX + moonY * moonY + moonZ * moonZ);
    if (!std::isfinite(moonNorm) || moonNorm <= kEarthRadiusKm) {
        return {};
    }

    double const subLatitude =
        std::asin(qBound(-1.0, moonZ / moonNorm, 1.0));
    double const subLongitude = std::atan2(moonY, moonX);
    QPointF const sublunar(
        wrappedLongitudeDegrees(qRadiansToDegrees(subLongitude)),
        qRadiansToDegrees(subLatitude));

    QImage result(outputSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    double const visibleRange = qMax(1.0, moonNorm - kEarthRadiusKm);
    for (int y = 0; y < result.height(); ++y) {
        double const surfaceLatitude = qDegreesToRadians(
            90.0 - (y + 0.5) * 180.0 / result.height());
        double const surfaceCosLatitude = std::cos(surfaceLatitude);
        double const surfaceSinLatitude = std::sin(surfaceLatitude);
        QRgb* pixels = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            double const surfaceLongitude = qDegreesToRadians(
                -180.0 + (x + 0.5) * 360.0 / result.width());
            double const normalX =
                surfaceCosLatitude * std::cos(surfaceLongitude);
            double const normalY =
                surfaceCosLatitude * std::sin(surfaceLongitude);
            double const normalZ = surfaceSinLatitude;
            double const dot =
                normalX * moonX + normalY * moonY + normalZ * moonZ;
            if (dot <= kEarthRadiusKm) {
                continue;
            }
            double const strength =
                qBound(0.0, (dot - kEarthRadiusKm) / visibleRange, 1.0);
            int const alpha = 10 + qRound(24.0 * std::sqrt(strength));
            pixels[x] = qRgba(154, 204, 255, alpha);
        }
    }

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QColor const moonColor(219, 231, 255, 220);
    QPen routePen(moonColor, 1.2, Qt::DashLine);
    routePen.setCosmetic(true);
    painter.setPen(routePen);

    double const targetX = std::cos(subLatitude) * std::cos(subLongitude);
    double const targetY = std::cos(subLatitude) * std::sin(subLongitude);
    double const targetZ = std::sin(subLatitude);
    double const endpointDot =
        qBound(-1.0, upX * targetX + upY * targetY + upZ * targetZ, 1.0);
    double const arc = std::acos(endpointDot);
    double previousLongitude = stationLongitude;
    QPainterPath route;
    for (int step = 0; step <= 64; ++step) {
        double const fraction = step / 64.0;
        double pointX = upX;
        double pointY = upY;
        double pointZ = upZ;
        if (arc > 1.0e-8) {
            double const denominator = std::sin(arc);
            double const leftWeight =
                std::sin((1.0 - fraction) * arc) / denominator;
            double const rightWeight =
                std::sin(fraction * arc) / denominator;
            pointX = leftWeight * upX + rightWeight * targetX;
            pointY = leftWeight * upY + rightWeight * targetY;
            pointZ = leftWeight * upZ + rightWeight * targetZ;
        }
        double pointLongitude =
            qRadiansToDegrees(std::atan2(pointY, pointX));
        while (pointLongitude - previousLongitude > 180.0) {
            pointLongitude -= 360.0;
        }
        while (pointLongitude - previousLongitude < -180.0) {
            pointLongitude += 360.0;
        }
        previousLongitude = pointLongitude;
        QPointF const point(
            pointLongitude,
            qRadiansToDegrees(std::asin(qBound(-1.0, pointZ, 1.0))));
        if (step == 0) {
            route.moveTo(mapPoint(point, outputSize));
        } else {
            route.lineTo(mapPoint(point, outputSize));
        }
    }
    painter.drawPath(route);
    painter.drawPath(route.translated(-outputSize.width(), 0));
    painter.drawPath(route.translated(outputSize.width(), 0));

    painter.setBrush(QColor(219, 231, 255, 45));
    painter.setPen(QPen(QColor(219, 231, 255, 150), 1.5));
    QPointF const marker = mapPoint(sublunar, outputSize);
    for (int wrap = -1; wrap <= 1; ++wrap) {
        QPointF wrapped = marker;
        wrapped.rx() += wrap * outputSize.width();
        if (wrapped.x() < -30 || wrapped.x() > outputSize.width() + 30) {
            continue;
        }
        painter.drawEllipse(wrapped, 10.0, 10.0);
        painter.setBrush(QColor(219, 231, 255, 230));
        painter.setPen(QPen(QColor(20, 35, 58, 245), 1.2));
        painter.drawEllipse(wrapped, 5.5, 5.5);

        QString const label = QStringLiteral("MOON %1%")
            .arg(qRound(qBound(0.0, illuminationPercent, 100.0)));
        QFont labelFont(QStringLiteral("Monospace"), 9, QFont::DemiBold);
        painter.setFont(labelFont);
        QFontMetrics const metrics(labelFont);
        QRectF labelRect = metrics.boundingRect(label);
        labelRect.moveTopLeft(
            wrapped + QPointF(9.0, -labelRect.height() - 4.0));
        labelRect.adjust(-4.0, -2.0, 4.0, 2.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(8, 18, 34, 220));
        painter.drawRoundedRect(labelRect, 3.0, 3.0);
        painter.setPen(QColor(235, 243, 255, 245));
        painter.drawText(labelRect, Qt::AlignCenter, label);
        painter.setBrush(QColor(219, 231, 255, 45));
        painter.setPen(QPen(QColor(219, 231, 255, 150), 1.5));
    }
    painter.end();
    return result;
}

QImage MapExternalOverlayService::renderTropoPayload(
    const QByteArray& payload,
    int* featureCount,
    QString* error,
    const QSize& outputSize)
{
    if (featureCount) {
        *featureCount = 0;
    }
    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        if (error) {
            *error = parseError.errorString();
        }
        return {};
    }

    QJsonArray const rows =
        document.object().value(QStringLiteral("update_calls")).toArray();
    QImage result(outputSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    qint64 const now = QDateTime::currentSecsSinceEpoch();
    int rendered = 0;

    for (QJsonValue const& value : rows) {
        QJsonObject const row = value.toObject();
        QJsonArray const spokes = row.value(QStringLiteral("spokes")).toArray();
        if (spokes.size() < 4 || (spokes.size() % 2) != 0) {
            continue;
        }
        double const latitude = row.value(QStringLiteral("lat")).toDouble();
        double const longitude = row.value(QStringLiteral("lon")).toDouble();
        qint64 const timestamp =
            static_cast<qint64>(row.value(QStringLiteral("ts")).toDouble());

        QVector<QPointF> perimeter;
        perimeter.reserve(spokes.size() / 2);
        double maxDistanceKm = 0.0;
        double previousLongitude = qRadiansToDegrees(longitude);
        for (qsizetype index = 0; index < spokes.size(); index += 2) {
            double const bearing = spokes.at(index).toDouble();
            double const distance = spokes.at(index + 1).toDouble();
            QPointF point = destinationPoint(
                latitude, longitude, bearing, distance);
            double unwrappedLongitude = point.x();
            while (unwrappedLongitude - previousLongitude > 180.0) {
                unwrappedLongitude -= 360.0;
            }
            while (unwrappedLongitude - previousLongitude < -180.0) {
                unwrappedLongitude += 360.0;
            }
            previousLongitude = unwrappedLongitude;
            point.setX(unwrappedLongitude);
            perimeter.append(point);
            maxDistanceKm = qMax(maxDistanceKm, distance * kEarthRadiusKm);
        }
        if (perimeter.size() < 3) {
            continue;
        }

        QColor const color = tropoColor(
            maxDistanceKm, qMax<qint64>(0, now - timestamp));
        painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(),
                                  qMin(190, color.alpha() + 48)),
                            0.8));
        painter.setBrush(color);
        for (int wrap = -1; wrap <= 1; ++wrap) {
            QPainterPath path;
            QPointF first = perimeter.first();
            first.rx() += wrap * 360.0;
            path.moveTo(mapPoint(first, outputSize));
            for (qsizetype index = 1; index < perimeter.size(); ++index) {
                QPointF point = perimeter.at(index);
                point.rx() += wrap * 360.0;
                path.lineTo(mapPoint(point, outputSize));
            }
            path.closeSubpath();
            painter.drawPath(path);
        }
        ++rendered;
    }

    // DXView can additionally publish observed 6 m Sporadic-E cloud
    // perimeters.  They intentionally share the TROPO request and are drawn
    // in a distinct colour: the response currently has no cloud timestamp,
    // so replacing the complete image for every refresh avoids presenting a
    // stale observation as live data.
    QJsonArray const clouds =
        document.object().value(QStringLiteral("cloud_list")).toArray();
    for (QJsonValue const& value : clouds) {
        QJsonObject const cloud = value.toObject();
        QJsonArray const points = cloud.value(QStringLiteral("perim")).toArray();
        if (points.size() < 3 || points.size() > 2048) {
            continue;
        }

        QVector<QPointF> perimeter;
        perimeter.reserve(points.size());
        double previousLongitude = 0.0;
        bool havePreviousLongitude = false;
        for (QJsonValue const& pointValue : points) {
            QJsonObject const point = pointValue.toObject();
            QJsonValue const latitudeValue = point.value(QStringLiteral("lat"));
            QJsonValue const longitudeValue = point.value(QStringLiteral("lon"));
            if (!latitudeValue.isDouble() || !longitudeValue.isDouble()) {
                perimeter.clear();
                break;
            }
            double const latitude = qRadiansToDegrees(latitudeValue.toDouble());
            double longitude = qRadiansToDegrees(longitudeValue.toDouble());
            if (!std::isfinite(latitude) || !std::isfinite(longitude)
                || latitude < -90.0 || latitude > 90.0) {
                perimeter.clear();
                break;
            }
            if (havePreviousLongitude) {
                while (longitude - previousLongitude > 180.0) longitude -= 360.0;
                while (longitude - previousLongitude < -180.0) longitude += 360.0;
            }
            previousLongitude = longitude;
            havePreviousLongitude = true;
            perimeter.append(QPointF(longitude, latitude));
        }
        if (perimeter.size() < 3) {
            continue;
        }

        QColor const color(168, 92, 247, 142);
        painter.setPen(QPen(QColor(198, 144, 255, 205), 1.0));
        painter.setBrush(color);
        for (int wrap = -1; wrap <= 1; ++wrap) {
            QPainterPath path;
            QPointF first = perimeter.first();
            first.rx() += wrap * 360.0;
            path.moveTo(mapPoint(first, outputSize));
            for (qsizetype index = 1; index < perimeter.size(); ++index) {
                QPointF point = perimeter.at(index);
                point.rx() += wrap * 360.0;
                path.lineTo(mapPoint(point, outputSize));
            }
            path.closeSubpath();
            painter.drawPath(path);
        }
        ++rendered;
    }
    painter.end();

    if (featureCount) {
        *featureCount = rendered;
    }
    if (rendered == 0 && error) {
        *error = QStringLiteral("No valid tropo polygons");
    }
    return rendered > 0 ? result : QImage();
}

QImage MapExternalOverlayService::renderEarthquakePayload(
    const QByteArray& payload,
    int* featureCount,
    QString* error,
    const QSize& outputSize)
{
    if (featureCount) *featureCount = 0;
    if (error) error->clear();
    if (payload.isEmpty() || payload.size() > kMaxJsonPayloadBytes
        || outputSize.isEmpty()) {
        if (error) *error = QStringLiteral("Invalid earthquake payload");
        return {};
    }

    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString();
        return {};
    }

    QJsonArray const features =
        document.object().value(QStringLiteral("features")).toArray();
    QImage result(outputSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    int rendered = 0;

    for (QJsonValue const& featureValue : features) {
        QJsonObject const feature = featureValue.toObject();
        QJsonObject const geometry =
            feature.value(QStringLiteral("geometry")).toObject();
        if (geometry.value(QStringLiteral("type")).toString()
            != QStringLiteral("Point")) {
            continue;
        }
        QJsonArray const coordinates =
            geometry.value(QStringLiteral("coordinates")).toArray();
        if (coordinates.size() < 2) continue;
        double const longitude = coordinates.at(0).toDouble(
            std::numeric_limits<double>::quiet_NaN());
        double const latitude = coordinates.at(1).toDouble(
            std::numeric_limits<double>::quiet_NaN());
        if (!std::isfinite(longitude) || !std::isfinite(latitude)
            || longitude < -180.0 || longitude > 180.0
            || latitude < -90.0 || latitude > 90.0) {
            continue;
        }

        QJsonObject const properties =
            feature.value(QStringLiteral("properties")).toObject();
        double const magnitude = properties.value(QStringLiteral("mag"))
                                     .toDouble(0.0);
        double const radius = qBound(2.0, 2.0 + qMax(0.0, magnitude) * 1.25,
                                     11.0);
        QColor fill;
        if (magnitude >= 6.0) {
            fill = QColor(255, 59, 48, 220);
        } else if (magnitude >= 4.5) {
            fill = QColor(255, 149, 0, 210);
        } else {
            fill = QColor(255, 214, 10, 195);
        }
        QPointF const point =
            mapPoint(QPointF(longitude, latitude), outputSize);
        painter.setPen(QPen(QColor(255, 255, 255, 220), 0.8));
        painter.setBrush(fill);
        painter.drawEllipse(point, radius, radius);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(fill.red(), fill.green(), fill.blue(), 120),
                            1.0));
        painter.drawEllipse(point, radius + 2.5, radius + 2.5);
        ++rendered;
    }
    painter.end();

    if (featureCount) *featureCount = rendered;
    if (rendered == 0 && error) {
        *error = QStringLiteral("No valid earthquake points");
    }
    return rendered > 0 ? result : QImage();
}

QVariantList MapExternalOverlayService::parseEarthquakeFeatures(
    const QByteArray& payload, QString* error)
{
    if (error) error->clear();
    if (payload.isEmpty() || payload.size() > kMaxJsonPayloadBytes) {
        if (error) *error = QStringLiteral("Invalid earthquake payload");
        return {};
    }

    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString();
        return {};
    }

    QVariantList result;
    QJsonArray const features = document.object().value(QStringLiteral("features")).toArray();
    result.reserve(features.size());
    for (QJsonValue const& featureValue : features) {
        QJsonObject const feature = featureValue.toObject();
        QJsonObject const geometry = feature.value(QStringLiteral("geometry")).toObject();
        if (geometry.value(QStringLiteral("type")).toString() != QStringLiteral("Point")) {
            continue;
        }
        QJsonArray const coordinates = geometry.value(QStringLiteral("coordinates")).toArray();
        if (coordinates.size() < 2) {
            continue;
        }
        double const longitude = coordinates.at(0).toDouble(
            std::numeric_limits<double>::quiet_NaN());
        double const latitude = coordinates.at(1).toDouble(
            std::numeric_limits<double>::quiet_NaN());
        if (!std::isfinite(longitude) || !std::isfinite(latitude)
            || longitude < -180.0 || longitude > 180.0
            || latitude < -90.0 || latitude > 90.0) {
            continue;
        }

        QJsonObject const properties = feature.value(QStringLiteral("properties")).toObject();
        double const magnitude = properties.value(QStringLiteral("mag")).toDouble(-1.0);
        double const depthKm = coordinates.size() >= 3
            ? coordinates.at(2).toDouble(-1.0) : -1.0;
        qint64 const eventMs = static_cast<qint64>(properties.value(QStringLiteral("time")).toDouble(0.0));
        QString const place = properties.value(QStringLiteral("place")).toString().trimmed();
        QColor color = magnitude >= 6.0
            ? QColor(255, 89, 94)
            : (magnitude >= 4.5 ? QColor(255, 160, 54) : QColor(255, 219, 91));
        QString id = feature.value(QStringLiteral("id")).toString().trimmed();
        if (id.isEmpty()) {
            id = QStringLiteral("%1:%2:%3")
                .arg(longitude, 0, 'f', 4)
                .arg(latitude, 0, 'f', 4)
                .arg(eventMs);
        }
        QVariantMap item;
        item.insert(QStringLiteral("id"), id);
        item.insert(QStringLiteral("type"), QStringLiteral("earthquake"));
        item.insert(QStringLiteral("label"), magnitude >= 0.0
            ? QStringLiteral("M %1  %2").arg(QString::number(magnitude, 'f', 1), place)
            : place);
        item.insert(QStringLiteral("place"), place);
        item.insert(QStringLiteral("longitude"), longitude);
        item.insert(QStringLiteral("latitude"), latitude);
        item.insert(QStringLiteral("magnitude"), magnitude);
        item.insert(QStringLiteral("depthKm"), depthKm);
        item.insert(QStringLiteral("eventMs"), eventMs);
        item.insert(QStringLiteral("timeUtc"), eventMs > 0
            ? QDateTime::fromMSecsSinceEpoch(eventMs, QTimeZone::UTC).toString(Qt::ISODate)
            : QString());
        item.insert(QStringLiteral("status"), properties.value(QStringLiteral("status")).toString());
        item.insert(QStringLiteral("tsunami"), properties.value(QStringLiteral("tsunami")).toInt() != 0);
        item.insert(QStringLiteral("url"), properties.value(QStringLiteral("url")).toString());
        item.insert(QStringLiteral("color"), color.name());
        item.insert(QStringLiteral("hitRadius"), qBound(11.0, 10.0 + qMax(0.0, magnitude) * 1.2, 20.0));
        result.append(item);
    }

    if (result.isEmpty() && error) {
        *error = QStringLiteral("No valid earthquake points");
    }
    return result;
}

QImage MapExternalOverlayService::renderWildfirePayload(
    const QByteArray& payload,
    int* featureCount,
    QString* error,
    const QSize& outputSize)
{
    if (featureCount) *featureCount = 0;
    if (error) error->clear();
    if (payload.isEmpty() || payload.size() > kMaxJsonPayloadBytes
        || outputSize.isEmpty()) {
        if (error) *error = QStringLiteral("Invalid wildfire payload");
        return {};
    }

    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString();
        return {};
    }

    QJsonArray const events =
        document.object().value(QStringLiteral("events")).toArray();
    QImage result(outputSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    int rendered = 0;

    for (QJsonValue const& eventValue : events) {
        QJsonArray const geometries =
            eventValue.toObject().value(QStringLiteral("geometry")).toArray();
        QPointF lonLat;
        bool found = false;
        for (qsizetype index = geometries.size(); index > 0; --index) {
            QJsonObject const geometry = geometries.at(index - 1).toObject();
            if (geometry.value(QStringLiteral("type")).toString()
                != QStringLiteral("Point")) {
                continue;
            }
            QJsonArray const coordinates =
                geometry.value(QStringLiteral("coordinates")).toArray();
            if (coordinates.size() < 2) continue;
            double const longitude = coordinates.at(0).toDouble(
                std::numeric_limits<double>::quiet_NaN());
            double const latitude = coordinates.at(1).toDouble(
                std::numeric_limits<double>::quiet_NaN());
            if (!std::isfinite(longitude) || !std::isfinite(latitude)
                || longitude < -180.0 || longitude > 180.0
                || latitude < -90.0 || latitude > 90.0) {
                continue;
            }
            lonLat = QPointF(longitude, latitude);
            found = true;
            break;
        }
        if (!found) continue;

        QPointF const point = mapPoint(lonLat, outputSize);
        QColor const fireColor(255, 107, 0, 225);
        painter.setPen(QPen(QColor(255, 214, 102, 230), 1.0));
        painter.setBrush(fireColor);
        painter.drawEllipse(point, 3.5, 3.5);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 69, 0, 150), 1.2));
        painter.drawEllipse(point, 7.0, 7.0);
        painter.drawLine(point + QPointF(-5.0, 0.0),
                         point + QPointF(5.0, 0.0));
        painter.drawLine(point + QPointF(0.0, -5.0),
                         point + QPointF(0.0, 5.0));
        ++rendered;
    }
    painter.end();

    if (featureCount) *featureCount = rendered;
    if (rendered == 0 && error) {
        *error = QStringLiteral("No valid wildfire points");
    }
    return rendered > 0 ? result : QImage();
}
