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
#include <QMetaObject>
#include <atomic>

class QSGNode;
class QSGSimpleTextureNode;
class QQuickWindow;

class PanadapterItem : public QQuickItem
{
    Q_OBJECT

    // ── Display range ───────────────────────────────────────────────────────
    Q_PROPERTY(float minDb       READ minDb       WRITE setMinDb       NOTIFY minDbChanged)
    Q_PROPERTY(float maxDb       READ maxDb       WRITE setMaxDb       NOTIFY maxDbChanged)
    Q_PROPERTY(bool  autoRange   READ autoRange   WRITE setAutoRange   NOTIFY autoRangeChanged)

    // ── Spectrum features ───────────────────────────────────────────────────
    Q_PROPERTY(bool  peakHold    READ peakHold    WRITE setPeakHold    NOTIFY peakHoldChanged)

    // ── Spettro 3D a tracce impilate (opt-in, default spento) ───────────────
    // La storia grezza in dB e' gia' conservata per la cascata: qui viene solo
    // ridisegnata in prospettiva, senza raccogliere altri dati.
    Q_PROPERTY(bool  spectrum3d           READ spectrum3d           WRITE setSpectrum3d           NOTIFY spectrum3dChanged)
    Q_PROPERTY(int   spectrum3dTraces     READ spectrum3dTraces     WRITE setSpectrum3dTraces     NOTIFY spectrum3dTracesChanged)
    Q_PROPERTY(float spectrum3dFloorDepth READ spectrum3dFloorDepth WRITE setSpectrum3dFloorDepth NOTIFY spectrum3dFloorDepthChanged)
    // Quanta parte dello spettro la soglia automatica dichiara rumore.
    // Percentile: 10 e' la taratura storica (1.0.495), alzarlo taglia di piu'.
    Q_PROPERTY(int noiseFloorPercentile READ noiseFloorPercentile WRITE setNoiseFloorPercentile NOTIFY noiseFloorPercentileChanged)
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
    Q_PROPERTY(bool  externalSpectrumActive READ externalSpectrumActive WRITE setExternalSpectrumActive NOTIFY externalSpectrumActiveChanged)
    Q_PROPERTY(bool  showTxBrackets READ showTxBrackets WRITE setShowTxBrackets NOTIFY showTxBracketsChanged)
    Q_PROPERTY(int   colorGain   READ colorGain   WRITE setColorGain   NOTIFY colorGainChanged)
    Q_PROPERTY(int   blackLevel  READ blackLevel  WRITE setBlackLevel  NOTIFY blackLevelChanged)
    Q_PROPERTY(int   contrastLevel READ contrastLevel WRITE setContrastLevel NOTIFY contrastLevelChanged)

    // ── Render throttle (paint cap durante carico CPU elevato, es. FT2 attivo)
    Q_PROPERTY(bool  throttleActive READ throttleActive WRITE setThrottleActive NOTIFY throttleActiveChanged)
    // 1.0.98 fix 4/4: intervallo minimo (ms) tra emit update() quando throttleActive=true.
    // Default 100 (10 fps). QML imposta 200 (5 fps) durante QSO FT2 sotto propagazione cattiva
    // — riduce ulteriormente il carico GPU + main-thread e contrasta lo "scattoso" sul
    // Full Spectrum cherry-picked dall'upstream 1.0.95.
    Q_PROPERTY(int   throttleIntervalMs READ throttleIntervalMs WRITE setThrottleIntervalMs NOTIFY throttleIntervalMsChanged)

    // ── Decode labels (callsign overlay) ────────────────────────────────────
    Q_PROPERTY(int   labelFontSize      READ labelFontSize      WRITE setLabelFontSize      NOTIFY labelFontSizeChanged)
    Q_PROPERTY(int   labelSpacing       READ labelSpacing       WRITE setLabelSpacing       NOTIFY labelSpacingChanged)
    Q_PROPERTY(bool  labelBold          READ labelBold          WRITE setLabelBold          NOTIFY labelBoldChanged)
    Q_PROPERTY(QColor labelColor        READ labelColor         WRITE setLabelColor         NOTIFY labelColorChanged)
    Q_PROPERTY(bool  labelUseCustomColor READ labelUseCustomColor WRITE setLabelUseCustomColor NOTIFY labelUseCustomColorChanged)

