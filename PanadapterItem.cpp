// PanadapterItem.cpp — High-resolution SDR panadapter for Decodium3
// Copyright IU8LMC 2025 — GPL v3
// Waterfall palette ported from deskHPSDR waterfall.c (GPL G0ORX/DL1BZ)

#include "PanadapterItem.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QQuickWindow>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMutexLocker>
#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef DECODIUM_WATERFALL_SHADER_QSB
class WaterfallPaletteMaterial final : public QSGMaterial
{
public:
    ~WaterfallPaletteMaterial() override
    {
        delete intensityTexture;
        delete paletteTexture;
    }

    QSGMaterialType* type() const override
    {
        static QSGMaterialType type;
        return &type;
    }

    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode) const override;

    int compare(const QSGMaterial* other) const override
    {
        auto const* rhs = static_cast<const WaterfallPaletteMaterial*>(other);
        if (intensityTexture == rhs->intensityTexture)
            return paletteTexture == rhs->paletteTexture ? 0 : (paletteTexture < rhs->paletteTexture ? -1 : 1);
        return intensityTexture < rhs->intensityTexture ? -1 : 1;
    }

    QSGTexture* intensityTexture = nullptr;
    QSGTexture* paletteTexture = nullptr;
};

class WaterfallPaletteShader final : public QSGMaterialShader
{
public:
    WaterfallPaletteShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/waterfall_palette.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/waterfall_palette.frag.qsb"));
    }

    bool updateUniformData(RenderState& state, QSGMaterial*, QSGMaterial*) override
    {
        QByteArray* uniformData = state.uniformData();
        bool changed = false;
        if (uniformData->size() < 80) {
            uniformData->resize(80);
            changed = true;
        }
        if (state.isMatrixDirty()) {
            const QMatrix4x4 matrix = state.combinedMatrix();
            std::memcpy(uniformData->data(), matrix.constData(), 64);
            changed = true;
        }
        if (state.isOpacityDirty()) {
            float const opacity = state.opacity();
            std::memcpy(uniformData->data() + 64, &opacity, 4);
            changed = true;
        }
        return changed;
    }

    void updateSampledImage(RenderState&, int binding, QSGTexture** texture,
                            QSGMaterial* newMaterial, QSGMaterial*) override
    {
        auto* material = static_cast<WaterfallPaletteMaterial*>(newMaterial);
        if (binding == 1) {
            *texture = material->intensityTexture;
        } else if (binding == 2) {
            *texture = material->paletteTexture;
        }
    }
};

QSGMaterialShader* WaterfallPaletteMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new WaterfallPaletteShader;
}
#endif

// ─── FlexRadio SmartSDR waterfall palette ────────────────────────────────────
// Nero → blu scuro → blu → ciano → verde → giallo → arancio → rosso
// Riprodotto dalle descrizioni della community FlexRadio
static QRgb smartsdrWfColor(float p)
{
    if (p <= 0.f)  return qRgb(0, 0, 0);
    if (p >= 1.f)  return qRgb(255, 0, 0);

    // Segmenti calibrati su SmartSDR: molto nero in basso, esplosione di colore per segnali
    auto lerp = [](int a, int b, float t) { return a + (int)((b-a)*t); };

    if (p < 0.12f) {
        // nero → blu notte
        float t = p / 0.12f;
        return qRgb(0, 0, lerp(0, 60, t));
    } else if (p < 0.28f) {
        // blu notte → blu medio
        float t = (p-0.12f) / 0.16f;
        return qRgb(0, lerp(0,30,t), lerp(60,180,t));
    } else if (p < 0.45f) {
        // blu → ciano
        float t = (p-0.28f) / 0.17f;
        return qRgb(0, lerp(30,200,t), lerp(180,255,t));
    } else if (p < 0.60f) {
        // ciano → verde
        float t = (p-0.45f) / 0.15f;
        return qRgb(lerp(0,0,t), lerp(200,255,t), lerp(255,50,t));
    } else if (p < 0.75f) {
        // verde → giallo
        float t = (p-0.60f) / 0.15f;
        return qRgb(lerp(0,255,t), 255, lerp(50,0,t));
    } else if (p < 0.90f) {
        // giallo → arancio → rosso
        float t = (p-0.75f) / 0.15f;
        return qRgb(255, lerp(255,80,t), 0);
    } else {
        // rosso → rosso intenso
        float t = (p-0.90f) / 0.10f;
        return qRgb(255, lerp(80,0,t), lerp(0,0,t));
    }
}

// ─── deskHPSDR palette (mantenuta come opzione) ──────────────────────────────
static QRgb hpsdrColor(float p)
{
    if (p <= 0.f) return qRgb(0,0,0);
    if (p >= 1.f) return qRgb(255,255,0);
    if (p < 0.222222f) { float s=p*4.5f; return qRgb(0,0,(int)(s*255)); }
    if (p < 0.333333f) { float s=(p-0.222222f)*9.f; return qRgb(0,(int)(s*255),255); }
    if (p < 0.444444f) { float s=(p-0.333333f)*9.f; return qRgb(0,255,(int)((1-s)*255)); }
    if (p < 0.555555f) { float s=(p-0.444444f)*9.f; return qRgb((int)(s*255),255,0); }
    if (p < 0.777777f) { float s=(p-0.555555f)*4.5f; return qRgb(255,(int)((1-s)*255),0); }
    if (p < 0.888888f) { float s=(p-0.777777f)*9.f; return qRgb(255,0,(int)(s*255)); }
    float s=(p-0.888888f)*9.f;
    return qRgb((int)((0.75f+0.25f*(1-s))*255),(int)(s*255*.5f),255);
}

