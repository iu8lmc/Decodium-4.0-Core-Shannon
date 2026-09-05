// decodium_bridge.cpp — backend SIMD del decoder fastldpc.
//
// Questo file e' intenzionalmente una translation unit separata, compilata
// con AVX2/FMA sui target x86-64 oppure con NEON sui target ARM64. Le API
// pubbliche, il rilevamento CPU e il fallback generico vivono in
// decodium_dispatch.cpp, che chiama questo backend soltanto dopo aver
// verificato tutte le capacita' richieste dall'architettura in uso.
//
// In Detector/FtxFt2Stage7.cpp:
//     ftx_decode174_91_c (llr.data (), 91, maxosd, 3, apmask.data (), ...);
// diventa
//     fastldpc_decode174_91_c (llr.data (), 91, maxosd, 3, apmask.data (), ...);
//
// Aggiungere questo file e i cpp/*.hpp di fastldpc alla build. FtxLdpc.cpp
// resta dov'e': serve ancora per encode174_91_nocrc_, le tabelle e le CRC.
//
// COSE DA SAPERE:
//
//  * Convenzione dei segni. Decodium usa LLR positivo = bit 1, fastldpc =
//    bit 0. La conversione la fa questo file, il chiamante non cambia.
//
//  * maxosd e norder sono accettati per compatibilita' di firma ma non hanno
//    lo stesso significato: fastldpc non lavora sugli snapshot del BP, l'OSD
//    parte sempre dai posterior finali. maxosd < 0 disattiva l'OSD (come in
//    Decodium), norder sceglie il preset (<=2 conservativo, >=3 sensibile).
//
//  * Keff diverso da 91 non e' supportato: si ricade sul decoder originale.
//
//  * UNA PAROLA PER CHIAMATA E' LO SPRECO PRINCIPALE. Il min-sum lavora su 16
//    parole per registro AVX2 oppure 8 per registro NEON; con una sola parola
//    utile quasi tutte le corsie restano vuote. Se il chiamante puo' raccogliere i candidati e decodificarli
//    insieme (Ft2Decoder::decode_batch), il costo per parola cala di circa un
//    ordine di grandezza. Con kFt2MaxCand = 300 candidati per passata, e' la
//    differenza fra ~100 us e ~14 us a candidato.
//
//  * Un'istanza per thread (thread_local): FT2DecodeWorker decodifica in
//    parallelo e il decoder ha stato interno.
#include "ft2_decoder.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

extern "C" void ftx_ldpc174_91_tables_c (int* Mn_out, int* Nm_out, int* nrw_out, int* ncw_out);

