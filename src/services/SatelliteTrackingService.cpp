#include "SatelliteTrackingService.h"

#include "RotatorService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMetaObject>
#include <QPointer>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QThreadPool>
#include <QtMath>

#include <cmath>
#include <cstring>

namespace {

constexpr double kJulianUnixEpoch = 2440587.5;
constexpr double kMillisecondsPerDay = 86400000.0;
constexpr double kEarthRotationRadPerSecond = 7.29211514670698e-5;
constexpr double kSpeedOfLightKmPerSecond = 299792.458;
constexpr double kDegreesToRadians = M_PI / 180.0;
constexpr double kRadiansToDegrees = 180.0 / M_PI;

QString stateDateTime(qint64 timestampMs)
{
    return QDateTime::fromMSecsSinceEpoch(timestampMs, Qt::UTC).toString(Qt::ISODate);
}

bool finiteVector(double const values[3])
{
    return qIsFinite(values[0]) && qIsFinite(values[1]) && qIsFinite(values[2]);
}

}

SatelliteTrackingService::SatelliteTrackingService(QObject* parent)
    : QObject(parent),
      m_network(new QNetworkAccessManager(this)),
      m_timer(new QTimer(this)),
      m_rotator(new RotatorService(this)),
      m_sourceUrl(QStringLiteral("https://celestrak.org/NORAD/elements/gp.php?GROUP=amateur&FORMAT=tle"))
{
    if (m_rotator) {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        settings.beginGroup(QStringLiteral("SatelliteRotator"));
        m_rotatorHost = settings.value(QStringLiteral("Host"), m_rotatorHost).toString().trimmed();
        QString const configuredProtocol = settings.value(
            QStringLiteral("Protocol"), QStringLiteral("PSTRotator")).toString();
        m_rotatorPort = qBound(
            1, settings.value(QStringLiteral("Port"),
                              RotatorService::defaultPortForProtocol(configuredProtocol)).toInt(),
            65535);
        m_rotatorEnabled = settings.value(QStringLiteral("Enabled"), false).toBool();
        settings.endGroup();
        m_rotator->setProtocol(configuredProtocol);
        m_rotator->setHost(m_rotatorHost);
        m_rotator->setPort(m_rotatorPort);
        m_rotator->setEnabled(m_rotatorEnabled);
        connect(m_rotator, &RotatorService::statusChanged, this, [this]() {
            if (m_rotator && !m_rotator->status().isEmpty())
                setStatus(m_rotator->status());
        });
    }
    QString const root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir cacheDirectory(root.isEmpty() ? QDir::tempPath() : root);
    cacheDirectory.mkpath(QStringLiteral("satellites"));
    m_cachePath = cacheDirectory.absoluteFilePath(QStringLiteral("satellites/amateur.tle"));

    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &SatelliteTrackingService::updateTrackingState);
    connect(m_network, &QNetworkAccessManager::finished,
            this, [this](QNetworkReply* reply) {
        if (reply == m_reply) {
            handleTleReply();
        }
    });

    QTimer::singleShot(0, this, &SatelliteTrackingService::loadCacheAsync);
}

SatelliteTrackingService::~SatelliteTrackingService()
{
    // The network manager is our QObject child and synchronously destroys any
    // outstanding replies during the base QObject teardown.  At application
    // shutdown the event dispatcher may already have drained a reply's
    // deferred delete, so dereferencing it here is both unnecessary and, for
    // some Qt network backends, unsafe.  Receiver connections are removed by
    // QObject teardown before the child manager is deleted.
    m_reply.clear();
}

void SatelliteTrackingService::setOfflineMode(bool offline)
{
    if (m_offlineMode == offline) {
        return;
    }
    m_offlineMode = offline;
    if (offline) {
        if (m_reply) {
            m_reply->abort();
        }
        setStatus(QStringLiteral("Offline: using cached satellite TLE data"));
    } else if (m_records.isEmpty()) {
        refreshTle();
    } else {
        setStatus(QStringLiteral("Online: satellite TLE refresh enabled"));
    }
    emit offlineModeChanged();
}

void SatelliteTrackingService::setSelectedSatellite(const QString& name)
{
    QString const normalized = name.trimmed();
    if (normalized == m_selectedSatellite) return;
    if (!normalized.isEmpty() && !findSatellite(normalized)) return;
    m_selectedSatellite = normalized;
    refreshSelectedState();
    emit selectedSatelliteChanged();
}

void SatelliteTrackingService::setObserverLatitude(double value)
{
    double const normalized = clamp(value, -90.0, 90.0);
    if (qFuzzyCompare(normalized, m_observerLatitude)) return;
    m_observerLatitude = normalized;
    m_observerGrid.clear();
    refreshSelectedState();
    emit observerChanged();
}

void SatelliteTrackingService::setObserverLongitude(double value)
{
    double normalized = std::fmod(value + 180.0, 360.0);
    if (normalized < 0.0) normalized += 360.0;
    normalized -= 180.0;
    if (qFuzzyCompare(normalized, m_observerLongitude)) return;
    m_observerLongitude = normalized;
    m_observerGrid.clear();
    refreshSelectedState();
    emit observerChanged();
}

