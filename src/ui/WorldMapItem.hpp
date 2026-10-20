#pragma once

#include <QQuickPaintedItem>
#include <QMetaObject>
#include <QPointer>
#include <QVariantList>
#include <QTimer>

#include "widgets/worldmapwidget.h"

class MapExternalOverlayService;
class MapBaseMapService;
class QHoverEvent;

class WorldMapItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(bool gpuAccelerated READ gpuAccelerated CONSTANT)
    Q_PROPERTY(bool lowSpecMode READ lowSpecMode CONSTANT)
    Q_PROPERTY(bool greylineShaderAvailable READ greylineShaderAvailable CONSTANT)

public:
    explicit WorldMapItem(QQuickItem* parent = nullptr);

    bool gpuAccelerated() const { return m_gpuAccelerated; }
    bool lowSpecMode() const { return m_lowSpecMode; }
    // The painter path does not require a shader; expose the same capability
    // flag as WorldMapGpuItem so the QML Loader can use one fallback rule.
    bool greylineShaderAvailable() const { return true; }

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
    Q_INVOKABLE void setLayerStyles(const QVariantMap& styles)
    { Q_UNUSED(styles); }
    Q_INVOKABLE QVariantMap viewportState() const { return {}; }
    Q_INVOKABLE void setViewportState(const QVariantMap& state)
    { Q_UNUSED(state); }
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
    // 1.0.213 — Permette a LiveMapPanel di sospendere l'animation timer
    // interno del widget legacy quando il pannello non e' visibile
    // (Pop minimizzato, tab non selezionato, finestra detach nascosta).
    Q_INVOKABLE void setActive(bool active);

    // 1.0.221 — Stub di compatibilita' con WorldMapGpuItem per uniformare
    // il toolbar QML. Il widget CPU legacy gia' applica auto-fit + smooth
    // viewport; il zoom manuale non e' supportato lato CPU per ora (vedi
    // worldmapwidget.cpp). I metodi ritornano early senza effetto.
    Q_INVOKABLE void zoomIn(double factor = 1.5) { Q_UNUSED(factor); }
    Q_INVOKABLE void zoomOut(double factor = 1.5) { Q_UNUSED(factor); }
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void panBy(double deltaLonDeg, double deltaLatDeg)
    { Q_UNUSED(deltaLonDeg); Q_UNUSED(deltaLatDeg); }
    Q_INVOKABLE void focusLocation(double longitude, double latitude,
                                   double spanLongitude = 90.0,
                                   double spanLatitude = 54.0);
    Q_INVOKABLE bool greylineEnabled() const { return true; }

    void paint(QPainter* painter) override;

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

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void itemChange(ItemChange change, const ItemChangeData& data) override;

private:
    static WorldMapWidget::PathRole pathRoleFromInt(int role);
    void syncWidgetSize();
    void markDirty();

    // 1.0.213 — Rileva GPU/CPU/RAM e configura render target + intervalli.
    // Chiamata una sola volta quando il QQuickItem ottiene la finestra
    // (il backend RHI e' affidabile solo dopo windowChanged).
    void configureForHardware();
    void applyAnimationCadence(bool visible);
    void handlePaintProfile(double paintMs, double paintAvgMs,
                            double greylineMs, double greylineAvgMs,
                            int contactsCount, bool cacheRebuild);
    void setRepaintIntervalMs(int ms, const QString& reason,
                              double paintMs = 0.0, double paintAvgMs = 0.0,
                              double steadyPaintMs = 0.0, double steadyPaintAvgMs = 0.0,
                              int contactsCount = 0, bool cacheRebuild = false);

    WorldMapWidget m_widget;
    QPointer<MapBaseMapService> m_baseMapService;
    QMetaObject::Connection m_baseMapConnection;
    QPointer<MapExternalOverlayService> m_externalOverlayService;
    QMetaObject::Connection m_externalOverlayConnection;
    QTimer m_repaintTimer;
    bool m_dirty {true};
    bool m_hardwareConfigured {false};
    bool m_gpuAccelerated {false};
    bool m_lowSpecMode {false};
    bool m_userActive {true};
    QString m_hoveredCoverageGrid;
    QVariantMap m_hoveredGeographicFeature;
    int m_baseRepaintIntervalMs {250};
    int m_repaintIntervalMs {250};
    int m_animationIntervalActiveMs {60};
    int m_fastPaintProfileCount {0};
};
