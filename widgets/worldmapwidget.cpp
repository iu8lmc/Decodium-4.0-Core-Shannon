#include "worldmapwidget.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QMouseEvent>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QLineF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QSizePolicy>
#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
int const kContactLifetimeSeconds = 2 * 60;
int const kMaxContacts = 40;
int const kRoleDowngradeHoldSeconds = 75;
int const kPostTxQueueMs = 10 * 1000;
int const kPostTxQueueMaxVisible = 6;
int const kClickHighlightMs = 1600;
double constexpr kEarthRadiusKm = 6371.0;

qint64 monotonicNowMs()
{
  static QElapsedTimer timer;
  static bool started = false;
  if (!started)
    {
      timer.start();
      started = true;
    }
  return timer.elapsed();
}

qint64 ageSecondsFromMonotonic(qint64 nowMs, qint64 seenMs)
{
  if (seenMs <= 0 || nowMs <= seenMs)
    {
      return 0;
    }
  return (nowMs - seenMs) / 1000;
}

QPixmap loadPixmapWithFallback(QStringList const& candidates)
{
  for (auto const& candidate : candidates)
    {
      QPixmap p {candidate};
      if (!p.isNull())
        {
          return p;
        }
    }
  return QPixmap {};
}

int rolePriority(WorldMapWidget::PathRole role)
{
  switch (role)
    {
    case WorldMapWidget::PathRole::IncomingToMe:
      return 3;
    case WorldMapWidget::PathRole::OutgoingFromMe:
      return 2;
    case WorldMapWidget::PathRole::Generic:
      return 1;
    case WorldMapWidget::PathRole::BandOnly:
      return 0;
    }
  return 0;
}

double greatCircleDistanceKm(QPointF const& aLonLat, QPointF const& bLonLat)
{
  double const lat1 = qDegreesToRadians(aLonLat.y());
  double const lat2 = qDegreesToRadians(bLonLat.y());
  double const dLat = lat2 - lat1;
  double const dLon = qDegreesToRadians(bLonLat.x() - aLonLat.x());

  double const sLat = std::sin(dLat * 0.5);
  double const sLon = std::sin(dLon * 0.5);
  double h = sLat * sLat + std::cos(lat1) * std::cos(lat2) * sLon * sLon;
  h = qBound(0.0, h, 1.0);
  double const c = 2.0 * std::atan2(std::sqrt(h), std::sqrt(1.0 - h));
  return kEarthRadiusKm * c;
}

QPointF projectedPathMidpoint(QVector<QPointF> const& projected, QRectF const& bounds)
{
  if (projected.size() < 2)
    {
      return projected.isEmpty() ? QPointF {} : projected.first();
    }

  struct Segment
  {
    QPointF a;
    QPointF b;
    qreal length {0.0};
  };

  QVector<Segment> segments;
  segments.reserve(projected.size() - 1);
  qreal totalLength = 0.0;
  for (int i = 0; i < projected.size() - 1; ++i)
    {
      QPointF const a = projected[i];
      QPointF const b = projected[i + 1];
      qreal len = QLineF {a, b}.length();
      if (len <= 1.0 || qAbs(a.x() - b.x()) >= bounds.width() * 0.42)
        {
          continue;
        }
      Segment s;
      s.a = a;
      s.b = b;
      s.length = len;
      segments.push_back(s);
      totalLength += len;
    }

  if (totalLength <= 0.0 || segments.isEmpty())
    {
      return projected.at(projected.size() / 2);
    }

  qreal targetDistance = totalLength * 0.5;
  qreal walked = 0.0;
  for (auto const& s : segments)
    {
      if (walked + s.length >= targetDistance)
        {
          qreal t = (targetDistance - walked) / s.length;
          return s.a + (s.b - s.a) * t;
        }
      walked += s.length;
    }

  return segments.last().b;
}

QString formatDistanceLabel(double distanceKm, bool miles)
{
  double const value = miles ? distanceKm * 0.621371192 : distanceKm;
  int const rounded = qMax(0, qRound(value));
  return QStringLiteral("%1 %2").arg(rounded).arg(miles ? QStringLiteral("mi") : QStringLiteral("km"));
}
}

WorldMapWidget::WorldMapWidget(QWidget * parent)
  : QWidget {parent}
{
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMinimumHeight(190);
  setAttribute(Qt::WA_OpaquePaintEvent, false);
  setMouseTracking(false);

  auto const appDir = QCoreApplication::applicationDirPath();
  auto const cwd = QDir::currentPath();

  m_worldTexture = loadPixmapWithFallback({
      ":/earth_2048x1024.jpg",
      ":/artwork/maps/earth_2048x1024.jpg",
      QDir {appDir}.absoluteFilePath("../Resources/earth_2048x1024.jpg"),
      QDir {appDir}.absoluteFilePath("../Resources/wsjtx/earth_2048x1024.jpg"),
      QDir {appDir}.absoluteFilePath("../Resources/wsjtx/maps/earth_2048x1024.jpg"),
      QDir {appDir}.absoluteFilePath("../../../../artwork/maps/earth_2048x1024.jpg"),
      QDir {cwd}.absoluteFilePath("artwork/maps/earth_2048x1024.jpg")
    });

  m_worldOverlay = loadPixmapWithFallback({
      ":/world_overlay_2048x1024.png",
      ":/artwork/maps/world_overlay_2048x1024.png",
      QDir {appDir}.absoluteFilePath("../Resources/world_overlay_2048x1024.png"),
      QDir {appDir}.absoluteFilePath("../Resources/wsjtx/world_overlay_2048x1024.png"),
      QDir {appDir}.absoluteFilePath("../Resources/wsjtx/maps/world_overlay_2048x1024.png"),
      QDir {appDir}.absoluteFilePath("../../../../artwork/maps/world_overlay_2048x1024.png"),
      QDir {cwd}.absoluteFilePath("artwork/maps/world_overlay_2048x1024.png")
    });

  m_animationTimer.setInterval(60);
  connect(&m_animationTimer, &QTimer::timeout, this, [this] {
    pruneExpiredContacts();
    updateViewportTargets();
    smoothViewport();
    m_animationPhase = std::fmod(m_animationPhase + 0.006, 1.0);
    update();
  });
  m_animationTimer.start();
}

void WorldMapWidget::setHomeGrid(QString const& grid)
{
  QPointF lonLat;
  if (maidenheadToLonLat(grid, &lonLat))
    {
      m_homeGrid = grid.trimmed().toUpper();
      m_homeLonLat = lonLat;
      m_hasHome = true;
    }
  else
    {
      m_homeGrid.clear();
      m_hasHome = false;
    }
  updateViewportTargets();
  update();
}

void WorldMapWidget::setGreylineEnabled(bool enabled)
{
  if (m_greylineEnabled == enabled)
    {
      return;
    }

  m_greylineEnabled = enabled;
  update();
}

void WorldMapWidget::setDistanceInMiles(bool enabled)
{
  if (m_distanceInMiles == enabled)
    {
      return;
    }

  m_distanceInMiles = enabled;
  update();
}