    // ── DX Cluster spots overlay ────────────────────────────────────────────
    Q_PROPERTY(bool   showDxClusterSpots READ showDxClusterSpots WRITE setShowDxClusterSpots NOTIFY showDxClusterSpotsChanged)
    Q_PROPERTY(QColor dxClusterSpotColor READ dxClusterSpotColor WRITE setDxClusterSpotColor NOTIFY dxClusterSpotColorChanged)

    // ── Read-only status ────────────────────────────────────────────────────
    Q_PROPERTY(float measuredFloor READ measuredFloor NOTIFY measuredFloorChanged)
    Q_PROPERTY(float measuredPeak  READ measuredPeak  NOTIFY measuredPeakChanged)
    Q_PROPERTY(int   fftBins       READ fftBins       CONSTANT)
    Q_PROPERTY(bool  spectrumGpuOverlayAvailable READ spectrumGpuOverlayAvailable NOTIFY spectrumGpuOverlayAvailableChanged)

public:
    explicit PanadapterItem(QQuickItem* parent = nullptr);
    ~PanadapterItem() override;

    // ── Getters ─────────────────────────────────────────────────────────────
    float minDb()          const { return m_minDb; }
    float maxDb()          const { return m_maxDb; }
    bool  autoRange()      const { return m_autoRange; }
    bool  peakHold()       const { return m_peakHold; }
    bool  spectrum3d()           const { return m_spectrum3d; }
    // The GPU-direct FFT keeps its history in RHI textures.  The stacked 3D
    // shader consumes those textures directly; this asks the bridge for its
    // asynchronous CPU history only when that GPU path is unavailable.
    bool  requiresCpuSpectrumHistory() const;
    int   spectrum3dTraces()     const { return m_spectrum3dTraces; }
    float spectrum3dFloorDepth() const { return m_spectrum3dFloorDepth; }
    int   noiseFloorPercentile() const { return m_noiseFloorPercentile; }
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
    bool  externalSpectrumActive() const { return m_externalSpectrumActive; }
    bool  showTxBrackets() const { return m_showTxBrackets; }
    float measuredFloor()  const { return m_measuredFloor; }
    float measuredPeak()   const { return m_measuredPeak; }
    int   fftBins()        const { return 4096; }
    bool  spectrumGpuOverlayAvailable() const { return spectrumGraphSupported(); }
    QStringList paletteNames() const {
        return {"SDR Classic","Raptor Green","Grayscale","SmartSDR","Hot (SDR#)","deskHPSDR",
                "Aether Default","Aether BlueGreen","Aether Fire","Aether Plasma","FlexRadio"};
    }
    int   colorGain()      const { return m_colorGain; }
    int   blackLevel()     const { return m_blackLevel; }
    int   contrastLevel()  const { return m_contrastLevel; }
    int   labelFontSize()  const { return m_labelFontSize; }
    int   labelSpacing()   const { return m_labelSpacing; }
    bool  labelBold()      const { return m_labelBold; }
    QColor labelColor()    const { return m_labelColor; }
    bool  labelUseCustomColor() const { return m_labelUseCustomColor; }
    bool  throttleActive() const { return m_throttleActive; }
    int   throttleIntervalMs() const { return m_throttleIntervalMs; }
    bool   showDxClusterSpots() const { return m_showDxClusterSpots; }
    QColor dxClusterSpotColor() const { return m_dxClusterSpotColor; }

