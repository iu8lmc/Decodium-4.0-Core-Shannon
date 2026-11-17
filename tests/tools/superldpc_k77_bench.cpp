// Banco di misura per l'OSD sul codice concatenato (174,77) e per il gate a
// margine di superldpc (Detector/superldpc: OsdFast::crc_in_base, gate.hpp
// margin_accept), contro la configurazione che il programma usa davvero.
//
// "Davvero" vuol dire che le parole passano da superldpc_simd_decode174_91_batch_c,
// la stessa funzione che chiamano FT8 (FtxFt8Stage4.cpp) e FT2
// (FtxFt2Stage7.cpp): preset di esercizio (OSD ordine 2, span 64, ricerca a
// coppie, alpha 0,578, nd_max 0,065), gate appreso acceso in FT8, controllo di
// plausibilita', soglia sui bit ribaltati (58 in FT8, 22 in FT2) e coerenza
// AP. Non i preset del README. La variante "base" gira con DECODIUM_LDPC_K77=0
// (la ricerca su (174,91) di prima), la variante "new" con DECODIUM_LDPC_K77=1
// (ricerca nuova e gate a margine), "k77" con la sola ricerca nuova e i gate
// di prima (DECODIUM_LDPC_K77_NOGATE=1, configurazione di misura); tutte
// vedono le stesse parole, generate dallo stesso seme.
//
// Per ogni Eb/N0 (riferito ai 91 bit, come nel README) stampa FER e false
// decodifiche sulle parole con segnale, e le false accettazioni sulle parole
// di solo rumore. Una parola accettata che non e' quella trasmessa e' una
// falsa decodifica.
//
// Canali:
//   chan=bpsk  AWGN/BPSK, LLR esatti
//   chan=fsk   M-FSK non coerente simbolo per simbolo (M=8 per mode=ft8, M=4
//              per mode=ft2), metriche max-log sulle ampiezze come ft8b/ft4
//   chan=chain catena FSK a livello di simbolo con gli insiemi di metriche del
//              demodulatore, contata a trame (vedi piu' sotto, "chan=chain");
//              variants=base,clip2.0,clip0,new confronta i tagli degli LLR
// In entrambi gli LLR sono normalizzati come fa il demodulatore (deviazione
// standard 1 sulla parola, poi x 2,83): il gate appreso di FT8 dipende dalla
// scala, quindi senza questo passo non si misurerebbe la configurazione vera.
//
// Non fa parte della build normale: -DDECODIUM_BUILD_SUPERLDPC_K77_BENCH=ON
// costruisce il target superldpc_k77_bench e registra selftest=1 in ctest
// (tests/CMakeLists.txt). A mano, dalla radice dei sorgenti:
//
//   g++ -O3 -DNDEBUG -std=gnu++17 -mavx2 -mfma -DSUPERLDPC_HAVE_GATE_WEIGHTS=1 -I.
//       -o superldpc_k77_bench tests/tools/superldpc_k77_bench.cpp
//       Detector/superldpc/decodium_bridge.cpp Detector/FtxLdpc.cpp
//       lib/crc14.cpp lib/crc13.cpp -lpthread
//
//   ./superldpc_k77_bench selftest=1
//   ./superldpc_k77_bench mode=ft8 chan=fsk ebn0=0.5,1,1.5,2 n=20000 noise=200000 seeds=11,12,13
//   ./superldpc_k77_bench mode=ft2 chan=fsk ap=32 ...        (32, 61 o 77 bit noti)
//   ./superldpc_k77_bench mode=ft8 dump=righe.txt ...        (taratura, vedi sotto)
//
// selftest=1 verifica le 14 righe della CRC e la ricerca nuova, ed esce con 1
// se qualcosa non torna:
//   - ogni parola prodotta dal codificatore del progetto (crc14 di lib/ +
//     encode174_91_nocrc_) soddisfa le 83 righe di H e le 14 della CRC;
//   - ogni singolo bit 0..90 ribaltato viola almeno una riga della CRC;
//   - il risultato della ricerca (174,77) passa SEMPRE crc14_ok e la crc14 del
//     progetto, su segnale e su rumore, con e senza bit AP, e con il vincolo
//     AP attivo non contraddice mai un bit noto;
//   - la copia della configurazione usata da dump= decide come il bridge.
//
// dump=FILE scrive una riga per ogni parola a cui l'OSD (174,77) ha trovato un
// candidato, a gate aperti, per tarare il gate a margine fuori linea:
//   D seme S|N ebn0 vero nd nd2 ha_concorrente log2_provati frazione_liberi nhard logit
// (logit = uscita del gate appreso sullo stesso candidato)
// (dumpnd=X scrive solo le righe di rumore con nd <= X: sono le sole che una
// soglia puo' accettare, e le altre riempiono il disco)
// piu' una riga "B seme S|N ebn0 parole ok_minsum falsi_minsum". Le soglie si
// scelgono su un insieme di semi e si riportano su semi diversi.
#include "Detector/superldpc/ft2_decoder.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

extern "C" {
void superldpc_simd_set_ft8_mode_c (int on);
void superldpc_simd_decode174_91_batch_c (int n, float const* llr, signed char const* apmask,
                                          int Keff, int maxosd, int norder,
                                          signed char* message91, signed char* cw,
                                          int* ntype, int* nharderror, float* dmin);
void ftx_ldpc174_91_tables_c (int* Mn_out, int* Nm_out, int* nrw_out, int* ncw_out);
void encode174_91_nocrc_ (signed char* message_in, signed char* codeword_out);
short crc14 (unsigned char const* data, int length);

// Simboli che FtxLdpc.cpp e decodium_bridge.cpp si aspettano dal resto del
// programma. Il banco non passa mai dal decoder originale, quindi i primi due
// non vengono chiamati.
void ftx_ldpc174_91_metrics_c (signed char const*, float const*, int*, float*) { std::abort (); }
int ftx_ldpc174_91_finalize_c (signed char const*, float const*, signed char*, int*, float*)
{
  std::abort ();
}
// Vettore di scrambling di FT2: copia di raw_rvec in Detector/FtxFt2Stage7.cpp
// (ftx_ft2_rvec_c), che non si puo' collegare qui senza tutto il decoder FT2.
void ftx_ft2_rvec_c (signed char* out77)
{
  static int const raw_rvec[77] = {
    0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0,
    1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1,
    0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1
  };
  for (int i = 0; i < 77; ++i) out77[i] = static_cast<signed char> (raw_rvec[i]);
}
}

