#include "DecodiumCatShare.h"

#include "../cw/CwKeyer.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QStringList>
#include <QtGlobal>

namespace {

// Risposte del protocollo. RPRT 0 = accettato; -1 = rifiutato; -11 = non
// implementato. Sono i codici che i client Hamlib si aspettano.
const char* const kOk        = "RPRT 0\n";
const char* const kRefused   = "RPRT -1\n";
const char* const kNotImpl   = "RPRT -11\n";

} // namespace


DecodiumCatShare::DecodiumCatShare(Hooks hooks, QObject* parent)
    : QObject(parent)
    , m_hooks(std::move(hooks))
{
    m_morseTimer.setSingleShot(true);
    connect(&m_morseTimer, &QTimer::timeout, this, [this]() {
        if (m_morseWaiting) {
            qWarning() << "[CATSHARE] CW command timed out; stopping shared keyer";
            stopMorse();
        } else if (!m_morseText.isEmpty()) {
            sendNextMorseChunk();
        } else {
            m_morseOwner.clear();
            m_morseChunk.clear();
        }
    });
}

DecodiumCatShare::~DecodiumCatShare()
{
    configure(false, m_port, false, false);
}

bool DecodiumCatShare::listening() const
{
    return m_server && m_server->isListening();
}

QString DecodiumCatShare::status() const
{
    if (!m_enabled)
        return QStringLiteral("off");
    if (!listening())
        return m_lastError.isEmpty() ? QStringLiteral("error") : m_lastError;
    return QStringLiteral("listening");
}

bool DecodiumCatShare::configure(bool enabled, int port, bool allowControl, bool allowPtt)
{
    bool const changed = (enabled != m_enabled) || (port != m_port)
                         || (allowControl != m_allowControl) || (allowPtt != m_allowPtt);
    bool const restart = (enabled != m_enabled) || (port != m_port);

    m_allowControl = allowControl;
    // La trasmissione ha un interruttore proprio: cambiare frequenza a una
    // radio altrui e' un fastidio, mandarla in aria e' un'altra cosa. Se il
    // controllo e' disabilitato, il PTT lo e' comunque.
    m_allowPtt = allowPtt && allowControl;
    m_port = port;
    m_enabled = enabled;

    if (m_morseOwner && (restart || !m_allowPtt))
        stopMorse();

    if (restart) {
        if (m_server) {
            // disconnectFromHost can synchronously remove an entry.
            const auto clients = m_buffers.keys();
            for (auto* client : clients)
                client->disconnectFromHost();
            m_buffers.clear();
            m_replies.clear();
            m_server->close();
            m_server->deleteLater();
            m_server = nullptr;
            emit clientsChanged();
        }
        m_lastError.clear();

        if (m_enabled) {
            m_server = new QTcpServer(this);
            connect(m_server, &QTcpServer::newConnection,
                    this, &DecodiumCatShare::onNewConnection);
            // Solo loopback: la condivisione in rete locale richiederebbe una
            // lista di indirizzi ammessi, e quella verso Internet
            // autenticazione e cifratura. Nessuna delle due e' un primo passo.
            if (!m_server->listen(QHostAddress::LocalHost, quint16(m_port))) {
                m_lastError = m_server->errorString();
                qWarning().noquote() << "[CATSHARE] apertura porta" << m_port
                                     << "fallita:" << m_lastError;
                m_server->deleteLater();
                m_server = nullptr;
                emit listeningChanged();
                if (changed) emit configChanged();
                return false;
            }
            qInfo().noquote() << "[CATSHARE] in ascolto su 127.0.0.1:" << m_port
                              << "controllo=" << m_allowControl
                              << "ptt=" << m_allowPtt;
        } else {
            qInfo().noquote() << "[CATSHARE] condivisione disattivata";
        }
        emit listeningChanged();
    }

    if (changed)
        emit configChanged();
    return true;
}