// ─── AetherSDR / FlexRadio gradient ─────────────────────────────────────────
struct GradStop { float pos; int r, g, b; };
static QRgb gradInterp(float p, const GradStop* s, int n) {
    if (p<=0.f) return qRgb(s[0].r,s[0].g,s[0].b);
    if (p>=1.f) return qRgb(s[n-1].r,s[n-1].g,s[n-1].b);
    for (int i=0;i<n-1;++i) if (p<=s[i+1].pos) {
        float t=(p-s[i].pos)/(s[i+1].pos-s[i].pos);
        return qRgb(s[i].r+(int)((s[i+1].r-s[i].r)*t),s[i].g+(int)((s[i+1].g-s[i].g)*t),s[i].b+(int)((s[i+1].b-s[i].b)*t));
    }
    return qRgb(s[n-1].r,s[n-1].g,s[n-1].b);
}
static const GradStop kDefault[]={{0,0,0,0},{.15f,0,0,128},{.3f,0,64,255},{.45f,0,200,255},{.6f,0,220,0},{.8f,255,255,0},{1,255,0,0}};
static const GradStop kBlueGreen[]={{0,0,0,0},{.25f,0,0,100},{.5f,0,80,160},{.75f,0,180,140},{1,100,255,200}};
static const GradStop kFire[]={{0,0,0,0},{.25f,100,0,0},{.5f,200,60,0},{.75f,255,200,0},{1,255,255,200}};
static const GradStop kPlasma[]={{0,0,0,0},{.25f,80,0,120},{.5f,200,0,100},{.75f,255,140,0},{1,255,255,0}};
static const GradStop kFlex[]={{0,0,0,0},{.10f,0,0,30},{.20f,0,0,80},{.30f,0,20,140},{.42f,0,80,200},{.55f,0,180,255},{.68f,0,240,160},{.80f,80,255,0},{.90f,255,255,0},{1,255,80,0}};

// ─── Constructor ────────────────────────────────────────────────────────────
PanadapterItem::PanadapterItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(false);
    setAcceptTouchEvents(false);
    buildPalette(m_paletteIndex);
}

PanadapterItem::~PanadapterItem() = default;

bool PanadapterItem::shaderWaterfallSupported() const
{
#ifdef DECODIUM_WATERFALL_SHADER_QSB
    if (qEnvironmentVariableIsSet("DECODIUM_DISABLE_WATERFALL_SHADER")) {
        return false;
    }
    if (!qEnvironmentVariableIsSet("DECODIUM_ENABLE_WATERFALL_SHADER")) {
        return false;
    }
    if (!window() || !window()->rendererInterface()) {
        return false;
    }
    QSGRendererInterface::GraphicsApi const api = window()->rendererInterface()->graphicsApi();
    if (api == QSGRendererInterface::Metal &&
        !qEnvironmentVariableIsSet("DECODIUM_ENABLE_UNSAFE_METAL_WATERFALL_SHADER")) {
        return false;
    }
    return api != QSGRendererInterface::Software
        && api != QSGRendererInterface::Null
        && api != QSGRendererInterface::Unknown;
#else
    return false;
#endif
}

// ─── Palette ────────────────────────────────────────────────────────────────
void PanadapterItem::buildPalette(int idx)
{
    m_palette.resize(256);
    switch (idx) {
    case 1: // Raptor Green
        for (int i=0;i<256;++i){float t=i/255.f;
            if(t<.25f){float s=t/.25f;m_palette[i]=qRgb(0,(int)((0.05f+0.1f*s)*255),0);}
            else if(t<.5f){float s=(t-.25f)/.25f;m_palette[i]=qRgb((int)(.1f*s*255),(int)((.15f+.35f*s)*255),(int)(.05f*s*255));}
            else if(t<.75f){float s=(t-.5f)/.25f;m_palette[i]=qRgb((int)((.1f+.4f*s)*255),(int)((.5f+.3f*s)*255),(int)((.05f+.15f*s)*255));}
            else{float s=(t-.75f)/.25f;m_palette[i]=qRgb((int)((.5f+.5f*s)*255),(int)((.8f+.2f*s)*255),(int)((.2f+.3f*s)*255));}}
        break;
    case 2: // Grayscale
        for (int i=0;i<256;++i) m_palette[i]=qRgb(i,i,i);
        break;
    case 3: // SmartSDR (default) — FlexRadio style
        for (int i=0;i<256;++i) m_palette[i]=smartsdrWfColor(i/255.f);
        break;
    case 5: // deskHPSDR
        for (int i=0;i<256;++i) m_palette[i]=hpsdrColor(i/255.f);
        break;
    case 4: // Hot
        for (int i=0;i<256;++i){float t=i/255.f;
            m_palette[i]=qRgb(qBound(0,(int)(t*3*255),255),
                               qBound(0,(int)((t*3-1)*255),255),
                               qBound(0,(int)((t*3-2)*255),255));}
        break;
    case 6: for(int i=0;i<256;++i) m_palette[i]=gradInterp(i/255.f,kDefault,7); break;
    case 7: for(int i=0;i<256;++i) m_palette[i]=gradInterp(i/255.f,kBlueGreen,5); break;
    case 8: for(int i=0;i<256;++i) m_palette[i]=gradInterp(i/255.f,kFire,5); break;
    case 9: for(int i=0;i<256;++i) m_palette[i]=gradInterp(i/255.f,kPlasma,5); break;
    case 10: for(int i=0;i<256;++i) m_palette[i]=gradInterp(i/255.f,kFlex,10); break;
    case 11: // Stellar Light — pastel light palette per design mockup
        for (int i=0;i<256;++i){
            float t = i/255.f;
            int r, g, b;
            // 0.0  : #FFFFFF (white)
            // 0.30 : #DCE8F6 (very pale blue)
            // 0.55 : #6E9DD1 (pastel blue)
            // 0.80 : #5EAE82 (pastel green)
            // 1.00 : #D86A6A (pastel red — hot signals)
            if (t < 0.30f) {
                float s = t / 0.30f;
                r = (int)(255 + (220 - 255) * s);
                g = (int)(255 + (232 - 255) * s);
                b = (int)(255 + (246 - 255) * s);
            } else if (t < 0.55f) {
                float s = (t - 0.30f) / 0.25f;
                r = (int)(220 + (110 - 220) * s);
                g = (int)(232 + (157 - 232) * s);
                b = (int)(246 + (209 - 246) * s);
            } else if (t < 0.80f) {
                float s = (t - 0.55f) / 0.25f;
                r = (int)(110 + (94 - 110) * s);
                g = (int)(157 + (174 - 157) * s);
                b = (int)(209 + (130 - 209) * s);
            } else {
                float s = (t - 0.80f) / 0.20f;
                r = (int)(94 + (216 - 94) * s);
                g = (int)(174 + (106 - 174) * s);
                b = (int)(130 + (106 - 130) * s);
            }
            m_palette[i] = qRgb(r, g, b);
        }
        break;
    default: // 0 — SDR Classic
        for (int i=0;i<256;++i){float t=i/255.f;
            int r,g,b;
            if(t<.2f){r=0;g=0;b=(int)((0.1f+0.4f*(t/.2f))*255);}
            else if(t<.4f){float s=(t-.2f)/.2f;r=0;g=(int)(s*.8f*255);b=(int)((0.5f+.3f*s)*255);}
            else if(t<.6f){float s=(t-.4f)/.2f;r=0;g=(int)((.8f+.2f*s)*255);b=(int)(.8f*(1-s)*255);}
            else if(t<.8f){float s=(t-.6f)/.2f;r=(int)(s*255);g=255;b=0;}
            else{float s=(t-.8f)/.2f;r=255;g=(int)((1-s)*255);b=0;}
            m_palette[i]=qRgb(r,g,b);}
        break;
    }
}

