// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecoPortCatMux.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QVariantMap>

using namespace decoport::cat;

namespace {
// I protocolli senza cornice si leggono a silenzi: un programma che scrive un
// comando in due pezzi va aspettato un attimo prima di metterlo in fila.
constexpr int kCoalesceMs = 15;
// Ogni quanto guardare se dall'altra parte di una porta virtuale c'e' ancora
// qualcuno.
constexpr int kPinPollMs = 250;
} // namespace

DecoPortCatMux::DecoPortCatMux(QObject* parent)
    : QObject(parent)
{
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] { finish(true); });

    m_silence = new QTimer(this);
    m_silence->setSingleShot(true);
    connect(m_silence, &QTimer::timeout, this, [this] { finish(false); });

    m_pins = new QTimer(this);
    m_pins->setInterval(kPinPollMs);
    connect(m_pins, &QTimer::timeout, this, &DecoPortCatMux::pollPins);

    connect(&m_line, &DecoPortSerialLine::readyRead, this, &DecoPortCatMux::onPhysicalData);
    connect(&m_line, &DecoPortSerialLine::lost, this, [this](const QString& why) {
        m_error = why;
        m_queue.clear();
        m_inFlight.reset();
        m_timeout->stop();
        m_silence->stop();
        emit lost(why);
    });
}

DecoPortCatMux::~DecoPortCatMux()
{
    close();
}

bool DecoPortCatMux::open(const QString& physical, const DecoPortLineSettings& settings)
{
    m_error.clear();
    if (!m_line.open(physical, settings, 3000, QStringLiteral("DecoPort CAT physical"))) {
        m_error = m_line.errorString();
        return false;
    }
    m_pins->start();
    return true;
}

void DecoPortCatMux::close()
{
    // Chi chiude con la radio in trasmissione la lascerebbe in aria.
    if (m_keyedTap != -1 && m_line.isOpen()) {
        const QByteArray release = releasePtt(m_dialect, m_keyedCiv);
        if (!release.isEmpty() && m_line.device()) {
            m_line.device()->write(release);
            m_line.device()->waitForBytesWritten(200);
        }
        m_keyedTap = -1;
    }
    m_pins->stop();
    m_timeout->stop();
    m_silence->stop();
    m_queue.clear();
    m_inFlight.reset();

    for (Server& s : m_servers) {
        if (s.server) {
            s.server->close();
            s.server->deleteLater();
        }
    }
    m_servers.clear();
    const QList<int> ids = m_taps.keys();
    for (int id : ids)
        removeTap(id);
    m_line.close();
    emit tapsChanged();
}

bool DecoPortCatMux::isOpen() const
{
    return m_line.isOpen();
}

// ── prese ───────────────────────────────────────────────────────────────────

int DecoPortCatMux::addTap(Tap tap)
{
    tap.id = m_nextTapId++;
    const int id = tap.id;
    if (framedBySilence(m_dialect)) {
        tap.coalesce = new QTimer(this);
        tap.coalesce->setSingleShot(true);
        tap.coalesce->setInterval(kCoalesceMs);
        connect(tap.coalesce, &QTimer::timeout, this, [this, id] { takeTapCommands(id); });
    }
    m_taps.insert(id, tap);
    emit tapsChanged();
    return id;
}

quint16 DecoPortCatMux::addTcpTap(quint16 port, const QString& name, bool unsolicited,
                                  bool trusted)
{
    Server s;
    s.name = name;
    s.unsolicited = unsolicited;
    s.trusted = trusted;
    s.server = new QTcpServer(this);
    // Solo locale, come le prese rigctld: il CAT grezzo non ha serratura.
    if (!s.server->listen(QHostAddress::LocalHost, port)) {
        s.error = s.server->errorString();
        s.server->deleteLater();
        s.server = nullptr;
        m_servers.append(s);
        emit tapsChanged();
        return 0;
    }
    QTcpServer* server = s.server;
    connect(server, &QTcpServer::newConnection, this, [this, server, name, unsolicited, trusted] {
        while (QTcpSocket* sock = server->nextPendingConnection()) {
            sock->setSocketOption(QAbstractSocket::LowDelayOption, 1);
            Tap tap;
            tap.name = name;
            tap.kind = QStringLiteral("tcp");
            tap.endpoint = QStringLiteral("127.0.0.1:%1").arg(server->serverPort());
            tap.io = sock;
            tap.unsolicited = unsolicited;
            tap.trusted = trusted;
            tap.clientPresent = true;
            const int id = addTap(tap);
            connect(sock, &QTcpSocket::readyRead, this, [this, id] { onTapData(id); });
            connect(sock, &QTcpSocket::disconnected, this, [this, id] {
                clientGone(id);
                removeTap(id);
                emit tapsChanged();
            });
        }
    });
    m_servers.append(s);
    emit tapsChanged();
    return server->serverPort();
}

