// AetherWaterfall — Standalone waterfall + spectrum widget extracted from AetherSDR.
// Pure QPainter rendering, no GPU/QRhi dependencies. Requires Qt6 Core/Widgets/Gui.

#include "AetherWaterfall.h"

#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QMouseEvent>
#include <cmath>
#include <cstring>
#include <algorithm>

// ─── Waterfall color scheme gradient presets (from AetherSDR) ────────────────

static constexpr WfGradientStop kDefaultStops[] = {
    {0.00f,   0,   0,   0},   // black
    {0.15f,   0,   0, 128},   // dark blue
    {0.30f,   0,  64, 255},   // blue
    {0.45f,   0, 200, 255},   // cyan
    {0.60f,   0, 220,   0},   // green
    {0.80f, 255, 255,   0},   // yellow
    {1.00f, 255,   0,   0},   // red
};
static constexpr WfGradientStop kGrayscaleStops[] = {
    {0.00f,   0,   0,   0},
    {1.00f, 255, 255, 255},
};
static constexpr WfGradientStop kBlueGreenStops[] = {
    {0.00f,   0,   0,   0},
    {0.25f,   0,  30, 120},
    {0.50f,   0, 100, 180},
    {0.75f,   0, 200, 130},
    {1.00f, 220, 255, 220},
};
static constexpr WfGradientStop kFireStops[] = {
    {0.00f,   0,   0,   0},
    {0.25f, 128,   0,   0},
    {0.50f, 220,  80,   0},
    {0.75f, 255, 200,   0},
    {1.00f, 255, 255, 220},
};
static constexpr WfGradientStop kPlasmaStops[] = {
    {0.00f,   0,   0,   0},
    {0.25f,  80,   0, 140},
    {0.50f, 200,   0, 120},
    {0.75f, 240, 120,   0},
    {1.00f, 255, 255,  80},
};

const WfGradientStop* wfSchemeStops(WfColorScheme scheme, int& count)
{
    switch (scheme) {
    case WfColorScheme::Grayscale: count = 2; return kGrayscaleStops;
    case WfColorScheme::BlueGreen: count = 5; return kBlueGreenStops;
    case WfColorScheme::Fire:      count = 5; return kFireStops;
    case WfColorScheme::Plasma:    count = 5; return kPlasmaStops;
    default:                       count = 7; return kDefaultStops;
    }
}

// Interpolate a normalized value t (0-1) through the given gradient stops.
static QRgb interpolateGradient(float t, const WfGradientStop* stops, int n)
{
    t = qBound(0.0f, t, 1.0f);
    int i = 0;
    while (i < n - 2 && stops[i + 1].pos < t) ++i;
    const float seg = (stops[i + 1].pos > stops[i].pos)
        ? (t - stops[i].pos) / (stops[i + 1].pos - stops[i].pos)
        : 0.0f;
    const int r = static_cast<int>(stops[i].r + seg * (stops[i + 1].r - stops[i].r));
    const int g = static_cast<int>(stops[i].g + seg * (stops[i + 1].g - stops[i].g));
    const int b = static_cast<int>(stops[i].b + seg * (stops[i + 1].b - stops[i].b));
    return qRgb(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255));
}

// ─── Constructor ─────────────────────────────────────────────────────────────

AetherWaterfall::AetherWaterfall(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);
}

// ─── Public API ──────────────────────────────────────────────────────────────

