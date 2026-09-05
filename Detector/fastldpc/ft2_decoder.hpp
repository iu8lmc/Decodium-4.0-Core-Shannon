// ft2_decoder.hpp — l'unico header che serve includere per usare fastldpc.
//
// Incapsula la catena completa: min-sum SIMD (AVX2 o NEON) su un batch di
// candidati, OSD sui non convergenti, CRC-14 e gate sulla distanza soft. Gestisce da solo il
// riempimento del batch, quindi si puo' chiamare con un numero qualsiasi di
// parole.
//
//     Code code = Code::load("data/ldpc_174_91.h.txt");
//     Ft2Decoder dec(code, Ft2Decoder::sensibile());
//     int ok = dec.decode_batch(llr, n, bits, flags);   // llr = [n][174] float
//
// llr: LLR di canale, positivo = bit 0, nello stesso formato del demodulatore.
// bits: [n][174] con i bit decisi. flags[i] = 1 se la parola i e' accettata:
// solo quelle vanno passate al livello superiore. I 77 bit di messaggio sono
// bits[i*174 .. i*174+76].
//
// I tre preset sono tarati su 20 000 parole per punto fra 0,5 e 3 dB
// (vedi README). A parita' di false decodifiche `sensibile` vale circa +0,7 dB
// rispetto alla configurazione OSD-2/span32 da cui il progetto era partito.
#pragma once
#include "minsum_avx2.hpp"
#include "osd_fast.hpp"
#include "gate.hpp"
#include <algorithm>
#include <cstring>
#include <functional>
#include <vector>

struct Ft2Config {
    int   batch     = 64;       // parole per chiamata al min-sum, multiplo di 16
    int   max_iter  = 30;       // iterazioni min-sum
    int   osd_order = 3;        // -1 = nessun OSD, 0..3
    int   span2     = 91;       // bit d'informazione esplorati a coppie
    int   span3     = 48;       // ... e a terne
    float nd_max    = 0.075f;   // gate anti-false-decode; 1.0 lo disattiva
    // Strato 2 (FASTLDPC-AI-SPEC-001 §2): gate appreso al posto della sola
    // soglia su nd. 0 = solo nd_max (oggi, invariato). 1 = l'OSD accetta
    // candidati fino a nd <= gate_relax e la decisione finale la prende
    // gate_accept() sulle feature del candidato (gate.hpp).
    //
    // ATTENZIONE: gate_weights.hpp sono i pesi del pacchetto di ricerca
    // originale, addestrati PRIMA che il decoder passasse ad alpha 0,578,
    // ntau 13 e span2 64 (vedi il commento in cima a gate_weights.hpp). Il
    // meccanismo e' collaudato — a gate_mode=0 il comportamento resta
    // bit-identico — ma i pesi NON sono stati riaddestrati ne' rimisurati su
    // questo decoder. gate_mode=1 e' un banco di prova, non una soglia pronta.
    int   gate_mode  = 0;
    float gate_relax = 0.25f;
    // Raccolta dati per il riaddestramento (FASTLDPC-AI-SPEC-001 §2b): se
    // impostato, viene chiamato per OGNI candidato che chiude (min-sum o
    // OSD), gate_mode permettendo, con le feature appena calcolate e i 174
    // bit del candidato -- accettato dal gate o no. Il chiamante (banco di
    // prova con messaggio noto) confronta la parola con la verita' e scrive
    // la riga del dataset; a callback nullo (default, produzione) costo zero
    // e comportamento invariato. Vedi Detector/fastldpc/decodium_bridge.cpp.
    std::function<void(int, const GateFeatures&, const uint8_t*)> gate_dump_cb = nullptr;
    // Tipi di messaggio i3 ammessi dal controllo di plausibilita' dentro
    // l'OSD: 0 lo spegne. Vedi cpp/plausible.hpp.
    uint32_t tipi_ammessi = 0;
    // Bit d'informazione ammessi nelle coppie; 0 = tutti. Vedi OsdFast.
    int pair_span   = 0;
    // Limite sui |LLR| in ingresso, in multipli della media della parola.
    // 0 = disattivato. Un LLR molto piu' grande della media e' quasi sempre un
    // artefatto (interferenza impulsiva) e non informazione: un solo LLR
    // "sicuro e sbagliato" avvelena tutti i check che lo toccano. La soglia e'
    // relativa alla parola, quindi resta invariante di scala come nd.
    float llr_clip  = 2.5f;
    // Ricerca a coppie mirata (vedi OsdFast::pair_search): trova le coppie di
    // bit che azzerano i bit di parita' piu' affidabili, invece di provarle
    // tutte. E' il meccanismo che rende efficace l'OSD di WSJT-X a ordine 1.
    bool  pair_search = false;
    int   ntau        = 14;
    // Fattore di normalizzazione del min-sum, in 1/65536. 49152 = 3/4, la
    // costante classica; 37888 = 0,578 e' quella misurata su QUESTO codice.
    // Vedi lab/README.md: 3/4 e' tarato per far convergere il min-sum, mentre
    // qui il min-sum prepara i posteriori per l'OSD, che e' un altro mestiere.
    unsigned alpha_w  = 49152;
};

