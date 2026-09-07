// PanadapterItem.cpp — High-resolution SDR panadapter for Decodium3
// Copyright IU8LMC 2025 — GPL v3
// Waterfall palette ported from deskHPSDR waterfall.c (GPL G0ORX/DL1BZ)

#include "PanadapterItem.hpp"
#include "DecodiumBridge.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QSGFlatColorMaterial>
#include <QSGVertexColorMaterial>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QQuickWindow>
#include <QFile>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMutexLocker>
#include <QPointer>
#include <QDebug>
#include <QRunnable>
#include <QVariant>
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
#include <functional>
#include <limits>

namespace
{
constexpr qint64 kPanMetricLogIntervalMs = 60000;

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

qint64 monotonicUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

QString metricMs(qint64 us)
{
    if (us < 0)
        return QStringLiteral("n/a");
    return QString::number(static_cast<double>(us) / 1000.0, 'f', 2);
}

struct PanadapterFreqView
{
    float dataStart = 0.0f;
    float dataEnd = 1.0f;
    float dataRange = 1.0f;
    float viewStart = 0.0f;
    float viewRange = 1.0f;
    bool clipsData = false;
};

PanadapterFreqView makePanadapterFreqView(float configuredStart,
                                          float configuredBandwidth,
                                          float dataFreqMin,
                                          float dataFreqMax,
                                          float zoomFactor,
                                          float panHz)
{
    PanadapterFreqView view;
    view.dataStart = dataFreqMin;
    view.dataEnd = dataFreqMax;
    view.dataRange = view.dataEnd - view.dataStart;
    if (!std::isfinite(view.dataRange) || view.dataRange <= 0.0f)
        view.dataRange = 1.0f;

    float const baseStart = configuredBandwidth > 0.0f ? configuredStart : view.dataStart;
    float const baseEnd = configuredBandwidth > 0.0f ? configuredStart + configuredBandwidth : view.dataEnd;
    float viewportRange = baseEnd - baseStart;
    if (!std::isfinite(viewportRange) || viewportRange <= 0.0f)
        viewportRange = view.dataRange;
    float const safeZoom = qMax(0.01f, zoomFactor);
    view.viewRange = viewportRange / safeZoom;
    if (!std::isfinite(view.viewRange) || view.viewRange <= 0.0f)
        view.viewRange = 1.0f;
    float const viewCenter = baseStart + viewportRange * 0.5f + panHz;
    view.viewStart = viewCenter - view.viewRange * 0.5f;

    float const tolHz = qMax(1.0f, view.viewRange * 0.0025f);
    view.clipsData = view.viewStart < view.dataStart - tolHz
        || view.viewStart + view.viewRange > view.dataEnd + tolHz;
    return view;
}

class ScopeExit
{
public:
    explicit ScopeExit(std::function<void()> fn)
        : m_fn(std::move(fn))
    {
    }

    ~ScopeExit()
    {
        if (m_fn)
            m_fn();
    }

    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;

private:
    std::function<void()> m_fn;
};

bool panadapterDebugLoggingEnabled()
{
    static bool const enabled = qEnvironmentVariableIsSet("DECODIUM_PANADAPTER_DEBUG");
    return enabled;
}

qint64 gpuFftReadbackTimeoutMs()
{
#if defined(Q_OS_MACOS)
    return 500;
#else
    return 1500;
#endif
}

qint64 gpuFftSlowReadbackMs()
{
    int const envValue = qEnvironmentVariableIntValue("DECODIUM_GPU_PANADAPTER_SLOW_READBACK_MS");
    if (envValue > 0)
        return envValue;
#if defined(Q_OS_MACOS)
    return 120;
#else
    return 300;
#endif
}

int gpuFftSlowReadbackLimit()
{
    int const envValue = qEnvironmentVariableIntValue("DECODIUM_GPU_PANADAPTER_SLOW_READBACK_LIMIT");
    return envValue > 0 ? qBound(1, envValue, 20) : 3;
}

int gpuFftTimeoutLimit()
{
    int const envValue = qEnvironmentVariableIntValue("DECODIUM_GPU_PANADAPTER_TIMEOUT_LIMIT");
    return envValue > 0 ? qBound(1, envValue, 20) : 2;
}

bool gpuDirectPanadapterEnabled()
{
    if (!qEnvironmentVariableIsSet("DECODIUM_GPU_PANADAPTER_DIRECT"))
        return true;
    return qEnvironmentVariableIntValue("DECODIUM_GPU_PANADAPTER_DIRECT") != 0;
}

bool gpuDirectDebugReadbackEnabled()
{
    return qEnvironmentVariableIntValue("DECODIUM_GPU_PANADAPTER_DEBUG_READBACK") != 0;
}

float gpuDirectAutoRangeFloorOffsetDb()
{
    return static_cast<float>(qEnvironmentVariableIntValue("DECODIUM_GPU_PANADAPTER_DIRECT_FLOOR_OFFSET_DB"));
}

float gpuDirectAutoRangeFloorMarginDb()
{
    int const env = qEnvironmentVariableIntValue("DECODIUM_GPU_PANADAPTER_DIRECT_FLOOR_MARGIN_DB");
    return env > 0 ? static_cast<float>(qBound(0, env, 40)) : 6.0f;
}

float gpuDirectNoiseCutBiasDb(int noiseFloorPercentile)
{
    // GPU Direct estimates the floor from time-domain RMS and therefore has no
    // sorted FFT-bin percentile to select.  Convert the UI percentile into the
    // equivalent white-noise FFT power quantile instead: 10% remains exactly
    // neutral, lower values reveal weaker signals and higher values cut more.
    float const probability = static_cast<float>(qBound(5, noiseFloorPercentile, 40)) / 100.0f;
    float const quantilePower = -std::log1p(-probability);
    float const referencePower = -std::log1p(-0.10f);
    return 10.0f * std::log10(quantilePower / referencePower);
}

float gpuDirectAutoRangeSpanDb(int contrastLevel)
{
    int const env = qEnvironmentVariableIntValue("DECODIUM_GPU_PANADAPTER_DIRECT_SPAN_DB");
    if (env > 0)
        return static_cast<float>(qBound(20, env, 100));

    // QML contrast 10..150: higher contrast means a narrower visible dB span.
    float const t = static_cast<float>(qBound(10, contrastLevel, 150) - 10) / 140.0f;
    return 70.0f - t * 50.0f;
}

float waterfallBlackThresholdForLevel(int blackLevel, bool gpuDirect)
{
    float const blackScale = gpuDirect ? 0.0022f : 0.006f;
    float const blackCap = gpuDirect ? 0.30f : 0.95f;
    return qBound(0.0f, static_cast<float>(blackLevel) * blackScale, blackCap);
}

float estimateGpuDirectNoiseFloorDb(float sampleRms, int n)
{
    if (n <= 0 || !std::isfinite(sampleRms) || sampleRms <= 0.0f)
        return -130.0f;

    static const float windowPowerSum = [] {
        constexpr int kN = 4096;
        constexpr float a0 = 0.35875f;
        constexpr float a1 = 0.48829f;
        constexpr float a2 = 0.14128f;
        constexpr float a3 = 0.01168f;
        constexpr float twoPi = 6.2831853071795864769f;
        float sum = 0.0f;
        for (int i = 0; i < kN; ++i) {
            float const phase = twoPi * static_cast<float>(i) / static_cast<float>(kN - 1);
            float const w = a0
                          - a1 * std::cos(phase)
                          + a2 * std::cos(2.0f * phase)
                          - a3 * std::cos(3.0f * phase);
            sum += w * w;
        }
        return sum;
    }();

    float const fftNorm = static_cast<float>(n) / 2.0f;
    float const normalizedPower =
        (sampleRms * sampleRms * windowPowerSum) / qMax(1.0f, fftNorm * fftNorm);
    return normalizedPower > 1e-24f
        ? 10.0f * std::log10(normalizedPower)
        : -200.0f;
}

int waterfallHistoryRowsForVisibleHeight(int visibleRows)
{
    if (visibleRows <= 0)
        return 0;
    // Retain every visible row, but avoid a permanent 256-row allocation for
    // compact layouts. Small allocation steps still prevent resize churn.
    static constexpr int kMinHistoryRows = 128;
    static constexpr int kHistoryRowsStep = 32;
    int const rows = qMax(kMinHistoryRows, visibleRows);
    return ((rows + kHistoryRowsStep - 1) / kHistoryRowsStep) * kHistoryRowsStep;
}

QFont panadapterMonoFont(int pointSize, QFont::Weight weight = QFont::Normal)
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(pointSize);
    font.setWeight(weight);
    return font;
}

qreal panadapterOverlayDevicePixelRatio(QQuickWindow* window)
{
    qreal dpr = window ? window->devicePixelRatio() : 1.0;
    if (!std::isfinite(dpr) || dpr < 1.0)
        dpr = 1.0;
    return qBound<qreal>(1.0, dpr, 3.0);
}

QSize panadapterOverlayTextureSize(int logicalWidth, int logicalHeight, qreal dpr)
{
    return QSize(qMax(1, qRound(static_cast<qreal>(logicalWidth) * dpr)),
                 qMax(1, qRound(static_cast<qreal>(logicalHeight) * dpr)));
}

void drawCrispOverlayText(QPainter& painter, int x, int y, const QString& text, const QColor& color)
{
    painter.setPen(QColor(0, 0, 0, 220));
    painter.drawText(x - 1, y, text);
    painter.drawText(x + 1, y, text);
    painter.drawText(x, y - 1, text);
    painter.drawText(x, y + 1, text);
    painter.setPen(color);
    painter.drawText(x, y, text);
}

void drawCrispOverlayText(QPainter& painter, const QRect& rect, int flags, const QString& text, const QColor& color)
{
    painter.setPen(QColor(0, 0, 0, 220));
    painter.drawText(rect.translated(-1, 0), flags, text);
    painter.drawText(rect.translated(1, 0), flags, text);
    painter.drawText(rect.translated(0, -1), flags, text);
    painter.drawText(rect.translated(0, 1), flags, text);
    painter.setPen(color);
    painter.drawText(rect, flags, text);
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

// Come ensureFlatColorNode, ma con il colore per VERTICE: serve allo spettro 3D,
// dove la cresta cambia tinta con l'ampiezza lungo la stessa traccia.
// Se al posto indicato c'e' un nodo di tipo diverso (per esempio quello piatto
// del 2D) viene rimosso insieme ai successivi: e' cosi' che il passaggio
// 2D<->3D si ripulisce da solo senza codice di transizione.
QSGGeometryNode* ensureVertexColorNode(QSGNode* parent,
                                       int index,
                                       int vertexCount,
                                       QSGGeometry::DrawingMode drawingMode)
{
    if (!parent || vertexCount <= 0)
        return nullptr;

    QSGNode* child = sceneGraphChildAt(parent, index);
    auto* node = dynamic_cast<QSGGeometryNode*>(child);
    auto* material = node ? dynamic_cast<QSGVertexColorMaterial*>(node->material()) : nullptr;
    if (child && (!node || !material)) {
        removeSceneGraphChildrenFrom(parent, child);
        child = nullptr;
        node = nullptr;
    }

    if (!node) {
        node = new QSGGeometryNode();
        auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), vertexCount);
        geometry->setDrawingMode(drawingMode);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new QSGVertexColorMaterial());
        node->setFlag(QSGNode::OwnsMaterial);
        parent->appendChildNode(node);
    } else {
        QSGGeometry* geometry = node->geometry();
        if (geometry && geometry->vertexCount() != vertexCount)
            geometry->allocate(vertexCount);
        if (geometry)
            geometry->setDrawingMode(drawingMode);
    }

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

class PanadapterSpectrumOverlayNode final : public QSGSimpleTextureNode
{
};

class PanadapterSpectrum3dNode final : public QSGNode
{
public:
    ~PanadapterSpectrum3dNode() override
    {
        delete sharedMaterial;
    }

    QSize meshSize;
    int meshTraces = 0;
    int meshPoints = 0;
    QSGMaterial* sharedMaterial = nullptr;
};

#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
QImage rgba8888TextureImage(const QImage& image)
{
    if (image.isNull())
        return {};
    if (image.format() == QImage::Format_RGBA8888
        || image.format() == QImage::Format_RGBA8888_Premultiplied)
        return image;
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

class DecodiumExternalRhiTexture final : public QSGTexture
{
public:
    DecodiumExternalRhiTexture(QRhiTexture* texture, QSize size, bool alpha = false)
        : m_texture(texture)
        , m_size(size)
        , m_hasAlpha(alpha)
    {
    }

    qint64 comparisonKey() const override
    {
        return static_cast<qint64>(reinterpret_cast<quintptr>(m_texture));
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

private:
    QRhiTexture* m_texture = nullptr; // not owned; lifetime belongs to GpuFftState
    QSize m_size;
    bool m_hasAlpha = false;
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

struct GpuFftDirectParams
{
    qint32 n = 4096;
    qint32 binStart = 0;
    qint32 nBins = 0;
    qint32 waterfallRows = 0;
    qint32 waterfallWriteRow = 0;
    qint32 peakHold = 0;
    qint32 resetPeak = 0;
    qint32 reserved0 = 0;
    float inverseNormSquared = 1.0f;
    float powerFloor = 1e-24f;
    float binStep = 1.0f;
    float displayMinDb = -70.0f;
    float displayInvRange = 1.0f / 100.0f;
    float peakDecay = 0.97f;
    float reserved1 = 0.0f;
    float reserved2 = 0.0f;
};

static_assert(sizeof(GpuFftDirectParams) == 64);
#endif
}

#if defined(DECODIUM_QT_RHI_TEXTURE_UPLOAD) && defined(DECODIUM_GPU_PANADAPTER_FFT_QSB)
struct PanadapterItem::GpuFftState
{
    QRhi* rhi = nullptr;
    QRhiBuffer* sampleBuffer = nullptr;
    QRhiBuffer* outputBuffer = nullptr;
    QRhiBuffer* paramsBuffer = nullptr;
    QRhiBuffer* directParamsBuffer = nullptr;
    QRhiShaderResourceBindings* srb = nullptr;
    QRhiShaderResourceBindings* directSrb = nullptr;
    QRhiComputePipeline* pipeline = nullptr;
    QRhiComputePipeline* directPipeline = nullptr;
    QRhiTexture* directSpectrumTexture = nullptr;
    QRhiTexture* directWaterfallTexture = nullptr;
    QRhiTexture* directRowParamsTexture = nullptr;
    QRhiTexture* directPeakTexture = nullptr;
    QShader shader;
    QShader directShader;
    int sampleCapacity = 0;
    int outputCapacity = 0;
    QSize directSpectrumSize;
    QSize directWaterfallSize;
    QSize directRowParamsSize;
    QSize directPeakSize;
    int directBins = 0;
    int directRows = 0;
    int directWriteRow = 0;
    bool directTexturePathActive = false;
    bool loggedDirectActive = false;
    bool loggedDirectDebugReadback = false;
    bool loggedDirectAutoRange = false;
    int loggedDirectNoiseCutPercent = -1;
    bool readbackPending = false;
    qint64 readbackPendingSinceMs = 0;
    quint64 readbackSerial = 0;
    bool loggedActive = false;
    bool loggedReadbackStats = false;
    QVector<QRhiTexture*> retiredDirectTextures;

    ~GpuFftState()
    {
        reset();
    }

    void retireDirectTexture(QRhiTexture*& texture)
    {
        if (!texture)
            return;
        retiredDirectTextures.append(texture);
        texture = nullptr;
        while (retiredDirectTextures.size() > 16)
            delete retiredDirectTextures.takeFirst();
    }

    void reset()
    {
        delete pipeline;
        delete directPipeline;
        delete srb;
        delete directSrb;
        delete paramsBuffer;
        delete directParamsBuffer;
        delete outputBuffer;
        delete sampleBuffer;
        delete directSpectrumTexture;
        delete directWaterfallTexture;
        delete directRowParamsTexture;
        delete directPeakTexture;
        for (QRhiTexture* texture : retiredDirectTextures)
            delete texture;
        retiredDirectTextures.clear();
        pipeline = nullptr;
        directPipeline = nullptr;
        srb = nullptr;
        directSrb = nullptr;
        paramsBuffer = nullptr;
        directParamsBuffer = nullptr;
        outputBuffer = nullptr;
        sampleBuffer = nullptr;
        directSpectrumTexture = nullptr;
        directWaterfallTexture = nullptr;
        directRowParamsTexture = nullptr;
        directPeakTexture = nullptr;
        rhi = nullptr;
        sampleCapacity = 0;
        outputCapacity = 0;
        directSpectrumSize = QSize();
        directWaterfallSize = QSize();
        directRowParamsSize = QSize();
        directPeakSize = QSize();
        directBins = 0;
        directRows = 0;
        directWriteRow = 0;
        directTexturePathActive = false;
        readbackPending = false;
        readbackPendingSinceMs = 0;
        ++readbackSerial;
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
#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
        delete fallbackTexture;
#endif
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

#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
    QSGTexture* fallbackSampledTexture()
    {
        if (!fallbackTexture) {
            auto* texture = new DecodiumRhiImageTexture(false);
            texture->setFiltering(QSGTexture::Nearest);
            QImage image(1, 1, QImage::Format_RGBA8888);
            image.fill(Qt::black);
            texture->uploadFullRgbaImage(image, false);
            fallbackTexture = texture;
        }
        return fallbackTexture;
    }

    QSGTexture* fallbackTexture = nullptr;
#endif
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
        QSGTexture* selected = nullptr;
        if (binding == 1)
            selected = material->intensityTexture;
        else if (binding == 2)
            selected = material->paletteTexture;
        else if (binding == 3)
            selected = material->rowParamsTexture;

#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
        auto commitReadyTexture = [&state](QSGTexture* candidate) -> QSGTexture* {
            if (!candidate)
                return nullptr;
            candidate->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
            return candidate->rhiTexture() ? candidate : nullptr;
        };

        QSGTexture* ready = commitReadyTexture(selected);
        if (!ready)
            ready = commitReadyTexture(material->fallbackSampledTexture());
        *texture = ready;
#else
        *texture = selected;
        if (*texture)
            (*texture)->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
#endif
        if (*texture) {
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

#if defined(DECODIUM_GPU_PANADAPTER_SPECTRUM_QSB)
class PanadapterSpectrumMaterial final : public QSGMaterial
{
public:
    PanadapterSpectrumMaterial()
    {
        setFlag(QSGMaterial::NoBatching);
        setFlag(QSGMaterial::RequiresFullMatrix);
    }

    ~PanadapterSpectrumMaterial() override
    {
        delete spectrumTexture;
        delete peakTexture;
#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
        delete fallbackTexture;
#endif
    }

    QSGMaterialType* type() const override
    {
        static QSGMaterialType type;
        return &type;
    }

    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode) const override;

    int compare(const QSGMaterial* other) const override
    {
        auto const* rhs = static_cast<const PanadapterSpectrumMaterial*>(other);
        if (spectrumTexture != rhs->spectrumTexture)
            return spectrumTexture < rhs->spectrumTexture ? -1 : 1;
        if (peakTexture != rhs->peakTexture)
            return peakTexture < rhs->peakTexture ? -1 : 1;
        return 0;
    }

    QSGTexture* spectrumTexture = nullptr;
    QSGTexture* peakTexture = nullptr;
    float params[4] = {-70.0f, 1.0f / 100.0f, 0.006f, 1.0f};
    float xParams[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float background[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float fill[4] = {0.0f, 0.75f, 0.95f, 0.35f};
    float glow[4] = {0.2f, 0.9f, 1.0f, 0.45f};
    float trace[4] = {0.8f, 1.0f, 1.0f, 1.0f};
    float peak[4] = {1.0f, 1.0f, 1.0f, 0.65f};
#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
    QSGTexture* fallbackSampledTexture()
    {
        if (!fallbackTexture) {
            auto* texture = new DecodiumRhiImageTexture(false);
            texture->setFiltering(QSGTexture::Nearest);
            QImage image(1, 1, QImage::Format_RGBA8888);
            image.fill(Qt::black);
            texture->uploadFullRgbaImage(image, false);
            fallbackTexture = texture;
        }
        return fallbackTexture;
    }

    QSGTexture* fallbackTexture = nullptr;
#endif
};

class PanadapterSpectrumShader final : public QSGMaterialShader
{
public:
    PanadapterSpectrumShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/panadapter_spectrum.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/panadapter_spectrum.frag.qsb"));
    }

    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial*) override
    {
        QByteArray* uniformData = state.uniformData();
        if (uniformData->size() < 192) {
            uniformData->resize(192);
            std::memset(uniformData->data(), 0, static_cast<size_t>(uniformData->size()));
        }
        auto* material = static_cast<PanadapterSpectrumMaterial*>(newMaterial);
        const QMatrix4x4 matrix = state.combinedMatrix();
        std::memcpy(uniformData->data(), matrix.constData(), 64);
        float const opacity = state.opacity();
        std::memcpy(uniformData->data() + 64, &opacity, 4);
        std::memcpy(uniformData->data() + 80, material->params, sizeof(material->params));
        std::memcpy(uniformData->data() + 96, material->xParams, sizeof(material->xParams));
        std::memcpy(uniformData->data() + 112, material->background, sizeof(material->background));
        std::memcpy(uniformData->data() + 128, material->fill, sizeof(material->fill));
        std::memcpy(uniformData->data() + 144, material->glow, sizeof(material->glow));
        std::memcpy(uniformData->data() + 160, material->trace, sizeof(material->trace));
        std::memcpy(uniformData->data() + 176, material->peak, sizeof(material->peak));
        return true;
    }

    void updateSampledImage(RenderState& state, int binding, QSGTexture** texture,
                            QSGMaterial* newMaterial, QSGMaterial*) override
    {
        auto* material = static_cast<PanadapterSpectrumMaterial*>(newMaterial);
        QSGTexture* selected = nullptr;
        if (binding == 1)
            selected = material->spectrumTexture;
        else if (binding == 2)
            selected = material->peakTexture;

#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
        auto commitReadyTexture = [&state](QSGTexture* candidate) -> QSGTexture* {
            if (!candidate)
                return nullptr;
            candidate->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
            return candidate->rhiTexture() ? candidate : nullptr;
        };

        QSGTexture* ready = commitReadyTexture(selected);
        if (!ready)
            ready = commitReadyTexture(material->fallbackSampledTexture());
        *texture = ready;
#else
        *texture = selected;
        if (*texture)
            (*texture)->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
#endif
    }
};

QSGMaterialShader* PanadapterSpectrumMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new PanadapterSpectrumShader;
}
#endif

#if defined(DECODIUM_GPU_PANADAPTER_SPECTRUM_3D_QSB)
class PanadapterSpectrum3dMaterial final : public QSGMaterial
{
public:
    PanadapterSpectrum3dMaterial()
    {
        setFlag(QSGMaterial::NoBatching);
        setFlag(QSGMaterial::RequiresFullMatrix);
    }

    ~PanadapterSpectrum3dMaterial() override
    {
        delete historyTexture;
        delete rowParamsTexture;
        delete paletteTexture;
#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
        delete fallbackTexture;
#endif
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
        auto const* rhs = static_cast<const PanadapterSpectrum3dMaterial*>(other);
        if (historyTexture != rhs->historyTexture)
            return historyTexture < rhs->historyTexture ? -1 : 1;
        if (rowParamsTexture != rhs->rowParamsTexture)
            return rowParamsTexture < rhs->rowParamsTexture ? -1 : 1;
        if (paletteTexture != rhs->paletteTexture)
            return paletteTexture < rhs->paletteTexture ? -1 : 1;
        return 0;
    }

    QSGTexture* historyTexture = nullptr;
    QSGTexture* rowParamsTexture = nullptr;
    QSGTexture* paletteTexture = nullptr;
    // panel width/height, ridge line width, glow width (logical pixels)
    float geometryParams[4] = {1.0f, 1.0f, 1.5f, 4.5f};
    // next write row, history rows, trace count, floor depth in dB
    float historyParams[4] = {0.0f, 1.0f, 2.0f, 6.0f};
    // far Y ratio, maximum X shrink, ridge height ratio, depth exponent
    float perspectiveParams[4] = {0.28f, 0.32f, 0.55f, 2.0f};
    // source X scale/bias, clamp flag, minimum 3D dB span
    float xParams[4] = {1.0f, 0.0f, 1.0f, 80.0f};
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

#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
    QSGTexture* fallbackSampledTexture()
    {
        if (!fallbackTexture) {
            auto* texture = new DecodiumRhiImageTexture(false);
            texture->setFiltering(QSGTexture::Nearest);
            QImage image(1, 1, QImage::Format_RGBA8888);
            image.fill(Qt::black);
            texture->uploadFullRgbaImage(image, false);
            fallbackTexture = texture;
        }
        return fallbackTexture;
    }

    QSGTexture* fallbackTexture = nullptr;
#endif
    QVector<QSGTexture*> retiredTextures;
};

class PanadapterSpectrum3dShader final : public QSGMaterialShader
{
public:
    PanadapterSpectrum3dShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/panadapter_spectrum3d.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/panadapter_spectrum3d.frag.qsb"));
    }

    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial*) override
    {
        QByteArray* uniformData = state.uniformData();
        if (uniformData->size() < 144) {
            uniformData->resize(144);
            std::memset(uniformData->data(), 0, static_cast<size_t>(uniformData->size()));
        }
        auto* material = static_cast<PanadapterSpectrum3dMaterial*>(newMaterial);
        QMatrix4x4 const matrix = state.combinedMatrix();
        std::memcpy(uniformData->data(), matrix.constData(), 64);
        float const opacity = state.opacity();
        std::memcpy(uniformData->data() + 64, &opacity, 4);
        std::memcpy(uniformData->data() + 80,
                    material->geometryParams,
                    sizeof(material->geometryParams));
        std::memcpy(uniformData->data() + 96,
                    material->historyParams,
                    sizeof(material->historyParams));
        std::memcpy(uniformData->data() + 112,
                    material->perspectiveParams,
                    sizeof(material->perspectiveParams));
        std::memcpy(uniformData->data() + 128,
                    material->xParams,
                    sizeof(material->xParams));
        return true;
    }

    void updateSampledImage(RenderState& state, int binding, QSGTexture** texture,
                            QSGMaterial* newMaterial, QSGMaterial*) override
    {
        auto* material = static_cast<PanadapterSpectrum3dMaterial*>(newMaterial);
        QSGTexture* selected = nullptr;
        if (binding == 1)
            selected = material->historyTexture;
        else if (binding == 2)
            selected = material->rowParamsTexture;
        else if (binding == 3)
            selected = material->paletteTexture;

#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
        auto commitReadyTexture = [&state](QSGTexture* candidate) -> QSGTexture* {
            if (!candidate)
                return nullptr;
            candidate->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
            return candidate->rhiTexture() ? candidate : nullptr;
        };

        QSGTexture* ready = commitReadyTexture(selected);
        if (!ready)
            ready = commitReadyTexture(material->fallbackSampledTexture());
        *texture = ready;
#else
        *texture = selected;
        if (*texture)
            (*texture)->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
#endif
    }
};

QSGMaterialShader* PanadapterSpectrum3dMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new PanadapterSpectrum3dShader;
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
    connectBridgePcmFrameFeed();
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow* win) {
        if (m_qsgMetricWindow == win)
            return;
        auto disconnectMetricSignal = [](QMetaObject::Connection& connection) {
            if (connection) {
                QObject::disconnect(connection);
                connection = QMetaObject::Connection();
            }
        };
        disconnectMetricSignal(m_qsgFrameConnection);
        disconnectMetricSignal(m_qsgBeforeSyncConnection);
        disconnectMetricSignal(m_qsgBeforeRenderConnection);
        disconnectMetricSignal(m_qsgAfterRenderConnection);
        m_qsgMetricWindow = win;
        m_qsgFrameLastSwapUs = 0;
        m_qsgBeforeSyncUs.store(0, std::memory_order_relaxed);
        m_qsgBeforeRenderUs.store(0, std::memory_order_relaxed);
        m_qsgAfterRenderUs.store(0, std::memory_order_relaxed);
        m_qsgBeforeSyncCount.store(0, std::memory_order_relaxed);
        m_qsgBeforeRenderCount.store(0, std::memory_order_relaxed);
        m_qsgAfterRenderCount.store(0, std::memory_order_relaxed);
        m_qsgLastBeforeSyncCount = 0;
        m_qsgLastBeforeRenderCount = 0;
        m_qsgLastAfterRenderCount = 0;
        m_qsgLastSwapCount = 0;
        m_qsgSwapCount = 0;
        if (!win)
            return;
        m_qsgBeforeSyncConnection = connect(win, &QQuickWindow::beforeSynchronizing, this, [this]() {
            m_qsgBeforeSyncUs.store(monotonicUs(), std::memory_order_relaxed);
            m_qsgBeforeSyncCount.fetch_add(1, std::memory_order_relaxed);
        }, Qt::DirectConnection);
        m_qsgBeforeRenderConnection = connect(win, &QQuickWindow::beforeRendering, this, [this]() {
            m_qsgBeforeRenderUs.store(monotonicUs(), std::memory_order_relaxed);
            m_qsgBeforeRenderCount.fetch_add(1, std::memory_order_relaxed);
        }, Qt::DirectConnection);
        m_qsgAfterRenderConnection = connect(win, &QQuickWindow::afterRendering, this, [this]() {
            m_qsgAfterRenderUs.store(monotonicUs(), std::memory_order_relaxed);
            m_qsgAfterRenderCount.fetch_add(1, std::memory_order_relaxed);
        }, Qt::DirectConnection);
        m_qsgFrameConnection = connect(win, &QQuickWindow::frameSwapped, this, [this]() {
            qint64 const nowUs = monotonicUs();
            ++m_qsgSwapCount;
            if (m_qsgFrameLastSwapUs > 0)
                recordQsgFrameMetric(nowUs - m_qsgFrameLastSwapUs, nowUs);
            m_qsgFrameLastSwapUs = nowUs;
        });
    });
}

