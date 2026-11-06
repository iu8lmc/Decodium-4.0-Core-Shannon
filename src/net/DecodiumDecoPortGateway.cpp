#include "DecodiumDecoPortGateway.h"

#include "DecodiumRigDetector.h"

#include <QDateTime>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QTimer>
#include <QUdpSocket>
#include <QtGlobal>

using namespace decoport;

namespace {

constexpr int kContextIntervalMs = 250;
// Cinque firme sbagliate in un minuto e il mittente resta fuori per cinque.
constexpr int    kMaxAuthFailures = 5;
constexpr qint64 kAuthWindowMs = 60000;
constexpr qint64 kAuthBlockMs = 300000;
constexpr int kPlayoutTickMs = 5;
// Un frame TX consegnato con piu' di questo ritardo non si suona: meglio un
// buco che simboli fuori posto.
constexpr qint64 kMaxTxLatenessNs = 40ll * 1000000ll;   // 40 ms

QString modeForApplication(Mode m)
{
    // Il vocabolario che usa l'applicazione. DIGU/DIGL diventano "DATA-U" e
    // "DATA-L": e' il backend CAT, sotto, a tradurli per la radio collegata.
    // Qui non si sa quale sia, ed e' voluto.
    switch (m) {
    case Mode::Usb:   return QStringLiteral("USB");
    case Mode::Lsb:   return QStringLiteral("LSB");
    case Mode::Cw:    return QStringLiteral("CW");
    case Mode::Cwr:   return QStringLiteral("CW-R");
    case Mode::Am:    return QStringLiteral("AM");
    case Mode::Fm:    return QStringLiteral("FM");
    case Mode::Digu:  return QStringLiteral("DATA-U");
    case Mode::Digl:  return QStringLiteral("DATA-L");
    case Mode::Rtty:  return QStringLiteral("RTTY");
    case Mode::Rttyr: return QStringLiteral("RTTY-R");
    case Mode::PktFm: return QStringLiteral("PKT-FM");
    case Mode::Unknown: break;
    }
    return QString();
}

} // namespace

DecodiumDecoPortGateway::DecodiumDecoPortGateway(QObject* parent)
    : QObject(parent)
{
    m_streamId = QRandomGenerator::global()->generate();
    if (m_streamId == 0)
        m_streamId = 1;
    m_status = tr("stopped");
}

DecodiumDecoPortGateway::~DecodiumDecoPortGateway()
{
    stop();
}

QString DecodiumDecoPortGateway::clientKey(const QHostAddress& addr, quint16 port)
{
    return addr.toString() + QLatin1Char(':') + QString::number(port);
}

void DecodiumDecoPortGateway::setStatus(const QString& s)
{
    if (m_status == s)
        return;
    m_status = s;
    emit statusChanged();
}