bool DecoPortCatMux::addSerialTap(const QString& portName, const QString& name, bool openByDcd,
                                  QString* error)
{
    auto* line = new DecoPortSerialLine(this);
    // L'estremo nascosto di una coppia virtuale non ha una velocita' vera: i
    // byte passano dall'altra parte qualunque sia quella dichiarata. DTR
    // alzato dice al programma dall'altra parte che c'e' qualcuno.
    DecoPortLineSettings s;
    s.baud = 9600;
    s.dtr = 1;
    s.rts = 1;
    if (!line->open(portName, s, 3000, QStringLiteral("DecoPort virtual CAT tap"))) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(portName, line->errorString());
        line->deleteLater();
        return false;
    }
    Tap tap;
    tap.name = name;
    tap.kind = QStringLiteral("serial");
    tap.endpoint = portName;
    tap.io = line->device();
    tap.line = line;
    tap.unsolicited = true;
    tap.openByDcd = openByDcd;
    tap.lastDsr = openByDcd ? line->dcd() : line->dsr();
    tap.clientPresent = tap.lastDsr == 1;
    const int id = addTap(tap);
    connect(line, &DecoPortSerialLine::readyRead, this, [this, id] { onTapData(id); });
    connect(line, &DecoPortSerialLine::lost, this, [this, id](const QString&) {
        clientGone(id);
        removeTap(id);
        emit tapsChanged();
    });
    return true;
}

void DecoPortCatMux::removeTap(int tapId)
{
    auto it = m_taps.find(tapId);
    if (it == m_taps.end())
        return;
    Tap tap = it.value();
    m_taps.erase(it);
    if (tap.coalesce)
        tap.coalesce->deleteLater();
    if (tap.line) {
        tap.line->close();
        tap.line->deleteLater();
    } else if (tap.io) {
        tap.io->disconnect(this);
        tap.io->deleteLater();
    }
    for (int i = m_queue.size() - 1; i >= 0; --i)
        if (m_queue.at(i).tapId == tapId)
            m_queue.removeAt(i);
    if (m_inFlight && m_inFlight->tapId == tapId)
        m_inFlight->tapId = -1;
}

QVariantList DecoPortCatMux::taps() const
{
    QVariantList out;
    for (const Server& s : m_servers) {
        int clients = 0;
        const QString endpoint = s.server ? QStringLiteral("127.0.0.1:%1").arg(s.server->serverPort())
                                          : QString();
        for (const Tap& t : m_taps)
            if (t.kind == QLatin1String("tcp") && t.endpoint == endpoint)
                ++clients;
        QVariantMap m;
        m.insert(QStringLiteral("name"), s.name);
        m.insert(QStringLiteral("kind"), QStringLiteral("tcp"));
        m.insert(QStringLiteral("endpoint"), endpoint);
        m.insert(QStringLiteral("clients"), clients);
        m.insert(QStringLiteral("error"), s.error);
        out.append(m);
    }
    for (const Tap& t : m_taps) {
        if (t.kind != QLatin1String("serial"))
            continue;
        QVariantMap m;
        m.insert(QStringLiteral("name"), t.name);
        m.insert(QStringLiteral("kind"), t.kind);
        m.insert(QStringLiteral("endpoint"), t.endpoint);
        m.insert(QStringLiteral("clients"), t.clientPresent ? 1 : 0);
        m.insert(QStringLiteral("keyed"), t.id == m_keyedTap);
        out.append(m);
    }
    return out;
}

// ── traffico ────────────────────────────────────────────────────────────────

void DecoPortCatMux::onTapData(int tapId)
{
    auto it = m_taps.find(tapId);
    if (it == m_taps.end() || !it->io)
        return;
    it->inbox.append(it->io->readAll());
    // Chi scrive ha la porta aperta, anche se non alza DTR.
    it->clientPresent = true;
    if (it->coalesce)
        it->coalesce->start();
    else
        takeTapCommands(tapId);
}

void DecoPortCatMux::takeTapCommands(int tapId)
{
    auto it = m_taps.find(tapId);
    if (it == m_taps.end())
        return;
    for (const Command& c : takeCommands(m_dialect, it->inbox)) {
        // Un programma dimenticato non deve poter mandare in trasmissione la
        // radio di qualcun altro: il comando si ferma qui. Abbassare il PTT
        // invece passa sempre.
        if (c.ptt == 1 && !m_pttAllowed && !it->trusted) {
            ++m_pttRefused;
            if (m_dialect == Dialect::Civ && it->io)
                it->io->write(QByteArray("\xFE\xFE", 2) + char(c.civFrom) + char(c.civTo)
                              + QByteArray("\xFA\xFD", 2));
            emit pttRefused(it->name);
            continue;
        }
        m_queue.enqueue({tapId, c});
    }
    pump();
}