void WorldMapWidget::setTransmitState(bool transmitting, QString const& targetCall, QString const& targetGrid, QString const& mode)
{
  auto normalizedCall = targetCall.trimmed().toUpper();
  normalizedCall.remove('<');
  normalizedCall.remove('>');

  auto normalizedGrid = targetGrid.trimmed().toUpper();
  if (normalizedGrid.size() > 6)
    {
      normalizedGrid = normalizedGrid.left(6);
    }

  int travelMs = 5600;
  auto modeUpper = mode.trimmed().toUpper();
  if (modeUpper == "FT2")
    {
      travelMs = 4000;
    }
  else if (modeUpper == "FT4")
    {
      travelMs = 5400;
    }
  else if (modeUpper == "FT8")
    {
      travelMs = 6200;
    }

  bool resetProgress = transmitting
    && (!m_transmitting
        || normalizedCall != m_txTargetCall
        || normalizedGrid != m_txTargetGrid
        || travelMs != m_txTravelMs);

  bool wasTransmitting = m_transmitting;
  m_transmitting = transmitting;
  m_txTravelMs = travelMs;
  bool removedOutgoing = false;

  if (transmitting)
    {
      if (!wasTransmitting)
        {
          m_postTxQueueUntilMs = 0;
          for (auto it = m_contacts.begin(); it != m_contacts.end(); ++it)
            {
              it.value().queuedDuringTx = false;
            }
        }
      m_txTargetCall = normalizedCall;
      m_txTargetGrid = normalizedGrid;
      if (resetProgress)
        {
          m_txStartMs = QDateTime::currentMSecsSinceEpoch();
        }
    }
  else
    {
      m_txTargetCall.clear();
      m_txTargetGrid.clear();
      m_txStartMs = 0;
      if (wasTransmitting)
        {
          bool hasQueuedCallers = false;
          for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it)
            {
              if (it.value().role == PathRole::IncomingToMe && it.value().queuedDuringTx)
                {
                  hasQueuedCallers = true;
                  break;
                }
            }
          m_postTxQueueUntilMs = hasQueuedCallers ? (QDateTime::currentMSecsSinceEpoch() + kPostTxQueueMs) : 0;
        }
      for (auto it = m_contacts.begin(); it != m_contacts.end(); )
        {
          if (it.value().role == PathRole::OutgoingFromMe)
            {
              it = m_contacts.erase(it);
              removedOutgoing = true;
            }
          else
            {
              ++it;
            }
        }
    }

  if (removedOutgoing)
    {
      updateViewportTargets();
    }

  update();
}

void WorldMapWidget::downgradeContactToBand(QString const& call)
{
  auto normalizedCall = call.trimmed().toUpper();
  normalizedCall.remove('<');
  normalizedCall.remove('>');
  if (normalizedCall.isEmpty())
    {
      return;
    }

  auto sameStation = [&normalizedCall] (QString contactCall) {
      contactCall = contactCall.trimmed().toUpper();
      contactCall.remove('<');
      contactCall.remove('>');
      if (contactCall.isEmpty())
        {
          return false;
        }
      if (contactCall == normalizedCall
          || contactCall.startsWith(normalizedCall + "/")
          || normalizedCall.startsWith(contactCall + "/"))
        {
          return true;
        }
      return false;
    };

  bool changed = false;
  auto const nowMs = monotonicNowMs();
  for (auto it = m_contacts.begin(); it != m_contacts.end(); ++it)
    {
      if (!sameStation(it.value().call))
        {
          continue;
        }
      if (it.value().role != PathRole::BandOnly)
        {
          it.value().role = PathRole::BandOnly;
          it.value().queuedDuringTx = false;
          it.value().lastSeenMonotonicMs = nowMs;
          changed = true;
        }
    }

  if (changed)
    {
      updateViewportTargets();
      update();
    }
}

void WorldMapWidget::addContact(QString const& call, QString const& sourceGrid, QString const& destinationGrid, PathRole role)
{
  QPointF sourceLonLat;
  if (!maidenheadToLonLat(sourceGrid, &sourceLonLat))
    {
      return;
    }

  QPointF destinationLonLat;
  if (!maidenheadToLonLat(destinationGrid, &destinationLonLat))
    {
      if (!m_hasHome)
        {
          return;
        }
      destinationLonLat = m_homeLonLat;
    }

  Contact contact;
  contact.call = call.trimmed().toUpper();
  contact.sourceGrid = sourceGrid.trimmed().toUpper();
  contact.destinationGrid = destinationGrid.trimmed().toUpper();
  contact.sourceLonLat = sourceLonLat;
  contact.destinationLonLat = destinationLonLat;
  auto const nowMs = monotonicNowMs();
  contact.lastSeenMonotonicMs = nowMs;
  contact.role = role;
  contact.queuedDuringTx = (role == PathRole::IncomingToMe && m_transmitting);

  auto key = contact.call.isEmpty() ? contact.sourceGrid : contact.call;
  auto existing = m_contacts.find(key);
  if (existing != m_contacts.end())
    {
      auto const& prev = existing.value();
      contact.queuedDuringTx = contact.queuedDuringTx || prev.queuedDuringTx;
      bool directionalPrev = (prev.role == PathRole::IncomingToMe || prev.role == PathRole::OutgoingFromMe);
      bool downgradeToBand = (role == PathRole::BandOnly && directionalPrev);
      bool stillFreshDirectional = (nowMs - prev.lastSeenMonotonicMs) <= (kRoleDowngradeHoldSeconds * 1000LL);
      if (downgradeToBand && stillFreshDirectional)
        {
          // Keep a fresh directed contact visible instead of immediately downgrading it to BAND.
          return;
        }
    }
  m_contacts.insert(key, contact);

  while (m_contacts.size() > kMaxContacts)
    {
      auto oldest = m_contacts.end();
      for (auto it = m_contacts.begin(); it != m_contacts.end(); ++it)
        {
          if (oldest == m_contacts.end() || it.value().lastSeenMonotonicMs < oldest.value().lastSeenMonotonicMs)
            {
              oldest = it;
            }
        }
      if (oldest != m_contacts.end())
        {
          m_contacts.erase(oldest);
        }
      else
        {
          break;
        }
    }

  updateViewportTargets();
  update();
}

void WorldMapWidget::addContactByLonLat(QString const& call, QPointF const& sourceLonLat, QString const& destinationGrid, PathRole role)
{
  if (!std::isfinite(sourceLonLat.x()) || !std::isfinite(sourceLonLat.y()))
    {
      return;
    }

  QPointF destinationLonLat;
  if (!maidenheadToLonLat(destinationGrid, &destinationLonLat))
    {
      if (!m_hasHome)
        {
          return;
        }
      destinationLonLat = m_homeLonLat;
    }

  Contact contact;
  contact.call = call.trimmed().toUpper();
  contact.sourceGrid = QString {};
  contact.destinationGrid = destinationGrid.trimmed().toUpper();
  contact.sourceLonLat = QPointF {wrapLongitude(sourceLonLat.x()), qBound(-90.0, static_cast<double>(sourceLonLat.y()), 90.0)};
  contact.destinationLonLat = destinationLonLat;
  auto const nowMs = monotonicNowMs();
  contact.lastSeenMonotonicMs = nowMs;
  contact.role = role;
  contact.queuedDuringTx = (role == PathRole::IncomingToMe && m_transmitting);

  auto key = contact.call.isEmpty() ? QString {"LL:%1:%2"}
                                        .arg(contact.sourceLonLat.x(), 0, 'f', 2)
                                        .arg(contact.sourceLonLat.y(), 0, 'f', 2)
                                    : contact.call;
  auto existing = m_contacts.find(key);
  if (existing != m_contacts.end())
    {
      auto const& prev = existing.value();
      contact.queuedDuringTx = contact.queuedDuringTx || prev.queuedDuringTx;
      bool directionalPrev = (prev.role == PathRole::IncomingToMe || prev.role == PathRole::OutgoingFromMe);
      bool downgradeToBand = (role == PathRole::BandOnly && directionalPrev);
      bool stillFreshDirectional = (nowMs - prev.lastSeenMonotonicMs) <= (kRoleDowngradeHoldSeconds * 1000LL);
      if (downgradeToBand && stillFreshDirectional)
        {
          return;
        }
    }
  m_contacts.insert(key, contact);

  while (m_contacts.size() > kMaxContacts)
    {
      auto oldest = m_contacts.end();
      for (auto it = m_contacts.begin(); it != m_contacts.end(); ++it)
        {
          if (oldest == m_contacts.end() || it.value().lastSeenMonotonicMs < oldest.value().lastSeenMonotonicMs)
            {
              oldest = it;
            }
        }
      if (oldest != m_contacts.end())
        {
          m_contacts.erase(oldest);
        }
      else
        {
          break;
        }
    }

  updateViewportTargets();
  update();
}

