#pragma once
// PanadapterItem — High-resolution SDR panadapter for Decodium3
// QQuickItem + QSGSimpleTextureNode (GPU-native, no CPU rasterizer)
// Ring-buffer waterfall, FFTW 4096-bin spectrum, deskHPSDR palette
// By IU8LMC / Decodium3 — 2025

#include <QQuickItem>
#include <QImage>
#include <QVector>
#include <QMutex>
#include <QTimer>
#include <QStringList>
#include <QColor>

class QSGSimpleTextureNode;

class PanadapterItem : public QQuickItem
{
    Q_OBJECT

    // ── Display range ───────────────────────────────────────────────────────
    Q_PROPERTY(float minDb       READ minDb       WRITE setMinDb       NOTIFY minDbChanged)
    Q_PROPERTY(float maxDb       READ maxDb       WRITE setMaxDb       NOTIFY maxDbChanged)
    Q_PROPERTY(bool  autoRange   READ autoRange   WRITE setAutoRange   NOTIFY autoRangeChanged)

    // ── Spectrum features ───────────────────────────────────────────────────
    Q_PROPERTY(bool  peakHold    READ peakHold    WRITE setPeakHold    NOTIFY peakHoldChanged)
    Q_PROPERTY(float peakDecay   READ peakDecay   WRITE setPeakDecay   NOTIFY peakDecayChanged)
    Q_PROPERTY(int   avgFrames   READ avgFrames   WRITE setAvgFrames   NOTIFY avgFramesChanged)
    Q_PROPERTY(int   spectrumHeight READ spectrumHeight WRITE setSpectrumHeight NOTIFY spectrumHeightChanged)

    // ── Frequency ───────────────────────────────────────────────────────────
    Q_PROPERTY(int   rxFreq      READ rxFreq      WRITE setRxFreq      NOTIFY rxFreqChanged)
    Q_PROPERTY(int   txFreq      READ txFreq      WRITE setTxFreq      NOTIFY txFreqChanged)
    Q_PROPERTY(int   startFreq   READ startFreq   WRITE setStartFreq   NOTIFY startFreqChanged)
    Q_PROPERTY(int   bandwidth   READ bandwidth   WRITE setBandwidth   NOTIFY bandwidthChanged)

    // ── Zoom / Pan ──────────────────────────────────────────────────────────
    Q_PROPERTY(float zoomFactor  READ zoomFactor  WRITE setZoomFactor  NOTIFY zoomFactorChanged)
    Q_PROPERTY(int   panHz       READ panHz       WRITE setPanHz       NOTIFY panHzChanged)

    // ── Palette / style ─────────────────────────────────────────────────────
    Q_PROPERTY(int   paletteIndex READ paletteIndex WRITE setPaletteIndex NOTIFY paletteIndexChanged)
    Q_PROPERTY(QStringList paletteNames READ paletteNames CONSTANT)
    Q_PROPERTY(bool  running     READ running     WRITE setRunning     NOTIFY runningChanged)
    Q_PROPERTY(bool  showTxBrackets READ showTxBrackets WRITE setShowTxBrackets NOTIFY showTxBracketsChanged)
    Q_PROPERTY(int   colorGain   READ colorGain   WRITE setColorGain   NOTIFY colorGainChanged)
    Q_PROPERTY(int   blackLevel  READ blackLevel  WRITE setBlackLevel  NOTIFY blackLevelChanged)

    // ── Decode labels (callsign overlay) ────────────────────────────────────
    Q_PROPERTY(int   labelFontSize      READ labelFontSize      WRITE setLabelFontSize      NOTIFY labelFontSizeChanged)
    Q_PROPERTY(int   labelSpacing       READ labelSpacing       WRITE setLabelSpacing       NOTIFY labelSpacingChanged)
    Q_PROPERTY(bool  labelBold          READ labelBold          WRITE setLabelBold          NOTIFY labelBoldChanged)
    Q_PROPERTY(QColor labelColor        READ labelColor         WRITE setLabelColor         NOTIFY labelColorChanged)
    Q_PROPERTY(bool  labelUseCustomColor READ labelUseCustomColor WRITE setLabelUseCustomColor NOTIFY labelUseCustomColorChanged)

