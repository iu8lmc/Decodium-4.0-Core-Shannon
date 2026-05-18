#include "WorldMapItem.hpp"

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QThread>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {

// 1.0.213 — Cadenze tre-livello scelte dinamicamente a runtime in base
// alla detection hardware:
//   GPU full      -> 80ms  (~12Hz, scene-graph blitta texture FBO, costo basso)
//   GPU low-mem   -> 160ms (~6Hz, riduce upload texture su VRAM scarsa)
//   CPU/Software  -> 333ms (~3Hz, blit raster pesante: meno frame, piu' fluido)
// Animation phase widget legacy:
//   synced to the effective repaint cap so hidden/coalesced animation ticks do
//   not burn CPU without producing visible frames.
constexpr int kRepaintGpuFullMs = 80;
constexpr int kRepaintGpuLowMs = 160;
constexpr int kRepaintCpuMs = 333;
constexpr double kPaintStepDownMs = 12.0;
constexpr double kPaintStepDownAvgMs = 8.0;
constexpr double kPaintSevereMs = 24.0;
constexpr double kPaintSevereAvgMs = 16.0;
constexpr int kPaintRecoveryProfiles = 3;

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
    // 1.0.213 — Pausato all'avvio. L'attivazione vera arriva da QML
    // setActive(true) o dal primo visibleChanged.
    m_widget.setAnimationActive(false);

    connect(&m_widget, &WorldMapWidget::contactClicked,
            this, &WorldMapItem::contactClicked);
    // 1.0.214 — propaga l'animation tick dal widget legacy al QQuickItem
    // (greyline pulse, tx travel arc, contact age fade). markDirty fa
    // throttle vero: max 1 paint per m_repaintIntervalMs.
    connect(&m_widget, &WorldMapWidget::repaintRequested,
            this, &WorldMapItem::markDirty);
    connect(&m_widget, &WorldMapWidget::paintProfileUpdated,
            this, &WorldMapItem::handlePaintProfile);

    connect(this, &QQuickItem::widthChanged, this, [this]() {
        syncWidgetSize();
        markDirty();
    });
    connect(this, &QQuickItem::heightChanged, this, [this]() {
        syncWidgetSize();
        markDirty();
    });
    connect(this, &QQuickItem::visibleChanged, this, [this]() {
        applyAnimationCadence(isVisible());
        if (isVisible()) {
            syncWidgetSize();
            markDirty();
        }
    });

    // Default conservativo; configureForHardware() lo aggiorna appena
    // il QQuickItem entra in una finestra (windowChanged signal).
    m_repaintTimer.setInterval(m_repaintIntervalMs);
    connect(&m_repaintTimer, &QTimer::timeout, this, [this]() {
        if (isVisible() && m_dirty) {
            m_dirty = false;
            update();
        }
    });
    m_repaintTimer.start();
}

void WorldMapItem::markDirty()
{
    // 1.0.214 — throttle vero. Pre-1.0.214 markDirty() chiamava update()
    // immediato ad ogni invocazione: durante burst FT8 (5-25 decode per
    // slot ognuno scatena markDirty via addContact) lo scene-graph
    // schedulava paint() ad ogni frame = 60 widget.render() al secondo
    // = scattosita' anche con GPU+FBO+cache.
    //
    // Ora: solo il primo markDirty di un ciclo "clean" schedula update()
    // immediato (preserva 1.0.210 first-paint reattivo). Markup successivi
    // settano m_dirty=true ma non re-update: il prossimo tick del
    // m_repaintTimer (80/160/333ms) prendera' il clear+update unico.
    // Rate paint cap effettivo: 1 / m_repaintIntervalMs.
    bool const wasDirty = m_dirty;
    m_dirty = true;
    if (isVisible() && !wasDirty) {
        update();
    }
}

void WorldMapItem::setActive(bool active)
{
    m_userActive = active;
    applyAnimationCadence(active && isVisible());
    if (active && isVisible()) {
        markDirty();
    }
}

