#include "DecodiumBridge.h"
#include "DxccLookup.h"
#include "Radio.hpp"
#include "Sequencer/MessageTokenRules.hpp"
#include <QElapsedTimer>
#include <QFile>
#include <QMetaObject>
#include <QPointer>
#include <QRunnable>
#include <QSet>
#include <QTextStream>
#include <QThreadPool>
#include <cmath>
#include <utility>

using decodium::seq::isGridTokenStrict;
QString decodiumBridgeWorldMapCallKey(QString);
QString decodiumBridgeNormalizeWorldMapCall(QString);
void decodiumBridgeAudioLevelLog(const QString&);

static QString maidenheadFromLonLat(double lonEast, double lat)
{
    if (!std::isfinite(lonEast) || !std::isfinite(lat)) {
        return {};
    }

    while (lonEast < -180.0) lonEast += 360.0;
    while (lonEast >= 180.0) lonEast -= 360.0;
    lat = qBound(-89.999999, lat, 89.999999);

    double x = lonEast + 180.0;
    double y = lat + 90.0;
    int const fieldLon = qBound(0, static_cast<int>(std::floor(x / 20.0)), 17);
    int const fieldLat = qBound(0, static_cast<int>(std::floor(y / 10.0)), 17);
    x -= fieldLon * 20.0;
    y -= fieldLat * 10.0;
    int const squareLon = qBound(0, static_cast<int>(std::floor(x / 2.0)), 9);
    int const squareLat = qBound(0, static_cast<int>(std::floor(y)), 9);

    QString grid;
    grid.reserve(4);
    grid.append(QChar(QLatin1Char('A' + fieldLon)));
    grid.append(QChar(QLatin1Char('A' + fieldLat)));
    grid.append(QChar(QLatin1Char('0' + squareLon)));
    grid.append(QChar(QLatin1Char('0' + squareLat)));
    return grid;
}

struct WorldMapCall3LoadResult {
    QHash<QString, QString> grids;
    QString path;
    int loaded {0};
    qint64 elapsedMs {0};
};

QStringList worldMapCall3CandidatePaths();

static WorldMapCall3LoadResult loadWorldMapCall3CacheFromFiles(QStringList const& candidates)
{
    WorldMapCall3LoadResult result;
    QElapsedTimer timer;
    timer.start();

    QSet<QString> openedPaths;
    for (QString const& path : candidates) {
        if (path.isEmpty() || openedPaths.contains(path)) {
            continue;
        }
        openedPaths.insert(path);

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QTextStream in(&file);
        while (!in.atEnd()) {
            QString const line = in.readLine().trimmed();
            if (line.isEmpty()
                || line.startsWith(QLatin1Char('#'))
                || line.startsWith(QLatin1Char(';'))) {
                continue;
            }

            int const comma1 = line.indexOf(QLatin1Char(','));
            if (comma1 <= 0) {
                continue;
            }
            int comma2 = line.indexOf(QLatin1Char(','), comma1 + 1);
            if (comma2 < 0) {
                comma2 = line.size();
            }

            QString const key = decodiumBridgeWorldMapCallKey(line.left(comma1));
            QString const locator = line.mid(comma1 + 1, comma2 - comma1 - 1).trimmed().toUpper();
            if (!key.isEmpty() && isGridTokenStrict(locator)) {
                result.grids.insert(key, locator.left(qMin(6, locator.size())));
            }
        }

        if (!result.grids.isEmpty()) {
            result.loaded = result.grids.size();
            result.path = path;
            break;
        }
    }

    result.elapsedMs = timer.elapsed();
    return result;
}

void DecodiumBridge::rememberWorldMapGrid(const QString& call, const QString& grid)
{
    QString const key = decodiumBridgeWorldMapCallKey(call);
    QString locator = grid.trimmed().toUpper();
    if (key.isEmpty() || !isGridTokenStrict(locator)) {
        return;
    }

    m_worldMapGridByCall.insert(key, locator.left(qMin(6, locator.size())));
}