bool DecodiumDecoPortGateway::start(int sessionPort)
{
    if (m_running)
        return true;

    // Fallire chiuso: senza password non si pubblica niente.
    if (m_authKey.isEmpty()) {
        setStatus(tr("set a DecoPort password before publishing the radio"));
        return false;
    }

    m_sessionPort = (sessionPort > 0 && sessionPort < 65536) ? sessionPort : kSessionPort;

    m_session = new QUdpSocket(this);
    if (!m_session->bind(QHostAddress::AnyIPv4, static_cast<quint16>(m_sessionPort),
                         QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        setStatus(tr("cannot bind session port %1: %2")
                      .arg(m_sessionPort).arg(m_session->errorString()));
        delete m_session;
        m_session = nullptr;
        return false;
    }
    connect(m_session, &QUdpSocket::readyRead, this, &DecodiumDecoPortGateway::onSessionDatagrams);

    // Il socket di annuncio serve solo a trasmettere: non lo si lega a nessuna
    // porta, cosi' non litiga con eventuali altri gateway sulla stessa macchina.
    m_announce = new QUdpSocket(this);

    refreshDetection();

    m_announceTimer = new QTimer(this);
    m_announceTimer->setInterval(kAnnounceIntervalMs);
    connect(m_announceTimer, &QTimer::timeout, this, &DecodiumDecoPortGateway::onAnnounceTick);
    m_announceTimer->start();

    m_contextTimer = new QTimer(this);
    m_contextTimer->setInterval(kContextIntervalMs);
    connect(m_contextTimer, &QTimer::timeout, this, &DecodiumDecoPortGateway::onContextTick);
    m_contextTimer->start();

    m_playoutTimer = new QTimer(this);
    m_playoutTimer->setInterval(kPlayoutTickMs);
    connect(m_playoutTimer, &QTimer::timeout, this, &DecodiumDecoPortGateway::onPlayoutTick);
    m_playoutTimer->start();

    // Il conto riparte da zero a ogni pubblicazione: i numeri di ieri non
    // dicono niente su perche' oggi non si collega nessuno.
    m_traffico = Traffico {};
    m_trafficoSporco = false;
    m_traficoDaRegistrare = false;
    m_traficoUltimoLogMs = 0;   // la prima riga della nuova pubblicazione esce subito
    emit trafficoChanged();

    m_running = true;
    emit runningChanged();
    onAnnounceTick();
    return true;
}

void DecodiumDecoPortGateway::stop()
{
    if (!m_running && !m_session)
        return;

    for (QTimer** t : {&m_announceTimer, &m_contextTimer, &m_playoutTimer}) {
        if (*t) {
            (*t)->stop();
            (*t)->deleteLater();
            *t = nullptr;
        }
    }
    if (m_session) { m_session->close();  m_session->deleteLater();  m_session = nullptr; }
    if (m_announce) { m_announce->close(); m_announce->deleteLater(); m_announce = nullptr; }

    m_clients.clear();
    m_txQueue.clear();
    m_running = false;
    setStatus(tr("stopped"));
    emit clientsChanged();
    emit runningChanged();
}

// Nessuna scelta chiesta all'operatore: si guarda soltanto cosa dice il sistema
// sulle porte e sulle schede audio, e si prende il candidato piu' attendibile.
// E' una lettura passiva: non apre porte, non manda comandi, quindi e' sicura
// anche con il CAT connesso e con la radio in trasmissione.
void DecodiumDecoPortGateway::refreshDetection()
{
    QVariantList const candidates = DecodiumRigDetector::detect();
    if (candidates.isEmpty()) {
        m_detected.clear();
        m_rigLabel = tr("no radio found");
        m_audioInputName.clear();
        m_audioOutputName.clear();
        emit radioChanged();
        setStatus(tr("no USB radio detected"));
        return;
    }

    m_detected = candidates.first().toMap();
    m_rigLabel = m_detected.value(QStringLiteral("rigLabel")).toString();
    m_audioInputName = m_detected.value(QStringLiteral("audioInput")).toString();
    m_audioOutputName = m_detected.value(QStringLiteral("audioOutput")).toString();
    emit radioChanged();

    bool const catUp = m_hooks.connected && m_hooks.connected();
    setStatus(catUp ? tr("publishing %1").arg(m_rigLabel)
                    : tr("found %1, waiting for the CAT").arg(m_rigLabel));
}

Context DecodiumDecoPortGateway::buildContext() const
{
    Context ctx;
    ctx.setSampleRate(m_sampleRate);
    ctx.setChannels(m_channels);
    ctx.setSessionPort(static_cast<quint16>(m_sessionPort));
    ctx.setTxAudioLeadMs(m_txAudioLeadMs);
    ctx.setRigLabel(m_rigLabel.isEmpty() ? tr("unknown radio") : m_rigLabel);

    quint32 flags = 0;
    if (m_hooks.connected && m_hooks.connected()) {
        flags |= StateCatOnline;
        if (m_hooks.frequencyHz)
            ctx.setFrequency(static_cast<qint64>(m_hooks.frequencyHz()));
        if (m_hooks.modeName)
            ctx.setMode(modeFromString(m_hooks.modeName()));
        if (m_hooks.pttActive)
            ctx.setPtt(m_hooks.pttActive());
        if (m_hooks.canTransmit && m_hooks.canTransmit())
            flags |= StateCanTransmit;

        // Gli strumenti: si chiedono tutti, si mandano solo quelli che hanno
        // risposto. Il gancio assente e la lettura fallita finiscono nello
        // stesso posto — il bit resta spento — ed e' giusto cosi': per chi
        // guarda il quadrante non c'e' differenza fra una radio che il ROS non
        // lo misura e una che in questo istante non lo sa dire.
        auto const misura = [&ctx](const std::function<bool(double&)>& leggi,
                                         void (Context::*posa)(double)) {
            double v = 0.0;
            if (leggi && leggi(v))
                (ctx.*posa)(v);
        };
        misura(m_hooks.sMeterDbm,       &Context::setSMeterDbm);
        misura(m_hooks.forwardPowerW,   &Context::setForwardPowerW);
        misura(m_hooks.swr,             &Context::setSwr);
        misura(m_hooks.alcPct,          &Context::setAlcPct);
        misura(m_hooks.drainVoltage,    &Context::setDrainVoltage);
        misura(m_hooks.drainCurrent,    &Context::setDrainCurrent);
        misura(m_hooks.paTemperature,   &Context::setPaTemperature);
        misura(m_hooks.compressionDb,   &Context::setCompressionDb);
        misura(m_hooks.powerSettingPct, &Context::setPowerSettingPct);
    }
    if (!m_audioInputName.isEmpty())
        flags |= StateAudioIn;
    if (!m_audioOutputName.isEmpty())
        flags |= StateAudioOut;
    ctx.setStateFlags(flags);
    return ctx;
}

void DecodiumDecoPortGateway::sendTo(const Client& c, Type type, const QByteArray& payload, quint64 tsNs)
{
    if (!m_session)
        return;
    quint32 seq = 0;
    switch (type) {
    case Type::Context: seq = m_contextSeq++; break;
    case Type::Status:  seq = m_statusSeq++;  break;
    default:            seq = 0;              break;
    }
    QByteArray const pkt = buildPacket(type, m_streamId, seq, tsNs, payload, m_authKey);
    m_session->writeDatagram(pkt, c.address, c.port);
}

bool DecodiumDecoPortGateway::isBlocked(const QString& key, qint64 nowMs) const
{
    auto const it = m_authFailures.constFind(key);
    return it != m_authFailures.constEnd() && it->blockedUntilMs > nowMs;
}

void DecodiumDecoPortGateway::noteAuthFailure(const QString& key,
                                              const QHostAddress& addr,
                                              qint64 nowMs)
{
    AuthFailures& f = m_authFailures[key];
    if (f.windowStartMs == 0 || nowMs - f.windowStartMs > kAuthWindowMs) {
        f.windowStartMs = nowMs;
        f.count = 0;
    }
    ++f.count;
    if (f.count >= kMaxAuthFailures && f.blockedUntilMs <= nowMs) {
        f.blockedUntilMs = nowMs + kAuthBlockMs;
        qWarning().noquote()
            << "[DecoPort] rifiutato" << addr.toString()
            << "- firme non valide:" << f.count
            << "- bloccato per" << (kAuthBlockMs / 1000) << "s";
    }
}

void DecodiumDecoPortGateway::broadcastToClients(Type type, const QByteArray& payload, quint64 tsNs)
{
    for (const Client& c : std::as_const(m_clients))
        sendTo(c, type, payload, tsNs);
}

void DecodiumDecoPortGateway::onAnnounceTick()
{
    if (!m_announce)
        return;
    QByteArray const payload = encodeContextPayload(buildContext());
    QByteArray const pkt = buildPacket(Type::Announce, m_streamId, m_announceSeq++,
                                       nowUnixNs(), payload, m_authKey);

    // Un broadcast generico esce da UNA sola interfaccia, quella che decide la
    // tabella di instradamento — e su un PC con VirtualBox, una VPN o due schede
    // quella non e' la rete di casa. Si annuncia sul broadcast di OGNI
    // interfaccia attiva, altrimenti il secondo PC non vede mai niente.
    int sent = 0;
    const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : ifaces) {
        QNetworkInterface::InterfaceFlags const f = iface.flags();
        if (!f.testFlag(QNetworkInterface::IsUp)
            || !f.testFlag(QNetworkInterface::IsRunning)
            || f.testFlag(QNetworkInterface::IsLoopBack)
            || !f.testFlag(QNetworkInterface::CanBroadcast)) {
            continue;
        }
        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& e : entries) {
            QHostAddress const bcast = e.broadcast();
            if (bcast.isNull() || e.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            m_announce->writeDatagram(pkt, bcast, kAnnouncePort);
            ++sent;
        }
    }
    if (sent == 0) {
        // Nessuna interfaccia utilizzabile: resta il broadcast generico, che su
        // una macchina con una scheda sola funziona benissimo.
        m_announce->writeDatagram(pkt, QHostAddress::Broadcast, kAnnouncePort);
    }
}

