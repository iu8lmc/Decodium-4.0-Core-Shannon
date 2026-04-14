#include "WaterfallItem.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>

WaterfallItem::WaterfallItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setOpaquePainting(true);
    setAntialiasing(false);
    initPaletteTurbo();  // default: Google Turbo — il migliore per SDR
}

// ─── Palette helpers ────────────────────────────────────────────────────────
static inline QRgb rgbF(double r, double g, double b) {
    return qRgb(qBound(0,(int)(r*255),255), qBound(0,(int)(g*255),255), qBound(0,(int)(b*255),255));
}

void WaterfallItem::initPalette()  // 0 — SDR Classic: dark-blue → cyan → green → yellow → orange
{
    m_palette.resize(256);
    for (int i = 0; i < 256; ++i) {
        float t = i / 255.0f;
        if      (t < 0.2f) { float s=t/0.2f;       m_palette[i]=rgbF(0,0,0.1+0.4*s); }
        else if (t < 0.4f) { float s=(t-0.2f)/0.2f; m_palette[i]=rgbF(0,s*0.8,0.5+0.3*s); }
        else if (t < 0.6f) { float s=(t-0.4f)/0.2f; m_palette[i]=rgbF(0,0.8+0.2*s,0.8*(1.0-s)); }
        else if (t < 0.8f) { float s=(t-0.6f)/0.2f; m_palette[i]=rgbF(s,1.0,0); }
        else                { float s=(t-0.8f)/0.2f; m_palette[i]=rgbF(1.0,1.0-s,0); }
    }
}

// ─── deskHPSDR palette (portata da waterfall.c, Copyright G0ORX/N6LYT + DL1BZ) ──────────────
// Identica all'originale GPL: 9 segmenti nero→blu→ciano→verde→giallo→rosso→magenta→bianco
static inline QRgb hpsdrWaterfallColor(float percent)
{
    if (percent <= 0.0f) return qRgb(0,0,0);
    if (percent >= 1.0f) return qRgb(255,255,0); // oltre soglia: giallo saturo

    if (percent < 0.222222f) {
        float s = percent * 4.5f;
        return qRgb(0, 0, (int)(s * 255));
    } else if (percent < 0.333333f) {
        float s = (percent - 0.222222f) * 9.0f;
        return qRgb(0, (int)(s * 255), 255);
    } else if (percent < 0.444444f) {
        float s = (percent - 0.333333f) * 9.0f;
        return qRgb(0, 255, (int)((1.0f - s) * 255));
    } else if (percent < 0.555555f) {
        float s = (percent - 0.444444f) * 9.0f;
        return qRgb((int)(s * 255), 255, 0);
    } else if (percent < 0.777777f) {
        float s = (percent - 0.555555f) * 4.5f;
        return qRgb(255, (int)((1.0f - s) * 255), 0);
    } else if (percent < 0.888888f) {
        float s = (percent - 0.777777f) * 9.0f;
        return qRgb(255, 0, (int)(s * 255));
    } else {
        float s = (percent - 0.888888f) * 9.0f;
        return qRgb((int)((0.75f + 0.25f * (1.0f - s)) * 255),
                    (int)(s * 255 * 0.5f),
                    255);
    }
}

void WaterfallItem::initPaletteTurbo()  // 3 — deskHPSDR (9 segmenti, GPL G0ORX/DL1BZ)
{
    m_palette.resize(256);
    for (int i = 0; i < 256; ++i)
        m_palette[i] = hpsdrWaterfallColor(i / 255.0f);
}

void WaterfallItem::initPaletteHot()  // 4 — Hot (SDR# classic: black→red→orange→yellow→white)
{
    m_palette.resize(256);
    for (int i = 0; i < 256; ++i) {
        float t = i / 255.0f;
        float r = qMin(1.0f, t * 3.0f);
        float g = qMax(0.0f, qMin(1.0f, t * 3.0f - 1.0f));
        float b = qMax(0.0f, t * 3.0f - 2.0f);
        m_palette[i] = rgbF(r, g, b);
    }
}

QColor WaterfallItem::spectrumColor(float value) const
{
    int idx = valueToIdx(value);
    return QColor(m_palette[idx]);
}

int WaterfallItem::freqToX(int freq) const
{
    if (m_bandwidth <= 0) return 0;
    return static_cast<int>((freq - m_startFreq) * width() / m_bandwidth);
}

int WaterfallItem::xToFreq(int x) const
{
    if (width() <= 0) return m_startFreq;
    return m_startFreq + x * m_bandwidth / static_cast<int>(width());
}

