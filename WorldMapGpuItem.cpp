#include "WorldMapGpuItem.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QLineF>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QSet>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGNode>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QQuickWindow>
#include <QVector3D>
#include <QtAlgorithms>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace {

constexpr int kFrameMs = 80;
constexpr int kOpenGlFrameMs = 160;
constexpr int kContactLifetimeSeconds = 2 * 60;
constexpr int kMaxContacts = 40;
constexpr int kMaxVisibleContacts = 28;
constexpr int kMaxVisibleContactLabels = 20;
constexpr int kRoleDowngradeHoldSeconds = 75;
constexpr int kGreatCircleSteps = 56;
constexpr qint64 kGpuProfileLogMs = 5000;

const char* liveMapGraphicsApiName(QSGRendererInterface::GraphicsApi api)
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

void clearNode(QSGNode* node)
{
    while (auto* child = node->firstChild()) {
        node->removeChildNode(child);
        delete child;
    }
}

class GreylineLayerNode final : public QSGNode {};

class GeometryLayerNode final : public QSGNode
{
public:
    QSGGeometryNode* gridLines {nullptr};
    QSGGeometryNode* genericPaths {nullptr};
    QSGGeometryNode* incomingPaths {nullptr};
    QSGGeometryNode* outgoingPaths {nullptr};
    QSGGeometryNode* genericHaloMarkers {nullptr};
    QSGGeometryNode* incomingHaloMarkers {nullptr};
    QSGGeometryNode* outgoingHaloMarkers {nullptr};
    QSGGeometryNode* bandHaloMarkers {nullptr};
    QSGGeometryNode* genericCoreMarkers {nullptr};
    QSGGeometryNode* incomingCoreMarkers {nullptr};
    QSGGeometryNode* outgoingCoreMarkers {nullptr};
    QSGGeometryNode* bandCoreMarkers {nullptr};
    QSGGeometryNode* legendIncomingLine {nullptr};
    QSGGeometryNode* legendOutgoingLine {nullptr};
    QSGGeometryNode* legendBandMarker {nullptr};
};

class LabelLayerNode final : public QSGNode
{
public:
    ~LabelLayerNode() override
    {
        clearNode(this);
        qDeleteAll(textureCache);
    }

    QHash<QString, QSGTexture*> textureCache;
    QVector<QSGSimpleTextureNode*> labelNodes;
};

class AnimationLayerNode final : public QSGNode
{
public:
    QSGGeometryNode* genericArrows {nullptr};
    QSGGeometryNode* incomingArrows {nullptr};
    QSGGeometryNode* outgoingArrows {nullptr};
};

class MapLayerNode final : public QSGNode
{
public:
    ~MapLayerNode() override
    {
        clearNode(this);
        delete texture;
    }

    QSGTexture* texture {nullptr};
    QVector<QSGSimpleTextureNode*> tileNodes;
};

qint64 monotonicNowMs()
{
    static QElapsedTimer timer;
    static bool started = false;
    if (!started) {
        timer.start();
        started = true;
    }
    return timer.elapsed();
}

QColor colorForRole(WorldMapGpuItem::PathRole role)
{
    switch (role) {
    case WorldMapGpuItem::PathRole::IncomingToMe:
        return QColor(255, 126, 92, 225);
    case WorldMapGpuItem::PathRole::OutgoingFromMe:
        return QColor(84, 238, 165, 225);
    case WorldMapGpuItem::PathRole::BandOnly:
        return QColor(255, 212, 96, 230);
    case WorldMapGpuItem::PathRole::Generic:
    default:
        return QColor(92, 190, 255, 210);
    }
}

int rolePriority(WorldMapGpuItem::PathRole role)
{
    switch (role) {
    case WorldMapGpuItem::PathRole::IncomingToMe:
        return 4;
    case WorldMapGpuItem::PathRole::OutgoingFromMe:
        return 3;
    case WorldMapGpuItem::PathRole::Generic:
        return 2;
    case WorldMapGpuItem::PathRole::BandOnly:
    default:
        return 1;
    }
}

QString normalizeMapCall(QString call)
{
    call = call.trimmed().toUpper();
    call.remove(QLatin1Char('<'));
    call.remove(QLatin1Char('>'));
    return call;
}

bool sameStationCall(const QString& a, const QString& b)
{
    QString const lhs = normalizeMapCall(a);
    QString const rhs = normalizeMapCall(b);
    if (lhs.isEmpty() || rhs.isEmpty()) {
        return false;
    }
    return lhs == rhs
        || lhs.startsWith(rhs + QLatin1Char('/'))
        || rhs.startsWith(lhs + QLatin1Char('/'));
}

QColor withAlpha(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return color;
}

bool isInViewport(const QRectF& rect, const QPointF& point, qreal margin)
{
    return rect.adjusted(-margin, -margin, margin, margin).contains(point);
}

QVector<QPointF> greatCircle(const QPointF& startLonLat, const QPointF& endLonLat, int steps)
{
    auto toVector = [](const QPointF& lonLat) -> QVector3D {
        double const lon = qDegreesToRadians(lonLat.x());
        double const lat = qDegreesToRadians(lonLat.y());
        double const cosLat = std::cos(lat);
        return QVector3D(cosLat * std::cos(lon), cosLat * std::sin(lon), std::sin(lat));
    };

    auto toLonLat = [](const QVector3D& v) -> QPointF {
        QVector3D const n = v.normalized();
        double const lon = std::atan2(n.y(), n.x());
        double const lat = std::asin(qBound(-1.0f, n.z(), 1.0f));
        return QPointF(qRadiansToDegrees(lon), qRadiansToDegrees(lat));
    };

    QVector3D const a = toVector(startLonLat);
    QVector3D const b = toVector(endLonLat);
    double dot = QVector3D::dotProduct(a, b);
    dot = qBound(-1.0, dot, 1.0);
    double const omega = std::acos(dot);
    double const sinOmega = std::sin(omega);

    QVector<QPointF> points;
    points.reserve(steps + 1);
    for (int i = 0; i <= steps; ++i) {
        double const t = static_cast<double>(i) / static_cast<double>(steps);
        QVector3D v;
        if (std::abs(sinOmega) < 1e-6) {
            v = a * float(1.0 - t) + b * float(t);
        } else {
            double const wa = std::sin((1.0 - t) * omega) / sinOmega;
            double const wb = std::sin(t * omega) / sinOmega;
            v = a * float(wa) + b * float(wb);
        }
        points.push_back(toLonLat(v));
    }
    return points;
}

QPointF pointOnPath(const QVector<QPointF>& points, qreal progress)
{
    if (points.isEmpty()) {
        return {};
    }
    if (points.size() == 1) {
        return points.first();
    }

    struct Segment {
        QPointF a;
        QPointF b;
        qreal length {0.0};
    };

    QVector<Segment> segments;
    segments.reserve(points.size() - 1);
    qreal totalLength = 0.0;
    for (int i = 0; i < points.size() - 1; ++i) {
        qreal const length = QLineF(points[i], points[i + 1]).length();
        if (length <= 2.0) {
            continue;
        }
        segments.push_back({points[i], points[i + 1], length});
        totalLength += length;
    }

    if (segments.isEmpty() || totalLength <= 0.0) {
        return points.at(points.size() / 2);
    }

    qreal const targetDistance = qBound<qreal>(0.0, progress, 0.999999) * totalLength;
    qreal walked = 0.0;
    for (const auto& segment : segments) {
        if (walked + segment.length >= targetDistance) {
            qreal const t = (targetDistance - walked) / segment.length;
            return segment.a + (segment.b - segment.a) * t;
        }
        walked += segment.length;
    }
    return segments.last().b;
}

bool arrowOnPath(const QVector<QPointF>& points, qreal progress, QPolygonF* arrow)
{
    if (!arrow || points.size() < 2) {
        return false;
    }

    QPointF const tip = pointOnPath(points, progress);
    QPointF const tail = pointOnPath(points, qMax<qreal>(0.0, progress - 0.022));
    QPointF direction = tip - tail;
    qreal const len = std::hypot(direction.x(), direction.y());
    if (len <= 0.001) {
        return false;
    }

    QPointF const u = direction / len;
    QPointF const n {-u.y(), u.x()};
    qreal const arrowLen = 9.0;
    qreal const arrowWid = 4.2;
    *arrow = QPolygonF {tip, tip - u * arrowLen + n * arrowWid, tip - u * arrowLen - n * arrowWid};
    return true;
}

