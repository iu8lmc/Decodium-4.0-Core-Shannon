#include "DecolinkLink.h"

#include <QDateTime>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUdpSocket>
#include <QUrl>
#include <QDebug>

using namespace decolink;

namespace {
// S9 sulle HF e' -73 dBm; rigctl dice STRENGTH in dB rispetto a S9.
constexpr double kS9Dbm = -73.0;
constexpr int    kTxFrameSamples = 120;        // 10 ms a 12 kHz
constexpr int    kTxFrameMs = 10;
constexpr qint64 kRegisterEveryMs = 5000;      // il relay scarta chi tace da 15 s
constexpr qint64 kRelaySilentMs = 12000;
constexpr qint64 kPttIdleMs = 3000;            // PTT su senza audio: lo si molla
}  // namespace

DecolinkLink::DecolinkLink(QObject* parent)
    : RemoteRadioLink(parent)
{
    auto mkTimer = [this](int ms, auto slot, bool single = false) {
        auto* t = new QTimer(this);
        t->setInterval(ms);
        t->setSingleShot(single);
        connect(t, &QTimer::timeout, this, slot);
        return t;
    };
    m_keepAlive = mkTimer(int(kRegisterEveryMs), &DecolinkLink::onKeepAlive);
    m_pingTimer = mkTimer(2000, &DecolinkLink::onPing);
    m_pollTimer = mkTimer(1000, &DecolinkLink::onPoll);
    m_watchTimer = mkTimer(1000, &DecolinkLink::onWatch);
    m_txTimer = mkTimer(kTxFrameMs, &DecolinkLink::onTxTick);
    m_renewTimer = new QTimer(this);
    m_renewTimer->setSingleShot(true);
    connect(m_renewTimer, &QTimer::timeout, this, [this]() {
        // Il token sta per scadere: lo si rifa' da soli, se la password e' a
        // portata. Altrimenti il relay comincia a rifiutare e l'audio si ferma.
        if (m_password.isEmpty() || m_authHost.isEmpty()) {
            setStatus(tr("Access expired: log in again"));
            return;
        }
        login(m_authHost, m_email, m_password, m_station.isEmpty() ? m_stationWanted : m_station);
    });
    m_status = tr("Not connected");
}

DecolinkLink::~DecolinkLink()
{
    closeRelay(true);
}

qint64 DecolinkLink::nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QString DecolinkLink::peerAddress() const
{
    if (m_relayAddr.isNull())
        return QString();
    return m_relayAddr.toString() + QLatin1Char(':') + QString::number(m_relayPort);
}

void DecolinkLink::setStatus(const QString& s)
{
    if (m_status == s)
        return;
    m_status = s;
    emit statusChanged();
}

void DecolinkLink::recomputeLinked()
{
    const bool now = isLinked();
    if (now != m_linkedReported) {
        m_linkedReported = now;
        emit linkedChanged();
        emit stateChanged();
    }
}

// ── accesso ────────────────────────────────────────────────────────────────