void SatelliteTrackingService::setObserverAltitudeMeters(double value)
{
    double const normalized = qMax(-500.0, value);
    if (qFuzzyCompare(normalized, m_observerAltitudeMeters)) return;
    m_observerAltitudeMeters = normalized;
    refreshSelectedState();
    emit observerChanged();
}

void SatelliteTrackingService::setNominalFrequencyHz(double value)
{
    double const normalized = qMax(0.0, value);
    if (qFuzzyCompare(normalized, m_nominalFrequencyHz)) return;
    m_nominalFrequencyHz = normalized;
    refreshSelectedState();
    emit trackingSettingsChanged();
}

void SatelliteTrackingService::setDopplerTracking(bool enabled)
{
    if (enabled == m_dopplerTracking) return;
    m_dopplerTracking = enabled;
    refreshSelectedState();
    emit trackingSettingsChanged();
}

void SatelliteTrackingService::setAutoRotator(bool enabled)
{
    if (enabled == m_autoRotator) return;
    m_autoRotator = enabled;
    if (!enabled && m_rotator) m_rotator->stopTracking();
    emit trackingSettingsChanged();
}

void SatelliteTrackingService::setRotatorHost(const QString& host)
{
    QString const normalized = host.trimmed();
    if (normalized.isEmpty() || normalized == m_rotatorHost) return;
    m_rotatorHost = normalized;
    if (m_rotator) m_rotator->setHost(normalized);
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("SatelliteRotator"));
    settings.setValue(QStringLiteral("Host"), normalized);
    settings.endGroup();
    emit trackingSettingsChanged();
}

void SatelliteTrackingService::setRotatorPort(int port)
{
    int const normalized = qBound(1, port, 65535);
    if (normalized == m_rotatorPort) return;
    m_rotatorPort = normalized;
    if (m_rotator) m_rotator->setPort(normalized);
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("SatelliteRotator"));
    settings.setValue(QStringLiteral("Port"), normalized);
    settings.endGroup();
    emit trackingSettingsChanged();
}

void SatelliteTrackingService::setRotatorEnabled(bool enabled)
{
    if (enabled == m_rotatorEnabled) return;
    m_rotatorEnabled = enabled;
    if (m_rotator) m_rotator->setEnabled(enabled);
    if (!enabled && m_rotator) m_rotator->stopTracking();
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("SatelliteRotator"));
    settings.setValue(QStringLiteral("Enabled"), enabled);
    settings.endGroup();
    emit trackingSettingsChanged();
}

void SatelliteTrackingService::setRotatorProtocol(const QString& protocol)
{
    if (!m_rotator) return;
    QString const previousProtocol = m_rotator->protocol();
    bool const followedDefaultPort =
        m_rotatorPort == RotatorService::defaultPortForProtocol(previousProtocol);
    m_rotator->setProtocol(protocol);
    if (followedDefaultPort) {
        int const recommendedPort =
            RotatorService::defaultPortForProtocol(m_rotator->protocol());
        if (recommendedPort != m_rotatorPort)
            setRotatorPort(recommendedPort);
    }
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("SatelliteRotator"));
    settings.setValue(QStringLiteral("Protocol"), m_rotator->protocol());
    settings.endGroup();
    emit trackingSettingsChanged();
}

QString SatelliteTrackingService::rotatorProtocol() const
{
    return m_rotator ? m_rotator->protocol() : QStringLiteral("PSTRotator");
}

QObject* SatelliteTrackingService::rotator() const
{
    return m_rotator;
}

void SatelliteTrackingService::setSourceUrl(const QString& url)
{
    QString const normalized = url.trimmed();
    if (normalized.isEmpty() || normalized == m_sourceUrl) return;
    m_sourceUrl = normalized;
    emit sourceChanged();
}

void SatelliteTrackingService::refreshTle()
{
    if (m_offlineMode || m_updateInFlight) return;
    QUrl const url(m_sourceUrl);
    if (!url.isValid() || url.scheme().isEmpty()) {
        setStatus(QStringLiteral("TLE source URL is invalid"));
        return;
    }
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Decodium Satellite Tracking/1.0"));
    request.setTransferTimeout(20000);
    m_reply = m_network->get(request);
    m_updateInFlight = true;
    emit updatingChanged();
    setStatus(QStringLiteral("Downloading amateur satellite TLE data"));
}

