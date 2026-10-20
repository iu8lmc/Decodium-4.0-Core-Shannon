#include "WorldMapGpuItem.hpp"

#include "src/services/MapBaseMapService.h"
#include "src/services/MapExternalOverlayService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QLinearGradient>
#include <QLineF>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
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
#include <QStringList>

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
constexpr qint64 kGpuProfileLogMs = 60000;
constexpr int kMaxLabelImageCache = 192;
constexpr qint64 kLabelImageTtlMs = 5 * 60 * 1000;
constexpr int kLabelAtlasMaxWidth = 2048;
constexpr int kLabelAtlasPadding = 2;

bool maidenheadCellBounds(const QString& locator, QPointF* southWest, QPointF* northEast)
{
    if (!southWest || !northEast) {
        return false;
    }
    QString const candidate = locator.trimmed().toUpper();
    QString const grid = candidate.left(candidate.size() >= 6 ? 6 : 4);
    if ((grid.size() != 4 && grid.size() != 6)
        || grid.at(0) < QLatin1Char('A') || grid.at(0) > QLatin1Char('R')
        || grid.at(1) < QLatin1Char('A') || grid.at(1) > QLatin1Char('R')
        || !grid.at(2).isDigit() || !grid.at(3).isDigit()) {
        return false;
    }
    double west = (grid.at(0).unicode() - 'A') * 20.0 - 180.0
        + (grid.at(2).unicode() - '0') * 2.0;
    double south = (grid.at(1).unicode() - 'A') * 10.0 - 90.0
        + (grid.at(3).unicode() - '0');
    double lonStep = 2.0;
    double latStep = 1.0;
    if (grid.size() == 6) {
        if (grid.at(4) < QLatin1Char('A') || grid.at(4) > QLatin1Char('X')
            || grid.at(5) < QLatin1Char('A') || grid.at(5) > QLatin1Char('X')) {
            return false;
        }
        lonStep = 5.0 / 60.0;
        latStep = 2.5 / 60.0;
        west += (grid.at(4).unicode() - 'A') * lonStep;
        south += (grid.at(5).unicode() - 'A') * latStep;
    }
    *southWest = QPointF(west, south);
    *northEast = QPointF(west + lonStep, south + latStep);
    return true;
}

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

bool environmentFlag(const char* name)
{
    if (!qEnvironmentVariableIsSet(name)) {
        return false;
    }
    QByteArray const value = qgetenv(name).trimmed().toLower();
    return value.isEmpty() || value == "1" || value == "true"
        || value == "yes" || value == "on";
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
    QSGGeometryNode* workedCoverageFill {nullptr};
    QSGGeometryNode* confirmedCoverageFill {nullptr};
    QSGGeometryNode* activeCoverageFill {nullptr};
    QSGGeometryNode* activeCoverageMediumFill {nullptr};
    QSGGeometryNode* activeCoverageFadedFill {nullptr};
    QSGGeometryNode* missingCoverageFill {nullptr};
    QSGGeometryNode* missingCoverageMediumFill {nullptr};
    QSGGeometryNode* missingCoverageFadedFill {nullptr};
    QSGGeometryNode* pskCoverageFill {nullptr};
    QSGGeometryNode* pskCoverageMediumFill {nullptr};
    QSGGeometryNode* pskCoverageFadedFill {nullptr};
    QSGGeometryNode* workedCoverageLines {nullptr};
    QSGGeometryNode* confirmedCoverageLines {nullptr};
    QSGGeometryNode* activeCoverageLines {nullptr};
    QSGGeometryNode* activeCoverageMediumLines {nullptr};
    QSGGeometryNode* activeCoverageFadedLines {nullptr};
    QSGGeometryNode* missingCoverageLines {nullptr};
    QSGGeometryNode* missingCoverageMediumLines {nullptr};
    QSGGeometryNode* missingCoverageFadedLines {nullptr};
    QSGGeometryNode* pskCoverageLines {nullptr};
    QSGGeometryNode* pskCoverageMediumLines {nullptr};
    QSGGeometryNode* pskCoverageFadedLines {nullptr};
    QSGGeometryNode* gridLines {nullptr};
    QSGGeometryNode* timeZoneLines {nullptr};
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
    QSGGeometryNode* countyBoundaryFill {nullptr};
    QSGGeometryNode* stateBoundaryFill {nullptr};
    QSGGeometryNode* countyBoundaryLines {nullptr};
    QSGGeometryNode* stateBoundaryLines {nullptr};
    QSGGeometryNode* earthquakeLowPulseMarkers {nullptr};
    QSGGeometryNode* earthquakeMediumPulseMarkers {nullptr};
    QSGGeometryNode* earthquakeHighPulseMarkers {nullptr};
    QSGGeometryNode* earthquakeLowCoreMarkers {nullptr};
    QSGGeometryNode* earthquakeMediumCoreMarkers {nullptr};
    QSGGeometryNode* earthquakeHighCoreMarkers {nullptr};
    QSGGeometryNode* potaHaloMarkers {nullptr};
    QSGGeometryNode* iotaHaloMarkers {nullptr};
    QSGGeometryNode* wpxHaloMarkers {nullptr};
    QSGGeometryNode* moonHaloMarkers {nullptr};
    QSGGeometryNode* satelliteHaloMarkers {nullptr};
    QSGGeometryNode* potaCoreMarkers {nullptr};
    QSGGeometryNode* iotaCoreMarkers {nullptr};
    QSGGeometryNode* wpxCoreMarkers {nullptr};
    QSGGeometryNode* moonCoreMarkers {nullptr};
    QSGGeometryNode* satelliteCoreMarkers {nullptr};
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
        delete stableAtlasTexture;
        qDeleteAll(retiredAtlasTextures);
        qDeleteAll(transientTextures);
        delete blankTexture;
    }

    QHash<QString, QImage> stableImageCache;
    QHash<QString, qint64> stableImageLastUsedMs;
    QHash<QString, QRectF> stableAtlasRects;
    QString stableAtlasSignature;
    QSGTexture* stableAtlasTexture {nullptr};
    QVector<QSGTexture*> retiredAtlasTextures;
    QVector<QSGSimpleTextureNode*> labelNodes;
    QVector<QSGTexture*> transientTextures;
    QVector<QString> transientTextureKeys;
    QSGTexture* blankTexture {nullptr};
};

class AnimationLayerNode final : public QSGNode
{
public:
    QSGGeometryNode* genericArrows {nullptr};
    QSGGeometryNode* incomingArrows {nullptr};
    QSGGeometryNode* outgoingArrows {nullptr};
    int genericArrowVertices {0};
    int incomingArrowVertices {0};
    int outgoingArrowVertices {0};
};

class MapLayerNode final : public QSGNode
{
public:
    ~MapLayerNode() override
    {
        clearNode(this);
        delete texture;
        delete blankTexture;
    }

    QSGTexture* texture {nullptr};
    QSGTexture* blankTexture {nullptr};
    QVector<QSGSimpleTextureNode*> tileNodes;
};

// QSGSimpleTextureNode does not own its texture.  Always remove the nodes
// before replacing or deleting their shared texture, otherwise Metal can
// compare a material that still contains a dangling QSGTexture pointer.
void clearMapTileNodes(MapLayerNode* layer)
{
    if (!layer) {
        return;
    }
    while (!layer->tileNodes.isEmpty()) {
        auto* node = layer->tileNodes.takeLast();
        layer->removeChildNode(node);
        delete node;
    }
}

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

bool geographicCoordinateToLonLat(const QVariant& value, QPointF* lonLat)
{
    if (!lonLat) {
        return false;
    }
    QVariantList const coordinate = value.toList();
    if (coordinate.size() < 2) {
        return false;
    }
    bool longitudeOk = false;
    bool latitudeOk = false;
    double const longitude = coordinate.at(0).toDouble(&longitudeOk);
    double const latitude = coordinate.at(1).toDouble(&latitudeOk);
    if (!longitudeOk || !latitudeOk || !std::isfinite(longitude)
        || !std::isfinite(latitude) || longitude < -180.0 || longitude > 180.0
        || latitude < -90.0 || latitude > 90.0) {
        return false;
    }
    *lonLat = QPointF(longitude, latitude);
    return true;
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

QSGTexture* mapBlankTexture(MapLayerNode* layer, QQuickWindow* window)
{
    if (!layer || !window) {
        return nullptr;
    }
    if (!layer->blankTexture) {
        QImage image(1, 1, QImage::Format_RGBA8888_Premultiplied);
        image.fill(QColor(10, 18, 24, 255));
        layer->blankTexture = window->createTextureFromImage(image);
        if (layer->blankTexture) {
            layer->blankTexture->setFiltering(QSGTexture::Nearest);
        }
    }
    return layer->blankTexture;
}

double projectedLatitudeForTexture(double latitude, const QString& projection)
{
    double const bounded = qBound(-90.0, latitude, 90.0);
    if (projection == QStringLiteral("Mercator")) {
        double const radians = qDegreesToRadians(
            qBound(-85.05112878, bounded, 85.05112878));
        return std::log(std::tan(M_PI_4 + radians * 0.5));
    }
    if (projection == QStringLiteral("Miller")) {
        double const radians = qDegreesToRadians(bounded);
        return 1.25 * std::log(std::tan(M_PI_4 + 0.4 * radians));
    }
    return bounded;
}

double inverseProjectedLatitudeForTexture(double projected,
                                          const QString& projection)
{
    if (projection == QStringLiteral("Mercator")) {
        return qRadiansToDegrees(2.0 * std::atan(std::exp(projected))
                                 - M_PI_2);
    }
    if (projection == QStringLiteral("Miller")) {
        return qRadiansToDegrees(
            2.5 * (std::atan(std::exp(projected / 1.25)) - M_PI_4));
    }
    return projected;
}

void appendMapTileNodes(MapLayerNode* layer,
                        const QRectF& rect,
                        double centerLon,
                        double centerLat,
                        double spanLon,
                        double spanLat,
                        int textureWidth,
                        int textureHeight,
                        const QString& projection)
{
    if (!layer || rect.isEmpty() || spanLon <= 0.0 || spanLat <= 0.0) {
        clearMapTileNodes(layer);
        return;
    }
    QSGTexture* layerTexture = layer->texture ? layer->texture : layer->blankTexture;
    if (!layerTexture) {
        clearMapTileNodes(layer);
        return;
    }
    textureWidth = qMax(1, textureWidth);
    textureHeight = qMax(1, textureHeight);

    double const topLat = qBound(-90.0, centerLat + 0.5 * spanLat, 90.0);
    double const bottomLat = qBound(-90.0, centerLat - 0.5 * spanLat, 90.0);
    double const projectedTop =
        projectedLatitudeForTexture(topLat, projection);
    double const projectedBottom =
        projectedLatitudeForTexture(bottomLat, projection);
    double const projectedSpan =
        qMax(0.000001, projectedTop - projectedBottom);
    int const verticalSlices =
        projection == QStringLiteral("Equirectangular") ? 1 : 72;
    double const leftLon = centerLon - 0.5 * spanLon;
    double const rightLon = centerLon + 0.5 * spanLon;

    struct Tile {
        QRectF target;
        QRectF source;
    };
    QVector<Tile> tiles;
    tiles.reserve(3 * verticalSlices);

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

        for (int slice = 0; slice < verticalSlices; ++slice) {
            double const fraction0 =
                static_cast<double>(slice) / verticalSlices;
            double const fraction1 =
                static_cast<double>(slice + 1) / verticalSlices;
            double const projected0 =
                projectedTop - fraction0 * projectedSpan;
            double const projected1 =
                projectedTop - fraction1 * projectedSpan;
            double const latitude0 = inverseProjectedLatitudeForTexture(
                projected0, projection);
            double const latitude1 = inverseProjectedLatitudeForTexture(
                projected1, projection);
            qreal const srcY0 = static_cast<qreal>(
                (90.0 - latitude0) / 180.0 * textureHeight);
            qreal const srcY1 = static_cast<qreal>(
                (90.0 - latitude1) / 180.0 * textureHeight);
            qreal const targetY0 =
                rect.top() + fraction0 * rect.height();
            qreal const targetY1 =
                rect.top() + fraction1 * rect.height();
            tiles.push_back({
                QRectF(x0, targetY0, x1 - x0,
                       qMax<qreal>(0.5, targetY1 - targetY0)),
                QRectF(srcX0, srcY0,
                       qMax<qreal>(1.0, srcX1 - srcX0),
                       qMax<qreal>(0.5, srcY1 - srcY0))
            });
        }
    }

    while (layer->tileNodes.size() < tiles.size()) {
        auto* node = new QSGSimpleTextureNode;
        node->setTexture(layerTexture);
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
        node->setTexture(layerTexture);
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

    // 1.0.223 — default maxAlpha 0.85 (era 0.62, poco evidente)
    float sunParams[4] = {0.0f, 0.0f, 1.0f, 0.85f};
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
    QFont const font = liveMapLabelFont();
    return text
        + QLatin1Char('|')
        + QString::number(static_cast<qulonglong>(color.rgba()), 16)
        + QLatin1Char('|')
        + font.family()
        + QLatin1Char('|')
        + QString::number(font.pixelSize())
        + QLatin1Char('|')
        + QString::number(font.pointSizeF(), 'f', 2)
        + QLatin1Char('|')
        + QString::number(font.weight());
}

QSGTexture* createLabelTexture(QQuickWindow* window,
                               const QString& text,
                               const QColor& color,
                               qint64* textureCreateUs = nullptr)
{
    if (!window || text.isEmpty()) {
        return nullptr;
    }

    QElapsedTimer timer;
    if (textureCreateUs) {
        timer.start();
    }
    QSGTexture* texture = window->createTextureFromImage(renderLabelTexture(text, color));
    if (textureCreateUs) {
        *textureCreateUs += timer.nsecsElapsed() / 1000;
    }
    if (!texture) {
        return nullptr;
    }
    texture->setFiltering(QSGTexture::Linear);
    return texture;
}

QSGTexture* labelBlankTexture(LabelLayerNode* layer, QQuickWindow* window)
{
    if (!layer || !window) {
        return nullptr;
    }
    if (!layer->blankTexture) {
        QImage image(1, 1, QImage::Format_RGBA8888_Premultiplied);
        image.fill(QColor(0, 0, 0, 0));
        layer->blankTexture = window->createTextureFromImage(image);
        if (layer->blankTexture) {
            layer->blankTexture->setFiltering(QSGTexture::Nearest);
        }
    }
    return layer->blankTexture;
}

QRectF textureSourceRect(QSGTexture* texture)
{
    QSize const size = texture ? texture->textureSize() : QSize(1, 1);
    return QRectF(0.0,
                  0.0,
                  qMax(1, size.width()),
                  qMax(1, size.height()));
}

struct StableLabelRequest {
    QString key;
    QString text;
    QColor color;
};

void pruneStableLabelImageCache(LabelLayerNode* layer)
{
    if (!layer) {
        return;
    }

    qint64 const nowMs = qMax<qint64>(1, monotonicNowMs());
    for (auto it = layer->stableImageCache.begin(); it != layer->stableImageCache.end(); ) {
        qint64 const lastUsed = layer->stableImageLastUsedMs.value(it.key(), 0);
        if (lastUsed <= 0 || nowMs - lastUsed <= kLabelImageTtlMs) {
            ++it;
            continue;
        }
        layer->stableImageLastUsedMs.remove(it.key());
        it = layer->stableImageCache.erase(it);
    }

    if (layer->stableImageCache.size() <= kMaxLabelImageCache) {
        return;
    }

    QVector<QPair<qint64, QString>> entries;
    entries.reserve(layer->stableImageCache.size());
    for (auto it = layer->stableImageCache.constBegin(); it != layer->stableImageCache.constEnd(); ++it) {
        entries.push_back({layer->stableImageLastUsedMs.value(it.key(), 0), it.key()});
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    int removeCount = layer->stableImageCache.size() - kMaxLabelImageCache;
    for (int i = 0; i < entries.size() && removeCount > 0; ++i) {
        if (layer->stableAtlasRects.contains(entries[i].second)) {
            continue;
        }
        layer->stableImageCache.remove(entries[i].second);
        layer->stableImageLastUsedMs.remove(entries[i].second);
        --removeCount;
    }
}

void ensureStableLabelAtlas(LabelLayerNode* layer,
                            QQuickWindow* window,
                            const QVector<StableLabelRequest>& labels,
                            qint64* textureCreateUs)
{
    if (!layer || !window) {
        return;
    }

    QHash<QString, StableLabelRequest> uniqueRequests;
    for (const StableLabelRequest& label : labels) {
        if (label.key.isEmpty() || label.text.isEmpty()) {
            continue;
        }
        if (!uniqueRequests.contains(label.key)) {
            uniqueRequests.insert(label.key, label);
        }
    }

    QStringList keys = uniqueRequests.keys();
    keys.sort();
    QString const signature = keys.join(QLatin1Char('\n'));
    qint64 const nowMs = qMax<qint64>(1, monotonicNowMs());
    for (const QString& key : keys) {
        layer->stableImageLastUsedMs.insert(key, nowMs);
    }

    if (signature == layer->stableAtlasSignature && layer->stableAtlasTexture) {
        pruneStableLabelImageCache(layer);
        return;
    }

    if (keys.isEmpty()) {
        if (layer->stableAtlasTexture) {
            layer->retiredAtlasTextures.push_back(layer->stableAtlasTexture);
            layer->stableAtlasTexture = nullptr;
        }
        layer->stableAtlasRects.clear();
        layer->stableAtlasSignature.clear();
        pruneStableLabelImageCache(layer);
        return;
    }

    struct AtlasImage {
        QString key;
        QImage image;
        QRect sourceRect;
    };

    QVector<AtlasImage> atlasImages;
    atlasImages.reserve(keys.size());
    for (const QString& key : keys) {
        QImage image = layer->stableImageCache.value(key);
        if (image.isNull()) {
            const StableLabelRequest request = uniqueRequests.value(key);
            image = renderLabelTexture(request.text, request.color);
            layer->stableImageCache.insert(key, image);
        }
        if (!image.isNull()) {
            atlasImages.push_back({key, image, QRect()});
        }
    }

    int x = kLabelAtlasPadding;
    int y = kLabelAtlasPadding;
    int rowHeight = 0;
    int atlasWidth = 1;
    int atlasHeight = 1;
    for (AtlasImage& entry : atlasImages) {
        if (x > kLabelAtlasPadding && x + entry.image.width() + kLabelAtlasPadding > kLabelAtlasMaxWidth) {
            x = kLabelAtlasPadding;
            y += rowHeight + kLabelAtlasPadding;
            rowHeight = 0;
        }

        entry.sourceRect = QRect(x, y, entry.image.width(), entry.image.height());
        atlasWidth = qMax(atlasWidth, x + entry.image.width() + kLabelAtlasPadding);
        atlasHeight = qMax(atlasHeight, y + entry.image.height() + kLabelAtlasPadding);
        x += entry.image.width() + kLabelAtlasPadding;
        rowHeight = qMax(rowHeight, entry.image.height());
    }

    QImage atlas(QSize(qMin(kLabelAtlasMaxWidth, atlasWidth), atlasHeight), QImage::Format_ARGB32_Premultiplied);
    atlas.fill(Qt::transparent);
    QPainter painter(&atlas);
    QHash<QString, QRectF> sourceRects;
    sourceRects.reserve(atlasImages.size());
    for (const AtlasImage& entry : atlasImages) {
        painter.drawImage(entry.sourceRect.topLeft(), entry.image);
        sourceRects.insert(entry.key, QRectF(entry.sourceRect));
    }
    painter.end();

    QElapsedTimer timer;
    if (textureCreateUs) {
        timer.start();
    }
    QSGTexture* atlasTexture = window->createTextureFromImage(atlas);
    if (textureCreateUs) {
        *textureCreateUs += timer.nsecsElapsed() / 1000;
    }
    if (!atlasTexture) {
        return;
    }
    atlasTexture->setFiltering(QSGTexture::Linear);

    if (layer->stableAtlasTexture) {
        layer->retiredAtlasTextures.push_back(layer->stableAtlasTexture);
    }
    layer->stableAtlasTexture = atlasTexture;
    layer->stableAtlasRects = sourceRects;
    layer->stableAtlasSignature = signature;
    pruneStableLabelImageCache(layer);
}

struct MarkerVertex {
    float lon;
    float lat;
    float offsetX;
    float offsetY;
    float texU;
    float texV;
};

const QSGGeometry::AttributeSet& markerAttributes()
{
    static QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::create(0, 2, QSGGeometry::FloatType),
        QSGGeometry::Attribute::create(1, 4, QSGGeometry::FloatType)
    };
    static QSGGeometry::AttributeSet attributeSet {
        2,
        sizeof(MarkerVertex),
        attributes
    };
    return attributeSet;
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
        if (uniformData->size() < 128) {
            uniformData->resize(128);
            std::memset(uniformData->data(), 0, static_cast<size_t>(uniformData->size()));
        }
        auto* material = static_cast<MarkerMaterial*>(newMaterial);
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

struct ArrowPathRequest {
    QString key;
    QPointF sourceLonLat;
    QPointF destinationLonLat;
    WorldMapGpuItem::PathRole role {WorldMapGpuItem::PathRole::Generic};
};

struct ArrowVertex {
    float sourceLon;
    float sourceLat;
    float destinationLon;
    float destinationLat;
    float phaseOffset;
    float cornerForward;
    float cornerNormal;
    float txFlag;
};

const QSGGeometry::AttributeSet& arrowAttributes()
{
    static QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::create(0, 4, QSGGeometry::FloatType),
        QSGGeometry::Attribute::create(1, 4, QSGGeometry::FloatType)
    };
    static QSGGeometry::AttributeSet attributeSet {
        2,
        sizeof(ArrowVertex),
        attributes
    };
    return attributeSet;
}

#ifdef DECODIUM_LIVEMAP_ARROW_QSB
class ArrowMaterial final : public QSGMaterial
{
public:
    ArrowMaterial()
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
        auto const* rhs = static_cast<const ArrowMaterial*>(other);
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
            if (animParams[i] < rhs->animParams[i])
                return -1;
            if (animParams[i] > rhs->animParams[i])
                return 1;
        }
        return 0;
    }

    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float viewParams[4] = {0.0f, 0.0f, 360.0f, 180.0f};
    float rectParams[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    float animParams[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

class ArrowShader final : public QSGMaterialShader
{
public:
    ArrowShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/livemap_arrow.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/livemap_arrow.frag.qsb"));
    }

    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial*) override
    {
        QByteArray* uniformData = state.uniformData();
        if (uniformData->size() < 144) {
            uniformData->resize(144);
            std::memset(uniformData->data(), 0, static_cast<size_t>(uniformData->size()));
        }
        auto* material = static_cast<ArrowMaterial*>(newMaterial);
        QMatrix4x4 const matrix = state.combinedMatrix();
        std::memcpy(uniformData->data(), matrix.constData(), 64);
        float const opacity = state.opacity();
        std::memcpy(uniformData->data() + 64, &opacity, 4);
        std::memcpy(uniformData->data() + 80, material->color, sizeof(material->color));
        std::memcpy(uniformData->data() + 96, material->viewParams, sizeof(material->viewParams));
        std::memcpy(uniformData->data() + 112, material->rectParams, sizeof(material->rectParams));
        std::memcpy(uniformData->data() + 128, material->animParams, sizeof(material->animParams));
        return true;
    }
};

