// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecoPortCatFraming.h"

#include <QHash>

#include <cctype>

namespace decoport::cat {

namespace {

constexpr char kCivPreamble = '\xFE';
constexpr char kCivEnd      = '\xFD';
constexpr quint8 kCivPtt    = 0x1C;
// Oltre questa lunghezza un messaggio senza fine non e' un messaggio: e'
// rumore sulla linea, o un programma che parla un altro dialetto.
constexpr int kMaxPending = 4096;

bool isText(Dialect d)
{
    return d == Dialect::Kenwood || d == Dialect::Newcat;
}

QByteArray trimLeading(const QByteArray& b)
{
    int i = 0;
    while (i < b.size() && (b[i] == '\r' || b[i] == '\n' || b[i] == ' ' || b[i] == '\0'))
        ++i;
    return b.mid(i);
}

// I comandi senza parametri che non sono richieste: cambiano qualcosa e la
// radio non risponde. Tutto il resto, senza parametri, e' una domanda.
bool isBareSet(Dialect d, const QByteArray& code)
{
    static const QByteArray common[] = {"AB", "BA", "SV", "AM", "MA", "ZI", "QI", "QR", "UP", "DN"};
    for (const QByteArray& c : common)
        if (code == c)
            return true;
    // Su Kenwood "TX;" trasmette e "RX;" riceve; su Yaesu "TX;" chiede lo stato.
    if (d == Dialect::Kenwood)
        return code == "TX" || code == "RX" || code == "BU" || code == "BD";
    return false;
}

// Le richieste con un selettore: "AG0;" chiede il volume del ricevitore
// principale, "AG0100;" lo imposta. Il numero e' quante cifre di selettore ha
// la richiesta; un comando con piu' cifre e' un'impostazione.
int selectorDigits(Dialect d, const QByteArray& code)
{
    if (d == Dialect::Newcat) {
        static const QHash<QByteArray, int> newcat = {
            {"AG", 1}, {"RG", 1}, {"SQ", 1}, {"RM", 1}, {"MD", 1}, {"SH", 1},
            {"NA", 1}, {"IS", 1}, {"NB", 1}, {"NR", 1}, {"PA", 1}, {"RA", 1},
            {"RL", 1}, {"CT", 1}, {"ML", 1}, {"SM", 1}, {"OS", 1}, {"CN", 2},
            {"BP", 2}, {"EX", 6},
        };
        return newcat.value(code, 0);
    }
    static const QHash<QByteArray, int> kenwood = {
        {"AG", 1}, {"SM", 1}, {"SQ", 1}, {"EX", 7},
    };
    return kenwood.value(code, 0);
}

bool allDigits(const QByteArray& b)
{
    for (char c : b)
        if (c < '0' || c > '9')
            return false;
    return true;
}

Command textCommand(Dialect d, const QByteArray& raw)
{
    Command c;
    c.bytes = raw;
    const QByteArray body = raw.left(raw.size() - 1);   // senza ';'
    if (body.size() < 2 || !isalpha(static_cast<uchar>(body[0])) || !isalpha(static_cast<uchar>(body[1]))) {
        // ";" da solo, o qualcosa che non e' un comando: la radio rispondera'
        // al massimo con un errore, che la breve attesa raccoglie.
        c.expectsReply = false;
        return c;
    }
    c.code = body.left(2).toUpper();
    const QByteArray params = body.mid(2);

    if (params.isEmpty()) {
        c.expectsReply = !isBareSet(d, c.code);
    } else {
        c.expectsReply = allDigits(params) && params.size() <= selectorDigits(d, c.code);
        c.uncertain = c.expectsReply;
    }

    if (d == Dialect::Newcat && c.code == "TX" && !params.isEmpty())
        c.ptt = params == "0" ? 0 : 1;
    else if (d == Dialect::Kenwood && c.code == "TX")
        c.ptt = 1;
    else if (d == Dialect::Kenwood && c.code == "RX")
        c.ptt = 0;
    return c;
}

// Le cornici CI-V di un flusso. Il preambolo puo' avere piu' di due FE: si
// riporta a due, che e' come lo scrivono tutti.
QVector<QByteArray> takeCivFrames(QByteArray& buf)
{
    QVector<QByteArray> out;
    for (;;) {
        const int s = buf.indexOf(QByteArray(2, kCivPreamble));
        if (s < 0) {
            // Un FE in coda puo' essere l'inizio di un preambolo.
            if (!buf.isEmpty() && buf.endsWith(kCivPreamble))
                buf = buf.right(1);
            else
                buf.clear();
            break;
        }
        if (s > 0)
            buf.remove(0, s);
        int i = 2;
        while (i < buf.size() && buf[i] == kCivPreamble)
            ++i;
        const int e = buf.indexOf(kCivEnd, i);
        if (e < 0) {
            if (buf.size() > kMaxPending)
                buf.clear();
            break;
        }
        QByteArray frame = QByteArray(2, kCivPreamble) + buf.mid(i, e - i + 1);
        buf.remove(0, e + 1);
        // FE FE a da comando FD: piu' corta non e' una cornice.
        if (frame.size() >= 6)
            out.append(frame);
    }
    return out;
}

Command civCommand(const QByteArray& frame)
{
    Command c;
    c.bytes = frame;
    c.civTo = static_cast<quint8>(frame[2]);
    c.civFrom = static_cast<quint8>(frame[3]);
    c.code = frame.mid(4, frame.size() > 6 ? 2 : 1);
    // Ogni comando CI-V ha una risposta: i dati, o FB/FA.
    c.expectsReply = true;
    if (static_cast<quint8>(frame[4]) == kCivPtt && frame.size() >= 8
        && static_cast<quint8>(frame[5]) == 0x00 && frame[6] != kCivEnd)
        c.ptt = frame[6] == '\x01' ? 1 : 0;
    return c;
}

} // namespace

Dialect dialectFromName(const QString& name)
{
    const QString n = name.trimmed().toLower();
    if (n == QLatin1String("kenwood"))
        return Dialect::Kenwood;
    if (n == QLatin1String("newcat"))
        return Dialect::Newcat;
    if (n == QLatin1String("civ"))
        return Dialect::Civ;
    if (n == QLatin1String("yaesu5"))
        return Dialect::Yaesu5;
    return Dialect::Binary;
}

QString dialectName(Dialect d)
{
    switch (d) {
    case Dialect::Kenwood: return QStringLiteral("kenwood");
    case Dialect::Newcat:  return QStringLiteral("newcat");
    case Dialect::Civ:     return QStringLiteral("civ");
    case Dialect::Yaesu5:  return QStringLiteral("yaesu5");
    case Dialect::Binary:  break;
    }
    return QStringLiteral("binary");
}

bool framedBySilence(Dialect d)
{
    return d == Dialect::Yaesu5 || d == Dialect::Binary;
}

QVector<Command> takeCommands(Dialect d, QByteArray& buf)
{
    QVector<Command> out;
    if (isText(d)) {
        int end;
        while ((end = buf.indexOf(';')) >= 0) {
            const QByteArray raw = trimLeading(buf.left(end + 1));
            buf.remove(0, end + 1);
            if (!raw.isEmpty())
                out.append(textCommand(d, raw));
        }
        if (buf.size() > kMaxPending)
            buf.clear();
        return out;
    }
    if (d == Dialect::Civ) {
        for (const QByteArray& frame : takeCivFrames(buf))
            out.append(civCommand(frame));
        return out;
    }
    if (d == Dialect::Yaesu5) {
        // Sempre cinque byte: quattro parametri e il codice operativo.
        while (buf.size() >= 5) {
            Command c;
            c.bytes = buf.left(5);
            c.code = buf.mid(4, 1);
            buf.remove(0, 5);
            out.append(c);
        }
        return out;
    }
    if (!buf.isEmpty()) {
        Command c;
        c.bytes = buf;
        buf.clear();
        out.append(c);
    }
    return out;
}

QVector<QByteArray> takeReplies(Dialect d, QByteArray& buf)
{
    QVector<QByteArray> out;
    if (isText(d)) {
        int end;
        while ((end = buf.indexOf(';')) >= 0) {
            const QByteArray raw = trimLeading(buf.left(end + 1));
            buf.remove(0, end + 1);
            if (!raw.isEmpty())
                out.append(raw);
        }
        if (buf.size() > kMaxPending)
            buf.clear();
        return out;
    }
    if (d == Dialect::Civ)
        return takeCivFrames(buf);
    if (!buf.isEmpty()) {
        out.append(buf);
        buf.clear();
    }
    return out;
}

Reply classify(Dialect d, const Command* inFlight, const QByteArray& reply)
{
    if (isText(d)) {
        const bool error = reply == "?;" || (d == Dialect::Kenwood && (reply == "E;" || reply == "O;"));
        if (error)
            return inFlight ? Reply::Error : Reply::Unsolicited;
        if (inFlight && inFlight->expectsReply && reply.size() >= 3
            && reply.left(2).toUpper() == inFlight->code)
            return Reply::Answer;
        return Reply::Unsolicited;
    }
    if (d == Dialect::Civ) {
        if (reply.size() < 6)
            return Reply::Unsolicited;
        if (inFlight && reply == inFlight->bytes)
            return Reply::Echo;
        const quint8 to = static_cast<quint8>(reply[2]);
        const quint8 from = static_cast<quint8>(reply[3]);
        if (inFlight && to == inFlight->civFrom
            && (inFlight->civTo == 0x00 || from == inFlight->civTo))
            return Reply::Answer;
        return Reply::Unsolicited;
    }
    return inFlight ? Reply::Answer : Reply::Unsolicited;
}

QByteArray releasePtt(Dialect d, quint8 civRadio)
{
    switch (d) {
    case Dialect::Kenwood:
        return QByteArrayLiteral("RX;");
    case Dialect::Newcat:
        return QByteArrayLiteral("TX0;");
    case Dialect::Civ:
        if (civRadio == 0)
            return {};
        return QByteArray("\xFE\xFE", 2) + char(civRadio) + QByteArray("\xE0\x1C\x00\x00\xFD", 5);
    case Dialect::Yaesu5:
    case Dialect::Binary:
        break;
    }
    return {};
}

int replyTimeoutMs(Dialect d, const Command& c)
{
    if (isText(d)) {
        // Senza risposta attesa resta solo il tempo di raccogliere un "?;". Una
        // richiesta incerta aspetta quanto basta a una radio vera per
        // rispondere, anche a 4800 baud, e non di piu'.
        if (!c.expectsReply)
            return 50;
        return c.uncertain ? 300 : 1000;
    }
    if (d == Dialect::Civ)
        return 500;
    // Il primo byte di una risposta binaria: le radio vecchie rispondono in
    // poche decine di millisecondi, oppure non rispondono affatto.
    return 200;
}

int silenceMs(Dialect d)
{
    return d == Dialect::Yaesu5 ? 30 : 40;
}

} // namespace decoport::cat
