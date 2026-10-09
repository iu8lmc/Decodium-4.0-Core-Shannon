#include "MapBaseMapService.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLinearGradient>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>
#include <QRunnable>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QtMath>

#include <cmath>

namespace {

constexpr int kNetworkTimeoutMs = 20000;
constexpr int kMaxPayloadBytes = 24 * 1024 * 1024;
constexpr int kOutputWidth = 2048;
constexpr int kOutputHeight = 1024;
constexpr int kCacheVersion = 2;

QString settingsOrganization()
{
    return QStringLiteral("Decodium");
}

QString settingsApplication()
{
    return QStringLiteral("Decodium3");
}

bool acceptedHttpReply(QNetworkReply* reply, QString* error)
{
    if (!reply) {
        if (error) *error = QStringLiteral("No network reply");
        return false;
    }
    int const status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        if (error) *error = reply->errorString();
        return false;
    }
    if (status > 0 && (status < 200 || status >= 300)) {
        if (error) *error = QStringLiteral("HTTP %1").arg(status);
        return false;
    }
    return true;
}

QString cacheToken(const QString& value)
{
    return QString::fromLatin1(
               QCryptographicHash::hash(value.toUtf8(),
                                         QCryptographicHash::Sha256)
                   .toHex())
        .left(16);
}

} // namespace

MapBaseMapService::MapBaseMapService(QObject* parent, const QString& cachePath)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_localAtlas(loadLocalAtlas())
    , m_currentImage(m_localAtlas)
    , m_cachePath(cachePath.trimmed().isEmpty()
          ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                .absoluteFilePath(QStringLiteral("map-base"))
          : QFileInfo(cachePath).absoluteFilePath())
{
    m_workerPool.setMaxThreadCount(1);
    m_workerPool.setExpiryTimeout(30000);
    QDir().mkpath(m_cachePath);
    m_offlinePackPath = QDir(m_cachePath).absoluteFilePath(
        QStringLiteral("offline-pack.png"));
    m_offlinePackStatus = QStringLiteral("No offline raster pack imported");

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       settingsOrganization(), settingsApplication());
    settings.beginGroup(QStringLiteral("LiveMapBase"));
    m_provider = normalizeProvider(settings.value(
        QStringLiteral("Provider"), QStringLiteral("Decodium Atlas")).toString());
    m_activeProvider = m_provider;
    m_style = normalizeStyle(settings.value(
        QStringLiteral("Style"), QStringLiteral("Day")).toString());
    m_offlineMode = settings.value(QStringLiteral("OfflineMode"), false).toBool();
    m_mapTilerApiKey = settings.value(QStringLiteral("MapTilerApiKey"))
                           .toString().trimmed();
    m_cacheMaxAgeDays = qBound(1,
        settings.value(QStringLiteral("CacheMaxAgeDays"), 7).toInt(), 90);
    m_fallbackProviders = settings.value(QStringLiteral("FallbackProviders"))
                              .toStringList();
    settings.endGroup();
    setFallbackProviders(m_fallbackProviders.isEmpty()
                             ? defaultFallbackProviders()
                             : m_fallbackProviders);
    m_status = m_offlineMode
        ? QStringLiteral("Offline: Decodium Atlas")
        : QStringLiteral("Decodium Atlas (local)");
    loadOfflinePackAsync();

    if (!m_offlineMode && m_provider != QStringLiteral("Decodium Atlas")) {
        QMetaObject::invokeMethod(this, &MapBaseMapService::refresh,
                                  Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(this, [this] {
            applyLocalAtlas(m_offlineMode
                                ? QStringLiteral("Offline: Decodium Atlas")
                                : QStringLiteral("Decodium Atlas (local)"),
                            m_generation);
        }, Qt::QueuedConnection);
    }
}

MapBaseMapService::~MapBaseMapService()
{
    ++m_generation;
    m_workerPool.clear();
}

QStringList MapBaseMapService::availableProviders() const
{
    return {
        QStringLiteral("Decodium Atlas"),
        QStringLiteral("OpenStreetMap"),
        QStringLiteral("OpenTopoMap"),
        QStringLiteral("GEBCO bathymetry"),
        QStringLiteral("NASA GIBS satellite"),
        QStringLiteral("MapTiler satellite"),
        QStringLiteral("Offline raster pack")
    };
}

QStringList MapBaseMapService::availableStyles() const
{
    return {QStringLiteral("Day"), QStringLiteral("Night")};
}

QStringList MapBaseMapService::defaultFallbackProviders()
{
    return {
        QStringLiteral("OpenStreetMap"),
        QStringLiteral("NASA GIBS satellite"),
        QStringLiteral("Decodium Atlas")
    };
}

QString MapBaseMapService::attribution() const
{
    QString const active = m_offlineMode
        ? (m_offlinePackAvailable ? QStringLiteral("Offline raster pack")
                                  : QStringLiteral("Decodium Atlas"))
        : m_activeProvider;
    if (active == QStringLiteral("Offline raster pack")) {
        return QStringLiteral("User-provided offline raster pack (verify source licence)");
    }
    if (active == QStringLiteral("OpenStreetMap")) {
        return QStringLiteral("© OpenStreetMap contributors");
    }
    if (active == QStringLiteral("OpenTopoMap")) {
        return QStringLiteral("© OpenTopoMap · © OpenStreetMap contributors");
    }
    if (active == QStringLiteral("GEBCO bathymetry")) {
        return QStringLiteral("GEBCO bathymetry");
    }
    if (active == QStringLiteral("NASA GIBS satellite")) {
        return QStringLiteral("NASA EOSDIS GIBS");
    }
    if (active == QStringLiteral("MapTiler satellite")) {
        return QStringLiteral("MapTiler · OpenStreetMap contributors");
    }
    return QStringLiteral("Decodium Atlas (local)");
}