QRgb PanadapterItem::wfColor(float pct) const
{
    if (m_paletteIndex == 3) return smartsdrWfColor(pct);
    if (m_paletteIndex == 5) return hpsdrColor(pct);
    int idx = qBound(0,(int)(pct*255),255);
    return m_palette[idx];
}

void PanadapterItem::setPaletteIndex(int v)
{
    v = qBound(0, v, 11);
    if (m_paletteIndex==v) return;
    m_paletteIndex=v;
    buildPalette(v);
    // Repaint background of existing images to match new palette (white for Stellar Light, else black)
    QColor const bg = (v == 11) ? QColor(255, 255, 255) : QColor(0, 0, 0);
    if (!m_spectrumImage.isNull())                  m_spectrumImage.fill(bg);
    if (!m_waterfallImage.isNull())                 m_waterfallImage.fill(bg);
    if (!m_waterfallDisplayImage.isNull())          m_waterfallDisplayImage.fill(bg);
    if (!m_waterfallIntensityImage.isNull())        m_waterfallIntensityImage.fill(0);
    if (!m_waterfallIntensityDisplayImage.isNull()) m_waterfallIntensityDisplayImage.fill(0);
    emit paletteIndexChanged();
    markDirty();
}

// ─── Frequency ↔ Pixel ───────────────────────────────────────────────────────
// Conversioni frequenza ↔ pixel usando il range EFFETTIVO dei bin (m_dataFreqMin/Max)
// Questo garantisce perfetta congruenza tra segnale visualizzato e label frequenza
int PanadapterItem::freqToX(int freq) const
{
    float const baseStart = (m_bandwidth > 0) ? static_cast<float>(m_startFreq) : m_dataFreqMin;
    float const baseEnd = (m_bandwidth > 0) ? static_cast<float>(m_startFreq + m_bandwidth) : m_dataFreqMax;
    float range = (baseEnd - baseStart) / m_zoomFactor;
    if (range <= 0.f) return 0;
    float center = baseStart + (baseEnd - baseStart) * 0.5f + m_panHz;
    float start  = center - range * 0.5f;
    return (int)((freq - start) * width() / range);
}

int PanadapterItem::xToFreq(int x) const
{
    float const baseStart = (m_bandwidth > 0) ? static_cast<float>(m_startFreq) : m_dataFreqMin;
    float const baseEnd = (m_bandwidth > 0) ? static_cast<float>(m_startFreq + m_bandwidth) : m_dataFreqMax;
    float range = (baseEnd - baseStart) / m_zoomFactor;
    float center = baseStart + (baseEnd - baseStart) * 0.5f + m_panHz;
    float start  = center - range * 0.5f;
    return (int)(start + x * range / width());
}

// ─── Image buffers ──────────────────────────────────────────────────────────
void PanadapterItem::rebuildImages(int w, int h)
{
    if (w <= 0) return;

    // L'altezza spettro usa al massimo tutta l'altezza disponibile
    int specH = qMin(m_spectrumH, qMax(1, h - 1));
    int wfH   = qMax(0, h - specH);

    // Crea sempre lo spectrum image (richiede solo w > 0)
    if (m_spectrumImage.isNull() ||
        m_spectrumImage.width() != w ||
        m_spectrumImage.height() != specH) {
        m_spectrumImage = QImage(w, qMax(1, specH), QImage::Format_ARGB32_Premultiplied);
        m_spectrumImage.fill(m_paletteIndex == 11 ? QColor(255, 255, 255) : QColor(0, 0, 0));
    }

    // Crea il waterfall image solo se c'è spazio
    if (wfH > 0) {
        if (m_waterfallImage.isNull() ||
            m_waterfallImage.width() != w ||
            m_waterfallImage.height() != wfH) {
            m_waterfallImage = QImage(w, wfH, QImage::Format_RGB32);
            m_waterfallImage.fill(m_paletteIndex == 11 ? QColor(255, 255, 255) : QColor(0, 0, 0));
            m_waterfallDisplayImage = QImage(w, wfH, QImage::Format_RGB32);
            m_waterfallDisplayImage.fill(m_paletteIndex == 11 ? QColor(255, 255, 255) : QColor(0, 0, 0));
            m_waterfallIntensityImage = QImage(w, wfH, QImage::Format_Grayscale8);
            m_waterfallIntensityImage.fill(0);
            m_waterfallIntensityDisplayImage = QImage(w, wfH, QImage::Format_Grayscale8);
            m_waterfallIntensityDisplayImage.fill(0);
            m_wfWriteRow = 0;
        }
    } else {
        m_waterfallImage = QImage();
        m_waterfallDisplayImage = QImage();
        m_waterfallIntensityImage = QImage();
        m_waterfallIntensityDisplayImage = QImage();
        m_wfWriteRow = 0;
    }
}