void DecolinkLink::login(const QString& authHost, const QString& email,
                         const QString& password, const QString& station)
{
    QString host = authHost.trimmed();
    if (host.isEmpty()) {
        setStatus(tr("The access server is missing"));
        return;
    }
    if (!host.startsWith(QLatin1String("http")))
        host.prepend(QStringLiteral("https://"));
    while (host.endsWith(QLatin1Char('/')))
        host.chop(1);

    m_authHost = host;
    m_email = email.trimmed();
    m_password = password;
    m_stationWanted = station.trimmed();
    m_lastRelogin = nowMs();

    if (!m_nam)
        m_nam = new QNetworkAccessManager(this);
    if (m_loginReply) {
        // un accesso nuovo manda a monte quello in corso, senza che il suo
        // segnale di fine ci arrivi addosso
        m_loginReply->disconnect(this);
        m_loginReply->abort();
        m_loginReply->deleteLater();
        m_loginReply = nullptr;
    }

    QJsonObject body;
    body.insert(QStringLiteral("email"), m_email);
    body.insert(QStringLiteral("password"), password);
    if (!m_stationWanted.isEmpty())
        body.insert(QStringLiteral("station"), m_stationWanted);

    QNetworkRequest req{QUrl(host + QStringLiteral("/api/login"))};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(10000);
    setStatus(tr("Logging in…"));

    QNetworkReply* rep = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_loginReply = rep;
    connect(rep, &QNetworkReply::finished, this, [this, rep]() {
        rep->deleteLater();
        if (m_loginReply == rep)
            m_loginReply = nullptr;
        const QJsonObject r = QJsonDocument::fromJson(rep->readAll()).object();
        // Un errore di rete senza corpo JSON e' un problema di collegamento, non
        // di credenziali: distinguerli evita di far cercare una password
        // sbagliata quando il server non risponde.
        if (r.isEmpty()) {
            setStatus(rep->error() == QNetworkReply::NoError
                          ? tr("Unintelligible reply from the server")
                          : tr("Server not reachable: %1").arg(rep->errorString()));
            return;
        }

        m_stations.clear();
        for (const QJsonValue& v : r.value(QStringLiteral("stations")).toArray()) {
            const QJsonObject o = v.toObject();
            QVariantMap m;
            m.insert(QStringLiteral("slug"), o.value(QStringLiteral("slug")).toString());
            m.insert(QStringLiteral("name"), o.value(QStringLiteral("name")).toString());
            m.insert(QStringLiteral("role"), o.value(QStringLiteral("role")).toString());
            m_stations.append(m);
        }

        if (!r.value(QStringLiteral("ok")).toBool()) {
            const bool needStation = r.value(QStringLiteral("need_station")).toBool();
            clearAuth();
            setStatus(needStation
                          ? tr("Choose the station to connect to")
                          : r.value(QStringLiteral("error")).toString(tr("Access refused")));
            emit authChanged();
            return;
        }

        setAuth(r.value(QStringLiteral("token")).toString(),
                r.value(QStringLiteral("callsign")).toString(),
                r.value(QStringLiteral("station")).toString(),
                r.value(QStringLiteral("station_name")).toString(),
                r.value(QStringLiteral("role")).toString(),
                r.value(QStringLiteral("can_transmit")).toBool(),
                r.value(QStringLiteral("expires_in")).toInt(3600));
    });
}

void DecolinkLink::setAuth(const QString& token, const QString& callsign,
                           const QString& station, const QString& stationName,
                           const QString& role, bool canTx, int expiresIn)
{
    m_token = token;
    m_callsign = callsign;
    m_station = station;
    m_stationName = stationName.isEmpty() ? station : stationName;
    m_role = role;
    m_canTx = canTx;
    m_tokenExpiresMs = nowMs() + qint64(expiresIn) * 1000;
    m_state.setRigLabel(m_stationName);
    m_state.setStateFlags((m_state.stateFlags & ~decoport::StateCanTransmit)
                          | (canTx ? decoport::StateCanTransmit : 0u));

    const QString roleText = role == QLatin1String("own") ? tr("owner")
                           : role == QLatin1String("opr") ? tr("operator")
                                                          : tr("listener");
    setStatus(tr("%1 on %2, as %3").arg(callsign, m_stationName, roleText));

    // Il token dura un'ora: lo si rifa' un minuto prima.
    if (expiresIn > 120)
        m_renewTimer->start(int(qint64(expiresIn - 60) * 1000));
    emit authChanged();
    emit stateChanged();

    if (m_wantRelay) {
        if (m_socket)
            sendRegister();     // rinnovo: lo stesso canale, col token nuovo
        else
            startRelay();
    }
}

void DecolinkLink::clearAuth()
{
    m_token.clear();
    m_role.clear();
    m_canTx = false;
    m_renewTimer->stop();
}

// ── relay ──────────────────────────────────────────────────────────────────

void DecolinkLink::connectTo(const QString& authHost, const QString& relayHost,
                             int relayPort, const QString& email,
                             const QString& password, const QString& station)
{
    closeRelay(true);
    m_relayHost = relayHost.trimmed();
    m_relayPort = relayPort > 0 && relayPort < 65536 ? relayPort : int(kDefaultRelayPort);
    m_wantRelay = true;
    login(authHost, email, password, station);
}