void WaterfallItem::paint(QPainter *painter)
{
    int w = static_cast<int>(width());
    int h = static_cast<int>(height());
    if (w <= 0 || h <= 0) return;

    painter->setRenderHint(QPainter::Antialiasing, false);
    // deskHPSDR background: COLOUR_PAN_BACKGND = (0.15, 0.15, 0.15) = #262626
    painter->fillRect(0, 0, w, h, QColor(38, 38, 38));

    drawSpectrum(painter, 0, m_spectrumHeight);

    int wfY = m_spectrumHeight;
    int wfH = h - m_spectrumHeight;
    drawWaterfall(painter, wfY, wfH);

    drawMarkers(painter, 0, h);

    if (m_showTxBrackets)
        drawTxBrackets(painter, 0, h);

    if (!m_foxSlotMarkers.isEmpty())
        drawFoxSlotMarkers(painter, 0, h);

    painter->setPen(QColor("#808890"));
    painter->setFont(QFont("Consolas", 8));
    int step = 500;
    if (m_bandwidth > 3000) step = 500;
    else if (m_bandwidth > 1000) step = 200;
    else step = 100;

    for (int f = ((m_startFreq / step) + 1) * step; f < m_startFreq + m_bandwidth; f += step) {
        int x = freqToX(f);
        painter->drawLine(x, m_spectrumHeight - 5, x, m_spectrumHeight);
        painter->drawText(x - 20, m_spectrumHeight - 7, 40, 12, Qt::AlignCenter, QString::number(f));
    }
}

void WaterfallItem::drawSpectrum(QPainter *painter, int y, int h)
{
    // ── deskHPSDR panadapter style (GPL G0ORX/DL1BZ, ported to Qt6) ──────
    QMutexLocker lock(&m_mutex);
    if (m_currentSpectrum.isEmpty() || h < 4) return;

    int w = static_cast<int>(width());
    int nBins = m_currentSpectrum.size();

    float lo = m_autoRange ? m_noiseFloorEma : (m_zero - 50) / 50.0f;
    float hi = m_autoRange ? m_peakEma       : lo + m_gain / 50.0f;
    float range = (hi - lo) < 1e-6f ? 1e-6f : (hi - lo);

    // ── griglia dB orizzontale (stile deskHPSDR: 5 righe bianche trasp.) ──
    painter->setFont(QFont("FreeSans", 8));
    for (int step = 1; step <= 4; ++step) {
        float norm = step / 5.0f;
        int gy = y + h - static_cast<int>(norm * (h - 2));
        painter->setPen(QPen(QColor(255, 255, 255, 35), 1, Qt::DotLine));
        painter->drawLine(0, gy, w, gy);
        // label S-meter style
        painter->setPen(QColor(255, 255, 255, 100));
        painter->drawText(2, gy - 1, QString("S%1").arg(step * 2));
    }

    // ── costruisci path spettro ───────────────────────────────────────────
    QPainterPath path;
    path.moveTo(0, y + h);
    bool first = true;
    for (int x = 0; x < w; ++x) {
        int bin = x * nBins / w;
        if (bin >= nBins) bin = nBins - 1;
        float val = m_currentSpectrum[bin];
        float norm = qBound(0.0f, (val - lo) / range, 1.0f);
        int py = y + h - 1 - static_cast<int>(norm * (h - 2));
        if (first) { path.lineTo(x, py); first = false; }
        else        { path.lineTo(x, py); }
    }
    path.lineTo(w, y + h);
    path.closeSubpath();

    // ── fill gradiente deskHPSDR: verde→arancio→giallo→rosso (COLOUR_GRAD1-4) ──
    // Gradiente verticale: segnale forte in alto (rosso) → noise in basso (verde)
    QLinearGradient grad(0, y, 0, y + h);
    // COLOUR_GRAD4 = rosso (top = segnale forte)
    grad.setColorAt(0.00, QColor(255,   0,   0, 180));   // COLOUR_GRAD4
    // COLOUR_GRAD3 = giallo
    grad.setColorAt(0.30, QColor(255, 255,   0, 150));   // COLOUR_GRAD3
    // COLOUR_GRAD2 = arancio
    grad.setColorAt(0.55, QColor(255, 168,   0, 100));   // COLOUR_GRAD2
    // COLOUR_GRAD1 = verde (bottom = noise floor)
    grad.setColorAt(1.00, QColor(  0, 255,   0,  40));   // COLOUR_GRAD1
    painter->fillPath(path, grad);

    // ── linea spettro: COLOUR_PAN_LINE = ciano (0, 1, 1) ─────────────────
    QPainterPath linePath;
    first = true;
    for (int x = 0; x < w; ++x) {
        int bin = x * nBins / w;
        if (bin >= nBins) bin = nBins - 1;
        float val = m_currentSpectrum[bin];
        float norm = qBound(0.0f, (val - lo) / range, 1.0f);
        int py = y + h - 1 - static_cast<int>(norm * (h - 2));
        if (first) { linePath.moveTo(x, py); first = false; }
        else        { linePath.lineTo(x, py); }
    }
    // PAN_LINE_THICK = 1.0 (linea principale), COLOUR_PAN_LINE = ciano puro
    painter->setPen(QPen(QColor(0, 255, 255, 255), 1.0));
    painter->drawPath(linePath);

    // ── noise floor marker (auto mode) — linea tratteggiata gialla ────────
    if (m_autoRange) {
        painter->setPen(QPen(QColor(255, 255, 0, 80), 1, Qt::DashLine));
        painter->drawLine(0, y + h - 2, w, y + h - 2);
    }
}