QString MapBaseMapService::attributionUrl() const
{
    QString const active = m_offlineMode
        ? (m_offlinePackAvailable ? QStringLiteral("Offline raster pack")
                                  : QStringLiteral("Decodium Atlas"))
        : m_activeProvider;
    if (active == QStringLiteral("Offline raster pack")) {
        return {};
    }
    if (active == QStringLiteral("OpenStreetMap")) {
        return QStringLiteral("https://www.openstreetmap.org/copyright");
    }
    if (active == QStringLiteral("OpenTopoMap")) {
        return QStringLiteral("https://opentopomap.org/about");
    }
    if (active == QStringLiteral("GEBCO bathymetry")) {
        return QStringLiteral("https://www.gebco.net/data-products/gebco-web-services/web-map-service");
    }
    if (active == QStringLiteral("NASA GIBS satellite")) {
        return QStringLiteral("https://www.earthdata.nasa.gov/eosdis/science-system-description/eosdis-components/gibs");
    }
    if (active == QStringLiteral("MapTiler satellite")) {
        return QStringLiteral("https://www.maptiler.com/copyright/");
    }
    return {};
}

bool MapBaseMapService::apiKeyRequired() const
{
    return m_provider == QStringLiteral("MapTiler satellite")
        && m_mapTilerApiKey.isEmpty();
}

void MapBaseMapService::setProvider(const QString& provider)
{
    QString const next = normalizeProvider(provider);
    if (m_provider == next) {
        return;
    }

    ++m_generation;
    for (QNetworkReply* reply : findChildren<QNetworkReply*>()) {
        reply->abort();
    }
    m_tiles.clear();
    m_tileExpected = 0;
    m_tileCompleted = 0;
    m_tileFailures = 0;
    m_provider = next;
    m_fallbackIndex = 0;
    m_staleCache = false;
    setActiveProvider(m_provider);
    if (m_offlineMode || m_provider == QStringLiteral("Decodium Atlas")
        || m_provider == QStringLiteral("Offline raster pack")) {
        if ((m_offlineMode || m_provider == QStringLiteral("Offline raster pack"))
            && m_offlinePackAvailable) {
            applyOfflinePack(QStringLiteral("Offline: imported raster pack"), m_generation);
        } else {
            applyLocalAtlas(m_offlineMode
                                ? QStringLiteral("Offline: Decodium Atlas")
                                : QStringLiteral("Decodium Atlas (local)"),
                            m_generation);
        }
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       settingsOrganization(), settingsApplication());
    settings.beginGroup(QStringLiteral("LiveMapBase"));
    settings.setValue(QStringLiteral("Provider"), m_provider);
    settings.endGroup();
    settings.sync();
    emit providerChanged();
    emit fallbackActiveChanged();
    if (!m_offlineMode && m_provider != QStringLiteral("Decodium Atlas")
        && m_provider != QStringLiteral("Offline raster pack")) {
        refresh();
    }
}

void MapBaseMapService::setFallbackProviders(const QStringList& providers)
{
    QStringList normalized;
    for (QString const& provider : providers) {
        QString const value = normalizeProvider(provider);
        if (!normalized.contains(value)) {
            normalized.append(value);
        }
    }
    if (!normalized.contains(QStringLiteral("Decodium Atlas"))) {
        normalized.append(QStringLiteral("Decodium Atlas"));
    }
    if (normalized == m_fallbackProviders) {
        return;
    }
    m_fallbackProviders = normalized;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       settingsOrganization(), settingsApplication());
    settings.beginGroup(QStringLiteral("LiveMapBase"));
    settings.setValue(QStringLiteral("FallbackProviders"), m_fallbackProviders);
    settings.endGroup();
    settings.sync();
    emit fallbackProvidersChanged();
}

void MapBaseMapService::setStyle(const QString& style)
{
    QString const next = normalizeStyle(style);
    if (m_style == next) {
        return;
    }
    ++m_generation;
    for (QNetworkReply* reply : findChildren<QNetworkReply*>()) {
        reply->abort();
    }
    m_tiles.clear();
    m_style = next;
    m_staleCache = false;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       settingsOrganization(), settingsApplication());
    settings.beginGroup(QStringLiteral("LiveMapBase"));
    settings.setValue(QStringLiteral("Style"), m_style);
    settings.endGroup();
    settings.sync();
    emit styleChanged();
    if (m_offlineMode || m_provider == QStringLiteral("Decodium Atlas")
        || m_provider == QStringLiteral("Offline raster pack")) {
        if ((m_offlineMode || m_provider == QStringLiteral("Offline raster pack"))
            && m_offlinePackAvailable) {
            applyOfflinePack(QStringLiteral("Offline: imported raster pack"), m_generation);
        } else {
            applyLocalAtlas(m_offlineMode
                                ? QStringLiteral("Offline: Decodium Atlas")
                                : QStringLiteral("Decodium Atlas (local)"),
                            m_generation);
        }
    } else {
        refresh();
    }
}

void MapBaseMapService::setCacheMaxAgeDays(int days)
{
    int const bounded = qBound(1, days, 90);
    if (m_cacheMaxAgeDays == bounded) {
        return;
    }
    m_cacheMaxAgeDays = bounded;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       settingsOrganization(), settingsApplication());
    settings.beginGroup(QStringLiteral("LiveMapBase"));
    settings.setValue(QStringLiteral("CacheMaxAgeDays"), m_cacheMaxAgeDays);
    settings.endGroup();
    settings.sync();
    emit cacheStateChanged();
}