// ------------------------------------------------------------------ connessioni
void DecodiumCatShare::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* s = m_server->nextPendingConnection();
        if (!s) break;
        m_buffers.insert(s, QByteArray());
        connect(s, &QTcpSocket::readyRead, this, &DecodiumCatShare::onReadyRead);
        connect(s, &QTcpSocket::disconnected, this, &DecodiumCatShare::onDisconnected);
        qInfo().noquote() << "[CATSHARE] client collegato:"
                          << s->peerAddress().toString() << "totale" << m_buffers.size();
        emit clientsChanged();
    }
}

void DecodiumCatShare::onDisconnected()
{
    auto* s = qobject_cast<QTcpSocket*>(sender());
    if (!s) return;
    if (m_morseOwner == s) stopMorse();
    m_buffers.remove(s);
    m_replies.remove(s);
    s->deleteLater();
    qInfo().noquote() << "[CATSHARE] client scollegato, restano" << m_buffers.size();
    emit clientsChanged();
}

void DecodiumCatShare::onReadyRead()
{
    auto* s = qobject_cast<QTcpSocket*>(sender());
    if (!s) return;
    auto it = m_buffers.find(s);
    if (it == m_buffers.end()) return;

    it.value().append(s->readAll());
    // Un client che non manda mai una riga intera non deve far crescere il
    // buffer senza fine.
    if (it.value().size() > 8192) {
        s->disconnectFromHost();
        return;
    }

    while (m_buffers.contains(s)) {
        auto& buffer = m_buffers[s];
        const int nl = buffer.indexOf('\n');
        if (nl < 0) break;
        QByteArray const raw = buffer.left(nl);
        buffer.remove(0, nl + 1);
        QString const line = QString::fromLatin1(raw).trimmed();
        if (line.isEmpty())
            continue;
        if (m_replies[s].size() >= 64) {
            s->disconnectFromHost();
            return;
        }
        const quint64 id = ++m_replyId;
        m_replies[s].enqueue({id, {}, false});
        QPointer<DecodiumCatShare> self(this);
        QPointer<QTcpSocket> socket(s);
        auto completed = [self, socket, id](const QString& reply) {
            if (self && socket) self->finishReply(socket, id, reply);
        };
        if (handleMorse(s, line, completed)) continue;
        QString const reply = handleLine(line);
        if (reply.isNull()) {          // comando di chiusura
            // rigctld risponde RPRT 0 anche a "q", e il client di Hamlib lo
            // aspetta: senza, registra un errore di chiusura che non c'e'.
            completed(QString::fromLatin1(kOk));
            s->disconnectFromHost();
            return;
        }
        completed(reply);
    }
}

void DecodiumCatShare::finishReply(QTcpSocket* socket, quint64 id, const QString& text)
{
    auto it = m_replies.find(socket);
    if (it == m_replies.end()) return;
    for (auto& reply : it.value()) {
        if (reply.id == id) { reply.text = text; reply.ready = true; break; }
    }
    // CAT completions are asynchronous, but replies must stay in wire order.
    while (!it.value().isEmpty() && it.value().head().ready)
        socket->write(it.value().dequeue().text.toLatin1());
}