namespace {

constexpr int kN = 174;

// La matrice di parita' viene presa dalle tabelle che Decodium ha gia'
// compilate dentro (ftx_ldpc174_91_tables_c), non da un file: niente
// dipendenze esterne a runtime, e la certezza di decodificare con la stessa H
// del resto del programma. Costruita una volta per processo.
const Code& shared_code () {
    static Code code = [] {
        std::vector<int> mn (3 * kN), nm (7 * 83), nrw (83);
        int ncw = 0;
        ftx_ldpc174_91_tables_c (mn.data (), nm.data (), nrw.data (), &ncw);
        Code c;
        c.M = 83; c.N = kN;
        c.row_ptr.push_back (0);
        for (int m = 0; m < c.M; ++m) {
            for (int r = 0; r < nrw[(size_t) m]; ++r)
                c.col_idx.push_back (nm[(size_t) (r + 7 * m)] - 1);   // le tabelle sono 1-based
            c.row_ptr.push_back ((int) c.col_idx.size ());
        }
        return c;
    }();
    return code;
}

// ATTENZIONE al significato di "norder": nel decoder originale NON e' l'ordine
// dell'OSD, e' un INDICE di preset. La tabella in FtxLdpc.cpp (righe 619-649)
// per ndeep=3 -- il valore con cui FT2 chiama -- sceglie nord=1, cioe' l'ordine
// UNO con i due passi di preprocessing, non l'ordine tre.
//
// Leggendolo come ordine si passava a osd_order=3 su span2=91 e span3=48, cioe'
// 4095 coppie + 17296 terne = ~21400 candidati per chiamata invece di qualche
// centinaio. L'unico filtro sui candidati e' il CRC-14, che ne lascia passare
// uno ogni 16384: ~1,3 false decodifiche per chiamata, moltiplicate per le
// centinaia di candidati di un ciclo FT2. Sono le decine di nominativi fantasma
// osservate in FT2 il 27/08/2026, sparite spegnendo fastldpc.
//
// Qui la corrispondenza segue la tabella originale.
// FT8 e FT4 ammettono tipi di messaggio che FT2 non usa: i formati da contest
// i3 = 2, 3 e 5. Con la maschera di FT2 (kSoloUsati) quei messaggi non
// verrebbero decodificati MAI, quindi la modalita' FT8 tiene tutti i tipi
// definiti. Costa meta' del potere filtrante del controllo di plausibilita',
// ed e' il prezzo giusto: un filtro non deve rendere cieco il decoder.
static thread_local bool g_modo_ft8 = false;

extern "C" void fastldpc_simd_set_ft8_mode_c (int on) { g_modo_ft8 = on != 0; }

// Manopole per misurare in FT8 quanto costa ciascun filtro tarato su FT2.
// Si applicano a TUTTI i preset e SOLO in modalita' FT8: la taratura FT2
// resta quella che tiene i nominativi fantasma a zero, qualunque cosa si
// misuri qui. Senza variabili impostate non cambia nulla.
void manopole_ft8 (Ft2Config& c) {
    if (!g_modo_ft8) return;
    if (char const* e = std::getenv ("DECODIUM_LDPC_OSD_ORDER")) {
        int const n = std::atoi (e);
        if (n >= -1 && n <= 3) c.osd_order = n;
    }
    if (char const* e = std::getenv ("DECODIUM_LDPC_SPAN2")) {
        int const n = std::atoi (e);
        if (n > 0 && n <= 91) c.span2 = n;
    }
    if (char const* e = std::getenv ("DECODIUM_LDPC_SPAN3")) {
        int const n = std::atoi (e);
        if (n >= 0 && n <= 91) c.span3 = n;
    }
    if (char const* e = std::getenv ("DECODIUM_LDPC_ND_MAX")) {
        float const v = static_cast<float> (std::atof (e));
        if (v > 0.0f && v <= 1.0f) c.nd_max = v;
    }
    if (char const* e = std::getenv ("DECODIUM_LDPC_LLR_CLIP")) {
        float const v = static_cast<float> (std::atof (e));
        if (v >= 0.0f) c.llr_clip = v;      // 0 = nessun taglio
    }
    if (char const* e = std::getenv ("DECODIUM_LDPC_PAIR_SEARCH")) {
        c.pair_search = (e[0] != '0');
    }
}

// Fattore di normalizzazione del min-sum, in unita' di 1/65536.
//
// 49152 = 3/4 e' la costante di Chen-Fossorier (2002), quella che usano tutti.
// Su QUESTO codice non era mai stata verificata, e non e' il valore giusto: il
// min-sum qui non e' un decoder ma il preprocessore soft dell'OSD, che usa i
// suoi posteriori per ordinare il set d'informazione. 3/4 e' tarato per far
// CONVERGERE il min-sum, che e' un altro mestiere -- e infatti alzandolo il
// min-sum chiude piu' parole e il sistema ne decodifica di MENO.
//
// 37888 = 0,578, misurato sul preset di esercizio e validato su dati
// indipendenti (seme diverso, tre SNR, entrambi i filtri). Confronto a parita'
// di nominativi fantasma, su 2 000 000 di candidati di rumore per cella:
//
//   fantasmi     3/4    0,578
//         38   11446    11417    -0,3%
//        124   12194    12460    +2,2%
//        315   12757    13146    +3,0%
//       1478   13342    13866    +3,9%
//
// Al punto di lavoro (nd_max 0,065, cioe' ~84 fantasmi) il guadagno in
// decodifiche e' +1,4% con incertezza +-0,8%: marginale, e non e' la ragione
// per cui si adotta. Le ragioni sono le altre due, solide e indipendenti dalla
// soglia: -16% di tempo e -60% di candidati sottoposti alla CRC-14 -- cioe'
// meno esposizione ai falsi positivi, non piu'. Costa un'istruzione IN MENO
// (mulhi al posto di mullo+srai).
//
// Con DECODIUM_LDPC_ALPHA si torna a 49152 senza ricompilare, per il confronto
// appaiato in aria. Vale in entrambi i modi, come ntau e pair_span.
// Iterazioni del min-sum. Il valore storico era 30, ma ne bastano 10: da 30 a 6
// le decodifiche sono IDENTICHE (-0,1%, dentro il rumore) e si risparmia il 7%
// del tempo. Misurato su tre SNR e 150 000 candidati di rumore
// (lab/cpp/manopole_mai_toccate.cpp):
//
//   max_iter   v0.0    v0.5    v1.0    us/cand
//     30       8262   12649   16630     58,1
//     10       8245   12636   16623     55,1   -5%
//      6       8239   12633   16614     53,9   -7%
//
// E' la terza conferma indipendente, dopo alpha e la tabella dei pesi, che la
// CONVERGENZA del min-sum non e' l'obiettivo: qui il min-sum prepara i
// posteriori per l'OSD, e ventiquattro iterazioni su trenta sono lavoro
// buttato.
//
// Si tiene 10 e non 6 perche' il margine costa poco (2% di tempo) e su segnali
// veri, che hanno interferenza e derive che il rumore gaussiano non ha, le
// iterazioni in piu' potrebbero contare piu' che al banco. La misura dice che a
// 6 non si perde nulla su AWGN, non che non si perda nulla in aria.
//
// DECODIUM_LDPC_MAX_ITER riporta al valore che si vuole, 30 compreso.
int max_iter_scelto ()
{
    static int const v = [] {
        int n = 10;
        if (char const* e = std::getenv ("DECODIUM_LDPC_MAX_ITER")) {
            int const k = std::atoi (e);
            if (k >= 1 && k <= 100) n = k;
        }
        return n;
    }();
    return v;
}

unsigned alpha_scelto () {
    static unsigned const v = [] {
        unsigned w = 37888;
        if (char const* e = std::getenv ("DECODIUM_LDPC_ALPHA")) {
            int const n = std::atoi (e);
            if (n >= 16384 && n <= 65535) w = (unsigned) n;
        }
        return w;
    }();
    return v;
}

// Strato 2 (FASTLDPC-AI-SPEC-001 §2): gate appreso al posto della sola soglia
// su nd. Spento di default: DECODIUM_LDPC_GATE=1 lo accende.
//
// ATTENZIONE: i pesi (gate_weights.hpp) vengono dal pacchetto di ricerca
// originale e non sono stati riaddestrati ne' rimisurati su QUESTO decoder
// (alpha, ntau, span2 sono cambiati da allora, vedi il commento in cima al
// file dei pesi). Il meccanismo e' collaudato -- a flag spento resta
// bit-identico -- ma accendere DECODIUM_LDPC_GATE=1 e' un banco di prova, non
// una soglia pronta all'uso.
bool gate_mode_scelto () {
    static bool const v = [] {
        char const* e = std::getenv ("DECODIUM_LDPC_GATE");
        return e && e[0] != '0' && e[0] != 0;
    }();
    return v;
}

// DECODIUM_LDPC_GATE_RELAX: quanto l'OSD si allarga oltre nd_max quando il
// gate e' acceso, prima che gate_accept() decida. 0,25 e' il valore misurato
// nel pacchetto originale (gate/README.md); piu' alto recupera piu' candidati
// veri ma ne affida di piu' al classificatore invece che alla sola distanza.
float gate_relax_scelto () {
    static float const v = [] {
        float f = 0.25f;
        if (char const* e = std::getenv ("DECODIUM_LDPC_GATE_RELAX")) {
            float const n = static_cast<float> (std::atof (e));
            if (n > 0.0f && n <= 1.0f) f = n;
        }
        return f;
    }();
    return v;
}

// Raccolta LLR reali per il riaddestramento del gate (FASTLDPC-AI-SPEC-001
// §2b, vedi il commento su gate_weights.hpp e Detector/fastldpc/lab/neural/gate/).
// Il pacchetto di ricerca originale genera il dataset su un canale AWGN
// sintetico (train/ft2chan.py); qui invece si raccolgono le feature VERE che
// il decoder calcola su un WAV con contenuto NOTO, passato per la stessa
// catena (sync, demod, LLR) del traffico reale. In produzione nessuno chiama
// i quattro setter sotto: g_gate_dump_file resta nullo, gate_dump_cb non
// viene mai impostato in Ft2Config, zero costo e zero cambio di comportamento.
//
// g_gate_truth_cw174: 174 bit (dominio scrambled+LDPC) del messaggio che il
// banco di prova si aspetta in QUESTO momento -- thread_local perche' il
// decoder e' per-thread e il banco di prova gira su un solo thread, ma cosi'
// non si rischia di condividere stato fra thread se mai lo si chiamasse da
// piu' d'uno.
thread_local std::vector<uint8_t> g_gate_truth_cw174;

std::mutex& gate_dump_mutex () {
    static std::mutex m;
    return m;
}

std::FILE*& gate_dump_file () {
    static std::FILE* f = nullptr;
    return f;
}

// Riga nello stesso formato di gate/make_dataset.sh e letto da train_gate.py:
// f0..f9 label(1=vero) acc(1=il gate compilato oggi accetterebbe). L'ultima
// colonna non serve al training, solo a confrontare a occhio il gate vecchio
// con le etichette vere sullo stesso file.
void gate_dump_write (const GateFeatures& g, bool label) {
    std::FILE* f = gate_dump_file ();
    if (!f) return;
    std::lock_guard<std::mutex> lock (gate_dump_mutex ());
    std::fprintf (f, "%.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %d %d\n",
                 (double) g.f[0], (double) g.f[1], (double) g.f[2], (double) g.f[3],
                 (double) g.f[4], (double) g.f[5], (double) g.f[6], (double) g.f[7],
                 (double) g.f[8], (double) g.f[9], label ? 1 : 0, gate_accept (g) ? 1 : 0);
    std::fflush (f);
}

void gate_dump_callback (int /*i*/, const GateFeatures& g, const uint8_t* word) {
    if (g_gate_truth_cw174.empty ()) return;   // nessuna verita' nota: riga non etichettabile, si scarta
    bool label = true;
    for (int v = 0; v < kN && label; ++v)
        if (word[v] != g_gate_truth_cw174[(size_t) v]) label = false;
    gate_dump_write (g, label);
}

Ft2Decoder& decoder_for_preset (int ndeep) {
    // Anche i decoder per thread sono perdite volute, per lo stesso motivo:
    // il distruttore di Ft2Decoder libererebbe memoria da dentro
    // LdrShutdownThread.
    static thread_local Ft2Decoder* ord1 = nullptr, *ord2 = nullptr, *ord3 = nullptr;
    static thread_local Ft2Decoder* ord1_ft8 = nullptr, *ord2_ft8 = nullptr, *ord3_ft8 = nullptr;
    Ft2Decoder*& slot1 = g_modo_ft8 ? ord1_ft8 : ord1;
    Ft2Decoder*& slot2 = g_modo_ft8 ? ord2_ft8 : ord2;
    Ft2Decoder*& slot3 = g_modo_ft8 ? ord3_ft8 : ord3;

    if (ndeep <= 3) {                       // preset 1..3 -> nord=1 nell'originale
        if (!slot1) {
            Ft2Config c = Ft2Decoder::conservativo();
            // L'originale a ndeep=3 non fa "ordine 1 e basta": fa ordine 1 piu'
            // due passi euristici (npre1, npre2) che cercano le coppie di bit
            // capaci di azzerare i bit di parita' piu' affidabili. Sono molto
            // efficaci, e senza di essi qui si decodificava il 15% in meno.
            // pair_search riproduce quel meccanismo (vedi OsdFast).
            // Sulla ricerca larga (ordine 3, span 91/48), provata e ritirata:
            // al banco sembrava vincere su entrambi i fronti -- su 20000 parole
            // a Eb/N0=1 dB e 100000 candidati di rumore, a soglia 0,065, dava
            // 16821 decodifiche e 6 fantasmi contro 16168 e 10 della stretta
            // senza filtro. Sul traffico vero non ha retto: vedi sotto.
            // RICERCA STRETTA. La larga (ordine 3, span 91/48) e' stata
            // provata dal vivo il 28/08/2026 e va tolta: prova ~21400
            // candidati per parola contro ~600, e la CRC-14 ne ammette
            // uno ogni 16384, cioe' ~1,3 falsi attesi per parola. Il
            // controllo di plausibilita' vale 1,9 bit, divide per ~3,7 e
            // ne lascia ~0,35: moltiplicati per le parole che un ciclo
            // FT2 accetta fanno 2,8 nominativi fantasma per ciclo, misurati
            // sul traffico reale (113 decode in 10 minuti, 102 su 106 mai
            // ripetuti). Il filtro paga ~2 bit, l'allargamento ne costa ~5:
            // non lo copre. Il banco non lo vedeva perche' contava i falsi
            // su un numero fisso di candidati di rumore, non sul ritmo con
            // cui FT2 chiama davvero il decoder.
            // Il controllo di plausibilita' RESTA: con la ricerca stretta
            // i suoi bit si sommano a un tasso di falsi gia' basso.
            // RICERCA STRETTA. La larga (ordine 3, span 91/48) e' stata provata
            // due volte il 28/08/2026 e ritirata due volte. La seconda con i
            // controlli strutturali completi: sei minuti a zero fantasmi
            // sembravano assolverla, ma su una banda senza trasmissioni FT2
            // sei minuti non dimostrano niente, e con piu' tempo i fantasmi
            // sono tornati copiosi. Prova molti piu' candidati per parola:
            // la CRC-14 ne ammette uno sbagliato ogni 16384 e i filtri
            // strutturali pagano ~2 bit contro i ~5 che costa l'allargamento.
            //
            // I due numeri sono ora MISURATI (lab, cpp/candidati.cpp, contatore
            // OSD_COUNT su ogni test di CRC): 6241 candidati per parola qui e
            // 33295 con la ricerca larga, non ~600 e ~21400. La stima vecchia
            // contava le coppie dello span ma non le liste della ricerca a
            // coppie, che sono la voce dominante; il rapporto fra le due, 5,4x,
            // reggeva, i valori assoluti no.
            //
            // Serve saperlo perche' il conteggio dei fantasmi al banco NON
            // arbitra le configurazioni: su 150 000 candidati di rumore
            // gaussiano la ricerca larga ritirata due volte da' 5 fantasmi
            // esatti come questa. E' il motivo per cui il banco l'aveva
            // assolta. I candidati per parola le separano di 5,4x, coerente
            // con quello che ha fatto il traffico vero.
            c.osd_order = 2;
            // span2 = 64, non 32. E' possibile SOLO grazie al margine liberato
            // da alpha: il peso nuovo fa scendere i candidati sottoposti alla
            // CRC da 5614 a 2271 per parola, e allargare le coppie ne rimette
            // 2717. Il totale, 4988, resta SOTTO l'esposizione di prima.
            //
            // A parita' di nominativi fantasma, su 2 000 000 di candidati di
            // rumore per cella (lab/cpp/pesi_fantasmi.cpp):
            //
            //   fantasmi    prima   alpha+span64
            //         38    11446   +0,6%
            //        124    12194   +3,4%
            //        315    12757   +4,9%
            //       1478    13342   +6,9%
            //
            // Al punto di lavoro (~84 fantasmi) e' +2,5%, e il tempo resta
            // sotto quello di prima: 57,0 us contro 60,2.
            //
            // E' la prima volta in questo progetto che una larghezza di
            // ricerca si puo' aumentare senza pagarla in falsi: le due
            // precedenti sono state ritirate dall'aria proprio per quello, e
            // la differenza e' che quelle allargavano SENZA aver prima
            // guadagnato margine.
            //
            // DECODIUM_LDPC_SPAN2_BASE=32 torna al valore di prima.
            c.span2 = 64;
            if (char const* e = std::getenv ("DECODIUM_LDPC_SPAN2_BASE")) {
                int const n = std::atoi (e);
                if (n > 0 && n <= 91) c.span2 = n;
            }
            c.span3 = 0;
            c.pair_search = true;
            // ntau 13 e coppie sui primi 64, non 14 e tutti i 91 come
            // l'originale a ndeep=3. Le due manopole non erano mai state
            // misurate, e nessuna delle due fa quello che sembra:
            //
            //   ntau e' la lunghezza della chiave della tabella delle coppie.
            //   Piu' corta = piu' collisioni = liste piu' lunghe = piu'
            //   candidati provati. NON e' una manopola di cache, e' ampiezza
            //   di ricerca, e va nel verso opposto a quello che sembra.
            //
            //   pair_span limita le coppie ai bit meno affidabili. Le coppie
            //   fra bit affidabili si costruiscono, si inseriscono, e poi il
            //   taglio per limite inferiore le scarta: lavoro buttato.
            //
            // Accorciare la chiave allarga, stringere le coppie restringe. La
            // combinazione 13/64 sta SOTTO 14/91 su tutti e tre gli assi che
            // contano, misurata a 0,0 - 0,5 - 1,0 dB (lab, cpp/finale.cpp):
            //
            //   candidati alla CRC per parola   5629 contro 6241   (-10%)
            //   microsecondi per candidato      60,6 contro 78,9   (-23%)
            //   decodifiche a 1 dB             16216 contro 16170  (+46)
            //
            // Meno candidati significa meno falsi positivi della CRC, quindi
            // questo cambiamento va nel verso OPPOSTO alle due ricerche larghe
            // ritirate: quelle allargavano la ricerca, questa la stringe e
            // decodifica lo stesso di piu'. Se il traffico vero dicesse che
            // serve altro margine, 14/64 costa -0,2% di decodifiche e toglie il
            // 34% dei candidati, 16/64 costa -1,0% e ne toglie il 64%.
            c.ntau = 13;
            c.pair_span = 64;
            // Le due manopole sono forzabili da ambiente, e a differenza di
            // quelle in manopole_ft8() queste valgono ANCHE in FT2. E' una
            // deroga voluta e circoscritta: il giudice di questa scelta e' il
            // traffico vero, e senza un modo di tornare indietro riavviando, il
            // confronto in aria costerebbe una ricompilazione per ogni
            // passaggio. Le due ricerche larghe sono state ritirate proprio
            // perche' il banco non le sapeva giudicare.
            //
            //   DECODIUM_LDPC_NTAU=14 DECODIUM_LDPC_PAIR_SPAN=0   com'era prima
            //   DECODIUM_LDPC_NTAU=14 DECODIUM_LDPC_PAIR_SPAN=64  piu' prudente
            //   DECODIUM_LDPC_NTAU=16 DECODIUM_LDPC_PAIR_SPAN=64  molto prudente
            //
            // Senza variabili impostate non cambia niente.
            if (char const* e = std::getenv ("DECODIUM_LDPC_NTAU")) {
                int const n = std::atoi (e);
                if (n >= 4 && n <= 20) c.ntau = n;
            }
            if (char const* e = std::getenv ("DECODIUM_LDPC_PAIR_SPAN")) {
                int const n = std::atoi (e);
                if (n >= 0 && n <= 91) c.pair_span = n;   // 0 = tutti i 91 bit
            }
            // Soglia del gate scelta sul RUMORE, non sulle parole vere: e' il
            // caso che domina in FT2, dove la maggior parte dei candidati non
            // contiene alcun segnale e ogni accettazione e' un nominativo
            // fantasma. Su 50000 candidati di puro rumore: 0,12 falsi per mille
            // a 0,065 contro 0,42 a 0,070 e 1,30 a 0,075. Il decoder originale
            // sta a 0,33. Costa il 2,6% di decodifiche senza AP e lo 0,9% con.
            c.nd_max = 0.065f;

            // I tipi di messaggio ammessi dal controllo di plausibilita'.
            // kSoloUsati tiene standard, testo libero e nominativi non
            // standard, e lascia fuori i tre formati da contest (EU VHF, ARRL
            // RTTY): in FT2 non si vedono, e tenerli fuori vale la meta' del
            // filtro. E' una POLITICA, non un test di formato: un messaggio di
            // quei tipi non verrebbe mai decodificato. Con FASTLDPC_TIPI=tutti
            // si torna a non escludere niente, al prezzo di piu' fantasmi.
            {
                char const* env = std::getenv ("FASTLDPC_TIPI");
                c.tipi_ammessi = (g_modo_ft8 || (env && std::string (env) == "tutti"))
                                     ? plaus::kTuttiDefiniti
                                     : plaus::kSoloUsati;
            }
            c.batch = 16;                   // una parola per chiamata: batch minimo
            c.alpha_w = alpha_scelto ();
            c.max_iter = max_iter_scelto ();
            c.gate_mode = gate_mode_scelto () ? 1 : 0;
            c.gate_relax = gate_relax_scelto ();
            c.gate_dump_cb = gate_dump_callback;
            manopole_ft8 (c);
            slot1 = new Ft2Decoder (shared_code(), c);
        }
        return *slot1;
    }
    if (ndeep <= 5) {                       // preset 4..5 -> nord=2
        if (!slot2) {
            Ft2Config c = Ft2Decoder::conservativo();
            c.batch = 16;
            c.alpha_w = alpha_scelto ();
            c.max_iter = max_iter_scelto ();
            c.gate_mode = gate_mode_scelto () ? 1 : 0;
            c.gate_relax = gate_relax_scelto ();
            c.gate_dump_cb = gate_dump_callback;
            manopole_ft8 (c);
            slot2 = new Ft2Decoder (shared_code(), c);
        }
        return *slot2;
    }
    // preset >= 6 -> nord=4 nell'originale; qui l'ordine massimo e' 3.
    if (!slot3) {
        Ft2Config c = Ft2Decoder::sensibile();
        c.batch = 16;
        c.alpha_w = alpha_scelto ();
        c.max_iter = max_iter_scelto ();
        c.gate_mode = gate_mode_scelto () ? 1 : 0;
        c.gate_relax = gate_relax_scelto ();
        c.gate_dump_cb = gate_dump_callback;
        manopole_ft8 (c);
        slot3 = new Ft2Decoder (shared_code(), c);
    }
    return *slot3;
}

}  // namespace

