#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>

class MapLayerModel;
class QNetworkAccessManager;
class QNetworkReply;
class RotatorService;
class QTimer;

class MapOperationsService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList potaSpots READ potaSpots NOTIFY potaSpotsChanged)
    Q_PROPERTY(QVariantMap selectedPotaPark READ selectedPotaPark NOTIFY selectedPotaParkChanged)
    Q_PROPERTY(QString operatorCall READ operatorCall WRITE setOperatorCall NOTIFY operatorCallChanged)
    Q_PROPERTY(QVariantList operationalMarkers READ operationalMarkers NOTIFY operationalMarkersChanged)
    Q_PROPERTY(QVariantList geographicFeatures READ geographicFeatures NOTIFY geographicFeaturesChanged)
    Q_PROPERTY(QVariantList logbookRows READ logbookRows NOTIFY logbookChanged)
    Q_PROPERTY(int logbookTotal READ logbookTotal NOTIFY logbookChanged)
    Q_PROPERTY(QVariantMap scorecard READ scorecard NOTIFY statisticsChanged)
    Q_PROPERTY(QVariantList chartData READ chartData NOTIFY statisticsChanged)
    Q_PROPERTY(QVariantMap comparison READ comparison NOTIFY statisticsChanged)
    Q_PROPERTY(QVariantList awardProgression READ awardProgression NOTIFY statisticsChanged)
    Q_PROPERTY(QVariantList topStatistics READ topStatistics NOTIFY statisticsChanged)
    Q_PROPERTY(QVariantList periodComparison READ periodComparison NOTIFY statisticsChanged)
    Q_PROPERTY(QVariantList profileStatistics READ profileStatistics NOTIFY statisticsChanged)
    Q_PROPERTY(QString statisticsDrilldown READ statisticsDrilldown NOTIFY logbookFiltersChanged)
    Q_PROPERTY(QStringList availableProjections READ availableProjections CONSTANT)
    Q_PROPERTY(QStringList availableDataViews READ availableDataViews CONSTANT)
    Q_PROPERTY(QStringList mapPresets READ mapPresets NOTIFY mapPresetsChanged)
    Q_PROPERTY(QString mapProjection READ mapProjection WRITE setMapProjection NOTIFY mapProjectionChanged)
    Q_PROPERTY(QString dataViewMode READ dataViewMode WRITE setDataViewMode NOTIFY dataViewModeChanged)
    Q_PROPERTY(QString activeMapPreset READ activeMapPreset NOTIFY activeMapPresetChanged)
    Q_PROPERTY(QString logbookSearch READ logbookSearch WRITE setLogbookSearch NOTIFY logbookFiltersChanged)
    Q_PROPERTY(QString logbookBand READ logbookBand WRITE setLogbookBand NOTIFY logbookFiltersChanged)
    Q_PROPERTY(QString logbookMode READ logbookMode WRITE setLogbookMode NOTIFY logbookFiltersChanged)
    Q_PROPERTY(QString logbookPeriod READ logbookPeriod WRITE setLogbookPeriod NOTIFY logbookFiltersChanged)
    Q_PROPERTY(QString logbookSort READ logbookSort WRITE setLogbookSort NOTIFY logbookFiltersChanged)
    Q_PROPERTY(bool logbookSortDescending READ logbookSortDescending WRITE setLogbookSortDescending NOTIFY logbookFiltersChanged)
    Q_PROPERTY(int logbookLimit READ logbookLimit WRITE setLogbookLimit NOTIFY logbookFiltersChanged)
    Q_PROPERTY(bool logbookLoading READ logbookLoading NOTIFY logbookLoadingChanged)
    Q_PROPERTY(bool geographicLoading READ geographicLoading NOTIFY geographicLoadingChanged)
    Q_PROPERTY(bool potaLoading READ potaLoading NOTIFY potaLoadingChanged)
    Q_PROPERTY(bool exportInProgress READ exportInProgress NOTIFY exportInProgressChanged)
    Q_PROPERTY(QString rotatorHost READ rotatorHost WRITE setRotatorHost NOTIFY rotatorSettingsChanged)
    Q_PROPERTY(int rotatorPort READ rotatorPort WRITE setRotatorPort NOTIFY rotatorSettingsChanged)
    Q_PROPERTY(bool rotatorEnabled READ rotatorEnabled WRITE setRotatorEnabled NOTIFY rotatorSettingsChanged)
    Q_PROPERTY(QString rotatorProtocol READ rotatorProtocol WRITE setRotatorProtocol NOTIFY rotatorSettingsChanged)
    Q_PROPERTY(QString rotatorTransport READ rotatorTransport NOTIFY rotatorSettingsChanged)
    Q_PROPERTY(QStringList rotatorProtocols READ rotatorProtocols CONSTANT)
    Q_PROPERTY(bool rotatorFeedbackAvailable READ rotatorFeedbackAvailable NOTIFY rotatorFeedbackChanged)
    Q_PROPERTY(qint64 rotatorLastFeedbackMs READ rotatorLastFeedbackMs NOTIFY rotatorFeedbackChanged)
    Q_PROPERTY(double rotatorCurrentAzimuth READ rotatorCurrentAzimuth NOTIFY rotatorFeedbackChanged)
    Q_PROPERTY(double rotatorCurrentElevation READ rotatorCurrentElevation NOTIFY rotatorFeedbackChanged)
    Q_PROPERTY(double rotatorTargetAzimuth READ rotatorTargetAzimuth NOTIFY rotatorTargetChanged)
    Q_PROPERTY(double rotatorTargetElevation READ rotatorTargetElevation NOTIFY rotatorTargetChanged)
    Q_PROPERTY(bool rotatorTracking READ rotatorTracking NOTIFY rotatorTrackingChanged)
    Q_PROPERTY(int rotatorTrackingIntervalMs READ rotatorTrackingIntervalMs WRITE setRotatorTrackingIntervalMs NOTIFY rotatorSettingsChanged)
    Q_PROPERTY(bool rotatorSafetyEnabled READ rotatorSafetyEnabled WRITE setRotatorSafetyEnabled NOTIFY rotatorSafetyChanged)
    Q_PROPERTY(double rotatorMinAzimuth READ rotatorMinAzimuth WRITE setRotatorMinAzimuth NOTIFY rotatorSafetyChanged)
    Q_PROPERTY(double rotatorMaxAzimuth READ rotatorMaxAzimuth WRITE setRotatorMaxAzimuth NOTIFY rotatorSafetyChanged)
    Q_PROPERTY(double rotatorMinElevation READ rotatorMinElevation WRITE setRotatorMinElevation NOTIFY rotatorSafetyChanged)
    Q_PROPERTY(double rotatorMaxElevation READ rotatorMaxElevation WRITE setRotatorMaxElevation NOTIFY rotatorSafetyChanged)
    Q_PROPERTY(bool rotatorParkOnStop READ rotatorParkOnStop WRITE setRotatorParkOnStop NOTIFY rotatorSafetyChanged)
    Q_PROPERTY(double rotatorParkAzimuth READ rotatorParkAzimuth WRITE setRotatorParkAzimuth NOTIFY rotatorSafetyChanged)
    Q_PROPERTY(double rotatorParkElevation READ rotatorParkElevation WRITE setRotatorParkElevation NOTIFY rotatorSafetyChanged)
    Q_PROPERTY(QString rotatorStatus READ rotatorStatus NOTIFY rotatorStatusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString lastExportPath READ lastExportPath NOTIFY lastExportPathChanged)
    Q_PROPERTY(bool offlineMode READ offlineMode WRITE setOfflineMode NOTIFY offlineModeChanged)