void WorldMapItem::applyAnimationCadence(bool visible)
{
    // 1.0.215 — sync anim interval con il paint rate cap. Pre-1.0.215
    // anim girava a 60ms (16Hz) mentre paint rate era capped a 80ms
    // (12.5Hz GPU full) o 333ms (CPU): l'anim tick eccedente NON causava
    // paint visibile = lavoro sprecato (computeCircularLongitudeBounds +
    // smoothViewport + emit signal coalesced dal markDirty throttle).
    // Floor a 60ms per non degradare percezione fluida di greyline.
    m_animationIntervalActiveMs = qMax(60, m_repaintIntervalMs);
    m_widget.setAnimationInterval(m_animationIntervalActiveMs);
    m_widget.setAnimationActive(visible);
}

void WorldMapItem::setRepaintIntervalMs(int ms, const QString& reason,
                                        double paintMs, double paintAvgMs,
                                        double steadyPaintMs, double steadyPaintAvgMs,
                                        int contactsCount, bool cacheRebuild)
{
    int const clamped = qBound(kRepaintGpuFullMs, ms, kRepaintCpuMs);
    if (m_repaintIntervalMs == clamped) {
        return;
    }

    m_repaintIntervalMs = clamped;
    m_repaintTimer.setInterval(m_repaintIntervalMs);
    applyAnimationCadence(m_userActive && isVisible());

    qInfo().nospace()
        << "[WorldMapItem] adaptive repaint interval_ms=" << m_repaintIntervalMs
        << " reason=" << reason
        << " paint_ms=" << QString::number(paintMs, 'f', 2)
        << " paint_avg_ms=" << QString::number(paintAvgMs, 'f', 2)
        << " steady_paint_ms=" << QString::number(steadyPaintMs, 'f', 2)
        << " steady_avg_ms=" << QString::number(steadyPaintAvgMs, 'f', 2)
        << " contacts=" << contactsCount
        << " cache_rebuild=" << (cacheRebuild ? 1 : 0)
        << " animMs=" << (m_userActive && isVisible() ? m_animationIntervalActiveMs : 0)
        << " animActive=" << (m_userActive && isVisible());
}