// ─── Add spectrum data ───────────────────────────────────────────────────────
void PanadapterItem::addSpectrumData(const QVector<float>& dbValues,
                                      float minDb, float maxDb,
                                      float freqMinHz, float freqMaxHz)
{
    QMutexLocker lock(&m_mutex);
    if (dbValues.isEmpty()) return;

    // Salva il range frequenze ESATTO dei bin — unica fonte di verità per l'asse X
    if (freqMinHz > 0.f && freqMaxHz > freqMinHz) {
        m_dataFreqMin = freqMinHz;
        m_dataFreqMax = freqMaxHz;
    }

    m_bins = dbValues;

    // Auto-range stile SmartSDR: noise floor come riferimento, range fisso 80dB
    // Il noise floor è stimato dalla media dei bin più bassi (10° percentile)
    // Il ceiling = floor + 80dB → rumore = NERO, segnali = colorati
    if (m_autoRange) {
        QVector<float> s = dbValues;
        std::sort(s.begin(), s.end());
        int n = s.size();
        // Stima noise floor dal 10° percentile (più robusto del 5°)
        float fl = s[qBound(0, n*10/100, n-1)];
        // IIR smooth lento per stabilità visiva
        m_measuredFloor = 0.03f*fl + 0.97f*m_measuredFloor;
        // Range fisso 80dB sopra il noise floor — solo i segnali veri appaiono colorati
        m_measuredPeak  = m_measuredFloor + 80.0f;
        m_minDb = m_measuredFloor;
        m_maxDb = m_measuredPeak;
        emit measuredFloorChanged();
        emit measuredPeakChanged();
    } else if (maxDb > minDb) {
        // Quando auto-range è spento, usa il range reale fornito dal bridge.
        // Questo evita la saturazione totale con i valori FFT moderni sul path mac legacy.
        m_minDb = minDb;
        m_maxDb = maxDb;
    }

    // Peak hold con decay
    if (m_peakHold) {
        if (m_peakBins.size() != dbValues.size()) {
            m_peakBins = dbValues;
        } else {
            for (int i = 0; i < dbValues.size(); ++i) {
                float decayed = m_peakBins[i] * m_peakDecay + m_minDb * (1.f - m_peakDecay);
                m_peakBins[i] = qMax(decayed, dbValues[i]);
            }
        }
    }

    // Average stack
    if (m_avgFrames > 1) {
        m_avgStack.append(dbValues);
        while (m_avgStack.size() > m_avgFrames) m_avgStack.removeFirst();
        if (m_avgStack.size() == m_avgFrames) {
            QVector<float> avg(dbValues.size(), 0.f);
            for (const auto& v : m_avgStack)
                for (int i=0;i<v.size()&&i<avg.size();++i) avg[i]+=v[i];
            float inv = 1.f / m_avgFrames;
            for (auto& a : avg) a *= inv;
            m_bins = avg;
        }
    }

    m_spectrumDirty = true;
    lock.unlock();
    update();
}

// Compatibilità: riceve valori 0-1 normalizzati e li converte in dB
void PanadapterItem::addSpectrumDataNorm(const QVector<float>& normValues)
{
    // Converti 0-1 → dB range [-130, -40], range frequenze: nfa(200Hz) - nfb(4000Hz)
    QVector<float> db;
    db.reserve(normValues.size());
    for (float v : normValues)
        db.append(-130.f + v * 90.f);
    addSpectrumData(db, -130.f, -40.f, 200.f, 4000.f);
}

void PanadapterItem::resetWaterfall()
{
    QMutexLocker lock(&m_mutex);
    if (!m_waterfallImage.isNull())
        m_waterfallImage.fill(m_paletteIndex == 11 ? QColor(255,255,255) : QColor(0,0,0));
    if (!m_waterfallDisplayImage.isNull())
        m_waterfallDisplayImage.fill(m_paletteIndex == 11 ? QColor(255,255,255) : QColor(0,0,0));
    if (!m_waterfallIntensityImage.isNull())
        m_waterfallIntensityImage.fill(0);
    if (!m_waterfallIntensityDisplayImage.isNull())
        m_waterfallIntensityDisplayImage.fill(0);
    m_wfWriteRow = 0;
    m_spectrumDirty = true;
    lock.unlock();
    update();
}

