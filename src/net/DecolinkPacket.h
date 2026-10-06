// Decolink, il piano dati: pacchetti HFGW v2 e un decimatore per l'audio.
//
// Decolink e' il servizio che porta una radio in rete tramite un relay (sul VPS
// decolink.ft2.it) con accesso controllato: il PC accanto alla radio fa da
// gateway, chi la usa da remoto entra come operatore o ascoltatore. Qui c'e'
// solo la parte pura, senza socket e senza Qt-GUI: si prova da sola.
//
// Pacchetto v2, header di 22 byte, big-endian:
//   "HFGW"(4) | versione(1) | flag(1) | seq u32 | t_ms u64 | rate u32
// seguito dal corpo. Il corpo dell'audio e' PCM int16 little-endian mono; i
// comandi CAT sono righe di testo nel dialetto di rigctl.
//
// La specifica sta in Decolink (server/LEGGIMI.md e gui/main.cpp): questo file
// la ricalca, non la inventa.
#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <cstring>

namespace decolink {

constexpr int     kHdrSize          = 22;
constexpr quint8  kVersion          = 2;
constexpr quint16 kDefaultRelayPort = 5555;
// La frequenza a cui il decoder di Decodium lavora: tutto l'audio ricevuto
// esce da qui a questa frequenza, qualunque sia quella del gateway.
constexpr int     kDecoderRate      = 12000;
constexpr double  kPi               = 3.14159265358979323846;

enum Flag : quint8 {
    Audio    = 0,   // audio ricevuto dalla radio: gateway -> operatori
    Ping     = 1,
    Pong     = 2,
    Register = 3,   // "gw <token>" / "op <token>"; il relay risponde con lo stesso flag
    PeerUp   = 4,   // la stanza e' pronta: c'e' un gateway e almeno un operatore
    CatReq   = 5,   // comando CAT: operatore -> gateway
    CatRsp   = 6,   // risposta: gateway -> chi ha chiesto
    TxAudio  = 7,   // audio da trasmettere: operatore -> gateway
    Denied   = 8,   // il relay spiega perche' non si entra
};

struct Header {
    quint8  flag {0};
    quint32 seq {0};
    quint64 tMs {0};
    quint32 rate {0};
};

inline void putU32(char* p, quint32 v)
{
    p[0] = char(v >> 24); p[1] = char(v >> 16); p[2] = char(v >> 8); p[3] = char(v);
}
inline void putU64(char* p, quint64 v)
{
    for (int i = 0; i < 8; ++i) p[i] = char(v >> (56 - 8 * i));
}
inline quint32 getU32(const uchar* p)
{
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
}
inline quint64 getU64(const uchar* p)
{
    quint64 v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | quint64(p[i]);
    return v;
}

inline QByteArray makePacket(quint8 flag, quint32 seq, const QByteArray& body,
                             quint32 rate, quint64 tMs)
{
    QByteArray pkt(kHdrSize, Qt::Uninitialized);
    char* p = pkt.data();
    std::memcpy(p, "HFGW", 4);
    p[4] = char(kVersion);
    p[5] = char(flag);
    putU32(p + 6, seq);
    putU64(p + 10, tMs);
    putU32(p + 18, rate);
    pkt.append(body);
    return pkt;
}

// Falso se non e' un pacchetto HFGW o e' di un'altra versione. La versione
// conta: il relay rifiuta la v1 con un messaggio, e un client che non la
// controlla scambierebbe quel messaggio per audio.
inline bool parsePacket(const QByteArray& dg, Header* h, QByteArray* body)
{
    if (dg.size() < kHdrSize || std::memcmp(dg.constData(), "HFGW", 4) != 0)
        return false;
    const uchar* p = reinterpret_cast<const uchar*>(dg.constData());
    if (p[4] != kVersion)
        return false;
    if (h) {
        h->flag = p[5];
        h->seq  = getU32(p + 6);
        h->tMs  = getU64(p + 10);
        h->rate = getU32(p + 18);
    }
    if (body)
        *body = dg.mid(kHdrSize);
    return true;
}

// PCM int16 little-endian <-> campioni. Il filo e' little-endian qualunque sia
// la macchina.
inline QVector<short> pcmToSamples(const QByteArray& pcm)
{
    const int n = pcm.size() / 2;
    QVector<short> out(n);
    const uchar* p = reinterpret_cast<const uchar*>(pcm.constData());
    for (int i = 0; i < n; ++i)
        out[i] = short(quint16(p[2 * i]) | (quint16(p[2 * i + 1]) << 8));
    return out;
}

inline QByteArray samplesToPcm(const short* s, int n)
{
    QByteArray out(n * 2, Qt::Uninitialized);
    uchar* p = reinterpret_cast<uchar*>(out.data());
    for (int i = 0; i < n; ++i) {
        const quint16 v = quint16(s[i]);
        p[2 * i]     = uchar(v & 0xFF);
        p[2 * i + 1] = uchar(v >> 8);
    }
    return out;
}

// Decimatore intero con filtro FIR a finestra, a stato: si alimenta a pezzi e
// l'uscita e' la stessa che si avrebbe in un colpo solo. Serve perche' l'audio
// del gateway arriva a 48 kHz e il decoder lavora a 12: senza il passa-basso si
// ripiegherebbero sul passabanda i 6-24 kHz di rumore della radio.
class Decimator {
public:
    explicit Decimator(int factor = 1) { setFactor(factor); }