PanadapterItem::~PanadapterItem()
{
    if (m_qsgFrameConnection)
        QObject::disconnect(m_qsgFrameConnection);
    if (m_qsgBeforeSyncConnection)
        QObject::disconnect(m_qsgBeforeSyncConnection);
    if (m_qsgBeforeRenderConnection)
        QObject::disconnect(m_qsgBeforeRenderConnection);
    if (m_qsgAfterRenderConnection)
        QObject::disconnect(m_qsgAfterRenderConnection);
    releaseGpuFftResources();
}

void PanadapterItem::recordOverlayMetric(qint64 elapsedUs,
                                         int decodeLabels,
                                         int clusterLabels,
                                         const QSize& size)
{
    qint64 const safeElapsedUs = qMax<qint64>(0, elapsedUs);
    m_overlayMetricLastUs = safeElapsedUs;
    m_overlayMetricAccumUs += safeElapsedUs;
    ++m_overlayMetricSamples;
    m_overlayMetricMaxUs = qMax(m_overlayMetricMaxUs, static_cast<int>(qMin<qint64>(safeElapsedUs, std::numeric_limits<int>::max())));
    m_overlayMetricDecodeLabels = decodeLabels;
    m_overlayMetricClusterLabels = clusterLabels;
    m_overlayMetricSize = size;

    qint64 const nowMs = monotonicMs();
    if (m_overlayMetricLastLogMs == 0)
        m_overlayMetricLastLogMs = nowMs;
    if (nowMs - m_overlayMetricLastLogMs < kPanMetricLogIntervalMs || m_overlayMetricSamples <= 0)
        return;

    qInfo().noquote()
        << "[PANMETRIC] overlay"
        << "avg_us=" << (m_overlayMetricAccumUs / m_overlayMetricSamples)
        << "max_us=" << m_overlayMetricMaxUs
        << "samples=" << m_overlayMetricSamples
        << "decode_labels=" << m_overlayMetricDecodeLabels
        << "cluster_labels=" << m_overlayMetricClusterLabels
        << "size=" << QStringLiteral("%1x%2").arg(m_overlayMetricSize.width()).arg(m_overlayMetricSize.height());
    m_overlayMetricLastLogMs = nowMs;
    m_overlayMetricAccumUs = 0;
    m_overlayMetricSamples = 0;
    m_overlayMetricMaxUs = 0;
}

void PanadapterItem::recordOverlayNodeMetric(qint64 elapsedUs,
                                             qint64 rebuildUs,
                                             qint64 textureUs,
                                             qint64 nodeUs,
                                             bool needsUpload)
{
    qint64 const safeElapsedUs = qMax<qint64>(0, elapsedUs);
    qint64 const safeRebuildUs = qMax<qint64>(0, rebuildUs);
    qint64 const safeTextureUs = qMax<qint64>(0, textureUs);
    qint64 const safeNodeUs = qMax<qint64>(0, nodeUs);
    m_overlayNodeMetricAccumUs += safeElapsedUs;
    m_overlayNodeMetricRebuildAccumUs += safeRebuildUs;
    m_overlayNodeMetricTextureAccumUs += safeTextureUs;
    m_overlayNodeMetricNodeAccumUs += safeNodeUs;
    ++m_overlayNodeMetricSamples;
    if (needsUpload)
        ++m_overlayNodeMetricUploadSamples;
    m_overlayNodeMetricMaxUs = qMax(m_overlayNodeMetricMaxUs, static_cast<int>(qMin<qint64>(safeElapsedUs, std::numeric_limits<int>::max())));
    m_overlayNodeMetricRebuildMaxUs = qMax(m_overlayNodeMetricRebuildMaxUs, static_cast<int>(qMin<qint64>(safeRebuildUs, std::numeric_limits<int>::max())));
    m_overlayNodeMetricTextureMaxUs = qMax(m_overlayNodeMetricTextureMaxUs, static_cast<int>(qMin<qint64>(safeTextureUs, std::numeric_limits<int>::max())));
    m_overlayNodeMetricNodeMaxUs = qMax(m_overlayNodeMetricNodeMaxUs, static_cast<int>(qMin<qint64>(safeNodeUs, std::numeric_limits<int>::max())));

    qint64 const nowMs = monotonicMs();
    if (m_overlayNodeMetricLastLogMs == 0)
        m_overlayNodeMetricLastLogMs = nowMs;
    if (nowMs - m_overlayNodeMetricLastLogMs < kPanMetricLogIntervalMs || m_overlayNodeMetricSamples <= 0)
        return;

    qInfo().noquote()
        << "[PANMETRIC] overlay_node"
        << "avg_us=" << (m_overlayNodeMetricAccumUs / m_overlayNodeMetricSamples)
        << "max_us=" << m_overlayNodeMetricMaxUs
        << "samples=" << m_overlayNodeMetricSamples
        << "upload_samples=" << m_overlayNodeMetricUploadSamples
        << "rebuild_avg_us=" << (m_overlayNodeMetricRebuildAccumUs / m_overlayNodeMetricSamples)
        << "rebuild_max_us=" << m_overlayNodeMetricRebuildMaxUs
        << "texture_avg_us=" << (m_overlayNodeMetricTextureAccumUs / m_overlayNodeMetricSamples)
        << "texture_max_us=" << m_overlayNodeMetricTextureMaxUs
        << "node_avg_us=" << (m_overlayNodeMetricNodeAccumUs / m_overlayNodeMetricSamples)
        << "node_max_us=" << m_overlayNodeMetricNodeMaxUs;
    m_overlayNodeMetricLastLogMs = nowMs;
    m_overlayNodeMetricAccumUs = 0;
    m_overlayNodeMetricRebuildAccumUs = 0;
    m_overlayNodeMetricTextureAccumUs = 0;
    m_overlayNodeMetricNodeAccumUs = 0;
    m_overlayNodeMetricSamples = 0;
    m_overlayNodeMetricUploadSamples = 0;
    m_overlayNodeMetricMaxUs = 0;
    m_overlayNodeMetricRebuildMaxUs = 0;
    m_overlayNodeMetricTextureMaxUs = 0;
    m_overlayNodeMetricNodeMaxUs = 0;
}

void PanadapterItem::recordPaintMetric(qint64 elapsedUs,
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
                                       int pendingRows)
{
    qint64 const safeElapsedUs = qMax<qint64>(0, elapsedUs);
    qint64 const safeLockWaitUs = qMax<qint64>(0, lockWaitUs);
    qint64 const safeGeometryUs = qMax<qint64>(0, geometryUs);
    qint64 const safeDrainUs = qMax<qint64>(0, drainUs);
    qint64 const safeOverlayUs = qMax<qint64>(0, overlayUs);
    qint64 const safeNodesUs = qMax<qint64>(0, nodesUs);
    qint64 const safeSpectrumNodeUs = qMax<qint64>(0, spectrumNodeUs);
    qint64 const safeWaterfallNodeUs = qMax<qint64>(0, waterfallNodeUs);
    qint64 const safeWaterfallTextureUs = qMax<qint64>(0, waterfallTextureUs);
    qint64 const safeWaterfallDisplayUs = qMax<qint64>(0, waterfallDisplayUs);
    qint64 const safeWaterfallSetupUs = qMax<qint64>(0, waterfallSetupUs);
    qint64 const safeWaterfallMarkUs = qMax<qint64>(0, waterfallMarkUs);
    qint64 const safeWaterfallLogUs = qMax<qint64>(0, waterfallLogUs);
    m_paintMetricLastUs = safeElapsedUs;
    m_paintMetricAccumUs += safeElapsedUs;
    m_paintMetricLockWaitAccumUs += safeLockWaitUs;
    m_paintMetricGeometryAccumUs += safeGeometryUs;
    m_paintMetricDrainAccumUs += safeDrainUs;
    m_paintMetricOverlayAccumUs += safeOverlayUs;
    m_paintMetricNodesAccumUs += safeNodesUs;
    m_paintMetricSpectrumNodeAccumUs += safeSpectrumNodeUs;
    m_paintMetricWaterfallNodeAccumUs += safeWaterfallNodeUs;
    m_paintMetricWaterfallTextureAccumUs += safeWaterfallTextureUs;
    m_paintMetricWaterfallDisplayAccumUs += safeWaterfallDisplayUs;
    m_paintMetricWaterfallSetupAccumUs += safeWaterfallSetupUs;
    m_paintMetricWaterfallMarkAccumUs += safeWaterfallMarkUs;
    m_paintMetricWaterfallLogAccumUs += safeWaterfallLogUs;
    ++m_paintMetricSamples;
    m_paintMetricMaxUs = qMax(m_paintMetricMaxUs, static_cast<int>(qMin<qint64>(safeElapsedUs, std::numeric_limits<int>::max())));
    m_paintMetricLockWaitMaxUs = qMax(m_paintMetricLockWaitMaxUs, static_cast<int>(qMin<qint64>(safeLockWaitUs, std::numeric_limits<int>::max())));
    m_paintMetricGeometryMaxUs = qMax(m_paintMetricGeometryMaxUs, static_cast<int>(qMin<qint64>(safeGeometryUs, std::numeric_limits<int>::max())));
    m_paintMetricDrainMaxUs = qMax(m_paintMetricDrainMaxUs, static_cast<int>(qMin<qint64>(safeDrainUs, std::numeric_limits<int>::max())));
    m_paintMetricOverlayMaxUs = qMax(m_paintMetricOverlayMaxUs, static_cast<int>(qMin<qint64>(safeOverlayUs, std::numeric_limits<int>::max())));
    m_paintMetricNodesMaxUs = qMax(m_paintMetricNodesMaxUs, static_cast<int>(qMin<qint64>(safeNodesUs, std::numeric_limits<int>::max())));
    m_paintMetricSpectrumNodeMaxUs = qMax(m_paintMetricSpectrumNodeMaxUs, static_cast<int>(qMin<qint64>(safeSpectrumNodeUs, std::numeric_limits<int>::max())));
    m_paintMetricWaterfallNodeMaxUs = qMax(m_paintMetricWaterfallNodeMaxUs, static_cast<int>(qMin<qint64>(safeWaterfallNodeUs, std::numeric_limits<int>::max())));
    m_paintMetricWaterfallTextureMaxUs = qMax(m_paintMetricWaterfallTextureMaxUs, static_cast<int>(qMin<qint64>(safeWaterfallTextureUs, std::numeric_limits<int>::max())));
    m_paintMetricWaterfallDisplayMaxUs = qMax(m_paintMetricWaterfallDisplayMaxUs, static_cast<int>(qMin<qint64>(safeWaterfallDisplayUs, std::numeric_limits<int>::max())));
    m_paintMetricWaterfallSetupMaxUs = qMax(m_paintMetricWaterfallSetupMaxUs, static_cast<int>(qMin<qint64>(safeWaterfallSetupUs, std::numeric_limits<int>::max())));
    m_paintMetricWaterfallMarkMaxUs = qMax(m_paintMetricWaterfallMarkMaxUs, static_cast<int>(qMin<qint64>(safeWaterfallMarkUs, std::numeric_limits<int>::max())));
    m_paintMetricWaterfallLogMaxUs = qMax(m_paintMetricWaterfallLogMaxUs, static_cast<int>(qMin<qint64>(safeWaterfallLogUs, std::numeric_limits<int>::max())));
    switch (waterfallPath) {
    case 1: ++m_paintMetricWaterfallPathDirectSamples; break;
    case 2: ++m_paintMetricWaterfallPathShaderSamples; break;
    case 3: ++m_paintMetricWaterfallPathCpuSamples; break;
    default: ++m_paintMetricWaterfallPathNoneSamples; break;
    }
    m_paintMetricTextureCreateCount += qMax(0, textureCreateCount);
    m_paintMetricTextureUploadRows += qMax(0, textureUploadRows);
    m_paintMetricTextureFullUploads += qMax(0, textureFullUploads);
    m_paintMetricGpuDirectReady = gpuDirectReady;
    m_paintMetricPendingRows = pendingRows;

    qint64 const nowMs = monotonicMs();
    if (m_paintMetricLastLogMs == 0)
        m_paintMetricLastLogMs = nowMs;
    if (nowMs - m_paintMetricLastLogMs < kPanMetricLogIntervalMs || m_paintMetricSamples <= 0)
        return;

    qInfo().noquote()
        << "[PANMETRIC] paint"
        << "avg_us=" << (m_paintMetricAccumUs / m_paintMetricSamples)
        << "max_us=" << m_paintMetricMaxUs
        << "samples=" << m_paintMetricSamples
        << "lock_wait_avg_us=" << (m_paintMetricLockWaitAccumUs / m_paintMetricSamples)
        << "lock_wait_max_us=" << m_paintMetricLockWaitMaxUs
        << "geometry_avg_us=" << (m_paintMetricGeometryAccumUs / m_paintMetricSamples)
        << "geometry_max_us=" << m_paintMetricGeometryMaxUs
        << "drain_avg_us=" << (m_paintMetricDrainAccumUs / m_paintMetricSamples)
        << "drain_max_us=" << m_paintMetricDrainMaxUs
        << "overlay_avg_us=" << (m_paintMetricOverlayAccumUs / m_paintMetricSamples)
        << "overlay_max_us=" << m_paintMetricOverlayMaxUs
        << "nodes_avg_us=" << (m_paintMetricNodesAccumUs / m_paintMetricSamples)
        << "nodes_max_us=" << m_paintMetricNodesMaxUs
        << "spectrum_node_avg_us=" << (m_paintMetricSpectrumNodeAccumUs / m_paintMetricSamples)
        << "spectrum_node_max_us=" << m_paintMetricSpectrumNodeMaxUs
        << "waterfall_node_avg_us=" << (m_paintMetricWaterfallNodeAccumUs / m_paintMetricSamples)
        << "waterfall_node_max_us=" << m_paintMetricWaterfallNodeMaxUs
        << "waterfall_texture_avg_us=" << (m_paintMetricWaterfallTextureAccumUs / m_paintMetricSamples)
        << "waterfall_texture_max_us=" << m_paintMetricWaterfallTextureMaxUs
        << "waterfall_display_avg_us=" << (m_paintMetricWaterfallDisplayAccumUs / m_paintMetricSamples)
        << "waterfall_display_max_us=" << m_paintMetricWaterfallDisplayMaxUs
        << "waterfall_setup_avg_us=" << (m_paintMetricWaterfallSetupAccumUs / m_paintMetricSamples)
        << "waterfall_setup_max_us=" << m_paintMetricWaterfallSetupMaxUs
        << "waterfall_mark_avg_us=" << (m_paintMetricWaterfallMarkAccumUs / m_paintMetricSamples)
        << "waterfall_mark_max_us=" << m_paintMetricWaterfallMarkMaxUs
        << "waterfall_log_avg_us=" << (m_paintMetricWaterfallLogAccumUs / m_paintMetricSamples)
        << "waterfall_log_max_us=" << m_paintMetricWaterfallLogMaxUs
        << "waterfall_paths_none_direct_shader_cpu="
        << QStringLiteral("%1/%2/%3/%4")
            .arg(m_paintMetricWaterfallPathNoneSamples)
            .arg(m_paintMetricWaterfallPathDirectSamples)
            .arg(m_paintMetricWaterfallPathShaderSamples)
            .arg(m_paintMetricWaterfallPathCpuSamples)
        << "texture_creates=" << m_paintMetricTextureCreateCount
        << "texture_upload_rows=" << m_paintMetricTextureUploadRows
        << "texture_full_uploads=" << m_paintMetricTextureFullUploads
        << "gpu_direct=" << (m_paintMetricGpuDirectReady ? 1 : 0)
        << "pending_rows=" << m_paintMetricPendingRows
        << "waterfall_rows=" << m_renderWaterfallHistoryRows;
    m_paintMetricLastLogMs = nowMs;
    m_paintMetricAccumUs = 0;
    m_paintMetricLockWaitAccumUs = 0;
    m_paintMetricGeometryAccumUs = 0;
    m_paintMetricDrainAccumUs = 0;
    m_paintMetricOverlayAccumUs = 0;
    m_paintMetricNodesAccumUs = 0;
    m_paintMetricSpectrumNodeAccumUs = 0;
    m_paintMetricWaterfallNodeAccumUs = 0;
    m_paintMetricWaterfallTextureAccumUs = 0;
    m_paintMetricWaterfallDisplayAccumUs = 0;
    m_paintMetricWaterfallSetupAccumUs = 0;
    m_paintMetricWaterfallMarkAccumUs = 0;
    m_paintMetricWaterfallLogAccumUs = 0;
    m_paintMetricSamples = 0;
    m_paintMetricMaxUs = 0;
    m_paintMetricLockWaitMaxUs = 0;
    m_paintMetricGeometryMaxUs = 0;
    m_paintMetricDrainMaxUs = 0;
    m_paintMetricOverlayMaxUs = 0;
    m_paintMetricNodesMaxUs = 0;
    m_paintMetricSpectrumNodeMaxUs = 0;
    m_paintMetricWaterfallNodeMaxUs = 0;
    m_paintMetricWaterfallTextureMaxUs = 0;
    m_paintMetricWaterfallDisplayMaxUs = 0;
    m_paintMetricWaterfallSetupMaxUs = 0;
    m_paintMetricWaterfallMarkMaxUs = 0;
    m_paintMetricWaterfallLogMaxUs = 0;
    m_paintMetricWaterfallPathNoneSamples = 0;
    m_paintMetricWaterfallPathDirectSamples = 0;
    m_paintMetricWaterfallPathShaderSamples = 0;
    m_paintMetricWaterfallPathCpuSamples = 0;
    m_paintMetricTextureCreateCount = 0;
    m_paintMetricTextureUploadRows = 0;
    m_paintMetricTextureFullUploads = 0;
}

