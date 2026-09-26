#pragma once

#include <QDateTime>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include "satellite/sgp4/SGP4.h"

class QNetworkAccessManager;
class QNetworkReply;
class RotatorService;
class QTimer;

class SatelliteTrackingService final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList satelliteNames READ satelliteNames NOTIFY satellitesChanged)
    Q_PROPERTY(QVariantList satellites READ satellites NOTIFY satellitesChanged)
    Q_PROPERTY(QString selectedSatellite READ selectedSatellite WRITE setSelectedSatellite NOTIFY selectedSatelliteChanged)
    Q_PROPERTY(QVariantMap selectedState READ selectedState NOTIFY stateChanged)
    Q_PROPERTY(QVariantList upcomingPasses READ upcomingPasses NOTIFY passesChanged)
    Q_PROPERTY(QVariantList mapMarkers READ mapMarkers NOTIFY stateChanged)
    Q_PROPERTY(double observerLatitude READ observerLatitude WRITE setObserverLatitude NOTIFY observerChanged)
    Q_PROPERTY(double observerLongitude READ observerLongitude WRITE setObserverLongitude NOTIFY observerChanged)
    Q_PROPERTY(double observerAltitudeMeters READ observerAltitudeMeters WRITE setObserverAltitudeMeters NOTIFY observerChanged)
    Q_PROPERTY(QString observerGrid READ observerGrid NOTIFY observerChanged)
    Q_PROPERTY(double azimuth READ azimuth NOTIFY stateChanged)
    Q_PROPERTY(double elevation READ elevation NOTIFY stateChanged)
    Q_PROPERTY(double rangeKm READ rangeKm NOTIFY stateChanged)
    Q_PROPERTY(double rangeRateKmPerSec READ rangeRateKmPerSec NOTIFY stateChanged)
    Q_PROPERTY(double dopplerHz READ dopplerHz NOTIFY stateChanged)
    Q_PROPERTY(double nominalFrequencyHz READ nominalFrequencyHz WRITE setNominalFrequencyHz NOTIFY trackingSettingsChanged)
    Q_PROPERTY(double dopplerFrequencyHz READ dopplerFrequencyHz NOTIFY stateChanged)
    Q_PROPERTY(bool dopplerTracking READ dopplerTracking WRITE setDopplerTracking NOTIFY trackingSettingsChanged)
    Q_PROPERTY(bool visible READ visible NOTIFY stateChanged)
    Q_PROPERTY(bool tracking READ tracking NOTIFY trackingChanged)
    Q_PROPERTY(bool autoRotator READ autoRotator WRITE setAutoRotator NOTIFY trackingSettingsChanged)
    Q_PROPERTY(QString rotatorHost READ rotatorHost WRITE setRotatorHost NOTIFY trackingSettingsChanged)
    Q_PROPERTY(int rotatorPort READ rotatorPort WRITE setRotatorPort NOTIFY trackingSettingsChanged)
    Q_PROPERTY(bool rotatorEnabled READ rotatorEnabled WRITE setRotatorEnabled NOTIFY trackingSettingsChanged)
    Q_PROPERTY(QString rotatorProtocol READ rotatorProtocol WRITE setRotatorProtocol NOTIFY trackingSettingsChanged)
    Q_PROPERTY(QObject* rotator READ rotator CONSTANT)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString sourceUrl READ sourceUrl WRITE setSourceUrl NOTIFY sourceChanged)
    Q_PROPERTY(QString cachePath READ cachePath CONSTANT)
    Q_PROPERTY(qint64 tleUpdatedMs READ tleUpdatedMs NOTIFY satellitesChanged)
    Q_PROPERTY(bool updating READ updating NOTIFY updatingChanged)
    Q_PROPERTY(bool predictingPasses READ predictingPasses NOTIFY predictingPassesChanged)
    Q_PROPERTY(bool offlineMode READ offlineMode WRITE setOfflineMode NOTIFY offlineModeChanged)

public:
    explicit SatelliteTrackingService(QObject* parent = nullptr);
    ~SatelliteTrackingService() override;

    QStringList satelliteNames() const { return m_satelliteNames; }
    QVariantList satellites() const { return m_satellites; }
    QString selectedSatellite() const { return m_selectedSatellite; }
    QVariantMap selectedState() const { return m_selectedState; }
    QVariantList upcomingPasses() const { return m_upcomingPasses; }
    QVariantList mapMarkers() const { return m_mapMarkers; }
    double observerLatitude() const { return m_observerLatitude; }
    double observerLongitude() const { return m_observerLongitude; }
    double observerAltitudeMeters() const { return m_observerAltitudeMeters; }
    QString observerGrid() const { return m_observerGrid; }
    double azimuth() const { return m_selectedState.value(QStringLiteral("azimuth")).toDouble(); }
    double elevation() const { return m_selectedState.value(QStringLiteral("elevation")).toDouble(); }
    double rangeKm() const { return m_selectedState.value(QStringLiteral("rangeKm")).toDouble(); }
    double rangeRateKmPerSec() const { return m_selectedState.value(QStringLiteral("rangeRateKmPerSec")).toDouble(); }
    double dopplerHz() const { return m_selectedState.value(QStringLiteral("dopplerHz")).toDouble(); }
    double nominalFrequencyHz() const { return m_nominalFrequencyHz; }
    double dopplerFrequencyHz() const { return m_dopplerFrequencyHz; }
    bool dopplerTracking() const { return m_dopplerTracking; }
    bool visible() const { return m_selectedState.value(QStringLiteral("visible")).toBool(); }
    bool tracking() const { return m_tracking; }
    bool autoRotator() const { return m_autoRotator; }
    QString rotatorHost() const { return m_rotatorHost; }
    int rotatorPort() const { return m_rotatorPort; }
    bool rotatorEnabled() const { return m_rotatorEnabled; }
    QString rotatorProtocol() const;
    QObject* rotator() const;
    QString statusMessage() const { return m_statusMessage; }
    QString sourceUrl() const { return m_sourceUrl; }
    QString cachePath() const { return m_cachePath; }
    qint64 tleUpdatedMs() const { return m_tleUpdatedMs; }
    bool updating() const { return m_updateInFlight; }
    bool predictingPasses() const { return m_predictingPasses; }
    bool offlineMode() const { return m_offlineMode; }

    void setSelectedSatellite(const QString& name);
    void setObserverLatitude(double value);
    void setObserverLongitude(double value);
    void setObserverAltitudeMeters(double value);
    void setNominalFrequencyHz(double value);
    void setDopplerTracking(bool enabled);
    void setAutoRotator(bool enabled);
    void setRotatorHost(const QString& host);
    void setRotatorPort(int port);
    void setRotatorEnabled(bool enabled);
    void setRotatorProtocol(const QString& protocol);
    void setSourceUrl(const QString& url);
    void setOfflineMode(bool offline);

    Q_INVOKABLE void refreshTle();
    Q_INVOKABLE void selectSatellite(const QString& name);
    Q_INVOKABLE bool setObserverGrid(const QString& grid);
    Q_INVOKABLE QVariantList predictPasses(int hours = 24, double minimumElevation = 0.0);
    Q_INVOKABLE void predictPassesAsync(int hours = 24, double minimumElevation = 0.0);
    Q_INVOKABLE void startTracking();
    Q_INVOKABLE void stopTracking();

