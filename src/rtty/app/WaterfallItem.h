// DecoRTTY — scrolling waterfall with the RTTY tone markers drawn on it.
//
// Implemented as a painted item over a scrolling QImage: one new row per
// spectrum update, the rest blitted down. That is far cheaper than repainting a
// history of lines from a model every frame, and it keeps the whole display
// inside one texture upload.
#pragma once

#include <QColor>
#include <QImage>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>
#include <QVector>

namespace decortty::app {

class WaterfallItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(Waterfall)

    Q_PROPERTY(double minDb READ minDb WRITE setMinDb NOTIFY rangeChanged)
    Q_PROPERTY(double maxDb READ maxDb WRITE setMaxDb NOTIFY rangeChanged)
    // The visible slice of the spectrum, in hertz.
    Q_PROPERTY(double startHz READ startHz WRITE setStartHz NOTIFY rangeChanged)
    Q_PROPERTY(double endHz READ endHz WRITE setEndHz NOTIFY rangeChanged)
    // Tone markers.
    Q_PROPERTY(double markHz READ markHz WRITE setMarkHz NOTIFY markersChanged)
    Q_PROPERTY(double shiftHz READ shiftHz WRITE setShiftHz NOTIFY markersChanged)
    Q_PROPERTY(QColor markerColor READ markerColor WRITE setMarkerColor NOTIFY markersChanged)
    Q_PROPERTY(int colorMap READ colorMap WRITE setColorMap NOTIFY colorMapChanged)
    // Il notch, dove e' inserito: sulla cascata si vede la fetta di banda tolta
    // di mezzo, e si capisce a colpo d'occhio se e' finita sulla portante
    // giusta o su un tono del corrispondente.
    Q_PROPERTY(double notchHz READ notchHz WRITE setNotchHz NOTIFY markersChanged)
    Q_PROPERTY(bool notchEnabled READ notchEnabled WRITE setNotchEnabled NOTIFY markersChanged)

public:
    explicit WaterfallItem(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;

    double minDb() const { return m_minDb; }
    double maxDb() const { return m_maxDb; }
    double startHz() const { return m_startHz; }
    double endHz() const { return m_endHz; }
    double markHz() const { return m_markHz; }
    double shiftHz() const { return m_shiftHz; }
    QColor markerColor() const { return m_markerColor; }
    int    colorMap() const { return m_colorMap; }
    double notchHz() const { return m_notchHz; }
    bool   notchEnabled() const { return m_notchEnabled; }

    void setMinDb(double db);
    void setMaxDb(double db);
    void setStartHz(double hz);
    void setEndHz(double hz);
    void setMarkHz(double hz);
    void setShiftHz(double hz);
    void setMarkerColor(const QColor& colour);
    void setColorMap(int colorMap);
    void setNotchHz(double hz);
    void setNotchEnabled(bool on);

    // Push one spectrum. `binHz` is the width of a bin so the item can map
    // frequency to column without being told the FFT size.
    Q_INVOKABLE void pushSpectrum(const QVector<double>& spectrumDb, double binHz);
    Q_INVOKABLE void clear();

    // Frequency under a click, for tuning by pointing at the signal.
    Q_INVOKABLE double frequencyAt(double x) const;

signals:
    void rangeChanged();
    void markersChanged();
    void colorMapChanged();

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    QRgb  colourFor(double db) const;
    void  rebuildImage();

    QImage m_image;
    double m_minDb{-110.0};
    double m_maxDb{-30.0};
    double m_startHz{500.0};
    double m_endHz{3000.0};
    double m_markHz{2125.0};
    double m_shiftHz{170.0};
    QColor m_markerColor{QStringLiteral("#00FF88")};
    int    m_colorMap{0};
    double m_notchHz{0.0};
    bool   m_notchEnabled{false};
};

} // namespace decortty::app
