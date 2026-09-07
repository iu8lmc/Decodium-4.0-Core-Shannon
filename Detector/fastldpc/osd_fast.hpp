// osd_fast.hpp — OsdFast: lo stesso OSD di decoder.hpp, riscritto, piu' l'ordine 3.
// Il risultato e' identico parola per parola (cpp/verify.cpp lo confronta con
// l'implementazione di riferimento), ma il costo non dipende quasi piu' dal
// numero di candidati provati: a 1 dB, per parola tentata, 15,2 us con OSD-0 e
// 16,9 us con OSD-2 su span 91, cioe' ~4100 candidati. Prima erano 34 e 456.
//
// LA COSA CHE CONTA: LA CRC-14 E' LINEARE.
//
// L'LFSR parte da zero e non c'e' XOR finale, quindi crc14_syn (decoder.hpp) e'
// una funzione lineare su GF(2) dei 91 bit. Il contributo alla sindrome del
// flip di un bit d'informazione si precalcola una volta per parola, e allora
// testare la CRC di un candidato costa UNO XOR su un uint16 invece di
// ricostruire i 174 bit e ripercorrere l'LFSR.
//
// Da qui discende l'ordine dei filtri, che e' il cuore del guadagno: la CRC e'
// il filtro piu' selettivo (accetta 1 candidato su 16384) ed e' diventato anche
// il piu' economico, quindi va per primo. Score e costruzione della parola si
// pagano solo sui pochissimi candidati che la superano — e con OSD-2 su span 91
// o OSD-3, che sono decine di migliaia, e' la differenza fra praticabile e no.
// I contributi dei 91 bit si ricavano in blocco per bit-slicing: 14
// accumulatori da 256 bit, uno per bit di CRC, invece di ripercorrere le M
// righe per ogni bit.
//
// Il resto, in ordine di peso:
//
//  * GAUSS BRANCHLESS SU RIGHE DA 256 BIT. Una riga di H e' 174 bit: sta in
//    quattro uint64 allineati, cioe' in un registro YMM. L'eliminazione e'
//    `riga ^= pivot & maschera` senza salto condizionato, su due intervalli
//    attorno alla riga pivot. Il riferimento fa `if (bit) xorrow(...)` su tre
//    uint64 sciolti, e quel salto sbaglia predizione una volta su due, 83x83
//    volte per parola. Resta comunque la voce piu' grossa: vedi roadmap.
//
//  * PARITA' DEI CANDIDATI COME BITMASK. Il pattern di parita' e' un uint64[2]
//    e il flip di un bit d'informazione e' uno XOR, non M=83 assegnamenti. Lo
//    score e' la somma dei pesi dei bit a 1 di d = par ^ hardpiv, scorsi con ctz
//    e interrotta appena supera il migliore trovato.
//
//  * PRUNING PER LOWER BOUND. Lo score e' >= la somma dei |LLR| dei bit
//    flippati, nota prima di valutare il candidato; info_ e' ordinato per
//    affidabilita' crescente, quindi appena quel bound raggiunge il best si
//    esce dal ciclo (prune_limit lo trova per ricerca binaria).
//
//  * COLMASK PIGRE, radix sort al posto di std::sort, e test CRC vettorizzato
//    a 16 candidati per volta con AVX2.
//
// Il sort per affidabilita' ha tie-break esplicito sull'indice: i |LLR| sono
// interi quantizzati e le parita' sono frequenti, quindi senza un tie-break
// definito due implementazioni equivalenti sceglierebbero candidati diversi e
// non sarebbero confrontabili.
//
// Il gate sulla distanza soft (nd_max) e' identico a OsdDecoder ed e' cio' che
// rende usabile l'ordine alto: senza, a 0,5 dB l'OSD-3 produce 3542 false
// decodifiche su 20000. Vedi README.
#pragma once
#include "decoder.hpp"
#include "bits.hpp"
#include "plausible.hpp"
#include <array>
#include <climits>
#include <cassert>
#include <chrono>
#if defined(__AVX2__)
#include <immintrin.h>
#endif

