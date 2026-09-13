// ft8_bicm_genie.cpp — demodulazione iterativa in FT8: genio, anello vero e
// a priori dell'AP.
//
// IL GENIO ERA IL PRIMO PASSO. Su FT2 l'anello completo (tests/ft2_bicm_id.cpp)
// e' stato implementato e misurato: +6,3% di decodifiche, mentre il genio dava
// +34,5%. L'iterazione cattura il 18% di quello che c'e', perche' l'estrinseca
// la si ottiene solo da un decodificatore che ha gia' fallito. Il genio su FT8,
// dove il divario dell'informazione mutua e' il doppio (1,63 dB su terne contro
// 1,08 di FT2), ha dato 3,8x le decodifiche: il massimo qui e' molto piu' alto.
//
// ORA I DUE BRACCI CHE MANCAVANO (10/9/2026):
//
//   bicmN  L'ANELLO VERO, che su FT8 non era mai stato chiuso: dopo un
//          tentativo fallito si prende l'estrinseca dai posteriori del min-sum
//          e la si rimanda al demodulatore come a priori MORBIDA sugli altri
//          bit del simbolo. Dice quanto dei 3,8x si prende davvero, invece di
//          stimarlo per analogia col 18% di FT2.
//
//   ap     L'a priori dell'AP, che e' informazione VERA e gia' disponibile.
//          Stage4 costruisce apmask/llrz per le ipotesi note (il proprio
//          nominativo, quello del corrispondente, "CQ") e li passa SOLO al
//          decodificatore LDPC: il demodulatore non li vede mai. Ma un bit
//          noto dimezza le ipotesi di tono anche per gli ALTRI due bit dello
//          stesso simbolo -- e' il genio, ristretto ai bit che l'AP conosce
//          per davvero, e costa UNA demodulazione, non due giri.
//          `--ap-bits K` fissa i primi K bit del messaggio (29 = "CQ" + flag,
//          58 = i due nominativi: sono i casi veri di iaptype).
//
// IL GENIO IN MAX-LOG. FT8 demodula con max-log su 8 ipotesi di tono, 3 bit
// per simbolo:
//     llr(j) = max_{i: bit_j(i)=1} s2[i] - max_{i: bit_j(i)=0} s2[i]
// Con i due ALTRI bit del simbolo noti, restano esattamente due ipotesi
// compatibili, una per valore di bit_j: la metrica diventa la differenza fra
// quelle due sole. E' il limite superiore di qualunque schema iterativo.
//
// La mappatura e' quella di run_ft8_bitmetrics: ihalf = c/87, k = (c%87)/3,
// ib = c%3, simbolo ks = k+8 nella prima meta' e k+44 nella seconda (i due
// blocchi Costas stanno in mezzo), bit dell'ipotesi = 2-ib, tono = graymap.
//
// Il banco verifica per prima cosa che a priori nulla riproduca la llra di
// produzione: senza quella verifica i numeri non vogliono dire niente.
// (Su FT2 quella verifica ha trovato tre errori veri.)
//
// Uso:
//   ft8_bicm_genie --verifica
//   ft8_bicm_genie --snr-list "-21,-20,-19,-18" --seeds 25
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

#include <fftw3.h>

#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"
#include "Detector/fastldpc/ft2_decoder.hpp"

extern "C"
{
  void ftx_sync8_search_stage4_c (float const* dd, int npts, float nfa, float nfb,
                                  float syncmin, float nfqso, int maxcand, int ipass,
                                  int candthin, float* candidates, int* ncand, float* sbase);
  void ftx_ft8_downsample_c (float const* dd, int* newdat, float f0, fftwf_complex* c1);
  void ftx_ft8_a7_search_initial_c (std::complex<float> const* cd0, int np2, float fs2,
                                    float xdt_in, int* ibest_out, float* delfbest_out);
  void ftx_ft8_a7_refine_search_c (std::complex<float> const* cd0, int np2, float fs2,
                                   int ibest_in, int* ibest_out, float* sync_out,
                                   float* xdt_out);
  void ftx_ft8_bitmetrics_capture_c (std::complex<float> const* cd0, int np2, int ibest,
                                     int imetric, float scale, int weak_deep,
                                     int equalize_tone_power,
                                     std::complex<float> const* history_cs,
                                     std::complex<float>* current_cs_out,
                                     float* s8_out, int* nsync_out,
                                     float* llra, float* llrb, float* llrc,
                                     float* llrd, float* llre);
  void ftx_ldpc174_91_tables_c (int* Mn_out, int* Nm_out, int* nrw_out, int* ncw_out);
  int ftx_encode174_91_message77_c (signed char const* message77, signed char* codeword_out);
  // la versione INNESTATA in Stage4, quella che va in aria
  void ftx_ft8_bitmetrics_bicm_c (std::complex<float> const* cs, float scale,
                                  float const* la, float* llra, float* llrb,
                                  float* llrc, float* llrd, float* llre);
}

