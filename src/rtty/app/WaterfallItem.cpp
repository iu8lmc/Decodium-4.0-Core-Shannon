#include "app/WaterfallItem.h"

#include <QPainter>

#include <cstring>

#include <algorithm>
#include <cmath>

namespace decortty::app {

namespace {

// Interpolate through a small set of stops. Cheaper and easier to tune than a
// formula, and it keeps each palette's character explicit.
QRgb rampColour(double t, const QVector<QColor>& stops)
{
    t = std::clamp(t, 0.0, 1.0);
    if (stops.size() < 2)
        return qRgb(0, 0, 0);

    const double scaled = t * (stops.size() - 1);
    const int    index  = std::min(static_cast<int>(scaled), static_cast<int>(stops.size()) - 2);
    const double frac   = scaled - index;

    const QColor& a = stops[index];
    const QColor& b = stops[index + 1];
    return qRgb(static_cast<int>(a.red()   + frac * (b.red()   - a.red())),
                static_cast<int>(a.green() + frac * (b.green() - a.green())),
                static_cast<int>(a.blue()  + frac * (b.blue()  - a.blue())));
}

const QVector<QColor>& paletteStops(int colorMap)
{
    // Deep blue → cyan → green → amber → white: the DecoRTTY house palette,
    // matching the interface accents so a strong signal reads as the same green
    // the rest of the application uses for "good".
    static const QVector<QColor> kDeco = {
        QColor("#050A14"), QColor("#0A1F3C"), QColor("#12507A"),
        QColor("#00D4FF"), QColor("#00FF88"), QColor("#FFD24A"), QColor("#FFFFFF")
    };
    // Classic grey scale, for operators who find colour distracting.
    static const QVector<QColor> kMono = {
        QColor("#000000"), QColor("#303030"), QColor("#808080"), QColor("#FFFFFF")
    };
    // High-contrast, for daylight screens.
    static const QVector<QColor> kFire = {
        QColor("#000000"), QColor("#5A0033"), QColor("#C21807"),
        QColor("#FF8C00"), QColor("#FFE066"), QColor("#FFFFFF")
    };

    switch (colorMap) {
    case 1:  return kMono;
    case 2:  return kFire;
    default: return kDeco;
    }
}

} // namespace

WaterfallItem::WaterfallItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setFlag(ItemHasContents, true);
    // The waterfall is opaque and covers its whole rectangle, so there is no
    // reason to make the scene graph blend it.
    setOpaquePainting(true);
}

void WaterfallItem::setMinDb(double db) { if (!qFuzzyCompare(m_minDb, db)) { m_minDb = db; emit rangeChanged(); } }
void WaterfallItem::setMaxDb(double db) { if (!qFuzzyCompare(m_maxDb, db)) { m_maxDb = db; emit rangeChanged(); } }

void WaterfallItem::setStartHz(double hz)
{
    if (qFuzzyCompare(m_startHz, hz))
        return;
    m_startHz = hz;
    emit rangeChanged();
    update();
}

void WaterfallItem::setEndHz(double hz)
{
    if (qFuzzyCompare(m_endHz, hz))
        return;
    m_endHz = hz;
    emit rangeChanged();
    update();
}

void WaterfallItem::setMarkHz(double hz)
{
    if (qFuzzyCompare(m_markHz, hz))
        return;
    m_markHz = hz;
    emit markersChanged();
    update();
}

void WaterfallItem::setShiftHz(double hz)
{
    if (qFuzzyCompare(m_shiftHz, hz))
        return;
    m_shiftHz = hz;
    emit markersChanged();
    update();
}

void WaterfallItem::setMarkerColor(const QColor& colour)
{
    if (m_markerColor == colour)
        return;
    m_markerColor = colour;
    emit markersChanged();
    update();
}

void WaterfallItem::setColorMap(int colorMap)
{
    if (m_colorMap == colorMap)
        return;
    m_colorMap = colorMap;
    emit colorMapChanged();
    update();
}

void WaterfallItem::setNotchHz(double hz)
{
    if (qFuzzyCompare(m_notchHz, hz))
        return;
    m_notchHz = hz;
    emit markersChanged();
    update();
}

void WaterfallItem::setNotchEnabled(bool on)
{
    if (m_notchEnabled == on)
        return;
    m_notchEnabled = on;
    emit markersChanged();
    update();
}

void WaterfallItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        rebuildImage();
}

void WaterfallItem::rebuildImage()
{
    const int w = std::max(1, static_cast<int>(width()));
    const int h = std::max(1, static_cast<int>(height()));
    if (m_image.width() == w && m_image.height() == h)
        return;

    QImage fresh(w, h, QImage::Format_RGB32);
    fresh.fill(paletteStops(m_colorMap).first());
    if (!m_image.isNull()) {
        // Keep what is already on screen when the panel is resized, rather than
        // blanking the operator's history.
        QPainter painter(&fresh);
        painter.drawImage(0, 0, m_image.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }
    m_image = fresh;
    update();
}

QRgb WaterfallItem::colourFor(double db) const
{
    const double span = std::max(1.0, m_maxDb - m_minDb);
    return rampColour((db - m_minDb) / span, paletteStops(m_colorMap));
}

void WaterfallItem::pushSpectrum(const QVector<double>& spectrumDb, double binHz)
{
    if (spectrumDb.isEmpty() || binHz <= 0.0)
        return;
    if (m_image.isNull())
        rebuildImage();
    if (m_image.isNull())
        return;

    const int w = m_image.width();
    const int h = m_image.height();

    // Scroll down by one row, then draw the new spectrum at the top.
    if (h > 1) {
        std::memmove(m_image.scanLine(1), m_image.scanLine(0),
                     static_cast<size_t>(m_image.bytesPerLine()) * static_cast<size_t>(h - 1));
    }

    auto* row = reinterpret_cast<QRgb*>(m_image.scanLine(0));
    const double hzPerPixel = (m_endHz - m_startHz) / std::max(1, w);

    for (int x = 0; x < w; ++x) {
        const double hz = m_startHz + x * hzPerPixel;
        // Take the strongest bin covering this pixel: with more bins than
        // pixels, averaging would hide the narrow RTTY tones we most want to see.
        const int first = static_cast<int>(hz / binHz);
        const int last  = std::max(first, static_cast<int>((hz + hzPerPixel) / binHz));
        double peak = m_minDb;
        for (int bin = first; bin <= last && bin < spectrumDb.size(); ++bin) {
            if (bin >= 0)
                peak = std::max(peak, spectrumDb.at(bin));
        }
        row[x] = colourFor(peak);
    }

    update();
}

void WaterfallItem::clear()
{
    if (!m_image.isNull())
        m_image.fill(paletteStops(m_colorMap).first());
    update();
}

double WaterfallItem::frequencyAt(double x) const
{
    const double w = std::max(1.0, width());
    return m_startHz + (x / w) * (m_endHz - m_startHz);
}

void WaterfallItem::paint(QPainter* painter)
{
    if (m_image.isNull()) {
        painter->fillRect(boundingRect(), paletteStops(m_colorMap).first());
        return;
    }

    painter->drawImage(0, 0, m_image);

    // Tone markers: two vertical lines at mark and space, with the passband
    // between them tinted. This is what the operator actually tunes by.
    const double span = m_endHz - m_startHz;
    if (span <= 0.0)
        return;

    const double w = width();
    const double markX  = (m_markHz - m_startHz) / span * w;
    const double spaceX = ((m_markHz - m_shiftHz) - m_startHz) / span * w;

    QColor fill = m_markerColor;
    fill.setAlpha(38);
    painter->fillRect(QRectF(std::min(markX, spaceX), 0.0,
                             std::abs(markX - spaceX), height()), fill);

    QPen pen(m_markerColor);
    pen.setWidthF(1.0);
    painter->setPen(pen);
    painter->drawLine(QPointF(markX, 0.0), QPointF(markX, height()));
    painter->drawLine(QPointF(spaceX, 0.0), QPointF(spaceX, height()));

    if (m_notchEnabled) {
        const double notchX = (m_notchHz - m_startHz) / span * w;
        QPen notchPen(QColor(0xFF, 0x4D, 0x6D));
        notchPen.setWidthF(1.0);
        notchPen.setStyle(Qt::DashLine);
        painter->setPen(notchPen);
        painter->drawLine(QPointF(notchX, 0.0), QPointF(notchX, height()));
    }
}

} // namespace decortty::app
