// PanadapterItem.cpp — High-resolution SDR panadapter for Decodium3
// Copyright IU8LMC 2025 — GPL v3
// Waterfall palette ported from deskHPSDR waterfall.c (GPL G0ORX/DL1BZ)

#include "PanadapterItem.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QQuickWindow>
#include <QFile>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMutexLocker>
#include <QPointer>
#include <QDebug>
#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
#if __has_include(<rhi/qrhi.h>)
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#elif __has_include(<private/qrhi_p.h>)
#include <private/qrhi_p.h>
#else
#error "DECODIUM_QT_RHI_TEXTURE_UPLOAD requires Qt RHI private headers"
#endif
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
using DecodiumRhiBufferReadbackResult = QRhiBufferReadbackResult;
#else
using DecodiumRhiBufferReadbackResult = QRhiReadbackResult;
#endif
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
const char* waterfallGraphicsApiName(QSGRendererInterface::GraphicsApi api)
{
    switch (api) {
    case QSGRendererInterface::Software: return "Software";
    case QSGRendererInterface::OpenVG: return "OpenVG";
    case QSGRendererInterface::OpenGL: return "OpenGL";
    case QSGRendererInterface::Direct3D11: return "Direct3D11";
    case QSGRendererInterface::Vulkan: return "Vulkan";
    case QSGRendererInterface::Metal: return "Metal";
#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
    case QSGRendererInterface::Direct3D12: return "Direct3D12";
#endif
    case QSGRendererInterface::Null: return "Null";
    case QSGRendererInterface::Unknown:
    default: return "Unknown";
    }
}

const QSGGeometry::AttributeSet& waterfallTexturedPoint2DAttributes()
{
    static QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::createWithAttributeType(
            0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(
            1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute)
    };
    static QSGGeometry::AttributeSet attributeSet {
        2,
        sizeof(QSGGeometry::TexturedPoint2D),
        attributes
    };
    return attributeSet;
}

qint64 monotonicMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

QSGNode* sceneGraphChildAt(QSGNode* parent, int index)
{
    if (!parent || index < 0)
        return nullptr;
    QSGNode* child = parent->firstChild();
    while (child && index-- > 0)
        child = child->nextSibling();
    return child;
}

void removeSceneGraphChildrenFrom(QSGNode* parent, QSGNode* first)
{
    if (!parent || !first)
        return;
    QSGNode* child = first;
    while (child) {
        QSGNode* next = child->nextSibling();
        parent->removeChildNode(child);
        delete child;
        child = next;
    }
}

QSGGeometryNode* ensureFlatColorNode(QSGNode* parent,
                                     int index,
                                     int vertexCount,
                                     QSGGeometry::DrawingMode drawingMode,
                                     QColor const& color)
{
    if (!parent || vertexCount <= 0)
        return nullptr;

    QSGNode* child = sceneGraphChildAt(parent, index);
    auto* node = dynamic_cast<QSGGeometryNode*>(child);
    auto* material = node ? dynamic_cast<QSGFlatColorMaterial*>(node->material()) : nullptr;
    if (child && (!node || !material)) {
        removeSceneGraphChildrenFrom(parent, child);
        child = nullptr;
        node = nullptr;
        material = nullptr;
    }

    if (!node) {
        node = new QSGGeometryNode();
        auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), vertexCount);
        geometry->setDrawingMode(drawingMode);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        material = new QSGFlatColorMaterial();
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
        parent->appendChildNode(node);
    } else {
        QSGGeometry* geometry = node->geometry();
        if (geometry && geometry->vertexCount() != vertexCount)
            geometry->allocate(vertexCount);
        if (geometry)
            geometry->setDrawingMode(drawingMode);
    }

    if (material)
        material->setColor(color);
    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    return node;
}

void writeRectGeometry(QSGGeometry::Point2D* vertices, QRectF const& rect)
{
    if (!vertices)
        return;

    QRectF const r = rect.normalized();
    float const left = static_cast<float>(r.left());
    float const top = static_cast<float>(r.top());
    float const right = static_cast<float>(r.right());
    float const bottom = static_cast<float>(r.bottom());

    vertices[0].set(left, top);
    vertices[1].set(right, top);
    vertices[2].set(left, bottom);
    vertices[3].set(left, bottom);
    vertices[4].set(right, top);
    vertices[5].set(right, bottom);
}

#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
QImage rgba8888TextureImage(const QImage& image)
{
    if (image.isNull())
        return {};
    if (image.format() == QImage::Format_RGBA8888)
        return image.copy();
    return image.convertToFormat(QImage::Format_RGBA8888);
}

struct RawDbUploadStats
{
    float minDb = std::numeric_limits<float>::infinity();
    float maxDb = -std::numeric_limits<float>::infinity();
    qsizetype finiteSamples = 0;
};

void accumulateRawDbStats(float const* src, int width, RawDbUploadStats& stats)
{
    if (!src || width <= 0)
        return;
    for (int x = 0; x < width; ++x) {
        float const value = src[x];
        if (!std::isfinite(value))
            continue;
        ++stats.finiteSamples;
        stats.minDb = qMin(stats.minDb, value);
        stats.maxDb = qMax(stats.maxDb, value);
    }
}

RawDbUploadStats rawDbImageStats(const QVector<float>& values, int width, int height)
{
    RawDbUploadStats stats;
    if (width <= 0 || height <= 0 || values.size() < width * height)
        return stats;
    for (int y = 0; y < height; ++y)
        accumulateRawDbStats(values.constData() + y * width, width, stats);
    return stats;
}

class DecodiumRhiImageTexture final : public QSGTexture
{
public:
    explicit DecodiumRhiImageTexture(bool alpha = false)
        : m_hasAlpha(alpha)
    {
    }

    ~DecodiumRhiImageTexture() override
    {
        delete m_texture;
    }

    qint64 comparisonKey() const override
    {
        return static_cast<qint64>(reinterpret_cast<quintptr>(this));
    }

    QRhiTexture* rhiTexture() const override
    {
        return m_texture;
    }

    QSize textureSize() const override
    {
        return m_size;
    }

    bool hasAlphaChannel() const override
    {
        return m_hasAlpha;
    }

    bool hasMipmaps() const override
    {
        return false;
    }

    bool failed() const
    {
        return m_failed;
    }

    void uploadFullImage(const QImage& image, bool alpha)
    {
        if (image.isNull())
            return;
        m_size = image.size();
        m_hasAlpha = alpha;
        m_pendingFullImage = rgba8888TextureImage(image);
        m_pendingUploads.clear();
        m_failed = false;
    }

    void uploadFullRgbaImage(const QImage& image, bool alpha)
    {
        if (image.isNull())
            return;
        m_size = image.size();
        m_hasAlpha = alpha;
        m_pendingFullImage = (image.format() == QImage::Format_RGBA8888)
            ? image.copy()
            : image.convertToFormat(QImage::Format_RGBA8888);
        m_pendingUploads.clear();
        m_failed = false;
    }

    void uploadSubImage(const QPoint& topLeft, const QImage& image)
    {
        if (image.isNull() || m_size.isEmpty())
            return;
        QRect const dstRect(topLeft, image.size());
        if (!QRect(QPoint(0, 0), m_size).contains(dstRect))
            return;
        PendingUpload upload;
        upload.topLeft = topLeft;
        upload.image = rgba8888TextureImage(image);
        m_pendingUploads.append(upload);
        m_failed = false;
    }

    void commitTextureOperations(QRhi* rhi, QRhiResourceUpdateBatch* resourceUpdates) override
    {
        if (!rhi || !resourceUpdates || m_size.isEmpty())
            return;

        bool const needsTexture = !m_texture
            || m_rhi != rhi
            || m_texture->pixelSize() != m_size
            || m_texture->format() != QRhiTexture::RGBA8;

        if (needsTexture) {
            delete m_texture;
            m_texture = rhi->newTexture(QRhiTexture::RGBA8, m_size);
            m_rhi = rhi;
            if (!m_texture || !m_texture->create()) {
                delete m_texture;
                m_texture = nullptr;
                m_failed = true;
                m_pendingFullImage = QImage();
                m_pendingUploads.clear();
                return;
            }
            if (m_pendingFullImage.isNull()) {
                m_pendingFullImage = QImage(m_size, QImage::Format_RGBA8888);
                m_pendingFullImage.fill(Qt::transparent);
            }
        }

        if (!m_texture)
            return;

        if (!m_pendingFullImage.isNull()) {
            resourceUpdates->uploadTexture(m_texture, m_pendingFullImage);
            m_pendingFullImage = QImage();
            m_pendingUploads.clear();
            return;
        }

        if (m_pendingUploads.isEmpty())
            return;

        QVector<QRhiTextureUploadEntry> entries;
        entries.reserve(m_pendingUploads.size());
        for (const PendingUpload& upload : std::as_const(m_pendingUploads)) {
            QRhiTextureSubresourceUploadDescription desc(upload.image);
            desc.setDestinationTopLeft(upload.topLeft);
            entries.append(QRhiTextureUploadEntry(0, 0, desc));
        }
        QRhiTextureUploadDescription uploadDescription;
        uploadDescription.setEntries(entries.cbegin(), entries.cend());
        resourceUpdates->uploadTexture(m_texture, uploadDescription);
        m_pendingUploads.clear();
    }

private:
    struct PendingUpload
    {
        QPoint topLeft;
        QImage image;
    };

    QSize m_size;
    bool m_hasAlpha = false;
    bool m_failed = false;
    QRhi* m_rhi = nullptr;
    QRhiTexture* m_texture = nullptr;
    QImage m_pendingFullImage;
    QVector<PendingUpload> m_pendingUploads;
};

class DecodiumRhiFloatTexture final : public QSGTexture
{
public:
    ~DecodiumRhiFloatTexture() override
    {
        delete m_texture;
    }

    qint64 comparisonKey() const override
    {
        return static_cast<qint64>(reinterpret_cast<quintptr>(this));
    }

    QRhiTexture* rhiTexture() const override
    {
        return m_texture;
    }

    QSize textureSize() const override
    {
        return m_size;
    }

    bool hasAlphaChannel() const override
    {
        return false;
    }

    bool hasMipmaps() const override
    {
        return false;
    }

    bool failed() const
    {
        return m_failed;
    }

    void uploadFullFloats(QSize size, float const* values)
    {
        if (size.isEmpty() || !values)
            return;
        m_size = size;
        m_pendingFullData = QByteArray(reinterpret_cast<char const*>(values),
                                       size.width() * size.height() * static_cast<int>(sizeof(float)));
        m_pendingUploads.clear();
        m_failed = false;
    }

    void uploadFloatRow(int row, int width, float const* values)
    {
        if (row < 0 || width <= 0 || !values || m_size.isEmpty())
            return;
        if (row >= m_size.height() || width > m_size.width())
            return;
        PendingFloatUpload upload;
        upload.topLeft = QPoint(0, row);
        upload.size = QSize(width, 1);
        upload.data = QByteArray(reinterpret_cast<char const*>(values),
                                 width * static_cast<int>(sizeof(float)));
        m_pendingUploads.append(upload);
        m_failed = false;
    }