// Soglia sui bit ribaltati, condivisa fra la via singola e quella batch.
// Coerenza con l'ipotesi AP: ACCESO di default. Se una passata AP impone dei
// bit e il decoder li ribalta lo stesso, la parola contraddice l'ipotesi che
// l'ha prodotta. E' un test strutturale, quindi non penalizza i segnali
// deboli come fa una soglia: per questo si tiene insieme al gate sui bit
// ribaltati invece che al suo posto.
// Da verificare con banda aperta: l'AP e' un'ipotesi soft e il decoder ha il
// diritto di contraddirla, quindi in teoria il test puo' scartare decodifiche
// vere. Con DECODIUM_LDPC_AP_CHECK=0 si spegne senza ricompilare.
static bool fastldpc_ap_check () {
    static bool const v = [] {
        char const* raw = std::getenv ("DECODIUM_LDPC_AP_CHECK");
        return !raw || (raw[0] != '0' && raw[0] != 0);
    }();
    return v;
}

// Traccia cosa i gate stanno scartando: DECODIUM_LDPC_GATE_LOG=1.
static bool fastldpc_gate_log () {
    static bool const v = [] {
        char const* raw = std::getenv ("DECODIUM_LDPC_GATE_LOG");
        return raw && raw[0] != '0' && raw[0] != 0;
    }();
    return v;
}