void DecodiumDecoPortGateway::onContextTick()
{
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();

    // I contatori si pubblicano qui e non a ogni datagramma: sotto carico
    // sarebbero migliaia di segnali al secondo. Sta prima del ritorno
    // anticipato qui sotto, perche' il caso che interessa davvero e'
    // proprio quello senza clienti — nessuno collegato, e i numeri che
    // dicono se e' perche' non arriva niente o perche' li rifiutiamo tutti.
    if (m_trafficoSporco) {
        m_trafficoSporco = false;
        m_traficoDaRegistrare = true;
        emit trafficoChanged();

        // Una riga nel log, al massimo una al minuto. La finestra mostra gli
        // stessi numeri dal vivo, ma chi diagnostica dopo — o da lontano — ha
        // solo il log: senza questa riga, "non si collega nessuno" resta
        // indistinguibile da "arrivano e li rifiuto tutti" anche a posteriori.
        // Al minuto e non a ogni cambiamento, perche' un client attivo manda
        // decine di datagrammi al secondo e riempirebbe il log da solo.
    }

    // Il freno sul log rimanda la riga, non la butta via. Tenerlo insieme al
    // flag della finestra faceva scrivere il conto del PRIMO datagramma e mai
    // piu': una raffica di nove risultava "ricevuti 1" per sempre, e un numero
    // falso nel log e' peggio di nessun numero. Percio' due flag distinti —
    // quello della finestra si azzera a ogni aggiornamento, questo solo quando
    // la riga e' stata scritta davvero.
    if (m_traficoDaRegistrare) {
        if (nowMs - m_traficoUltimoLogMs >= 60000) {
            m_traficoDaRegistrare = false;
            m_traficoUltimoLogMs = nowMs;
            qInfo().noquote()
                << "[DecoPort] traffico: ricevuti" << m_traffico.ricevuti
                << "accettati" << m_traffico.accettati
                << "- respinti: firma" << m_traffico.firmaErrata
                << "tempo" << m_traffico.fuoriTempo
                << "malformati" << m_traffico.malformati
                << "bloccati" << m_traffico.daBloccati
                << "- ultimo da" << (m_traffico.ultimoMittente.isEmpty()
                                         ? QStringLiteral("nessuno")
                                         : m_traffico.ultimoMittente);
        }
    }

    reapClients(nowMs);
    if (m_clients.isEmpty())
        return;
    broadcastToClients(Type::Context, encodeContextPayload(buildContext()), nowUnixNs());
}