    void commitTextureOperations(QRhi* rhi, QRhiResourceUpdateBatch* resourceUpdates) override
    {
        if (!rhi || !resourceUpdates || m_size.isEmpty())
            return;

        bool const needsTexture = !m_texture
            || m_rhi != rhi
            || m_texture->pixelSize() != m_size
            || m_texture->format() != QRhiTexture::R32F;

        if (needsTexture) {
            delete m_texture;
            m_texture = rhi->newTexture(QRhiTexture::R32F, m_size);
            m_rhi = rhi;
            if (!m_texture || !m_texture->create()) {
                delete m_texture;
                m_texture = nullptr;
                m_failed = true;
                m_pendingFullData.clear();
                m_pendingUploads.clear();
                return;
            }
            if (m_pendingFullData.isEmpty())
                m_pendingFullData = QByteArray(m_size.width() * m_size.height() * static_cast<int>(sizeof(float)), 0);
        }

        if (!m_texture)
            return;

        auto uploadBytes = [&](QByteArray const& data, QSize const& size, QPoint const& topLeft) {
            if (data.isEmpty() || size.isEmpty())
                return;
            QRhiTextureSubresourceUploadDescription desc(data);
            desc.setSourceSize(size);
            desc.setDataStride(size.width() * static_cast<quint32>(sizeof(float)));
            desc.setDestinationTopLeft(topLeft);
            QRhiTextureUploadDescription uploadDescription;
            QRhiTextureUploadEntry entry(0, 0, desc);
            uploadDescription.setEntries(&entry, &entry + 1);
            resourceUpdates->uploadTexture(m_texture, uploadDescription);
        };

        if (!m_pendingFullData.isEmpty()) {
            uploadBytes(m_pendingFullData, m_size, QPoint(0, 0));
            m_pendingFullData.clear();
            m_pendingUploads.clear();
            return;
        }

        for (PendingFloatUpload const& upload : std::as_const(m_pendingUploads))
            uploadBytes(upload.data, upload.size, upload.topLeft);
        m_pendingUploads.clear();
    }

private:
    struct PendingFloatUpload
    {
        QPoint topLeft;
        QSize size;
        QByteArray data;
    };

    QSize m_size;
    bool m_failed = false;
    QRhi* m_rhi = nullptr;
    QRhiTexture* m_texture = nullptr;
    QByteArray m_pendingFullData;
    QVector<PendingFloatUpload> m_pendingUploads;
};
#endif

#if defined(DECODIUM_QT_RHI_TEXTURE_UPLOAD) && defined(DECODIUM_GPU_PANADAPTER_FFT_QSB)
struct GpuFftParams
{
    qint32 n = 4096;
    qint32 binStart = 0;
    qint32 nBins = 0;
    qint32 mode = 0;
    qint32 stage = 0;
    qint32 srcA = 1;
    qint32 reserved0 = 0;
    qint32 reserved1 = 0;
    float inverseNormSquared = 1.0f;
    float powerFloor = 1e-24f;
    float reserved2 = 0.0f;
    float reserved3 = 0.0f;
};

static_assert(sizeof(GpuFftParams) == 48);
#endif
}

#if defined(DECODIUM_QT_RHI_TEXTURE_UPLOAD) && defined(DECODIUM_GPU_PANADAPTER_FFT_QSB)
struct PanadapterItem::GpuFftState
{
    QRhi* rhi = nullptr;
    QRhiBuffer* sampleBuffer = nullptr;
    QRhiBuffer* outputBuffer = nullptr;
    QRhiBuffer* paramsBuffer = nullptr;
    QRhiShaderResourceBindings* srb = nullptr;
    QRhiComputePipeline* pipeline = nullptr;
    QShader shader;
    int sampleCapacity = 0;
    int outputCapacity = 0;
    bool readbackPending = false;
    bool loggedActive = false;
    bool loggedReadbackStats = false;

    ~GpuFftState()
    {
        reset();
    }

    void reset()
    {
        delete pipeline;
        delete srb;
        delete paramsBuffer;
        delete outputBuffer;
        delete sampleBuffer;
        pipeline = nullptr;
        srb = nullptr;
        paramsBuffer = nullptr;
        outputBuffer = nullptr;
        sampleBuffer = nullptr;
        rhi = nullptr;
        sampleCapacity = 0;
        outputCapacity = 0;
        readbackPending = false;
    }
};
#else
struct PanadapterItem::GpuFftState {};
#endif

#ifdef DECODIUM_WATERFALL_SHADER_QSB
class WaterfallPaletteMaterial final : public QSGMaterial
{
public:
    WaterfallPaletteMaterial()
    {
        setFlag(QSGMaterial::NoBatching);
        setFlag(QSGMaterial::RequiresFullMatrix);
    }

    ~WaterfallPaletteMaterial() override
    {
        delete intensityTexture;
        delete paletteTexture;
        delete rowParamsTexture;
        for (QSGTexture* texture : retiredTextures)
            delete texture;
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
        if (intensityTexture != rhs->intensityTexture)
            return intensityTexture < rhs->intensityTexture ? -1 : 1;
        if (paletteTexture != rhs->paletteTexture)
            return paletteTexture < rhs->paletteTexture ? -1 : 1;
        if (rowParamsTexture != rhs->rowParamsTexture)
            return rowParamsTexture < rhs->rowParamsTexture ? -1 : 1;
        return 0;
    }