// Profilo delle fasi: -DOSD_PROFILE lo accende, altrimenti costo zero.
#ifdef OSD_PROFILE
#define OSD_T0() const auto _t0 = std::chrono::steady_clock::now()
#define OSD_T(acc) do { const auto _t1 = std::chrono::steady_clock::now();     acc += std::chrono::duration<double>(_t1 - _tp).count(); _tp = _t1; } while (0)
#define OSD_TSTART() auto _tp = std::chrono::steady_clock::now()
#else
#define OSD_T0() do {} while (0)
#define OSD_T(acc) do {} while (0)
#define OSD_TSTART() do {} while (0)
#endif

// Conteggio dei candidati sottoposti alla CRC-14: -DOSD_COUNT lo accende,
// altrimenti costo zero. Sta nei cicli piu' caldi del decoder, quindi non puo'
// essere sempre attivo; ma e' la grandezza che predice i nominativi fantasma
// (vedi il commento su n_crc piu' sotto).
#ifdef OSD_COUNT
#define OSD_C(k) (n_crc += (k))
#else
#define OSD_C(k) do {} while (0)
#endif

class OsdFast {
public:
    static constexpr int RW    = 4;     // 174 bit -> 4 x uint64 = un registro YMM
    static constexpr int MWMAX = 2;     // fino a 128 check
    static constexpr int NCRC  = 91;    // bit coperti dalla CRC-14: 77 messaggio + 14

    float nd_max = 0.085f;              // vedi OsdDecoder::nd_max

    // Maschera dei tipi di messaggio i3 ammessi: 0 spegne il controllo,
    // plaus::kTuttiDefiniti non scarta niente di valido, plaus::kSoloUsati
    // stringe ai tipi che un QSO usa davvero. Vedi cpp/plausible.hpp.
    uint32_t tipi_ammessi = 0;

    // Ricerca a coppie mirata, sul modello del passo npre2 di WSJT-X
    // (osd174_91). Invece di provare tutte le C(K,2) coppie di bit
    // d'informazione, si cercano solo quelle che azzerano i `ntau` bit di
    // parita' PIU' AFFIDABILI: se un candidato sbaglia proprio quelli, la
    // coppia che li corregge e' quasi sempre la strada giusta.
    //
    // La chiave e' lineare su GF(2), quindi chiave(a^b) = chiave(a)^chiave(b):
    // si calcola una volta per bit e ogni coppia costa uno XOR.
    bool pair_search = false;
    int  ntau = 14;                     // bit di parita' usati come chiave

    // Quanti bit d'informazione entrano nelle COPPIE. 0 = tutti.
    //
    // build_pairs costruisce C(K,2) coppie, e con K=91 sono 4095, ognuna con
    // sei scritture su una tabella da 16384 voci: misurato, e' il 77% del tempo
    // dell'intero decoder. Ma le coppie fra bit molto affidabili vengono
    // costruite, inserite, e poi SCARTATE al momento dell'uso dal taglio per
    // limite inferiore, perche' la somma dei loro |LLR| supera gia' il migliore
    // corrente. Costruirle e' lavoro buttato.
    //
    // info_ e' ordinato per affidabilita' crescente, quindi limitarsi ai primi
    // pair_span significa tenere esattamente quelli dove gli errori stanno.
    int  pair_span = 0;

    OsdFast(const Code& code, int order = 1, int span2 = 32, int span3 = 0)
        : c_(code), order_(order), span2_(span2), span3_(span3 > 0 ? span3 : span2 / 2),
          absL_(code.N), hard_(code.N), best_(code.N), tmp_(code.N), ispiv_(code.N),
          rbuf_(code.N), perm_(code.N), pivcol_(code.M),
          rows_(code.M), base_(code.M), w_(code.M),
          colmask_(code.N), cm_gen_(code.N, 0), gpiv_(code.M), dcrc_(code.N),
          pkey_(code.N), tau_idx_(code.M) {
        if (code.M > 64 * MWMAX) throw std::runtime_error("OsdFast: M troppo grande");
        if (code.N >= 512) throw std::runtime_error("OsdFast: N troppo grande per la chiave di sort");
        if (code.N < NCRC) throw std::runtime_error("OsdFast: N < 91, layout CRC non compatibile");
        for (int m = 0; m < code.M; ++m) {
            base_[m] = Row{};
            for (int e = code.row_ptr[m]; e < code.row_ptr[m + 1]; ++e) setb(base_[m], code.col_idx[e]);
        }
        // g_[v] = sindrome CRC del vettore con il solo bit v acceso. Poiche'
        // crc14_syn e' lineare, la sindrome di una parola qualsiasi e' lo XOR
        // dei g_ dei suoi bit a 1: e' cio' che rende il test incrementale.
        std::vector<uint8_t> e(code.N, 0);
        for (int v = 0; v < NCRC; ++v) { e[v] = 1; g_[v] = crc14_syn(e.data()); e[v] = 0; }
    }