QSGMaterialShader* ArrowMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new ArrowShader;
}
#endif

QSGGeometryNode* ensureFlatLineNode(QSGNode* parent, QSGGeometryNode*& node,
                                    const QColor& color, float width = 1.0f)
{
    if (!node) {
        auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawLines);
        geometry->setLineWidth(width);
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
    node->geometry()->setLineWidth(width);
    return node;
}

void updateFlatLineNode(QSGNode* parent, QSGGeometryNode*& node,
                        const QVector<QPointF>& points, const QColor& color,
                        float width = 1.0f)
{
    auto* lineNode = ensureFlatLineNode(parent, node, color, width);
    auto* geometry = lineNode->geometry();
    geometry->allocate(points.size());
    geometry->setDrawingMode(QSGGeometry::DrawLines);
    geometry->setLineWidth(width);
    auto* vertices = geometry->vertexDataAsPoint2D();
    for (int i = 0; i < points.size(); ++i) {
        vertices[i].set(static_cast<float>(points[i].x()), static_cast<float>(points[i].y()));
    }
    geometry->markVertexDataDirty();
    lineNode->markDirty(QSGNode::DirtyGeometry);
}

void updateCoverageTriangleNode(QSGNode* parent,
                                QSGGeometryNode*& node,
                                const QVector<QPointF>& points,
                                const QColor& color)
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
        vertices[i].set(static_cast<float>(points.at(i).x()),
                        static_cast<float>(points.at(i).y()));
    }
    geometry->markVertexDataDirty();
    // Dynamic vertex data alone is insufficient on some RHI backends. Mark
    // the owning node as dirty as well, otherwise newly-enabled coverage and
    // geographic layers can retain an empty GPU buffer until a full rebuild.
    node->markDirty(QSGNode::DirtyGeometry);
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

