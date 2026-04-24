#include "WorldMapItem.hpp"

#include <QMouseEvent>
#include <QPainter>

namespace {

constexpr int kWorldMapRepaintMs = 60;

}

WorldMapItem::WorldMapItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
    , m_widget(nullptr)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(false);
    setAntialiasing(true);
    setImplicitWidth(340);
    setImplicitHeight(220);

    connect(&m_widget, &WorldMapWidget::contactClicked,
            this, &WorldMapItem::contactClicked);

    connect(this, &QQuickItem::widthChanged, this, [this]() {
        syncWidgetSize();
        update();
    });
    connect(this, &QQuickItem::heightChanged, this, [this]() {
        syncWidgetSize();
        update();
    });
    connect(this, &QQuickItem::visibleChanged, this, [this]() {
        if (isVisible()) {
            syncWidgetSize();
            update();
        }
    });

    m_repaintTimer.setInterval(kWorldMapRepaintMs);
    connect(&m_repaintTimer, &QTimer::timeout, this, [this]() {
        if (isVisible()) {
            update();
        }
    });
    m_repaintTimer.start();
}

void WorldMapItem::setHomeGrid(const QString& grid)
{
    m_widget.setHomeGrid(grid);
    update();
}

void WorldMapItem::setGreylineEnabled(bool enabled)
{
    m_widget.setGreylineEnabled(enabled);
    update();
}

void WorldMapItem::setDistanceInMiles(bool enabled)
{
    m_widget.setDistanceInMiles(enabled);
    update();
}

void WorldMapItem::setTransmitState(bool transmitting,
                                    const QString& targetCall,
                                    const QString& targetGrid,
                                    const QString& mode)
{
    m_widget.setTransmitState(transmitting, targetCall, targetGrid, mode);
    update();
}

void WorldMapItem::clearContacts()
{
    m_widget.clearContacts();
    update();
}

void WorldMapItem::downgradeContactToBand(const QString& call)
{
    m_widget.downgradeContactToBand(call);
    update();
}

void WorldMapItem::addContact(const QString& call,
                              const QString& sourceGrid,
                              const QString& destinationGrid,
                              int role)
{
    m_widget.addContact(call, sourceGrid, destinationGrid, pathRoleFromInt(role));
    update();
}

void WorldMapItem::addContactByLonLat(const QString& call,
                                      double sourceLon,
                                      double sourceLat,
                                      const QString& destinationGrid,
                                      int role)
{
    m_widget.addContactByLonLat(call,
                                QPointF(sourceLon, sourceLat),
                                destinationGrid,
                                pathRoleFromInt(role));
    update();
}

void WorldMapItem::paint(QPainter* painter)
{
    if (!painter) {
        return;
    }

    syncWidgetSize();
    m_widget.render(painter, QPoint(), QRegion(), QWidget::DrawChildren);
}

void WorldMapItem::mousePressEvent(QMouseEvent* event)
{
    if (event && event->button() == Qt::LeftButton) {
        syncWidgetSize();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_widget.handleMapClick(event->position());
#else
        m_widget.handleMapClick(event->localPos());
#endif
        event->accept();
        return;
    }

    QQuickPaintedItem::mousePressEvent(event);
}

WorldMapWidget::PathRole WorldMapItem::pathRoleFromInt(int role)
{
    switch (role) {
    case 1:
        return WorldMapWidget::PathRole::IncomingToMe;
    case 2:
        return WorldMapWidget::PathRole::OutgoingFromMe;
    case 3:
        return WorldMapWidget::PathRole::BandOnly;
    case 0:
    default:
        return WorldMapWidget::PathRole::Generic;
    }
}

void WorldMapItem::syncWidgetSize()
{
    int const w = qMax(1, qRound(width()));
    int const h = qMax(1, qRound(height()));
    if (m_widget.size() != QSize(w, h)) {
        m_widget.resize(w, h);
    }
}