    void setFactor(int factor)
    {
        m_factor = qMax(1, factor);
        m_history.clear();
        m_phase = 0;
        m_taps.clear();
        if (m_factor == 1)
            return;
        // Taglio al 45% della frequenza di uscita, 16 punti per ogni campione
        // scartato: abbastanza ripido per il passabanda SSB, corto abbastanza da
        // costare poco a 48 kHz.
        const int n = 16 * m_factor + 1;
        const double fc = 0.45 / m_factor;   // frazione della frequenza d'ingresso
        const double mid = (n - 1) / 2.0;
        m_taps.resize(n);
        double sum = 0.0;
        for (int i = 0; i < n; ++i) {
            const double x = i - mid;
            const double sinc = x == 0.0 ? 2.0 * fc
                                         : std::sin(2.0 * kPi * fc * x) / (kPi * x);
            // finestra di Blackman: lobi laterali a -58 dB, piu' che sufficienti
            const double w = 0.42 - 0.5 * std::cos(2.0 * kPi * i / (n - 1))
                                  + 0.08 * std::cos(4.0 * kPi * i / (n - 1));
            m_taps[i] = sinc * w;
            sum += m_taps[i];
        }
        for (double& t : m_taps) t /= sum;   // guadagno unitario in continua
        m_history.fill(0.0f, n - 1);
    }

    int factor() const { return m_factor; }

    QVector<short> process(const short* in, int n)
    {
        if (m_factor == 1)
            return QVector<short>(in, in + n);
        QVector<short> out;
        out.reserve(n / m_factor + 1);
        const int taps = m_taps.size();
        // history = gli ultimi taps-1 campioni gia' visti
        QVector<float> buf;
        buf.reserve(m_history.size() + n);
        buf += m_history;
        for (int i = 0; i < n; ++i) buf.append(float(in[i]));
        // m_phase = quanti campioni mancano alla prossima uscita
        int pos = m_phase;
        while (pos + taps <= buf.size()) {
            double acc = 0.0;
            for (int k = 0; k < taps; ++k)
                acc += double(buf[pos + k]) * m_taps[k];
            out.append(short(qBound(-32768.0, std::floor(acc + 0.5), 32767.0)));
            pos += m_factor;
        }
        // si conserva quel che serve alla prossima volta
        const int keepFrom = pos;
        m_history = buf.mid(keepFrom);
        m_phase = 0;
        // la fase e' implicita nella lunghezza della storia conservata: la
        // prossima uscita comincia da 0 su (history + nuovi campioni)
        return out;
    }

    QVector<short> process(const QVector<short>& in) { return process(in.constData(), in.size()); }

private:
    int            m_factor {1};
    QVector<double> m_taps;
    QVector<float>  m_history;
    int            m_phase {0};
};

// Dalla frequenza dichiarata nell'header alla frequenza del decoder. Si
// accettano i rapporti interi (48000, 24000, 12000): e' quello che il gateway
// manda. Qualunque altro valore torna 0 e il pacchetto si scarta, perche'
// riprodurre audio a una frequenza sbagliata lo farebbe suonare a un'altra
// velocita' e il decoder non troverebbe niente.
inline int decimationFactorFor(quint32 rate)
{
    if (rate == 0 || rate % kDecoderRate != 0)
        return 0;
    const quint32 f = rate / kDecoderRate;
    return (f == 1 || f == 2 || f == 4) ? int(f) : 0;
}

}  // namespace decolink