void updateGeoMarkerNode(QSGNode* parent, QSGGeometryNode*& node,
                         const QVector<QPointF>& lonLatPoints,
                         const QColor& color,
                         float radius,
                         double centerLon,
                         double centerLat,
                         double spanLon,
                         double spanLat,
                         const QRectF& rect,
                         bool updateGeometry)
{
#ifdef DECODIUM_LIVEMAP_MARKER_QSB
    bool created = false;
    if (!node) {
        auto* geometry = new QSGGeometry(markerAttributes(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);

        auto* material = new MarkerMaterial;

        node = new QSGGeometryNode;
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
        parent->appendChildNode(node);
        created = true;
    }
    if (auto* material = static_cast<MarkerMaterial*>(node->material())) {
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
    int const vertexCount = lonLatPoints.size() * 6;
    if (!created && !updateGeometry && geometry->vertexCount() == vertexCount) {
        return;
    }

    geometry->allocate(vertexCount);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vertices = static_cast<MarkerVertex*>(geometry->vertexData());
    int out = 0;
    auto write = [&](const QPointF& p, float dx, float dy, float u, float v) {
        vertices[out++] = {static_cast<float>(p.x()),
                           static_cast<float>(p.y()),
                           dx * radius,
                           dy * radius,
                           u,
                           v};
    };
    for (const QPointF& p : lonLatPoints) {
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
    Q_UNUSED(lonLatPoints);
    Q_UNUSED(color);
    Q_UNUSED(radius);
    Q_UNUSED(centerLon);
    Q_UNUSED(centerLat);
    Q_UNUSED(spanLon);
    Q_UNUSED(spanLat);
    Q_UNUSED(rect);
    Q_UNUSED(updateGeometry);
#endif
}

void updateScreenCircleNode(QSGNode* parent, QSGGeometryNode*& node,
                            const QVector<QPointF>& points, const QColor& color, float radius)
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

    constexpr int segments = 14;
    auto* geometry = node->geometry();
    geometry->allocate(points.size() * segments * 3);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vertices = geometry->vertexDataAsPoint2D();
    int out = 0;
    for (const QPointF& center : points) {
        for (int i = 0; i < segments; ++i) {
            double const a0 = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(segments);
            double const a1 = 2.0 * M_PI * static_cast<double>(i + 1) / static_cast<double>(segments);
            QPointF const p0 = center + QPointF(std::cos(a0) * radius, std::sin(a0) * radius);
            QPointF const p1 = center + QPointF(std::cos(a1) * radius, std::sin(a1) * radius);
            vertices[out++].set(static_cast<float>(center.x()), static_cast<float>(center.y()));
            vertices[out++].set(static_cast<float>(p0.x()), static_cast<float>(p0.y()));
            vertices[out++].set(static_cast<float>(p1.x()), static_cast<float>(p1.y()));
        }
    }
    geometry->markVertexDataDirty();
    node->markDirty(QSGNode::DirtyGeometry);
}

void updateScreenSatelliteNode(QSGNode* parent, QSGGeometryNode*& node,
                               const QVector<QPointF>& points, const QColor& color,
                               float scale)
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

    // A compact, platform-independent satellite silhouette: central bus,
    // solar panels, panel struts and a short antenna.  Keeping this as QSG
    // triangle geometry avoids font/emoji differences and texture uploads.
    constexpr int verticesPerSatellite = 45;
    auto* geometry = node->geometry();
    geometry->allocate(points.size() * verticesPerSatellite);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vertices = geometry->vertexDataAsPoint2D();
    int out = 0;
    auto write = [&](const QPointF& point) {
        vertices[out++].set(static_cast<float>(point.x()),
                            static_cast<float>(point.y()));
    };
    auto triangle = [&](const QPointF& a, const QPointF& b, const QPointF& c) {
        write(a);
        write(b);
        write(c);
    };
    auto quad = [&](const QPointF& topLeft, const QPointF& bottomRight) {
        QPointF const topRight(bottomRight.x(), topLeft.y());
        QPointF const bottomLeft(topLeft.x(), bottomRight.y());
        triangle(topLeft, topRight, bottomLeft);
        triangle(topRight, bottomRight, bottomLeft);
    };
    auto relative = [scale](qreal x, qreal y) {
        return QPointF(x * scale, y * scale);
    };

    for (const QPointF& center : points) {
        // Central equipment bus (a softly hexagonal body).
        QPointF const bodyTopLeft = center + relative(-3.4, -4.0);
        QPointF const bodyTopRight = center + relative(3.4, -4.0);
        QPointF const bodyRight = center + relative(4.4, 0.0);
        QPointF const bodyBottomRight = center + relative(3.4, 4.0);
        QPointF const bodyBottomLeft = center + relative(-3.4, 4.0);
        QPointF const bodyLeft = center + relative(-4.4, 0.0);
        triangle(center, bodyTopLeft, bodyTopRight);
        triangle(center, bodyTopRight, bodyRight);
        triangle(center, bodyRight, bodyBottomRight);
        triangle(center, bodyBottomRight, bodyBottomLeft);
        triangle(center, bodyBottomLeft, bodyLeft);
        triangle(center, bodyLeft, bodyTopLeft);

        // Two solar panels and their short support arms.
        quad(center + relative(-12.0, -3.0), center + relative(-6.0, 3.0));
        quad(center + relative(6.0, -3.0), center + relative(12.0, 3.0));
        quad(center + relative(-6.0, -0.65), center + relative(-4.2, 0.65));
        quad(center + relative(4.2, -0.65), center + relative(6.0, 0.65));

        // Antenna mast, wide enough to remain visible at normal map scale.
        triangle(center + relative(-0.8, -3.8),
                 center + relative(0.8, -3.8),
                 center + relative(0.0, -7.2));
    }
    Q_ASSERT(out == points.size() * verticesPerSatellite);
    geometry->markVertexDataDirty();
    node->markDirty(QSGNode::DirtyGeometry);
}

void updatePathNode(QSGNode* parent, QSGGeometryNode*& node,
                    const QVector<WorldMapGpuItem::PathLine>& paths,
                    const QColor& color,
                    double centerLon,
                    double centerLat,
                    double spanLon,
                    double spanLat,
                    const QRectF& rect,
                    bool updateGeometry)
{
#ifdef DECODIUM_LIVEMAP_PATH_QSB
    bool created = false;
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
        created = true;
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
    if (!created && !updateGeometry && geometry->vertexCount() == vertexCount) {
        return;
    }

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
    Q_UNUSED(updateGeometry);
#endif
}

void updateArrowNode(QSGNode* parent,
                     QSGGeometryNode*& node,
                     int& previousVertexCount,
                     const QVector<ArrowPathRequest>& paths,
                     WorldMapGpuItem::PathRole role,
                     const QColor& color,
                     double centerLon,
                     double centerLat,
                     double spanLon,
                     double spanLat,
                     const QRectF& rect,
                     qreal phase,
                     qreal txProgress,
                     bool transmitting,
                     const QString& txTargetCall,
                     bool updateGeometry)
{
#ifdef DECODIUM_LIVEMAP_ARROW_QSB
    bool created = false;
    if (!node) {
        auto* geometry = new QSGGeometry(arrowAttributes(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);

        auto* material = new ArrowMaterial;

        node = new QSGGeometryNode;
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
        parent->appendChildNode(node);
        created = true;
    }

    if (auto* material = static_cast<ArrowMaterial*>(node->material())) {
        colorToFloat4(color, material->color);
        material->viewParams[0] = static_cast<float>(centerLon);
        material->viewParams[1] = static_cast<float>(centerLat);
        material->viewParams[2] = static_cast<float>(spanLon);
        material->viewParams[3] = static_cast<float>(spanLat);
        material->rectParams[0] = static_cast<float>(rect.left());
        material->rectParams[1] = static_cast<float>(rect.top());
        material->rectParams[2] = static_cast<float>(rect.width());
        material->rectParams[3] = static_cast<float>(rect.height());
        material->animParams[0] = static_cast<float>(phase);
        material->animParams[1] = static_cast<float>(txProgress);
        material->animParams[2] = transmitting ? 1.0f : 0.0f;
        material->animParams[3] = 0.0f;
        node->markDirty(QSGNode::DirtyMaterial);
    }

    int pathCount = 0;
    for (const ArrowPathRequest& path : paths) {
        if (path.role == role) {
            ++pathCount;
        }
    }

    auto* geometry = node->geometry();
    int const vertexCount = pathCount * 3;
    previousVertexCount = vertexCount;
    if (!created && !updateGeometry && geometry->vertexCount() == vertexCount) {
        return;
    }

    geometry->allocate(vertexCount);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* vertices = static_cast<ArrowVertex*>(geometry->vertexData());
    int out = 0;
    auto write = [&](const ArrowPathRequest& path, float phaseOffset, float forward, float normal, float txFlag) {
        vertices[out++] = {static_cast<float>(path.sourceLonLat.x()),
                           static_cast<float>(path.sourceLonLat.y()),
                           static_cast<float>(path.destinationLonLat.x()),
                           static_cast<float>(path.destinationLonLat.y()),
                           phaseOffset,
                           forward,
                           normal,
                           txFlag};
    };

    for (const ArrowPathRequest& path : paths) {
        if (path.role != role) {
            continue;
        }
        float const phaseOffset = static_cast<float>((qHash(path.key) % 17) * 0.057);
        bool const txMatch = transmitting
            && role == WorldMapGpuItem::PathRole::OutgoingFromMe
            && (txTargetCall.isEmpty() || sameStationCall(path.key, txTargetCall));
        float const txFlag = txMatch ? 1.0f : 0.0f;
        write(path, phaseOffset, 0.0f, 0.0f, txFlag);
        write(path, phaseOffset, -9.0f, 4.2f, txFlag);
        write(path, phaseOffset, -9.0f, -4.2f, txFlag);
    }

    geometry->markVertexDataDirty();
    node->markDirty(QSGNode::DirtyGeometry);
#else
    Q_UNUSED(parent);
    Q_UNUSED(node);
    Q_UNUSED(previousVertexCount);
    Q_UNUSED(paths);
    Q_UNUSED(role);
    Q_UNUSED(color);
    Q_UNUSED(centerLon);
    Q_UNUSED(centerLat);
    Q_UNUSED(spanLon);
    Q_UNUSED(spanLat);
    Q_UNUSED(rect);
    Q_UNUSED(phase);
    Q_UNUSED(txProgress);
    Q_UNUSED(transmitting);
    Q_UNUSED(txTargetCall);
    Q_UNUSED(updateGeometry);
#endif
}

}

WorldMapGpuItem::WorldMapGpuItem(QQuickItem* parent)
    : QQuickItem(parent)
    , m_mapImage(buildMapTexture())
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);

    m_frameTimer.setInterval(m_frameIntervalMs);
    connect(&m_frameTimer, &QTimer::timeout, this, [this]() {
        if (m_active && isVisible()) {
            m_animationPhase = std::fmod(m_animationPhase + (m_conservativeRenderer ? 0.006 : 0.010), 1.0);
            bool const pruned = pruneExpiredContacts();
            updateViewportTargets();
            bool const viewportChanged = smoothViewport();
            if (pruned) {
                m_greylineGeometryDirty = true;
                m_geometryDirty = true;
                m_contactGeometryDirty = true;
                m_animationGeometryDirty = true;
            } else if (viewportChanged) {
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
    m_azimuthalMapDirty = true;
    m_azimuthalExternalOverlayDirty = true;
    m_baseMapTextureDirty = true;
    m_externalOverlayTextureDirty = true;
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
    if (m_distanceInMiles == enabled) {
        return;
    }
    m_distanceInMiles = enabled;
    m_geometryDirty = true;
    update();
}

void WorldMapGpuItem::setCoveragePushPins(bool enabled)
{
    if (m_coveragePushPins == enabled) {
        return;
    }
    m_coveragePushPins = enabled;
    markDirty(false);
}

void WorldMapGpuItem::setTimeZoneOverlayEnabled(bool enabled)
{
    if (m_timeZoneOverlayEnabled == enabled) {
        return;
    }
    m_timeZoneOverlayEnabled = enabled;
    markDirty(false);
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

    QString const nextTargetCall = transmitting ? normalizedCall : QString();
    QString const nextTargetGrid = transmitting ? normalizedGrid : QString();
    bool const animationStateChanged = m_transmitting != transmitting
        || m_txTargetCall != nextTargetCall
        || m_txTargetGrid != nextTargetGrid
        || m_txTravelMs != travelMs;
    bool const resetProgress = transmitting
        && (!m_transmitting
            || normalizedCall != m_txTargetCall
            || normalizedGrid != m_txTargetGrid
            || travelMs != m_txTravelMs);

    m_transmitting = transmitting;
    m_txTargetCall = nextTargetCall;
    m_txTargetGrid = nextTargetGrid;
    m_txTravelMs = travelMs;
    if (resetProgress) {
        m_txStartMs = QDateTime::currentMSecsSinceEpoch();
    } else if (!transmitting) {
        m_txStartMs = 0;
    }
    if (animationStateChanged) {
        m_animationGeometryDirty = true;
    }
    if (isVisible()) {
        update();
    }
}

void WorldMapGpuItem::setBaseMapEnabled(bool enabled)
{
    if (m_baseMapEnabled == enabled) {
        return;
    }
    m_baseMapEnabled = enabled;
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

void WorldMapGpuItem::setCoverageCells(const QVariantList& cells)
{
    QVector<CoverageCell> next;
    next.reserve(cells.size());
    int fourCharacterCells = 0;
    int sixCharacterCells = 0;
    int activeCells = 0;
    int missingCells = 0;
    int pskCells = 0;
    int workedCells = 0;
    int confirmedCells = 0;
    for (const QVariant& value : cells) {
        QVariantMap const row = value.toMap();
        CoverageCell cell;
        QString const grid = row.value(QStringLiteral("grid")).toString().trimmed().toUpper();
        cell.grid = grid.left(grid.size() >= 6 ? 6 : 4);
        cell.workedCount = row.value(QStringLiteral("workedCount")).toInt();
        cell.confirmedCount = row.value(QStringLiteral("confirmedCount")).toInt();
        cell.activeCount = row.value(QStringLiteral("activeCount")).toInt();
        cell.pskCount = row.value(QStringLiteral("pskCount")).toInt();
        cell.historicalStatus = row.value(QStringLiteral("historicalStatus")).toString();
        cell.liveStatus = row.value(QStringLiteral("liveStatus")).toString();
        cell.liveOpacity = qBound<qreal>(
            0.2, row.value(QStringLiteral("liveOpacity"), 1.0).toReal(), 1.0);
        cell.split = row.value(QStringLiteral("split")).toBool();
        cell.worked = row.value(QStringLiteral("worked")).toBool();
        cell.confirmed = row.value(QStringLiteral("confirmed")).toBool();
        cell.active = row.value(QStringLiteral("active")).toBool();
        cell.missing = row.value(QStringLiteral("missing")).toBool();
        cell.psk = row.value(QStringLiteral("psk")).toBool();
        QPointF southWest;
        QPointF northEast;
        if (maidenheadCellBounds(cell.grid, &southWest, &northEast)) {
            if (cell.grid.size() >= 6) {
                ++sixCharacterCells;
            } else {
                ++fourCharacterCells;
            }
            activeCells += cell.active ? 1 : 0;
            missingCells += cell.missing ? 1 : 0;
            pskCells += cell.psk ? 1 : 0;
            workedCells += cell.worked ? 1 : 0;
            confirmedCells += cell.confirmed ? 1 : 0;
            next.push_back(cell);
        }
    }
    QString const ingressDiagnostic = QStringLiteral(
        "coverage input=%1 accepted=%2 grid4=%3 grid6=%4 active=%5 missing=%6 psk=%7 worked=%8 confirmed=%9")
        .arg(cells.size())
        .arg(next.size())
        .arg(fourCharacterCells)
        .arg(sixCharacterCells)
        .arg(activeCells)
        .arg(missingCells)
        .arg(pskCells)
        .arg(workedCells)
        .arg(confirmedCells);
    if (ingressDiagnostic != m_lastCoverageIngressDiagnostic) {
        m_lastCoverageIngressDiagnostic = ingressDiagnostic;
        qInfo().noquote() << "[MAPGEO]" << ingressDiagnostic;
    }
    m_coverageCells = next;
    m_geometryDirty = true;
    if (isVisible()) {
        update();
    }
}

void WorldMapGpuItem::setOperationalMarkers(const QVariantList& markers)
{
    if (m_operationalMarkers == markers) {
        return;
    }
    m_operationalMarkers = markers;
    markDirty(false);
}

void WorldMapGpuItem::setGeographicFeatures(const QVariantList& features)
{
    int stateFeatures = 0;
    int countyFeatures = 0;
    int earthquakeFeatures = 0;
    int polygonCount = 0;
    int ringCount = 0;
    int pointCount = 0;
    QString coordinateShape;
    for (const QVariant& featureValue : features) {
        QVariantMap const feature = featureValue.toMap();
        QString const type = feature.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("states")) {
            ++stateFeatures;
        } else if (type == QStringLiteral("counties")) {
            ++countyFeatures;
        } else if (type == QStringLiteral("earthquake")) {
            ++earthquakeFeatures;
        }
        for (const QVariant& polygonValue
             : feature.value(QStringLiteral("polygons")).toList()) {
            ++polygonCount;
            for (const QVariant& ringValue : polygonValue.toList()) {
                QVariantList const ring = ringValue.toList();
                ++ringCount;
                pointCount += ring.size();
                if (coordinateShape.isEmpty() && !ring.isEmpty()) {
                    QVariant const point = ring.constFirst();
                    QVariantList const components = point.toList();
                    QStringList componentTypes;
                    for (const QVariant& component : components) {
                        componentTypes << QString::fromLatin1(component.metaType().name());
                    }
                    coordinateShape = QStringLiteral("point=%1 components=%2 [%3]")
                        .arg(QString::fromLatin1(point.metaType().name()))
                        .arg(components.size())
                        .arg(componentTypes.join(QLatin1Char(',')));
                }
            }
        }
    }
    QString const ingressDiagnostic = QStringLiteral(
        "geographic input=%1 states=%2 counties=%3 earthquakes=%4 polygons=%5 rings=%6 points=%7 shape=%8")
        .arg(features.size())
        .arg(stateFeatures)
        .arg(countyFeatures)
        .arg(earthquakeFeatures)
        .arg(polygonCount)
        .arg(ringCount)
        .arg(pointCount)
        .arg(coordinateShape.isEmpty() ? QStringLiteral("none") : coordinateShape);
    if (ingressDiagnostic != m_lastGeographicIngressDiagnostic) {
        m_lastGeographicIngressDiagnostic = ingressDiagnostic;
        qInfo().noquote() << "[MAPGEO]" << ingressDiagnostic;
    }
    if (m_geographicFeatures == features) {
        return;
    }
    m_geographicFeatures = features;
    markDirty(false);
}

void WorldMapGpuItem::setProjection(const QString& projection)
{
    QString normalized = projection.trimmed();
    if (normalized != QStringLiteral("Mercator")
        && normalized != QStringLiteral("Miller")
        && normalized != QStringLiteral("Azimuthal Equidistant")) {
        normalized = QStringLiteral("Equirectangular");
    }
    if (m_projection == normalized) {
        return;
    }
    m_projection = normalized;
    m_azimuthalMapDirty = true;
    m_azimuthalExternalOverlayDirty = true;
    m_projectionNodeDirty = true;
    m_baseMapTextureDirty = true;
    m_externalOverlayTextureDirty = true;
    m_greylineGeometryDirty = true;
    updateViewportTargets();
    markDirty();
}

void WorldMapGpuItem::setLayerStyles(const QVariantMap& styles)
{
    if (m_layerStyles == styles) {
        return;
    }
    m_layerStyles = styles;
    m_geometryDirty = true;
    m_contactGeometryDirty = true;
    markDirty(false);
}

QVariantMap WorldMapGpuItem::viewportState() const
{
    return {
        {QStringLiteral("centerLongitude"), m_targetCenterLon},
        {QStringLiteral("centerLatitude"), m_targetCenterLat},
        {QStringLiteral("spanLongitude"), m_targetSpanLon},
        {QStringLiteral("spanLatitude"), m_targetSpanLat},
        {QStringLiteral("locked"), m_userViewportLocked}
    };
}

void WorldMapGpuItem::setViewportState(const QVariantMap& state)
{
    bool okLon = false;
    bool okLat = false;
    bool okSpanLon = false;
    bool okSpanLat = false;
    double const lon = state.value(QStringLiteral("centerLongitude")).toDouble(&okLon);
    double const lat = state.value(QStringLiteral("centerLatitude")).toDouble(&okLat);
    double const spanLon = state.value(QStringLiteral("spanLongitude")).toDouble(&okSpanLon);
    double const spanLat = state.value(QStringLiteral("spanLatitude")).toDouble(&okSpanLat);
    if (!okLon || !okLat || !okSpanLon || !okSpanLat
        || !std::isfinite(lon) || !std::isfinite(lat)
        || !std::isfinite(spanLon) || !std::isfinite(spanLat)) {
        return;
    }
    m_targetCenterLon = wrapLongitude(lon);
    m_targetSpanLon = qBound(12.0, spanLon, 360.0);
    m_targetSpanLat = qBound(8.0, spanLat, 180.0);
    m_targetCenterLat = qBound(-90.0 + 0.5 * m_targetSpanLat,
                               lat, 90.0 - 0.5 * m_targetSpanLat);
    m_userViewportLocked = state.value(QStringLiteral("locked"), true).toBool();
    m_viewVelocityLon = 0.0;
    m_viewVelocityLat = 0.0;
    m_viewVelocitySpanLon = 0.0;
    m_viewVelocitySpanLat = 0.0;
    markDirty(false);
}

QColor WorldMapGpuItem::styledColor(const QString& layerId,
                                    const QColor& fallback, int alpha) const
{
    QColor result = fallback;
    QVariantMap const style = m_layerStyles.value(layerId).toMap();
    QColor const configured(style.value(QStringLiteral("color")).toString());
    if (configured.isValid()) {
        result = configured;
    }
    double const opacity = qBound(0.05,
        style.value(QStringLiteral("opacity"), 1.0).toDouble(), 1.0);
    int const baseAlpha = alpha >= 0 ? alpha : result.alpha();
    result.setAlpha(qBound(0, qRound(baseAlpha * opacity), 255));
    return result;
}

double WorldMapGpuItem::layerThickness(const QString& layerId, double fallback) const
{
    QVariantMap const style = m_layerStyles.value(layerId).toMap();
    return qBound(0.5, style.value(QStringLiteral("thickness"), fallback).toDouble(), 8.0);
}

int WorldMapGpuItem::layerLabelDensity(const QString& layerId) const
{
    QVariantMap const style = m_layerStyles.value(layerId).toMap();
    return qBound(0, style.value(QStringLiteral("labelDensity"), 100).toInt(), 100);
}

void WorldMapGpuItem::setBaseMapService(QObject* service)
{
    auto* baseMapService = qobject_cast<MapBaseMapService*>(service);
    if (m_baseMapService == baseMapService) {
        return;
    }
    if (m_baseMapConnection) {
        disconnect(m_baseMapConnection);
    }
    m_baseMapService = baseMapService;

    auto syncBaseMap = [this] {
        m_mapImage = m_baseMapService
            ? m_baseMapService->baseMapImage()
            : buildMapTexture();
        m_baseMapTextureDirty = true;
        m_azimuthalMapDirty = true;
        if (isVisible()) {
            update();
        }
    };
    if (m_baseMapService) {
        m_baseMapConnection = connect(
            m_baseMapService,
            &MapBaseMapService::baseMapImageChanged,
            this,
            syncBaseMap);
    }
    syncBaseMap();
}

void WorldMapGpuItem::setExternalOverlayService(QObject* service)
{
    auto* overlayService = qobject_cast<MapExternalOverlayService*>(service);
    if (m_externalOverlayService == overlayService) {
        return;
    }
    if (m_externalOverlayConnection) {
        disconnect(m_externalOverlayConnection);
    }
    m_externalOverlayService = overlayService;

    auto syncOverlay = [this] {
        m_externalOverlayImage = m_externalOverlayService
            ? m_externalOverlayService->overlayImage()
            : QImage();
        m_externalOverlayTextureDirty = true;
        m_azimuthalExternalOverlayDirty = true;
        if (isVisible()) {
            update();
        }
    };
    if (m_externalOverlayService) {
        m_externalOverlayConnection = connect(
            m_externalOverlayService,
            &MapExternalOverlayService::overlayImageChanged,
            this,
            syncOverlay);
    }
    syncOverlay();
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
    // Qt Quick commonly selects OpenGL on Linux.  The old policy disabled the
    // greyline shader for every OpenGL renderer, so the toolbar toggle changed
    // state but the layer was immediately cleared.  QSB already provides a
    // GLSL 120/150/330 fallback; keep OpenGL enabled by default and retain an
    // explicit opt-out for drivers that are known to misbehave.
    bool const forceOpenGlGreyline = environmentFlag(
        "DECODIUM_ENABLE_OPENGL_LIVEMAP_GREYLINE");
    bool const disableOpenGlGreyline = environmentFlag(
        "DECODIUM_DISABLE_OPENGL_LIVEMAP_GREYLINE");
    bool const greylineShaderAllowed = !openGl || !disableOpenGlGreyline;
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
        << (openGl && disableOpenGlGreyline
                ? " reason=OpenGL_disabled_by_environment"
                : (openGl && forceOpenGlGreyline
                       ? " reason=OpenGL_enabled_by_environment" : ""));
}

QSGNode* WorldMapGpuItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    if (!window()) {
        delete oldNode;
        return nullptr;
    }
    configureRendererPolicy();

    QElapsedTimer mapSyncTimer;
    mapSyncTimer.start();
    m_lastMapRebuildUs = 0;
    m_lastLabelLayoutUs = 0;
    m_lastLabelTextureCreateUs = 0;
    m_lastMapSyncNodesUs = 0;

    auto* root = oldNode ? oldNode : new QSGNode;

    QRectF const rect = mapRect();
    if (rect.isEmpty()) {
        clearNode(root);
        return root;
    }

    // Marker/path QSB materials encode longitude/latitude directly.  Recreate
    // the layer when switching projection so AEQD can use screen-space nodes.
    if (m_projectionNodeDirty) {
        clearNode(root);
        m_projectionNodeDirty = false;
        m_geometryDirty = true;
        m_contactGeometryDirty = true;
        m_animationGeometryDirty = true;
    }

    QImage const& mapImage = displayedMapImage();
    QImage const& externalOverlayImage = displayedExternalOverlayImage();
    QString const textureProjection = azimuthalProjectionEnabled()
        ? QStringLiteral("Equirectangular") : m_projection;

    auto* mapLayer = dynamic_cast<MapLayerNode*>(root->firstChild());
    if (!mapLayer) {
        clearNode(root);
        mapLayer = new MapLayerNode;
        mapLayer->texture = window()->createTextureFromImage(mapImage);
        if (mapLayer->texture) {
            mapLayer->texture->setFiltering(QSGTexture::Linear);
            mapLayer->texture->setMipmapFiltering(QSGTexture::Linear);
        }
        root->appendChildNode(mapLayer);
        m_baseMapTextureDirty = false;
    } else if (m_baseMapTextureDirty) {
        // Tile nodes retain a non-owning QSGTexture pointer.  They must be
        // destroyed before the old texture is released (notably when Offline
        // mode swaps an online image for the local atlas).
        clearMapTileNodes(mapLayer);
        delete mapLayer->texture;
        mapLayer->texture = nullptr;
        mapLayer->texture = window()->createTextureFromImage(mapImage);
        if (mapLayer->texture) {
            mapLayer->texture->setFiltering(QSGTexture::Linear);
            mapLayer->texture->setMipmapFiltering(QSGTexture::Linear);
        }
        m_baseMapTextureDirty = false;
    }
    int mapTextureWidth = mapImage.width();
    int mapTextureHeight = mapImage.height();
    if (!mapLayer->texture) {
        mapBlankTexture(mapLayer, window());
        mapTextureWidth = 1;
        mapTextureHeight = 1;
    }
    if (m_baseMapEnabled) {
        appendMapTileNodes(mapLayer, rect,
                           m_viewCenterLon, m_viewCenterLat,
                           m_viewSpanLon, m_viewSpanLat,
                           mapTextureWidth, mapTextureHeight,
                           textureProjection);
    } else {
        appendMapTileNodes(mapLayer, QRectF(),
                           m_viewCenterLon, m_viewCenterLat,
                           m_viewSpanLon, m_viewSpanLat,
                           mapTextureWidth, mapTextureHeight,
                           textureProjection);
    }

    auto* externalOverlayLayer =
        dynamic_cast<MapLayerNode*>(mapLayer->nextSibling());
    auto* greylineLayer = externalOverlayLayer
        ? dynamic_cast<GreylineLayerNode*>(externalOverlayLayer->nextSibling())
        : nullptr;
    auto* geometryLayer = greylineLayer
        ? dynamic_cast<GeometryLayerNode*>(greylineLayer->nextSibling())
        : nullptr;
    auto* labelLayer = geometryLayer
        ? dynamic_cast<LabelLayerNode*>(geometryLayer->nextSibling())
        : nullptr;
    auto* animationLayer = labelLayer
        ? dynamic_cast<AnimationLayerNode*>(labelLayer->nextSibling())
        : nullptr;
    if (!externalOverlayLayer || !greylineLayer
        || !geometryLayer || !labelLayer || !animationLayer) {
        while (auto* child = mapLayer->nextSibling()) {
            root->removeChildNode(child);
            delete child;
        }
        externalOverlayLayer = new MapLayerNode;
        greylineLayer = new GreylineLayerNode;
        geometryLayer = new GeometryLayerNode;
        labelLayer = new LabelLayerNode;
        animationLayer = new AnimationLayerNode;
        root->appendChildNode(externalOverlayLayer);
        root->appendChildNode(greylineLayer);
        root->appendChildNode(geometryLayer);
        root->appendChildNode(labelLayer);
        root->appendChildNode(animationLayer);
        m_greylineGeometryDirty = true;
        m_geometryDirty = true;
        m_contactGeometryDirty = true;
        m_animationGeometryDirty = true;
        m_externalOverlayTextureDirty = true;
    }

    if (m_externalOverlayTextureDirty) {
        clearMapTileNodes(externalOverlayLayer);
        delete externalOverlayLayer->texture;
        externalOverlayLayer->texture = nullptr;
        if (!externalOverlayImage.isNull()) {
            externalOverlayLayer->texture =
                window()->createTextureFromImage(externalOverlayImage);
            if (externalOverlayLayer->texture) {
                externalOverlayLayer->texture->setFiltering(QSGTexture::Linear);
            }
        }
        m_externalOverlayTextureDirty = false;
    }
    if (externalOverlayLayer->texture) {
        appendMapTileNodes(externalOverlayLayer, rect,
                           m_viewCenterLon, m_viewCenterLat,
                           m_viewSpanLon, m_viewSpanLat,
                           externalOverlayImage.width(),
                           externalOverlayImage.height(),
                           textureProjection);
    } else {
        appendMapTileNodes(externalOverlayLayer, QRectF(),
                           m_viewCenterLon, m_viewCenterLat,
                           m_viewSpanLon, m_viewSpanLat, 1, 1,
                           textureProjection);
    }

    if (m_greylineEnabled && m_greylineShaderAllowed
        && !azimuthalProjectionEnabled()) {
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
            // 1.0.223 — maxAlpha 0.62 -> 0.85 per greyline piu' evidente
            material->sunParams[3] = 0.85f;
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

    bool const rebuildBatch = m_geometryDirty || m_contactGeometryDirty;
    bool const uploadContactGeometry = m_contactGeometryDirty;
    if (rebuildBatch) {
        QElapsedTimer rebuildTimer;
        rebuildTimer.start();
        rebuildGeometryBatch();
        m_lastMapRebuildUs = rebuildTimer.nsecsElapsed() / 1000;
        updateCoverageTriangleNode(geometryLayer, geometryLayer->workedCoverageFill,
                                   m_batch.workedCoverageTriangles,
                                   styledColor(QStringLiteral("worked"), QColor(0, 216, 255), 38));
        updateCoverageTriangleNode(geometryLayer, geometryLayer->confirmedCoverageFill,
                                   m_batch.confirmedCoverageTriangles,
                                   styledColor(QStringLiteral("confirmed"), QColor(46, 204, 113), 54));
        updateCoverageTriangleNode(geometryLayer, geometryLayer->activeCoverageFill,
                                   m_batch.activeCoverageTriangles,
                                   styledColor(QStringLiteral("active"), QColor(246, 195, 68), 76));
        updateCoverageTriangleNode(geometryLayer, geometryLayer->activeCoverageMediumFill,
                                   m_batch.activeCoverageMediumTriangles,
                                   styledColor(QStringLiteral("active"), QColor(246, 195, 68), 52));
        updateCoverageTriangleNode(geometryLayer, geometryLayer->activeCoverageFadedFill,
                                   m_batch.activeCoverageFadedTriangles,
                                   styledColor(QStringLiteral("active"), QColor(246, 195, 68), 32));
        updateCoverageTriangleNode(geometryLayer, geometryLayer->missingCoverageFill,
                                   m_batch.missingCoverageTriangles,
                                   styledColor(QStringLiteral("missing"), QColor(255, 140, 66), 88));
        updateCoverageTriangleNode(geometryLayer, geometryLayer->missingCoverageMediumFill,
                                   m_batch.missingCoverageMediumTriangles,
                                   styledColor(QStringLiteral("missing"), QColor(255, 140, 66), 58));
        updateCoverageTriangleNode(geometryLayer, geometryLayer->missingCoverageFadedFill,
                                   m_batch.missingCoverageFadedTriangles,
                                   styledColor(QStringLiteral("missing"), QColor(255, 140, 66), 38));
        updateCoverageTriangleNode(geometryLayer, geometryLayer->pskCoverageFill,
                                   m_batch.pskCoverageTriangles,
                                   styledColor(QStringLiteral("psk"), QColor(186, 124, 255), 78));
        updateCoverageTriangleNode(geometryLayer, geometryLayer->pskCoverageMediumFill,
                                   m_batch.pskCoverageMediumTriangles,
                                   styledColor(QStringLiteral("psk"), QColor(186, 124, 255), 52));
        updateCoverageTriangleNode(geometryLayer, geometryLayer->pskCoverageFadedFill,
                                   m_batch.pskCoverageFadedTriangles,
                                   styledColor(QStringLiteral("psk"), QColor(186, 124, 255), 32));
        updateFlatLineNode(geometryLayer, geometryLayer->workedCoverageLines,
                           m_batch.workedCoverageLines,
                           styledColor(QStringLiteral("worked"), QColor(0, 216, 255), 150),
                           layerThickness(QStringLiteral("worked")));
        updateFlatLineNode(geometryLayer, geometryLayer->confirmedCoverageLines,
                           m_batch.confirmedCoverageLines,
                           styledColor(QStringLiteral("confirmed"), QColor(84, 255, 145), 205),
                           layerThickness(QStringLiteral("confirmed")));
        updateFlatLineNode(geometryLayer, geometryLayer->activeCoverageLines,
                           m_batch.activeCoverageLines,
                           styledColor(QStringLiteral("active"), QColor(246, 195, 68), 190),
                           layerThickness(QStringLiteral("active")));
        updateFlatLineNode(geometryLayer, geometryLayer->activeCoverageMediumLines,
                           m_batch.activeCoverageMediumLines,
                           styledColor(QStringLiteral("active"), QColor(246, 195, 68), 120),
                           layerThickness(QStringLiteral("active")));
        updateFlatLineNode(geometryLayer, geometryLayer->activeCoverageFadedLines,
                           m_batch.activeCoverageFadedLines,
                           styledColor(QStringLiteral("active"), QColor(246, 195, 68), 72),
                           layerThickness(QStringLiteral("active")));
        updateFlatLineNode(geometryLayer, geometryLayer->missingCoverageLines,
                           m_batch.missingCoverageLines,
                           styledColor(QStringLiteral("missing"), QColor(255, 140, 66), 215),
                           layerThickness(QStringLiteral("missing")));
        updateFlatLineNode(geometryLayer, geometryLayer->missingCoverageMediumLines,
                           m_batch.missingCoverageMediumLines,
                           styledColor(QStringLiteral("missing"), QColor(255, 140, 66), 136),
                           layerThickness(QStringLiteral("missing")));
        updateFlatLineNode(geometryLayer, geometryLayer->missingCoverageFadedLines,
                           m_batch.missingCoverageFadedLines,
                           styledColor(QStringLiteral("missing"), QColor(255, 140, 66), 82),
                           layerThickness(QStringLiteral("missing")));
        updateFlatLineNode(geometryLayer, geometryLayer->pskCoverageLines,
                           m_batch.pskCoverageLines,
                           styledColor(QStringLiteral("psk"), QColor(186, 124, 255), 205),
                           layerThickness(QStringLiteral("psk")));
        updateFlatLineNode(geometryLayer, geometryLayer->pskCoverageMediumLines,
                           m_batch.pskCoverageMediumLines,
                           styledColor(QStringLiteral("psk"), QColor(186, 124, 255), 130),
                           layerThickness(QStringLiteral("psk")));
        updateFlatLineNode(geometryLayer, geometryLayer->pskCoverageFadedLines,
                           m_batch.pskCoverageFadedLines,
                           styledColor(QStringLiteral("psk"), QColor(186, 124, 255), 78),
                           layerThickness(QStringLiteral("psk")));
        updateFlatLineNode(geometryLayer, geometryLayer->gridLines, m_batch.gridLines,
                           styledColor(QStringLiteral("live"), QColor(170, 210, 225), 42),
                           layerThickness(QStringLiteral("live")));
        updateFlatLineNode(geometryLayer, geometryLayer->timeZoneLines,
                           m_batch.timeZoneLines,
                           styledColor(QStringLiteral("propagation"), QColor(112, 223, 255), 104),
                           layerThickness(QStringLiteral("propagation")));
        updateCoverageTriangleNode(geometryLayer, geometryLayer->countyBoundaryFill,
                                   m_batch.countyBoundaryTriangles,
                                   styledColor(QStringLiteral("counties"), QColor(124, 190, 228), 148));
        updateCoverageTriangleNode(geometryLayer, geometryLayer->stateBoundaryFill,
                                   m_batch.stateBoundaryTriangles,
                                   styledColor(QStringLiteral("states"), QColor(112, 235, 255), 238));
        updateFlatLineNode(geometryLayer, geometryLayer->countyBoundaryLines,
                           m_batch.countyBoundaryLines,
                           styledColor(QStringLiteral("counties"), QColor(150, 205, 232), 225),
                           layerThickness(QStringLiteral("counties")));
        updateFlatLineNode(geometryLayer, geometryLayer->stateBoundaryLines,
                           m_batch.stateBoundaryLines,
                           styledColor(QStringLiteral("states"), QColor(132, 245, 255), 255),
                           layerThickness(QStringLiteral("states")));
        updateScreenCircleNode(geometryLayer, geometryLayer->potaHaloMarkers,
                               m_batch.potaMarkers,
                               styledColor(QStringLiteral("pota"), QColor(72, 191, 92), 105),
                               static_cast<float>(7.0 * layerThickness(QStringLiteral("pota"))));
        updateScreenCircleNode(geometryLayer, geometryLayer->iotaHaloMarkers,
                               m_batch.iotaMarkers,
                               styledColor(QStringLiteral("iota"), QColor(46, 190, 226), 105),
                               static_cast<float>(7.0 * layerThickness(QStringLiteral("iota"))));
        updateScreenCircleNode(geometryLayer, geometryLayer->wpxHaloMarkers,
                               m_batch.wpxMarkers,
                               styledColor(QStringLiteral("wpx"), QColor(242, 178, 61), 105),
                               static_cast<float>(7.0 * layerThickness(QStringLiteral("wpx"))));
        updateScreenCircleNode(geometryLayer, geometryLayer->moonHaloMarkers,
                               m_batch.moonMarkers,
                               styledColor(QStringLiteral("moon"), QColor(196, 224, 255), 145),
                               static_cast<float>(10.0 * layerThickness(QStringLiteral("moon"))));
        updateScreenSatelliteNode(geometryLayer, geometryLayer->satelliteHaloMarkers,
                                  m_batch.satelliteMarkers,
                                  styledColor(QStringLiteral("wpx"), QColor(242, 178, 61), 120),
                                  1.22f);
        updateScreenCircleNode(geometryLayer, geometryLayer->potaCoreMarkers,
                               m_batch.potaMarkers,
                               styledColor(QStringLiteral("pota"), QColor(166, 255, 154), 240),
                               static_cast<float>(3.7 * layerThickness(QStringLiteral("pota"))));
        updateScreenCircleNode(geometryLayer, geometryLayer->iotaCoreMarkers,
                               m_batch.iotaMarkers,
                               styledColor(QStringLiteral("iota"), QColor(130, 236, 255), 240),
                               static_cast<float>(3.7 * layerThickness(QStringLiteral("iota"))));
        updateScreenCircleNode(geometryLayer, geometryLayer->wpxCoreMarkers,
                               m_batch.wpxMarkers,
                               styledColor(QStringLiteral("wpx"), QColor(255, 215, 145), 240),
                               static_cast<float>(3.7 * layerThickness(QStringLiteral("wpx"))));
        updateScreenCircleNode(geometryLayer, geometryLayer->moonCoreMarkers,
                               m_batch.moonMarkers,
                               styledColor(QStringLiteral("moon"), QColor(245, 249, 255), 250),
                               static_cast<float>(4.5 * layerThickness(QStringLiteral("moon"))));
        updateScreenSatelliteNode(geometryLayer, geometryLayer->satelliteCoreMarkers,
                                  m_batch.satelliteMarkers,
                                  styledColor(QStringLiteral("wpx"), QColor(255, 215, 145), 245),
                                  1.0f);
        if (azimuthalProjectionEnabled()) {
            auto screenPaths = [this, &rect](const QVector<PathLine>& paths) {
                QVector<QPointF> lines;
                for (const PathLine& path : paths) {
                    QVector<QPointF> const arc = greatCircle(path.sourceLonLat,
                                                             path.destinationLonLat,
                                                             kGreatCircleSteps);
                    QPointF previous;
                    bool havePrevious = false;
                    for (const QPointF& lonLat : arc) {
                        QPointF const current = projectLonLatToPoint(lonLat);
                        bool const visible = rect.adjusted(-3.0, -3.0, 3.0, 3.0)
                            .contains(current);
                        if (visible && havePrevious
                            && QLineF(previous, current).length() < rect.width() * 0.55) {
                            lines << previous << current;
                        }
                        previous = current;
                        havePrevious = visible;
                    }
                }
                return lines;
            };
            auto screenMarkers = [this, &rect](const QVector<QPointF>& lonLatMarkers) {
                QVector<QPointF> markers;
                markers.reserve(lonLatMarkers.size());
                for (const QPointF& lonLat : lonLatMarkers) {
                    QPointF const marker = projectLonLatToPoint(lonLat);
                    if (rect.adjusted(-8.0, -8.0, 8.0, 8.0).contains(marker)) {
                        markers.push_back(marker);
                    }
                }
                return markers;
            };
            QVector<QPointF> const genericPaths = screenPaths(m_batch.genericPaths);
            QVector<QPointF> const incomingPaths = screenPaths(m_batch.incomingPaths);
            QVector<QPointF> const outgoingPaths = screenPaths(m_batch.outgoingPaths);
            QVector<QPointF> const genericMarkers = screenMarkers(m_batch.genericMarkers);
            QVector<QPointF> const incomingMarkers = screenMarkers(m_batch.incomingMarkers);
            QVector<QPointF> const outgoingMarkers = screenMarkers(m_batch.outgoingMarkers);
            QVector<QPointF> const bandMarkers = screenMarkers(m_batch.bandMarkers);
            updateFlatLineNode(geometryLayer, geometryLayer->genericPaths, genericPaths,
                               styledColor(QStringLiteral("live"), colorForRole(PathRole::Generic)),
                               layerThickness(QStringLiteral("live")));
            updateFlatLineNode(geometryLayer, geometryLayer->incomingPaths, incomingPaths,
                               styledColor(QStringLiteral("live"), colorForRole(PathRole::IncomingToMe)),
                               layerThickness(QStringLiteral("live")));
            updateFlatLineNode(geometryLayer, geometryLayer->outgoingPaths, outgoingPaths,
                               styledColor(QStringLiteral("live"), colorForRole(PathRole::OutgoingFromMe)),
                               layerThickness(QStringLiteral("live")));
            updateScreenCircleNode(geometryLayer, geometryLayer->genericHaloMarkers,
                                   genericMarkers, withAlpha(colorForRole(PathRole::Generic), 90), 6.4f);
            updateScreenCircleNode(geometryLayer, geometryLayer->incomingHaloMarkers,
                                   incomingMarkers, withAlpha(colorForRole(PathRole::IncomingToMe), 92), 6.4f);
            updateScreenCircleNode(geometryLayer, geometryLayer->outgoingHaloMarkers,
                                   outgoingMarkers, withAlpha(colorForRole(PathRole::OutgoingFromMe), 92), 6.4f);
            updateScreenCircleNode(geometryLayer, geometryLayer->bandHaloMarkers,
                                   bandMarkers, withAlpha(colorForRole(PathRole::BandOnly), 95), 6.2f);
            updateScreenCircleNode(geometryLayer, geometryLayer->genericCoreMarkers,
                                   genericMarkers, colorForRole(PathRole::Generic), 3.5f);
            updateScreenCircleNode(geometryLayer, geometryLayer->incomingCoreMarkers,
                                   incomingMarkers, colorForRole(PathRole::IncomingToMe), 3.5f);
            updateScreenCircleNode(geometryLayer, geometryLayer->outgoingCoreMarkers,
                                   outgoingMarkers, colorForRole(PathRole::OutgoingFromMe), 3.5f);
            updateScreenCircleNode(geometryLayer, geometryLayer->bandCoreMarkers,
                                   bandMarkers, colorForRole(PathRole::BandOnly), 3.4f);
        } else {
            updatePathNode(geometryLayer, geometryLayer->genericPaths, m_batch.genericPaths, colorForRole(PathRole::Generic),
                           m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect, uploadContactGeometry);
            updatePathNode(geometryLayer, geometryLayer->incomingPaths, m_batch.incomingPaths, colorForRole(PathRole::IncomingToMe),
                           m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect, uploadContactGeometry);
            updatePathNode(geometryLayer, geometryLayer->outgoingPaths, m_batch.outgoingPaths, colorForRole(PathRole::OutgoingFromMe),
                           m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect, uploadContactGeometry);

            updateGeoMarkerNode(geometryLayer, geometryLayer->genericHaloMarkers,
                                m_batch.genericMarkers, withAlpha(colorForRole(PathRole::Generic), 90), 6.4f,
                                m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect, uploadContactGeometry);
            updateGeoMarkerNode(geometryLayer, geometryLayer->incomingHaloMarkers,
                                m_batch.incomingMarkers, withAlpha(colorForRole(PathRole::IncomingToMe), 92), 6.4f,
                                m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect, uploadContactGeometry);
            updateGeoMarkerNode(geometryLayer, geometryLayer->outgoingHaloMarkers,
                                m_batch.outgoingMarkers, withAlpha(colorForRole(PathRole::OutgoingFromMe), 92), 6.4f,
                                m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect, uploadContactGeometry);
            updateGeoMarkerNode(geometryLayer, geometryLayer->bandHaloMarkers,
                                m_batch.bandMarkers, withAlpha(colorForRole(PathRole::BandOnly), 95), 6.2f,
                                m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect, uploadContactGeometry);

            updateGeoMarkerNode(geometryLayer, geometryLayer->genericCoreMarkers,
                                m_batch.genericMarkers, colorForRole(PathRole::Generic), 3.5f,
                                m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect, uploadContactGeometry);
            updateGeoMarkerNode(geometryLayer, geometryLayer->incomingCoreMarkers,
                                m_batch.incomingMarkers, colorForRole(PathRole::IncomingToMe), 3.5f,
                                m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect, uploadContactGeometry);
            updateGeoMarkerNode(geometryLayer, geometryLayer->outgoingCoreMarkers,
                                m_batch.outgoingMarkers, colorForRole(PathRole::OutgoingFromMe), 3.5f,
                                m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect, uploadContactGeometry);
            updateGeoMarkerNode(geometryLayer, geometryLayer->bandCoreMarkers,
                                m_batch.bandMarkers, colorForRole(PathRole::BandOnly), 3.4f,
                                m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect, uploadContactGeometry);
        }

        if (uploadContactGeometry) {
            m_contactGeometryDirty = false;
        }
    }

    // Earthquakes stay vector features: their pulse is cheap geometry updated
    // by the existing map animation timer, while the source feed is parsed off
    // the UI thread by MapExternalOverlayService.
    qreal const quakePulse = 8.0 + m_animationPhase * 11.0;
    qreal const quakePulseAlpha = 86.0 * (1.0 - m_animationPhase);
    updateScreenCircleNode(geometryLayer, geometryLayer->earthquakeLowPulseMarkers,
                           m_batch.earthquakeLowMarkers,
                           QColor(255, 219, 91, qRound(quakePulseAlpha)),
                           static_cast<float>(quakePulse));
    updateScreenCircleNode(geometryLayer, geometryLayer->earthquakeMediumPulseMarkers,
                           m_batch.earthquakeMediumMarkers,
                           QColor(255, 160, 54, qRound(quakePulseAlpha)),
                           static_cast<float>(quakePulse + 1.8));
    updateScreenCircleNode(geometryLayer, geometryLayer->earthquakeHighPulseMarkers,
                           m_batch.earthquakeHighMarkers,
                           QColor(255, 89, 94, qRound(quakePulseAlpha + 18.0)),
                           static_cast<float>(quakePulse + 3.0));
    updateScreenCircleNode(geometryLayer, geometryLayer->earthquakeLowCoreMarkers,
                           m_batch.earthquakeLowMarkers, QColor(255, 219, 91, 232), 3.8f);
    updateScreenCircleNode(geometryLayer, geometryLayer->earthquakeMediumCoreMarkers,
                           m_batch.earthquakeMediumMarkers, QColor(255, 160, 54, 238), 4.5f);
    updateScreenCircleNode(geometryLayer, geometryLayer->earthquakeHighCoreMarkers,
                           m_batch.earthquakeHighMarkers, QColor(255, 89, 94, 244), 5.2f);

    qreal txProgress = m_animationPhase;
    if (m_transmitting && m_txStartMs > 0 && m_txTravelMs > 0) {
        qint64 const elapsedMs = qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - m_txStartMs);
        txProgress = std::fmod(static_cast<qreal>(elapsedMs) / static_cast<qreal>(m_txTravelMs), 1.0);
    }

    if (azimuthalProjectionEnabled()) {
        QVector<QPointF> genericArrows;
        QVector<QPointF> incomingArrows;
        QVector<QPointF> outgoingArrows;
        for (const AnimatedPath& path : std::as_const(m_animatedPaths)) {
            qreal progress = std::fmod(m_animationPhase + (qHash(path.key) % 17) * 0.057, 1.0);
            if (m_transmitting && path.role == PathRole::OutgoingFromMe
                && (m_txTargetCall.isEmpty() || sameStationCall(path.key, m_txTargetCall))) {
                progress = txProgress;
            }

            QVector<QPointF> projectedPath;
            projectedPath.reserve(path.points.size());
            for (const QPointF& lonLat : path.points) {
                projectedPath.push_back(projectLonLatToPoint(lonLat));
            }

            QPolygonF arrow;
            if (!arrowOnPath(projectedPath, progress, &arrow)) {
                continue;
            }
            QVector<QPointF>* target = &genericArrows;
            if (path.role == PathRole::IncomingToMe) {
                target = &incomingArrows;
            } else if (path.role == PathRole::OutgoingFromMe) {
                target = &outgoingArrows;
            }
            for (const QPointF& point : arrow) {
                target->push_back(point);
            }
        }
        updateTriangleNode(animationLayer, animationLayer->genericArrows,
                           genericArrows, QColor(255, 244, 196, 230));
        updateTriangleNode(animationLayer, animationLayer->incomingArrows,
                           incomingArrows, QColor(255, 195, 140, 240));
        updateTriangleNode(animationLayer, animationLayer->outgoingArrows,
                           outgoingArrows, QColor(255, 233, 132, 240));
        animationLayer->genericArrowVertices = genericArrows.size();
        animationLayer->incomingArrowVertices = incomingArrows.size();
        animationLayer->outgoingArrowVertices = outgoingArrows.size();
        m_animationGeometryDirty = false;
    } else {
#ifdef DECODIUM_LIVEMAP_ARROW_QSB
    QVector<ArrowPathRequest> arrowPaths;
    arrowPaths.reserve(m_animatedPaths.size());
    for (const AnimatedPath& path : std::as_const(m_animatedPaths)) {
        arrowPaths.push_back({path.key, path.sourceLonLat, path.destinationLonLat, path.role});
    }

    bool const uploadAnimationGeometry = m_animationGeometryDirty || uploadContactGeometry;
    updateArrowNode(animationLayer, animationLayer->genericArrows, animationLayer->genericArrowVertices,
                    arrowPaths, PathRole::Generic, QColor(255, 244, 196, 230),
                    m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect,
                    m_animationPhase, txProgress, m_transmitting, m_txTargetCall, uploadAnimationGeometry);
    updateArrowNode(animationLayer, animationLayer->incomingArrows, animationLayer->incomingArrowVertices,
                    arrowPaths, PathRole::IncomingToMe, QColor(255, 195, 140, 240),
                    m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect,
                    m_animationPhase, txProgress, m_transmitting, m_txTargetCall, uploadAnimationGeometry);
    updateArrowNode(animationLayer, animationLayer->outgoingArrows, animationLayer->outgoingArrowVertices,
                    arrowPaths, PathRole::OutgoingFromMe, QColor(255, 233, 132, 240),
                    m_viewCenterLon, m_viewCenterLat, m_viewSpanLon, m_viewSpanLat, rect,
                    m_animationPhase, txProgress, m_transmitting, m_txTargetCall, uploadAnimationGeometry);
    if (uploadAnimationGeometry) {
        m_animationGeometryDirty = false;
    }
#else
    QVector<QPointF> genericArrows;
    QVector<QPointF> incomingArrows;
    QVector<QPointF> outgoingArrows;
    for (const AnimatedPath& path : std::as_const(m_animatedPaths)) {
        qreal progress = std::fmod(m_animationPhase + (qHash(path.key) % 17) * 0.057, 1.0);
        if (m_transmitting && path.role == PathRole::OutgoingFromMe) {
            bool const callMatch = m_txTargetCall.isEmpty() || sameStationCall(path.key, m_txTargetCall);
            if (callMatch) {
                progress = txProgress;
            }
        }

        QVector<QPointF> projectedPath;
        projectedPath.reserve(path.points.size());
        for (const QPointF& lonLat : path.points) {
            projectedPath.push_back(projectLonLatToPoint(lonLat));
        }

        QPolygonF arrow;
        if (!arrowOnPath(projectedPath, progress, &arrow)) {
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
    animationLayer->genericArrowVertices = genericArrows.size();
    animationLayer->incomingArrowVertices = incomingArrows.size();
    animationLayer->outgoingArrowVertices = outgoingArrows.size();
    m_animationGeometryDirty = false;
#endif
    }

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
        updateScreenCircleNode(geometryLayer, geometryLayer->legendBandMarker,
                               QVector<QPointF> {QPointF(x + 8.0, legendY)},
                               QColor(255, 212, 96, 230), 3.4f);
        displayLabels.push_back({QStringLiteral("BAND"),
                                 QPointF(x + 21.0, legendY + 4.0),
                                 labelTextureRectForBaseline(QStringLiteral("BAND"), QPointF(x + 21.0, legendY + 4.0)),
                                 overlayTextColor});
    } else {
        updateFlatLineNode(geometryLayer, geometryLayer->legendIncomingLine, {}, QColor(255, 126, 92, 230));
        updateFlatLineNode(geometryLayer, geometryLayer->legendOutgoingLine, {}, QColor(84, 238, 165, 230));
        updateScreenCircleNode(geometryLayer, geometryLayer->legendBandMarker, {}, QColor(255, 212, 96, 230), 3.4f);
    }

    QString const bottomLeft = m_lastVisibleBandCount > 0
        ? QStringLiteral("%1 active paths | %2 in band").arg(m_lastVisiblePathCount).arg(m_lastVisibleBandCount)
        : QStringLiteral("%1 active paths").arg(m_lastVisiblePathCount);
    QPointF const bottomLeftBaseline(rect.left() + 8.0, rect.bottom() - 6.0);
    displayLabels.push_back({bottomLeft,
                             bottomLeftBaseline,
                             labelTextureRectForBaseline(bottomLeft, bottomLeftBaseline),
                             QColor(225, 235, 245, 205),
                             false});

    QString const utcText = QDateTime::currentDateTimeUtc().toString(QStringLiteral("hh:mm:ss 'UTC'"));
    QRectF const utcRect = labelTextureRectForBaseline(utcText, QPointF(0.0, 0.0));
    QPointF const utcBaseline(rect.right() - 8.0 - utcRect.width() + 2.0,
                              rect.bottom() - 6.0);
    displayLabels.push_back({utcText,
                             utcBaseline,
                             labelTextureRectForBaseline(utcText, utcBaseline),
                             QColor(225, 235, 245, 205),
                             false});

    QVector<StableLabelRequest> stableLabelRequests;
    stableLabelRequests.reserve(displayLabels.size());
    for (const Label& label : std::as_const(displayLabels)) {
        if (!label.persistentCache || label.text.isEmpty()) {
            continue;
        }
        stableLabelRequests.push_back({labelTextureKey(label.text, label.color), label.text, label.color});
    }
    ensureStableLabelAtlas(labelLayer, window(), stableLabelRequests, &m_lastLabelTextureCreateUs);

    while (labelLayer->labelNodes.size() < displayLabels.size()) {
        auto* node = new QSGSimpleTextureNode;
        node->setOwnsTexture(false);
        node->setFiltering(QSGTexture::Linear);
        if (QSGTexture* blankTexture = labelBlankTexture(labelLayer, window())) {
            node->setTexture(blankTexture);
            node->setSourceRect(textureSourceRect(blankTexture));
        }
        node->setRect(QRectF());
        labelLayer->appendChildNode(node);
        labelLayer->labelNodes.push_back(node);
        labelLayer->transientTextures.push_back(nullptr);
        labelLayer->transientTextureKeys.push_back(QString());
    }
    while (labelLayer->labelNodes.size() > displayLabels.size()) {
        auto* node = labelLayer->labelNodes.takeLast();
        QSGTexture* transientTexture = labelLayer->transientTextures.takeLast();
        labelLayer->transientTextureKeys.takeLast();
        labelLayer->removeChildNode(node);
        delete node;
        delete transientTexture;
    }

    for (int i = 0; i < displayLabels.size(); ++i) {
        const Label& label = displayLabels[i];
        auto* node = labelLayer->labelNodes[i];
        QSGTexture* oldTransientTexture = labelLayer->transientTextures[i];

        if (label.persistentCache) {
            QString const key = labelTextureKey(label.text, label.color);
            QSGTexture* atlasTexture = labelLayer->stableAtlasTexture;
            QRectF const sourceRect = labelLayer->stableAtlasRects.value(key);
            if (atlasTexture && !sourceRect.isEmpty()) {
                node->setTexture(atlasTexture);
                node->setSourceRect(sourceRect);
                node->setRect(label.rect);
                labelLayer->transientTextures[i] = nullptr;
                labelLayer->transientTextureKeys[i].clear();
                delete oldTransientTexture;
            } else if (QSGTexture* blankTexture = labelBlankTexture(labelLayer, window())) {
                node->setTexture(blankTexture);
                node->setSourceRect(textureSourceRect(blankTexture));
                node->setRect(QRectF());
                labelLayer->transientTextures[i] = nullptr;
                labelLayer->transientTextureKeys[i].clear();
                delete oldTransientTexture;
            } else {
                labelLayer->transientTextures[i] = oldTransientTexture;
                node->setRect(QRectF());
            }
            continue;
        }

        QString const transientKey = labelTextureKey(label.text, label.color);
        if (oldTransientTexture && labelLayer->transientTextureKeys.value(i) == transientKey) {
            node->setTexture(oldTransientTexture);
            node->setSourceRect(textureSourceRect(oldTransientTexture));
            node->setRect(label.rect);
            continue;
        }

        QSGTexture* texture = createLabelTexture(window(), label.text, label.color, &m_lastLabelTextureCreateUs);
        QSGTexture* replacementTexture = texture ? texture : labelBlankTexture(labelLayer, window());
        if (replacementTexture) {
            node->setTexture(replacementTexture);
            node->setSourceRect(textureSourceRect(replacementTexture));
            node->setRect(texture ? label.rect : QRectF());
            labelLayer->transientTextures[i] = texture;
            labelLayer->transientTextureKeys[i] = texture ? transientKey : QString();
            delete oldTransientTexture;
        } else {
            labelLayer->transientTextures[i] = oldTransientTexture;
            node->setRect(QRectF());
        }
    }
    qDeleteAll(labelLayer->retiredAtlasTextures);
    labelLayer->retiredAtlasTextures.clear();

#ifdef DECODIUM_LIVEMAP_GREYLINE_QSB
    int const greylineShaderActive = m_greylineShaderAllowed ? 1 : 0;
#else
    int const greylineShaderActive = 0;
#endif
#ifdef DECODIUM_LIVEMAP_MARKER_QSB
    int const markerShaderActive = 1;
#else
    int const markerShaderActive = 0;
#endif
#ifdef DECODIUM_LIVEMAP_PATH_QSB
    int const pathShaderActive = 1;
#else
    int const pathShaderActive = 0;
#endif
#ifdef DECODIUM_LIVEMAP_ARROW_QSB
    int const arrowShaderActive = 1;
#else
    int const arrowShaderActive = 0;
#endif
    int const labelAtlasTextures = labelLayer->stableAtlasTexture ? 1 : 0;
    qint64 const totalSyncUs = mapSyncTimer.nsecsElapsed() / 1000;
    m_lastMapSyncNodesUs = qMax<qint64>(0, totalSyncUs - m_lastMapRebuildUs - m_lastLabelTextureCreateUs);

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
            << " stateBoundaryVertices=" << m_lastStateBoundaryVertexCount
            << " countyBoundaryVertices=" << m_lastCountyBoundaryVertexCount
            << " stateBoundaryLineVertices=" << m_lastStateBoundaryLineVertexCount
            << " countyBoundaryLineVertices=" << m_lastCountyBoundaryLineVertexCount
            << " markerVertices=" << m_lastMarkerVertexCount
            << " labels=" << m_lastLabelCount
            << " visiblePaths=" << m_lastVisiblePathCount
            << " visibleBand=" << m_lastVisibleBandCount
            << " labelAtlasTextures=" << labelAtlasTextures
            << " labelAtlasEntries=" << labelLayer->stableAtlasRects.size()
            << " labelImageCache=" << labelLayer->stableImageCache.size()
            << " markerShader=" << markerShaderActive
            << " pathShader=" << pathShaderActive
            << " arrowShader=" << arrowShaderActive
            << " map_rebuild_us=" << m_lastMapRebuildUs
            << " map_label_layout_us=" << m_lastLabelLayoutUs
            << " map_texture_create_us=" << m_lastLabelTextureCreateUs
            << " map_sync_nodes_us=" << m_lastMapSyncNodesUs;
        qInfo().nospace()
            << "[MAPGPU] LiveMap geometry optimization markerGpuProjection="
            << markerShaderActive
            << " pathVboReuse="
            << pathShaderActive
            << " arrowShader=" << arrowShaderActive
            << " labelAtlasStable=1";
    }

    qint64 const nowMs = monotonicNowMs();
    if (!m_loggedFirstProfile) {
        m_loggedFirstProfile = true;
        m_lastProfileLogMs = nowMs;
    }
    if (nowMs - m_lastProfileLogMs >= kGpuProfileLogMs) {
        m_lastProfileLogMs = nowMs;
        qInfo().noquote().nospace()
            << "[MAPGPU] LiveMap QSG profile contacts=" << m_lastContactCount
            << " lineVertices=" << m_lastLineVertexCount
            << " stateBoundaryVertices=" << m_lastStateBoundaryVertexCount
            << " countyBoundaryVertices=" << m_lastCountyBoundaryVertexCount
            << " stateBoundaryLineVertices=" << m_lastStateBoundaryLineVertexCount
            << " countyBoundaryLineVertices=" << m_lastCountyBoundaryLineVertexCount
            << " markerVertices=" << m_lastMarkerVertexCount
            << " labels=" << m_lastLabelCount
            << " visiblePaths=" << m_lastVisiblePathCount
            << " visibleBand=" << m_lastVisibleBandCount
            << " animatedPaths=" << m_animatedPaths.size()
            << " viewCenter=" << QString::number(m_viewCenterLon, 'f', 2)
            << "," << QString::number(m_viewCenterLat, 'f', 2)
            << " viewSpan=" << QString::number(m_viewSpanLon, 'f', 2)
            << "," << QString::number(m_viewSpanLat, 'f', 2)
            << " labelAtlasTextures=" << labelAtlasTextures
            << " labelAtlasEntries=" << labelLayer->stableAtlasRects.size()
            << " labelImageCache=" << labelLayer->stableImageCache.size()
            << " arrowVertices=" << (animationLayer->genericArrowVertices
                                     + animationLayer->incomingArrowVertices
                                     + animationLayer->outgoingArrowVertices)
            << " conservative=" << (m_conservativeRenderer ? 1 : 0)
            << " frameMs=" << m_frameIntervalMs
            << " markerShader=" << markerShaderActive
            << " pathShader=" << pathShaderActive
            << " arrowShader=" << arrowShaderActive
            << " greylineShader="
            << greylineShaderActive
            << " markerGpuProjection="
            << markerShaderActive
            << " pathVboReuse="
            << pathShaderActive
            << " labelAtlasStable=1"
            << " map_rebuild_us=" << m_lastMapRebuildUs
            << " map_label_layout_us=" << m_lastLabelLayoutUs
            << " map_texture_create_us=" << m_lastLabelTextureCreateUs
            << " map_sync_nodes_us=" << m_lastMapSyncNodesUs;
    }

    return root;
}

void WorldMapGpuItem::mousePressEvent(QMouseEvent* event)
{
    if (!event) {
        return;
    }
    if (event->button() == Qt::RightButton) {
        QVariantMap const cell = coverageCellAt(event->position());
        if (!cell.isEmpty()) {
            event->accept();
            Q_EMIT coverageCellClicked(cell, event->position().x(),
                                       event->position().y());
            return;
        }
    }
    if (event->button() != Qt::LeftButton) {
        QQuickItem::mousePressEvent(event);
        return;
    }

    QPointF const pos = event->position();
    QVariantMap const operationalMarker = operationalMarkerAt(pos);
    if (!operationalMarker.isEmpty()) {
        event->accept();
        Q_EMIT operationalMarkerClicked(
            operationalMarker, pos.x(), pos.y());
        return;
    }
    QVariantMap const geographicFeature = geographicFeatureAt(pos);
    if (!geographicFeature.isEmpty()) {
        event->accept();
        Q_EMIT geographicFeatureClicked(
            geographicFeature, pos.x(), pos.y());
        return;
    }
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

    // 1.0.221 — Left click su area vuota -> attiva pan drag.
    m_panActive = true;
    m_panLastPos = pos;
    setCursor(Qt::ClosedHandCursor);
    event->accept();
}

void WorldMapGpuItem::hoverMoveEvent(QHoverEvent* event)
{
    if (!event) {
        return;
    }
    QVariantMap const geographicFeature = geographicFeatureAt(event->position());
    if (!geographicFeature.isEmpty()) {
        if (!m_hoveredCoverageGrid.isEmpty()) {
            m_hoveredCoverageGrid.clear();
            Q_EMIT coverageCellHoverEnded();
        }
        if (m_hoveredGeographicFeature != geographicFeature) {
            m_hoveredGeographicFeature = geographicFeature;
            Q_EMIT geographicFeatureHovered(
                geographicFeature, event->position().x(), event->position().y());
        }
        if (!m_panActive) {
            setCursor(Qt::PointingHandCursor);
        }
        return;
    }
    if (!m_hoveredGeographicFeature.isEmpty()) {
        m_hoveredGeographicFeature.clear();
        Q_EMIT geographicFeatureHoverEnded();
    }
    QVariantMap const cell = coverageCellAt(event->position());
    QString const grid = cell.value(QStringLiteral("grid")).toString();
    if (grid.isEmpty()) {
        if (!m_hoveredCoverageGrid.isEmpty()) {
            m_hoveredCoverageGrid.clear();
            Q_EMIT coverageCellHoverEnded();
        }
        if (!m_panActive) {
            setCursor(Qt::ArrowCursor);
        }
        return;
    }
    m_hoveredCoverageGrid = grid;
    if (!m_panActive) {
        setCursor(Qt::PointingHandCursor);
    }
    Q_EMIT coverageCellHovered(cell, event->position().x(),
                               event->position().y());
    Q_EMIT coverageCellSegmentHovered(
        cell, event->position().x(), event->position().y(),
        cell.value(QStringLiteral("splitSegment"),
                   QStringLiteral("Combined")).toString());
}

void WorldMapGpuItem::hoverLeaveEvent(QHoverEvent* event)
{
    Q_UNUSED(event);
    if (!m_hoveredCoverageGrid.isEmpty()) {
        m_hoveredCoverageGrid.clear();
        Q_EMIT coverageCellHoverEnded();
    }
    if (!m_hoveredGeographicFeature.isEmpty()) {
        m_hoveredGeographicFeature.clear();
        Q_EMIT geographicFeatureHoverEnded();
    }
    if (!m_panActive) {
        setCursor(Qt::ArrowCursor);
    }
}

void WorldMapGpuItem::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_panActive || !event) {
        QQuickItem::mouseMoveEvent(event);
        return;
    }
    QPointF const pos = event->position();
    QPointF const delta = pos - m_panLastPos;
    m_panLastPos = pos;

    QRectF const rect = mapRect();
    if (rect.width() <= 0.0 || rect.height() <= 0.0) {
        event->accept();
        return;
    }

    // Converti delta pixel in delta lon/lat in funzione dello span corrente.
    double const dLon = -static_cast<double>(delta.x()) / rect.width() * m_viewSpanLon;
    double const dLat = static_cast<double>(delta.y()) / rect.height() * m_viewSpanLat;

    if (!m_userViewportLocked) {
        m_userViewportLocked = true;
        Q_EMIT viewportLockedChanged(true);
    }
    m_targetCenterLon = wrapLongitude(m_targetCenterLon + dLon);
    double const newLat = m_targetCenterLat + dLat;
    m_targetCenterLat = qBound(-90.0 + 0.5 * m_targetSpanLat,
                                newLat,
                                90.0 - 0.5 * m_targetSpanLat);
    markDirty(false);
    event->accept();
}

void WorldMapGpuItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_panActive) {
        m_panActive = false;
        setCursor(Qt::ArrowCursor);
    }
    QQuickItem::mouseReleaseEvent(event);
}

void WorldMapGpuItem::wheelEvent(QWheelEvent* event)
{
    if (!event) {
        QQuickItem::wheelEvent(event);
        return;
    }
    // Standard: 120 unit / notch. Positive y = zoom in.
    int const delta = event->angleDelta().y();
    if (delta == 0) {
        QQuickItem::wheelEvent(event);
        return;
    }
    double const stepFactor = (delta > 0) ? 1.18 : 1.0 / 1.18;
    if (delta > 0) {
        zoomIn(stepFactor);
    } else {
        zoomOut(1.0 / stepFactor);
    }
    event->accept();
}

void WorldMapGpuItem::zoomIn(double factor)
{
    if (factor <= 1.0) factor = 1.5;
    if (!m_userViewportLocked) {
        m_userViewportLocked = true;
        Q_EMIT viewportLockedChanged(true);
    }
    // Range minimo span: 12° (city-level zoom). Sotto e' troppo per RX FT8.
    constexpr double kMinSpanLon = 12.0;
    constexpr double kMinSpanLat = 8.0;
    m_targetSpanLon = qMax(kMinSpanLon, m_targetSpanLon / factor);
    m_targetSpanLat = qMax(kMinSpanLat, m_targetSpanLat / factor);
    markDirty(false);
}