QSGGeometryNode* makeTexturedQuadNode(const QRectF& rect, QSGMaterial* material)
{
    auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
    geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
    auto* vertices = geometry->vertexDataAsTexturedPoint2D();
    vertices[0].set(static_cast<float>(rect.left()), static_cast<float>(rect.top()), 0.0f, 0.0f);
    vertices[1].set(static_cast<float>(rect.right()), static_cast<float>(rect.top()), 1.0f, 0.0f);
    vertices[2].set(static_cast<float>(rect.left()), static_cast<float>(rect.bottom()), 0.0f, 1.0f);
    vertices[3].set(static_cast<float>(rect.right()), static_cast<float>(rect.bottom()), 1.0f, 1.0f);

    auto* node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

void appendMapTileNodes(MapLayerNode* layer,
                        const QRectF& rect,
                        double centerLon,
                        double centerLat,
                        double spanLon,
                        double spanLat,
                        int textureWidth,
                        int textureHeight)
{
    if (!layer || !layer->texture || rect.isEmpty() || textureWidth <= 0 || textureHeight <= 0) {
        if (layer) {
            while (!layer->tileNodes.isEmpty()) {
                auto* node = layer->tileNodes.takeLast();
                layer->removeChildNode(node);
                delete node;
            }
        }
        return;
    }

    double const topLat = qBound(-90.0, centerLat + 0.5 * spanLat, 90.0);
    double const bottomLat = qBound(-90.0, centerLat - 0.5 * spanLat, 90.0);
    qreal const srcY0 = static_cast<qreal>((90.0 - topLat) / 180.0 * textureHeight);
    qreal const srcY1 = static_cast<qreal>((90.0 - bottomLat) / 180.0 * textureHeight);
    qreal const srcH = qMax<qreal>(1.0, srcY1 - srcY0);
    double const leftLon = centerLon - 0.5 * spanLon;
    double const rightLon = centerLon + 0.5 * spanLon;

    struct Tile {
        QRectF target;
        QRectF source;
    };
    QVector<Tile> tiles;
    tiles.reserve(3);

    for (int k = -2; k <= 2; ++k) {
        double const lonStart = -180.0 + 360.0 * k;
        double const lonEnd = lonStart + 360.0;

        double const visibleLon0 = qMax(leftLon, lonStart);
        double const visibleLon1 = qMin(rightLon, lonEnd);
        if (visibleLon1 <= visibleLon0) {
            continue;
        }

        qreal const x0 = rect.left() + static_cast<qreal>((visibleLon0 - leftLon) / spanLon) * rect.width();
        qreal const x1 = rect.left() + static_cast<qreal>((visibleLon1 - leftLon) / spanLon) * rect.width();
        qreal const srcX0 = static_cast<qreal>((visibleLon0 - lonStart) / 360.0 * textureWidth);
        qreal const srcX1 = static_cast<qreal>((visibleLon1 - lonStart) / 360.0 * textureWidth);

        tiles.push_back({QRectF(x0, rect.top(), x1 - x0, rect.height()),
                         QRectF(srcX0, srcY0, qMax<qreal>(1.0, srcX1 - srcX0), srcH)});
    }

    while (layer->tileNodes.size() < tiles.size()) {
        auto* node = new QSGSimpleTextureNode;
        node->setTexture(layer->texture);
        node->setOwnsTexture(false);
        node->setFiltering(QSGTexture::Linear);
        layer->appendChildNode(node);
        layer->tileNodes.push_back(node);
    }

    while (layer->tileNodes.size() > tiles.size()) {
        auto* node = layer->tileNodes.takeLast();
        layer->removeChildNode(node);
        delete node;
    }

    for (int i = 0; i < tiles.size(); ++i) {
        auto* node = layer->tileNodes[i];
        node->setTexture(layer->texture);
        node->setRect(tiles[i].target);
        node->setSourceRect(tiles[i].source);
    }
}

#ifdef DECODIUM_LIVEMAP_GREYLINE_QSB
class GreylineMaterial final : public QSGMaterial
{
public:
    GreylineMaterial()
    {
        setFlag(QSGMaterial::Blending);
        setFlag(QSGMaterial::RequiresFullMatrix);
    }

    QSGMaterialType* type() const override
    {
        static QSGMaterialType type;
        return &type;
    }

    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode) const override;

    int compare(const QSGMaterial* other) const override
    {
        auto const* rhs = static_cast<const GreylineMaterial*>(other);
        for (int i = 0; i < 4; ++i) {
            if (sunParams[i] < rhs->sunParams[i])
                return -1;
            if (sunParams[i] > rhs->sunParams[i])
                return 1;
        }
        return 0;
    }

    float sunParams[4] = {0.0f, 0.0f, 1.0f, 0.62f};
    float viewParams[4] = {0.0f, 0.0f, 360.0f, 180.0f};
};

class GreylineShader final : public QSGMaterialShader
{
public:
    GreylineShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/livemap_greyline.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/livemap_greyline.frag.qsb"));
    }

    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial*) override
    {
        QByteArray* uniformData = state.uniformData();
        if (uniformData->size() < 112) {
            uniformData->resize(112);
            std::memset(uniformData->data(), 0, static_cast<size_t>(uniformData->size()));
        }
        auto* material = static_cast<GreylineMaterial*>(newMaterial);
        QMatrix4x4 const matrix = state.combinedMatrix();
        std::memcpy(uniformData->data(), matrix.constData(), 64);
        float const opacity = state.opacity();
        std::memcpy(uniformData->data() + 64, &opacity, 4);
        std::memcpy(uniformData->data() + 80, material->sunParams, sizeof(material->sunParams));
        std::memcpy(uniformData->data() + 96, material->viewParams, sizeof(material->viewParams));
        return true;
    }
};

QSGMaterialShader* GreylineMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new GreylineShader;
}
#endif

void colorToFloat4(const QColor& color, float out[4])
{
    QColor const c = color.toRgb();
    out[0] = static_cast<float>(c.redF());
    out[1] = static_cast<float>(c.greenF());
    out[2] = static_cast<float>(c.blueF());
    out[3] = static_cast<float>(c.alphaF());
}

QFont liveMapLabelFont()
{
    QFont font = QGuiApplication::font();
    if (font.pixelSize() > 0) {
#ifdef Q_OS_WIN
        font.setPixelSize(qBound(8, font.pixelSize() - 3, 10));
#else
        font.setPixelSize(qBound(9, font.pixelSize() - 1, 12));
#endif
    } else {
        qreal pointSize = font.pointSizeF();
        if (pointSize <= 0.0) {
            pointSize = 9.0;
        }
#ifdef Q_OS_WIN
        font.setPointSizeF(qBound(6.5, pointSize - 1.75, 8.0));
#else
        font.setPointSizeF(qBound(7.5, pointSize - 0.5, 9.5));
#endif
    }
    font.setBold(true);
    return font;
}

QImage renderLabelTexture(const QString& text, const QColor& color)
{
    QFont const font = liveMapLabelFont();
    QFontMetricsF const fm(font);
    QSize const size(qMax(1, qCeil(fm.horizontalAdvance(text) + 6.0)),
                     qMax(1, qCeil(fm.height() + 4.0)));

    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setFont(font);
    QPointF const baseline(2.0, 2.0 + fm.ascent());
    painter.setPen(QColor(0, 8, 18, 190));
    painter.drawText(baseline + QPointF(1.0, 1.0), text);
    painter.setPen(color);
    painter.drawText(baseline, text);
    painter.end();
    return image;
}

QRectF labelTextureRectForBaseline(const QString& text, const QPointF& baseline)
{
    QFont const font = liveMapLabelFont();
    QFontMetricsF const fm(font);
    return QRectF(baseline + QPointF(-2.0, -fm.ascent() - 2.0),
                  QSizeF(fm.horizontalAdvance(text) + 6.0, fm.height() + 4.0));
}

QString labelTextureKey(const QString& text, const QColor& color)
{
    return text
        + QLatin1Char('|')
        + QString::number(static_cast<qulonglong>(color.rgba()), 16);
}

QSGTexture* cachedLabelTexture(LabelLayerNode* layer, QQuickWindow* window, const QString& text, const QColor& color)
{
    if (!layer || !window || text.isEmpty()) {
        return nullptr;
    }

    QString const key = labelTextureKey(text, color);
    QSGTexture* texture = layer->textureCache.value(key, nullptr);
    if (texture) {
        return texture;
    }

    texture = window->createTextureFromImage(renderLabelTexture(text, color));
    if (!texture) {
        return nullptr;
    }
    texture->setFiltering(QSGTexture::Linear);
    layer->textureCache.insert(key, texture);
    return texture;
}

void pruneLabelTextureCache(LabelLayerNode* layer, const QSet<QString>& liveKeys)
{
    if (!layer) {
        return;
    }

    for (auto it = layer->textureCache.begin(); it != layer->textureCache.end(); ) {
        if (liveKeys.contains(it.key())) {
            ++it;
            continue;
        }
        delete it.value();
        it = layer->textureCache.erase(it);
    }
}

#ifdef DECODIUM_LIVEMAP_MARKER_QSB
class MarkerMaterial final : public QSGMaterial
{
public:
    MarkerMaterial()
    {
        setFlag(QSGMaterial::Blending);
        setFlag(QSGMaterial::RequiresFullMatrix);
    }

    QSGMaterialType* type() const override
    {
        static QSGMaterialType type;
        return &type;
    }

    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode) const override;

    int compare(const QSGMaterial* other) const override
    {
        auto const* rhs = static_cast<const MarkerMaterial*>(other);
        for (int i = 0; i < 4; ++i) {
            if (color[i] < rhs->color[i])
                return -1;
            if (color[i] > rhs->color[i])
                return 1;
        }
        return 0;
    }

    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

class MarkerShader final : public QSGMaterialShader
{
public:
    MarkerShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/livemap_marker.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/livemap_marker.frag.qsb"));
    }

    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial*) override
    {
        QByteArray* uniformData = state.uniformData();
        if (uniformData->size() < 96) {
            uniformData->resize(96);
            std::memset(uniformData->data(), 0, static_cast<size_t>(uniformData->size()));
        }
        auto* material = static_cast<MarkerMaterial*>(newMaterial);
        QMatrix4x4 const matrix = state.combinedMatrix();
        std::memcpy(uniformData->data(), matrix.constData(), 64);
        float const opacity = state.opacity();
        std::memcpy(uniformData->data() + 64, &opacity, 4);
        std::memcpy(uniformData->data() + 80, material->color, sizeof(material->color));
        return true;
    }
};

QSGMaterialShader* MarkerMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new MarkerShader;
}
#endif

struct PathVertex {
    float sourceLon;
    float sourceLat;
    float destinationLon;
    float destinationLat;
    float progress;
};

const QSGGeometry::AttributeSet& pathAttributes()
{
    static QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::create(0, 4, QSGGeometry::FloatType),
        QSGGeometry::Attribute::create(1, 1, QSGGeometry::FloatType)
    };
    static QSGGeometry::AttributeSet attributeSet {
        2,
        sizeof(PathVertex),
        attributes
    };
    return attributeSet;
}

#ifdef DECODIUM_LIVEMAP_PATH_QSB
class PathMaterial final : public QSGMaterial
{
public:
    PathMaterial()
    {
        setFlag(QSGMaterial::Blending);
        setFlag(QSGMaterial::RequiresFullMatrix);
    }