    // ── Setters ─────────────────────────────────────────────────────────────
    void setMinDb(float v)         { if (m_minDb!=v){m_minDb=v;emit minDbChanged();markAllDirty();} }
    void setMaxDb(float v)         { if (m_maxDb!=v){m_maxDb=v;emit maxDbChanged();markAllDirty();} }
    void setAutoRange(bool v)      { if (m_autoRange!=v){m_autoRange=v;emit autoRangeChanged();markAllDirty();} }
    void setPeakHold(bool v)       { if (m_peakHold!=v){m_peakHold=v;if(!v)m_peakBins.clear();emit peakHoldChanged();} }
    void setSpectrum3d(bool v);
    void setSpectrum3dTraces(int v) {
        int const clamped = qBound(8, v, 128);
        if (m_spectrum3dTraces!=clamped){m_spectrum3dTraces=clamped;emit spectrum3dTracesChanged();update();}
    }
    void setSpectrum3dFloorDepth(float v) {
        float const clamped = qBound(0.0f, v, 40.0f);
        if (!qFuzzyCompare(m_spectrum3dFloorDepth, clamped)){m_spectrum3dFloorDepth=clamped;emit spectrum3dFloorDepthChanged();update();}
    }
    void setNoiseFloorPercentile(int v);
    void setPeakDecay(float v)     { if (m_peakDecay!=v){m_peakDecay=v;emit peakDecayChanged();} }
    void setAvgFrames(int v)       { if (m_avgFrames!=v){m_avgFrames=qBound(1,v,32);emit avgFramesChanged();} }
    void setSpectrumHeight(int v)  { if (m_spectrumH!=v){m_spectrumH=v;emit spectrumHeightChanged();markGeomDirty();} }
    void setRxFreq(int v)          { if (m_rxFreq!=v){m_rxFreq=v;emit rxFreqChanged();markAllDirty();} }
    void setTxFreq(int v)          { if (m_txFreq!=v){m_txFreq=v;emit txFreqChanged();markAllDirty();} }
    void setStartFreq(int v)       { if (m_startFreq!=v){m_startFreq=v;emit startFreqChanged();markAllDirty();} }
    void setBandwidth(int v)       { if (m_bandwidth!=v){m_bandwidth=v;emit bandwidthChanged();markAllDirty();} }
    void setZoomFactor(float v)    { if (m_zoomFactor!=v){m_zoomFactor=qBound(1.0f,v,16.0f);emit zoomFactorChanged();markAllDirty();} }
    void setPanHz(int v)           { if (m_panHz!=v){m_panHz=v;emit panHzChanged();markAllDirty();} }
    void setPaletteIndex(int v);
    void setRunning(bool v)        { if (m_running!=v){m_running=v;emit runningChanged();} }
    void setExternalSpectrumActive(bool active);
    void setShowTxBrackets(bool v) { if (m_showTxBrackets!=v){m_showTxBrackets=v;emit showTxBracketsChanged();markAllDirty();} }
    void setColorGain(int v)       { v=qBound(0,v,100); if(m_colorGain!=v){m_colorGain=v;m_waterfallRgbValid=false;emit colorGainChanged();markDirty();} }
    void setBlackLevel(int v)      { v=qBound(0,v,100); if(m_blackLevel!=v){m_blackLevel=v;m_waterfallRgbValid=false;emit blackLevelChanged();markDirty();} }
    void setContrastLevel(int v)   { v=qBound(10,v,150); if(m_contrastLevel!=v){m_contrastLevel=v;m_waterfallRgbValid=false;emit contrastLevelChanged();markDirty();} }
    void setLabelFontSize(int v)   { v=qBound(6,v,24); if(m_labelFontSize!=v){m_labelFontSize=v;emit labelFontSizeChanged();markOverlayDirty();} }
    void setLabelSpacing(int v)    { v=qBound(0,v,20); if(m_labelSpacing!=v){m_labelSpacing=v;emit labelSpacingChanged();markOverlayDirty();} }
    void setLabelBold(bool v)      { if(m_labelBold!=v){m_labelBold=v;emit labelBoldChanged();markOverlayDirty();} }
    void setLabelColor(QColor v)   { if(m_labelColor!=v){m_labelColor=v;emit labelColorChanged();markOverlayDirty();} }
    void setLabelUseCustomColor(bool v) { if(m_labelUseCustomColor!=v){m_labelUseCustomColor=v;emit labelUseCustomColorChanged();markOverlayDirty();} }
    void setThrottleActive(bool v)      { if(m_throttleActive!=v){m_throttleActive=v;emit throttleActiveChanged(); if(!v) update();} }
    void setThrottleIntervalMs(int v)   { v=qBound(20,v,1000); if(m_throttleIntervalMs!=v){m_throttleIntervalMs=v;emit throttleIntervalMsChanged();} }
    void setShowDxClusterSpots(bool v)  { if(m_showDxClusterSpots!=v){m_showDxClusterSpots=v;emit showDxClusterSpotsChanged();markOverlayDirty();} }
    void setDxClusterSpotColor(QColor v){ if(m_dxClusterSpotColor!=v){m_dxClusterSpotColor=v;emit dxClusterSpotColorChanged();markOverlayDirty();} }