void PanadapterItem::recordQsgFrameMetric(qint64 frameUs, qint64 swapUs)
{
    constexpr qint64 kSpikeThresholdUs = 80000;

    if (frameUs <= 0)
        return;
    m_qsgFrameMetricAccumUs += frameUs;
    ++m_qsgFrameMetricSamples;
    m_qsgFrameMetricMaxUs = qMax(m_qsgFrameMetricMaxUs, static_cast<int>(qMin<qint64>(frameUs, std::numeric_limits<int>::max())));

    int const beforeSyncCount = m_qsgBeforeSyncCount.load(std::memory_order_relaxed);
    int const beforeRenderCount = m_qsgBeforeRenderCount.load(std::memory_order_relaxed);
    int const afterRenderCount = m_qsgAfterRenderCount.load(std::memory_order_relaxed);
    int const swapCount = m_qsgSwapCount;
    int const beforeSyncDelta = beforeSyncCount - m_qsgLastBeforeSyncCount;
    int const beforeRenderDelta = beforeRenderCount - m_qsgLastBeforeRenderCount;
    int const afterRenderDelta = afterRenderCount - m_qsgLastAfterRenderCount;
    int const swapDelta = swapCount - m_qsgLastSwapCount;
    m_qsgLastBeforeSyncCount = beforeSyncCount;
    m_qsgLastBeforeRenderCount = beforeRenderCount;
    m_qsgLastAfterRenderCount = afterRenderCount;
    m_qsgLastSwapCount = swapCount;

    qint64 const previousSwapUs = swapUs - frameUs;
    qint64 const beforeSyncUs = m_qsgBeforeSyncUs.load(std::memory_order_relaxed);
    qint64 const beforeRenderUs = m_qsgBeforeRenderUs.load(std::memory_order_relaxed);
    qint64 const afterRenderUs = m_qsgAfterRenderUs.load(std::memory_order_relaxed);
    qint64 const idleUs = (beforeSyncUs >= previousSwapUs && beforeSyncUs <= swapUs)
        ? beforeSyncUs - previousSwapUs : -1;
    qint64 const syncUs = (beforeSyncUs > 0 && beforeRenderUs >= beforeSyncUs && beforeRenderUs <= swapUs)
        ? beforeRenderUs - beforeSyncUs : -1;
    qint64 const renderUs = (beforeRenderUs > 0 && afterRenderUs >= beforeRenderUs && afterRenderUs <= swapUs)
        ? afterRenderUs - beforeRenderUs : -1;
    qint64 const presentUs = (afterRenderUs > 0 && afterRenderUs <= swapUs)
        ? swapUs - afterRenderUs : -1;
    if (idleUs >= 0 && syncUs >= 0 && renderUs >= 0 && presentUs >= 0) {
        ++m_qsgPhaseSamples;
        m_qsgPhaseIdleAccumUs += idleUs;
        m_qsgPhaseSyncAccumUs += syncUs;
        m_qsgPhaseRenderAccumUs += renderUs;
        m_qsgPhasePresentAccumUs += presentUs;
        m_qsgPhaseIdleMaxUs = qMax(m_qsgPhaseIdleMaxUs, static_cast<int>(qMin<qint64>(idleUs, std::numeric_limits<int>::max())));
        if (syncUs > m_qsgPhaseSyncMaxUs) {
            m_qsgPhaseSyncMaxUs = static_cast<int>(qMin<qint64>(syncUs, std::numeric_limits<int>::max()));
            m_qsgPhaseSyncMaxDecodeReadyStartAgoMs = DecodiumBridge::msSinceLastDecodeReadySlotStart();
            m_qsgPhaseSyncMaxDecodeReadyEndAgoMs = DecodiumBridge::msSinceLastDecodeReadySlotEnd();
            m_qsgPhaseSyncMaxDecodeModelEmitStartAgoMs = DecodiumBridge::msSinceLastDecodeModelEmitStart();
            m_qsgPhaseSyncMaxDecodeModelEmitEndAgoMs = DecodiumBridge::msSinceLastDecodeModelEmitEnd();
        }
        m_qsgPhaseRenderMaxUs = qMax(m_qsgPhaseRenderMaxUs, static_cast<int>(qMin<qint64>(renderUs, std::numeric_limits<int>::max())));
        m_qsgPhasePresentMaxUs = qMax(m_qsgPhasePresentMaxUs, static_cast<int>(qMin<qint64>(presentUs, std::numeric_limits<int>::max())));
    }

    qint64 const nowMs = monotonicMs();
    if (frameUs >= kSpikeThresholdUs) {
        ++m_qsgFrameSpikeCount;
        ++m_qsgFrameMetricSpikeSamples;
        m_qsgFrameMetricSpikeMaxUs = qMax(
            m_qsgFrameMetricSpikeMaxUs,
            static_cast<int>(qMin<qint64>(frameUs, std::numeric_limits<int>::max())));
    }

    if (m_qsgFrameMetricLastLogMs == 0)
        m_qsgFrameMetricLastLogMs = nowMs;
    if (nowMs - m_qsgFrameMetricLastLogMs < kPanMetricLogIntervalMs || m_qsgFrameMetricSamples <= 0)
        return;

    qInfo().noquote()
        << "[PANMETRIC] qsg_frame"
        << "avg_ms=" << QString::number(static_cast<double>(m_qsgFrameMetricAccumUs) / m_qsgFrameMetricSamples / 1000.0, 'f', 2)
        << "max_ms=" << QString::number(static_cast<double>(m_qsgFrameMetricMaxUs) / 1000.0, 'f', 2)
        << "samples=" << m_qsgFrameMetricSamples
        << "spikes=" << m_qsgFrameMetricSpikeSamples
        << "spike_max_ms=" << QString::number(static_cast<double>(m_qsgFrameMetricSpikeMaxUs) / 1000.0, 'f', 2)
        << "spikes_total=" << m_qsgFrameSpikeCount
        << "phase_counts_last="
        << QStringLiteral("%1/%2/%3/%4").arg(beforeSyncDelta).arg(beforeRenderDelta).arg(afterRenderDelta).arg(swapDelta)
        << "gpu_direct=" << (m_gpuDirectTextureReady ? 1 : 0)
        << "running=" << (m_running ? 1 : 0)
        << "pending_rows=" << m_pendingWaterfallRows.size()
        << "item_size=" << QStringLiteral("%1x%2").arg(qRound(width())).arg(qRound(height()));
    if (m_qsgPhaseSamples > 0) {
        qInfo().noquote()
            << "[PANMETRIC] qsg_phase"
            << "samples=" << m_qsgPhaseSamples
            << "idle_avg_ms=" << metricMs(m_qsgPhaseIdleAccumUs / m_qsgPhaseSamples)
            << "idle_max_ms=" << metricMs(m_qsgPhaseIdleMaxUs)
            << "sync_avg_ms=" << metricMs(m_qsgPhaseSyncAccumUs / m_qsgPhaseSamples)
            << "sync_max_ms=" << metricMs(m_qsgPhaseSyncMaxUs)
            << "render_avg_ms=" << metricMs(m_qsgPhaseRenderAccumUs / m_qsgPhaseSamples)
            << "render_max_ms=" << metricMs(m_qsgPhaseRenderMaxUs)
            << "present_avg_ms=" << metricMs(m_qsgPhasePresentAccumUs / m_qsgPhaseSamples)
            << "present_max_ms=" << metricMs(m_qsgPhasePresentMaxUs)
            << "syncmax_decode_ready_start_ago_ms=" << m_qsgPhaseSyncMaxDecodeReadyStartAgoMs
            << "syncmax_decode_ready_end_ago_ms=" << m_qsgPhaseSyncMaxDecodeReadyEndAgoMs
            << "syncmax_model_emit_start_ago_ms=" << m_qsgPhaseSyncMaxDecodeModelEmitStartAgoMs
            << "syncmax_model_emit_end_ago_ms=" << m_qsgPhaseSyncMaxDecodeModelEmitEndAgoMs;
    }
    m_qsgFrameMetricLastLogMs = nowMs;
    m_qsgFrameMetricAccumUs = 0;
    m_qsgFrameMetricSamples = 0;
    m_qsgFrameMetricMaxUs = 0;
    m_qsgFrameMetricSpikeSamples = 0;
    m_qsgFrameMetricSpikeMaxUs = 0;
    m_qsgPhaseIdleAccumUs = 0;
    m_qsgPhaseSyncAccumUs = 0;
    m_qsgPhaseRenderAccumUs = 0;
    m_qsgPhasePresentAccumUs = 0;
    m_qsgPhaseSamples = 0;
    m_qsgPhaseIdleMaxUs = 0;
    m_qsgPhaseSyncMaxUs = 0;
    m_qsgPhaseRenderMaxUs = 0;
    m_qsgPhasePresentMaxUs = 0;
    m_qsgPhaseSyncMaxDecodeReadyStartAgoMs = -1;
    m_qsgPhaseSyncMaxDecodeReadyEndAgoMs = -1;
    m_qsgPhaseSyncMaxDecodeModelEmitStartAgoMs = -1;
    m_qsgPhaseSyncMaxDecodeModelEmitEndAgoMs = -1;
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
    QSGRendererInterface::GraphicsApi const api =
        static_cast<QSGRendererInterface::GraphicsApi>(sceneGraphApiKey());
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
    if (!window() || !window()->rendererInterface())
        return false;
    QSGRendererInterface::GraphicsApi const api =
        static_cast<QSGRendererInterface::GraphicsApi>(sceneGraphApiKey());
    return api != QSGRendererInterface::Software
        && api != QSGRendererInterface::Null
        && api != QSGRendererInterface::Unknown
        && api != QSGRendererInterface::OpenVG;
#else
    return false;
#endif
}

bool PanadapterItem::spectrum3dGpuSupported() const
{
#if defined(DECODIUM_QT_RHI_TEXTURE_UPLOAD) \
    && defined(DECODIUM_GPU_PANADAPTER_FFT_QSB) \
    && defined(DECODIUM_GPU_PANADAPTER_SPECTRUM_3D_QSB)
    if (qEnvironmentVariableIsSet("DECODIUM_DISABLE_SPECTRUM_3D_GPU")
        || m_spectrum3dGpuBlocked.load(std::memory_order_acquire)) {
        return false;
    }
    if (!window() || !window()->rendererInterface())
        return false;
    QSGRendererInterface::GraphicsApi const api =
        static_cast<QSGRendererInterface::GraphicsApi>(sceneGraphApiKey());
    return QSGRendererInterface::isApiRhiBased(api)
        && api != QSGRendererInterface::Software
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
    markAllDirty();
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
    int const wfHistoryH = waterfallHistoryRowsForVisibleHeight(wfH);
    m_renderSpectrumSize = QSize(w, qMax(1, specH));
    m_renderWaterfallSize = QSize(w, wfH);
    m_renderWaterfallHistoryRows = wfHistoryH;

    // Crea sempre lo spectrum image (richiede solo w > 0)
    if (m_spectrumImage.isNull() ||
        m_spectrumImage.width() != w ||
        m_spectrumImage.height() != specH) {
        m_spectrumImage = QImage(w, qMax(1, specH), QImage::Format_ARGB32_Premultiplied);
        m_spectrumImage.fill(m_paletteIndex == 11 ? QColor(255, 255, 255) : QColor(0, 0, 0));
    }


    // Crea il waterfall image solo se c'è spazio. La profondità storica è
    // stabilizzata a scaglioni: l'altezza visibile QML può variare di pochi
    // pixel durante layout/resize, ma la texture GPU non deve essere ricreata
    // a ogni oscillazione.
    if (wfH > 0) {
        if (m_waterfallImage.isNull() ||
            m_waterfallImage.width() != w ||
            m_waterfallImage.height() != wfHistoryH) {
            m_waterfallImage = QImage(w, wfHistoryH, QImage::Format_RGB32);
            m_waterfallImage.fill(m_paletteIndex == 11 ? QColor(255, 255, 255) : QColor(0, 0, 0));
            m_waterfallDisplayImage = QImage(w, wfH, QImage::Format_RGB32);
            m_waterfallDisplayImage.fill(m_paletteIndex == 11 ? QColor(255, 255, 255) : QColor(0, 0, 0));
            m_waterfallIntensityImage = QImage(w, wfHistoryH, QImage::Format_Grayscale8);
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
bool PanadapterItem::consumeUpdateBudgetLocked()
{
    if (!m_throttleActive) {
        return true;
    }

    qint64 const throttleNs = qint64(m_throttleIntervalMs) * 1000 * 1000;
    qint64 const nowNs = std::chrono::steady_clock::now().time_since_epoch().count();
    if (nowNs - m_lastUpdateNs < throttleNs) {
        return false;
    }
    m_lastUpdateNs = nowNs;
    return true;
}

void PanadapterItem::setExternalSpectrumActive(bool active)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_externalSpectrumActive == active)
            return;

        m_externalSpectrumActive = active;

        // The PCM path owns a GPU FFT with a resolution derived from the
        // decoder audio passband.  RTL-SDR supplies an already-computed RF
        // spectrum instead.  Invalidate every PCM/GPU ownership marker when
        // switching sources, otherwise a receiver restart can leave the old
        // audio-bin count active and reject all subsequent RF frames.
        m_gpuFftUiBinsExpected = 0;
        m_gpuDirectTextureReady = false;
        m_hasPendingPcmFrame = false;
        m_lastGpuFftFrameMs = 0;
        m_lastGpuFftReadbackMs = 0;
        if (m_gpuFft) {
            m_gpuFft->readbackPending = false;
            m_gpuFft->readbackPendingSinceMs = 0;
            ++m_gpuFft->readbackSerial;
        }

        m_bins.clear();
        m_peakBins.clear();
        m_avgStack.clear();
        m_pendingWaterfallRows.clear();
        m_spectrumDirty = true;
        m_spectrumOverlayDirty = true;
        m_loggedMismatchedSpectrumSuppressed = false;
    }

    resetWaterfall();
    qInfo().noquote()
        << "[PANDBG] Panadapter spectrum source switched"
        << "source=" << (active ? "external_RF" : "decoder_PCM")
        << "gpu_audio_fft_reset=1";
    emit externalSpectrumActiveChanged();
    update();
}

bool PanadapterItem::requiresCpuSpectrumHistory() const
{
    QMutexLocker lock(&m_mutex);
    // The normal 3D path samples the GPU history texture directly.  Ask the
    // bridge for asynchronous FFTW rows only when that shader path is not
    // compiled, explicitly disabled, or blocked after a resource failure.
    return m_spectrum3d
        && !m_externalSpectrumActive
        && m_gpuDirectTextureReady
        && !m_gpuFftFailed
        && m_gpuFft
        && m_gpuFft->directTexturePathActive
        && !spectrum3dGpuSupported();
}

void PanadapterItem::setSpectrum3d(bool enabled)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_spectrum3d == enabled)
            return;
        m_spectrum3d = enabled;
        // The next scene-graph pass must replace the direct/2D nodes.  Do not
        // tear down the direct textures: disabling 3D must resume that fast
        // path immediately, without reallocating or touching the waterfall.
        m_spectrumDirty = true;
        m_spectrumOverlayDirty = true;
    }

    emit spectrum3dChanged();
    update();
}

void PanadapterItem::setNoiseFloorPercentile(int value)
{
    int const clamped = qBound(5, value, 40);
    {
        QMutexLocker lock(&m_mutex);
        if (m_noiseFloorPercentile == clamped)
            return;
        m_noiseFloorPercentile = clamped;
    }

    emit noiseFloorPercentileChanged();
    update();
}