    // ── Read-only status ────────────────────────────────────────────────────
    Q_PROPERTY(float measuredFloor READ measuredFloor NOTIFY measuredFloorChanged)
    Q_PROPERTY(float measuredPeak  READ measuredPeak  NOTIFY measuredPeakChanged)
    Q_PROPERTY(int   fftBins       READ fftBins       CONSTANT)

public:
    explicit PanadapterItem(QQuickItem* parent = nullptr);
    ~PanadapterItem() override;

    // ── Getters ─────────────────────────────────────────────────────────────
    float minDb()          const { return m_minDb; }
    float maxDb()          const { return m_maxDb; }
    bool  autoRange()      const { return m_autoRange; }
    bool  peakHold()       const { return m_peakHold; }
    float peakDecay()      const { return m_peakDecay; }
    int   avgFrames()      const { return m_avgFrames; }
    int   spectrumHeight() const { return m_spectrumH; }
    int   rxFreq()         const { return m_rxFreq; }
    int   txFreq()         const { return m_txFreq; }
    int   startFreq()      const { return m_startFreq; }
    int   bandwidth()      const { return m_bandwidth; }
    float zoomFactor()     const { return m_zoomFactor; }
    int   panHz()          const { return m_panHz; }
    int   paletteIndex()   const { return m_paletteIndex; }
    bool  running()        const { return m_running; }
    bool  showTxBrackets() const { return m_showTxBrackets; }
    float measuredFloor()  const { return m_measuredFloor; }
    float measuredPeak()   const { return m_measuredPeak; }
    int   fftBins()        const { return 4096; }
    QStringList paletteNames() const {
        return {"SDR Classic","Raptor Green","Grayscale","SmartSDR","Hot (SDR#)","deskHPSDR",
                "Aether Default","Aether BlueGreen","Aether Fire","Aether Plasma","FlexRadio"};
    }
    int   colorGain()      const { return m_colorGain; }
    int   blackLevel()     const { return m_blackLevel; }
    int   labelFontSize()  const { return m_labelFontSize; }
    int   labelSpacing()   const { return m_labelSpacing; }
    bool  labelBold()      const { return m_labelBold; }
    QColor labelColor()    const { return m_labelColor; }
    bool  labelUseCustomColor() const { return m_labelUseCustomColor; }

