#pragma once
// AetherWaterfallItem — QQuickPaintedItem wrapper around AetherWaterfall
// Permette l'uso del waterfall AetherSDR in QML come item nativo.
#include <QQuickPaintedItem>
#include <QVector>
#include <QImage>
#include "AetherWaterfall.h"

class AetherWaterfallItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(int colorScheme READ colorScheme WRITE setColorScheme NOTIFY colorSchemeChanged)
    Q_PROPERTY(bool autoBlack READ autoBlack WRITE setAutoBlack NOTIFY autoBlackChanged)
    Q_PROPERTY(bool blankerEnabled READ blankerEnabled WRITE setBlankerEnabled NOTIFY blankerEnabledChanged)
    Q_PROPERTY(float blankerThreshold READ blankerThreshold WRITE setBlankerThreshold NOTIFY blankerThresholdChanged)
    Q_PROPERTY(float refLevel READ refLevel WRITE setRefLevel NOTIFY refLevelChanged)
    Q_PROPERTY(float dynRange READ dynRange WRITE setDynRange NOTIFY dynRangeChanged)
    Q_PROPERTY(int rxFrequency READ rxFrequency WRITE setRxFrequency NOTIFY rxFrequencyChanged)
    Q_PROPERTY(int txFrequency READ txFrequency WRITE setTxFrequency NOTIFY txFrequencyChanged)
    Q_PROPERTY(QStringList schemeNames READ schemeNames CONSTANT)

public:
    explicit AetherWaterfallItem(QQuickItem* parent = nullptr);

    int   colorScheme()      const { return m_wf.colorScheme(); }
    bool  autoBlack()        const { return m_wf.autoBlack(); }
    bool  blankerEnabled()   const { return m_wf.blankerEnabled(); }
    float blankerThreshold() const { return m_wf.blankerThreshold(); }
    float refLevel()         const { return m_refLevel; }
    float dynRange()         const { return m_dynRange; }
    int   rxFrequency()      const { return m_rxFreq; }
    int   txFrequency()      const { return m_txFreq; }
    QStringList schemeNames() const { return {"AetherSDR Default","Grayscale","BlueGreen","Fire","Plasma"}; }

    void setColorScheme(int v)       { m_wf.setColorScheme(v); emit colorSchemeChanged(); update(); }
    void setAutoBlack(bool v)        { m_wf.setAutoBlack(v); emit autoBlackChanged(); }
    void setBlankerEnabled(bool v)   { m_wf.setBlankerEnabled(v); emit blankerEnabledChanged(); }
    void setBlankerThreshold(float v){ m_wf.setBlankerThreshold(v); emit blankerThresholdChanged(); }
    void setRefLevel(float v)        { m_refLevel = v; m_wf.setRefLevel(v); emit refLevelChanged(); update(); }
    void setDynRange(float v)        { m_dynRange = v; m_wf.setDynRange(v); emit dynRangeChanged(); update(); }
    void setRxFrequency(int v)       { m_rxFreq = v; m_wf.setRxFrequency(v); update(); emit rxFrequencyChanged(); }
    void setTxFrequency(int v)       { m_txFreq = v; m_wf.setTxFrequency(v); update(); emit txFrequencyChanged(); }

    // Chiamato dal bridge con dati FFT
    Q_INVOKABLE void addRow(const QVector<float>& dbValues, float minDb, float maxDb,
                            float freqMinHz = 0.f, float freqMaxHz = 0.f);

    void paint(QPainter* painter) override;

signals:
    void colorSchemeChanged();
    void autoBlackChanged();
    void blankerEnabledChanged();
    void blankerThresholdChanged();
    void refLevelChanged();
    void dynRangeChanged();
    void rxFrequencyChanged();
    void txFrequencyChanged();
    void frequencyClicked(int freq);

protected:
    void mousePressEvent(QMouseEvent* ev) override;

private:
    AetherWaterfall m_wf;
    float m_refLevel = -80.f;
    float m_dynRange = 80.f;
    int   m_rxFreq = 1500;
    int   m_txFreq = 1500;
};
