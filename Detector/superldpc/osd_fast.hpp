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
#include <cstring>
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
// Sempre attivo: un'addizione per candidato (16 per blocco AVX2). Serve al
// gate appreso (feature 10 e 11 in gate.hpp): il rango del candidato accettato
// e' la grandezza che predice i fantasmi. n_crc resta cumulativo per le
// statistiche, n_tested_ si azzera a ogni decode().
#define OSD_C(k) (n_crc += (k), n_tested_ += (k))

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

    // 77 bit di scrambling da togliere PRIMA del controllo di plausibilita'.
    // Serve a FT2, che mescola il payload con rvec prima di codificare e lo
    // rimette in chiaro solo dopo il decoder (FtxFt2Stage7.cpp): senza questo
    // il filtro legge un numero casuale al posto del messaggio, e finisce per
    // scartare parole VERE -- misurato, ~7% delle decodifiche FT2 e +24% alla
    // soglia. FT8 non mescola e lascia il puntatore nullo.
    const uint8_t* descramble77 = nullptr;

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

    // RICERCA SUL CODICE CONCATENATO (174,77).
    //
    // La CRC-14 e' lineare sui bit 0..90 (vedi decoder.hpp), quindi i suoi 14
    // vincoli sono 14 righe di parita' in piu': H diventa 97 x 174 e il codice
    // su cui si cerca e' quello che viene davvero trasmesso, LDPC + CRC, di
    // dimensione 77. Con `crc_in_base` l'eliminazione di Gauss lavora su quelle
    // 97 righe, la base piu' affidabile ha 77 posizioni d'informazione e OGNI
    // candidato soddisfa la CRC per costruzione: non c'e' piu' niente da
    // filtrare, ogni candidato enumerato e' una parola valida.
    //
    // Due conseguenze. La ricerca rende di piu' a parita' di ordine: i 14 bit
    // meno affidabili che prima erano posizioni d'informazione diventano
    // parita', e un errore li' non costa piu' un flip. E i candidati validi
    // per parola passano da ~uno a migliaia, quindi esiste sempre un SECONDO
    // migliore: la distanza fra i due (last_nd2() - nd) e' la misura del
    // margine che il gate a margine usa (gate.hpp, margin_accept). Senza
    // quel gate la ricerca larga non paga: a pari false decodifiche la sola
    // soglia su nd restituisce la sensibilita' di prima (README).
    //
    // Spento, decode() e' identica a prima, bit per bit.
    bool crc_in_base = false;

    // Solo con crc_in_base: i bit noti per ipotesi a priori (apmask) sono un
    // VINCOLO della ricerca. Un candidato che ne contraddice uno non viene
    // generato (bit d'informazione) o viene scartato (bit di parita'), invece
    // di essere scelto e poi buttato dal controllo di coerenza a valle
    // (decodium_bridge.cpp, superldpc_ap_check). Il valore atteso di un bit
    // noto e' il segno del suo LLR di canale, come in quel controllo.
    bool ap_vincolo = false;

    static constexpr int NCRCB = 14;    // righe aggiunte da crc_in_base

    OsdFast(const Code& code, int order = 1, int span2 = 32, int span3 = 0)
        : c_(code), order_(order), span2_(span2), span3_(span3 > 0 ? span3 : span2 / 2),
          absL_(code.N), hard_(code.N), best_(code.N), tmp_(code.N), ispiv_(code.N),
          rbuf_(code.N), perm_(code.N), pivcol_(code.M + NCRCB),
          rows_(code.M + NCRCB), base_(code.M), w_(code.M + NCRCB),
          colmask_(code.N), cm_gen_(code.N, 0), gpiv_(code.M), dcrc_(code.N),
          pkey_(code.N), tau_idx_(code.M + NCRCB), mrows_(code.M),
          alt_{std::vector<uint8_t>(code.N), std::vector<uint8_t>(code.N)},
          apinfo_(code.N), base77_(code.M + NCRCB) {
        if (code.M + NCRCB > 64 * MWMAX) throw std::runtime_error("OsdFast: M troppo grande");
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
        // Le 14 righe della CRC, ricavate dalla stessa crc14_syn: la riga j
        // tocca i bit v il cui contributo alla sindrome ha il bit j acceso.
        // Una parola ha sindrome nulla se e solo se soddisfa tutte e 14.
        for (int m = 0; m < code.M; ++m) base77_[m] = base_[m];
        for (int j = 0; j < NCRCB; ++j) {
            base77_[code.M + j] = Row{};
            for (int v = 0; v < NCRC; ++v) if ((g_[v] >> j) & 1) setb(base77_[code.M + j], v);
        }
    }

    // Le 14 righe di parita' della CRC-14 come insiemi di bit, per i test.
    bool crc_row_bit(int j, int v) const {
        return (base77_[(size_t)c_.M + j].w[v >> 6] >> (v & 63)) & 1ull;
    }

    bool decode(const int16_t* L, uint8_t* out, const float* llr_ch = nullptr,
                float* nd = nullptr, const uint8_t* apmask = nullptr) {
        have2_ = false; last_nd2_ = 1.0f;
        if (crc_in_base) return decode77(L, out, llr_ch, nd, apmask);
        OSD_TSTART();
        const int N = c_.N, M = c_.M;
        mrows_ = M;
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
        n_tested_ = 0; n_pass_ = 0; rank_at_best_ = 0; pass_at_best_ = 0;
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
    // Rango del candidato accettato: quanti candidati erano stati sottoposti
    // alla CRC-14 quando e' stato trovato, e quanti l'avevano gia' passata.
    long long last_rank() const { return rank_at_best_; }
    long long last_pass() const { return pass_at_best_; }
    // Candidati valutati dall'ultima decode(), in tutto.
    long long last_tested() const { return n_tested_; }
    // Solo con crc_in_base: distanza soft normalizzata del CONCORRENTE del
    // candidato restituito, cioe' della migliore fra le altre parole provate
    // (vedi eval77), con la stessa definizione di nd (LLR di canale, bit AP
    // esclusi). Puo' essere PIU' PICCOLA di nd: succede quando la parola piu'
    // vicina non descrive un messaggio possibile e quella restituita e' solo
    // la migliore fra le plausibili. Vale 1, e last_has_second() e' falso, se
    // un concorrente non esiste (tutti i bit d'informazione noti) o se a
    // decode() non sono stati passati gli LLR di canale.
    float last_nd2() const { return last_nd2_; }
    bool  last_has_second() const { return have2_; }

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
    long long n_tested_ = 0, n_pass_ = 0, rank_at_best_ = 0, pass_at_best_ = 0;

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
            // Senza salto: il bit e' 0 o 1 con la stessa probabilita' e un
            // `if` qui sbaglia predizione una volta su due, per ogni riga.
            const int v = info_[t], wd = v >> 6, sh = v & 63;
            for (int r = 0; r < mrows_; ++r) cm[r >> 6] |= ((rows_[r].w[wd] >> sh) & 1ull) << (r & 63);
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
        ++n_pass_;
        if (!plausibile(tmp_.data())) return;
        best_score_ = score; found_ = true; best_ = tmp_;
        rank_at_best_ = n_tested_; pass_at_best_ = n_pass_;
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
        ++n_pass_;
        if (!plausibile(tmp_.data())) return;
        best_score_ = score; found_ = true; best_ = tmp_;
        rank_at_best_ = n_tested_; pass_at_best_ = n_pass_;
    }

    // ------------------------------------------------ ricerca su (174,77)
    // Stessa struttura di decode(): ordinamento, Gauss, enumerazione per
    // ordine crescente e ricerca a coppie mirata. Cambiano tre cose: le righe
    // sono 97 (H piu' le 14 della CRC), non c'e' test di CRC, e oltre al
    // candidato da restituire si tiene il suo concorrente (vedi eval77). Il
    // taglio per limite inferiore si fa quindi su bound_, non sul migliore.
    bool decode77(const int16_t* L, uint8_t* out, const float* llr_ch,
                  float* nd, const uint8_t* apmask) {
        OSD_TSTART();
        const int N = c_.N, M = c_.M + NCRCB;
        mrows_ = M;
        const bool vincolo = ap_vincolo && apmask && llr_ch;
        for (int v = 0; v < N; ++v) {
            const int a = L[v] < 0 ? -L[v] : L[v];
            absL_[v] = a; hard_[v] = (uint8_t)(L[v] < 0);
        }
        // I bit noti entrano con il loro valore e la massima affidabilita',
        // qualunque cosa ne abbia fatto il min-sum: finiscono in fondo
        // all'ordinamento, quindi nel set d'informazione finche' sono
        // indipendenti, e li' non vengono mai flippati.
        if (vincolo)
            for (int v = 0; v < N; ++v)
                if (apmask[v]) { hard_[v] = (uint8_t)(llr_ch[v] < 0); absL_[v] = kAbsMax; }
        radix_sort_by_absL(N);

        OSD_T(t_sort);
        std::copy(base77_.begin(), base77_.end(), rows_.begin());
        int nused = 0;
        for (int k = 0; k < N && nused < M; ++k) {
            const int col = perm_[k], wd = col >> 6, sh = col & 63;
            const uint64_t bit = 1ull << sh;
            int pr = -1;
            for (int m = nused; m < M; ++m) if (rows_[m].w[wd] & bit) { pr = m; break; }
            if (pr < 0) continue;
            if (pr != nused) std::swap(rows_[pr], rows_[nused]);
            const Row pv = rows_[nused];
            eliminate(0, nused, pv, wd, sh);
            eliminate(nused + 1, M, pv, wd, sh);
            pivcol_[nused] = col; ++nused;
        }
        if (nused < M) return false;                    // non accade: il rango e' 97

        OSD_T(t_gauss);
        std::fill(ispiv_.begin(), ispiv_.end(), (uint8_t)0);
        for (int r = 0; r < M; ++r) ispiv_[pivcol_[r]] = 1;
        info_.clear();
        for (int k = N - 1; k >= 0; --k) if (!ispiv_[perm_[k]]) info_.push_back(perm_[k]);
        std::reverse(info_.begin(), info_.end());       // info_[0] = meno affidabile
        const int K = (int)info_.size();                // 77

        ++gen_;
        for (int r = 0; r < M; ++r) w_[r] = absL_[pivcol_[r]];
        Row hx{};
        for (int t = 0; t < K; ++t) if (hard_[info_[t]]) setb(hx, info_[t]);
        Mask d0{};
        for (int r = 0; r < M; ++r) {
            uint64_t x = 0;
            for (int w = 0; w < RW; ++w) x ^= rows_[r].w[w] & hx.w[w];
            if ((fl_popcount64(x) & 1) ^ (int)hard_[pivcol_[r]])
                d0[r >> 6] |= 1ull << (r & 63);
        }
        // Vincolo AP: un bit noto finito fra i pivot non puo' essere smentito
        // (apm_), uno nel set d'informazione non si flippa (apinfo_).
        build_lut(M);
        apm_ = Mask{};
        for (int t = 0; t < K; ++t) apinfo_[t] = 0;
        if (vincolo) {
            for (int r = 0; r < M; ++r) if (apmask[pivcol_[r]]) apm_[r >> 6] |= 1ull << (r & 63);
            for (int t = 0; t < K; ++t) apinfo_[t] = apmask[info_[t]] ? 1 : 0;
        }

        OSD_T(t_pre);
        best_score_ = LONG_MAX; bound_ = LONG_MAX; found_ = false;
        alt_score_[0] = alt_score_[1] = LONG_MAX;
        n_tested_ = 0; n_pass_ = 0; rank_at_best_ = 0; pass_at_best_ = 0;
        eval77(d0, -1, -1, -1, 0);                                      // OSD-0
        if (order_ >= 1) {
            const int lim2 = std::min(span2_, K), lim3 = std::min(span3_, K);
            for (int t = 0; t < K; ++t) {
                const long a_t = absL_[info_[t]];
                if (a_t >= bound_) break;           // info_ ordinato: il resto e' peggio
                if (apinfo_[t]) continue;
                Mask d1 = d0; xorm(d1, colmask(t));
                eval77(d1, t, -1, -1, a_t);
                if (order_ >= 2 && t < lim2) {
                    for (int u = t + 1; u < lim2; ++u) {
                        const long lb = a_t + absL_[info_[u]];
                        if (lb >= bound_) break;
                        if (apinfo_[u]) continue;
                        Mask d2 = d1; xorm(d2, colmask(u));
                        eval77(d2, t, u, -1, lb);
                    }
                }
                if (order_ >= 3 && t < lim3) {
                    for (int u = t + 1; u < lim3; ++u) {
                        const long lb2 = a_t + absL_[info_[u]];
                        if (lb2 >= bound_) break;
                        if (apinfo_[u]) continue;
                        Mask d2 = d1; xorm(d2, colmask(u));
                        for (int x = u + 1; x < lim3; ++x) {
                            const long lb3 = lb2 + absL_[info_[x]];
                            if (lb3 >= bound_) break;
                            if (apinfo_[x]) continue;
                            Mask d3 = d2; xorm(d3, colmask(x));
                            eval77(d3, t, u, x, lb3);
                        }
                    }
                }
            }
        }
        OSD_T(t_enum);
        // Ricerca a coppie mirata, come in decode(). Ritrova anche candidati
        // gia' visti dall'enumerazione: eval77 li riconosce dai flip.
        if (pair_search && K > 2) {
            const int tau = std::min (ntau, std::min (M, 20));
            pick_tau (M, tau);
            const int Kp = (pair_span > 0 && pair_span < K) ? pair_span : K;
            build_pairs (Kp, tau);
            const int nstart = std::min (K, span2_ > 0 ? span2_ : K);
            for (int st = -1; st < nstart; ++st) {
                Mask dbase = d0;
                long lb0 = 0;
                if (st >= 0) {
                    lb0 = absL_[info_[st]];
                    if (lb0 >= bound_) break;
                    if (apinfo_[st]) continue;
                    xorm (dbase, colmask (st));
                }
                const uint32_t want = key_of (dbase, tau);
                if (box_gen_[want] != box_generation_) continue;
                for (int idx = box_head_[want]; idx >= 0; idx = box_next_[(size_t) idx]) {
                    const int i1 = box_i1_[(size_t) idx], i2 = box_i2_[(size_t) idx];
                    if (i1 == st || i2 == st) continue;
                    const long lb = lb0 + absL_[info_[i1]] + absL_[info_[i2]];
                    if (lb >= bound_) continue;
                    if (apinfo_[i1] || apinfo_[i2]) continue;
                    Mask d = dbase;
                    xorm (d, colmask (i1));
                    xorm (d, colmask (i2));
                    eval77 (d, st, i1, i2, lb);
                }
            }
        }

        OSD_T(t_crc);                                   // qui: ricerca a coppie
        if (!found_) return false;
        // Il concorrente: il migliore in assoluto, o il secondo se il migliore
        // e' proprio il candidato restituito.
        const bool primo = altf_[0] == bestf_[0] && altf_[1] == bestf_[1] && altf_[2] == bestf_[2];
        const int conc = primo ? 1 : 0;
        have2_ = alt_score_[conc] != LONG_MAX;
        const std::vector<uint8_t>& second = alt_[conc];

        if (llr_ch) {
            double num = 0, num2 = 0, den = 0;
            for (int v = 0; v < N; ++v) {
                if (apmask && apmask[v]) continue;
                const double a = llr_ch[v] < 0 ? -llr_ch[v] : llr_ch[v];
                den += a;
                const uint8_t h = (uint8_t)(llr_ch[v] < 0);
                if (best_[v] != h) num += a;
                if (have2_ && second[v] != h) num2 += a;
            }
            const float d = den > 0 ? (float)(num / den) : 1.0f;
            if (have2_ && den > 0) last_nd2_ = (float)(num2 / den);
            if (nd) *nd = d;
            if (d > nd_max) return false;               // gate anti-false-decode
        }
        for (int v = 0; v < N; ++v) out[v] = best_[v];
        return true;
    }

    // Score di un pattern di parita' con una tabella per byte: lut_[b][x] e'
    // la somma dei pesi dei pivot 8b..8b+7 selezionati dai bit di x. Con
    // crc_in_base lo score si calcola per OGNI candidato e non per uno su
    // 16384, e i pivot sono le 97 posizioni MENO affidabili: i loro pesi sono
    // piccoli, quindi la somma interrotta di weight() non si interrompe quasi
    // mai e costa ~50 termini a candidato. Cosi' sono 13 letture senza salti.
    void build_lut(int M) {
        const int nb = (M + 7) >> 3;
        for (int b = 0; b < nb; ++b) {
            int32_t* t = lut_[b];
            t[0] = 0;
            for (int x = 1; x < 256; ++x) {
                const int r = (b << 3) + fl_ctz64((uint64_t)x);
                t[x] = t[x & (x - 1)] + (r < M ? w_[r] : 0);
            }
        }
        for (int b = nb; b < 8 * MWMAX; ++b) std::memset(lut_[b], 0, sizeof lut_[b]);
    }

    inline long weight_lut(const Mask& d) const {
        long s = 0;
        for (int w = 0; w < MWMAX; ++w) {
            const uint64_t x = d[w];
            const int32_t (*t)[256] = &lut_[w << 3];
            s += t[0][x & 255] + t[1][(x >> 8) & 255] + t[2][(x >> 16) & 255] + t[3][(x >> 24) & 255]
               + t[4][(x >> 32) & 255] + t[5][(x >> 40) & 255] + t[6][(x >> 48) & 255] + t[7][x >> 56];
        }
        return s;
    }

    // Valuta il candidato con i bit d'informazione di INDICE t1, t2, t3 in
    // info_ flippati (-1 = assente) e pattern di parita' d. Tiene tre cose:
    // il miglior candidato che supera il controllo di plausibilita' (best_,
    // quello che decode() restituisce) e i due migliori in assoluto, plausibili
    // o no (alt_[0], alt_[1]). Il CONCORRENTE del candidato restituito e' il
    // migliore fra gli altri: alt_[0], oppure alt_[1] se alt_[0] e' lui stesso.
    // Un concorrente non plausibile conta lo stesso: non puo' essere il
    // messaggio trasmesso, ma dice che il rumore ha portato una parola
    // qualsiasi a quella distanza, e sul solo rumore la parola piu' vicina e'
    // non plausibile tre volte su quattro.
    inline void eval77(const Mask& d, int t1, int t2, int t3, long lb) {
        OSD_C(1);
        if ((d[0] & apm_[0]) | (d[1] & apm_[1])) return;        // smentisce un bit noto
        const long score = lb + weight_lut(d);
        if (score >= bound_) return;
        // La stessa terna di flip puo' arrivare per strade diverse
        // (enumerazione e ricerca a coppie): ordinata, identifica il candidato.
        if (t1 < t2) std::swap(t1, t2);
        if (t2 < t3) std::swap(t2, t3);
        if (t1 < t2) std::swap(t1, t2);
        if (alt_score_[0] != LONG_MAX && t1 == altf_[0] && t2 == altf_[1] && t3 == altf_[2]) return;
        const int N = c_.N, M = mrows_;
        for (int v = 0; v < N; ++v) tmp_[v] = hard_[v];
        if (t1 >= 0) tmp_[info_[t1]] ^= 1;
        if (t2 >= 0) tmp_[info_[t2]] ^= 1;
        if (t3 >= 0) tmp_[info_[t3]] ^= 1;
        for (int r = 0; r < M; ++r)
            tmp_[pivcol_[r]] = (uint8_t)(hard_[pivcol_[r]] ^ ((d[r >> 6] >> (r & 63)) & 1));
        assert(crc14_ok(tmp_.data()) && "candidato (174,77) che non passa la CRC");
        ++n_pass_;
        if (score < best_score_ && plausibile(tmp_.data())) {
            best_score_ = score; found_ = true; best_ = tmp_;
            bestf_[0] = t1; bestf_[1] = t2; bestf_[2] = t3;
            rank_at_best_ = n_tested_; pass_at_best_ = n_pass_;
        }
        if (score < alt_score_[0]) {
            alt_score_[1] = alt_score_[0]; alt_[1].swap(alt_[0]);
            alt_score_[0] = score; alt_[0] = tmp_;
            altf_[0] = t1; altf_[1] = t2; altf_[2] = t3;
        } else if (score < alt_score_[1]) {
            alt_score_[1] = score; alt_[1] = tmp_;
        }
        bound_ = std::max(alt_score_[1], best_score_);
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
        if (tipi_ammessi == 0) return true;
        if (!descramble77) return plaus::message77_ok(cw, tipi_ammessi);
        // Costa 77 XOR su un candidato ogni 16384 (solo quelli che hanno gia'
        // passato la CRC), quindi non si vede nel tempo di decodifica.
        uint8_t chiaro[77];
        for (int i = 0; i < 77; ++i) chiaro[i] = (uint8_t)((cw[i] ^ descramble77[i]) & 1);
        return plaus::message77_ok(chiaro, tipi_ammessi);
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
    // ricerca su (174,77): vedi crc_in_base
    static constexpr int kAbsMax = 2047;            // MinSumV3::LLR_MAX
    int mrows_;                                     // righe della base in uso: M oppure M+14
    long bound_ = LONG_MAX;                         // taglio: max(alt_score_[1], best_score_)
    bool have2_ = false;
    float last_nd2_ = 1.0f;
    int bestf_[3] = {-1, -1, -1};
    long alt_score_[2] = {LONG_MAX, LONG_MAX};
    int altf_[3] = {-1, -1, -1};
    std::vector<uint8_t> alt_[2];
    Mask apm_{};
    std::vector<uint8_t> apinfo_;
    std::vector<Row> base77_;
    int32_t lut_[8 * MWMAX][256] = {};
};