namespace {

constexpr int kN = 174;

// setenv/unsetenv non esistono con MSVC. valore == nullptr toglie la variabile.
void imposta_env (char const* nome, char const* valore)
{
#ifdef _WIN32
  _putenv_s (nome, valore ? valore : "");
#else
  if (valore) setenv (nome, valore, 1);
  else unsetenv (nome);
#endif
}

Code project_code ()
{
  std::vector<int> mn (3 * kN), nm (7 * 83), nrw (83);
  int ncw = 0;
  ftx_ldpc174_91_tables_c (mn.data (), nm.data (), nrw.data (), &ncw);
  Code c;
  c.M = 83; c.N = kN;
  c.row_ptr.push_back (0);
  for (int m = 0; m < c.M; ++m)
    {
      for (int r = 0; r < nrw[static_cast<size_t> (m)]; ++r)
        c.col_idx.push_back (nm[static_cast<size_t> (r + 7 * m)] - 1);
      c.row_ptr.push_back (static_cast<int> (c.col_idx.size ()));
    }
  return c;
}

struct Options
{
  bool ft8 = true;
  bool fsk = true;
  std::vector<double> ebn0 {0.5, 1.0, 1.5, 2.0};
  std::vector<int> seeds {11, 12, 13};
  int n = 20000;
  int noise = 200000;
  int ap = 0;
  bool apwrong = false;
  bool selftest = false;
  std::string dump;
  double dumpnd = 1.0;
  double dumplogit = -1e9;
  std::string variants = "base,new";
  bool chain = false;
  std::vector<double> clips {0.0, 2.5, 2.0};
};

std::vector<std::string> split (std::string const& s)
{
  std::vector<std::string> out;
  std::stringstream ss (s);
  std::string item;
  while (std::getline (ss, item, ',')) if (!item.empty ()) out.push_back (item);
  return out;
}

// CRC-14 del progetto sui 77 bit del messaggio, come la calcola il
// codificatore: 96 bit in 12 byte, i 19 finali a zero.
int project_crc14 (uint8_t const* msg77)
{
  unsigned char bytes[12] = {};
  for (int i = 0; i < 77; ++i)
    if (msg77[i]) bytes[i >> 3] = static_cast<unsigned char> (bytes[i >> 3] | (0x80 >> (i & 7)));
  return static_cast<int> (crc14 (bytes, 12)) & 0x3FFF;
}

// Parola di codice del messaggio (gia' mescolato, in FT2), dal codificatore
// del progetto.
void project_encode (uint8_t const* msg77, uint8_t* cw)
{
  signed char m91[91], out[kN];
  for (int i = 0; i < 77; ++i) m91[i] = static_cast<signed char> (msg77[i]);
  int const crc = project_crc14 (msg77);
  for (int i = 0; i < 14; ++i) m91[77 + i] = static_cast<signed char> ((crc >> (13 - i)) & 1);
  encode174_91_nocrc_ (m91, out);
  for (int i = 0; i < kN; ++i) cw[i] = static_cast<uint8_t> (out[i]);
}

struct Generator
{
  bool ft8;
  bool fsk;
  std::mt19937_64 rng;
  std::normal_distribution<double> nrm {0.0, 1.0};
  uint8_t rvec[77];

  Generator (bool ft8_, bool fsk_, uint64_t seed) : ft8 (ft8_), fsk (fsk_), rng (seed)
  {
    signed char tmp[77];
    ftx_ft2_rvec_c (tmp);
    for (int i = 0; i < 77; ++i) rvec[i] = static_cast<uint8_t> (tmp[i] & 1);
  }

  // Messaggio casuale fra quelli che il controllo di plausibilita' del modo
  // ammette (un messaggio vero lo supera sempre), nel dominio che il decoder
  // vede: in chiaro per FT8, mescolato con rvec per FT2.
  void message (uint8_t* msg77)
  {
    uint32_t const tipi = ft8 ? plaus::kTuttiDefiniti : plaus::kSoloUsati;
    uint8_t chiaro[77];
    do
      {
        for (int i = 0; i < 77; ++i) chiaro[i] = static_cast<uint8_t> (rng () & 1);
      }
    while (!plaus::message77_ok (chiaro, tipi));
    for (int i = 0; i < 77; ++i)
      msg77[i] = ft8 ? chiaro[i] : static_cast<uint8_t> (chiaro[i] ^ rvec[i]);
  }

