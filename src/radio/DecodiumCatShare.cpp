#include "DecodiumCatShare.h"

#include "DecodiumTransceiverManager.h"

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


DecodiumCatShare::DecodiumCatShare(DecodiumTransceiverManager* rig, QObject* parent)
    : QObject(parent)
    , m_rig(rig)
{
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

    if (restart) {
        if (m_server) {
            for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it)
                it.key()->disconnectFromHost();
            m_buffers.clear();
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
    m_buffers.remove(s);
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

    int nl;
    while ((nl = it.value().indexOf('\n')) >= 0) {
        QByteArray const raw = it.value().left(nl);
        it.value().remove(0, nl + 1);
        QString const line = QString::fromLatin1(raw).trimmed();
        if (line.isEmpty())
            continue;
        QString const reply = handleLine(line);
        if (reply.isNull()) {          // comando di chiusura
            s->disconnectFromHost();
            return;
        }
        s->write(reply.toLatin1());
    }
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
        // Sono in sola lettura: la scrittura resta 0x0, perche' impostare la
        // potenza di una radio altrui non e' compito di questo canale.
        "0x0\n0x0\n0x8170000000\n0x0\n0x0\n0x0\n"
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

    // Hamlib lo chiede all'apertura del collegamento. Non gestirlo lasciava
    // una riga «comando non gestito» nel registro a ogni client che si
    // collegava: la radio non ha un blocco del VFO, e dirlo e' piu' chiaro
    // che tacere.
    if (cmd == QLatin1String("\\get_lock_mode"))
        return QStringLiteral("0\n");

    bool const up = m_rig && m_rig->connected();

    // ---- letture: rispondono dallo stato gia' in memoria, senza generare
    // traffico sulla seriale. Interrogare la radio a ogni domanda di ogni
    // client saturerebbe il bus.
    if (cmd == QLatin1String("f") || cmd == QLatin1String("\\get_freq"))
        return QStringLiteral("%1\n").arg(qint64(up ? m_rig->frequency() : 0));
    if (cmd == QLatin1String("m") || cmd == QLatin1String("\\get_mode"))
        return QStringLiteral("%1\n3000\n")
            .arg(toHamlibMode(up ? m_rig->mode() : QString()));
    if (cmd == QLatin1String("t") || cmd == QLatin1String("\\get_ptt"))
        return QStringLiteral("%1\n").arg(up && m_rig->pttActive() ? 1 : 0);
    if (cmd == QLatin1String("v") || cmd == QLatin1String("\\get_vfo"))
        return QStringLiteral("VFOA\n");
    if (cmd == QLatin1String("s") || cmd == QLatin1String("\\get_split_vfo"))
        return QStringLiteral("%1\nVFOB\n").arg(up && m_rig->split() ? 1 : 0);
    if (cmd == QLatin1String("i") || cmd == QLatin1String("\\get_split_freq"))
        return QStringLiteral("%1\n").arg(qint64(up ? m_rig->txFrequency() : 0));

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
        if (diTx && !m_rig->pttActive()) return fail();

        // S-meter: e' l'unico livello di RICEZIONE, quindi non passa dal
        // filtro "diTx" qui sopra — anzi vale solo mentre NON si trasmette.
        // Il rig lo legge solo se lo dichiara fra le proprie capacita'; se non
        // c'e', si risponde che non c'e' invece di mandare uno zero, che su
        // questa scala vorrebbe dire S9.
        if (quale == QLatin1String("STRENGTH")) {
            if (!m_rig->strengthValid()) return fail();
            return ok(m_rig->strengthDb());
        }

        if (quale == QLatin1String("SWR")) {
            double const v = m_rig->swr();
            if (v < 1.0) return fail();
            return ok(v);
        }
        if (quale == QLatin1String("ALC")) {
            // Se la radio non sa dare l'ALC, Decodium lo segna non valido: quel
            // «non lo so» va riportato tale e quale, non tradotto in zero.
            if (!m_rig->alcValid()) return fail();
            return ok(m_rig->alc() / 100.0);
        }
        if (quale == QLatin1String("RFPOWER_METER_WATTS")) {
            double const w = m_rig->powerWatts();
            if (w <= 0.0) return fail();
            return ok(w);
        }
        return fail();
    }


    // ---- scritture
    if (cmd == QLatin1String("F") || cmd == QLatin1String("\\set_freq")) {
        if (refuseWrite(cmd, false)) return QString::fromLatin1(kRefused);
        if (parts.size() < 2 || !up) return QString::fromLatin1(kRefused);
        m_rig->setRigFrequency(parts.last().toDouble());
        return QString::fromLatin1(kOk);
    }
    if (cmd == QLatin1String("I") || cmd == QLatin1String("\\set_split_freq")) {
        if (refuseWrite(cmd, false)) return QString::fromLatin1(kRefused);
        if (parts.size() < 2 || !up) return QString::fromLatin1(kRefused);
        m_rig->setRigTxFrequency(parts.last().toDouble());
        return QString::fromLatin1(kOk);
    }
    if (cmd == QLatin1String("M") || cmd == QLatin1String("\\set_mode")) {
        if (refuseWrite(cmd, false)) return QString::fromLatin1(kRefused);
        if (parts.size() < 2 || !up) return QString::fromLatin1(kRefused);
        m_rig->setRigMode(fromHamlibMode(parts.at(1)));
        return QString::fromLatin1(kOk);
    }
    if (cmd == QLatin1String("T") || cmd == QLatin1String("\\set_ptt")) {
        if (refuseWrite(cmd, true)) return QString::fromLatin1(kRefused);
        if (parts.size() < 2 || !up) return QString::fromLatin1(kRefused);
        m_rig->setRigPtt(parts.last().toInt() != 0);
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