    // ── Invokable methods ───────────────────────────────────────────────────
    // Chiamato dal bridge: dB raw + range dB + range frequenze exact
    Q_INVOKABLE void addSpectrumData(const QVector<float>& dbValues,
                                      float minDb = -130.f, float maxDb = -40.f,
                                      float freqMinHz = 0.f, float freqMaxHz = 0.f);
    // Compatibilità con WaterfallItem (valori normalizzati 0-1)
    Q_INVOKABLE void addSpectrumDataNorm(const QVector<float>& normValues);
    Q_INVOKABLE bool addPcmFrame(const QVector<float>& samples,
                                 int usableSamples,
                                 int nfa,
                                 int nfb,
                                 float freqMinHz,
                                 float freqMaxHz,
                                 quint64 serial);
    bool addPcmFrameI16(const short* ring,
                        int ringSize,
                        int ringStart,
                        int firstChunk,
                        int usableSamples,
                        int nfa,
                        int nfb,
                        float freqMinHz,
                        float freqMaxHz,
                        quint64 serial);
    void activateCpuSpectrumFallback();
    void prepareGpuSpectrumRetry();
    // 1.0.569+ - vero solo se questo pannello puo' davvero consumare e disegnare
    // un frame PCM. Un pannello nascosto (es. il waterfall classico mentre e'
    // attivo il workspace DX-Pedition) non e' un consumatore: il suo "accetto"
    // di cortesia mascherava il rifiuto di quello visibile e teneva il feed
    // inchiodato sul path GPU, senza mai attivare il fallback FFTW CPU.
    bool isFrameConsumer() const;

    Q_INVOKABLE void resetPeakHold()  { m_peakBins.clear(); markDirty(); }
    Q_INVOKABLE void resetWaterfall();
    // Mostra callsign decodificati sul grafico spettro
    Q_INVOKABLE void setDecodeLabels(const QVariantList& labels);
    // Mostra spot DX cluster sul waterfall (lista [{call, freq audio Hz}, ...])
    Q_INVOKABLE void setDxClusterSpots(const QVariantList& spots);

signals:
    void minDbChanged();
    void maxDbChanged();
    void autoRangeChanged();
    void peakHoldChanged();
    void spectrum3dChanged();
    void spectrum3dTracesChanged();
    void spectrum3dFloorDepthChanged();
    void noiseFloorPercentileChanged();
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
    void externalSpectrumActiveChanged();
    void showTxBracketsChanged();
    void measuredFloorChanged();
    void measuredPeakChanged();
    void frequencySelected(int freq);
    void txFrequencySelected(int freq);
    void colorGainChanged();
    void blackLevelChanged();
    void contrastLevelChanged();
    void labelFontSizeChanged();
    void labelSpacingChanged();
    void labelBoldChanged();
    void labelColorChanged();
    void labelUseCustomColorChanged();
    void throttleActiveChanged();
    void throttleIntervalMsChanged();
    void spectrumGpuOverlayAvailableChanged();
    void gpuFftActivated(QString backend);
    void gpuFftUnavailable(QString reason);
    void showDxClusterSpotsChanged();
    void dxClusterSpotColorChanged();
    // Emesso al click su uno spot cluster nel waterfall (call + freq audio Hz)
    void dxClusterSpotClicked(QString call, int audioFreqHz);
    // Emesso al click su una etichetta decode (callsign decodificato) nel waterfall
    void decodeLabelClicked(QString call, int audioFreqHz);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
    void geometryChange(const QRectF& newGeom, const QRectF& oldGeom) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void releaseResources() override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev)  override;
    void wheelEvent(QWheelEvent* ev)      override;