void MapBaseMapService::setOfflineMode(bool offline)
{
    if (m_offlineMode == offline) {
        return;
    }

    ++m_generation;
    m_offlineMode = offline;
    for (QNetworkReply* reply : findChildren<QNetworkReply*>()) {
        reply->abort();
    }
    m_tiles.clear();
    m_tileExpected = 0;
    m_tileCompleted = 0;
    m_tileFailures = 0;
    m_staleCache = false;
    if (m_offlineMode) {
        setActiveProvider(QStringLiteral("Decodium Atlas"));
        if (m_offlinePackAvailable) {
            applyOfflinePack(QStringLiteral("Offline: imported raster pack"), m_generation);
        } else {
            applyLocalAtlas(QStringLiteral("Offline: Decodium Atlas"), m_generation);
        }
    } else {
        setActiveProvider(m_provider);
        setStatus(QStringLiteral("Online base map ready"));
        refresh();
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       settingsOrganization(), settingsApplication());
    settings.beginGroup(QStringLiteral("LiveMapBase"));
    settings.setValue(QStringLiteral("OfflineMode"), m_offlineMode);
    settings.endGroup();
    settings.sync();
    emit offlineModeChanged();
    emit providerChanged();
    emit fallbackActiveChanged();
}

void MapBaseMapService::setMapTilerApiKey(const QString& apiKey)
{
    QString const key = apiKey.trimmed();
    if (m_mapTilerApiKey == key) {
        return;
    }
    m_mapTilerApiKey = key;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       settingsOrganization(), settingsApplication());
    settings.beginGroup(QStringLiteral("LiveMapBase"));
    settings.setValue(QStringLiteral("MapTilerApiKey"), m_mapTilerApiKey);
    settings.endGroup();
    settings.sync();
    emit mapTilerApiKeyChanged();
    emit providerChanged();
    if (!m_offlineMode && m_provider == QStringLiteral("MapTiler satellite")) {
        refresh();
    }
}

void MapBaseMapService::refresh()
{
    if (m_offlineMode) {
        if (m_offlinePackAvailable) {
            applyOfflinePack(QStringLiteral("Offline: imported raster pack"), m_generation);
        } else {
            applyLocalAtlas(QStringLiteral("Offline: Decodium Atlas"), m_generation);
        }
        return;
    }

    ++m_generation;
    int const generation = m_generation;
    for (QNetworkReply* reply : findChildren<QNetworkReply*>()) {
        reply->abort();
    }
    m_tiles.clear();
    m_tileExpected = 0;
    m_tileCompleted = 0;
    m_tileFailures = 0;
    m_fallbackIndex = 0;
    m_staleCache = false;
    setActiveProvider(m_provider);
    if (m_provider == QStringLiteral("Decodium Atlas")) {
        applyLocalAtlas(QStringLiteral("Decodium Atlas (local)"), generation);
        return;
    }
    if (m_provider == QStringLiteral("Offline raster pack")) {
        if (m_offlinePackAvailable) {
            applyOfflinePack(QStringLiteral("Offline: imported raster pack"), generation);
        } else {
            applyLocalAtlas(QStringLiteral("Offline raster pack not imported"), generation);
        }
        return;
    }
    setLoading(true);
    loadCachedOnlineImage(generation);
    requestCandidate(0, generation);
}

void MapBaseMapService::invalidateCache()
{
    ++m_generation;
    for (QNetworkReply* reply : findChildren<QNetworkReply*>()) {
        reply->abort();
    }
    QString const provider = m_provider;
    QString const imagePath = cacheFilePath(provider);
    QString const metadataPath = cacheMetadataPath(provider);
    QPointer<MapBaseMapService> guard(this);
    m_workerPool.start(QRunnable::create(
        [guard, imagePath, metadataPath] {
            QFile::remove(imagePath);
            QFile::remove(metadataPath);
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(), [guard] {
                if (!guard) return;
                guard->m_staleCache = false;
                guard->cacheStateChanged();
                guard->setStatus(QStringLiteral("Base map cache invalidated"));
                if (!guard->m_offlineMode) guard->refresh();
            }, Qt::QueuedConnection);
        }));
}

QString MapBaseMapService::normalizeProvider(const QString& provider)
{
    QString const value = provider.trimmed();
    const QStringList known = {
        QStringLiteral("Decodium Atlas"), QStringLiteral("OpenStreetMap"),
        QStringLiteral("OpenTopoMap"), QStringLiteral("GEBCO bathymetry"),
        QStringLiteral("NASA GIBS satellite"), QStringLiteral("MapTiler satellite"),
        QStringLiteral("Offline raster pack")
    };
    for (QString const& candidate : known) {
        if (candidate.compare(value, Qt::CaseInsensitive) == 0) {
            return candidate;
        }
    }
    return QStringLiteral("Decodium Atlas");
}