void WorldMapWidget::paintEvent(QPaintEvent * event)
{
  Q_UNUSED(event);

  QPainter painter {this};
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);

  QRectF frame = rect().adjusted(3, 3, -3, -3);
  QLinearGradient borderGradient {frame.topLeft(), frame.bottomLeft()};
  borderGradient.setColorAt(0.0, QColor(16, 44, 78));
  borderGradient.setColorAt(1.0, QColor(8, 25, 46));
  painter.fillRect(frame, borderGradient);

  painter.setPen(QPen(QColor(110, 165, 210, 120), 1.0));
  painter.drawRoundedRect(frame, 8, 8);

  QRectF mapBounds = frame.adjusted(2, 2, -2, -2);
  QPainterPath clipPath;
  clipPath.addRoundedRect(mapBounds, 7, 7);

  painter.save();
  painter.setClipPath(clipPath);

  drawBackground(&painter, mapBounds);
  drawGeoOverlay(&painter, mapBounds);
  drawDayNightMask(&painter, mapBounds);
  drawGrid(&painter, mapBounds);

  QList<Contact> contacts = m_contacts.values();
  std::sort(contacts.begin(), contacts.end(), [] (Contact const& a, Contact const& b) {
    int pa = rolePriority(a.role);
    int pb = rolePriority(b.role);
    if (pa != pb)
      {
        return pa > pb;
      }
    return a.lastSeenMonotonicMs > b.lastSeenMonotonicMs;
  });
  int visiblePaths = 0;
  int visibleBand = 0;

  QVector<QRectF> usedLabelAreas;
  auto normalizeCall = [] (QString call)
    {
      call = call.trimmed().toUpper();
      call.remove('<');
      call.remove('>');
      return call;
    };

  if (m_transmitting)
    {
      int selectedIndex = -1;
      int bestScore = std::numeric_limits<int>::min();
      auto const nowMs = monotonicNowMs();
      QString targetCall = normalizeCall(m_txTargetCall);
      QString targetGrid = m_txTargetGrid.trimmed().toUpper();
      bool const hasExplicitTxTarget = !targetCall.isEmpty() || !targetGrid.isEmpty();

      for (int i = 0; hasExplicitTxTarget && i < contacts.size(); ++i)
        {
          auto const& c = contacts[i];
          if (c.role != PathRole::OutgoingFromMe)
            {
              continue;
            }

          QString contactCall = normalizeCall(c.call);
          QString contactGrid = c.destinationGrid.trimmed().toUpper();

          bool callMatch = targetCall.isEmpty()
            || contactCall == targetCall
            || contactCall.startsWith(targetCall + "/")
            || targetCall.startsWith(contactCall + "/");

          bool gridMatch = targetGrid.isEmpty();
          if (!targetGrid.isEmpty())
            {
              if (targetGrid.size() >= 6 && contactGrid.size() >= 6)
                {
                  gridMatch = (contactGrid.left(6) == targetGrid.left(6));
                }
              else
                {
                  gridMatch = (contactGrid.left(4) == targetGrid.left(4));
                }
            }

          int score = 0;
          if (callMatch) score += 6;
          if (gridMatch) score += 4;
          score += qMax(0, 1800 - static_cast<int>(ageSecondsFromMonotonic(nowMs, c.lastSeenMonotonicMs)));

          if (score > bestScore)
            {
              bestScore = score;
              selectedIndex = i;
            }
        }

      if (selectedIndex >= 0)
        {
          qreal progress = 0.0;
          if (m_txTravelMs > 0 && m_txStartMs > 0)
            {
              qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - m_txStartMs;
              if (elapsedMs < 0) elapsedMs = 0;
              progress = std::fmod(static_cast<double>(elapsedMs) / static_cast<double>(m_txTravelMs), 1.0);
            }
          drawContact(&painter, mapBounds, contacts[selectedIndex], &usedLabelAreas, true, true, progress);
          visiblePaths = 1;
        }
    }
  else
    {
      bool queueActive = (m_postTxQueueUntilMs > 0
                          && QDateTime::currentMSecsSinceEpoch() < m_postTxQueueUntilMs);
      int const maxVisible = qMin(28, contacts.size());
      int drawn = 0;
      int limit = queueActive ? kPostTxQueueMaxVisible : maxVisible;
      for (int i = 0; i < contacts.size() && drawn < limit; ++i)
        {
          auto const& contact = contacts[i];
          if (contact.role == PathRole::OutgoingFromMe)
            {
              continue;
            }
          if (queueActive)
            {
              if (contact.role != PathRole::IncomingToMe || !contact.queuedDuringTx)
                {
                  continue;
                }
            }
          bool drawLabel = drawn < 20;
          bool drawArrow = (contact.role == PathRole::IncomingToMe);
          drawContact(&painter, mapBounds, contact, &usedLabelAreas, drawLabel, drawArrow);
          if (contact.role == PathRole::BandOnly)
            {
              ++visibleBand;
            }
          else
            {
              ++visiblePaths;
            }
          ++drawn;
        }
      if (queueActive && drawn == 0)
        {
          m_postTxQueueUntilMs = 0;
        }
    }

  if (m_hasHome)
    {
      auto const p = projectLonLatToPoint(m_homeLonLat, mapBounds);
      QColor rxColor {60, 220, 255, 240};

      painter.setPen(QPen(QColor(210, 250, 255, 210), 1.5));
      painter.setBrush(QColor(rxColor.red(), rxColor.green(), rxColor.blue(), 95));
      painter.drawEllipse(p, 6.2, 6.2);

      painter.setPen(Qt::NoPen);
      painter.setBrush(rxColor);
      painter.drawEllipse(p, 3.6, 3.6);

      painter.setPen(QColor(235, 250, 255, 235));
      painter.drawText(p + QPointF(9.0, -8.0), m_homeGrid.left(6));
    }
  else
    {
      painter.setPen(QColor(230, 235, 245, 220));
      painter.drawText(mapBounds, Qt::AlignCenter, tr("Set your locator to enable the map"));
    }

  painter.restore();

  if (frame.width() > 430.0)
    {
      qreal legendY = frame.bottom() - 21.0;
      qreal x = frame.left() + 10.0;

      painter.setPen(QPen(QColor(255, 126, 92, 230), 2.0));
      painter.drawLine(QPointF {x, legendY}, QPointF {x + 17.0, legendY});
      painter.setPen(QColor(226, 236, 246, 210));
      painter.drawText(QPointF {x + 21.0, legendY + 4.0}, tr("IN->ME"));

      x += 100.0;
      painter.setPen(QPen(QColor(84, 238, 165, 230), 2.0));
      painter.drawLine(QPointF {x, legendY}, QPointF {x + 17.0, legendY});
      painter.setPen(QColor(226, 236, 246, 210));
      painter.drawText(QPointF {x + 21.0, legendY + 4.0}, tr("ME->DX"));

      x += 104.0;
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(255, 212, 96, 230));
      painter.drawEllipse(QPointF {x + 8.0, legendY}, 3.2, 3.2);
      painter.setPen(QColor(226, 236, 246, 210));
      painter.drawText(QPointF {x + 21.0, legendY + 4.0}, tr("BAND"));
    }

  if (!m_transmitting && m_postTxQueueUntilMs > QDateTime::currentMSecsSinceEpoch())
    {
      int remaining = qMax(1, static_cast<int>((m_postTxQueueUntilMs - QDateTime::currentMSecsSinceEpoch() + 999) / 1000));
      painter.setPen(QColor(255, 214, 148, 220));
      painter.drawText(frame.adjusted(10, 8, -10, -8), Qt::AlignLeft | Qt::AlignTop,
                       tr("Callers queue %1s").arg(remaining));
    }

  painter.setPen(QColor(225, 235, 245, 200));
  QString bottomLeft;
  if (visibleBand > 0)
    {
      bottomLeft = tr("%1 active paths | %2 in band").arg(visiblePaths).arg(visibleBand);
    }
  else
    {
      bottomLeft = tr("%1 active paths").arg(visiblePaths);
    }
  painter.drawText(frame.adjusted(8, 0, -8, -6), Qt::AlignLeft | Qt::AlignBottom, bottomLeft);
  painter.drawText(frame.adjusted(8, 0, -8, -6), Qt::AlignRight | Qt::AlignBottom,
                   QDateTime::currentDateTimeUtc().toString("hh:mm:ss 'UTC'"));
}