QVariantMap DecodiumDecoPortGateway::traffico() const
{
    QVariantMap m;
    m.insert(QStringLiteral("ricevuti"),    static_cast<qulonglong>(m_traffico.ricevuti));
    m.insert(QStringLiteral("accettati"),   static_cast<qulonglong>(m_traffico.accettati));
    m.insert(QStringLiteral("daBloccati"),  static_cast<qulonglong>(m_traffico.daBloccati));
    m.insert(QStringLiteral("malformati"),  static_cast<qulonglong>(m_traffico.malformati));
    m.insert(QStringLiteral("firmaErrata"), static_cast<qulonglong>(m_traffico.firmaErrata));
    m.insert(QStringLiteral("fuoriTempo"),  static_cast<qulonglong>(m_traffico.fuoriTempo));
    m.insert(QStringLiteral("ultimoMittente"), m_traffico.ultimoMittente);
    // Secondi dall'ultimo datagramma, -1 se non ne e' mai arrivato uno. Si
    // manda un'eta' e non un istante: il QML non deve sapere di orologi, e un
    // "mai" deve restare distinguibile da "adesso".
    m.insert(QStringLiteral("ultimoDaSecondi"),
             m_traffico.ultimoMs > 0
                 ? static_cast<qlonglong>((QDateTime::currentMSecsSinceEpoch() - m_traffico.ultimoMs) / 1000)
                 : qlonglong {-1});
    return m;
}

