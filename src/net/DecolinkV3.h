// Decolink v3, il piano dati: intestazione di 10 byte e i tipi che servono al
// profilo "digitali senza perdite".
//
// La v2 (DecolinkPacket.h) resta per la registrazione, il CAT e il PCM grezzo.
// La v3 porta solo l'audio compresso e la negoziazione del profilo; il relay la
// inoltra senza guardarci dentro. Il formato e' quello di gui/dlproto.h del
// repository iu8lmc/Decolink: qui c'e' solo quello che il client usa, e di
// Opus, Codec2 e del tasto CW non si legge niente.
//
//   byte 0     'D'
//   byte 1     versione(4) | tipo(4)
//   byte 2     profilo(4)  | flag(4)
//   byte 3     numero di stazione locale
//   byte 4-5   seq u16 (ricircola)
//   byte 6-9   marca temporale u32, in campioni dell'orologio del profilo
#pragma once

#include <QByteArray>
#include <QtGlobal>

#include <cstring>

namespace decolink {
namespace v3 {

constexpr char   kMagic = 'D';
constexpr quint8 kVer   = 3;
constexpr int    kHdr   = 10;

enum Type : quint8 {
    AudioRx = 0,   // gateway -> operatori
    AudioTx = 1,   // operatore -> gateway
    Ctrl    = 2,   // negoziazione e report; il sottotipo e' il primo byte del corpo
    Cat     = 3,
    Nack    = 4,   // sequenze mancanti da rimandare
    Ping    = 5,
};

enum Profile : quint8 {
    Pcm48  = 0,    // 48 kHz int16 grezzo: la v2
    Voice  = 1,    // Opus
    Cw     = 2,    // Opus
    Digi   = 3,    // PCM 12 kHz senza perdite
    Emrg   = 4,    // Codec2
    CwKey  = 5,    // solo gli istanti del tasto
};

enum CtrlKind : quint8 {
    Hello   = 1,
    Offer   = 2,
    Choose  = 3,
    Active  = 4,
    Report  = 5,
};

inline const char* profileName(int p)
{
    switch (p) {
    case Pcm48:  return "PCM 48 kHz";
    case Voice:  return "voice (Opus)";
    case Cw:     return "CW (Opus)";
    case Digi:   return "digital (lossless)";
    case Emrg:   return "emergency (Codec2)";
    case CwKey:  return "CW keying";
    default:     return "?";
    }
}

struct Header {
    quint8  type {0};
    quint8  profile {0};
    quint8  flags {0};
    quint8  station {0};
    quint16 seq {0};
    quint32 time {0};
};

inline QByteArray makePacket(quint8 type, quint8 profile, quint8 flags, quint8 station,
                             quint16 seq, quint32 time, const QByteArray& body = QByteArray())
{
    QByteArray p(kHdr, Qt::Uninitialized);
    char* d = p.data();
    d[0] = kMagic;
    d[1] = char((kVer << 4) | (type & 0xF));
    d[2] = char(((profile & 0xF) << 4) | (flags & 0xF));
    d[3] = char(station);
    d[4] = char(seq >> 8); d[5] = char(seq);
    d[6] = char(time >> 24); d[7] = char(time >> 16); d[8] = char(time >> 8); d[9] = char(time);
    p.append(body);
    return p;
}

// Un datagramma che comincia con 'D' e dichiara la versione 3. Non guarda
// altro: l'arrivo tagliato a meta' lo scarta chi lo legge.
inline bool looksLikeV3(const QByteArray& dg)
{
    return dg.size() >= kHdr && dg.at(0) == kMagic
           && ((uchar(dg.at(1)) >> 4) == kVer);
}

inline bool parsePacket(const QByteArray& dg, Header* h, QByteArray* body)
{
    if (!looksLikeV3(dg))
        return false;
    const uchar* d = reinterpret_cast<const uchar*>(dg.constData());
    if (h) {
        h->type    = d[1] & 0xF;
        h->profile = d[2] >> 4;
        h->flags   = d[2] & 0xF;
        h->station = d[3];
        h->seq     = quint16((quint16(d[4]) << 8) | d[5]);
        h->time    = (quint32(d[6]) << 24) | (quint32(d[7]) << 16) | (quint32(d[8]) << 8) | d[9];
    }
    if (body)
        *body = dg.mid(kHdr);
    return true;
}

// Il corpo del profilo CW a tasto: la nota e gli istanti in cui il tasto si apre
// e si chiude, e il tono lo rigenera il gateway.
//   byte 0     nota in decine di Hz (70 = 700 Hz)
//   byte 1     numero di eventi (al massimo 255)
//   poi        u16 per evento: bit 15 = tasto giu', bit 0-14 = ms dal precedente
constexpr int kCwKeyMaxEvents = 255;

// Il corpo: intestazione (nota e numero di eventi) e poi due byte per evento.
inline QByteArray cwKeyHeader(int toneHz, int count)
{
    QByteArray h;
    h.append(char(qBound(0, toneHz / 10, 255)));
    h.append(char(qBound(0, count, kCwKeyMaxEvents)));
    return h;
}

inline QByteArray cwKeyEvent(quint16 deltaMs, bool down)
{
    const quint16 v = quint16((down ? 0x8000 : 0) | (deltaMs & 0x7FFF));
    QByteArray e;
    e.append(char((v >> 8) & 0xFF));
    e.append(char(v & 0xFF));
    return e;
}

}  // namespace v3
}  // namespace decolink