    bool decode(const int16_t* L, uint8_t* out, const float* llr_ch = nullptr,
                float* nd = nullptr, const uint8_t* apmask = nullptr) {
        OSD_TSTART();
        const int N = c_.N, M = c_.M;
        for (int v = 0; v < N; ++v) {
            const int a = L[v] < 0 ? -L[v] : L[v];
            absL_[v] = a; hard_[v] = (uint8_t)(L[v] < 0);
        }
        radix_sort_by_absL(N);      // perm_ = indici per |LLR| crescente, poi per indice

        OSD_T(t_sort);
        // ---- Gauss: pivot sulle colonne meno affidabili (most reliable basis)
        std::copy(base_.begin(), base_.end(), rows_.begin());
        int nused = 0;
        for (int k = 0; k < N && nused < M; ++k) {
            const int col = perm_[k], wd = col >> 6, sh = col & 63;

            // Ricerca del pivot con uscita anticipata: circa meta' delle
            // colonne provate e' dipendente e finisce nel set d'informazione,
            // e per quelle non si deve pagare nient'altro.
            const uint64_t bit = 1ull << sh;
            int pr = -1;
            for (int m = nused; m < M; ++m) if (rows_[m].w[wd] & bit) { pr = m; break; }
            if (pr < 0) continue;
            if (pr != nused) std::swap(rows_[pr], rows_[nused]);

            // Eliminazione su due intervalli attorno alla riga pivot: nessun
            // salto ne' confronto nel corpo, solo una maschera che vale 0 o ~0.
            // Provato anche a estrarre prima la colonna come bitmask per
            // toccare le sole righe con 1 (meta' degli XOR): costa di piu', 83
            // letture a stride 32 byte con catena seriale di OR battono il
            // risparmio. Idem per lo srotolamento esplicito.
            const Row pv = rows_[nused];
            eliminate(0, nused, pv, wd, sh);
            eliminate(nused + 1, M, pv, wd, sh);
            pivcol_[nused] = col; ++nused;
        }
        if (nused < M) return false;                    // H singolare: non accade per questo codice

        OSD_T(t_gauss);
        std::fill(ispiv_.begin(), ispiv_.end(), (uint8_t)0);
        for (int r = 0; r < M; ++r) ispiv_[pivcol_[r]] = 1;
        info_.clear();
        for (int k = N - 1; k >= 0; --k) if (!ispiv_[perm_[k]]) info_.push_back(perm_[k]);
        std::reverse(info_.begin(), info_.end());       // info_[0] = meno affidabile
        const int K = (int)info_.size();

        // ---- precalcolo per parola
        ++gen_;                                         // invalida le colmask senza ripulirle
        for (int r = 0; r < M; ++r) w_[r] = absL_[pivcol_[r]];
        Row hx{};
        for (int t = 0; t < K; ++t) if (hard_[info_[t]]) setb(hx, info_[t]);
        Mask d0{};
        for (int r = 0; r < M; ++r) {
            uint64_t x = 0;
            for (int w = 0; w < RW; ++w) x ^= rows_[r].w[w] & hx.w[w];
            if ((fl_popcount64(x) & 1) ^ (int)hard_[pivcol_[r]])
                d0[r >> 6] |= 1ull << (r & 63);         // d0 = basepar ^ hardpiv
        }

        OSD_T(t_pre);
        // ---- sindrome CRC incrementale
        for (int r = 0; r < M; ++r) gpiv_[r] = gcol(pivcol_[r]);
        // Bit-slicing: acc_[b] = XOR delle righe pivot il cui g ha il bit b.
        // Cosi' il contributo CRC del flip di un bit d'informazione v si legge
        // come i 14 bit in posizione v di acc_[0..13], invece di ripercorrere
        // le M righe per ogni v.
        for (int b = 0; b < 14; ++b) acc_[b] = Row{};
        for (int r = 0; r < M; ++r) {
            uint16_t gp = gpiv_[r];
            while (gp) {
                const int b = fl_ctz64(gp); gp = (uint16_t)(gp & (gp - 1));
                for (int w = 0; w < RW; ++w) acc_[b].w[w] ^= rows_[r].w[w];
            }
        }
        uint16_t syn0 = 0;                              // sindrome CRC del candidato OSD-0
        for (int v = 0; v < NCRC; ++v) if (hard_[v]) syn0 ^= g_[v];
        for (int w = 0; w < MWMAX; ++w) {
            uint64_t x = d0[w];
            while (x) { const int b = fl_ctz64(x); x &= x - 1; syn0 ^= gpiv_[(w << 6) + b]; }
        }
        for (int t = 0; t < K; ++t) {                   // contributo di ogni flip
            const int v = info_[t], wd = v >> 6, sh = v & 63;
            uint16_t s = gcol(v);
            for (int b = 0; b < 14; ++b) s ^= (uint16_t)(((acc_[b].w[wd] >> sh) & 1ull) << b);
            dcrc_[t] = s;
        }

        OSD_T(t_crc);
        // ---- enumerazione dei candidati, dal piu' probabile in giu'
        // Ordine dei filtri: prima il lower bound (interrompe il ciclo), poi la
        // CRC (uno XOR e un test), e solo per i pochi candidati che la passano
        // lo score completo e la costruzione della parola.
        best_score_ = LONG_MAX; found_ = false;
        OSD_C(1);
        if (syn0 == 0) eval(d0, -1, -1, 0);                             // OSD-0
        if (order_ >= 1) {
            const int lim2 = std::min(span2_, K), lim3 = std::min(span3_, K);
            for (int t = 0; t < K; ++t) {
                const long a_t = absL_[info_[t]];
                if (a_t >= best_score_) break;          // info_ ordinato: il resto e' peggio
                OSD_C(1);
                const uint16_t s1 = (uint16_t)(syn0 ^ dcrc_[t]);
                if (s1 == 0) {
                    Mask d1 = d0; xorm(d1, colmask(t));
                    eval(d1, info_[t], -1, a_t);
                }
                if (order_ >= 2 && t < lim2) {
                    int u = t + 1, umax = prune_limit(t + 1, lim2, a_t);
#if defined(__AVX2__)
                    // Il test CRC e' un confronto a zero su uint16: se ne fanno
                    // 16 per volta, e nella stragrande maggioranza dei blocchi
                    // non passa nessuno (la CRC accetta 1 su 16384).
                    const __m256i vs1 = _mm256_set1_epi16((short)s1);
                    const __m256i vz  = _mm256_setzero_si256();
                    for (; u + 16 <= umax; u += 16) {
                        OSD_C(16);
                        const __m256i dv = _mm256_loadu_si256((const __m256i*)&dcrc_[u]);
                        unsigned mk = (unsigned)_mm256_movemask_epi8(
                                _mm256_cmpeq_epi16(_mm256_xor_si256(dv, vs1), vz));
                        while (mk) {
                            const int k = fl_ctz64(mk) >> 1;    // 2 bit di maschera per lane
                            mk &= ~(3u << (2 * k));
                            const int uu = u + k;
                            const long lb = a_t + absL_[info_[uu]];
                            if (lb >= best_score_) continue;
                            Mask d2 = d0; xorm(d2, colmask(t)); xorm(d2, colmask(uu));
                            eval(d2, info_[t], info_[uu], lb);
                        }
                        if (found_) {                            // best migliorato: restringi
                            umax = prune_limit(u + 16, lim2, a_t);
                            if (u + 16 >= umax) break;
                        }
                    }
#endif
                    for (; u < umax; ++u) {
                        const long lb = a_t + absL_[info_[u]];
                        if (lb >= best_score_) break;
                        OSD_C(1);
                        if ((uint16_t)(s1 ^ dcrc_[u]) != 0) continue;
                        Mask d2 = d0; xorm(d2, colmask(t)); xorm(d2, colmask(u));
                        eval(d2, info_[t], info_[u], lb);
                    }
                }
                // Ordine 3. Ha senso solo perche' la CRC di un candidato costa
                // uno XOR: le C(span3,3) triple si scremano quasi tutte senza
                // toccare ne' la parola ne' lo score.
                if (order_ >= 3 && t < lim3) {
                    for (int u = t + 1; u < lim3; ++u) {
                        const long lb2 = a_t + absL_[info_[u]];
                        if (lb2 >= best_score_) break;
                        const uint16_t s2 = (uint16_t)(s1 ^ dcrc_[u]);
                        const int xmax = prune_limit(u + 1, lim3, lb2);
                        for (int x = u + 1; x < xmax; ++x) {
                            const long lb3 = lb2 + absL_[info_[x]];
                            if (lb3 >= best_score_) break;
                            OSD_C(1);
                            if ((uint16_t)(s2 ^ dcrc_[x]) != 0) continue;
                            Mask d3 = d0;
                            xorm(d3, colmask(t)); xorm(d3, colmask(u)); xorm(d3, colmask(x));
                            eval3(d3, info_[t], info_[u], info_[x], lb3);
                        }
                    }
                }
            }
        }
        OSD_T(t_enum);
        // ---- ricerca a coppie mirata (modello npre2 di WSJT-X)
        // Per il candidato base e per ogni singolo flip, si cercano le coppie
        // che azzerano i tau bit di parita' piu' affidabili. La chiave e'
        // lineare, quindi il pattern di una coppia e' lo XOR delle due chiavi.
        if (pair_search && K > 2) {
            const int tau = std::min (ntau, std::min (M, 20));
            pick_tau (M, tau);
            const int Kp = (pair_span > 0 && pair_span < K) ? pair_span : K;
            build_pairs (Kp, tau);

            // 1) dal candidato base, 2) da ciascun singolo flip: sono i punti
            //    di partenza da cui la coppia deve completare la correzione.
            const int nstart = std::min (K, span2_ > 0 ? span2_ : K);
            for (int st = -1; st < nstart; ++st) {
                Mask dbase = d0;
                long lb0 = 0;
                uint16_t syn_base = syn0;
                if (st >= 0) {
                    lb0 = absL_[info_[st]];
                    if (lb0 >= best_score_) break;      // info_ ordinato per |LLR| crescente
                    xorm (dbase, colmask (st));
                    syn_base = (uint16_t) (syn_base ^ dcrc_[st]);
                }
                const uint32_t want = key_of (dbase, tau);
                if (box_gen_[want] != box_generation_) continue;   // nessuna coppia con quel pattern
                for (int idx = box_head_[want]; idx >= 0; idx = box_next_[(size_t) idx]) {
                    const int i1 = box_i1_[(size_t) idx], i2 = box_i2_[(size_t) idx];
                    if (i1 == st || i2 == st) continue;            // gia' flippato
                    const long lb = lb0 + absL_[info_[i1]] + absL_[info_[i2]];
                    if (lb >= best_score_) continue;
                    OSD_C(1);
                    if ((uint16_t) (syn_base ^ dcrc_[i1] ^ dcrc_[i2]) != 0) continue;   // CRC: uno XOR
                    Mask d = dbase;
                    xorm (d, colmask (i1));
                    xorm (d, colmask (i2));
                    if (st >= 0) eval3 (d, info_[st], info_[i1], info_[i2], lb);
                    else         eval  (d, info_[i1], info_[i2], lb);
                }
            }
        }

        if (!found_) return false;

        if (llr_ch) {
            // I bit noti per ipotesi a priori sono esclusi: hanno |LLR|
            // saturato e, lasciati dentro, gonfierebbero il denominatore fino
            // a rendere nd minuscolo per qualunque candidato, disattivando di
            // fatto il gate proprio quando l'AP e' attivo.
            double num = 0, den = 0;
            for (int v = 0; v < N; ++v) {
                if (apmask && apmask[v]) continue;
                const double a = llr_ch[v] < 0 ? -llr_ch[v] : llr_ch[v];
                den += a;
                if (best_[v] != (uint8_t)(llr_ch[v] < 0)) num += a;
            }
            const float d = den > 0 ? (float)(num / den) : 1.0f;
            if (nd) *nd = d;
            if (d > nd_max) return false;               // gate anti-false-decode
        }
        for (int v = 0; v < N; ++v) out[v] = best_[v];
        return true;
    }