void AetherWaterfall::addRow(const QVector<float>& dbValues, float minDb, float maxDb)
{
    if (dbValues.isEmpty()) return;

    m_wfMinDbm = minDb;
    m_wfMaxDbm = maxDb;

    // Exponential smoothing for the spectrum line
    if (m_smoothed.size() != dbValues.size()) {
        m_smoothed = dbValues;
    } else {
        for (int i = 0; i < dbValues.size(); ++i) {
            m_smoothed[i] = kSmoothAlpha * dbValues[i]
                          + (1.0f - kSmoothAlpha) * m_smoothed[i];
        }
    }
    m_bins = dbValues;

    // Auto-black: track the noise floor using a trimmed mean.
    if (m_autoBlack) {
        // Pass 1: overall mean (sampled every 8th bin for speed)
        float sum = 0;
        int count = 0;
        for (int i = 0; i < dbValues.size(); i += 8) {
            sum += dbValues[i];
            count++;
        }
        if (count > 0) {
            const float mean = sum / count;
            // Pass 2: mean of bins at or below overall mean (filters out signals)
            float noiseSum = 0;
            int noiseCount = 0;
            for (int i = 0; i < dbValues.size(); i += 8) {
                if (dbValues[i] <= mean) {
                    noiseSum += dbValues[i];
                    noiseCount++;
                }
            }
            const float noiseFloor = (noiseCount > 0) ? (noiseSum / noiseCount) : mean;
            // Smooth to prevent jitter
            m_autoBlackThresh = 0.95f * m_autoBlackThresh + 0.05f * noiseFloor;
        }
    }

    // Crea il waterfall image se non esiste o dimensione cambiata
    {
        int w = width();
        int h = height();
        if (w <= 0) w = 800;
        if (h <= 0) h = 300;
        int chromeH = kFreqScaleH + kDividerH;
        int wfH = static_cast<int>((h - chromeH) * (1.0f - m_spectrumFrac));
        if (wfH < 10) wfH = 100;
        if (m_waterfall.isNull() || m_waterfall.width() != w || m_waterfall.height() != wfH) {
            m_waterfall = QImage(w, wfH, QImage::Format_RGB32);
            m_waterfall.fill(Qt::black);
            m_wfWriteRow = 0;
        }
    }

    // Push to waterfall ring buffer
    pushWaterfallRow(dbValues, m_waterfall.width());

    update();
}

void AetherWaterfall::setColorScheme(int scheme)
{
    auto clamped = static_cast<WfColorScheme>(
        std::clamp(scheme, 0, static_cast<int>(WfColorScheme::Count) - 1));
    m_colorScheme = clamped;
    update();
}

void AetherWaterfall::setAutoBlack(bool enabled)
{
    m_autoBlack = enabled;
}

void AetherWaterfall::setBlankerEnabled(bool enabled)
{
    m_blankerEnabled = enabled;
    if (!enabled) {
        m_blankerRingCount = 0;
        m_blankerRingIdx = 0;
    }
}

void AetherWaterfall::setBlankerThreshold(float threshold)
{
    m_blankerThreshold = std::clamp(threshold, 1.05f, 2.0f);
}

void AetherWaterfall::setRefLevel(float dbm)
{
    m_refLevel = dbm;
    update();
}

void AetherWaterfall::setDynRange(float db)
{
    m_dynRange = std::max(db, 1.0f);
    update();
}

void AetherWaterfall::setColorGain(int gain)
{
    m_colorGain = std::clamp(gain, 0, 100);
    update();
}

void AetherWaterfall::setBlackLevel(int level)
{
    m_blackLevel = std::clamp(level, 0, 100);
    update();
}

void AetherWaterfall::setFrequencyRange(float minHz, float maxHz)
{
    if (maxHz <= minHz) return;
    m_minHz = minHz;
    m_maxHz = maxHz;
    update();
}

void AetherWaterfall::setRxFrequency(int hz)
{
    m_rxFreqHz = hz;
    update();
}

void AetherWaterfall::setTxFrequency(int hz)
{
    m_txFreqHz = hz;
    update();
}

