#pragma once

#include <QQuickPaintedItem>
#include <QTimer>

#include "widgets/worldmapwidget.h"

class WorldMapItem : public QQuickPaintedItem
{
    Q_OBJECT

public:
    explicit WorldMapItem(QQuickItem* parent = nullptr);

    Q_INVOKABLE void setHomeGrid(const QString& grid);
    Q_INVOKABLE void setGreylineEnabled(bool enabled);
    Q_INVOKABLE void setDistanceInMiles(bool enabled);
    Q_INVOKABLE void setTransmitState(bool transmitting,
                                      const QString& targetCall,
                                      const QString& targetGrid,
                                      const QString& mode);
    Q_INVOKABLE void clearContacts();
    Q_INVOKABLE void downgradeContactToBand(const QString& call);
    Q_INVOKABLE void addContact(const QString& call,
                                const QString& sourceGrid,
                                const QString& destinationGrid,
                                int role = 0);
    Q_INVOKABLE void addContactByLonLat(const QString& call,
                                        double sourceLon,
                                        double sourceLat,
                                        const QString& destinationGrid,
                                        int role = 0);

    void paint(QPainter* painter) override;

Q_SIGNALS:
    void contactClicked(const QString& call, const QString& grid);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    static WorldMapWidget::PathRole pathRoleFromInt(int role);
    void syncWidgetSize();

    WorldMapWidget m_widget;
    QTimer m_repaintTimer;
};