class Ft2Decoder {
public:
    // Magnitudine assegnata a un bit noto: il massimo che la quantizzazione
    // interna del min-sum rappresenta (LLR_MAX / LLR_FIX).
    static constexpr float kApMag = 2047.0f / 8.0f;

    // Solo min-sum: ~5 us/parola, nessuna falsa decodifica, meno sensibile.
    static Ft2Config veloce() {
        Ft2Config c; c.osd_order = -1; return c;
    }
    // OSD-2 su span 32: il compromesso classico, tipo WSJT-X.
    static Ft2Config conservativo() {
        Ft2Config c; c.osd_order = 2; c.span2 = 32; c.span3 = 0; c.nd_max = 0.085f; return c;
    }
    // OSD-3: piu' sensibile a parita' di false decodifiche, ~4x piu' lento del
    // conservativo. E' possibile solo perche' il test CRC di un candidato costa
    // uno XOR (vedi osd_fast.hpp) e perche' il gate tiene a freno le false.
    static Ft2Config sensibile() { return Ft2Config{}; }

    Ft2Decoder(const Code& code, const Ft2Config& cfg = Ft2Config{})
        : cfg_(cfg), N_(code.N), c_M_(code.M),
          ms_(code, cfg.batch, cfg.max_iter),
          osd_(code, cfg.osd_order, cfg.span2, cfg.span3),
          bits_((size_t)cfg.batch * code.N), ok_(cfg.batch), iters_(cfg.batch),
          buf_((size_t)cfg.batch * code.N), word_(code.N) {
        osd_.nd_max = cfg.nd_max;
        osd_.tipi_ammessi = cfg.tipi_ammessi;
        osd_.pair_span = cfg.pair_span;
        osd_.pair_search = cfg.pair_search;
        osd_.ntau = cfg.ntau;
        ms_.set_alpha(cfg.alpha_w);
    }

    // Statistiche cumulate dall'ultima reset_stats().
    struct Stats { long words = 0, by_bp = 0, by_osd = 0, osd_tried = 0, gate_rejected = 0; };
    const Stats& stats() const { return st_; }
    void reset_stats() { st_ = Stats{}; }

    // Candidati sottoposti alla CRC-14 (con -DOSD_COUNT; senza, resta zero).
    // Diviso per le parole tentate da' i candidati per parola, cioe' il numero
    // da cui dipendono i nominativi fantasma: la CRC ne ammette uno ogni 16384.
    // E' strutturale e deterministico, quindi dice quello che il conteggio dei
    // fantasmi su una piscina di rumore gaussiano non riesce a dire.
    long long crc_tests() const { return osd_.n_crc; }
    void reset_crc_tests() { osd_.n_crc = 0; }

