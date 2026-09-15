// ft2_bicm_id.cpp — demodulazione iterativa (BICM-ID) per FT2.
//
// PERCHE'. Il demodulatore scompone ogni simbolo a 4 toni in due bit
// indipendenti. Non lo sono: condividono lo stesso simbolo, e trattarli come
// separati costa. Il calcolo dell'informazione mutua (lab/tools/fsk_gmi.py)
// misura quanto: fra capacita' a livello di simbolo e metriche di bit ci sono
// 0,96 dB su FT2 a coppie e 1,08 su quaterne (1,63 su FT8 a terne). E' il
// tetto di qualunque schema iterativo.
//
// COME. Il decodificatore, dopo un tentativo fallito, sa comunque qualcosa sui
// bit: i posteriori del min-sum. Sottraendo l'LLR di canale si ottiene
// l'informazione ESTRINSECA, che al demodulatore e' notizia nuova. Rimettendola
// nel calcolo delle metriche, l'ipotesi di tono viene pesata anche per quanto
// e' plausibile l'ALTRO bit dello stesso simbolo: lo spazio si stringe da 4
// ipotesi a circa 2, e le metriche del bit che interessa migliorano. Poi si
// ridecodifica. Due o tre giri.
//
// Il demodulatore a priori nulla e' identico a quello di produzione
// (run_ft2_bitmetrics_impl, ramo nseq=1): il banco lo verifica come prima cosa
// con --verifica, perche' se non coincide ogni confronto successivo e' falso.
//
// Tre bracci:
//   base    un solo passaggio, come oggi;
//   bicm    N giri con l'estrinseca del decodificatore;
//   genio   a priori PERFETTA (i bit veri): e' il limite superiore di
//           qualunque implementazione, e dice se vale la pena scrivere quella
//           vera dentro Stage7.
//
// Uso:
//   ft2_bicm_id --verifica
//   ft2_bicm_id --snr-list "-20,-19,-18,-17" --seeds 25 --giri 3
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <complex>
#include <random>
#include <stdexcept>
#include <vector>

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"
#include "Detector/fastldpc/ft2_decoder.hpp"

extern "C"
{
  void ftx_sync2d_c (std::complex<float> const* cd0, int np, int i0,
                     std::complex<float> const* ctwk, int itwk, float* sync);
  void ftx_ft2_downsample_c (float const* dd, int* newdata, float f0, std::complex<float>* c);
  void ftx_twkfreq1_c (std::complex<float> const* ca, int const* npts, float const* fsample,
                       float const* a, std::complex<float>* cb);
  void ftx_ft2_symbol_spectra_c (std::complex<float> const* cd, std::complex<float>* cs_out,
                                 float* beta_out);
  void ftx_ft2_bitmetrics_c (std::complex<float> const* cd, float* bitmetrics, int* badsync);
  void ftx_ft2_bitmetrics_diag_c (std::complex<float> const* cd, float* bitmetrics_final,
                                  float* bitmetrics_base, float* bmet_eq_raw, float* bmet_eq,
                                  std::complex<float>* cd_eq, float* ch_snr, int* badsync,
                                  int* use_cheq, float* snr_min, float* snr_max, float* snr_mean,
                                  float* fading_depth, float* noise_var, float* noise_var_eq);
  void ftx_ft2_rvec_c (signed char* out77);
  void ftx_ldpc174_91_tables_c (int* Mn_out, int* Nm_out, int* nrw_out, int* ncw_out);
  int ftx_encode174_91_message77_c (signed char const* message77, signed char* codeword_out);
}