    // ── Setters ─────────────────────────────────────────────────────────────
    void setMinDb(float v)         { if (m_minDb!=v){m_minDb=v;emit minDbChanged();markDirty();} }
    void setMaxDb(float v)         { if (m_maxDb!=v){m_maxDb=v;emit maxDbChanged();markDirty();} }
    void setAutoRange(bool v)      { if (m_autoRange!=v){m_autoRange=v;emit autoRangeChanged();} }
    void setPeakHold(bool v)       { if (m_peakHold!=v){m_peakHold=v;if(!v)m_peakBins.clear();emit peakHoldChanged();} }
    void setPeakDecay(float v)     { if (m_peakDecay!=v){m_peakDecay=v;emit peakDecayChanged();} }
    void setAvgFrames(int v)       { if (m_avgFrames!=v){m_avgFrames=qBound(1,v,32);emit avgFramesChanged();} }
    void setSpectrumHeight(int v)  { if (m_spectrumH!=v){m_spectrumH=v;emit spectrumHeightChanged();markGeomDirty();} }
    void setRxFreq(int v)          { if (m_rxFreq!=v){m_rxFreq=v;emit rxFreqChanged();markDirty();} }
    void setTxFreq(int v)          { if (m_txFreq!=v){m_txFreq=v;emit txFreqChanged();markDirty();} }
    void setStartFreq(int v)       { if (m_startFreq!=v){m_startFreq=v;emit startFreqChanged();markDirty();} }
    void setBandwidth(int v)       { if (m_bandwidth!=v){m_bandwidth=v;emit bandwidthChanged();markDirty();} }
    void setZoomFactor(float v)    { if (m_zoomFactor!=v){m_zoomFactor=qBound(1.0f,v,16.0f);emit zoomFactorChanged();markDirty();} }
    void setPanHz(int v)           { if (m_panHz!=v){m_panHz=v;emit panHzChanged();markDirty();} }
    void setPaletteIndex(int v);
    void setRunning(bool v)        { if (m_running!=v){m_running=v;emit runningChanged();} }
    void setShowTxBrackets(bool v) { if (m_showTxBrackets!=v){m_showTxBrackets=v;emit showTxBracketsChanged();markDirty();} }
    void setColorGain(int v)       { v=qBound(0,v,100); if(m_colorGain!=v){m_colorGain=v;emit colorGainChanged();markDirty();} }
    void setBlackLevel(int v)      { v=qBound(0,v,100); if(m_blackLevel!=v){m_blackLevel=v;emit blackLevelChanged();markDirty();} }
    void setLabelFontSize(int v)   { v=qBound(6,v,24); if(m_labelFontSize!=v){m_labelFontSize=v;emit labelFontSizeChanged();markDirty();} }
    void setLabelSpacing(int v)    { v=qBound(0,v,20); if(m_labelSpacing!=v){m_labelSpacing=v;emit labelSpacingChanged();markDirty();} }
    void setLabelBold(bool v)      { if(m_labelBold!=v){m_labelBold=v;emit labelBoldChanged();markDirty();} }
    void setLabelColor(QColor v)   { if(m_labelColor!=v){m_labelColor=v;emit labelColorChanged();markDirty();} }
    void setLabelUseCustomColor(bool v) { if(m_labelUseCustomColor!=v){m_labelUseCustomColor=v;emit labelUseCustomColorChanged();markDirty();} }

    // ── Invokable methods ───────────────────────────────────────────────────
    // Chiamato dal bridge: dB raw + range dB + range frequenze exact
    Q_INVOKABLE void addSpectrumData(const QVector<float>& dbValues,
                                      float minDb = -130.f, float maxDb = -40.f,
                                      float freqMinHz = 0.f, float freqMaxHz = 0.f);
    // Compatibilità con WaterfallItem (valori normalizzati 0-1)
    Q_INVOKABLE void addSpectrumDataNorm(const QVector<float>& normValues);

    Q_INVOKABLE void resetPeakHold()  { m_peakBins.clear(); markDirty(); }
    Q_INVOKABLE void resetWaterfall();
    // Mostra callsign decodificati sul grafico spettro
    Q_INVOKABLE void setDecodeLabels(const QVariantList& labels);

signals:
    void minDbChanged();
    void maxDbChanged();
    void autoRangeChanged();
    void peakHoldChanged();
    void peakDecayChanged();
    void avgFramesChanged();
    void spectrumHeightChanged();
    void rxFreqChanged();
    void txFreqChanged();
    void startFreqChanged();
    void bandwidthChanged();
    void zoomFactorChanged();
    void panHzChanged();
    void paletteIndexChanged();
    void runningChanged();
    void showTxBracketsChanged();
    void measuredFloorChanged();
    void measuredPeakChanged();
    void frequencySelected(int freq);
    void txFrequencySelected(int freq);
    void colorGainChanged();
    void blackLevelChanged();
    void labelFontSizeChanged();
    void labelSpacingChanged();
    void labelBoldChanged();
    void labelColorChanged();
    void labelUseCustomColorChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
    void geometryChange(const QRectF& newGeom, const QRectF& oldGeom) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev)  override;
    void wheelEvent(QWheelEvent* ev)      override;

