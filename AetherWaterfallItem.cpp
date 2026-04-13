#include "AetherWaterfallItem.h"
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QCoreApplication>

AetherWaterfallItem::AetherWaterfallItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setAntialiasing(false);

    connect(&m_wf, &AetherWaterfall::frequencyClicked, this, &AetherWaterfallItem::frequencyClicked);

    // Default freq range per FT8/FT2
    m_wf.setFrequencyRange(200, 4000);
    m_wf.setRefLevel(-50);
    m_wf.setDynRange(90);
}

void AetherWaterfallItem::addRow(const QVector<float>& dbValues, float minDb, float maxDb,
                                  float freqMinHz, float freqMaxHz)
{
    if (freqMinHz > 0 && freqMaxHz > freqMinHz)
        m_wf.setFrequencyRange(freqMinHz, freqMaxHz);

    // Assicurati che il widget interno abbia le dimensioni corrette
    int w = static_cast<int>(width());
    int h = static_cast<int>(height());
    if (w > 0 && h > 0) {
        if (m_wf.width() != w || m_wf.height() != h) {
            m_wf.resize(w, h);
            // Forza il resize event manualmente (il widget non è visibile)
            QResizeEvent re(QSize(w, h), m_wf.size());
            QCoreApplication::sendEvent(&m_wf, &re);
        }
    }

    m_wf.addRow(dbValues, minDb, maxDb);
    update();
}

void AetherWaterfallItem::paint(QPainter* painter)
{
    int w = static_cast<int>(width());
    int h = static_cast<int>(height());
    if (w <= 0 || h <= 0) return;

    // Forza dimensioni widget (necessario perché non è visibile)
    if (m_wf.width() != w || m_wf.height() != h) {
        m_wf.resize(w, h);
        // Il widget non è visibile, forza resize manualmente
        m_wf.setMinimumSize(w, h);
        m_wf.setMaximumSize(w, h);
    }

    // Usa QWidget::render() che dipinge il widget su un painter/device
    // Questo funziona anche per widget non visibili
    m_wf.render(painter, QPoint(0, 0), QRegion(), QWidget::DrawChildren | QWidget::DrawWindowBackground);
}

void AetherWaterfallItem::mousePressEvent(QMouseEvent* ev)
{
    if (!ev) return;
    // Delega al widget interno per il calcolo della frequenza
    int w = static_cast<int>(width());
    if (w <= 0) return;

    float minHz = 200, maxHz = 4000;
    float clickFreq = minHz + (maxHz - minHz) * ev->position().x() / w;

    if (ev->button() == Qt::LeftButton)
        emit frequencyClicked(static_cast<int>(clickFreq));
}
