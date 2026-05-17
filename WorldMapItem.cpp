#include "WorldMapItem.hpp"

#include <QMouseEvent>
#include <QPainter>

namespace {

// 1.0.209 — Repaint timer ridotto da 60ms (16Hz always-on, ~50% CPU sprecato
// in idle perche' paint() invocava QWidget::render full su texture 1280x720)
// a 250ms (4Hz). Combinato con dirty flag: il timer fa update() SOLO se c'e'
// un cambio pending (m_dirty true). In idle, zero paint.
constexpr int kWorldMapRepaintMs = 250;

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

    // 1.0.212 — Su Qt 6 RHI un QWidget mai mostrato puo' non avere lo state
    // necessario per render() off-screen (style/policy/pipeline non
    // inizializzati). Set DontShowOnScreen + show() prepara il widget per
    // il rendering senza farlo apparire fisicamente sullo schermo.
    m_widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    m_widget.setAttribute(Qt::WA_TranslucentBackground, false);
    m_widget.resize(1280, 720);
    m_widget.show();
    m_widget.ensurePolished();
    // 1.0.213 — RIMOSSO setTextureSize(1280x720) ereditato da 1.0.156.
    // Il cap forzava il backing buffer di QQuickPaintedItem a 1280x720,
    // ma con drawImage(0,0,QImage(w_effettivo,h_effettivo)) si finiva
    // per copiare solo il top-left 1280x720 dell'immagine (resto clippato)
    // = utente vedeva solo l'angolo della mappa = sembrava vuota.
    // Senza cap, il backing scala con l'item e drawImage riesce a copiare
    // l'intera immagine renderizzata dal widget legacy.

    connect(&m_widget, &WorldMapWidget::contactClicked,
            this, &WorldMapItem::contactClicked);

    connect(this, &QQuickItem::widthChanged, this, [this]() {
        syncWidgetSize();
        markDirty();
    });
    connect(this, &QQuickItem::heightChanged, this, [this]() {
        syncWidgetSize();
        markDirty();
    });
    connect(this, &QQuickItem::visibleChanged, this, [this]() {
        if (isVisible()) {
            syncWidgetSize();
            markDirty();
        }
    });

    m_repaintTimer.setInterval(kWorldMapRepaintMs);
    connect(&m_repaintTimer, &QTimer::timeout, this, [this]() {
        // 1.0.209 — Solo se visibile E dirty. Risparmia il 50% CPU in idle
        // (era timer 60ms always-on che chiamava QWidget::render full anche
        // quando la mappa non era cambiata).
        if (isVisible() && m_dirty) {
            m_dirty = false;
            update();
        }
    });
    m_repaintTimer.start();
}

void WorldMapItem::markDirty()
{
    m_dirty = true;
    // 1.0.210 — Se visibile, chiama subito update() per primo paint
    // istantaneo. Il timer 250ms resta come coalesce backup per evitare
    // burst >4Hz quando arrivano molti addContact ravvicinati: il timer
    // tick fa update solo se m_dirty true (ancora marcato dopo paint).
    if (isVisible()) {
        update();
    }
}

void WorldMapItem::setHomeGrid(const QString& grid)
{
    m_widget.setHomeGrid(grid);
    markDirty();
}

void WorldMapItem::setGreylineEnabled(bool enabled)
{
    m_widget.setGreylineEnabled(enabled);
    markDirty();
}

void WorldMapItem::setDistanceInMiles(bool enabled)
{
    m_widget.setDistanceInMiles(enabled);
    markDirty();
}

void WorldMapItem::setTransmitState(bool transmitting,
                                    const QString& targetCall,
                                    const QString& targetGrid,
                                    const QString& mode)
{
    m_widget.setTransmitState(transmitting, targetCall, targetGrid, mode);
    markDirty();
}

void WorldMapItem::clearContacts()
{
    m_widget.clearContacts();
    markDirty();
}

void WorldMapItem::downgradeContactToBand(const QString& call)
{
    m_widget.downgradeContactToBand(call);
    markDirty();
}

void WorldMapItem::addContact(const QString& call,
                              const QString& sourceGrid,
                              const QString& destinationGrid,
                              int role)
{
    m_widget.addContact(call, sourceGrid, destinationGrid, pathRoleFromInt(role));
    markDirty();
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
    markDirty();
}

void WorldMapItem::paint(QPainter* painter)
{
    if (!painter) {
        return;
    }

    syncWidgetSize();
    // 1.0.213e — Approccio finale: render direct via render(painter) senza
    // flag (default DrawWindowBackground | DrawChildren). Niente FBO,
    // niente offscreen QImage, niente fillColor. Il widget legacy disegna
    // direttamente sul painter del QQuickPaintedItem.
    m_widget.render(painter);
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