QSize WorldMapWidget::minimumSizeHint() const
{
  return QSize {280, 200};
}

void WorldMapWidget::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
    {
      handleMapClick(event->pos());
      event->accept();
      return;
    }
  QWidget::mousePressEvent(event);
}

void WorldMapWidget::handleMapClick(QPointF const& clickPos)
{
  // Use exact same bounds as paintEvent (frame + 2px adjustment = 5px offset)
  QRectF frame = QRectF(rect()).adjusted(3, 3, -3, -3);
  QRectF mapBounds = frame.adjusted(2, 2, -2, -2);

  qDebug() << "[Map] handleMapClick at" << clickPos << "contacts=" << m_contacts.size();

  // Dynamic click radius: adapt to current zoom level and HiDPI scaling.
  double spanLon = qBound(30.0, m_viewSpanLon, 360.0);
  double zoomFactor = qBound(1.0, std::sqrt(360.0 / spanLon), 1.9);
  double dprFactor = qBound(1.0, std::sqrt(devicePixelRatioF()), 1.5);
  double clickRadiusPx = qBound(11.0, 11.0 * zoomFactor * dprFactor, 30.0);
  double minDistanceSq = clickRadiusPx * clickRadiusPx;
  QString closestCall;
  QString closestGrid;

  for (auto const& contact : m_contacts)
    {
      auto normalizedCall = contact.call.trimmed();
      if (normalizedCall.isEmpty())
        {
          // Markers without a callsign are not actionable from map click.
          continue;
        }

      // Click target should match the visually relevant marker:
      // - ME->DX: destination marker (remote station)
      // - all others: source marker
      QPointF markerPoint;
      QString markerGrid;
      if (contact.role == PathRole::OutgoingFromMe)
        {
          markerPoint = projectLonLatToPoint(contact.destinationLonLat, mapBounds);
          markerGrid = contact.destinationGrid;
        }
      else
        {
          markerPoint = projectLonLatToPoint(contact.sourceLonLat, mapBounds);
          markerGrid = contact.sourceGrid;
        }

      if (markerGrid.isEmpty())
        {
          markerGrid = contact.destinationGrid;
        }

      double distSq = (markerPoint.x() - clickPos.x()) * (markerPoint.x() - clickPos.x()) +
                      (markerPoint.y() - clickPos.y()) * (markerPoint.y() - clickPos.y());

      if (distSq < minDistanceSq)
        {
          minDistanceSq = distSq;
          closestCall = normalizedCall.toUpper();
          closestGrid = markerGrid;
        }
    }

  if (!closestCall.isEmpty())
    {
      m_lastClickedCall = closestCall;
      m_lastClickedUntilMs = monotonicNowMs() + kClickHighlightMs;
      update();
      qDebug() << "[Map] found contact" << closestCall << closestGrid;
      Q_EMIT contactClicked(closestCall, closestGrid);
    }
}

bool WorldMapWidget::maidenheadToLonLat(QString const& locator, QPointF * lonLat) const
{
  if (!lonLat)
    {
      return false;
    }

  auto l = locator.trimmed().toUpper();
  if (l.size() < 4 || (l.size() != 4 && l.size() != 6))
    {
      return false;
    }

  auto const c0 = l.at(0).unicode();
  auto const c1 = l.at(1).unicode();
  auto const c2 = l.at(2).unicode();
  auto const c3 = l.at(3).unicode();
  if (c0 < 'A' || c0 > 'R' || c1 < 'A' || c1 > 'R' || c2 < '0' || c2 > '9' || c3 < '0' || c3 > '9')
    {
      return false;
    }

  double lon = (c0 - 'A') * 20.0 - 180.0;
  double lat = (c1 - 'A') * 10.0 - 90.0;

  lon += (c2 - '0') * 2.0;
  lat += (c3 - '0') * 1.0;

  double lonStep = 2.0;
  double latStep = 1.0;

  if (l.size() == 6)
    {
      auto const c4 = l.at(4).unicode();
      auto const c5 = l.at(5).unicode();
      if (c4 < 'A' || c4 > 'X' || c5 < 'A' || c5 > 'X')
        {
          return false;
        }

      lonStep = 5.0 / 60.0;
      latStep = 2.5 / 60.0;
      lon += (c4 - 'A') * lonStep;
      lat += (c5 - 'A') * latStep;
    }

  lon += lonStep / 2.0;
  lat += latStep / 2.0;

  *lonLat = QPointF {lon, lat};
  return true;
}

QPointF WorldMapWidget::projectLonLatToPoint(QPointF const& lonLat, QRectF const& bounds) const
{
  double leftLon = m_viewCenterLon - 0.5 * m_viewSpanLon;
  double lon = lonLat.x();
  while (lon < leftLon)
    {
      lon += 360.0;
    }
  while (lon > leftLon + 360.0)
    {
      lon -= 360.0;
    }

  double lat = qBound(-90.0, static_cast<double>(lonLat.y()), 90.0);
  double topLat = m_viewCenterLat + 0.5 * m_viewSpanLat;

  qreal x = bounds.left() + static_cast<qreal>((lon - leftLon) / m_viewSpanLon) * bounds.width();
  qreal y = bounds.top() + static_cast<qreal>((topLat - lat) / m_viewSpanLat) * bounds.height();
  return QPointF {x, y};
}

QVector<QPointF> WorldMapWidget::greatCircle(QPointF const& startLonLat, QPointF const& endLonLat, int steps) const
{
  auto toVector = [] (QPointF const& lonLat) -> QVector3D {
    double lon = qDegreesToRadians(lonLat.x());
    double lat = qDegreesToRadians(lonLat.y());
    double cosLat = std::cos(lat);
    return QVector3D(cosLat * std::cos(lon), cosLat * std::sin(lon), std::sin(lat));
  };

  auto toLonLat = [] (QVector3D const& v) -> QPointF {
    double lon = std::atan2(v.y(), v.x());
    double lat = std::asin(qBound(-1.0, static_cast<double>(v.z()), 1.0));
    return QPointF {qRadiansToDegrees(lon), qRadiansToDegrees(lat)};
  };

  QVector3D a = toVector(startLonLat);
  QVector3D b = toVector(endLonLat);
  a.normalize();
  b.normalize();

  double dot = qBound(-1.0, static_cast<double>(QVector3D::dotProduct(a, b)), 1.0);
  double omega = std::acos(dot);

  QVector<QPointF> points;
  if (omega < 1e-6)
    {
      points << startLonLat << endLonLat;
      return points;
    }

  int n = qMax(4, steps);
  double sinOmega = std::sin(omega);
  for (int i = 0; i <= n; ++i)
    {
      double t = static_cast<double>(i) / static_cast<double>(n);
      double s0 = std::sin((1.0 - t) * omega) / sinOmega;
      double s1 = std::sin(t * omega) / sinOmega;
      QVector3D v = s0 * a + s1 * b;
      if (v.lengthSquared() > 0.0f)
        {
          v.normalize();
          points << toLonLat(v);
        }
    }

  return points;
}