    QSGTexture* intensityTexture = nullptr;
    QSGTexture* paletteTexture = nullptr;
    QSGTexture* rowParamsTexture = nullptr;
    float params[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float levelParams[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    float xParams[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    int paletteGeneration = -1;

    void retireTexture(QSGTexture*& texture)
    {
        if (!texture)
            return;
        retiredTextures.append(texture);
        texture = nullptr;
        while (retiredTextures.size() > 4)
            delete retiredTextures.takeFirst();
    }

    QVector<QSGTexture*> retiredTextures;
};

class WaterfallPaletteShader final : public QSGMaterialShader
{
public:
    WaterfallPaletteShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/waterfall_palette.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/waterfall_palette.frag.qsb"));
    }

    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial*) override
    {
        QByteArray* uniformData = state.uniformData();
        if (uniformData->size() < 128) {
            uniformData->resize(128);
            std::memset(uniformData->data(), 0, static_cast<size_t>(uniformData->size()));
        }
        auto* material = static_cast<WaterfallPaletteMaterial*>(newMaterial);
        const QMatrix4x4 matrix = state.combinedMatrix();
        std::memcpy(uniformData->data(), matrix.constData(), 64);
        float const opacity = state.opacity();
        std::memcpy(uniformData->data() + 64, &opacity, 4);
        std::memcpy(uniformData->data() + 80, material->params, sizeof(material->params));
        std::memcpy(uniformData->data() + 96, material->levelParams, sizeof(material->levelParams));
        std::memcpy(uniformData->data() + 112, material->xParams, sizeof(material->xParams));
        return true;
    }

    void updateSampledImage(RenderState& state, int binding, QSGTexture** texture,
                            QSGMaterial* newMaterial, QSGMaterial*) override
    {
        auto* material = static_cast<WaterfallPaletteMaterial*>(newMaterial);
        static std::atomic_uint loggedBindings {0};
        unsigned const bit = (binding >= 0 && binding < 31) ? (1u << binding) : 0u;
        unsigned previous = loggedBindings.load(std::memory_order_relaxed);
        if (bit && !(previous & bit)
            && !(loggedBindings.fetch_or(bit, std::memory_order_relaxed) & bit)) {
            qInfo().noquote()
                << "[GPUDBG] Panadapter waterfall shader sampled-image binding"
                << binding;
        }
        if (binding == 1)
            *texture = material->intensityTexture;
        else if (binding == 2)
            *texture = material->paletteTexture;
        else if (binding == 3)
            *texture = material->rowParamsTexture;

        if (*texture) {
            (*texture)->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
            static std::atomic_bool loggedTextureCommit {false};
            if (!loggedTextureCommit.exchange(true, std::memory_order_relaxed)) {
                qInfo().noquote()
                    << "[GPUDBG] Panadapter waterfall shader texture upload committed via RHI batch";
            }
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

PanadapterItem::~PanadapterItem()
{
    releaseGpuFftResources();
}

bool PanadapterItem::shaderWaterfallSupported()
{
    m_shaderWaterfallDisabledReason.clear();
#ifdef DECODIUM_WATERFALL_SHADER_QSB
    if (qEnvironmentVariableIsSet("DECODIUM_DISABLE_WATERFALL_SHADER")) {
        m_shaderWaterfallDisabledReason =
            QStringLiteral("shader disabled by DECODIUM_DISABLE_WATERFALL_SHADER; colored texture upload");
        return false;
    }
    if (m_shaderWaterfallBlocked) {
        m_shaderWaterfallDisabledReason = QStringLiteral("shader resource fallback");
        return false;
    }
    if (!window() || !window()->rendererInterface()) {
        m_shaderWaterfallDisabledReason = QStringLiteral("scenegraph window not ready; colored texture upload");
        return false;
    }
    QSGRendererInterface::GraphicsApi const api = window()->rendererInterface()->graphicsApi();
    if (api == QSGRendererInterface::Software
        || api == QSGRendererInterface::Null
        || api == QSGRendererInterface::Unknown) {
        m_shaderWaterfallDisabledReason =
            QStringLiteral("scenegraph api has no shader path; CPU fallback");
        return false;
    }
    bool const globalExperimentalShader =
        qEnvironmentVariableIsSet("DECODIUM_ENABLE_EXPERIMENTAL_WATERFALL_SHADER");
    bool apiExperimentalShader = false;
    switch (api) {
    case QSGRendererInterface::OpenGL:
        apiExperimentalShader = qEnvironmentVariableIsSet("DECODIUM_ENABLE_EXPERIMENTAL_OPENGL_WATERFALL_SHADER");
        break;
    case QSGRendererInterface::Metal:
        apiExperimentalShader = qEnvironmentVariableIsSet("DECODIUM_ENABLE_EXPERIMENTAL_METAL_WATERFALL_SHADER");
        break;
    case QSGRendererInterface::Direct3D11:
        apiExperimentalShader = qEnvironmentVariableIsSet("DECODIUM_ENABLE_EXPERIMENTAL_D3D11_WATERFALL_SHADER");
        break;
    case QSGRendererInterface::Vulkan:
        apiExperimentalShader = qEnvironmentVariableIsSet("DECODIUM_ENABLE_EXPERIMENTAL_VULKAN_WATERFALL_SHADER");
        break;
#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
    case QSGRendererInterface::Direct3D12:
        apiExperimentalShader = qEnvironmentVariableIsSet("DECODIUM_ENABLE_EXPERIMENTAL_D3D12_WATERFALL_SHADER");
        break;
#endif
    default:
        break;
    }
    bool const apiShaderEnabledByDefault =
        api == QSGRendererInterface::OpenGL
        || api == QSGRendererInterface::Metal
        || api == QSGRendererInterface::Direct3D11
        || api == QSGRendererInterface::Vulkan
#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
        || api == QSGRendererInterface::Direct3D12
#endif
        ;
    if (!apiShaderEnabledByDefault && !globalExperimentalShader && !apiExperimentalShader) {
        m_shaderWaterfallDisabledReason =
            QStringLiteral("shader disabled by default on %1; colored texture upload")
                .arg(QString::fromLatin1(waterfallGraphicsApiName(api)));
        return false;
    }
    return true;
#else
    m_shaderWaterfallDisabledReason = QStringLiteral("qsb shaders not compiled");
    return false;
#endif
}

bool PanadapterItem::spectrumGraphSupported() const
{
#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
    if (qEnvironmentVariableIsSet("DECODIUM_DISABLE_SPECTRUM_GPU_GRAPH"))
        return false;
    if (!qEnvironmentVariableIsSet("DECODIUM_ENABLE_SPECTRUM_GPU_GRAPH"))
        return false;
    if (!window() || !window()->rendererInterface())
        return false;
    QSGRendererInterface::GraphicsApi const api = window()->rendererInterface()->graphicsApi();
    return api != QSGRendererInterface::Software
        && api != QSGRendererInterface::Null
        && api != QSGRendererInterface::Unknown
        && api != QSGRendererInterface::OpenVG;
#else
    return false;
#endif
}

// ─── Palette ────────────────────────────────────────────────────────────────
void PanadapterItem::buildPalette(int idx)
{
    ++m_paletteGeneration;
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
    m_waterfallRgbValid = false;
    QColor const bg = (v == 11) ? QColor(255, 255, 255) : QColor(0, 0, 0);
    if (!m_spectrumImage.isNull())                  m_spectrumImage.fill(bg);
    if (!m_waterfallImage.isNull())                 m_waterfallImage.fill(bg);
    if (!m_waterfallDisplayImage.isNull())          m_waterfallDisplayImage.fill(bg);
    if (!m_waterfallIntensityImage.isNull())        m_waterfallIntensityImage.fill(0);
    if (!m_waterfallIntensityDisplayImage.isNull()) m_waterfallIntensityDisplayImage.fill(0);
    m_loggedWaterfallGpuUploadStats = false;
    m_lastWaterfallGpuStatsRow = -1;
    m_waterfallGpuUploadedWriteRow = 0;
    m_waterfallGpuUploadedSize = QSize();
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
            m_waterfallIntensityTextureImage = QImage(w, wfH, QImage::Format_ARGB32_Premultiplied);
            m_waterfallIntensityTextureImage.fill(QColor(0, 0, 0, 255));
            m_waterfallDbRows.clear();
            m_waterfallDbRowParams.clear();
            m_waterfallRawBinsWidth = 0;
            m_wfWriteRow = 0;
            m_waterfallRgbValid = true;
            m_shaderWaterfallBlocked = false;
            m_loggedWaterfallGpuUploadStats = false;
            m_lastWaterfallGpuStatsRow = -1;
            m_waterfallGpuUploadedWriteRow = 0;
            m_waterfallGpuUploadedSize = QSize();
        }
    } else {
        m_waterfallImage = QImage();
        m_waterfallDisplayImage = QImage();
        m_waterfallIntensityImage = QImage();
        m_waterfallIntensityDisplayImage = QImage();
        m_waterfallIntensityTextureImage = QImage();
        m_waterfallDbRows.clear();
        m_waterfallDbRowParams.clear();
        m_waterfallRawBinsWidth = 0;
        m_wfWriteRow = 0;
        m_waterfallRgbValid = true;
        m_shaderWaterfallBlocked = false;
        m_loggedWaterfallGpuUploadStats = false;
        m_lastWaterfallGpuStatsRow = -1;
        m_waterfallGpuUploadedWriteRow = 0;
        m_waterfallGpuUploadedSize = QSize();
    }
}

// ─── Add spectrum data ───────────────────────────────────────────────────────
void PanadapterItem::addSpectrumData(const QVector<float>& dbValues,
                                      float minDb, float maxDb,
                                      float freqMinHz, float freqMaxHz)
{
    QMutexLocker lock(&m_mutex);
    if (dbValues.isEmpty()) return;

    bool const gpuFftRecentlyFed =
        m_lastGpuFftFrameMs > 0
        && !m_gpuFftFailed
        && monotonicMs() - m_lastGpuFftFrameMs < 1500;
    if (gpuFftRecentlyFed
        && m_gpuFftUiBinsExpected > 0
        && dbValues.size() != m_gpuFftUiBinsExpected) {
        if (!m_loggedMismatchedSpectrumSuppressed) {
            qInfo().noquote()
                << "[PANDBG] Panadapter mismatched spectrum suppressed"
                << "reason=GPU_FFT_feed_active"
                << "bins=" << dbValues.size()
                << "expected_bins=" << m_gpuFftUiBinsExpected;
            m_loggedMismatchedSpectrumSuppressed = true;
        }
        return;
    }

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

    m_pendingWaterfallRows.append(WaterfallFrame {
        m_bins,
        m_minDb,
        m_maxDb,
        m_dataFreqMin,
        m_dataFreqMax
    });
    static constexpr int kMaxPendingWaterfallRows = 8;
    while (m_pendingWaterfallRows.size() > kMaxPendingWaterfallRows)
        m_pendingWaterfallRows.removeFirst();

    m_spectrumDirty = true;

    // Render throttle: quando attivo (es. FT2 engine non-Idle), saltiamo
    // update() finché non sono passati 100 ms dall'ultimo paint. I dati FFT
    // restano in m_pendingWaterfallRows (max 8 frame) e verranno tutti
    // processati al prossimo updatePaintNode. Così riduciamo il carico
    // main-thread sul render durante slot decode pesanti.
    bool shouldEmitUpdate = true;
    if (m_throttleActive) {
        // 1.0.98: intervallo letto da m_throttleIntervalMs (default 100ms = 10 fps).
        // QML può alzarlo a 200ms (5 fps) durante FT2 sotto propagazione cattiva
        // per recuperare il bottleneck GPU/main introdotto dal panadapter
        // cherry-pickato dall'upstream 1.0.95.
        const qint64 throttleNs = qint64(m_throttleIntervalMs) * 1000 * 1000;
        const qint64 nowNs = std::chrono::steady_clock::now().time_since_epoch().count();
        if (nowNs - m_lastUpdateNs < throttleNs) {
            shouldEmitUpdate = false;
        } else {
            m_lastUpdateNs = nowNs;
        }
    }
    lock.unlock();
    if (shouldEmitUpdate) update();
}

// Compatibilità: riceve valori 0-1 normalizzati e li converte in dB
void PanadapterItem::addSpectrumDataNorm(const QVector<float>& normValues)
{
    {
        QMutexLocker lock(&m_mutex);
        bool const gpuFftRecentlyFed =
            m_lastGpuFftFrameMs > 0
            && !m_gpuFftFailed
            && monotonicMs() - m_lastGpuFftFrameMs < 1500;
        if (gpuFftRecentlyFed) {
            if (!m_loggedLegacySpectrumSuppressed) {
                qInfo().noquote()
                    << "[PANDBG] Panadapter legacy normalized spectrum suppressed"
                    << "reason=GPU_FFT_feed_active"
                    << "legacy_bins=" << normValues.size();
                m_loggedLegacySpectrumSuppressed = true;
            }
            return;
        }
    }

    // Converti 0-1 → dB range [-130, -40], range frequenze: nfa(200Hz) - nfb(4000Hz)
    QVector<float> db;
    db.reserve(normValues.size());
    for (float v : normValues)
        db.append(-130.f + v * 90.f);
    addSpectrumData(db, -130.f, -40.f, 200.f, 4000.f);
}

bool PanadapterItem::gpuFftSupported(QString* reason) const
{
#if defined(DECODIUM_QT_RHI_TEXTURE_UPLOAD) && defined(DECODIUM_GPU_PANADAPTER_FFT_QSB)
    if (qEnvironmentVariableIsSet("DECODIUM_DISABLE_GPU_PANADAPTER_FFT")) {
        if (reason)
            *reason = QStringLiteral("disabled by DECODIUM_DISABLE_GPU_PANADAPTER_FFT");
        return false;
    }
    if (m_gpuFftFailed) {
        if (reason)
            *reason = m_gpuFftFailureReason.isEmpty()
                ? QStringLiteral("previous GPU FFT failure")
                : m_gpuFftFailureReason;
        return false;
    }
    QQuickWindow* win = window();
    if (!win) {
        if (reason)
            *reason = QStringLiteral("no QQuickWindow yet");
        return false;
    }
    auto* rif = win->rendererInterface();
    if (!rif) {
        if (reason)
            *reason = QStringLiteral("no QSGRendererInterface");
        return false;
    }
    auto api = rif->graphicsApi();
    if (!QSGRendererInterface::isApiRhiBased(api)
        || api == QSGRendererInterface::Software
        || api == QSGRendererInterface::Null) {
        if (reason)
            *reason = QStringLiteral("non-RHI graphics API %1")
                .arg(QString::fromLatin1(waterfallGraphicsApiName(api)));
        return false;
    }
    if (reason)
        reason->clear();
    return true;
#else
    if (reason)
        *reason = QStringLiteral("compiled without Qt RHI compute shader support");
    return false;
#endif
}

bool PanadapterItem::addPcmFrame(const QVector<float>& samples,
                                 int usableSamples,
                                 int nfa,
                                 int nfb,
                                 float freqMinHz,
                                 float freqMaxHz,
                                 quint64 serial)
{
    QString reason;
    if (!gpuFftSupported(&reason)) {
        if (!m_loggedGpuFftRejected) {
            qWarning().noquote()
                << "[PANDBG] Panadapter visual FFT GPU path rejected"
                << "reason=" << reason;
            m_loggedGpuFftRejected = true;
        }
        return false;
    }
    if (samples.size() < 4096 || nfb <= nfa || freqMaxHz <= freqMinHz) {
        if (reason.isEmpty())
            reason = QStringLiteral("invalid PCM frame");
        return false;
    }
    if (usableSamples < 1024) {
        if (!m_loggedGpuFftWarmupSkip) {
            qInfo().noquote()
                << "[PANDBG] Panadapter visual FFT GPU warmup skipped"
                << "reason=not enough usable PCM samples yet"
                << "usable=" << usableSamples
                << "required=" << 1024;
            m_loggedGpuFftWarmupSkip = true;
        }
        return true;
    }

    double sumSq = 0.0;
    float samplePeak = 0.0f;
    int const statsSamples = qMin(samples.size(), 4096);
    for (int i = 0; i < statsSamples; ++i) {
        float const v = samples.at(i);
        samplePeak = qMax(samplePeak, std::abs(v));
        sumSq += static_cast<double>(v) * static_cast<double>(v);
    }
    float const sampleRms = statsSamples > 0
        ? static_cast<float>(std::sqrt(sumSq / static_cast<double>(statsSamples)))
        : 0.0f;
    if (!m_loggedGpuFftInputStats) {
        qInfo().noquote()
            << "[GPUDBG] Panadapter visual FFT input stats"
            << "samples=" << statsSamples
            << "usable=" << usableSamples
            << "peak=" << samplePeak
            << "rms=" << sampleRms;
        m_loggedGpuFftInputStats = true;
    }

    QMutexLocker lock(&m_mutex);
    m_pendingPcmFrame.samples = samples;
    if (m_pendingPcmFrame.samples.size() > 4096)
        m_pendingPcmFrame.samples.resize(4096);
    m_pendingPcmFrame.usableSamples = usableSamples;
    m_pendingPcmFrame.nfa = nfa;
    m_pendingPcmFrame.nfb = nfb;
    m_pendingPcmFrame.freqMinHz = freqMinHz;
    m_pendingPcmFrame.freqMaxHz = freqMaxHz;
    m_pendingPcmFrame.samplePeak = samplePeak;
    m_pendingPcmFrame.sampleRms = sampleRms;
    m_pendingPcmFrame.serial = serial;
    m_hasPendingPcmFrame = true;
    constexpr int kGpuFftN = 4096;
    float const gpuFreqPerBin = 12000.0f / static_cast<float>(kGpuFftN);
    int const gpuBinStart = qBound(0, static_cast<int>(nfa / gpuFreqPerBin), kGpuFftN / 2);
    int const gpuBinEnd = qBound(gpuBinStart, static_cast<int>(nfb / gpuFreqPerBin), kGpuFftN / 2);
    m_gpuFftUiBinsExpected = qMax(0, gpuBinEnd - gpuBinStart);
    m_lastGpuFftFrameMs = monotonicMs();
    lock.unlock();

    if (!m_loggedGpuFftAccepted) {
        qInfo().noquote()
            << "[PANDBG] Panadapter visual FFT path active"
            << "engine=RHI_compute_gpu"
            << "algorithm=single_pass_dft_recurrence_4096"
            << "fallback=FFTW_CPU_on_failure";
        m_loggedGpuFftAccepted = true;
    }
    update();
    return true;
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
    if (!m_waterfallIntensityTextureImage.isNull())
        m_waterfallIntensityTextureImage.fill(QColor(0, 0, 0, 255));
    std::fill(m_waterfallDbRows.begin(), m_waterfallDbRows.end(), -200.0f);
    for (int row = 0; row < m_waterfallDbRowParams.size() / 2; ++row) {
        m_waterfallDbRowParams[row * 2] = -130.0f;
        m_waterfallDbRowParams[row * 2 + 1] = 1.0f / 90.0f;
    }
    m_wfWriteRow = 0;
    m_pendingWaterfallRows.clear();
    m_waterfallRgbValid = true;
    m_loggedWaterfallGpuUploadStats = false;
    m_lastWaterfallGpuStatsRow = -1;
    m_waterfallGpuUploadedWriteRow = 0;
    m_waterfallGpuUploadedSize = QSize();
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
    bool const gpuSpectrumGraph = spectrumGraphSupported();

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

    if (!gpuSpectrumGraph) {
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
            QString call;
            int freq;
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

            // 1.0.131: WSJT-X palette cascade highlight passed from QML
            // (bridge.decodeHighlightBg) as hex string in "color" field.
            // Falls back to legacy fixed palette when empty/invalid.
            QString const highlightHex = d.value("color").toString();
            QColor const highlight(highlightHex);

            QColor col;
            if (highlight.isValid()) {
                col = highlight;
            } else if (m_labelUseCustomColor) {
                col = m_labelColor;
            } else {
                col = isCQ ? QColor(0, 230, 100)
                           : (isMyCall ? QColor(255, 80, 80) : QColor(0, 200, 255));
            }

            items.push_back({lx, textW, text, col, call, freq});
        }

        std::sort(items.begin(), items.end(),
                  [](const LabelItem& a, const LabelItem& b){ return a.x < b.x; });

        QVector<int> rowRightX(maxRows, -1000000);

        m_decodeHitRects.clear();
        m_decodeHitRects.reserve(items.size());
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

            // Hit-rect per click-to-call sul callsign decodificato
            QRect hr(textX - 2, textY - rowH + fm.descent(),
                     it.textW + 4, rowH);
            m_decodeHitRects.push_back({hr, it.call, it.freq});
        }
    } else {
        m_decodeHitRects.clear();
    }