private:
    void markDirty()     { m_spectrumDirty = true; update(); }
    void markOverlayDirty() { m_spectrumOverlayDirty = true; update(); }
    void markAllDirty()  { m_spectrumDirty = true; m_spectrumOverlayDirty = true; update(); }
    void markGeomDirty() { m_geometryDirty = true; m_spectrumDirty = true; m_spectrumOverlayDirty = true; update(); }

    void rebuildImages(int w, int h);
    void renderSpectrum();
    void addWaterfallRow(const QVector<float>& bins,
                         float minDb,
                         float maxDb,
                         float dataFreqMin,
                         float dataFreqMax);
    void rebuildRgbWaterfallFromIntensity();
    void logWaterfallRenderPath(bool gpu, const QString& reason);
    int sceneGraphApiKey() const;
    bool consumeUpdateBudgetLocked();
    bool shaderWaterfallSupported();
    bool spectrumGraphSupported() const;
    bool spectrum3dGpuSupported() const;
    void updateSpectrumGraphNodes(QSGNode* spectrumRoot, int w, int h);
    void removeSpectrumGraphNodes(QSGNode* spectrumRoot);
    void updateSpectrum3dNodes(QSGNode* spectrumRoot, int w, int h);
    void updateSpectrum3dGpuNodes(QSGNode* spectrumRoot, int w, int h);
    void rebuildSpectrumOverlayImage(int w, int h, bool gpuDirectReady);
    void updateSpectrumOverlayNode(QSGNode* spectrumRoot, int w, int h, bool gpuDirectReady, bool gpuSpectrumGraph);
    bool gpuFftSupported(QString* reason = nullptr) const;
    void recordGpuFftCompute();
    void releaseGpuFftResources();
    void failGpuFft(const QString& reason);
    void notifyGpuFftActive(const QString& backend);
    void connectBridgePcmFrameFeed();
    void recordOverlayMetric(qint64 elapsedUs, int decodeLabels, int clusterLabels, const QSize& size);
    void recordOverlayNodeMetric(qint64 elapsedUs,
                                 qint64 rebuildUs,
                                 qint64 textureUs,
                                 qint64 nodeUs,
                                 bool needsUpload);
    void recordPaintMetric(qint64 elapsedUs,
                           qint64 lockWaitUs,
                           qint64 geometryUs,
                           qint64 drainUs,
                           qint64 overlayUs,
                           qint64 nodesUs,
                           qint64 spectrumNodeUs,
                           qint64 waterfallNodeUs,
                           qint64 waterfallTextureUs,
                           qint64 waterfallDisplayUs,
                           qint64 waterfallSetupUs,
                           qint64 waterfallMarkUs,
                           qint64 waterfallLogUs,
                           int waterfallPath,
                           int textureCreateCount,
                           int textureUploadRows,
                           int textureFullUploads,
                           bool gpuDirectReady,
                           int pendingRows);
    void recordQsgFrameMetric(qint64 frameUs, qint64 swapUs);

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
    struct WaterfallFrame
    {
        QVector<float> bins;
        float minDb {-130.f};
        float maxDb {-40.f};
        float dataFreqMin {200.f};
        float dataFreqMax {4000.f};
    };
    QVector<WaterfallFrame> m_pendingWaterfallRows;
    struct PcmFrame
    {
        QVector<float> samples;
        int usableSamples {0};
        int nfa {0};
        int nfb {0};
        float freqMinHz {0.0f};
        float freqMaxHz {0.0f};
        float samplePeak {0.0f};
        float sampleRms {0.0f};
        quint64 serial {0};
    };
    PcmFrame m_pendingPcmFrame;
    bool m_hasPendingPcmFrame {false};

    // ── Immagini ────────────────────────────────────────────────────────────
    QImage m_spectrumImage;  // spectrum (larghezza × spectrumH)
    QImage m_spectrumOverlayImage; // griglia/label/marker batched per QSG
    QImage m_waterfallImage; // waterfall (larghezza × waterfallH)
    QImage m_waterfallDisplayImage; // waterfall lineare pronta per upload GPU
    QImage m_waterfallIntensityImage; // fallback CPU: intensità 0..255 per shader palette
    QImage m_waterfallIntensityDisplayImage; // intensità lineare pronta per upload GPU
    QImage m_waterfallIntensityTextureImage; // atlas RGBA: palette + intensità per shader GPU
    QSize m_renderSpectrumSize; // dimensioni logiche QSG, indipendenti dalle QImage fallback
    QSize m_renderWaterfallSize;
    int   m_renderWaterfallHistoryRows = 0;
    QVector<float> m_waterfallDbRows; // GPU shader path: dB grezzi per bin/riga
    QVector<float> m_waterfallDbRowParams; // minDb, inverseRange per riga
    int    m_waterfallRawBinsWidth = 0;
    int    m_wfWriteRow = 0; // riga corrente ring buffer
    QVector<QRgb> m_palette; // 256 colori waterfall

    // ── Configurazione ──────────────────────────────────────────────────────
    float m_minDb        = -130.f;
    float m_maxDb        = -40.f;
    bool  m_autoRange    = true;
    bool  m_peakHold     = true;
    bool  m_spectrum3d   = false;   // opt-in: costa vertici, non si accende da sola
    int   m_spectrum3dTraces = 28;  // tracce di storia disegnate
    float m_spectrum3dFloorDepth = 6.0f; // dB sopra il minimo sotto cui la traccia e' piatta
    int   m_noiseFloorPercentile = 10;   // taratura della 1.0.495
    // Su quanti dB si misura l'altezza della cresta. NON e' l'ampiezza
    // della finestra dei colori: con la soglia di rumore automatica quella
    // si ancora al rumore e sale di 80 dB, dove non c'e' nulla, e i segnali
    // veri - che stanno nei primi venti - restavano schiacciati al suolo.
    // Qui si misura l'escursione che c'e' davvero, smorzata nel tempo:
    // una scala che insegue ogni fotogramma fa respirare la superficie.
    float m_spectrum3dSpanDb = 35.0f;
    bool  m_spectrum3dSpanInit = false;
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
    bool  m_externalSpectrumActive = false;
    bool  m_showTxBrackets = true;

    int   m_colorGain    = 50;
    int   m_blackLevel   = 15;
    int   m_contrastLevel = 80;
    QVariantList m_decodeLabels;  // [{call:"IU8LMC",freq:1500,snr:-5,isCQ:true}, ...]

    // ── Stile label callsign (overlay spettro) ──────────────────────────────
    int    m_labelFontSize       = 8;
    int    m_labelSpacing        = 2;
    bool   m_labelBold           = true;
    QColor m_labelColor          = QColor(0, 230, 255);
    bool   m_labelUseCustomColor = false;

    // ── DX Cluster spots overlay ────────────────────────────────────────────
    QVariantList m_dxClusterSpots;       // [{call,freq}, ...] freq in audio Hz
    bool   m_showDxClusterSpots  = false;
    QColor m_dxClusterSpotColor  = QColor(255, 200, 0); // Giallo brillante
    struct ClusterHit { QRect rect; QString call; int freq; };
    QVector<ClusterHit> m_clusterHitRects;
    // Stesso pattern per le decode labels: bounding-box per click-to-call.
    QVector<ClusterHit> m_decodeHitRects;

    // ── Stato rendering ─────────────────────────────────────────────────────
    bool  m_spectrumDirty = true;
    bool  m_spectrumOverlayDirty = true;
    bool  m_geometryDirty = true;
    bool  m_useShaderWaterfall = false;
    bool  m_shaderWaterfallBlocked = false;
    std::atomic_bool m_spectrum3dGpuBlocked {false};
    bool  m_waterfallRgbValid = true;
    int   m_loggedWaterfallPath = -1;
    int   m_loggedWaterfallApi = -1;
    QString m_loggedWaterfallReason;
    QString m_shaderWaterfallDisabledReason;
    mutable std::atomic<int> m_sceneGraphApiKey {-1};
    bool  m_loggedWaterfallGpuUploadStats = false;
    int   m_lastWaterfallGpuStatsRow = -1;
    int   m_waterfallGpuUploadedWriteRow = 0;
    QSize m_waterfallGpuUploadedSize;
    int   m_paletteGeneration = 0;
    bool  m_gpuFftFailed = false;
    QString m_gpuFftFailureReason;
    bool  m_gpuFftActiveNotified = false;
    quint64 m_gpuFftFallbackGeneration = 0;
    bool  m_bridgePcmFrameFeedRegistered = false;
    bool  m_loggedGpuFftRejected = false;
    bool  m_loggedGpuFftAccepted = false;
    bool  m_loggedGpuFftInputStats = false;
    bool  m_loggedGpuFftWarmupSkip = false;
    bool  m_loggedGpuFftI16Accepted = false;
    int   m_gpuFftInvalidReadbacks = 0;
    int   m_gpuFftReadbackTimeouts = 0;
    int   m_gpuFftSlowReadbacks = 0;
    qint64 m_lastGpuFftSlowLogMs = 0;
    bool  m_loggedLegacySpectrumSuppressed = false;
    bool  m_loggedMismatchedSpectrumSuppressed = false;
    qint64 m_lastGpuFftFrameMs = 0;
    qint64 m_lastGpuFftReadbackMs = 0;
    qint64 m_lastGpuFftTimeoutLogMs = 0;
    int   m_gpuFftUiBinsExpected = 0;
    bool  m_gpuDirectTextureReady = false;
    float m_gpuDirectDisplayMinDb = -70.0f;
    float m_gpuDirectDisplayMaxDb = 35.0f;
    float m_spectrumOverlayDisplayMinDb = -9999.0f;
    float m_spectrumOverlayDisplayMaxDb = -9999.0f;
    QSize m_spectrumOverlaySize;
    qint64 m_lastSpectrumOverlayRebuildMs = 0;
    bool  m_loggedSpectrumCppOverlay = false;
    bool  m_loggedGpuWaterfallDetached = false;
    bool  m_loggedGpuSpectrum3d = false;
    QQuickWindow* m_qsgMetricWindow = nullptr;
    QMetaObject::Connection m_qsgFrameConnection;
    QMetaObject::Connection m_qsgBeforeSyncConnection;
    QMetaObject::Connection m_qsgBeforeRenderConnection;
    QMetaObject::Connection m_qsgAfterRenderConnection;
    qint64 m_qsgFrameLastSwapUs = 0;
    qint64 m_qsgFrameMetricLastLogMs = 0;
    qint64 m_qsgFrameMetricAccumUs = 0;
    int    m_qsgFrameMetricSamples = 0;
    int    m_qsgFrameMetricMaxUs = 0;
    int    m_qsgFrameSpikeCount = 0;
    int    m_qsgFrameMetricSpikeSamples = 0;
    int    m_qsgFrameMetricSpikeMaxUs = 0;
    std::atomic<qint64> m_qsgBeforeSyncUs {0};
    std::atomic<qint64> m_qsgBeforeRenderUs {0};
    std::atomic<qint64> m_qsgAfterRenderUs {0};
    std::atomic<int> m_qsgBeforeSyncCount {0};
    std::atomic<int> m_qsgBeforeRenderCount {0};
    std::atomic<int> m_qsgAfterRenderCount {0};
    int    m_qsgLastBeforeSyncCount = 0;
    int    m_qsgLastBeforeRenderCount = 0;
    int    m_qsgLastAfterRenderCount = 0;
    int    m_qsgLastSwapCount = 0;
    int    m_qsgSwapCount = 0;
    qint64 m_qsgPhaseIdleAccumUs = 0;
    qint64 m_qsgPhaseSyncAccumUs = 0;
    qint64 m_qsgPhaseRenderAccumUs = 0;
    qint64 m_qsgPhasePresentAccumUs = 0;
    int    m_qsgPhaseSamples = 0;
    int    m_qsgPhaseIdleMaxUs = 0;
    int    m_qsgPhaseSyncMaxUs = 0;
    int    m_qsgPhaseRenderMaxUs = 0;
    int    m_qsgPhasePresentMaxUs = 0;
    qint64 m_qsgPhaseSyncMaxDecodeReadyStartAgoMs = -1;
    qint64 m_qsgPhaseSyncMaxDecodeReadyEndAgoMs = -1;
    qint64 m_qsgPhaseSyncMaxDecodeModelEmitStartAgoMs = -1;
    qint64 m_qsgPhaseSyncMaxDecodeModelEmitEndAgoMs = -1;
    qint64 m_overlayMetricLastLogMs = 0;
    qint64 m_overlayMetricAccumUs = 0;
    int    m_overlayMetricSamples = 0;
    int    m_overlayMetricMaxUs = 0;
    qint64 m_overlayMetricLastUs = 0;
    int    m_overlayMetricDecodeLabels = 0;
    int    m_overlayMetricClusterLabels = 0;
    QSize  m_overlayMetricSize;
    qint64 m_overlayNodeMetricLastLogMs = 0;
    qint64 m_overlayNodeMetricAccumUs = 0;
    qint64 m_overlayNodeMetricRebuildAccumUs = 0;
    qint64 m_overlayNodeMetricTextureAccumUs = 0;
    qint64 m_overlayNodeMetricNodeAccumUs = 0;
    int    m_overlayNodeMetricSamples = 0;
    int    m_overlayNodeMetricUploadSamples = 0;
    int    m_overlayNodeMetricMaxUs = 0;
    int    m_overlayNodeMetricRebuildMaxUs = 0;
    int    m_overlayNodeMetricTextureMaxUs = 0;
    int    m_overlayNodeMetricNodeMaxUs = 0;
    qint64 m_paintMetricLastLogMs = 0;
    qint64 m_paintMetricAccumUs = 0;
    qint64 m_paintMetricLockWaitAccumUs = 0;
    qint64 m_paintMetricGeometryAccumUs = 0;
    qint64 m_paintMetricDrainAccumUs = 0;
    qint64 m_paintMetricOverlayAccumUs = 0;
    qint64 m_paintMetricNodesAccumUs = 0;
    qint64 m_paintMetricSpectrumNodeAccumUs = 0;
    qint64 m_paintMetricWaterfallNodeAccumUs = 0;
    qint64 m_paintMetricWaterfallTextureAccumUs = 0;
    qint64 m_paintMetricWaterfallDisplayAccumUs = 0;
    qint64 m_paintMetricWaterfallSetupAccumUs = 0;
    qint64 m_paintMetricWaterfallMarkAccumUs = 0;
    qint64 m_paintMetricWaterfallLogAccumUs = 0;
    int    m_paintMetricSamples = 0;
    int    m_paintMetricMaxUs = 0;
    int    m_paintMetricLockWaitMaxUs = 0;
    int    m_paintMetricGeometryMaxUs = 0;
    int    m_paintMetricDrainMaxUs = 0;
    int    m_paintMetricOverlayMaxUs = 0;
    int    m_paintMetricNodesMaxUs = 0;
    int    m_paintMetricSpectrumNodeMaxUs = 0;
    int    m_paintMetricWaterfallNodeMaxUs = 0;
    int    m_paintMetricWaterfallTextureMaxUs = 0;
    int    m_paintMetricWaterfallDisplayMaxUs = 0;
    int    m_paintMetricWaterfallSetupMaxUs = 0;
    int    m_paintMetricWaterfallMarkMaxUs = 0;
    int    m_paintMetricWaterfallLogMaxUs = 0;
    int    m_paintMetricWaterfallPathNoneSamples = 0;
    int    m_paintMetricWaterfallPathDirectSamples = 0;
    int    m_paintMetricWaterfallPathShaderSamples = 0;
    int    m_paintMetricWaterfallPathCpuSamples = 0;
    int    m_paintMetricTextureCreateCount = 0;
    int    m_paintMetricTextureUploadRows = 0;
    int    m_paintMetricTextureFullUploads = 0;
    qint64 m_paintMetricLastUs = 0;
    bool   m_paintMetricGpuDirectReady = false;
    int    m_paintMetricPendingRows = 0;
    qint64 m_decodeLabelMetricLastLogMs = 0;
    struct GpuFftState;
    GpuFftState* m_gpuFft = nullptr;
    mutable QMutex m_mutex;

    // Throttle: quando attivo, addSpectrumData chiama update() al massimo
    // ogni kThrottleIntervalMs (10 fps invece dei normali ~50 fps).
    // I dati FFT non vengono persi: m_pendingWaterfallRows li bufferizza.
    bool   m_throttleActive {false};
    int    m_throttleIntervalMs {100};   // 1.0.98: configurabile da QML (5-50 fps)
    qint64 m_lastUpdateNs   {0};
};