  // LLR nella convenzione di Decodium (positivo = bit 1), normalizzati come
  // normalizebmet e scalati per 2,83. cw nullo = solo rumore.
  void llr (uint8_t const* cw, double ebn0_db, float* out)
  {
    double const ebn0 = std::pow (10.0, ebn0_db / 10.0);
    if (!fsk)
      {
        double const esn0 = ebn0 * 91.0 / 174.0;
        double const sigma = std::sqrt (1.0 / (2.0 * esn0));
        for (int v = 0; v < kN; ++v)
          {
            double const x = cw ? (cw[v] ? 1.0 : -1.0) : 0.0;
            out[v] = static_cast<float> (2.0 * (x + sigma * nrm (rng)) / (sigma * sigma));
          }
      }
    else
      {
        static int const gray8[8] = {0, 1, 3, 2, 5, 6, 4, 7};
        static int const gray4[4] = {0, 1, 3, 2};
        int const bps = ft8 ? 3 : 2, M = 1 << bps;
        int const* gray = ft8 ? gray8 : gray4;
        double const amp = std::sqrt (ebn0 * bps * 91.0 / 174.0);
        double const sd = std::sqrt (0.5);
        for (int s = 0; s < kN / bps; ++s)
          {
            int sym = 0;
            if (cw) for (int b = 0; b < bps; ++b) sym = (sym << 1) | cw[s * bps + b];
            double mag[8];
            for (int i = 0; i < M; ++i)     // i = valore del simbolo, tono = gray[i]
              {
                double re = sd * nrm (rng), im = sd * nrm (rng);
                if (cw && i == sym) re += amp;
                mag[i] = std::sqrt (re * re + im * im);
              }
            (void) gray;                    // i toni sono scambiabili: basta la mappa bit
            for (int b = 0; b < bps; ++b)
              {
                double m1 = -1.0, m0 = -1.0;
                for (int i = 0; i < M; ++i)
                  {
                    if ((i >> (bps - 1 - b)) & 1) m1 = std::max (m1, mag[i]);
                    else m0 = std::max (m0, mag[i]);
                  }
                out[s * bps + b] = static_cast<float> (m1 - m0);
              }
          }
      }
    double s1 = 0, s2 = 0;
    for (int v = 0; v < kN; ++v) { s1 += out[v]; s2 += static_cast<double> (out[v]) * out[v]; }
    double const mean = s1 / kN, var = s2 / kN - mean * mean;
    double const sig = var > 0 ? std::sqrt (var) : std::sqrt (s2 / kN);
    for (int v = 0; v < kN; ++v) out[v] = static_cast<float> (2.83 * out[v] / sig);
  }
};

// Maschera AP con `nap` bit noti, sulle posizioni che usano FT8 e FT2:
// 32 = primo nominativo + i3, 61 = due nominativi + i3, 77 = messaggio intero.
void ap_mask (int nap, signed char* mask)
{
  std::memset (mask, 0, kN);
  if (nap <= 0) return;
  int const ncall = nap >= 77 ? 77 : (nap >= 61 ? 58 : 29);
  for (int i = 0; i < ncall; ++i) mask[i] = 1;
  for (int i = 74; i < 77; ++i) mask[i] = 1;
}

void ap_apply (signed char const* mask, uint8_t const* bits, float* llr)
{
  float apmag = 0;
  for (int v = 0; v < kN; ++v) apmag = std::max (apmag, std::fabs (llr[v]));
  apmag *= 1.01f;
  for (int v = 0; v < kN; ++v)
    if (mask[v]) llr[v] = bits[v] ? apmag : -apmag;
}

struct Tally
{
  long words = 0, ok = 0, falsi = 0;
  long parole_false = 0;        // solo chan=chain: parole, non trame
  double sec = 0;
};

// Un punto di misura attraverso il bridge: `n` parole, con segnale a ebn0
// oppure di solo rumore.
Tally run_bridge (Options const& o, uint64_t seed, bool segnale, double ebn0, int n)
{
  constexpr int kBlock = 64;
  Generator gen (o.ft8, o.fsk, seed);
  std::vector<float> llr (static_cast<size_t> (kBlock) * kN);
  std::vector<signed char> apmask (static_cast<size_t> (kBlock) * kN, 0);
  std::vector<signed char> msg (static_cast<size_t> (kBlock) * 91), cw (static_cast<size_t> (kBlock) * kN);
  std::vector<int> ntype (kBlock), nhard (kBlock);
  std::vector<float> dmin (kBlock);
  std::vector<uint8_t> truth (static_cast<size_t> (kBlock) * kN);
  uint8_t m77[77], ipotesi[kN];
  Tally t;
  superldpc_simd_set_ft8_mode_c (o.ft8 ? 1 : 0);
  for (int off = 0; off < n; off += kBlock)
    {
      int const cnt = std::min (kBlock, n - off);
      for (int b = 0; b < cnt; ++b)
        {
          uint8_t* tw = &truth[static_cast<size_t> (b) * kN];
          gen.message (m77);
          project_encode (m77, tw);
          float* w = &llr[static_cast<size_t> (b) * kN];
          gen.llr (segnale ? tw : nullptr, ebn0, w);
          signed char* am = &apmask[static_cast<size_t> (b) * kN];
          ap_mask (o.ap, am);
          if (o.ap > 0)
            {
              // Con segnale l'ipotesi e' quella giusta, a meno di apwrong; sul
              // rumore e' il messaggio sorteggiato sopra, che nessuno ha
              // trasmesso.
              std::memcpy (ipotesi, tw, kN);
              if (segnale && o.apwrong) { gen.message (m77); project_encode (m77, ipotesi); }
              ap_apply (am, ipotesi, w);
            }
        }
      auto const t0 = std::chrono::steady_clock::now ();
      superldpc_simd_decode174_91_batch_c (cnt, llr.data (), apmask.data (), 91, 3,
                                           o.ft8 ? 2 : 3, msg.data (), cw.data (),
                                           ntype.data (), nhard.data (), dmin.data ());
      t.sec += std::chrono::duration<double> (std::chrono::steady_clock::now () - t0).count ();
      for (int b = 0; b < cnt; ++b)
        {
          ++t.words;
          if (ntype[static_cast<size_t> (b)] == 0) continue;
          bool uguale = segnale;
          for (int v = 0; v < kN && uguale; ++v)
            uguale = cw[static_cast<size_t> (b) * kN + v]
                     == static_cast<signed char> (truth[static_cast<size_t> (b) * kN + v]);
          if (uguale) ++t.ok; else ++t.falsi;
        }
    }
  return t;
}

// Copia del preset di esercizio di decodium_bridge.cpp (decoder_for_preset,
// norder <= 3), per il percorso dump= che deve leggere le grandezze interne.
// selftest=1 controlla che decida come il bridge.
Ft2Config shipped_config (bool ft8, uint8_t const* rvec)
{
  Ft2Config c = Ft2Decoder::conservativo ();
  c.osd_order = 2; c.span2 = 64; c.span3 = 0;
  c.pair_search = true; c.ntau = 13; c.pair_span = 64;
  c.nd_max = 0.065f;
  c.tipi_ammessi = ft8 ? plaus::kTuttiDefiniti : plaus::kSoloUsati;
  c.descramble77 = ft8 ? nullptr : rvec;
  c.batch = 16; c.alpha_w = 37888; c.max_iter = 10;
  c.gate_mode = ft8 ? 1 : 0; c.gate_relax = 0.25f; c.gate_is_ft8 = ft8;
  return c;
}

int max_hard (bool ft8, int nap) { return (ft8 || nap >= 77) ? 58 : 22; }

// Decodifica diretta con Ft2Decoder piu' i due controlli a valle del bridge
// (bit ribaltati, coerenza AP). Ritorna 1 se la parola e' accettata.
struct Direct
{
  Ft2Decoder dec;
  bool ft8;
  Direct (Code const& code, Ft2Config const& c, bool ft8_) : dec (code, c), ft8 (ft8_) {}

