#include "MapLayerModel.h"

#include <QtMath>

MapLayerModel::MapLayerModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_layers {
          {QStringLiteral("live"), QStringLiteral("LIVE"), QStringLiteral("#00d8ff"), true, 0},
          {QStringLiteral("worked"), QStringLiteral("HISTORICAL"), QStringLiteral("#42c7d9"), true, 0},
          {QStringLiteral("confirmed"), QStringLiteral("CONFIRMED"), QStringLiteral("#2ecc71"), true, 0},
          {QStringLiteral("active"), QStringLiteral("ACTIVE GRIDS"), QStringLiteral("#f6c344"), true, 0},
          {QStringLiteral("missing"), QStringLiteral("MISSING GRIDS"), QStringLiteral("#ff8c42"), true, 0},
          {QStringLiteral("psk"), QStringLiteral("PSK REPORTER"), QStringLiteral("#ba7cff"), true, 0},
          {QStringLiteral("pota"), QStringLiteral("POTA"), QStringLiteral("#74d66a"), false, 0},
          {QStringLiteral("states"), QStringLiteral("STATES"), QStringLiteral("#58b8d6"), false, 0},
          {QStringLiteral("counties"), QStringLiteral("COUNTIES"), QStringLiteral("#7c91a8"), false, 0},
          {QStringLiteral("iota"), QStringLiteral("IOTA"), QStringLiteral("#44d7e8"), false, 0},
          {QStringLiteral("wpx"), QStringLiteral("WPX"), QStringLiteral("#f0b94d"), false, 0},
          {QStringLiteral("moon"), QStringLiteral("MOON"), QStringLiteral("#dbe7ff"), false, 0},
          {QStringLiteral("propagation"), QStringLiteral("PROPAGATION"), QStringLiteral("#ffcf66"), false, 0},
          {QStringLiteral("radar"), QStringLiteral("RADAR WORLD"), QStringLiteral("#ff4d4d"), false, 0},
          {QStringLiteral("lightning"), QStringLiteral("LIGHTNING"), QStringLiteral("#ffffff"), false, 0},
          {QStringLiteral("muf"), QStringLiteral("MUF"), QStringLiteral("#f6c344"), false, 0},
          {QStringLiteral("fof2"), QStringLiteral("foF2"), QStringLiteral("#66d9ff"), false, 0},
          {QStringLiteral("nvis"), QStringLiteral("NVIS"), QStringLiteral("#b18cff"), false, 0},
          {QStringLiteral("es"), QStringLiteral("Sporadic-E"), QStringLiteral("#ff9f43"), false, 0},
          {QStringLiteral("aurora"), QStringLiteral("AURORA"), QStringLiteral("#77ff9f"), false, 0},
          {QStringLiteral("tropo"), QStringLiteral("TROPO"), QStringLiteral("#ffd166"), false, 0},
          {QStringLiteral("earthquakes"), QStringLiteral("EARTHQUAKES"), QStringLiteral("#ff5f57"), false, 0},
          {QStringLiteral("wildfires"), QStringLiteral("WILDFIRES"), QStringLiteral("#ff9f43"), false, 0},
          {QStringLiteral("offline"), QStringLiteral("OFFLINE MODE"), QStringLiteral("#8fa8c4"), false, 0}
      }
{
    // Preserve the established composite-overlay balance as the initial
    // per-layer opacity.  The value then becomes user-editable and is carried
    // by presets/configuration bundles.
    setLayerOpacity(QStringLiteral("radar"), 0.72);
    setLayerOpacity(QStringLiteral("muf"), 0.52);
    setLayerOpacity(QStringLiteral("fof2"), 0.52);
    setLayerOpacity(QStringLiteral("nvis"), 0.44);
    setLayerOpacity(QStringLiteral("es"), 0.58);
    setLayerOpacity(QStringLiteral("aurora"), 0.62);
    setLayerOpacity(QStringLiteral("tropo"), 0.58);
    setLayerOpacity(QStringLiteral("lightning"), 0.90);
    setLayerOpacity(QStringLiteral("earthquakes"), 0.94);
    setLayerOpacity(QStringLiteral("wildfires"), 0.92);
}

int MapLayerModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_layers.size();
}

QVariant MapLayerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_layers.size()) {
        return {};
    }

    Layer const& layer = m_layers.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return layer.label;
    case LayerIdRole:
        return layer.id;
    case ColorRole:
        return layer.color;
    case LayerEnabledRole:
        return layer.enabled;
    case CountRole:
        return layer.count;
    case OpacityRole:
        return layer.opacity;
    case ThicknessRole:
        return layer.thickness;
    case LabelDensityRole:
        return layer.labelDensity;
    default:
        return {};
    }
}

QHash<int, QByteArray> MapLayerModel::roleNames() const
{
    return {
        {LayerIdRole, QByteArrayLiteral("layerId")},
        {LabelRole, QByteArrayLiteral("label")},
        {ColorRole, QByteArrayLiteral("layerColor")},
        {LayerEnabledRole, QByteArrayLiteral("layerEnabled")},
        {CountRole, QByteArrayLiteral("layerCount")},
        {OpacityRole, QByteArrayLiteral("layerOpacity")},
        {ThicknessRole, QByteArrayLiteral("layerThickness")},
        {LabelDensityRole, QByteArrayLiteral("labelDensity")}
    };
}