void AetherWaterfall::setSpectrumFrac(float frac)
{
    m_spectrumFrac = std::clamp(frac, 0.10f, 0.90f);
    // Rebuild waterfall image for new split
    const int chromeH  = kFreqScaleH + kDividerH;
    const int contentH = height() - chromeH;
    const int wfHeight = static_cast<int>(contentH * (1.0f - m_spectrumFrac));
    if (wfHeight > 0 && width() > 0) {
        QImage newWf(width(), wfHeight, QImage::Format_RGB32);
        newWf.fill(Qt::black);
        if (!m_waterfall.isNull()) {
            newWf = m_waterfall.scaled(width(), wfHeight,
                        Qt::IgnoreAspectRatio, Qt::FastTransformation);
        }
        m_waterfall = std::move(newWf);
        m_wfWriteRow = 0;
    }
    update();
}

// ─── Coordinate helpers ──────────────────────────────────────────────────────

int AetherWaterfall::hzToX(float hz) const
{
    const float bw = m_maxHz - m_minHz;
    if (bw <= 0.0f) return -1;
    const float px = (hz - m_minHz) / bw * width();
    if (std::isnan(px) || std::isinf(px)) return -1;
    return static_cast<int>(std::clamp(px, -1.0e6f, 1.0e6f));
}

float AetherWaterfall::xToHz(int x) const
{
    return m_minHz + (static_cast<float>(x) / width()) * (m_maxHz - m_minHz);
}

// ─── Color mapping (from AetherSDR SpectrumWidget::dbmToRgb) ────────────────

QRgb AetherWaterfall::dbmToRgb(float dbm) const
{
    float effectiveMin;
    float visRange;

    if (m_autoBlack) {
        // Auto-black mode: use tracked noise floor as the black threshold
        effectiveMin = m_autoBlackThresh;
        visRange = std::max(1.0f, 120.0f - m_colorGain * 0.91f);
    } else {
        // Manual mode: black_level and color_gain control the mapping
        // black_level 0-100: higher = floor moves closer to signals (more black)
        // color_gain 0-100: higher = narrower visible range = more contrast
        const float floorShift = (125 - m_blackLevel) * 0.4f;
        visRange = 80.0f - m_colorGain * 0.7f;
        effectiveMin = m_wfMinDbm + floorShift;
    }
    const float effectiveMax = effectiveMin + visRange;

    const float t = qBound(0.0f, (dbm - effectiveMin) / (effectiveMax - effectiveMin), 1.0f);

    int n = 0;
    const auto* stops = wfSchemeStops(m_colorScheme, n);
    return interpolateGradient(t, stops, n);
}

// ─── Waterfall ring buffer row push (from AetherSDR) ─────────────────────────

void AetherWaterfall::pushWaterfallRow(const QVector<float>& bins, int destWidth)
{
    if (m_waterfall.isNull() || destWidth <= 0) return;

    const int h = m_waterfall.height();
    if (h <= 1) return;

    // Map bins to a scanline of RGB values
    QVector<QRgb> scanline(destWidth, qRgb(0, 0, 0));
    for (int x = 0; x < destWidth; ++x) {
        const int binIdx = x * bins.size() / destWidth;
        const float dbm = (binIdx >= 0 && binIdx < bins.size()) ? bins[binIdx] : m_wfMinDbm;
        scanline[x] = dbmToRgb(dbm);
    }

    // ── NB Waterfall Blanker: suppress impulse rows ──────────────────────
    if (m_blankerEnabled) {
        // Compute row mean
        float rowSum = 0.0f;
        for (int i = 0; i < bins.size(); ++i) {
            rowSum += bins[i];
        }
        const float rowMean = bins.isEmpty() ? 0.0f : (rowSum / bins.size());

        // Rolling baseline from ring buffer
        float baseline = 0.0f;
        for (int i = 0; i < m_blankerRingCount; ++i) {
            baseline += m_blankerRing[i];
        }
        if (m_blankerRingCount > 0) {
            baseline /= m_blankerRingCount;
        }

        // Detect impulse (need at least 8 rows of history)
        if (m_blankerRingCount >= 8 && baseline > 0.0f
                && rowMean > baseline * m_blankerThreshold) {
            // Impulse detected: replace with last good row
            if (m_lastGoodRow.size() == destWidth) {
                scanline = m_lastGoodRow;
            } else {
                const QRgb floorColor = dbmToRgb(baseline);
                std::fill(scanline.begin(), scanline.end(), floorColor);
            }
            // Clamp ring entry to avoid poisoning baseline
            m_blankerRing[m_blankerRingIdx] = std::min(rowMean, baseline * 1.05f);
        } else {
            m_lastGoodRow = scanline;
            m_blankerRing[m_blankerRingIdx] = rowMean;
        }
        m_blankerRingIdx = (m_blankerRingIdx + 1) % kBlankerRingSize;
        if (m_blankerRingCount < kBlankerRingSize) {
            ++m_blankerRingCount;
        }
    }

    // Ring buffer: write new row at m_wfWriteRow, decrement and wrap
    uchar* imgBits = m_waterfall.bits();
    const qsizetype bpl = m_waterfall.bytesPerLine();

    m_wfWriteRow = (m_wfWriteRow - 1 + h) % h;
    auto* row = reinterpret_cast<QRgb*>(imgBits + m_wfWriteRow * bpl);
    std::memcpy(row, scanline.constData(), destWidth * sizeof(QRgb));
}