    // Score del candidato accettato dall'ultima decode() riuscita: somma dei
    // |LLR a posteriori| smentiti, in unita' Q=1/8.
    long last_score() const { return best_score_; }

    // Tempi cumulati per fase, in secondi (solo con -DOSD_PROFILE).
    double t_sort = 0, t_gauss = 0, t_pre = 0, t_crc = 0, t_enum = 0;
    long n_search = 0, n_elim = 0;

    // Candidati sottoposti alla CRC-14 (contati solo con -DOSD_COUNT).
    //
    // E' LA grandezza che predice i nominativi fantasma, e per molto tempo non
    // e' stata contata. La CRC ammette un candidato sbagliato ogni 16384:
    // le false accettazioni attese per parola sono n_crc/16384, ridotte poi dai
    // bit del controllo di plausibilita' e dal gate nd. Contare invece i
    // fantasmi su un numero FISSO di candidati di rumore e' esattamente
    // l'errore che fece sembrare buona la ricerca larga, ritirata poi due volte
    // dal traffico vero: quel conteggio non vede quanti candidati per parola si
    // stanno davvero sottoponendo alla CRC.
    long long n_crc = 0;

private:
    struct alignas(32) Row { uint64_t w[RW]; };     // bitset su N
    using Mask = std::array<uint64_t, MWMAX>;       // bitset su M