private:
    void markDirty()     { m_spectrumDirty = true; update(); }
    void markGeomDirty() { m_geometryDirty = true; m_spectrumDirty = true; update(); }

    void rebuildImages(int w, int h);
    void renderSpectrum();
    void addWaterfallRow();
    void rebuildRgbWaterfallFromIntensity();
    void logWaterfallRenderPath(bool gpu, const QString& reason);
    bool shaderWaterfallSupported();

    // Conversioni frequenza ↔ pixel (rispetta zoom/pan)
    int   freqToX(int freq) const;
    int   xToFreq(int x)   const;

    // Palette waterfall
    void  buildPalette(int idx);
    QRgb  wfColor(float percent) const; // deskHPSDR 9 segmenti

    // ── Dati spettrali ──────────────────────────────────────────────────────
    // Range frequenza EFFETTIVO dei bin ricevuti — unica fonte di verità per le coordinate
    float m_dataFreqMin {200.f};  // Hz del bin[0]
    float m_dataFreqMax {4000.f}; // Hz del bin[nBins-1]
    QVector<float> m_bins;      // valori correnti in dB
    QVector<float> m_peakBins;  // peak hold in dB
    QVector<QVector<float>> m_avgStack; // stack per average
    float m_measuredFloor = -130.f;
    float m_measuredPeak  = -40.f;

    // ── Immagini ────────────────────────────────────────────────────────────
    QImage m_spectrumImage;  // spectrum (larghezza × spectrumH)
    QImage m_waterfallImage; // waterfall (larghezza × waterfallH)
    QImage m_waterfallDisplayImage; // waterfall lineare pronta per upload GPU
    QImage m_waterfallIntensityImage; // intensità 0..255 per shader palette
    QImage m_waterfallIntensityDisplayImage; // intensità lineare pronta per upload GPU
    QImage m_waterfallIntensityTextureImage; // atlas RGBA: palette + intensità per shader GPU
    int    m_wfWriteRow = 0; // riga corrente ring buffer
    QVector<QRgb> m_palette; // 256 colori waterfall

    // ── Configurazione ──────────────────────────────────────────────────────
    float m_minDb        = -130.f;
    float m_maxDb        = -40.f;
    bool  m_autoRange    = true;
    bool  m_peakHold     = true;
    float m_peakDecay    = 0.97f;
    int   m_avgFrames    = 1;
    int   m_spectrumH    = 150;
    int   m_rxFreq       = 1500;
    int   m_txFreq       = 1500;
    int   m_startFreq    = 200;
    int   m_bandwidth    = 3800;
    float m_zoomFactor   = 1.0f;
    int   m_panHz        = 0;
    int   m_paletteIndex = 3;   // deskHPSDR default
    bool  m_running      = false;
    bool  m_showTxBrackets = true;

    int   m_colorGain    = 50;
    int   m_blackLevel   = 15;
    QVariantList m_decodeLabels;  // [{call:"IU8LMC",freq:1500,snr:-5,isCQ:true}, ...]

    // ── Stile label callsign (overlay spettro) ──────────────────────────────
    int    m_labelFontSize       = 8;
    int    m_labelSpacing        = 2;
    bool   m_labelBold           = true;
    QColor m_labelColor          = QColor(0, 230, 255);
    bool   m_labelUseCustomColor = false;

    // ── Stato rendering ─────────────────────────────────────────────────────
    bool  m_spectrumDirty = true;
    bool  m_geometryDirty = true;
    bool  m_useShaderWaterfall = false;
    bool  m_shaderWaterfallBlocked = false;
    bool  m_waterfallRgbValid = true;
    int   m_loggedWaterfallPath = -1;
    int   m_loggedWaterfallApi = -1;
    QString m_loggedWaterfallReason;
    QString m_shaderWaterfallDisabledReason;
    bool  m_loggedWaterfallGpuUploadStats = false;
    int   m_lastWaterfallGpuStatsRow = -1;
    int   m_paletteGeneration = 0;
    QMutex m_mutex;
};