void SatelliteTrackingService::handleTleReply()
{
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    if (!reply) return;

    QByteArray const payload = reply->readAll();
    bool const networkOk = reply->error() == QNetworkReply::NoError;
    QString const networkError = reply->errorString();
    reply->deleteLater();
    if (m_offlineMode) {
        m_updateInFlight = false;
        emit updatingChanged();
        setStatus(QStringLiteral("Offline: using cached satellite TLE data"));
        return;
    }
    QString const cachePath = m_cachePath;
    QPointer<SatelliteTrackingService> guard(this);
    QThreadPool::globalInstance()->start(QRunnable::create(
        [guard, payload, networkOk, networkError, cachePath] {
        QString parseError;
        QVector<SatelliteRecord> records;
        bool const ok = networkOk && guard
            && guard->parseTleData(payload, &records, &parseError)
            && !records.isEmpty();
        qint64 cacheTimestamp = 0;
        if (ok) {
            QFile cache(cachePath);
            if (cache.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                cache.write(payload);
                cache.close();
                cacheTimestamp = QFileInfo(cachePath).lastModified().toMSecsSinceEpoch();
            } else {
                cacheTimestamp = QDateTime::currentMSecsSinceEpoch();
            }
        }
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(),
            [guard, ok, records, cacheTimestamp, networkError, parseError] {
            if (!guard) return;
            if (guard->m_offlineMode) {
                guard->m_updateInFlight = false;
                emit guard->updatingChanged();
                guard->setStatus(QStringLiteral("Offline: using cached satellite TLE data"));
                return;
            }
            if (ok) {
                QString const selected = guard->m_selectedSatellite;
                guard->m_tleUpdatedMs = cacheTimestamp;
                guard->m_records = records;
                guard->rebuildSatelliteProperties();
                if (!selected.isEmpty() && guard->findSatellite(selected)) {
                    guard->m_selectedSatellite = selected;
                } else if (guard->m_selectedSatellite.isEmpty() && !guard->m_records.isEmpty()) {
                    guard->m_selectedSatellite = guard->m_records.first().name;
                } else if (!guard->findSatellite(guard->m_selectedSatellite)) {
                    guard->m_selectedSatellite.clear();
                }
                guard->refreshSelectedState();
                emit guard->selectedSatelliteChanged();
                guard->setStatus(QStringLiteral("Loaded %1 satellite TLEs")
                                     .arg(guard->m_records.size()));
            } else {
                QString message = networkError;
                if (message.isEmpty()) message = parseError;
                if (message.isEmpty()) message = QStringLiteral("TLE data is empty or invalid");
                guard->setStatus(QStringLiteral("TLE update failed: %1").arg(message));
            }
            guard->m_updateInFlight = false;
            emit guard->updatingChanged();
        }, Qt::QueuedConnection);
    }));
}

void SatelliteTrackingService::loadCacheAsync()
{
    QString const cachePath = m_cachePath;
    QPointer<SatelliteTrackingService> guard(this);
    QThreadPool::globalInstance()->start(QRunnable::create([guard, cachePath] {
        QFile cache(cachePath);
        QByteArray payload;
        qint64 timestamp = 0;
        bool const readable = cache.exists() && cache.open(QIODevice::ReadOnly);
        if (readable) {
            payload = cache.readAll();
            cache.close();
            timestamp = QFileInfo(cachePath).lastModified().toMSecsSinceEpoch();
        }
        QString error;
        QVector<SatelliteRecord> records;
        bool const ok = guard && readable
            && guard->parseTleData(payload, &records, &error)
            && !records.isEmpty();
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, ok, records, timestamp, error] {
            if (!guard) return;
            if (!ok) {
                if (!error.isEmpty())
                    guard->setStatus(QStringLiteral("TLE cache ignored: %1").arg(error));
                QTimer::singleShot(1000, guard.data(), &SatelliteTrackingService::refreshTle);
                return;
            }
            guard->m_tleUpdatedMs = timestamp;
            guard->m_records = records;
            guard->rebuildSatelliteProperties();
            if (!guard->m_records.isEmpty())
                guard->m_selectedSatellite = guard->m_records.first().name;
            guard->refreshSelectedState();
            emit guard->selectedSatelliteChanged();
            guard->setStatus(QStringLiteral("Loaded %1 cached satellite TLEs")
                                 .arg(guard->m_records.size()));
            if (guard->m_tleUpdatedMs > 0
                && QDateTime::currentMSecsSinceEpoch() - guard->m_tleUpdatedMs > 86400000) {
                QTimer::singleShot(1500, guard.data(), &SatelliteTrackingService::refreshTle);
            }
        }, Qt::QueuedConnection);
    }));
}