void DecodiumDecoPortGateway::reapClients(qint64 nowMs)
{
    QStringList stale;
    for (auto it = m_clients.constBegin(); it != m_clients.constEnd(); ++it) {
        if (nowMs - it.value().lastSeenMs > kClientTimeoutMs)
            stale << it.key();
    }
    if (stale.isEmpty())
        return;
    for (const QString& k : stale)
        m_clients.remove(k);
    emit clientsChanged();
}

void DecodiumDecoPortGateway::touchClient(const QHostAddress& addr, quint16 port)
{
    QString const key = clientKey(addr, port);
    auto it = m_clients.find(key);
    if (it == m_clients.end()) {
        Client c;
        c.address = addr;
        c.port = port;
        c.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
        m_clients.insert(key, c);
        emit clientsChanged();
        return;
    }
    it->lastSeenMs = QDateTime::currentMSecsSinceEpoch();
}

void DecodiumDecoPortGateway::onSessionDatagrams()
{
    while (m_session && m_session->hasPendingDatagrams()) {
        QNetworkDatagram const dg = m_session->receiveDatagram();
        QHostAddress const from = dg.senderAddress();
        quint16 const fromPort = static_cast<quint16>(dg.senderPort());
        QString const peerKey = from.toString();
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();

        // Si conta prima di ogni giudizio: il numero che conta davvero e'
        // quanti ne arrivano. Se resta a zero mentre il client sta provando,
        // il problema e' prima del gateway — rete, firewall, porta — e nessuna
        // password lo risolverebbe.
        ++m_traffico.ricevuti;
        m_traffico.ultimoMittente = peerKey;
        m_traffico.ultimoMs = nowMs;
        m_trafficoSporco = true;

        if (isBlocked(peerKey, nowMs)) {
            ++m_traffico.daBloccati;
            continue;   // in castigo: nemmeno si guarda cosa ha mandato
        }

        Header h;
        QByteArray payload;
        bool authed = false;
        if (!parsePacket(dg.data(), &h, &payload, m_authKey, &authed)) {
            ++m_traffico.malformati;
            continue;   // non e' roba nostra, o e' malformata
        }

        // Firma assente o sbagliata: si conta e si tace. Rispondere qualcosa
        // direbbe a chi prova che il pacchetto e' arrivato a destinazione.
        if (!authed) {
            ++m_traffico.firmaErrata;
            noteAuthFailure(peerKey, from, nowMs);
            continue;
        }
        // Firma giusta ma timestamp fuori finestra: e' un pacchetto registrato
        // e rigiocato. La firma da sola non protegge da questo.
        if (!timestampAcceptable(h)) {
            ++m_traffico.fuoriTempo;
            qWarning().noquote() << "[DecoPort] scartato pacchetto fuori tempo da"
                                 << from.toString();
            continue;
        }

        ++m_traffico.accettati;

        switch (h.type) {
        case Type::Hello:
            touchClient(from, fromPort);
            if (auto it = m_clients.find(clientKey(from, fromPort)); it != m_clients.end())
                sendTo(*it, Type::Context, encodeContextPayload(buildContext()), nowUnixNs());
            break;
        case Type::KeepAlive:
            touchClient(from, fromPort);
            break;
        case Type::Bye:
            if (m_clients.remove(clientKey(from, fromPort)) > 0)
                emit clientsChanged();
            break;
        case Type::Command:
            touchClient(from, fromPort);
            handleCommand(h, payload, from, fromPort);
            break;
        case Type::AudioTx:
            touchClient(from, fromPort);
            handleAudioTx(h, payload);
            break;
        default:
            break;   // ANNOUNCE/CONTEXT/STATUS/AUDIO_RX arrivano dal gateway, non a lui
        }
    }
}