bool DecodiumCatShare::handleMorse(QTcpSocket* socket, const QString& line,
                                  std::function<void(QString)> reply)
{
    QString cleaned = line;
    const bool extended = cleaned.startsWith(QLatin1Char('+'));
    while (cleaned.startsWith(QLatin1Char('+')) || cleaned.startsWith(QLatin1Char(';')))
        cleaned.remove(0, 1);
    const auto parts = cleaned.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;
    const auto cmd = parts.first();
    const bool send = cmd == QLatin1String("b") || cmd == QLatin1String("\\send_morse");
    const bool stop = cmd == QLatin1String("\\stop_morse") || cmd == QString(QChar(0xbb))
        || ((cmd == QLatin1String("T") || cmd == QLatin1String("\\set_ptt"))
            && parts.size() == 2 && parts.last() == QLatin1String("0"));
    const bool readSpeed = (cmd == QLatin1String("l") || cmd == QLatin1String("\\get_level"))
        && parts.size() >= 2 && parts.at(1).compare(QLatin1String("KEYSPD"), Qt::CaseInsensitive) == 0;
    const bool writeSpeed = (cmd == QLatin1String("L") || cmd == QLatin1String("\\set_level"))
        && parts.size() >= 2 && parts.at(1).compare(QLatin1String("KEYSPD"), Qt::CaseInsensitive) == 0;
    if (!send && !stop && !readSpeed && !writeSpeed) return false;

    auto result = [reply](int rc) { reply(QStringLiteral("RPRT %1\n").arg(rc)); };
    if (!readSpeed && refuseWrite(cmd, send || stop)) { result(-1); return true; }
    const State state = m_hooks.state ? m_hooks.state() : State{};
    if (!state.connected) { result(-6); return true; }
    if (stop) { stopMorse(result); return true; }
    if (readSpeed || writeSpeed) {
        bool valid = false;
        const int wpm = parts.last().toInt(&valid);
        if (writeSpeed && (parts.size() != 3 || !valid || wpm < 5 || wpm > 60)) {
            result(-1); return true;
        }
        if (!m_hooks.morseSpeed) { result(-11); return true; }
        QPointer<DecodiumCatShare> self(this);
        m_hooks.morseSpeed(writeSpeed, wpm, [self, reply, writeSpeed, readSpeed, extended](int rc, int speed) {
            if (!self) return;
            if (writeSpeed && rc == 0 && self->m_morseOwner && !self->m_morseWaiting
                && !self->m_morseChunk.isEmpty()) {
                // A slower speed mid-chunk must not let the next chunk
                // overwrite a still-playing finite radio buffer.
                self->m_morseTimer.start(qMax(self->m_morseTimer.remainingTime(),
                    decodium::cw::CwKeyer::millisFor(self->m_morseChunk, qBound(5, speed, 60)) + 720));
            }
            if (readSpeed && rc == 0) {
                reply(extended ? QStringLiteral("get_level: KEYSPD\nLevel Value: %1\nRPRT 0\n").arg(speed)
                               : QStringLiteral("%1\n").arg(speed));
            } else {
                reply(QStringLiteral("RPRT %1\n").arg(rc));
            }
        });
        return true;
    }
    if (!m_hooks.morseAvailable || !m_hooks.morseAvailable() || !m_hooks.sendMorse) {
        result(-11); return true;
    }
    const QString text = cleaned.mid(cmd.size()).simplified().toUpper();
    if (text.isEmpty() || text.size() + m_morseText.size() > 4096) { result(-1); return true; }
    for (const QChar c : text) {
        if (c != QLatin1Char(' ') && decodium::cw::CwKeyer::morseOf(c).isEmpty()) {
            result(-1); return true;
        }
    }
    // Never interleave messages from two clients into one physical keyer.
    if (m_morseOwner && m_morseOwner != socket) { result(-9); return true; }
    if (m_morseOwner) {
        m_morseText += QLatin1Char(' ') + text;
        result(0); // accepted into this client's bounded keyer queue
    } else {
        m_morseOwner = socket;
        m_morseText = text;
        m_morseAccepted = std::move(reply);
        sendNextMorseChunk();
    }
    return true;
}

