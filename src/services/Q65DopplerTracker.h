#pragma once

#include <QObject>
#include <QDateTime>
#include <QVariantMap>
#include <QVector>

// CAT-independent policy/state machine. It never keys a transmitter. The
// bridge applies the returned absolute logical dials through its calibration
// and Hamlib split paths, not through the user's nominal-frequency setter.
class Q65DopplerTracker final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY stateChanged)
    Q_PROPERTY(int method READ method WRITE setMethod NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY stateChanged)
public:
    enum Method { ConstantOnMoon = 0, FullDoppler = 1, OwnEcho = 2 };
    Q_ENUM(Method)
    struct Context {
        QString mode, myGrid, dxGrid;
        double nominalHz = 0;
        int periodMs = 60000;
        bool connected = false, supported = false, split = false;
        bool monitoring = false, transmitting = false, conflict = false;
    };
    struct Action {
        bool tune = false;
        bool restore = false;
        double rxHz = 0, txHz = 0;
    };
    explicit Q65DopplerTracker(QObject* parent = nullptr) : QObject(parent) {}
    bool enabled() const { return m_enabled; }
    int method() const { return m_method; }
    bool applied() const { return m_applied; }
    double txDialHz() const { return m_txDialHz; }
    QVariantMap snapshot() const { return m_snapshot; }
    void setEnabled(bool enabled);
    void setMethod(int method);
    Action update(Context const&, QDateTime const& utc);
    // Suppress echoes of our own CAT commands (including delayed polls and
    // Fake It TX reports). Unrelated manual tuning disarms the tracker.
    bool consumeFrequencyReport(double logicalHz, qint64 nowMs);
    void disconnected();
    static bool validGrid(QString const& grid);
    static QPair<double, double> offsets(int method, double ownOneWayHz, double dxOneWayHz);
    static QDateTime txMidpoint(QDateTime const& utc, int periodMs);
signals:
    void stateChanged();
    void configurationChanged();
private:
    void remember(double hz, qint64 nowMs);
    bool m_enabled = false, m_applied = false, m_ready = false, m_busy = false;
    int m_method = ConstantOnMoon;
    double m_nominalHz = 0, m_rxDialHz = 0, m_txDialHz = 0;
    struct Report { double hz; qint64 expires; };
    QVector<Report> m_reports;
    QVariantMap m_snapshot;
};