void DecolinkLink::startRelay()
{
    if (m_token.isEmpty())
        return;
    QString host = m_relayHost;
    if (host.isEmpty())
        host = QUrl(m_authHost).host();
    if (host.isEmpty()) {
        setStatus(tr("The relay address is missing"));
        return;
    }
    QHostAddress direct;
    if (direct.setAddress(host)) {
        m_relayAddr = direct;
        openSocket();
        return;
    }
    setStatus(tr("Resolving %1…").arg(host));
    QHostInfo::lookupHost(host, this, [this, host](const QHostInfo& info) {
        if (!m_wantRelay)
            return;
        if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
            setStatus(tr("Name not resolved: %1").arg(host));
            return;
        }
        // un indirizzo IPv4: il relay ascolta su 0.0.0.0
        QHostAddress pick = info.addresses().first();
        for (const QHostAddress& a : info.addresses())
            if (a.protocol() == QAbstractSocket::IPv4Protocol) { pick = a; break; }
        m_relayAddr = pick;
        openSocket();
    });
}

void DecolinkLink::openSocket()
{
    if (m_socket)
        return;
    m_socket = new QUdpSocket(this);
    if (!m_socket->bind()) {
        setStatus(tr("Could not open a UDP port: %1").arg(m_socket->errorString()));
        delete m_socket;
        m_socket = nullptr;
        return;
    }
    connect(m_socket, &QUdpSocket::readyRead, this, &DecolinkLink::onDatagrams);
    m_lastPacketMs = nowMs();
    m_keepAlive->start();
    m_pingTimer->start();
    m_pollTimer->start();
    m_watchTimer->start();
    setStatus(tr("Registering on the relay…"));
    sendRegister();
    emit linkedChanged();
}

void DecolinkLink::closeRelay(bool releasePtt)
{
    if (releasePtt && m_socket && m_pttRequested) {
        // Meglio tre volte: un datagramma perso lascerebbe la radio in
        // trasmissione senza nessuno che la sorvegli.
        for (int i = 0; i < 3; ++i)
            sendCat(QStringLiteral("T 0"), Kind::Ptt);
    }
    m_pttRequested = false;
    m_txTimer->stop();
    m_txSamples.clear();
    m_txPos = 0;
    m_keepAlive->stop();
    m_pingTimer->stop();
    m_pollTimer->stop();
    m_watchTimer->stop();
    m_renewTimer->stop();
    if (m_loginReply) {
        m_loginReply->disconnect(this);
        m_loginReply->abort();
        m_loginReply->deleteLater();
        m_loginReply = nullptr;
    }
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_pending.clear();
    m_registered = false;
    m_gatewayUp = false;
    m_haveRxSeq = false;
    m_state.ptt = false;
    recomputeLinked();
}

void DecolinkLink::disconnectFromRelay()
{
    m_wantRelay = false;
    closeRelay(true);
    clearAuth();
    m_password.clear();
    m_relayAddr = QHostAddress();
    setStatus(tr("Not connected"));
    emit authChanged();
    emit stateChanged();
}

void DecolinkLink::sendPacket(quint8 flag, quint32 seq, const QByteArray& body, quint32 rate)
{
    if (!m_socket || m_relayAddr.isNull())
        return;
    m_socket->writeDatagram(makePacket(flag, seq, body, rate, quint64(nowMs())),
                            m_relayAddr, quint16(m_relayPort));
}

void DecolinkLink::sendRegister()
{
    if (m_token.isEmpty())
        return;
    sendPacket(Register, ++m_regSeq, QByteArray("op ") + m_token.toLatin1());
}

void DecolinkLink::onKeepAlive()
{
    // Lo stesso REGISTER tiene aperto il buco nel NAT e rinnova la sessione sul
    // relay. Dopo un riavvio del relay e' anche quello che ci riporta dentro.
    sendRegister();
}

void DecolinkLink::onPing()
{
    if (!m_registered)
        return;
    sendPacket(Ping, 0, QByteArray());
}

