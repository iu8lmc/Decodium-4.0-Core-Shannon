#pragma once

#include <QQuickPaintedItem>
#include <QTimer>

#include "widgets/worldmapwidget.h"

class WorldMapItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(bool gpuAccelerated READ gpuAccelerated CONSTANT)
    Q_PROPERTY(bool lowSpecMode READ lowSpecMode CONSTANT)

public:
    explicit WorldMapItem(QQuickItem* parent = nullptr);

    bool gpuAccelerated() const { return m_gpuAccelerated; }
    bool lowSpecMode() const { return m_lowSpecMode; }

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
    // 1.0.213 — Permette a LiveMapPanel di sospendere l'animation timer
    // interno del widget legacy quando il pannello non e' visibile
    // (Pop minimizzato, tab non selezionato, finestra detach nascosta).
    Q_INVOKABLE void setActive(bool active);

    void paint(QPainter* painter) override;

Q_SIGNALS:
    void contactClicked(const QString& call, const QString& grid);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void itemChange(ItemChange change, const ItemChangeData& data) override;

private:
    static WorldMapWidget::PathRole pathRoleFromInt(int role);
    void syncWidgetSize();
    void markDirty();

    // 1.0.213 — Rileva GPU/CPU/RAM e configura render target + intervalli.
    // Chiamata una sola volta quando il QQuickItem ottiene la finestra
    // (il backend RHI e' affidabile solo dopo windowChanged).
    void configureForHardware();
    void applyAnimationCadence(bool visible);
    void handlePaintProfile(double paintMs, double paintAvgMs,
                            double greylineMs, double greylineAvgMs,
                            int contactsCount, bool cacheRebuild);
    void setRepaintIntervalMs(int ms, const QString& reason,
                              double paintMs = 0.0, double paintAvgMs = 0.0,
                              double steadyPaintMs = 0.0, double steadyPaintAvgMs = 0.0,
                              int contactsCount = 0, bool cacheRebuild = false);

    WorldMapWidget m_widget;
    QTimer m_repaintTimer;
    bool m_dirty {true};
    bool m_hardwareConfigured {false};
    bool m_gpuAccelerated {false};
    bool m_lowSpecMode {false};
    bool m_userActive {true};
    int m_baseRepaintIntervalMs {250};
    int m_repaintIntervalMs {250};
    int m_animationIntervalActiveMs {60};
    int m_fastPaintProfileCount {0};
};