bool MapLayerModel::layerEnabled(const QString& id) const
{
    int const row = indexOf(id);
    return row >= 0 ? m_layers.at(row).enabled : false;
}

QString MapLayerModel::layerColor(const QString& id) const
{
    int const row = indexOf(id);
    return row >= 0 ? m_layers.at(row).color : QString();
}

double MapLayerModel::layerOpacity(const QString& id) const
{
    int const row = indexOf(id);
    return row >= 0 ? m_layers.at(row).opacity : 1.0;
}

double MapLayerModel::layerThickness(const QString& id) const
{
    int const row = indexOf(id);
    return row >= 0 ? m_layers.at(row).thickness : 1.0;
}

int MapLayerModel::labelDensity(const QString& id) const
{
    int const row = indexOf(id);
    return row >= 0 ? m_layers.at(row).labelDensity : 100;
}

QVariantMap MapLayerModel::layerStyle(const QString& id) const
{
    int const row = indexOf(id);
    if (row < 0) {
        return {};
    }
    Layer const& layer = m_layers.at(row);
    return {
        {QStringLiteral("color"), layer.color},
        {QStringLiteral("opacity"), layer.opacity},
        {QStringLiteral("thickness"), layer.thickness},
        {QStringLiteral("labelDensity"), layer.labelDensity}
    };
}

QVariantMap MapLayerModel::allLayerStyles() const
{
    QVariantMap result;
    for (Layer const& layer : m_layers) {
        result.insert(layer.id, QVariantMap {
            {QStringLiteral("color"), layer.color},
            {QStringLiteral("opacity"), layer.opacity},
            {QStringLiteral("thickness"), layer.thickness},
            {QStringLiteral("labelDensity"), layer.labelDensity}
        });
    }
    return result;
}

void MapLayerModel::setCount(const QString& id, int count)
{
    int const row = indexOf(id);
    if (row < 0 || m_layers.at(row).count == count) {
        return;
    }
    m_layers[row].count = count;
    QModelIndex const modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {CountRole});
}

void MapLayerModel::setLayerEnabled(const QString& id, bool enabled)
{
    int const row = indexOf(id);
    if (row < 0 || m_layers.at(row).enabled == enabled) {
        return;
    }
    m_layers[row].enabled = enabled;
    QModelIndex const modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {LayerEnabledRole});
    emit layerToggled(m_layers.at(row).id, enabled);
}

void MapLayerModel::toggleLayer(const QString& id)
{
    setLayerEnabled(id, !layerEnabled(id));
}

void MapLayerModel::setLayerColor(const QString& id, const QString& color)
{
    QColor const parsed(color.trimmed());
    if (!parsed.isValid()) {
        return;
    }
    int const row = indexOf(id);
    QString const normalized = parsed.name(QColor::HexRgb);
    if (row < 0 || m_layers.at(row).color.compare(normalized, Qt::CaseInsensitive) == 0) {
        return;
    }
    m_layers[row].color = normalized;
    QModelIndex const modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {ColorRole});
    emit layerStyleChanged(m_layers.at(row).id);
}

void MapLayerModel::setLayerOpacity(const QString& id, double opacity)
{
    int const row = indexOf(id);
    double const bounded = qBound(0.05, opacity, 1.0);
    if (row < 0 || qFuzzyCompare(m_layers.at(row).opacity, bounded)) {
        return;
    }
    m_layers[row].opacity = bounded;
    QModelIndex const modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {OpacityRole});
    emit layerStyleChanged(m_layers.at(row).id);
}

void MapLayerModel::setLayerThickness(const QString& id, double thickness)
{
    int const row = indexOf(id);
    double const bounded = qBound(0.5, thickness, 8.0);
    if (row < 0 || qFuzzyCompare(m_layers.at(row).thickness, bounded)) {
        return;
    }
    m_layers[row].thickness = bounded;
    QModelIndex const modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {ThicknessRole});
    emit layerStyleChanged(m_layers.at(row).id);
}

void MapLayerModel::setLabelDensity(const QString& id, int density)
{
    int const row = indexOf(id);
    int const bounded = qBound(0, density, 100);
    if (row < 0 || m_layers.at(row).labelDensity == bounded) {
        return;
    }
    m_layers[row].labelDensity = bounded;
    QModelIndex const modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, {LabelDensityRole});
    emit layerStyleChanged(m_layers.at(row).id);
}

void MapLayerModel::setLayerStyle(const QString& id, const QString& color,
                                  double opacity, double thickness, int density)
{
    setLayerColor(id, color);
    setLayerOpacity(id, opacity);
    setLayerThickness(id, thickness);
    setLabelDensity(id, density);
}

int MapLayerModel::indexOf(const QString& id) const
{
    for (int row = 0; row < m_layers.size(); ++row) {
        if (m_layers.at(row).id.compare(id, Qt::CaseInsensitive) == 0) {
            return row;
        }
    }
    return -1;
}
