#ifndef WORLDMAPWIDGET_H
#define WORLDMAPWIDGET_H

#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QPixmap>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QPaintEvent;
class QPainter;
class QRectF;

class WorldMapWidget final : public QWidget
{
  Q_OBJECT

public:
  enum class PathRole
  {
    Generic,
    IncomingToMe,
    OutgoingFromMe,
    BandOnly
  };

  explicit WorldMapWidget(QWidget * parent = nullptr);
  void setHomeGrid(QString const& grid);
  void setBaseMapEnabled(bool enabled);
  void setGreylineEnabled(bool enabled);
  void setDistanceInMiles(bool enabled);
  void setTransmitState(bool transmitting, QString const& targetCall, QString const& targetGrid, QString const& mode);
  void clearContacts();
  void setCoverageCells(QVariantList const& cells);
  void setCoveragePushPins(bool enabled);
  void setTimeZoneOverlayEnabled(bool enabled);
  void setOperationalMarkers(QVariantList const& markers);
  void setGeographicFeatures(QVariantList const& features);
  void setProjection(QString const& projection);
  void setBaseMapImage(QImage const& image);
  void setExternalOverlayImage(QImage const& image);
  void downgradeContactToBand(QString const& call);
  void addContact(QString const& call, QString const& sourceGrid, QString const& destinationGrid,
                  PathRole role = PathRole::Generic);
  void addContactByLonLat(QString const& call, QPointF const& sourceLonLat, QString const& destinationGrid,
                          PathRole role = PathRole::Generic);
  void handleMapClick(QPointF const& clickPos);  // called from MainWindow's eventFilter
  QVariantMap coverageCellAt(QPointF const& point) const;
  QVariantMap operationalMarkerAt(QPointF const& point) const;
  QVariantMap geographicFeatureAt(QPointF const& point) const;

  // 1.0.213/215 — animation timer (greyline pulse + tx travel arc)
  // controllabile dal QQuickItem host: pausa quando il pannello non e'
  // visibile, interval sincronizzato al repaint cap effettivo.
  void setAnimationActive(bool active);
  void setAnimationInterval(int ms);
  void focusLocation(double longitude, double latitude,
                     double spanLongitude = 90.0,
                     double spanLatitude = 54.0);
  void resetView();

signals:
  void contactClicked(QString const& call, QString const& grid);
  void operationalMarkerClicked(QVariantMap const& details, qreal x, qreal y);
  void geographicFeatureClicked(QVariantMap const& details, qreal x, qreal y);
  void paintProfileUpdated(double paintMs, double paintAvgMs,
                           double greylineMs, double greylineAvgMs,
                           int contactsCount, bool cacheRebuild);
  // 1.0.214 — emit ad ogni tick del m_animationTimer interno cosi' il
  // QQuickPaintedItem host (WorldMapItem) puo' propagare la richiesta di
  // repaint al scene-graph. Senza questo segnale l'animation phase del
  // widget legacy avanzerebbe ma il QQuickItem non lo saprebbe mai
  // (greyline pulse + tx travel arc invisibili in QML).
  void repaintRequested();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent * event) override;
  void resizeEvent(QResizeEvent * event) override;
  QSize minimumSizeHint() const override;

