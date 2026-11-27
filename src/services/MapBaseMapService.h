#pragma once

#include <QImage>
#include <QHash>
#include <QObject>
#include <QSize>
#include <QStringList>
#include <QThreadPool>

class QNetworkAccessManager;

// Owns the optional cartographic base layer. The radio/map overlays continue
// to use MapExternalOverlayService so switching imagery never affects them.
// Network requests, cache I/O and raster transformations are asynchronous.
class MapBaseMapService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList availableProviders READ availableProviders CONSTANT)
    Q_PROPERTY(QStringList availableStyles READ availableStyles CONSTANT)
    Q_PROPERTY(QString provider READ provider WRITE setProvider NOTIFY providerChanged)
    Q_PROPERTY(QString activeProvider READ activeProvider NOTIFY activeProviderChanged)
    Q_PROPERTY(QStringList fallbackProviders READ fallbackProviders WRITE setFallbackProviders NOTIFY fallbackProvidersChanged)
    Q_PROPERTY(QString style READ style WRITE setStyle NOTIFY styleChanged)
    Q_PROPERTY(bool fallbackActive READ fallbackActive NOTIFY fallbackActiveChanged)
    Q_PROPERTY(bool staleCache READ staleCache NOTIFY cacheStateChanged)
    Q_PROPERTY(int cacheMaxAgeDays READ cacheMaxAgeDays WRITE setCacheMaxAgeDays NOTIFY cacheStateChanged)
    Q_PROPERTY(bool offlineMode READ offlineMode WRITE setOfflineMode NOTIFY offlineModeChanged)
    Q_PROPERTY(QString mapTilerApiKey READ mapTilerApiKey WRITE setMapTilerApiKey NOTIFY mapTilerApiKeyChanged)
    Q_PROPERTY(bool apiKeyRequired READ apiKeyRequired NOTIFY providerChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString attribution READ attribution NOTIFY activeProviderChanged)
    Q_PROPERTY(QString attributionUrl READ attributionUrl NOTIFY activeProviderChanged)
    Q_PROPERTY(bool offlinePackAvailable READ offlinePackAvailable NOTIFY offlinePackChanged)
    Q_PROPERTY(QString offlinePackPath READ offlinePackPath NOTIFY offlinePackChanged)
    Q_PROPERTY(QString offlinePackStatus READ offlinePackStatus NOTIFY offlinePackChanged)

public:
    explicit MapBaseMapService(QObject* parent = nullptr,
                               const QString& cachePath = {});
    ~MapBaseMapService() override;

    QStringList availableProviders() const;
    QStringList availableStyles() const;
    QString provider() const { return m_provider; }
    QString activeProvider() const { return m_activeProvider; }
    QStringList fallbackProviders() const { return m_fallbackProviders; }
    QString style() const { return m_style; }
    bool fallbackActive() const { return !m_offlineMode && m_activeProvider != m_provider; }
    bool staleCache() const { return m_staleCache; }
    int cacheMaxAgeDays() const { return m_cacheMaxAgeDays; }
    bool offlineMode() const { return m_offlineMode; }
    QString mapTilerApiKey() const { return m_mapTilerApiKey; }
    bool apiKeyRequired() const;
    bool loading() const { return m_loading; }
    QString status() const { return m_status; }
    QString attribution() const;
    QString attributionUrl() const;
    bool offlinePackAvailable() const { return m_offlinePackAvailable; }
    QString offlinePackPath() const { return m_offlinePackPath; }
    QString offlinePackStatus() const { return m_offlinePackStatus; }
    QImage baseMapImage() const { return m_currentImage; }

    void setProvider(const QString& provider);
    void setFallbackProviders(const QStringList& providers);
    void setStyle(const QString& style);
    void setCacheMaxAgeDays(int days);
    void setOfflineMode(bool offline);
    void setMapTilerApiKey(const QString& apiKey);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void invalidateCache();
    Q_INVOKABLE void importOfflinePack(const QString& path);
    Q_INVOKABLE void clearOfflinePack();

signals:
    void baseMapImageChanged();
    void providerChanged();
    void activeProviderChanged();
    void fallbackActiveChanged();
    void fallbackProvidersChanged();
    void styleChanged();
    void cacheStateChanged();
    void offlineModeChanged();
    void mapTilerApiKeyChanged();
    void loadingChanged();
    void statusChanged();
    void offlinePackChanged();

private:
    static QString normalizeProvider(const QString& provider);
    static QString normalizeStyle(const QString& style);
    static QStringList defaultFallbackProviders();
    static QImage loadLocalAtlas();
    static QImage webMercatorToEquirectangular(const QImage& source,
                                               const QSize& outputSize);
    static QImage applyStyle(const QImage& source, const QString& style);
    static QImage readOfflinePack(const QString& path, QString* error);
    static QString tileUrl(const QString& provider, int zoom, int x, int y,
                           const QString& apiKey);

    QStringList candidateProviders() const;
    QString providerCacheSignature(const QString& provider) const;
    QString cacheFilePath(const QString& provider) const;
    QString cacheMetadataPath(const QString& provider) const;
    void setLoading(bool loading);
    void setStatus(const QString& status);
    void setActiveProvider(const QString& provider);
    void applyLocalAtlas(const QString& status, int generation);
    void applyImageAsync(const QImage& source, const QString& provider,
                         const QString& status, int generation,
                         bool cacheImage);
    void processImagePayloadAsync(const QByteArray& payload,
                                  const QString& provider,
                                  const QString& status,
                                  int generation,
                                  int fallbackIndex,
                                  bool webMercator,
                                  bool cacheImage);
    void applyProcessedImage(const QImage& image, const QString& provider,
                             const QString& status, int generation,
                             bool staleCache, bool fromCache = false);
    void loadCachedOnlineImage(int generation);
    void requestCandidate(int fallbackIndex, int generation);
    void providerFailed(const QString& provider, const QString& error,
                        int fallbackIndex, int generation);
    void requestNasaGibs(const QString& provider, int fallbackIndex,
                         int generation);
    void requestGebcoBathymetry(const QString& provider, int fallbackIndex,
                                int generation);
    void requestXyzTiles(const QString& provider, int fallbackIndex,
                         int generation);
    void finishXyzRequest(const QString& provider, int fallbackIndex,
                          int generation);
    void loadOfflinePackAsync();
    void applyOfflinePack(const QString& status, int generation);

    QNetworkAccessManager* m_network {nullptr};
    QThreadPool m_workerPool;
    QImage m_localAtlas;
    QImage m_currentImage;
    QString m_provider {QStringLiteral("Decodium Atlas")};
    QString m_activeProvider {QStringLiteral("Decodium Atlas")};
    QStringList m_fallbackProviders;
    QString m_style {QStringLiteral("Day")};
    QString m_mapTilerApiKey;
    QString m_status;
    QString m_cachePath;
    QString m_offlinePackPath;
    QString m_offlinePackStatus;
    QImage m_offlinePackImage;
    quint64 m_offlinePackGeneration {0};
    bool m_offlinePackAvailable {false};
    bool m_offlineMode {false};
    bool m_loading {false};
    bool m_staleCache {false};
    int m_cacheMaxAgeDays {7};
    int m_generation {0};
    int m_tileExpected {0};
    int m_tileCompleted {0};
    int m_tileFailures {0};
    int m_tileZoom {2};
    int m_fallbackIndex {0};
    QHash<QString, QImage> m_tiles;
};