// ─── Resize ──────────────────────────────────────────────────────────────────

void AetherWaterfall::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);

    const int chromeH  = kFreqScaleH + kDividerH;
    const int contentH = height() - chromeH;
    const int wfHeight = static_cast<int>(contentH * (1.0f - m_spectrumFrac));
    if (wfHeight > 0 && width() > 0) {
        QImage newWf(width(), wfHeight, QImage::Format_RGB32);
        newWf.fill(Qt::black);
        if (!m_waterfall.isNull()) {
            newWf = m_waterfall.scaled(width(), wfHeight,
                        Qt::IgnoreAspectRatio, Qt::FastTransformation);
        }
        m_waterfall = std::move(newWf);
        m_wfWriteRow = 0;
    }
}

// ─── Mouse ───────────────────────────────────────────────────────────────────

void AetherWaterfall::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton) {
        const float hz = xToHz(static_cast<int>(ev->position().x()));
        emit frequencyClicked(static_cast<int>(std::round(hz)));
        ev->accept();
    }
}

// ─── Paint ───────────────────────────────────────────────────────────────────

void AetherWaterfall::renderToImage(QImage* target)
{
    if (!target || target->isNull()) return;
    QPainter p(target);
    p.setRenderHint(QPainter::Antialiasing, false);

    int w = target->width(), h = target->height();
    if (w <= 0 || h <= kFreqScaleH + kDividerH + 2) return;

    const int chromeH  = kFreqScaleH + kDividerH;
    const int contentH = h - chromeH;
    const int specH    = static_cast<int>(contentH * m_spectrumFrac);
    const int wfH      = contentH - specH;
    const int divY     = specH;
    const int scaleY   = specH + kDividerH;
    const int wfY      = scaleY + kFreqScaleH;

    const QRect specRect (0, 0,      w, specH);
    const QRect divRect  (0, divY,   w, kDividerH);
    const QRect scaleRect(0, scaleY, w, kFreqScaleH);
    const QRect wfRect   (0, wfY,    w, wfH);

    p.fillRect(specRect, QColor(0x0a, 0x0a, 0x14));
    drawGrid(p, specRect);
    drawSpectrum(p, specRect);
    p.fillRect(divRect, QColor(0x18, 0x28, 0x38));
    drawFreqScale(p, scaleRect);
    drawWaterfall(p, wfRect);

    if (m_rxFreqHz > 0) {
        int rx = hzToX(static_cast<float>(m_rxFreqHz));
        if (rx >= 0 && rx < w) { p.setPen(QPen(QColor(0xff,0x80,0x00),2)); p.drawLine(rx, specRect.top(), rx, wfRect.bottom()); }
    }
    if (m_txFreqHz > 0) {
        int tx = hzToX(static_cast<float>(m_txFreqHz));
        if (tx >= 0 && tx < w) { p.setPen(QPen(QColor(0xff,0x20,0x20),2)); p.drawLine(tx, specRect.top(), tx, wfRect.bottom()); }
    }
}