namespace {

using Complex = std::complex<float>;

constexpr int kSampleRate {12000};
constexpr int kFrameSamples {45000};
constexpr int kNsps {288};
constexpr int kNDown {9};
constexpr int kNss {kNsps / kNDown};
constexpr int kNn {103};
constexpr int kRows {2 * kNn};
constexpr int kNSeg {kNn * kNss};
constexpr int kNdMax {kFrameSamples / kNDown};
constexpr int kCodeword {174};
constexpr int kBits77 {77};
constexpr float kFsDown {12000.0f / static_cast<float> (kNDown)};
constexpr float kScaleFac {2.83f};        // come FtxFt2Stage7::build_llr_sets
constexpr int kGraymap[4] = {0, 1, 3, 2};

[[noreturn]] void fail (QString const& m) { throw std::runtime_error {m.toStdString ()}; }

// Riga del vettore di bit-metrics che porta il bit c del codeword. Gli 8
// simboli di sincronismo (4 gruppi da 4) spezzano i 174 bit in tre blocchi da
// 58: la mappa e' quella di build_llr_sets.
int riga_di (int c)
{
  if (c < 58) return 8 + c;
  if (c < 116) return 74 + (c - 58);
  return 140 + (c - 116);
}

// Mappa inversa riga -> bit del codeword (-1 sulle righe di sincronismo).
int const* bit_di_riga ()
{
  static std::array<int, kRows> t = [] {
    std::array<int, kRows> v {};
    v.fill (-1);
    for (int c = 0; c < kCodeword; ++c) v[static_cast<size_t> (riga_di (c))] = c;
    return v;
  }();
  return t.data ();
}

// Demodulatore a un simbolo con informazione a priori.
//
// Senza a priori riproduce il ramo nseq=1 di run_ft2_bitmetrics_impl: metrica
// = log-sum-exp sulle ipotesi con il bit a 1 meno quella sulle ipotesi con il
// bit a 0, con peso beta sulla magnitudine del tono; poi la normalizzazione
// per sigma su tutte le 206 righe, come normalizebmet_cpp. (finalize_ft2_
// bitmetric_columns tocca solo righe di sincronismo, fuori dai 174 bit.)
//
// Con a priori, ogni ipotesi di tono porta in piu' il log-verosimiglianza
// dell'ALTRO bit dello stesso simbolo: w[i] = beta*|cs| + bit_altro(i)*La.
// Escludere il bit che si sta calcolando rende l'uscita ESTRINSECA, cioe'
// notizia nuova per il decodificatore e non l'eco della sua stessa opinione.
//
// la: [174] in convenzione Decodium (positivo = bit 1); nullo = nessuna.
void demodula (Complex const* cs, float beta, float const* la, float* llr_out)
{
  int const* const bdr = bit_di_riga ();
  std::array<float, kRows> m {};

  for (int r = 0; r < kRows; ++r)
    {
      int const s = r / 2;
      int const ib = r % 2;
      int const mio = 1 - ib;         // one[i][ibmax-ib] con ibmax=1
      int const altro = ib;

      float la_altro = 0.0f;
      if (la)
        {
          int const c_altro = bdr[static_cast<size_t> (2 * s + (1 - ib))];
          if (c_altro >= 0) la_altro = la[c_altro];
        }

      float w[4];
      for (int i = 0; i < 4; ++i)
        {
          // abs2 = |S|^2 (energia), come sp[i] in run_ft2_bitmetrics_impl:
          // beta = 0,5/varianza del rumore, quindi beta*|S|^2 e' l'esponente
          // della verosimiglianza, non il modulo.
          float const energia = std::norm (cs[s * 4 + kGraymap[i]]);
          w[i] = beta * energia + (((i & (1 << altro)) != 0) ? la_altro : 0.0f);
        }

      float max1 = -1.0e30f, max0 = -1.0e30f;
      for (int i = 0; i < 4; ++i)
        {
          if ((i & (1 << mio)) != 0) max1 = std::max (max1, w[i]);
          else max0 = std::max (max0, w[i]);
        }
      float lse1 = 0.0f, lse0 = 0.0f;
      for (int i = 0; i < 4; ++i)
        {
          if ((i & (1 << mio)) != 0) lse1 += std::exp (w[i] - max1);
          else lse0 += std::exp (w[i] - max0);
        }
      m[static_cast<size_t> (r)] = (max1 + std::log (std::max (lse1, 1.0e-30f)))
                                   - (max0 + std::log (std::max (lse0, 1.0e-30f)));
    }

  // normalizebmet_cpp: divisione per la deviazione standard delle 206 righe.
  double sum = 0.0, sum2 = 0.0;
  for (int r = 0; r < kRows; ++r) { sum += m[static_cast<size_t> (r)]; sum2 += static_cast<double> (m[static_cast<size_t> (r)]) * m[static_cast<size_t> (r)]; }
  double const media = sum / kRows, media2 = sum2 / kRows;
  double const varianza = media2 - media * media;
  double sigma = varianza > 0.0 ? std::sqrt (varianza) : std::sqrt (std::max (media2, 0.0));
  if (sigma <= 0.0) sigma = 1.0;

  for (int c = 0; c < kCodeword; ++c)
    llr_out[c] = kScaleFac * static_cast<float> (m[static_cast<size_t> (riga_di (c))] / sigma);
}

float rms (std::vector<float> const& v)
{
  double s = 0.0;
  for (float x : v) s += static_cast<double> (x) * x;
  return static_cast<float> (std::sqrt (s / std::max<size_t> (1, v.size ())));
}

void normalizza (Complex* d, int n)
{
  float s = 0.0f;
  for (int i = 0; i < n; ++i) s += std::norm (d[i]);
  s /= static_cast<float> (n);
  if (s <= 0.0f) return;
  float const k = 1.0f / std::sqrt (s);
  for (int i = 0; i < n; ++i) d[i] *= k;
}

void finestra (std::array<Complex, kNSeg>& cd, std::array<Complex, kNdMax> const& cb, int istart)
{
  std::fill (cd.begin (), cd.end (), Complex {});
  if (istart >= 0)
    {
      int const it = std::min (kNdMax - 1, istart + kNSeg - 1);
      int const np = it - istart + 1;
      if (np > 0) std::copy_n (cb.begin () + istart, np, cd.begin ());
    }
}

std::array<std::array<Complex, 2 * kNss>, 33> tweaks_tab ()
{
  std::array<std::array<Complex, 2 * kNss>, 33> t {};
  std::array<Complex, 2 * kNss> uno {};
  std::fill (uno.begin (), uno.end (), Complex {1.0f, 0.0f});
  int const npts = static_cast<int> (uno.size ());
  float const fs = kFsDown / 2.0f;
  for (int idf = -16; idf <= 16; ++idf)
    {
      std::array<float, 5> a {};
      a[0] = static_cast<float> (idf);
      ftx_twkfreq1_c (uno.data (), &npts, &fs, a.data (), t[static_cast<size_t> (idf + 16)].data ());
    }
  return t;
}

std::array<uint8_t, kCodeword> codeword_vero (QByteArray const& bits77)
{
  std::array<signed char, kBits77> rvec {};
  ftx_ft2_rvec_c (rvec.data ());
  std::array<signed char, kBits77> scr {};
  for (int i = 0; i < kBits77; ++i)
    scr[static_cast<size_t> (i)] =
        static_cast<signed char> ((static_cast<int> (bits77.at (i) != 0) + rvec[static_cast<size_t> (i)]) & 1);
  std::array<signed char, kCodeword> cw {};
  if (ftx_encode174_91_message77_c (scr.data (), cw.data ()) == 0)
    fail (QStringLiteral ("encode174_91 fallito"));
  std::array<uint8_t, kCodeword> out {};
  for (int i = 0; i < kCodeword; ++i) out[static_cast<size_t> (i)] = static_cast<uint8_t> (cw[static_cast<size_t> (i)] & 1);
  return out;
}

// Fase stimata dai soli SIMBOLI DI SINCRONISMO, cioe' quello che una
// implementazione vera potrebbe fare senza sapere nulla dei dati.
//
// FT2 trasmette quattro gruppi Costas da 4 simboli (ai simboli 0-3, 33-36,
// 66-69, 99-102) con toni NOTI: su quelli la fase del canale si misura
// direttamente. Ogni gruppo da' un'ancora (somma coerente dei suoi 4 simboli
// al tono giusto, per alzare il rapporto segnale-rumore della stima), e fra
// un'ancora e l'altra si interpola. L'interpolazione si fa sui vettori
// complessi unitari e non sugli angoli, cosi' non serve gestire i salti di
// 2 pi greco.
//
// E' la differenza fra il genio e il realizzabile: qui la stima e' rumorosa,
// ed e' proprio quel rumore che decide se la strada vale la pena.
void fase_da_sync (Complex const* cs, float* fase_out)
{
  static int const icos[4][4] = {{0,1,3,2},{1,0,2,3},{2,3,1,0},{3,2,0,1}};
  static int const base[4] = {0, 33, 66, 99};

  Complex ancora[4];
  float centro[4];
  for (int g = 0; g < 4; ++g)
    {
      Complex somma {};
      for (int k = 0; k < 4; ++k)
        {
          int const s = base[g] + k;
          somma += cs[s * 4 + icos[g][k]];
        }
      float const mag = std::abs (somma);
      ancora[g] = mag > 0.0f ? somma / mag : Complex {1.0f, 0.0f};
      centro[g] = static_cast<float> (base[g]) + 1.5f;
    }

  for (int s = 0; s < kNn; ++s)
    {
      float const x = static_cast<float> (s);
      Complex r;
      if (x <= centro[0]) r = ancora[0];
      else if (x >= centro[3]) r = ancora[3];
      else
        {
          int g = 0;
          while (g < 3 && x > centro[g + 1]) ++g;
          float const t = (x - centro[g]) / (centro[g + 1] - centro[g]);
          r = ancora[g] * (1.0f - t) + ancora[g + 1] * t;
        }
      float const mag = std::abs (r);
      fase_out[s] = mag > 0.0f ? std::arg (r) : 0.0f;
    }
}

// Le CINQUE passate cieche di produzione (llra..llre, ipass 1..5 di
// FtxFt2Stage7), ricavate dalle metriche vere del demodulatore invece che
// reimplementate: e' il riferimento onesto contro cui misurare un ramo in
// piu'. llra/llrb/llrc sono i tre piani (1, 2 e 4 simboli combinati
// coerentemente), llrd il piu' forte per bit e llre il secondo.
void passate_di_produzione (Complex const* cd, std::array<std::array<float, kCodeword>, 5>& out)
{
  std::array<float, kRows * 3> bm {};
  int badsync = 0;
  ftx_ft2_bitmetrics_c (cd, bm.data (), &badsync);

  for (int i = 0; i < 58; ++i)
    {
      for (int p = 0; p < 3; ++p)
        {
          out[static_cast<size_t> (p)][static_cast<size_t> (i)] = bm[static_cast<size_t> (8 + i + p * kRows)] * kScaleFac;
          out[static_cast<size_t> (p)][static_cast<size_t> (58 + i)] = bm[static_cast<size_t> (74 + i + p * kRows)] * kScaleFac;
          out[static_cast<size_t> (p)][static_cast<size_t> (116 + i)] = bm[static_cast<size_t> (140 + i + p * kRows)] * kScaleFac;
        }
    }
  for (int c = 0; c < kCodeword; ++c)
    {
      float const a = out[0][static_cast<size_t> (c)];
      float const b = out[1][static_cast<size_t> (c)];
      float const d = out[2][static_cast<size_t> (c)];
      float primo = a, secondo = b;
      if (std::fabs (b) > std::fabs (primo)) { secondo = primo; primo = b; }
      else if (std::fabs (b) > std::fabs (secondo)) { secondo = b; }
      if (std::fabs (d) > std::fabs (primo)) { secondo = primo; primo = d; }
      else if (std::fabs (d) > std::fabs (secondo)) { secondo = d; }
      out[3][static_cast<size_t> (c)] = primo;
      out[4][static_cast<size_t> (c)] = secondo;
    }
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
    for (int m = 0; m < k.M; ++m)
      {
        for (int r = 0; r < nrw[static_cast<size_t> (m)]; ++r)
          k.col_idx.push_back (nm[static_cast<size_t> (r + 7 * m)] - 1);
        k.row_ptr.push_back (static_cast<int> (k.col_idx.size ()));
      }
    return k;
  }();
  return c;
}

