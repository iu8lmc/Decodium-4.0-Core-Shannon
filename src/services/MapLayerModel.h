#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

class MapLayerModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        LayerIdRole = Qt::UserRole + 1,
        LabelRole,
        ColorRole,
        LayerEnabledRole,
        CountRole,
        OpacityRole,
        ThicknessRole,
        LabelDensityRole
    };
    Q_ENUM(Role)

    explicit MapLayerModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool layerEnabled(const QString& id) const;
    Q_INVOKABLE QString layerColor(const QString& id) const;
    Q_INVOKABLE double layerOpacity(const QString& id) const;
    Q_INVOKABLE double layerThickness(const QString& id) const;
    Q_INVOKABLE int labelDensity(const QString& id) const;
    Q_INVOKABLE QVariantMap layerStyle(const QString& id) const;
    Q_INVOKABLE QVariantMap allLayerStyles() const;
    void setCount(const QString& id, int count);

    Q_INVOKABLE void setLayerEnabled(const QString& id, bool enabled);
    Q_INVOKABLE void toggleLayer(const QString& id);
    Q_INVOKABLE void setLayerColor(const QString& id, const QString& color);
    Q_INVOKABLE void setLayerOpacity(const QString& id, double opacity);
    Q_INVOKABLE void setLayerThickness(const QString& id, double thickness);
    Q_INVOKABLE void setLabelDensity(const QString& id, int density);
    Q_INVOKABLE void setLayerStyle(const QString& id, const QString& color,
                                   double opacity, double thickness, int density);

signals:
    void layerToggled(const QString& id, bool enabled);
    void layerStyleChanged(const QString& id);

private:
    struct Layer {
        QString id;
        QString label;
        QString color;
        bool enabled {true};
        int count {0};
        double opacity {1.0};
        double thickness {1.0};
        int labelDensity {100};
    };

    int indexOf(const QString& id) const;

    QVector<Layer> m_layers;
};