void DecodiumCatShare::sendNextMorseChunk()
{
    if (!m_morseOwner || m_morseText.isEmpty()) return;
    const State state = m_hooks.state ? m_hooks.state() : State{};
    if (!m_allowPtt || !state.connected || !m_hooks.morseAvailable || !m_hooks.morseAvailable()) {
        stopMorse(); return;
    }
    const int limit = m_hooks.morseTextLimit ? m_hooks.morseTextLimit() : 0;
    // An unknown downstream NET rigctl keyer may also have a finite FIFO.
    const int count = qMin(limit > 0 ? limit : 30, int(m_morseText.size()));
    m_morseChunk = m_morseText.left(count);
    m_morseText.remove(0, count);
    const QString chunk = m_morseChunk;
    const quint64 generation = ++m_morseGeneration;
    m_morseWaiting = true;
    m_morseTimer.start(30000);
    QPointer<DecodiumCatShare> self(this);
    auto ready = [self, generation, chunk](int rc, int speed) {
        if (!self || generation != self->m_morseGeneration || !self->m_morseOwner) return;
        // Missing speed telemetry must not disable send_morse. Pace at the
        // slowest supported speed in that case; never invent a radio readback.
        const int pacingWpm = rc == 0 && speed >= 5 && speed <= 60 ? speed : 5;
        self->m_hooks.sendMorse(chunk, [self, generation, chunk, pacingWpm](int sent) {
            if (!self || generation != self->m_morseGeneration || !self->m_morseOwner) return;
            self->m_morseWaiting = false;
            auto accepted = std::move(self->m_morseAccepted);
            self->m_morseAccepted = {};
            if (accepted) accepted(QStringLiteral("RPRT %1\n").arg(sent));
            if (sent != 0) {
                qWarning() << "[CATSHARE] CW keyer rejected text, rc=" << sent;
                self->stopMorse();
                return;
            }
            self->m_morseTimer.start(decodium::cw::CwKeyer::millisFor(chunk, pacingWpm)
                                    + qMax(100, 3600 / pacingWpm));
        });
    };
    if (m_hooks.morseSpeed) m_hooks.morseSpeed(false, 0, ready);
    else ready(-11, 0);
}

void DecodiumCatShare::cancelMorse()
{
    ++m_morseGeneration;
    m_morseTimer.stop();
    m_morseWaiting = false;
    m_morseText.clear();
    m_morseChunk.clear();
    m_morseOwner.clear();
    auto accepted = std::move(m_morseAccepted);
    m_morseAccepted = {};
    if (accepted) accepted(QStringLiteral("RPRT -9\n"));
}

void DecodiumCatShare::stopMorse(Completion completion)
{
    cancelMorse();
    if (!completion) completion = [](int rc) {
        if (rc != 0) qWarning() << "[CATSHARE] CW stop failed, rc=" << rc;
    };
    if (m_hooks.stopMorse) m_hooks.stopMorse(std::move(completion));
    else completion(-11);
}

// -------------------------------------------------------------------- protocollo
QString DecodiumCatShare::dumpState() const
{
    // Formato verificato contro il vero client Hamlib; vedi
    // doc/cat-condivisa-protocollo.md prima di toccarlo.
    // Si annuncia il modello 1 (dummy) e non quello reale: dichiarare il
    // modello vero obbligherebbe a riprodurne fedelmente ogni capacita', e
    // ogni scostamento diventerebbe un difetto nei client.
    static const char* const kState =
        "1\n"                                        // versione del protocollo
        "1\n"                                        // modello
        "2\n"                                        // regione ITU
        "30000.000000 56000000.000000 0x2ffffff -1 -1 0x3 0x3\n"
        "0 0 0 0 0 0 0\n"
        "1800000.000000 54000000.000000 0x2ffffff 5000 100000 0x3 0x3\n"
        "0 0 0 0 0 0 0\n"
        "0x2ffffff 1\n"                              // passi di sintonia
        "0 0\n"
        "0x82 500\n"                                 // filtri
        "0x221 3000\n"
        "0 0\n"
        "0\n0\n0\n0\n"                               // rit, xit, ifshift, announces
        "0\n"                                        // preamplificatori
        "0\n"                                        // attenuatori
        // has_get_func, has_set_func, has_get_level, has_set_level, has_get_parm,
        // has_set_parm. La terza riga e' quella che conta: dichiarando 0x0 si
        // diceva ai client «questo rig non ha misuratori», e loro smettevano di
        // chiederli — Decolink non mandava nemmeno la domanda, e le barre di
        // potenza, ROS e ALC restavano vuote per sempre.
        //
        // 0x8170000000 = SWR | ALC | STRENGTH | RFPOWER_METER | RFPOWER_METER_WATTS.
        // KEYSPD (0x4000) is read/write; meters remain read-only. An actual
        // unsupported radio operation still returns its Hamlib error.
        "0x0\n0x0\n0x8170004000\n0x4000\n0x0\n0x0\n"
        "vfo_ops=0x0\n"
        "ptt_type=0x1\n"
        "targetable_vfo=0x0\n"
        "done\n";
    return QString::fromLatin1(kState);
}