void WaterfallItem::drawWaterfall(QPainter *painter, int y, int h)
{
    if (m_waterfallImage.isNull()) return;
    QRect target(0, y, static_cast<int>(width()), h);
    QRect source(0, 0, m_waterfallImage.width(), m_waterfallImage.height());
    painter->drawImage(target, m_waterfallImage, source);
}

void WaterfallItem::drawMarkers(QPainter *painter, int y, int h)
{
    int rxX = freqToX(m_rxFreq);
    painter->setPen(QPen(QColor("#ff3333"), 2));
    painter->drawLine(rxX, y, rxX, y + h);

    if (m_txFreq != m_rxFreq) {
        int txX = freqToX(m_txFreq);
        painter->setPen(QPen(QColor("#00ff00"), 2));
        painter->drawLine(txX, y, txX, y + h);
    }

    painter->setPen(QColor("#ff3333"));
    painter->setFont(QFont("Consolas", 9, QFont::Bold));
    painter->drawText(rxX + 3, y + 12, QString("RX %1").arg(m_rxFreq));

    if (m_txFreq != m_rxFreq) {
        int txX = freqToX(m_txFreq);
        painter->setPen(QColor("#00ff00"));
        painter->drawText(txX + 3, y + 24, QString("TX %1").arg(m_txFreq));
    }
}

void WaterfallItem::drawTxBrackets(QPainter *painter, int y, int h)
{
    Q_UNUSED(h)
    int txX = freqToX(m_txFreq);
    int halfBw = freqToX(m_startFreq + 50) - freqToX(m_startFreq);
    int bracketLeft = txX - halfBw;
    int bracketRight = txX + halfBw;
    int bracketH = 16;

    QPen bracketPen(QColor("#ff3333"), 2, Qt::SolidLine);
    painter->setPen(bracketPen);

    painter->drawLine(bracketLeft, y + 2, bracketLeft, y + bracketH);
    painter->drawLine(bracketLeft, y + 2, bracketLeft + 4, y + 2);
    painter->drawLine(bracketLeft, y + bracketH, bracketLeft + 4, y + bracketH);

    painter->drawLine(bracketRight, y + 2, bracketRight, y + bracketH);
    painter->drawLine(bracketRight, y + 2, bracketRight - 4, y + 2);
    painter->drawLine(bracketRight, y + bracketH, bracketRight - 4, y + bracketH);

    painter->setFont(QFont("Consolas", 7));
    painter->drawText(bracketLeft + 6, y + bracketH - 3, "TX");
}

void WaterfallItem::drawFoxSlotMarkers(QPainter *painter, int y, int h)
{
    QPen foxPen(QColor("#FF9800"), 1, Qt::DashLine);
    painter->setPen(foxPen);

    for (const auto &marker : m_foxSlotMarkers) {
        int freq = marker.toInt();
        int x = freqToX(freq);
        painter->drawLine(x, y, x, y + h);
    }

    if (!m_foxSlotMarkers.isEmpty()) {
        painter->setFont(QFont("Consolas", 7));
        painter->setPen(QColor("#FF9800"));
        int x0 = freqToX(m_foxSlotMarkers.first().toInt());
        painter->drawText(x0 + 3, y + m_spectrumHeight + 12, "FOX");
    }
}