// Soglia sui bit ribaltati, TARATA PER MODO.
// FT2 resta a 22: e' la calibrazione dei nominativi fantasma, dove ogni
// accettazione di troppo diventa un falso visibile in UI e in LiveMap.
// FT8 sale a 58, cioe' esattamente il limite del decoder originale
// (kFt8MaxHardErrors in FtxFt8Stage4.cpp). In FT8 il nemico non e' il falso
// ma la sensibilita': un segnale debole si decodifica correggendo molti piu'
// di 22 bit, e tagliare a 22 buttava via decodifiche valide proprio nella
// zona in cui la sensibilita' conta. Non si diventa piu' permissivi
// dell'originale: lo stadio 4 applica comunque a valle 58 per i messaggi
// standard e 36 per i non standard.
// DECODIUM_LDPC_MAX_HARD forza il valore per entrambi i modi.
static int fastldpc_max_hard () {
    static int const forzato = [] {
        char const* raw = std::getenv ("DECODIUM_LDPC_MAX_HARD");
        if (!raw) raw = std::getenv ("DECODIUM_FT2_LDPC_MAX_HARD");
        int const n = raw ? std::atoi (raw) : 0;
        return (n > 0 && n <= kN) ? n : 0;
    }();
    if (forzato) return forzato;
    return g_modo_ft8 ? 58 : 22;
}