void DecodiumDecoPortGateway::handleCommand(const Header& h, const QByteArray& payload,
                                            const QHostAddress& from, quint16 fromPort)
{
    Context cmd;
    if (!decodeContextPayload(payload, &cmd))
        return;

    bool const catUp = m_hooks.connected && m_hooks.connected();
    if (catUp) {
        if (cmd.has(FieldFrequency) && cmd.frequencyHz > 0 && m_hooks.setFrequencyHz)
            m_hooks.setFrequencyHz(static_cast<double>(cmd.frequencyHz));
        if (cmd.has(FieldMode) && cmd.mode != Mode::Unknown && m_hooks.setModeName) {
            QString const name = modeForApplication(cmd.mode);
            if (!name.isEmpty())
                m_hooks.setModeName(name);
        }
    }

    if (cmd.has(FieldPtt)) {
        // Il PTT porta l'istante in cui deve avvenire. Se e' nel futuro lo si
        // aspetta: chiudere il PTT quando arriva il pacchetto invece che quando
        // dice il timestamp rimette in gioco tutto il jitter che i timestamp
        // servono a togliere.
        quint64 const dueNs = (h.flags & FlagHasTimestamp)
                                  ? (static_cast<quint64>(h.tsSeconds) * 1000000000ull + h.tsNanos)
                                  : 0ull;
        quint64 const now = nowUnixNs();
        bool const on = cmd.ptt;
        auto applyPtt = [this](bool state) {
            if (m_hooks.setPtt && (!m_hooks.canTransmit || m_hooks.canTransmit()))
                m_hooks.setPtt(state);
            emit txKeyRequested(state);
        };
        if (dueNs > now && (dueNs - now) < 5000000000ull) {
            int const delayMs = static_cast<int>((dueNs - now) / 1000000ull);
            QTimer::singleShot(delayMs, this, [applyPtt, on]() { applyPtt(on); });
        } else {
            applyPtt(on);
        }
    }

    // Un riscontro immediato: il client non deve aspettare il prossimo contesto
    // periodico per sapere com'e' finita.
    if (auto it = m_clients.find(clientKey(from, fromPort)); it != m_clients.end())
        sendTo(*it, Type::Status, encodeContextPayload(buildContext()), nowUnixNs());
}

void DecodiumDecoPortGateway::handleAudioTx(const Header& h, const QByteArray& payload)
{
    if (payload.isEmpty() || (payload.size() % 2) != 0)
        return;

    int const count = payload.size() / 2;
    QVector<short> samples(count);
    // PCM int16 little-endian, come lo scrivono le schede audio.
    const uchar* p = reinterpret_cast<const uchar*>(payload.constData());
    for (int i = 0; i < count; ++i)
        samples[i] = static_cast<short>(static_cast<quint16>(p[2 * i]) |
                                        (static_cast<quint16>(p[2 * i + 1]) << 8));

    quint64 const dueNs = (h.flags & FlagHasTimestamp)
                              ? (static_cast<quint64>(h.tsSeconds) * 1000000000ull + h.tsNanos)
                              : nowUnixNs();
    quint64 const now = nowUnixNs();
    if (dueNs + kMaxTxLatenessNs < now) {
        ++m_txLateFrames;   // arrivato troppo tardi: si conta e si butta
        return;
    }

    PendingTx item;
    item.dueNs = dueNs;
    item.samples = std::move(samples);
    // Inserimento ordinato: i datagrammi UDP possono scavalcarsi.
    int pos = m_txQueue.size();
    while (pos > 0 && m_txQueue[pos - 1].dueNs > item.dueNs)
        --pos;
    m_txQueue.insert(pos, std::move(item));
}