    QSGMaterialType* type() const override
    {
        static QSGMaterialType type;
        return &type;
    }

    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode) const override;

    int compare(const QSGMaterial* other) const override
    {
        auto const* rhs = static_cast<const PathMaterial*>(other);
        for (int i = 0; i < 4; ++i) {
            if (color[i] < rhs->color[i])
                return -1;
            if (color[i] > rhs->color[i])
                return 1;
            if (viewParams[i] < rhs->viewParams[i])
                return -1;
            if (viewParams[i] > rhs->viewParams[i])
                return 1;
            if (rectParams[i] < rhs->rectParams[i])
                return -1;
            if (rectParams[i] > rhs->rectParams[i])
                return 1;
        }
        return 0;
    }

    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float viewParams[4] = {0.0f, 0.0f, 360.0f, 180.0f};
    float rectParams[4] = {0.0f, 0.0f, 1.0f, 1.0f};
};

class PathShader final : public QSGMaterialShader
{
public:
    PathShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/livemap_path.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/livemap_path.frag.qsb"));
    }

    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial*) override
    {
        QByteArray* uniformData = state.uniformData();
        if (uniformData->size() < 128) {
            uniformData->resize(128);
            std::memset(uniformData->data(), 0, static_cast<size_t>(uniformData->size()));
        }
        auto* material = static_cast<PathMaterial*>(newMaterial);
        QMatrix4x4 const matrix = state.combinedMatrix();
        std::memcpy(uniformData->data(), matrix.constData(), 64);
        float const opacity = state.opacity();
        std::memcpy(uniformData->data() + 64, &opacity, 4);
        std::memcpy(uniformData->data() + 80, material->color, sizeof(material->color));
        std::memcpy(uniformData->data() + 96, material->viewParams, sizeof(material->viewParams));
        std::memcpy(uniformData->data() + 112, material->rectParams, sizeof(material->rectParams));
        return true;
    }
};

QSGMaterialShader* PathMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new PathShader;
}
#endif

QSGGeometryNode* ensureFlatLineNode(QSGNode* parent, QSGGeometryNode*& node, const QColor& color)
{
    if (!node) {
        auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawLines);
        geometry->setLineWidth(1.0f);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);

        auto* material = new QSGFlatColorMaterial;
        material->setColor(color);

        node = new QSGGeometryNode;
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
        parent->appendChildNode(node);
    }
    if (auto* material = static_cast<QSGFlatColorMaterial*>(node->material())) {
        material->setColor(color);
        node->markDirty(QSGNode::DirtyMaterial);
    }
    return node;
}

void updateFlatLineNode(QSGNode* parent, QSGGeometryNode*& node, const QVector<QPointF>& points, const QColor& color)
{
    auto* lineNode = ensureFlatLineNode(parent, node, color);
    auto* geometry = lineNode->geometry();
    geometry->allocate(points.size());
    geometry->setDrawingMode(QSGGeometry::DrawLines);
    geometry->setLineWidth(1.0f);
    auto* vertices = geometry->vertexDataAsPoint2D();
    for (int i = 0; i < points.size(); ++i) {
        vertices[i].set(static_cast<float>(points[i].x()), static_cast<float>(points[i].y()));
    }
    geometry->markVertexDataDirty();
    lineNode->markDirty(QSGNode::DirtyGeometry);
}

void updateTriangleNode(QSGNode* parent, QSGGeometryNode*& node, const QVector<QPointF>& points, const QColor& color)
{
    if (!node) {
        auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);

        auto* material = new QSGFlatColorMaterial;
        material->setColor(color);

        node = new QSGGeometryNode;
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
        parent->appendChildNode(node);
    }
    if (auto* material = static_cast<QSGFlatColorMaterial*>(node->material())) {
        material->setColor(color);
        node->markDirty(QSGNode::DirtyMaterial);
    }
    auto* geometry = node->geometry();
    geometry->allocate(points.size());
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vertices = geometry->vertexDataAsPoint2D();
    for (int i = 0; i < points.size(); ++i) {
        vertices[i].set(static_cast<float>(points[i].x()), static_cast<float>(points[i].y()));
    }
    geometry->markVertexDataDirty();
    node->markDirty(QSGNode::DirtyGeometry);
}

void updateMarkerNode(QSGNode* parent, QSGGeometryNode*& node,
                      const QVector<QPointF>& points, const QColor& color, float radius)
{
#ifdef DECODIUM_LIVEMAP_MARKER_QSB
    if (!node) {
        auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);

        auto* material = new MarkerMaterial;

        node = new QSGGeometryNode;
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
        parent->appendChildNode(node);
    }
    if (auto* material = static_cast<MarkerMaterial*>(node->material())) {
        colorToFloat4(color, material->color);
        node->markDirty(QSGNode::DirtyMaterial);
    }
    auto* geometry = node->geometry();
    geometry->allocate(points.size() * 6);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vertices = geometry->vertexDataAsTexturedPoint2D();
    int out = 0;
    auto write = [&](const QPointF& p, float dx, float dy, float u, float v) {
        vertices[out++].set(static_cast<float>(p.x()) + dx * radius,
                            static_cast<float>(p.y()) + dy * radius,
                            u, v);
    };
    for (const QPointF& p : points) {
        write(p, -1.0f, -1.0f, 0.0f, 0.0f);
        write(p,  1.0f, -1.0f, 1.0f, 0.0f);
        write(p, -1.0f,  1.0f, 0.0f, 1.0f);
        write(p,  1.0f, -1.0f, 1.0f, 0.0f);
        write(p,  1.0f,  1.0f, 1.0f, 1.0f);
        write(p, -1.0f,  1.0f, 0.0f, 1.0f);
    }
    geometry->markVertexDataDirty();
    node->markDirty(QSGNode::DirtyGeometry);
#else
    Q_UNUSED(parent);
    Q_UNUSED(node);
    Q_UNUSED(points);
    Q_UNUSED(color);
    Q_UNUSED(radius);
#endif
}

void updatePathNode(QSGNode* parent, QSGGeometryNode*& node,
                    const QVector<WorldMapGpuItem::PathLine>& paths,
                    const QColor& color,
                    double centerLon,
                    double centerLat,
                    double spanLon,
                    double spanLat,
                    const QRectF& rect)
{
#ifdef DECODIUM_LIVEMAP_PATH_QSB
    if (!node) {
        auto* geometry = new QSGGeometry(pathAttributes(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawLines);
        geometry->setLineWidth(1.0f);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);

        auto* material = new PathMaterial;

        node = new QSGGeometryNode;
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
        parent->appendChildNode(node);
    }
    if (auto* material = static_cast<PathMaterial*>(node->material())) {
        colorToFloat4(color, material->color);
        material->viewParams[0] = static_cast<float>(centerLon);
        material->viewParams[1] = static_cast<float>(centerLat);
        material->viewParams[2] = static_cast<float>(spanLon);
        material->viewParams[3] = static_cast<float>(spanLat);
        material->rectParams[0] = static_cast<float>(rect.left());
        material->rectParams[1] = static_cast<float>(rect.top());
        material->rectParams[2] = static_cast<float>(rect.width());
        material->rectParams[3] = static_cast<float>(rect.height());
        node->markDirty(QSGNode::DirtyMaterial);
    }
    auto* geometry = node->geometry();
    int const vertexCount = paths.size() * kGreatCircleSteps * 2;
    geometry->allocate(vertexCount);
    geometry->setDrawingMode(QSGGeometry::DrawLines);
    geometry->setLineWidth(1.0f);
    auto* vertices = static_cast<PathVertex*>(geometry->vertexData());
    int out = 0;
    for (const WorldMapGpuItem::PathLine& path : paths) {
        for (int i = 0; i < kGreatCircleSteps; ++i) {
            float const t0 = static_cast<float>(i) / static_cast<float>(kGreatCircleSteps);
            float const t1 = static_cast<float>(i + 1) / static_cast<float>(kGreatCircleSteps);
            vertices[out++] = {static_cast<float>(path.sourceLonLat.x()),
                               static_cast<float>(path.sourceLonLat.y()),
                               static_cast<float>(path.destinationLonLat.x()),
                               static_cast<float>(path.destinationLonLat.y()),
                               t0};
            vertices[out++] = {static_cast<float>(path.sourceLonLat.x()),
                               static_cast<float>(path.sourceLonLat.y()),
                               static_cast<float>(path.destinationLonLat.x()),
                               static_cast<float>(path.destinationLonLat.y()),
                               t1};
        }
    }
    geometry->markVertexDataDirty();
    node->markDirty(QSGNode::DirtyGeometry);
#else
    Q_UNUSED(parent);
    Q_UNUSED(node);
    Q_UNUSED(paths);
    Q_UNUSED(color);
    Q_UNUSED(centerLon);
    Q_UNUSED(centerLat);
    Q_UNUSED(spanLon);
    Q_UNUSED(spanLat);
    Q_UNUSED(rect);
#endif
}

}

WorldMapGpuItem::WorldMapGpuItem(QQuickItem* parent)
    : QQuickItem(parent)
    , m_mapImage(buildMapTexture())
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);

    m_frameTimer.setInterval(m_frameIntervalMs);
    connect(&m_frameTimer, &QTimer::timeout, this, [this]() {
        if (m_active && isVisible()) {
            m_animationPhase = std::fmod(m_animationPhase + (m_conservativeRenderer ? 0.006 : 0.010), 1.0);
            bool const pruned = pruneExpiredContacts();
            updateViewportTargets();
            bool const viewportChanged = smoothViewport();
            if (pruned || viewportChanged) {
                m_greylineGeometryDirty = true;
                m_geometryDirty = true;
            }
            update();
        }
    });
    m_frameTimer.start();

    qInfo() << "[WorldMapGpuItem] QSG renderer enabled; CPU fallback remains WorldMapItem";
}