bool SatelliteTrackingService::parseTleData(const QByteArray& data,
                                            QVector<SatelliteRecord>* records,
                                            QString* error) const
{
    if (!records) return false;
    records->clear();
    QString pendingName;
    QStringList const lines = QString::fromUtf8(data).split(QChar('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        QString const line = lines.at(i).trimmed();
        if (line.isEmpty()) continue;
        if (!line.startsWith(QStringLiteral("1 "))) {
            if (!line.startsWith(QStringLiteral("2 "))) pendingName = line;
            continue;
        }
        if (i + 1 >= lines.size()) break;
        QString const line2 = lines.at(++i).trimmed();
        if (!line2.startsWith(QStringLiteral("2 "))) {
            pendingName.clear();
            continue;
        }
        SatelliteRecord record;
        QString const name = pendingName;
        pendingName.clear();
        if (!parseTlePair(name, line, line2, &record)) continue;
        records->append(record);
    }
    if (records->isEmpty() && error) {
        *error = QStringLiteral("No valid two-line elements found");
    }
    return !records->isEmpty();
}

bool SatelliteTrackingService::parseTlePair(const QString& name,
                                            const QString& line1,
                                            const QString& line2,
                                            SatelliteRecord* record) const
{
    if (!record || line1.size() < 69 || line2.size() < 69
        || !line1.startsWith(QStringLiteral("1 "))
        || !line2.startsWith(QStringLiteral("2 "))
        || !validTleChecksum(line1) || !validTleChecksum(line2)) {
        return false;
    }
    bool noradOk = false;
    int const norad = line1.mid(2, 5).trimmed().toInt(&noradOk);
    if (!noradOk || norad <= 0) return false;

    char tle1[130];
    char tle2[130];
    std::memset(tle1, ' ', sizeof(tle1));
    std::memset(tle2, ' ', sizeof(tle2));
    QByteArray const raw1 = line1.toLatin1();
    QByteArray const raw2 = line2.toLatin1();
    std::memcpy(tle1, raw1.constData(), qMin(raw1.size(), 129));
    std::memcpy(tle2, raw2.constData(), qMin(raw2.size(), 129));
    tle1[129] = '\0';
    tle2[129] = '\0';

    SatelliteRecord candidate;
    std::memset(&candidate.elements, 0, sizeof(candidate.elements));
    double startMfe = 0.0;
    double stopMfe = 0.0;
    double deltaMfe = 0.0;
    decodium_sgp4::SGP4Funcs::twoline2rv(
        tle1, tle2, 'c', 'e', 'i', decodium_sgp4::wgs84,
        startMfe, stopMfe, deltaMfe, candidate.elements);
    if (candidate.elements.error != 0 || !qIsFinite(candidate.elements.jdsatepoch)) {
        return false;
    }
    candidate.name = name.trimmed().isEmpty()
        ? QStringLiteral("NORAD %1").arg(norad)
        : name.trimmed();
    candidate.line1 = line1;
    candidate.line2 = line2;
    candidate.norad = norad;
    candidate.epochJulian = candidate.elements.jdsatepoch + candidate.elements.jdsatepochF;
    candidate.epochMs = timestampFromJulian(candidate.epochJulian);
    *record = candidate;
    return true;
}

void SatelliteTrackingService::selectSatellite(const QString& name)
{
    setSelectedSatellite(name);
}

bool SatelliteTrackingService::setObserverGrid(const QString& grid)
{
    QString const normalized = normaliseGrid(grid);
    if (normalized.size() < 4) return false;
    int const fieldLon = normalized.at(0).unicode() - QChar('A').unicode();
    int const fieldLat = normalized.at(1).unicode() - QChar('A').unicode();
    int const squareLon = normalized.at(2).digitValue();
    int const squareLat = normalized.at(3).digitValue();
    if (fieldLon < 0 || fieldLon > 17 || fieldLat < 0 || fieldLat > 17
        || squareLon < 0 || squareLon > 9 || squareLat < 0 || squareLat > 9) {
        return false;
    }
    double longitude = -180.0 + fieldLon * 20.0 + squareLon * 2.0 + 1.0;
    double latitude = -90.0 + fieldLat * 10.0 + squareLat * 1.0 + 0.5;
    if (normalized.size() >= 6) {
        int const subLon = normalized.at(4).unicode() - QChar('A').unicode();
        int const subLat = normalized.at(5).unicode() - QChar('A').unicode();
        if (subLon < 0 || subLon > 23 || subLat < 0 || subLat > 23) return false;
        longitude += (subLon + 0.5) * (2.0 / 24.0);
        latitude += (subLat + 0.5) * (1.0 / 24.0);
    }
    m_observerGrid = normalized;
    bool changed = !qFuzzyCompare(latitude, m_observerLatitude)
        || !qFuzzyCompare(longitude, m_observerLongitude);
    m_observerLatitude = latitude;
    m_observerLongitude = longitude;
    refreshSelectedState();
    if (changed) emit observerChanged();
    return true;
}

QVariantList SatelliteTrackingService::predictPasses(int hours, double minimumElevation)
{
    predictPassesAsync(hours, minimumElevation);
    return m_upcomingPasses;
}

void SatelliteTrackingService::predictPassesAsync(int hours, double minimumElevation)
{
    SatelliteRecord const* satellite = findSatellite(m_selectedSatellite);
    if (!satellite) {
        m_upcomingPasses.clear();
        emit passesChanged();
        return;
    }
    if (m_predictingPasses) return;
    m_predictingPasses = true;
    ++m_predictionGeneration;
    quint64 const generation = m_predictionGeneration;
    SatelliteRecord const snapshot = *satellite;
    int const boundedHours = qBound(1, hours, 168);
    double const minElevation = clamp(minimumElevation, -5.0, 89.0);
    qint64 const startMs = QDateTime::currentMSecsSinceEpoch();
    double const observerLatitude = m_observerLatitude;
    double const observerLongitude = m_observerLongitude;
    double const observerAltitudeMeters = m_observerAltitudeMeters;
    double const nominalFrequencyHz = m_nominalFrequencyHz;
    QString const satelliteName = snapshot.name;
    QPointer<SatelliteTrackingService> guard(this);
    emit predictingPassesChanged();
    setStatus(QStringLiteral("Predicting passes for %1") .arg(satelliteName));
    QThreadPool::globalInstance()->start(QRunnable::create(
        [guard, snapshot, startMs, boundedHours, minElevation,
         observerLatitude, observerLongitude, observerAltitudeMeters,
         nominalFrequencyHz, generation, satelliteName] {
        QVariantList const passes = SatelliteTrackingService::predictPassesSnapshot(
            snapshot, startMs, boundedHours, minElevation,
            observerLatitude, observerLongitude, observerAltitudeMeters,
            nominalFrequencyHz);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, passes, generation, satelliteName] {
            if (!guard || guard->m_predictionGeneration != generation) return;
            guard->m_upcomingPasses = passes;
            guard->m_predictingPasses = false;
            emit guard->passesChanged();
            emit guard->predictingPassesChanged();
            guard->setStatus(QStringLiteral("Predicted %1 pass(es) for %2")
                                 .arg(passes.size()).arg(satelliteName));
        }, Qt::QueuedConnection);
    }));
}