void WorldMapWidget::drawBackground(QPainter * painter, QRectF const& bounds) const
{
  if (m_worldTexture.isNull())
    {
      QLinearGradient oceanGradient {bounds.topLeft(), bounds.bottomLeft()};
      oceanGradient.setColorAt(0.0, QColor(10, 56, 95));
      oceanGradient.setColorAt(1.0, QColor(4, 24, 42));
      painter->fillRect(bounds, oceanGradient);
      return;
    }

  double topLat = qBound(-90.0, m_viewCenterLat + 0.5 * m_viewSpanLat, 90.0);
  double bottomLat = qBound(-90.0, m_viewCenterLat - 0.5 * m_viewSpanLat, 90.0);
  qreal texW = static_cast<qreal>(m_worldTexture.width());
  qreal texH = static_cast<qreal>(m_worldTexture.height());
  qreal srcY0 = static_cast<qreal>((90.0 - topLat) / 180.0) * texH;
  qreal srcY1 = static_cast<qreal>((90.0 - bottomLat) / 180.0) * texH;
  QRectF sourceRect {0.0, srcY0, texW, qMax<qreal>(1.0, srcY1 - srcY0)};
  double leftLon = m_viewCenterLon - 0.5 * m_viewSpanLon;

  painter->save();
  painter->setOpacity(0.98);
  for (int k = -2; k <= 2; ++k)
    {
      double lonStart = -180.0 + 360.0 * k;
      double lonEnd = lonStart + 360.0;
      qreal x1 = bounds.left() + static_cast<qreal>((lonStart - leftLon) / m_viewSpanLon) * bounds.width();
      qreal x2 = bounds.left() + static_cast<qreal>((lonEnd - leftLon) / m_viewSpanLon) * bounds.width();
      QRectF tileRect {x1, bounds.top(), x2 - x1, bounds.height()};
      if (tileRect.right() < bounds.left() || tileRect.left() > bounds.right())
        {
          continue;
        }
      painter->drawPixmap(tileRect, m_worldTexture, sourceRect);
    }
  painter->restore();

  painter->fillRect(bounds, QColor(0, 14, 24, 18));
}

void WorldMapWidget::drawGeoOverlay(QPainter * painter, QRectF const& bounds) const
{
  if (m_worldOverlay.isNull())
    {
      return;
    }

  double topLat = qBound(-90.0, m_viewCenterLat + 0.5 * m_viewSpanLat, 90.0);
  double bottomLat = qBound(-90.0, m_viewCenterLat - 0.5 * m_viewSpanLat, 90.0);
  qreal texW = static_cast<qreal>(m_worldOverlay.width());
  qreal texH = static_cast<qreal>(m_worldOverlay.height());
  qreal srcY0 = static_cast<qreal>((90.0 - topLat) / 180.0) * texH;
  qreal srcY1 = static_cast<qreal>((90.0 - bottomLat) / 180.0) * texH;
  QRectF sourceRect {0.0, srcY0, texW, qMax<qreal>(1.0, srcY1 - srcY0)};
  double leftLon = m_viewCenterLon - 0.5 * m_viewSpanLon;

  painter->save();
  painter->setCompositionMode(QPainter::CompositionMode_Screen);
  painter->setOpacity(0.44);
  for (int k = -2; k <= 2; ++k)
    {
      double lonStart = -180.0 + 360.0 * k;
      double lonEnd = lonStart + 360.0;
      qreal x1 = bounds.left() + static_cast<qreal>((lonStart - leftLon) / m_viewSpanLon) * bounds.width();
      qreal x2 = bounds.left() + static_cast<qreal>((lonEnd - leftLon) / m_viewSpanLon) * bounds.width();
      QRectF tileRect {x1, bounds.top(), x2 - x1, bounds.height()};
      if (tileRect.right() < bounds.left() || tileRect.left() > bounds.right())
        {
          continue;
        }
      painter->drawPixmap(tileRect, m_worldOverlay, sourceRect);
    }
  painter->restore();
}

void WorldMapWidget::drawGrid(QPainter * painter, QRectF const& bounds) const
{
  painter->setBrush(Qt::NoBrush);

  double lonStep = 30.0;
  if (m_viewSpanLon < 220.0) lonStep = 20.0;
  if (m_viewSpanLon < 130.0) lonStep = 10.0;
  if (m_viewSpanLon < 75.0) lonStep = 5.0;

  double latStep = 20.0;
  if (m_viewSpanLat < 110.0) latStep = 10.0;
  if (m_viewSpanLat < 55.0) latStep = 5.0;

  painter->setPen(QPen(QColor(170, 210, 225, 42), 1.0));

  double leftLon = m_viewCenterLon - 0.5 * m_viewSpanLon;
  double rightLon = m_viewCenterLon + 0.5 * m_viewSpanLon;
  double startLon = std::floor(leftLon / lonStep) * lonStep;
  for (double lon = startLon; lon <= rightLon; lon += lonStep)
    {
      qreal x = bounds.left() + static_cast<qreal>((lon - leftLon) / m_viewSpanLon) * bounds.width();
      painter->drawLine(QPointF {x, bounds.top()}, QPointF {x, bounds.bottom()});
    }

  double topLat = m_viewCenterLat + 0.5 * m_viewSpanLat;
  double bottomLat = m_viewCenterLat - 0.5 * m_viewSpanLat;
  double startLat = std::floor(bottomLat / latStep) * latStep;
  for (double lat = startLat; lat <= topLat; lat += latStep)
    {
      qreal y = bounds.top() + static_cast<qreal>((topLat - lat) / m_viewSpanLat) * bounds.height();
      painter->drawLine(QPointF {bounds.left(), y}, QPointF {bounds.right(), y});
    }
}

void WorldMapWidget::drawDayNightMask(QPainter * painter, QRectF const& bounds) const
{
  if (!m_greylineEnabled)
    {
      return;
    }

  auto now = QDateTime::currentDateTimeUtc();
  auto t = now.time();
  double utcHours = t.hour() + (t.minute() / 60.0) + (t.second() / 3600.0);
  double subSolarLon = wrapLongitude((12.0 - utcHours) * 15.0);

  int dayOfYear = now.date().dayOfYear();
  double fractionalYear = (2.0 * M_PI / 365.24) * (dayOfYear - 1 + utcHours / 24.0);
  double declinationRadians = 0.006918 - 0.399912 * std::cos(fractionalYear) + 0.070257 * std::sin(fractionalYear)
                              - 0.006758 * std::cos(2.0 * fractionalYear) + 0.000907 * std::sin(2.0 * fractionalYear)
                              - 0.002697 * std::cos(3.0 * fractionalYear) + 0.00148 * std::sin(3.0 * fractionalYear);
  double subSolarLat = qRadiansToDegrees(declinationRadians);

  double leftLon = m_viewCenterLon - 0.5 * m_viewSpanLon;
  double topLat = m_viewCenterLat + 0.5 * m_viewSpanLat;

  int width = qMax(1, static_cast<int>(bounds.width()));
  int height = qMax(1, static_cast<int>(bounds.height()));

  double subLatRad = qDegreesToRadians(subSolarLat);

  painter->save();
  painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
  painter->setPen(Qt::NoPen);

  for (int x = 0; x < width; ++x)
    {
      double lon = leftLon + m_viewSpanLon * static_cast<double>(x) / static_cast<double>(width);
      double dLon = qDegreesToRadians(lon - subSolarLon);
      double cosDLon = std::cos(dLon);

      // Find terminator latitude for this column (where sun altitude = 0)
      // sin(decl)*sin(lat) + cos(decl)*cos(lat)*cos(dlon) = 0
      // => tan(lat) = -cos(dlon)/tan(decl)
      // Then find which pixels above/below are in night

      // Scan top-to-bottom to find transition
      int nightStartY = -1;
      int nightEndY = height;
      bool inNight = false;

      // Sample at top pixel
      {
        double lat = topLat - m_viewSpanLat * 0.0;
        double latRad = qDegreesToRadians(lat);
        double alt = std::sin(subLatRad) * std::sin(latRad) + std::cos(subLatRad) * std::cos(latRad) * cosDLon;
        inNight = (alt < 0.0);
      }

      if (inNight)
        {
          nightStartY = 0;
        }

      for (int y = 1; y < height; ++y)
        {
          double lat = topLat - m_viewSpanLat * static_cast<double>(y) / static_cast<double>(height);
          double latRad = qDegreesToRadians(lat);
          double alt = std::sin(subLatRad) * std::sin(latRad) + std::cos(subLatRad) * std::cos(latRad) * cosDLon;
          bool nowNight = (alt < 0.0);

          if (!inNight && nowNight)
            {
              nightStartY = y;
              inNight = true;
            }
          else if (inNight && !nowNight)
            {
              nightEndY = y;
              inNight = false;
              // partial column done - draw it
              if (nightStartY >= 0 && nightEndY > nightStartY)
                {
                  painter->setBrush(QColor(0, 5, 30, 145));
                  painter->drawRect(QRectF(bounds.left() + x, bounds.top() + nightStartY,
                                          1.0, nightEndY - nightStartY));
                }
              nightStartY = -1;
              nightEndY = height;
            }
        }

      // Close any open night region
      if (inNight && nightStartY >= 0)
        {
          painter->setBrush(QColor(0, 5, 30, 145));
          painter->drawRect(QRectF(bounds.left() + x, bounds.top() + nightStartY,
                                  1.0, height - nightStartY));
        }
    }

  // Draw the sun indicator
  QPointF sunPoint = projectLonLatToPoint(QPointF{subSolarLon, subSolarLat}, bounds);
  painter->setPen(Qt::NoPen);
  painter->setBrush(QColor(255, 240, 80, 220));
  painter->drawEllipse(sunPoint, 5.0, 5.0);
  painter->setBrush(QColor(255, 210, 50, 90));
  painter->drawEllipse(sunPoint, 9.0, 9.0);

  painter->restore();
}