    // ── DX Cluster spots overlay ─────────────────────────────────────────
    // Render in colore distinto (giallo) sotto le decode label, riga in basso.
    // Lista già pre-filtrata dal QML per banda/dial corrente: ogni voce ha
    // {call, freq} con freq in audio Hz relativi alla dial.
    m_clusterHitRects.clear();
    if (m_showDxClusterSpots && !m_dxClusterSpots.isEmpty()) {
        QFont clusterFont("Consolas", m_labelFontSize, QFont::Bold);
        p.setFont(clusterFont);
        QFontMetrics fm(clusterFont);
        const int rowH = fm.height();
        // Riga dedicata: subito sopra il bottomKeepOut (h-22).
        const int clusterBaseline = h - 24;

        QVector<int> takenLeft;
        takenLeft.reserve(m_dxClusterSpots.size());
        for (const auto& v : m_dxClusterSpots) {
            QVariantMap d = v.toMap();
            QString call = d.value("call").toString();
            int freq = d.value("freq").toInt();
            if (call.isEmpty() || freq <= 0) continue;
            int lx = fToX(freq);
            if (lx < 0 || lx >= w) continue;

            QString text = call;
            int textW = fm.horizontalAdvance(text);
            int textX = lx + 2;

            // Anti-overlap rudimentale: salta se la X parte troppo vicina.
            bool conflict = false;
            for (int taken : takenLeft) {
                if (qAbs(textX - taken) < textW + m_labelSpacing + 4) {
                    conflict = true;
                    break;
                }
            }
            if (conflict) continue;
            takenLeft.push_back(textX);

            // Linea verticale tratteggiata gialla (solo sopra la riga label)
            p.setPen(QPen(m_dxClusterSpotColor, 1, Qt::DashDotLine));
            p.drawLine(lx, 0, lx, clusterBaseline - rowH);

            // Background scuro per leggibilità
            QRect labelRect(textX - 2,
                            clusterBaseline - rowH + fm.descent(),
                            textW + 4,
                            rowH);
            p.fillRect(labelRect, QColor(0, 0, 0, 180));
            p.setPen(QPen(m_dxClusterSpotColor, 1));
            p.drawRect(labelRect);
            p.drawText(textX, clusterBaseline, text);

            // Hit-test rect (espanso un po' verticalmente per click facili)
            QRect hitRect = labelRect.adjusted(-2, -2, 2, 2);
            m_clusterHitRects.push_back({hitRect, call, freq});
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

void PanadapterItem::removeSpectrumGraphNodes(QSGNode* spectrumRoot)
{
    if (!spectrumRoot)
        return;
    QSGNode* firstGraphChild = sceneGraphChildAt(spectrumRoot, 1);
    removeSceneGraphChildrenFrom(spectrumRoot, firstGraphChild);
}

void PanadapterItem::updateSpectrumGraphNodes(QSGNode* spectrumRoot, int w, int h)
{
#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
    if (!spectrumRoot || w <= 1 || h <= 1 || m_bins.isEmpty()) {
        removeSpectrumGraphNodes(spectrumRoot);
        return;
    }

    int const nBins = m_bins.size();
    float range = m_maxDb - m_minDb;
    if (range < 1.f)
        range = 1.f;

    auto binToY = [&](float db) -> float {
        float const norm = qBound(0.f, (db - m_minDb) / range, 1.f);
        return static_cast<float>(h - 1) - norm * static_cast<float>(h - 2);
    };

    float const baseStart = (m_bandwidth > 0) ? static_cast<float>(m_startFreq) : m_dataFreqMin;
    float const baseEnd = (m_bandwidth > 0) ? static_cast<float>(m_startFreq + m_bandwidth) : m_dataFreqMax;
    float viewportRange = baseEnd - baseStart;
    if (viewportRange <= 0.f)
        viewportRange = 1.f;
    float dataRange = m_dataFreqMax - m_dataFreqMin;
    if (dataRange <= 0.f)
        dataRange = 1.f;
    float const viewRange = viewportRange / m_zoomFactor;
    float const viewCenter = baseStart + viewportRange * 0.5f + m_panHz;
    float const viewStart = viewCenter - viewRange * 0.5f;

    auto yForX = [&](int x, QVector<float> const& values) -> float {
        float const pixFreq = viewStart + static_cast<float>(x) * viewRange / static_cast<float>(w);
        if (pixFreq < m_dataFreqMin || pixFreq > m_dataFreqMax)
            return static_cast<float>(h - 1);
        int bin = static_cast<int>((pixFreq - m_dataFreqMin) / dataRange * nBins);
        bin = qBound(0, bin, nBins - 1);
        return binToY(values[bin]);
    };

    QColor fillColor = QColor::fromRgb(wfColor(0.82f));
    QColor traceGlowColor = QColor::fromRgb(wfColor(0.92f)).lighter(145);
    QColor traceColor = QColor::fromRgb(wfColor(0.98f));
    QColor peakColor(255, 255, 255);
    fillColor.setAlpha(70);
    traceGlowColor.setAlpha(80);
    traceColor.setAlpha(255);
    peakColor.setAlpha(105);

    if (auto* fillNode = ensureFlatColorNode(spectrumRoot,
                                             1,
                                             w * 2,
                                             QSGGeometry::DrawTriangleStrip,
                                             fillColor)) {
        auto* vertices = fillNode->geometry()->vertexDataAsPoint2D();
        for (int x = 0; x < w; ++x) {
            float const y = yForX(x, m_bins);
            vertices[x * 2].set(static_cast<float>(x), y);
            vertices[x * 2 + 1].set(static_cast<float>(x), static_cast<float>(h));
        }
    }

    if (auto* glowNode = ensureFlatColorNode(spectrumRoot,
                                             2,
                                             w,
                                             QSGGeometry::DrawLineStrip,
                                             traceGlowColor)) {
        auto* vertices = glowNode->geometry()->vertexDataAsPoint2D();
        for (int x = 0; x < w; ++x)
            vertices[x].set(static_cast<float>(x), yForX(x, m_bins));
    }

    if (auto* traceNode = ensureFlatColorNode(spectrumRoot,
                                              3,
                                              w,
                                              QSGGeometry::DrawLineStrip,
                                              traceColor)) {
        auto* vertices = traceNode->geometry()->vertexDataAsPoint2D();
        for (int x = 0; x < w; ++x)
            vertices[x].set(static_cast<float>(x), yForX(x, m_bins));
    }

    if (m_peakHold && m_peakBins.size() == nBins) {
        if (auto* peakNode = ensureFlatColorNode(spectrumRoot,
                                                 4,
                                                 w,
                                                 QSGGeometry::DrawLineStrip,
                                                 peakColor)) {
            auto* vertices = peakNode->geometry()->vertexDataAsPoint2D();
            for (int x = 0; x < w; ++x)
                vertices[x].set(static_cast<float>(x), yForX(x, m_peakBins));
        }
    } else {
        QSGNode* peakNode = sceneGraphChildAt(spectrumRoot, 4);
        removeSceneGraphChildrenFrom(spectrumRoot, peakNode);
    }

    static std::atomic_bool loggedSpectrumGraph {false};
    if (!loggedSpectrumGraph.exchange(true, std::memory_order_relaxed)) {
        char const* apiName = "Unknown";
        if (window() && window()->rendererInterface())
            apiName = waterfallGraphicsApiName(window()->rendererInterface()->graphicsApi());
        qInfo().noquote()
            << "[GPUDBG] Panadapter spectrum GPU geometry path active"
            << "api=" << apiName
            << "reason= scene graph line/fill/peak nodes; overlay grid/text/markers rendered by Qt Quick scene graph";
    }
#else
    Q_UNUSED(spectrumRoot)
    Q_UNUSED(w)
    Q_UNUSED(h)
#endif
}

void PanadapterItem::setDecodeLabels(const QVariantList& labels)
{
    m_decodeLabels = labels;
    markDirty();
}

void PanadapterItem::setDxClusterSpots(const QVariantList& spots)
{
    m_dxClusterSpots = spots;
    markDirty();
}

// ─── Add waterfall row ────────────────────────────────────────────────────────
void PanadapterItem::addWaterfallRow(const QVector<float>& bins,
                                     float minDb,
                                     float maxDb,
                                     float dataFreqMin,
                                     float dataFreqMax)
{
    if (m_waterfallImage.isNull() || bins.isEmpty()) return;
    int w  = m_waterfallImage.width();
    int h  = m_waterfallImage.height();
    if (w <= 0 || h <= 0) return;

    int nBins  = bins.size();
    float range = maxDb - minDb;
    if (range < 1.f) range = 1.f;

    // Ring buffer: scrivi nella riga corrente
    // Usa lo stesso sistema di coordinate del renderSpectrum per allineamento perfetto
    float const baseStart = (m_bandwidth > 0) ? static_cast<float>(m_startFreq) : dataFreqMin;
    float const baseEnd = (m_bandwidth > 0) ? static_cast<float>(m_startFreq + m_bandwidth) : dataFreqMax;
    float viewportRange = baseEnd - baseStart;
    if (viewportRange <= 0.f) viewportRange = 1.f;
    float dataRange = dataFreqMax - dataFreqMin;
    if (dataRange <= 0.f) dataRange = 1.f;
    float viewRange  = viewportRange / m_zoomFactor;
    float viewCenter = baseStart + viewportRange * 0.5f + m_panHz;
    float viewStart  = viewCenter - viewRange * 0.5f;

    // BlackLevel: soglia sotto cui tutto è nero (0=nulla, 100=aggressivo)
    // ColorGain: gamma/contrasto (0=molto gamma, 50=lineare, 100=invertito)
    // Gamma > 1 comprime i bassi verso il nero → sfondo pulito, segnali netti
    float blackThresh = qBound(0.0f, m_blackLevel * 0.006f, 0.95f);  // 0.0-0.6 del range normalizzato
    float usableRange = qMax(0.001f, 1.0f - blackThresh);
    float gamma = 2.5f - m_colorGain * 0.02f;   // gain 0→gamma 2.5, gain 50→gamma 1.5, gain 100→gamma 0.5
    if (gamma < 0.3f) gamma = 0.3f;

    QRgb const wfBg = (m_paletteIndex == 11) ? qRgb(255,255,255) : qRgb(0,0,0);

    bool const gpuRawRowPath =
#if defined(DECODIUM_QT_RHI_TEXTURE_UPLOAD) && defined(DECODIUM_WATERFALL_SHADER_QSB)
        shaderWaterfallSupported() && !m_shaderWaterfallBlocked;
#else
        false;
#endif

    if (m_waterfallRawBinsWidth != nBins
        || m_waterfallDbRows.size() != nBins * h
        || m_waterfallDbRowParams.size() != h * 2) {
        m_waterfallRawBinsWidth = nBins;
        m_waterfallDbRows.fill(-200.0f, nBins * h);
        m_waterfallDbRowParams.fill(0.0f, h * 2);
        for (int paramRow = 0; paramRow < h; ++paramRow) {
            m_waterfallDbRowParams[paramRow * 2] = minDb;
            m_waterfallDbRowParams[paramRow * 2 + 1] = 1.0f / range;
        }
        m_wfWriteRow = 0;
        m_waterfallGpuUploadedWriteRow = 0;
        m_waterfallGpuUploadedSize = QSize();
        m_loggedWaterfallGpuUploadStats = false;
        m_lastWaterfallGpuStatsRow = -1;
    }

    if (m_waterfallIntensityImage.isNull()
        || m_waterfallIntensityImage.width() != nBins
        || m_waterfallIntensityImage.height() != h) {
        m_waterfallIntensityImage = QImage(nBins, h, QImage::Format_Grayscale8);
        m_waterfallIntensityImage.fill(0);
        if (!gpuRawRowPath) {
            m_waterfallImage.fill(QColor::fromRgb(wfBg));
            m_wfWriteRow = 0;
            m_waterfallGpuUploadedWriteRow = 0;
            m_waterfallGpuUploadedSize = QSize();
            m_loggedWaterfallGpuUploadStats = false;
            m_lastWaterfallGpuStatsRow = -1;
        }
        m_waterfallRgbValid = false;
    }

    int row = m_wfWriteRow % h;
    if (m_waterfallRawBinsWidth == nBins && m_waterfallDbRows.size() >= (row + 1) * nBins) {
        float* rawRow = m_waterfallDbRows.data() + row * nBins;
        for (int bin = 0; bin < nBins; ++bin)
            rawRow[bin] = std::isfinite(bins[bin]) ? bins[bin] : -200.0f;
    }
    if (m_waterfallDbRowParams.size() >= (row + 1) * 2) {
        m_waterfallDbRowParams[row * 2] = minDb;
        m_waterfallDbRowParams[row * 2 + 1] = 1.0f / range;
    }

    QRgb* line = reinterpret_cast<QRgb*>(m_waterfallImage.scanLine(row));
    uchar* intensityLine = m_waterfallIntensityImage.scanLine(row);
    if (!gpuRawRowPath && intensityLine) {
        for (int bin = 0; bin < nBins; ++bin) {
            float const rawNorm = qBound(0.0f, (bins[bin] - minDb) / range, 1.0f);
            intensityLine[bin] = static_cast<uchar>(qBound(0, static_cast<int>(rawNorm * 255.f + 0.5f), 255));
        }
    }

    bool const prepareCpuPixels = !gpuRawRowPath && (!m_useShaderWaterfall || m_wfWriteRow < h || m_shaderWaterfallBlocked);
    if (prepareCpuPixels && line) {
        for (int x = 0; x < w; ++x) {
            float pixFreq = viewStart + (float)x * viewRange / w;
            if (pixFreq < dataFreqMin || pixFreq > dataFreqMax) {
                line[x] = wfBg;
                continue;
            }
            int bin = (int)((pixFreq - dataFreqMin) / dataRange * nBins);
            bin = qBound(0, bin, nBins - 1);
            float rawNorm = static_cast<float>(intensityLine[bin]) / 255.0f;

            // CPU fallback: mantiene lo stesso colore finale. Il path shader usa
            // la riga raw per bin e fa mapping frequenza + black/gamma in GPU.
            float adjusted = (rawNorm - blackThresh) / usableRange;
            if (adjusted <= 0.f) {
                line[x] = wfBg;
                continue;
            }
            float pct = std::pow(qBound(0.f, adjusted, 1.f), gamma);
            line[x] = wfColor(pct);
        }
    } else {
        m_waterfallRgbValid = false;
    }
    // Se palette/black/gain hanno invalidato le righe storiche, resta false:
    // il fallback CPU farà un rebuild completo da m_waterfallIntensityImage.
    m_wfWriteRow++;
}

void PanadapterItem::rebuildRgbWaterfallFromIntensity()
{
    if (m_waterfallImage.isNull() || m_waterfallIntensityImage.isNull()
        || m_waterfallImage.height() != m_waterfallIntensityImage.height()) {
        m_waterfallRgbValid = false;
        return;
    }

    if (m_palette.size() < 256)
        buildPalette(m_paletteIndex);

    QRgb const fallback = (m_paletteIndex == 11) ? qRgb(255, 255, 255) : qRgb(0, 0, 0);
    float const blackThresh = qBound(0.0f, m_blackLevel * 0.006f, 0.95f);
    float const usableRange = qMax(0.001f, 1.0f - blackThresh);
    float gamma = 2.5f - m_colorGain * 0.02f;
    if (gamma < 0.3f)
        gamma = 0.3f;

    int const screenW = m_waterfallImage.width();
    int const intensityW = m_waterfallIntensityImage.width();
    float const baseStart = (m_bandwidth > 0) ? static_cast<float>(m_startFreq) : m_dataFreqMin;
    float const baseEnd = (m_bandwidth > 0) ? static_cast<float>(m_startFreq + m_bandwidth) : m_dataFreqMax;
    float viewportRange = baseEnd - baseStart;
    if (viewportRange <= 0.0f)
        viewportRange = 1.0f;
    float dataRange = m_dataFreqMax - m_dataFreqMin;
    if (dataRange <= 0.0f)
        dataRange = 1.0f;
    float const viewRange = viewportRange / m_zoomFactor;
    float const viewCenter = baseStart + viewportRange * 0.5f + m_panHz;
    float const viewStart = viewCenter - viewRange * 0.5f;

    for (int y = 0; y < m_waterfallImage.height(); ++y) {
        auto* dst = reinterpret_cast<QRgb*>(m_waterfallImage.scanLine(y));
        uchar const* src = m_waterfallIntensityImage.constScanLine(y);
        for (int x = 0; x < screenW; ++x) {
            int srcX = x;
            if (intensityW != screenW) {
                float const pixFreq = viewStart + static_cast<float>(x) * viewRange / static_cast<float>(screenW);
                if (pixFreq < m_dataFreqMin || pixFreq > m_dataFreqMax) {
                    dst[x] = fallback;
                    continue;
                }
                srcX = static_cast<int>((pixFreq - m_dataFreqMin) / dataRange * intensityW);
                srcX = qBound(0, srcX, intensityW - 1);
            }
            float adjusted = ((static_cast<float>(src[srcX]) / 255.0f) - blackThresh) / usableRange;
            if (adjusted <= 0.0f) {
                dst[x] = fallback;
                continue;
            }
            float const pct = std::pow(qBound(0.0f, adjusted, 1.0f), gamma);
            int const idx = qBound(0, static_cast<int>(pct * 255.0f + 0.5f), 255);
            dst[x] = m_palette.value(idx, fallback);
        }
    }
    m_waterfallRgbValid = true;
}

void PanadapterItem::itemChange(ItemChange change, const ItemChangeData& value)
{
    if (change == ItemSceneChange && value.window) {
        connect(value.window,
                &QQuickWindow::beforeRendering,
                this,
                &PanadapterItem::recordGpuFftCompute,
                Qt::ConnectionType(Qt::DirectConnection | Qt::UniqueConnection));
        emit spectrumGpuOverlayAvailableChanged();
    }
    QQuickItem::itemChange(change, value);
}

void PanadapterItem::releaseResources()
{
    releaseGpuFftResources();
    QQuickItem::releaseResources();
}

void PanadapterItem::releaseGpuFftResources()
{
#if defined(DECODIUM_QT_RHI_TEXTURE_UPLOAD) && defined(DECODIUM_GPU_PANADAPTER_FFT_QSB)
    delete m_gpuFft;
    m_gpuFft = nullptr;
#else
    m_gpuFft = nullptr;
#endif
}

void PanadapterItem::failGpuFft(const QString& reason)
{
    {
        QMutexLocker lock(&m_mutex);
        m_gpuFftFailed = true;
        m_gpuFftFailureReason = reason;
        m_hasPendingPcmFrame = false;
        m_lastGpuFftFrameMs = 0;
        m_gpuFftUiBinsExpected = 0;
        m_gpuFftInvalidReadbacks = 0;
    }
    qWarning().noquote()
        << "[PANDBG] Panadapter visual FFT GPU path unavailable"
        << "reason=" << reason
        << "fallback=FFTW_CPU";
    QMetaObject::invokeMethod(this, [this, reason]() {
        emit gpuFftUnavailable(reason);
    }, Qt::QueuedConnection);
}

void PanadapterItem::recordGpuFftCompute()
{
#if defined(DECODIUM_QT_RHI_TEXTURE_UPLOAD) && defined(DECODIUM_GPU_PANADAPTER_FFT_QSB)
    PcmFrame frame;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_hasPendingPcmFrame || m_gpuFftFailed)
            return;
        if (m_gpuFft && m_gpuFft->readbackPending)
            return;
        frame = m_pendingPcmFrame;
        m_hasPendingPcmFrame = false;
    }

    QQuickWindow* win = window();
    auto* rif = win ? win->rendererInterface() : nullptr;
    auto* rhi = rif ? static_cast<QRhi*>(rif->getResource(win, QSGRendererInterface::RhiResource)) : nullptr;
    QRhiCommandBuffer* cb = nullptr;
    if (rif) {
        cb = static_cast<QRhiCommandBuffer*>(
            rif->getResource(win, QSGRendererInterface::RhiRedirectCommandBuffer));
        if (!cb) {
            auto* swapchain = static_cast<QRhiSwapChain*>(
                rif->getResource(win, QSGRendererInterface::RhiSwapchainResource));
            cb = swapchain ? swapchain->currentFrameCommandBuffer() : nullptr;
        }
    }
    if (!rhi || !cb) {
        failGpuFft(QStringLiteral("RHI command buffer unavailable"));
        return;
    }

    const int N = 4096;
    if (frame.samples.size() < N || frame.nfb <= frame.nfa) {
        failGpuFft(QStringLiteral("invalid PCM frame for GPU compute"));
        return;
    }

    const float freqPerBin = 12000.0f / static_cast<float>(N);
    const int binStart = qBound(0, static_cast<int>(frame.nfa / freqPerBin), N / 2);
    const int binEnd = qBound(binStart, static_cast<int>(frame.nfb / freqPerBin), N / 2);
    const int sourceBins = binEnd - binStart;
    if (sourceBins <= 0) {
        failGpuFft(QStringLiteral("empty GPU FFT bin range"));
        return;
    }
    const int nBins = qMin(sourceBins, qEnvironmentVariableIntValue("DECODIUM_GPU_PANADAPTER_DFT_BINS") > 0
        ? qBound(64, qEnvironmentVariableIntValue("DECODIUM_GPU_PANADAPTER_DFT_BINS"), sourceBins)
        : qMin(512, sourceBins));

    if (!m_gpuFft)
        m_gpuFft = new GpuFftState;
    if (m_gpuFft->rhi && m_gpuFft->rhi != rhi)
        m_gpuFft->reset();
    m_gpuFft->rhi = rhi;

    if (!m_gpuFft->shader.isValid()) {
        QFile shaderFile(QStringLiteral(":/shaders/panadapter_fft.comp.qsb"));
        if (!shaderFile.open(QIODevice::ReadOnly)) {
            failGpuFft(QStringLiteral("missing :/shaders/panadapter_fft.comp.qsb"));
            return;
        }
        m_gpuFft->shader = QShader::fromSerialized(shaderFile.readAll());
        if (!m_gpuFft->shader.isValid()) {
            failGpuFft(QStringLiteral("invalid panadapter_fft compute shader"));
            return;
        }
    }

    const int sampleBytes = N * static_cast<int>(sizeof(float));
    const int outputBytes = nBins * static_cast<int>(sizeof(float));
    if (!m_gpuFft->sampleBuffer
        || !m_gpuFft->outputBuffer
        || !m_gpuFft->paramsBuffer
        || !m_gpuFft->srb
        || !m_gpuFft->pipeline
        || m_gpuFft->sampleCapacity < sampleBytes
        || m_gpuFft->outputCapacity < outputBytes) {
        delete m_gpuFft->pipeline;
        delete m_gpuFft->srb;
        delete m_gpuFft->paramsBuffer;
        delete m_gpuFft->outputBuffer;
        delete m_gpuFft->sampleBuffer;
        m_gpuFft->pipeline = nullptr;
        m_gpuFft->srb = nullptr;
        m_gpuFft->paramsBuffer = nullptr;
        m_gpuFft->outputBuffer = nullptr;
        m_gpuFft->sampleBuffer = nullptr;

        m_gpuFft->sampleBuffer = rhi->newBuffer(QRhiBuffer::Static,
                                                QRhiBuffer::StorageBuffer,
                                                sampleBytes);
        m_gpuFft->outputBuffer = rhi->newBuffer(QRhiBuffer::Static,
                                                QRhiBuffer::StorageBuffer,
                                                outputBytes);
        m_gpuFft->paramsBuffer = rhi->newBuffer(QRhiBuffer::Static,
                                                QRhiBuffer::StorageBuffer,
                                                sizeof(GpuFftParams));
        if (!m_gpuFft->sampleBuffer
            || !m_gpuFft->outputBuffer
            || !m_gpuFft->paramsBuffer
            || !m_gpuFft->sampleBuffer->create()
            || !m_gpuFft->outputBuffer->create()
            || !m_gpuFft->paramsBuffer->create()) {
            failGpuFft(QStringLiteral("failed to create GPU FFT buffers"));
            return;
        }

        m_gpuFft->srb = rhi->newShaderResourceBindings();
        m_gpuFft->srb->setBindings({
            QRhiShaderResourceBinding::bufferLoad(0,
                                                  QRhiShaderResourceBinding::ComputeStage,
                                                  m_gpuFft->paramsBuffer),
            QRhiShaderResourceBinding::bufferLoad(1,
                                                  QRhiShaderResourceBinding::ComputeStage,
                                                  m_gpuFft->sampleBuffer),
            QRhiShaderResourceBinding::bufferStore(2,
                                                   QRhiShaderResourceBinding::ComputeStage,
                                                   m_gpuFft->outputBuffer)
        });
        if (!m_gpuFft->srb->create()) {
            failGpuFft(QStringLiteral("failed to create GPU FFT shader bindings"));
            return;
        }

        m_gpuFft->pipeline = rhi->newComputePipeline();
        m_gpuFft->pipeline->setShaderStage(QRhiShaderStage(QRhiShaderStage::Compute,
                                                           m_gpuFft->shader));
        m_gpuFft->pipeline->setShaderResourceBindings(m_gpuFft->srb);
        if (!m_gpuFft->pipeline->create()) {
            failGpuFft(QStringLiteral("failed to create GPU FFT compute pipeline"));
            return;
        }

        m_gpuFft->sampleCapacity = sampleBytes;
        m_gpuFft->outputCapacity = outputBytes;
    }

    GpuFftParams params;
    params.n = N;
    params.binStart = binStart;
    params.nBins = nBins;
    params.mode = 0;
    params.stage = 0;
    params.srcA = 1;
    const float fftNorm = static_cast<float>(N) / 2.0f;
    params.inverseNormSquared = 1.0f / (fftNorm * fftNorm);
    params.powerFloor = 1e-24f;
    params.reserved2 = sourceBins > 1 && nBins > 1
        ? static_cast<float>(sourceBins - 1) / static_cast<float>(nBins - 1)
        : 1.0f;

    static const QVector<float> blackmanHarrisWindow = [] {
        QVector<float> window(4096);
        constexpr float a0 = 0.35875f;
        constexpr float a1 = 0.48829f;
        constexpr float a2 = 0.14128f;
        constexpr float a3 = 0.01168f;
        constexpr float twoPi = 6.2831853071795864769f;
        for (int i = 0; i < 4096; ++i) {
            float const phase = twoPi * static_cast<float>(i) / static_cast<float>(4096 - 1);
            window[i] = a0
                      - a1 * std::cos(phase)
                      + a2 * std::cos(2.0f * phase)
                      - a3 * std::cos(3.0f * phase);
        }
        return window;
    }();
    QVector<float> windowedSamples(N);
    for (int i = 0; i < N; ++i)
        windowedSamples[i] = frame.samples.at(i) * blackmanHarrisWindow.at(i);

    QRhiResourceUpdateBatch* uploads = rhi->nextResourceUpdateBatch();
    uploads->uploadStaticBuffer(m_gpuFft->sampleBuffer,
                                0,
                                sampleBytes,
                                windowedSamples.constData());
    uploads->uploadStaticBuffer(m_gpuFft->paramsBuffer,
                                0,
                                sizeof(GpuFftParams),
                                &params);

    auto* readback = new DecodiumRhiBufferReadbackResult;
    QPointer<PanadapterItem> guard(this);
    readback->completed = [guard,
                           readback,
                           nBins,
                           sourceBins,
                           inputPeak = frame.samplePeak,
                           inputRms = frame.sampleRms,
                           freqMinHz = frame.freqMinHz,
                           freqMaxHz = frame.freqMaxHz]() mutable {
        QByteArray const data = readback->data;
        delete readback;
        if (!guard)
            return;
        if (guard->m_gpuFft)
            guard->m_gpuFft->readbackPending = false;
        if (data.size() < nBins * static_cast<int>(sizeof(float))) {
            QMetaObject::invokeMethod(guard, [guard]() {
                if (guard)
                    guard->failGpuFft(QStringLiteral("short GPU FFT readback"));
            }, Qt::QueuedConnection);
            return;
        }

        QVector<float> values(nBins);
        std::memcpy(values.data(), data.constData(), static_cast<size_t>(nBins) * sizeof(float));
        float minDb = 200.0f;
        float maxDb = -200.0f;
        int finiteCount = 0;
        int activeCount = 0;
        for (float& db : values) {
            if (!std::isfinite(db)) {
                db = -200.0f;
                continue;
            }
            db = qBound(-200.0f, db, 140.0f);
            ++finiteCount;
            if (std::isfinite(db) && db > -190.0f) {
                ++activeCount;
                minDb = qMin(minDb, db);
                maxDb = qMax(maxDb, db);
            }
        }
        auto retryInvalidReadback = [&](const QString& reason) -> bool {
            int const invalidCount = ++guard->m_gpuFftInvalidReadbacks;
            if (invalidCount < 3) {
                qWarning().noquote()
                    << "[PANDBG] Panadapter visual FFT GPU readback ignored"
                    << "reason=" << reason
                    << "retry=" << invalidCount
                    << "of=2";
                return true;
            }
            QMetaObject::invokeMethod(guard, [guard, reason]() {
                if (guard)
                    guard->failGpuFft(reason);
            }, Qt::QueuedConnection);
            return true;
        };
        if (activeCount == 0 && inputPeak > 1.0f) {
            retryInvalidReadback(QStringLiteral("GPU FFT returned floor-only frame for non-silent input peak=%1 rms=%2")
                                     .arg(inputPeak, 0, 'f', 1)
                                     .arg(inputRms, 0, 'f', 1));
            return;
        }
        if (activeCount > 0 && inputPeak > 1.0f && std::abs(maxDb - minDb) < 0.01f) {
            retryInvalidReadback(QStringLiteral("GPU FFT returned flat frame min=%1 max=%2 for non-silent input peak=%3 rms=%4")
                                     .arg(minDb, 0, 'f', 3)
                                     .arg(maxDb, 0, 'f', 3)
                                     .arg(inputPeak, 0, 'f', 1)
                                     .arg(inputRms, 0, 'f', 1));
            return;
        }
        guard->m_gpuFftInvalidReadbacks = 0;
        if (minDb > maxDb) {
            minDb = -130.0f;
            maxDb = -40.0f;
        }
        if (guard->m_gpuFft && !guard->m_gpuFft->loggedReadbackStats) {
            qInfo().noquote()
                << "[GPUDBG] Panadapter visual FFT readback stats"
                << "bins=" << nBins
                << "ui_bins=" << sourceBins
                << "finite=" << finiteCount
                << "active=" << activeCount
                << "input_peak=" << inputPeak
                << "input_rms=" << inputRms
                << "minDb=" << minDb
                << "maxDb=" << maxDb;
            guard->m_gpuFft->loggedReadbackStats = true;
        }
        {
            QMutexLocker lock(&guard->m_mutex);
            guard->m_lastGpuFftFrameMs = monotonicMs();
        }

        QVector<float> spectrumValues;
        if (sourceBins == nBins) {
            spectrumValues = std::move(values);
        } else {
            spectrumValues.resize(sourceBins);
            if (nBins <= 1 || sourceBins <= 1) {
                std::fill(spectrumValues.begin(), spectrumValues.end(), values.value(0, -200.0f));
            } else {
                float const scale = static_cast<float>(nBins - 1) / static_cast<float>(sourceBins - 1);
                for (int i = 0; i < sourceBins; ++i) {
                    float const src = static_cast<float>(i) * scale;
                    int const left = qBound(0, static_cast<int>(std::floor(src)), nBins - 1);
                    int const right = qMin(left + 1, nBins - 1);
                    float const t = src - static_cast<float>(left);
                    spectrumValues[i] = values[left] * (1.0f - t) + values[right] * t;
                }
            }
        }

        QMetaObject::invokeMethod(guard,
                                  [guard,
                                   values = std::move(spectrumValues),
                                   minDb,
                                   maxDb,
                                   freqMinHz,
                                   freqMaxHz]() mutable {
            if (!guard)
                return;
            guard->addSpectrumData(values, minDb, maxDb, freqMinHz, freqMaxHz);
        }, Qt::QueuedConnection);
    };

    QRhiResourceUpdateBatch* readbackBatch = rhi->nextResourceUpdateBatch();
    readbackBatch->readBackBuffer(m_gpuFft->outputBuffer,
                                  0,
                                  outputBytes,
                                  readback);

    cb->beginComputePass(uploads);
    cb->setComputePipeline(m_gpuFft->pipeline);
    cb->setShaderResources(m_gpuFft->srb);
    cb->dispatch((nBins + 63) / 64, 1, 1);
    cb->endComputePass(readbackBatch);
    m_gpuFft->readbackPending = true;

    if (!m_gpuFft->loggedActive) {
        qInfo().noquote()
            << "[GPUDBG] Panadapter visual FFT GPU compute path active"
            << "api=" << waterfallGraphicsApiName(rif->graphicsApi())
            << "shader=panadapter_fft.comp.qsb"
            << "algorithm=single_pass_dft_recurrence_4096"
            << "passes=1"
            << "source_bins=" << sourceBins
            << "gpu_bins=" << nBins
            << "ui_bins=" << sourceBins
            << "bin_step=" << params.reserved2
            << "window=CPU_precomputed_blackman_harris"
            << "readback=async"
            << "bins=" << nBins
            << "fallback=FFTW_CPU";
        m_gpuFft->loggedActive = true;
    }
#endif
}

void PanadapterItem::logWaterfallRenderPath(bool gpu, const QString& reason)
{
    QSGRendererInterface::GraphicsApi api = QSGRendererInterface::Unknown;
    if (window() && window()->rendererInterface())
        api = window()->rendererInterface()->graphicsApi();

    int const path = gpu ? 1 : 0;
    int const apiKey = static_cast<int>(api);
    if (m_loggedWaterfallPath == path && m_loggedWaterfallApi == apiKey
        && m_loggedWaterfallReason == reason)
        return;

    m_loggedWaterfallPath = path;
    m_loggedWaterfallApi = apiKey;
    m_loggedWaterfallReason = reason;
    bool const texturedGpu = !gpu
        && api != QSGRendererInterface::Software
        && api != QSGRendererInterface::Null
        && api != QSGRendererInterface::Unknown;
    qInfo().noquote()
        << "[GPUDBG] Panadapter waterfall"
        << (gpu ? "GPU shader path active"
                : (texturedGpu ? "GPU texture path active" : "CPU fallback path active"))
        << "api=" << waterfallGraphicsApiName(api)
        << "qsb=" << (
#ifdef DECODIUM_WATERFALL_SHADER_QSB
            "yes"
#else
            "no"
#endif
        )
        << "reason=" << reason;
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

    bool const shaderSupported = shaderWaterfallSupported();
    bool const gpuSpectrumGraph = spectrumGraphSupported();
    m_useShaderWaterfall = shaderSupported && m_wfWriteRow > 0;
    if (m_spectrumDirty && !m_bins.isEmpty()) {
        // Drain UNA riga alla volta per paint = scroll fluido, niente "jump"
        // di N righe quando il backlog cresce. Se restano altre righe in coda
        // (m_pendingWaterfallRows), richiediamo un altro update() così il
        // QSG schedula un nuovo paint nel prossimo frame e scrolliamo
        // continuamente. Comportamento simile a Decodium 3.0.
        if (m_pendingWaterfallRows.isEmpty()) {
            addWaterfallRow(m_bins, m_minDb, m_maxDb, m_dataFreqMin, m_dataFreqMax);
        } else {
            WaterfallFrame const row = m_pendingWaterfallRows.takeFirst();
            addWaterfallRow(row.bins,
                            row.minDb,
                            row.maxDb,
                            row.dataFreqMin,
                            row.dataFreqMax);
            if (!m_pendingWaterfallRows.isEmpty()) {
                // Backlog non vuoto → richiedi un altro paint per drenarlo.
                // QMetaObject::invokeMethod con queued garantisce che update()
                // venga chiamato fuori dal threading del scenegraph.
                QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
            }
        }
        if (!gpuSpectrumGraph)
            renderSpectrum();
        // Lascia m_spectrumDirty=true se ci sono ancora righe in coda così il
        // prossimo paint le draina anche senza altre addSpectrumData.
        if (m_pendingWaterfallRows.isEmpty())
            m_spectrumDirty = false;
    }
    m_useShaderWaterfall = shaderSupported && m_wfWriteRow > 0;

    // ── Crea/aggiorna due zone: spectrum root + waterfall node ─────────────
    // Node structure: root → spectrumRoot(background/legacy texture + GPU graph), waterfall node
    QSGNode* root = oldNode ? oldNode : new QSGNode();

    QSGNode* spectrumRoot = root->firstChild();
    if (auto* legacySpectrumNode = dynamic_cast<QSGSimpleTextureNode*>(spectrumRoot)) {
        root->removeChildNode(legacySpectrumNode);
        delete legacySpectrumNode;
        spectrumRoot = nullptr;
    }
    if (!spectrumRoot) {
        spectrumRoot = new QSGNode();
        root->prependChildNode(spectrumRoot);
    }

    // Spectrum node (top)
    if (!m_spectrumImage.isNull() || gpuSpectrumGraph) {
        int const specH = m_spectrumImage.isNull() ? qMin(m_spectrumH, h) : m_spectrumImage.height();
        QRectF specRect(0, 0, w, specH);
        if (gpuSpectrumGraph) {
            QColor const bg = (m_paletteIndex == 11) ? QColor(255, 255, 255) : QColor(0, 0, 0);
            if (auto* bgNode = ensureFlatColorNode(spectrumRoot,
                                                   0,
                                                   6,
                                                   QSGGeometry::DrawTriangles,
                                                   bg)) {
                writeRectGeometry(bgNode->geometry()->vertexDataAsPoint2D(), specRect);
            }
            updateSpectrumGraphNodes(spectrumRoot, w, specH);
        } else {
            auto* sn = dynamic_cast<QSGSimpleTextureNode*>(sceneGraphChildAt(spectrumRoot, 0));
            if (!sn) {
                removeSceneGraphChildrenFrom(spectrumRoot, spectrumRoot->firstChild());
                sn = new QSGSimpleTextureNode();
                sn->setOwnsTexture(true);
                spectrumRoot->appendChildNode(sn);
            }
#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
            auto* tex = dynamic_cast<DecodiumRhiImageTexture*>(sn->texture());
            if (!tex || tex->textureSize() != m_spectrumImage.size()
                || !tex->hasAlphaChannel() || tex->failed()) {
                tex = new DecodiumRhiImageTexture(true);
                tex->setFiltering(QSGTexture::Linear);
                sn->setTexture(tex);
            }
            tex->uploadFullImage(m_spectrumImage, true);
            sn->setFiltering(QSGTexture::Linear);
#else
            auto* tex = window()->createTextureFromImage(m_spectrumImage,
                QQuickWindow::CreateTextureOptions(QQuickWindow::TextureHasAlphaChannel));
            tex->setFiltering(QSGTexture::Linear);
            sn->setTexture(tex);
#endif
            sn->setRect(specRect);
            sn->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
            removeSpectrumGraphNodes(spectrumRoot);
        }
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
        if (root->childCount() > 1 && root->firstChild())
            waterfallChild = root->firstChild()->nextSibling();

#ifdef DECODIUM_WATERFALL_SHADER_QSB
        if (m_useShaderWaterfall
            && ((m_waterfallRawBinsWidth > 0 && !m_waterfallDbRows.isEmpty() && !m_waterfallDbRowParams.isEmpty())
                || !m_waterfallIntensityImage.isNull())) {
#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
            if (!m_shaderWaterfallBlocked) {
                if (auto* oldSimple = dynamic_cast<QSGSimpleTextureNode*>(waterfallChild)) {
                    root->removeChildNode(oldSimple);
                    delete oldSimple;
                    waterfallChild = nullptr;
                }

                auto* wn = dynamic_cast<QSGGeometryNode*>(waterfallChild);
                if (!wn) {
                    wn = new QSGGeometryNode();
                    auto* geometry = new QSGGeometry(waterfallTexturedPoint2DAttributes(), 4);
                    geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
                    geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
                    wn->setGeometry(geometry);
                    wn->setFlag(QSGNode::OwnsGeometry);
                    wn->setMaterial(new WaterfallPaletteMaterial());
                    wn->setFlag(QSGNode::OwnsMaterial);
                    root->appendChildNode(wn);
                    static std::atomic_bool loggedExplicitAttributes {false};
                    if (!loggedExplicitAttributes.exchange(true, std::memory_order_relaxed)) {
                        qInfo().noquote()
                            << "[GPUDBG] Panadapter waterfall shader geometry attributes explicit position=0 texcoord=1";
                    }
                }

                QSGGeometry::updateTexturedRectGeometry(wn->geometry(),
                                                        QRectF(0, specH, w, wfH),
                                                        QRectF(0, 0, 1, 1));

                auto* material = static_cast<WaterfallPaletteMaterial*>(wn->material());
                material->params[0] = 255.0f / 256.0f;
                material->params[1] = 0.5f / 256.0f;
                material->params[2] = static_cast<float>(m_wfWriteRow % rows);
                material->params[3] = static_cast<float>(rows);
                float const blackThresh = qBound(0.0f, m_blackLevel * 0.006f, 0.95f);
                float gamma = 2.5f - m_colorGain * 0.02f;
                if (gamma < 0.3f)
                    gamma = 0.3f;
                material->levelParams[0] = blackThresh;
                material->levelParams[1] = 1.0f / qMax(0.001f, 1.0f - blackThresh);
                material->levelParams[2] = gamma;
                material->levelParams[3] = 2.0f;
                float const baseStart = (m_bandwidth > 0) ? static_cast<float>(m_startFreq) : m_dataFreqMin;
                float const baseEnd = (m_bandwidth > 0) ? static_cast<float>(m_startFreq + m_bandwidth) : m_dataFreqMax;
                float viewportRange = baseEnd - baseStart;
                if (viewportRange <= 0.0f)
                    viewportRange = 1.0f;
                float dataRange = m_dataFreqMax - m_dataFreqMin;
                if (dataRange <= 0.0f)
                    dataRange = 1.0f;
                float const viewRange = viewportRange / m_zoomFactor;
                float const viewCenter = baseStart + viewportRange * 0.5f + m_panHz;
                float const viewStart = viewCenter - viewRange * 0.5f;
                material->xParams[0] = viewRange / dataRange;
                material->xParams[1] = (viewStart - m_dataFreqMin) / dataRange;
                material->xParams[2] = 0.0f;
                material->xParams[3] = 1.0f;

                if (material->paletteGeneration != m_paletteGeneration || !material->paletteTexture) {
                    QImage paletteImage(256, 1, QImage::Format_RGBA8888);
                    uchar* dst = paletteImage.scanLine(0);
                    for (int x = 0; x < 256; ++x) {
                        QColor const c = QColor::fromRgb(m_palette.value(x, qRgb(0, 0, 0)));
                        int const offset = x * 4;
                        dst[offset + 0] = static_cast<uchar>(c.red());
                        dst[offset + 1] = static_cast<uchar>(c.green());
                        dst[offset + 2] = static_cast<uchar>(c.blue());
                        dst[offset + 3] = 255;
                    }
                    material->retireTexture(material->paletteTexture);
                    auto* paletteTexture = new DecodiumRhiImageTexture(false);
                    paletteTexture->setFiltering(QSGTexture::Linear);
                    paletteTexture->uploadFullRgbaImage(paletteImage, false);
                    material->paletteTexture = paletteTexture;
                    material->paletteGeneration = m_paletteGeneration;
                }

                if (auto* paletteTexture = dynamic_cast<DecodiumRhiImageTexture*>(material->paletteTexture);
                    paletteTexture && paletteTexture->failed()) {
                    m_shaderWaterfallBlocked = true;
                    qWarning().noquote() << "[GPUDBG] Panadapter waterfall GPU palette texture failed; falling back to CPU";
                }

                RawDbUploadStats uploadStats;
                bool uploadedFullTexture = false;
                bool uploadedTextureData = false;
                auto* intensityTexture = dynamic_cast<DecodiumRhiFloatTexture*>(material->intensityTexture);
                auto* rowParamsTexture = dynamic_cast<DecodiumRhiFloatTexture*>(material->rowParamsTexture);
                QSize const intensitySize(m_waterfallRawBinsWidth, rows);
                QSize const rowParamsSize(2, rows);
                int const intensityW = intensitySize.width();
                bool const needsFullUpload = !intensityTexture
                    || intensityTexture->failed()
                    || intensityTexture->textureSize() != intensitySize
                    || !rowParamsTexture
                    || rowParamsTexture->failed()
                    || rowParamsTexture->textureSize() != rowParamsSize
                    || m_waterfallGpuUploadedSize != intensitySize
                    || m_waterfallGpuUploadedWriteRow > m_wfWriteRow;

                if (needsFullUpload) {
                    material->retireTexture(material->intensityTexture);
                    material->retireTexture(material->rowParamsTexture);
                    intensityTexture = new DecodiumRhiFloatTexture();
                    rowParamsTexture = new DecodiumRhiFloatTexture();
                    intensityTexture->setFiltering(QSGTexture::Nearest);
                    rowParamsTexture->setFiltering(QSGTexture::Nearest);
                    intensityTexture->uploadFullFloats(intensitySize, m_waterfallDbRows.constData());
                    rowParamsTexture->uploadFullFloats(rowParamsSize, m_waterfallDbRowParams.constData());
                    material->intensityTexture = intensityTexture;
                    material->rowParamsTexture = rowParamsTexture;
                    m_waterfallGpuUploadedSize = intensitySize;
                    m_waterfallGpuUploadedWriteRow = m_wfWriteRow;
                    uploadStats = rawDbImageStats(m_waterfallDbRows, intensityW, rows);
                    uploadedFullTexture = true;
                    uploadedTextureData = true;
                } else {
                    int rowsToUpload = m_wfWriteRow - m_waterfallGpuUploadedWriteRow;
                    if (rowsToUpload >= rows) {
                        intensityTexture->uploadFullFloats(intensitySize, m_waterfallDbRows.constData());
                        rowParamsTexture->uploadFullFloats(rowParamsSize, m_waterfallDbRowParams.constData());
                        m_waterfallGpuUploadedWriteRow = m_wfWriteRow;
                        uploadStats = rawDbImageStats(m_waterfallDbRows, intensityW, rows);
                        uploadedFullTexture = true;
                        uploadedTextureData = true;
                    } else if (rowsToUpload > 0) {
                        for (int i = 0; i < rowsToUpload; ++i) {
                            int const row = (m_waterfallGpuUploadedWriteRow + i) % rows;
                            float const* rawSrc = m_waterfallDbRows.constData() + row * intensityW;
                            accumulateRawDbStats(rawSrc, intensityW, uploadStats);
                            intensityTexture->uploadFloatRow(row, intensityW, rawSrc);
                            rowParamsTexture->uploadFloatRow(row, 2, m_waterfallDbRowParams.constData() + row * 2);
                        }
                        m_waterfallGpuUploadedWriteRow += rowsToUpload;
                        uploadedTextureData = true;
                    }
                }

                if (auto* texture = dynamic_cast<DecodiumRhiFloatTexture*>(material->intensityTexture);
                    texture && texture->failed()) {
                    m_shaderWaterfallBlocked = true;
                    qWarning().noquote() << "[GPUDBG] Panadapter waterfall GPU raw dB texture failed; falling back to CPU";
                }
                if (auto* texture = dynamic_cast<DecodiumRhiFloatTexture*>(material->rowParamsTexture);
                    texture && texture->failed()) {
                    m_shaderWaterfallBlocked = true;
                    qWarning().noquote() << "[GPUDBG] Panadapter waterfall GPU row params texture failed; falling back to CPU";
                }

                if (uploadedFullTexture && m_wfWriteRow >= rows && uploadStats.finiteSamples == 0) {
                    m_shaderWaterfallBlocked = true;
                    qWarning().noquote()
                        << "[GPUDBG] Panadapter waterfall GPU upload empty after warmup; falling back to CPU"
                        << "intensity=" << QStringLiteral("%1x%2").arg(intensitySize.width()).arg(intensitySize.height())
                        << "rows_written=" << m_wfWriteRow
                        << "finite_samples=" << uploadStats.finiteSamples;
                }

                if (!m_shaderWaterfallBlocked && material->intensityTexture && material->paletteTexture && material->rowParamsTexture) {
                    wn->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
                    bool const shouldLogStats = uploadedTextureData
                        && (!m_loggedWaterfallGpuUploadStats
                        || uploadedFullTexture
                        || (m_lastWaterfallGpuStatsRow >= 0
                            && m_wfWriteRow - m_lastWaterfallGpuStatsRow >= rows));
                    if (shouldLogStats) {
                        m_loggedWaterfallGpuUploadStats = true;
                        m_lastWaterfallGpuStatsRow = m_wfWriteRow;
                        qInfo().noquote()
                            << "[GPUDBG] Panadapter waterfall GPU persistent upload"
                            << "texture=RHI_R32F"
                            << "update=" << (uploadedFullTexture ? "full" : "partial_rows")
                            << "level=raw_db_gpu_norm_freqmap_black_gain_gamma"
                            << "intensity=" << QStringLiteral("%1x%2").arg(intensitySize.width()).arg(intensitySize.height())
                            << "row_params=2x" << rows
                            << "palette=256x1"
                            << "rows_written=" << m_wfWriteRow
                            << "min_db=" << (uploadStats.finiteSamples > 0 ? uploadStats.minDb : 0.0f)
                            << "max_db=" << (uploadStats.finiteSamples > 0 ? uploadStats.maxDb : 0.0f)
                            << "finite_samples=" << uploadStats.finiteSamples;
                    }
                    logWaterfallRenderPath(true, "shader persistent raw dB texture + row params + palette texture; dB normalization + frequency mapping + black/gain/gamma in shader");
                    return root;
                }
            }
	#else
	            int const intensityW = m_waterfallIntensityImage.width();
	            if (m_waterfallIntensityTextureImage.isNull() ||
	                m_waterfallIntensityTextureImage.width() != intensityW ||
	                m_waterfallIntensityTextureImage.height() != wfH) {
	                m_waterfallIntensityTextureImage = QImage(intensityW, wfH, QImage::Format_ARGB32_Premultiplied);
	                m_waterfallIntensityTextureImage.fill(QColor(0, 0, 0, 255));
	                m_loggedWaterfallGpuUploadStats = false;
	                m_lastWaterfallGpuStatsRow = -1;
            }

            int maxUploadedLevel = 0;
            qsizetype nonZeroUploaded = 0;
            for (int y = 0; y < wfH; ++y) {
	                int const srcRow = ((m_wfWriteRow - 1 - y) % rows + rows) % rows;
	                uchar const* src = m_waterfallIntensityImage.constScanLine(srcRow);
	                auto* dst = reinterpret_cast<QRgb*>(m_waterfallIntensityTextureImage.scanLine(y));
	                for (int x = 0; x < intensityW; ++x) {
	                    uchar const level = src[x];
	                    dst[x] = qRgba(level, level, level, 255);
                    if (level) {
                        ++nonZeroUploaded;
                        maxUploadedLevel = qMax(maxUploadedLevel, static_cast<int>(level));
                    }
                }
            }

            if (m_wfWriteRow >= rows && nonZeroUploaded == 0) {
                m_shaderWaterfallBlocked = true;
                qWarning().noquote()
	                    << "[GPUDBG] Panadapter waterfall GPU upload empty after warmup; falling back to CPU"
	                    << "intensity=" << QStringLiteral("%1x%2").arg(intensityW).arg(wfH)
                    << "rows_written=" << m_wfWriteRow
                    << "max_level=" << maxUploadedLevel
                    << "nonzero_pixels=" << nonZeroUploaded;
            }

            if (!m_shaderWaterfallBlocked) {
                if (auto* oldSimple = dynamic_cast<QSGSimpleTextureNode*>(waterfallChild)) {
                    root->removeChildNode(oldSimple);
                    delete oldSimple;
                    waterfallChild = nullptr;
                }

                auto* wn = dynamic_cast<QSGGeometryNode*>(waterfallChild);
                if (!wn) {
                    wn = new QSGGeometryNode();
                    auto* geometry = new QSGGeometry(waterfallTexturedPoint2DAttributes(), 4);
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
                material->params[0] = 255.0f / 256.0f;
                material->params[1] = 0.5f / 256.0f;
                material->params[2] = -1.0f;
                material->params[3] = 0.0f;
                float const blackThresh = qBound(0.0f, m_blackLevel * 0.006f, 0.95f);
                float gamma = 2.5f - m_colorGain * 0.02f;
                if (gamma < 0.3f)
                    gamma = 0.3f;
                material->levelParams[0] = blackThresh;
                material->levelParams[1] = 1.0f / qMax(0.001f, 1.0f - blackThresh);
                material->levelParams[2] = gamma;
                material->levelParams[3] = 1.0f;
                float const baseStart = (m_bandwidth > 0) ? static_cast<float>(m_startFreq) : m_dataFreqMin;
                float const baseEnd = (m_bandwidth > 0) ? static_cast<float>(m_startFreq + m_bandwidth) : m_dataFreqMax;
                float viewportRange = baseEnd - baseStart;
                if (viewportRange <= 0.0f)
                    viewportRange = 1.0f;
                float dataRange = m_dataFreqMax - m_dataFreqMin;
                if (dataRange <= 0.0f)
                    dataRange = 1.0f;
                float const viewRange = viewportRange / m_zoomFactor;
                float const viewCenter = baseStart + viewportRange * 0.5f + m_panHz;
                float const viewStart = viewCenter - viewRange * 0.5f;
                material->xParams[0] = viewRange / dataRange;
                material->xParams[1] = (viewStart - m_dataFreqMin) / dataRange;
                material->xParams[2] = 0.0f;
                material->xParams[3] = 1.0f;

                if (material->paletteGeneration != m_paletteGeneration || !material->paletteTexture) {
                    QImage paletteImage(256, 1, QImage::Format_ARGB32_Premultiplied);
                    auto* dst = reinterpret_cast<QRgb*>(paletteImage.scanLine(0));
                    for (int x = 0; x < 256; ++x) {
                        QColor const c = QColor::fromRgb(m_palette.value(x, qRgb(0, 0, 0)));
                        dst[x] = qRgba(c.red(), c.green(), c.blue(), 255);
                    }
                    auto* newPaletteTexture = window()->createTextureFromImage(
                        paletteImage,
                        QQuickWindow::CreateTextureOptions(QQuickWindow::TextureIsOpaque));
                    if (!newPaletteTexture) {
                        m_shaderWaterfallBlocked = true;
                        qWarning().noquote() << "[GPUDBG] Panadapter waterfall GPU palette texture creation failed; falling back to CPU";
                    } else {
                        newPaletteTexture->setFiltering(QSGTexture::Linear);
                        material->retireTexture(material->paletteTexture);
                        material->paletteTexture = newPaletteTexture;
                        material->paletteGeneration = m_paletteGeneration;
                    }
                }

                auto* newIntensityTexture = window()->createTextureFromImage(
                    m_waterfallIntensityTextureImage,
                    QQuickWindow::CreateTextureOptions(QQuickWindow::TextureIsOpaque));
                if (!newIntensityTexture) {
                    m_shaderWaterfallBlocked = true;
                    qWarning().noquote() << "[GPUDBG] Panadapter waterfall GPU intensity texture creation failed; falling back to CPU";
                } else {
                    newIntensityTexture->setFiltering(QSGTexture::Nearest);
                    material->retireTexture(material->intensityTexture);
                    material->intensityTexture = newIntensityTexture;
                }

                if (!m_shaderWaterfallBlocked && material->intensityTexture && material->paletteTexture) {
                    wn->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
                    logWaterfallRenderPath(true, "shader raw-bin intensity + palette textures; frequency mapping + black/gain/gamma in shader");
                    return root;
                }
            }
#endif
            m_useShaderWaterfall = false;
        }
#endif

        QString waterfallFallbackReason;
#ifdef DECODIUM_WATERFALL_SHADER_QSB
        if (!m_shaderWaterfallDisabledReason.isEmpty()) {
            waterfallFallbackReason = m_shaderWaterfallDisabledReason;
        } else if (m_shaderWaterfallBlocked) {
            waterfallFallbackReason = QStringLiteral("shader resource fallback");
        } else {
            waterfallFallbackReason = QStringLiteral("shader unavailable/disabled; colored texture upload");
        }
#else
        waterfallFallbackReason = QStringLiteral("qsb shaders not compiled");
#endif
        logWaterfallRenderPath(false, waterfallFallbackReason);

        if (auto* oldGeometry = dynamic_cast<QSGGeometryNode*>(waterfallChild)) {
            if (!dynamic_cast<QSGSimpleTextureNode*>(oldGeometry)) {
                root->removeChildNode(oldGeometry);
                delete oldGeometry;
                waterfallChild = nullptr;
            }
        }

        if (!m_waterfallRgbValid)
            rebuildRgbWaterfallFromIntensity();

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
#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
        auto* tex = dynamic_cast<DecodiumRhiImageTexture*>(wn->texture());
        if (!tex || tex->textureSize() != m_waterfallDisplayImage.size()
            || tex->hasAlphaChannel() || tex->failed()) {
            tex = new DecodiumRhiImageTexture(false);
            tex->setFiltering(QSGTexture::Nearest);
            wn->setTexture(tex);
        }
        tex->uploadFullImage(m_waterfallDisplayImage, false);
        wn->setFiltering(QSGTexture::Nearest);
#else
        auto* tex = window()->createTextureFromImage(m_waterfallDisplayImage, QQuickWindow::CreateTextureOptions(QQuickWindow::TextureIsOpaque));
        tex->setFiltering(QSGTexture::Nearest);
        wn->setTexture(tex);
#endif
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
    // Hit-test sulle label callsign (decode + cluster) prima del normale
    // freq-selection: click sinistro su una label = chiama la stazione.
    if (ev->button() == Qt::LeftButton) {
        QPoint pos((int)ev->position().x(), (int)ev->position().y());
        // Cluster prima (sono in basso, più visibili) se attivi
        if (m_showDxClusterSpots) {
            for (const auto& hit : m_clusterHitRects) {
                if (hit.rect.contains(pos)) {
                    emit dxClusterSpotClicked(hit.call, hit.freq);
                    ev->accept();
                    return;
                }
            }
        }
        // Poi i decode label (sono in alto)
        for (const auto& hit : m_decodeHitRects) {
            if (hit.rect.contains(pos)) {
                emit decodeLabelClicked(hit.call, hit.freq);
                ev->accept();
                return;
            }
        }
    }

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