QString MapBaseMapService::normalizeStyle(const QString& style)
{
    return style.compare(QStringLiteral("Night"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("Night") : QStringLiteral("Day");
}

QStringList MapBaseMapService::candidateProviders() const
{
    QStringList result;
    auto appendUnique = [&result](const QString& value) {
        if (!result.contains(value)) result.append(value);
    };
    appendUnique(m_provider);
    for (QString const& value : m_fallbackProviders) {
        appendUnique(normalizeProvider(value));
    }
    appendUnique(QStringLiteral("Decodium Atlas"));
    return result;
}

void MapBaseMapService::setActiveProvider(const QString& provider)
{
    QString const normalized = normalizeProvider(provider);
    if (m_activeProvider == normalized) {
        return;
    }
    m_activeProvider = normalized;
    emit activeProviderChanged();
    emit fallbackActiveChanged();
}

void MapBaseMapService::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    emit loadingChanged();
}

void MapBaseMapService::setStatus(const QString& status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

void MapBaseMapService::applyLocalAtlas(const QString& status, int generation)
{
    if (generation != m_generation) return;
    setActiveProvider(QStringLiteral("Decodium Atlas"));
    applyImageAsync(m_localAtlas, QStringLiteral("Decodium Atlas"),
                    status, generation, false);
}

void MapBaseMapService::applyOfflinePack(const QString& status, int generation)
{
    if (generation != m_generation || !m_offlinePackAvailable
        || m_offlinePackImage.isNull()) {
        return;
    }
    setActiveProvider(QStringLiteral("Offline raster pack"));
    applyImageAsync(m_offlinePackImage, QStringLiteral("Offline raster pack"),
                    status, generation, false);
}

QImage MapBaseMapService::loadLocalAtlas()
{
    QString const appDir = QCoreApplication::applicationDirPath();
    QString const cwd = QDir::currentPath();
    QStringList const candidates {
        QStringLiteral(":/earth_2048x1024.jpg"),
        QStringLiteral(":/artwork/maps/earth_2048x1024.jpg"),
        QDir(appDir).absoluteFilePath(QStringLiteral("artwork/maps/earth_2048x1024.jpg")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/earth_2048x1024.jpg")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/wsjtx/maps/earth_2048x1024.jpg")),
        QDir(cwd).absoluteFilePath(QStringLiteral("artwork/maps/earth_2048x1024.jpg"))
    };
    for (QString const& path : candidates) {
        QImage const image(path);
        if (!image.isNull()) {
            return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }
    }

    QImage image(kOutputWidth, kOutputHeight, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(5, 24, 42));
    QPainter painter(&image);
    QLinearGradient gradient(QPointF(0, 0), QPointF(0, image.height()));
    gradient.setColorAt(0.0, QColor(9, 55, 91));
    gradient.setColorAt(1.0, QColor(3, 19, 34));
    painter.fillRect(image.rect(), gradient);
    painter.end();
    return image;
}

QImage MapBaseMapService::readOfflinePack(const QString& path, QString* error)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        if (error) {
            *error = reader.errorString().isEmpty()
                ? QStringLiteral("unsupported or unreadable raster")
                : reader.errorString();
        }
        return {};
    }
    constexpr int maxWidth = 8192;
    constexpr int maxHeight = 4096;
    if (image.width() > maxWidth || image.height() > maxHeight) {
        image = image.scaled(QSize(maxWidth, maxHeight), Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    }
    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

void MapBaseMapService::loadOfflinePackAsync()
{
    const quint64 generation = ++m_offlinePackGeneration;
    const QString path = m_offlinePackPath;
    QPointer<MapBaseMapService> guard(this);
    m_workerPool.start(QRunnable::create([guard, path, generation] {
        QString error;
        QImage image;
        if (QFile::exists(path)) {
            image = MapBaseMapService::readOfflinePack(path, &error);
        }
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, generation, image, error] {
            if (!guard || generation != guard->m_offlinePackGeneration) return;
            if (image.isNull()) {
                guard->m_offlinePackAvailable = false;
                guard->m_offlinePackStatus = error.isEmpty()
                    ? QStringLiteral("No offline raster pack imported")
                    : QStringLiteral("Offline raster pack unavailable: %1").arg(error);
                emit guard->offlinePackChanged();
                return;
            }
            guard->m_offlinePackImage = image;
            guard->m_offlinePackAvailable = true;
            guard->m_offlinePackStatus = QStringLiteral(
                "Offline raster pack ready (%1 x %2); verify source licence")
                .arg(image.width()).arg(image.height());
            emit guard->offlinePackChanged();
            if (guard->m_offlineMode
                || guard->m_provider == QStringLiteral("Offline raster pack")) {
                guard->applyOfflinePack(QStringLiteral("Offline: imported raster pack"),
                                        guard->m_generation);
            }
        }, Qt::QueuedConnection);
    }));
}

void MapBaseMapService::importOfflinePack(const QString& path)
{
    QString source = path.trimmed();
    if (source.startsWith(QStringLiteral("file://"))) {
        source = QUrl(source).toLocalFile();
    }
    if (source.isEmpty()) {
        m_offlinePackStatus = QStringLiteral("Offline raster import cancelled");
        emit offlinePackChanged();
        return;
    }
    const quint64 generation = ++m_offlinePackGeneration;
    const QString destination = m_offlinePackPath;
    const QString metadataPath = destination + QStringLiteral(".json");
    const QString sourceName = QFileInfo(source).fileName();
    QPointer<MapBaseMapService> guard(this);
    m_offlinePackStatus = QStringLiteral("Importing offline raster pack...");
    emit offlinePackChanged();
    m_workerPool.start(QRunnable::create(
        [guard, source, sourceName, destination, metadataPath, generation] {
            QString error;
            QImage image = MapBaseMapService::readOfflinePack(source, &error);
            bool saved = false;
            if (!image.isNull()) {
                QSaveFile file(destination);
                if (file.open(QIODevice::WriteOnly)
                    && image.save(&file, "PNG") && file.commit()) {
                    QJsonObject metadata;
                    metadata.insert(QStringLiteral("version"), 1);
                    metadata.insert(QStringLiteral("format"),
                                    QStringLiteral("equirectangular-raster"));
                    metadata.insert(QStringLiteral("sourceFile"), sourceName);
                    metadata.insert(QStringLiteral("importedMs"),
                                    QDateTime::currentMSecsSinceEpoch());
                    QSaveFile metadataFile(metadataPath);
                    if (metadataFile.open(QIODevice::WriteOnly)) {
                        const QByteArray data = QJsonDocument(metadata).toJson(
                            QJsonDocument::Compact);
                        saved = metadataFile.write(data) == data.size()
                            && metadataFile.commit();
                    }
                }
            }
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(),
                [guard, generation, image, error, saved] {
                    if (!guard || generation != guard->m_offlinePackGeneration) return;
                    if (image.isNull() || !saved) {
                        guard->m_offlinePackStatus = QStringLiteral(
                            "Offline raster import failed: %1")
                            .arg(error.isEmpty() ? QStringLiteral("cannot write local pack")
                                                 : error);
                        emit guard->offlinePackChanged();
                        return;
                    }
                    guard->m_offlinePackImage = image;
                    guard->m_offlinePackAvailable = true;
                    guard->m_offlinePackStatus = QStringLiteral(
                        "Offline raster pack ready (%1 x %2); verify source licence")
                        .arg(image.width()).arg(image.height());
                    emit guard->offlinePackChanged();
                    if (guard->m_offlineMode
                        || guard->m_provider == QStringLiteral("Offline raster pack")) {
                        guard->applyOfflinePack(
                            QStringLiteral("Offline: imported raster pack"),
                            guard->m_generation);
                    }
                }, Qt::QueuedConnection);
        }));
}