uint8_t const* rvec_bits ()
{
  static uint8_t const* const v = [] {
    static uint8_t b[77];
    signed char t[77];
    ftx_ft2_rvec_c (t);
    for (int i = 0; i < 77; ++i) b[i] = static_cast<uint8_t> (t[i] & 1);
    return b;
  }();
  return v;
}

// Configurazione di esercizio di FT2 (decodium_bridge.cpp, preset ndeep<=3).
Ft2Config produzione ()
{
  Ft2Config c = Ft2Decoder::conservativo ();
  c.osd_order = 2; c.span2 = 32; c.span3 = 0;
  c.pair_search = true; c.ntau = 14;
  c.nd_max = 0.065f;
  c.llr_clip = 2.5f;
  c.alpha_w = 37888;
  c.max_iter = 10;
  c.batch = 16;
  c.tipi_ammessi = plaus::kSoloUsati;
  c.descramble77 = rvec_bits ();
  return c;
}

QList<double> lista (QString const& raw, QString const& nome)
{
  QList<double> v;
  for (QString p : raw.split (QLatin1Char {','}, Qt::SkipEmptyParts))
    {
      bool ok = false;
      double const x = p.trimmed ().toDouble (&ok);
      if (!ok) fail (QStringLiteral ("valore non valido in --%1").arg (nome));
      v.append (x);
    }
  return v;
}