public:
    explicit MapOperationsService(const QString& databasePath,
                                  MapLayerModel* layerModel,
                                  QObject* parent = nullptr);
    ~MapOperationsService() override;

    QVariantList potaSpots() const { return m_potaSpots; }
    QVariantMap selectedPotaPark() const { return m_selectedPotaPark; }
    QString operatorCall() const { return m_operatorCall; }
    QVariantList operationalMarkers() const { return m_operationalMarkers; }
    QVariantList geographicFeatures() const { return m_geographicFeatures; }
    QVariantList logbookRows() const { return m_logbookRows; }
    int logbookTotal() const { return m_logbookTotal; }
    QVariantMap scorecard() const { return m_scorecard; }
    QVariantList chartData() const { return m_chartData; }
    QVariantMap comparison() const { return m_comparison; }
    QVariantList awardProgression() const { return m_awardProgression; }
    QVariantList topStatistics() const { return m_topStatistics; }
    QVariantList periodComparison() const { return m_periodComparison; }
    QVariantList profileStatistics() const { return m_profileStatistics; }
    QString statisticsDrilldown() const { return m_statisticsDrilldown; }
    QStringList availableProjections() const;
    QStringList availableDataViews() const;
    QStringList mapPresets() const { return m_mapPresets; }
    QString mapProjection() const { return m_mapProjection; }
    QString dataViewMode() const { return m_dataViewMode; }
    QString activeMapPreset() const { return m_activeMapPreset; }
    QString logbookSearch() const { return m_logbookSearch; }
    QString logbookBand() const { return m_logbookBand; }
    QString logbookMode() const { return m_logbookMode; }
    QString logbookPeriod() const { return m_logbookPeriod; }
    QString logbookSort() const { return m_logbookSort; }
    bool logbookSortDescending() const { return m_logbookSortDescending; }
    int logbookLimit() const { return m_logbookLimit; }
    bool logbookLoading() const { return m_logbookLoading; }
    bool geographicLoading() const { return m_geographicLoading; }
    bool potaLoading() const { return m_potaLoading; }
    bool exportInProgress() const { return m_exportInProgress; }
    QString rotatorHost() const { return m_rotatorHost; }
    int rotatorPort() const { return m_rotatorPort; }
    bool rotatorEnabled() const { return m_rotatorEnabled; }
    QString rotatorProtocol() const { return m_rotatorProtocol; }
    QString rotatorTransport() const;
    QStringList rotatorProtocols() const;
    bool rotatorFeedbackAvailable() const;
    qint64 rotatorLastFeedbackMs() const;
    double rotatorCurrentAzimuth() const;
    double rotatorCurrentElevation() const;
    double rotatorTargetAzimuth() const;
    double rotatorTargetElevation() const;
    bool rotatorTracking() const;
    int rotatorTrackingIntervalMs() const;
    bool rotatorSafetyEnabled() const;
    double rotatorMinAzimuth() const;
    double rotatorMaxAzimuth() const;
    double rotatorMinElevation() const;
    double rotatorMaxElevation() const;
    bool rotatorParkOnStop() const;
    double rotatorParkAzimuth() const;
    double rotatorParkElevation() const;
    QString rotatorStatus() const { return m_rotatorStatus; }
    QString statusMessage() const { return m_statusMessage; }
    QString lastExportPath() const { return m_lastExportPath; }
    bool offlineMode() const { return m_offlineMode; }

    void setMapProjection(const QString& projection);
    void setDataViewMode(const QString& mode);
    void setLogbookSearch(const QString& value);
    void setLogbookBand(const QString& value);
    void setLogbookMode(const QString& value);
    void setLogbookPeriod(const QString& value);
    void setLogbookSort(const QString& value);
    void setLogbookSortDescending(bool descending);
    void setLogbookLimit(int limit);
    void setRotatorHost(const QString& host);
    void setRotatorPort(int port);
    void setRotatorEnabled(bool enabled);
    void setRotatorProtocol(const QString& protocol);
    void setRotatorTrackingIntervalMs(int intervalMs);
    void setRotatorSafetyEnabled(bool enabled);
    void setRotatorMinAzimuth(double value);
    void setRotatorMaxAzimuth(double value);
    void setRotatorMinElevation(double value);
    void setRotatorMaxElevation(double value);
    void setRotatorParkOnStop(bool enabled);
    void setRotatorParkAzimuth(double value);
    void setRotatorParkElevation(double value);
    void setOperatorCall(const QString& call);
    void setOfflineMode(bool offline);

    Q_INVOKABLE void refreshPota();
    Q_INVOKABLE void selectPotaPark(const QString& reference);
    Q_INVOKABLE void clearSelectedPotaPark();
    Q_INVOKABLE QVariantMap preparePotaAction(
        const QVariantMap& spot,
        const QString& operatorCall = QString()) const;
    Q_INVOKABLE void refreshGeographicFeatures();
    Q_INVOKABLE void refreshIotaCatalog();
    Q_INVOKABLE void refreshLogbook();
    Q_INVOKABLE bool exportLogbook(const QString& path,
                                   const QString& format = QStringLiteral("CSV"));
    Q_INVOKABLE bool exportStatistics(const QString& path,
                                      const QString& format = QStringLiteral("JSON"));
    Q_INVOKABLE QString reserveStatisticsExportPath(
        const QString& format = QStringLiteral("JSON"));
    Q_INVOKABLE void drillDownStatistics(const QString& dimension,
                                         const QString& value);
    Q_INVOKABLE void cycleDataView();
    Q_INVOKABLE void applyMapPreset(const QString& name);
    Q_INVOKABLE void saveMapPreset(const QString& name);
    Q_INVOKABLE void deleteMapPreset(const QString& name);
    Q_INVOKABLE void aimRotator(double azimuth);
    Q_INVOKABLE void aimRotatorWithElevation(double azimuth, double elevation);
    Q_INVOKABLE void aimRotatorAt(double latitude, double longitude,
                                  double homeLatitude, double homeLongitude);
    Q_INVOKABLE void trackRotatorAt(double latitude, double longitude,
                                    double homeLatitude, double homeLongitude);
    Q_INVOKABLE void trackRotatorAtWithElevation(double latitude, double longitude,
                                                 double elevation,
                                                 double homeLatitude, double homeLongitude);
    Q_INVOKABLE void aimRotatorAtWithElevation(double latitude, double longitude,
                                               double elevation,
                                               double homeLatitude, double homeLongitude);
    Q_INVOKABLE void trackRotator(double azimuth, double elevation = 0.0,
                                  bool hasElevation = true);
    Q_INVOKABLE void stopRotator();
    Q_INVOKABLE void parkRotator();
    Q_INVOKABLE QString reserveScreenshotPath();
    Q_INVOKABLE QString reserveLogbookExportPath(
        const QString& format = QStringLiteral("CSV"));

