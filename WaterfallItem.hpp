#ifndef WATERFALLITEM_HPP
#define WATERFALLITEM_HPP

#include <QQuickPaintedItem>
#include <QImage>
#include <QColor>
#include <QVector>
#include <QVariantList>
#include <QMutex>

class WaterfallItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(int rxFreq READ rxFreq WRITE setRxFreq NOTIFY rxFreqChanged)
    Q_PROPERTY(int txFreq READ txFreq WRITE setTxFreq NOTIFY txFreqChanged)
    Q_PROPERTY(int startFreq READ startFreq WRITE setStartFreq NOTIFY startFreqChanged)
    Q_PROPERTY(int bandwidth READ bandwidth WRITE setBandwidth NOTIFY bandwidthChanged)
    Q_PROPERTY(int gain READ gain WRITE setGain NOTIFY gainChanged)
    Q_PROPERTY(int zero READ zero WRITE setZero NOTIFY zeroChanged)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(bool showTxBrackets READ showTxBrackets WRITE setShowTxBrackets NOTIFY showTxBracketsChanged)
    Q_PROPERTY(int paletteIndex READ paletteIndex WRITE setPaletteIndex NOTIFY paletteIndexChanged)
    Q_PROPERTY(QVariantList foxSlotMarkers READ foxSlotMarkers WRITE setFoxSlotMarkers NOTIFY foxSlotMarkersChanged)
    Q_PROPERTY(int spectrumMode READ spectrumMode WRITE setSpectrumMode NOTIFY spectrumModeChanged)
    Q_PROPERTY(int spectrumHeight READ spectrumHeight WRITE setSpectrumHeight NOTIFY spectrumHeightChanged)
    Q_PROPERTY(bool autoRange   READ autoRange   WRITE setAutoRange   NOTIFY autoRangeChanged)
    Q_PROPERTY(float noiseFloor READ noiseFloor  NOTIFY noiseFloorChanged)
    Q_PROPERTY(float peakLevel  READ peakLevel   NOTIFY peakLevelChanged)
    Q_PROPERTY(QStringList paletteNames READ paletteNames CONSTANT)

public:
    explicit WaterfallItem(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    int rxFreq() const { return m_rxFreq; }
    void setRxFreq(int f);
    int txFreq() const { return m_txFreq; }
    void setTxFreq(int f);
    int startFreq() const { return m_startFreq; }
    void setStartFreq(int f);
    int bandwidth() const { return m_bandwidth; }
    void setBandwidth(int bw);
    int gain() const { return m_gain; }
    void setGain(int g);
    int zero() const { return m_zero; }
    void setZero(int z);
    bool running() const { return m_running; }
    void setRunning(bool r);

    bool showTxBrackets() const { return m_showTxBrackets; }
    void setShowTxBrackets(bool v);
    int paletteIndex() const { return m_paletteIndex; }
    void setPaletteIndex(int idx);
    QVariantList foxSlotMarkers() const { return m_foxSlotMarkers; }
    void setFoxSlotMarkers(const QVariantList &markers);

    int spectrumMode() const { return m_spectrumMode; }
    void setSpectrumMode(int mode);

    int spectrumHeight() const { return m_spectrumHeight; }
    void setSpectrumHeight(int h) {
        if (m_spectrumHeight != h) { m_spectrumHeight = h; emit spectrumHeightChanged(); update(); }
    }

    bool autoRange()  const { return m_autoRange; }
    void setAutoRange(bool v) { if (m_autoRange!=v){m_autoRange=v; emit autoRangeChanged(); update();} }
    float noiseFloor() const { return m_noiseFloorEma; }
    float peakLevel()  const { return m_peakEma; }
    QStringList paletteNames() const {
        return {"SDR Classic","Raptor Green","Grayscale","deskHPSDR","Hot (SDR#)"};
    }

    Q_INVOKABLE void addSpectrumData(const QVector<float> &data);

signals:
    void rxFreqChanged();
    void txFreqChanged();
    void startFreqChanged();
    void bandwidthChanged();
    void gainChanged();
    void zeroChanged();
    void runningChanged();
    void showTxBracketsChanged();
    void paletteIndexChanged();
    void foxSlotMarkersChanged();
    void spectrumModeChanged();
    void spectrumHeightChanged();
    void frequencySelected(int freq);
    void autoRangeChanged();
    void noiseFloorChanged();
    void peakLevelChanged();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void drawSpectrum(QPainter *painter, int y, int h);
    void drawSpectrumDbmGrid(QPainter *painter, int y, int h);
    void drawWaterfall(QPainter *painter, int y, int h);
    void drawMarkers(QPainter *painter, int y, int h);
    void drawTxBrackets(QPainter *painter, int y, int h);
    void drawFoxSlotMarkers(QPainter *painter, int y, int h);
    void scrollWaterfall();
    QColor spectrumColor(float value) const;
    int freqToX(int freq) const;
    int xToFreq(int x) const;
    void updateAutoRange(const QVector<float> &data);
    inline int valueToIdx(float v) const {
        float lo = m_autoRange ? m_noiseFloorEma : (m_zero - 50) / 50.0f;
        float hi = m_autoRange ? m_peakEma       : (m_zero + (m_gain - 50)) / 50.0f;
        float range = hi - lo;
        if (range < 1e-6f) range = 1e-6f;
        return qBound(0, static_cast<int>((v - lo) / range * 255.0f), 255);
    }

    int m_rxFreq = 1500;
    int m_txFreq = 1500;
    int m_startFreq = 200;
    int m_bandwidth = 4000;
    int m_gain = 50;
    int m_zero = 50;
    bool m_running = false;
    bool m_showTxBrackets = true;
    int m_paletteIndex = 3;   // default: Google Turbo
    QVariantList m_foxSlotMarkers;
    int m_spectrumMode = 0;

    // Auto noise floor (IIR smoothing — come SDRangel)
    bool  m_autoRange    = true;
    float m_noiseFloorEma = 0.0f;  // EMA del noise floor (5° percentile)
    float m_peakEma      = 1.0f;   // EMA del picco (99° percentile)
    static constexpr float FLOOR_ALPHA = 0.05f; // lenta risposta (20 frame)
    static constexpr float PEAK_ALPHA  = 0.10f;
    bool  m_autoRangeInit = false;

    QImage m_waterfallImage;
    QVector<float> m_currentSpectrum;
    QVector<float> m_cumulativeSpectrum;
    QMutex m_mutex;
    int m_spectrumHeight = 80;

    QVector<QRgb> m_palette;  // pre-computata come QRgb per max performance
    void initPalette();
    void initPaletteRaptor();
    void initPaletteGrayscale();
    void initPaletteTurbo();
    void initPaletteHot();
};

#endif // WATERFALLITEM_HPP