void DecodiumDecoPortGateway::onPlayoutTick()
{
    if (m_txQueue.isEmpty())
        return;
    quint64 const now = nowUnixNs();
    while (!m_txQueue.isEmpty() && m_txQueue.first().dueNs <= now) {
        PendingTx item = m_txQueue.takeFirst();
        ++m_txPlayedFrames;
        emit txAudioDue(item.samples);
    }
}

// Gli indirizzi IPv4 su cui questo gateway risponde, dal piu' probabile al
// meno. Si scartano loopback e gli auto-assegnati 169.254 (che significano
// "nessun DHCP"), e si mettono in fondo i 192.168.56 di VirtualBox: sono una
// rete verso una macchina virtuale, non verso l'altro PC della stazione.
QStringList DecodiumDecoPortGateway::addresses() const
{
    QStringList good;
    QStringList unlikely;
    const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : ifaces) {
        QNetworkInterface::InterfaceFlags const f = iface.flags();
        if (!f.testFlag(QNetworkInterface::IsUp)
            || !f.testFlag(QNetworkInterface::IsRunning)
            || f.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& e : entries) {
            QHostAddress const ip = e.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            QString const text = ip.toString();
            if (text.startsWith(QLatin1String("169.254.")))
                continue;
            if (text.startsWith(QLatin1String("192.168.56.")))
                unlikely << text;
            else
                good << text;
        }
    }
    return good + unlikely;
}

QString DecodiumDecoPortGateway::primaryAddress() const
{
    const QStringList all = addresses();
    return all.isEmpty() ? QString() : all.first();
}

void DecodiumDecoPortGateway::setAudioFormat(quint32 sampleRate, quint8 channels)
{
    if (sampleRate >= 4000 && sampleRate <= 384000)
        m_sampleRate = sampleRate;
    if (channels >= 1 && channels <= 2)
        m_channels = channels;
}

void DecodiumDecoPortGateway::pushRxAudio(const QVector<short>& samples, quint64 captureTsNs)
{
    if (!m_running || !m_session)
        return;
    if (m_clients.isEmpty()) {
        // Nessuno in ascolto: non si accumula, altrimenti al primo client che
        // arriva gli si rovescia addosso audio vecchio di minuti.
        m_rxAccum.clear();
        return;
    }
    if (samples.isEmpty())
        return;

    m_rxAccum += samples;

    int const frameSamples = qMax(1, static_cast<int>(m_sampleRate) * kAudioFrameMs / 1000);
    quint64 const arrivalNs = captureTsNs > 0 ? captureTsNs : nowUnixNs();

    while (m_rxAccum.size() >= frameSamples) {
        QVector<short> frame = m_rxAccum.mid(0, frameSamples);
        m_rxAccum.remove(0, frameSamples);

        QByteArray payload;
        payload.resize(frameSamples * 2);
        uchar* p = reinterpret_cast<uchar*>(payload.data());
        for (int i = 0; i < frameSamples; ++i) {
            quint16 const v = static_cast<quint16>(frame[i]);
            p[2 * i]     = static_cast<uchar>(v & 0xFF);
            p[2 * i + 1] = static_cast<uchar>((v >> 8) & 0xFF);
        }

        // Il timestamp e' l'istante di CATTURA del primo campione del frame,
        // non quello di spedizione: e' quello che permette al client di
        // misurare la deriva fra il quarzo della scheda e il proprio.
        quint64 const backlogNs = static_cast<quint64>(m_rxAccum.size())
                                * 1000000000ull / qMax<quint32>(1, m_sampleRate);
        quint64 const ts = arrivalNs > backlogNs ? arrivalNs - backlogNs : arrivalNs;

        for (Client& c : m_clients) {
            QByteArray const pkt = buildPacket(Type::AudioRx, m_streamId, c.rxSequence++,
                                               ts, payload, m_authKey);
            m_session->writeDatagram(pkt, c.address, c.port);
        }
        ++m_rxFramesSent;
    }
}