private:
  struct Contact
  {
    QString call;
    QString sourceGrid;
    QString destinationGrid;
    QPointF sourceLonLat;
    QPointF destinationLonLat;
    qint64 lastSeenMonotonicMs {0};
    PathRole role {PathRole::Generic};
    bool queuedDuringTx {false};
  };

  struct CoverageCell
  {
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

  bool maidenheadToLonLat(QString const& locator, QPointF * lonLat) const;
  QPointF projectLonLatToPoint(QPointF const& lonLat, QRectF const& bounds) const;
  bool azimuthalProjectionEnabled() const;
  QPixmap azimuthalProjectionPixmap(QPixmap const& source, int cacheSlot) const;
  double projectLatitude(double latitude) const;
  QVector<QPointF> greatCircle(QPointF const& startLonLat, QPointF const& endLonLat, int steps) const;
  void drawProjectedPixmap(QPainter * painter, QRectF const& bounds,
                           QPixmap const& texture, qreal opacity,
                           bool screenBlend) const;
  void drawBackground(QPainter * painter, QRectF const& bounds) const;
  void drawGeoOverlay(QPainter * painter, QRectF const& bounds) const;
  void drawGrid(QPainter * painter, QRectF const& bounds) const;
  void drawCoverage(QPainter * painter, QRectF const& bounds) const;
  void drawOperationalLayers(QPainter * painter, QRectF const& bounds) const;
  void drawGeographicFeatures(QPainter * painter, QRectF const& bounds) const;
  void drawExternalOverlay(QPainter * painter, QRectF const& bounds) const;
  void drawDayNightMask(QPainter * painter, QRectF const& bounds) const;
  void drawContact(QPainter * painter, QRectF const& bounds, Contact const& contact,
                   QVector<QRectF> * usedLabelAreas, bool drawLabel,
                   bool drawArrow = true, qreal forcedProgress = -1.0) const;
  void updateViewportTargets();
  void smoothViewport();
  bool computeCircularLongitudeBounds(QVector<double> const& longitudes, double * centerLon, double * spanLon) const;
  void pruneExpiredContacts();
  static double wrapLongitude(double lon);

  // 1.0.213 — cache layered: pre-renderizza earth + overlay + grid in
  // QPixmap statica, ricostruita solo se viewport/size cambiano. In idle
  // (viewport stabilizzato) ogni paintEvent diventa blit cheap + overlay
  // dinamici (day/night + contacts + transmit arc). Su CPU bassa il
  // guadagno e' >70% del tempo paint.
  bool backgroundCacheValid(QRectF const& bounds) const;
  void rebuildBackgroundCache(QRectF const& bounds);
  bool greylineCacheValid(QRectF const& bounds) const;
  void rebuildGreylineCache(QRectF const& bounds);
  void invalidateGreylineCache();

  QString m_homeGrid;
  QPointF m_homeLonLat;
  bool m_hasHome {false};
  QHash<QString, Contact> m_contacts;
  QVector<CoverageCell> m_coverageCells;
  QVariantList m_operationalMarkers;
  QVariantList m_geographicFeatures;
  QPixmap m_worldTexture;
  QPixmap m_worldOverlay;
  QPixmap m_externalOverlay;
  QTimer m_animationTimer;
  qreal m_animationPhase {0.0};
  QString m_lastClickedCall;
  qint64 m_lastClickedUntilMs {0};
  bool m_transmitting {false};
  QString m_txTargetCall;
  QString m_txTargetGrid;
  qint64 m_txStartMs {0};
  int m_txTravelMs {5200};
  qint64 m_postTxQueueUntilMs {0};
  bool m_baseMapEnabled {true};
  bool m_greylineEnabled {true};
  bool m_coveragePushPins {false};
  bool m_timeZoneOverlayEnabled {false};
  bool m_distanceInMiles {false};
  bool m_userViewportLocked {false};
  QString m_projection {QStringLiteral("Equirectangular")};
  mutable QPixmap m_azimuthalBaseCache;
  mutable QPixmap m_azimuthalOverlayCache;
  mutable QPixmap m_azimuthalExternalCache;
  mutable bool m_azimuthalBaseDirty {true};
  mutable bool m_azimuthalOverlayDirty {true};
  mutable bool m_azimuthalExternalDirty {true};

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

  // 1.0.213 — Cache del background renderizzato (earth + overlay + grid).
  // Invalidata implicitamente quando i parametri cambiano (check via
  // backgroundCacheValid). Devicepixel ratio salvato per HiDPI.
  QPixmap m_backgroundCache;
  QSizeF m_bgCacheBoundsSize;
  double m_bgCacheCenterLon {0.0};
  double m_bgCacheCenterLat {0.0};
  double m_bgCacheSpanLon {0.0};
  double m_bgCacheSpanLat {0.0};
  qreal m_bgCacheDpr {1.0};

  // Greyline/night overlay cache. Il calcolo e' per-colonna e dipende solo
  // da UTC, viewport e size: non serve rifarlo ad ogni frame animato.
  QPixmap m_greylineCache;
  QSizeF m_greylineCacheBoundsSize;
  double m_greylineCacheCenterLon {0.0};
  double m_greylineCacheCenterLat {0.0};
  double m_greylineCacheSpanLon {0.0};
  double m_greylineCacheSpanLat {0.0};
  qreal m_greylineCacheDpr {1.0};
  qint64 m_greylineCacheUtcMs {0};

  qint64 m_lastPaintProfileLogMs {0};
  int m_profilePaintSamples {0};
  double m_profilePaintMsSum {0.0};
  double m_profileGreylineMsSum {0.0};
  bool m_profileCacheRebuildSinceLastLog {false};
};

#endif // WORLDMAPWIDGET_H