void MapBaseMapService::clearOfflinePack()
{
    const quint64 generation = ++m_offlinePackGeneration;
    const QString path = m_offlinePackPath;
    const QString metadataPath = path + QStringLiteral(".json");
    QPointer<MapBaseMapService> guard(this);
    m_workerPool.start(QRunnable::create([guard, path, metadataPath, generation] {
        QFile::remove(path);
        QFile::remove(metadataPath);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, generation] {
            if (!guard || generation != guard->m_offlinePackGeneration) return;
            guard->m_offlinePackImage = {};
            guard->m_offlinePackAvailable = false;
            guard->m_offlinePackStatus = QStringLiteral("No offline raster pack imported");
            emit guard->offlinePackChanged();
            if (guard->m_offlineMode
                || guard->m_provider == QStringLiteral("Offline raster pack")) {
                guard->applyLocalAtlas(QStringLiteral("Offline: Decodium Atlas"),
                                       guard->m_generation);
            }
        }, Qt::QueuedConnection);
    }));
}

QImage MapBaseMapService::webMercatorToEquirectangular(const QImage& source,
                                                        const QSize& outputSize)
{
    if (source.isNull() || outputSize.isEmpty()) {
        return {};
    }
    QImage result(outputSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QImage const input = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    constexpr double latitudeLimit = 85.0511287798066;
    double const pi = std::acos(-1.0);
    for (int y = 0; y < result.height(); ++y) {
        double const latitude = 90.0 - (180.0 * (y + 0.5) / result.height());
        double const clampedLatitude = qBound(-latitudeLimit, latitude, latitudeLimit);
        double const radians = qDegreesToRadians(clampedLatitude);
        double const mercatorY = (1.0 - std::asinh(std::tan(radians)) / pi) * 0.5;
        int const sourceY = qBound(0, qFloor(mercatorY * input.height()), input.height() - 1);
        QRgb* destination = reinterpret_cast<QRgb*>(result.scanLine(y));
        QRgb const* inputRow = reinterpret_cast<QRgb const*>(input.constScanLine(sourceY));
        for (int x = 0; x < result.width(); ++x) {
            int const sourceX = qBound(0, qFloor((x + 0.5) * input.width() / result.width()), input.width() - 1);
            destination[x] = inputRow[sourceX];
        }
    }
    return result;
}

QImage MapBaseMapService::applyStyle(const QImage& source, const QString& style)
{
    if (source.isNull() || style != QStringLiteral("Night")) {
        return source;
    }
    QImage result = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < result.height(); ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            QRgb const pixel = row[x];
            int const red = qRound(qRed(pixel) * 0.25);
            int const green = qRound(qGreen(pixel) * 0.34);
            int const blue = qBound(0, qRound(qBlue(pixel) * 0.62 + 12), 255);
            row[x] = qRgba(red, green, blue, qAlpha(pixel));
        }
    }
    return result;
}

QString MapBaseMapService::tileUrl(const QString& provider, int zoom, int x, int y,
                                   const QString& apiKey)
{
    if (provider == QStringLiteral("OpenStreetMap")) {
        return QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png")
            .arg(zoom).arg(x).arg(y);
    }
    if (provider == QStringLiteral("OpenTopoMap")) {
        return QStringLiteral("https://tile.opentopomap.org/%1/%2/%3.png")
            .arg(zoom).arg(x).arg(y);
    }
    if (provider == QStringLiteral("MapTiler satellite")) {
        QUrl url(QStringLiteral("https://api.maptiler.com/maps/satellite/%1/%2/%3.jpg")
                     .arg(zoom).arg(x).arg(y));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("key"), apiKey);
        url.setQuery(query);
        return url.toString();
    }
    return {};
}

QString MapBaseMapService::providerCacheSignature(const QString& provider) const
{
    QString signature = provider + QLatin1Char('|') + m_style;
    if (provider == QStringLiteral("MapTiler satellite")) {
        signature += QLatin1Char('|') + m_mapTilerApiKey;
    }
    return signature;
}

QString MapBaseMapService::cacheFilePath(const QString& provider) const
{
    QString const token = cacheToken(providerCacheSignature(provider));
    QString key = provider;
    key.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9]+")), QStringLiteral("_"));
    return QDir(m_cachePath).absoluteFilePath(
        key + QLatin1Char('_') + m_style.toLower() + QLatin1Char('_')
        + token + QStringLiteral(".png"));
}

QString MapBaseMapService::cacheMetadataPath(const QString& provider) const
{
    QString path = cacheFilePath(provider);
    path += QStringLiteral(".json");
    return path;
}