signals:
    void satellitesChanged();
    void selectedSatelliteChanged();
    void stateChanged();
    void passesChanged();
    void observerChanged();
    void trackingSettingsChanged();
    void trackingChanged();
    void statusChanged();
    void sourceChanged();
    void updatingChanged();
    void predictingPassesChanged();
    void offlineModeChanged();
    void dopplerFrequencyChanged(double frequencyHz);

private slots:
    void updateTrackingState();
    void handleTleReply();

private:
    struct SatelliteRecord {
        QString name;
        QString line1;
        QString line2;
        int norad {0};
        qint64 epochMs {0};
        double epochJulian {0.0};
        decodium_sgp4::elsetrec elements {};
    };

    struct Vector3 {
        double x {0.0};
        double y {0.0};
        double z {0.0};
    };

    void loadCacheAsync();
    bool parseTleData(const QByteArray& data, QVector<SatelliteRecord>* records,
                      QString* error) const;
    bool parseTlePair(const QString& name, const QString& line1, const QString& line2,
                      SatelliteRecord* record) const;
    static bool propagateSnapshot(const SatelliteRecord& satellite, qint64 timestampMs,
                                  double observerLatitude, double observerLongitude,
                                  double observerAltitudeMeters, double nominalFrequencyHz,
                                  QVariantMap* state);
    static QVariantList predictPassesSnapshot(const SatelliteRecord& satellite,
                                              qint64 startMs, int hours,
                                              double minimumElevation,
                                              double observerLatitude,
                                              double observerLongitude,
                                              double observerAltitudeMeters,
                                              double nominalFrequencyHz);
    QVariantMap recordToVariant(const SatelliteRecord& record) const;
    const SatelliteRecord* findSatellite(const QString& name) const;
    void rebuildSatelliteProperties();
    void refreshSelectedState();
    void applySelectedState(const QVariantMap& state);
    void setStatus(const QString& message);
    void sendRotator(double azimuth, double elevation);
    static bool validTleChecksum(const QString& line);
    static double julianDate(qint64 timestampMs);
    static qint64 timestampFromJulian(double julianDate);
    static Vector3 observerEcef(double latitude, double longitude, double altitudeMeters);
    static QString normaliseGrid(const QString& grid);
    static double clamp(double value, double low, double high);
    static qint64 refineHorizonCrossingSnapshot(const SatelliteRecord& satellite,
                                                qint64 lowMs, qint64 highMs,
                                                double minimumElevation, bool rising,
                                                double observerLatitude,
                                                double observerLongitude,
                                                double observerAltitudeMeters,
                                                double nominalFrequencyHz);

    QNetworkAccessManager* m_network {nullptr};
    // A reply may be destroyed by QNetworkAccessManager while application
    // shutdown is already draining deferred deletes. Keep a guarded pointer
    // so the service destructor never dereferences a stale reply.
    QPointer<QNetworkReply> m_reply;
    QTimer* m_timer {nullptr};
    RotatorService* m_rotator {nullptr};
    QVector<SatelliteRecord> m_records;
    QStringList m_satelliteNames;
    QVariantList m_satellites;
    QVariantMap m_selectedState;
    QVariantList m_upcomingPasses;
    QVariantList m_mapMarkers;
    QString m_selectedSatellite;
    QString m_observerGrid;
    double m_observerLatitude {0.0};
    double m_observerLongitude {0.0};
    double m_observerAltitudeMeters {0.0};
    double m_nominalFrequencyHz {0.0};
    double m_dopplerFrequencyHz {0.0};
    bool m_dopplerTracking {false};
    bool m_tracking {false};
    bool m_autoRotator {false};
    QString m_rotatorHost {QStringLiteral("127.0.0.1")};
    int m_rotatorPort {12000};
    bool m_rotatorEnabled {false};
    QString m_statusMessage;
    QString m_sourceUrl;
    QString m_cachePath;
    qint64 m_tleUpdatedMs {0};
    bool m_updateInFlight {false};
    bool m_offlineMode {false};
    bool m_predictingPasses {false};
    quint64 m_predictionGeneration {0};
    bool m_stateUpdateInFlight {false};
    quint64 m_stateGeneration {0};
};