// Parola con TUTTI i 77 bit del messaggio imposti dall'a priori (tipo 8):
// gli errori contati sono sui 97 bit di parita' liberi, e la parola di codice
// compatibile e' una sola, quindi il limite di 22 tarato sui fantasmi di FT2
// non c'entra: taglierebbe proprio le verifiche deboli che sono il senso
// dell'ipotesi. Si usa il valore di FT8, dove il tipo 8 e' stato misurato.
// DECODIUM_LDPC_MAX_HARD_APMSG lo forza.
static int fastldpc_max_hard_apmsg () {
    static int const v = [] {
        char const* raw = std::getenv ("DECODIUM_LDPC_MAX_HARD_APMSG");
        int const n = raw ? std::atoi (raw) : 0;
        return (n > 0 && n <= kN) ? n : 58;
    }();
    return v;
}

// Le quattro leve del banco di raccolta dati (tests/ft2_gate_dump.cpp).
// Nessun altro chiamante nel programma le usa: in produzione restano mute.
//
// fastldpc_simd_gate_dump_open_c: apre (in append) il file dove scrivere le righe
// del dataset; path vuoto o nullo chiude e basta. Va chiamata PRIMA del primo
// decode del thread che decodifica, insieme a DECODIUM_LDPC_GATE=1 (altrimenti
// gate_mode e' spento e nessuna feature viene calcolata).
extern "C" void fastldpc_simd_gate_dump_open_c (char const* path) {
    std::lock_guard<std::mutex> lock (gate_dump_mutex ());
    std::FILE*& f = gate_dump_file ();
    if (f) { std::fclose (f); f = nullptr; }
    if (path && path[0]) f = std::fopen (path, "a");
}

