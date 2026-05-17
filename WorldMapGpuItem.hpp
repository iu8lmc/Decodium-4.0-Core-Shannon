#pragma once

#include <QHash>
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QQuickItem>
#include <QRectF>
#include <QTimer>
#include <QVector>

class WorldMapGpuItem : public QQuickItem
{
    Q_OBJECT

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
    Q_INVOKABLE void setGreylineEnabled(bool enabled);
    Q_INVOKABLE void setDistanceInMiles(bool enabled);
    Q_INVOKABLE void setTransmitState(bool transmitting,
                                      const QString& targetCall,
                                      const QString& targetGrid,
                                      const QString& mode);
    Q_INVOKABLE void clearContacts();
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

Q_SIGNALS:
    void contactClicked(const QString& call, const QString& grid);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
    void mousePressEvent(QMouseEvent* event) override;
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
        QVector<QPointF> gridLines;
        QVector<PathLine> genericPaths;
        QVector<PathLine> incomingPaths;
        QVector<PathLine> outgoingPaths;
        QVector<QPointF> genericMarkers;
        QVector<QPointF> incomingMarkers;
        QVector<QPointF> outgoingMarkers;
        QVector<QPointF> bandMarkers;
    };

    struct Label {
        QString text;
        QPointF baseline;
        QRectF rect;
        QColor color;
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

    QPointF projectLonLatToPoint(const QPointF& lonLat) const;
    QRectF mapRect() const;
    bool computeCircularLongitudeBounds(const QVector<double>& longitudes, double* centerLon, double* spanLon) const;
    void updateViewportTargets();
    bool smoothViewport();
    void rebuildGeometryBatch();
    void layoutLabels(const QRectF& rect);
    void configureRendererPolicy();
    bool pruneExpiredContacts();
    void trimContactsToLimit();
    void markDirty();

    QImage m_mapImage;
    QHash<QString, Contact> m_contacts;
    BatchedGeometry m_batch;
    QVector<Label> m_labels;
    QVector<AnimatedPath> m_animatedPaths;
    QString m_homeGrid;
    QString m_txTargetCall;
    QString m_txTargetGrid;
    QPointF m_homeLonLat;
    qint64 m_txStartMs {0};
    qint64 m_lastProfileLogMs {0};
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
    bool m_greylineEnabled {true};
    bool m_distanceInMiles {false};
    bool m_active {true};
    bool m_transmitting {false};
    bool m_geometryDirty {true};
    bool m_greylineGeometryDirty {true};
    bool m_rendererPolicyInitialized {false};
    bool m_conservativeRenderer {false};
    bool m_greylineShaderAllowed {true};
    bool m_loggedFirstFrame {false};
    bool m_loggedFirstProfile {false};
    int m_lastContactCount {0};
    int m_lastLineVertexCount {0};
    int m_lastMarkerVertexCount {0};
    int m_lastLabelCount {0};
    int m_lastVisiblePathCount {0};
    int m_lastVisibleBandCount {0};
    QTimer m_frameTimer;
};