signals:
    void potaSpotsChanged();
    void selectedPotaParkChanged();
    void operatorCallChanged();
    void operationalMarkersChanged();
    void geographicFeaturesChanged();
    void logbookChanged();
    void statisticsChanged();
    void mapPresetsChanged();
    void mapProjectionChanged();
    void dataViewModeChanged();
    void activeMapPresetChanged();
    void logbookFiltersChanged();
    void logbookLoadingChanged();
    void geographicLoadingChanged();
    void potaLoadingChanged();
    void exportInProgressChanged();
    void rotatorSettingsChanged();
    void rotatorFeedbackChanged();
    void rotatorTargetChanged();
    void rotatorTrackingChanged();
    void rotatorSafetyChanged();
    void rotatorStatusChanged();
    void statusMessageChanged();
    void lastExportPathChanged();
    void screenshotPathReserved(const QString& path);
    void offlineModeChanged();

private:
    struct LogbookSnapshot {
        QVariantList rows;
        QVariantList markers;
        QVariantMap scorecard;
        QVariantList chartData;
        QVariantMap comparison;
        QVariantList awardProgression;
        QVariantList topStatistics;
        QVariantList periodComparison;
        QVariantList profileStatistics;
        int total {0};
        QString error;
    };

    struct GeoSnapshot {
        QVariantList features;
        QString error;
    };

    struct IotaSnapshot {
        QVariantList markers;
        QString error;
    };

    struct ExportResult {
        QString path;
        QString error;
        int rows {0};
    };

    void loadSettings();
    void saveSetting(const QString& key, const QVariant& value) const;
    void loadMapPresets();
    void rebuildOperationalMarkers();
    void ensureIotaCatalog();
    void requestIotaCatalog();
    void parseIotaCatalog(const QByteArray& data, bool persist,
                          bool refreshAfterParse);
    void handleIotaCatalogReply(QNetworkReply* reply);
    QString iotaCachePath() const;
    void requestGeoLayer(const QString& layerId, const QUrl& url);
    void handlePotaReply(QNetworkReply* reply);
    void handlePotaParkReply(QNetworkReply* reply);
    void pruneExpiredPotaSpots();
    void handleGeoReply(const QString& layerId, quint64 generation,
                        QNetworkReply* reply);
    void setLogbookLoading(bool loading);
    void setGeographicLoading(bool loading);
    void setPotaLoading(bool loading);
    void setExportInProgress(bool inProgress);
    void setStatusMessage(const QString& message);
    void setRotatorStatus(const QString& message);
    static LogbookSnapshot queryLogbookDatabase(
        const QString& databasePath, const QString& search,
        const QString& band, const QString& mode, const QString& period,
        const QString& sort, bool descending, int limit);
    static GeoSnapshot parseGeoJson(const QByteArray& data,
                                    const QString& layerId);
    static IotaSnapshot parseIotaCatalogData(const QByteArray& data);
    static ExportResult exportLogbookDatabase(
        const QString& databasePath, const QString& path,
        const QString& format, const QString& search,
        const QString& band, const QString& mode, const QString& period,
        const QString& sort, bool descending);
    static QVariantMap markerFromPotaSpot(const QVariantMap& spot);
    static QVariantMap potaSpotState(const QVariantMap& spot,
                                     const QString& operatorCall,
                                     qint64 nowMs);
    static QString normalizedLocalPath(const QString& path);

    QString m_databasePath;
    MapLayerModel* m_layerModel {nullptr};
    QNetworkAccessManager* m_network {nullptr};
    QTimer* m_potaExpiryTimer {nullptr};
    RotatorService* m_rotatorService {nullptr};
    QVariantList m_potaSpots;
    QVariantMap m_selectedPotaPark;
    QString m_operatorCall;
    QVariantList m_potaMarkers;
    QVariantList m_databaseMarkers;
    QVariantList m_iotaCatalogMarkers;
    QVariantList m_operationalMarkers;
    QVariantList m_stateFeatures;
    QVariantList m_countyFeatures;
    QVariantList m_geographicFeatures;
    QVariantList m_logbookRows;
    QVariantMap m_scorecard;
    QVariantList m_chartData;
    QVariantMap m_comparison;
    QVariantList m_awardProgression;
    QVariantList m_topStatistics;
    QVariantList m_periodComparison;
    QVariantList m_profileStatistics;
    QString m_statisticsDrilldown;
    QHash<QString, QByteArray> m_geoCache;
    QStringList m_mapPresets;
    QString m_mapProjection {QStringLiteral("Equirectangular")};
    QString m_dataViewMode {QStringLiteral("Live + Logbook")};
    QString m_activeMapPreset {QStringLiteral("Operational")};
    QString m_logbookSearch;
    QString m_logbookBand {QStringLiteral("All")};
    QString m_logbookMode {QStringLiteral("All")};
    QString m_logbookPeriod {QStringLiteral("All time")};
    QString m_logbookSort {QStringLiteral("Date")};
    bool m_logbookSortDescending {true};
    int m_logbookLimit {500};
    int m_logbookTotal {0};
    bool m_logbookLoading {false};
    bool m_geographicLoading {false};
    bool m_potaLoading {false};
    bool m_iotaLoading {false};
    bool m_exportInProgress {false};
    bool m_offlineMode {false};
    QString m_rotatorHost {QStringLiteral("127.0.0.1")};
    int m_rotatorPort {12000};
    bool m_rotatorEnabled {false};
    QString m_rotatorProtocol {QStringLiteral("PSTRotator")};
    int m_rotatorTrackingIntervalMs {1000};
    bool m_rotatorSafetyEnabled {true};
    double m_rotatorMinAzimuth {0.0};
    double m_rotatorMaxAzimuth {360.0};
    double m_rotatorMinElevation {0.0};
    double m_rotatorMaxElevation {180.0};
    bool m_rotatorParkOnStop {false};
    double m_rotatorParkAzimuth {0.0};
    double m_rotatorParkElevation {0.0};
    QString m_rotatorStatus {QStringLiteral("Rotator disabled")};
    QString m_statusMessage;
    QString m_lastExportPath;
    std::atomic<quint64> m_logbookGeneration {0};
    std::atomic<quint64> m_geoGeneration {0};
    std::atomic<quint64> m_iotaGeneration {0};
    QHash<QString, quint64> m_geoLayerGeneration;
    QSet<QString> m_geoPendingLayers;
};