// ─── FlexRadio SmartSDR spectrum render ──────────────────────────────────────
void PanadapterItem::renderSpectrum()
{
    int w = (int)width();
    if (w <= 0 || m_bins.isEmpty()) return;
    if (m_spectrumImage.isNull() || m_spectrumImage.width() != w) return; // attende init
    int h = m_spectrumImage.height();
    if (h <= 0) return;

    // Background: bianco per palette Stellar Light, nero per le altre (SDR-style)
    m_spectrumImage.fill(m_paletteIndex == 11 ? QColor(255, 255, 255) : QColor(0, 0, 0));

    QPainter p(&m_spectrumImage);
    p.setRenderHint(QPainter::Antialiasing, false);

    int nBins = m_bins.size();
    float range = m_maxDb - m_minDb;
    if (range < 1.f) range = 1.f;

    auto binToY = [&](float db) {
        float norm = qBound(0.f, (db - m_minDb) / range, 1.f);
        return h - 1 - (int)(norm * (h - 2));
    };

    // Sistema di coordinate: usa m_dataFreqMin/Max — stessa base usata dai bin FFT
    float const baseStart = (m_bandwidth > 0) ? static_cast<float>(m_startFreq) : m_dataFreqMin;
    float const baseEnd = (m_bandwidth > 0) ? static_cast<float>(m_startFreq + m_bandwidth) : m_dataFreqMax;
    float viewportRange = baseEnd - baseStart;
    if (viewportRange <= 0.f) viewportRange = 1.f;
    float dataRange = m_dataFreqMax - m_dataFreqMin;
    if (dataRange <= 0.f) dataRange = 1.f;
    float viewRange  = viewportRange / m_zoomFactor;
    float viewCenter = baseStart + viewportRange * 0.5f + m_panHz;
    float viewStart  = viewCenter - viewRange * 0.5f;
    // fToX: mappa freq → pixel usando le coordinate dei bin reali
    auto fToX = [&](float f) -> int { return (int)((f - viewStart) * w / viewRange); };
    // Anche i BIN devono usare questa stessa mappatura.
    // bin[i] corrisponde al range dati reale m_dataFreqMin..m_dataFreqMax,
    // mentre la view può essere un sotto-range custom 0..3200.

    // ── Griglia dB orizzontale (SmartSDR: 5 livelli, labels dBm) ──────────
    const int DB_STEPS = 5;
    p.setFont(QFont("Consolas", 8));
    for (int step = 0; step <= DB_STEPS; ++step) {
        float norm = (float)step / DB_STEPS;
        int gy = h - 1 - (int)(norm * (h - 16));
        p.setPen(QPen(QColor(38, 38, 38), 1));   // grigio SmartSDR quasi invisibile
        p.drawLine(0, gy, w, gy);
        float dbLabel = m_minDb + norm * range;
        p.setPen(QColor(160, 160, 160));
        p.drawText(2, gy - 1, QString("%1").arg((int)dbLabel));
    }

    // ── Griglia frequenza verticale (label grandi e leggibili) ────────────
    int freqStep = (int)viewRange > 3000 ? 500 : ((int)viewRange > 1000 ? 200 : 100);
    p.setFont(QFont("Consolas", 9, QFont::Bold));
    int fGridStart = (int)viewStart;
    for (int f = ((fGridStart/freqStep)+1)*freqStep; f < (int)(viewStart + viewRange); f += freqStep) {
        int x = fToX(f);
        if (x < 0 || x >= w) continue;
        // Linea griglia: grigio scuro SmartSDR
        p.setPen(QPen(QColor(40, 40, 40), 1));
        p.drawLine(x, 0, x, h - 16);
        // Label: bianca, dimensione leggibile
        p.setPen(QColor(220, 220, 220));
        QString label = f >= 1000 ? QString("%1k").arg(f/1000.0, 0, 'f', 1) : QString::number(f);
        p.drawText(x - 18, h - 3, label);
    }

    // ── Marker RX (SmartSDR Slice A: ciano #00E5FF) ───────────────────────
    int rxX = fToX(m_rxFreq);
    // Passband stretto: solo ±150Hz attorno al cursore RX (ampiezza filtro FT8 ~300Hz)
    int narrowPassHz = 300;
    int pLeft  = fToX(m_rxFreq - narrowPassHz/2);
    int pRight = fToX(m_rxFreq + narrowPassHz/2);
    if (pLeft < 0) pLeft = 0;
    if (pRight > w) pRight = w;
    if (pRight > pLeft) { p.fillRect(pLeft, 0, pRight - pLeft, h, QColor(190, 190, 190, 32)); }

    // ── Path spettro ───────────────────────────────────────────────────────
    QPainterPath fillPath, linePath;
    fillPath.moveTo(0, h);
    bool first = true;
    for (int x = 0; x < w; ++x) {
        // Mappa pixel x → frequenza → bin usando il range effettivo dei dati FFT
        float pixFreq = viewStart + (float)x * viewRange / w;
        if (pixFreq < m_dataFreqMin || pixFreq > m_dataFreqMax) {
            int const y = h - 1;
            fillPath.lineTo(x, y);
            if (first) { linePath.moveTo(x, y); first = false; }
            else        { linePath.lineTo(x, y); }
            continue;
        }
        int bin = (int)((pixFreq - m_dataFreqMin) / dataRange * nBins);
        bin = qBound(0, bin, nBins - 1);
        int y = binToY(m_bins[bin]);
        fillPath.lineTo(x, y);
        if (first) { linePath.moveTo(x, y); first = false; }
        else        { linePath.lineTo(x, y); }
    }
    fillPath.lineTo(w, h);
    fillPath.closeSubpath();

    // ── Fill spettro: usa la palette selezionata, non sempre il bianco ─────
    QColor fillTopColor = QColor::fromRgb(wfColor(0.82f));
    QColor fillMidColor = QColor::fromRgb(wfColor(0.58f));
    QColor glowColor    = QColor::fromRgb(wfColor(0.92f)).lighter(145);
    QColor traceColor   = QColor::fromRgb(wfColor(0.98f));
    fillTopColor.setAlpha(78);
    fillMidColor.setAlpha(22);
    glowColor.setAlpha(60);
    traceColor.setAlpha(255);

    QLinearGradient fillGrad(0, 0, 0, h);
    fillGrad.setColorAt(0.00, fillTopColor);
    fillGrad.setColorAt(0.40, fillMidColor);
    fillGrad.setColorAt(1.00, QColor(0, 0, 0, 0));
    p.fillPath(fillPath, fillGrad);

    // ── Glow/trace: segue la palette per dare feedback immediato al cambio ─
    p.setPen(QPen(glowColor, 3.0));
    p.drawPath(linePath);
    p.setPen(QPen(traceColor, 1.0));
    p.drawPath(linePath);

    // ── Peak hold: linea bianca tratteggiata più in alto ──────────────────
    if (m_peakHold && m_peakBins.size() == nBins) {
        QPainterPath pkPath;
        first = true;
        for (int x = 0; x < w; ++x) {
            float pixFreq = viewStart + (float)x * viewRange / w;
            if (pixFreq < m_dataFreqMin || pixFreq > m_dataFreqMax) continue;
            int bin = (int)((pixFreq - m_dataFreqMin) / dataRange * nBins);
            bin = qBound(0, bin, nBins - 1);
            int y = binToY(m_peakBins[bin]);
            if (first) { pkPath.moveTo(x, y); first = false; }
            else        { pkPath.lineTo(x, y); }
        }
        p.setPen(QPen(QColor(255, 255, 255, 90), 1.0, Qt::DotLine));
        p.drawPath(pkPath);
    }

    int txX = fToX(m_txFreq);
    bool const txVisible = txX >= 0 && txX < w && m_txFreq != m_rxFreq;
    auto drawMarkerLabel = [&](int markerX, int preferredCenterY, const QString& text, const QColor& accent) {
        QFont labelFont("Segoe UI", 9, QFont::Bold);
        p.setFont(labelFont);
        QFontMetrics fm(labelFont);

        int const padX = 6;
        int const padY = 3;
        int const boxW = fm.horizontalAdvance(text) + padX * 2;
        int const boxH = fm.height() + padY * 2;
        int const minY = boxH / 2 + 3;
        int const maxY = qMax(minY, h - boxH / 2 - 22);
        int const centerY = qBound(minY, preferredCenterY, maxY);

        int boxX = markerX + 8;
        if (boxX + boxW > w - 2)
            boxX = markerX - boxW - 8;
        boxX = qBound(2, boxX, qMax(2, w - boxW - 2));
        int const boxY = qBound(2, centerY - boxH / 2, qMax(2, h - boxH - 22));

        QPainterPath box;
        box.addRoundedRect(QRectF(boxX, boxY, boxW, boxH), 4, 4);
        QColor bg(0, 0, 0, 180);
        QColor border = accent;
        border.setAlpha(210);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillPath(box, bg);
        p.setPen(QPen(border, 1));
        p.drawPath(box);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(accent);
        p.drawText(QRect(boxX + padX, boxY + padY, boxW - padX * 2, boxH - padY * 2),
                   Qt::AlignCenter, text);
    };

    // ── Marker VFO RX: linea verticale ciano #00E5FF (spessa + glow) ──────
    if (rxX >= 0 && rxX < w) {
        // Glow esterno (alone ampio)
        p.setPen(QPen(QColor(0, 229, 255, 70), 7.0));
        p.drawLine(rxX, 0, rxX, h);
        // Linea principale spessa
        p.setPen(QPen(QColor(0, 229, 255, 240), 3.0));
        p.drawLine(rxX, 0, rxX, h);
        // Core brillante
        p.setPen(QPen(QColor(180, 255, 255, 255), 1.0));
        p.drawLine(rxX, 0, rxX, h);
    }

    // ── Frequency ticks ogni 500Hz ────────────────────────────────────────
    p.setFont(QFont("Consolas", 9, QFont::Bold));
    for (int calFreq = (int)(viewStart/500)*500 + 500; calFreq < (int)(viewStart+viewRange); calFreq += 500) {
        int cx = fToX(calFreq);
        if (cx < 0 || cx >= w) continue;
        p.setPen(QPen(QColor(255, 230, 0, 180), 1));
        p.drawLine(cx, h-18, cx, h-6);
        p.setPen(QColor(220, 210, 0));
        p.drawText(cx - 20, h-20, QString::number(calFreq));
    }

    // ── Marker TX: magenta (Slice B SmartSDR) — spessa + glow ───────────
    if (txVisible) {
        // Glow esterno
        p.setPen(QPen(QColor(255, 0, 255, 70), 7.0));
        p.drawLine(txX, 0, txX, h);
        // Linea principale spessa
        p.setPen(QPen(QColor(255, 0, 255, 240), 3.0));
        p.drawLine(txX, 0, txX, h);
        // Core brillante
        p.setPen(QPen(QColor(255, 200, 255, 255), 1.0));
        p.drawLine(txX, 0, txX, h);
    }

    // ── Decode labels: mostra callsign delle stazioni decodificate ─────
    // Algoritmo anti-overlap: assegnazione automatica su più righe.
    if (!m_decodeLabels.isEmpty()) {
        QFont labelFont("Consolas", m_labelFontSize, m_labelBold ? QFont::Bold : QFont::Normal);
        p.setFont(labelFont);
        QFontMetrics fm(labelFont);
        const int rowH = fm.height();
        const int topPad = 2;
        const int bottomKeepOut = 20;
        const int gap = m_labelSpacing;

        int maxRows = qMax(1, (h - bottomKeepOut - topPad) / rowH);

        struct LabelItem {
            int x;
            int textW;
            QString text;
            QColor color;
        };
        QVector<LabelItem> items;
        items.reserve(m_decodeLabels.size());

        for (const auto& v : m_decodeLabels) {
            QVariantMap d = v.toMap();
            QString call = d.value("call").toString();
            int freq = d.value("freq").toInt();
            bool isCQ = d.value("isCQ").toBool();
            bool isMyCall = d.value("isMyCall").toBool();
            int snr = d.value("snr").toInt();
            if (call.isEmpty() || freq <= 0) continue;

            int lx = fToX(freq);
            if (lx < 0 || lx >= w) continue;

            QString text = call + " " + QString::number(snr);
            int textW = fm.horizontalAdvance(text);

            QColor col;
            if (m_labelUseCustomColor) {
                col = m_labelColor;
            } else {
                col = isCQ ? QColor(0, 230, 100)
                           : (isMyCall ? QColor(255, 80, 80) : QColor(0, 200, 255));
            }

            items.push_back({lx, textW, text, col});
        }

        std::sort(items.begin(), items.end(),
                  [](const LabelItem& a, const LabelItem& b){ return a.x < b.x; });

        QVector<int> rowRightX(maxRows, -1000000);

        for (const auto& it : items) {
            int textX = it.x + 2;
            int chosenRow = -1;
            for (int r = 0; r < maxRows; ++r) {
                if (textX > rowRightX[r] + gap) {
                    chosenRow = r;
                    break;
                }
            }
            if (chosenRow < 0) continue;

            int textY = topPad + rowH * (chosenRow + 1) - fm.descent();

            p.setPen(QPen(it.color, 1, Qt::DotLine));
            p.drawLine(it.x, 0, it.x, h - bottomKeepOut);

            p.setPen(it.color);
            p.drawText(textX, textY, it.text);

            rowRightX[chosenRow] = textX + it.textW;
        }
    }

    int const centerY = h / 2;
    if (rxX >= 0 && rxX < w) {
        drawMarkerLabel(rxX, centerY - (txVisible ? 12 : 0),
                        QString("RX %1").arg(m_rxFreq), QColor(0, 229, 255));
    }
    if (txVisible) {
        drawMarkerLabel(txX, centerY + 12,
                        QString("TX %1").arg(m_txFreq), QColor(255, 0, 255));
    }

    // ── Info in basso a destra ────────────────────────────────────────────
    if (m_autoRange) {
        p.setFont(QFont("Consolas", 8));
        p.setPen(QColor(100, 100, 100));
        p.drawText(w - 100, h - 3,
                   QString("NF:%1dB").arg((int)m_measuredFloor));
    }
}