struct Cornice
{
  std::array<Complex, kNn * 4> cs {};
  std::array<Complex, kNSeg> cd {};   // finestra sincronizzata, per il riferimento
  std::array<float, kNn> fase {};     // fase VERA del canale per simbolo (genio)
  float beta {0.0f};
  bool ok {false};
};

// Demodulatore COERENTE con fase nota: al posto del modulo dell'ipotesi si usa
// la sua proiezione sulla fase vera del canale. E' il genio della fase, cioe'
// il limite superiore di un tracciamento alla Wiener (BCJR sugli stati di
// fase): il divario fra questo e il demodulatore di oggi e' quanto vale al
// massimo quella strada. Su fsk_gmi.py il tetto era 1,1 dB per FT2.
void demodula_coerente (Complex const* cs, float const* fase, float beta, float* llr_out)
{
  std::array<float, kRows> m {};
  for (int r = 0; r < kRows; ++r)
    {
      int const s = r / 2;
      int const mio = 1 - (r % 2);
      Complex const rot = std::polar (1.0f, -fase[s]);
      float w[4];
      for (int i = 0; i < 4; ++i)
        {
          // proiezione sulla fase vera: con fase nota l'informazione sta nella
          // parte reale, la quadratura e' solo rumore.
          float const proj = std::real (cs[s * 4 + kGraymap[i]] * rot);
          w[i] = beta * proj * std::fabs (proj);
        }
      float max1 = -1.0e30f, max0 = -1.0e30f;
      for (int i = 0; i < 4; ++i)
        {
          if ((i & (1 << mio)) != 0) max1 = std::max (max1, w[i]);
          else max0 = std::max (max0, w[i]);
        }
      float lse1 = 0.0f, lse0 = 0.0f;
      for (int i = 0; i < 4; ++i)
        {
          if ((i & (1 << mio)) != 0) lse1 += std::exp (w[i] - max1);
          else lse0 += std::exp (w[i] - max0);
        }
      m[static_cast<size_t> (r)] = (max1 + std::log (std::max (lse1, 1.0e-30f)))
                                   - (max0 + std::log (std::max (lse0, 1.0e-30f)));
    }
  double sum = 0.0, sum2 = 0.0;
  for (int r = 0; r < kRows; ++r) { sum += m[static_cast<size_t> (r)]; sum2 += static_cast<double> (m[static_cast<size_t> (r)]) * m[static_cast<size_t> (r)]; }
  double const media = sum / kRows, media2 = sum2 / kRows;
  double const varianza = media2 - media * media;
  double sigma = varianza > 0.0 ? std::sqrt (varianza) : std::sqrt (std::max (media2, 0.0));
  if (sigma <= 0.0) sigma = 1.0;
  for (int c = 0; c < kCodeword; ++c)
    llr_out[c] = kScaleFac * static_cast<float> (m[static_cast<size_t> (riga_di (c))] / sigma);
}

