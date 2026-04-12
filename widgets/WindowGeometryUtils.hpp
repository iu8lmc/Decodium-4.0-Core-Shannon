#ifndef WINDOW_GEOMETRY_UTILS_HPP
#define WINDOW_GEOMETRY_UTILS_HPP

#include <algorithm>

#include <QGuiApplication>
#include <QRect>
#include <QScreen>
#include <QWidget>

namespace WindowGeometryUtils
{
inline int intersection_area(QRect const& a, QRect const& b)
{
  auto const i = a.intersected(b);
  return i.isValid() ? i.width() * i.height() : 0;
}

inline QRect preferred_available_geometry(QRect const& desired)
{
  auto const screens = QGuiApplication::screens();
  if (screens.isEmpty())
    {
      return QRect {};
    }

  QScreen * best = nullptr;
  int bestArea = -1;
  for (auto * screen : screens)
    {
      if (!screen)
        {
          continue;
        }
      auto const available = screen->availableGeometry();
      int const area = intersection_area(desired, available);
      if (area > bestArea)
        {
          bestArea = area;
          best = screen;
        }
    }

  if (!best)
    {
      best = QGuiApplication::primaryScreen();
    }

  return best ? best->availableGeometry() : QRect {};
}

inline void ensure_window_on_screen(QWidget * widget)
{
  if (!widget || !widget->isWindow())
    {
      return;
    }

  QRect g = widget->geometry();
  if (!g.isValid() || g.width() < 120 || g.height() < 80)
    {
      return;
    }

  auto const available = preferred_available_geometry(g);
  if (!available.isValid() || available.width() < 120 || available.height() < 80)
    {
      return;
    }

  QSize const clampedSize = g.size().boundedTo(available.size());
  int const minX = available.left();
  int const minY = available.top();
  int const maxX = available.right() - clampedSize.width() + 1;
  int const maxY = available.bottom() - clampedSize.height() + 1;

  int const x = std::max(minX, std::min(g.x(), maxX));
  int const y = std::max(minY, std::min(g.y(), maxY));

  if (widget->size() != clampedSize)
    {
      widget->resize(clampedSize);
    }
  widget->move(x, y);
}

inline bool restore_window_geometry(QWidget * widget, QByteArray const& geometry)
{
  bool restored = false;
  if (widget && !geometry.isEmpty())
    {
      restored = widget->restoreGeometry(geometry);
    }
  ensure_window_on_screen(widget);
  return restored;
}
}

#endif // WINDOW_GEOMETRY_UTILS_HPP