void WorldMapGpuItem::setHomeGrid(const QString& grid)
{
    QPointF lonLat;
    if (maidenheadToLonLat(grid, &lonLat)) {
        m_homeGrid = grid.trimmed().toUpper();
        m_homeLonLat = lonLat;
        m_hasHome = true;
    } else {
        m_homeGrid.clear();
        m_homeLonLat = QPointF();
        m_hasHome = false;
    }
    updateViewportTargets();
    markDirty();
}

void WorldMapGpuItem::setGreylineEnabled(bool enabled)
{
    if (m_greylineEnabled == enabled) {
        return;
    }
    m_greylineEnabled = enabled;
    m_greylineGeometryDirty = true;
    if (isVisible()) {
        update();
    }
}

void WorldMapGpuItem::setDistanceInMiles(bool enabled)
{
    m_distanceInMiles = enabled;
    Q_UNUSED(m_distanceInMiles);
}

void WorldMapGpuItem::setTransmitState(bool transmitting,
                                       const QString& targetCall,
                                       const QString& targetGrid,
                                       const QString& mode)
{
    QString normalizedCall = targetCall.trimmed().toUpper();
    normalizedCall.remove('<');
    normalizedCall.remove('>');

    QString normalizedGrid = targetGrid.trimmed().toUpper();
    if (normalizedGrid.size() > 6) {
        normalizedGrid = normalizedGrid.left(6);
    }

    int travelMs = 5600;
    QString const modeUpper = mode.trimmed().toUpper();
    if (modeUpper == QLatin1String("FT2")) {
        travelMs = 4000;
    } else if (modeUpper == QLatin1String("FT4")) {
        travelMs = 5400;
    } else if (modeUpper == QLatin1String("FT8")) {
        travelMs = 6200;
    }

    bool const resetProgress = transmitting
        && (!m_transmitting
            || normalizedCall != m_txTargetCall
            || normalizedGrid != m_txTargetGrid
            || travelMs != m_txTravelMs);

    m_transmitting = transmitting;
    m_txTargetCall = transmitting ? normalizedCall : QString();
    m_txTargetGrid = transmitting ? normalizedGrid : QString();
    m_txTravelMs = travelMs;
    if (resetProgress) {
        m_txStartMs = QDateTime::currentMSecsSinceEpoch();
    } else if (!transmitting) {
        m_txStartMs = 0;
    }
    if (isVisible()) {
        update();
    }
}

void WorldMapGpuItem::clearContacts()
{
    if (m_contacts.isEmpty()) {
        return;
    }
    m_contacts.clear();
    updateViewportTargets();
    markDirty();
}

void WorldMapGpuItem::downgradeContactToBand(const QString& call)
{
    QString const normalizedCall = normalizeMapCall(call);
    if (normalizedCall.isEmpty()) {
        return;
    }

    bool changed = false;
    qint64 const nowMs = monotonicNowMs();
    for (auto it = m_contacts.begin(); it != m_contacts.end(); ++it) {
        if (!sameStationCall(it.value().call, normalizedCall)) {
            continue;
        }
        if (it.value().role != PathRole::BandOnly) {
            it.value().role = PathRole::BandOnly;
            it.value().queuedDuringTx = false;
            it.value().lastSeenMonotonicMs = nowMs;
            changed = true;
        }
    }
    if (changed) {
        updateViewportTargets();
        markDirty();
    }
}

void WorldMapGpuItem::addContact(const QString& call,
                                 const QString& sourceGrid,
                                 const QString& destinationGrid,
                                 int role)
{
    QPointF sourceLonLat;
    QPointF destinationLonLat;
    if (!maidenheadToLonLat(sourceGrid, &sourceLonLat)) {
        return;
    }
    if (!maidenheadToLonLat(destinationGrid, &destinationLonLat)) {
        if (!m_hasHome) {
            return;
        }
        destinationLonLat = m_homeLonLat;
    }

    Contact contact;
    contact.call = normalizeMapCall(call);
    contact.sourceGrid = sourceGrid.trimmed().toUpper();
    contact.destinationGrid = destinationGrid.trimmed().toUpper();
    contact.sourceLonLat = sourceLonLat;
    contact.destinationLonLat = destinationLonLat;
    contact.role = pathRoleFromInt(role);
    contact.lastSeenMonotonicMs = monotonicNowMs();
    contact.queuedDuringTx = (contact.role == PathRole::IncomingToMe && m_transmitting);

    QString const key = contact.call.isEmpty() ? contact.sourceGrid : contact.call;
    auto existing = m_contacts.find(key);
    if (existing != m_contacts.end()) {
        const Contact& prev = existing.value();
        contact.queuedDuringTx = contact.queuedDuringTx || prev.queuedDuringTx;
        bool const directionalPrev = prev.role == PathRole::IncomingToMe || prev.role == PathRole::OutgoingFromMe;
        bool const downgradeToBand = contact.role == PathRole::BandOnly && directionalPrev;
        bool const stillFreshDirectional =
            (contact.lastSeenMonotonicMs - prev.lastSeenMonotonicMs) <= (kRoleDowngradeHoldSeconds * 1000LL);
        if (downgradeToBand && stillFreshDirectional) {
            return;
        }
    }

    pruneExpiredContacts();
    m_contacts.insert(key, contact);
    trimContactsToLimit();
    updateViewportTargets();
    markDirty();
}

void WorldMapGpuItem::addContactByLonLat(const QString& call,
                                         double sourceLon,
                                         double sourceLat,
                                         const QString& destinationGrid,
                                         int role)
{
    if (!std::isfinite(sourceLon) || !std::isfinite(sourceLat)) {
        return;
    }

    QPointF destinationLonLat;
    if (!maidenheadToLonLat(destinationGrid, &destinationLonLat)) {
        if (!m_hasHome) {
            return;
        }
        destinationLonLat = m_homeLonLat;
    }

    Contact contact;
    contact.call = normalizeMapCall(call);
    contact.destinationGrid = destinationGrid.trimmed().toUpper();
    contact.sourceLonLat = QPointF(wrapLongitude(sourceLon), qBound(-90.0, sourceLat, 90.0));
    contact.destinationLonLat = destinationLonLat;
    contact.role = pathRoleFromInt(role);
    contact.lastSeenMonotonicMs = monotonicNowMs();
    contact.queuedDuringTx = (contact.role == PathRole::IncomingToMe && m_transmitting);

    QString const key = contact.call.isEmpty()
        ? QStringLiteral("LL:%1:%2").arg(contact.sourceLonLat.x(), 0, 'f', 2).arg(contact.sourceLonLat.y(), 0, 'f', 2)
        : contact.call;
    auto existing = m_contacts.find(key);
    if (existing != m_contacts.end()) {
        const Contact& prev = existing.value();
        contact.queuedDuringTx = contact.queuedDuringTx || prev.queuedDuringTx;
        bool const directionalPrev = prev.role == PathRole::IncomingToMe || prev.role == PathRole::OutgoingFromMe;
        bool const downgradeToBand = contact.role == PathRole::BandOnly && directionalPrev;
        bool const stillFreshDirectional =
            (contact.lastSeenMonotonicMs - prev.lastSeenMonotonicMs) <= (kRoleDowngradeHoldSeconds * 1000LL);
        if (downgradeToBand && stillFreshDirectional) {
            return;
        }
    }

    pruneExpiredContacts();
    m_contacts.insert(key, contact);
    trimContactsToLimit();
    updateViewportTargets();
    markDirty();
}

bool WorldMapGpuItem::pruneExpiredContacts()
{
    bool changed = false;
    qint64 const nowMs = monotonicNowMs();
    qint64 const lifetimeMs = static_cast<qint64>(kContactLifetimeSeconds) * 1000;
    qint64 const downgradeMs = static_cast<qint64>(kRoleDowngradeHoldSeconds) * 1000;

    for (auto it = m_contacts.begin(); it != m_contacts.end(); ) {
        qint64 const ageMs = nowMs - it.value().lastSeenMonotonicMs;
        if (ageMs > lifetimeMs) {
            it = m_contacts.erase(it);
            changed = true;
            continue;
        }
        if ((it.value().role == PathRole::IncomingToMe || it.value().role == PathRole::OutgoingFromMe)
            && ageMs > downgradeMs) {
            it.value().role = PathRole::BandOnly;
            it.value().queuedDuringTx = false;
            changed = true;
        }
        ++it;
    }

    return changed;
}

void WorldMapGpuItem::trimContactsToLimit()
{
    while (m_contacts.size() > kMaxContacts) {
        auto oldest = m_contacts.end();
        for (auto it = m_contacts.begin(); it != m_contacts.end(); ++it) {
            if (oldest == m_contacts.end()
                || it.value().lastSeenMonotonicMs < oldest.value().lastSeenMonotonicMs) {
                oldest = it;
            }
        }
        if (oldest == m_contacts.end()) {
            break;
        }
        m_contacts.erase(oldest);
    }
}

void WorldMapGpuItem::setActive(bool active)
{
    m_active = active;
    if (m_active && isVisible()) {
        m_frameTimer.start();
        update();
    } else {
        m_frameTimer.stop();
    }
}