void SatelliteTrackingService::startTracking()
{
    if (!findSatellite(m_selectedSatellite)) {
        setStatus(QStringLiteral("Select a satellite before starting tracking"));
        return;
    }
    if (m_tracking) return;
    m_tracking = true;
    m_timer->start();
    refreshSelectedState();
    emit trackingChanged();
    setStatus(QStringLiteral("Tracking %1").arg(m_selectedSatellite));
}

void SatelliteTrackingService::stopTracking()
{
    if (!m_tracking) return;
    m_tracking = false;
    m_timer->stop();
    if (m_rotator) m_rotator->stopTracking();
    emit trackingChanged();
    setStatus(QStringLiteral("Satellite tracking stopped"));
}

void SatelliteTrackingService::updateTrackingState()
{
    if (m_tracking) refreshSelectedState();
}

void SatelliteTrackingService::refreshSelectedState()
{
    ++m_stateGeneration;
    if (m_stateUpdateInFlight) return;
    SatelliteRecord const* satellite = findSatellite(m_selectedSatellite);
    if (!satellite) {
        applySelectedState(QVariantMap());
        return;
    }
    SatelliteRecord const snapshot = *satellite;
    double const observerLatitude = m_observerLatitude;
    double const observerLongitude = m_observerLongitude;
    double const observerAltitudeMeters = m_observerAltitudeMeters;
    double const nominalFrequencyHz = m_nominalFrequencyHz;
    quint64 const generation = m_stateGeneration;
    qint64 const timestampMs = QDateTime::currentMSecsSinceEpoch();
    QPointer<SatelliteTrackingService> guard(this);
    m_stateUpdateInFlight = true;
    QThreadPool::globalInstance()->start(QRunnable::create(
        [guard, snapshot, observerLatitude, observerLongitude,
         observerAltitudeMeters, nominalFrequencyHz, timestampMs, generation] {
        QVariantMap state;
        SatelliteTrackingService::propagateSnapshot(
            snapshot, timestampMs, observerLatitude, observerLongitude,
            observerAltitudeMeters, nominalFrequencyHz, &state);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, state, generation] {
            if (!guard) return;
            guard->m_stateUpdateInFlight = false;
            if (generation != guard->m_stateGeneration) {
                guard->refreshSelectedState();
                return;
            }
            guard->applySelectedState(state);
        }, Qt::QueuedConnection);
    }));
}

void SatelliteTrackingService::applySelectedState(const QVariantMap& state)
{
    if (state != m_selectedState) {
        m_selectedState = state;
        double const newDopplerFrequency = state.value(QStringLiteral("dopplerFrequencyHz"), 0.0).toDouble();
        bool const frequencyChanged = !qFuzzyCompare(newDopplerFrequency + 1.0,
                                                      m_dopplerFrequencyHz + 1.0);
        m_dopplerFrequencyHz = newDopplerFrequency;
        emit stateChanged();
        if (frequencyChanged) emit dopplerFrequencyChanged(m_dopplerFrequencyHz);
        if (m_autoRotator && m_rotatorEnabled && state.value(QStringLiteral("visible")).toBool()) {
            sendRotator(state.value(QStringLiteral("azimuth")).toDouble(),
                        state.value(QStringLiteral("elevation")).toDouble());
        } else if (m_rotator && m_rotator->tracking()) {
            // Never keep a physical antenna moving after the target drops
            // below the configured horizon or the tracking state disappears.
            m_rotator->stopTracking();
        }
        QVariantList markers;
        if (state.value(QStringLiteral("valid")).toBool()) {
            QVariantMap marker;
            int const norad = state.value(QStringLiteral("norad")).toInt();
            QString const satelliteName = state.value(QStringLiteral("satelliteName")).toString();
            marker.insert(QStringLiteral("id"), QStringLiteral("satellite:%1").arg(norad));
            marker.insert(QStringLiteral("type"), QStringLiteral("SATELLITE"));
            marker.insert(QStringLiteral("reference"), QString::number(norad));
            marker.insert(QStringLiteral("label"), satelliteName);
            marker.insert(QStringLiteral("name"), satelliteName);
            marker.insert(QStringLiteral("latitude"), state.value(QStringLiteral("satelliteLatitude")));
            marker.insert(QStringLiteral("longitude"), state.value(QStringLiteral("satelliteLongitude")));
            marker.insert(QStringLiteral("azimuth"), state.value(QStringLiteral("azimuth")));
            marker.insert(QStringLiteral("elevation"), state.value(QStringLiteral("elevation")));
            marker.insert(QStringLiteral("rangeKm"), state.value(QStringLiteral("rangeKm")));
            marker.insert(QStringLiteral("dopplerHz"), state.value(QStringLiteral("dopplerHz")));
            marker.insert(QStringLiteral("visible"), state.value(QStringLiteral("visible")));
            markers.append(marker);
        }
        m_mapMarkers = markers;
    }
}

