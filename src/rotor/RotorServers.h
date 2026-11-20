// I server di rete del rotore: i tre modi con cui qualcuno altro puo' comandarlo.
//
//   - rotctld   TCP, il dialetto di rete di Hamlib: lo parlano N1MM+, Log4OM,
//               PstRotator, SatPC32, gpredict, rotctl;
//   - WebSocket JSON, per le app Android e iPhone di DecoRotor;
//   - HTTP      una web UI di riserva, l'API REST per gli automatismi e i
//               riquadri della mappa satellitare, con la cache su disco.
//
// Nessuno di questi tocca la seriale: chiedono una fotografia al controllo o
// gli accodano un comando. Restano spenti finche' l'operatore non li accende.
#pragma once

#include "RotorConfig.h"
#include "RotorController.h"

#include <QHash>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketServer>

#include <functional>
#include <optional>

namespace decodium::rotor {

// Quello che i server non possono sapere da soli: configurazione modificabile a
// caldo e libro degli spot, che stanno nel modulo.
struct ServerDelegate {
    std::function<QVariantMap()> configPayload;
    std::function<QVariantMap(const QVariantMap& values)> configApply;
    std::function<QVariantMap()> spotsPayload;
};

// ── rotctld ────────────────────────────────────────────────────────────────
class RotctldServer : public QObject {
    Q_OBJECT
public:
    RotctldServer(RotorController* rotor, QObject* parent = nullptr);
    bool listen(const QHostAddress& address, quint16 port, QString* error = nullptr);
    void close();
    bool isListening() const { return m_server.isListening(); }
    quint16 serverPort() const { return m_server.serverPort(); }

    // Esegue una riga di comando; nullopt = chiudi la sessione. Pubblica per i
    // test: il dialetto deve combaciare con quello di Hamlib.
    std::optional<QByteArray> execute(const QString& line);

private:
    void onConnection();
    void onData(QTcpSocket* socket);
    QByteArray getPosition(bool extended) const;
    QByteArray setPosition(const QStringList& args);
    QByteArray move(const QStringList& args);
    QByteArray dumpState() const;

    RotorController* m_rotor;
    QTcpServer m_server;
};

// ── WebSocket ──────────────────────────────────────────────────────────────
class WsServer : public QObject {
    Q_OBJECT
public:
    WsServer(RotorController* rotor, ServerDelegate delegate, QObject* parent = nullptr);
    bool listen(const QHostAddress& address, quint16 port, const QString& token, QString* error = nullptr);
    void close();
    bool isListening() const { return m_server && m_server->isListening(); }
    quint16 serverPort() const { return m_server ? m_server->serverPort() : 0; }

private:
    struct Client {
        bool authorized {false};
    };
    void onConnection();
    void onText(QWebSocket* socket, const QString& text);
    void dispatch(QWebSocket* socket, const QString& command, const QVariantMap& request, bool* authorized);
    void send(QWebSocket* socket, const QVariantMap& message);
    void broadcastState();

    RotorController* m_rotor;
    ServerDelegate m_delegate;
    QWebSocketServer* m_server {nullptr};
    QString m_token;
    QHash<QWebSocket*, Client> m_clients;
};

// ── riquadri della mappa ───────────────────────────────────────────────────
// La mappa dello shack guarda sempre le stesse zone: ogni riquadro scaricato
// resta su disco e la seconda volta parte da li', cosi' la mappa funziona anche
// con la rete giu' e il servizio remoto riceve una richiesta sola per riquadro.
class TileCache : public QObject {
    Q_OBJECT
public:
    static constexpr int kMaxZoom = 19;
    static constexpr qint64 kMaxTileBytes = 2'000'000;

    TileCache(const QString& directory, QObject* parent = nullptr);
    void configure(const QString& urlTemplate, double maxAgeDays, double maxMegabytes);
    static bool valid(int z, int x, int y);
    // Il riquadro, dalla cache o dalla rete; `done` riceve un array vuoto se manca.
    void tile(int z, int x, int y, std::function<void(const QByteArray&)> done);

private:
    QString path(int z, int x, int y) const;
    QByteArray readFresh(const QString& path) const;
    void store(const QString& path, const QByteArray& data);
    void trim();

    QString m_directory;
    QString m_template;
    qint64 m_maxAgeSeconds {30 * 86400};
    qint64 m_maxBytes {400LL * 1024 * 1024};
    QNetworkAccessManager m_network;
    QHash<QString, QList<std::function<void(const QByteArray&)>>> m_pending;
    int m_written {0};
};

// ── HTTP ───────────────────────────────────────────────────────────────────
class HttpServer : public QObject {
    Q_OBJECT
public:
    HttpServer(RotorController* rotor, TileCache* tiles, ServerDelegate delegate, QObject* parent = nullptr);
    bool listen(const QHostAddress& address, quint16 port, const QString& token,
                int wsPort, int rotctldPort, QString* error = nullptr);
    void close();
    bool isListening() const { return m_server.isListening(); }
    quint16 serverPort() const { return m_server.serverPort(); }

private:
    void onConnection();
    void onData(QTcpSocket* socket);
    void handle(QTcpSocket* socket, const QString& method, const QString& target,
                const QHash<QString, QString>& headers, const QByteArray& body);
    void sendJson(QTcpSocket* socket, const QVariantMap& payload, int status = 200);
    void sendResponse(QTcpSocket* socket, int status, const QByteArray& contentType,
                      const QByteArray& body, const QByteArray& extraHeaders = {});
    bool authorized(const QHash<QString, QString>& headers, const QString& queryToken) const;

    RotorController* m_rotor;
    TileCache* m_tiles;
    ServerDelegate m_delegate;
    QTcpServer m_server;
    QString m_token;
    int m_wsPort {0};
    int m_rotctldPort {0};
};

}  // namespace decodium::rotor