void WorldMapGpuItem::configureRendererPolicy()
{
    QQuickWindow* win = window();
    QSGRendererInterface* rendererInterface = win ? win->rendererInterface() : nullptr;
    QSGRendererInterface::GraphicsApi const api = rendererInterface
        ? rendererInterface->graphicsApi()
        : QSGRendererInterface::Unknown;

    bool const openGl = api == QSGRendererInterface::OpenGL;
    bool const conservativeRenderer = openGl;
    bool const forceOpenGlGreyline = qEnvironmentVariableIsSet("DECODIUM_ENABLE_OPENGL_LIVEMAP_GREYLINE");
    bool const greylineShaderAllowed = !openGl || forceOpenGlGreyline;
    int const frameIntervalMs = conservativeRenderer ? kOpenGlFrameMs : kFrameMs;

    if (m_rendererPolicyInitialized
        && m_conservativeRenderer == conservativeRenderer
        && m_greylineShaderAllowed == greylineShaderAllowed
        && m_frameIntervalMs == frameIntervalMs) {
        return;
    }

    bool const previousGreylineShaderAllowed = m_greylineShaderAllowed;
    m_rendererPolicyInitialized = true;
    m_conservativeRenderer = conservativeRenderer;
    m_greylineShaderAllowed = greylineShaderAllowed;

    if (previousGreylineShaderAllowed != m_greylineShaderAllowed) {
        m_greylineGeometryDirty = true;
    }

    if (m_frameIntervalMs != frameIntervalMs) {
        m_frameIntervalMs = frameIntervalMs;
        QMetaObject::invokeMethod(this, [this, frameIntervalMs]() {
            if (m_frameTimer.interval() != frameIntervalMs) {
                m_frameTimer.setInterval(frameIntervalMs);
            }
        }, Qt::QueuedConnection);
    }

    qInfo().nospace()
        << "[WorldMapGpuItem] renderer api=" << liveMapGraphicsApiName(api)
        << " conservative=" << (m_conservativeRenderer ? 1 : 0)
        << " frameMs=" << m_frameIntervalMs
        << " greylineShader=" << (m_greylineShaderAllowed ? 1 : 0)
        << (openGl && !forceOpenGlGreyline ? " reason=OpenGL_conservative" : "");
}

QSGNode* WorldMapGpuItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    if (!window()) {
        delete oldNode;
        return nullptr;
    }
    configureRendererPolicy();

    auto* root = oldNode ? oldNode : new QSGNode;

    QRectF const rect = mapRect();
    if (rect.isEmpty()) {
        clearNode(root);
        return root;
    }

    auto* mapLayer = dynamic_cast<MapLayerNode*>(root->firstChild());
    if (!mapLayer) {
        clearNode(root);
        mapLayer = new MapLayerNode;
        mapLayer->texture = window()->createTextureFromImage(m_mapImage);
        if (mapLayer->texture) {
            mapLayer->texture->setFiltering(QSGTexture::Linear);
            mapLayer->texture->setMipmapFiltering(QSGTexture::Linear);
        }
        root->appendChildNode(mapLayer);
    }
    appendMapTileNodes(mapLayer, rect,
                       m_viewCenterLon, m_viewCenterLat,
                       m_viewSpanLon, m_viewSpanLat,
                       m_mapImage.width(), m_mapImage.height());

    auto* greylineLayer = dynamic_cast<GreylineLayerNode*>(mapLayer->nextSibling());
    auto* geometryLayer = greylineLayer
        ? dynamic_cast<GeometryLayerNode*>(greylineLayer->nextSibling())
        : nullptr;
    auto* labelLayer = geometryLayer
        ? dynamic_cast<LabelLayerNode*>(geometryLayer->nextSibling())
        : nullptr;
    auto* animationLayer = labelLayer
        ? dynamic_cast<AnimationLayerNode*>(labelLayer->nextSibling())
        : nullptr;
    if (!greylineLayer || !geometryLayer || !labelLayer || !animationLayer) {
        while (auto* child = mapLayer->nextSibling()) {
            root->removeChildNode(child);
            delete child;
        }
        greylineLayer = new GreylineLayerNode;
        geometryLayer = new GeometryLayerNode;
        labelLayer = new LabelLayerNode;
        animationLayer = new AnimationLayerNode;
        root->appendChildNode(greylineLayer);
        root->appendChildNode(geometryLayer);
        root->appendChildNode(labelLayer);
        root->appendChildNode(animationLayer);
        m_greylineGeometryDirty = true;
        m_geometryDirty = true;
    }

    if (m_greylineEnabled && m_greylineShaderAllowed) {
#ifdef DECODIUM_LIVEMAP_GREYLINE_QSB
        QPointF const sun = subSolarLonLat();
        auto* greylineNode = dynamic_cast<QSGGeometryNode*>(greylineLayer->firstChild());
        if (!greylineNode || m_greylineGeometryDirty) {
            clearNode(greylineLayer);
            auto* material = new GreylineMaterial;
            greylineNode = makeTexturedQuadNode(rect, material);
            greylineLayer->appendChildNode(greylineNode);
            m_greylineGeometryDirty = false;
        }

        if (auto* material = static_cast<GreylineMaterial*>(greylineNode->material())) {
            material->sunParams[0] = static_cast<float>(sun.x());
            material->sunParams[1] = static_cast<float>(sun.y());
            material->sunParams[2] = 1.0f;
            material->sunParams[3] = 0.62f;
            material->viewParams[0] = static_cast<float>(m_viewCenterLon);
            material->viewParams[1] = static_cast<float>(m_viewCenterLat);
            material->viewParams[2] = static_cast<float>(m_viewSpanLon);
            material->viewParams[3] = static_cast<float>(m_viewSpanLat);
            greylineNode->markDirty(QSGNode::DirtyMaterial);
        }
#else
        static bool loggedNoGreylineShader = false;
        if (!loggedNoGreylineShader) {
            loggedNoGreylineShader = true;
            qInfo() << "[WorldMapGpuItem] greyline shader unavailable; QSG prototype continues without greyline overlay";
        }
#endif
    } else if (greylineLayer->firstChild()) {
        clearNode(greylineLayer);
    }

    if (m_geometryDirty) {
        rebuildGeometryBatch();
        updateFlatLineNode(geometryLayer, geometryLayer->gridLines, m_batch.gridLines, QColor(170, 210, 225, 42));
        updatePathNode(geometryLayer, geometryLayer->genericPaths, m_batch.genericPaths, colorForRole(PathRole::Generic),
                       m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect);
        updatePathNode(geometryLayer, geometryLayer->incomingPaths, m_batch.incomingPaths, colorForRole(PathRole::IncomingToMe),
                       m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect);
        updatePathNode(geometryLayer, geometryLayer->outgoingPaths, m_batch.outgoingPaths, colorForRole(PathRole::OutgoingFromMe),
                       m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect);

        updateMarkerNode(geometryLayer, geometryLayer->genericHaloMarkers,
                         m_batch.genericMarkers, withAlpha(colorForRole(PathRole::Generic), 90), 6.4f);
        updateMarkerNode(geometryLayer, geometryLayer->incomingHaloMarkers,
                         m_batch.incomingMarkers, withAlpha(colorForRole(PathRole::IncomingToMe), 92), 6.4f);
        updateMarkerNode(geometryLayer, geometryLayer->outgoingHaloMarkers,
                         m_batch.outgoingMarkers, withAlpha(colorForRole(PathRole::OutgoingFromMe), 92), 6.4f);
        updateMarkerNode(geometryLayer, geometryLayer->bandHaloMarkers,
                         m_batch.bandMarkers, withAlpha(colorForRole(PathRole::BandOnly), 95), 6.2f);

        updateMarkerNode(geometryLayer, geometryLayer->genericCoreMarkers,
                         m_batch.genericMarkers, colorForRole(PathRole::Generic), 3.5f);
        updateMarkerNode(geometryLayer, geometryLayer->incomingCoreMarkers,
                         m_batch.incomingMarkers, colorForRole(PathRole::IncomingToMe), 3.5f);
        updateMarkerNode(geometryLayer, geometryLayer->outgoingCoreMarkers,
                         m_batch.outgoingMarkers, colorForRole(PathRole::OutgoingFromMe), 3.5f);
        updateMarkerNode(geometryLayer, geometryLayer->bandCoreMarkers,
                         m_batch.bandMarkers, colorForRole(PathRole::BandOnly), 3.4f);
    }

    QVector<QPointF> genericArrows;
    QVector<QPointF> incomingArrows;
    QVector<QPointF> outgoingArrows;
    for (const AnimatedPath& path : std::as_const(m_animatedPaths)) {
        qreal progress = std::fmod(m_animationPhase + (qHash(path.key) % 17) * 0.057, 1.0);
        if (m_transmitting && path.role == PathRole::OutgoingFromMe) {
            QString normalized = path.key.trimmed().toUpper();
            bool const callMatch = m_txTargetCall.isEmpty()
                || normalized == m_txTargetCall
                || normalized.startsWith(m_txTargetCall + QLatin1Char('/'))
                || m_txTargetCall.startsWith(normalized + QLatin1Char('/'));
            if (callMatch && m_txStartMs > 0 && m_txTravelMs > 0) {
                qint64 const elapsedMs = qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - m_txStartMs);
                progress = std::fmod(static_cast<qreal>(elapsedMs) / static_cast<qreal>(m_txTravelMs), 1.0);
            }
        }

        QPolygonF arrow;
        if (!arrowOnPath(path.points, progress, &arrow)) {
            continue;
        }
        QVector<QPointF>* target = &genericArrows;
        if (path.role == PathRole::IncomingToMe) {
            target = &incomingArrows;
        } else if (path.role == PathRole::OutgoingFromMe) {
            target = &outgoingArrows;
        }
        for (const QPointF& p : arrow) {
            target->push_back(p);
        }
    }
    updateTriangleNode(animationLayer, animationLayer->genericArrows, genericArrows, QColor(255, 244, 196, 230));
    updateTriangleNode(animationLayer, animationLayer->incomingArrows, incomingArrows, QColor(255, 195, 140, 240));
    updateTriangleNode(animationLayer, animationLayer->outgoingArrows, outgoingArrows, QColor(255, 233, 132, 240));

    QVector<Label> displayLabels = m_labels;
    QColor const overlayTextColor(226, 236, 246, 215);
    if (rect.width() > 430.0) {
        qreal const legendY = rect.bottom() - 21.0;
        qreal x = rect.left() + 10.0;

        updateFlatLineNode(geometryLayer, geometryLayer->legendIncomingLine,
                           QVector<QPointF> {QPointF(x, legendY), QPointF(x + 17.0, legendY)},
                           QColor(255, 126, 92, 230));
        displayLabels.push_back({QStringLiteral("IN->ME"),
                                 QPointF(x + 21.0, legendY + 4.0),
                                 labelTextureRectForBaseline(QStringLiteral("IN->ME"), QPointF(x + 21.0, legendY + 4.0)),
                                 overlayTextColor});

        x += 100.0;
        updateFlatLineNode(geometryLayer, geometryLayer->legendOutgoingLine,
                           QVector<QPointF> {QPointF(x, legendY), QPointF(x + 17.0, legendY)},
                           QColor(84, 238, 165, 230));
        displayLabels.push_back({QStringLiteral("ME->DX"),
                                 QPointF(x + 21.0, legendY + 4.0),
                                 labelTextureRectForBaseline(QStringLiteral("ME->DX"), QPointF(x + 21.0, legendY + 4.0)),
                                 overlayTextColor});

        x += 104.0;
        updateMarkerNode(geometryLayer, geometryLayer->legendBandMarker,
                         QVector<QPointF> {QPointF(x + 8.0, legendY)},
                         QColor(255, 212, 96, 230), 3.4f);
        displayLabels.push_back({QStringLiteral("BAND"),
                                 QPointF(x + 21.0, legendY + 4.0),
                                 labelTextureRectForBaseline(QStringLiteral("BAND"), QPointF(x + 21.0, legendY + 4.0)),
                                 overlayTextColor});
    } else {
        updateFlatLineNode(geometryLayer, geometryLayer->legendIncomingLine, {}, QColor(255, 126, 92, 230));
        updateFlatLineNode(geometryLayer, geometryLayer->legendOutgoingLine, {}, QColor(84, 238, 165, 230));
        updateMarkerNode(geometryLayer, geometryLayer->legendBandMarker, {}, QColor(255, 212, 96, 230), 3.4f);
    }

    QString const bottomLeft = m_lastVisibleBandCount > 0
        ? QStringLiteral("%1 active paths | %2 in band").arg(m_lastVisiblePathCount).arg(m_lastVisibleBandCount)
        : QStringLiteral("%1 active paths").arg(m_lastVisiblePathCount);
    QPointF const bottomLeftBaseline(rect.left() + 8.0, rect.bottom() - 6.0);
    displayLabels.push_back({bottomLeft,
                             bottomLeftBaseline,
                             labelTextureRectForBaseline(bottomLeft, bottomLeftBaseline),
                             QColor(225, 235, 245, 205)});

    QString const utcText = QDateTime::currentDateTimeUtc().toString(QStringLiteral("hh:mm:ss 'UTC'"));
    QRectF const utcRect = labelTextureRectForBaseline(utcText, QPointF(0.0, 0.0));
    QPointF const utcBaseline(rect.right() - 8.0 - utcRect.width() + 2.0,
                              rect.bottom() - 6.0);
    displayLabels.push_back({utcText,
                             utcBaseline,
                             labelTextureRectForBaseline(utcText, utcBaseline),
                             QColor(225, 235, 245, 205)});

    while (labelLayer->labelNodes.size() < displayLabels.size()) {
        auto* node = new QSGSimpleTextureNode;
        node->setOwnsTexture(false);
        node->setFiltering(QSGTexture::Linear);
        labelLayer->appendChildNode(node);
        labelLayer->labelNodes.push_back(node);
    }
    while (labelLayer->labelNodes.size() > displayLabels.size()) {
        auto* node = labelLayer->labelNodes.takeLast();
        labelLayer->removeChildNode(node);
        delete node;
    }

    QSet<QString> liveLabelTextureKeys;
    bool allLabelTexturesAssigned = true;
    for (int i = 0; i < displayLabels.size(); ++i) {
        const Label& label = displayLabels[i];
        auto* node = labelLayer->labelNodes[i];
        if (QSGTexture* texture = cachedLabelTexture(labelLayer, window(), label.text, label.color)) {
            liveLabelTextureKeys.insert(labelTextureKey(label.text, label.color));
            node->setTexture(texture);
            node->setRect(label.rect);
        } else {
            allLabelTexturesAssigned = false;
            node->setRect(QRectF());
        }
    }
    if (allLabelTexturesAssigned) {
        pruneLabelTextureCache(labelLayer, liveLabelTextureKeys);
    }