void AetherWaterfall::paintEvent(QPaintEvent* ev)
{
    Q_UNUSED(ev);
    if (width() <= 0 || height() <= kFreqScaleH + kDividerH + 2) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int chromeH  = kFreqScaleH + kDividerH;
    const int contentH = height() - chromeH;
    const int specH    = static_cast<int>(contentH * m_spectrumFrac);
    const int wfH      = contentH - specH;

    const int divY     = specH;
    const int scaleY   = specH + kDividerH;
    const int wfY      = scaleY + kFreqScaleH;

    const QRect specRect (0, 0,      width(), specH);
    const QRect divRect  (0, divY,   width(), kDividerH);
    const QRect scaleRect(0, scaleY, width(), kFreqScaleH);
    const QRect wfRect   (0, wfY,    width(), wfH);

    // ── Spectrum background ──────────────────────────────────────────────
    p.fillRect(specRect, QColor(0x0a, 0x0a, 0x14));

    // ── Grid ─────────────────────────────────────────────────────────────
    drawGrid(p, specRect);

    // ── Spectrum line ────────────────────────────────────────────────────
    drawSpectrum(p, specRect);

    // ── Divider bar ──────────────────────────────────────────────────────
    p.fillRect(divRect, QColor(0x18, 0x28, 0x38));
    p.setPen(QColor(0x30, 0x40, 0x50));
    p.drawLine(divRect.left(), divRect.center().y(),
               divRect.right(), divRect.center().y());

    // ── Frequency scale ──────────────────────────────────────────────────
    drawFreqScale(p, scaleRect);

    // ── Waterfall ────────────────────────────────────────────────────────
    drawWaterfall(p, wfRect);

    // ── RX marker (orange vertical line) ─────────────────────────────────
    if (m_rxFreqHz > 0) {
        const int rx = hzToX(static_cast<float>(m_rxFreqHz));
        if (rx >= 0 && rx < width()) {
            p.setPen(QPen(QColor(0xff, 0x80, 0x00), 2));
            p.drawLine(rx, specRect.top(), rx, wfRect.bottom());
        }
    }

    // ── TX marker (red vertical line) ────────────────────────────────────
    if (m_txFreqHz > 0) {
        const int tx = hzToX(static_cast<float>(m_txFreqHz));
        if (tx >= 0 && tx < width()) {
            p.setPen(QPen(QColor(0xff, 0x20, 0x20), 2));
            p.drawLine(tx, specRect.top(), tx, wfRect.bottom());
        }
    }
}

// ─── Grid (from AetherSDR SpectrumWidget::drawGrid) ──────────────────────────

void AetherWaterfall::drawGrid(QPainter& p, const QRect& r)
{
    const int w = r.width();
    const int h = r.height();

    // Horizontal dB grid lines
    float rawDbStep = m_dynRange / 5.0f;
    float dbStep;
    if      (rawDbStep >= 20.0f) dbStep = 20.0f;
    else if (rawDbStep >= 10.0f) dbStep = 10.0f;
    else if (rawDbStep >= 5.0f)  dbStep = 5.0f;
    else                          dbStep = 2.0f;

    const float bottomDbm = m_refLevel - m_dynRange;
    const float firstDb = std::ceil(bottomDbm / dbStep) * dbStep;
    p.setPen(QPen(QColor(0x20, 0x30, 0x40), 1, Qt::DotLine));
    for (float dbm = firstDb; dbm <= m_refLevel; dbm += dbStep) {
        const float frac = (m_refLevel - dbm) / m_dynRange;
        const int y = r.top() + static_cast<int>(frac * h);
        p.drawLine(0, y, w, y);
    }

    // Vertical frequency grid lines (1-2-5 sequence)
    const float bwHz = m_maxHz - m_minHz;
    if (bwHz <= 0) return;
    const double rawStep  = static_cast<double>(bwHz) / 5.0;
    const double gridMag  = std::pow(10.0, std::floor(std::log10(rawStep)));
    const double gridNorm = rawStep / gridMag;
    double gridStep;
    if      (gridNorm >= 5.0) gridStep = 5.0 * gridMag;
    else if (gridNorm >= 2.0) gridStep = 2.0 * gridMag;
    else                      gridStep = 1.0 * gridMag;

    const double firstLine = std::ceil(static_cast<double>(m_minHz) / gridStep) * gridStep;
    p.setPen(QPen(QColor(0x20, 0x30, 0x40), 1, Qt::DotLine));
    for (double f = firstLine; f <= m_maxHz; f += gridStep) {
        const int x = hzToX(static_cast<float>(f));
        p.drawLine(x, r.top(), x, r.bottom());
    }
}