void DecodiumBridge::loadWorldMapCall3Cache()
{
    if (m_worldMapCall3Loaded || m_worldMapCall3Loading) {
        return;
    }
    m_worldMapCall3Loading = true;

    QStringList const candidates = worldMapCall3CandidatePaths();
    QPointer<DecodiumBridge> self(this);
    auto* task = QRunnable::create([self, candidates]() {
        WorldMapCall3LoadResult result = loadWorldMapCall3CacheFromFiles(candidates);
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, result = std::move(result)]() mutable {
            if (!self) {
                return;
            }
            self->m_worldMapCall3Loading = false;
            self->m_worldMapCall3Loaded = true;
            self->m_worldMapCall3RecordCount = result.loaded;
            for (auto it = result.grids.cbegin(); it != result.grids.cend(); ++it) {
                if (!self->m_worldMapGridByCall.contains(it.key())) {
                    self->m_worldMapGridByCall.insert(it.key(), it.value());
                }
            }
            if (result.loaded > 0) {
                decodiumBridgeAudioLevelLog(QStringLiteral("WorldMap CALL3 loaded %1 grids from %2 in background (%3 ms)")
                              .arg(result.loaded)
                              .arg(result.path)
                              .arg(result.elapsedMs));
            } else {
                decodiumBridgeAudioLevelLog(QStringLiteral("WorldMap CALL3 not found or empty; async lookup finished in %1 ms")
                              .arg(result.elapsedMs));
            }
            emit self->localDataStateChanged();
        }, Qt::QueuedConnection);
    });
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task, -1);
}

QString DecodiumBridge::lookupWorldMapGrid(const QString& call)
{
    QString const key = decodiumBridgeWorldMapCallKey(call);
    if (key.isEmpty()) {
        return {};
    }

    QString locator = m_worldMapGridByCall.value(key);
    if (locator.isEmpty()) {
        loadWorldMapCall3Cache();
        locator = m_worldMapGridByCall.value(key);
    }
    return isGridTokenStrict(locator) ? locator.left(qMin(6, locator.size())) : QString {};
}

QString DecodiumBridge::approximateWorldMapGridForCall(const QString& call)
{
    QString const mapCall = decodiumBridgeNormalizeWorldMapCall(call);
    if (mapCall.isEmpty() || !m_dxccLookup) {
        return {};
    }

    if (!m_dxccLookup->isLoaded()) {
        loadDxccLookupAsync();
    }
    if (!m_dxccLookup->isLoaded()) {
        return {};
    }

    QStringList lookupCandidates;
    lookupCandidates << mapCall;
    QString const baseCall = Radio::base_callsign(mapCall).trimmed().toUpper();
    if (!baseCall.isEmpty()) {
        lookupCandidates << baseCall;
    }
    int const slashPos = mapCall.indexOf(QLatin1Char('/'));
    if (slashPos > 0) {
        QString const left = mapCall.left(slashPos).trimmed().toUpper();
        QString const right = mapCall.mid(slashPos + 1).trimmed().toUpper();
        if (!left.isEmpty()) {
            lookupCandidates << left;
        }
        if (!right.isEmpty()) {
            lookupCandidates << right;
        }
    }
    lookupCandidates.removeDuplicates();

    DxccEntity entity;
    for (QString const& candidate : std::as_const(lookupCandidates)) {
        entity = m_dxccLookup->lookup(candidate);
        if (entity.isValid() && std::isfinite(entity.lat) && std::isfinite(entity.lon)) {
            break;
        }
        entity = {};
    }
    if (!entity.isValid()) {
        return {};
    }

    // cty.dat stores longitude as positive west; map/grid code uses positive east.
    QString const locator = maidenheadFromLonLat(-entity.lon, entity.lat);
    return isGridTokenStrict(locator) ? locator : QString {};
}

