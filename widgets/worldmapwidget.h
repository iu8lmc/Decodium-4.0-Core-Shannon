#ifndef WORLDMAPWIDGET_H
#define WORLDMAPWIDGET_H

#include <QDateTime>
#include <QHash>
#include <QPointF>
#include <QPixmap>
#include <QTimer>
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
  void setGreylineEnabled(bool enabled);
  void setDistanceInMiles(bool enabled);
  void setTransmitState(bool transmitting, QString const& targetCall, QString const& targetGrid, QString const& mode);
  void downgradeContactToBand(QString const& call);
  void addContact(QString const& call, QString const& sourceGrid, QString const& destinationGrid,
                  PathRole role = PathRole::Generic);
  void addContactByLonLat(QString const& call, QPointF const& sourceLonLat, QString const& destinationGrid,
                          PathRole role = PathRole::Generic);
  void handleMapClick(QPointF const& clickPos);  // called from MainWindow's eventFilter

signals:
  void contactClicked(QString const& call, QString const& grid);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent * event) override;
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

  bool maidenheadToLonLat(QString const& locator, QPointF * lonLat) const;
  QPointF projectLonLatToPoint(QPointF const& lonLat, QRectF const& bounds) const;
  QVector<QPointF> greatCircle(QPointF const& startLonLat, QPointF const& endLonLat, int steps) const;
  void drawBackground(QPainter * painter, QRectF const& bounds) const;
  void drawGeoOverlay(QPainter * painter, QRectF const& bounds) const;
  void drawGrid(QPainter * painter, QRectF const& bounds) const;
  void drawDayNightMask(QPainter * painter, QRectF const& bounds) const;
  void drawContact(QPainter * painter, QRectF const& bounds, Contact const& contact,
                   QVector<QRectF> * usedLabelAreas, bool drawLabel,
                   bool drawArrow = true, qreal forcedProgress = -1.0) const;
  void updateViewportTargets();
  void smoothViewport();
  bool computeCircularLongitudeBounds(QVector<double> const& longitudes, double * centerLon, double * spanLon) const;
  void pruneExpiredContacts();
  static double wrapLongitude(double lon);

  QString m_homeGrid;
  QPointF m_homeLonLat;
  bool m_hasHome {false};
  QHash<QString, Contact> m_contacts;
  QPixmap m_worldTexture;
  QPixmap m_worldOverlay;
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
  bool m_greylineEnabled {true};
  bool m_distanceInMiles {false};

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
};

#endif // WORLDMAPWIDGET_H
