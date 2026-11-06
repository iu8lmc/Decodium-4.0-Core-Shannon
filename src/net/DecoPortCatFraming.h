// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

// La grammatica minima del CAT nativo: dove finisce un comando, se aspetta una
// risposta, a chi va una risposta, quando un comando tocca il PTT.
//
// Non e' un'implementazione dei protocolli. Il multiplexer non capisce cosa
// chiede un programma alla radio: gli basta sapere dove tagliare il flusso e
// come riconoscere la risposta giusta, per far parlare piu' programmi con la
// stessa porta senza che le risposte di uno finiscano all'altro. E' per questo
// che funziona con ogni radio di un dialetto, comprese quelle che nessuno ha
// ancora provato, e che i programmi restano configurati come prima.
//
// Niente I/O qui dentro, come in DecoPortPacket: si prova da solo.
namespace decoport::cat {

enum class Dialect {
    // "FA;" -> "FA00014074000;". Kenwood, Elecraft, QRP Labs, (tr)uSDX...
    Kenwood,
    // Lo stesso alfabeto con le regole Yaesu: TX0 e' ricezione, TX1 trasmissione.
    Newcat,
    // Icom e compatibili (Xiegu): cornici FE FE a da ... FD.
    Civ,
    // Gli Yaesu di prima del newcat: comandi binari di cinque byte.
    Yaesu5,
    // Tutto il resto: botta e risposta, senza capire la grammatica. Va con
    // ogni radio che parla solo quando interrogata.
    Binary,
};

Dialect dialectFromName(const QString& name);
QString dialectName(Dialect d);

struct Command {
    QByteArray bytes;
    // Se la radio rispondera'. Sbagliare costa solo tempo: una richiesta presa
    // per un comando fa arrivare la risposta a tutti, un comando preso per una
    // richiesta fa aspettare fino alla scadenza.
    bool expectsReply {true};
    // Una richiesta con un selettore ("EX032;" legge il menu 32, "EX0321;" lo
    // imposta sull'FT-991 ma lo leggerebbe su una radio con menu a quattro
    // cifre): forse risponde, forse no. Si aspetta poco, ma si aspetta.
    bool uncertain {false};
    // Testo: le due lettere ("FA"). CI-V: comando e sottocomando.
    QByteArray code;
    // 1 alza il PTT, 0 lo abbassa, -1 non lo tocca.
    int ptt {-1};
    // CI-V: destinatario (la radio, o 00 per tutti) e mittente (il controller).
    quint8 civTo {0};
    quint8 civFrom {0};
};

// Toglie da `buf` i comandi completi scritti da un programma. Quello che resta
// in `buf` e' un comando a meta', che aspetta il resto.
QVector<Command> takeCommands(Dialect d, QByteArray& buf);

// Toglie da `buf` i messaggi completi della radio. Per i dialetti senza cornice
// (Yaesu5, Binary) restituisce tutto quello che c'e' in un pezzo solo: la fine
// la decide il silenzio, ed e' affare del multiplexer.
QVector<QByteArray> takeReplies(Dialect d, QByteArray& buf);

// I dialetti in cui una risposta finisce quando la radio tace.
bool framedBySilence(Dialect d);

enum class Reply {
    Answer,       // risponde alla richiesta in volo
    Error,        // "?;" e simili: chiude la richiesta in volo
    Echo,         // CI-V: il comando stesso, ripetuto dal bus
    Unsolicited,  // la radio parla da sola (AI, transceive): a tutti
};

Reply classify(Dialect d, const Command* inFlight, const QByteArray& reply);

// Il comando che riporta la radio in ricezione, se il dialetto ne ha uno
// sicuro. Vuoto per Yaesu5 e Binary: un byte sbagliato su una radio
// sconosciuta e' peggio di un PTT da abbassare a mano.
QByteArray releasePtt(Dialect d, quint8 civRadio = 0);

// Quanto aspettare la risposta a un comando.
int replyTimeoutMs(Dialect d, const Command& c);
// Per i dialetti a silenzio: quanto silenzio chiude una risposta.
int silenceMs(Dialect d);

} // namespace decoport::cat
