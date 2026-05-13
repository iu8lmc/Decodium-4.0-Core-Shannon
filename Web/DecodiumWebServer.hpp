// -*- Mode: C++ -*-
//
// Decodium 1.0.167 — HTTP server minimale per remote viewer (iPad/PWA).
//
// Serve una pagina HTML responsive e una API JSON con lo stato corrente
// del bridge: decode list, dxCall, freq, mode, NTP status.
//
// Endpoint:
//   GET /              → index.html (UI responsive con polling JS)
//   GET /api/state     → JSON snapshot dello stato
//   GET /api/decodes   → JSON solo decode list
//   GET /manifest.json → PWA manifest (per "Aggiungi a home" su iPad)
//   GET /favicon.ico   → 1x1 transparent gif placeholder
//
// Polling 2s lato client. Nessuna dipendenza esterna (no QtHttpServer,
// no QtWebSockets) → portabile e leggero. Su rete LAN gli iPad/iPhone
// puntano semplicemente a http://<pc-ip>:8080/

#ifndef DECODIUM_WEB_SERVER_HPP__
#define DECODIUM_WEB_SERVER_HPP__

#include <QObject>
#include <QTcpServer>
#include <QString>
#include <QPointer>

class DecodiumBridge;
class QTcpSocket;

class DecodiumWebServer : public QObject
{
    Q_OBJECT
public:
    explicit DecodiumWebServer(DecodiumBridge* bridge, QObject* parent = nullptr);
    ~DecodiumWebServer() override;

    // Avvia il server sulla porta indicata (default 8080). Ritorna true se
    // il bind ha avuto successo. Se gia' running su un'altra porta, ferma
    // e riavvia.
    bool start(quint16 port = 8080);
    void stop();
    bool isRunning() const;
    quint16 port() const { return m_port; }

    // URL completo accessibile da altri device LAN (es. http://192.168.1.20:8080).
    // Ritorna stringa vuota se non running o se non riesce a determinare un IP.
    QString accessUrl() const;

Q_SIGNALS:
    void runningChanged(bool running);
    void errorOccurred(QString const& msg);

private Q_SLOTS:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void handleRequest(QTcpSocket* socket, QString const& method,
                       QString const& path, QString const& query);
    QByteArray buildIndexHtml() const;
    QByteArray buildStateJson() const;
    QByteArray buildDecodesJson() const;
    QByteArray buildManifestJson() const;
    QByteArray buildFavicon() const;
    void writeHttpResponse(QTcpSocket* socket, int statusCode,
                           QByteArray const& contentType,
                           QByteArray const& body,
                           bool keepAlive = false) const;

    DecodiumBridge*       m_bridge {nullptr};
    QPointer<QTcpServer>  m_server;
    quint16               m_port {0};
};

#endif  // DECODIUM_WEB_SERVER_HPP__