    // XOR della riga pivot su tutte le righe di [lo,hi) che hanno 1 nella
    // colonna in esame, senza salto condizionato: la maschera vale 0 oppure ~0.
    inline void eliminate(int lo, int hi, const Row& pv, int wd, int sh) {
        for (int m = lo; m < hi; ++m) {
            const uint64_t sel = (uint64_t)0 - ((rows_[m].w[wd] >> sh) & 1ull);
            for (int w = 0; w < RW; ++w) rows_[m].w[w] ^= pv.w[w] & sel;
        }
    }

    static void setb(Row& r, int i) { r.w[i >> 6] |= 1ull << (i & 63); }
    static void xorm(Mask& a, const Mask& b) { for (int w = 0; w < MWMAX; ++w) a[w] ^= b[w]; }

    // Ordina gli indici per |LLR| crescente. I |LLR| sono interi a 11 bit
    // (saturati a LLR_MAX = 2047), quindi due passate radix da 6 bit bastano e
    // costano meno di un std::sort con confronti. Il radix LSD e' stabile e si
    // parte dall'ordine naturale degli indici: il tie-break e' quindi
    // l'indice crescente, deterministico e uguale a quello del riferimento.
    void radix_sort_by_absL(int N) {
        constexpr int BITS = 6, BUCKETS = 1 << BITS, MASKB = BUCKETS - 1;
        int cnt[BUCKETS];
        for (int pass = 0; pass < 2; ++pass) {
            const int sh = pass * BITS;
            const int* src = pass == 0 ? nullptr : rbuf_.data();
            for (int i = 0; i < BUCKETS; ++i) cnt[i] = 0;
            for (int i = 0; i < N; ++i) ++cnt[(absL_[pass == 0 ? i : src[i]] >> sh) & MASKB];
            int sum = 0;
            for (int i = 0; i < BUCKETS; ++i) { const int c = cnt[i]; cnt[i] = sum; sum += c; }
            int* dst = pass == 0 ? rbuf_.data() : perm_.data();
            for (int i = 0; i < N; ++i) {
                const int v = pass == 0 ? i : src[i];
                dst[cnt[(absL_[v] >> sh) & MASKB]++] = v;
            }
        }
    }

