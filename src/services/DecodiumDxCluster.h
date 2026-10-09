#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QList>
#include <QPair>
#include <QTcpSocket>
#include <QSettings>

#include <QAbstractListModel>

class QTimer;

// Gli spot come modello a righe, per le liste QML. Leggere la proprieta' spots
// ricopia in JavaScript l'intero elenco (fino a 200 spot) e una ListView che
// la usa come modello ricostruisce tutte le righe a ogni spot nuovo: 100-180 ms
// di thread dell'interfaccia ogni volta, che durante un pile-up facevano
// perdere campioni alla cattura audio. Qui una riga si aggiunge in fondo e
// la piu' vecchia si toglie in testa, senza toccare le altre.
// Un solo ruolo, "modelData": il delegato legge modelData["dxCall"] ecc.
class DxClusterSpotModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    explicit DxClusterSpotModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
    }
    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
            return {};
        return role == SpotRole || role == Qt::DisplayRole ? m_rows.at(index.row()) : QVariant();
    }
    QHash<int, QByteArray> roleNames() const override { return {{SpotRole, "modelData"}}; }

    void append(const QVariantMap& spot)
    {
        int const row = static_cast<int>(m_rows.size());
        beginInsertRows(QModelIndex(), row, row);
        m_rows.append(spot);
        endInsertRows();
        emit countChanged();
    }
    void removeAt(int row)
    {
        if (row < 0 || row >= m_rows.size())
            return;
        beginRemoveRows(QModelIndex(), row, row);
        m_rows.removeAt(row);
        endRemoveRows();
        emit countChanged();
    }
    void clear()
    {
        if (m_rows.isEmpty())
            return;
        beginResetModel();
        m_rows.clear();
        endResetModel();
        emit countChanged();
    }

signals:
    void countChanged();

private:
    enum { SpotRole = Qt::UserRole + 1 };
    QList<QVariant> m_rows;
};

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
    Q_PROPERTY(QString     lastStatus READ lastStatus NOTIFY lastStatusChanged)
    Q_PROPERTY(QVariantList spots    READ spots      NOTIFY spotsChanged)
    Q_PROPERTY(QObject*    spotModel READ spotModel  CONSTANT)
    Q_PROPERTY(bool        offlineMode READ offlineMode WRITE setOfflineMode NOTIFY offlineModeChanged)

public:
    explicit DecodiumDxCluster(QObject* parent = nullptr);
    ~DecodiumDxCluster() override;

    static QString bandLabelFromFrequencyKhz(double freqKhz);

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
        if (m_callsign != v) { m_callsign = v; emit callsignChanged(); saveSettings(); }
    }
    QString     lastStatus() const { return m_lastStatus; }

    QVariantList spots() const { return m_spots; }
    QObject*    spotModel() const { return m_spotModel; }
    bool offlineMode() const { return m_offlineMode; }
    void setOfflineMode(bool offline);

    // --- persistence ---
    void saveSettings();
    void loadSettings();

public slots:
    Q_INVOKABLE void connectCluster();
    Q_INVOKABLE void disconnectCluster();
    // Send a raw command string (e.g. "SH/DX 30", "BYE").
    Q_INVOKABLE void sendCommand(const QString& cmd);
    Q_INVOKABLE bool sendSpot(const QString& dxCall, double freqKhz, const QString& comment = QString());
    Q_INVOKABLE bool submitSpotVerified(const QString& dxCall, double freqKhz, const QString& comment = QString());
    Q_INVOKABLE void clearSpots();
    // Uno spot che arriva da un'altra fonte (il cluster di DecoLog): stesse chiavi
    // di newSpot. Lo stesso DX sulla stessa banda e modo sostituisce la riga vecchia.
    void injectSpot(const QVariantMap& spot);

signals:
    void connectedChanged();
    void hostChanged();
    void portChanged();
    void callsignChanged();
    void spotsChanged();
    void lastStatusChanged();
    // Emitted for every successfully parsed spot.
    // Keys: dxCall, frequency, spotter, comment, time, band, mode
    void newSpot(const QVariantMap& spot);
    void statusUpdate(const QString& msg);
    void errorOccurred(const QString& msg);
    void offlineModeChanged();

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);

private:
    void        ensureSocket();
    void        sendLogin();
    void        scheduleReconnect(const QString& reason);
    bool        tryNextConnectionCandidate(const QString& failureReason);
    QList<QPair<QString, int>> buildConnectionCandidates(const QString& host, int port) const;
    QString     endpointLabel(const QString& host, int port) const;
    // Process one complete line received from the cluster.
    void processLine(const QString& line);
    // Parse a "DX de …" line into a QVariantMap.
    QVariantMap parseSpotLine(const QString& line) const;
    // Derive amateur band string from frequency in kHz.
    QString     bandFromFreq(double freqKhz) const;
    void        setLastStatus(const QString& msg);
    void        scheduleSpotsChanged(int delayMs = 200);
    // Ogni modifica a m_spots passa da qui, cosi' il modello resta allineato.
    void        appendSpot(const QVariantMap& spot);
    void        removeSpotAt(int index);

    static constexpr int k_maxSpots = 200;

    QTcpSocket*  m_socket    {nullptr};
    QString      m_rxBuf;
    bool         m_connected  {false};
    bool         m_loginSent  {false};
    QString      m_host      {"dx.iz7auh.net"};
    int          m_port      {8000};
    QString      m_callsign;
    QString      m_lastStatus;
    QVariantList m_spots;      // newest spot is appended at the back
    DxClusterSpotModel* m_spotModel {nullptr};   // stesse righe di m_spots
    QList<QPair<QString, int>> m_connectionCandidates;
    int          m_connectionCandidateIndex {-1};
    QString      m_activeHost;
    int          m_activePort {0};
    bool         m_connectSequenceActive {false};
    bool         m_ignoreNextSocketError {false};
    bool         m_manualDisconnect {false};
    bool         m_offlineMode {false};
    QTimer*      m_connectTimeoutTimer {nullptr};
    QTimer*      m_refreshTimer {nullptr};
    QTimer*      m_reconnectTimer {nullptr};
    QTimer*      m_spotsChangedTimer {nullptr};
    int          m_pendingSpotsChangedCount {0};
};
