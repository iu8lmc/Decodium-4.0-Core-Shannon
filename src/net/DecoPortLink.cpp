#include "DecoPortLink.h"

#include <QDateTime>
#include <QNetworkDatagram>
#include <QTimer>
#include <QUdpSocket>
#include <QVariantMap>

using namespace decoport;

namespace {
constexpr int kKeepAliveMs = 2000;
constexpr int kReapMs = 2000;
// Senza contesto per questo tempo il gateway e' considerato perso: il contesto
// arriva 4 volte al secondo, quindi e' un silenzio lungo, non un pacchetto solo.
constexpr qint64 kContextTimeoutMs = 6000;
} // namespace

// ── scoperta ────────────────────────────────────────────────────────────────

DecoPortDiscovery::DecoPortDiscovery(QObject* parent)
    : QObject(parent)
{
}

DecoPortDiscovery::~DecoPortDiscovery()
{
    stop();
}

bool DecoPortDiscovery::start()
{
    if (m_socket)
        return true;

    m_socket = new QUdpSocket(this);
    // ShareAddress: sulla stessa macchina possono ascoltare piu' programmi, e
    // uno di questi puo' benissimo essere il gateway stesso.
    if (!m_socket->bind(QHostAddress::AnyIPv4, kAnnouncePort,
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        delete m_socket;
        m_socket = nullptr;
        return false;
    }
    connect(m_socket, &QUdpSocket::readyRead, this, &DecoPortDiscovery::onDatagrams);

    m_reap = new QTimer(this);
    m_reap->setInterval(kReapMs);
    connect(m_reap, &QTimer::timeout, this, &DecoPortDiscovery::onReap);
    m_reap->start();

    emit listeningChanged();
    return true;
}

void DecoPortDiscovery::stop()
{
    if (m_reap)   { m_reap->stop();   m_reap->deleteLater();   m_reap = nullptr; }
    if (m_socket) { m_socket->close(); m_socket->deleteLater(); m_socket = nullptr; }
    if (!m_seen.isEmpty()) {
        m_seen.clear();
        emit radiosChanged();
    }
    emit listeningChanged();
}

void DecoPortDiscovery::onDatagrams()
{
    bool changed = false;
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QNetworkDatagram const dg = m_socket->receiveDatagram();
        Header h;
        QByteArray payload;
        bool authed = false;
        if (!parsePacket(dg.data(), &h, &payload, m_authKey, &authed))
            continue;
        if (h.type != Type::Announce)
            continue;
        // Annuncio non firmato o firmato male: non entra nell'elenco.
        if (!authed || !timestampAcceptable(h))
            continue;
        Context ctx;
        if (!decodeContextPayload(payload, &ctx))
            continue;

        Seen entry;
        entry.address = dg.senderAddress();
        entry.sessionPort = ctx.has(FieldSessionPort) ? ctx.sessionPort : kSessionPort;
        entry.streamId = h.streamId;
        entry.rigLabel = ctx.rigLabel;
        entry.stateFlags = ctx.stateFlags;
        entry.lastSeenMs = QDateTime::currentMSecsSinceEpoch();

        bool found = false;
        for (Seen& s : m_seen) {
            if (s.streamId == entry.streamId) {
                bool const sameLabel = (s.rigLabel == entry.rigLabel)
                                    && (s.stateFlags == entry.stateFlags)
                                    && (s.address == entry.address);
                s = entry;
                if (!sameLabel)
                    changed = true;
                found = true;
                break;
            }
        }
        if (!found) {
            m_seen.append(entry);
            changed = true;
        }
    }
    if (changed)
        emit radiosChanged();
}

void DecoPortDiscovery::onReap()
{
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    int const before = m_seen.size();
    for (int i = m_seen.size() - 1; i >= 0; --i) {
        if (nowMs - m_seen.at(i).lastSeenMs > kClientTimeoutMs)
            m_seen.removeAt(i);
    }
    if (m_seen.size() != before)
        emit radiosChanged();
}