void PanadapterItem::setDecodeLabels(const QVariantList& labels)
{
    m_decodeLabels = labels;
    markDirty();
}

// ─── Add waterfall row ────────────────────────────────────────────────────────
void PanadapterItem::addWaterfallRow()
{
    if (m_waterfallImage.isNull() || m_bins.isEmpty()) return;
    int w  = m_waterfallImage.width();
    int h  = m_waterfallImage.height();
    if (w <= 0 || h <= 0) return;

    int nBins  = m_bins.size();
    float range = m_maxDb - m_minDb;
    if (range < 1.f) range = 1.f;

    // Ring buffer: scrivi nella riga corrente
    // Usa lo stesso sistema di coordinate del renderSpectrum per allineamento perfetto
    float const baseStart = (m_bandwidth > 0) ? static_cast<float>(m_startFreq) : m_dataFreqMin;
    float const baseEnd = (m_bandwidth > 0) ? static_cast<float>(m_startFreq + m_bandwidth) : m_dataFreqMax;
    float viewportRange = baseEnd - baseStart;
    if (viewportRange <= 0.f) viewportRange = 1.f;
    float dataRange = m_dataFreqMax - m_dataFreqMin;
    if (dataRange <= 0.f) dataRange = 1.f;
    float viewRange  = viewportRange / m_zoomFactor;
    float viewCenter = baseStart + viewportRange * 0.5f + m_panHz;
    float viewStart  = viewCenter - viewRange * 0.5f;

    // BlackLevel: soglia sotto cui tutto è nero (0=nulla, 100=aggressivo)
    // ColorGain: gamma/contrasto (0=molto gamma, 50=lineare, 100=invertito)
    // Gamma > 1 comprime i bassi verso il nero → sfondo pulito, segnali netti
    float blackThresh = m_blackLevel * 0.006f;  // 0.0-0.6 del range normalizzato
    float gamma = 2.5f - m_colorGain * 0.02f;   // gain 0→gamma 2.5, gain 50→gamma 1.5, gain 100→gamma 0.5
    if (gamma < 0.3f) gamma = 0.3f;

    QRgb const wfBg = (m_paletteIndex == 11) ? qRgb(255,255,255) : qRgb(0,0,0);

    int row = m_wfWriteRow % h;
    QRgb* line = m_useShaderWaterfall ? nullptr : reinterpret_cast<QRgb*>(m_waterfallImage.scanLine(row));
    uchar* intensityLine = (!m_waterfallIntensityImage.isNull()
                            && m_waterfallIntensityImage.width() == w
                            && m_waterfallIntensityImage.height() == h)
        ? m_waterfallIntensityImage.scanLine(row)
        : nullptr;
    for (int x = 0; x < w; ++x) {
        float pixFreq = viewStart + (float)x * viewRange / w;
        if (pixFreq < m_dataFreqMin || pixFreq > m_dataFreqMax) {
            if (line)
                line[x] = wfBg;
            if (intensityLine)
                intensityLine[x] = 0;
            continue;
        }
        int bin = (int)((pixFreq - m_dataFreqMin) / dataRange * nBins);
        bin = qBound(0, bin, nBins - 1);
        float raw = (m_bins[bin] - m_minDb) / range;
        // Sottrai la soglia nero e riscala
        raw = (raw - blackThresh) / (1.0f - blackThresh);
        if (raw <= 0.f) {
            if (line)
                line[x] = wfBg;
            if (intensityLine)
                intensityLine[x] = 0;
            continue;
        }
        // Gamma: valori bassi → nero, solo segnali veri → colore
        float pct = std::pow(qBound(0.f, raw, 1.f), gamma);
        if (intensityLine)
            intensityLine[x] = static_cast<uchar>(qBound(0, static_cast<int>(pct * 255.f + 0.5f), 255));
        if (line)
            line[x] = wfColor(pct);
    }
    m_wfWriteRow++;
}

