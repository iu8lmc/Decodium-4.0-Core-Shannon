// Compressione senza perdite dell'audio per i modi digitali, formato del profilo
// DIGI di Decolink v3. Il formato dei blocchi e' quello di gui/lossless.h del
// repository iu8lmc/Decolink e va tenuto identico bit per bit: gateway e client
// devono rifare gli stessi conti interi.
//
// Perche' non Opus: e' un codificatore percettivo, butta via cio' che l'orecchio
// non sente. Su FT8 a -24 dB di rapporto segnale/rumore quello che l'orecchio
// non sente E' il segnale, e il gateway non deve essere il motivo per cui una
// decodifica manca.
//
// Perche' non libFLAC: farebbe esattamente questo lavoro, ma la sua interfaccia
// e' pensata per i file e vuole callback e blocchi grandi. Qui servono blocchi da
// 40 ms decodificabili uno per uno, indipendenti fra loro — se un pacchetto si
// perde, gli altri devono restare leggibili. La tecnica e' la stessa che usa
// FLAC nella modalita' a predittore fisso, in 150 righe che si possono provare
// bit per bit.
//
// Come funziona:
//
//   1. Predizione a differenze finite. L'audio e' correlato: il campione dopo
//      assomiglia a quello prima. Invece dei campioni si trasmette l'errore di
//      previsione, che e' un numero molto piu' piccolo. Si provano gli ordini da
//      0 a 3 e si tiene quello che sbaglia meno — su un segnale rumoroso vince
//      l'ordine basso, su un tono pulito l'ordine alto.
//
//   2. Codifica di Rice. Numeri piccoli ma di grandezza variabile: si divide
//      ciascuno per 2^k, si scrive il quoziente in unario e il resto in k bit.
//      Con k scelto sulla media dei residui, un valore tipico costa k+1 bit
//      invece di 16.
//
// Sull'audio di una radio (fruscio piu' segnali) rende il 50-60%: non e' la meta'
// di Opus, ma e' l'unico modo di garantire che il decodificatore riceva
// esattamente cio' che e' uscito dal rig.

#pragma once

#include <cmath>

#include <QByteArray>
#include <QVector>

#include <cstdint>
#include <cstdlib>