void WorldMapGpuItem::zoomOut(double factor)
{
    if (factor <= 1.0) factor = 1.5;
    if (!m_userViewportLocked) {
        m_userViewportLocked = true;
        Q_EMIT viewportLockedChanged(true);
    }
    constexpr double kMaxSpanLon = 360.0;
    constexpr double kMaxSpanLat = 180.0;
    m_targetSpanLon = qMin(kMaxSpanLon, m_targetSpanLon * factor);
    m_targetSpanLat = qMin(kMaxSpanLat, m_targetSpanLat * factor);
    // Anche il center va bound (no clipping verticale ai poli oltre lo span).
    m_targetCenterLat = qBound(-90.0 + 0.5 * m_targetSpanLat,
                                m_targetCenterLat,
                                90.0 - 0.5 * m_targetSpanLat);
    markDirty(false);
}

void WorldMapGpuItem::resetView()
{
    if (m_userViewportLocked) {
        m_userViewportLocked = false;
        Q_EMIT viewportLockedChanged(false);
    }
    // Forza un updateViewportTargets immediato per ricalcolare auto-fit.
    updateViewportTargets();
    markDirty(false);
}

void WorldMapGpuItem::panBy(double deltaLonDeg, double deltaLatDeg)
{
    if (!m_userViewportLocked) {
        m_userViewportLocked = true;
        Q_EMIT viewportLockedChanged(true);
    }
    m_targetCenterLon = wrapLongitude(m_targetCenterLon + deltaLonDeg);
    m_targetCenterLat = qBound(-90.0 + 0.5 * m_targetSpanLat,
                                m_targetCenterLat + deltaLatDeg,
                                90.0 - 0.5 * m_targetSpanLat);
    markDirty(false);
}