// ── ricezione ──────────────────────────────────────────────────────────────

void DecolinkLink::onDatagrams()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QByteArray dg;
        dg.resize(int(m_socket->pendingDatagramSize()));
        QHostAddress from;
        quint16 port = 0;
        m_socket->readDatagram(dg.data(), dg.size(), &from, &port);
        // Solo dal relay: un datagramma da altri e' rumore o un tentativo di
        // farci credere che la radio sia qui.
        if (from != m_relayAddr && !from.isEqual(m_relayAddr, QHostAddress::ConvertV4MappedToIPv4))
            continue;
        Header h;
        QByteArray body;
        if (!parsePacket(dg, &h, &body))
            continue;
        m_lastPacketMs = nowMs();

        switch (h.flag) {
        case Register:
            if (!m_registered) {
                m_registered = true;
                setStatus(m_gatewayUp ? tr("Connected to %1").arg(m_stationName)
                                      : tr("Registered; waiting for the station's Decolink"));
                recomputeLinked();
            }
            break;
        case PeerUp:
            if (!m_gatewayUp) {
                m_gatewayUp = true;
                setStatus(tr("Connected to %1").arg(m_stationName));
                recomputeLinked();
            }
            break;
        case Pong: {
            const int rtt = int(qMax<qint64>(0, nowMs() - qint64(h.tMs)));
            m_rttMs = m_rttMs == 0 ? rtt : (m_rttMs * 7 + rtt * 3) / 10;
            // Anticipo dell'audio da trasmettere: meta' del viaggio di andata e
            // ritorno e un margine per la variazione.
            m_txLeadMs = qBound(80, m_rttMs / 2 + 60, 600);
            m_state.setTxAudioLeadMs(quint16(m_txLeadMs));
            break;
        }
        case Audio:
            handleAudio(h, body);
            break;
        case CatRsp:
            handleCatResponse(h.seq, body);
            break;
        case Denied:
            handleDenied(QString::fromUtf8(body));
            break;
        default:
            break;
        }
    }
}

void DecolinkLink::handleAudio(const Header& h, const QByteArray& body)
{
    const int factor = decimationFactorFor(h.rate);
    if (factor == 0)
        return;     // frequenza che non sappiamo riportare ai 12 kHz del decoder
    if (m_decRate != h.rate) {
        m_dec.setFactor(factor);
        m_decRate = h.rate;
        m_haveRxSeq = false;
    }
    const QVector<short> pcm = pcmToSamples(body);
    if (pcm.isEmpty())
        return;

    int missing = 0;
    if (m_haveRxSeq) {
        const qint32 diff = qint32(h.seq - (m_lastRxSeq + 1));
        if (diff < 0)
            return;                     // vecchio o duplicato
        if (diff > 0 && diff <= 25)
            missing = diff;             // fino a 250 ms si riempie di silenzio
        if (diff > 0)
            ++m_rxGaps;
    }
    m_lastRxSeq = h.seq;
    m_haveRxSeq = true;

    QVector<short> out = m_dec.process(pcm);
    if (out.isEmpty())
        return;
    m_frameSamples = out.size();
    if (missing > 0) {
        // I buchi si riempiono: il decoder lavora sul tempo, e un pacchetto
        // saltato senza sostituto accorcerebbe il periodo e sfaserebbe tutto.
        QVector<short> filled(missing * m_frameSamples, 0);
        filled += out;
        out = filled;
    }
    m_lastAudioMs = nowMs();
    m_rxSamples += out.size();
    const quint64 ts = h.tMs * 1000000ULL;
    emit rxAudioProduced(out, ts, streamId());
    emit rxAudio(out, ts);
}

// ── CAT ────────────────────────────────────────────────────────────────────

void DecolinkLink::sendCat(const QString& line, Kind kind)
{
    if (!m_socket || !m_registered)
        return;
    const quint32 seq = ++m_catSeq;
    m_pending.insert(seq, Pending{kind, nowMs()});
    sendPacket(CatReq, seq, (line + QLatin1Char('\n')).toLatin1());
}

