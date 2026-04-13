#pragma once
// AetherWaterfall — Standalone waterfall + spectrum widget extracted from AetherSDR.
// Pure QPainter rendering, no GPU/QRhi dependencies. Requires Qt6 Core/Widgets/Gui.

#include <QWidget>
#include <QVector>
#include <QImage>
#include <QColor>

// ─── Waterfall color scheme presets ──────────────────────────────────────────

enum class WfColorScheme : int {
    Default = 0,   // black -> dark blue -> blue -> cyan -> green -> yellow -> red
    Grayscale,     // black -> white
    BlueGreen,     // black -> blue -> teal -> green -> white
    Fire,          // black -> red -> orange -> yellow -> white
    Plasma,        // black -> purple -> magenta -> orange -> yellow
    Count
};

struct WfGradientStop { float pos; int r, g, b; };

const WfGradientStop* wfSchemeStops(WfColorScheme scheme, int& count);

// ─── AetherWaterfall widget ──────────────────────────────────────────────────

class AetherWaterfall : public QWidget {
    Q_OBJECT

public:
    explicit AetherWaterfall(QWidget* parent = nullptr);

    QSize sizeHint() const override { return {800, 300}; }

    // Render the current state to a QImage (for QQuickPaintedItem integration)
    void renderToImage(QImage* target);

    // ── Public API ───────────────────────────────────────────────────────────

    // Feed one waterfall row from FFT data (dBm per bin).
    // minDb/maxDb set the mapping range for this row.
    void addRow(const QVector<float>& dbValues, float minDb, float maxDb);

    // Color scheme: 0=Default, 1=Grayscale, 2=BlueGreen, 3=Fire, 4=Plasma
    void setColorScheme(int scheme);
    int  colorScheme() const { return static_cast<int>(m_colorScheme); }

    // Auto-black: track noise floor and set black threshold automatically.
    void setAutoBlack(bool enabled);
    bool autoBlack() const { return m_autoBlack; }

    // Waterfall blanker: suppress impulse noise rows.
    void setBlankerEnabled(bool enabled);
    void setBlankerThreshold(float threshold);
    bool blankerEnabled() const { return m_blankerEnabled; }
    float blankerThreshold() const { return m_blankerThreshold; }

    // Reference level (top of spectrum, dBm) and dynamic range (dB span).
    void setRefLevel(float dbm);
    void setDynRange(float db);
    float refLevel() const { return m_refLevel; }
    float dynRange() const { return m_dynRange; }

    // Color gain (0-100) and black level (0-100) for manual tuning.
    void setColorGain(int gain);
    void setBlackLevel(int level);
    int  colorGain() const { return m_colorGain; }
    int  blackLevel() const { return m_blackLevel; }

    // Frequency range for the horizontal axis labels.
    void setFrequencyRange(float minHz, float maxHz);

    // RX/TX marker frequencies (Hz). Set to 0 to hide.
    void setRxFrequency(int hz);
    void setTxFrequency(int hz);

    // Spectrum/waterfall split ratio (0.1 - 0.9). Default 0.40.
    void setSpectrumFrac(float frac);
    float spectrumFrac() const { return m_spectrumFrac; }

signals:
    // Emitted when the user clicks on the waterfall or spectrum area.
    void frequencyClicked(int hz);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    // ── Drawing helpers ──────────────────────────────────────────────────────
    void drawGrid(QPainter& p, const QRect& r);
    void drawSpectrum(QPainter& p, const QRect& r);
    void drawWaterfall(QPainter& p, const QRect& r);
    void drawFreqScale(QPainter& p, const QRect& r);

    // ── Waterfall row push (ring buffer) ─────────────────────────────────────
    void pushWaterfallRow(const QVector<float>& bins, int destWidth);

    // ── Color mapping ────────────────────────────────────────────────────────
    QRgb dbmToRgb(float dbm) const;

    // ── Coordinate helpers ───────────────────────────────────────────────────
    int    hzToX(float hz) const;
    float  xToHz(int x) const;

    // ── FFT data ─────────────────────────────────────────────────────────────
    QVector<float> m_bins;         // raw FFT frame (dBm)
    QVector<float> m_smoothed;     // exponential-smoothed for display
    static constexpr float kSmoothAlpha = 0.35f;

    // ── Frequency range ──────────────────────────────────────────────────────
    float m_minHz{14.1e6f};
    float m_maxHz{14.35e6f};

    // ── Markers ──────────────────────────────────────────────────────────────
    int m_rxFreqHz{0};
    int m_txFreqHz{0};

    // ── Display parameters ───────────────────────────────────────────────────
    float m_refLevel{-50.0f};
    float m_dynRange{100.0f};
    int   m_colorGain{50};
    int   m_blackLevel{15};
    float m_spectrumFrac{0.40f};

    // ── Waterfall colour range (set per addRow call) ─────────────────────────
    float m_wfMinDbm{-130.0f};
    float m_wfMaxDbm{-50.0f};

    // ── Waterfall ring buffer ────────────────────────────────────────────────
    QImage m_waterfall;
    int    m_wfWriteRow{0};

    // ── Color scheme ─────────────────────────────────────────────────────────
    WfColorScheme m_colorScheme{WfColorScheme::Default};

    // ── Auto-black ───────────────────────────────────────────────────────────
    bool  m_autoBlack{true};
    float m_autoBlackThresh{-120.0f};

    // ── Blanker ──────────────────────────────────────────────────────────────
    bool  m_blankerEnabled{false};
    float m_blankerThreshold{1.15f};
    static constexpr int kBlankerRingSize = 32;
    float m_blankerRing[kBlankerRingSize]{};
    int   m_blankerRingIdx{0};
    int   m_blankerRingCount{0};
    QVector<QRgb> m_lastGoodRow;

    // ── Layout constants ─────────────────────────────────────────────────────
    static constexpr int kFreqScaleH = 20;
    static constexpr int kDividerH   = 4;
};