// Genera, sincronizza e restituisce gli spettri per simbolo: e' il punto in cui
// il demodulatore vero consegna quello che serve al BICM-ID.
// Con `solo_rumore` la cornice non contiene alcun segnale: il sincronismo si
// aggancia dove capita, come farebbe su un candidato di rumore proposto dalla
// ricerca, e QUALUNQUE parola accettata a valle e' un fantasma per
// costruzione. E' il modo di misurare quanto costa una passata in piu' in
// termini di falsi: piu' passate = piu' candidati alla CRC-14, che ne ammette
// uno sbagliato ogni 16384.
Cornice prepara (QString const& messaggio, float freq, float offset_ms, double snr_db,
                 unsigned seme, std::array<std::array<Complex, 2 * kNss>, 33> const& tw,
                 bool solo_rumore = false)
{
  Cornice out;
  decodium::txmsg::EncodedMessage const enc = decodium::txmsg::encodeFt2 (messaggio);
  if (!enc.ok || enc.tones.isEmpty ()) return out;
  QVector<float> const wave = decodium::txwave::generateFt2Wave (
      enc.tones.constData (), enc.tones.size (), kNsps, static_cast<float> (kSampleRate), freq);
  if (wave.isEmpty ()) return out;

  int const off = static_cast<int> (std::lround (static_cast<double> (offset_ms) * kSampleRate / 1000.0));
  std::vector<float> frame (static_cast<size_t> (kFrameSamples), 0.0f);
  if (!solo_rumore)
    for (int i = 0; i < wave.size (); ++i) frame[static_cast<size_t> (off + i)] = 0.85f * wave[i];
  float const r = solo_rumore ? 0.05f : rms (frame);
  double const sigma = solo_rumore ? 0.05 : static_cast<double> (r) / std::pow (10.0, snr_db / 20.0);
  std::mt19937 rng {seme};
  std::normal_distribution<float> noise {0.0f, static_cast<float> (sigma)};
  for (float& x : frame) x += noise (rng);

  std::array<Complex, kNdMax> cb {};
  int newdata = 1;
  ftx_ft2_downsample_c (frame.data (), &newdata, freq, cb.data ());
  normalizza (cb.data (), kNdMax);

  int ibest = -1, idfbest = 0;
  for (int isync = 1; isync <= 2; ++isync)
    {
      int idfmin = -12, idfmax = 12, idfstp = 3, ibmin = 216, ibmax = 1120, ibstp = 4;
      if (isync == 2)
        {
          idfmin = idfbest - 4; idfmax = idfbest + 4; idfstp = 1;
          ibmin = ibest - 5; ibmax = ibest + 5; ibstp = 1;
        }
      int bi = -1, bdf = 0; float smax = -99.0f;
      for (int idf = idfmin; idf <= idfmax; idf += idfstp)
        for (int st = ibmin; st <= ibmax; st += ibstp)
          {
            float sy = 0.0f;
            ftx_sync2d_c (cb.data (), kNdMax, st, tw[static_cast<size_t> (idf + 16)].data (), 1, &sy);
            if (sy > smax) { smax = sy; bi = st; bdf = idf; }
          }
      ibest = bi; idfbest = bdf;
    }

  std::array<Complex, kNdMax> cb2 {};
  newdata = 0;
  ftx_ft2_downsample_c (frame.data (), &newdata, freq + static_cast<float> (idfbest), cb2.data ());
  normalizza (cb2.data (), kNdMax);
  finestra (out.cd, cb2, ibest);

  ftx_ft2_symbol_spectra_c (out.cd.data (), out.cs.data (), &out.beta);

  // Fase VERA del canale: stessa strada ma senza rumore. Nella cornice pulita
  // il tono trasmesso e' l'unico con energia, quindi la sua fase e' la fase
  // del canale a quel simbolo. Serve al braccio "genio della fase".
  {
    std::vector<float> pulito (static_cast<size_t> (kFrameSamples), 0.0f);
    for (int i = 0; i < wave.size (); ++i) pulito[static_cast<size_t> (off + i)] = 0.85f * wave[i];
    std::array<Complex, kNdMax> cbp {};
    int nd = 1;
    ftx_ft2_downsample_c (pulito.data (), &nd, freq + static_cast<float> (idfbest), cbp.data ());
    normalizza (cbp.data (), kNdMax);
    std::array<Complex, kNSeg> cdp {};
    finestra (cdp, cbp, ibest);
    std::array<Complex, kNn * 4> csp {};
    float betap = 0.0f;
    ftx_ft2_symbol_spectra_c (cdp.data (), csp.data (), &betap);
    for (int k = 0; k < kNn; ++k)
      {
        int best_t = 0;
        float best_m = -1.0f;
        for (int t = 0; t < 4; ++t)
          {
            float const mm = std::abs (csp[k * 4 + t]);
            if (mm > best_m) { best_m = mm; best_t = t; }
          }
        out.fase[static_cast<size_t> (k)] = std::arg (csp[k * 4 + best_t]);
      }
  }

  out.ok = true;
  return out;
}

}  // namespace