QVariantList DecoPortDiscovery::radios() const
{
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    QVariantList out;
    out.reserve(m_seen.size());
    for (const Seen& s : m_seen) {
        QVariantMap m;
        m.insert(QStringLiteral("host"), s.address.toString());
        m.insert(QStringLiteral("port"), s.sessionPort);
        m.insert(QStringLiteral("streamId"), s.streamId);
        m.insert(QStringLiteral("rigLabel"), s.rigLabel);
        m.insert(QStringLiteral("catOnline"), (s.stateFlags & StateCatOnline) != 0);
        m.insert(QStringLiteral("canTransmit"), (s.stateFlags & StateCanTransmit) != 0);
        m.insert(QStringLiteral("ageMs"), static_cast<int>(nowMs - s.lastSeenMs));
        out.append(m);
    }
    return out;
}

// ── collegamento ────────────────────────────────────────────────────────────

DecoPortLink::DecoPortLink(QObject* parent)
    : RadioLink(parent)
{
    m_status = tr("not connected");
}

DecoPortLink::~DecoPortLink()
{
    disconnectFromGateway();
}

void DecoPortLink::setStatus(const QString& s)
{
    if (m_status == s)
        return;
    m_status = s;
    emit statusChanged();
}

void DecoPortLink::setLinked(bool v)
{
    if (m_linked == v)
        return;
    m_linked = v;
    emit linkedChanged();
}

void DecoPortLink::setRemoteStreamId(quint32 streamId)
{
    if (m_remoteStreamId == streamId)
        return;
    m_remoteStreamId = streamId;
    m_haveRxSeq = false;
    emit remoteStreamChanged(streamId);
}

bool DecoPortLink::connectTo(const QString& host, int port)
{
    disconnectFromGateway();

    if (m_authKey.isEmpty()) {
        setStatus(tr("set the DecoPort password first"));
        return false;
    }
    QHostAddress addr(host);
    if (addr.isNull()) {
        setStatus(tr("bad address: %1").arg(host));
        return false;
    }
    m_peer = addr;
    m_peerPort = (port > 0 && port < 65536) ? static_cast<quint16>(port)
                                            : static_cast<quint16>(kSessionPort);

    m_socket = new QUdpSocket(this);
    // Porta effimera: al gateway serve solo sapere da dove arriviamo, e cosi'
    // due client sulla stessa macchina non si pestano.
    if (!m_socket->bind(QHostAddress::AnyIPv4, 0)) {
        setStatus(tr("cannot open a local port: %1").arg(m_socket->errorString()));
        delete m_socket;
        m_socket = nullptr;
        return false;
    }
    connect(m_socket, &QUdpSocket::readyRead, this, &DecoPortLink::onDatagrams);

    m_keepAlive = new QTimer(this);
    m_keepAlive->setInterval(kKeepAliveMs);
    connect(m_keepAlive, &QTimer::timeout, this, &DecoPortLink::onKeepAlive);
    m_keepAlive->start();

    sendBare(Type::Hello);
    setStatus(tr("calling %1:%2").arg(host).arg(m_peerPort));
    return true;
}

void DecoPortLink::disconnectFromGateway()
{
    if (m_socket)
        sendBare(Type::Bye);
    if (m_keepAlive) { m_keepAlive->stop(); m_keepAlive->deleteLater(); m_keepAlive = nullptr; }
    if (m_socket)    { m_socket->close();   m_socket->deleteLater();   m_socket = nullptr; }
    m_haveRxSeq = false;
    setLinked(false);
    setRemoteStreamId(0);
    setStatus(tr("not connected"));
}

void DecoPortLink::sendBare(Type type)
{
    if (!m_socket)
        return;
    QByteArray const pkt = buildPacket(type, 0, 0, nowUnixNs(), QByteArray(), m_authKey);
    m_socket->writeDatagram(pkt, m_peer, m_peerPort);
}

void DecoPortLink::onKeepAlive()
{
    sendBare(Type::KeepAlive);
    // Il collegamento vive finche' arriva contesto: il keepalive lo mandiamo
    // noi, quindi non dice niente sullo stato dell'altro capo.
    if (m_linked && m_lastContextMs > 0
        && QDateTime::currentMSecsSinceEpoch() - m_lastContextMs > kContextTimeoutMs) {
        setLinked(false);
        setStatus(tr("gateway silent"));
    }
}