#ifdef DECODIUM_LIVEMAP_GREYLINE_QSB
    int const greylineShaderActive = m_greylineShaderAllowed ? 1 : 0;
#else
    int const greylineShaderActive = 0;
#endif

    if (!m_loggedFirstFrame) {
        m_loggedFirstFrame = true;
        qInfo().nospace()
            << "[WorldMapGpuItem] first frame renderer=QSG texture="
            << m_mapImage.width() << "x" << m_mapImage.height()
            << " contacts=" << m_lastContactCount
            << " conservative=" << (m_conservativeRenderer ? 1 : 0)
            << " frameMs=" << m_frameIntervalMs
            << " greylineShader=" << greylineShaderActive
            << " lineVertices=" << m_lastLineVertexCount
            << " markerVertices=" << m_lastMarkerVertexCount
            << " labels=" << m_lastLabelCount
            << " visiblePaths=" << m_lastVisiblePathCount
            << " visibleBand=" << m_lastVisibleBandCount
            << " labelTextures=" << labelLayer->textureCache.size()
            << " markerShader="
#ifdef DECODIUM_LIVEMAP_MARKER_QSB
            << 1
#else
            << 0
#endif
            << " pathShader="
#ifdef DECODIUM_LIVEMAP_PATH_QSB
            << 1;
#else
            << 0;
#endif
    }

    qint64 const nowMs = monotonicNowMs();
    if (!m_loggedFirstProfile || nowMs - m_lastProfileLogMs >= kGpuProfileLogMs) {
        m_loggedFirstProfile = true;
        m_lastProfileLogMs = nowMs;
        qInfo().noquote().nospace()
            << "[MAPGPU] LiveMap QSG profile contacts=" << m_lastContactCount
            << " lineVertices=" << m_lastLineVertexCount
            << " markerVertices=" << m_lastMarkerVertexCount
            << " labels=" << m_lastLabelCount
            << " visiblePaths=" << m_lastVisiblePathCount
            << " visibleBand=" << m_lastVisibleBandCount
            << " animatedPaths=" << m_animatedPaths.size()
            << " viewCenter=" << QString::number(m_viewCenterLon, 'f', 2)
            << "," << QString::number(m_viewCenterLat, 'f', 2)
            << " viewSpan=" << QString::number(m_viewSpanLon, 'f', 2)
            << "," << QString::number(m_viewSpanLat, 'f', 2)
            << " labelTextures=" << labelLayer->textureCache.size()
            << " conservative=" << (m_conservativeRenderer ? 1 : 0)
            << " frameMs=" << m_frameIntervalMs
            << " markerShader="
#ifdef DECODIUM_LIVEMAP_MARKER_QSB
            << 1
#else
            << 0
#endif
            << " pathShader="
#ifdef DECODIUM_LIVEMAP_PATH_QSB
            << 1
#else
            << 0
#endif
            << " greylineShader="
            << greylineShaderActive;
    }

    return root;
}

void WorldMapGpuItem::mousePressEvent(QMouseEvent* event)
{
    if (!event || event->button() != Qt::LeftButton) {
        QQuickItem::mousePressEvent(event);
        return;
    }

    QPointF const pos = event->position();
    QString closestCall;
    QString closestGrid;
    qreal closestDistance = 14.0;

    for (const Contact& contact : m_contacts) {
        QPointF const marker = contact.role == PathRole::OutgoingFromMe
            ? projectLonLatToPoint(contact.destinationLonLat)
            : projectLonLatToPoint(contact.sourceLonLat);
        qreal const distance = QLineF(pos, marker).length();
        if (distance < closestDistance) {
            closestDistance = distance;
            closestCall = contact.call;
            closestGrid = contact.role == PathRole::OutgoingFromMe
                ? contact.destinationGrid
                : contact.sourceGrid;
        }
    }

    if (!closestCall.isEmpty()) {
        event->accept();
        Q_EMIT contactClicked(closestCall, closestGrid);
        return;
    }

    QQuickItem::mousePressEvent(event);
}

void WorldMapGpuItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        m_greylineGeometryDirty = true;
        markDirty();
    }
}

WorldMapGpuItem::PathRole WorldMapGpuItem::pathRoleFromInt(int role)
{
    switch (role) {
    case 1:
        return PathRole::IncomingToMe;
    case 2:
        return PathRole::OutgoingFromMe;
    case 3:
        return PathRole::BandOnly;
    case 0:
    default:
        return PathRole::Generic;
    }
}

double WorldMapGpuItem::wrapLongitude(double lon)
{
    while (lon < -180.0) {
        lon += 360.0;
    }
    while (lon >= 180.0) {
        lon -= 360.0;
    }
    return lon;
}

bool WorldMapGpuItem::maidenheadToLonLat(const QString& locator, QPointF* lonLat)
{
    if (!lonLat) {
        return false;
    }

    QString const l = locator.trimmed().toUpper();
    if (l.size() < 4 || (l.size() != 4 && l.size() != 6)) {
        return false;
    }

    auto const c0 = l.at(0).unicode();
    auto const c1 = l.at(1).unicode();
    auto const c2 = l.at(2).unicode();
    auto const c3 = l.at(3).unicode();
    if (c0 < 'A' || c0 > 'R' || c1 < 'A' || c1 > 'R'
        || c2 < '0' || c2 > '9' || c3 < '0' || c3 > '9') {
        return false;
    }

    double lon = (c0 - 'A') * 20.0 - 180.0;
    double lat = (c1 - 'A') * 10.0 - 90.0;
    lon += (c2 - '0') * 2.0;
    lat += (c3 - '0') * 1.0;

    double lonStep = 2.0;
    double latStep = 1.0;
    if (l.size() == 6) {
        auto const c4 = l.at(4).unicode();
        auto const c5 = l.at(5).unicode();
        if (c4 < 'A' || c4 > 'X' || c5 < 'A' || c5 > 'X') {
            return false;
        }
        lonStep = 5.0 / 60.0;
        latStep = 2.5 / 60.0;
        lon += (c4 - 'A') * lonStep;
        lat += (c5 - 'A') * latStep;
    }

    *lonLat = QPointF(lon + lonStep / 2.0, lat + latStep / 2.0);
    return true;
}