bool SatelliteTrackingService::propagateSnapshot(const SatelliteRecord& satellite,
                                                 qint64 timestampMs,
                                                 double observerLatitude,
                                                 double observerLongitude,
                                                 double observerAltitudeMeters,
                                                 double nominalFrequencyHz,
                                                 QVariantMap* state)
{
    if (!state) return false;
    state->clear();
    double const julian = julianDate(timestampMs);
    double const tsince = (julian - satellite.epochJulian) * 1440.0;
    decodium_sgp4::elsetrec elements = satellite.elements;
    double positionTeme[3] {0.0, 0.0, 0.0};
    double velocityTeme[3] {0.0, 0.0, 0.0};
    if (!decodium_sgp4::SGP4Funcs::sgp4(elements, tsince, positionTeme, velocityTeme)
        || elements.error != 0 || !finiteVector(positionTeme) || !finiteVector(velocityTeme)) {
        state->insert(QStringLiteral("valid"), false);
        state->insert(QStringLiteral("satelliteName"), satellite.name);
        state->insert(QStringLiteral("norad"), satellite.norad);
        return false;
    }

    double const gmst = decodium_sgp4::SGP4Funcs::gstime(julian);
    double const cosGmst = std::cos(gmst);
    double const sinGmst = std::sin(gmst);
    Vector3 const positionEcef {
        cosGmst * positionTeme[0] + sinGmst * positionTeme[1],
        -sinGmst * positionTeme[0] + cosGmst * positionTeme[1],
        positionTeme[2]
    };
    Vector3 const velocityEcef {
        cosGmst * velocityTeme[0] + sinGmst * velocityTeme[1]
            + kEarthRotationRadPerSecond * (-sinGmst * positionTeme[0]
                                             + cosGmst * positionTeme[1]),
        -sinGmst * velocityTeme[0] + cosGmst * velocityTeme[1]
            + kEarthRotationRadPerSecond * (-cosGmst * positionTeme[0]
                                             - sinGmst * positionTeme[1]),
        velocityTeme[2]
    };
    Vector3 const observer = observerEcef(observerLatitude, observerLongitude,
                                          observerAltitudeMeters);
    Vector3 const observerVelocity {
        -kEarthRotationRadPerSecond * observer.y,
        kEarthRotationRadPerSecond * observer.x,
        0.0
    };
    Vector3 const relative {
        positionEcef.x - observer.x,
        positionEcef.y - observer.y,
        positionEcef.z - observer.z
    };
    Vector3 const relativeVelocity {
        velocityEcef.x - observerVelocity.x,
        velocityEcef.y - observerVelocity.y,
        velocityEcef.z - observerVelocity.z
    };
    double const latitude = observerLatitude * kDegreesToRadians;
    double const longitude = observerLongitude * kDegreesToRadians;
    double const sinLatitude = std::sin(latitude);
    double const cosLatitude = std::cos(latitude);
    double const sinLongitude = std::sin(longitude);
    double const cosLongitude = std::cos(longitude);
    double const east = -sinLongitude * relative.x + cosLongitude * relative.y;
    double const north = -sinLatitude * cosLongitude * relative.x
        - sinLatitude * sinLongitude * relative.y + cosLatitude * relative.z;
    double const up = cosLatitude * cosLongitude * relative.x
        + cosLatitude * sinLongitude * relative.y + sinLatitude * relative.z;
    double const range = std::sqrt(east * east + north * north + up * up);
    if (!qIsFinite(range) || range <= 0.0) {
        state->insert(QStringLiteral("valid"), false);
        return false;
    }
    double azimuth = std::atan2(east, north) * kRadiansToDegrees;
    if (azimuth < 0.0) azimuth += 360.0;
    double const elevation = std::asin(clamp(up / range, -1.0, 1.0)) * kRadiansToDegrees;
    double const rangeRate = (relative.x * relativeVelocity.x
                              + relative.y * relativeVelocity.y
                              + relative.z * relativeVelocity.z) / range;
    double const doppler = nominalFrequencyHz > 0.0
        ? -rangeRate / kSpeedOfLightKmPerSecond * nominalFrequencyHz : 0.0;
    double const dopplerFrequency = nominalFrequencyHz > 0.0
        ? nominalFrequencyHz + doppler : 0.0;
    double const satelliteLatitude = std::atan2(positionEcef.z,
                                                std::sqrt(positionEcef.x * positionEcef.x
                                                          + positionEcef.y * positionEcef.y))
        * kRadiansToDegrees;
    double satelliteLongitude = std::atan2(positionEcef.y, positionEcef.x) * kRadiansToDegrees;
    if (satelliteLongitude > 180.0) satelliteLongitude -= 360.0;

    state->insert(QStringLiteral("valid"), true);
    state->insert(QStringLiteral("timestampMs"), timestampMs);
    state->insert(QStringLiteral("timestamp"), stateDateTime(timestampMs));
    state->insert(QStringLiteral("satelliteName"), satellite.name);
    state->insert(QStringLiteral("norad"), satellite.norad);
    state->insert(QStringLiteral("azimuth"), azimuth);
    state->insert(QStringLiteral("elevation"), elevation);
    state->insert(QStringLiteral("rangeKm"), range);
    state->insert(QStringLiteral("rangeRateKmPerSec"), rangeRate);
    state->insert(QStringLiteral("dopplerHz"), doppler);
    state->insert(QStringLiteral("dopplerFrequencyHz"), dopplerFrequency);
    state->insert(QStringLiteral("visible"), elevation >= 0.0);
    state->insert(QStringLiteral("eastKm"), east);
    state->insert(QStringLiteral("northKm"), north);
    state->insert(QStringLiteral("upKm"), up);
    state->insert(QStringLiteral("satelliteLatitude"), satelliteLatitude);
    state->insert(QStringLiteral("satelliteLongitude"), satelliteLongitude);
    return true;
}

