#pragma once

#include <QHash>
#include <QColor>
#include <QImage>
#include <QMetaObject>
#include <QPointF>
#include <QPointer>
#include <QQuickItem>
#include <QRectF>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class MapExternalOverlayService;
class MapBaseMapService;
class QHoverEvent;

class WorldMapGpuItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool greylineShaderAvailable READ greylineShaderAvailable CONSTANT)

public:
    enum class PathRole {
        Generic = 0,
        IncomingToMe = 1,
        OutgoingFromMe = 2,
        BandOnly = 3
    };

    struct PathLine {
        QPointF sourceLonLat;
        QPointF destinationLonLat;
    };

    explicit WorldMapGpuItem(QQuickItem* parent = nullptr);

    Q_INVOKABLE void setHomeGrid(const QString& grid);
    Q_INVOKABLE void setBaseMapEnabled(bool enabled);
    Q_INVOKABLE void setGreylineEnabled(bool enabled);
    Q_INVOKABLE void setDistanceInMiles(bool enabled);
    Q_INVOKABLE void setTransmitState(bool transmitting,
                                      const QString& targetCall,
                                      const QString& targetGrid,
                                      const QString& mode);
    Q_INVOKABLE void clearContacts();
    Q_INVOKABLE void setCoverageCells(const QVariantList& cells);
    Q_INVOKABLE void setCoveragePushPins(bool enabled);
    Q_INVOKABLE void setTimeZoneOverlayEnabled(bool enabled);
    Q_INVOKABLE void setOperationalMarkers(const QVariantList& markers);
    Q_INVOKABLE void setGeographicFeatures(const QVariantList& features);
    Q_INVOKABLE void setProjection(const QString& projection);
    Q_INVOKABLE void setLayerStyles(const QVariantMap& styles);
    Q_INVOKABLE QVariantMap viewportState() const;
    Q_INVOKABLE void setViewportState(const QVariantMap& state);
    Q_INVOKABLE void setBaseMapService(QObject* service);
    Q_INVOKABLE void setExternalOverlayService(QObject* service);
    Q_INVOKABLE void downgradeContactToBand(const QString& call);
    Q_INVOKABLE void addContact(const QString& call,
                                const QString& sourceGrid,
                                const QString& destinationGrid,
                                int role = 0);
    Q_INVOKABLE void addContactByLonLat(const QString& call,
                                        double sourceLon,
                                        double sourceLat,
                                        const QString& destinationGrid,
                                        int role = 0);
    Q_INVOKABLE void setActive(bool active);

    // 1.0.221 — Controllo zoom + pan utente.
    // zoomIn/Out: fattore moltiplicativo sullo span (1.5x in/out).
    // resetView: torna ad auto-fit (riabilita updateViewportTargets).
    // setUserViewport: imposta center+span esatti (per programma esterno).
    // Quando l'utente ha zoomato manualmente, updateViewportTargets non
    // sovrascrive piu' i parametri: m_userViewportLocked = true.
    Q_INVOKABLE void zoomIn(double factor = 1.5);
    Q_INVOKABLE void zoomOut(double factor = 1.5);
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void panBy(double deltaLonDeg, double deltaLatDeg);
    Q_INVOKABLE void focusLocation(double longitude, double latitude,
                                   double spanLongitude = 90.0,
                                   double spanLatitude = 54.0);
    Q_INVOKABLE bool greylineEnabled() const { return m_greylineEnabled; }

    // Lets QML select the painter implementation for a custom build that was
    // made without Qt ShaderTools.  Release builds still use the GPU map, but
    // they no longer fail silently with a missing greyline layer.
    bool greylineShaderAvailable() const
    {
#ifdef DECODIUM_LIVEMAP_GREYLINE_QSB
        return true;
#else
        return false;
#endif
    }