decoport::Mode DecolinkLink::modeFromRigctl(const QString& name)
{
    using decoport::Mode;
    const QString n = name.trimmed().toUpper();
    if (n == QLatin1String("USB"))   return Mode::Usb;
    if (n == QLatin1String("LSB"))   return Mode::Lsb;
    if (n == QLatin1String("CW"))    return Mode::Cw;
    if (n == QLatin1String("CWR") || n == QLatin1String("CW-R")) return Mode::Cwr;
    if (n == QLatin1String("AM") || n == QLatin1String("AMN"))   return Mode::Am;
    if (n == QLatin1String("FM") || n == QLatin1String("FMN"))   return Mode::Fm;
    if (n == QLatin1String("PKTUSB") || n == QLatin1String("DATA-USB")
        || n == QLatin1String("DATA-U") || n == QLatin1String("DIGU"))
        return Mode::Digu;
    if (n == QLatin1String("PKTLSB") || n == QLatin1String("DATA-LSB")
        || n == QLatin1String("DATA-L") || n == QLatin1String("DIGL"))
        return Mode::Digl;
    if (n == QLatin1String("RTTY"))  return Mode::Rtty;
    if (n == QLatin1String("RTTYR") || n == QLatin1String("RTTY-R")) return Mode::Rttyr;
    if (n == QLatin1String("PKTFM")) return Mode::PktFm;
    return Mode::Unknown;
}

QString DecolinkLink::rigctlModeName(decoport::Mode m)
{
    using decoport::Mode;
    switch (m) {
    case Mode::Usb:   return QStringLiteral("USB");
    case Mode::Lsb:   return QStringLiteral("LSB");
    case Mode::Cw:    return QStringLiteral("CW");
    case Mode::Cwr:   return QStringLiteral("CWR");
    case Mode::Am:    return QStringLiteral("AM");
    case Mode::Fm:    return QStringLiteral("FM");
    case Mode::Digu:  return QStringLiteral("PKTUSB");
    case Mode::Digl:  return QStringLiteral("PKTLSB");
    case Mode::Rtty:  return QStringLiteral("RTTY");
    case Mode::Rttyr: return QStringLiteral("RTTYR");
    case Mode::PktFm: return QStringLiteral("PKTFM");
    default:          return QString();
    }
}

void DecolinkLink::handleCatResponse(quint32 seq, const QByteArray& body)
{
    const auto it = m_pending.find(seq);
    if (it == m_pending.end())
        return;                 // una risposta a una domanda che non e' nostra
    const Kind kind = it->kind;
    m_pending.erase(it);
    m_lastCatMs = nowMs();

    const QString text = QString::fromLatin1(body);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.isEmpty() || lines.first().startsWith(QLatin1String("RPRT")))
        return;                 // "RPRT 0" e' un si', "RPRT -1" un no: niente da leggere

    bool changed = false;
    switch (kind) {
    case Kind::Freq: {
        bool ok = false;
        const qint64 hz = lines.first().trimmed().toLongLong(&ok);
        if (ok && hz > 0 && hz != m_state.frequencyHz) {
            m_state.setFrequency(hz);
            changed = true;
        }
        break;
    }
    case Kind::Mode: {
        const decoport::Mode m = modeFromRigctl(lines.first());
        if (m != decoport::Mode::Unknown && m != m_state.mode) {
            m_state.setMode(m);
            changed = true;
        }
        break;
    }
    case Kind::Ptt: {
        const bool on = lines.first().trimmed() == QLatin1String("1");
        if (on != m_state.ptt) {
            m_state.setPtt(on);
            changed = true;
        }
        break;
    }
    case Kind::Strength: {
        bool ok = false;
        const double db = lines.first().trimmed().toDouble(&ok);
        if (ok) {
            m_state.setSMeterDbm(kS9Dbm + db);
            changed = true;
        }
        break;
    }
    default:
        break;
    }
    if (changed)
        emit stateChanged();
}