namespace decolink {
namespace lossless {

// Scrittore di bit. Accumula in un intero a 64 bit e svuota a byte interi: e'
// piu' veloce che maneggiare un bit per volta.
class BitOut
{
public:
    explicit BitOut(QByteArray& dove) : m_out(dove) {}
    void bit(quint32 v, int n)
    {
        while (n > 0) {
            int const quanti = qMin(n, 64 - m_usati);
            m_acc = (m_acc << quanti) | ((quint64(v) >> (n - quanti)) & ((quint64(1) << quanti) - 1));
            m_usati += quanti;
            n -= quanti;
            while (m_usati >= 8) {
                m_usati -= 8;
                m_out.append(char((m_acc >> m_usati) & 0xFF));
            }
        }
    }
    // Unario: q zeri e un uno. I quozienti grandi si tagliano a monte scegliendo
    // bene k, quindi qui non c'e' rischio di file che esplodono.
    void unario(quint32 q)
    {
        while (q >= 32) { bit(0, 32); q -= 32; }
        if (q > 0) bit(0, int(q));
        bit(1, 1);
    }
    void chiudi()
    {
        if (m_usati > 0) {
            m_out.append(char((m_acc << (8 - m_usati)) & 0xFF));
            m_usati = 0;
        }
    }

private:
    QByteArray& m_out;
    quint64 m_acc {0};
    int m_usati {0};
};

// Lettore di bit. Ogni lettura oltre la fine restituisce zeri e alza il flag di
// esaurimento: un pacchetto tagliato deve dare un errore, non leggere memoria
// che non gli appartiene.
class BitIn
{
public:
    BitIn(const uchar* dati, int len) : m_d(dati), m_len(len) {}
    quint32 bit(int n)
    {
        quint32 v = 0;
        for (int i = 0; i < n; ++i) {
            if (m_pos >= m_len * 8) { m_fine = true; return v << (n - i); }
            int const byte = m_pos >> 3;
            int const off = 7 - (m_pos & 7);
            v = (v << 1) | ((m_d[byte] >> off) & 1);
            ++m_pos;
        }
        return v;
    }
    quint32 unario()
    {
        quint32 q = 0;
        while (true) {
            if (m_pos >= m_len * 8) { m_fine = true; return q; }
            int const byte = m_pos >> 3;
            int const off = 7 - (m_pos & 7);
            bool const uno = (m_d[byte] >> off) & 1;
            ++m_pos;
            if (uno) return q;
            if (++q > 1u << 20) { m_fine = true; return q; }   // dati assurdi
        }
    }
    bool esaurito() const { return m_fine; }

private:
    const uchar* m_d;
    int m_len;
    int m_pos {0};
    bool m_fine {false};
};

// Da intero con segno a intero senza segno alternando i segni (0,-1,1,-2,2...):
// serve perche' Rice codifica numeri non negativi e i residui sono per metà
// negativi.
inline quint32 zigzag(qint32 v) { return quint32((v << 1) ^ (v >> 31)); }
inline qint32 unzigzag(quint32 v) { return qint32((v >> 1) ^ (~(v & 1) + 1)); }

// ---- predittore adattivo (LPC) ----
//
// Le differenze finite vanno bene su segnali lenti, ma su un tono a 1500 Hz
// campionato a 12 kHz — otto campioni per ciclo — sbagliano di brutto: il
// predittore giusto per una sinusoide e' x[n] = 2cos(w)·x[n-1] − x[n-2], e
// 2cos(w) vale 1,41 a quella frequenza, non 2. Usando 2 il "residuo" diventa
// piu' grande del campione, e comprimere lo peggiora invece di migliorarlo.
//
// Qui i coefficienti si calcolano sul blocco: autocorrelazione, Levinson-Durbin,
// e poi si quantizzano a interi. La quantizzazione non e' un dettaglio: encoder e
// decoder devono fare *esattamente* gli stessi conti, e con i numeri in virgola
// mobile due macchine diverse potrebbero arrotondare in modo diverso, il che
// vorrebbe dire audio corrotto invece di audio identico. Quindi la predizione
// vive nei numeri interi, con uno spostamento comune.

constexpr int kLpcMaxOrdine = 8;
constexpr int kLpcPrecisione = 14;        // bit dei coefficienti quantizzati

// Coefficienti ottimali per il blocco. Restituisce false se il segnale non ha
// struttura da sfruttare (silenzio, rumore puro).
inline bool lpcCoeff(const qint16* x, int n, int ordine, QVector<double>& a)
{
    if (n <= ordine * 2 || ordine < 1 || ordine > kLpcMaxOrdine) return false;

    QVector<double> r(ordine + 1, 0.0);
    for (int lag = 0; lag <= ordine; ++lag) {
        double acc = 0;
        for (int i = lag; i < n; ++i) acc += double(x[i]) * double(x[i - lag]);
        r[lag] = acc;
    }
    if (r[0] <= 0.0) return false;

    // Levinson-Durbin
    a.assign(ordine + 1, 0.0);
    double err = r[0];
    for (int i = 1; i <= ordine; ++i) {
        double acc = r[i];
        for (int j = 1; j < i; ++j) acc -= a[j] * r[i - j];
        double const k = acc / err;
        if (!std::isfinite(k) || std::fabs(k) >= 1.0) {
            // Il predittore diventerebbe instabile: si tiene l'ordine raggiunto
            // finora, che e' comunque valido.
            return i > 1;
        }
        QVector<double> nuovo = a;
        nuovo[i] = k;
        for (int j = 1; j < i; ++j) nuovo[j] = a[j] - k * a[i - j];
        a = nuovo;
        err *= (1.0 - k * k);
        if (err <= 0.0) return i > 1;
    }
    return true;
}

// Comprime un blocco di campioni int16 mono.
//
//   u16  numero di campioni
//   u8   ordine(bit 0-3) | 0x80 se predittore adattivo
//   u8   k di Rice(bit 0-3) | spostamento dei coefficienti(bit 4-7)
//   i16  × ordine: i campioni iniziali, che non hanno un passato da cui prevedere
//   i16  × ordine: i coefficienti, solo col predittore adattivo
//   bit  residui in codifica di Rice
inline QByteArray comprimi(const qint16* campioni, int n)
{
    QByteArray out;
    if (n <= 0 || n > 65535) return out;

    // Si prova ogni ordine e si guarda quale lascia i residui piu' piccoli.
    // Provare costa quattro passate su 480 campioni: nulla, rispetto al
    // guadagno di scegliere bene.
    int ordineMigliore = 0;
    qint64 costoMigliore = -1;
    QVector<qint32> residui(n), migliori;
    for (int ordine = 0; ordine <= 3 && ordine < n; ++ordine) {
        qint64 somma = 0;
        for (int i = 0; i < n; ++i) {
            qint32 r;
            if (i < ordine) {
                r = 0;                       // i primi vanno in chiaro
            } else {
                switch (ordine) {
                case 0: r = campioni[i]; break;
                case 1: r = campioni[i] - campioni[i - 1]; break;
                case 2: r = campioni[i] - 2 * campioni[i - 1] + campioni[i - 2]; break;
                default: r = campioni[i] - 3 * campioni[i - 1] + 3 * campioni[i - 2]
                             - campioni[i - 3]; break;
                }
                somma += std::abs(r);
            }
            residui[i] = r;
        }
        if (costoMigliore < 0 || somma < costoMigliore) {
            costoMigliore = somma;
            ordineMigliore = ordine;
            migliori = residui;
        }
    }

    // Ora si prova il predittore adattivo e si tiene il migliore dei due. Su
    // audio con struttura (toni, segnali digitali) vince quasi sempre lui; sul
    // rumore puro non c'e' niente da prevedere e vincono le differenze finite,
    // che almeno non costano i coefficienti nell'intestazione.
    bool usaLpc = false;
    int ordineLpc = 0, shift = 0;
    QVector<qint32> coeffQ;
    QVector<qint32> residuiLpc;
    {
        QVector<double> a;
        int const ordineProva = qMin(kLpcMaxOrdine, n / 8);
        if (ordineProva >= 2 && lpcCoeff(campioni, n, ordineProva, a)) {
            int const ordine = a.size() - 1;
            // Quantizzazione: si cerca lo spostamento che fa stare i coefficienti
            // nella precisione scelta senza troncarli.
            double massimo = 0;
            for (int i = 1; i <= ordine; ++i) massimo = qMax(massimo, std::fabs(a[i]));
            if (massimo > 0) {
                shift = kLpcPrecisione - 1;
                while (shift > 0 && massimo * double(1 << shift) > double((1 << (kLpcPrecisione - 1)) - 1))
                    --shift;
                coeffQ.resize(ordine + 1);
                for (int i = 1; i <= ordine; ++i)
                    coeffQ[i] = qint32(std::llround(a[i] * double(1 << shift)));

                // Residui con i coefficienti QUANTIZZATI e in aritmetica intera:
                // e' l'unico modo perche' il decodificatore rifaccia gli stessi
                // conti bit per bit.
                residuiLpc.resize(n);
                qint64 costo = 0;
                bool ok = true;
                for (int i = 0; i < n; ++i) {
                    if (i < ordine) { residuiLpc[i] = 0; continue; }
                    qint64 pred = 0;
                    for (int j = 1; j <= ordine; ++j)
                        pred += qint64(coeffQ[j]) * qint64(campioni[i - j]);
                    qint64 const res = qint64(campioni[i]) - (pred >> shift);
                    if (res > 0x7FFFFFFFLL || res < -0x80000000LL) { ok = false; break; }
                    residuiLpc[i] = qint32(res);
                    costo += std::llabs(res);
                }
                int const utiliLpc = n - ordine;
                // Ai coefficienti serve spazio nell'intestazione: due byte
                // ciascuno. Si tiene il predittore adattivo solo se li ripaga.
                if (ok && utiliLpc > 0 && costo + qint64(ordine) * 2 * 8 < costoMigliore) {
                    usaLpc = true;
                    ordineLpc = ordine;
                    costoMigliore = costo;
                }
            }
        }
    }

    int const ordine = usaLpc ? ordineLpc : ordineMigliore;
    QVector<qint32> const& res = usaLpc ? residuiLpc : migliori;

    // k di Rice dalla media dei residui: si cerca il k tale che 2^k sia vicino
    // alla grandezza tipica, cosi' il quoziente unario resta piccolo.
    int const utili = n - ordine;
    double const media = utili > 0 ? double(costoMigliore) / double(utili) : 1.0;
    int k = 0;
    while (k < 15 && (double(1u << k) < media)) ++k;

    out.append(char((n >> 8) & 0xFF));
    out.append(char(n & 0xFF));
    out.append(char((ordine & 0xF) | (usaLpc ? 0x80 : 0)));
    out.append(char((k & 0xF) | ((shift & 0xF) << 4)));
    for (int i = 0; i < ordine; ++i) {
        out.append(char((quint16(campioni[i]) >> 8) & 0xFF));
        out.append(char(quint16(campioni[i]) & 0xFF));
    }
    if (usaLpc) {
        for (int i = 1; i <= ordine; ++i) {
            out.append(char((quint16(qint16(coeffQ[i])) >> 8) & 0xFF));
            out.append(char(quint16(qint16(coeffQ[i])) & 0xFF));
        }
    }

    BitOut bo(out);
    for (int i = ordine; i < n; ++i) {
        quint32 const z = zigzag(res[i]);
        bo.unario(z >> k);
        if (k > 0) bo.bit(z & ((1u << k) - 1), k);
    }
    bo.chiudi();
    return out;
}

// Decomprime un blocco. Restituisce i campioni, o una lista vuota se il blocco
// e' incoerente o tagliato.
inline QVector<qint16> decomprimi(const QByteArray& blocco)
{
    QVector<qint16> out;
    if (blocco.size() < 4) return out;
    const uchar* d = reinterpret_cast<const uchar*>(blocco.constData());

    int const n = (int(d[0]) << 8) | d[1];
    bool const lpc = (d[2] & 0x80) != 0;
    int const ordine = d[2] & 0xF;
    int const k = d[3] & 0xF;
    int const shift = (d[3] >> 4) & 0xF;
    if (n <= 0 || ordine > (lpc ? kLpcMaxOrdine : 3) || (lpc && ordine < 1)) return out;

    int const testa = 4 + 2 * ordine + (lpc ? 2 * ordine : 0);
    if (blocco.size() < testa) return out;

    out.resize(n);
    for (int i = 0; i < ordine; ++i)
        out[i] = qint16((quint16(d[4 + 2 * i]) << 8) | d[5 + 2 * i]);

    QVector<qint32> coeff;
    if (lpc) {
        coeff.resize(ordine + 1);
        int const base = 4 + 2 * ordine;
        for (int i = 1; i <= ordine; ++i)
            coeff[i] = qint32(qint16((quint16(d[base + 2 * (i - 1)]) << 8)
                                     | d[base + 2 * (i - 1) + 1]));
    }

    BitIn bi(d + testa, blocco.size() - testa);
    for (int i = ordine; i < n; ++i) {
        quint32 const q = bi.unario();
        quint32 const r = (k > 0) ? bi.bit(k) : 0;
        if (bi.esaurito()) { out.clear(); return out; }
        qint32 const res = unzigzag((q << k) | r);
        qint64 v;
        if (lpc) {
            // Gli stessi conti interi dell'encoder, nello stesso ordine: e' cio'
            // che rende la ricostruzione identica e non soltanto simile.
            qint64 pred = 0;
            for (int j = 1; j <= ordine; ++j)
                pred += qint64(coeff[j]) * qint64(out[i - j]);
            v = qint64(res) + (pred >> shift);
        } else {
            switch (ordine) {
            case 0: v = res; break;
            case 1: v = qint64(res) + out[i - 1]; break;
            case 2: v = qint64(res) + 2 * out[i - 1] - out[i - 2]; break;
            default: v = qint64(res) + 3 * out[i - 1] - 3 * out[i - 2] + out[i - 3]; break;
            }
        }
        out[i] = qint16(v);
    }
    return out;
}

} // namespace lossless
} // namespace decolink