void DecoPortCatMux::pump()
{
    if (m_inFlight || m_queue.isEmpty() || !m_line.isOpen() || !m_line.device())
        return;
    Pending p = m_queue.dequeue();
    m_line.device()->write(p.cmd.bytes);
    ++m_sent;

    if (p.cmd.ptt == 1) {
        m_keyedTap = p.tapId;
        m_keyedCiv = p.cmd.civTo;
    } else if (p.cmd.ptt == 0) {
        m_keyedTap = -1;
    }

    m_inFlight = p;
    m_gotReply = false;
    m_timeout->start(replyTimeoutMs(m_dialect, p.cmd));
}

void DecoPortCatMux::finish(bool timedOut)
{
    if (!m_inFlight)
        return;
    // Una richiesta incerta senza risposta era un'impostazione: non e' un guasto.
    if (timedOut && m_inFlight->cmd.expectsReply && !m_inFlight->cmd.uncertain && !m_gotReply)
        ++m_timeouts;
    m_timeout->stop();
    m_silence->stop();
    m_inFlight.reset();
    pump();
}

void DecoPortCatMux::onPhysicalData()
{
    if (!m_line.device())
        return;
    m_rx.append(m_line.device()->readAll());

    if (framedBySilence(m_dialect)) {
        if (m_rx.isEmpty())
            return;
        const QByteArray chunk = m_rx;
        m_rx.clear();
        if (m_inFlight) {
            deliver(m_inFlight->tapId, chunk);
            m_gotReply = true;
            m_timeout->stop();
            m_silence->start(silenceMs(m_dialect));
        } else {
            broadcast(chunk);
        }
        return;
    }

    for (const QByteArray& reply : takeReplies(m_dialect, m_rx)) {
        switch (classify(m_dialect, m_inFlight ? &m_inFlight->cmd : nullptr, reply)) {
        case Reply::Answer:
        case Reply::Error:
            m_gotReply = true;
            deliver(m_inFlight->tapId, reply);
            finish(false);
            break;
        case Reply::Echo:
            // Un bus CI-V ripete quello che riceve: il programma che ha
            // scritto se lo aspetta, come se fosse attaccato da solo.
            deliver(m_inFlight->tapId, reply);
            break;
        case Reply::Unsolicited:
            broadcast(reply);
            break;
        }
    }
}

void DecoPortCatMux::deliver(int tapId, const QByteArray& bytes)
{
    const auto it = m_taps.constFind(tapId);
    if (it == m_taps.constEnd() || !it->io)
        return;
    it->io->write(bytes);
    ++m_routed;
}

void DecoPortCatMux::broadcast(const QByteArray& bytes)
{
    for (const Tap& t : m_taps)
        if (t.unsolicited && t.io && t.clientPresent)
            t.io->write(bytes);
    ++m_broadcast;
}

// ── chi se ne va ────────────────────────────────────────────────────────────

void DecoPortCatMux::clientGone(int tapId)
{
    auto it = m_taps.find(tapId);
    if (it == m_taps.end())
        return;
    it->clientPresent = false;
    it->inbox.clear();
    for (int i = m_queue.size() - 1; i >= 0; --i)
        if (m_queue.at(i).tapId == tapId)
            m_queue.removeAt(i);

    if (m_keyedTap != tapId)
        return;
    m_keyedTap = -1;
    QByteArray release = releasePtt(m_dialect, m_keyedCiv);
    if (release.isEmpty())
        return;
    // Davanti a tutti: la radio in aria non aspetta il turno.
    const QVector<Command> cmds = takeCommands(m_dialect, release);
    for (int i = cmds.size() - 1; i >= 0; --i)
        m_queue.prepend({-1, cmds.at(i)});
    emit pttReleased(it->name);
    pump();
}

void DecoPortCatMux::pollPins()
{
    for (auto it = m_taps.begin(); it != m_taps.end(); ++it) {
        if (!it->line)
            continue;
        const int dsr = it->openByDcd ? it->line->dcd() : it->line->dsr();
        const int before = it->lastDsr;
        it->lastDsr = dsr;
        if (dsr == 1 && !it->clientPresent) {
            it->clientPresent = true;
            emit tapsChanged();
        } else if (before == 1 && dsr == 0) {
            // Il programma ha chiuso la porta virtuale: DTR e' sceso.
            const int id = it.key();
            clientGone(id);
            emit tapsChanged();
            return;   // clientGone puo' aver cambiato m_taps
        }
    }
}