    // apmask (opzionale): [n][174], 1 sui bit gia' noti per ipotesi a priori.
    // Gli LLR di quei bit devono gia' portare il valore noto, come fa FT2
    // (llr[i] = apmag * apbits[i]); qui vengono saturati al massimo
    // rappresentabile, cosi' il min-sum non li ribalta e l'OSD, che ordina per
    // affidabilita', li lascia in fondo al set d'informazione e non li flippa.
    //
    // Ritorna il numero di parole accettate. `nd`, se non nullo, riceve la
    // distanza soft normalizzata delle parole chiuse dall'OSD (0 per le altre).
    // Un'istanza per thread: lo stato interno (batch, matrice ridotta) e'
    // per-istanza e non c'e' niente di condiviso, quindi nessuna sincronizzazione.
    int decode_batch(const float* llr, int n, uint8_t* out, uint8_t* accepted,
                     float* nd = nullptr, const uint8_t* apmask = nullptr) {
        const int B = cfg_.batch, N = N_;
        int total = 0;
        std::memset(accepted, 0, (size_t)n);
        if (nd) std::memset(nd, 0, (size_t)n * sizeof(float));
        // Solo se il gate e' attivo: altrimenti il costo di calcolarle (un
        // ordinamento su N per candidato chiuso) sarebbe un cambio silenzioso
        // di prestazioni a flag spento, non solo di comportamento.
        if (cfg_.gate_mode) feat_.assign((size_t)n, GateFeatures{});

        for (int off = 0; off < n; off += B) {
            const int cnt = std::min(B, n - off);
            std::memcpy(buf_.data(), &llr[(size_t)off * N], (size_t)cnt * N * sizeof(float));
            // I bit noti per ipotesi a priori vanno bloccati PRIMA del clip,
            // altrimenti il clip stesso ne ridurrebbe la magnitudine.
            if (apmask) {
                for (int b = 0; b < cnt; ++b) {
                    const uint8_t* m = &apmask[(size_t)(off + b) * N];
                    float* w = &buf_[(size_t)b * N];
                    for (int v = 0; v < N; ++v)
                        if (m[v]) w[v] = w[v] >= 0 ? kApMag : -kApMag;
                }
            }
            if (cfg_.llr_clip > 0) {
                for (int b = 0; b < cnt; ++b) {
                    const uint8_t* m = apmask ? &apmask[(size_t)(off + b) * N] : nullptr;
                    float* w = &buf_[(size_t)b * N];
                    float s = 0; int cnt_free = 0;
                    for (int v = 0; v < N; ++v)
                        if (!m || !m[v]) { s += w[v] < 0 ? -w[v] : w[v]; ++cnt_free; }
                    if (cnt_free > 0) {
                        const float lim = cfg_.llr_clip * s / cnt_free;
                        for (int v = 0; v < N; ++v) {
                            if (m && m[v]) continue;                 // i bit AP restano al massimo
                            w[v] = w[v] > lim ? lim : (w[v] < -lim ? -lim : w[v]);
                        }
                    }
                }
            }
            // Le lane in eccesso ripetono la prima parola: il min-sum lavora
            // sempre a batch pieno e il costo per parola resta quello nominale.
            for (int b = cnt; b < B; ++b)
                std::memcpy(&buf_[(size_t)b * N], buf_.data(), (size_t)N * sizeof(float));

            ms_.decode(buf_.data(), bits_.data(), iters_.data(), ok_.data());

            for (int b = 0; b < cnt; ++b) {
                ++st_.words;
                const int i = off + b;
                uint8_t* dst = &out[(size_t)i * N];
                const uint8_t* dec = &bits_[(size_t)b * N];
                if (ok_[b] && crc14_ok(dec)) {          // chiusa dal min-sum
                    std::memcpy(dst, dec, (size_t)N);
                    accepted[i] = 1; ++st_.by_bp; ++total;
                    // Il gate vale solo per i candidati che l'OSD scava dal
                    // rumore: una parola chiusa dal min-sum ha gia' convergenza
                    // vera sul codice, non viene mai messa in discussione.
                    if (cfg_.gate_mode) {
                        const GateFeatures& g = fill_features(i, b, dec, 0.0f,
                                      apmask ? &apmask[(size_t)i * N] : nullptr, 0);
                        if (cfg_.gate_dump_cb) cfg_.gate_dump_cb(i, g, dec);
                    }
                    continue;
                }
                if (cfg_.osd_order < 0) continue;
                ++st_.osd_tried;
                // La soglia del gate si allarga quando ci sono bit noti: lo
                // spazio dei candidati compatibili si riduce di 2^K, quindi i
                // falsi positivi crollano, e allo stesso tempo l'AP fa
                // decodificare parole con piu' errori di canale, che hanno nd
                // piu' alto. Il fattore (N/N_liberi)^2 e' tarato su 0, 14, 29 e
                // 58 bit noti: predice il ginocchio della curva in tutti e
                // quattro i casi (vedi README).
                // Con il gate acceso l'OSD accetta fino a gate_relax (piu'
                // largo di nd_max): e' gate_accept() sulle feature, non piu'
                // la sola distanza soft, a decidere. Non costa candidati alla
                // CRC in piu': nd_max e' l'ultimo controllo dentro decode(),
                // dopo che la CRC ha gia' selezionato il candidato.
                const float base_nd = cfg_.gate_mode ? cfg_.gate_relax : cfg_.nd_max;
                if (apmask) {
                    int free_bits = 0;
                    const uint8_t* m = &apmask[(size_t)i * N];
                    for (int v = 0; v < N; ++v) free_bits += !m[v];
                    const float r = free_bits > 0 ? (float)N / (float)free_bits : 1.0f;
                    osd_.nd_max = base_nd * r * r;
                } else {
                    osd_.nd_max = base_nd;
                }
                float d = 0;
                if (osd_.decode(ms_.posterior(b), word_.data(), &buf_[(size_t)b * N], &d,
                                apmask ? &apmask[(size_t)i * N] : nullptr)) {
                    if (cfg_.gate_mode) {
                        const GateFeatures& g = fill_features(i, b, word_.data(), d,
                                                              apmask ? &apmask[(size_t)i * N] : nullptr, 1);
                        if (cfg_.gate_dump_cb) cfg_.gate_dump_cb(i, g, word_.data());
                        if (!gate_accept(g)) { ++st_.gate_rejected; continue; }
                    }
                    std::memcpy(dst, word_.data(), (size_t)N);
                    accepted[i] = 1; ++st_.by_osd; ++total;
                    if (nd) nd[i] = d;
                }
            }
        }
        return total;
    }