void DecolinkLink::onPoll()
{
    // Un ascoltatore non puo' interrogare la radio: il relay scarta i suoi
    // comandi CAT in silenzio. Meglio non mandarli.
    if (!isLinked() || !m_canTx)
        return;
    ++m_pollTick;
    sendCat(QStringLiteral("f"), Kind::Freq);
    sendCat(QStringLiteral("m"), Kind::Mode);
    if (m_pollTick % 2 == 0)
        sendCat(QStringLiteral("t"), Kind::Ptt);
    if (m_pollTick % 3 == 0)
        sendCat(QStringLiteral("l STRENGTH"), Kind::Strength);
}

void DecolinkLink::onWatch()
{
    const qint64 now = nowMs();

    // pulizia delle domande senza risposta
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (now - it->sentMs > 3000) it = m_pending.erase(it);
        else ++it;
    }

    if (m_socket && now - m_lastPacketMs > kRelaySilentMs && (m_registered || m_gatewayUp)) {
        m_registered = false;
        m_gatewayUp = false;
        setStatus(tr("The relay is not answering"));
        recomputeLinked();
    } else if (isLinked() && m_lastAudioMs > 0 && now - m_lastAudioMs > 5000) {
        setStatus(tr("No audio from the station"));
    }

    // Lo stato che si vede: audio che arriva, CAT che risponde, poteri.
    quint32 flags = 0;
    if (m_canTx)
        flags |= decoport::StateCanTransmit;
    if (m_lastAudioMs > 0 && now - m_lastAudioMs < 3000)
        flags |= decoport::StateAudioIn;
    if (m_lastCatMs > 0 && now - m_lastCatMs < 5000)
        flags |= decoport::StateCatOnline;
    if (flags != m_state.stateFlags) {
        m_state.setStateFlags(flags);
        emit stateChanged();
    }

    // PTT acceso senza audio che lo giustifichi: si molla. E' la rete di
    // sicurezza lato client; quella lato gateway va aggiunta in Decolink.
    if (m_pttRequested && now - m_pttOnMs > kPttIdleMs && now - m_lastTxPacketMs > kPttIdleMs) {
        qWarning().noquote() << "[Decolink] PTT left on without audio for"
                             << kPttIdleMs << "ms: releasing it";
        releasePttNow();
    }
}

void DecolinkLink::handleDenied(const QString& reason)
{
    qInfo().noquote() << "[Decolink] relay says:" << reason;
    // Dopo un riavvio del relay non ci conosce piu': ci si ripresenta.
    if (reason.contains(QLatin1String("non registrato"))) {
        m_registered = false;
        recomputeLinked();
        sendRegister();
        return;
    }
    // Una credenziale scaduta o non piu' buona si cura rifacendo l'accesso, se la
    // password e' a portata e non lo si e' appena fatto.
    const bool credential = reason.contains(QLatin1String("scaduto"))
                         || reason.contains(QLatin1String("chiave"))
                         || reason.contains(QLatin1String("registrat"));
    if (credential && !m_password.isEmpty() && nowMs() - m_lastRelogin > 10000) {
        setStatus(tr("Credentials expired: logging in again"));
        login(m_authHost, m_email, m_password, m_station.isEmpty() ? m_stationWanted : m_station);
        return;
    }
    setStatus(tr("The relay refused the connection: %1").arg(reason));
    m_registered = false;
    recomputeLinked();
}

// ── comandi ────────────────────────────────────────────────────────────────

void DecolinkLink::setFrequency(qint64 hz)
{
    if (hz <= 0 || !m_canTx || !isLinked())
        return;
    sendCat(QStringLiteral("F %1").arg(hz), Kind::Other);
    m_state.setFrequency(hz);
    emit stateChanged();
    // la verifica: si rilegge la frequenza dopo che la radio ha avuto il tempo
    QTimer::singleShot(400, this, [this]() { sendCat(QStringLiteral("f"), Kind::Freq); });
}

void DecolinkLink::setMode(decoport::Mode mode)
{
    const QString name = rigctlModeName(mode);
    if (name.isEmpty() || !m_canTx || !isLinked())
        return;
    sendCat(QStringLiteral("M %1 0").arg(name), Kind::Other);
    m_state.setMode(mode);
    emit stateChanged();
    QTimer::singleShot(400, this, [this]() { sendCat(QStringLiteral("m"), Kind::Mode); });
}