Q_SIGNALS:
    void contactClicked(const QString& call, const QString& grid);
    void coverageCellHovered(const QVariantMap& details, qreal x, qreal y);
    void coverageCellSegmentHovered(const QVariantMap& details, qreal x, qreal y,
                                    const QString& segment);
    void coverageCellHoverEnded();
    void coverageCellClicked(const QVariantMap& details, qreal x, qreal y);
    void operationalMarkerClicked(const QVariantMap& details, qreal x, qreal y);
    void geographicFeatureClicked(const QVariantMap& details, qreal x, qreal y);
    void geographicFeatureHovered(const QVariantMap& details, qreal x, qreal y);
    void geographicFeatureHoverEnded();
    void viewportLockedChanged(bool locked);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    struct Contact {
        QString call;
        QString sourceGrid;
        QString destinationGrid;
        QPointF sourceLonLat;
        QPointF destinationLonLat;
        qint64 lastSeenMonotonicMs {0};
        PathRole role {PathRole::Generic};
        bool queuedDuringTx {false};
    };

    struct BatchedGeometry {
        QVector<QPointF> workedCoverageTriangles;
        QVector<QPointF> confirmedCoverageTriangles;
        QVector<QPointF> activeCoverageTriangles;
        QVector<QPointF> activeCoverageMediumTriangles;
        QVector<QPointF> activeCoverageFadedTriangles;
        QVector<QPointF> missingCoverageTriangles;
        QVector<QPointF> missingCoverageMediumTriangles;
        QVector<QPointF> missingCoverageFadedTriangles;
        QVector<QPointF> pskCoverageTriangles;
        QVector<QPointF> pskCoverageMediumTriangles;
        QVector<QPointF> pskCoverageFadedTriangles;
        QVector<QPointF> workedCoverageLines;
        QVector<QPointF> confirmedCoverageLines;
        QVector<QPointF> activeCoverageLines;
        QVector<QPointF> activeCoverageMediumLines;
        QVector<QPointF> activeCoverageFadedLines;
        QVector<QPointF> missingCoverageLines;
        QVector<QPointF> missingCoverageMediumLines;
        QVector<QPointF> missingCoverageFadedLines;
        QVector<QPointF> pskCoverageLines;
        QVector<QPointF> pskCoverageMediumLines;
        QVector<QPointF> pskCoverageFadedLines;
        QVector<QPointF> gridLines;
        QVector<QPointF> timeZoneLines;
        QVector<PathLine> genericPaths;
        QVector<PathLine> incomingPaths;
        QVector<PathLine> outgoingPaths;
        QVector<QPointF> genericMarkers;
        QVector<QPointF> incomingMarkers;
        QVector<QPointF> outgoingMarkers;
        QVector<QPointF> bandMarkers;
        QVector<QPointF> potaMarkers;
        QVector<QPointF> iotaMarkers;
        QVector<QPointF> wpxMarkers;
        QVector<QPointF> moonMarkers;
        QVector<QPointF> satelliteMarkers;
        // Geographic boundaries have both a filled wide-line path and a thin
        // line fallback. The latter remains visible on renderers that do not
        // reliably update dynamic triangle buffers after a layer is enabled.
        QVector<QPointF> stateBoundaryTriangles;
        QVector<QPointF> countyBoundaryTriangles;
        QVector<QPointF> stateBoundaryLines;
        QVector<QPointF> countyBoundaryLines;
        QVector<QPointF> earthquakeLowMarkers;
        QVector<QPointF> earthquakeMediumMarkers;
        QVector<QPointF> earthquakeHighMarkers;
    };

    struct CoverageCell {
        QString grid;
        int workedCount {0};
        int confirmedCount {0};
        int activeCount {0};
        int pskCount {0};
        QString historicalStatus;
        QString liveStatus;
        qreal liveOpacity {1.0};
        bool split {false};
        bool worked {false};
        bool confirmed {false};
        bool active {false};
        bool missing {false};
        bool psk {false};
    };

    struct Label {
        QString text;
        QPointF baseline;
        QRectF rect;
        QColor color;
        bool persistentCache {true};
    };

    struct AnimatedPath {
        QString key;
        QPointF sourceLonLat;
        QPointF destinationLonLat;
        QVector<QPointF> points;
        PathRole role {PathRole::Generic};
    };

    static PathRole pathRoleFromInt(int role);
    static double wrapLongitude(double lon);
    static bool maidenheadToLonLat(const QString& locator, QPointF* lonLat);
    static QImage loadImageWithFallback(const QStringList& candidates);
    static QImage buildMapTexture();
    static QPointF subSolarLonLat();

    bool azimuthalProjectionEnabled() const;
    QImage azimuthalProjectionImage(const QImage& image) const;
    const QImage& displayedMapImage();
    const QImage& displayedExternalOverlayImage();
    double projectLatitude(double latitude) const;
    QPointF projectLonLatToPoint(const QPointF& lonLat) const;
    QVariantMap operationalMarkerAt(const QPointF& point) const;
    QVariantMap geographicFeatureAt(const QPointF& point) const;
    QVariantMap coverageCellAt(const QPointF& point) const;
    QRectF mapRect() const;
    QColor styledColor(const QString& layerId, const QColor& fallback,
                       int alpha = -1) const;
    double layerThickness(const QString& layerId, double fallback = 1.0) const;
    int layerLabelDensity(const QString& layerId) const;
    bool computeCircularLongitudeBounds(const QVector<double>& longitudes, double* centerLon, double* spanLon) const;
    void updateViewportTargets();
    bool smoothViewport();
    void rebuildGeometryBatch();
    void layoutLabels(const QRectF& rect);
    void configureRendererPolicy();
    bool pruneExpiredContacts();
    void trimContactsToLimit();
    void markDirty(bool contactGeometryChanged = true);

    QImage m_mapImage;
    QImage m_externalOverlayImage;
    QImage m_azimuthalMapImage;
    QImage m_azimuthalExternalOverlayImage;
    QPointer<MapBaseMapService> m_baseMapService;
    QMetaObject::Connection m_baseMapConnection;
    QPointer<MapExternalOverlayService> m_externalOverlayService;
    QMetaObject::Connection m_externalOverlayConnection;
    bool m_baseMapTextureDirty {false};
    QHash<QString, Contact> m_contacts;
    QVector<CoverageCell> m_coverageCells;
    QVariantList m_operationalMarkers;
    QVariantList m_geographicFeatures;
    BatchedGeometry m_batch;
    QVector<Label> m_labels;
    QVector<AnimatedPath> m_animatedPaths;
    QString m_homeGrid;
    QString m_hoveredCoverageGrid;
    QVariantMap m_hoveredGeographicFeature;
    QString m_txTargetCall;
    QString m_txTargetGrid;
    QString m_projection {QStringLiteral("Equirectangular")};
    QVariantMap m_layerStyles;
    QPointF m_homeLonLat;
    qint64 m_txStartMs {0};
    qint64 m_lastProfileLogMs {0};
    qint64 m_lastMapRebuildUs {0};
    qint64 m_lastLabelLayoutUs {0};
    qint64 m_lastLabelTextureCreateUs {0};
    qint64 m_lastMapSyncNodesUs {0};
    int m_txTravelMs {5200};
    int m_frameIntervalMs {80};
    qreal m_animationPhase {0.0};
    double m_viewCenterLon {0.0};
    double m_viewCenterLat {0.0};
    double m_viewSpanLon {360.0};
    double m_viewSpanLat {180.0};
    double m_viewVelocityLon {0.0};
    double m_viewVelocityLat {0.0};
    double m_viewVelocitySpanLon {0.0};
    double m_viewVelocitySpanLat {0.0};
    double m_targetCenterLon {0.0};
    double m_targetCenterLat {0.0};
    double m_targetSpanLon {360.0};
    double m_targetSpanLat {180.0};
    bool m_hasHome {false};
    bool m_baseMapEnabled {true};
    bool m_greylineEnabled {true};
    bool m_coveragePushPins {false};
    bool m_timeZoneOverlayEnabled {false};
    bool m_distanceInMiles {false};
    bool m_active {true};
    bool m_transmitting {false};
    bool m_geometryDirty {true};
    bool m_contactGeometryDirty {true};
    bool m_animationGeometryDirty {true};
    bool m_greylineGeometryDirty {true};
    bool m_externalOverlayTextureDirty {true};
    bool m_azimuthalMapDirty {true};
    bool m_azimuthalExternalOverlayDirty {true};
    bool m_projectionNodeDirty {false};
    bool m_rendererPolicyInitialized {false};
    bool m_conservativeRenderer {false};
    bool m_greylineShaderAllowed {true};
    bool m_loggedFirstFrame {false};
    bool m_loggedFirstProfile {false};
    int m_lastContactCount {0};
    int m_lastLineVertexCount {0};
    int m_lastStateBoundaryVertexCount {0};
    int m_lastCountyBoundaryVertexCount {0};
    int m_lastStateBoundaryLineVertexCount {0};
    int m_lastCountyBoundaryLineVertexCount {0};
    QString m_lastGeographicGeometryDiagnostic;
    QString m_lastGeographicIngressDiagnostic;
    QString m_lastCoverageIngressDiagnostic;
    int m_lastMarkerVertexCount {0};
    int m_lastLabelCount {0};
    int m_lastVisiblePathCount {0};
    int m_lastVisibleBandCount {0};
    QTimer m_frameTimer;
    // 1.0.221 — Zoom + pan utente. Quando true updateViewportTargets()
    // non sovrascrive m_targetCenter/m_targetSpan (smoothViewport continua
    // a interpolare verso i target settati manualmente). resetView()
    // riabilita l'auto-fit.
    bool m_userViewportLocked {false};
    bool m_panActive {false};
    QPointF m_panLastPos;
};