namespace {

using Complex = std::complex<float>;

constexpr int kSampleRate {12000};
constexpr int kFrameSamples {180000};     // 15 s
constexpr int kNsps {1920};
constexpr float kBt {2.0f};
constexpr int kNp2 {3200};
constexpr float kFs2 {200.0f};
constexpr int kCodeword {174};
constexpr int kBits77 {77};
constexpr int kSyms {79};
constexpr float kScale {2.83f};           // kFt8BitMetricScale
constexpr int kMaxCand {300};
constexpr int kSbase {1800};
constexpr int kGraymap[8] = {0, 1, 3, 2, 5, 6, 4, 7};

[[noreturn]] void fail (QString const& m) { throw std::runtime_error {m.toStdString ()}; }

struct Posto { int simbolo; int bit_ipotesi; };

// Dove vive il bit c del codeword: quale simbolo e quale bit dell'ipotesi.
Posto posto_di (int c)
{
  int const ihalf = c / 87;                 // 0 o 1
  int const dentro = c % 87;
  int const k = dentro / 3 + 1;             // 1..29
  int const ib = dentro % 3;
  int const ks = (ihalf == 0) ? (k + 7) : (k + 43);   // 1-based
  return Posto {ks - 1, 2 - ib};            // one[i][ibmax-ib] con ibmax=2
}

// Demodulatore FT8 a un simbolo (ramo nseq=1 di run_ft8_bitmetrics: max-log su
// 8 ipotesi, metrica = |cs| del tono), con informazione a priori sugli ALTRI
// due bit dello stesso simbolo. Il bit che si sta calcolando non entra mai nel
// proprio a priori: l'uscita resta ESTRINSECA, cioe' notizia nuova per il
// decodificatore e non l'eco della sua stessa opinione.
//
//   veri      bit veri del codeword; con `maschera` nulla fa il GENIO pieno
//             (restringe a due le ipotesi compatibili), con `maschera` fa il
//             genio ristretto ai bit segnati, che e' quello che l'AP sa.
//   la        a priori MORBIDA in LLR (convenzione Decodium: positivo = bit 1),
//             usata sui bit che `maschera` non copre.
//   conv      porta un LLR nelle unita' della metrica |cs|: e' sigma/kScale
//             della passata senza a priori, perche' la produzione emette
//             llr = kScale * (max1-max0) / sigma. Senza questa conversione
//             l'a priori peserebbe a caso.
//   sigma_out se non nullo riceve la sigma di questa passata.
void demodula (Complex const* cs, uint8_t const* veri, uint8_t const* maschera,
               float const* la, float conv, float* llr_out, double* sigma_out = nullptr)
{
  std::array<float, kCodeword> m {};
  for (int c = 0; c < kCodeword; ++c)
    {
      Posto const p = posto_di (c);
      float s2[8];
      for (int i = 0; i < 8; ++i)
        s2[i] = std::abs (cs[p.simbolo * 8 + kGraymap[i]]);

      int const base = c - (2 - p.bit_ipotesi);   // primo bit del simbolo
      int maschera_ip = 0, valore = 0;            // a priori dura
      float peso[8] = {};                         // a priori morbida
      for (int ib = 0; ib < 3; ++ib)
        {
          int const bi = 2 - ib;
          if (bi == p.bit_ipotesi) continue;      // mai il proprio bit
          int const c_altro = base + ib;
          if (veri && (!maschera || maschera[static_cast<size_t> (c_altro)]))
            {
              maschera_ip |= (1 << bi);
              if (veri[static_cast<size_t> (c_altro)]) valore |= (1 << bi);
            }
          else if (la)
            {
              float const a = conv * la[c_altro];
              for (int i = 0; i < 8; ++i)
                if ((i & (1 << bi)) != 0) peso[i] += a;
            }
        }

      float max1 = -1.0e30f, max0 = -1.0e30f;
      for (int i = 0; i < 8; ++i)
        {
          if (maschera_ip && ((i & maschera_ip) != valore)) continue;
          float const w = s2[i] + peso[i];
          if ((i & (1 << p.bit_ipotesi)) != 0) max1 = std::max (max1, w);
          else max0 = std::max (max0, w);
        }
      m[static_cast<size_t> (c)] = max1 - max0;
    }

  // normalizebmet_rms_cpp sui 174, poi la scala di produzione
  double s = 0.0;
  for (int i = 0; i < kCodeword; ++i) s += static_cast<double> (m[static_cast<size_t> (i)]) * m[static_cast<size_t> (i)];
  double sigma = std::sqrt (std::max (s / kCodeword, 0.0));
  if (sigma <= 0.0) sigma = 1.0;
  if (sigma_out) *sigma_out = sigma;
  for (int i = 0; i < kCodeword; ++i)
    llr_out[i] = kScale * static_cast<float> (m[static_cast<size_t> (i)] / sigma);
}

// Demodulatore COERENTE per FT8: la fase del canale stimata dai 21 simboli
// Costas (tre gruppi da 7 ai simboli k, k+36, k+72, toni {3,1,4,0,6,5,2}),
// invece di essere ignorata.
//
// Stessa idea della passata coerente gia' portata in Stage7 per FT2, ma qui il
// tetto e' piu' alto: dall'informazione mutua, FT8 a terne sta a 3,96 contro
// 2,69 del limite coerente, cioe' 1,27 dB contro l'1,1 di FT2. E in FT8 la
// prova in aria e' possibile, perche' la propagazione c'e'.
//
// Con la fase nota l'informazione sta nella parte reale: l'ipotesi si pesa
// sulla sua proiezione, che a differenza del modulo puo' essere NEGATIVA --
// ed e' proprio quella l'informazione in piu' (un tono in controfase e' meno
// probabile di uno a energia nulla).
void demodula_coerente_ft8 (Complex const* cs, float* llr_out)
{
  static int const icos7[7] = {3, 1, 4, 0, 6, 5, 2};
  static int const gruppo[3] = {0, 36, 72};

  Complex ancora[3];
  float centro[3];
  for (int g = 0; g < 3; ++g)
    {
      Complex somma {};
      for (int k = 0; k < 7; ++k)
        somma += cs[(gruppo[g] + k) * 8 + icos7[k]];
      float const mag = std::abs (somma);
      ancora[g] = mag > 0.0f ? somma / mag : Complex {1.0f, 0.0f};
      centro[g] = static_cast<float> (gruppo[g]) + 3.0f;
    }

  std::array<float, kCodeword> m {};
  for (int c = 0; c < kCodeword; ++c)
    {
      Posto const p = posto_di (c);
      float const x = static_cast<float> (p.simbolo);
      Complex rif;
      if (x <= centro[0]) rif = ancora[0];
      else if (x >= centro[2]) rif = ancora[2];
      else
        {
          int const g = (x > centro[1]) ? 1 : 0;
          float const t = (x - centro[g]) / (centro[g + 1] - centro[g]);
          rif = ancora[g] * (1.0f - t) + ancora[g + 1] * t;
        }
      float const rmag = std::abs (rif);
      Complex const rot = rmag > 0.0f ? std::conj (rif / rmag) : Complex {1.0f, 0.0f};

      float s2[8];
      for (int i = 0; i < 8; ++i)
        s2[i] = std::real (cs[p.simbolo * 8 + kGraymap[i]] * rot);

      float max1 = -1.0e30f, max0 = -1.0e30f;
      for (int i = 0; i < 8; ++i)
        {
          if ((i & (1 << p.bit_ipotesi)) != 0) max1 = std::max (max1, s2[i]);
          else max0 = std::max (max0, s2[i]);
        }
      m[static_cast<size_t> (c)] = max1 - max0;
    }

  double s = 0.0;
  for (int i = 0; i < kCodeword; ++i) s += static_cast<double> (m[static_cast<size_t> (i)]) * m[static_cast<size_t> (i)];
  double sigma = std::sqrt (std::max (s / kCodeword, 0.0));
  if (sigma <= 0.0) sigma = 1.0;
  for (int i = 0; i < kCodeword; ++i)
    llr_out[i] = kScale * static_cast<float> (m[static_cast<size_t> (i)] / sigma);
}

Code const& codice ()
{
  static Code c = [] {
    std::vector<int> mn (3 * kCodeword), nm (7 * 83), nrw (83);
    int ncw = 0;
    ftx_ldpc174_91_tables_c (mn.data (), nm.data (), nrw.data (), &ncw);
    Code k;
    k.M = 83; k.N = kCodeword;
    k.row_ptr.push_back (0);
    for (int r = 0; r < k.M; ++r)
      {
        for (int j = 0; j < nrw[static_cast<size_t> (r)]; ++j)
          k.col_idx.push_back (nm[static_cast<size_t> (j + 7 * r)] - 1);
        k.row_ptr.push_back (static_cast<int> (k.col_idx.size ()));
      }
    return k;
  }();
  return c;
}

// FT8 non mescola: la parola vera e' l'encode diretto dei 77 bit.
std::array<uint8_t, kCodeword> codeword_vero (QByteArray const& bits77)
{
  std::array<signed char, kBits77> msg {};
  for (int i = 0; i < kBits77; ++i) msg[static_cast<size_t> (i)] = static_cast<signed char> (bits77.at (i) != 0 ? 1 : 0);
  std::array<signed char, kCodeword> cw {};
  if (ftx_encode174_91_message77_c (msg.data (), cw.data ()) == 0)
    fail (QStringLiteral ("encode174_91 fallito"));
  std::array<uint8_t, kCodeword> out {};
  for (int i = 0; i < kCodeword; ++i) out[static_cast<size_t> (i)] = static_cast<uint8_t> (cw[static_cast<size_t> (i)] & 1);
  return out;
}

Ft2Config produzione_ft8 ()
{
  Ft2Config c = Ft2Decoder::conservativo ();
  c.osd_order = 2; c.span2 = 32; c.span3 = 0;
  c.pair_search = true; c.ntau = 14;
  c.nd_max = 0.065f;
  c.llr_clip = 2.5f;
  c.alpha_w = 37888;
  c.max_iter = 10;
  c.batch = 16;
  c.tipi_ammessi = plaus::kTuttiDefiniti;   // FT8: tutti i tipi definiti
  c.descramble77 = nullptr;                 // FT8 non mescola
  return c;
}

struct Cornice
{
  std::array<Complex, kSyms * 8> cs {};
  std::array<float, kCodeword> llra_prod {}, llrb_prod {}, llrc_prod {}, llrd_prod {}, llre_prod {};
  bool ok {false};
};

// Con `solo_rumore` la cornice non contiene alcun segnale: il sincronismo si
// aggancia dove capita, come farebbe su un candidato di rumore proposto dalla
// ricerca in aria, e QUALUNQUE parola accettata e' un fantasma per costruzione.
Cornice prepara (QString const& messaggio, float freq, float dt_s, double snr_db, unsigned seme,
                 bool solo_rumore = false)
{
  Cornice out;
  std::vector<float> dd (static_cast<size_t> (kFrameSamples), 0.0f);
  if (!solo_rumore)
    {
      decodium::txmsg::EncodedMessage const enc = decodium::txmsg::encodeFt8 (messaggio);
      if (!enc.ok || enc.tones.isEmpty ()) return out;
      QVector<float> const wave = decodium::txwave::generateFt8Wave (
          enc.tones.constData (), enc.tones.size (), kNsps, kBt, static_cast<float> (kSampleRate), freq);
      if (wave.isEmpty ()) return out;

      int const off = static_cast<int> (std::lround (static_cast<double> (dt_s) * kSampleRate));
      for (int i = 0; i < wave.size () && off + i < kFrameSamples; ++i)
        dd[static_cast<size_t> (off + i)] = 0.5f * wave[i];
    }

  double s2 = 0.0;
  for (float x : dd) s2 += static_cast<double> (x) * x;
  double const rms = std::sqrt (s2 / kFrameSamples);
  double const sigma = solo_rumore ? 0.05 : rms / std::pow (10.0, snr_db / 20.0);
  std::mt19937 rng {seme};
  std::normal_distribution<float> noise {0.0f, static_cast<float> (sigma)};
  for (float& x : dd) x += noise (rng);

  std::array<float, 4 * kMaxCand> cand {};
  std::array<float, kSbase> sbase {};
  int ncand = 0;
  ftx_sync8_search_stage4_c (dd.data (), kFrameSamples, 200.0f, 3000.0f, 1.2f, freq,
                             kMaxCand, 1, 100, cand.data (), &ncand, sbase.data ());
  if (ncand <= 0) return out;

  int best = -1;
  if (solo_rumore)
    {
      best = 0;                                  // il primo che la ricerca propone
    }
  else
    {
      // il candidato piu' vicino alla frequenza vera
      float bestd = 1.0e30f;
      for (int i = 0; i < ncand; ++i)
        {
          float const d = std::fabs (cand[static_cast<size_t> (i * 4)] - freq);
          if (d < bestd) { bestd = d; best = i; }
        }
      if (best < 0 || bestd > 20.0f) return out;
    }

  std::array<Complex, kNp2> cd0 {};
  int newdat = 1;
  float f1 = cand[static_cast<size_t> (best * 4)];
  float xdt = cand[static_cast<size_t> (best * 4 + 1)];
  ftx_ft8_downsample_c (dd.data (), &newdat, f1, reinterpret_cast<fftwf_complex*> (cd0.data ()));
  int ibest = 0; float delf = 0.0f;
  ftx_ft8_a7_search_initial_c (cd0.data (), kNp2, kFs2, xdt, &ibest, &delf);
  f1 += delf;
  int newdat2 = 0;
  ftx_ft8_downsample_c (dd.data (), &newdat2, f1, reinterpret_cast<fftwf_complex*> (cd0.data ()));
  float sy = 0.0f;
  ftx_ft8_a7_refine_search_c (cd0.data (), kNp2, kFs2, ibest, &ibest, &sy, &xdt);

  std::array<float, 8 * kSyms> s8 {};
  int nsync = 0;
  ftx_ft8_bitmetrics_capture_c (cd0.data (), kNp2, ibest, 0, kScale, 0, 0,
                                nullptr, out.cs.data (), s8.data (), &nsync,
                                out.llra_prod.data (), out.llrb_prod.data (),
                                out.llrc_prod.data (), out.llrd_prod.data (),
                                out.llre_prod.data ());
  out.ok = true;
  return out;
}

QList<double> lista (QString const& raw)
{
  QList<double> v;
  for (QString p : raw.split (QLatin1Char {','}, Qt::SkipEmptyParts))
    {
      bool ok = false;
      double const x = p.trimmed ().toDouble (&ok);
      if (ok) v.append (x);
    }
  return v;
}

}  // namespace