void DecoPortLink::onDatagrams()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QNetworkDatagram const dg = m_socket->receiveDatagram();
        Header h;
        QByteArray payload;
        bool authed = false;
        if (!parsePacket(dg.data(), &h, &payload, m_authKey, &authed))
            continue;
        // Un gateway che non sa la password non e' il nostro gateway.
        if (!authed)
            continue;

        // A gateway process chooses a fresh non-zero stream id. Publish the
        // boundary before AudioRx so consumers can reject/rebind old-session
        // work without relabelling the first packet of the new session.
        if (h.streamId != 0)
            setRemoteStreamId(h.streamId);

        switch (h.type) {
        case Type::Context:
        case Type::Status: {
            Context ctx;
            if (!decodeContextPayload(payload, &ctx))
                break;
            m_state = ctx;
            m_lastContextMs = QDateTime::currentMSecsSinceEpoch();
            if (!m_linked) {
                setLinked(true);
                setStatus(tr("linked to %1").arg(ctx.rigLabel.isEmpty()
                                                     ? tr("a radio") : ctx.rigLabel));
            }
            emit stateChanged();
            break;
        }
        case Type::AudioRx: {
            if (payload.isEmpty() || (payload.size() % 2) != 0)
                break;
            if (m_haveRxSeq && h.sequence != m_lastRxSeq + 1)
                ++m_rxGaps;   // un buco: si conta, non si finge che non ci sia
            m_lastRxSeq = h.sequence;
            m_haveRxSeq = true;

            int const count = payload.size() / 2;
            QVector<short> samples(count);
            const uchar* p = reinterpret_cast<const uchar*>(payload.constData());
            for (int i = 0; i < count; ++i)
                samples[i] = static_cast<short>(static_cast<quint16>(p[2 * i]) |
                                                (static_cast<quint16>(p[2 * i + 1]) << 8));
            quint64 const ts = static_cast<quint64>(h.tsSeconds) * 1000000000ull + h.tsNanos;
            emit rxAudioProduced(samples, ts, h.streamId);
            emit rxAudio(samples, ts);
            break;
        }
        default:
            break;
        }
    }
}

void DecoPortLink::sendCommand(const Context& cmd, quint64 whenNs)
{
    if (!m_socket)
        return;
    QByteArray const pkt = buildPacket(Type::Command, 0, m_commandSeq++,
                                       whenNs ? whenNs : nowUnixNs(),
                                       encodeContextPayload(cmd), m_authKey);
    m_socket->writeDatagram(pkt, m_peer, m_peerPort);
}

void DecoPortLink::setFrequency(qint64 hz)
{
    Context cmd;
    cmd.setFrequency(hz);
    sendCommand(cmd, 0);
}

void DecoPortLink::setMode(Mode mode)
{
    Context cmd;
    cmd.setMode(mode);
    sendCommand(cmd, 0);
}

void DecoPortLink::setPtt(bool on, quint64 whenNs)
{
    Context cmd;
    cmd.setPtt(on);
    sendCommand(cmd, whenNs);
}

void DecoPortLink::sendTxAudio(const QVector<short>& samples, quint64 playAtNs)
{
    if (!m_socket || samples.isEmpty())
        return;
    QByteArray payload;
    payload.resize(samples.size() * 2);
    uchar* p = reinterpret_cast<uchar*>(payload.data());
    for (int i = 0; i < samples.size(); ++i) {
        quint16 const v = static_cast<quint16>(samples[i]);
        p[2 * i]     = static_cast<uchar>(v & 0xFF);
        p[2 * i + 1] = static_cast<uchar>((v >> 8) & 0xFF);
    }
    // Il timestamp e' l'ora di suonata, non l'ora di spedizione.
    QByteArray const pkt = buildPacket(Type::AudioTx, 0, m_txAudioSeq++,
                                       playAtNs ? playAtNs : nowUnixNs(), payload, m_authKey);
    m_socket->writeDatagram(pkt, m_peer, m_peerPort);
}

void DecoPortLink::tune(double hz)
{
    if (hz > 0.0)
        setFrequency(static_cast<qint64>(hz));
}

void DecoPortLink::setModeName(const QString& name)
{
    Mode const m = modeFromString(name);
    if (m != Mode::Unknown)
        setMode(m);
}

void DecoPortLink::key(bool on)
{
    setPtt(on, 0);
}