void MapBaseMapService::applyImageAsync(const QImage& source,
                                        const QString& provider,
                                        const QString& status,
                                        int generation,
                                        bool cacheImage)
{
    if (source.isNull()) return;
    QString const style = m_style;
    QString const imagePath = cacheFilePath(provider);
    QString const metadataPath = cacheMetadataPath(provider);
    QString const signature = providerCacheSignature(provider);
    QPointer<MapBaseMapService> guard(this);
    m_workerPool.start(QRunnable::create(
        [guard, source, provider, status, generation, cacheImage, style,
         imagePath, metadataPath, signature] {
            QImage const image = MapBaseMapService::applyStyle(source, style);
            if (cacheImage && !image.isNull()) {
                QSaveFile file(imagePath);
                if (file.open(QIODevice::WriteOnly)
                    && image.save(&file, "PNG")
                    && file.commit()) {
                    QJsonObject metadata;
                    metadata.insert(QStringLiteral("version"), kCacheVersion);
                    metadata.insert(QStringLiteral("provider"), provider);
                    metadata.insert(QStringLiteral("style"), style);
                    metadata.insert(QStringLiteral("signature"), signature);
                    metadata.insert(QStringLiteral("createdMs"),
                                    QDateTime::currentMSecsSinceEpoch());
                    QSaveFile metadataFile(metadataPath);
                    if (metadataFile.open(QIODevice::WriteOnly)) {
                        QByteArray const data = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
                        if (metadataFile.write(data) == data.size()) {
                            metadataFile.commit();
                        }
                    }
                }
            }
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(),
                [guard, image, provider, status, generation] {
                    if (guard) {
                        guard->applyProcessedImage(image, provider, status,
                                                   generation, false);
                    }
                }, Qt::QueuedConnection);
        }));
}

void MapBaseMapService::processImagePayloadAsync(const QByteArray& payload,
                                                 const QString& provider,
                                                 const QString& status,
                                                 int generation,
                                                 int fallbackIndex,
                                                 bool webMercator,
                                                 bool cacheImage)
{
    QString const style = m_style;
    QString const imagePath = cacheFilePath(provider);
    QString const metadataPath = cacheMetadataPath(provider);
    QString const signature = providerCacheSignature(provider);
    QPointer<MapBaseMapService> guard(this);
    m_workerPool.start(QRunnable::create(
        [guard, payload, provider, status, generation, fallbackIndex,
         webMercator, cacheImage, style, imagePath, metadataPath, signature] {
            QImage image;
            if (payload.size() <= kMaxPayloadBytes) {
                image.loadFromData(payload);
            }
            if (webMercator && !image.isNull()) {
                image = MapBaseMapService::webMercatorToEquirectangular(
                    image, QSize(kOutputWidth, kOutputHeight));
            } else if (!image.isNull()) {
                image = image.scaled(kOutputWidth, kOutputHeight,
                                     Qt::IgnoreAspectRatio,
                                     Qt::SmoothTransformation);
            }
            image = MapBaseMapService::applyStyle(image, style);
            if (cacheImage && !image.isNull()) {
                QSaveFile file(imagePath);
                if (file.open(QIODevice::WriteOnly)
                    && image.save(&file, "PNG")
                    && file.commit()) {
                    QJsonObject metadata;
                    metadata.insert(QStringLiteral("version"), kCacheVersion);
                    metadata.insert(QStringLiteral("provider"), provider);
                    metadata.insert(QStringLiteral("style"), style);
                    metadata.insert(QStringLiteral("signature"), signature);
                    metadata.insert(QStringLiteral("createdMs"),
                                    QDateTime::currentMSecsSinceEpoch());
                    QSaveFile metadataFile(metadataPath);
                    if (metadataFile.open(QIODevice::WriteOnly)) {
                        QByteArray const data = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
                        if (metadataFile.write(data) == data.size()) {
                            metadataFile.commit();
                        }
                    }
                }
            }
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(),
                [guard, image, provider, status, generation, fallbackIndex] {
                    if (!guard) return;
                    if (image.isNull()) {
                        guard->providerFailed(provider,
                            QStringLiteral("Invalid image returned by provider"),
                            fallbackIndex, generation);
                    } else {
                        guard->applyProcessedImage(image, provider, status,
                                                   generation, false);
                    }
                }, Qt::QueuedConnection);
        }));
}

void MapBaseMapService::applyProcessedImage(const QImage& image,
                                            const QString& provider,
                                            const QString& status,
                                            int generation,
                                            bool staleCache,
                                            bool fromCache)
{
    if (generation != m_generation || image.isNull()
        || (m_offlineMode
            && provider != QStringLiteral("Decodium Atlas")
            && provider != QStringLiteral("Offline raster pack"))) {
        return;
    }
    m_currentImage = image;
    m_staleCache = staleCache;
    setActiveProvider(provider);
    emit baseMapImageChanged();
    emit cacheStateChanged();
    if (!fromCache) {
        setLoading(false);
    }
    setStatus(status);
}

void MapBaseMapService::loadCachedOnlineImage(int generation)
{
    QString const provider = m_provider;
    QString const imagePath = cacheFilePath(provider);
    QString const metadataPath = cacheMetadataPath(provider);
    QString const signature = providerCacheSignature(provider);
    int const maxAgeDays = m_cacheMaxAgeDays;
    QPointer<MapBaseMapService> guard(this);
    m_workerPool.start(QRunnable::create(
        [guard, provider, imagePath, metadataPath, signature, generation,
         maxAgeDays] {
            QFile metadataFile(metadataPath);
            QImage image(imagePath);
            bool valid = !image.isNull() && metadataFile.open(QIODevice::ReadOnly);
            qint64 createdMs = 0;
            if (valid) {
                QJsonParseError parseError;
                QJsonDocument const document =
                    QJsonDocument::fromJson(metadataFile.readAll(), &parseError);
                QJsonObject const object = document.object();
                valid = parseError.error == QJsonParseError::NoError
                    && object.value(QStringLiteral("version")).toInt() == kCacheVersion
                    && object.value(QStringLiteral("provider")).toString() == provider
                    && object.value(QStringLiteral("signature")).toString() == signature;
                createdMs = object.value(QStringLiteral("createdMs")).toVariant().toLongLong();
            }
            if (!valid) {
                QFile::remove(imagePath);
                QFile::remove(metadataPath);
                image = QImage();
            }
            qint64 const ageMs = createdMs > 0
                ? qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - createdMs)
                : 0;
            bool const stale = valid
                && ageMs > static_cast<qint64>(maxAgeDays) * 86400000LL;
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(),
                [guard, image, provider, generation, stale] {
                    if (!guard || image.isNull() || !guard->m_loading
                        || guard->m_activeProvider != provider) return;
                    guard->applyProcessedImage(
                        image, provider,
                        stale ? QStringLiteral("Stale cached %1; refreshing")
                                    .arg(provider)
                              : QStringLiteral("Cached %1").arg(provider),
                        generation, stale, true);
                }, Qt::QueuedConnection);
        }));
}