void WorldMapGpuItem::focusLocation(double longitude, double latitude,
                                    double spanLongitude, double spanLatitude)
{
    if (!std::isfinite(longitude) || !std::isfinite(latitude)) {
        return;
    }
    if (!m_userViewportLocked) {
        m_userViewportLocked = true;
        Q_EMIT viewportLockedChanged(true);
    }
    m_targetSpanLon = qBound(12.0, spanLongitude, 360.0);
    m_targetSpanLat = qBound(8.0, spanLatitude, 180.0);
    m_targetCenterLon = wrapLongitude(longitude);
    m_targetCenterLat = qBound(-90.0 + 0.5 * m_targetSpanLat,
                               latitude,
                               90.0 - 0.5 * m_targetSpanLat);
    markDirty(false);
}

void WorldMapGpuItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        m_greylineGeometryDirty = true;
        markDirty(false);
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

bool WorldMapGpuItem::azimuthalProjectionEnabled() const
{
    return m_projection == QStringLiteral("Azimuthal Equidistant");
}

QImage WorldMapGpuItem::azimuthalProjectionImage(const QImage& image) const
{
    if (image.isNull()) {
        return {};
    }

    QImage const source = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    int const outputWidth = qBound(640, source.width(), 1280);
    int const outputHeight = qMax(320, outputWidth / 2);
    QImage projected(outputWidth, outputHeight, QImage::Format_ARGB32_Premultiplied);
    projected.fill(Qt::transparent);

    QPointF const origin = m_hasHome ? m_homeLonLat : QPointF();
    double const lon0 = qDegreesToRadians(origin.x());
    double const lat0 = qDegreesToRadians(origin.y());
    double const sinLat0 = std::sin(lat0);
    double const cosLat0 = std::cos(lat0);

    for (int y = 0; y < outputHeight; ++y) {
        QRgb* destination = reinterpret_cast<QRgb*>(projected.scanLine(y));
        double const normalizedY = 1.0
            - (2.0 * (static_cast<double>(y) + 0.5) / outputHeight);
        for (int x = 0; x < outputWidth; ++x) {
            double const normalizedX = (2.0 * (static_cast<double>(x) + 0.5)
                                        / outputWidth) - 1.0;
            double const radius = std::hypot(normalizedX, normalizedY);
            if (radius > 1.0) {
                continue;
            }

            double const c = M_PI * radius;
            double latitude = lat0;
            double longitude = lon0;
            if (radius > 1.0e-9) {
                double const sinC = std::sin(c);
                double const cosC = std::cos(c);
                latitude = std::asin(qBound(-1.0,
                    cosC * sinLat0 + (normalizedY * sinC * cosLat0 / radius),
                    1.0));
                longitude = lon0 + std::atan2(normalizedX * sinC,
                    radius * cosLat0 * cosC
                    - normalizedY * sinLat0 * sinC);
            }

            double const lonDegrees = wrapLongitude(qRadiansToDegrees(longitude));
            double const latDegrees = qBound(-90.0, qRadiansToDegrees(latitude), 90.0);
            int const sourceX = qBound(0, qRound((lonDegrees + 180.0) / 360.0
                                                   * (source.width() - 1)),
                                       source.width() - 1);
            int const sourceY = qBound(0, qRound((90.0 - latDegrees) / 180.0
                                                   * (source.height() - 1)),
                                       source.height() - 1);
            destination[x] = reinterpret_cast<const QRgb*>(source.constScanLine(sourceY))[sourceX];
        }
    }
    return projected;
}

