#include "DecodiumSpotShare.h"

#include "DecodiumDxCluster.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QVariantList>

DecodiumSpotShare::DecodiumSpotShare(DecodiumDxCluster* cluster, QObject* parent)
    : QObject(parent)
    , m_cluster(cluster)
{
    if (m_cluster) {
        connect(m_cluster, &DecodiumDxCluster::newSpot,
                this, &DecodiumSpotShare::onNewSpot);
    }

    // Il battito parte solo con la porta aperta: a servizio spento non deve
    // esistere alcun timer che gira per niente.
    m_battito = new QTimer(this);
    m_battito->setInterval(kBattitoMs);
    connect(m_battito, &QTimer::timeout, this, [this] {
        inviaATutti(QByteArrayLiteral("{\"tipo\":\"battito\"}\n"));
    });
}

DecodiumSpotShare::~DecodiumSpotShare()
{
    configure(false, m_port);
}

bool DecodiumSpotShare::listening() const
{
    return m_server && m_server->isListening();
}

QString DecodiumSpotShare::status() const
{
    if (!m_enabled)
        return tr("Spot sharing off");
    if (!listening())
        return m_lastError.isEmpty() ? tr("Spot sharing not listening") : m_lastError;
    return tr("Sharing spots on port %1 — %2 client(s)")
        .arg(m_port)
        .arg(m_clienti.size());
}

bool DecodiumSpotShare::configure(bool enabled, int port)
{
    if (port < 1024 || port > 65535)
        port = kPortaPredefinita;

    bool const cambiato = (enabled != m_enabled) || (port != m_port);
    m_enabled = enabled;
    m_port = port;

    // Si chiude sempre prima: cambiare porta con la vecchia ancora aperta
    // lascerebbe due servizi in ascolto, e chi si collega non saprebbe a quale
    // dei due ha parlato.
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    for (auto it = m_clienti.cbegin(); it != m_clienti.cend(); ++it)
        it.key()->deleteLater();
    m_clienti.clear();
    m_battito->stop();
    m_lastError.clear();

    if (cambiato)
        emit configChanged();
    emit clientsChanged();

    if (!m_enabled) {
        emit listeningChanged();
        return true;
    }

    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &DecodiumSpotShare::onNewConnection);
    // Su tutte le interfacce: il telefono arriva dal WiFi, non dal loopback.
    if (!m_server->listen(QHostAddress::Any, quint16(m_port))) {
        m_lastError = tr("Port %1 unavailable: %2").arg(m_port).arg(m_server->errorString());
        m_server->deleteLater();
        m_server = nullptr;
        emit listeningChanged();
        return false;
    }

    m_battito->start();
    emit listeningChanged();
    return true;
}

QByteArray DecodiumSpotShare::rigaSpot(const QVariantMap& spot)
{
    QJsonObject o = QJsonObject::fromVariantMap(spot);
    o.insert(QStringLiteral("tipo"), QStringLiteral("spot"));
    return QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n';
}

void DecodiumSpotShare::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* c = m_server->nextPendingConnection();
        if (!c) break;
        // Righe corte e rare, ma quando arrivano devono partire subito: chi
        // guarda gli spot sul telefono non deve aspettare che il buffer si
        // riempia.
        c->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        connect(c, &QTcpSocket::disconnected, this, &DecodiumSpotShare::onDisconnected);
        // Nessun comando in entrata: questo servizio si legge e basta. Quel
        // che arriva si scarta, altrimenti resterebbe nel buffer del sistema
        // finche' non lo riempie.
        connect(c, &QTcpSocket::readyRead, this, [c] { c->readAll(); });
        m_clienti.insert(c, QByteArray());

        QVariantList const tutti = m_cluster ? m_cluster->spots() : QVariantList{};
        int const da = qMax(0, int(tutti.size()) - kSnapshotMax);
        int const quanti = int(tutti.size()) - da;

        QJsonObject benvenuto;
        benvenuto.insert(QStringLiteral("tipo"), QStringLiteral("benvenuto"));
        benvenuto.insert(QStringLiteral("servizio"), QStringLiteral("decodium-spot-share"));
        benvenuto.insert(QStringLiteral("versione"), 1);
        benvenuto.insert(QStringLiteral("attesi"), quanti);
        invia(c, QJsonDocument(benvenuto).toJson(QJsonDocument::Compact) + '\n');

        // Gli spot gia' raccolti, dal piu' vecchio al piu' recente: chi si
        // collega adesso vede la stessa lista che ha davanti chi sta al PC,
        // invece di una finestra vuota fino al prossimo spot del nodo.
        for (int i = da; i < tutti.size(); ++i)
            invia(c, rigaSpot(tutti.at(i).toMap()));

        emit clientsChanged();
    }
}

void DecodiumSpotShare::onDisconnected()
{
    auto* c = qobject_cast<QTcpSocket*>(sender());
    if (!c) return;
    m_clienti.remove(c);
    c->deleteLater();
    emit clientsChanged();
    emit listeningChanged();   // il conteggio compare nella riga di stato
}

void DecodiumSpotShare::onNewSpot(const QVariantMap& spot)
{
    if (m_clienti.isEmpty() || spot.isEmpty())
        return;
    inviaATutti(rigaSpot(spot));
}

void DecodiumSpotShare::invia(QTcpSocket* c, const QByteArray& riga)
{
    if (!c || c->state() != QAbstractSocket::ConnectedState)
        return;
    // Coda che non scende: dall'altra parte non legge piu' nessuno. Meglio
    // staccare che tenere in memoria uno storico destinato a nessuno.
    if (c->bytesToWrite() > kArretratoMax) {
        c->abort();
        return;
    }
    c->write(riga);
}

void DecodiumSpotShare::inviaATutti(const QByteArray& riga)
{
    for (auto it = m_clienti.cbegin(); it != m_clienti.cend(); ++it)
        invia(it.key(), riga);
}