void WorldMapWidget::drawContact(QPainter * painter, QRectF const& bounds, Contact const& contact,
                                 QVector<QRectF> * usedLabelAreas, bool drawLabel,
                                 bool drawArrow, qreal forcedProgress) const
{
  auto arc = greatCircle(contact.sourceLonLat, contact.destinationLonLat, 72);
  if (arc.size() < 2)
    {
      return;
    }

  qint64 ageSecs = ageSecondsFromMonotonic(monotonicNowMs(), contact.lastSeenMonotonicMs);

  auto const source = projectLonLatToPoint(contact.sourceLonLat, bounds);
  auto const destination = projectLonLatToPoint(contact.destinationLonLat, bounds);

  if (contact.role == PathRole::BandOnly)
    {
      int alpha = static_cast<int>(qBound(70.0, 238.0 - ageSecs * 2.0, 238.0));
      QColor markerColor {255, 212, 96, alpha};
      QColor haloColor {255, 225, 150, qMin(170, alpha)};

      painter->setPen(Qt::NoPen);
      painter->setBrush(haloColor);
      painter->drawEllipse(source, 4.6, 4.6);
      painter->setBrush(markerColor);
      painter->drawEllipse(source, 2.8, 2.8);

      if (drawLabel && !contact.call.isEmpty() && usedLabelAreas)
        {
          QPointF label = source + QPointF(7.0, -7.0);
          if (label.x() > bounds.right() - 95.0)
            {
              label.setX(bounds.right() - 95.0);
            }
          if (label.y() < bounds.top() + 14.0)
            {
              label.setY(bounds.top() + 14.0);
            }
          auto text = contact.call.left(12);
          QFont labelFont = painter->font();
          if (labelFont.pixelSize() > 0)
            {
              labelFont.setPixelSize(qMax(9, labelFont.pixelSize() - 2));
            }
          else
            {
              auto pointSize = labelFont.pointSizeF();
              if (pointSize > 0.0)
                {
                  labelFont.setPointSizeF(qMax(7.5, pointSize - 1.5));
                }
            }
          QFontMetricsF fm {labelFont};
          QRectF textRect {label + QPointF(-2.0, -12.0), QSizeF(fm.horizontalAdvance(text) + 6.0, 14.0)};
          bool blocked = false;
          for (auto const& used : *usedLabelAreas)
            {
              if (used.intersects(textRect))
                {
                  blocked = true;
                  break;
                }
            }
          if (!blocked)
            {
              painter->save();
              painter->setFont(labelFont);
              painter->setPen(QColor(255, 238, 174, qMin(235, alpha)));
              painter->drawText(label, text);
              painter->restore();
              usedLabelAreas->append(textRect);
            }
        }
      return;
    }

  QColor txColor {235, 110, 98};
  QColor rxColor {130, 190, 255};
  QColor arrowColor {255, 244, 196};
  if (contact.role == PathRole::IncomingToMe)
    {
      txColor = QColor(255, 126, 92);
      rxColor = QColor(86, 226, 255);
      arrowColor = QColor(255, 195, 140);
    }
  else if (contact.role == PathRole::OutgoingFromMe)
    {
      txColor = QColor(84, 238, 165);
      rxColor = QColor(255, 213, 92);
      arrowColor = QColor(255, 233, 132);
    }
  int alpha = static_cast<int>(qBound(65.0, 245.0 - ageSecs * 2.2, 245.0));
  txColor.setAlpha(alpha);
  rxColor.setAlpha(alpha);

  QPainterPath path;
  bool firstPoint = true;
  QPointF previous;
  QVector<QPointF> projected;
  projected.reserve(arc.size());
  for (auto const& lonLat : arc)
    {
      auto const p = projectLonLatToPoint(lonLat, bounds);
      projected << p;
      if (firstPoint)
        {
          path.moveTo(p);
          firstPoint = false;
        }
      else if (qAbs(p.x() - previous.x()) > (bounds.width() * 0.45))
        {
          path.moveTo(p);
        }
      else
        {
          path.lineTo(p);
        }
      previous = p;
    }

  auto normalizeCall = [] (QString call) {
    call = call.trimmed().toUpper();
    call.remove('<');
    call.remove('>');
    return call;
  };
  qint64 nowMs = monotonicNowMs();
  bool clickedActive = !m_lastClickedCall.isEmpty()
    && m_lastClickedUntilMs > nowMs
    && normalizeCall(contact.call) == m_lastClickedCall;
  bool txActive = (forcedProgress >= 0.0);

  QLinearGradient grad {source, destination};
  grad.setColorAt(0.0, txColor);
  grad.setColorAt(1.0, rxColor);
  QPen pathPen {QBrush {grad}, 1.8};
  pathPen.setCosmetic(true);
  painter->setPen(pathPen);
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(path);

  painter->setPen(Qt::NoPen);
  painter->setBrush(txColor);
  painter->drawEllipse(source, 3.2, 3.2);

  painter->setBrush(QColor(rxColor.red(), rxColor.green(), rxColor.blue(), qMin(220, alpha)));
  painter->drawEllipse(destination, 4.2, 4.2);
  painter->setBrush(rxColor);
  painter->drawEllipse(destination, 2.6, 2.6);

  if (contact.role != PathRole::BandOnly && (txActive || clickedActive))
    {
      double const distanceKm = greatCircleDistanceKm(contact.sourceLonLat, contact.destinationLonLat);
      QString const distanceText = formatDistanceLabel(distanceKm, m_distanceInMiles);
      QPointF anchor = projectedPathMidpoint(projected, bounds);

      painter->save();
      QFont distanceFont = painter->font();
      if (distanceFont.pixelSize() > 0)
        {
          distanceFont.setPixelSize(qMax(9, distanceFont.pixelSize() - 1));
        }
      else if (distanceFont.pointSizeF() > 0.0)
        {
          distanceFont.setPointSizeF(qMax(8.0, distanceFont.pointSizeF() - 0.5));
        }
      painter->setFont(distanceFont);

      QFontMetricsF fm {distanceFont};
      qreal const padX = 6.0;
      qreal const width = fm.horizontalAdvance(distanceText) + padX * 2.0;
      qreal const height = fm.height() + 2.0;
      QRectF badge {anchor.x() - width * 0.5, anchor.y() - height * 0.5, width, height};
      badge.moveLeft(qBound(bounds.left() + 2.0, badge.left(), bounds.right() - badge.width() - 2.0));
      badge.moveTop(qBound(bounds.top() + 2.0, badge.top(), bounds.bottom() - badge.height() - 2.0));

      QColor bg = txActive ? QColor(21, 64, 39, 205) : QColor(9, 28, 58, 198);
      painter->setBrush(bg);
      painter->setPen(QPen(QColor(240, 232, 168, 220), 1.0));
      painter->drawRoundedRect(badge, 4.0, 4.0);
      painter->setPen(QColor(245, 248, 235, 240));
      painter->drawText(badge, Qt::AlignCenter, distanceText);
      painter->restore();
    }

  // Brief visual feedback for the selected marker.
  if (!m_lastClickedCall.isEmpty() && m_lastClickedUntilMs > nowMs
      && normalizeCall(contact.call) == m_lastClickedCall)
    {
      qreal remaining = qBound<qreal>(0.0,
                                      static_cast<qreal>(m_lastClickedUntilMs - nowMs) / static_cast<qreal>(kClickHighlightMs),
                                      1.0);
      QPointF markerPoint = (contact.role == PathRole::OutgoingFromMe) ? destination : source;
      qreal outerRadius = 7.0 + remaining * 2.2;
      qreal innerRadius = 4.8 + remaining * 1.4;

      painter->save();
      painter->setBrush(Qt::NoBrush);
      painter->setPen(QPen(QColor(255, 108, 52, static_cast<int>(150 + 85 * remaining)), 2.2));
      painter->drawEllipse(markerPoint, outerRadius, outerRadius);
      painter->setPen(QPen(QColor(255, 210, 118, static_cast<int>(105 + 85 * remaining)), 1.4));
      painter->drawEllipse(markerPoint, innerRadius, innerRadius);
      painter->restore();
    }

  if (drawArrow && projected.size() >= 3)
    {
      struct Segment
        {
          QPointF a;
          QPointF b;
          qreal length {0.0};
        };

      QVector<Segment> segments;
      segments.reserve(projected.size() - 1);
      qreal totalLength = 0.0;
      for (int i = 0; i < projected.size() - 1; ++i)
        {
          QPointF const a = projected[i];
          QPointF const b = projected[i + 1];
          qreal len = QLineF {a, b}.length();
          if (len <= 2.0 || qAbs(a.x() - b.x()) >= bounds.width() * 0.42)
            {
              continue;
            }
          Segment s;
          s.a = a;
          s.b = b;
          s.length = len;
          segments.push_back(s);
          totalLength += len;
        }

      if (totalLength > 0.0 && !segments.isEmpty())
        {
          qreal progress = forcedProgress;
          if (progress < 0.0)
            {
              progress = std::fmod(m_animationPhase + (qHash(contact.call) % 17) * 0.057, 1.0);
            }
          if (progress < 0.0)
            {
              progress += 1.0;
            }

          qreal targetDistance = qBound<qreal>(0.0, progress, 0.999999) * totalLength;
          qreal walked = 0.0;
          Segment current = segments.first();
          for (auto const& s : segments)
            {
              if (walked + s.length >= targetDistance)
                {
                  current = s;
                  break;
                }
              walked += s.length;
            }

          qreal localDistance = targetDistance - walked;
          qreal t = current.length > 0.0 ? (localDistance / current.length) : 0.0;
          t = qBound<qreal>(0.0, t, 1.0);

          QPointF tip = current.a + (current.b - current.a) * t;
          QPointF direction = current.b - current.a;
          qreal dirLen = std::hypot(direction.x(), direction.y());
          if (dirLen > 0.0)
            {
              QPointF u = direction / dirLen;
              QPointF n {-u.y(), u.x()};
              qreal arrowLen = 9.0;
              qreal arrowWid = 4.2;
              QPolygonF arrow;
              arrow << tip
                    << tip - u * arrowLen + n * arrowWid
                    << tip - u * arrowLen - n * arrowWid;
              arrowColor.setAlpha(qMin(250, alpha + 10));
              painter->setBrush(arrowColor);
              painter->setPen(Qt::NoPen);
              painter->drawPolygon(arrow);
            }
        }
    }

  if (drawLabel && !contact.call.isEmpty() && usedLabelAreas)
    {
      QPointF labelAnchor = source;
      if (contact.role == PathRole::OutgoingFromMe)
        {
          // For ME->DX paths show the callsign at destination, not on home locator.
          labelAnchor = destination;
        }
      QPointF label = labelAnchor + QPointF(7.0, -7.0);
      if (label.x() < bounds.left() + 2.0)
        {
          label.setX(bounds.left() + 2.0);
        }
      if (label.x() > bounds.right() - 95.0)
        {
          label.setX(bounds.right() - 95.0);
        }
      if (label.y() < bounds.top() + 14.0)
        {
          label.setY(bounds.top() + 14.0);
        }
      if (label.y() > bounds.bottom() - 2.0)
        {
          label.setY(bounds.bottom() - 2.0);
        }
      auto text = contact.call.left(12);
      QFont labelFont = painter->font();
      if (labelFont.pixelSize() > 0)
        {
          labelFont.setPixelSize(qMax(9, labelFont.pixelSize() - 2));
        }
      else
        {
          auto pointSize = labelFont.pointSizeF();
          if (pointSize > 0.0)
            {
              labelFont.setPointSizeF(qMax(7.5, pointSize - 1.5));
            }
        }
      QFontMetricsF fm {labelFont};
      QRectF textRect {label + QPointF(-2.0, -12.0), QSizeF(fm.horizontalAdvance(text) + 6.0, 14.0)};
      bool blocked = false;
      for (auto const& used : *usedLabelAreas)
        {
          if (used.intersects(textRect))
            {
              blocked = true;
              break;
            }
        }
      if (!blocked)
        {
          painter->save();
          painter->setFont(labelFont);
          painter->setPen(QColor(255, 244, 196, 235));
          painter->drawText(label, text);
          painter->restore();
          usedLabelAreas->append(textRect);
        }
    }
}