const QImage& WorldMapGpuItem::displayedMapImage()
{
    if (!azimuthalProjectionEnabled()) {
        return m_mapImage;
    }
    if (m_azimuthalMapDirty) {
        m_azimuthalMapImage = azimuthalProjectionImage(m_mapImage);
        m_azimuthalMapDirty = false;
    }
    return m_azimuthalMapImage;
}

const QImage& WorldMapGpuItem::displayedExternalOverlayImage()
{
    if (!azimuthalProjectionEnabled()) {
        return m_externalOverlayImage;
    }
    if (m_azimuthalExternalOverlayDirty) {
        m_azimuthalExternalOverlayImage =
            azimuthalProjectionImage(m_externalOverlayImage);
        m_azimuthalExternalOverlayDirty = false;
    }
    return m_azimuthalExternalOverlayImage;
}

double WorldMapGpuItem::projectLatitude(double latitude) const
{
    double const bounded = qBound(-90.0, latitude, 90.0);
    if (m_projection == QStringLiteral("Mercator")) {
        double const clamped = qBound(-85.05112878, bounded, 85.05112878);
        double const radians = qDegreesToRadians(clamped);
        double const scale = std::log(std::tan(M_PI_4
            + qDegreesToRadians(85.05112878) * 0.5));
        return 90.0 * std::log(std::tan(M_PI_4 + radians * 0.5)) / scale;
    }
    if (m_projection == QStringLiteral("Miller")) {
        double const radians = qDegreesToRadians(bounded);
        double const scale = 1.25 * std::log(
            std::tan(M_PI_4 + 0.4 * M_PI_2));
        return 90.0 * 1.25
            * std::log(std::tan(M_PI_4 + 0.4 * radians)) / scale;
    }
    return bounded;
}

QPointF WorldMapGpuItem::projectLonLatToPoint(const QPointF& lonLat) const
{
    QRectF const bounds = mapRect();
    if (azimuthalProjectionEnabled()) {
        QPointF const origin = m_hasHome ? m_homeLonLat : QPointF();
        double const lon0 = qDegreesToRadians(origin.x());
        double const lat0 = qDegreesToRadians(origin.y());
        double const lon = qDegreesToRadians(lonLat.x());
        double const lat = qDegreesToRadians(lonLat.y());
        double const deltaLon = lon - lon0;
        double const sinLat0 = std::sin(lat0);
        double const cosLat0 = std::cos(lat0);
        double const sinLat = std::sin(lat);
        double const cosLat = std::cos(lat);
        double const cosC = qBound(-1.0,
            sinLat0 * sinLat + cosLat0 * cosLat * std::cos(deltaLon), 1.0);
        if (cosC < -0.999999) {
            return QPointF(-1.0e6, -1.0e6);
        }
        double const c = std::acos(cosC);
        double const scale = c < 1.0e-9 ? 1.0 : c / std::sin(c);
        double const virtualLon = scale * cosLat * std::sin(deltaLon) * 180.0;
        double const virtualLat = scale * (cosLat0 * sinLat
            - sinLat0 * cosLat * std::cos(deltaLon)) * 90.0;
        qreal const x = bounds.left()
            + static_cast<qreal>((virtualLon - m_viewCenterLon
                + 0.5 * m_viewSpanLon) / m_viewSpanLon) * bounds.width();
        qreal const y = bounds.top()
            + static_cast<qreal>((m_viewCenterLat + 0.5 * m_viewSpanLat
                - virtualLat) / m_viewSpanLat) * bounds.height();
        return QPointF(x, y);
    }
    double const lonDelta = wrapLongitude(lonLat.x() - m_viewCenterLon);
    double const lat = projectLatitude(lonLat.y());
    double const topLat = projectLatitude(
        m_viewCenterLat + 0.5 * m_viewSpanLat);
    double const bottomLat = projectLatitude(
        m_viewCenterLat - 0.5 * m_viewSpanLat);
    double const projectedSpan = qMax(0.0001, topLat - bottomLat);
    qreal const x = bounds.left()
        + static_cast<qreal>((lonDelta + 0.5 * m_viewSpanLon) / m_viewSpanLon) * bounds.width();
    qreal const y = bounds.top()
        + static_cast<qreal>((topLat - lat) / projectedSpan) * bounds.height();
    return QPointF(x, y);
}

QVariantMap WorldMapGpuItem::operationalMarkerAt(const QPointF& point) const
{
    qreal bestDistance = 15.0;
    QVariantMap best;
    for (QVariant const& value : m_operationalMarkers) {
        QVariantMap const marker = value.toMap();
        bool okLon = false;
        bool okLat = false;
        double const lon = marker.value(QStringLiteral("longitude"))
                               .toDouble(&okLon);
        double const lat = marker.value(QStringLiteral("latitude"))
                               .toDouble(&okLat);
        if (!okLon || !okLat) {
            continue;
        }
        qreal const distance = QLineF(
            point, projectLonLatToPoint(QPointF(lon, lat))).length();
        if (distance < bestDistance) {
            bestDistance = distance;
            best = marker;
        }
    }
    return best;
}

QVariantMap WorldMapGpuItem::geographicFeatureAt(const QPointF& point) const
{
    for (auto featureIt = m_geographicFeatures.crbegin();
         featureIt != m_geographicFeatures.crend(); ++featureIt) {
        QVariantMap const feature = featureIt->toMap();
        bool okLongitude = false;
        bool okLatitude = false;
        double const longitude = feature.value(QStringLiteral("longitude")).toDouble(&okLongitude);
        double const latitude = feature.value(QStringLiteral("latitude")).toDouble(&okLatitude);
        if (okLongitude && okLatitude) {
            QPointF const marker = projectLonLatToPoint(QPointF(longitude, latitude));
            double const hitRadius = qBound(
                9.0, feature.value(QStringLiteral("hitRadius"), 14.0).toDouble(), 28.0);
            if (QLineF(marker, point).length() <= hitRadius) {
                return feature;
            }
        }
        for (QVariant const& polygonValue
             : feature.value(QStringLiteral("polygons")).toList()) {
            QVariantList const rings = polygonValue.toList();
            if (rings.isEmpty()) {
                continue;
            }
            QPolygonF polygon;
            for (QVariant const& coordinateValue : rings.first().toList()) {
                QPointF lonLat;
                if (geographicCoordinateToLonLat(coordinateValue, &lonLat)) {
                    polygon << projectLonLatToPoint(lonLat);
                }
            }
            if (polygon.containsPoint(point, Qt::OddEvenFill)) {
                return feature;
            }
        }
    }
    return {};
}