int main (int argc, char* argv[])
{
  try
    {
      QCoreApplication app {argc, argv};
      QCoreApplication::setApplicationName (QStringLiteral ("ft8_bicm_genie"));
      QCommandLineParser parser;
      parser.setApplicationDescription (
          QStringLiteral ("Demodulazione iterativa in FT8: genio, anello vero e a priori dell'AP."));
      parser.addHelpOption ();
      QCommandLineOption ver_opt {"verifica", "Controlla che a priori nulla riproduca la llra di produzione."};
      QCommandLineOption msg_opt {"message", "Messaggio FT8 (ripetibile).", "text"};
      QCommandLineOption snr_opt {"snr-list", "SNR in dB.", "list", "-22,-21,-20,-19"};
      QCommandLineOption seeds_opt {"seeds", "Semi per punto.", "n", "25"};
      QCommandLineOption freq_opt {"freq", "Frequenza audio.", "hz", "1500.0"};
      QCommandLineOption dt_opt {"dt", "Ritardo del burst in s.", "s", "0.5"};
      QCommandLineOption giri_opt {"giri", "Giri dell'anello BICM-ID.", "n", "2"};
      QCommandLineOption clamp_opt {"clamp", "Limite sull'estrinseca rimandata al demodulatore.", "v", "2.0"};
      QCommandLineOption damp_opt {"damp", "Smorzamento dell'estrinseca (1 = nessuno).", "v", "1.0"};
      QCommandLineOption ap_opt {"ap-bits", "Bit del messaggio noti all'AP (29 = CQ+flag, 58 = i due nominativi).", "n", "58"};
      QCommandLineOption rumore_opt {"rumore", "Cornici di solo rumore: conta i fantasmi del base e dell'anello."};
      parser.addOption (ver_opt); parser.addOption (msg_opt); parser.addOption (snr_opt);
      parser.addOption (seeds_opt); parser.addOption (freq_opt); parser.addOption (dt_opt);
      parser.addOption (giri_opt); parser.addOption (clamp_opt); parser.addOption (damp_opt);
      parser.addOption (ap_opt); parser.addOption (rumore_opt);
      parser.process (app);

      QStringList messaggi = parser.values (msg_opt);
      if (messaggi.isEmpty ())
        messaggi = {QStringLiteral ("CQ IU8LMC JN70"), QStringLiteral ("IU8LMC DL9XYZ -12"),
                    QStringLiteral ("DL9XYZ IU8LMC R-08"), QStringLiteral ("IU8LMC DL9XYZ 73")};
      bool ok = false;
      int const semi = parser.value (seeds_opt).toInt (&ok);
      float const freq = parser.value (freq_opt).toFloat (&ok);
      float const dt = parser.value (dt_opt).toFloat (&ok);
      int const giri = std::max (1, parser.value (giri_opt).toInt (&ok));
      float const clamp = parser.value (clamp_opt).toFloat (&ok);
      float const damp = parser.value (damp_opt).toFloat (&ok);
      int const ap_bits = std::max (0, std::min (kBits77, parser.value (ap_opt).toInt (&ok)));
      QTextStream out {stdout};

      // maschera dell'AP: i primi ap_bits bit del messaggio. Il codice e'
      // sistematico (77 bit di messaggio, 14 di CRC, 83 di parita'), quindi
      // sono i primi ap_bits indici del codeword -- la --verifica lo controlla.
      std::array<uint8_t, kCodeword> mask_ap {};
      for (int i = 0; i < ap_bits; ++i) mask_ap[static_cast<size_t> (i)] = 1;

      if (parser.isSet (ver_opt))
        {
          Cornice const c = prepara (messaggi.first (), freq, dt, -10.0, 1000000u);
          if (!c.ok) fail (QStringLiteral ("preparazione fallita (nessun candidato?)"));
          // il codice e' sistematico? i primi 77 bit del codeword devono essere
          // il messaggio, altrimenti la maschera dell'AP punta altrove.
          decodium::txmsg::EncodedMessage const enc = decodium::txmsg::encodeFt8 (messaggi.first ());
          if (enc.ok)
            {
              std::array<uint8_t, kCodeword> const cw = codeword_vero (enc.msgbits);
              int diversi = 0;
              for (int i = 0; i < kBits77; ++i)
                if (cw[static_cast<size_t> (i)] != (enc.msgbits.at (i) != 0 ? 1 : 0)) ++diversi;
              out << "codeword sistematico: " << (diversi == 0 ? "SI" : "NO")
                  << " (" << diversi << " bit diversi sui primi 77)\n";
              if (diversi != 0) return 1;
            }
          std::array<float, kCodeword> mio {};
          demodula (c.cs.data (), nullptr, nullptr, nullptr, 0.0f, mio.data ());
          double dmax = 0.0; int peggio = -1;
          for (int i = 0; i < kCodeword; ++i)
            {
              double const d = std::fabs (static_cast<double> (mio[static_cast<size_t> (i)] - c.llra_prod[static_cast<size_t> (i)]));
              if (d > dmax) { dmax = d; peggio = i; }
            }
          out << "scarto massimo " << dmax << " (bit " << peggio << ")\n";
          if (peggio >= 0)
            out << "  mio=" << mio[static_cast<size_t> (peggio)]
                << "  produzione=" << c.llra_prod[static_cast<size_t> (peggio)] << "\n";
          out << (dmax < 1e-3 ? "OK: demodulatore identico a quello di produzione\n"
                              : "DIVERSO: la mappatura non combacia\n");

          // La funzione INNESTATA in Stage4: con estrinseca nulla deve
          // riprodurre tutte e cinque le metriche di produzione, altrimenti
          // l'innesto cambia il comportamento anche da spento.
          std::array<float, kCodeword> zero {}, ba {}, bb {}, bc {}, bd {}, be {};
          ftx_ft8_bitmetrics_bicm_c (c.cs.data (), kScale, zero.data (),
                                     ba.data (), bb.data (), bc.data (), bd.data (), be.data ());
          struct Coppia { char const* nome; float const* mio; float const* prod; };
          Coppia const coppie[5] = {{"llra", ba.data (), c.llra_prod.data ()},
                                    {"llrb", bb.data (), c.llrb_prod.data ()},
                                    {"llrc", bc.data (), c.llrc_prod.data ()},
                                    {"llrd", bd.data (), c.llrd_prod.data ()},
                                    {"llre", be.data (), c.llre_prod.data ()}};
          double peggiore = 0.0;
          for (auto const& cp : coppie)
            {
              double d = 0.0;
              for (int i = 0; i < kCodeword; ++i)
                d = std::max (d, std::fabs (static_cast<double> (cp.mio[i] - cp.prod[i])));
              out << "  bicm(la=0) vs produzione, " << cp.nome << ": scarto massimo " << d << "\n";
              peggiore = std::max (peggiore, d);
            }
          out << (peggiore < 1e-3 ? "OK: la funzione innestata e' inerte a estrinseca nulla\n"
                                  : "DIVERSO: l'innesto NON e' inerte\n");
          return (dmax < 1e-3 && peggiore < 1e-3) ? 0 : 1;
        }

      // --- fantasmi: cornici di solo rumore, ogni accettazione e' un falso.
      // L'anello prova due volte in piu' per ogni candidato che fallisce: sono
      // due occasioni in piu' di passare la CRC-14 per caso (una ogni 16384).
      if (parser.isSet (rumore_opt))
        {
          Ft2Decoder dec {codice (), produzione_ft8 ()};
          long prove = 0, fbase = 0, fanello = 0;
          std::vector<uint8_t> bits (kCodeword), acc (1);
          std::array<float, kCodeword> lf {};
          auto accetta = [&] (float const* llrd) {
            for (int i = 0; i < kCodeword; ++i) lf[static_cast<size_t> (i)] = -llrd[i];
            dec.decode_batch (lf.data (), 1, bits.data (), acc.data ());
            return acc[0] != 0;
          };
          for (int s = 0; s < semi; ++s)
            {
              Cornice const c = prepara (messaggi.first (), freq, dt, 0.0,
                                         static_cast<unsigned> (3000000 + s), true);
              if (!c.ok) continue;
              ++prove;
              std::array<float, kCodeword> l {}, la {};
              double sigma0 = 1.0;
              demodula (c.cs.data (), nullptr, nullptr, nullptr, 0.0f, l.data (), &sigma0);
              float const conv = static_cast<float> (sigma0) / kScale;
              bool falso = accetta (l.data ());
              if (falso) ++fbase;
              bool falso_anello = falso;
              for (int g = 0; g < giri && !falso_anello; ++g)
                {
                  for (int i = 0; i < kCodeword; ++i) lf[static_cast<size_t> (i)] = -l[static_cast<size_t> (i)];
                  dec.decode_batch (lf.data (), 1, bits.data (), acc.data ());
                  int16_t const* post = dec.posterior (0);
                  for (int i = 0; i < kCodeword; ++i)
                    {
                      float const p = static_cast<float> (post[i]) / Ft2Decoder::kPosteriorFix;
                      float const est = p - lf[static_cast<size_t> (i)];
                      float const v = -est * damp;
                      la[static_cast<size_t> (i)] = std::max (-clamp, std::min (clamp, v));
                    }
                  demodula (c.cs.data (), nullptr, nullptr, la.data (), conv, l.data ());
                  falso_anello = accetta (l.data ());
                }
              if (falso_anello) ++fanello;
              if ((s % 500) == 499) { out << "  " << prove << " cornici...\n"; out.flush (); }
            }
          out << "\ncornici di solo rumore: " << prove << " (giri=" << giri << ")\n";
          out << "| configurazione | fantasmi | per mille |\n|---|---:|---:|\n";
          out << "| base (produzione) | " << fbase << " | "
              << QString::number (1000.0 * static_cast<double> (fbase) / std::max<long> (1, prove), 'f', 2) << " |\n";
          out << "| base + anello BICM-ID | " << fanello << " | "
              << QString::number (1000.0 * static_cast<double> (fanello) / std::max<long> (1, prove), 'f', 2) << " |\n";
          return 0;
        }

      QList<double> const snrs = lista (parser.value (snr_opt));
      out << "semi=" << semi << " messaggi=" << messaggi.size ()
          << " giri=" << giri << " clamp=" << clamp << " damp=" << damp
          << " ap-bits=" << ap_bits << "\n\n";
      out << "| SNR | prove | base |";
      for (int g = 1; g <= giri; ++g) out << " bicm" << g << " |";
      out << " apdec | apdem | apdue | genio | coerente | unione |\n|---:|---:|---:|";
      for (int g = 0; g < giri; ++g) out << "---:|";
      out << "---:|---:|---:|---:|---:|---:|\n";

      Ft2Decoder dec {codice (), produzione_ft8 ()};
      long tp = 0, tb = 0, tg = 0, tc = 0, tu = 0;
      long tapdec = 0, tapdem = 0, tapdue = 0;
      long fbase = 0, fbicm = 0;      // parole accettate ma SBAGLIATE
      std::vector<long> tot_bicm (static_cast<size_t> (giri), 0);
      for (double snr : snrs)
        {
          long prove = 0, base = 0, genio = 0, coer = 0, unione = 0;
          long apdec = 0, apdem = 0, apdue = 0;
          std::vector<long> bicm (static_cast<size_t> (giri), 0);
          for (QString const& m : messaggi)
            {
              decodium::txmsg::EncodedMessage const enc = decodium::txmsg::encodeFt8 (m);
              if (!enc.ok) continue;
              std::array<uint8_t, kCodeword> const vero = codeword_vero (enc.msgbits);
              for (int s = 0; s < semi; ++s)
                {
                  Cornice const c = prepara (m, freq, dt, snr, static_cast<unsigned> (2000000 + s));
                  if (!c.ok) continue;
                  ++prove;
                  std::array<float, kCodeword> l {}, lf {};
                  std::vector<uint8_t> bits (kCodeword), acc (1);
                  // `con_ap` riproduce quello che Stage4 fa GIA' oggi: i bit
                  // noti forzati a +-apmag (1,1 volte il massimo, come
                  // FtxDecodeBookkeeping.cpp:2925) e apmask al decodificatore,
                  // che cosi' non li flippa nella ricerca OSD.
                  // `falso` (se passato) conta le parole ACCETTATE ma diverse
                  // dalla vera: sono i fantasmi in presenza di segnale, che in
                  // aria contano piu' di quelli sul rumore puro.
                  auto tenta = [&] (float const* llrd, bool con_ap = false, long* falso = nullptr) {
                    for (int i = 0; i < kCodeword; ++i) lf[static_cast<size_t> (i)] = -llrd[i];
                    if (con_ap && ap_bits > 0)
                      {
                        float amax = 0.0f;
                        for (int i = 0; i < kCodeword; ++i) amax = std::max (amax, std::fabs (lf[static_cast<size_t> (i)]));
                        float const apmag = 1.1f * amax;
                        // in fastldpc il segno e' invertito: positivo = bit 0
                        for (int i = 0; i < ap_bits; ++i)
                          lf[static_cast<size_t> (i)] = vero[static_cast<size_t> (i)] ? -apmag : apmag;
                      }
                    dec.decode_batch (lf.data (), 1, bits.data (), acc.data (), nullptr,
                                      (con_ap && ap_bits > 0) ? mask_ap.data () : nullptr);
                    if (!acc[0]) return false;
                    for (int i = 0; i < kCodeword; ++i)
                      if (bits[static_cast<size_t> (i)] != vero[static_cast<size_t> (i)])
                        {
                          if (falso) ++*falso;
                          return false;
                        }
                    return true;
                  };
                  double sigma0 = 1.0;
                  demodula (c.cs.data (), nullptr, nullptr, nullptr, 0.0f, l.data (), &sigma0);
                  bool const ok_base = tenta (l.data (), false, &fbase);
                  if (ok_base) ++base;
                  // un LLR vale sigma/kScale unita' della metrica |cs|
                  float const conv = static_cast<float> (sigma0) / kScale;

                  // ANELLO BICM-ID: giri con l'estrinseca del decodificatore
                  {
                    std::array<float, kCodeword> li {}, la {};
                    demodula (c.cs.data (), nullptr, nullptr, nullptr, 0.0f, li.data ());
                    bool fatto = ok_base;
                    for (int g = 0; g < giri; ++g)
                      {
                        if (!fatto)
                          {
                            for (int i = 0; i < kCodeword; ++i) lf[static_cast<size_t> (i)] = -li[static_cast<size_t> (i)];
                            dec.decode_batch (lf.data (), 1, bits.data (), acc.data ());
                            int16_t const* post = dec.posterior (0);
                            for (int i = 0; i < kCodeword; ++i)
                              {
                                float const p = static_cast<float> (post[i]) / Ft2Decoder::kPosteriorFix;
                                float const est = p - lf[static_cast<size_t> (i)];
                                // -> convenzione Decodium, smorzata e limitata:
                                // un'estrinseca enorme e' quasi sempre un
                                // artefatto della quantizzazione.
                                float const v = -est * damp;
                                la[static_cast<size_t> (i)] = std::max (-clamp, std::min (clamp, v));
                              }
                            demodula (c.cs.data (), nullptr, nullptr, la.data (), conv, li.data ());
                            fatto = tenta (li.data (), false, &fbicm);
                          }
                        if (fatto) ++bicm[static_cast<size_t> (g)];
                      }
                  }

                  // A PRIORI DELL'AP, tre bracci per non sbagliare riferimento:
                  //   apdec  l'AP al solo decodificatore = quello che Stage4 fa OGGI
                  //   apdem  l'AP al solo demodulatore
                  //   apdue  a tutti e due = la proposta.
                  // Il guadagno da citare e' apdue - apdec, non apdue - base.
                  if (ap_bits > 0)
                    {
                      demodula (c.cs.data (), nullptr, nullptr, nullptr, 0.0f, l.data ());
                      if (tenta (l.data (), true)) ++apdec;
                      demodula (c.cs.data (), vero.data (), mask_ap.data (), nullptr, 0.0f, l.data ());
                      if (tenta (l.data ())) ++apdem;
                      if (tenta (l.data (), true)) ++apdue;
                    }

                  demodula (c.cs.data (), vero.data (), nullptr, nullptr, 0.0f, l.data ());
                  if (tenta (l.data ())) ++genio;
                  // ramo coerente REALIZZABILE: fase dai Costas
                  demodula_coerente_ft8 (c.cs.data (), l.data ());
                  bool const ok_coer = tenta (l.data ());
                  if (ok_coer) ++coer;
                  if (ok_base || ok_coer) ++unione;
                }
            }
          out << "| " << snr << " | " << prove << " | " << base << " | ";
          for (int g = 0; g < giri; ++g) out << bicm[static_cast<size_t> (g)] << " | ";
          out << apdec << " | " << apdem << " | " << apdue << " | " << genio
              << " | " << coer << " | " << unione << " |\n";
          out.flush ();
          tp += prove; tb += base; tg += genio; tc += coer; tu += unione;
          tapdec += apdec; tapdem += apdem; tapdue += apdue;
          for (int g = 0; g < giri; ++g) tot_bicm[static_cast<size_t> (g)] += bicm[static_cast<size_t> (g)];
        }
      out << "\ntotale su " << tp << " prove: base " << tb;
      for (int g = 0; g < giri; ++g) out << ", bicm" << (g + 1) << " " << tot_bicm[static_cast<size_t> (g)];
      out << ", apdec " << tapdec << ", apdem " << tapdem << ", apdue " << tapdue
          << ", genio " << tg << ", coerente " << tc << ", unione " << tu << "\n";
      if (tapdec > 0)
        out << "guadagno vero dell'AP al demodulatore: " << (tapdue - tapdec)
            << " su " << tapdec << " = "
            << (100.0 * static_cast<double> (tapdue - tapdec) / static_cast<double> (tapdec))
            << "%\n";
      out << "parole accettate ma SBAGLIATE: base " << fbase << ", anello " << fbicm
          << " (su " << tp << " prove)\n";
      return 0;
    }
  catch (std::exception const& e)
    {
      QTextStream err {stderr};
      err << "ft8_bicm_genie: " << e.what () << '\n';
      return 1;
    }
}