void DecolinkLink::setModeName(const QString& name)
{
    setMode(modeFromRigctl(name));
}

void DecolinkLink::setPtt(bool on, quint64 whenNs)
{
    if (on && (!m_canTx || !isLinked())) {
        setStatus(m_canTx ? tr("Not connected to the station")
                          : tr("Your access is listen-only: you cannot transmit"));
        return;
    }
    if (!m_socket)
        return;
    if (whenNs > 0) {
        // L'istante e' quello in cui il PTT deve essere alzato: la richiesta parte
        // un po' prima, per quanto ci mette ad arrivare.
        const qint64 nowNs = nowMs() * 1000000LL;
        const qint64 delayMs = (qint64(whenNs) - nowNs) / 1000000LL - m_txLeadMs;
        if (delayMs > 5) {
            QTimer::singleShot(int(delayMs), this, [this, on]() { setPtt(on, 0); });
            return;
        }
    }
    m_pttRequested = on;
    if (on) {
        m_pttOnMs = nowMs();
        m_lastTxPacketMs = m_pttOnMs;
    }
    sendCat(on ? QStringLiteral("T 1") : QStringLiteral("T 0"), Kind::Ptt);
    m_state.setPtt(on);
    emit stateChanged();
}

void DecolinkLink::releasePttNow()
{
    m_pttRequested = false;
    sendCat(QStringLiteral("T 0"), Kind::Ptt);
    m_state.setPtt(false);
    emit stateChanged();
}

// ── trasmissione ───────────────────────────────────────────────────────────

void DecolinkLink::sendTxAudio(const QVector<short>& samples, quint64 playAtNs)
{
    if (samples.isEmpty() || !m_socket || !isLinked())
        return;
    if (!m_canTx) {
        setStatus(tr("Your access is listen-only: you cannot transmit"));
        return;
    }
    // Chi trasmette un segnale lungo lo consegna a pezzi, ognuno col suo istante:
    // se il flusso e' gia' in corso i pezzi si accodano uno dopo l'altro, senza
    // buttare via quelli non ancora partiti e senza riagganciare il tempo.
    if (!m_txSamples.isEmpty()) {
        m_txSamples += samples;
        return;
    }
    m_txSamples = samples;
    m_txPos = 0;
    const qint64 now = nowMs();
    qint64 start = now;
    if (playAtNs > 0) {
        // Il gateway suona quel che arriva, appena arriva: per averlo in aria
        // all'istante voluto il primo pezzo deve partire in anticipo di quanto
        // ci mette il viaggio.
        start = qint64(playAtNs / 1000000ULL) - m_txLeadMs;
        if (start < now)
            start = now;
    }
    m_txStartMs = start;
    m_txTimer->start();
    onTxTick();     // se e' gia' ora, il primo pezzo subito
}

void DecolinkLink::onTxTick()
{
    if (m_txSamples.isEmpty()) {
        m_txTimer->stop();
        return;
    }
    const qint64 now = nowMs();
    if (now < m_txStartMs)
        return;
    // Si mandano i pezzi che a quest'ora dovrebbero gia' essere partiti: un
    // ritardo del timer non accumula scarto e l'audio resta agganciato al suo
    // istante.
    const qint64 due = (now - m_txStartMs) / kTxFrameMs + 1;
    while (m_txPos < m_txSamples.size() && m_txPos / kTxFrameSamples < due) {
        const int n = qMin(kTxFrameSamples, m_txSamples.size() - m_txPos);
        QByteArray body = samplesToPcm(m_txSamples.constData() + m_txPos, n);
        if (n < kTxFrameSamples)
            body.append(QByteArray((kTxFrameSamples - n) * 2, '\0'));
        sendPacket(TxAudio, ++m_txSeq, body, quint32(kDecoderRate));
        m_lastTxPacketMs = now;
        m_txPos += kTxFrameSamples;
    }
    if (m_txPos >= m_txSamples.size()) {
        m_txTimer->stop();
        m_txSamples.clear();
        m_txPos = 0;
    }
}