void MapBaseMapService::requestCandidate(int fallbackIndex, int generation)
{
    if (generation != m_generation || m_offlineMode) return;
    QStringList const candidates = candidateProviders();
    if (fallbackIndex < 0 || fallbackIndex >= candidates.size()) {
        applyLocalAtlas(QStringLiteral("All online providers unavailable; local atlas"),
                        generation);
        return;
    }
    m_fallbackIndex = fallbackIndex;
    QString const provider = candidates.at(fallbackIndex);
    setActiveProvider(provider);
    setStatus(fallbackIndex == 0
                  ? QStringLiteral("Loading %1").arg(provider)
                  : QStringLiteral("%1 unavailable; trying %2")
                        .arg(candidates.at(fallbackIndex - 1), provider));
    if (provider == QStringLiteral("Decodium Atlas")) {
        applyLocalAtlas(fallbackIndex == 0
                            ? QStringLiteral("Decodium Atlas (local)")
                            : QStringLiteral("Fallback: Decodium Atlas (local)"),
                        generation);
        return;
    }
    if (provider == QStringLiteral("MapTiler satellite")
        && m_mapTilerApiKey.isEmpty()) {
        providerFailed(provider, QStringLiteral("MapTiler requires your API key"),
                       fallbackIndex, generation);
        return;
    }
    if (provider == QStringLiteral("NASA GIBS satellite")) {
        requestNasaGibs(provider, fallbackIndex, generation);
    } else if (provider == QStringLiteral("GEBCO bathymetry")) {
        requestGebcoBathymetry(provider, fallbackIndex, generation);
    } else {
        requestXyzTiles(provider, fallbackIndex, generation);
    }
}

void MapBaseMapService::providerFailed(const QString& provider,
                                       const QString& error,
                                       int fallbackIndex,
                                       int generation)
{
    if (generation != m_generation || m_offlineMode) return;
    QStringList const candidates = candidateProviders();
    if (fallbackIndex + 1 < candidates.size()) {
        setStatus(QStringLiteral("%1: %2; trying fallback")
                      .arg(provider, error));
        QTimer::singleShot(0, this, [this, fallbackIndex, generation] {
            requestCandidate(fallbackIndex + 1, generation);
        });
        return;
    }
    setLoading(false);
    applyLocalAtlas(QStringLiteral("%1; local atlas available").arg(error), generation);
}

void MapBaseMapService::requestNasaGibs(const QString& provider,
                                        int fallbackIndex,
                                        int generation)
{
    QUrl url(QStringLiteral("https://gibs.earthdata.nasa.gov/wms/epsg4326/best/wms.cgi"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("SERVICE"), QStringLiteral("WMS"));
    query.addQueryItem(QStringLiteral("VERSION"), QStringLiteral("1.1.1"));
    query.addQueryItem(QStringLiteral("REQUEST"), QStringLiteral("GetMap"));
    query.addQueryItem(QStringLiteral("LAYERS"), QStringLiteral("MODIS_Terra_CorrectedReflectance_TrueColor"));
    query.addQueryItem(QStringLiteral("STYLES"), QString());
    query.addQueryItem(QStringLiteral("SRS"), QStringLiteral("EPSG:4326"));
    query.addQueryItem(QStringLiteral("BBOX"), QStringLiteral("-180,-90,180,90"));
    query.addQueryItem(QStringLiteral("WIDTH"), QString::number(kOutputWidth));
    query.addQueryItem(QStringLiteral("HEIGHT"), QString::number(kOutputHeight));
    query.addQueryItem(QStringLiteral("FORMAT"), QStringLiteral("image/jpeg"));
    query.addQueryItem(QStringLiteral("TRANSPARENT"), QStringLiteral("FALSE"));
    query.addQueryItem(QStringLiteral("TIME"), QDate::currentDate().addDays(-1).toString(Qt::ISODate));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium/4 MapBase"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kNetworkTimeoutMs);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, provider, fallbackIndex, generation] {
        QString error;
        bool const ok = acceptedHttpReply(reply, &error);
        QByteArray const payload = ok ? reply->readAll() : QByteArray();
        reply->deleteLater();
        if (generation != m_generation || m_offlineMode
            || m_activeProvider != provider) return;
        if (!ok || payload.isEmpty() || payload.size() > kMaxPayloadBytes) {
            providerFailed(provider, error.isEmpty()
                               ? QStringLiteral("empty or oversized response")
                               : error,
                           fallbackIndex, generation);
            return;
        }
        processImagePayloadAsync(payload, provider,
            QStringLiteral("NASA GIBS satellite updated %1 UTC")
                .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("HH:mm"))),
            generation, fallbackIndex, false, true);
    });
}