QVariantList SatelliteTrackingService::predictPassesSnapshot(
    const SatelliteRecord& satellite, qint64 startMs, int hours,
    double minimumElevation, double observerLatitude, double observerLongitude,
    double observerAltitudeMeters, double nominalFrequencyHz)
{
    QVariantList passes;
    int const boundedHours = qBound(1, hours, 168);
    qint64 const endMs = startMs + static_cast<qint64>(boundedHours) * 3600000;
    qint64 const stepMs = 30000;
    auto stateAt = [&satellite, observerLatitude, observerLongitude,
                    observerAltitudeMeters, nominalFrequencyHz](qint64 timestampMs) {
        QVariantMap state;
        propagateSnapshot(satellite, timestampMs, observerLatitude, observerLongitude,
                          observerAltitudeMeters, nominalFrequencyHz, &state);
        return state;
    };

    qint64 previousMs = startMs;
    QVariantMap previous = stateAt(previousMs);
    bool inPass = previous.value(QStringLiteral("valid")).toBool()
        && previous.value(QStringLiteral("elevation")).toDouble() >= minimumElevation;
    qint64 aosMs = inPass ? startMs : 0;
    qint64 maxMs = inPass ? startMs : 0;
    double maxElevation = inPass ? previous.value(QStringLiteral("elevation")).toDouble() : -90.0;

    auto appendPass = [&passes, &satellite, minimumElevation, &stateAt,
                       &aosMs, &maxMs, &maxElevation](qint64 losMs) {
        if (aosMs <= 0) return;
        QVariantMap const aosState = stateAt(aosMs);
        QVariantMap const maxState = stateAt(maxMs);
        QVariantMap const losState = stateAt(losMs);
        QVariantMap pass;
        pass.insert(QStringLiteral("satellite"), satellite.name);
        pass.insert(QStringLiteral("norad"), satellite.norad);
        pass.insert(QStringLiteral("aosMs"), aosMs);
        pass.insert(QStringLiteral("maxMs"), maxMs);
        pass.insert(QStringLiteral("losMs"), losMs);
        pass.insert(QStringLiteral("aos"), stateDateTime(aosMs));
        pass.insert(QStringLiteral("max"), stateDateTime(maxMs));
        pass.insert(QStringLiteral("los"), stateDateTime(losMs));
        pass.insert(QStringLiteral("durationSeconds"), (losMs - aosMs) / 1000);
        pass.insert(QStringLiteral("maxElevation"), maxElevation);
        pass.insert(QStringLiteral("aosAzimuth"), aosState.value(QStringLiteral("azimuth")));
        pass.insert(QStringLiteral("maxAzimuth"), maxState.value(QStringLiteral("azimuth")));
        pass.insert(QStringLiteral("losAzimuth"), losState.value(QStringLiteral("azimuth")));
        pass.insert(QStringLiteral("minimumElevation"), minimumElevation);
        passes.append(pass);
        aosMs = 0;
        maxMs = 0;
        maxElevation = -90.0;
    };

    for (qint64 currentMs = startMs + stepMs; currentMs <= endMs; currentMs += stepMs) {
        QVariantMap const current = stateAt(currentMs);
        bool const valid = current.value(QStringLiteral("valid")).toBool();
        if (!valid) {
            previousMs = currentMs;
            previous = current;
            continue;
        }
        double const elevation = current.value(QStringLiteral("elevation")).toDouble();
        if (!inPass && elevation >= minimumElevation
            && previous.value(QStringLiteral("valid")).toBool()
            && previous.value(QStringLiteral("elevation")).toDouble() < minimumElevation) {
            aosMs = refineHorizonCrossingSnapshot(
                satellite, previousMs, currentMs, minimumElevation, true,
                observerLatitude, observerLongitude, observerAltitudeMeters,
                nominalFrequencyHz);
            maxMs = currentMs;
            maxElevation = elevation;
            inPass = true;
        } else if (inPass && elevation > maxElevation) {
            maxMs = currentMs;
            maxElevation = elevation;
        }
        if (inPass && elevation < minimumElevation
            && previous.value(QStringLiteral("valid")).toBool()
            && previous.value(QStringLiteral("elevation")).toDouble() >= minimumElevation) {
            qint64 const losMs = refineHorizonCrossingSnapshot(
                satellite, previousMs, currentMs, minimumElevation, false,
                observerLatitude, observerLongitude, observerAltitudeMeters,
                nominalFrequencyHz);
            appendPass(losMs);
            inPass = false;
        }
        previousMs = currentMs;
        previous = current;
    }
    if (inPass) appendPass(endMs);
    return passes;
}