extern "C" void fastldpc_simd_gate_dump_close_c () {
    std::lock_guard<std::mutex> lock (gate_dump_mutex ());
    std::FILE*& f = gate_dump_file ();
    if (f) { std::fclose (f); f = nullptr; }
}

// fastldpc_simd_gate_truth_set_c: i 174 bit (dominio scrambled+LDPC) del messaggio
// che il banco di prova sta per far decodificare. Senza questa chiamata i
// candidati passano da gate_dump_callback ma vengono scartati (nessuna
// etichetta nota): serve per non scrivere righe non etichettabili quando il
// banco genera anche slot di solo rumore.
extern "C" void fastldpc_simd_gate_truth_set_c (signed char const* cw174) {
    g_gate_truth_cw174.assign (cw174, cw174 + kN);
}

extern "C" void fastldpc_simd_gate_truth_clear_c () {
    g_gate_truth_cw174.clear ();
}

static int fastldpc_max_hard_per (signed char const* apmask_word) {
    if (!apmask_word) return fastldpc_max_hard ();
    int n_ap = 0;
    for (int i = 0; i < kN; ++i) n_ap += apmask_word[i] != 0;
    return n_ap >= 77 ? fastldpc_max_hard_apmsg () : fastldpc_max_hard ();
}

extern "C" void fastldpc_simd_decode174_91_c (float const* llr_in, int Keff,
                                              int maxosd, int norder,
                                              signed char const* apmask_in,
                                              signed char* message91_out,
                                              signed char* cw_out, int* ntype_out,
                                              int* nharderror_out, float* dmin_out)
{
    if (message91_out) std::memset (message91_out, 0, 91);
    if (cw_out) std::memset (cw_out, 0, kN);
    if (ntype_out) *ntype_out = 0;
    if (nharderror_out) *nharderror_out = -1;
    if (dmin_out) *dmin_out = 0.0f;
    if (!llr_in || !apmask_in || Keff != 91) return;

    Ft2Decoder& dec = decoder_for_preset (norder);

    // Decodium: positivo = bit 1. fastldpc: positivo = bit 0.
    float llr[kN];
    uint8_t apmask[kN];
    for (int i = 0; i < kN; ++i) {
        llr[i] = -llr_in[i];
        apmask[i] = apmask_in[i] != 0 ? 1 : 0;
    }

    uint8_t bits[kN], accepted = 0;
    const long bp_before = dec.stats().by_bp;
    if (maxosd < 0) {
        // niente OSD: si passa per il preset veloce, riusando la stessa istanza
        // non si puo', quindi si accetta solo cio' che chiude il min-sum
        Ft2Config c = Ft2Decoder::veloce();
        c.batch = 16;
        // perdita voluta, come gli altri decoder per thread
        static thread_local Ft2Decoder* solo_bp = nullptr;
        if (!solo_bp) solo_bp = new Ft2Decoder (shared_code(), c);
        solo_bp->decode_batch (llr, 1, bits, &accepted, nullptr, apmask);
        if (!accepted) return;
        if (ntype_out) *ntype_out = 1;
    } else {
        dec.decode_batch (llr, 1, bits, &accepted, nullptr, apmask);
        if (!accepted) return;
        if (ntype_out) *ntype_out = (dec.stats().by_bp > bp_before) ? 1 : 2;
    }

    if (message91_out)
        for (int i = 0; i < 91; ++i) message91_out[i] = (signed char) bits[i];
    if (cw_out)
        for (int i = 0; i < kN; ++i) cw_out[i] = (signed char) bits[i];

    // Stesse metriche di ftx_ldpc174_91_metrics_c, calcolate sugli LLR nella
    // convenzione del chiamante: nharderror = bit in disaccordo con la
    // decisione hard, dmin = somma dei |LLR| corrispondenti.
    int nhard = 0; float dmin = 0.0f;
    for (int i = 0; i < kN; ++i) {
        const int bit = bits[i] ? 1 : 0;
        const int hdec = llr_in[i] >= 0.0f ? 1 : 0;
        if ((hdec ^ bit) != 0) { ++nhard; dmin += std::fabs (llr_in[i]); }
    }
    // Gate sui bit ribaltati. Con fastldpc attivo il percorso NON passa da
    // ftx_decode174_91_c, quindi ldpc174_reject_by_nd non viene mai
    // applicato: senza questo controllo l'unico filtro resta nd, e nd non
    // basta perche' pesa i bit per il loro |LLR| e nel rumore vero gli LLR
    // sono deboli -- ribaltarne quaranta costa poco e nd resta basso.
    //
    // Tarato sul traffico reale del 28/08/2026: su 74 decodifiche di
    // stazioni ripetute (UX5HY, RV3ZN, F5PBG, QSO IK7VKC/F5PBG, da +11 a
    // -26 dB) nharderror aveva mediana 1, p99 16, massimo 20; i fantasmi
    // partivano da 23, con una valle netta fra 19 e 22. Stessa variabile
    // d'ambiente del gate consolidato, cosi' i due percorsi si regolano
    // insieme.
    {
        if (nhard > fastldpc_max_hard_per (apmask_in)) {
            if (message91_out) std::memset (message91_out, 0, 91);
            if (cw_out) std::memset (cw_out, 0, kN);
            if (ntype_out) *ntype_out = 0;
            if (nharderror_out) *nharderror_out = -1;
            if (dmin_out) *dmin_out = 0.0f;
            return;
        }
    }

    if (nharderror_out) *nharderror_out = nhard;
    if (dmin_out) *dmin_out = dmin;
}