    // Colonna del bit d'informazione t nella base ridotta, calcolata al primo uso.
    const Mask& colmask(int t) {
        if (cm_gen_[t] != gen_) {
            Mask cm{};
            const int v = info_[t], wd = v >> 6;
            const uint64_t bit = 1ull << (v & 63);
            for (int r = 0; r < c_.M; ++r) if (rows_[r].w[wd] & bit) cm[r >> 6] |= 1ull << (r & 63);
            colmask_[t] = cm; cm_gen_[t] = gen_;
        }
        return colmask_[t];
    }

    // I `ntau` pivot piu' affidabili, in ordine di affidabilita' decrescente:
    // sono quelli su cui un errore e' meno probabile, quindi vederli smentiti e'
    // il segnale piu' forte che il candidato sia sbagliato.
    void pick_tau (int M, int tau) {
        tau_idx_.resize (M);
        for (int r = 0; r < M; ++r) tau_idx_[r] = r;
        std::partial_sort (tau_idx_.begin (), tau_idx_.begin () + tau, tau_idx_.begin () + M,
                           [&] (int a, int b) { return w_[a] > w_[b]; });
        tau_idx_.resize (tau);
    }

    // Estrae i `tau` bit scelti e li impacchetta in un intero. Lineare su
    // GF(2): key(a ^ b) == key(a) ^ key(b).
    inline uint32_t key_of (const Mask& m, int tau) const {
        uint32_t k = 0;
        for (int j = 0; j < tau; ++j) {
            const int r = tau_idx_[(size_t) j];
            k |= (uint32_t) ((m[r >> 6] >> (r & 63)) & 1ull) << j;
        }
        return k;
    }

