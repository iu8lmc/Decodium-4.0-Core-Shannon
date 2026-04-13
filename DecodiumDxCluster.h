#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QTcpSocket>
#include <QSettings>

// Full DX Cluster client (telnet ASCII, port 7300 or 23).
// Replaces the empty DxClusterManager stub in DecodiumSubManagers.h.
// Register with the QML engine as a singleton or as a context property.
class DecodiumDxCluster : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool        connected READ connected  NOTIFY connectedChanged)
    Q_PROPERTY(QString     host      READ host       WRITE setHost       NOTIFY hostChanged)
    Q_PROPERTY(int         port      READ port       WRITE setPort       NOTIFY portChanged)
    Q_PROPERTY(QString     callsign  READ callsign   WRITE setCallsign   NOTIFY callsignChanged)
    Q_PROPERTY(QVariantList spots    READ spots      NOTIFY spotsChanged)

public:
    explicit DecodiumDxCluster(QObject* parent = nullptr);
    ~DecodiumDxCluster() override;

    // --- property accessors ---
    bool        connected() const { return m_connected; }

    QString     host()      const { return m_host; }
    void        setHost(const QString& v)
    {
        if (m_host != v) { m_host = v; emit hostChanged(); saveSettings(); }
    }

    int         port()      const { return m_port; }
    void        setPort(int v)
    {
        if (m_port != v) { m_port = v; emit portChanged(); saveSettings(); }
    }

    QString     callsign()  const { return m_callsign; }
    void        setCallsign(const QString& v)
    {
        if (m_callsign != v) { m_callsign = v; emit callsignChanged(); }
    }

    QVariantList spots() const { return m_spots; }

    // --- persistence ---
    void saveSettings();
    void loadSettings();

public slots:
    Q_INVOKABLE void connectCluster();
    Q_INVOKABLE void disconnectCluster();
    // Send a raw command string (e.g. "SH/DX 30", "BYE").
    Q_INVOKABLE void sendCommand(const QString& cmd);
    Q_INVOKABLE void clearSpots();

signals:
    void connectedChanged();
    void hostChanged();
    void portChanged();
    void callsignChanged();
    void spotsChanged();
    // Emitted for every successfully parsed spot.
    // Keys: dxCall, frequency, spotter, comment, time, band, mode
    void newSpot(const QVariantMap& spot);
    void statusUpdate(const QString& msg);
    void errorOccurred(const QString& msg);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);

private:
    // Process one complete line received from the cluster.
    void processLine(const QString& line);
    // Parse a "DX de …" line into a QVariantMap.
    QVariantMap parseSpotLine(const QString& line) const;
    // Derive amateur band string from frequency in kHz.
    QString     bandFromFreq(double freqKhz) const;

    static constexpr int k_maxSpots = 200;

    QTcpSocket*  m_socket    {nullptr};
    QString      m_rxBuf;
    bool         m_connected  {false};
    bool         m_loginSent  {false};
    QString      m_host      {"dx.iz7auh.net"};
    int          m_port      {8000};
    QString      m_callsign;
    QVariantList m_spots;      // newest spot is appended at the back
};