int main (int argc, char* argv[])
{
  try
    {
      QCoreApplication app {argc, argv};
      QCoreApplication::setApplicationName (QStringLiteral ("ft2_bicm_id"));

      QCommandLineParser parser;
      parser.setApplicationDescription (
          QStringLiteral ("Demodulazione iterativa (BICM-ID) per FT2: base, iterativo e limite col genio."));
      parser.addHelpOption ();
      QCommandLineOption verifica_opt {"verifica", "Controlla che a priori nulla si riproduca il demodulatore di produzione."};
      QCommandLineOption msg_opt {"message", "Messaggio FT2 (ripetibile).", "text"};
      QCommandLineOption snr_opt {"snr-list", "SNR in dB separati da virgola.", "list", "-21,-20,-19,-18"};
      QCommandLineOption seeds_opt {"seeds", "Semi per (messaggio,SNR).", "n", "25"};
      QCommandLineOption giri_opt {"giri", "Giri di BICM-ID.", "n", "3"};
      QCommandLineOption freq_opt {"freq", "Frequenza audio.", "hz", "1500.0"};
      QCommandLineOption off_opt {"offset-ms", "Inizio del burst.", "ms", "600.0"};
      QCommandLineOption rumore_opt {"rumore", "Cornici di solo rumore: conta i fantasmi con 5 passate e con 6."};
      QCommandLineOption clamp_opt {"clamp", "Limite sull'estrinseca rimandata al demodulatore.", "v", "2.0"};
      QCommandLineOption damp_opt {"damp", "Smorzamento dell'estrinseca (1 = nessuno).", "v", "1.0"};
      parser.addOption (verifica_opt); parser.addOption (msg_opt); parser.addOption (snr_opt);
      parser.addOption (seeds_opt); parser.addOption (giri_opt); parser.addOption (freq_opt);
      parser.addOption (off_opt); parser.addOption (clamp_opt); parser.addOption (damp_opt);
      parser.addOption (rumore_opt);
      parser.process (app);

      QStringList messaggi = parser.values (msg_opt);
      if (messaggi.isEmpty ())
        messaggi = {QStringLiteral ("CQ IU8LMC JN70"), QStringLiteral ("IU8LMC DL9XYZ -12"),
                    QStringLiteral ("DL9XYZ IU8LMC R-08"), QStringLiteral ("IU8LMC DL9XYZ 73")};
      bool ok = false;
      int const semi = parser.value (seeds_opt).toInt (&ok);
      int const giri = parser.value (giri_opt).toInt (&ok);
      float const freq = parser.value (freq_opt).toFloat (&ok);
      float const offms = parser.value (off_opt).toFloat (&ok);
      float const clamp = parser.value (clamp_opt).toFloat (&ok);
      float const damp = parser.value (damp_opt).toFloat (&ok);
      QList<double> const snrs = lista (parser.value (snr_opt), "snr-list");

      auto const tw = tweaks_tab ();
      QTextStream out {stdout};

      // --- fantasmi: cornici di solo rumore, ogni accettazione e' un falso
      if (parser.isSet (rumore_opt))
        {
          Ft2Decoder dec {codice (), produzione ()};
          long prove = 0, f5 = 0, fcoer = 0, f6 = 0;
          std::vector<uint8_t> bits (kCodeword), acc (1);
          std::array<float, kCodeword> lf {};
          auto accetta = [&] (float const* llrd) {
            for (int i = 0; i < kCodeword; ++i) lf[static_cast<size_t> (i)] = -llrd[i];
            dec.decode_batch (lf.data (), 1, bits.data (), acc.data ());
            return acc[0] != 0;
          };
          for (int s = 0; s < semi; ++s)
            {
              Cornice const c = prepara (messaggi.first (), freq, offms, 0.0,
                                         static_cast<unsigned> (3000000 + s), tw, true);
              if (!c.ok) continue;
              ++prove;
              std::array<std::array<float, kCodeword>, 5> prod {};
              passate_di_produzione (c.cd.data (), prod);
              bool p5 = false;
              for (int p = 0; p < 5; ++p) if (accetta (prod[static_cast<size_t> (p)].data ())) p5 = true;
              std::array<float, kNn> stima {};
              fase_da_sync (c.cs.data (), stima.data ());
              std::array<float, kCodeword> l {};
              demodula_coerente (c.cs.data (), stima.data (), c.beta, l.data ());
              bool const pc = accetta (l.data ());
              if (p5) ++f5;
              if (pc) ++fcoer;
              if (p5 || pc) ++f6;
              if ((s % 200) == 199) { out << "  " << prove << " cornici...\n"; out.flush (); }
            }
          out << "\ncornici di solo rumore: " << prove << "\n";
          out << "| configurazione | fantasmi | per mille |\n|---|---:|---:|\n";
          out << "| 5 passate (produzione) | " << f5 << " | "
              << QString::number (1000.0 * f5 / std::max<long> (1, prove), 'f', 2) << " |\n";
          out << "| solo ramo coerente | " << fcoer << " | "
              << QString::number (1000.0 * fcoer / std::max<long> (1, prove), 'f', 2) << " |\n";
          out << "| 6 passate (con coerente) | " << f6 << " | "
              << QString::number (1000.0 * f6 / std::max<long> (1, prove), 'f', 2) << " |\n";
          return 0;
        }


      // --- verifica: a priori nulla deve dare il demodulatore di produzione
      if (parser.isSet (verifica_opt))
        {
          Cornice const c = prepara (messaggi.first (), freq, offms, -14.0, 1000000u, tw);
          if (!c.ok) fail (QStringLiteral ("preparazione fallita"));
          std::vector<float> mio (kCodeword);
          demodula (c.cs.data (), c.beta, nullptr, mio.data ());

          // Riferimento VERO: il demodulatore di produzione sulla stessa
          // finestra, letto con la mappatura di build_llr_sets (llra, nseq=1).
          // Confrontarsi con una propria reimplementazione non proverebbe
          // niente: un errore di mappatura sarebbe identico da entrambe le
          // parti.
          std::array<float, kRows * 3> bm {}, bm_base {}, eqraw {}, eq {};
          std::vector<Complex> cdeq (kNSeg);
          std::array<float, kNn> chsnr {};
          int badsync = 0, usecheq = 0;
          float a = 0, b = 0, d = 0, e = 0, f = 0, g = 0;
          ftx_ft2_bitmetrics_diag_c (c.cd.data (), bm.data (), bm_base.data (), eqraw.data (),
                                     eq.data (), cdeq.data (), chsnr.data (), &badsync, &usecheq,
                                     &a, &b, &d, &e, &f, &g);
          out << "use_cheq=" << usecheq << " (ramo equalizzato "
              << (usecheq ? "attivo: il mio modella solo quello base" : "spento") << ")\n";
          std::vector<float> rif (kCodeword);
          for (int i = 0; i < 58; ++i)
            {
              rif[static_cast<size_t> (i)] = bm_base[static_cast<size_t> (8 + i)] * kScaleFac;
              rif[static_cast<size_t> (58 + i)] = bm_base[static_cast<size_t> (74 + i)] * kScaleFac;
              rif[static_cast<size_t> (116 + i)] = bm_base[static_cast<size_t> (140 + i)] * kScaleFac;
            }
          double dmax = 0.0;
          int peggiore = -1;
          for (int i = 0; i < kCodeword; ++i)
            {
              double const d = std::fabs (static_cast<double> (mio[static_cast<size_t> (i)] - rif[static_cast<size_t> (i)]));
              if (d > dmax) { dmax = d; peggiore = i; }
            }
          out << "badsync=" << badsync << "  scarto massimo " << dmax
              << " (bit " << peggiore << ")\n";
          if (peggiore >= 0)
            out << "  mio=" << mio[static_cast<size_t> (peggiore)]
                << "  produzione=" << rif[static_cast<size_t> (peggiore)] << "\n";
          out << (dmax < 1e-3 ? "OK: demodulatore identico a quello di produzione\n"
                              : "DIVERSO: la mappatura non combacia\n");
          return dmax < 1e-3 ? 0 : 1;
        }

      // --- misura
      out << "semi=" << semi << " messaggi=" << messaggi.size () << " giri=" << giri << "\n\n";
      out << "| SNR | prove | base | " ;
      for (int g = 1; g <= giri; ++g) out << "bicm" << g << " | ";
      out << "genio | fase | fase-sync | unione | base5 | unione6 |\n|---:|---:|---:|";
      for (int g = 1; g <= giri; ++g) out << "---:|";
      out << "---:|---:|\n";

      Ft2Decoder dec {codice (), produzione ()};
      long tot_base = 0, tot_genio = 0, tot_prove = 0, tot_fase = 0, tot_stim = 0, tot_unione = 0, tot_base5 = 0, tot_un6 = 0;
      std::vector<long> tot_bicm (static_cast<size_t> (giri), 0);

      for (double snr : snrs)
        {
          long prove = 0, base = 0, genio = 0, fase_ok = 0, fase_stim = 0, unione = 0, base5 = 0, unione6 = 0;
          std::vector<long> bicm (static_cast<size_t> (giri), 0);
          for (QString const& m : messaggi)
            {
              decodium::txmsg::EncodedMessage const enc = decodium::txmsg::encodeFt2 (m);
              if (!enc.ok) continue;
              std::array<uint8_t, kCodeword> const vero = codeword_vero (enc.msgbits);

              for (int s = 0; s < semi; ++s)
                {
                  Cornice const c = prepara (m, freq, offms, snr, static_cast<unsigned> (1000000 + s), tw);
                  if (!c.ok) continue;
                  ++prove;

                  std::vector<float> llr_dec (kCodeword), llr_fast (kCodeword);
                  std::vector<uint8_t> bits (kCodeword), acc (1);

                  auto tenta = [&] (float const* llrd) {
                    for (int i = 0; i < kCodeword; ++i) llr_fast[static_cast<size_t> (i)] = -llrd[i];
                    dec.decode_batch (llr_fast.data (), 1, bits.data (), acc.data ());
                    if (!acc[0]) return false;
                    for (int i = 0; i < kCodeword; ++i)
                      if (bits[static_cast<size_t> (i)] != vero[static_cast<size_t> (i)]) return false;
                    return true;
                  };

                  // braccio base
                  demodula (c.cs.data (), c.beta, nullptr, llr_dec.data ());
                  bool preso = tenta (llr_dec.data ());
                  if (preso) ++base;

                  // braccio BICM-ID: giri con l'estrinseca del decodificatore
                  {
                    std::vector<float> l (kCodeword), la (kCodeword, 0.0f);
                    demodula (c.cs.data (), c.beta, nullptr, l.data ());
                    bool fatto = preso;
                    for (int g = 0; g < giri; ++g)
                      {
                        if (!fatto)
                          {
                            // estrinseca = posteriore - ingresso, in convenzione
                            // fastldpc; riportata a quella del demodulatore.
                            for (int i = 0; i < kCodeword; ++i) llr_fast[static_cast<size_t> (i)] = -l[static_cast<size_t> (i)];
                            dec.decode_batch (llr_fast.data (), 1, bits.data (), acc.data ());
                            int16_t const* post = dec.posterior (0);
                            for (int i = 0; i < kCodeword; ++i)
                              {
                                float const p = static_cast<float> (post[i]) / Ft2Decoder::kPosteriorFix;
                                float const est = p - llr_fast[static_cast<size_t> (i)];
                                // -> convenzione Decodium, smorzata e limitata:
                                // un'estrinseca enorme e' quasi sempre un
                                // artefatto della quantizzazione.
                                float v = -est * damp;
                                la[static_cast<size_t> (i)] = std::max (-clamp, std::min (clamp, v));
                              }
                            demodula (c.cs.data (), c.beta, la.data (), l.data ());
                            fatto = tenta (l.data ());
                          }
                        if (fatto) ++bicm[static_cast<size_t> (g)];
                      }
                  }

                  // braccio genio della FASE: rivelazione coerente, fase vera
                  {
                    std::vector<float> l (kCodeword);
                    demodula_coerente (c.cs.data (), c.fase.data (), c.beta, l.data ());
                    if (tenta (l.data ())) ++fase_ok;
                  }

                  // braccio REALIZZABILE: fase stimata dai soli simboli di
                  // sincronismo, come potrebbe fare il decoder vero.
                  //
                  // "unione" e' il numero che conta per una decisione: FT2 fa
                  // gia' cinque passate cieche e tiene quella che decodifica,
                  // quindi il ramo coerente aggiunto come SESTA passata darebbe
                  // l'unione, non la sostituzione. Dove la stima peggiora
                  // (SNR alti) decodifica comunque una delle altre.
                  {
                    std::array<float, kNn> stima {};
                    fase_da_sync (c.cs.data (), stima.data ());
                    std::vector<float> l (kCodeword);
                    demodula_coerente (c.cs.data (), stima.data (), c.beta, l.data ());
                    bool const ok_coer = tenta (l.data ());
                    if (ok_coer) ++fase_stim;
                    if (ok_coer || preso) ++unione;

                    // Il confronto che conta davvero: le CINQUE passate di
                    // produzione contro le stesse cinque piu' il ramo coerente
                    // come sesta. Il riferimento a una passata sola
                    // sopravvaluta il guadagno, perche' la produzione una parte
                    // della coerenza se la prende gia' combinando 2 e 4 simboli.
                    std::array<std::array<float, kCodeword>, 5> prod {};
                    passate_di_produzione (c.cd.data (), prod);
                    bool ok5 = false;
                    for (int p = 0; p < 5 && !ok5; ++p)
                      ok5 = tenta (prod[static_cast<size_t> (p)].data ());
                    if (ok5) ++base5;
                    if (ok5 || ok_coer) ++unione6;
                  }

                  // braccio genio: a priori perfetta
                  {
                    std::vector<float> la (kCodeword), l (kCodeword);
                    for (int i = 0; i < kCodeword; ++i)
                      la[static_cast<size_t> (i)] = vero[static_cast<size_t> (i)] ? 6.0f : -6.0f;
                    demodula (c.cs.data (), c.beta, la.data (), l.data ());
                    if (tenta (l.data ())) ++genio;
                  }
                }
            }
          out << "| " << snr << " | " << prove << " | " << base << " | ";
          for (int g = 0; g < giri; ++g) out << bicm[static_cast<size_t> (g)] << " | ";
          out << genio << " | " << fase_ok << " | " << fase_stim << " | " << unione
              << " | " << base5 << " | " << unione6 << " |\n";
          out.flush ();
          tot_prove += prove; tot_base += base; tot_genio += genio; tot_fase += fase_ok; tot_stim += fase_stim; tot_unione += unione; tot_base5 += base5; tot_un6 += unione6;
          for (int g = 0; g < giri; ++g) tot_bicm[static_cast<size_t> (g)] += bicm[static_cast<size_t> (g)];
        }

      out << "\ntotale su " << tot_prove << " prove: base " << tot_base;
      for (int g = 0; g < giri; ++g) out << ", bicm" << (g + 1) << " " << tot_bicm[static_cast<size_t> (g)];
      out << ", genio " << tot_genio << ", fase " << tot_fase
          << ", fase-sync " << tot_stim << ", unione " << tot_unione
          << " | PRODUZIONE: base5 " << tot_base5 << ", unione6 " << tot_un6 << "\n";
      return 0;
    }
  catch (std::exception const& e)
    {
      QTextStream err {stderr};
      err << "ft2_bicm_id: " << e.what () << '\n';
      return 1;
    }
}