void WaterfallItem::updateAutoRange(const QVector<float> &data)
{
    if (data.isEmpty()) return;
    // Copia e ordina per calcolare percentili
    QVector<float> sorted = data;
    std::sort(sorted.begin(), sorted.end());
    int n = sorted.size();
    float floor5  = sorted[qBound(0, n * 5  / 100, n-1)];
    float peak99  = sorted[qBound(0, n * 99 / 100, n-1)];
    if (!m_autoRangeInit) {
        m_noiseFloorEma = floor5;
        m_peakEma       = peak99;
        m_autoRangeInit = true;
    } else {
        m_noiseFloorEma = FLOOR_ALPHA * floor5  + (1.0f - FLOOR_ALPHA) * m_noiseFloorEma;
        m_peakEma       = PEAK_ALPHA  * peak99  + (1.0f - PEAK_ALPHA)  * m_peakEma;
    }
    emit noiseFloorChanged();
    emit peakLevelChanged();
}

void WaterfallItem::initPaletteRaptor()  // 1 — Raptor Military Green
{
    m_palette.resize(256);
    for (int i = 0; i < 256; ++i) {
        float t = i / 255.0f;
        if      (t < 0.25f) { float s=t/0.25f;       m_palette[i]=rgbF(0,0.05+0.1*s,0); }
        else if (t < 0.5f)  { float s=(t-0.25f)/0.25f;m_palette[i]=rgbF(0.1*s,0.15+0.35*s,0.05*s); }
        else if (t < 0.75f) { float s=(t-0.5f)/0.25f; m_palette[i]=rgbF(0.1+0.4*s,0.5+0.3*s,0.05+0.15*s); }
        else                { float s=(t-0.75f)/0.25f; m_palette[i]=rgbF(0.5+0.5*s,0.8+0.2*s,0.2+0.3*s); }
    }
}

void WaterfallItem::initPaletteGrayscale()  // 2 — Grayscale
{
    m_palette.resize(256);
    for (int i = 0; i < 256; ++i)
        m_palette[i] = qRgb(i, i, i);
}

void WaterfallItem::setShowTxBrackets(bool v)
{
    if (m_showTxBrackets != v) { m_showTxBrackets = v; emit showTxBracketsChanged(); update(); }
}

void WaterfallItem::setPaletteIndex(int idx)
{
    if (m_paletteIndex != idx && idx >= 0 && idx <= 4) {
        m_paletteIndex = idx;
        switch (idx) {
        case 0: initPalette();         break;
        case 1: initPaletteRaptor();   break;
        case 2: initPaletteGrayscale();break;
        case 3: initPaletteTurbo();    break;
        case 4: initPaletteHot();      break;
        }
        emit paletteIndexChanged();
        update();
    }
}

void WaterfallItem::setFoxSlotMarkers(const QVariantList &markers)
{
    if (m_foxSlotMarkers != markers) {
        m_foxSlotMarkers = markers;
        emit foxSlotMarkersChanged();
        update();
    }
}

void WaterfallItem::setSpectrumMode(int mode)
{
    if (m_spectrumMode != mode && mode >= 0 && mode <= 2) {
        m_spectrumMode = mode;
        m_cumulativeSpectrum.clear();
        emit spectrumModeChanged();
        update();
    }
}