QVariantMap WorldMapGpuItem::coverageCellAt(const QPointF& point) const
{
    QRectF const bounds = mapRect();
    if (!bounds.contains(point)) {
        return {};
    }
    for (const CoverageCell& cell : m_coverageCells) {
        QPointF southWest;
        QPointF northEast;
        if (!maidenheadCellBounds(cell.grid, &southWest, &northEast)) {
            continue;
        }
        QPointF const sw = projectLonLatToPoint(southWest);
        QPointF const se = projectLonLatToPoint(
            QPointF(northEast.x(), southWest.y()));
        QPointF const ne = projectLonLatToPoint(northEast);
        QPointF const nw = projectLonLatToPoint(
            QPointF(southWest.x(), northEast.y()));
        if (!azimuthalProjectionEnabled()
            && (qAbs(se.x() - sw.x()) > bounds.width() * 0.5
                || qAbs(ne.x() - nw.x()) > bounds.width() * 0.5)) {
            continue;
        }
        QPolygonF const cellPolygon {sw, se, ne, nw};
        if (!cellPolygon.containsPoint(point, Qt::OddEvenFill)) {
            continue;
        }
        QString splitSegment;
        if (cell.split && (cell.confirmed || cell.worked)
            && (cell.active || cell.psk || cell.missing)) {
            QPolygonF const historicalHalf {sw, se, ne};
            splitSegment = historicalHalf.containsPoint(point, Qt::OddEvenFill)
                ? QStringLiteral("Historical") : QStringLiteral("Live");
        }
        return {
            {QStringLiteral("grid"), cell.grid},
            {QStringLiteral("workedCount"), cell.workedCount},
            {QStringLiteral("confirmedCount"), cell.confirmedCount},
            {QStringLiteral("activeCount"), cell.activeCount},
            {QStringLiteral("pskCount"), cell.pskCount},
            {QStringLiteral("historicalStatus"), cell.historicalStatus},
            {QStringLiteral("liveStatus"), cell.liveStatus},
            {QStringLiteral("liveOpacity"), cell.liveOpacity},
            {QStringLiteral("split"), cell.split},
            {QStringLiteral("splitSegment"), splitSegment},
            {QStringLiteral("worked"), cell.worked},
            {QStringLiteral("confirmed"), cell.confirmed},
            {QStringLiteral("active"), cell.active},
            {QStringLiteral("missing"), cell.missing},
            {QStringLiteral("psk"), cell.psk}
        };
    }
    return {};
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
    // 1.0.221 — Se l'utente ha zoomato/pannato manualmente, NON sovrascrive
    // i target: smoothViewport continua a interpolare verso i parametri
    // user-set. resetView() rimuove il lock e riabilita l'auto-fit.
    if (m_userViewportLocked) {
        return;
    }

    // AEQD is a local, QTH-centred globe view.  Keep the globe centred on
    // the virtual origin; manual pan/zoom remains available through the
    // existing viewport controls.
    if (azimuthalProjectionEnabled()) {
        m_targetCenterLon = 0.0;
        m_targetCenterLat = 0.0;
        m_targetSpanLon = 360.0;
        m_targetSpanLat = 180.0;
        return;
    }

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
    m_lastLabelLayoutUs = 0;

    QRectF const rect = mapRect();
    if (rect.isEmpty()) {
        m_lastContactCount = 0;
        m_lastLineVertexCount = 0;
        m_lastStateBoundaryVertexCount = 0;
        m_lastCountyBoundaryVertexCount = 0;
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
    double latStep = 20.0;
    if (m_viewSpanLat < 110.0) latStep = 10.0;
    if (m_viewSpanLat < 55.0) latStep = 5.0;

    auto appendProjectedLine = [&rect, this](QVector<QPointF>* target,
                                               bool meridian, double value) {
        if (!target) {
            return;
        }
        QPointF previous;
        bool havePrevious = false;
        constexpr int steps = 72;
        for (int index = 0; index <= steps; ++index) {
            double const fraction = static_cast<double>(index) / steps;
            QPointF const lonLat = meridian
                ? QPointF(value, -89.0 + 178.0 * fraction)
                : QPointF(-180.0 + 360.0 * fraction, value);
            QPointF const current = projectLonLatToPoint(lonLat);
            bool const visible = rect.adjusted(-3.0, -3.0, 3.0, 3.0)
                .contains(current);
            if (visible && havePrevious
                && QLineF(previous, current).length() < rect.width() * 0.36) {
                *target << previous << current;
            }
            previous = current;
            havePrevious = visible;
        }
    };

    if (azimuthalProjectionEnabled()) {
        for (double lon = -180.0; lon <= 180.0; lon += lonStep) {
            appendProjectedLine(&m_batch.gridLines, true, lon);
        }
        for (double lat = -80.0; lat <= 80.0; lat += latStep) {
            appendProjectedLine(&m_batch.gridLines, false, lat);
        }
    } else {
        double const leftLon = m_viewCenterLon - 0.5 * m_viewSpanLon;
        double const rightLon = m_viewCenterLon + 0.5 * m_viewSpanLon;
        double const startLon = std::floor(leftLon / lonStep) * lonStep;
        for (double lon = startLon; lon <= rightLon; lon += lonStep) {
            qreal const x = projectLonLatToPoint(QPointF(lon, m_viewCenterLat)).x();
            m_batch.gridLines.push_back(QPointF(x, rect.top()));
            m_batch.gridLines.push_back(QPointF(x, rect.bottom()));
        }

        double const topLat = m_viewCenterLat + 0.5 * m_viewSpanLat;
        double const bottomLat = m_viewCenterLat - 0.5 * m_viewSpanLat;
        double const startLat = std::floor(bottomLat / latStep) * latStep;
        for (double lat = startLat; lat <= topLat; lat += latStep) {
            qreal const y = projectLonLatToPoint(QPointF(m_viewCenterLon, lat)).y();
            m_batch.gridLines.push_back(QPointF(rect.left(), y));
            m_batch.gridLines.push_back(QPointF(rect.right(), y));
        }
    }

    if (m_timeZoneOverlayEnabled) {
        int timeZoneLabels = 0;
        for (int utcOffset = -12; utcOffset <= 12; ++utcOffset) {
            double const longitude = utcOffset * 15.0;
            appendProjectedLine(&m_batch.timeZoneLines, true, longitude);
            if ((utcOffset % 2) != 0 || timeZoneLabels >= 13) {
                continue;
            }
            QPointF const labelPoint = projectLonLatToPoint(
                QPointF(longitude + 7.5, 0.0));
            if (!rect.adjusted(8.0, 8.0, -8.0, -8.0).contains(labelPoint)) {
                continue;
            }
            QString const label = utcOffset == 0 ? QStringLiteral("UTC")
                : QStringLiteral("UTC%1%2")
                    .arg(utcOffset > 0 ? QLatin1Char('+') : QLatin1Char('-'))
                    .arg(qAbs(utcOffset));
            m_labels.push_back({label, labelPoint + QPointF(3.0, -4.0),
                                QRectF(), QColor(112, 223, 255, 165), false});
            ++timeZoneLabels;
        }
    }

    // A six-character Maidenhead square is smaller than a pixel when the
    // whole continent is visible. Keep the source model and hit testing exact,
    // but merge fine squares to their four-character parent for this frame.
    bool const aggregateFineCoverage = m_viewSpanLon >= 55.0
        || m_viewSpanLat >= 34.0;
    QVector<CoverageCell> visualCoverageCells;
    visualCoverageCells.reserve(m_coverageCells.size());
    QHash<QString, int> visualCoverageIndex;
    for (const CoverageCell& source : m_coverageCells) {
        CoverageCell cell = source;
        if (aggregateFineCoverage && cell.grid.size() >= 6) {
            cell.grid = cell.grid.left(4);
        }
        int const existing = visualCoverageIndex.value(cell.grid, -1);
        if (existing < 0) {
            visualCoverageIndex.insert(cell.grid, visualCoverageCells.size());
            visualCoverageCells.push_back(cell);
            continue;
        }

        CoverageCell& merged = visualCoverageCells[existing];
        merged.workedCount += cell.workedCount;
        merged.confirmedCount += cell.confirmedCount;
        merged.activeCount += cell.activeCount;
        merged.pskCount += cell.pskCount;
        merged.liveOpacity = qMax(merged.liveOpacity, cell.liveOpacity);
        merged.split = merged.split || cell.split;
        merged.worked = merged.worked || cell.worked;
        merged.confirmed = merged.confirmed || cell.confirmed;
        merged.active = merged.active || cell.active;
        merged.missing = merged.missing || cell.missing;
        merged.psk = merged.psk || cell.psk;
        if (merged.historicalStatus.isEmpty() || cell.confirmed) {
            merged.historicalStatus = cell.historicalStatus;
        }
        QString const incomingStatus = cell.liveStatus.trimmed().toUpper();
        QString const currentStatus = merged.liveStatus.trimmed().toUpper();
        if (currentStatus.isEmpty()
            || incomingStatus == QStringLiteral("CQDX")
            || incomingStatus == QStringLiteral("QRZ")
            || (currentStatus != QStringLiteral("CQDX")
                && currentStatus != QStringLiteral("QRZ")
                && !incomingStatus.isEmpty())) {
            merged.liveStatus = cell.liveStatus;
        }
    }

    for (const CoverageCell& cell : visualCoverageCells) {
        QPointF southWest;
        QPointF northEast;
        if (!maidenheadCellBounds(cell.grid, &southWest, &northEast)) {
            continue;
        }
        QPointF const southEast(northEast.x(), southWest.y());
        QPointF const northWest(southWest.x(), northEast.y());
        QPointF const sw = projectLonLatToPoint(southWest);
        QPointF const se = projectLonLatToPoint(southEast);
        QPointF const ne = projectLonLatToPoint(northEast);
        QPointF const nw = projectLonLatToPoint(northWest);
        if (!azimuthalProjectionEnabled()
            && (qAbs(se.x() - sw.x()) > rect.width() * 0.5
                || qAbs(ne.x() - nw.x()) > rect.width() * 0.5)) {
            continue;
        }

        auto liveGeometry = [this](const CoverageCell& coverage) {
            QString const status = coverage.liveStatus.trimmed().toUpper();
            int const opacityBucket = coverage.liveOpacity >= 0.72
                ? 0 : (coverage.liveOpacity >= 0.45 ? 1 : 2);
            if (status == QStringLiteral("CQDX")
                || status == QStringLiteral("QRZ")
                || coverage.missing) {
                if (opacityBucket == 1) {
                    return qMakePair(&m_batch.missingCoverageMediumTriangles,
                                     &m_batch.missingCoverageMediumLines);
                }
                if (opacityBucket == 2) {
                    return qMakePair(&m_batch.missingCoverageFadedTriangles,
                                     &m_batch.missingCoverageFadedLines);
                }
                return qMakePair(&m_batch.missingCoverageTriangles,
                                 &m_batch.missingCoverageLines);
            }
            if (status == QStringLiteral("WSPR")
                || status == QStringLiteral("QSX")
                || status == QStringLiteral("PSK")
                || coverage.psk) {
                if (opacityBucket == 1) {
                    return qMakePair(&m_batch.pskCoverageMediumTriangles,
                                     &m_batch.pskCoverageMediumLines);
                }
                if (opacityBucket == 2) {
                    return qMakePair(&m_batch.pskCoverageFadedTriangles,
                                     &m_batch.pskCoverageFadedLines);
                }
                return qMakePair(&m_batch.pskCoverageTriangles,
                                 &m_batch.pskCoverageLines);
            }
            if (opacityBucket == 1) {
                return qMakePair(&m_batch.activeCoverageMediumTriangles,
                                 &m_batch.activeCoverageMediumLines);
            }
            if (opacityBucket == 2) {
                return qMakePair(&m_batch.activeCoverageFadedTriangles,
                                 &m_batch.activeCoverageFadedLines);
            }
            return qMakePair(&m_batch.activeCoverageTriangles,
                             &m_batch.activeCoverageLines);
        };
        auto historyGeometry = [this](const CoverageCell& coverage) {
            if (coverage.confirmed) {
                return qMakePair(&m_batch.confirmedCoverageTriangles,
                                 &m_batch.confirmedCoverageLines);
            }
            return qMakePair(&m_batch.workedCoverageTriangles,
                             &m_batch.workedCoverageLines);
        };

        bool const hasHistory = cell.confirmed || cell.worked;
        bool const hasLive = cell.active || cell.psk || cell.missing;
        auto appendPin = [](const QPair<QVector<QPointF>*, QVector<QPointF>*>& geometry,
                            const QPointF& center) {
            QPointF const headTop(center.x(), center.y() - 7.0);
            QPointF const headLeft(center.x() - 5.0, center.y() - 1.0);
            QPointF const headRight(center.x() + 5.0, center.y() - 1.0);
            QPointF const tip(center.x(), center.y() + 4.0);
            (*geometry.first) << headTop << headLeft << tip
                              << headTop << tip << headRight;
            (*geometry.second) << tip << QPointF(center.x(), center.y() + 12.0);
        };
        if (m_coveragePushPins) {
            if (cell.split && hasHistory && hasLive) {
                appendPin(historyGeometry(cell), (sw + se + ne) / 3.0);
                appendPin(liveGeometry(cell), (sw + ne + nw) / 3.0);
            } else {
                appendPin(hasHistory ? historyGeometry(cell) : liveGeometry(cell),
                          (sw + se + ne + nw) / 4.0);
            }
            continue;
        }
        if (cell.split && hasHistory && hasLive) {
            auto const history = historyGeometry(cell);
            auto const live = liveGeometry(cell);
            (*history.first) << sw << se << ne;
            (*live.first) << sw << ne << nw;
            (*history.second) << sw << se << se << ne << ne << nw << nw << sw;
            (*live.second) << sw << ne;
        } else {
            auto const geometry = hasHistory
                ? historyGeometry(cell)
                : liveGeometry(cell);
            (*geometry.first) << sw << se << ne << sw << ne << nw;
            (*geometry.second) << sw << se << se << ne << ne << nw << nw << sw;
        }
    }

    auto appendBoundarySegment = [&rect](QVector<QPointF>* triangles,
                                         QVector<QPointF>* lines,
                                         const QPointF& start,
                                         const QPointF& end,
                                         qreal width) {
        if (!triangles && !lines) {
            return;
        }
        QLineF const segment(start, end);
        double const length = segment.length();
        if (length < 0.001 || length >= rect.width() * 0.5) {
            return;
        }
        QRectF const segmentBounds(start, end);
        if (!segmentBounds.normalized().adjusted(-width, -width, width, width)
                 .intersects(rect)) {
            return;
        }
        if (lines) {
            *lines << start << end;
        }
        if (!triangles) {
            return;
        }
        double const halfWidth = width * 0.5;
        QPointF const normal(-(end.y() - start.y()) / length * halfWidth,
                             (end.x() - start.x()) / length * halfWidth);
        QPointF const startLeft = start + normal;
        QPointF const startRight = start - normal;
        QPointF const endLeft = end + normal;
        QPointF const endRight = end - normal;
        *triangles << startLeft << endLeft << endRight
                   << startLeft << endRight << startRight;
    };

    int geographicCoordinateTotal = 0;
    int geographicCoordinateAccepted = 0;
    int geographicCoordinateInvalid = 0;
    int geographicCoordinateOffscreen = 0;
    for (QVariant const& featureValue : m_geographicFeatures) {
        QVariantMap const feature = featureValue.toMap();
        QString const featureType = feature.value(QStringLiteral("type")).toString();
        if (featureType == QStringLiteral("earthquake")) {
            bool okLongitude = false;
            bool okLatitude = false;
            double const longitude = feature.value(QStringLiteral("longitude")).toDouble(&okLongitude);
            double const latitude = feature.value(QStringLiteral("latitude")).toDouble(&okLatitude);
            if (okLongitude && okLatitude) {
                QPointF const point = projectLonLatToPoint(QPointF(longitude, latitude));
                if (isInViewport(rect, point, 24.0)) {
                    double const magnitude = feature.value(QStringLiteral("magnitude"), 0.0).toDouble();
                    if (magnitude >= 6.0) {
                        m_batch.earthquakeHighMarkers << point;
                    } else if (magnitude >= 4.5) {
                        m_batch.earthquakeMediumMarkers << point;
                    } else {
                        m_batch.earthquakeLowMarkers << point;
                    }
                }
            }
            continue;
        }
        QVector<QPointF>* const boundaryGeometry =
            featureType == QStringLiteral("states")
                ? &m_batch.stateBoundaryTriangles
                : &m_batch.countyBoundaryTriangles;
        QVector<QPointF>* const boundaryLines =
            featureType == QStringLiteral("states")
                ? &m_batch.stateBoundaryLines
                : &m_batch.countyBoundaryLines;
        qreal const boundaryWidth = featureType == QStringLiteral("states")
            ? 2.6 : 1.15;
        for (QVariant const& polygonValue
             : feature.value(QStringLiteral("polygons")).toList()) {
            for (QVariant const& ringValue : polygonValue.toList()) {
                QVariantList const coordinates = ringValue.toList();
                QPointF previous;
                QPointF first;
                QPointF lastDrawn;
                bool havePrevious = false;
                bool haveDrawnPoint = false;
                // TIGER GeoJSON is intentionally dense. At a continental zoom
                // adjacent source points are often below one physical pixel.
                // Keep accumulating them until they create a visible segment;
                // dropping them individually leaves a non-empty layer with no
                // actual state/county geometry.
                qreal const minimumDrawLength = featureType == QStringLiteral("states")
                    ? 0.35 : 0.55;
                for (QVariant const& coordinateValue : coordinates) {
                    ++geographicCoordinateTotal;
                    QPointF lonLat;
                    if (!geographicCoordinateToLonLat(coordinateValue, &lonLat)) {
                        ++geographicCoordinateInvalid;
                        continue;
                    }
                    QPointF const current = projectLonLatToPoint(lonLat);
                    if (!std::isfinite(current.x()) || !std::isfinite(current.y())) {
                        ++geographicCoordinateInvalid;
                        continue;
                    }
                    ++geographicCoordinateAccepted;
                    if (!rect.adjusted(-rect.width(), -rect.height(),
                                       rect.width(), rect.height()).contains(current)) {
                        ++geographicCoordinateOffscreen;
                    }
                    if (!havePrevious) {
                        first = current;
                        lastDrawn = current;
                        haveDrawnPoint = true;
                    } else if (qAbs(current.x() - previous.x()) >= rect.width() * 0.5) {
                        // Do not bridge a wrap/projection discontinuity.
                        if (haveDrawnPoint
                            && QLineF(lastDrawn, previous).length() >= 0.001) {
                            appendBoundarySegment(boundaryGeometry, boundaryLines,
                                                  lastDrawn, previous,
                                                  boundaryWidth);
                        }
                        first = current;
                        lastDrawn = current;
                        haveDrawnPoint = true;
                    } else if (QLineF(lastDrawn, current).length() >= minimumDrawLength) {
                        appendBoundarySegment(boundaryGeometry, boundaryLines,
                                              lastDrawn, current,
                                              boundaryWidth);
                        lastDrawn = current;
                    }
                    previous = current;
                    havePrevious = true;
                }
                if (havePrevious && haveDrawnPoint
                    && qAbs(first.x() - previous.x()) < rect.width() * 0.5) {
                    if (QLineF(lastDrawn, previous).length() >= 0.001) {
                        appendBoundarySegment(boundaryGeometry, boundaryLines,
                                              lastDrawn, previous,
                                              boundaryWidth);
                    }
                    if (QLineF(previous, first).length() >= 0.001) {
                        appendBoundarySegment(boundaryGeometry, boundaryLines,
                                              previous, first,
                                              boundaryWidth);
                    }
                }
            }
        }
    }

    int operationalLabelCount = 0;
    for (QVariant const& markerValue : m_operationalMarkers) {
        QVariantMap const marker = markerValue.toMap();
        bool okLon = false;
        bool okLat = false;
        double const lon = marker.value(QStringLiteral("longitude"))
                               .toDouble(&okLon);
        double const lat = marker.value(QStringLiteral("latitude"))
                               .toDouble(&okLat);
        if (!okLon || !okLat) {
            continue;
        }
        QPointF const point = projectLonLatToPoint(QPointF(lon, lat));
        if (!isInViewport(rect, point, 18.0)) {
            continue;
        }
        QString const type = marker.value(QStringLiteral("type"))
                                 .toString().trimmed().toUpper();
        if (type == QStringLiteral("POTA")) {
            m_batch.potaMarkers << point;
        } else if (type == QStringLiteral("IOTA")) {
            m_batch.iotaMarkers << point;
        } else if (type == QStringLiteral("MOON")) {
            m_batch.moonMarkers << point;
        } else if (type == QStringLiteral("SATELLITE")) {
            m_batch.satelliteMarkers << point;
        } else {
            m_batch.wpxMarkers << point;
        }
        int const operationalLabelLimit = qRound(
            16.0 * layerLabelDensity(type == QStringLiteral("POTA")
                                          ? QStringLiteral("pota")
                                          : type == QStringLiteral("IOTA")
                                                ? QStringLiteral("iota")
                                                : type == QStringLiteral("MOON")
                                                      ? QStringLiteral("moon")
                                                      : QStringLiteral("wpx")) / 100.0);
        if (operationalLabelCount < operationalLabelLimit) {
            QString label = marker.value(QStringLiteral("label")).toString();
            if (label.isEmpty()) {
                label = marker.value(QStringLiteral("reference")).toString();
            }
            if (!label.isEmpty()) {
                QColor const color = type == QStringLiteral("POTA")
                    ? QColor(166, 255, 154, 235)
                    : (type == QStringLiteral("IOTA")
                       ? QColor(130, 236, 255, 235)
                       : (type == QStringLiteral("MOON")
                          ? QColor(235, 244, 255, 245)
                          : QColor(255, 215, 145, 235)));
                QPointF const labelOffset = type == QStringLiteral("SATELLITE")
                    ? QPointF(15.0, -8.0) : QPointF(7.0, -7.0);
                m_labels.push_back(
                    {label.left(18), point + labelOffset, QRectF(), color});
                ++operationalLabelCount;
            }
        }
    }

    if (m_hasHome) {
        QPointF const home = projectLonLatToPoint(m_homeLonLat);
        bool const homeVisible = isInViewport(rect, home, 18.0);
        if (homeVisible) {
            m_batch.genericMarkers.push_back(m_homeLonLat);
        }
        if (homeVisible && !m_homeGrid.isEmpty()
            && layerLabelDensity(QStringLiteral("live")) > 0) {
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

            auto const arc = greatCircle(contact.sourceLonLat, contact.destinationLonLat, kGreatCircleSteps);
            if (arc.size() >= 2) {
                AnimatedPath animated;
                animated.key = contact.call;
                animated.role = contact.role;
                animated.sourceLonLat = contact.sourceLonLat;
                animated.destinationLonLat = contact.destinationLonLat;
                animated.points = arc;
                m_animatedPaths.push_back(animated);
            }
        }

        QPointF const markerLonLat = contact.role == PathRole::OutgoingFromMe
            ? contact.destinationLonLat
            : contact.sourceLonLat;
        QPointF const marker = contact.role == PathRole::OutgoingFromMe ? destination : source;
        bool const markerVisible = isInViewport(rect, marker, 18.0);
        if (markerVisible) {
            switch (contact.role) {
            case PathRole::IncomingToMe:
                m_batch.incomingMarkers.push_back(markerLonLat);
                ++m_lastVisiblePathCount;
                break;
            case PathRole::OutgoingFromMe:
                m_batch.outgoingMarkers.push_back(markerLonLat);
                ++m_lastVisiblePathCount;
                break;
            case PathRole::BandOnly:
                m_batch.bandMarkers.push_back(markerLonLat);
                ++m_lastVisibleBandCount;
                break;
            case PathRole::Generic:
            default:
                m_batch.genericMarkers.push_back(markerLonLat);
                ++m_lastVisiblePathCount;
                break;
            }
        }

        int const contactLabelLimit = qRound(
            kMaxVisibleContactLabels * layerLabelDensity(QStringLiteral("live")) / 100.0);
        if (markerVisible && !contact.call.isEmpty()
            && contactLabels < contactLabelLimit) {
            QColor labelColor = contact.role == PathRole::BandOnly
                ? QColor(255, 238, 174, 235)
                : QColor(255, 244, 196, 235);
            m_labels.push_back({contact.call.left(12), marker + QPointF(7.0, -7.0), QRectF(), labelColor});
            ++contactLabels;
        }
    }

    QElapsedTimer labelLayoutTimer;
    labelLayoutTimer.start();
    layoutLabels(rect);
    m_lastLabelLayoutUs = labelLayoutTimer.nsecsElapsed() / 1000;
    m_lastContactCount = totalContactCount;
    m_lastLineVertexCount = kGreatCircleSteps * 2 * (m_batch.genericPaths.size()
        + m_batch.incomingPaths.size()
        + m_batch.outgoingPaths.size());
    m_lastStateBoundaryVertexCount = m_batch.stateBoundaryTriangles.size();
    m_lastCountyBoundaryVertexCount = m_batch.countyBoundaryTriangles.size();
    m_lastStateBoundaryLineVertexCount = m_batch.stateBoundaryLines.size();
    m_lastCountyBoundaryLineVertexCount = m_batch.countyBoundaryLines.size();
    QString const geographicDiagnostic = QStringLiteral(
        "features=%1 stateTriangles=%2 stateLines=%3 countyTriangles=%4 countyLines=%5 geoCoords=%6/%7 invalid=%8 offscreen=%9 rect=%10x%11 coverageCells=%12 renderCells=%13 activeCoverageLines=%14")
        .arg(m_geographicFeatures.size())
        .arg(m_lastStateBoundaryVertexCount)
        .arg(m_lastStateBoundaryLineVertexCount)
        .arg(m_lastCountyBoundaryVertexCount)
        .arg(m_lastCountyBoundaryLineVertexCount)
        .arg(geographicCoordinateAccepted)
        .arg(geographicCoordinateTotal)
        .arg(geographicCoordinateInvalid)
        .arg(geographicCoordinateOffscreen)
        .arg(rect.width(), 0, 'f', 0)
        .arg(rect.height(), 0, 'f', 0)
        .arg(m_coverageCells.size())
        .arg(visualCoverageCells.size())
        .arg(m_batch.activeCoverageLines.size());
    if (geographicDiagnostic != m_lastGeographicGeometryDiagnostic) {
        m_lastGeographicGeometryDiagnostic = geographicDiagnostic;
        qInfo().noquote() << "[MAPGEO]" << geographicDiagnostic;
    }
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

void WorldMapGpuItem::markDirty(bool contactGeometryChanged)
{
    m_geometryDirty = true;
    if (contactGeometryChanged) {
        m_contactGeometryDirty = true;
        m_animationGeometryDirty = true;
    }
    if (isVisible()) {
        update();
    }
}