void MapBaseMapService::requestGebcoBathymetry(const QString& provider,
                                               int fallbackIndex,
                                               int generation)
{
    QUrl url(QStringLiteral("https://wms.gebco.net/mapserv"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("SERVICE"), QStringLiteral("WMS"));
    query.addQueryItem(QStringLiteral("VERSION"), QStringLiteral("1.1.1"));
    query.addQueryItem(QStringLiteral("REQUEST"), QStringLiteral("GetMap"));
    query.addQueryItem(QStringLiteral("LAYERS"), QStringLiteral("GEBCO_LATEST"));
    query.addQueryItem(QStringLiteral("STYLES"), QString());
    query.addQueryItem(QStringLiteral("SRS"), QStringLiteral("EPSG:4326"));
    query.addQueryItem(QStringLiteral("BBOX"), QStringLiteral("-180,-90,180,90"));
    query.addQueryItem(QStringLiteral("WIDTH"), QString::number(kOutputWidth));
    query.addQueryItem(QStringLiteral("HEIGHT"), QString::number(kOutputHeight));
    query.addQueryItem(QStringLiteral("FORMAT"), QStringLiteral("image/png"));
    query.addQueryItem(QStringLiteral("TRANSPARENT"), QStringLiteral("FALSE"));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium/4 MapBase"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kNetworkTimeoutMs);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, provider, fallbackIndex, generation] {
        QString error;
        bool const ok = acceptedHttpReply(reply, &error);
        QByteArray const payload = ok ? reply->readAll() : QByteArray();
        reply->deleteLater();
        if (generation != m_generation || m_offlineMode
            || m_activeProvider != provider) return;
        if (!ok || payload.isEmpty() || payload.size() > kMaxPayloadBytes) {
            providerFailed(provider, error.isEmpty()
                               ? QStringLiteral("empty or oversized response")
                               : error,
                           fallbackIndex, generation);
            return;
        }
        processImagePayloadAsync(payload, provider,
            QStringLiteral("GEBCO bathymetry updated"),
            generation, fallbackIndex, false, true);
    });
}

void MapBaseMapService::requestXyzTiles(const QString& provider,
                                        int fallbackIndex,
                                        int generation)
{
    int const count = 1 << m_tileZoom;
    m_tiles.clear();
    m_tileExpected = count * count;
    m_tileCompleted = 0;
    m_tileFailures = 0;
    setLoading(true);
    for (int y = 0; y < count; ++y) {
        for (int x = 0; x < count; ++x) {
            QUrl const url(tileUrl(provider, m_tileZoom, x, y,
                                   m_mapTilerApiKey));
            QNetworkRequest request(url);
            request.setHeader(QNetworkRequest::UserAgentHeader,
                              QStringLiteral("Decodium/4 MapBase"));
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);
            request.setTransferTimeout(kNetworkTimeoutMs);
            QNetworkReply* reply = m_network->get(request);
            connect(reply, &QNetworkReply::finished, this,
                    [this, reply, provider, fallbackIndex, generation, x, y] {
                bool const ok = acceptedHttpReply(reply, nullptr);
                QByteArray const payload = ok ? reply->readAll() : QByteArray();
                reply->deleteLater();
                if (generation != m_generation || m_offlineMode
                    || m_activeProvider != provider) return;
                QImage image;
                if (!payload.isEmpty() && payload.size() <= kMaxPayloadBytes) {
                    image.loadFromData(payload);
                }
                if (image.isNull()) {
                    ++m_tileFailures;
                } else {
                    m_tiles.insert(QStringLiteral("%1/%2").arg(x).arg(y), image);
                }
                ++m_tileCompleted;
                if (m_tileCompleted == m_tileExpected) {
                    finishXyzRequest(provider, fallbackIndex, generation);
                }
            });
        }
    }
}

void MapBaseMapService::finishXyzRequest(const QString& provider,
                                         int fallbackIndex,
                                         int generation)
{
    if (generation != m_generation || m_offlineMode
        || m_activeProvider != provider) return;
    int const count = 1 << m_tileZoom;
    if (m_tiles.isEmpty()) {
        providerFailed(provider, QStringLiteral("no valid tiles returned"),
                       fallbackIndex, generation);
        return;
    }
    QSize tileSize;
    for (QImage const& image : std::as_const(m_tiles)) {
        if (!image.isNull()) {
            tileSize = image.size();
            break;
        }
    }
    if (tileSize.isEmpty()) {
        providerFailed(provider, QStringLiteral("provider returned invalid tiles"),
                       fallbackIndex, generation);
        return;
    }
    QHash<QString, QImage> const tiles = m_tiles;
    int const failures = m_tileFailures;
    QString const status = QStringLiteral("%1 updated%2").arg(provider)
        .arg(failures > 0
                 ? QStringLiteral(" (%1 tiles unavailable)").arg(failures)
                 : QString());
    QPointer<MapBaseMapService> guard(this);
    m_workerPool.start(QRunnable::create(
        [guard, tiles, tileSize, count, provider, status, generation,
         fallbackIndex] {
            QImage mercator(tileSize.width() * count, tileSize.height() * count,
                            QImage::Format_ARGB32_Premultiplied);
            mercator.fill(QColor(4, 22, 38));
            QPainter painter(&mercator);
            for (int y = 0; y < count; ++y) {
                for (int x = 0; x < count; ++x) {
                    QImage const tile = tiles.value(
                        QStringLiteral("%1/%2").arg(x).arg(y));
                    if (!tile.isNull()) {
                        painter.drawImage(
                            QRect(x * tileSize.width(), y * tileSize.height(),
                                  tileSize.width(), tileSize.height()), tile);
                    }
                }
            }
            painter.end();
            QImage const image = MapBaseMapService::webMercatorToEquirectangular(
                mercator, QSize(kOutputWidth, kOutputHeight));
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(),
                [guard, image, provider, status, generation, fallbackIndex] {
                    if (!guard) return;
                    if (image.isNull()) {
                        guard->providerFailed(provider,
                            QStringLiteral("tile projection failed"),
                            fallbackIndex, generation);
                        return;
                    }
                    guard->applyImageAsync(image, provider, status,
                                           generation, true);
                }, Qt::QueuedConnection);
        }));
}