// ─── Spectrum line (from AetherSDR SpectrumWidget::drawSpectrum) ─────────────

void AetherWaterfall::drawSpectrum(QPainter& p, const QRect& r)
{
    if (m_smoothed.isEmpty()) {
        p.setPen(QColor(0x00, 0x60, 0x80));
        p.drawText(r, Qt::AlignCenter, "No data");
        return;
    }

    const int w = r.width();
    const int h = r.height();
    const int n = m_smoothed.size();

    // Heat map color: blue -> cyan -> green -> yellow -> red
    auto heatColor = [](float t) -> QColor {
        float cr, cg, cb;
        if (t < 0.25f) {
            float s = t / 0.25f;
            cr = 0.0f; cg = s; cb = 1.0f;
        } else if (t < 0.5f) {
            float s = (t - 0.25f) / 0.25f;
            cr = 0.0f; cg = 1.0f; cb = 1.0f - s;
        } else if (t < 0.75f) {
            float s = (t - 0.5f) / 0.25f;
            cr = s; cg = 1.0f; cb = 0.0f;
        } else {
            float s = (t - 0.75f) / 0.25f;
            cr = 1.0f; cg = 1.0f - s; cb = 0.0f;
        }
        return QColor::fromRgbF(cr, cg, cb);
    };

    // Pre-compute positions and normalized levels
    struct Pt { int x, y; float t; };
    QVector<Pt> pts(n);
    for (int i = 0; i < n; ++i) {
        const float dbm  = m_smoothed[i];
        const float norm = qBound(0.0f, (m_refLevel - dbm) / m_dynRange, 1.0f);
        pts[i].x = r.left() + static_cast<int>(static_cast<float>(i) / n * w);
        pts[i].y = r.top()  + qMin(static_cast<int>(norm * h), h - 1);
        pts[i].t = 1.0f - norm;  // 0=noise floor, 1=strong signal
    }

    p.setRenderHint(QPainter::Antialiasing, true);

    // Heat map fill: per-column vertical gradient from heat color to dark blue
    const int bottom = r.bottom();
    for (int i = 0; i < n - 1; ++i) {
        QPolygonF trapezoid;
        trapezoid << QPointF(pts[i].x, pts[i].y)
                  << QPointF(pts[i+1].x, pts[i+1].y)
                  << QPointF(pts[i+1].x, bottom)
                  << QPointF(pts[i].x, bottom);

        float avgT = (pts[i].t + pts[i+1].t) * 0.5f;
        QColor top = heatColor(avgT);
        top.setAlphaF(0.7f * 0.3f);
        QColor bot(0, 0, 77, static_cast<int>(255 * 0.7f));
        QLinearGradient grad(0, qMin(pts[i].y, pts[i+1].y), 0, bottom);
        grad.setColorAt(0.0, top);
        grad.setColorAt(1.0, bot);
        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawPolygon(trapezoid);
    }

    // Heat map line: per-segment coloring
    for (int i = 0; i < n - 1; ++i) {
        float avgT = (pts[i].t + pts[i+1].t) * 0.5f;
        p.setPen(QPen(heatColor(avgT), 1.5));
        p.drawLine(pts[i].x, pts[i].y, pts[i+1].x, pts[i+1].y);
    }

    p.setRenderHint(QPainter::Antialiasing, false);
}