void WorldMapItem::handlePaintProfile(double paintMs, double paintAvgMs,
                                      double greylineMs, double greylineAvgMs,
                                      int contactsCount, bool cacheRebuild)
{
    if (!m_gpuAccelerated || m_baseRepaintIntervalMs >= kRepaintCpuMs) {
        return;
    }

    // Cache rebuilds, especially the 5s greyline refresh, are expected
    // one-off costs. Throttle on the steady paint cost so a single 25-30ms
    // cache refresh does not make the live map feel permanently slow.
    double const steadyPaintMs = cacheRebuild ? qMax(0.0, paintMs - greylineMs) : paintMs;
    double const steadyPaintAvgMs = cacheRebuild
        ? qMax(0.0, paintAvgMs - greylineAvgMs)
        : paintAvgMs;

    int targetInterval = m_baseRepaintIntervalMs;
    QString reason;
    if (steadyPaintMs >= kPaintSevereMs || steadyPaintAvgMs >= kPaintSevereAvgMs) {
        targetInterval = kRepaintCpuMs;
        reason = QStringLiteral("paint-severe");
    } else if (steadyPaintMs >= kPaintStepDownMs || steadyPaintAvgMs >= kPaintStepDownAvgMs) {
        targetInterval = qMax(kRepaintGpuLowMs, m_baseRepaintIntervalMs);
        reason = QStringLiteral("paint-pressure");
    }

    if (targetInterval > m_repaintIntervalMs) {
        m_fastPaintProfileCount = 0;
        setRepaintIntervalMs(targetInterval, reason, paintMs, paintAvgMs,
                             steadyPaintMs, steadyPaintAvgMs,
                             contactsCount, cacheRebuild);
        return;
    }

    if (targetInterval == m_baseRepaintIntervalMs && m_repaintIntervalMs > m_baseRepaintIntervalMs) {
        ++m_fastPaintProfileCount;
        if (m_fastPaintProfileCount >= kPaintRecoveryProfiles) {
            int const recoveredInterval = (m_repaintIntervalMs >= kRepaintCpuMs)
                ? qMax(kRepaintGpuLowMs, m_baseRepaintIntervalMs)
                : m_baseRepaintIntervalMs;
            m_fastPaintProfileCount = 0;
            setRepaintIntervalMs(recoveredInterval, QStringLiteral("paint-recovered"),
                                 paintMs, paintAvgMs,
                                 steadyPaintMs, steadyPaintAvgMs,
                                 contactsCount, cacheRebuild);
        }
    } else {
        m_fastPaintProfileCount = 0;
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
    // Render diretto del widget legacy. Con renderTarget=FramebufferObject
    // (GPU mode) Qt Quick blitta la texture FBO senza ridipingere finche'
    // !m_dirty: il widget legacy stesso pero' fa cache layered del background
    // (earth+overlay+grid) -> il render() reale e' molto piu' leggero del
    // pre-1.0.213 anche in dirty case.
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

void WorldMapItem::itemChange(ItemChange change, const ItemChangeData& data)
{
    if (change == ItemSceneChange && data.window && !m_hardwareConfigured) {
        configureForHardware();
    }
    QQuickPaintedItem::itemChange(change, data);
}

void WorldMapItem::configureForHardware()
{
    m_hardwareConfigured = true;

    QSGRendererInterface::GraphicsApi api = QSGRendererInterface::Unknown;
    if (auto* win = window()) {
        if (auto* iface = win->rendererInterface()) {
            api = iface->graphicsApi();
        }
    }
    if (api == QSGRendererInterface::Unknown) {
        api = QQuickWindow::graphicsApi();
    }

    m_gpuAccelerated = (api != QSGRendererInterface::Software
                        && api != QSGRendererInterface::Unknown);

    int const cores = qMax(1, QThread::idealThreadCount());
    bool lowMemory = false;
    quint64 totalRamMb = 0;
#ifdef Q_OS_WIN
    MEMORYSTATUSEX mem;
    ZeroMemory(&mem, sizeof(mem));
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        totalRamMb = mem.ullTotalPhys / (1024ull * 1024ull);
        // <= 4 GB totale = low memory
        lowMemory = totalRamMb <= 4096ull;
    }
#endif

    m_lowSpecMode = (cores < 4) || lowMemory || !m_gpuAccelerated;

    // Adapt render target + perf hints
    if (m_gpuAccelerated) {
        setRenderTarget(QQuickPaintedItem::FramebufferObject);
        setPerformanceHint(QQuickPaintedItem::FastFBOResizing, true);
        setMipmap(false);
        m_baseRepaintIntervalMs = m_lowSpecMode ? kRepaintGpuLowMs : kRepaintGpuFullMs;
    } else {
        setRenderTarget(QQuickPaintedItem::Image);
        m_baseRepaintIntervalMs = kRepaintCpuMs;
    }
    m_repaintIntervalMs = m_baseRepaintIntervalMs;
    m_repaintTimer.setInterval(m_repaintIntervalMs);

    // Apply animation cadence if already visible
    applyAnimationCadence(m_userActive && isVisible());

    qInfo().nospace()
        << "[WorldMapItem] hw: gpuApi=" << int(api)
        << " gpuAccel=" << m_gpuAccelerated
        << " cores=" << cores
        << " ramMB=" << totalRamMb
        << " lowSpec=" << m_lowSpecMode
        << " repaintMs=" << m_repaintIntervalMs
        << " animMs=" << (m_userActive && isVisible() ? m_animationIntervalActiveMs : 0)
        << " animActive=" << (m_userActive && isVisible());
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