// ─── Qt Scene Graph update ────────────────────────────────────────────────────
QSGNode* PanadapterItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    if (!window()) return nullptr;
    int w = (int)width();
    int h = (int)height();
    if (w <= 0 || h <= 0) { delete oldNode; return nullptr; }

    QMutexLocker lock(&m_mutex);

    if (m_geometryDirty) {
        rebuildImages(w, h);
        m_geometryDirty = false;
    }

    m_useShaderWaterfall = shaderWaterfallSupported();

    if (m_spectrumDirty && !m_bins.isEmpty()) {
        addWaterfallRow();
        renderSpectrum();
        m_spectrumDirty = false;
    }

    // ── Crea/aggiorna due texture node: spectrum + waterfall ─────────────
    // Node structure: root → spectrum node, waterfall node
    QSGNode* root = oldNode ? oldNode : new QSGNode();

    bool isNew = (root->childCount() == 0);

    // Spectrum node (top)
    if (!m_spectrumImage.isNull()) {
        int const specH = m_spectrumImage.height();
        QRectF specRect(0, 0, w, specH);
        QSGSimpleTextureNode* sn = nullptr;
        if (!isNew && root->firstChild())
            sn = static_cast<QSGSimpleTextureNode*>(root->firstChild());
        if (!sn) { sn = new QSGSimpleTextureNode(); sn->setOwnsTexture(true); root->prependChildNode(sn); }
        auto* tex = window()->createTextureFromImage(m_spectrumImage,
            QQuickWindow::CreateTextureOptions(QQuickWindow::TextureHasAlphaChannel));
        tex->setFiltering(QSGTexture::Linear);
        sn->setTexture(tex);
        sn->setRect(specRect);
        sn->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    }

    // Waterfall node (bottom) — ring buffer: due draw calls per wrap-around
    if (!m_waterfallImage.isNull()) {
        int const specH = m_spectrumImage.isNull() ? qMin(m_spectrumH, h) : m_spectrumImage.height();
        int wfH = h - specH;
        int wfW = m_waterfallImage.width();
        int rows = m_waterfallImage.height();
        if (wfH <= 0 || wfW <= 0 || rows <= 0)
            return root;

        QSGNode* waterfallChild = nullptr;
        if (!isNew && root->childCount() > 1 && root->firstChild())
            waterfallChild = root->firstChild()->nextSibling();

#ifdef DECODIUM_WATERFALL_SHADER_QSB
        if (m_useShaderWaterfall && !m_waterfallIntensityImage.isNull()) {
            if (m_waterfallIntensityDisplayImage.isNull() ||
                m_waterfallIntensityDisplayImage.width() != wfW ||
                m_waterfallIntensityDisplayImage.height() != wfH) {
                m_waterfallIntensityDisplayImage = QImage(wfW, wfH, QImage::Format_Grayscale8);
                m_waterfallIntensityDisplayImage.fill(0);
            }

            for (int y = 0; y < wfH; ++y) {
                int const srcRow = ((m_wfWriteRow - 1 - y) % rows + rows) % rows;
                std::memcpy(m_waterfallIntensityDisplayImage.scanLine(y),
                            m_waterfallIntensityImage.scanLine(srcRow),
                            static_cast<size_t>(wfW));
            }

            if (auto* oldSimple = dynamic_cast<QSGSimpleTextureNode*>(waterfallChild)) {
                root->removeChildNode(oldSimple);
                delete oldSimple;
                waterfallChild = nullptr;
            }

            auto* wn = static_cast<QSGGeometryNode*>(waterfallChild);
            if (!wn) {
                wn = new QSGGeometryNode();
                auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
                geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
                geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
                wn->setGeometry(geometry);
                wn->setFlag(QSGNode::OwnsGeometry);
                wn->setMaterial(new WaterfallPaletteMaterial());
                wn->setFlag(QSGNode::OwnsMaterial);
                root->appendChildNode(wn);
            }

            QSGGeometry::updateTexturedRectGeometry(wn->geometry(),
                                                    QRectF(0, specH, w, wfH),
                                                    QRectF(0, 0, 1, 1));

            auto* material = static_cast<WaterfallPaletteMaterial*>(wn->material());
            delete material->intensityTexture;
            material->intensityTexture = window()->createTextureFromImage(
                m_waterfallIntensityDisplayImage,
                QQuickWindow::CreateTextureOptions(QQuickWindow::TextureIsOpaque));
            material->intensityTexture->setFiltering(QSGTexture::Nearest);

            QImage paletteImage(256, 1, QImage::Format_RGBA8888);
            auto* paletteLine = paletteImage.scanLine(0);
            for (int i = 0; i < 256; ++i) {
                QColor const c = QColor::fromRgb(m_palette.value(i, qRgb(0, 0, 0)));
                paletteLine[i * 4 + 0] = static_cast<uchar>(c.red());
                paletteLine[i * 4 + 1] = static_cast<uchar>(c.green());
                paletteLine[i * 4 + 2] = static_cast<uchar>(c.blue());
                paletteLine[i * 4 + 3] = 255;
            }
            delete material->paletteTexture;
            material->paletteTexture = window()->createTextureFromImage(
                paletteImage,
                QQuickWindow::CreateTextureOptions(QQuickWindow::TextureHasAlphaChannel));
            material->paletteTexture->setFiltering(QSGTexture::Linear);

            wn->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
            return root;
        }
#endif

        if (auto* oldGeometry = dynamic_cast<QSGGeometryNode*>(waterfallChild)) {
            if (!dynamic_cast<QSGSimpleTextureNode*>(oldGeometry)) {
                root->removeChildNode(oldGeometry);
                delete oldGeometry;
                waterfallChild = nullptr;
            }
        }

        if (m_waterfallDisplayImage.isNull() ||
            m_waterfallDisplayImage.width() != wfW ||
            m_waterfallDisplayImage.height() != wfH) {
            m_waterfallDisplayImage = QImage(wfW, wfH, QImage::Format_RGB32);
            m_waterfallDisplayImage.fill(m_paletteIndex == 11 ? QColor(255, 255, 255) : QColor(0, 0, 0));
        }

        // Ring buffer → display: riga 0 = più recente (top), scende verso il basso
        // SmartSDR style: nuovi segnali appaiono in cima e "cadono" verso il basso
        for (int y = 0; y < wfH; ++y) {
            // y=0 → riga più recente, y=wfH-1 → riga più vecchia
            int srcRow = ((m_wfWriteRow - 1 - y) % rows + rows) % rows;
            memcpy(m_waterfallDisplayImage.scanLine(y), m_waterfallImage.scanLine(srcRow), wfW * 4);
        }

        QSGSimpleTextureNode* wn = dynamic_cast<QSGSimpleTextureNode*>(waterfallChild);
        if (!wn) { wn = new QSGSimpleTextureNode(); wn->setOwnsTexture(true); root->appendChildNode(wn); }
        auto* tex = window()->createTextureFromImage(m_waterfallDisplayImage, QQuickWindow::CreateTextureOptions(QQuickWindow::TextureIsOpaque));
        tex->setFiltering(QSGTexture::Nearest);
        wn->setTexture(tex);
        wn->setRect(QRectF(0, specH, w, wfH));
        wn->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    }

    return root;
}