// ─── Waterfall (ring buffer blit, from AetherSDR) ────────────────────────────

void AetherWaterfall::drawWaterfall(QPainter& p, const QRect& r)
{
    if (m_waterfall.isNull()) {
        p.fillRect(r, Qt::black);
        return;
    }

    // Ring buffer rendering: m_wfWriteRow is the newest row.
    // Draw in two halves: [writeRow..end] then [0..writeRow)
    const int h = m_waterfall.height();
    const int topRows = h - m_wfWriteRow;
    const int botRows = m_wfWriteRow;

    if (topRows >= h) {
        // writeRow == 0, no split needed
        p.drawImage(r, m_waterfall);
    } else {
        const double scale = static_cast<double>(r.height()) / h;
        const int topH = static_cast<int>(topRows * scale);
        const int botH = r.height() - topH;

        // Top part: newest rows (from writeRow to end of image)
        p.drawImage(QRect(r.x(), r.y(), r.width(), topH),
                    m_waterfall,
                    QRect(0, m_wfWriteRow, m_waterfall.width(), topRows));

        // Bottom part: older rows (from start of image to writeRow)
        if (botRows > 0 && botH > 0) {
            p.drawImage(QRect(r.x(), r.y() + topH, r.width(), botH),
                        m_waterfall,
                        QRect(0, 0, m_waterfall.width(), botRows));
        }
    }
}

// ─── Frequency scale (from AetherSDR SpectrumWidget::drawFreqScale) ──────────

void AetherWaterfall::drawFreqScale(QPainter& p, const QRect& r)
{
    p.fillRect(r, QColor(0x06, 0x06, 0x10));

    const float bwHz = m_maxHz - m_minHz;
    if (bwHz <= 0) return;

    // Pick a step using 1-2-5 sequence to give ~5-10 labels
    const double rawStep = static_cast<double>(bwHz) / 5.0;
    const double mag  = std::pow(10.0, std::floor(std::log10(rawStep)));
    const double norm = rawStep / mag;
    double stepHz;
    if      (norm >= 5.0) stepHz = 5.0 * mag;
    else if (norm >= 2.0) stepHz = 2.0 * mag;
    else                  stepHz = 1.0 * mag;

    const double firstLine = std::ceil(static_cast<double>(m_minHz) / stepHz) * stepHz;

    QFont f = p.font();
    f.setPointSize(8);
    p.setFont(f);
    const QFontMetrics fm(f);

    // Format: decide between Hz, kHz, MHz display
    auto formatFreq = [](double hz) -> QString {
        const double absHz = std::abs(hz);
        if (absHz >= 1e6) {
            // MHz with enough decimals
            double mhz = hz / 1e6;
            int decimals;
            if (absHz < 1e7)       decimals = 6;
            else if (absHz < 1e8)  decimals = 5;
            else                    decimals = 3;
            return QString::number(mhz, 'f', decimals);
        } else if (absHz >= 1e3) {
            return QString("%1k").arg(hz / 1e3, 0, 'f', 1);
        } else {
            return QString("%1").arg(static_cast<int>(hz));
        }
    };

    for (double freq = firstLine; freq <= m_maxHz; freq += stepHz) {
        const int x = hzToX(static_cast<float>(freq));

        // Tick mark
        p.setPen(QColor(0x40, 0x60, 0x80));
        p.drawLine(x, r.top(), x, r.top() + 4);

        const QString label = formatFreq(freq);
        const int tw = fm.horizontalAdvance(label);
        const int lx = qBound(0, x - tw / 2, width() - tw);

        p.setPen(QColor(0x70, 0x90, 0xb0));
        p.drawText(lx, r.bottom() - 2, label);
    }
}