void WorldMapWidget::updateViewportTargets()
{
  bool queueActive = (!m_transmitting
                      && m_postTxQueueUntilMs > QDateTime::currentMSecsSinceEpoch());
  QVector<QPointF> points;
  points.reserve(m_contacts.size() * 2 + (m_hasHome ? 1 : 0));
  Contact newestContact;
  bool hasNewestContact = false;

  if (m_hasHome)
    {
      points << m_homeLonLat;
    }

  for (auto const& contact : m_contacts)
    {
      if (queueActive)
        {
          if (contact.role != PathRole::IncomingToMe || !contact.queuedDuringTx)
            {
              continue;
            }
        }
      if (!m_transmitting && contact.role == PathRole::OutgoingFromMe)
        {
          continue;
        }
      points << contact.sourceLonLat;
      if (contact.role != PathRole::BandOnly)
        {
          points << contact.destinationLonLat;
        }
      if (!hasNewestContact || contact.lastSeenMonotonicMs > newestContact.lastSeenMonotonicMs)
        {
          newestContact = contact;
          hasNewestContact = true;
        }
    }

  if (points.isEmpty())
    {
      m_targetCenterLon = 0.0;
      m_targetCenterLat = 0.0;
      m_targetSpanLon = 360.0;
      m_targetSpanLat = 180.0;
      return;
    }

  if (points.size() == 1)
    {
      double centerLon = wrapLongitude(points[0].x());
      double centerLat = qBound(-44.0, static_cast<double>(points[0].y()), 44.0);
      m_targetCenterLon = centerLon;
      m_targetCenterLat = centerLat;
      m_targetSpanLon = 150.0;
      m_targetSpanLat = 88.0;
      return;
    }

  QVector<double> lons;
  lons.reserve(points.size());
  double minLat = 90.0;
  double maxLat = -90.0;
  for (auto const& point : points)
    {
      lons << wrapLongitude(point.x());
      minLat = qMin(minLat, static_cast<double>(point.y()));
      maxLat = qMax(maxLat, static_cast<double>(point.y()));
    }

  double centerLon = 0.0;
  double spanLon = 360.0;
  if (!computeCircularLongitudeBounds(lons, &centerLon, &spanLon))
    {
      centerLon = 0.0;
      spanLon = 360.0;
    }

  double centerLat = 0.5 * (minLat + maxLat);
  double spanLat = qMax(1.0, maxLat - minLat);

  spanLon = qBound(68.0, spanLon * 1.45 + 10.0, 240.0);
  spanLat = qBound(34.0, spanLat * 1.45 + 8.0, 126.0);

  if (hasNewestContact)
    {
      double focusLon = newestContact.sourceLonLat.x();
      double focusLat = newestContact.sourceLonLat.y();
      if (m_hasHome)
        {
          focusLon = wrapLongitude(m_homeLonLat.x() + wrapLongitude(focusLon - m_homeLonLat.x()) * 0.62);
          focusLat = m_homeLonLat.y() + (focusLat - m_homeLonLat.y()) * 0.62;
        }
      centerLon = wrapLongitude(centerLon + wrapLongitude(focusLon - centerLon) * 0.40);
      centerLat += (focusLat - centerLat) * 0.40;
    }

  double phase = m_animationPhase * 6.283185307179586;
  double orbitAmpLon = qBound(0.30, spanLon * 0.012, 1.60);
  double orbitAmpLat = qBound(0.20, spanLat * 0.010, 1.00);
  centerLon = wrapLongitude(centerLon + orbitAmpLon * std::sin(phase * 0.50));
  centerLat += orbitAmpLat * std::cos(phase * 0.42 + 0.7);

  double breathe = 1.0 + 0.010 * std::sin(phase * 0.45 + 1.2);
  spanLon *= breathe;
  spanLat *= breathe;

  double aspect = qMax(1.10, static_cast<double>(width()) / qMax(1.0, static_cast<double>(height())));
  if (spanLon < spanLat * aspect)
    {
      spanLon = qMin(240.0, spanLat * aspect);
    }
  if (spanLat < spanLon / aspect)
    {
      spanLat = qMin(126.0, spanLon / aspect);
    }

  centerLat = qBound(-90.0 + 0.5 * spanLat, centerLat, 90.0 - 0.5 * spanLat);

  m_targetCenterLon = wrapLongitude(centerLon);
  m_targetCenterLat = centerLat;
  m_targetSpanLon = spanLon;
  m_targetSpanLat = spanLat;
}