QVariantMap SatelliteTrackingService::recordToVariant(const SatelliteRecord& record) const
{
    QVariantMap result;
    result.insert(QStringLiteral("name"), record.name);
    result.insert(QStringLiteral("norad"), record.norad);
    result.insert(QStringLiteral("epochMs"), record.epochMs);
    result.insert(QStringLiteral("epoch"), stateDateTime(record.epochMs));
    result.insert(QStringLiteral("ageDays"),
                  (QDateTime::currentMSecsSinceEpoch() - record.epochMs) / 86400000.0);
    result.insert(QStringLiteral("valid"), record.elements.error == 0);
    result.insert(QStringLiteral("tleLine1"), record.line1);
    result.insert(QStringLiteral("tleLine2"), record.line2);
    return result;
}

const SatelliteTrackingService::SatelliteRecord*
SatelliteTrackingService::findSatellite(const QString& name) const
{
    for (QVector<SatelliteRecord>::const_iterator it = m_records.constBegin();
         it != m_records.constEnd(); ++it) {
        if (it->name.compare(name, Qt::CaseInsensitive) == 0) return &(*it);
    }
    return nullptr;
}

void SatelliteTrackingService::rebuildSatelliteProperties()
{
    m_satelliteNames.clear();
    m_satellites.clear();
    for (QVector<SatelliteRecord>::const_iterator it = m_records.constBegin();
         it != m_records.constEnd(); ++it) {
        m_satelliteNames.append(it->name);
        m_satellites.append(recordToVariant(*it));
    }
    emit satellitesChanged();
}

void SatelliteTrackingService::setStatus(const QString& message)
{
    if (message == m_statusMessage) return;
    m_statusMessage = message;
    emit statusChanged();
}

void SatelliteTrackingService::sendRotator(double azimuth, double elevation)
{
    if (!m_rotator) return;
    m_rotator->trackTarget(azimuth, qMax(0.0, elevation), true);
}

bool SatelliteTrackingService::validTleChecksum(const QString& line)
{
    if (line.size() < 69 || !line.at(68).isDigit()) return false;
    int sum = 0;
    for (int i = 0; i < 68; ++i) {
        QChar const c = line.at(i);
        if (c.isDigit()) sum += c.digitValue();
        else if (c == QChar('-')) ++sum;
    }
    return (sum % 10) == line.at(68).digitValue();
}

double SatelliteTrackingService::julianDate(qint64 timestampMs)
{
    return kJulianUnixEpoch + static_cast<double>(timestampMs) / kMillisecondsPerDay;
}

qint64 SatelliteTrackingService::timestampFromJulian(double julian)
{
    return qRound64((julian - kJulianUnixEpoch) * kMillisecondsPerDay);
}

SatelliteTrackingService::Vector3 SatelliteTrackingService::observerEcef(
    double latitude, double longitude, double altitudeMeters)
{
    double const lat = latitude * kDegreesToRadians;
    double const lon = longitude * kDegreesToRadians;
    double const sinLat = std::sin(lat);
    double const cosLat = std::cos(lat);
    double const sinLon = std::sin(lon);
    double const cosLon = std::cos(lon);
    double const a = 6378.137;
    double const e2 = 0.0066943799901413165;
    double const heightKm = altitudeMeters / 1000.0;
    double const n = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
    return Vector3 {(n + heightKm) * cosLat * cosLon,
                    (n + heightKm) * cosLat * sinLon,
                    (n * (1.0 - e2) + heightKm) * sinLat};
}

QString SatelliteTrackingService::normaliseGrid(const QString& grid)
{
    QString result;
    for (int i = 0; i < grid.size(); ++i) {
        QChar const c = grid.at(i).toUpper();
        if (!c.isSpace()) result.append(c);
    }
    return result;
}

double SatelliteTrackingService::clamp(double value, double low, double high)
{
    return qBound(low, value, high);
}

qint64 SatelliteTrackingService::refineHorizonCrossingSnapshot(
    const SatelliteRecord& satellite, qint64 lowMs, qint64 highMs,
    double minimumElevation, bool rising, double observerLatitude,
    double observerLongitude, double observerAltitudeMeters,
    double nominalFrequencyHz)
{
    for (int i = 0; i < 12; ++i) {
        qint64 const midMs = lowMs + (highMs - lowMs) / 2;
        QVariantMap state;
        propagateSnapshot(satellite, midMs, observerLatitude, observerLongitude,
                          observerAltitudeMeters, nominalFrequencyHz, &state);
        bool const above = state.value(QStringLiteral("valid")).toBool()
            && state.value(QStringLiteral("elevation")).toDouble() >= minimumElevation;
        if (rising) {
            if (above) highMs = midMs;
            else lowMs = midMs;
        } else {
            if (above) lowMs = midMs;
            else highMs = midMs;
        }
    }
    return lowMs + (highMs - lowMs) / 2;
}