QString DecodiumCatShare::toHamlibMode(const QString& m)
{
    QString const u = m.trimmed().toUpper();
    if (u == QLatin1String("DATA-U") || u == QLatin1String("DIGU")
        || u == QLatin1String("PKTUSB")) return QStringLiteral("PKTUSB");
    if (u == QLatin1String("DATA-L") || u == QLatin1String("DIGL")
        || u == QLatin1String("PKTLSB")) return QStringLiteral("PKTLSB");
    if (u == QLatin1String("CW-R") || u == QLatin1String("CWR")) return QStringLiteral("CWR");
    if (u == QLatin1String("USB") || u == QLatin1String("LSB")
        || u == QLatin1String("CW")  || u == QLatin1String("AM")
        || u == QLatin1String("FM")  || u == QLatin1String("RTTY")) return u;
    // Un modo sconosciuto non va tradotto a caso: USB e' la scelta neutra per
    // i modi digitali su HF, ed e' quella che i client gestiscono sempre.
    return QStringLiteral("USB");
}

QString DecodiumCatShare::fromHamlibMode(const QString& m)
{
    QString const u = m.trimmed().toUpper();
    if (u == QLatin1String("PKTUSB")) return QStringLiteral("DATA-U");
    if (u == QLatin1String("PKTLSB")) return QStringLiteral("DATA-L");
    if (u == QLatin1String("CWR"))    return QStringLiteral("CW-R");
    return u;
}

bool DecodiumCatShare::refuseWrite(const QString& command, bool pttCommand)
{
    bool const allowed = pttCommand ? m_allowPtt : m_allowControl;
    if (!allowed) {
        qInfo().noquote() << "[CATSHARE] rifiutato" << command
                          << (pttCommand ? "(trasmissione non abilitata)"
                                         : "(controllo non abilitato)");
    }
    emit controlAttempt(command, allowed);
    return !allowed;
}