void WorldMapWidget::smoothViewport()
{
  double const centerStiffness = 0.11;
  double const centerDamping = 0.83;
  double const spanStiffness = 0.10;
  double const spanDamping = 0.82;

  double deltaLon = wrapLongitude(m_targetCenterLon - m_viewCenterLon);
  m_viewVelocityLon = (m_viewVelocityLon + deltaLon * centerStiffness) * centerDamping;
  m_viewCenterLon = wrapLongitude(m_viewCenterLon + m_viewVelocityLon);

  double deltaLat = m_targetCenterLat - m_viewCenterLat;
  m_viewVelocityLat = (m_viewVelocityLat + deltaLat * centerStiffness) * centerDamping;
  m_viewCenterLat += m_viewVelocityLat;

  double deltaSpanLon = m_targetSpanLon - m_viewSpanLon;
  m_viewVelocitySpanLon = (m_viewVelocitySpanLon + deltaSpanLon * spanStiffness) * spanDamping;
  m_viewSpanLon += m_viewVelocitySpanLon;

  double deltaSpanLat = m_targetSpanLat - m_viewSpanLat;
  m_viewVelocitySpanLat = (m_viewVelocitySpanLat + deltaSpanLat * spanStiffness) * spanDamping;
  m_viewSpanLat += m_viewVelocitySpanLat;

  if (qAbs(deltaLon) < 0.003 && qAbs(m_viewVelocityLon) < 0.003)
    {
      m_viewVelocityLon = 0.0;
    }
  if (qAbs(deltaLat) < 0.003 && qAbs(m_viewVelocityLat) < 0.003)
    {
      m_viewVelocityLat = 0.0;
    }
  if (qAbs(deltaSpanLon) < 0.01 && qAbs(m_viewVelocitySpanLon) < 0.01)
    {
      m_viewVelocitySpanLon = 0.0;
    }
  if (qAbs(deltaSpanLat) < 0.01 && qAbs(m_viewVelocitySpanLat) < 0.01)
    {
      m_viewVelocitySpanLat = 0.0;
    }

  m_viewSpanLon = qBound(45.0, m_viewSpanLon, 360.0);
  m_viewSpanLat = qBound(24.0, m_viewSpanLat, 180.0);
  m_viewCenterLat = qBound(-90.0 + 0.5 * m_viewSpanLat, m_viewCenterLat, 90.0 - 0.5 * m_viewSpanLat);
}

bool WorldMapWidget::computeCircularLongitudeBounds(QVector<double> const& longitudes, double * centerLon, double * spanLon) const
{
  if (!centerLon || !spanLon || longitudes.isEmpty())
    {
      return false;
    }

  QVector<double> lons;
  lons.reserve(longitudes.size());
  for (auto lon : longitudes)
    {
      lon = wrapLongitude(lon);
      if (lon < 0.0) lon += 360.0;
      lons << lon;
    }
  std::sort(lons.begin(), lons.end());

  if (lons.size() == 1)
    {
      double c = lons[0];
      if (c > 180.0) c -= 360.0;
      *centerLon = c;
      *spanLon = 1.0;
      return true;
    }

  double largestGap = -1.0;
  int largestGapIndex = -1;
  int n = lons.size();
  for (int i = 0; i < n; ++i)
    {
      double a = lons[i];
      double b = (i == n - 1) ? (lons[0] + 360.0) : lons[i + 1];
      double gap = b - a;
      if (gap > largestGap)
        {
          largestGap = gap;
          largestGapIndex = i;
        }
    }
  if (largestGapIndex < 0)
    {
      return false;
    }

  double start = (largestGapIndex == n - 1) ? (lons[0] + 360.0) : lons[largestGapIndex + 1];
  double span = 360.0 - largestGap;
  double center = start + 0.5 * span;
  while (center >= 360.0) center -= 360.0;
  if (center > 180.0) center -= 360.0;

  *centerLon = center;
  *spanLon = qMax(0.1, span);
  return true;
}

void WorldMapWidget::pruneExpiredContacts()
{
  auto const nowMs = monotonicNowMs();
  auto const lifetimeMs = static_cast<qint64>(kContactLifetimeSeconds) * 1000;
  auto const downgradeMs = static_cast<qint64>(kRoleDowngradeHoldSeconds) * 1000;
  for (auto it = m_contacts.begin(); it != m_contacts.end(); )
    {
      auto ageMs = nowMs - it.value().lastSeenMonotonicMs;
      if (ageMs > lifetimeMs)
        {
          it = m_contacts.erase(it);
        }
      else
        {
          if (it.value().role == PathRole::IncomingToMe && ageMs > downgradeMs)
            {
              it.value().role = PathRole::BandOnly;
              it.value().queuedDuringTx = false;
            }
          ++it;
        }
    }
}

double WorldMapWidget::wrapLongitude(double lon)
{
  while (lon < -180.0)
    {
      lon += 360.0;
    }
  while (lon > 180.0)
    {
      lon -= 360.0;
    }
  return lon;
}