QImage WorldMapGpuItem::loadImageWithFallback(const QStringList& candidates)
{
    for (const QString& candidate : candidates) {
        QImage image(candidate);
        if (!image.isNull()) {
            return image;
        }
    }
    return QImage();
}

QImage WorldMapGpuItem::buildMapTexture()
{
    QString const appDir = QCoreApplication::applicationDirPath();
    QString const cwd = QDir::currentPath();

    QImage earth = loadImageWithFallback({
        QStringLiteral(":/earth_2048x1024.jpg"),
        QStringLiteral(":/artwork/maps/earth_2048x1024.jpg"),
        QDir(appDir).absoluteFilePath(QStringLiteral("artwork/maps/earth_2048x1024.jpg")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/earth_2048x1024.jpg")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/wsjtx/maps/earth_2048x1024.jpg")),
        QDir(cwd).absoluteFilePath(QStringLiteral("artwork/maps/earth_2048x1024.jpg"))
    });

    if (earth.isNull()) {
        earth = QImage(2048, 1024, QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&earth);
        QLinearGradient gradient(QPointF(0, 0), QPointF(0, earth.height()));
        gradient.setColorAt(0.0, QColor(10, 56, 95));
        gradient.setColorAt(1.0, QColor(4, 24, 42));
        painter.fillRect(earth.rect(), gradient);
        painter.end();
    } else {
        earth = earth.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    QImage overlay = loadImageWithFallback({
        QStringLiteral(":/world_overlay_2048x1024.png"),
        QStringLiteral(":/artwork/maps/world_overlay_2048x1024.png"),
        QDir(appDir).absoluteFilePath(QStringLiteral("artwork/maps/world_overlay_2048x1024.png")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/world_overlay_2048x1024.png")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/wsjtx/maps/world_overlay_2048x1024.png")),
        QDir(cwd).absoluteFilePath(QStringLiteral("artwork/maps/world_overlay_2048x1024.png"))
    });

    if (!overlay.isNull()) {
        QPainter painter(&earth);
        painter.setOpacity(0.44);
        painter.setCompositionMode(QPainter::CompositionMode_Screen);
        painter.drawImage(earth.rect(), overlay.convertToFormat(QImage::Format_ARGB32_Premultiplied));
        painter.end();
    }

    QPainter painter(&earth);
    painter.fillRect(earth.rect(), QColor(0, 14, 24, 18));
    painter.end();
    return earth;
}

QPointF WorldMapGpuItem::subSolarLonLat()
{
    QDateTime const now = QDateTime::currentDateTimeUtc();
    QTime const t = now.time();
    double const utcHours = t.hour() + (t.minute() / 60.0) + (t.second() / 3600.0);
    double const subSolarLon = wrapLongitude((12.0 - utcHours) * 15.0);

    int const dayOfYear = now.date().dayOfYear();
    double const fractionalYear = (2.0 * M_PI / 365.24) * (dayOfYear - 1 + utcHours / 24.0);
    double const declinationRadians = 0.006918
        - 0.399912 * std::cos(fractionalYear)
        + 0.070257 * std::sin(fractionalYear)
        - 0.006758 * std::cos(2.0 * fractionalYear)
        + 0.000907 * std::sin(2.0 * fractionalYear)
        - 0.002697 * std::cos(3.0 * fractionalYear)
        + 0.00148 * std::sin(3.0 * fractionalYear);
    return QPointF(subSolarLon, qRadiansToDegrees(declinationRadians));
}

QPointF WorldMapGpuItem::projectLonLatToPoint(const QPointF& lonLat) const
{
    QRectF const bounds = mapRect();
    double const lonDelta = wrapLongitude(lonLat.x() - m_viewCenterLon);
    double const lat = qBound(-90.0, static_cast<double>(lonLat.y()), 90.0);
    qreal const x = bounds.left()
        + static_cast<qreal>((lonDelta + 0.5 * m_viewSpanLon) / m_viewSpanLon) * bounds.width();
    qreal const y = bounds.top()
        + static_cast<qreal>((m_viewCenterLat + 0.5 * m_viewSpanLat - lat) / m_viewSpanLat) * bounds.height();
    return QPointF(x, y);
}

bool WorldMapGpuItem::computeCircularLongitudeBounds(const QVector<double>& longitudes,
                                                     double* centerLon,
                                                     double* spanLon) const
{
    if (!centerLon || !spanLon || longitudes.isEmpty()) {
        return false;
    }

    QVector<double> lons;
    lons.reserve(longitudes.size());
    for (double lon : longitudes) {
        lon = wrapLongitude(lon);
        if (lon < 0.0) {
            lon += 360.0;
        }
        lons.push_back(lon);
    }
    std::sort(lons.begin(), lons.end());

    if (lons.size() == 1) {
        double c = lons.first();
        if (c > 180.0) {
            c -= 360.0;
        }
        *centerLon = c;
        *spanLon = 1.0;
        return true;
    }

    double largestGap = -1.0;
    int gapStart = 0;
    for (int i = 0; i < lons.size(); ++i) {
        double const a = lons[i];
        double const b = (i == lons.size() - 1) ? lons.first() + 360.0 : lons[i + 1];
        double const gap = b - a;
        if (gap > largestGap) {
            largestGap = gap;
            gapStart = i;
        }
    }

    double const start = lons[(gapStart + 1) % lons.size()];
    double const end = lons[gapStart] + (gapStart == lons.size() - 1 ? 0.0 : 360.0);
    double c = start + (end - start) * 0.5;
    while (c >= 360.0) {
        c -= 360.0;
    }
    if (c > 180.0) {
        c -= 360.0;
    }

    *centerLon = c;
    *spanLon = qMax(1.0, 360.0 - largestGap);
    return true;
}

void WorldMapGpuItem::updateViewportTargets()
{
    QVector<QPointF> points;
    points.reserve(m_contacts.size() * 2 + (m_hasHome ? 1 : 0));
    if (m_hasHome) {
        points.push_back(m_homeLonLat);
    }

    for (const Contact& contact : std::as_const(m_contacts)) {
        points.push_back(contact.sourceLonLat);
        if (contact.role != PathRole::BandOnly) {
            points.push_back(contact.destinationLonLat);
        }
    }

    if (points.isEmpty()) {
        m_targetCenterLon = 0.0;
        m_targetCenterLat = 0.0;
        m_targetSpanLon = 360.0;
        m_targetSpanLat = 180.0;
        return;
    }

    if (points.size() == 1) {
        m_targetCenterLon = wrapLongitude(points.first().x());
        m_targetCenterLat = qBound(-44.0, static_cast<double>(points.first().y()), 44.0);
        m_targetSpanLon = 150.0;
        m_targetSpanLat = 88.0;
        return;
    }

    QVector<double> lons;
    lons.reserve(points.size());
    double minLat = 90.0;
    double maxLat = -90.0;
    for (const QPointF& point : points) {
        lons.push_back(point.x());
        minLat = qMin(minLat, static_cast<double>(point.y()));
        maxLat = qMax(maxLat, static_cast<double>(point.y()));
    }

    double centerLon = 0.0;
    double spanLon = 360.0;
    if (!computeCircularLongitudeBounds(lons, &centerLon, &spanLon)) {
        centerLon = 0.0;
        spanLon = 360.0;
    }

    double centerLat = 0.5 * (minLat + maxLat);
    double spanLat = qMax(1.0, maxLat - minLat);

    spanLon = qBound(68.0, spanLon * 1.45 + 10.0, 240.0);
    spanLat = qBound(34.0, spanLat * 1.45 + 8.0, 126.0);

    if (!points.isEmpty()) {
        QPointF const focus = points.last();
        centerLon = wrapLongitude(centerLon + wrapLongitude(focus.x() - centerLon) * 0.28);
        centerLat += (focus.y() - centerLat) * 0.28;
    }

    double const phase = m_animationPhase * 6.283185307179586;
    double const orbitAmpLon = qBound(0.30, spanLon * 0.012, 1.60);
    double const orbitAmpLat = qBound(0.20, spanLat * 0.010, 1.00);
    centerLon = wrapLongitude(centerLon + orbitAmpLon * std::sin(phase * 0.50));
    centerLat += orbitAmpLat * std::cos(phase * 0.42 + 0.7);

    double const breathe = 1.0 + 0.010 * std::sin(phase * 0.45 + 1.2);
    spanLon *= breathe;
    spanLat *= breathe;

    double const aspect = qMax(1.10, static_cast<double>(width()) / qMax(1.0, static_cast<double>(height())));
    if (spanLon < spanLat * aspect) {
        spanLon = qMin(240.0, spanLat * aspect);
    }
    if (spanLat < spanLon / aspect) {
        spanLat = qMin(126.0, spanLon / aspect);
    }

    centerLat = qBound(-90.0 + 0.5 * spanLat, centerLat, 90.0 - 0.5 * spanLat);
    m_targetCenterLon = wrapLongitude(centerLon);
    m_targetCenterLat = centerLat;
    m_targetSpanLon = spanLon;
    m_targetSpanLat = spanLat;
}