    // Alias breve, per comodita' nei banchi di prova.
    int decode(const float* llr, int n, uint8_t* out, uint8_t* acc,
               float* nd = nullptr, const uint8_t* apmask = nullptr) {
        return decode_batch(llr, n, out, acc, nd, apmask);
    }

    // Check non soddisfatti dall'ultima chiamata al min-sum, per la parola b del
    // batch corrente. Utile in diagnostica; misurato come predittore del
    // successo dell'OSD, NON funziona (vedi README).
    int unsat(int b) const { return ms_.unsat(b); }

    // Feature del gate per la parola i dell'ultima decode_batch(), valide sia
    // per le accettate sia per quelle scartate dal gate (gate_mode=1). Servono
    // alla raccolta dati sul traffico vero per il riaddestramento (vedi il
    // commento in cima a gate_weights.hpp). Vuote (has_candidate()==false) se
    // il gate era spento o la parola non ha chiuso ne' via min-sum ne' via OSD.
    const GateFeatures& gate_features(int i) const { return feat_[(size_t)i]; }
    bool has_candidate(int i) const { return cfg_.gate_mode && feat_[(size_t)i].f[4] > 0.0f; }

private:
    Ft2Config cfg_;
    int N_;
    int c_M_;
    MinSumV3 ms_;
    OsdFast  osd_;
    std::vector<uint8_t> bits_, ok_;
    std::vector<int> iters_;
    std::vector<float> buf_;
    std::vector<uint8_t> word_;
    std::vector<GateFeatures> feat_;
    Stats st_;

    // Feature del candidato per il gate appreso (strato 2). Costo: un
    // ordinamento su N per lo split "meta' piu' affidabile", pagato solo
    // quando gate_mode e' attivo e solo sui candidati che hanno gia' chiuso
    // (min-sum o OSD), mai sul rumore scartato prima.
    const GateFeatures& fill_features(int i, int b, const uint8_t* word, float d,
                                      const uint8_t* m, int by_osd) {
        const int N = N_;
        GateFeatures& g = feat_[(size_t)i];
        const float* w = &buf_[(size_t)b * N];
        const int16_t* L = ms_.posterior(b);
        double sum_abs = 0; float min_abs = 1e30f; int nfree = 0;
        for (int v = 0; v < N; ++v) {
            if (m && m[v]) continue;
            const float a = w[v] < 0 ? -w[v] : w[v];
            sum_abs += a; if (a < min_abs) min_abs = a; ++nfree;
        }
        const float mean_abs = nfree ? (float)(sum_abs / nfree) : 0.0f;
        // ordinamento per |LLR|: "top" = meta' piu' affidabile
        static thread_local std::vector<int> idx; idx.resize(N);
        for (int v = 0; v < N; ++v) idx[v] = v;
        std::sort(idx.begin(), idx.end(), [&](int a, int c) {
            const float x = w[a] < 0 ? -w[a] : w[a], y = w[c] < 0 ? -w[c] : w[c]; return x > y; });
        int nhard = 0, nhard_top = 0;
        for (int r = 0; r < N; ++r) {
            const int v = idx[r];
            if (m && m[v]) continue;
            const bool flip = word[v] != (uint8_t)(w[v] < 0);
            nhard += flip; if (r < N / 2) nhard_top += flip;
        }
        double sum16 = 0; for (int v = 0; v < N; ++v) sum16 += L[v] < 0 ? -L[v] : L[v];
        g.f[0] = d;
        g.f[1] = (float)nhard / N;
        g.f[2] = (float)nhard_top / N;
        g.f[3] = mean_abs > 0 ? min_abs / mean_abs : 0.0f;
        g.f[4] = mean_abs;
        g.f[5] = (float)iters_[b] / cfg_.max_iter;
        g.f[6] = (float)ms_.unsat(b) / c_M_;
        g.f[7] = (by_osd && sum16 > 0) ? (float)(osd_.last_score() / sum16) : 0.0f;
        g.f[8] = (float)nfree / N;
        g.f[9] = (float)by_osd;
        return g;
    }
};
