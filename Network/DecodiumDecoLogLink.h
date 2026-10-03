#ifndef DECODIUMDECOLOGLINK_H
#define DECODIUMDECOLOGLINK_H

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

class QTcpSocket;

// ---------------------------------------------------------------------------
// DecodiumDecoLogLink
//
// Client di DecoLink, il canale locale con DecoLog (protocollo v1, descritto in
// decolog/docs/DECOLINK.md). JSON su TCP, un messaggio per riga, solo verso
// 127.0.0.1.
//
// I QSO continuano ad andare a DecoLog via UDP (LoggedADIF), come a qualunque
// logger. DecoLink porta l'altra direzione: il log di DecoLog (chi e' stato
// lavorato e confermato, anche fuori dal file ADIF di Decodium), lo stato
// dell'FT2 Award e la conferma che il QSO appena fatto e' davvero nel log.
//
// Come DecodiumCloudlogLite, niente Configuration ne' Boost: le impostazioni
// arrivano dall'esterno.
// ---------------------------------------------------------------------------
class DecodiumDecoLogLink : public QObject
{
    Q_OBJECT

public:
    static constexpr int kProtocol = 1;
    static constexpr quint16 kDefaultPort = 52237;

    explicit DecodiumDecoLogLink(QObject* parent = nullptr);
    ~DecodiumDecoLogLink() override;

    void setIdentity(const QString& version, const QString& station);
    // Acceso: si collega e, se DecoLog non c'e', riprova ogni 5 s.
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }
    void setPort(quint16 port);
    quint16 port() const { return m_port; }

    bool isConnected() const { return m_connected; }
    QString peerVersion() const { return m_peerVersion; }
    // Nome del programma dall'altra parte ("product" nel saluto). Vuoto con i
    // log piu' vecchi del campo: in quel caso vale il nome di protocollo.
    QString peerProduct() const { return m_peerProduct; }

    // Che cosa sa DecoLog di questi nominativi; risposta con queryAnswered().
    int query(const QStringList& calls, const QString& band, const QString& mode);

signals:
    void connectedChanged(bool connected);
    // Un blocco del log: righe [call, banda, modo, data yyyyMMdd, locatore4, confermato].
    // `first` sul primo blocco di un elenco completo, `final` sull'ultimo.
    void workedRows(const QJsonArray& rows, bool first, bool final);
    // Un QSO appena scritto (o scartato) da DecoLog.
    void qsoWritten(const QJsonObject& message);
    void awardChanged(const QJsonObject& award);
    void queryAnswered(int id, const QJsonArray& results);
    // Uno spot del cluster di DecoLog, gia' confrontato con il log.
    void spotReceived(const QJsonObject& spot);
    // L'operatore ha scelto uno spot in DecoLog: sintonizzare (mai trasmettere).
    void tuneRequested(const QJsonObject& request);

private:
    void connectNow();
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void handle(const QJsonObject& message);
    void send(const QJsonObject& message);

    QTcpSocket* m_socket{nullptr};
    QTimer m_retry;
    QTimer m_ping;
    QByteArray m_buffer;
    bool m_enabled{false};
    bool m_connected{false};
    bool m_awaitingPong{false};
    quint16 m_port{kDefaultPort};
    int m_nextId{1};
    QString m_version;
    QString m_station;
    QString m_peerVersion;
    QString m_peerProduct;
};

#endif // DECODIUMDECOLOGLINK_H