// ---------------------------------------------------------------------------
// Versione a blocco.
//
// Il min-sum lavora su 16 parole per registro AVX2 o 8 per registro NEON: con
// una parola per chiamata quasi tutte le corsie restano vuote e si paga il
// batch intero per una parola sola. FT2 prova fino a 6 ipotesi AP sullo stesso candidato, e sono
// indipendenti fra loro (llr e apmask non dipendono dall'esito delle
// precedenti), quindi possono viaggiare insieme.
//
// Il conto: 6 passate in blocco costano quanto UNA chiamata singola di oggi.
// Nel caso migliore -- la prima passata decodifica -- non si perde nulla; nel
// caso peggiore, quando nessuna decodifica e nessuna passata si puo' saltare,
// si guadagna quasi il fattore sei.
//
// llr_in, apmask_in: [n][174] contigui. Le uscite sono [n] o [n][...].
// Il chiamante scorre poi i risultati nell'ordine originale e prende il primo
// valido: la semantica resta quella del ciclo sequenziale.
extern "C" void fastldpc_simd_decode174_91_batch_c (int n, float const* llr_in,
                                                    signed char const* apmask_in,
                                                    int Keff, int maxosd, int norder,
                                                    signed char* message91_out,
                                                    signed char* cw_out,
                                                    int* ntype_out,
                                                    int* nharderror_out,
                                                    float* dmin_out)
{
    if (n <= 0 || !llr_in || !apmask_in || Keff != 91) return;
    (void) maxosd; // mantenuto nella firma pubblica per compatibilita' ABI

    Ft2Decoder& dec = decoder_for_preset (norder);

    // I buffer di lavoro per thread NON si distruggono alla morte del
    // thread, e la perdita e' voluta. Il loro distruttore girerebbe dentro
    // LdrShutdownThread, quando la memoria per-thread e' gia' smontata:
    // con PageHeap si vede il segfault dentro ~vector<unsigned char> da
    // run_dtor_list, e senza PageHeap la stessa free corrompe lo heap in
    // silenzio, facendolo esplodere piu' tardi altrove. E' lo stesso motivo
    // per cui planner_mutex e gli spazi di lavoro del downsample sono
    // dichiarati come perdite volute.
    static thread_local std::vector<float>& llr = *new std::vector<float>;
    static thread_local std::vector<uint8_t>& apmask = *new std::vector<uint8_t>;
    static thread_local std::vector<uint8_t>& bits = *new std::vector<uint8_t>;
    static thread_local std::vector<uint8_t>& accepted = *new std::vector<uint8_t>;
    llr.resize ((size_t) n * kN);
    apmask.resize ((size_t) n * kN);
    bits.resize ((size_t) n * kN);
    accepted.resize ((size_t) n);

    for (size_t i = 0; i < (size_t) n * kN; ++i) {
        llr[i] = -llr_in[i];                       // Decodium: positivo = bit 1
        apmask[i] = apmask_in[i] != 0 ? 1 : 0;
    }

    const long bp_before = dec.stats ().by_bp;
    dec.decode_batch (llr.data (), n, bits.data (), accepted.data (), nullptr, apmask.data ());
    // by_bp cresce su tutto il blocco: non si puo' attribuire a una singola
    // parola, quindi ntype distingue solo accettata (2) da non accettata (0).
    // Il chiamante di FT2 usa ntype solo per il diario.
    (void) bp_before;

    for (int w = 0; w < n; ++w) {
        signed char* msg = message91_out ? message91_out + (size_t) w * 91 : nullptr;
        signed char* cw  = cw_out ? cw_out + (size_t) w * kN : nullptr;
        if (msg) std::memset (msg, 0, 91);
        if (cw) std::memset (cw, 0, kN);
        if (ntype_out) ntype_out[w] = 0;
        if (nharderror_out) nharderror_out[w] = -1;
        if (dmin_out) dmin_out[w] = 0.0f;
        if (!accepted[(size_t) w]) continue;

        const uint8_t* b = &bits[(size_t) w * kN];
        if (msg) for (int i = 0; i < 91; ++i) msg[i] = (signed char) b[i];
        if (cw)  for (int i = 0; i < kN; ++i) cw[i] = (signed char) b[i];
        if (ntype_out) ntype_out[w] = 2;

        int nhard = 0; float dmin = 0.0f;
        const float* src = llr_in + (size_t) w * kN;
        for (int i = 0; i < kN; ++i) {
            const int hdec = src[i] >= 0.0f ? 1 : 0;
            if ((b[i] ? 1 : 0) != hdec) { ++nhard; dmin += std::fabs (src[i]); }
        }

        // I due controlli qui sotto mancavano sulla via BATCH, che e' quella
        // che FT2 usa davvero: nharderror veniva calcolato e riportato al
        // chiamante, ma non filtrava niente. Il 28/08/2026 arrivavano in lista
        // decode con 31, 36, 38, 40, 41 e 43 bit ribaltati.

        // 1) bit ribaltati. Su 74 decodifiche vere di stazioni ripetute, da
        //    +11 a -26 dB: mediana 1, p99 16, massimo 20. I fantasmi partivano
        //    da 23, con una valle netta fra 19 e 22.
        int const max_hard = fastldpc_max_hard_per (apmask_in + (size_t) w * kN);
        if (nhard > max_hard) {
            if (fastldpc_gate_log ())
                std::fprintf (stderr, "[GATE] scartata: bit ribaltati %d > %d\n",
                              nhard, max_hard);
            if (msg) std::memset (msg, 0, 91);
            if (cw) std::memset (cw, 0, kN);
            if (ntype_out) ntype_out[w] = 0;
            continue;
        }

        // 2) coerenza con l'ipotesi a priori: se una passata AP ha imposto
        //    dei bit e il decoder li ha ribaltati lo stesso, la parola
        //    contraddice l'ipotesi che l'ha prodotta.
        //
        //    ATTENZIONE, da verificare con banda aperta: l'AP e' un'ipotesi
        //    SOFT e il decoder ha il diritto di contraddirla quando il
        //    segnale lo richiede, quindi questo test puo' scartare decodifiche
        //    vere deboli. Qui tiene fuori molti fantasmi, ma il bilancio sui
        //    segnali veri non e' stato misurato: la banda era ferma.
        if (fastldpc_ap_check ()) {
            const signed char* apm = apmask_in + (size_t) w * kN;
            bool coerente = true;
            for (int i = 0; i < kN && coerente; ++i) {
                if (!apm[i]) continue;
                const int atteso = src[i] >= 0.0f ? 1 : 0;
                if ((b[i] ? 1 : 0) != atteso) coerente = false;
            }
            if (!coerente) {
                if (fastldpc_gate_log ())
                    std::fprintf (stderr, "[GATE] scartata: bit AP contraddetti\n");
                if (msg) std::memset (msg, 0, 91);
                if (cw) std::memset (cw, 0, kN);
                if (ntype_out) ntype_out[w] = 0;
                continue;
            }
        }

        if (nharderror_out) nharderror_out[w] = nhard;
        if (dmin_out) dmin_out[w] = dmin;
    }
}