bool WorldMapGpuItem::smoothViewport()
{
    double const oldCenterLon = m_viewCenterLon;
    double const oldCenterLat = m_viewCenterLat;
    double const oldSpanLon = m_viewSpanLon;
    double const oldSpanLat = m_viewSpanLat;

    double const centerStiffness = 0.11;
    double const centerDamping = 0.83;
    double const spanStiffness = 0.10;
    double const spanDamping = 0.82;

    double const deltaLon = wrapLongitude(m_targetCenterLon - m_viewCenterLon);
    m_viewVelocityLon = (m_viewVelocityLon + deltaLon * centerStiffness) * centerDamping;
    m_viewCenterLon = wrapLongitude(m_viewCenterLon + m_viewVelocityLon);

    double const deltaLat = m_targetCenterLat - m_viewCenterLat;
    m_viewVelocityLat = (m_viewVelocityLat + deltaLat * centerStiffness) * centerDamping;
    m_viewCenterLat += m_viewVelocityLat;

    double const deltaSpanLon = m_targetSpanLon - m_viewSpanLon;
    m_viewVelocitySpanLon = (m_viewVelocitySpanLon + deltaSpanLon * spanStiffness) * spanDamping;
    m_viewSpanLon += m_viewVelocitySpanLon;

    double const deltaSpanLat = m_targetSpanLat - m_viewSpanLat;
    m_viewVelocitySpanLat = (m_viewVelocitySpanLat + deltaSpanLat * spanStiffness) * spanDamping;
    m_viewSpanLat += m_viewVelocitySpanLat;

    if (qAbs(deltaLon) < 0.003 && qAbs(m_viewVelocityLon) < 0.003) {
        m_viewVelocityLon = 0.0;
    }
    if (qAbs(deltaLat) < 0.003 && qAbs(m_viewVelocityLat) < 0.003) {
        m_viewVelocityLat = 0.0;
    }
    if (qAbs(deltaSpanLon) < 0.01 && qAbs(m_viewVelocitySpanLon) < 0.01) {
        m_viewVelocitySpanLon = 0.0;
    }
    if (qAbs(deltaSpanLat) < 0.01 && qAbs(m_viewVelocitySpanLat) < 0.01) {
        m_viewVelocitySpanLat = 0.0;
    }

    m_viewSpanLon = qBound(45.0, m_viewSpanLon, 360.0);
    m_viewSpanLat = qBound(24.0, m_viewSpanLat, 180.0);
    m_viewCenterLat = qBound(-90.0 + 0.5 * m_viewSpanLat, m_viewCenterLat, 90.0 - 0.5 * m_viewSpanLat);

    return qAbs(wrapLongitude(m_viewCenterLon - oldCenterLon)) > 0.002
        || qAbs(m_viewCenterLat - oldCenterLat) > 0.002
        || qAbs(m_viewSpanLon - oldSpanLon) > 0.004
        || qAbs(m_viewSpanLat - oldSpanLat) > 0.004;
}

void WorldMapGpuItem::rebuildGeometryBatch()
{
    m_batch = BatchedGeometry {};
    m_labels.clear();
    m_animatedPaths.clear();

    QRectF const rect = mapRect();
    if (rect.isEmpty()) {
        m_lastContactCount = 0;
        m_lastLineVertexCount = 0;
        m_lastMarkerVertexCount = 0;
        m_lastLabelCount = 0;
        m_lastVisiblePathCount = 0;
        m_lastVisibleBandCount = 0;
        m_geometryDirty = false;
        return;
    }
    m_lastVisiblePathCount = 0;
    m_lastVisibleBandCount = 0;

    double lonStep = 30.0;
    if (m_viewSpanLon < 220.0) lonStep = 20.0;
    if (m_viewSpanLon < 130.0) lonStep = 10.0;
    if (m_viewSpanLon < 75.0) lonStep = 5.0;
    double const leftLon = m_viewCenterLon - 0.5 * m_viewSpanLon;
    double const rightLon = m_viewCenterLon + 0.5 * m_viewSpanLon;
    double const startLon = std::floor(leftLon / lonStep) * lonStep;
    for (double lon = startLon; lon <= rightLon; lon += lonStep) {
        qreal const x = projectLonLatToPoint(QPointF(lon, m_viewCenterLat)).x();
        m_batch.gridLines.push_back(QPointF(x, rect.top()));
        m_batch.gridLines.push_back(QPointF(x, rect.bottom()));
    }

    double latStep = 20.0;
    if (m_viewSpanLat < 110.0) latStep = 10.0;
    if (m_viewSpanLat < 55.0) latStep = 5.0;
    double const topLat = m_viewCenterLat + 0.5 * m_viewSpanLat;
    double const bottomLat = m_viewCenterLat - 0.5 * m_viewSpanLat;
    double const startLat = std::floor(bottomLat / latStep) * latStep;
    for (double lat = startLat; lat <= topLat; lat += latStep) {
        qreal const y = projectLonLatToPoint(QPointF(m_viewCenterLon, lat)).y();
        m_batch.gridLines.push_back(QPointF(rect.left(), y));
        m_batch.gridLines.push_back(QPointF(rect.right(), y));
    }

    if (m_hasHome) {
        QPointF const home = projectLonLatToPoint(m_homeLonLat);
        bool const homeVisible = isInViewport(rect, home, 18.0);
        if (homeVisible) {
            m_batch.genericMarkers.push_back(home);
        }
        if (homeVisible && !m_homeGrid.isEmpty()) {
            m_labels.push_back({m_homeGrid.left(6), home + QPointF(9.0, -8.0), QRectF(), QColor(235, 250, 255, 235)});
        }
    }

    auto contacts = m_contacts.values();
    std::sort(contacts.begin(), contacts.end(), [](const Contact& a, const Contact& b) {
        int const pa = rolePriority(a.role);
        int const pb = rolePriority(b.role);
        if (pa != pb) {
            return pa > pb;
        }
        if (a.lastSeenMonotonicMs != b.lastSeenMonotonicMs) {
            return a.lastSeenMonotonicMs > b.lastSeenMonotonicMs;
        }
        return a.call < b.call;
    });
    int const totalContactCount = contacts.size();
    if (contacts.size() > kMaxVisibleContacts) {
        contacts.resize(kMaxVisibleContacts);
    }

    int contactLabels = 0;
    for (const Contact& contact : contacts) {
        QPointF const source = projectLonLatToPoint(contact.sourceLonLat);
        QPointF const destination = projectLonLatToPoint(contact.destinationLonLat);
        if (contact.role != PathRole::BandOnly) {
            PathLine const path {contact.sourceLonLat, contact.destinationLonLat};
            if (contact.role == PathRole::IncomingToMe) {
                m_batch.incomingPaths.push_back(path);
            } else if (contact.role == PathRole::OutgoingFromMe) {
                m_batch.outgoingPaths.push_back(path);
            } else {
                m_batch.genericPaths.push_back(path);
            }

            QVector<QPointF> projectedArc;
            auto const arc = greatCircle(contact.sourceLonLat, contact.destinationLonLat, kGreatCircleSteps);
            projectedArc.reserve(arc.size());
            for (const QPointF& lonLat : arc) {
                projectedArc.push_back(projectLonLatToPoint(lonLat));
            }
            if (projectedArc.size() >= 2) {
                AnimatedPath animated;
                animated.key = contact.call;
                animated.role = contact.role;
                animated.sourceLonLat = contact.sourceLonLat;
                animated.destinationLonLat = contact.destinationLonLat;
                animated.points = projectedArc;
                m_animatedPaths.push_back(animated);
            }
        }

        QPointF const marker = contact.role == PathRole::OutgoingFromMe ? destination : source;
        bool const markerVisible = isInViewport(rect, marker, 18.0);
        if (markerVisible) {
            switch (contact.role) {
            case PathRole::IncomingToMe:
                m_batch.incomingMarkers.push_back(marker);
                ++m_lastVisiblePathCount;
                break;
            case PathRole::OutgoingFromMe:
                m_batch.outgoingMarkers.push_back(marker);
                ++m_lastVisiblePathCount;
                break;
            case PathRole::BandOnly:
                m_batch.bandMarkers.push_back(marker);
                ++m_lastVisibleBandCount;
                break;
            case PathRole::Generic:
            default:
                m_batch.genericMarkers.push_back(marker);
                ++m_lastVisiblePathCount;
                break;
            }
        }

        if (markerVisible && !contact.call.isEmpty() && contactLabels < kMaxVisibleContactLabels) {
            QColor labelColor = contact.role == PathRole::BandOnly
                ? QColor(255, 238, 174, 235)
                : QColor(255, 244, 196, 235);
            m_labels.push_back({contact.call.left(12), marker + QPointF(7.0, -7.0), QRectF(), labelColor});
            ++contactLabels;
        }
    }

    layoutLabels(rect);
    m_lastContactCount = totalContactCount;
    m_lastLineVertexCount = kGreatCircleSteps * 2 * (m_batch.genericPaths.size()
        + m_batch.incomingPaths.size()
        + m_batch.outgoingPaths.size());
    m_lastMarkerVertexCount = 6 * 2 * (m_batch.genericMarkers.size()
        + m_batch.incomingMarkers.size()
        + m_batch.outgoingMarkers.size()
        + m_batch.bandMarkers.size());
    m_lastLabelCount = m_labels.size();
    m_geometryDirty = false;
}

void WorldMapGpuItem::layoutLabels(const QRectF& rect)
{
    QFont const font = liveMapLabelFont();
    QFontMetricsF const fm(font);

    QVector<Label> placedLabels;
    placedLabels.reserve(m_labels.size());
    for (Label labelData : std::as_const(m_labels)) {
        if (labelData.text.isEmpty()) {
            continue;
        }

        QPointF baseline = labelData.baseline;
        qreal const labelWidth = fm.horizontalAdvance(labelData.text) + 6.0;
        qreal const labelHeight = fm.height() + 4.0;
        baseline.setX(qBound(rect.left() + 2.0, baseline.x(), qMax(rect.left() + 2.0, rect.right() - labelWidth)));
        baseline.setY(qBound(rect.top() + 14.0, baseline.y(), qMax(rect.top() + 14.0, rect.bottom() - 2.0)));
        QRectF const textRect(baseline + QPointF(-2.0, -fm.ascent() - 2.0),
                              QSizeF(labelWidth, labelHeight));

        labelData.baseline = baseline;
        labelData.rect = textRect;
        placedLabels.push_back(labelData);
    }
    m_labels = placedLabels;
}

QRectF WorldMapGpuItem::mapRect() const
{
    return boundingRect();
}

void WorldMapGpuItem::markDirty()
{
    m_geometryDirty = true;
    if (isVisible()) {
        update();
    }
}
