#pragma once

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QThreadPool>
#include <QTimer>
#include <QVariantList>

class MapLayerModel;
class QNetworkAccessManager;
class QNetworkReply;

class MapExternalOverlayService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList providerStatus READ providerStatus NOTIFY providerStatusChanged)
    Q_PROPERTY(QVariantList temporalLegend READ temporalLegend NOTIFY providerStatusChanged)
    Q_PROPERTY(bool offlineMode READ offlineMode WRITE setOfflineMode NOTIFY offlineModeChanged)
    Q_PROPERTY(QVariantList earthquakeFeatures READ earthquakeFeatures NOTIFY earthquakeFeaturesChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool hasOverlay READ hasOverlay NOTIFY overlayImageChanged)
    Q_PROPERTY(bool moonDataAvailable READ moonDataAvailable NOTIFY moonDataChanged)
    Q_PROPERTY(double moonAzimuth READ moonAzimuth NOTIFY moonDataChanged)
    Q_PROPERTY(double moonElevation READ moonElevation NOTIFY moonDataChanged)
    Q_PROPERTY(double moonDistanceKm READ moonDistanceKm NOTIFY moonDataChanged)
    Q_PROPERTY(double moonIllumination READ moonIllumination NOTIFY moonDataChanged)
    Q_PROPERTY(double moonSublunarLatitude READ moonSublunarLatitude NOTIFY moonDataChanged)
    Q_PROPERTY(double moonSublunarLongitude READ moonSublunarLongitude NOTIFY moonDataChanged)

public:
    explicit MapExternalOverlayService(MapLayerModel* layerModel,
                                       QObject* parent = nullptr,
                                       const QString& cachePath = {});
    ~MapExternalOverlayService() override;

    QVariantList providerStatus() const;
    QVariantList temporalLegend() const;
    bool offlineMode() const { return m_offlineMode; }
    QVariantList earthquakeFeatures() const { return m_earthquakeFeatures; }
    bool loading() const { return m_loadingCount > 0; }
    bool hasOverlay() const { return !m_overlayImage.isNull(); }
    QImage overlayImage() const { return m_overlayImage; }
    bool moonDataAvailable() const { return m_moonDataAvailable; }
    double moonAzimuth() const { return m_moonAzimuth; }
    double moonElevation() const { return m_moonElevation; }
    double moonDistanceKm() const { return m_moonDistanceKm; }
    double moonIllumination() const { return m_moonIllumination; }
    double moonSublunarLatitude() const { return m_moonSublunarLatitude; }
    double moonSublunarLongitude() const { return m_moonSublunarLongitude; }

    Q_INVOKABLE void refreshAll();
    Q_INVOKABLE void refreshLayer(const QString& layerId);
    Q_INVOKABLE void setOfflineMode(bool offline);
    Q_INVOKABLE void updateMoonForStation(double stationLatitude,
                                          double stationLongitude);
    Q_INVOKABLE void setMoonData(bool enabled,
                                 double stationLatitude,
                                 double stationLongitude,
                                 double azimuthDegrees,
                                 double elevationDegrees,
                                 double distanceKm,
                                 double illuminationPercent);

    static QImage webMercatorToEquirectangular(const QImage& source,
                                               const QSize& outputSize = QSize(1024, 512));
    static QImage renderMoonOverlay(double stationLatitude,
                                    double stationLongitude,
                                    double azimuthDegrees,
                                    double elevationDegrees,
                                    double distanceKm,
                                    double illuminationPercent,
                                    const QSize& outputSize = QSize(1024, 512));
    static QImage renderTropoPayload(const QByteArray& payload,
                                     int* featureCount = nullptr,
                                     QString* error = nullptr,
                                     const QSize& outputSize = QSize(1024, 512));
    static QImage renderEarthquakePayload(
        const QByteArray& payload,
        int* featureCount = nullptr,
        QString* error = nullptr,
        const QSize& outputSize = QSize(1024, 512));
    static QImage renderWildfirePayload(
        const QByteArray& payload,
        int* featureCount = nullptr,
        QString* error = nullptr,
        const QSize& outputSize = QSize(1024, 512));

signals:
    void providerStatusChanged();
    void offlineModeChanged();
    void overlayImageChanged();
    void loadingChanged();
    void moonDataChanged();
    void earthquakeFeaturesChanged();

private:
    struct Provider {
        QString id;
        QString label;
        QString attribution;
        QString attributionUrl;
        QString sourceUrl;
        QString error;
        qint64 updatedMs {0};
        qint64 validUntilMs {0};
        int refreshSeconds {900};
        int validitySeconds {1800};
        int opacityPercent {65};
        int itemCount {0};
        int generation {0};
        int appliedGeneration {0};
        bool loading {false};
        bool enabled {false};
        bool derived {false};
        QTimer* timer {nullptr};
    };

    struct ProcessedPayload {
        QImage image;
        QVariantList features;
        QString error;
        int itemCount {0};
    };

    static ProcessedPayload processPayload(const QString& layerId,
                                           const QByteArray& payload);
    static QVariantList parseEarthquakeFeatures(const QByteArray& payload,
                                                QString* error = nullptr);
    static QString providerUrl(const QString& layerId, int fallbackOffset = 0);
    static QString normalizedLayerId(const QString& value);

    void setLayerEnabled(const QString& layerId, bool enabled);
    void setLayerStyle(const QString& layerId);
    void requestProvider(const QString& layerId, int fallbackOffset = 0);
    void requestRainViewerTile(const QString& layerId,
                               int generation,
                               const QByteArray& metadata);
    void handleReply(QNetworkReply* reply);
    void processPayloadAsync(const QString& layerId,
                             const QByteArray& payload,
                             int generation,
                             bool fromCache);
    void applyProcessedPayload(const QString& layerId,
                               int generation,
                               ProcessedPayload result,
                               bool fromCache);
    void loadCache(const QString& layerId);
    void saveCache(const QString& layerId, const QByteArray& payload) const;
    void rebuildComposite();
    void updateProviderStatus();
    void refreshProviderAges();
    void setProviderLoading(Provider& provider, bool loading);
    QString cacheFilePath(const QString& layerId) const;

    MapLayerModel* m_layerModel {nullptr};
    QNetworkAccessManager* m_network {nullptr};
    QThreadPool m_workerPool;
    QTimer m_ageTimer;
    QHash<QString, Provider> m_providers;
    QHash<QString, QImage> m_providerImages;
    QVariantList m_earthquakeFeatures;
    QImage m_overlayImage;
    QString m_cachePath;
    int m_loadingCount {0};
    int m_moonGeneration {0};
    bool m_moonEnabled {false};
    bool m_offlineMode {false};
    bool m_moonDataAvailable {false};
    double m_moonAzimuth {0.0};
    double m_moonElevation {0.0};
    double m_moonDistanceKm {0.0};
    double m_moonIllumination {0.0};
    double m_moonSublunarLatitude {0.0};
    double m_moonSublunarLongitude {0.0};
};