  int decode (float const* llr_dec, signed char const* apm, uint8_t* bits, int* nhard_out,
              float* nd_out = nullptr, bool* by_bp = nullptr)
  {
    float llr[kN];
    float nd = 0;
    uint8_t mask[kN], acc = 0;
    long const bp_prima = dec.stats ().by_bp;
    int nap = 0;
    for (int v = 0; v < kN; ++v)
      {
        llr[v] = -llr_dec[v];
        mask[v] = apm[v] != 0;
        nap += mask[v];
      }
    dec.decode_batch (llr, 1, bits, &acc, &nd, mask);
    if (!acc) return 0;
    if (nd_out) *nd_out = nd;
    if (by_bp) *by_bp = dec.stats ().by_bp > bp_prima;
    int nhard = 0;
    for (int v = 0; v < kN; ++v) nhard += (bits[v] ? 1 : 0) != (llr_dec[v] >= 0.0f ? 1 : 0);
    if (nhard_out) *nhard_out = nhard;
    if (nhard > max_hard (ft8, nap)) return 0;
    for (int v = 0; v < kN; ++v)
      if (mask[v] && (bits[v] ? 1 : 0) != (llr_dec[v] >= 0.0f ? 1 : 0)) return 0;
    return 1;
  }
};

void run_dump (Options const& o, Code const& code, FILE* f)
{
  signed char rv[77];
  ftx_ft2_rvec_c (rv);
  static uint8_t rvec[77];
  for (int i = 0; i < 77; ++i) rvec[i] = static_cast<uint8_t> (rv[i] & 1);
  Ft2Config c = shipped_config (o.ft8, rvec);
  c.osd_k77 = true; c.ap_vincolo = true;
  c.margin_gate = 1; c.margin_relax = 1.0f; c.margin = MarginGate {}; c.margin.thr = 1e9f;
  c.gate_mode = 1;      // solo per avere le feature del gate appreso nella riga
  Direct d (code, c, o.ft8);
  uint8_t m77[77], truth[kN], ipotesi[kN], bits[kN];
  float llr[kN];
  signed char am[kN];
  for (int seed : o.seeds)
    {
      for (size_t p = 0; p <= o.ebn0.size (); ++p)
        {
          bool const segnale = p < o.ebn0.size ();
          double const ebn0 = segnale ? o.ebn0[p] : 0.0;
          int const n = segnale ? o.n : o.noise;
          Generator gen (o.ft8, o.fsk,
                         static_cast<uint64_t> (seed) * 1000 + (segnale ? p + 1 : 0));
          long ok_bp = 0, falsi_bp = 0;
          for (int i = 0; i < n; ++i)
            {
              gen.message (m77);
              project_encode (m77, truth);
              gen.llr (segnale ? truth : nullptr, ebn0, llr);
              ap_mask (o.ap, am);
              if (o.ap > 0)
                {
                  std::memcpy (ipotesi, truth, kN);
                  if (segnale && o.apwrong) { gen.message (m77); project_encode (m77, ipotesi); }
                  ap_apply (am, ipotesi, llr);
                }
              int nhard = kN;       // resta cosi' se Ft2Decoder non accetta
              int const acc = d.decode (llr, am, bits, &nhard);
              bool const vero = segnale && std::memcmp (bits, truth, kN) == 0;
              MarginFeatures const& mf = d.dec.margin_features (0);
              if (!mf.valid)
                {
                  if (acc) { if (vero) ++ok_bp; else ++falsi_bp; }
                  continue;
                }
              if (!segnale && static_cast<double> (mf.nd) > o.dumpnd) continue;
              // nhard e' valido solo se Ft2Decoder ha accettato; a gate aperti
              // succede per ogni candidato dell'OSD che ha un concorrente (con
              // ap=77 non ne ha, decide la regola di prima e il dump non serve).
              std::fprintf (f, "D %d %c %.2f %d %.5f %.5f %d %.3f %.4f %d %.4f\n", seed,
                            segnale ? 'S' : 'N', ebn0, vero ? 1 : 0,
                            static_cast<double> (mf.nd), static_cast<double> (mf.nd2),
                            mf.has2 ? 1 : 0, static_cast<double> (mf.log2_tested),
                            static_cast<double> (mf.free_ratio), nhard,
                            static_cast<double> (gate_logit (d.dec.gate_features (0), o.ft8)));
            }
          std::fprintf (f, "B %d %c %.2f %d %ld %ld\n", seed, segnale ? 'S' : 'N', ebn0, n,
                        ok_bp, falsi_bp);
        }
    }
}

// ---------------------------------------------------------------------------
// chan=chain: catena a livello di simbolo dopo la sincronizzazione, con le
// metriche di bit che il demodulatore passa DAVVERO al decoder, cioe' piu'
// insiemi di LLR per la stessa trama e non uno solo.
//
//   FT8: 79 simboli 8-FSK ortogonali (tre blocchi Costas), fase della portante
//        costante sulla trama, AWGN complesso. Cinque insiemi come
//        run_ft8_bitmetrics: max-log sul modulo della somma coerente di gruppi
//        di 1, 2 e 3 simboli (A, B, C), l'insieme a un simbolo diviso per il
//        massimo (D) e, bit per bit, il piu' grande in modulo dei tre (E);
//        ciascuno normalizzato al valore efficace e scalato per 2,83.
//   FT2: 103 simboli 4-FSK (quattro blocchi di sincronismo da 4 simboli), tre
//        insiemi su gruppi di 1, 2 e 4 simboli che attraversano i blocchi di
//        sincronismo come in run_ft2_bitmetrics, normalizzati come
//        normalizebmet sulle 206 righe. Qui il demodulatore vero usa una
//        log-somma di esponenziali sulle potenze equalizzate; il banco usa il
//        max-log sulle ampiezze, quindi per FT2 e' un'approssimazione.
//
// Tempo e frequenza sono ideali e non c'e' l'interferenza fra simboli del
// GFSK. Una trama e' decodificata se almeno un insieme restituisce la parola
// trasmessa; ha una falsa decodifica se almeno un insieme restituisce un'altra
// parola. E' cosi' che conta il decoder vero, che prova gli insiemi uno dopo
// l'altro: i falsi per trama sono piu' di quelli per parola.
struct Catena
{
  bool ft8;
  int M, bps, nsimb, nset;
  std::vector<int> dato;        // per simbolo: indice del primo bit, -1 = sincronismo
  std::vector<int> tono_sync;
  std::mt19937_64 rng;
  std::normal_distribution<double> nrm {0.0, 1.0};

  Catena (bool ft8_, uint64_t seed) : ft8 (ft8_), rng (seed)
  {
    static int const costas[7] = {3, 1, 4, 0, 6, 5, 2};
    M = ft8 ? 8 : 4; bps = ft8 ? 3 : 2; nsimb = ft8 ? 79 : 103; nset = ft8 ? 5 : 3;
    dato.assign (static_cast<size_t> (nsimb), -1);
    tono_sync.assign (static_cast<size_t> (nsimb), 0);
    if (ft8)
      {
        for (int k = 0; k < 7; ++k)
          tono_sync[static_cast<size_t> (k)] = tono_sync[static_cast<size_t> (36 + k)]
              = tono_sync[static_cast<size_t> (72 + k)] = costas[k];
        for (int k = 0; k < 29; ++k)
          {
            dato[static_cast<size_t> (7 + k)] = 3 * k;
            dato[static_cast<size_t> (43 + k)] = 87 + 3 * k;
          }
      }
    else
      {
        for (int blocco = 0; blocco < 3; ++blocco)
          for (int k = 0; k < 29; ++k)
            dato[static_cast<size_t> (4 + 33 * blocco + k)] = 58 * blocco + 2 * k;
        for (int s = 0; s < nsimb; ++s)
          if (dato[static_cast<size_t> (s)] < 0) tono_sync[static_cast<size_t> (s)] = s & 3;
      }
  }