void WaterfallItem::addSpectrumData(const QVector<float> &data)
{
    QMutexLocker lock(&m_mutex);

    if (m_spectrumMode == 1) {
        if (m_cumulativeSpectrum.size() != data.size()) {
            m_cumulativeSpectrum = data;
        } else {
            for (int i = 0; i < data.size(); ++i)
                m_cumulativeSpectrum[i] = qMax(m_cumulativeSpectrum[i], data[i]);
        }
        m_currentSpectrum = m_cumulativeSpectrum;
    } else if (m_spectrumMode == 2) {
        if (m_cumulativeSpectrum.size() != data.size()) {
            m_cumulativeSpectrum = data;
        } else {
            const float alpha = 0.3f;
            for (int i = 0; i < data.size(); ++i)
                m_cumulativeSpectrum[i] = alpha * data[i] + (1.0f - alpha) * m_cumulativeSpectrum[i];
        }
        m_currentSpectrum = m_cumulativeSpectrum;
    } else {
        m_currentSpectrum = data;
    }

    int w = static_cast<int>(width());
    int wfH = static_cast<int>(height()) - m_spectrumHeight;
    if (w <= 0 || wfH <= 0) return;

    if (m_waterfallImage.isNull() || m_waterfallImage.width() != w || m_waterfallImage.height() != wfH) {
        m_waterfallImage = QImage(w, wfH, QImage::Format_RGB32);
        m_waterfallImage.fill(QColor("#0a0e18"));
    }

    if (wfH > 1) {
        int bytesPerLine = m_waterfallImage.bytesPerLine();
        uchar *bits = m_waterfallImage.bits();
        std::memmove(bits + bytesPerLine, bits, bytesPerLine * (wfH - 1));
    }

    // ── Auto-scaling robusto: percentili 5°/99° con smooth IIR ──────────────
    // (adattato da deskHPSDR: scala ai dati reali, non solo dBm)
    int nBins = data.size();
    float wf_low, wf_high;

    if (m_autoRange && nBins > 0) {
        // Calcola 5° e 99° percentile come deskHPSDR con smooth IIR
        QVector<float> sorted = data;
        std::sort(sorted.begin(), sorted.end());
        float floor5  = sorted[qBound(0, nBins *  5 / 100, nBins - 1)];
        float peak99  = sorted[qBound(0, nBins * 99 / 100, nBins - 1)];
        // Margine: come deskHPSDR, abbassa il floor di un 10% del range
        float margin = (peak99 - floor5) * 0.10f;
        float new_low  = floor5 - margin;
        float new_high = peak99 + margin * 0.5f;

        if (!m_autoRangeInit) {
            m_noiseFloorEma = new_low;
            m_peakEma = new_high;
            m_autoRangeInit = true;
        } else {
            m_noiseFloorEma = FLOOR_ALPHA * new_low  + (1.0f - FLOOR_ALPHA) * m_noiseFloorEma;
            m_peakEma       = PEAK_ALPHA  * new_high + (1.0f - PEAK_ALPHA)  * m_peakEma;
        }
        emit noiseFloorChanged();
        emit peakLevelChanged();
    }
    wf_low  = m_noiseFloorEma;
    wf_high = m_peakEma;
    float rangei = (wf_high - wf_low) < 1e-6f ? 1e6f : 1.0f / (wf_high - wf_low);

    if (nBins > 0) {
        QRgb *line = reinterpret_cast<QRgb*>(m_waterfallImage.scanLine(0));
        for (int x = 0; x < w; ++x) {
            int bin = x * nBins / w;
            if (bin >= nBins) bin = nBins - 1;
            // Usa hpsdrWaterfallColor con scala deskHPSDR
            float pct = (data[bin] - wf_low) * rangei;
            if (m_paletteIndex == 3) {
                // Palette deskHPSDR: usa la funzione originale diretta
                line[x] = hpsdrWaterfallColor(pct);
            } else {
                line[x] = m_palette[valueToIdx(data[bin])];
            }
        }
    }

    lock.unlock();
    update();
}

void WaterfallItem::scrollWaterfall()
{
    if (m_waterfallImage.isNull()) return;
    int h = m_waterfallImage.height();
    int bytesPerLine = m_waterfallImage.bytesPerLine();
    for (int y = h - 1; y > 0; --y)
        memcpy(m_waterfallImage.scanLine(y), m_waterfallImage.scanLine(y - 1), bytesPerLine);
}

void WaterfallItem::mousePressEvent(QMouseEvent *event)
{
    int freq = xToFreq(static_cast<int>(event->position().x()));
    if (event->button() == Qt::LeftButton) {
        setTxFreq(freq);           // Sinistro = TX
        emit txFrequencySelected(freq);
    } else if (event->button() == Qt::RightButton) {
        setRxFreq(freq);           // Destro = RX
        emit frequencySelected(freq);
    }
}

void WaterfallItem::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        int freq = xToFreq(static_cast<int>(event->position().x()));
        setTxFreq(freq);           // Drag sinistro = TX
        emit txFrequencySelected(freq);
    } else if (event->buttons() & Qt::RightButton) {
        int freq = xToFreq(static_cast<int>(event->position().x()));
        setRxFreq(freq);           // Drag destro = RX
        emit frequencySelected(freq);
    }
}

void WaterfallItem::setRxFreq(int f)
{
    if (m_rxFreq != f) { m_rxFreq = f; emit rxFreqChanged(); update(); }
}

void WaterfallItem::setTxFreq(int f)
{
    if (m_txFreq != f) { m_txFreq = f; emit txFreqChanged(); update(); }
}

void WaterfallItem::setStartFreq(int f)
{
    if (m_startFreq != f) { m_startFreq = f; emit startFreqChanged(); update(); }
}

void WaterfallItem::setBandwidth(int bw)
{
    if (m_bandwidth != bw) { m_bandwidth = bw; emit bandwidthChanged(); update(); }
}

void WaterfallItem::setGain(int g)
{
    if (m_gain != g) { m_gain = g; emit gainChanged(); update(); }
}

void WaterfallItem::setZero(int z)
{
    if (m_zero != z) { m_zero = z; emit zeroChanged(); update(); }
}

void WaterfallItem::setRunning(bool r)
{
    if (m_running != r) { m_running = r; emit runningChanged(); }
}