void PanadapterItem::addSpectrumData(const QVector<float>& dbValues,
                                      float minDb, float maxDb,
                                      float freqMinHz, float freqMaxHz)
{
    QMutexLocker lock(&m_mutex);
    if (dbValues.isEmpty()) return;

    qint64 const nowMs = monotonicMs();
    bool const gpuFftOwnsSpectrum =
        !m_externalSpectrumActive
        && !m_gpuFftFailed
        && m_gpuFftUiBinsExpected > 0;
    if (gpuFftOwnsSpectrum
        && dbValues.size() != m_gpuFftUiBinsExpected) {
        if (!m_loggedMismatchedSpectrumSuppressed) {
            if (panadapterDebugLoggingEnabled()) {
                qint64 const lastGpuReadbackAgeMs =
                    m_lastGpuFftReadbackMs > 0 ? nowMs - m_lastGpuFftReadbackMs : -1;
                qInfo().noquote()
                    << "[PANDBG] Panadapter mismatched spectrum suppressed"
                    << "reason=GPU_FFT_active_resolution_lock"
                    << "bins=" << dbValues.size()
                    << "expected_bins=" << m_gpuFftUiBinsExpected
                    << "last_gpu_readback_ms_ago=" << lastGpuReadbackAgeMs;
            }
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
        // Ignore the hidden high-frequency roll-off when estimating the
        // on-screen noise floor on CPU FFT paths.
        PanadapterFreqView const freqView = makePanadapterFreqView(
            static_cast<float>(m_startFreq),
            static_cast<float>(m_bandwidth),
            m_dataFreqMin,
            m_dataFreqMax,
            m_zoomFactor,
            static_cast<float>(m_panHz));
        int const nBins = dbValues.size();
        float const visibleStart = qMax(freqView.viewStart, m_dataFreqMin);
        float const visibleEnd = qMin(freqView.viewStart + freqView.viewRange, m_dataFreqMax);
        int const firstBin = qBound(0,
            static_cast<int>(std::floor((visibleStart - m_dataFreqMin)
                                        / freqView.dataRange * nBins)),
            qMax(0, nBins - 1));
        int const endBin = qBound(firstBin + 1,
            static_cast<int>(std::ceil((visibleEnd - m_dataFreqMin)
                                       / freqView.dataRange * nBins)),
            nBins);
        QVector<float> s;
        s.reserve(endBin - firstBin);
        for (int i = firstBin; i < endBin; ++i) {
            if (std::isfinite(dbValues.at(i)))
                s.append(dbValues.at(i));
        }
        if (s.isEmpty()) {
            for (float value : dbValues) {
                if (std::isfinite(value))
                    s.append(value);
            }
        }
        if (s.isEmpty())
            return;
        std::sort(s.begin(), s.end());
        int const n = s.size();
        // Decimo percentile e smorzamento lento, come dalla 1.0.495: col
        // venticinquesimo un quarto dello spettro veniva dichiarato rumore
        // e tagliato, e il filtro interveniva in modo drastico.
        float const fl = s[qBound(0, n * m_noiseFloorPercentile / 100, n - 1)];
        bool const resetFloor = !std::isfinite(m_measuredFloor)
            || m_measuredFloor < -120.0f
            || std::abs(m_measuredFloor - fl) > 35.0f;
        m_measuredFloor = resetFloor
            ? fl
            : (0.03f * fl + 0.97f * m_measuredFloor);
        m_measuredPeak = m_measuredFloor + 80.0f;
        m_minDb = m_measuredFloor;
        m_maxDb = m_measuredPeak;
        static std::atomic_bool loggedCpuAutoRange {false};
        if (!loggedCpuAutoRange.exchange(true, std::memory_order_relaxed)) {
            qInfo().noquote()
                << "[PANDBG] Panadapter CPU auto-range visible passband"
                << "full_bins=" << dbValues.size()
                << "visible_bins=" << s.size()
                << "visible_hz=" << QStringLiteral("%1..%2")
                                      .arg(visibleStart, 0, 'f', 1)
                                      .arg(visibleEnd, 0, 'f', 1)
                << "floor_percentile=" << m_noiseFloorPercentile
                << "floor_percentile_db=" << fl
                << "reset=" << resetFloor
                << "span_db=80";
        }
        emit measuredFloorChanged();
        emit measuredPeakChanged();
    } else if (maxDb > minDb) {
        // Quando auto-range è spento, usa il range reale fornito dal bridge.
        // Questo evita la saturazione totale con i valori FFT moderni sul path mac legacy.
        m_minDb = minDb;
        m_maxDb = maxDb;
    }

    // Escursione vera dei segnali, per l'altezza delle creste 3D. La
    // finestra dei colori non serve: con la soglia automatica parte dal
    // rumore e sale di 80 dB, e un segnale a +10 dB alzerebbe la traccia
    // dello 0,8% - cioe' niente. Qui si guarda dove arrivano i segnali.
    {
        float peak = -1000.0f;
        for (float value : dbValues) {
            if (std::isfinite(value) && value > peak)
                peak = value;
        }
        if (peak > -999.0f) {
            float const floorDb = m_minDb + m_spectrum3dFloorDepth;
            float const widest = qMax(1.0f, m_maxDb - floorDb);
            float const target = qBound(18.0f, peak - floorDb, widest);
            m_spectrum3dSpanDb = m_spectrum3dSpanInit
                ? 0.10f * target + 0.90f * m_spectrum3dSpanDb
                : target;
            m_spectrum3dSpanInit = true;
        }
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
    // 1.0.98: intervallo letto da m_throttleIntervalMs (default 100ms = 10 fps).
    // QML puo' alzarlo in FT2-Link/macOS per evitare render loop continui.
    bool const shouldEmitUpdate = consumeUpdateBudgetLocked();
    lock.unlock();
    if (shouldEmitUpdate) update();
}

void PanadapterItem::activateCpuSpectrumFallback()
{
    bool switched = false;
    {
        QMutexLocker lock(&m_mutex);
        switched = m_gpuDirectTextureReady
            || m_gpuFftUiBinsExpected > 0
            || m_hasPendingPcmFrame;
        m_gpuDirectTextureReady = false;
        m_gpuFftUiBinsExpected = 0;
        m_hasPendingPcmFrame = false;
        m_gpuFftActiveNotified = false;
        ++m_gpuFftFallbackGeneration;
        m_loggedLegacySpectrumSuppressed = false;
        m_loggedMismatchedSpectrumSuppressed = false;
        m_spectrumDirty = true;
        m_spectrumOverlayDirty = true;
    }

    if (!switched)
        return;

    qInfo().noquote()
        << "[PANDBG] Panadapter direct GPU texture released"
        << "reason=CPU_FFT_fallback"
        << "fallback=QSG_CPU_spectrum_and_waterfall";
    update();
}

void PanadapterItem::prepareGpuSpectrumRetry()
{
    QMutexLocker lock(&m_mutex);
    m_gpuFftActiveNotified = false;
    m_gpuFftFailed = false;
    m_gpuFftFailureReason.clear();
    m_hasPendingPcmFrame = false;
}

// Compatibilità: riceve valori 0-1 normalizzati e li converte in dB
void PanadapterItem::addSpectrumDataNorm(const QVector<float>& normValues)
{
    {
        QMutexLocker lock(&m_mutex);
        qint64 const nowMs = monotonicMs();
        bool const gpuFftOwnsSpectrum =
            !m_gpuFftFailed
            && m_gpuFftUiBinsExpected > 0;
        if (gpuFftOwnsSpectrum) {
            if (!m_loggedLegacySpectrumSuppressed) {
                if (panadapterDebugLoggingEnabled()) {
                    qint64 const lastGpuReadbackAgeMs =
                        m_lastGpuFftReadbackMs > 0 ? nowMs - m_lastGpuFftReadbackMs : -1;
                    qInfo().noquote()
                        << "[PANDBG] Panadapter legacy normalized spectrum suppressed"
                        << "reason=GPU_FFT_active_resolution_lock"
                        << "legacy_bins=" << normValues.size()
                        << "expected_bins=" << m_gpuFftUiBinsExpected
                        << "last_gpu_readback_ms_ago=" << lastGpuReadbackAgeMs;
                }
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
    bool openGlGpuFftEnabled =
        qEnvironmentVariableIsSet("DECODIUM_ENABLE_EXPERIMENTAL_OPENGL_GPU_PANADAPTER_FFT");
    if (api == QSGRendererInterface::OpenGL && !openGlGpuFftEnabled) {
        QCoreApplication* app = QCoreApplication::instance();
        QObject* bridgeObject = app
            ? app->property("decodiumBridge").value<QObject*>()
            : nullptr;
        auto* bridge = qobject_cast<DecodiumBridge*>(bridgeObject);
        openGlGpuFftEnabled = bridge && bridge->openGlGpuPanadapterFftEnabled();
    }
    if (api == QSGRendererInterface::OpenGL && !openGlGpuFftEnabled) {
        if (reason)
            *reason = QStringLiteral(
                "OpenGL GPU FFT disabled; enable Advanced > OpenGL GPU FFT or set DECODIUM_ENABLE_EXPERIMENTAL_OPENGL_GPU_PANADAPTER_FFT=1");
        return false;
    }
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
    {
        QMutexLocker lock(&m_mutex);
        if (m_externalSpectrumActive)
            return true;
    }

    QString reason;
    if (!gpuFftSupported(&reason)) {
        if (!m_loggedGpuFftRejected) {
            qWarning().noquote()
                << "[PANDBG] Panadapter visual FFT GPU path rejected"
                << "reason=" << reason;
            m_loggedGpuFftRejected = true;
        }
        if (reason == QStringLiteral("no QQuickWindow yet")
            || reason == QStringLiteral("no QSGRendererInterface")) {
            return true;
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
    bool const shouldEmitUpdate = consumeUpdateBudgetLocked();
    lock.unlock();

    if (!m_loggedGpuFftAccepted) {
        qInfo().noquote()
            << "[PANDBG] Panadapter visual FFT path active"
            << "engine=RHI_compute_gpu"
            << "algorithm=single_pass_dft_recurrence_4096"
            << "fallback=FFTW_CPU_on_failure";
        m_loggedGpuFftAccepted = true;
    }
    if (shouldEmitUpdate) {
        update();
    }
    return true;
}

// 1.0.569+ - stesse condizioni della guardia in testa ad addPcmFrameI16: se
// sono false l'item ritorna "accettato" senza fare nulla, quindi il chiamante
// deve poterlo escludere dal conteggio delle accettazioni.
bool PanadapterItem::isFrameConsumer() const
{
    return isVisible() && window() && width() > 0.0 && height() > 0.0;
}

bool PanadapterItem::addPcmFrameI16(const short* ring,
                                    int ringSize,
                                    int ringStart,
                                    int firstChunk,
                                    int usableSamples,
                                    int nfa,
                                    int nfb,
                                    float freqMinHz,
                                    float freqMaxHz,
                                    quint64 serial)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_externalSpectrumActive)
            return true;
    }

    if (!isVisible() || !window() || width() <= 0.0 || height() <= 0.0)
        return true;

    QString reason;
    if (!gpuFftSupported(&reason)) {
        if (!m_loggedGpuFftRejected) {
            qWarning().noquote()
                << "[PANDBG] Panadapter visual FFT GPU path rejected"
                << "reason=" << reason;
            m_loggedGpuFftRejected = true;
        }
        if (reason == QStringLiteral("no QQuickWindow yet")
            || reason == QStringLiteral("no QSGRendererInterface")) {
            return true;
        }
        return false;
    }

    constexpr int N = 4096;
    if (!ring
        || ringSize <= 0
        || ringStart < 0
        || ringStart >= ringSize
        || usableSamples <= 0
        || firstChunk < 0
        || firstChunk > usableSamples
        || nfb <= nfa
        || freqMaxHz <= freqMinHz) {
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

    int const copySamples = qMin(usableSamples, N);
    int const chunk0 = qMin(firstChunk, copySamples);
    int const chunk1 = copySamples - chunk0;
    if (ringStart + chunk0 > ringSize || chunk1 > ringSize)
        return false;

    double sumSq = 0.0;
    float samplePeak = 0.0f;
    auto accumulateStats = [&](const short* src, int count) {
        for (int i = 0; i < count; ++i) {
            float const v = static_cast<float>(src[i]);
            samplePeak = qMax(samplePeak, static_cast<float>(std::abs(static_cast<int>(src[i]))));
            sumSq += static_cast<double>(v) * static_cast<double>(v);
        }
    };
    accumulateStats(ring + ringStart, chunk0);
    if (chunk1 > 0)
        accumulateStats(ring, chunk1);
    float const sampleRms = static_cast<float>(std::sqrt(sumSq / static_cast<double>(N)));
    if (!m_loggedGpuFftInputStats) {
        qInfo().noquote()
            << "[GPUDBG] Panadapter visual FFT input stats"
            << "samples=" << N
            << "usable=" << usableSamples
            << "peak=" << samplePeak
            << "rms=" << sampleRms;
        m_loggedGpuFftInputStats = true;
    }

    QMutexLocker lock(&m_mutex);
    m_pendingPcmFrame.samples.resize(N);
    float* dst = m_pendingPcmFrame.samples.data();
    for (int i = 0; i < chunk0; ++i)
        dst[i] = static_cast<float>(ring[ringStart + i]);
    for (int i = 0; i < chunk1; ++i)
        dst[chunk0 + i] = static_cast<float>(ring[i]);
    if (copySamples < N)
        std::fill(dst + copySamples, dst + N, 0.0f);
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
    bool const shouldEmitUpdate = consumeUpdateBudgetLocked();
    lock.unlock();

    if (!m_loggedGpuFftAccepted) {
        qInfo().noquote()
            << "[PANDBG] Panadapter visual FFT path active"
            << "engine=RHI_compute_gpu"
            << "algorithm=single_pass_dft_recurrence_4096"
            << "fallback=FFTW_CPU_on_failure";
        m_loggedGpuFftAccepted = true;
    }
    if (!m_loggedGpuFftI16Accepted) {
        qInfo().noquote()
            << "[PANDBG] Panadapter PCM frame path active"
            << "route=C++_I16_ring"
            << "qml_bypass=1"
            << "bridge_vector_float=0"
            << "staging=float_reused";
        m_loggedGpuFftI16Accepted = true;
    }
    if (shouldEmitUpdate) {
        update();
    }
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

    int nBins = m_bins.size();
    float range = m_maxDb - m_minDb;
    if (range < 1.f) range = 1.f;

    auto binToY = [&](float db) {
        float norm = qBound(0.f, (db - m_minDb) / range, 1.f);
        return h - 1 - (int)(norm * (h - 2));
    };

    PanadapterFreqView const freqView = makePanadapterFreqView(
        static_cast<float>(m_startFreq),
        static_cast<float>(m_bandwidth),
        m_dataFreqMin,
        m_dataFreqMax,
        m_zoomFactor,
        static_cast<float>(m_panHz));
    float const dataRange = freqView.dataRange;
    float const viewRange = freqView.viewRange;
    float const viewStart = freqView.viewStart;
    bool const gpuSpectrumGraph = spectrumGraphSupported() && !freqView.clipsData;
    // fToX: mappa freq → pixel usando le coordinate dei bin reali
    auto fToX = [&](float f) -> int { return (int)((f - viewStart) * w / viewRange); };
    // Anche i BIN devono usare questa stessa mappatura.
    // bin[i] corrisponde al range dati reale m_dataFreqMin..m_dataFreqMax,
    // mentre la view può essere un sotto-range custom 0..3200.

    // ── Griglia dB orizzontale (SmartSDR: 5 livelli, labels dBm) ──────────
    const int DB_STEPS = 5;
    p.setFont(panadapterMonoFont(8));
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
    p.setFont(panadapterMonoFont(9, QFont::Bold));
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
    if (pRight > pLeft) { p.fillRect(pLeft, 0, pRight - pLeft, h, QColor(80, 110, 120, 22)); }

    if (m_spectrum3d && !gpuSpectrumGraph) {
        // The Qt software scene graph cannot reliably render the colored
        // geometry used by the normal 3D path. Rasterize the same history into
        // the spectrum image instead; D3D/OpenGL continue to use QSG nodes.
        int const histBins = m_waterfallRawBinsWidth;
        int const histRows = histBins > 0 ? m_waterfallDbRows.size() / histBins : 0;
        int const availableRows = qMin(m_wfWriteRow, histRows);
        if (histBins > 1 && availableRows >= 2 && !freqView.clipsData) {
            int const traces = qBound(2, qMin(m_spectrum3dTraces, availableRows), 48);
            int const pointCount = qBound(2, qMin(240, qMax(2, w / 3)), 240);
            float const floorDb = m_minDb + m_spectrum3dFloorDepth;
            float const floorRange = qMax(1.0f, m_spectrum3dSpanDb);
            float const nearY = static_cast<float>(h) - 1.0f;
            float const farY = static_cast<float>(h) * 0.28f;
            float const bandH = nearY - farY;
            float const shrinkMax = 0.32f;
            float const ridgeMax = bandH * 0.55f;

            p.setPen(QPen(QColor(35, 48, 60, 115), 1.0));
            float const farScale = 1.0f - shrinkMax;
            float const farOffset = static_cast<float>(w) * shrinkMax * 0.5f;
            for (int g = 0; g < 9; ++g) {
                float const fx = static_cast<float>(g) / 8.0f;
                p.drawLine(QPointF(fx * static_cast<float>(w), nearY),
                           QPointF(farOffset + fx * static_cast<float>(w) * farScale, farY));
            }

            QVector<QPointF> ridgePoints(pointCount);
            QVector<float> ridgeLevels(pointCount);
            for (int t = traces - 1; t >= 0; --t) {
                float const depth = traces > 1
                    ? static_cast<float>(t) / static_cast<float>(traces - 1)
                    : 0.0f;
                float const perspective = 1.0f - std::pow(1.0f - depth, 2.0f);
                float const scaleX = 1.0f - shrinkMax * perspective;
                float const offsetX = static_cast<float>(w) * shrinkMax * perspective * 0.5f;
                float const baseY = nearY - perspective * bandH;
                float const ridgeHeight = ridgeMax * (1.0f - 0.55f * perspective);

                int row = (m_wfWriteRow - 1 - t) % histRows;
                if (row < 0)
                    row += histRows;
                int rowPrev = (row - 1 + histRows) % histRows;
                int const rowNext = (row + 1) % histRows;
                float const* src = m_waterfallDbRows.constData()
                    + static_cast<qsizetype>(row) * histBins;
                float const* srcPrev = m_waterfallDbRows.constData()
                    + static_cast<qsizetype>(rowPrev) * histBins;
                float const* srcNext = m_waterfallDbRows.constData()
                    + static_cast<qsizetype>(rowNext) * histBins;

                float maxLevel = 0.0f;
                for (int i = 0; i < pointCount; ++i) {
                    int const x = i * (w - 1) / qMax(1, pointCount - 1);
                    float const pixFreq = freqView.viewStart
                        + static_cast<float>(x) * freqView.viewRange / static_cast<float>(w);
                    int bin = static_cast<int>((pixFreq - m_dataFreqMin) / freqView.dataRange
                                               * static_cast<float>(histBins));
                    bin = qBound(0, bin, histBins - 1);
                    float const a0 = src[bin];
                    float const a1 = srcPrev[bin];
                    float const a2 = srcNext[bin];
                    float const db = qMax(qMin(a0, a1), qMin(qMax(a0, a1), a2));
                    float const raw = qBound(0.0f, (db - floorDb) / floorRange, 1.0f);
                    float const level = raw * raw * (3.0f - 2.0f * raw);
                    ridgeLevels[i] = level;
                    maxLevel = qMax(maxLevel, level);
                    ridgePoints[i] = QPointF(offsetX + static_cast<float>(x) * scaleX,
                                             baseY - level * ridgeHeight);
                }

                QPainterPath fillPath;
                fillPath.moveTo(ridgePoints.constFirst());
                for (int i = 1; i < pointCount; ++i)
                    fillPath.lineTo(ridgePoints.at(i));
                fillPath.lineTo(ridgePoints.constLast().x(), static_cast<float>(h));
                fillPath.lineTo(ridgePoints.constFirst().x(), static_cast<float>(h));
                fillPath.closeSubpath();
                QColor topColor = QColor::fromRgb(wfColor(qMax(0.18f, maxLevel * 0.72f))).darker(180);
                QColor bottomColor = topColor.darker(420);
                QLinearGradient fillGradient(0.0, baseY - ridgeHeight, 0.0, static_cast<float>(h));
                fillGradient.setColorAt(0.0, topColor);
                fillGradient.setColorAt(1.0, bottomColor);
                p.fillPath(fillPath, fillGradient);

                constexpr int colorBuckets = 8;
                QVector<QPainterPath> ridgePaths(colorBuckets);
                for (int i = 1; i < pointCount; ++i) {
                    float const level = 0.5f * (ridgeLevels.at(i - 1) + ridgeLevels.at(i));
                    int const bucket = qBound(0, static_cast<int>(level * colorBuckets), colorBuckets - 1);
                    ridgePaths[bucket].moveTo(ridgePoints.at(i - 1));
                    ridgePaths[bucket].lineTo(ridgePoints.at(i));
                }
                for (int bucket = 0; bucket < colorBuckets; ++bucket) {
                    if (ridgePaths.at(bucket).isEmpty())
                        continue;
                    float const level = (static_cast<float>(bucket) + 0.5f)
                        / static_cast<float>(colorBuckets);
                    QColor color = QColor::fromRgb(wfColor(level));
                    color.setAlphaF(1.0f - 0.70f * perspective);
                    p.setPen(QPen(color, t == 0 ? 2.0 : 1.0));
                    p.drawPath(ridgePaths.at(bucket));
                }
            }
        }
    } else if (!gpuSpectrumGraph) {
        // ── Path spettro ───────────────────────────────────────────────────────
        QPainterPath fillPath, linePath;
        bool inSegment = false;
        auto closeSegment = [&](int x) {
            if (!inSegment)
                return;
            fillPath.lineTo(qBound(0, x, w), h);
            fillPath.closeSubpath();
            inSegment = false;
        };
        for (int x = 0; x < w; ++x) {
            // Mappa pixel x → frequenza → bin usando il range effettivo dei dati FFT
            float pixFreq = viewStart + (float)x * viewRange / w;
            if (pixFreq < m_dataFreqMin || pixFreq > m_dataFreqMax) {
                closeSegment(x);
                continue;
            }
            int bin = (int)((pixFreq - m_dataFreqMin) / dataRange * nBins);
            bin = qBound(0, bin, nBins - 1);
            int y = binToY(m_bins[bin]);
            if (!inSegment) {
                fillPath.moveTo(x, h);
                fillPath.lineTo(x, y);
                linePath.moveTo(x, y);
                inSegment = true;
            } else {
                fillPath.lineTo(x, y);
                linePath.lineTo(x, y);
            }
        }
        closeSegment(w);

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
            bool first = true;
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
            if (!first)
                p.drawPath(pkPath);
        }
    }

    int txX = fToX(m_txFreq);
    // 1.0.340: marker TX sempre visibile quando in range (rimosso il gate
    // m_txFreq!=m_rxFreq che lo nascondeva quando TX coincideva con RX -> dava
    // l'impressione che il click sinistro non impostasse la freq TX).
    bool const txVisible = txX >= 0 && txX < w;
    auto drawMarkerLabel = [&](int markerX, int preferredCenterY, const QString& text, const QColor& accent) {
        QFont labelFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        labelFont.setPointSize(9);
        labelFont.setBold(true);
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
        QFont labelFont = panadapterMonoFont(m_labelFontSize, m_labelBold ? QFont::Bold : QFont::Normal);
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
            if (call.isEmpty() || freq < 100 || freq > 5000) continue;

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
        QFont clusterFont = panadapterMonoFont(m_labelFontSize, QFont::Bold);
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
            if (call.isEmpty() || freq < 100 || freq > 5000) continue;
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
        p.setFont(panadapterMonoFont(8));
        p.setPen(QColor(100, 100, 100));
        p.drawText(w - 100, h - 3,
                   QString("NF:%1dB").arg((int)m_measuredFloor));
    }
}

// Spettro 3D a tracce impilate: la storia gia' conservata per la cascata
// (m_waterfallDbRows, buffer circolare di dB grezzi) viene ridisegnata in
// prospettiva, dalla traccia piu' lontana alla piu' vicina. Nessun dato nuovo
// viene raccolto e nessuno shader viene aggiunto: sono polilinee nello stesso
// meccanismo a nodi del 2D.
//
// Il costo e' tracce x punti, ed e' proprio quello che strozza i PC modesti,
// quindi i punti per traccia sono decimati a un tetto fisso e la funzione e'
// raggiungibile solo con l'opzione accesa a mano.
void PanadapterItem::updateSpectrum3dNodes(QSGNode* spectrumRoot, int w, int h)
{
    if (!spectrumRoot || w <= 1 || h <= 1) {
        removeSpectrumGraphNodes(spectrumRoot);
        return;
    }

    int const histBins = m_waterfallRawBinsWidth;
    int const histRows = histBins > 0 ? m_waterfallDbRows.size() / histBins : 0;
    if (histBins <= 1 || histRows < 2) {
        // Storia non ancora disponibile (avvio, cambio banda): meglio niente che
        // una superficie fatta di valori sentinella.
        removeSpectrumGraphNodes(spectrumRoot);
        return;
    }

    PanadapterFreqView const freqView = makePanadapterFreqView(
        static_cast<float>(m_startFreq),
        static_cast<float>(m_bandwidth),
        m_dataFreqMin,
        m_dataFreqMax,
        m_zoomFactor,
        static_cast<float>(m_panHz));
    if (freqView.clipsData) {
        removeSpectrumGraphNodes(spectrumRoot);
        return;
    }

    int const traces = qBound(2, qMin(m_spectrum3dTraces, histRows), 128);

    // Decimazione orizzontale: un punto ogni 'step' pixel, con un tetto di punti
    // per traccia. Su 1300 pixel e 48 tracce si passa da ~62k vertici a ~15k.
    int const maxPointsPerTrace = 320;
    int const step = qMax(1, (w + maxPointsPerTrace - 1) / maxPointsPerTrace);
    int const points = w / step + 1;
    if (points < 2) {
        removeSpectrumGraphNodes(spectrumRoot);
        return;
    }

    // L'altezza della cresta si misura sopra il fondo scelto dall'utente, non
    // sopra il minimo assoluto: cosi' il rumore resta piatto e spiccano i segnali.
    float const floorDb = m_minDb + m_spectrum3dFloorDepth;
    float floorRange = m_spectrum3dSpanDb;
    if (floorRange < 1.f)
        floorRange = 1.f;

    float const nearY = static_cast<float>(h) - 1.0f;
    float const farY = static_cast<float>(h) * 0.28f;
    float const bandH = nearY - farY;
    float const shrinkMax = 0.32f;          // restringimento della traccia lontana
    float const ridgeMax = bandH * 0.55f;   // cresta massima della traccia vicina
    // Spaziatura NON lineare: le tracce vicine si distanziano, quelle lontane si
    // addensano verso l'orizzonte. Con una spaziatura lineare il risultato legge
    // come una rete di linee invece che come una superficie che si allontana.
    float const perspectiveExponent = 2.0f;

    int childIndex = 1;

    // Reticolo di frequenza in prospettiva: linee che convergono verso il punto
    // di fuga. E' il segnale visivo che distingue uno spazio tridimensionale da
    // una pila di curve. Va disegnato PRIMA delle tracce: sotto viene coperto
    // dai riempimenti, e resta visibile solo nel cielo sopra i profili, che e'
    // esattamente dove serve.
    {
        int const gridLines = 9;
        float const farScale = 1.0f - shrinkMax;
        float const farOffset = static_cast<float>(w) * shrinkMax * 0.5f;
        if (auto* gridNode = ensureVertexColorNode(spectrumRoot, childIndex,
                                                   gridLines * 2,
                                                   QSGGeometry::DrawLines)) {
            auto* gv = gridNode->geometry()->vertexDataAsColoredPoint2D();
            for (int g = 0; g < gridLines; ++g) {
                float const fx = static_cast<float>(g) / static_cast<float>(gridLines - 1);
                float const nx = fx * static_cast<float>(w);
                float const fxFar = farOffset + fx * static_cast<float>(w) * farScale;
                // Sfumatura: piu' marcato vicino, quasi spento all'orizzonte.
                gv[g * 2].set(nx, nearY, 46, 60, 74, 170);
                gv[g * 2 + 1].set(fxFar, farY, 14, 19, 24, 58);
            }
            ++childIndex;
        }
    }

    for (int t = traces - 1; t >= 0; --t) {
        float const depth = traces > 1 ? static_cast<float>(t) / static_cast<float>(traces - 1) : 0.f;
        int row = (m_wfWriteRow - 1 - t) % histRows;
        if (row < 0)
            row += histRows;
        float const* src = m_waterfallDbRows.constData() + static_cast<qsizetype>(row) * histBins;
        // Reiezione degli impulsi: la mediana temporale su tre righe toglie i
        // picchi isolati di un solo fotogramma, che altrimenti costellano la
        // superficie di spuntoni e la fanno sembrare sporca.
        int rowPrev = (row - 1) % histRows;
        if (rowPrev < 0) rowPrev += histRows;
        int const rowNext = (row + 1) % histRows;
        float const* srcPrev = m_waterfallDbRows.constData() + static_cast<qsizetype>(rowPrev) * histBins;
        float const* srcNext = m_waterfallDbRows.constData() + static_cast<qsizetype>(rowNext) * histBins;

        // Due nodi per traccia: prima il RIEMPIMENTO, poi la cresta luminosa.
        // Il riempimento e' cio' che mancava perche' il disegno leggesse come una
        // superficie invece che come una rete di linee: essendo disegnato dopo, il
        // riempimento della traccia vicina COPRE quelle dietro, e delle lontane
        // resta visibile solo cio' che sporge sopra il profilo davanti. E' la
        // rimozione delle superfici nascoste, ottenuta con l'ordine di disegno.
        auto* fillNode = ensureVertexColorNode(spectrumRoot, childIndex, points * 2,
                                               QSGGeometry::DrawTriangleStrip);
        if (!fillNode)
            break;
        ++childIndex;
        auto* fillVertices = fillNode->geometry()->vertexDataAsColoredPoint2D();

        auto* node = ensureVertexColorNode(spectrumRoot, childIndex, points,
                                           QSGGeometry::DrawLineStrip);
        if (!node)
            break;
        ++childIndex;

        // La traccia piu' recente e' quella che l'operatore sta guardando: piu'
        // spessa, cosi' si stacca dalla storia che le sta dietro.
        node->geometry()->setLineWidth(t == 0 ? 2.0f : 1.0f);

        auto* vertices = node->geometry()->vertexDataAsColoredPoint2D();
        float const persp = 1.0f - std::pow(1.0f - depth, perspectiveExponent);
        float const scaleX = 1.0f - shrinkMax * persp;
        float const offsetX = static_cast<float>(w) * shrinkMax * persp * 0.5f;
        float const baseY = nearY - persp * bandH;
        float const ridge = ridgeMax * (1.0f - 0.55f * persp);
        // Le tracce lontane sbiadiscono: da' profondita' e toglie confusione
        // dove le creste si sovrappongono.
        int const alpha = static_cast<int>(255.0f * (1.0f - 0.72f * persp));

        for (int i = 0; i < points; ++i) {
            int const x = qMin(i * step, w - 1);
            float const pixFreq = freqView.viewStart
                + static_cast<float>(x) * freqView.viewRange / static_cast<float>(w);
            float const clampedFreq = qBound(m_dataFreqMin, pixFreq, m_dataFreqMax);
            int bin = static_cast<int>((clampedFreq - m_dataFreqMin) / freqView.dataRange
                                       * static_cast<float>(histBins));
            bin = qBound(0, bin, histBins - 1);

            float const a0 = src[bin];
            float const a1 = srcPrev[bin];
            float const a2 = srcNext[bin];
            float const db = qMax(qMin(a0, a1), qMin(qMax(a0, a1), a2)); // mediana
            float raw = qBound(0.f, (db - floorDb) / floorRange, 1.f);
            // Leggera curva: appiattisce il rumore e lascia svettare i segnali,
            // invece di una superficie uniformemente mossa.
            float const norm = raw * raw * (3.0f - 2.0f * raw);
            QRgb const rgb = wfColor(norm);
            // QSGVertexColorMaterial vuole il colore gia' premoltiplicato per alfa.
            auto const pm = [alpha](int c) {
                return static_cast<uchar>(c * alpha / 255);
            };
            float const px = offsetX + static_cast<float>(x) * scaleX;
            float const py = baseY - norm * ridge;
            vertices[i].set(px, py,
                            pm(qRed(rgb)), pm(qGreen(rgb)), pm(qBlue(rgb)),
                            static_cast<uchar>(alpha));

            // Il riempimento scende fino al fondo del pannello ed e' OPACO:
            // solo cosi' nasconde davvero le tracce dietro. Sotto la cresta
            // sfuma verso il nero, che da' rilievo al profilo luminoso sopra.
            auto const shade = [](int c, float k) {
                return static_cast<uchar>(qBound(0.f, static_cast<float>(c) * k, 255.f));
            };
            float const topShade = 0.30f * (1.0f - 0.45f * persp);
            fillVertices[i * 2].set(px, py,
                                    shade(qRed(rgb), topShade),
                                    shade(qGreen(rgb), topShade),
                                    shade(qBlue(rgb), topShade), 255);
            fillVertices[i * 2 + 1].set(px, static_cast<float>(h),
                                        shade(qRed(rgb), 0.06f),
                                        shade(qGreen(rgb), 0.06f),
                                        shade(qBlue(rgb), 0.06f), 255);
        }
    }

    // Rimuove SOLO le tracce in eccesso quando il loro numero cala, MAI il nodo
    // dell'overlay: quello porta griglia, etichette e contrassegni RX/TX, e viene
    // rimesso in coda a ogni fotogramma per restare sopra la superficie.
    // Cancellandolo qui insieme al resto spariva dallo schermo.
    QSGNode* extra = sceneGraphChildAt(spectrumRoot, childIndex);
    while (extra) {
        QSGNode* const next = extra->nextSibling();
        if (!dynamic_cast<PanadapterSpectrumOverlayNode*>(extra)) {
            spectrumRoot->removeChildNode(extra);
            delete extra;
        }
        extra = next;
    }
}

void PanadapterItem::updateSpectrum3dGpuNodes(QSGNode* spectrumRoot, int w, int h)
{
#if defined(DECODIUM_QT_RHI_TEXTURE_UPLOAD) \
    && defined(DECODIUM_GPU_PANADAPTER_FFT_QSB) \
    && defined(DECODIUM_GPU_PANADAPTER_SPECTRUM_3D_QSB)
    if (!spectrumRoot || w <= 1 || h <= 1
        || !spectrum3dGpuSupported()
        || !m_gpuFft
        || !m_gpuFft->directTexturePathActive
        || !m_gpuFft->directWaterfallTexture
        || !m_gpuFft->directRowParamsTexture) {
        updateSpectrum3dNodes(spectrumRoot, w, h);
        return;
    }

    int const historyRows = m_gpuFft->directRows;
    if (historyRows < 2 || m_gpuFft->directWaterfallSize.width() < 2) {
        updateSpectrum3dNodes(spectrumRoot, w, h);
        return;
    }

    int const traces = qBound(2, qMin(m_spectrum3dTraces, historyRows), 128);
    int const points = qBound(2, qMin(w, 320), 320);
    int const verticesPerTrace = points * 2;
    int childIndex = 1;

    // The perspective grid has only 18 static vertices.  Keeping it as a QSG
    // colour node avoids an extra shader pass while all spectrum history,
    // dB conversion and ridge projection remain on the GPU.
    {
        int constexpr gridLines = 9;
        float constexpr shrinkMax = 0.32f;
        float const nearY = static_cast<float>(h) - 1.0f;
        float const farY = static_cast<float>(h) * 0.28f;
        float const farScale = 1.0f - shrinkMax;
        float const farOffset = static_cast<float>(w) * shrinkMax * 0.5f;
        if (auto* gridNode = ensureVertexColorNode(spectrumRoot,
                                                   childIndex,
                                                   gridLines * 2,
                                                   QSGGeometry::DrawLines)) {
            auto* vertices = gridNode->geometry()->vertexDataAsColoredPoint2D();
            for (int line = 0; line < gridLines; ++line) {
                float const fx = static_cast<float>(line) / static_cast<float>(gridLines - 1);
                float const nearX = fx * static_cast<float>(w);
                float const farX = farOffset + nearX * farScale;
                vertices[line * 2].set(nearX, nearY, 46, 60, 74, 170);
                vertices[line * 2 + 1].set(farX, farY, 14, 19, 24, 58);
            }
            ++childIndex;
        }
    }

    QSGNode* child = sceneGraphChildAt(spectrumRoot, childIndex);
    auto* node = dynamic_cast<PanadapterSpectrum3dNode*>(child);
    auto* material = node
        ? dynamic_cast<PanadapterSpectrum3dMaterial*>(node->sharedMaterial)
        : nullptr;
    if (child && (!node || !material)) {
        removeSceneGraphChildrenFrom(spectrumRoot, child);
        child = nullptr;
        node = nullptr;
        material = nullptr;
    }

    if (!node) {
        node = new PanadapterSpectrum3dNode();
        material = new PanadapterSpectrum3dMaterial();
        node->sharedMaterial = material;
        spectrumRoot->appendChildNode(node);
    }

    if (auto* paletteTexture = dynamic_cast<DecodiumRhiImageTexture*>(material->paletteTexture);
        paletteTexture && paletteTexture->failed()) {
        bool const firstFailure = !m_spectrum3dGpuBlocked.exchange(true, std::memory_order_acq_rel);
        if (firstFailure) {
            qWarning().noquote()
                << "[GPUDBG] Panadapter 3D GPU palette texture failed"
                << "fallback=FFTW_CPU_history_QSG_geometry";
        }
        updateSpectrum3dNodes(spectrumRoot, w, h);
        return;
    }

    bool const rebuildMesh = node->meshSize != QSize(w, h)
        || node->meshTraces != traces
        || node->meshPoints != points;
    if (rebuildMesh) {
        while (QSGNode* traceChild = node->firstChild()) {
            node->removeChildNode(traceChild);
            delete traceChild;
        }

        float const denominator = static_cast<float>(qMax(1, points - 1));
        // Keep each historical surface in its own static node.  A single
        // triangle strip let overlapping primitives in the same GPU draw
        // collapse into the outer envelope on Metal, hiding all inner ridges.
        // Separate ordered draws reproduce the CPU painter order while the
        // FFT history, normalisation and projection still remain on the GPU.
        for (int trace = traces - 1; trace >= 0; --trace) {
            float const depth = traces > 1
                ? static_cast<float>(trace) / static_cast<float>(traces - 1)
                : 0.0f;

            auto* traceNode = new QSGGeometryNode();
            auto* geometry = new QSGGeometry(waterfallTexturedPoint2DAttributes(),
                                             verticesPerTrace);
            geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
            geometry->setVertexDataPattern(QSGGeometry::StaticPattern);
            auto* vertices = geometry->vertexDataAsTexturedPoint2D();
            for (int point = 0; point < points; ++point) {
                float const x = static_cast<float>(point) * static_cast<float>(w - 1) / denominator;
                vertices[point * 2].set(x, 0.0f, depth, 0.0f);
                vertices[point * 2 + 1].set(x, static_cast<float>(h), depth, 1.0f);
            }

            traceNode->setGeometry(geometry);
            traceNode->setFlag(QSGNode::OwnsGeometry);
            traceNode->setMaterial(material);
            node->appendChildNode(traceNode);
        }
        node->meshSize = QSize(w, h);
        node->meshTraces = traces;
        node->meshPoints = points;
    }

    auto replaceExternalTexture = [](QSGTexture*& slot,
                                     QRhiTexture* texture,
                                     QSize const& size) {
        auto* external = dynamic_cast<DecodiumExternalRhiTexture*>(slot);
        if (!external || external->rhiTexture() != texture || external->textureSize() != size) {
            delete slot;
            slot = new DecodiumExternalRhiTexture(texture, size, false);
        }
        slot->setFiltering(QSGTexture::Nearest);
    };
    replaceExternalTexture(material->historyTexture,
                           m_gpuFft->directWaterfallTexture,
                           m_gpuFft->directWaterfallSize);
    replaceExternalTexture(material->rowParamsTexture,
                           m_gpuFft->directRowParamsTexture,
                           m_gpuFft->directRowParamsSize);

    if (material->paletteGeneration != m_paletteGeneration || !material->paletteTexture) {
        QImage paletteImage(256, 1, QImage::Format_RGBA8888);
        uchar* dst = paletteImage.scanLine(0);
        for (int x = 0; x < 256; ++x) {
            QColor const color = QColor::fromRgb(m_palette.value(x, qRgb(0, 0, 0)));
            int const offset = x * 4;
            dst[offset + 0] = static_cast<uchar>(color.red());
            dst[offset + 1] = static_cast<uchar>(color.green());
            dst[offset + 2] = static_cast<uchar>(color.blue());
            dst[offset + 3] = 255;
        }
        material->retireTexture(material->paletteTexture);
        auto* paletteTexture = new DecodiumRhiImageTexture(false);
        paletteTexture->setFiltering(QSGTexture::Linear);
        paletteTexture->uploadFullRgbaImage(paletteImage, false);
        material->paletteTexture = paletteTexture;
        material->paletteGeneration = m_paletteGeneration;
    }

    material->geometryParams[0] = static_cast<float>(w);
    material->geometryParams[1] = static_cast<float>(h);
    material->geometryParams[2] = 2.0f;
    material->geometryParams[3] = 5.0f;
    material->historyParams[0] = static_cast<float>(m_gpuFft->directWriteRow);
    material->historyParams[1] = static_cast<float>(historyRows);
    material->historyParams[2] = static_cast<float>(traces);
    material->historyParams[3] = m_spectrum3dFloorDepth;
    material->perspectiveParams[0] = 0.28f;
    material->perspectiveParams[1] = 0.32f;
    material->perspectiveParams[2] = 0.55f;
    material->perspectiveParams[3] = 2.0f;

    PanadapterFreqView const freqView = makePanadapterFreqView(
        static_cast<float>(m_startFreq),
        static_cast<float>(m_bandwidth),
        m_dataFreqMin,
        m_dataFreqMax,
        m_zoomFactor,
        static_cast<float>(m_panHz));
    material->xParams[0] = freqView.viewRange / freqView.dataRange;
    material->xParams[1] = (freqView.viewStart - freqView.dataStart) / freqView.dataRange;
    material->xParams[2] = 1.0f;
    // The legacy/CPU 3D view normally spans about 80 dB.  The direct 2D GPU
    // auto-range is intentionally tighter (often 45 dB), but that would hide
    // most historical ridges if reused unchanged by this shader.
    material->xParams[3] = qMax(1.0f, m_spectrum3dSpanDb);

    for (QSGNode* traceChild = node->firstChild(); traceChild; traceChild = traceChild->nextSibling()) {
        if (auto* traceNode = dynamic_cast<QSGGeometryNode*>(traceChild))
            traceNode->markDirty(QSGNode::DirtyMaterial);
    }
    QSGNode* extra = node->nextSibling();
    while (extra) {
        QSGNode* const next = extra->nextSibling();
        if (!dynamic_cast<PanadapterSpectrumOverlayNode*>(extra)) {
            spectrumRoot->removeChildNode(extra);
            delete extra;
        }
        extra = next;
    }

    if (!m_loggedGpuSpectrum3d) {
        m_loggedGpuSpectrum3d = true;
        qInfo().noquote()
            << "[GPUDBG] Panadapter 3D GPU history path active"
            << "api=" << waterfallGraphicsApiName(window()->rendererInterface()->graphicsApi())
            << "traces=" << traces
            << "points=" << points
            << "ordered_draws=" << traces
            << "history_texture="
            << QStringLiteral("%1x%2")
                   .arg(m_gpuFft->directWaterfallSize.width())
                   .arg(m_gpuFft->directWaterfallSize.height())
            << "readback=off"
            << "fallback=FFTW_CPU_history_QSG_geometry";
    }
#else
    updateSpectrum3dNodes(spectrumRoot, w, h);
#endif
}

void PanadapterItem::removeSpectrumGraphNodes(QSGNode* spectrumRoot)
{
    if (!spectrumRoot)
        return;
    QSGNode* firstGraphChild = sceneGraphChildAt(spectrumRoot, 1);
    removeSceneGraphChildrenFrom(spectrumRoot, firstGraphChild);
}

void PanadapterItem::rebuildSpectrumOverlayImage(int w, int h, bool gpuDirectReady)
{
    if (w <= 1 || h <= 1)
        return;
    qint64 const overlayStartUs = monotonicUs();
    int renderedDecodeLabels = 0;
    int renderedClusterLabels = 0;
    qreal const dpr = panadapterOverlayDevicePixelRatio(window());
    QSize const textureSize = panadapterOverlayTextureSize(w, h, dpr);

    if (m_spectrumOverlayImage.size() != textureSize
        || m_spectrumOverlayImage.format() != QImage::Format_RGBA8888_Premultiplied) {
        m_spectrumOverlayImage = QImage(textureSize, QImage::Format_RGBA8888_Premultiplied);
    }
    m_spectrumOverlayImage.fill(Qt::transparent);
    m_decodeHitRects.clear();
    m_clusterHitRects.clear();

    QPainter p(&m_spectrumOverlayImage);
    p.scale(dpr, dpr);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHint(QPainter::TextAntialiasing, false);

    float const displayMinDb = gpuDirectReady ? m_gpuDirectDisplayMinDb : m_minDb;
    float const displayMaxDb = gpuDirectReady ? m_gpuDirectDisplayMaxDb : m_maxDb;
    float range = displayMaxDb - displayMinDb;
    if (range < 1.0f)
        range = 1.0f;

    float const baseStart = (m_bandwidth > 0) ? static_cast<float>(m_startFreq) : m_dataFreqMin;
    float const baseEnd = (m_bandwidth > 0) ? static_cast<float>(m_startFreq + m_bandwidth) : m_dataFreqMax;
    float viewportRange = baseEnd - baseStart;
    if (viewportRange <= 0.0f)
        viewportRange = 1.0f;
    float const viewRange = viewportRange / qMax(1.0f, m_zoomFactor);
    float const viewCenter = baseStart + viewportRange * 0.5f + m_panHz;
    float const viewStart = viewCenter - viewRange * 0.5f;
    auto fToX = [&](float f) -> int {
        return static_cast<int>((f - viewStart) * static_cast<float>(w) / viewRange);
    };
    auto clampInt = [](int value, int lo, int hi) -> int {
        return qBound(lo, value, qMax(lo, hi));
    };

    int const rxX = fToX(static_cast<float>(m_rxFreq));
    int const rxFilterHz = 300;
    int pLeft = fToX(static_cast<float>(m_rxFreq - rxFilterHz / 2));
    int pRight = fToX(static_cast<float>(m_rxFreq + rxFilterHz / 2));
    pLeft = qBound(0, pLeft, w);
    pRight = qBound(0, pRight, w);
    if (pRight > pLeft)
        p.fillRect(pLeft, 0, pRight - pLeft, h, QColor(80, 110, 120, 22));

    p.setFont(panadapterMonoFont(8));
    constexpr int kDbSteps = 5;
    for (int step = 0; step <= kDbSteps; ++step) {
        float const norm = static_cast<float>(step) / static_cast<float>(kDbSteps);
        int const gy = h - 1 - static_cast<int>(norm * static_cast<float>(qMax(1, h - 16)));
        p.setPen(QPen(QColor(38, 38, 38), 1));
        p.drawLine(0, gy, w, gy);
        drawCrispOverlayText(p,
                             2,
                             qMax(8, gy - 1),
                             QString::number(static_cast<int>(std::round(displayMinDb + norm * range))),
                             QColor(190, 190, 190));
    }

    // Keep roughly ten readable grid labels at every scale. The old fixed
    // 500 Hz step generated almost two thousand overlapping labels across a
    // 960 kHz RTL-SDR view.
    const double desiredStep = qMax(1.0, static_cast<double>(viewRange) / 10.0);
    const double stepMagnitude = std::pow(10.0, std::floor(std::log10(desiredStep)));
    const double normalizedStep = desiredStep / stepMagnitude;
    const double niceMultiplier = normalizedStep <= 1.0 ? 1.0
        : (normalizedStep <= 2.0 ? 2.0 : (normalizedStep <= 5.0 ? 5.0 : 10.0));
    int const freqStep = qMax(1, qRound(niceMultiplier * stepMagnitude));
    p.setFont(panadapterMonoFont(9, QFont::Bold));
    int const firstGrid = (static_cast<int>(std::floor(viewStart / static_cast<float>(freqStep))) + 1) * freqStep;
    for (int f = firstGrid; f < static_cast<int>(viewStart + viewRange); f += freqStep) {
        int const x = fToX(static_cast<float>(f));
        if (x < 0 || x >= w)
            continue;
        p.setPen(QPen(QColor(40, 40, 40), 1));
        // Con il 3D attivo la griglia dritta a tutta altezza attraversa il
        // paesaggio in prospettiva e rovina l'illusione: al suo posto c'e' il
        // reticolo che converge al punto di fuga. Le etichette sotto restano,
        // e restano anche i contrassegni RX/TX, che servono a operare.
        if (!m_spectrum3d)
            p.drawLine(x, 0, x, qMax(0, h - 16));
        QString label;
        if (qAbs(f) >= 1000000) {
            label = QString::number(static_cast<double>(f) / 1000000.0, 'f',
                                    viewRange < 1000000.0f ? 3 : 2);
        } else if (qAbs(f) >= 1000) {
            label = QStringLiteral("%1k").arg(static_cast<double>(f) / 1000.0, 0, 'f', 1);
        } else {
            label = QString::number(f);
        }
        QFontMetrics const fm(p.font());
        int const tx = clampInt(x - fm.horizontalAdvance(label) / 2, 2, w - fm.horizontalAdvance(label) - 2);
        drawCrispOverlayText(p, tx, h - 3, label, QColor(235, 235, 235));
    }

    if (rxX >= 0 && rxX < w) {
        p.setPen(QPen(QColor(0, 229, 255, 70), 7.0));
        p.drawLine(rxX, 0, rxX, h);
        p.setPen(QPen(QColor(0, 229, 255, 240), 3.0));
        p.drawLine(rxX, 0, rxX, h);
        p.setPen(QPen(QColor(180, 255, 255, 255), 1.0));
        p.drawLine(rxX, 0, rxX, h);
    }

    struct OverlayLabel {
        int x = 0;
        int textW = 0;
        QString text;
        QColor color;
        QString call;
        int freq = 0;
    };

    if (!m_decodeLabels.isEmpty()) {
        QFont labelFont = panadapterMonoFont(m_labelFontSize, m_labelBold ? QFont::Bold : QFont::Normal);
        p.setFont(labelFont);
        QFontMetrics const fm(labelFont);
        int const rowH = qMax(10, fm.height());
        int const topPad = 2;
        int const bottomKeepOut = 20;
        int const gap = m_labelSpacing;
        int const maxRows = qMax(1, (h - bottomKeepOut - topPad) / rowH);

        QVector<OverlayLabel> items;
        items.reserve(m_decodeLabels.size());
        for (const QVariant& v : std::as_const(m_decodeLabels)) {
            QVariantMap const d = v.toMap();
            QString const call = d.value(QStringLiteral("call")).toString();
            int const freq = d.value(QStringLiteral("freq")).toInt();
            if (call.isEmpty() || freq < 100 || freq > 5000)
                continue;
            int const x = fToX(static_cast<float>(freq));
            if (x < 0 || x >= w)
                continue;

            QString const text = call + QLatin1Char(' ') + QString::number(d.value(QStringLiteral("snr")).toInt());
            QColor color(d.value(QStringLiteral("color")).toString());
            if (!color.isValid()) {
                if (m_labelUseCustomColor)
                    color = m_labelColor;
                else if (d.value(QStringLiteral("isMyCall")).toBool())
                    color = QColor(255, 80, 80);
                else if (d.value(QStringLiteral("isCQ")).toBool())
                    color = QColor(0, 230, 100);
                else
                    color = QColor(0, 200, 255);
            }
            items.push_back({x, fm.horizontalAdvance(text), text, color, call, freq});
        }
        std::sort(items.begin(), items.end(), [](const OverlayLabel& a, const OverlayLabel& b) {
            return a.x < b.x;
        });

        QVector<int> rowRight(maxRows, -1000000);
        m_decodeHitRects.reserve(items.size());
        for (const OverlayLabel& it : std::as_const(items)) {
            int const textX = clampInt(it.x + 2, 2, w - it.textW - 2);
            int chosenRow = -1;
            for (int r = 0; r < maxRows; ++r) {
                if (textX > rowRight[r] + gap) {
                    chosenRow = r;
                    break;
                }
            }
            if (chosenRow < 0)
                continue;
            int const textY = topPad + rowH * (chosenRow + 1) - fm.descent();
            p.setPen(QPen(it.color, 1, Qt::DotLine));
            p.drawLine(it.x, 0, it.x, qMax(0, h - bottomKeepOut));
            drawCrispOverlayText(p, textX, textY, it.text, it.color);
            rowRight[chosenRow] = textX + it.textW;
            QRect const hitRect(textX - 2, textY - rowH + fm.descent(), it.textW + 4, rowH);
            m_decodeHitRects.push_back({hitRect, it.call, it.freq});
            ++renderedDecodeLabels;
        }
    }

    if (m_showDxClusterSpots && !m_dxClusterSpots.isEmpty()) {
        QFont clusterFont = panadapterMonoFont(m_labelFontSize, QFont::Bold);
        p.setFont(clusterFont);
        QFontMetrics const fm(clusterFont);
        int const rowH = qMax(10, fm.height());
        int const clusterBaseline = h - 24;
        QVector<int> takenLeft;
        takenLeft.reserve(m_dxClusterSpots.size());
        for (const QVariant& v : std::as_const(m_dxClusterSpots)) {
            QVariantMap const d = v.toMap();
            QString const call = d.value(QStringLiteral("call")).toString();
            int const freq = d.value(QStringLiteral("freq")).toInt();
            if (call.isEmpty() || freq < 100 || freq > 5000)
                continue;
            int const x = fToX(static_cast<float>(freq));
            if (x < 0 || x >= w)
                continue;
            int const textW = fm.horizontalAdvance(call);
            int const textX = clampInt(x + 2, 2, w - textW - 2);
            bool conflict = false;
            for (int taken : std::as_const(takenLeft)) {
                if (qAbs(textX - taken) < textW + m_labelSpacing + 4) {
                    conflict = true;
                    break;
                }
            }
            if (conflict)
                continue;
            takenLeft.push_back(textX);

            p.setPen(QPen(m_dxClusterSpotColor, 1, Qt::DashDotLine));
            p.drawLine(x, 0, x, qMax(0, clusterBaseline - rowH));
            QRect const labelRect(textX - 2, clusterBaseline - rowH + fm.descent(), textW + 4, rowH);
            p.fillRect(labelRect, QColor(0, 0, 0, 180));
            p.setPen(QPen(m_dxClusterSpotColor, 1));
            p.drawRect(labelRect);
            drawCrispOverlayText(p, textX, clusterBaseline, call, m_dxClusterSpotColor);
            m_clusterHitRects.push_back({labelRect.adjusted(-2, -2, 2, 2), call, freq});
            ++renderedClusterLabels;
        }
    }

    auto drawMarkerLabel = [&](int markerX, int preferredCenterY, const QString& text, const QColor& accent) {
        QFont labelFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        labelFont.setPointSize(9);
        labelFont.setBold(true);
        p.setFont(labelFont);
        QFontMetrics const fm(labelFont);
        int constexpr padX = 6;
        int constexpr padY = 3;
        int const boxW = fm.horizontalAdvance(text) + padX * 2;
        int const boxH = fm.height() + padY * 2;
        int const minY = boxH / 2 + 3;
        int const maxY = qMax(minY, h - boxH / 2 - 22);
        int const centerY = qBound(minY, preferredCenterY, maxY);
        int boxX = markerX + 8;
        if (boxX + boxW > w - 2)
            boxX = markerX - boxW - 8;
        boxX = clampInt(boxX, 2, w - boxW - 2);
        int const boxY = clampInt(centerY - boxH / 2, 2, h - boxH - 22);

        QPainterPath box;
        box.addRoundedRect(QRectF(boxX, boxY, boxW, boxH), 4, 4);
        QColor border = accent;
        border.setAlpha(210);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillPath(box, QColor(0, 0, 0, 180));
        p.setPen(QPen(border, 1));
        p.drawPath(box);
        p.setRenderHint(QPainter::Antialiasing, false);
        drawCrispOverlayText(p,
                             QRect(boxX + padX, boxY + padY, boxW - padX * 2, boxH - padY * 2),
                             Qt::AlignCenter,
                             text,
                             accent);
    };

    int const txX = fToX(static_cast<float>(m_txFreq));
    // 1.0.340: marker TX sempre visibile quando in range (rimosso il gate
    // m_txFreq!=m_rxFreq che lo nascondeva quando TX coincideva con RX -> dava
    // l'impressione che il click sinistro non impostasse la freq TX).
    bool const txVisible = txX >= 0 && txX < w;
    if (rxX >= 0 && rxX < w)
        drawMarkerLabel(rxX, h / 2 - (txVisible ? 12 : 0), QStringLiteral("RX %1").arg(m_rxFreq), QColor(0, 229, 255));

    // 1.0.341 — Marker TX magenta nel path GPU-direct overlay. Mancava: era
    // disegnato SOLO in renderSpectrum() (path CPU morto quando gpuDirectReady),
    // percio' col GPU-direct attivo il click sinistro impostava la freq TX ma il
    // marker non compariva. Replica 1:1 del blocco TX di renderSpectrum.
    if (txVisible) {
        p.setPen(QPen(QColor(255, 0, 255, 70), 7.0));    // glow esterno
        p.drawLine(txX, 0, txX, h);
        p.setPen(QPen(QColor(255, 0, 255, 240), 3.0));   // linea principale
        p.drawLine(txX, 0, txX, h);
        p.setPen(QPen(QColor(255, 200, 255, 255), 1.0)); // core brillante
        p.drawLine(txX, 0, txX, h);
        drawMarkerLabel(txX, h / 2 + 12, QStringLiteral("TX %1").arg(m_txFreq), QColor(255, 0, 255));
    }

    if (m_autoRange) {
        p.setFont(panadapterMonoFont(8));
        drawCrispOverlayText(p,
                             w - 100,
                             h - 3,
                             QStringLiteral("NF:%1dB").arg(static_cast<int>(m_measuredFloor)),
                             QColor(130, 130, 130));
    }

    m_spectrumOverlayDirty = false;
    m_spectrumOverlaySize = QSize(w, h);
    m_spectrumOverlayDisplayMinDb = displayMinDb;
    m_spectrumOverlayDisplayMaxDb = displayMaxDb;
    m_lastSpectrumOverlayRebuildMs = monotonicMs();
    recordOverlayMetric(monotonicUs() - overlayStartUs,
                        renderedDecodeLabels,
                        renderedClusterLabels,
                        QSize(w, h));
}

void PanadapterItem::updateSpectrumOverlayNode(QSGNode* spectrumRoot,
                                               int w,
                                               int h,
                                               bool gpuDirectReady,
                                               bool gpuSpectrumGraph)
{
    qint64 const overlayNodeStartUs = monotonicUs();
    qint64 overlayRebuildUs = 0;
    qint64 overlayTextureUs = 0;
    qint64 overlayNodeUs = 0;
    bool overlayNeedsUpload = false;
    ScopeExit overlayNodeMetric([this,
                                 overlayNodeStartUs,
                                 &overlayRebuildUs,
                                 &overlayTextureUs,
                                 &overlayNodeUs,
                                 &overlayNeedsUpload]() {
        recordOverlayNodeMetric(monotonicUs() - overlayNodeStartUs,
                                overlayRebuildUs,
                                overlayTextureUs,
                                overlayNodeUs,
                                overlayNeedsUpload);
    });

    if (!spectrumRoot)
        return;

    auto findOverlayNode = [&]() -> PanadapterSpectrumOverlayNode* {
        for (QSGNode* child = spectrumRoot->firstChild(); child; child = child->nextSibling()) {
            if (auto* overlay = dynamic_cast<PanadapterSpectrumOverlayNode*>(child))
                return overlay;
        }
        return nullptr;
    };

    auto removeOverlayNode = [&]() {
        if (auto* overlay = findOverlayNode()) {
            spectrumRoot->removeChildNode(overlay);
            delete overlay;
        }
    };

    bool const enabled = (gpuDirectReady || gpuSpectrumGraph) && w > 1 && h > 1;
    if (!enabled) {
        removeOverlayNode();
        return;
    }

    float const displayMinDb = gpuDirectReady ? m_gpuDirectDisplayMinDb : m_minDb;
    float const displayMaxDb = gpuDirectReady ? m_gpuDirectDisplayMaxDb : m_maxDb;
    bool const sizeChanged = m_spectrumOverlaySize != QSize(w, h);
    qreal const dpr = panadapterOverlayDevicePixelRatio(window());
    QSize const textureSize = panadapterOverlayTextureSize(w, h, dpr);
    bool const textureSizeChanged = m_spectrumOverlayImage.size() != textureSize;
    float const rangeThresholdDb = gpuDirectReady ? 8.0f : 2.0f;
    bool const rangeChanged = std::abs(displayMinDb - m_spectrumOverlayDisplayMinDb) > rangeThresholdDb
        || std::abs(displayMaxDb - m_spectrumOverlayDisplayMaxDb) > rangeThresholdDb;
    qint64 const rangeRefreshMs = gpuDirectReady ? 10000 : 5000;
    bool const rangeRefreshDue = monotonicMs() - m_lastSpectrumOverlayRebuildMs >= rangeRefreshMs;
    bool const autoRangeOverlayRefresh = !gpuDirectReady && rangeChanged && rangeRefreshDue;
    if (sizeChanged || textureSizeChanged || autoRangeOverlayRefresh) {
        m_spectrumOverlayDirty = true;
    }

    bool const needsUpload = m_spectrumOverlayDirty || textureSizeChanged;
    overlayNeedsUpload = needsUpload;
    if (needsUpload) {
        qint64 const rebuildStartUs = monotonicUs();
        rebuildSpectrumOverlayImage(w, h, gpuDirectReady);
        overlayRebuildUs += monotonicUs() - rebuildStartUs;
    }
    if (m_spectrumOverlayImage.isNull()) {
        removeOverlayNode();
        return;
    }

    auto* overlay = findOverlayNode();
    qint64 const nodeStartUs = monotonicUs();
    if (!overlay) {
        overlay = new PanadapterSpectrumOverlayNode();
        overlay->setOwnsTexture(true);
        spectrumRoot->appendChildNode(overlay);
    } else if (overlay->nextSibling()) {
        spectrumRoot->removeChildNode(overlay);
        spectrumRoot->appendChildNode(overlay);
    }
    overlayNodeUs += monotonicUs() - nodeStartUs;

#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
    qint64 const textureStartUs = monotonicUs();
    bool uploadTexture = needsUpload;
    bool const softwareTexture =
        static_cast<QSGRendererInterface::GraphicsApi>(sceneGraphApiKey())
        == QSGRendererInterface::Software;
    if (softwareTexture) {
        if (uploadTexture || !overlay->texture()) {
            auto* tex = window()->createTextureFromImage(
                m_spectrumOverlayImage,
                QQuickWindow::CreateTextureOptions(QQuickWindow::TextureHasAlphaChannel));
            if (tex) {
                tex->setFiltering(QSGTexture::Nearest);
                overlay->setTexture(tex);
            }
        }
    } else {
        auto* tex = dynamic_cast<DecodiumRhiImageTexture*>(overlay->texture());
        if (!tex || tex->textureSize() != m_spectrumOverlayImage.size()
            || !tex->hasAlphaChannel() || tex->failed()) {
            tex = new DecodiumRhiImageTexture(true);
            tex->setFiltering(QSGTexture::Nearest);
            overlay->setTexture(tex);
            uploadTexture = true;
        }
        if (uploadTexture)
            tex->uploadFullImage(m_spectrumOverlayImage, true);
        overlay->setFiltering(QSGTexture::Nearest);
    }
    overlay->setFiltering(QSGTexture::Nearest);
    overlayTextureUs += monotonicUs() - textureStartUs;
#else
    qint64 const textureStartUs = monotonicUs();
    if (needsUpload || !overlay->texture()) {
        auto* tex = window()->createTextureFromImage(
            m_spectrumOverlayImage,
            QQuickWindow::CreateTextureOptions(QQuickWindow::TextureHasAlphaChannel));
        if (tex) {
            tex->setFiltering(QSGTexture::Nearest);
            overlay->setTexture(tex);
        }
    }
    overlayTextureUs += monotonicUs() - textureStartUs;
#endif
    qint64 const nodeFinishStartUs = monotonicUs();
    QRectF const overlayRect(0, 0, w, h);
    bool const rectChanged = overlay->rect() != overlayRect;
    if (rectChanged)
        overlay->setRect(overlayRect);
    if (rectChanged && uploadTexture)
        overlay->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    else if (rectChanged)
        overlay->markDirty(QSGNode::DirtyGeometry);
    else if (uploadTexture)
        overlay->markDirty(QSGNode::DirtyMaterial);
    overlayNodeUs += monotonicUs() - nodeFinishStartUs;

    if (!m_loggedSpectrumCppOverlay) {
        m_loggedSpectrumCppOverlay = true;
        qInfo().noquote()
            << "[GPUDBG] Panadapter spectrum C++ overlay path active"
            << "api=" << (window() && window()->rendererInterface()
                              ? waterfallGraphicsApiName(window()->rendererInterface()->graphicsApi())
                              : "Unknown")
            << "dpr=" << dpr
            << "logical=" << QStringLiteral("%1x%2").arg(w).arg(h)
            << "texture=" << QStringLiteral("%1x%2").arg(m_spectrumOverlayImage.width()).arg(m_spectrumOverlayImage.height())
            << "reason= grid/labels/markers batched into one QSG texture; QML repeaters bypassed";
    }
}

void PanadapterItem::updateSpectrumGraphNodes(QSGNode* spectrumRoot, int w, int h)
{
#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
    if (!spectrumRoot || w <= 1 || h <= 1 || m_bins.isEmpty()) {
        removeSpectrumGraphNodes(spectrumRoot);
        return;
    }

    // Vista 3D: sostituisce la traccia 2D (riempimento, alone, picco) ma lascia
    // intatta la cascata sotto e tutte le sovrapposizioni, che vivono altrove.
    if (m_spectrum3d) {
        updateSpectrum3dNodes(spectrumRoot, w, h);
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

    PanadapterFreqView const freqView = makePanadapterFreqView(
        static_cast<float>(m_startFreq),
        static_cast<float>(m_bandwidth),
        m_dataFreqMin,
        m_dataFreqMax,
        m_zoomFactor,
        static_cast<float>(m_panHz));
    if (freqView.clipsData) {
        removeSpectrumGraphNodes(spectrumRoot);
        return;
    }
    float const dataRange = freqView.dataRange;
    float const viewRange = freqView.viewRange;
    float const viewStart = freqView.viewStart;

    auto yForX = [&](int x, QVector<float> const& values) -> float {
        float const pixFreq = viewStart + static_cast<float>(x) * viewRange / static_cast<float>(w);
        float const clampedFreq = qBound(m_dataFreqMin, pixFreq, m_dataFreqMax);
        int bin = static_cast<int>((clampedFreq - m_dataFreqMin) / dataRange * nBins);
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
        if (window() && window()->rendererInterface()) {
            apiName = waterfallGraphicsApiName(window()->rendererInterface()->graphicsApi());
        }
        qInfo().noquote()
            << "[GPUDBG] Panadapter spectrum GPU geometry path active"
            << "api=" << apiName
            << "reason= scene graph line/fill/peak nodes; overlay grid/text/markers batched by PanadapterItem QSG texture";
    }
#else
    Q_UNUSED(spectrumRoot)
    Q_UNUSED(w)
    Q_UNUSED(h)
#endif
}

void PanadapterItem::setDecodeLabels(const QVariantList& labels)
{
    qint64 const startUs = monotonicUs();
    if (m_decodeLabels == labels)
        return;
    m_decodeLabels = labels;
    markOverlayDirty();
    qint64 const elapsedUs = monotonicUs() - startUs;
    qint64 const nowMs = monotonicMs();
    if (m_decodeLabelMetricLastLogMs == 0)
        m_decodeLabelMetricLastLogMs = nowMs;
    if (nowMs - m_decodeLabelMetricLastLogMs >= kPanMetricLogIntervalMs) {
        m_decodeLabelMetricLastLogMs = nowMs;
        qInfo().noquote()
            << "[PANMETRIC] decode_labels"
            << "update_us=" << elapsedUs
            << "count=" << m_decodeLabels.size();
    }
}

void PanadapterItem::setDxClusterSpots(const QVariantList& spots)
{
    if (m_dxClusterSpots == spots)
        return;
    m_dxClusterSpots = spots;
    markOverlayDirty();
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

    if (gpuRawRowPath) {
        static std::atomic_bool loggedRawOnlyWaterfall {false};
        if (!loggedRawOnlyWaterfall.exchange(true, std::memory_order_relaxed)) {
            qInfo().noquote()
                << "[GPUDBG] Panadapter waterfall CPU backing image updates suppressed"
                << "path=RHI_R32F_raw_db_rows"
                << "fallback=CPU_images_on_shader_failure";
        }
    }

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

    if (!gpuRawRowPath
        && (m_waterfallIntensityImage.isNull()
            || m_waterfallIntensityImage.width() != nBins
            || m_waterfallIntensityImage.height() != h)) {
        m_waterfallIntensityImage = QImage(nBins, h, QImage::Format_Grayscale8);
        m_waterfallIntensityImage.fill(0);
        m_waterfallImage.fill(QColor::fromRgb(wfBg));
        m_wfWriteRow = 0;
        m_waterfallGpuUploadedWriteRow = 0;
        m_waterfallGpuUploadedSize = QSize();
        m_loggedWaterfallGpuUploadStats = false;
        m_lastWaterfallGpuStatsRow = -1;
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

    QRgb* line = nullptr;
    uchar* intensityLine = nullptr;
    if (!gpuRawRowPath) {
        line = reinterpret_cast<QRgb*>(m_waterfallImage.scanLine(row));
        intensityLine = m_waterfallIntensityImage.scanLine(row);
    }

    if (intensityLine) {
        for (int bin = 0; bin < nBins; ++bin) {
            float const rawNorm = qBound(0.0f, (bins[bin] - minDb) / range, 1.0f);
            intensityLine[bin] = static_cast<uchar>(qBound(0, static_cast<int>(rawNorm * 255.f + 0.5f), 255));
        }
    }

    bool const prepareCpuPixels =
        !gpuRawRowPath
        && intensityLine
        && (!m_useShaderWaterfall || m_wfWriteRow < h || m_shaderWaterfallBlocked);
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

void PanadapterItem::connectBridgePcmFrameFeed()
{
    if (m_bridgePcmFrameFeedRegistered) {
        return;
    }

    QCoreApplication* app = QCoreApplication::instance();
    QObject* bridgeObject = app
        ? app->property("decodiumBridge").value<QObject*>()
        : nullptr;
    auto* bridge = qobject_cast<DecodiumBridge*>(bridgeObject);
    if (!bridge) {
        return;
    }

    bridge->registerPanadapterItem(this);
    connect(this,
            &QObject::destroyed,
            bridge,
            [bridge, this]() {
                bridge->unregisterPanadapterItem(this);
            },
            Qt::AutoConnection);
    m_bridgePcmFrameFeedRegistered = true;

    qInfo().noquote()
        << "[PANDBG] Panadapter PCM frame feed connected"
        << "route=C++_I16_ring"
        << "qml_bypass=1"
        << "bridge_vector_float=0";
}

void PanadapterItem::itemChange(ItemChange change, const ItemChangeData& value)
{
    if (change == ItemSceneChange) {
        m_sceneGraphApiKey.store(-1, std::memory_order_release);
        m_spectrum3dGpuBlocked.store(false, std::memory_order_release);
        m_loggedGpuSpectrum3d = false;
        m_loggedWaterfallApi = -1;
        m_loggedWaterfallPath = -1;
        m_loggedWaterfallReason.clear();
    }
    if (change == ItemSceneChange && value.window) {
        connectBridgePcmFrameFeed();
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
    m_sceneGraphApiKey.store(-1, std::memory_order_release);
    m_spectrum3dGpuBlocked.store(false, std::memory_order_release);
    m_loggedGpuSpectrum3d = false;
    m_loggedWaterfallApi = -1;
    m_loggedWaterfallPath = -1;
    m_loggedWaterfallReason.clear();
    releaseGpuFftResources();
    QQuickItem::releaseResources();
}

void PanadapterItem::releaseGpuFftResources()
{
#if defined(DECODIUM_QT_RHI_TEXTURE_UPLOAD) && defined(DECODIUM_GPU_PANADAPTER_FFT_QSB)
    GpuFftState* state = m_gpuFft;
    m_gpuFft = nullptr;
    m_gpuDirectTextureReady = false;
    m_gpuFftUiBinsExpected = 0;
    m_hasPendingPcmFrame = false;
    if (!state)
        return;

    if (QQuickWindow* win = window()) {
        auto* cleanup = QRunnable::create([state]() {
            delete state;
        });
        win->scheduleRenderJob(cleanup, QQuickWindow::AfterSynchronizingStage);
    } else {
        delete state;
    }
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
        m_lastGpuFftReadbackMs = 0;
        m_gpuFftUiBinsExpected = 0;
        m_gpuDirectTextureReady = false;
        m_gpuFftActiveNotified = false;
        m_gpuFftInvalidReadbacks = 0;
        m_gpuFftReadbackTimeouts = 0;
        m_gpuFftSlowReadbacks = 0;
        if (m_gpuFft) {
            m_gpuFft->readbackPending = false;
            m_gpuFft->readbackPendingSinceMs = 0;
            ++m_gpuFft->readbackSerial;
        }
    }
    qWarning().noquote()
        << "[PANDBG] Panadapter visual FFT GPU path unavailable"
        << "reason=" << reason
        << "fallback=FFTW_CPU";
    QMetaObject::invokeMethod(this, [this, reason]() {
        emit gpuFftUnavailable(reason);
    }, Qt::QueuedConnection);
}

void PanadapterItem::notifyGpuFftActive(const QString& backend)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_gpuFftActiveNotified)
            return;
        m_gpuFftActiveNotified = true;
    }
    QPointer<PanadapterItem> guard(this);
    QMetaObject::invokeMethod(this, [guard, backend]() {
        if (guard)
            emit guard->gpuFftActivated(backend);
    }, Qt::QueuedConnection);
}

void PanadapterItem::recordGpuFftCompute()
{
#if defined(DECODIUM_QT_RHI_TEXTURE_UPLOAD) && defined(DECODIUM_GPU_PANADAPTER_FFT_QSB)
    PcmFrame frame;
    bool resetTimeout = false;
    qint64 timeoutAgeMs = 0;
    bool disableGpuFft = false;
    quint64 fallbackGeneration = 0;
    QString disableReason;
    {
        QMutexLocker lock(&m_mutex);
        if (m_externalSpectrumActive)
            return;
        if (m_gpuFft && m_gpuFft->readbackPending) {
            qint64 const pendingSince = m_gpuFft->readbackPendingSinceMs;
            qint64 const pendingAgeMs = pendingSince > 0 ? monotonicMs() - pendingSince : 0;
            // 1.0.163 — durante TX il render loop e' bloccato dal sink audio,
            // quindi la readback rimane pending finche' il TX finisce.
            // PRIMA chiamava failGpuFft() che disabilita PERMANENTEMENTE il
            // GPU FFT → Full Spectrum nero dopo TX. Adesso solo resetta lo
            // stato e prosegue col prossimo frame.
            qint64 const timeoutMs = gpuFftReadbackTimeoutMs();
            if (pendingAgeMs > timeoutMs) {
                int const timeoutCount = ++m_gpuFftReadbackTimeouts;
                if (timeoutCount >= gpuFftTimeoutLimit()) {
                    disableGpuFft = true;
                    disableReason = QStringLiteral("Metal GPU FFT readback timeout count=%1 age=%2ms threshold=%3ms")
                        .arg(timeoutCount)
                        .arg(pendingAgeMs)
                        .arg(timeoutMs);
                    m_hasPendingPcmFrame = false;
                } else {
                    m_gpuFft->readbackPending = false;
                    m_gpuFft->readbackPendingSinceMs = 0;
                    ++m_gpuFft->readbackSerial;
                    resetTimeout = true;
                    timeoutAgeMs = pendingAgeMs;
                }
            } else {
                return;
            }
        }
        if (disableGpuFft) {
            // Lascia il lock prima di emettere il fallback: failGpuFft()
            // aggiorna gli stessi stati e notifica il bridge.
        } else if (!m_hasPendingPcmFrame || m_gpuFftFailed) {
            return;
        } else {
            frame = m_pendingPcmFrame;
            m_hasPendingPcmFrame = false;
            fallbackGeneration = m_gpuFftFallbackGeneration;
        }
    }
    if (disableGpuFft) {
        failGpuFft(disableReason);
        return;
    }
    if (resetTimeout) {
        qint64 const nowMs = monotonicMs();
        bool const shouldLogTimeout = m_lastGpuFftTimeoutLogMs == 0
            || nowMs - m_lastGpuFftTimeoutLogMs > 5000;
        if (shouldLogTimeout)
            m_lastGpuFftTimeoutLogMs = nowMs;
        if (shouldLogTimeout) {
            qInfo().noquote() << "[PANDBG] GPU FFT readback timeout reset after"
                              << timeoutAgeMs << "ms"
                              << "threshold=" << gpuFftReadbackTimeoutMs()
                              << "(visual frame dropped; pipeline continues)";
        }
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
    if (!rhi->isFeatureSupported(QRhi::Compute)) {
        failGpuFft(QStringLiteral("RHI backend does not support compute shaders"));
        return;
    }

    const int N = 4096;
    if (frame.samples.size() < N || frame.nfb <= frame.nfa) {
        failGpuFft(QStringLiteral("invalid PCM frame for GPU compute"));
        return;
    }
    const float* uploadSampleData = frame.samples.constData();

    const float freqPerBin = 12000.0f / static_cast<float>(N);
    const int binStart = qBound(0, static_cast<int>(frame.nfa / freqPerBin), N / 2);
    const int binEnd = qBound(binStart, static_cast<int>(frame.nfb / freqPerBin), N / 2);
    const int sourceBins = binEnd - binStart;
    if (sourceBins <= 0) {
        failGpuFft(QStringLiteral("empty GPU FFT bin range"));
        return;
    }
    bool const wantsDirectTexturePath = gpuDirectPanadapterEnabled();
    int const requestedGpuBins = qEnvironmentVariableIntValue("DECODIUM_GPU_PANADAPTER_DFT_BINS");
    int const defaultGpuBins = wantsDirectTexturePath ? sourceBins : qMin(512, sourceBins);
    const int nBins = qMin(sourceBins,
        requestedGpuBins > 0 ? qBound(64, requestedGpuBins, sourceBins) : defaultGpuBins);

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

    bool const directTexturePath =
        wantsDirectTexturePath
        && rhi->isTextureFormatSupported(QRhiTexture::R32F, QRhiTexture::UsedWithLoadStore);

    if (directTexturePath) {
        if (!m_gpuFft->directShader.isValid()) {
            QFile shaderFile(QStringLiteral(":/shaders/panadapter_fft_direct.comp.qsb"));
            if (!shaderFile.open(QIODevice::ReadOnly)) {
                failGpuFft(QStringLiteral("missing :/shaders/panadapter_fft_direct.comp.qsb"));
                return;
            }
            m_gpuFft->directShader = QShader::fromSerialized(shaderFile.readAll());
            if (!m_gpuFft->directShader.isValid()) {
                failGpuFft(QStringLiteral("invalid panadapter_fft_direct compute shader"));
                return;
            }
        }

        int waterfallRows = 0;
        int waterfallWriteRow = 0;
        bool peakHold = false;
        float peakDecay = 0.97f;
        float displayMinDb = -70.0f;
        float displayMaxDb = 35.0f;
        int noiseCutPercent = 10;
        float noiseCutBiasDb = 0.0f;
        float estimatedFloorDb = estimateGpuDirectNoiseFloorDb(frame.sampleRms, N)
            + gpuDirectAutoRangeFloorOffsetDb();
        bool rangePropertiesChanged = false;
        {
            QMutexLocker lock(&m_mutex);
            waterfallRows = !m_waterfallImage.isNull() ? m_waterfallImage.height() : 0;
            if (waterfallRows <= 0)
                waterfallRows = 256;
            waterfallWriteRow = m_wfWriteRow % waterfallRows;
            peakHold = m_peakHold;
            peakDecay = m_peakDecay;
            noiseCutPercent = m_noiseFloorPercentile;
            noiseCutBiasDb = gpuDirectNoiseCutBiasDb(noiseCutPercent);
            if (m_autoRange) {
                if (!std::isfinite(estimatedFloorDb))
                    estimatedFloorDb = std::isfinite(m_measuredFloor) ? m_measuredFloor : -70.0f;
                bool const resetFloor =
                    m_lastGpuFftFrameMs <= 0
                    || !std::isfinite(m_measuredFloor)
                    || m_measuredFloor < -120.0f
                    || std::abs(m_measuredFloor - estimatedFloorDb) > 60.0f;
                float const measuredFloorDb = resetFloor
                    ? estimatedFloorDb
                    : (0.18f * estimatedFloorDb + 0.82f * m_measuredFloor);
                float const displayRangeDb = gpuDirectAutoRangeSpanDb(m_contrastLevel);
                float const directBlackThresh = waterfallBlackThresholdForLevel(m_blackLevel, true);
                displayMinDb = measuredFloorDb
                    - directBlackThresh * displayRangeDb
                    - gpuDirectAutoRangeFloorMarginDb()
                    + noiseCutBiasDb;
                displayMaxDb = displayMinDb + displayRangeDb;
                if (std::abs(m_minDb - displayMinDb) > 0.25f
                    || std::abs(m_maxDb - displayMaxDb) > 0.25f) {
                    m_measuredFloor = measuredFloorDb;
                    m_measuredPeak = measuredFloorDb + displayRangeDb;
                    m_minDb = displayMinDb;
                    m_maxDb = displayMaxDb;
                    rangePropertiesChanged = true;
                }
            } else if (m_maxDb > m_minDb) {
                displayMinDb = m_minDb;
                displayMaxDb = m_maxDb;
            }
        }
        if (rangePropertiesChanged) {
            QPointer<PanadapterItem> guard(this);
            QMetaObject::invokeMethod(this, [guard]() {
                if (!guard)
                    return;
                emit guard->measuredFloorChanged();
                emit guard->measuredPeakChanged();
                emit guard->minDbChanged();
                emit guard->maxDbChanged();
            }, Qt::QueuedConnection);
        }
        float const displayRange = qMax(1.0f, displayMaxDb - displayMinDb);

        bool reallocatedStorage = false;
        bool const needsSampleOutputBuffers =
            !m_gpuFft->sampleBuffer
            || !m_gpuFft->outputBuffer
            || m_gpuFft->sampleCapacity < sampleBytes
            || m_gpuFft->outputCapacity < outputBytes;
        if (needsSampleOutputBuffers) {
            delete m_gpuFft->pipeline;
            delete m_gpuFft->directPipeline;
            delete m_gpuFft->srb;
            delete m_gpuFft->directSrb;
            delete m_gpuFft->outputBuffer;
            delete m_gpuFft->sampleBuffer;
            m_gpuFft->pipeline = nullptr;
            m_gpuFft->directPipeline = nullptr;
            m_gpuFft->srb = nullptr;
            m_gpuFft->directSrb = nullptr;
            m_gpuFft->outputBuffer = nullptr;
            m_gpuFft->sampleBuffer = nullptr;

            m_gpuFft->sampleBuffer = rhi->newBuffer(QRhiBuffer::Static,
                                                    QRhiBuffer::StorageBuffer,
                                                    sampleBytes);
            m_gpuFft->outputBuffer = rhi->newBuffer(QRhiBuffer::Static,
                                                    QRhiBuffer::StorageBuffer,
                                                    outputBytes);
            if (!m_gpuFft->sampleBuffer
                || !m_gpuFft->outputBuffer
                || !m_gpuFft->sampleBuffer->create()
                || !m_gpuFft->outputBuffer->create()) {
                failGpuFft(QStringLiteral("failed to create GPU direct FFT buffers"));
                return;
            }
            m_gpuFft->sampleCapacity = sampleBytes;
            m_gpuFft->outputCapacity = outputBytes;
            reallocatedStorage = true;
        }

        if (!m_gpuFft->directParamsBuffer) {
            m_gpuFft->directParamsBuffer = rhi->newBuffer(QRhiBuffer::Static,
                                                          QRhiBuffer::StorageBuffer,
                                                          sizeof(GpuFftDirectParams));
            if (!m_gpuFft->directParamsBuffer || !m_gpuFft->directParamsBuffer->create()) {
                failGpuFft(QStringLiteral("failed to create GPU direct params buffer"));
                return;
            }
            delete m_gpuFft->directSrb;
            delete m_gpuFft->directPipeline;
            m_gpuFft->directSrb = nullptr;
            m_gpuFft->directPipeline = nullptr;
        }

        auto recreateTexture = [&](QRhiTexture*& texture, QSize& currentSize, QSize const& desiredSize) -> bool {
            if (desiredSize.isEmpty())
                return false;
            if (texture && currentSize == desiredSize && texture->pixelSize() == desiredSize)
                return false;
            m_gpuFft->retireDirectTexture(texture);
            texture = rhi->newTexture(QRhiTexture::R32F,
                                      desiredSize,
                                      1,
                                      QRhiTexture::Flags(QRhiTexture::UsedWithLoadStore
                                                         | QRhiTexture::UsedAsTransferSource));
            if (!texture || !texture->create()) {
                delete texture;
                texture = nullptr;
                currentSize = QSize();
                return false;
            }
            currentSize = desiredSize;
            delete m_gpuFft->directSrb;
            delete m_gpuFft->directPipeline;
            m_gpuFft->directSrb = nullptr;
            m_gpuFft->directPipeline = nullptr;
            return true;
        };

        QSize const spectrumSize(nBins, 1);
        QSize const waterfallSize(nBins, waterfallRows);
        QSize const rowParamsSize(2, waterfallRows);
        bool const spectrumRecreated = recreateTexture(m_gpuFft->directSpectrumTexture,
                                                       m_gpuFft->directSpectrumSize,
                                                       spectrumSize);
        bool const waterfallRecreated = recreateTexture(m_gpuFft->directWaterfallTexture,
                                                        m_gpuFft->directWaterfallSize,
                                                        waterfallSize);
        bool const rowParamsRecreated = recreateTexture(m_gpuFft->directRowParamsTexture,
                                                        m_gpuFft->directRowParamsSize,
                                                        rowParamsSize);
        bool const peakRecreated = recreateTexture(m_gpuFft->directPeakTexture,
                                                   m_gpuFft->directPeakSize,
                                                   spectrumSize);
        if (!m_gpuFft->directSpectrumTexture
            || !m_gpuFft->directWaterfallTexture
            || !m_gpuFft->directRowParamsTexture
            || !m_gpuFft->directPeakTexture) {
            failGpuFft(QStringLiteral("failed to create GPU direct R32F textures"));
            return;
        }

        if (reallocatedStorage || !m_gpuFft->directSrb || !m_gpuFft->directPipeline) {
            delete m_gpuFft->directSrb;
            delete m_gpuFft->directPipeline;
            m_gpuFft->directSrb = nullptr;
            m_gpuFft->directPipeline = nullptr;

            m_gpuFft->directSrb = rhi->newShaderResourceBindings();
            m_gpuFft->directSrb->setBindings({
                QRhiShaderResourceBinding::bufferLoad(0,
                                                      QRhiShaderResourceBinding::ComputeStage,
                                                      m_gpuFft->directParamsBuffer),
                QRhiShaderResourceBinding::bufferLoad(1,
                                                      QRhiShaderResourceBinding::ComputeStage,
                                                      m_gpuFft->sampleBuffer),
                QRhiShaderResourceBinding::bufferStore(2,
                                                       QRhiShaderResourceBinding::ComputeStage,
                                                       m_gpuFft->outputBuffer),
                QRhiShaderResourceBinding::imageStore(3,
                                                      QRhiShaderResourceBinding::ComputeStage,
                                                      m_gpuFft->directSpectrumTexture,
                                                      0),
                QRhiShaderResourceBinding::imageStore(4,
                                                      QRhiShaderResourceBinding::ComputeStage,
                                                      m_gpuFft->directWaterfallTexture,
                                                      0),
                QRhiShaderResourceBinding::imageStore(5,
                                                      QRhiShaderResourceBinding::ComputeStage,
                                                      m_gpuFft->directRowParamsTexture,
                                                      0),
                QRhiShaderResourceBinding::imageLoadStore(6,
                                                          QRhiShaderResourceBinding::ComputeStage,
                                                          m_gpuFft->directPeakTexture,
                                                          0)
            });
            if (!m_gpuFft->directSrb->create()) {
                failGpuFft(QStringLiteral("failed to create GPU direct shader bindings"));
                return;
            }

            m_gpuFft->directPipeline = rhi->newComputePipeline();
            m_gpuFft->directPipeline->setShaderStage(QRhiShaderStage(QRhiShaderStage::Compute,
                                                                     m_gpuFft->directShader));
            m_gpuFft->directPipeline->setShaderResourceBindings(m_gpuFft->directSrb);
            if (!m_gpuFft->directPipeline->create()) {
                failGpuFft(QStringLiteral("failed to create GPU direct compute pipeline"));
                return;
            }
        }

        GpuFftDirectParams directParams;
        directParams.n = N;
        directParams.binStart = binStart;
        directParams.nBins = nBins;
        directParams.waterfallRows = waterfallRows;
        directParams.waterfallWriteRow = waterfallWriteRow;
        directParams.peakHold = peakHold ? 1 : 0;
        directParams.resetPeak = peakRecreated || !peakHold ? 1 : 0;
        float const fftNorm = static_cast<float>(N) / 2.0f;
        directParams.inverseNormSquared = 1.0f / (fftNorm * fftNorm);
        directParams.powerFloor = 1e-24f;
        directParams.binStep = sourceBins > 1 && nBins > 1
            ? static_cast<float>(sourceBins - 1) / static_cast<float>(nBins - 1)
            : 1.0f;
        directParams.displayMinDb = displayMinDb;
        directParams.displayInvRange = 1.0f / displayRange;
        directParams.peakDecay = peakDecay;

        QRhiResourceUpdateBatch* uploads = rhi->nextResourceUpdateBatch();
        uploads->uploadStaticBuffer(m_gpuFft->sampleBuffer,
                                    0,
                                    sampleBytes,
                                    uploadSampleData);
        uploads->uploadStaticBuffer(m_gpuFft->directParamsBuffer,
                                    0,
                                    sizeof(GpuFftDirectParams),
                                    &directParams);

        auto uploadFullR32F = [&](QRhiTexture* texture, QSize const& size, QByteArray const& bytes) {
            if (!texture || size.isEmpty() || bytes.isEmpty())
                return;
            QRhiTextureSubresourceUploadDescription desc(bytes);
            desc.setSourceSize(size);
            desc.setDataStride(size.width() * static_cast<quint32>(sizeof(float)));
            QRhiTextureUploadDescription uploadDescription;
            QRhiTextureUploadEntry entry(0, 0, desc);
            uploadDescription.setEntries(&entry, &entry + 1);
            uploads->uploadTexture(texture, uploadDescription);
        };
        if (spectrumRecreated) {
            QVector<float> clear(spectrumSize.width(), -200.0f);
            uploadFullR32F(m_gpuFft->directSpectrumTexture,
                           spectrumSize,
                           QByteArray(reinterpret_cast<char const*>(clear.constData()),
                                      clear.size() * static_cast<int>(sizeof(float))));
        }
        if (peakRecreated) {
            QVector<float> clear(spectrumSize.width(), -200.0f);
            uploadFullR32F(m_gpuFft->directPeakTexture,
                           spectrumSize,
                           QByteArray(reinterpret_cast<char const*>(clear.constData()),
                                      clear.size() * static_cast<int>(sizeof(float))));
        }
        if (waterfallRecreated) {
            QVector<float> clear(waterfallSize.width() * waterfallSize.height(), -200.0f);
            uploadFullR32F(m_gpuFft->directWaterfallTexture,
                           waterfallSize,
                           QByteArray(reinterpret_cast<char const*>(clear.constData()),
                                      clear.size() * static_cast<int>(sizeof(float))));
        }
        if (rowParamsRecreated) {
            QVector<float> paramsRows(rowParamsSize.width() * rowParamsSize.height(), 0.0f);
            for (int row = 0; row < rowParamsSize.height(); ++row) {
                paramsRows[row * 2] = displayMinDb;
                paramsRows[row * 2 + 1] = 1.0f / displayRange;
            }
            uploadFullR32F(m_gpuFft->directRowParamsTexture,
                           rowParamsSize,
                           QByteArray(reinterpret_cast<char const*>(paramsRows.constData()),
                                      paramsRows.size() * static_cast<int>(sizeof(float))));
        }

        QRhiResourceUpdateBatch* debugReadbackBatch = nullptr;
        DecodiumRhiBufferReadbackResult* debugReadback = nullptr;
        bool const debugReadbackEnabled = gpuDirectDebugReadbackEnabled();
        quint64 debugReadbackSerial = 0;
        qint64 debugReadbackSubmitMs = 0;
        if (debugReadbackEnabled) {
            debugReadbackBatch = rhi->nextResourceUpdateBatch();
            debugReadback = new DecodiumRhiBufferReadbackResult;
            debugReadbackSerial = ++m_gpuFft->readbackSerial;
            debugReadbackSubmitMs = monotonicMs();
            QPointer<PanadapterItem> guard(this);
            debugReadback->completed = [guard,
                                        debugReadback,
                                        debugReadbackSerial,
                                        debugReadbackSubmitMs,
                                        nBins]() mutable {
                QByteArray const data = debugReadback->data;
                delete debugReadback;
                if (!guard)
                    return;
                {
                    QMutexLocker lock(&guard->m_mutex);
                    if (!guard->m_gpuFft || guard->m_gpuFft->readbackSerial != debugReadbackSerial)
                        return;
                    guard->m_gpuFft->readbackPending = false;
                    guard->m_gpuFft->readbackPendingSinceMs = 0;
                }
                if (data.size() < nBins * static_cast<int>(sizeof(float)))
                    return;
                QVector<float> values(nBins);
                std::memcpy(values.data(), data.constData(), static_cast<size_t>(nBins) * sizeof(float));
                float minDb = 200.0f;
                float maxDb = -200.0f;
                int finite = 0;
                for (float db : values) {
                    if (!std::isfinite(db))
                        continue;
                    ++finite;
                    minDb = qMin(minDb, db);
                    maxDb = qMax(maxDb, db);
                }
                qInfo().noquote()
                    << "[GPUDBG] Panadapter direct FFT debug readback"
                    << "age_ms=" << (monotonicMs() - debugReadbackSubmitMs)
                    << "bins=" << nBins
                    << "finite=" << finite
                    << "minDb=" << (finite ? minDb : 0.0f)
                    << "maxDb=" << (finite ? maxDb : 0.0f);
            };
            debugReadbackBatch->readBackBuffer(m_gpuFft->outputBuffer,
                                               0,
                                               outputBytes,
                                               debugReadback);
        }

        cb->beginComputePass(uploads);
        cb->setComputePipeline(m_gpuFft->directPipeline);
        cb->setShaderResources(m_gpuFft->directSrb);
        cb->dispatch((nBins + 63) / 64, 1, 1);
        cb->endComputePass(debugReadbackBatch);

        {
            QMutexLocker lock(&m_mutex);
            if (m_externalSpectrumActive
                || fallbackGeneration != m_gpuFftFallbackGeneration) {
                m_gpuDirectTextureReady = false;
                m_gpuFftUiBinsExpected = 0;
                return;
            }
            m_dataFreqMin = frame.freqMinHz;
            m_dataFreqMax = frame.freqMaxHz;
            m_waterfallRawBinsWidth = nBins;
            m_wfWriteRow += 1;
            m_gpuDirectTextureReady = true;
            m_gpuDirectDisplayMinDb = displayMinDb;
            m_gpuDirectDisplayMaxDb = displayMaxDb;
            m_lastGpuFftFrameMs = monotonicMs();
            m_lastGpuFftReadbackMs = debugReadbackEnabled ? 0 : m_lastGpuFftFrameMs;
            m_gpuFftUiBinsExpected = nBins;
            m_gpuFft->directTexturePathActive = true;
            m_gpuFft->directBins = nBins;
            m_gpuFft->directRows = waterfallRows;
            m_gpuFft->directWriteRow = m_wfWriteRow;
            m_gpuFft->readbackPending = debugReadbackEnabled;
            m_gpuFft->readbackPendingSinceMs = debugReadbackEnabled ? monotonicMs() : 0;
        }
        notifyGpuFftActive(QStringLiteral("%1 RHI compute")
                               .arg(QString::fromLatin1(
                                   waterfallGraphicsApiName(rif->graphicsApi()))));

        if (!m_gpuFft->loggedDirectActive) {
            qInfo().noquote()
                << "[GPUDBG] Panadapter visual FFT GPU direct texture path active"
                << "api=" << waterfallGraphicsApiName(rif->graphicsApi())
                << "shader=panadapter_fft_direct.comp.qsb"
                << "algorithm=single_pass_dft_recurrence_4096"
                << "source_bins=" << sourceBins
                << "gpu_bins=" << nBins
                << "waterfall=" << QStringLiteral("%1x%2").arg(nBins).arg(waterfallRows)
                << "window=shader_blackman_harris"
                << "readback=" << (debugReadbackEnabled ? "debug_async" : "off")
                << "direct_texture=1"
                << "fallback=FFTW_CPU";
            m_gpuFft->loggedDirectActive = true;
        }
        if (m_autoRange
            && (!m_gpuFft->loggedDirectAutoRange
                || m_gpuFft->loggedDirectNoiseCutPercent != noiseCutPercent)) {
            qInfo().noquote()
                << "[GPUDBG] Panadapter direct auto-range active"
                << "input_rms=" << frame.sampleRms
                << "estimated_floor_db=" << estimatedFloorDb
                << "display_min_db=" << displayMinDb
                << "display_max_db=" << displayMaxDb
                << "span_db=" << (displayMaxDb - displayMinDb)
                << "contrast=" << m_contrastLevel
                << "floor_margin_db=" << gpuDirectAutoRangeFloorMarginDb()
                << "cut_percent=" << noiseCutPercent
                << "cut_bias_db=" << noiseCutBiasDb
                << "black_threshold=" << waterfallBlackThresholdForLevel(m_blackLevel, true)
                << "readback=off";
            m_gpuFft->loggedDirectAutoRange = true;
            m_gpuFft->loggedDirectNoiseCutPercent = noiseCutPercent;
        }
        if (debugReadbackEnabled && !m_gpuFft->loggedDirectDebugReadback) {
            qInfo().noquote()
                << "[GPUDBG] Panadapter direct debug readback enabled"
                << "env=DECODIUM_GPU_PANADAPTER_DEBUG_READBACK";
            m_gpuFft->loggedDirectDebugReadback = true;
        }
        return;
    }

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
        windowedSamples[i] = uploadSampleData[i] * blackmanHarrisWindow.at(i);

    QRhiResourceUpdateBatch* uploads = rhi->nextResourceUpdateBatch();
    uploads->uploadStaticBuffer(m_gpuFft->sampleBuffer,
                                0,
                                sampleBytes,
                                windowedSamples.constData());
    uploads->uploadStaticBuffer(m_gpuFft->paramsBuffer,
                                0,
                                sizeof(GpuFftParams),
                                &params);

    quint64 const readbackSerial = ++m_gpuFft->readbackSerial;
    qint64 const readbackSubmitMs = monotonicMs();
    auto* readback = new DecodiumRhiBufferReadbackResult;
    QPointer<PanadapterItem> guard(this);
    readback->completed = [guard,
                           readback,
                           readbackSerial,
                           readbackSubmitMs,
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
        qint64 const readbackAgeMs = readbackSubmitMs > 0
            ? monotonicMs() - readbackSubmitMs
            : 0;
        QString disableAfterFrameReason;
        {
            QMutexLocker lock(&guard->m_mutex);
            if (!guard->m_gpuFft || guard->m_gpuFft->readbackSerial != readbackSerial) {
                return;
            }
            guard->m_gpuFft->readbackPending = false;
            guard->m_gpuFft->readbackPendingSinceMs = 0;
            qint64 const slowThresholdMs = gpuFftSlowReadbackMs();
            if (readbackAgeMs > slowThresholdMs) {
                int const slowCount = ++guard->m_gpuFftSlowReadbacks;
                qint64 const nowMs = monotonicMs();
                bool const shouldLogSlow = guard->m_lastGpuFftSlowLogMs == 0
                    || nowMs - guard->m_lastGpuFftSlowLogMs > 5000;
                if (shouldLogSlow) {
                    guard->m_lastGpuFftSlowLogMs = nowMs;
                    qInfo().noquote()
                        << "[PANDBG] Panadapter visual FFT GPU readback slow"
                        << "age_ms=" << readbackAgeMs
                        << "threshold=" << slowThresholdMs
                        << "count=" << slowCount
                        << "limit=" << gpuFftSlowReadbackLimit();
                }
                if (slowCount >= gpuFftSlowReadbackLimit()) {
                    disableAfterFrameReason =
                        QStringLiteral("Metal GPU FFT readback too slow count=%1 age=%2ms threshold=%3ms")
                            .arg(slowCount)
                            .arg(readbackAgeMs)
                            .arg(slowThresholdMs);
                }
            } else {
                guard->m_gpuFftSlowReadbacks = 0;
                guard->m_gpuFftReadbackTimeouts = 0;
            }
        }
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
            guard->m_lastGpuFftReadbackMs = guard->m_lastGpuFftFrameMs;
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
                                   freqMaxHz,
                                   disableAfterFrameReason]() mutable {
            if (!guard)
                return;
            guard->addSpectrumData(values, minDb, maxDb, freqMinHz, freqMaxHz);
            if (!disableAfterFrameReason.isEmpty())
                guard->failGpuFft(disableAfterFrameReason);
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
    notifyGpuFftActive(QStringLiteral("%1 RHI compute")
                           .arg(QString::fromLatin1(
                               waterfallGraphicsApiName(rif->graphicsApi()))));
    m_gpuFft->readbackPending = true;
    m_gpuFft->readbackPendingSinceMs = monotonicMs();

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
    int const path = gpu ? 1 : 0;
    // This runs inside updatePaintNode(). Querying QSGRendererInterface on
    // every frame can serialize with D3D present on some Windows drivers.
    if (m_loggedWaterfallPath == path && m_loggedWaterfallReason == reason)
        return;

    QSGRendererInterface::GraphicsApi const api =
        static_cast<QSGRendererInterface::GraphicsApi>(sceneGraphApiKey());
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

int PanadapterItem::sceneGraphApiKey() const
{
    int const cached = m_sceneGraphApiKey.load(std::memory_order_acquire);
    if (cached >= 0)
        return cached;

    QQuickWindow* win = window();
    QSGRendererInterface* renderer = win ? win->rendererInterface() : nullptr;
    QSGRendererInterface::GraphicsApi const api = renderer
        ? renderer->graphicsApi()
        : QSGRendererInterface::Unknown;
    int const key = static_cast<int>(api);
    if (api != QSGRendererInterface::Unknown)
        m_sceneGraphApiKey.store(key, std::memory_order_release);
    return key;
}

// ─── Qt Scene Graph update ────────────────────────────────────────────────────
QSGNode* PanadapterItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    if (!window()) return nullptr;
    int w = (int)width();
    int h = (int)height();
    if (w <= 0 || h <= 0) { delete oldNode; return nullptr; }

    qint64 const lockRequestUs = monotonicUs();
    QMutexLocker lock(&m_mutex);
    qint64 const paintStartUs = monotonicUs();
    qint64 paintGeometryUs = 0;
    qint64 paintDrainUs = 0;
    qint64 paintOverlayUs = 0;
    qint64 paintNodesStartUs = 0;
    qint64 paintSpectrumNodeUs = 0;
    qint64 paintWaterfallNodeUs = 0;
    qint64 paintWaterfallTextureUs = 0;
    qint64 paintWaterfallDisplayUs = 0;
    qint64 paintWaterfallSetupUs = 0;
    qint64 paintWaterfallMarkUs = 0;
    qint64 paintWaterfallLogUs = 0;
    int paintWaterfallPath = 0;
    int paintTextureCreateCount = 0;
    int paintTextureUploadRows = 0;
    int paintTextureFullUploads = 0;
    bool paintGpuDirectReady = false;
    int paintPendingRows = 0;
    ScopeExit paintMetric([this,
                           lockRequestUs,
                           paintStartUs,
                           &paintGeometryUs,
                           &paintDrainUs,
                           &paintOverlayUs,
                           &paintNodesStartUs,
                           &paintSpectrumNodeUs,
                           &paintWaterfallNodeUs,
                           &paintWaterfallTextureUs,
                           &paintWaterfallDisplayUs,
                           &paintWaterfallSetupUs,
                           &paintWaterfallMarkUs,
                           &paintWaterfallLogUs,
                           &paintWaterfallPath,
                           &paintTextureCreateCount,
                           &paintTextureUploadRows,
                           &paintTextureFullUploads,
                           &paintGpuDirectReady,
                           &paintPendingRows]() {
        qint64 const nowUs = monotonicUs();
        qint64 const nodesUs = paintNodesStartUs > 0 ? nowUs - paintNodesStartUs : 0;
        recordPaintMetric(nowUs - paintStartUs,
                          paintStartUs - lockRequestUs,
                          paintGeometryUs,
                          paintDrainUs,
                          paintOverlayUs,
                          nodesUs,
                          paintSpectrumNodeUs,
                          paintWaterfallNodeUs,
                          paintWaterfallTextureUs,
                          paintWaterfallDisplayUs,
                          paintWaterfallSetupUs,
                          paintWaterfallMarkUs,
                          paintWaterfallLogUs,
                          paintWaterfallPath,
                          paintTextureCreateCount,
                          paintTextureUploadRows,
                          paintTextureFullUploads,
                          paintGpuDirectReady,
                          paintPendingRows);
    });


    if (m_geometryDirty) {
        qint64 const geometryStartUs = monotonicUs();
        rebuildImages(w, h);
        paintGeometryUs += monotonicUs() - geometryStartUs;
        m_geometryDirty = false;
    }
    int const specH = !m_renderSpectrumSize.isEmpty()
        ? qBound(1, m_renderSpectrumSize.height(), h)
        : qMin(m_spectrumH, h);

    bool const shaderSupported = shaderWaterfallSupported();
    PanadapterFreqView const qsgFreqView = makePanadapterFreqView(
        static_cast<float>(m_startFreq),
        static_cast<float>(m_bandwidth),
        m_dataFreqMin,
        m_dataFreqMax,
        m_zoomFactor,
        static_cast<float>(m_panHz));
    if (qsgFreqView.clipsData) {
        static std::atomic<qint64> lastRangeClipLogMs {0};
        qint64 const nowMs = monotonicMs();
        qint64 previousMs = lastRangeClipLogMs.load(std::memory_order_relaxed);
        if (nowMs - previousMs >= 30000
            && lastRangeClipLogMs.compare_exchange_strong(previousMs, nowMs, std::memory_order_relaxed)) {
            qInfo().noquote()
                << "[PANDBG] Panadapter spectrum view clipped to data range"
                << "viewHz=" << QStringLiteral("%1..%2")
                                 .arg(qsgFreqView.viewStart, 0, 'f', 1)
                                 .arg(qsgFreqView.viewStart + qsgFreqView.viewRange, 0, 'f', 1)
                << "dataHz=" << QStringLiteral("%1..%2")
                                 .arg(qsgFreqView.dataStart, 0, 'f', 1)
                                 .arg(qsgFreqView.dataEnd, 0, 'f', 1)
                << "bins=" << m_bins.size()
                << "gpu_graph=disabled"
                << "reason=avoid_false_floor_slope";
        }
    }
    bool const gpuSpectrumGraph = spectrumGraphSupported() && !qsgFreqView.clipsData;
    bool const gpuDirectReady =
#if defined(DECODIUM_QT_RHI_TEXTURE_UPLOAD) && defined(DECODIUM_GPU_PANADAPTER_FFT_QSB)
        !m_externalSpectrumActive
        && m_gpuDirectTextureReady
        && !m_gpuFftFailed
        && m_gpuFft
        && m_gpuFft->directTexturePathActive
        && m_gpuFft->directSpectrumTexture
        && m_gpuFft->directWaterfallTexture
        && m_gpuFft->directRowParamsTexture
        && m_gpuFft->directPeakTexture
        // 3D samples the same history texture directly.  If its shader path
        // is unavailable this becomes false and the existing CPU producer and
        // QSG geometry path take over automatically.
        && (!m_spectrum3d || spectrum3dGpuSupported());
#else
        false;
#endif
    paintGpuDirectReady = gpuDirectReady;
    paintPendingRows = m_pendingWaterfallRows.size();
    m_useShaderWaterfall = shaderSupported && (m_wfWriteRow > 0 || gpuDirectReady);
    if (!gpuDirectReady && m_spectrumDirty && !m_bins.isEmpty()) {
        qint64 const drainStartUs = monotonicUs();
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
        paintDrainUs += monotonicUs() - drainStartUs;
        paintPendingRows = m_pendingWaterfallRows.size();
    }
    m_useShaderWaterfall = shaderSupported && (m_wfWriteRow > 0 || gpuDirectReady);
    paintNodesStartUs = monotonicUs();
    qint64 const spectrumNodeStartUs = monotonicUs();

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
    if (gpuDirectReady) {
#if defined(DECODIUM_QT_RHI_TEXTURE_UPLOAD) && defined(DECODIUM_GPU_PANADAPTER_SPECTRUM_QSB)
        QRectF specRect(0, 0, w, specH);
        QColor const bg = (m_paletteIndex == 11) ? QColor(255, 255, 255) : QColor(0, 0, 0);
        if (auto* bgNode = ensureFlatColorNode(spectrumRoot,
                                               0,
                                               6,
                                               QSGGeometry::DrawTriangles,
                                               bg)) {
            writeRectGeometry(bgNode->geometry()->vertexDataAsPoint2D(), specRect);
        }

        if (m_spectrum3d) {
            updateSpectrum3dGpuNodes(spectrumRoot, w, specH);
        } else {
        QSGNode* child = sceneGraphChildAt(spectrumRoot, 1);
        auto* directNode = dynamic_cast<QSGGeometryNode*>(child);
        auto* material = directNode ? dynamic_cast<PanadapterSpectrumMaterial*>(directNode->material()) : nullptr;
        if (child && (!directNode || !material)) {
            removeSceneGraphChildrenFrom(spectrumRoot, child);
            child = nullptr;
            directNode = nullptr;
            material = nullptr;
        }
        if (!directNode) {
            directNode = new QSGGeometryNode();
            auto* geometry = new QSGGeometry(waterfallTexturedPoint2DAttributes(), 4);
            geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
            geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
            directNode->setGeometry(geometry);
            directNode->setFlag(QSGNode::OwnsGeometry);
            material = new PanadapterSpectrumMaterial();
            directNode->setMaterial(material);
            directNode->setFlag(QSGNode::OwnsMaterial);
            spectrumRoot->appendChildNode(directNode);
        }

        QSGGeometry::updateTexturedRectGeometry(directNode->geometry(),
                                                specRect,
                                                QRectF(0, 0, 1, 1));

        auto replaceExternalTexture = [](QSGTexture*& slot,
                                         QRhiTexture* texture,
                                         QSize const& size,
                                         QSGTexture::Filtering filtering) {
            auto* ext = dynamic_cast<DecodiumExternalRhiTexture*>(slot);
            if (!ext || ext->rhiTexture() != texture || ext->textureSize() != size) {
                delete slot;
                slot = new DecodiumExternalRhiTexture(texture, size, false);
            }
            slot->setFiltering(filtering);
        };
        replaceExternalTexture(material->spectrumTexture,
                               m_gpuFft->directSpectrumTexture,
                               m_gpuFft->directSpectrumSize,
                               QSGTexture::Linear);
        replaceExternalTexture(material->peakTexture,
                               m_gpuFft->directPeakTexture,
                               m_gpuFft->directPeakSize,
                               QSGTexture::Linear);

        float const range = qMax(1.0f, m_gpuDirectDisplayMaxDb - m_gpuDirectDisplayMinDb);
        material->params[0] = m_gpuDirectDisplayMinDb;
        material->params[1] = 1.0f / range;
        material->params[2] = 1.5f / qMax(1.0f, static_cast<float>(specH));
        material->params[3] = m_peakHold ? 1.0f : 0.0f;
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

        auto writeColor = [](float* dst, QColor color, float alpha) {
            dst[0] = color.redF();
            dst[1] = color.greenF();
            dst[2] = color.blueF();
            dst[3] = alpha;
        };
        QColor const fillColor = QColor::fromRgb(wfColor(0.82f));
        QColor const glowColor = QColor::fromRgb(wfColor(0.92f)).lighter(145);
        QColor const traceColor = QColor::fromRgb(wfColor(0.98f));
        writeColor(material->background, bg, 1.0f);
        writeColor(material->fill, fillColor, 0.42f);
        writeColor(material->glow, glowColor, 0.45f);
        writeColor(material->trace, traceColor, 1.0f);
        writeColor(material->peak, QColor(255, 255, 255), 0.65f);

        directNode->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
        QSGNode* extra = directNode->nextSibling();
        if (extra && !dynamic_cast<PanadapterSpectrumOverlayNode*>(extra))
            removeSceneGraphChildrenFrom(spectrumRoot, extra);
        static std::atomic_bool loggedDirectSpectrum {false};
        if (!loggedDirectSpectrum.exchange(true, std::memory_order_relaxed)) {
            qInfo().noquote()
                << "[GPUDBG] Panadapter spectrum GPU direct texture path active"
                << "api=" << waterfallGraphicsApiName(window()->rendererInterface()->graphicsApi())
                << "reason= QSG shader samples FFT R32F texture; no CPU vertices/readback";
        }
        }
#endif
    } else if (!m_spectrumImage.isNull() || gpuSpectrumGraph) {
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
            bool const softwareTexture =
                static_cast<QSGRendererInterface::GraphicsApi>(sceneGraphApiKey())
                == QSGRendererInterface::Software;
            if (softwareTexture) {
                auto* tex = window()->createTextureFromImage(
                    m_spectrumImage,
                    QQuickWindow::CreateTextureOptions(QQuickWindow::TextureHasAlphaChannel));
                if (tex) {
                    tex->setFiltering(QSGTexture::Linear);
                    sn->setTexture(tex);
                }
            } else {
                auto* tex = dynamic_cast<DecodiumRhiImageTexture*>(sn->texture());
                if (!tex || tex->textureSize() != m_spectrumImage.size()
                    || !tex->hasAlphaChannel() || tex->failed()) {
                    tex = new DecodiumRhiImageTexture(true);
                    tex->setFiltering(QSGTexture::Linear);
                    sn->setTexture(tex);
                }
                tex->uploadFullImage(m_spectrumImage, true);
                sn->setFiltering(QSGTexture::Linear);
            }
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

    paintSpectrumNodeUs += monotonicUs() - spectrumNodeStartUs;

    {
        qint64 const overlayStartUs = monotonicUs();
        updateSpectrumOverlayNode(spectrumRoot, w, specH, gpuDirectReady, gpuSpectrumGraph);
        paintOverlayUs += monotonicUs() - overlayStartUs;
    }

    // Waterfall node (bottom) — ring buffer: due draw calls per wrap-around
    qint64 const waterfallNodeStartUs = monotonicUs();
    ScopeExit waterfallMetric([&paintWaterfallNodeUs, waterfallNodeStartUs]() {
        paintWaterfallNodeUs += monotonicUs() - waterfallNodeStartUs;
    });
    int wfH = !m_renderWaterfallSize.isEmpty() ? m_renderWaterfallSize.height() : h - specH;
    int wfW = !m_renderWaterfallSize.isEmpty() ? m_renderWaterfallSize.width() : w;
    int rows = m_renderWaterfallHistoryRows;
    if (gpuDirectReady && m_gpuFft && m_gpuFft->directRows > 0 && !m_gpuFft->directWaterfallSize.isEmpty()) {
        rows = m_gpuFft->directRows;
        wfW = m_gpuFft->directWaterfallSize.width();
    } else if (!m_waterfallImage.isNull()) {
        rows = m_waterfallImage.height();
        wfW = m_waterfallImage.width();
    }
    if (wfH > 0 && wfW > 0 && rows > 0) {

        QSGNode* waterfallChild = nullptr;
        if (root->childCount() > 1 && root->firstChild())
            waterfallChild = root->firstChild()->nextSibling();

#ifdef DECODIUM_WATERFALL_SHADER_QSB
        if (m_useShaderWaterfall
            && (gpuDirectReady
                || (m_waterfallRawBinsWidth > 0 && !m_waterfallDbRows.isEmpty() && !m_waterfallDbRowParams.isEmpty())
                || !m_waterfallIntensityImage.isNull())) {
#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
            if (!m_shaderWaterfallBlocked) {
                qint64 const waterfallSetupStartUs = monotonicUs();
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
                float const blackThresh = waterfallBlackThresholdForLevel(m_blackLevel, gpuDirectReady);
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
                paintWaterfallSetupUs += monotonicUs() - waterfallSetupStartUs;

                if (material->paletteGeneration != m_paletteGeneration || !material->paletteTexture) {
                    qint64 const textureStartUs = monotonicUs();
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
                    paintWaterfallTextureUs += monotonicUs() - textureStartUs;
                    ++paintTextureCreateCount;
                    ++paintTextureFullUploads;
                }

                if (auto* paletteTexture = dynamic_cast<DecodiumRhiImageTexture*>(material->paletteTexture);
                    paletteTexture && paletteTexture->failed()) {
                    m_shaderWaterfallBlocked = true;
                    qWarning().noquote() << "[GPUDBG] Panadapter waterfall GPU palette texture failed; falling back to CPU";
                }

                if (gpuDirectReady) {
                    paintWaterfallPath = 1;
                    auto replaceExternalTexture = [&paintTextureCreateCount](QSGTexture*& slot,
                                                                             QRhiTexture* texture,
                                                                             QSize const& size,
                                                                             QSGTexture::Filtering filtering) {
                        auto* ext = dynamic_cast<DecodiumExternalRhiTexture*>(slot);
                        if (!ext || ext->rhiTexture() != texture || ext->textureSize() != size) {
                            delete slot;
                            slot = new DecodiumExternalRhiTexture(texture, size, false);
                            ++paintTextureCreateCount;
                        }
                        slot->setFiltering(filtering);
                    };

                    qint64 const textureStartUs = monotonicUs();
                    replaceExternalTexture(material->intensityTexture,
                                           m_gpuFft->directWaterfallTexture,
                                           m_gpuFft->directWaterfallSize,
                                           QSGTexture::Nearest);
                    replaceExternalTexture(material->rowParamsTexture,
                                           m_gpuFft->directRowParamsTexture,
                                           m_gpuFft->directRowParamsSize,
                                           QSGTexture::Nearest);
                    paintWaterfallTextureUs += monotonicUs() - textureStartUs;

                    if (!m_shaderWaterfallBlocked
                        && material->intensityTexture
                        && material->paletteTexture
                        && material->rowParamsTexture) {
                        qint64 const markStartUs = monotonicUs();
                        wn->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
                        paintWaterfallMarkUs += monotonicUs() - markStartUs;
                        qint64 const logStartUs = monotonicUs();
                        if (!m_loggedGpuWaterfallDetached) {
                            m_loggedGpuWaterfallDetached = true;
                            qInfo().noquote()
                                << "[GPUDBG] Panadapter waterfall GPU direct node detached from QImage fallback"
                                << "texture=" << QStringLiteral("%1x%2")
                                    .arg(m_gpuFft->directWaterfallSize.width())
                                    .arg(m_gpuFft->directWaterfallSize.height())
                                << "rows=" << rows
                                << "qimage_dependency=0";
                        }
                        if (!m_loggedWaterfallGpuUploadStats && m_wfWriteRow > 0) {
                            m_loggedWaterfallGpuUploadStats = true;
                            m_lastWaterfallGpuStatsRow = m_wfWriteRow;
                        }
                        logWaterfallRenderPath(true, "shader samples GPU-direct FFT ring texture; no CPU row upload/readback");
                        paintWaterfallLogUs += monotonicUs() - logStartUs;
                        return root;
                    }
                }

                RawDbUploadStats uploadStats;
                bool uploadedFullTexture = false;
                bool uploadedTextureData = false;
                if (!gpuDirectReady)
                    paintWaterfallPath = 2;
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
                    qint64 const textureStartUs = monotonicUs();
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
                    paintWaterfallTextureUs += monotonicUs() - textureStartUs;
                    paintTextureCreateCount += 2;
                    paintTextureUploadRows += rows;
                    paintTextureFullUploads += 2;
                } else {
                    int rowsToUpload = m_wfWriteRow - m_waterfallGpuUploadedWriteRow;
                    if (rowsToUpload >= rows) {
                        qint64 const textureStartUs = monotonicUs();
                        intensityTexture->uploadFullFloats(intensitySize, m_waterfallDbRows.constData());
                        rowParamsTexture->uploadFullFloats(rowParamsSize, m_waterfallDbRowParams.constData());
                        m_waterfallGpuUploadedWriteRow = m_wfWriteRow;
                        uploadStats = rawDbImageStats(m_waterfallDbRows, intensityW, rows);
                        uploadedFullTexture = true;
                        uploadedTextureData = true;
                        paintWaterfallTextureUs += monotonicUs() - textureStartUs;
                        paintTextureUploadRows += rows;
                        paintTextureFullUploads += 2;
                    } else if (rowsToUpload > 0) {
                        qint64 const textureStartUs = monotonicUs();
                        for (int i = 0; i < rowsToUpload; ++i) {
                            int const row = (m_waterfallGpuUploadedWriteRow + i) % rows;
                            float const* rawSrc = m_waterfallDbRows.constData() + row * intensityW;
                            accumulateRawDbStats(rawSrc, intensityW, uploadStats);
                            intensityTexture->uploadFloatRow(row, intensityW, rawSrc);
                            rowParamsTexture->uploadFloatRow(row, 2, m_waterfallDbRowParams.constData() + row * 2);
                        }
                        m_waterfallGpuUploadedWriteRow += rowsToUpload;
                        uploadedTextureData = true;
                        paintWaterfallTextureUs += monotonicUs() - textureStartUs;
                        paintTextureUploadRows += rowsToUpload;
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
                    qint64 const markStartUs = monotonicUs();
                    wn->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
                    paintWaterfallMarkUs += monotonicUs() - markStartUs;
                    qint64 const logStartUs = monotonicUs();
                    bool const shouldLogStats = uploadedTextureData
                        && m_wfWriteRow > 0
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
                    paintWaterfallLogUs += monotonicUs() - logStartUs;
                    return root;
                }
            }
	#else
	            paintWaterfallPath = 2;
	            int const intensityW = m_waterfallIntensityImage.width();
	            if (m_waterfallIntensityTextureImage.isNull() ||
	                m_waterfallIntensityTextureImage.width() != intensityW ||
	                m_waterfallIntensityTextureImage.height() != wfH) {
	                m_waterfallIntensityTextureImage = QImage(intensityW, wfH, QImage::Format_ARGB32_Premultiplied);
	                m_waterfallIntensityTextureImage.fill(QColor(0, 0, 0, 255));
	                m_loggedWaterfallGpuUploadStats = false;
	                m_lastWaterfallGpuStatsRow = -1;
            }

            qint64 const displayStartUs = monotonicUs();
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
            paintWaterfallDisplayUs += monotonicUs() - displayStartUs;

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

                qint64 const textureStartUs = monotonicUs();
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
                    ++paintTextureCreateCount;
                    ++paintTextureFullUploads;
                }
                paintWaterfallTextureUs += monotonicUs() - textureStartUs;

                if (!m_shaderWaterfallBlocked && material->intensityTexture && material->paletteTexture) {
                    qint64 const markStartUs = monotonicUs();
                    wn->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
                    paintWaterfallMarkUs += monotonicUs() - markStartUs;
                    qint64 const logStartUs = monotonicUs();
                    logWaterfallRenderPath(true, "shader raw-bin intensity + palette textures; frequency mapping + black/gain/gamma in shader");
                    paintWaterfallLogUs += monotonicUs() - logStartUs;
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
        {
            qint64 const logStartUs = monotonicUs();
            logWaterfallRenderPath(false, waterfallFallbackReason);
            paintWaterfallLogUs += monotonicUs() - logStartUs;
        }
        if (m_waterfallImage.isNull()) {
	            if (waterfallChild) {
	                root->removeChildNode(waterfallChild);
	                delete waterfallChild;
	            }
	            return root;
	        }
        paintWaterfallPath = 3;

	        if (auto* oldGeometry = dynamic_cast<QSGGeometryNode*>(waterfallChild)) {
	            if (!dynamic_cast<QSGSimpleTextureNode*>(oldGeometry)) {
                root->removeChildNode(oldGeometry);
                delete oldGeometry;
                waterfallChild = nullptr;
            }
        }

        if (!m_waterfallRgbValid) {
            qint64 const displayStartUs = monotonicUs();
            rebuildRgbWaterfallFromIntensity();
            paintWaterfallDisplayUs += monotonicUs() - displayStartUs;
        }

        qint64 const displayStartUs = monotonicUs();
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
        paintWaterfallDisplayUs += monotonicUs() - displayStartUs;

        qint64 const setupStartUs = monotonicUs();
        QSGSimpleTextureNode* wn = dynamic_cast<QSGSimpleTextureNode*>(waterfallChild);
        if (!wn) { wn = new QSGSimpleTextureNode(); wn->setOwnsTexture(true); root->appendChildNode(wn); }
        paintWaterfallSetupUs += monotonicUs() - setupStartUs;
#ifdef DECODIUM_QT_RHI_TEXTURE_UPLOAD
        qint64 const textureStartUs = monotonicUs();
        bool createdTexture = false;
        bool const softwareTexture =
            static_cast<QSGRendererInterface::GraphicsApi>(sceneGraphApiKey())
            == QSGRendererInterface::Software;
        if (softwareTexture) {
            auto* tex = window()->createTextureFromImage(
                m_waterfallDisplayImage,
                QQuickWindow::CreateTextureOptions(QQuickWindow::TextureIsOpaque));
            if (tex) {
                tex->setFiltering(QSGTexture::Nearest);
                wn->setTexture(tex);
                createdTexture = true;
            }
        } else {
            auto* tex = dynamic_cast<DecodiumRhiImageTexture*>(wn->texture());
            if (!tex || tex->textureSize() != m_waterfallDisplayImage.size()
                || tex->hasAlphaChannel() || tex->failed()) {
                tex = new DecodiumRhiImageTexture(false);
                tex->setFiltering(QSGTexture::Nearest);
                wn->setTexture(tex);
                createdTexture = true;
            }
            tex->uploadFullImage(m_waterfallDisplayImage, false);
            wn->setFiltering(QSGTexture::Nearest);
        }
        wn->setFiltering(QSGTexture::Nearest);
        paintWaterfallTextureUs += monotonicUs() - textureStartUs;
        if (createdTexture)
            ++paintTextureCreateCount;
        paintTextureUploadRows += wfH;
        ++paintTextureFullUploads;
#else
        qint64 const textureStartUs = monotonicUs();
        auto* tex = window()->createTextureFromImage(m_waterfallDisplayImage, QQuickWindow::CreateTextureOptions(QQuickWindow::TextureIsOpaque));
        tex->setFiltering(QSGTexture::Nearest);
        wn->setTexture(tex);
        paintWaterfallTextureUs += monotonicUs() - textureStartUs;
        ++paintTextureCreateCount;
        paintTextureUploadRows += wfH;
        ++paintTextureFullUploads;
#endif
        {
            qint64 const setupRectStartUs = monotonicUs();
	        wn->setRect(QRectF(0, specH, w, wfH));
            paintWaterfallSetupUs += monotonicUs() - setupRectStartUs;
            qint64 const markStartUs = monotonicUs();
	        wn->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
            paintWaterfallMarkUs += monotonicUs() - markStartUs;
        }
	    } else {
	        QSGNode* waterfallChild = nullptr;
	        if (root->childCount() > 1 && root->firstChild())
	            waterfallChild = root->firstChild()->nextSibling();
	        if (waterfallChild) {
	            root->removeChildNode(waterfallChild);
	            delete waterfallChild;
	        }
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
    // Fix 1.0.337: SOLO Ctrl+click sinistro su una label = chiama la stazione.
    // Il click sinistro semplice imposta SEMPRE la freq TX (comportamento classico);
    // i decode-label QML coprivano lo spettro e rubavano il click TX in FT8/FT4.
    if (ev->button() == Qt::LeftButton && (ev->modifiers() & Qt::ControlModifier)) {
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