QString DecodiumCatShare::handleLine(const QString& line)
{
    // Alcuni client (rigctld "netrigctl", usato anche dall'app del telefono)
    // antepongono '+' per chiedere la forma estesa della risposta ai comandi
    // \get_level: due righe, "get_level: <LIVELLO>" e "Level Value: <numero>",
    // invece del solo numero nudo. Senza quella forma un client che manda
    // "+\get_level SWR" non riconosce affatto la risposta e il misuratore
    // resta vuoto per sempre, anche se il dato c'e'. Gli altri comandi
    // restano nella forma semplice, che tutti accettano comunque.
    bool const extended = !line.isEmpty() && line.at(0) == QLatin1Char('+');
    QString cleaned = line;
    while (!cleaned.isEmpty() && (cleaned.at(0) == QLatin1Char('+')
                                  || cleaned.at(0) == QLatin1Char(';')))
        cleaned.remove(0, 1);

    QStringList const parts = cleaned.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return QString::fromLatin1(kOk);
    QString const cmd = parts.first();

    if (cmd == QLatin1String("q") || cmd == QLatin1String("Q"))
        return QString();                       // chiusura

    if (cmd == QLatin1String("\\dump_state"))
        return dumpState();
    // Va risposto "0" e non "CHKVFO 0": con la forma lunga Hamlib rifiuta
    // l'apertura. Vedi doc/cat-condivisa-protocollo.md.
    if (cmd == QLatin1String("\\chk_vfo"))
        return QStringLiteral("0\n");
    if (cmd == QLatin1String("\\get_powerstat"))
        return QStringLiteral("1\n");

    // Hamlib lo chiede all'apertura del collegamento e, dalla 4.7, prima di
    // OGNI cambio di modo. La radio non ha un blocco del VFO: 0. Ma il client
    // (netrigctl_get_lock_mode in rigs/dummy/netrigctl.c di Hamlib 4.7.0)
    // dopo il valore legge una seconda riga, perche' rigctld fa seguire il
    // valore da RPRT 0 (il comando non ha ARG_OUT, tests/rigctl_parse.c).
    // Con la sola riga "0" il client aspettava fino al timeout a ogni set_mode
    // di WSJT-X e degli altri: misurato col banco di prova, prima e dopo.
    if (cmd == QLatin1String("\\get_lock_mode"))
        return QStringLiteral("0\nRPRT 0\n");

    const State state = m_hooks.state ? m_hooks.state() : State{};
    bool const up = state.connected;

    // ---- letture: rispondono dallo stato gia' in memoria, senza generare
    // traffico sulla seriale. Interrogare la radio a ogni domanda di ogni
    // client saturerebbe il bus.
    if (cmd == QLatin1String("f") || cmd == QLatin1String("\\get_freq"))
        return QStringLiteral("%1\n").arg(qint64(up ? state.frequency : 0));
    if (cmd == QLatin1String("m") || cmd == QLatin1String("\\get_mode"))
        return QStringLiteral("%1\n3000\n")
            .arg(toHamlibMode(up ? state.mode : QString()));
    if (cmd == QLatin1String("t") || cmd == QLatin1String("\\get_ptt"))
        return QStringLiteral("%1\n").arg(up && state.ptt ? 1 : 0);
    if (cmd == QLatin1String("v") || cmd == QLatin1String("\\get_vfo"))
        return QStringLiteral("VFOA\n");
    if (cmd == QLatin1String("s") || cmd == QLatin1String("\\get_split_vfo"))
        return QStringLiteral("%1\nVFOB\n").arg(up && state.split ? 1 : 0);
    if (cmd == QLatin1String("i") || cmd == QLatin1String("\\get_split_freq"))
        return QStringLiteral("%1\n").arg(qint64(up ? state.txFrequency : 0));

    // ---- misuratori: potenza, ROS, ALC
    //
    // Serve a chi opera da remoto. Quando la radio la tiene Decodium nessun
    // altro programma puo' aprire la stessa seriale, e dall'altra parte del
    // mondo si trasmetteva senza vedere ne' quanto si stava erogando ne' se
    // l'antenna rispondeva. Decodium queste tre cose le misura gia': mancava
    // soltanto il modo di chiederle da fuori.
    //
    // I nomi sono quelli di Hamlib, cosi' li capisce qualunque client scritto
    // per rigctl senza sapere che dall'altra parte c'e' Decodium.
    if (cmd == QLatin1String("l") || cmd == QLatin1String("\\get_level")) {
        if (parts.size() < 2 || !up) return QString::fromLatin1(kNotImpl);
        auto const quale = parts.at(1).trimmed().toUpper();

        // Nella forma estesa anche l'errore va introdotto dal nome del
        // livello: senza quella riga il client non saprebbe a quale
        // misuratore riferire il RPRT che segue.
        auto const fail = [&]() -> QString {
            if (extended)
                return QStringLiteral("get_level: %1\n").arg(quale)
                     + QString::fromLatin1(kNotImpl);
            return QString::fromLatin1(kNotImpl);
        };
        auto const ok = [&](double v) -> QString {
            if (extended)
                return QStringLiteral("get_level: %1\nLevel Value: %2\nRPRT 0\n")
                    .arg(quale).arg(v, 0, 'f', 6);
            return QStringLiteral("%1\n").arg(v, 0, 'f', 6);
        };

        // A riposo i misuratori di trasmissione non misurano niente. Rispondere
        // zero direbbe «nessuna potenza, ROS perfetto», che somiglia a una
        // stazione che va benissimo: meglio dire che il dato non c'e'.
        bool const diTx = quale == QLatin1String("SWR")
                          || quale == QLatin1String("ALC")
                          || quale.startsWith(QLatin1String("RFPOWER_METER"));
        if (diTx && !state.ptt) return fail();

        // S-meter: e' l'unico livello di RICEZIONE, quindi non passa dal
        // filtro "diTx" qui sopra — anzi vale solo mentre NON si trasmette.
        // Il rig lo legge solo se lo dichiara fra le proprie capacita'; se non
        // c'e', si risponde che non c'e' invece di mandare uno zero, che su
        // questa scala vorrebbe dire S9.
        if (quale == QLatin1String("STRENGTH")) {
            if (!state.strengthValid) return fail();
            return ok(state.strengthDb);
        }

        if (quale == QLatin1String("SWR")) {
            double const v = state.swr;
            if (v < 1.0) return fail();
            return ok(v);
        }
        if (quale == QLatin1String("ALC")) {
            // Se la radio non sa dare l'ALC, Decodium lo segna non valido: quel
            // «non lo so» va riportato tale e quale, non tradotto in zero.
            if (!state.alcValid) return fail();
            return ok(state.alc / 100.0);
        }
        if (quale == QLatin1String("RFPOWER_METER_WATTS")) {
            double const w = state.powerWatts;
            if (w <= 0.0) return fail();
            return ok(w);
        }
        return fail();
    }


    // ---- scritture
    if (cmd == QLatin1String("F") || cmd == QLatin1String("\\set_freq")) {
        if (refuseWrite(cmd, false)) return QString::fromLatin1(kRefused);
        if (parts.size() < 2 || !up) return QString::fromLatin1(kRefused);
        if (m_hooks.frequency) m_hooks.frequency(parts.last().toDouble(), false);
        return QString::fromLatin1(kOk);
    }
    if (cmd == QLatin1String("I") || cmd == QLatin1String("\\set_split_freq")) {
        if (refuseWrite(cmd, false)) return QString::fromLatin1(kRefused);
        if (parts.size() < 2 || !up) return QString::fromLatin1(kRefused);
        if (m_hooks.frequency) m_hooks.frequency(parts.last().toDouble(), true);
        return QString::fromLatin1(kOk);
    }
    if (cmd == QLatin1String("M") || cmd == QLatin1String("\\set_mode")) {
        if (refuseWrite(cmd, false)) return QString::fromLatin1(kRefused);
        if (parts.size() < 2 || !up) return QString::fromLatin1(kRefused);
        if (m_hooks.mode) m_hooks.mode(fromHamlibMode(parts.at(1)));
        return QString::fromLatin1(kOk);
    }
    if (cmd == QLatin1String("T") || cmd == QLatin1String("\\set_ptt")) {
        if (refuseWrite(cmd, true)) return QString::fromLatin1(kRefused);
        if (parts.size() < 2 || !up) return QString::fromLatin1(kRefused);
        if (m_hooks.ptt) m_hooks.ptt(parts.last().toInt() != 0);
        return QString::fromLatin1(kOk);
    }
    // VFO e split si accettano senza agire: i client li impostano di routine
    // all'apertura, e rifiutarli farebbe fallire il collegamento a chi non ha
    // alcuna intenzione di comandare la radio.
    if (cmd == QLatin1String("V") || cmd == QLatin1String("\\set_vfo")
        || cmd == QLatin1String("S") || cmd == QLatin1String("\\set_split_vfo"))
        return QString::fromLatin1(kOk);

    qInfo().noquote() << "[CATSHARE] comando non gestito:" << cleaned;
    return QString::fromLatin1(kNotImpl);
}