    // Tabella delle coppie, una volta per parola. La generazione evita di
    // azzerare le 2^ntau teste a ogni chiamata.
    //
    // PROVATO E SCARTATO: l'incontro a meta' strada, cioe' indicizzare i K
    // SINGOLI (91 inserimenti invece di 4095) e cercare il complemento
    // want^chiave(i1) per ogni i1. Da' risultati IDENTICI parola per parola, ma
    // e' ~10% piu' lento a ogni ampiezza provata: sostituisce 4095 scritture
    // sequenziali con nstart*K letture CASUALI in una tabella da 128 KB, e le
    // letture fuori dalla cache di primo livello costano piu' delle scritture
    // sequenziali. Misurato: 71.8 us contro 67.3 a K pieno, 46.2 contro 40.9 a
    // K=48.
    void build_pairs (int K, int tau) {
        const size_t nbox = (size_t) 1 << tau;
        if (box_head_.size () != nbox) { box_head_.assign (nbox, -1); box_gen_.assign (nbox, 0); }
        ++box_generation_;
        const size_t npair = (size_t) K * (K - 1) / 2;
        box_next_.resize (npair); box_i1_.resize (npair); box_i2_.resize (npair);
        for (int t = 0; t < K; ++t) pkey_[(size_t) t] = key_of (colmask (t), tau);
        int cnt = 0;
        for (int i1 = 0; i1 < K; ++i1)
            for (int i2 = i1 + 1; i2 < K; ++i2) {
                const uint32_t pat = pkey_[(size_t) i1] ^ pkey_[(size_t) i2];
                if (box_gen_[pat] != box_generation_) { box_gen_[pat] = box_generation_; box_head_[pat] = -1; }
                box_i1_[(size_t) cnt] = i1; box_i2_[(size_t) cnt] = i2;
                box_next_[(size_t) cnt] = box_head_[pat];
                box_head_[pat] = cnt;
                ++cnt;
            }
    }

    // Somma dei pesi dei pivot smentiti, interrotta appena supera `cap`.
    inline long weight(const Mask& d, long cap) const {
        long s = 0;
        for (int w = 0; w < MWMAX; ++w) {
            uint64_t x = d[w];
            while (x) {
                const int b = fl_ctz64(x);
                x &= x - 1;
                s += w_[(w << 6) + b];
                if (s >= cap) return s;
            }
        }
        return s;
    }

    static uint16_t gcolz(const uint16_t* g, int v) { return v < NCRC ? g[v] : (uint16_t)0; }
    uint16_t gcol(int v) const { return gcolz(g_, v); }

    // Primo u in [lo,hi) per cui a_t + |LLR(info_[u])| raggiunge il best
    // corrente: da li' in poi nessun candidato puo' migliorare. |LLR| e'
    // crescente lungo info_, quindi basta una ricerca binaria.
    int prune_limit(int lo, int hi, long a_t) const {
        if (best_score_ == LONG_MAX) return hi;
        const long room = best_score_ - a_t;
        while (lo < hi) {
            const int mid = (lo + hi) >> 1;
            if ((long)absL_[info_[mid]] < room) lo = mid + 1; else hi = mid;
        }
        return lo;
    }