  // out: nset * 174 LLR nella convenzione di Decodium (positivo = bit 1).
  void llr (uint8_t const* cw, double ebn0_db, float* out)
  {
    static int const gray8[8] = {0, 1, 3, 2, 5, 6, 4, 7};
    static int const gray4[4] = {0, 1, 3, 2};
    int const* gray = ft8 ? gray8 : gray4;
    double const amp = std::sqrt (std::pow (10.0, ebn0_db / 10.0) * bps * 91.0 / 174.0);
    double const sd = std::sqrt (0.5);
    double const fase = 6.283185307179586 * (static_cast<double> (rng () >> 11) / 9007199254740992.0);
    double const cr = amp * std::cos (fase), ci = amp * std::sin (fase);
    std::vector<double> yr (static_cast<size_t> (nsimb * M)), yi (static_cast<size_t> (nsimb * M));
    for (int s = 0; s < nsimb; ++s)
      {
        for (int t = 0; t < M; ++t)
          {
            yr[static_cast<size_t> (s * M + t)] = sd * nrm (rng);
            yi[static_cast<size_t> (s * M + t)] = sd * nrm (rng);
          }
        if (!cw) continue;
        int tono = tono_sync[static_cast<size_t> (s)];
        if (dato[static_cast<size_t> (s)] >= 0)
          {
            int v = 0;
            for (int b = 0; b < bps; ++b) v = (v << 1) | cw[dato[static_cast<size_t> (s)] + b];
            tono = gray[v];
          }
        yr[static_cast<size_t> (s * M + tono)] += cr;
        yi[static_cast<size_t> (s * M + tono)] += ci;
      }
    int const nrighe = ft8 ? kN : 2 * nsimb;
    std::vector<float> bm (static_cast<size_t> (5 * nrighe), 0.0f);
    static int const gruppi_ft8[3] = {1, 2, 3}, gruppi_ft2[3] = {1, 2, 4};
    std::vector<double> A;
    for (int g = 0; g < 3; ++g)
      {
        int const nsym = ft8 ? gruppi_ft8[g] : gruppi_ft2[g];
        int const nbits = bps * nsym, nt = 1 << nbits;
        A.resize (static_cast<size_t> (nt));
        // inizio (simbolo) e prima riga di ogni gruppo
        std::vector<std::pair<int, int>> inizi;
        if (ft8)
          {
            for (int meta = 0; meta < 2; ++meta)
              for (int k = 0; k < 29; k += nsym)
                inizi.emplace_back ((meta ? 43 : 7) + k, 87 * meta + 3 * k);
          }
        else
          {
            for (int ks = 0; ks + nsym <= nsimb; ks += nsym) inizi.emplace_back (ks, 2 * ks);
          }
        for (auto const& in : inizi)
          {
            for (int i = 0; i < nt; ++i)
              {
                double sr = 0, si = 0;
                for (int l = 0; l < nsym; ++l)
                  {
                    int const d = (i >> (bps * (nsym - 1 - l))) & (M - 1);
                    sr += yr[static_cast<size_t> ((in.first + l) * M + gray[d])];
                    si += yi[static_cast<size_t> ((in.first + l) * M + gray[d])];
                  }
                A[static_cast<size_t> (i)] = std::sqrt (sr * sr + si * si);
              }
            for (int ib = 0; ib < nbits; ++ib)
              {
                int const riga = in.second + ib;
                if (riga >= nrighe) continue;
                int const bit = nbits - 1 - ib;
                double m0 = -1.0, m1 = -1.0;
                for (int i = 0; i < nt; ++i)
                  {
                    if ((i >> bit) & 1) m1 = std::max (m1, A[static_cast<size_t> (i)]);
                    else m0 = std::max (m0, A[static_cast<size_t> (i)]);
                  }
                bm[static_cast<size_t> (g * nrighe + riga)] = static_cast<float> (m1 - m0);
                if (ft8 && nsym == 1)
                  {
                    double const den = std::max (m0, m1);
                    bm[static_cast<size_t> (3 * nrighe + riga)]
                        = den > 0 ? static_cast<float> ((m1 - m0) / den) : 0.0f;
                  }
              }
          }
      }
    if (ft8)
      for (int v = 0; v < kN; ++v)
        {
          float best = bm[static_cast<size_t> (v)];
          for (int g = 1; g < 3; ++g)
            if (std::fabs (bm[static_cast<size_t> (g * nrighe + v)]) > std::fabs (best))
              best = bm[static_cast<size_t> (g * nrighe + v)];
          bm[static_cast<size_t> (4 * nrighe + v)] = best;
        }
    for (int g = 0; g < nset; ++g)
      {
        float const* r = &bm[static_cast<size_t> (g * nrighe)];
        double s1 = 0, s2 = 0;
        for (int v = 0; v < nrighe; ++v) { s1 += r[v]; s2 += static_cast<double> (r[v]) * r[v]; }
        double const mean = ft8 ? 0.0 : s1 / nrighe, var = s2 / nrighe - mean * mean;
        double const sig = var > 0 ? std::sqrt (var) : std::sqrt (s2 / nrighe);
        float* o = out + static_cast<size_t> (g) * kN;
        if (ft8)
          for (int v = 0; v < kN; ++v) o[v] = static_cast<float> (2.83 * r[v] / sig);
        else
          for (int s = 0; s < nsimb; ++s)
            {
              int const d = dato[static_cast<size_t> (s)];
              if (d < 0) continue;
              for (int b = 0; b < 2; ++b) o[d + b] = static_cast<float> (2.83 * r[2 * s + b] / sig);
            }
      }
  }
};

// chan=chain senza dump=: le stesse trame attraverso il bridge, come
// run_bridge. Qui "parole" sono trame: ok se almeno un insieme di metriche
// restituisce la parola trasmessa, falsa se almeno uno ne restituisce un'altra
// (parole_false conta invece le parole sbagliate, una per insieme).
Tally run_bridge_chain (Options const& o, uint64_t seed, bool segnale, double ebn0, int n)
{
  constexpr int kBlock = 16;
  Catena cat (o.ft8, seed);
  Generator gen (o.ft8, true, seed ^ 0x5DEECE66Dull);
  int const ns = cat.nset;
  size_t const nw = static_cast<size_t> (kBlock) * static_cast<size_t> (ns);
  std::vector<float> llr (nw * kN);
  std::vector<signed char> apmask (nw * kN, 0), msg (nw * 91), cw (nw * kN);
  std::vector<int> ntype (nw), nhard (nw);
  std::vector<float> dmin (nw);
  std::vector<uint8_t> truth (static_cast<size_t> (kBlock) * kN);
  uint8_t m77[77];
  Tally t;
  superldpc_simd_set_ft8_mode_c (o.ft8 ? 1 : 0);
  for (int off = 0; off < n; off += kBlock)
    {
      int const cnt = std::min (kBlock, n - off);
      for (int b = 0; b < cnt; ++b)
        {
          uint8_t* tw = &truth[static_cast<size_t> (b) * kN];
          gen.message (m77);
          project_encode (m77, tw);
          cat.llr (segnale ? tw : nullptr, ebn0, &llr[static_cast<size_t> (b * ns) * kN]);
        }
      auto const t0 = std::chrono::steady_clock::now ();
      superldpc_simd_decode174_91_batch_c (cnt * ns, llr.data (), apmask.data (), 91, 3,
                                           o.ft8 ? 2 : 3, msg.data (), cw.data (),
                                           ntype.data (), nhard.data (), dmin.data ());
      t.sec += std::chrono::duration<double> (std::chrono::steady_clock::now () - t0).count ();
      for (int b = 0; b < cnt; ++b)
        {
          ++t.words;
          bool ok = false, falsa = false;
          for (int g = 0; g < ns; ++g)
            {
              size_t const w = static_cast<size_t> (b * ns + g);
              if (ntype[w] == 0) continue;
              bool uguale = segnale;
              for (int v = 0; v < kN && uguale; ++v)
                uguale = cw[w * kN + static_cast<size_t> (v)]
                         == static_cast<signed char> (truth[static_cast<size_t> (b) * kN + static_cast<size_t> (v)]);
              if (uguale) ok = true; else { falsa = true; ++t.parole_false; }
            }
          t.ok += ok; t.falsi += falsa;
        }
    }
  return t;
}

// chan=chain: ogni trama passa, per ogni valore di clips=, da un decoder con il
// preset di esercizio e i gate APERTI (nd fino a 0,25; soglia del gate appreso
// tolta), e si scrive una riga per ogni parola accettata:
//   C seme S|N ebn0 trama insieme clip via vero nd nhard logit
// (via: 0 = chiusa dal min-sum, sempre accettata; 1 = OSD). Le soglie -- nd in
// FT2, logit in FT8, piu' i bit ribaltati -- si applicano fuori linea, cosi'
// lo stesso file serve a contare decodifiche e false decodifiche a qualsiasi
// soglia. Sul rumore le righe con nd > dumpnd o con logit < dumplogit si
// saltano. In coda a ogni punto:
//   T seme S|N ebn0 trame clip us_per_trama
void run_chain (Options const& o, Code const& code, FILE* f)
{
  signed char rv[77];
  ftx_ft2_rvec_c (rv);
  static uint8_t rvec[77];
  for (int i = 0; i < 77; ++i) rvec[i] = static_cast<uint8_t> (rv[i] & 1);
  std::vector<Direct*> dec;
  for (double clip : o.clips)
    {
      Ft2Config c = shipped_config (o.ft8, rvec);
      c.llr_clip = static_cast<float> (clip);
      c.nd_max = 0.25f;
      dec.push_back (new Direct (code, c, o.ft8));
    }
  Generator msg (o.ft8, true, 1);
  uint8_t m77[77], truth[kN], bits[kN];
  signed char am[kN];
  std::memset (am, 0, sizeof am);
  std::vector<float> llr (static_cast<size_t> (5) * kN);
  for (int seed : o.seeds)
    {
      for (size_t p = 0; p <= o.ebn0.size (); ++p)
        {
          bool const segnale = p < o.ebn0.size ();
          double const ebn0 = segnale ? o.ebn0[p] : 0.0;
          int const n = segnale ? o.n : o.noise;
          uint64_t const s = static_cast<uint64_t> (seed) * 1000 + (segnale ? p + 1 : 0);
          Catena cat (o.ft8, s);
          msg.rng.seed (s ^ 0x5DEECE66Dull);
          std::vector<double> sec (o.clips.size (), 0.0);
          for (int i = 0; i < n; ++i)
            {
              msg.message (m77);
              project_encode (m77, truth);
              cat.llr (segnale ? truth : nullptr, ebn0, llr.data ());
              for (size_t k = 0; k < o.clips.size (); ++k)
                {
                  auto const t0 = std::chrono::steady_clock::now ();
                  for (int g = 0; g < cat.nset; ++g)
                    {
                      int nhard = 0; float nd = 0; bool bp = false;
                      gate_threshold_delta () = -1e9f;
                      if (!dec[k]->decode (&llr[static_cast<size_t> (g) * kN], am, bits, &nhard, &nd, &bp))
                        continue;
                      bool const vero = segnale && std::memcmp (bits, truth, kN) == 0;
                      // il logit esiste solo dove il gate appreso e' acceso (FT8)
                      double const logit = (bp || !o.ft8) ? 99.0
                          : static_cast<double> (gate_logit (dec[k]->dec.gate_features (0), true));
                      if (!segnale && !bp
                          && (static_cast<double> (nd) > o.dumpnd || logit < o.dumplogit)) continue;
                      std::fprintf (f, "C %d %c %.2f %d %d %.2f %d %d %.5f %d %.4f\n", seed,
                                    segnale ? 'S' : 'N', ebn0, i, g, o.clips[k], bp ? 0 : 1,
                                    vero ? 1 : 0, static_cast<double> (nd), nhard, logit);
                    }
                  sec[k] += std::chrono::duration<double> (std::chrono::steady_clock::now () - t0).count ();
                }
            }
          for (size_t k = 0; k < o.clips.size (); ++k)
            std::fprintf (f, "T %d %c %.2f %d %.2f %.1f\n", seed, segnale ? 'S' : 'N', ebn0, n,
                          o.clips[k], 1e6 * sec[k] / n);
          std::fflush (f);
        }
    }
}


bool check (bool cond, char const* cosa)
{
  if (!cond) std::fprintf (stderr, "selftest FALLITO: %s\n", cosa);
  return cond;
}

int selftest (Code const& code)
{
  bool ok = true;
  OsdFast osd (code, 2, 64, 0);
  std::mt19937_64 rng (20261117);
  uint8_t m77[77], cw[kN];

  // 1) le parole del codificatore del progetto soddisfano le 83 + 14 righe.
  long violazioni = 0, crc_male = 0;
  for (int trial = 0; trial < 5000; ++trial)
    {
      for (int i = 0; i < 77; ++i) m77[i] = static_cast<uint8_t> (rng () & 1);
      project_encode (m77, cw);
      for (int m = 0; m < code.M; ++m)
        {
          int par = 0;
          for (int e = code.row_ptr[static_cast<size_t> (m)]; e < code.row_ptr[static_cast<size_t> (m) + 1]; ++e)
            par ^= cw[code.col_idx[static_cast<size_t> (e)]];
          violazioni += par;
        }
      for (int j = 0; j < OsdFast::NCRCB; ++j)
        {
          int par = 0;
          for (int v = 0; v < kN; ++v) if (osd.crc_row_bit (j, v)) par ^= cw[v];
          violazioni += par;
        }
      crc_male += !crc14_ok (cw);
      // 2) un bit ribaltato fra 0 e 90 viola almeno una riga della CRC.
      int const v = static_cast<int> (rng () % 91);
      cw[v] ^= 1;
      int viola = 0;
      for (int j = 0; j < OsdFast::NCRCB; ++j)
        {
          int par = 0;
          for (int u = 0; u < kN; ++u) if (osd.crc_row_bit (j, u)) par ^= cw[u];
          viola += par;
        }
      ok &= check (viola > 0 && !crc14_ok (cw), "bit ribaltato non visto dalle righe CRC");
    }
  ok &= check (violazioni == 0, "parola valida che viola una riga di H o della CRC");
  ok &= check (crc_male == 0, "crc14_syn non concorda con la crc14 del progetto");
  for (int j = 0; j < OsdFast::NCRCB; ++j)
    for (int v = 91; v < kN; ++v)
      ok &= check (!osd.crc_row_bit (j, v), "riga CRC che tocca un bit di parita'");

  // 3) il risultato della ricerca (174,77) passa sempre la CRC ed e' una
  //    parola di codice; con il vincolo AP non contraddice i bit noti.
  osd.crc_in_base = true; osd.nd_max = 1.0f; osd.pair_search = true; osd.ntau = 13;
  osd.pair_span = 64;
  long prove = 0, trovate = 0, con_secondo = 0;
  for (int caso = 0; caso < 6; ++caso)
    {
      bool const segnale = caso % 2 == 0;
      int const nap = caso < 2 ? 0 : (caso < 4 ? 32 : 61);
      osd.ap_vincolo = nap > 0;
      Generator gen (true, caso % 3 == 0, 7000 + static_cast<uint64_t> (caso));
      for (int trial = 0; trial < 3000; ++trial)
        {
          float llr_dec[kN], llr[kN];
          int16_t L[kN];
          signed char am[kN];
          uint8_t mask[kN], out[kN];
          gen.message (m77);
          project_encode (m77, cw);
          gen.llr (segnale ? cw : nullptr, caso % 3 == 0 ? 3.0 : 1.0, llr_dec);
          ap_mask (nap, am);
          if (nap > 0) ap_apply (am, cw, llr_dec);
          for (int v = 0; v < kN; ++v)
            {
              mask[v] = am[v] != 0;
              llr[v] = -llr_dec[v];
              float x = llr[v] * 8.0f;
              x = std::max (-2047.0f, std::min (2047.0f, x));
              L[v] = static_cast<int16_t> (std::lrint (x));
            }
          float nd = 0;
          ++prove;
          if (!osd.decode (L, out, llr, &nd, nap > 0 ? mask : nullptr)) continue;
          ++trovate;
          con_secondo += osd.last_has_second ();
          ok &= check (crc14_ok (out), "risultato (174,77) che non passa crc14_ok");
          int crc_campo = 0;
          for (int i = 0; i < 14; ++i) crc_campo = (crc_campo << 1) | out[77 + i];
          ok &= check (project_crc14 (out) == crc_campo, "risultato (174,77) che non passa la crc14 del progetto");
          int par = 0;
          for (int m = 0; m < code.M; ++m)
            {
              int q = 0;
              for (int e = code.row_ptr[static_cast<size_t> (m)]; e < code.row_ptr[static_cast<size_t> (m) + 1]; ++e)
                q ^= out[code.col_idx[static_cast<size_t> (e)]];
              par += q;
            }
          ok &= check (par == 0, "risultato (174,77) che non e' una parola di H");
          for (int v = 0; v < kN; ++v)
            if (mask[v]) ok &= check (out[v] == cw[v], "bit AP contraddetto dalla ricerca vincolata");
          ok &= check (!osd.last_has_second () || osd.last_nd2 () >= 0.0f, "nd2 negativa");
        }
    }
  ok &= check (trovate == prove, "la ricerca (174,77) non ha restituito un candidato");
  std::printf ("selftest: %ld ricerche (174,77), %ld con candidato, %ld con secondo candidato\n",
               prove, trovate, con_secondo);

  // 4) la copia del preset decide come il bridge, in entrambi i modi.
  for (int modo = 0; modo < 2; ++modo)
    {
      bool const ft8 = modo == 0;
      signed char rv[77];
      ftx_ft2_rvec_c (rv);
      static uint8_t rvec[77];
      for (int i = 0; i < 77; ++i) rvec[i] = static_cast<uint8_t> (rv[i] & 1);
      long diversi = 0, accettate = 0;
      std::thread th ([&] {
        Direct d (code, shipped_config (ft8, rvec), ft8);
        superldpc_simd_set_ft8_mode_c (ft8 ? 1 : 0);
        Generator gen (ft8, true, 99);
        for (int trial = 0; trial < 6000; ++trial)
          {
            float llr[kN];
            signed char am[kN], msg[91], cwb[kN];
            uint8_t bits[kN];
            int ntype = 0, nhard = 0;
            float dmin = 0;
            gen.message (m77);
            project_encode (m77, cw);
            gen.llr (trial % 3 ? cw : nullptr, (ft8 ? 2.5 : 4.0) + 0.5 * (trial % 4), llr);
            ap_mask (trial % 5 == 0 ? 32 : 0, am);
            if (trial % 5 == 0) ap_apply (am, cw, llr);
            int const a = d.decode (llr, am, bits, nullptr);
            superldpc_simd_decode174_91_batch_c (1, llr, am, 91, 3, 3, msg, cwb, &ntype, &nhard, &dmin);
            int const b = ntype != 0;
            accettate += b;
            if (a != b) { ++diversi; continue; }
            if (a) for (int v = 0; v < kN; ++v) if (bits[v] != static_cast<uint8_t> (cwb[v])) { ++diversi; break; }
          }
      });
      th.join ();
      std::printf ("selftest: preset %s, copia contro bridge: %ld accettate, %ld differenze\n",
                   ft8 ? "FT8" : "FT2", accettate, diversi);
      ok &= check (diversi == 0, "la copia del preset non decide come il bridge");
    }
  std::printf ("selftest: %s\n", ok ? "OK" : "FALLITO");
  return ok ? 0 : 1;
}

}  // namespace

