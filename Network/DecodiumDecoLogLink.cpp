#include "DecodiumDecoLogLink.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QTcpSocket>

namespace {
constexpr int kRetryMs = 5000;
constexpr int kPingMs = 30000;
constexpr qsizetype kMaxLine = 16 * 1024 * 1024;
}

DecodiumDecoLogLink::DecodiumDecoLogLink(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    m_retry.setSingleShot(true);
    m_retry.setInterval(kRetryMs);
    connect(&m_retry, &QTimer::timeout, this, &DecodiumDecoLogLink::connectNow);

    // Un ping senza risposta entro il successivo vuol dire connessione appesa
    // (DecoLog bloccato o sospeso): si chiude e si riprova.
    m_ping.setInterval(kPingMs);
    connect(&m_ping, &QTimer::timeout, this, [this] {
        if (!m_connected)
            return;
        if (m_awaitingPong) {
            m_socket->abort();
            return;
        }
        m_awaitingPong = true;
        send(QJsonObject{{QStringLiteral("type"), QStringLiteral("ping")}, {QStringLiteral("id"), m_nextId++}});
    });

    connect(m_socket, &QTcpSocket::connected, this, &DecodiumDecoLogLink::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &DecodiumDecoLogLink::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        // Rifiutato (DecoLog chiuso) o caduto: stesso trattamento.
        if (m_socket->state() != QAbstractSocket::ConnectedState)
            onDisconnected();
    });
    connect(m_socket, &QTcpSocket::readyRead, this, &DecodiumDecoLogLink::onReadyRead);
}

DecodiumDecoLogLink::~DecodiumDecoLogLink()
{
    m_retry.stop();
    m_ping.stop();
    m_socket->disconnect(this);
    m_socket->abort();
}

void DecodiumDecoLogLink::setIdentity(const QString& version, const QString& station)
{
    m_version = version;
    m_station = station;
}

void DecodiumDecoLogLink::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;
    if (enabled) {
        connectNow();
    } else {
        m_retry.stop();
        m_ping.stop();
        m_socket->abort();
    }
}

void DecodiumDecoLogLink::setPort(quint16 port)
{
    if (port == m_port || port == 0)
        return;
    m_port = port;
    if (m_enabled) {
        m_socket->abort();
        connectNow();
    }
}

void DecodiumDecoLogLink::connectNow()
{
    if (!m_enabled || m_socket->state() != QAbstractSocket::UnconnectedState)
        return;
    m_buffer.clear();
    m_socket->connectToHost(QHostAddress::LocalHost, m_port);
}

void DecodiumDecoLogLink::onConnected()
{
    m_awaitingPong = false;
    send(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("hello")},
        {QStringLiteral("app"), QStringLiteral("Decodium")},
        {QStringLiteral("version"), m_version},
        {QStringLiteral("protocol"), kProtocol},
        {QStringLiteral("station"), m_station},
    });
    m_ping.start();
    // "Collegato" solo quando DecoLog risponde con il suo hello: una porta aperta
    // da un altro programma non e' DecoLog.
}

void DecodiumDecoLogLink::onDisconnected()
{
    m_ping.stop();
    const bool was = m_connected;
    m_connected = false;
    m_peerVersion.clear();
    m_peerProduct.clear();
    if (was)
        emit connectedChanged(false);
    if (m_enabled && !m_retry.isActive())
        m_retry.start();
}

void DecodiumDecoLogLink::onReadyRead()
{
    m_buffer += m_socket->readAll();
    qsizetype newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(newline).trimmed();
        m_buffer.remove(0, newline + 1);
        if (line.isEmpty())
            continue;
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &error);
        if (error.error == QJsonParseError::NoError && doc.isObject())
            handle(doc.object());
    }
    if (m_buffer.size() > kMaxLine)
        m_socket->abort();
}

void DecodiumDecoLogLink::handle(const QJsonObject& message)
{
    const QString type = message.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("hello")) {
        // "app" e' il nome del PROTOCOLLO, non del programma: DecoLog lo manda
        // cosi' anche dopo essersi rinominato DecoDXLog. Si accettano entrambi,
        // perche' un saluto rifiutato qui diventa una riconnessione ogni 5
        // secondi all'infinito, senza che si veda perche'.
        const QString app = message.value(QStringLiteral("app")).toString();
        if (app != QLatin1String("DecoLog") && app != QLatin1String("DecoDXLog")) {
            m_socket->abort();
            return;
        }
        m_peerVersion = message.value(QStringLiteral("version")).toString();
        // Nome vero del programma, se lo dichiara: e' quello da mostrare.
        m_peerProduct = message.value(QStringLiteral("product")).toString().trimmed();
        if (!m_connected) {
            m_connected = true;
            emit connectedChanged(true);
        }
    } else if (type == QLatin1String("worked")) {
        emit workedRows(message.value(QStringLiteral("rows")).toArray(),
                        message.value(QStringLiteral("seq")).toInt() == 1,
                        message.value(QStringLiteral("final")).toBool());
    } else if (type == QLatin1String("qso")) {
        emit qsoWritten(message);
    } else if (type == QLatin1String("award")) {
        emit awardChanged(message);
    } else if (type == QLatin1String("spot")) {
        emit spotReceived(message);
    } else if (type == QLatin1String("tune")) {
        emit tuneRequested(message);
    } else if (type == QLatin1String("status")) {
        emit queryAnswered(message.value(QStringLiteral("id")).toInt(), message.value(QStringLiteral("results")).toArray());
    } else if (type == QLatin1String("pong")) {
        m_awaitingPong = false;
    }
    // Tipi sconosciuti: ignorati (versioni successive del protocollo).
}

int DecodiumDecoLogLink::query(const QStringList& calls, const QString& band, const QString& mode)
{
    if (!m_connected)
        return 0;
    const int id = m_nextId++;
    QJsonArray list;
    for (const QString& c : calls)
        list.append(c);
    send(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("query")},
        {QStringLiteral("id"), id},
        {QStringLiteral("band"), band},
        {QStringLiteral("mode"), mode},
        {QStringLiteral("calls"), list},
    });
    return id;
}

void DecodiumDecoLogLink::send(const QJsonObject& message)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
        m_socket->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}