    // Valuta il candidato di pattern d con i bit d'informazione f1, f2 flippati
    // (-1 = assente). `lb` e' la parte di score gia' nota (peso dei flip).
    // PRECONDIZIONE: la CRC del candidato e' gia' stata verificata dal
    // chiamante tramite la sindrome incrementale.
    inline void eval3(const Mask& d, int f1, int f2, int f3, long lb) {
        const long score = lb + weight(d, best_score_ - lb);
        if (score >= best_score_) return;
        const int N = c_.N, M = c_.M;
        for (int v = 0; v < N; ++v) tmp_[v] = hard_[v];
        tmp_[f1] ^= 1; tmp_[f2] ^= 1; tmp_[f3] ^= 1;
        for (int r = 0; r < M; ++r)
            tmp_[pivcol_[r]] = (uint8_t)(hard_[pivcol_[r]] ^ ((d[r >> 6] >> (r & 63)) & 1));
        assert(crc14_ok(tmp_.data()) && "sindrome CRC incrementale incoerente");
        if (!plausibile(tmp_.data())) return;
        best_score_ = score; found_ = true; best_ = tmp_;
    }

    inline void eval(const Mask& d, int f1, int f2, long lb) {
        const long score = lb + weight(d, best_score_ - lb);
        if (score >= best_score_) return;
        const int N = c_.N, M = c_.M;
        for (int v = 0; v < N; ++v) tmp_[v] = hard_[v];
        if (f1 >= 0) tmp_[f1] ^= 1;
        if (f2 >= 0) tmp_[f2] ^= 1;
        for (int r = 0; r < M; ++r)                    // par[r] = hard[pivcol[r]] ^ d[r]
            tmp_[pivcol_[r]] = (uint8_t)(hard_[pivcol_[r]] ^ ((d[r >> 6] >> (r & 63)) & 1));
        assert(crc14_ok(tmp_.data()) && "sindrome CRC incrementale incoerente");
        if (!plausibile(tmp_.data())) return;
        best_score_ = score; found_ = true; best_ = tmp_;
    }

    // Il candidato ha passato la CRC-14: descrive anche un messaggio possibile?
    //
    // La CRC lascia passare un candidato sbagliato ogni 16384, e con la ricerca
    // larga se ne provano ~21400 per parola: e' il motivo per cui allargarla
    // non pagava. Il controllo di struttura del messaggio aggiunge da 1,1 a 2,2
    // bit di filtro (misurati) e non scarta MAI un messaggio vero.
    //
    // Sta QUI dentro e non dopo: se un candidato falso passa la CRC ma non e'
    // un messaggio, l'enumerazione prosegue e puo' ancora trovare quello giusto.
    // Applicato dopo, si limiterebbe a buttare via la parola lasciando il buco.
    //
    // Costa quasi niente: lo vedono solo i candidati che hanno gia' passato la
    // CRC, cioe' uno su 16384.
    inline bool plausibile(const uint8_t* cw) const {
        return tipi_ammessi == 0 || plaus::message77_ok(cw, tipi_ammessi);
    }

    const Code& c_;
    int order_, span2_, span3_;
    long best_score_ = LONG_MAX;
    bool found_ = false;
    uint32_t gen_ = 0;
    std::vector<int> absL_;
    std::vector<uint8_t> hard_, best_, tmp_, ispiv_;
    std::vector<int> rbuf_;
    std::vector<int> perm_, pivcol_, info_;
    std::vector<Row> rows_, base_;
    std::vector<int> w_;
    std::vector<Mask> colmask_;
    std::vector<uint32_t> cm_gen_;
    // ricerca a coppie: chiave per bit, tabella a liste concatenate

    uint16_t g_[NCRC] = {};
    std::vector<uint16_t> gpiv_, dcrc_;
    // ricerca a coppie: chiave per bit, tabella a liste concatenate
    std::vector<uint32_t> pkey_;
    std::vector<int> tau_idx_;
    std::vector<int> box_head_, box_next_, box_i1_, box_i2_;
    std::vector<uint32_t> box_gen_;
    uint32_t box_generation_ = 0;
    Row acc_[14] = {};
};