// ─── Geometry change ─────────────────────────────────────────────────────────
void PanadapterItem::geometryChange(const QRectF& newGeom, const QRectF& oldGeom)
{
    QQuickItem::geometryChange(newGeom, oldGeom);
    if (newGeom.size() != oldGeom.size())
        markGeomDirty();
}

// ─── Mouse / Wheel ───────────────────────────────────────────────────────────
void PanadapterItem::mousePressEvent(QMouseEvent* ev)
{
    int freq = xToFreq((int)ev->position().x());
    if (ev->button() == Qt::LeftButton) {
        setTxFreq(freq);
        emit txFrequencySelected(freq);
    } else if (ev->button() == Qt::RightButton) {
        setRxFreq(freq);
        emit frequencySelected(freq);
    }
    ev->accept();
}

void PanadapterItem::mouseMoveEvent(QMouseEvent* ev)
{
    if (ev->buttons() & Qt::LeftButton) {
        int freq = xToFreq((int)ev->position().x());
        setTxFreq(freq);
        emit txFrequencySelected(freq);
        ev->accept();
    } else if (ev->buttons() & Qt::RightButton) {
        int freq = xToFreq((int)ev->position().x());
        setRxFreq(freq);
        emit frequencySelected(freq);
        ev->accept();
    }
}

void PanadapterItem::wheelEvent(QWheelEvent* ev)
{
    float delta = ev->angleDelta().y() / 120.f;
    float newZoom = qBound(1.0f, m_zoomFactor * (1.f + delta * 0.2f), 16.f);
    setZoomFactor(newZoom);
    ev->accept();
}