int main (int argc, char** argv)
{
  Options o;
  for (int i = 1; i < argc; ++i)
    {
      std::string const a (argv[i]);
      auto const p = a.find ('=');
      if (p == std::string::npos) { std::fprintf (stderr, "argomento non valido: %s\n", argv[i]); return 2; }
      std::string const k = a.substr (0, p), v = a.substr (p + 1);
      if (k == "mode") o.ft8 = v != "ft2";
      else if (k == "chan") { o.fsk = v != "bpsk"; o.chain = v == "chain"; }
      else if (k == "clips") { o.clips.clear (); for (auto const& s : split (v)) o.clips.push_back (std::atof (s.c_str ())); }
      else if (k == "ebn0") { o.ebn0.clear (); for (auto const& s : split (v)) o.ebn0.push_back (std::atof (s.c_str ())); }
      else if (k == "seeds") { o.seeds.clear (); for (auto const& s : split (v)) o.seeds.push_back (std::atoi (s.c_str ())); }
      else if (k == "n") o.n = std::atoi (v.c_str ());
      else if (k == "noise") o.noise = std::atoi (v.c_str ());
      else if (k == "ap") o.ap = std::atoi (v.c_str ());
      else if (k == "apwrong") o.apwrong = v != "0";
      else if (k == "selftest") o.selftest = v != "0";
      else if (k == "dump") o.dump = v;
      else if (k == "dumpnd") o.dumpnd = std::atof (v.c_str ());
      else if (k == "dumplogit") o.dumplogit = std::atof (v.c_str ());
      else if (k == "variants") o.variants = v;
      else { std::fprintf (stderr, "argomento sconosciuto: %s\n", k.c_str ()); return 2; }
    }
  Code const code = project_code ();
  if (o.selftest) return selftest (code);
  if (!o.dump.empty ())
    {
      FILE* f = std::fopen (o.dump.c_str (), "w");
      if (!f) { std::fprintf (stderr, "non posso scrivere %s\n", o.dump.c_str ()); return 2; }
      if (o.chain) run_chain (o, code, f);
      else run_dump (o, code, f);
      std::fclose (f);
      return 0;
    }

  if (o.chain && o.ap > 0) { std::fprintf (stderr, "chan=chain non ha i bit AP\n"); return 2; }
  std::printf ("# mode=%s chan=%s ap=%d%s n=%d noise=%d per seme, Eb/N0 riferito a 91 bit\n",
               o.ft8 ? "ft8" : "ft2",
               o.chain ? "catena (trame)" : (o.fsk ? (o.ft8 ? "8fsk" : "4fsk") : "bpsk"), o.ap,
               o.apwrong ? " (ipotesi AP sbagliata)" : "", o.n, o.noise);
  std::printf ("# variante seme punto parole ok falsi FER us/parola\n");
  std::map<std::string, std::map<std::string, Tally>> somma;
  std::vector<std::string> ordine;
  for (std::string const& variante : split (o.variants))
    {
      // Le variabili si leggono alla costruzione del decoder, che e' per
      // thread: una variante = un thread nuovo.
      // "clipX" e' la base con DECODIUM_LDPC_LLR_CLIP=X (clip0 = senza taglio).
      bool const taglio = variante.rfind ("clip", 0) == 0;
      imposta_env ("DECODIUM_LDPC_LLR_CLIP", taglio ? variante.c_str () + 4 : nullptr);
      bool const nuovo = variante != "base" && !taglio;
      // "k77" e' la ricerca nuova con i gate di prima (DECODIUM_LDPC_K77_NOGATE).
      imposta_env ("DECODIUM_LDPC_K77", nuovo ? "1" : "0");
      imposta_env ("DECODIUM_LDPC_K77_NOGATE", variante == "k77" ? "1" : "0");
      std::thread th ([&] {
        for (int seed : o.seeds)
          {
            for (size_t p = 0; p <= o.ebn0.size (); ++p)
              {
                bool const segnale = p < o.ebn0.size ();
                double const ebn0 = segnale ? o.ebn0[p] : 0.0;
                char nome[32];
                if (segnale) std::snprintf (nome, sizeof nome, "%.2fdB", ebn0);
                else std::snprintf (nome, sizeof nome, "rumore");
                uint64_t const sp = static_cast<uint64_t> (seed) * 1000 + (segnale ? p + 1 : 0);
                Tally const t = o.chain ? run_bridge_chain (o, sp, segnale, ebn0, segnale ? o.n : o.noise)
                                        : run_bridge (o, sp, segnale, ebn0, segnale ? o.n : o.noise);
                std::printf ("%-5s %4d %-8s %7ld %7ld %5ld %.4f %6.1f\n", variante.c_str (), seed, nome,
                             t.words, t.ok, t.falsi,
                             segnale ? 1.0 - static_cast<double> (t.ok) / static_cast<double> (t.words) : 0.0,
                             1e6 * t.sec / static_cast<double> (t.words));
                std::fflush (stdout);
                Tally& s = somma[nome][variante];
                s.words += t.words; s.ok += t.ok; s.falsi += t.falsi; s.sec += t.sec;
                s.parole_false += t.parole_false;
                if (variante == split (o.variants)[0] && seed == o.seeds[0]) ordine.push_back (nome);
              }
          }
      });
      th.join ();
    }
  std::printf ("# TOTALE su %zu semi\n", o.seeds.size ());
  for (std::string const& nome : ordine)
    for (std::string const& variante : split (o.variants))
      {
        Tally const& s = somma[nome][variante];
        std::printf ("T %-5s %-8s parole=%ld ok=%ld falsi=%ld FER=%.4f us/parola=%.1f",
                     variante.c_str (), nome.c_str (), s.words, s.ok, s.falsi,
                     nome == "rumore" ? 0.0 : 1.0 - static_cast<double> (s.ok) / static_cast<double> (s.words),
                     1e6 * s.sec / static_cast<double> (s.words));
        if (o.chain) std::printf (" parole_false=%ld", s.parole_false);
        std::printf ("\n");
      }
  return 0;
}
