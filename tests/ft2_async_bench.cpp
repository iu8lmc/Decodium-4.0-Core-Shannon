// Banco del ricevitore asincrono FT2 (ASYMX / Async L2) — fase F0 del
// progetto FT2 asincrono (doc/PROGETTO_ASYMX_JTTY.md).
//
// Riproduce fuori dall'app quello che fa il percorso VIVO del bridge
// (DecodiumBridge::onAsyncDecodeTimer -> FT2DecodeWorker::decodeAsync ->
// onFt2AsyncDecodeReady), non quello del backend legacy in widgets/:
//   - l'audio entra campione per campione in un ring contiguo;
//   - un timer da 100 ms prende gli ultimi 45 000 campioni (3,75 s) e li passa
//     al decoder di produzione (ftx_ft2_async_decode_stage7_c), solo se il
//     worker e' libero e l'audio e' avanzato (single-flight);
//   - il tempo che il decoder impiega davvero (misurato) tiene occupato il
//     worker nel tempo simulato: i tick che cadono in quel tempo si perdono,
//     come nell'app;
//   - le righe passano dalla stessa chiave di doppione del bridge:
//     (slot di 3,75 s calcolato al dispatch, frequenza a gruppi di 20 Hz,
//     testo normalizzato).
//
// I filtri semantici, anti-fantasma e di eco del bridge NON sono modellati:
// il banco misura il ricevitore e la visualizzazione, non le politiche.
//
//   ft2_async_bench gen  out.wav truth.txt [opzioni]   scena sintetica
//   ft2_async_bench run  in.wav  [truth.txt] [opzioni]  simulazione
//
// Nel file di verita' una riga per trasmissione:
//   t_inizio_s  frequenza_hz  snr_db  messaggio

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <fftw3.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>

#include "Detector/Ft2AsyncRegistry.hpp"
#include "Detector/Ft2AsyncSottrazione.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"

extern "C"
{
  void ftx_ft2_async_decode_stage7_c (short const* iwave, int* nqsoprogress, int* nfqso,
                                      int* nfa, int* nfb, int* ndepth, int* ncontest,
                                      char const* mycall, char const* hiscall,
                                      int* snrs, float* dts, float* freqs, int* naps,
                                      float* quals, signed char* bits77, char* decodeds,
                                      int* nout);
  void ftx_ft2_stage7_clravg_c ();
  void ftx_ft2_set_async_ib_range_c (int lo, int hi);
  void ftx_ft2_set_async_expected_c (float f, int lo, int hi);
  void ftx_subtract_ft2_c (float* dd, int const* itone, float f0, float dt);
}

namespace
{

constexpr int kRate = 12000;
constexpr int kWindow = 45000;            // 3,75 s: la finestra del path async
constexpr int kNsps = 288;
constexpr int kSymbols = 103;
constexpr double kTxSeconds = kSymbols * kNsps / double (kRate);   // 2,472 s
constexpr int kPeriodMs = 3750;
constexpr int kMaxLines = 100;

// ------------------------------------------------------------------ WAV

bool read_wav (std::string const& path, std::vector<short>& out)
{
  std::ifstream f (path, std::ios::binary);
  if (!f) return false;
  std::vector<char> d ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char> ());
  std::size_t pos = 12;
  while (pos + 8 <= d.size ())
    {
      std::uint32_t size;
      std::memcpy (&size, &d[pos + 4], 4);
      if (std::memcmp (&d[pos], "data", 4) == 0)
        {
          std::size_t const n = std::min<std::size_t> (size, d.size () - pos - 8) / 2;
          out.resize (n);
          std::memcpy (out.data (), &d[pos + 8], n * 2);
          return true;
        }
      pos += 8 + size + (size & 1);
    }
  return false;
}

bool write_wav (std::string const& path, std::vector<short> const& s)
{
  std::ofstream f (path, std::ios::binary);
  if (!f) return false;
  std::uint32_t const bytes = static_cast<std::uint32_t> (s.size () * 2);
  auto u32 = [&] (std::uint32_t v) { f.write (reinterpret_cast<char const*> (&v), 4); };
  auto u16 = [&] (std::uint16_t v) { f.write (reinterpret_cast<char const*> (&v), 2); };
  f.write ("RIFF", 4); u32 (36 + bytes); f.write ("WAVEfmt ", 8); u32 (16); u16 (1); u16 (1);
  u32 (kRate); u32 (kRate * 2); u16 (2); u16 (16); f.write ("data", 4); u32 (bytes);
  f.write (reinterpret_cast<char const*> (s.data ()), bytes);
  return true;
}

// ------------------------------------------------------------------ opzioni

struct Args
{
  std::map<std::string, std::string> kv;
  std::vector<std::string> pos;
  std::string get (std::string const& k, std::string const& d) const
  {
    auto it = kv.find (k);
    return it == kv.end () ? d : it->second;
  }
  double num (std::string const& k, double d) const { return std::atof (get (k, std::to_string (d)).c_str ()); }
};

Args parse (int argc, char** argv, int from)
{
  Args a;
  for (int i = from; i < argc; ++i)
    {
      std::string s = argv[i];
      if (s.rfind ("--", 0) == 0)
        {
          auto eq = s.find ('=');
          if (eq != std::string::npos) a.kv[s.substr (2, eq - 2)] = s.substr (eq + 1);
          else a.kv[s.substr (2)] = "1";
        }
      else
        a.pos.push_back (s);
    }
  return a;
}

// ------------------------------------------------------------------ scena

struct Tx
{
  double t0 {0};
  double freq {0};
  double snr {0};
  std::string msg;
};

std::string random_call (std::mt19937& rng)
{
  static char const* pfx[] = {"K", "W", "N", "AA", "KA", "WB", "G", "DL", "F", "I", "IU", "JA", "VK", "EA", "OH", "SP", "PY", "LU"};
  std::uniform_int_distribution<int> p (0, int (sizeof pfx / sizeof *pfx) - 1), dig (0, 9), let (0, 25), n (1, 3);
  std::string c = pfx[p (rng)];
  c += char ('0' + dig (rng));
  int const k = n (rng);
  for (int i = 0; i < k; ++i) c += char ('A' + let (rng));
  return c;
}

std::string random_grid (std::mt19937& rng)
{
  std::uniform_int_distribution<int> l (0, 17), d (0, 9);
  std::string g;
  g += char ('A' + l (rng)); g += char ('A' + l (rng)); g += char ('0' + d (rng)); g += char ('0' + d (rng));
  return g;
}

std::string random_message (std::mt19937& rng)
{
  std::uniform_int_distribution<int> kind (0, 4), rep (-24, 5);
  std::string const a = random_call (rng), b = random_call (rng);
  char r[8];
  int const x = rep (rng);
  std::snprintf (r, sizeof r, "%+03d", x);
  switch (kind (rng))
    {
    case 0: return "CQ " + a + " " + random_grid (rng);
    case 1: return a + " " + b + " " + random_grid (rng);
    case 2: return a + " " + b + " " + r;
    case 3: return a + " " + b + " R" + r;
    default: return a + " " + b + " RR73";
    }
}

int gen (Args const& a)
{
  if (a.pos.size () < 2)
    {
      std::fprintf (stderr, "gen out.wav truth.txt [--seconds=120] [--seed=1] [--rate=12] (tx/min)\n"
                            "    [--snr-min=-20] [--snr-max=-8] [--repeat=0.15] [--fmin=300] [--fmax=2700]\n"
                            "    [--overlap=0] (1: permette sovrapposizioni in tempo e frequenza)\n");
      return 2;
    }
  double const seconds = a.num ("seconds", 120);
  unsigned const seed = unsigned (a.num ("seed", 1));
  double const rate = a.num ("rate", 12);
  double const snr_min = a.num ("snr-min", -20), snr_max = a.num ("snr-max", -8);
  double const p_repeat = a.num ("repeat", 0.15);
  double const fmin = a.num ("fmin", 300), fmax = a.num ("fmax", 2700);
  bool const overlap = a.num ("overlap", 0) != 0;
  std::mt19937 rng (seed);
  std::uniform_real_distribution<double> U (0.0, 1.0);

  std::vector<Tx> txs;
  int const n = int (std::lround (rate * seconds / 60.0));
  auto clash = [&] (Tx const& t) {
    if (overlap) return false;
    for (auto const& o : txs)
      if (std::abs (o.freq - t.freq) < 60 && std::abs (o.t0 - t.t0) < kTxSeconds + 0.1) return true;
    return false;
  };
  int guard = 0;
  while (int (txs.size ()) < n && guard++ < n * 200)
    {
      Tx t;
      t.t0 = 0.5 + U (rng) * (seconds - kTxSeconds - 1.0);
      t.freq = fmin + U (rng) * (fmax - fmin);
      t.snr = snr_min + U (rng) * (snr_max - snr_min);
      t.msg = random_message (rng);
      if (clash (t)) continue;
      txs.push_back (t);
      // Ripetizione legittima dello stesso messaggio, come chi ripete il
      // rapporto perche' non e' stato copiato: 2,6-4 s dopo, stessa frequenza.
      if (U (rng) < p_repeat)
        {
          Tx r = t;
          r.t0 = t.t0 + 2.6 + U (rng) * 1.4;
          if (r.t0 + kTxSeconds < seconds - 0.5 && !clash (r)) txs.push_back (r);
        }
    }
  std::sort (txs.begin (), txs.end (), [] (Tx const& x, Tx const& y) { return x.t0 < y.t0; });

  std::size_t const total = std::size_t (seconds * kRate);
  std::vector<float> buf (total, 0.0f);
  std::normal_distribution<float> N (0.0f, 1.0f);
  for (auto& s : buf) s = N (rng);
  for (auto const& t : txs)
    {
      auto const enc = decodium::txmsg::encodeFt2 (QString::fromStdString (t.msg));
      if (!enc.ok || enc.tones.isEmpty ()) continue;
      QVector<float> const w = decodium::txwave::generateFt2Wave (enc.tones.constData (), enc.tones.size (), kNsps,
                                                                  float (kRate), float (t.freq));
      // SNR nella banda di 2500 Hz, come sjtty/ft8sim: rumore di varianza 1.
      float const amp = float (std::sqrt (2.0 * 2500.0 / 6000.0) * std::pow (10.0, t.snr / 20.0));
      // Frazione di campione: l'inizio non e' allineato a niente.
      std::size_t const i0 = std::size_t (t.t0 * kRate);
      for (int i = 0; i < w.size () && i0 + std::size_t (i) < total; ++i) buf[i0 + std::size_t (i)] += amp * w[i];
    }
  std::vector<short> pcm (total);
  float const gain = 1000.0f;
  for (std::size_t i = 0; i < total; ++i)
    pcm[i] = short (std::lround (std::max (-32767.0f, std::min (32767.0f, gain * buf[i]))));
  if (!write_wav (a.pos[0], pcm)) return 1;
  std::ofstream tf (a.pos[1]);
  for (auto const& t : txs)
    {
      char line[200];
      std::snprintf (line, sizeof line, "%.4f %.1f %.1f %s\n", t.t0, t.freq, t.snr, t.msg.c_str ());
      tf << line;
    }
  std::printf ("scena: %.0f s, %zu trasmissioni -> %s\n", seconds, txs.size (), a.pos[0].c_str ());
  return 0;
}

// ------------------------------------------------------------------ QSO ASYMX
//
// genqso: una sequenza di scambi. La nostra trasmissione occupa 2,47 s (qui
// solo rumore: in half-duplex non riceviamo), il corrispondente risponde a
// una frequenza nota con una latenza fra lat-min e lat-max dalla fine della
// nostra. Con probabilita' 1-presence non risponde: le finestre attese vuote
// misurano i falsi. Il file delle attese ha una riga per scambio:
//   t_fine_tx  frequenza  nominativo_corrispondente  presente(0/1)

// genpair: coppie sovrapposte, un segnale forte e uno debole vicini in
// frequenza (dfmin..dfmax Hz) e in tempo (fino a +/-dtmax s). Scrive DUE file
// audio con lo stesso rumore e gli stessi deboli: alone.wav senza i forti e
// pair.wav con i forti. La differenza fra le decodifiche dei deboli nei due
// casi e' quello che la sovrapposizione fa perdere (il bersaglio di F4).
//   genpair alone.wav alone.txt pair.wav pair.txt [--seconds=900] [--seed=1]
//     [--weak-min=-16] [--weak-max=-10] [--strong-min=-8] [--strong-max=-2]
//     [--dfmin=5] [--dfmax=50] [--dtmax=1.0]

namespace
{
void add_tx (std::vector<float>& buf, Tx const& t)
{
  auto const enc = decodium::txmsg::encodeFt2 (QString::fromStdString (t.msg));
  if (!enc.ok || enc.tones.isEmpty ()) return;
  QVector<float> const w = decodium::txwave::generateFt2Wave (enc.tones.constData (), enc.tones.size (), kNsps,
                                                              float (kRate), float (t.freq));
  float const amp = float (std::sqrt (2.0 * 2500.0 / 6000.0) * std::pow (10.0, t.snr / 20.0));
  std::size_t const i0 = std::size_t (t.t0 * kRate);
  for (int i = 0; i < w.size () && i0 + std::size_t (i) < buf.size (); ++i) buf[i0 + std::size_t (i)] += amp * w[i];
}

void save_scene (std::string const& wav, std::string const& txt, std::vector<float> const& buf,
                 std::vector<Tx> const& txs)
{
  std::vector<short> pcm (buf.size ());
  for (std::size_t i = 0; i < buf.size (); ++i)
    pcm[i] = short (std::lround (std::max (-32767.0f, std::min (32767.0f, 1000.0f * buf[i]))));
  write_wav (wav, pcm);
  std::ofstream tf (txt);
  for (auto const& t : txs)
    {
      char line[200];
      std::snprintf (line, sizeof line, "%.4f %.1f %.1f %s\n", t.t0, t.freq, t.snr, t.msg.c_str ());
      tf << line;
    }
}
}

int genpair (Args const& a)
{
  if (a.pos.size () < 4)
    {
      std::fprintf (stderr, "genpair alone.wav alone.txt pair.wav pair.txt [opzioni]\n");
      return 2;
    }
  double const seconds = a.num ("seconds", 900);
  std::mt19937 rng (unsigned (a.num ("seed", 1)));
  std::uniform_real_distribution<double> U (0.0, 1.0);
  double const wmin = a.num ("weak-min", -16), wmax = a.num ("weak-max", -10);
  double const smin = a.num ("strong-min", -8), smax = a.num ("strong-max", -2);
  double const dfmin = a.num ("dfmin", 5), dfmax = a.num ("dfmax", 50), dtmax = a.num ("dtmax", 1.0);
  std::size_t const total = std::size_t (seconds * kRate);
  std::vector<float> buf (total);
  std::normal_distribution<float> N (0.0f, 1.0f);
  for (auto& s : buf) s = N (rng);
  std::vector<Tx> weak, strong;
  // Una coppia ogni ~6 s, frequenze sparse in banda: le coppie non si toccano fra loro.
  for (double t = 2.0; t + 2 * kTxSeconds + dtmax + 1.0 < seconds; t += 5.0 + U (rng) * 2.0)
    {
      Tx w, s;
      w.t0 = t + dtmax;
      w.freq = 500.0 + U (rng) * 2000.0;
      w.snr = wmin + U (rng) * (wmax - wmin);
      w.msg = random_message (rng);
      double const df = (dfmin + U (rng) * (dfmax - dfmin)) * (U (rng) < 0.5 ? -1.0 : 1.0);
      s.t0 = w.t0 + (U (rng) * 2.0 - 1.0) * dtmax;
      s.freq = w.freq + df;
      s.snr = smin + U (rng) * (smax - smin);
      s.msg = random_message (rng);
      weak.push_back (w);
      strong.push_back (s);
    }
  for (auto const& w : weak) add_tx (buf, w);
  save_scene (a.pos[0], a.pos[1], buf, weak);
  for (auto const& s : strong) add_tx (buf, s);
  std::vector<Tx> both = weak;
  both.insert (both.end (), strong.begin (), strong.end ());
  std::sort (both.begin (), both.end (), [] (Tx const& x, Tx const& y) { return x.t0 < y.t0; });
  save_scene (a.pos[2], a.pos[3], buf, both);
  std::printf ("genpair: %zu coppie\n", weak.size ());
  return 0;
}

// subres: quanto cancella la sottrazione del decoder (ftx_subtract_ft2_c) un
// segnale FT2 senza rumore, con dt e frequenza esatti e con gli errori che il
// decoder commette davvero (dt a passi di 1/1333,33 s, frequenza stimata).
// Il residuo e' in dB rispetto all'energia del segnale.
//   subres [--trials=40] [--seed=1]
int subres (Args const& a)
{
  int const trials = int (a.num ("trials", 40));
  std::mt19937 rng (unsigned (a.num ("seed", 1)));
  std::uniform_real_distribution<double> U (0.0, 1.0);
  std::vector<double> const dterr {0, 1, 2, 4.5, 9};
  std::vector<double> const dferr {0, 0.25, 0.5, 1.0, 2.0};
  std::vector<double> acc (dterr.size () * dferr.size (), 0.0);
  std::map<int, int> offsets;
  for (int k = 0; k < trials; ++k)
    {
      auto const enc = decodium::txmsg::encodeFt2 (QString::fromStdString (random_message (rng)));
      if (!enc.ok || enc.tones.size () < kSymbols) { --k; continue; }
      double const f = 500.0 + U (rng) * 2000.0;
      int const s0 = 3000 + int (U (rng) * 6000);
      QVector<float> const w = decodium::txwave::generateFt2Wave (enc.tones.constData (), enc.tones.size (), kNsps,
                                                                  float (kRate), float (f));
      std::vector<float> sig (kWindow, 0.0f);
      for (int i = 0; i < w.size () && s0 + i < kWindow; ++i) sig[std::size_t (s0 + i)] = 1000.0f * w[i];
      double e0 = 0;
      for (float v : sig) e0 += double (v) * v;
      auto residual = [&] (double dt, double f0) {
        std::vector<float> dd (sig);
        ftx_subtract_ft2_c (dd.data (), enc.tones.constData (), float (f0), float (dt));
        double e = 0;
        for (float v : dd) e += double (v) * v;
        return 10.0 * std::log10 (std::max (e / e0, 1e-12));
      };
      // convenzione di dt: il minimo del residuo al campione
      int best = 0;
      double rbest = 1e9;
      for (int off = -600; off <= 600; ++off)
        {
          double const r = residual ((s0 + off) / double (kRate), f);
          if (r < rbest) { rbest = r; best = off; }
        }
      ++offsets[best];
      for (std::size_t i = 0; i < dterr.size (); ++i)
        for (std::size_t j = 0; j < dferr.size (); ++j)
          acc[i * dferr.size () + j] += residual ((s0 + best + dterr[i]) / double (kRate), f + dferr[j]);
    }
  std::printf ("offset di dt al minimo (campioni):");
  for (auto const& o : offsets) std::printf (" %d x%d", o.first, o.second);
  std::printf ("\nresiduo medio (dB) dopo la sottrazione, %d segnali\n  errore dt / errore f:", trials);
  for (double d : dferr) std::printf (" %7.2f Hz", d);
  std::printf ("\n");
  for (std::size_t i = 0; i < dterr.size (); ++i)
    {
      std::printf ("  %5.1f camp. (%4.2f ms)  ", dterr[i], dterr[i] / 12.0);
      for (std::size_t j = 0; j < dferr.size (); ++j) std::printf (" %10.1f", acc[i * dferr.size () + j] / trials);
      std::printf ("\n");
    }
  return 0;
}

std::string canon (char const* p, int n);

// win: decodifica statica, fuori dal tempo simulato. Per ogni trasmissione di
// truth.txt prova 5 finestre che la contengono intera (fine finestra da 0,1 a
// 1,2 s dopo la fine della TX) su DUE file audio, e dice in quante la
// decodifica. Serve a separare le perdite del decoder da quelle dei tempi.
//   win a.wav b.wav truth.txt [--nfa=200] [--nfb=4000] [--nfqso=1500] [--depth=3]
//     [--sub=forti.txt] : terza colonna C = b.wav con i segnali di forti.txt
//     sottratti PRIMA della decodifica (come farebbe F4 con un segnale gia'
//     decodificato altrove), con l'errore del decoder: dt a passi di
//     1/1333,33 s e frequenza sbagliata fino a 0,5 Hz.
int win (Args const& a)
{
  if (a.pos.size () < 3)
    {
      std::fprintf (stderr, "win a.wav b.wav truth.txt [opzioni]\n");
      return 2;
    }
  std::vector<short> wa, wb;
  if (!read_wav (a.pos[0], wa) || !read_wav (a.pos[1], wb)) return 1;
  std::vector<Tx> truth;
  {
    std::ifstream tf (a.pos[2]);
    std::string line;
    while (std::getline (tf, line))
      {
        std::istringstream is (line);
        Tx t;
        if (!(is >> t.t0 >> t.freq >> t.snr)) continue;
        std::getline (is >> std::ws, t.msg);
        while (!t.msg.empty () && (t.msg.back () == '\r' || t.msg.back () == ' ')) t.msg.pop_back ();
        t.msg = canon (t.msg.c_str (), int (t.msg.size ()));
        truth.push_back (t);
      }
  }
  std::vector<Tx> subs;
  if (a.kv.count ("sub"))
    {
      std::ifstream sf (a.kv.at ("sub"));
      std::string line;
      while (std::getline (sf, line))
        {
          std::istringstream is (line);
          Tx t;
          if (!(is >> t.t0 >> t.freq >> t.snr)) continue;
          std::getline (is >> std::ws, t.msg);
          while (!t.msg.empty () && (t.msg.back () == '\r' || t.msg.back () == ' ')) t.msg.pop_back ();
          subs.push_back (t);
        }
    }
  std::mt19937 err_rng (7);
  std::uniform_real_distribution<double> Uerr (-1.0, 1.0);
  int nfa = int (a.num ("nfa", 200)), nfb = int (a.num ("nfb", 4000)), nfqso = int (a.num ("nfqso", 1500));
  int ndepth = int (a.num ("depth", 3));
  QByteArray const my = QByteArray ("IU8LMC").leftJustified (12, ' ', true);
  QByteArray const his = QByteArray ().leftJustified (12, ' ', true);
  double const ends[] = {0.1, 0.4, 0.7, 1.0, 1.2};
  auto decode = [&] (std::vector<short> const& audio, double end_s, std::string const& msg, bool pre_sub) {
    std::int64_t const pos = std::int64_t (end_s * kRate);
    if (pos < kWindow || pos > std::int64_t (audio.size ())) return false;
    std::vector<short> window (audio.begin () + (pos - kWindow), audio.begin () + pos);
    if (pre_sub)
      {
        double const w0 = double (pos - kWindow) / kRate;
        std::vector<float> dd (window.begin (), window.end ());
        for (auto const& st : subs)
          {
            if (st.t0 + kTxSeconds <= w0 || st.t0 >= end_s) continue;
            auto const enc = decodium::txmsg::encodeFt2 (QString::fromStdString (st.msg));
            if (!enc.ok || enc.tones.size () < kSymbols) continue;
            // dt del decoder: inizio nella finestra + 288 campioni, a passi di 9
            double const dt = std::round (((st.t0 - w0) * kRate + kNsps) / 9.0) * 9.0 / kRate;
            ftx_subtract_ft2_c (dd.data (), enc.tones.constData (), float (st.freq + 0.5 * Uerr (err_rng)), float (dt));
          }
        for (int i = 0; i < kWindow; ++i)
          window[std::size_t (i)] = short (std::lround (std::max (-32767.0f, std::min (32767.0f, dd[std::size_t (i)]))));
      }
    int snrs[kMaxLines] {}, naps[kMaxLines] {}, nout = 0, nqso = 0, ncontest = 0;
    float dts[kMaxLines] {}, freqs[kMaxLines] {}, quals[kMaxLines] {};
    signed char bits77[kMaxLines * 77] {};
    char decodeds[kMaxLines * 37] {};
    ftx_ft2_stage7_clravg_c ();
    ftx_ft2_async_decode_stage7_c (window.data (), &nqso, &nfqso, &nfa, &nfb, &ndepth, &ncontest,
                                   my.constData (), his.constData (), snrs, dts, freqs, naps, quals,
                                   bits77, decodeds, &nout);
    for (int i = 0; i < nout; ++i)
      if (canon (decodeds + i * 37, 37) == msg) return true;
    return false;
  };
  int ta = 0, tb = 0, sa = 0, sb = 0, tc = 0, sc = 0;
  for (auto const& t : truth)
    {
      int ha = 0, hb = 0, hc = 0;
      std::string bits_a, bits_b, bits_c;
      for (double e : ends)
        {
          double const end_s = t.t0 + kTxSeconds + e;
          bool const da = decode (wa, end_s, t.msg, false), db = decode (wb, end_s, t.msg, false);
          bool const dc = !subs.empty () && decode (wb, end_s, t.msg, true);
          hc += dc;
          bits_c += dc ? '1' : '0';
          ha += da;
          hb += db;
          bits_a += da ? '1' : '0';
          bits_b += db ? '1' : '0';
        }
      ta += ha; tb += hb; tc += hc; sa += ha > 0; sb += hb > 0; sc += hc > 0;
      std::printf ("%9.3f %7.1f %6.1f  A %d/5  B %d/5  C %d/5  %s %s %s  %s\n", t.t0, t.freq, t.snr, ha, hb, hc, bits_a.c_str (), bits_b.c_str (), bits_c.c_str (), t.msg.c_str ());
      std::fflush (stdout);
    }
  std::printf ("TOTALE %zu trasmissioni: A prese %d (finestre %d), B prese %d (finestre %d), C prese %d (finestre %d)\n",
               truth.size (), sa, ta, sb, tb, sc, tc);
  return 0;
}

// dtcheck: un segnale a inizio noto nella finestra; confronta il DT della riga
// con quello che F4 usa per ritrovarlo (inizio + 288 campioni = DT + 0,5 s) e
// misura quanto resta dopo averlo sottratto con i bit decodificati.
int dtcheck (Args const& a)
{
  std::mt19937 rng (unsigned (a.num ("seed", 3)));
  std::normal_distribution<float> N (0.0f, 1.0f);
  std::uniform_real_distribution<double> U (0.0, 1.0);
  int const trials = int (a.num ("trials", 10));
  for (int k = 0; k < trials; ++k)
    {
      std::string const msg = random_message (rng);
      auto const enc = decodium::txmsg::encodeFt2 (QString::fromStdString (msg));
      double const f = 800.0 + U (rng) * 1500.0;
      int const s0 = int (a.num ("s0", -1)) >= 0 ? int (a.num ("s0", 0)) : 2000 + int (U (rng) * 12000);
      QVector<float> const w = decodium::txwave::generateFt2Wave (enc.tones.constData (), enc.tones.size (), kNsps,
                                                                  float (kRate), float (f));
      std::vector<float> buf (kWindow);
      for (auto& v : buf) v = N (rng);
      float const amp = float (std::sqrt (2.0 * 2500.0 / 6000.0) * std::pow (10.0, -5.0 / 20.0));
      for (int i = 0; i < w.size () && s0 + i < kWindow; ++i) buf[std::size_t (s0 + i)] += amp * w[i];
      std::vector<short> win (kWindow);
      for (int i = 0; i < kWindow; ++i) win[std::size_t (i)] = short (std::lround (1000.0f * buf[std::size_t (i)]));
      decodium::ft2::AsyncDecodeOut o;
      int nq = 0, nf = 1500, a1 = 200, b1 = 4000, nd = 3, nc = 0;
      QByteArray const my = QByteArray ("IU8LMC").leftJustified (12, ' ', true);
      QByteArray const his = QByteArray ().leftJustified (12, ' ', true);
      ftx_ft2_stage7_clravg_c ();
      ftx_ft2_async_decode_stage7_c (win.data (), &nq, &nf, &a1, &b1, &nd, &nc, my.constData (), his.constData (),
                                     o.snrs, o.dts, o.freqs, o.naps, o.quals, o.bits77, o.decodeds, &o.nout);
      for (int i = 0; i < o.nout; ++i)
        {
          if (canon (o.decodeds + i * 37, 37) != canon (msg.c_str (), int (msg.size ()))) continue;
          double const atteso = (s0 + kNsps) / double (kRate) - 0.5;
          // residuo: sottrae con i bit e la stima del decoder
          std::vector<float> dd (win.begin (), win.end ()), sig (kWindow, 0.0f);
          for (int j = 0; j < w.size () && s0 + j < kWindow; ++j) sig[std::size_t (s0 + j)] = 1000.0f * amp * w[j];
          ftx_ft2_sottrai_bits77_c (dd.data (), o.bits77 + i * 77, o.freqs[i], o.dts[i] + 0.5f);
          double e_sig = 0, e_res = 0;
          for (int j = 0; j < kWindow; ++j)
            {
              double const r = dd[std::size_t (j)] - (win[std::size_t (j)] - sig[std::size_t (j)]);
              e_res += r * r;
              e_sig += double (sig[std::size_t (j)]) * sig[std::size_t (j)];
            }
          std::printf ("s0=%5d f=%7.1f  dt riga %.4f  atteso %.4f  diff %+6.1f campioni  df %+5.2f Hz  residuo %.1f dB\n",
                       s0, f, o.dts[i], atteso, (o.dts[i] - atteso) * kRate, o.freqs[i] - f,
                       10.0 * std::log10 (std::max (e_res / e_sig, 1e-12)));
        }
    }
  return 0;
}

// danno: un forte e un debole (-10 dB rispetto al forte) vicini; si sottrae il
// forte con dt e frequenza esatti e si misura quanto la sottrazione rovina il
// debole: errore rispetto al debole da solo, in dB sull'energia del debole.
//   danno [--trials=60] [--dfmin=5] [--dfmax=50]
int danno (Args const& a)
{
  int const trials = int (a.num ("trials", 60));
  double const dfmin = a.num ("dfmin", 5), dfmax = a.num ("dfmax", 50);
  std::mt19937 rng (unsigned (a.num ("seed", 5)));
  std::uniform_real_distribution<double> U (0.0, 1.0);
  double acc = 0, acc_res = 0;
  std::map<int, std::pair<double, int>> per_df;
  for (int k = 0; k < trials; ++k)
    {
      auto const es = decodium::txmsg::encodeFt2 (QString::fromStdString (random_message (rng)));
      auto const ew = decodium::txmsg::encodeFt2 (QString::fromStdString (random_message (rng)));
      double const fs = 800.0 + U (rng) * 1500.0;
      double const df = (dfmin + U (rng) * (dfmax - dfmin)) * (U (rng) < 0.5 ? -1.0 : 1.0);
      int const s0 = 6000 + int (U (rng) * 4000);
      int const w0 = s0 + int ((U (rng) * 2.0 - 1.0) * 5000);
      QVector<float> const ws = decodium::txwave::generateFt2Wave (es.tones.constData (), es.tones.size (), kNsps,
                                                                   float (kRate), float (fs));
      QVector<float> const ww = decodium::txwave::generateFt2Wave (ew.tones.constData (), ew.tones.size (), kNsps,
                                                                   float (kRate), float (fs + df));
      std::vector<float> weak (kWindow, 0.0f), mix (kWindow, 0.0f);
      float const aw = float (1000.0 * std::pow (10.0, -10.0 / 20.0));
      for (int i = 0; i < ww.size () && w0 + i < kWindow; ++i) if (w0 + i >= 0) weak[std::size_t (w0 + i)] = aw * ww[i];
      for (int i = 0; i < kWindow; ++i) mix[std::size_t (i)] = weak[std::size_t (i)];
      for (int i = 0; i < ws.size () && s0 + i < kWindow; ++i) mix[std::size_t (s0 + i)] += 1000.0f * ws[i];
      std::vector<float> only (kWindow, 0.0f);
      for (int i = 0; i < ws.size () && s0 + i < kWindow; ++i) only[std::size_t (s0 + i)] = 1000.0f * ws[i];
      float const dt = float ((s0 + kNsps) / double (kRate));
      ftx_subtract_ft2_c (mix.data (), es.tones.constData (), float (fs), dt);
      ftx_subtract_ft2_c (only.data (), es.tones.constData (), float (fs), dt);
      double ew2 = 0, err = 0, res = 0, es2 = 0;
      for (int i = 0; i < kWindow; ++i)
        {
          double const e = mix[std::size_t (i)] - weak[std::size_t (i)];
          err += e * e;
          ew2 += double (weak[std::size_t (i)]) * weak[std::size_t (i)];
          res += double (only[std::size_t (i)]) * only[std::size_t (i)];
        }
      es2 = ew2 * 10.0;   // il forte ha 10 dB in piu'
      double const d = 10.0 * std::log10 (err / ew2);
      acc += d;
      acc_res += 10.0 * std::log10 (std::max (res / es2, 1e-12));
      auto& b = per_df[int (std::fabs (df) / 10) * 10];
      b.first += d;
      ++b.second;
    }
  std::printf ("errore sul debole dopo la sottrazione del forte: media %.1f dB (residuo del forte da solo %.1f dB)\n",
               acc / trials, acc_res / trials);
  for (auto const& b : per_df)
    std::printf ("  |df| %2d-%2d Hz: %.1f dB (%d)\n", b.first, b.first + 10, b.second.first / b.second.second, b.second.second);
  return 0;
}

// deriva: residuo della sottrazione di un segnale che deriva in frequenza
// (r Hz/s, centrato: a meta' trasmissione e' alla frequenza nominale), con la
// frequenza nominale e il dt esatti. La deriva si applica al segnale analitico
// (trasformata di Hilbert via FFT).
//   deriva [--trials=20]
int deriva (Args const& a)
{
  int const trials = int (a.num ("trials", 20));
  std::mt19937 rng (unsigned (a.num ("seed", 9)));
  std::uniform_real_distribution<double> U (0.0, 1.0);
  std::vector<double> const rates {0.0, 0.25, 0.5, 1.0, 2.0, 4.0};
  std::vector<double> acc (rates.size (), 0.0);
  int const n = kWindow;
  fftwf_complex* buf = fftwf_alloc_complex (std::size_t (n));
  fftwf_plan fw = fftwf_plan_dft_1d (n, buf, buf, FFTW_FORWARD, FFTW_ESTIMATE);
  fftwf_plan bw = fftwf_plan_dft_1d (n, buf, buf, FFTW_BACKWARD, FFTW_ESTIMATE);
  for (int k = 0; k < trials; ++k)
    {
      auto const enc = decodium::txmsg::encodeFt2 (QString::fromStdString (random_message (rng)));
      double const f = 800.0 + U (rng) * 1500.0;
      int const s0 = 3000 + int (U (rng) * 6000);
      QVector<float> const w = decodium::txwave::generateFt2Wave (enc.tones.constData (), enc.tones.size (), kNsps,
                                                                  float (kRate), float (f));
      for (std::size_t r = 0; r < rates.size (); ++r)
        {
          for (int i = 0; i < n; ++i) { buf[i][0] = 0.0f; buf[i][1] = 0.0f; }
          for (int i = 0; i < w.size () && s0 + i < n; ++i) buf[s0 + i][0] = 1000.0f * w[i];
          fftwf_execute (fw);
          for (int i = 1; i < n / 2; ++i) { buf[i][0] *= 2.0f; buf[i][1] *= 2.0f; }
          for (int i = n / 2 + 1; i < n; ++i) { buf[i][0] = 0.0f; buf[i][1] = 0.0f; }
          fftwf_execute (bw);
          double const tmid = (s0 + w.size () / 2.0) / kRate;
          std::vector<float> sig (std::size_t (n), 0.0f);
          double e0 = 0;
          for (int i = 0; i < n; ++i)
            {
              double const tt = double (i) / kRate - tmid;
              double const ph = 2.0 * M_PI * 0.5 * rates[r] * tt * tt;
              double const re = buf[i][0] / n, im = buf[i][1] / n;
              sig[std::size_t (i)] = float (re * std::cos (ph) - im * std::sin (ph));
              e0 += double (sig[std::size_t (i)]) * sig[std::size_t (i)];
            }
          ftx_subtract_ft2_c (sig.data (), enc.tones.constData (), float (f), float ((s0 + kNsps) / double (kRate)));
          double e = 0;
          for (float v : sig) e += double (v) * v;
          acc[r] += 10.0 * std::log10 (std::max (e / e0, 1e-12));
        }
    }
  fftwf_destroy_plan (fw);
  fftwf_destroy_plan (bw);
  fftwf_free (buf);
  for (std::size_t r = 0; r < rates.size (); ++r)
    std::printf ("  deriva %4.2f Hz/s (%.1f Hz sul frame): residuo %.1f dB\n", rates[r], rates[r] * kTxSeconds,
                 acc[r] / trials);
  return 0;
}

int genqso (Args const& a)
{
  if (a.pos.size () < 3)
    {
      std::fprintf (stderr, "genqso out.wav truth.txt expect.txt [--seconds=600] [--seed=1] [--snr-min=-24]\n"
                            "    [--snr-max=-12] [--presence=0.8] [--lat-min=0.3] [--lat-max=0.9] [--mycall=IU8LMC]\n");
      return 2;
    }
  double const seconds = a.num ("seconds", 600);
  std::mt19937 rng (unsigned (a.num ("seed", 1)));
  std::uniform_real_distribution<double> U (0.0, 1.0);
  double const snr_min = a.num ("snr-min", -24), snr_max = a.num ("snr-max", -12);
  double const presence = a.num ("presence", 0.8);
  double const lat_min = a.num ("lat-min", 0.3), lat_max = a.num ("lat-max", 0.9);
  std::string const mycall = a.get ("mycall", "IU8LMC");
  std::size_t const total = std::size_t (seconds * kRate);
  std::vector<float> buf (total);
  std::normal_distribution<float> N (0.0f, 1.0f);
  for (auto& s : buf) s = N (rng);
  std::ofstream tf (a.pos[1]), ef (a.pos[2]);
  int n = 0, present = 0;
  double t = 1.0;
  while (true)
    {
      double const t_end = t + kTxSeconds;
      double const lat = lat_min + U (rng) * (lat_max - lat_min);
      double const t0 = t_end + lat;
      if (t0 + kTxSeconds + 1.0 > seconds) break;
      std::string const call = random_call (rng);
      double const f = 400.0 + U (rng) * 2200.0;
      bool const here = U (rng) < presence;
      ef << std::to_string (t_end) << " " << std::to_string (f) << " " << call << " " << (here ? 1 : 0) << "\n";
      if (here)
        {
          double const snr = snr_min + U (rng) * (snr_max - snr_min);
          char rep[8];
          std::snprintf (rep, sizeof rep, "%+03d", int (std::lround (snr)));
          int const k = int (U (rng) * 3);
          std::string const msg = k == 0 ? mycall + " " + call + " " + rep
                                : k == 1 ? mycall + " " + call + " R" + rep
                                         : mycall + " " + call + " RR73";
          auto const enc = decodium::txmsg::encodeFt2 (QString::fromStdString (msg));
          QVector<float> const w = decodium::txwave::generateFt2Wave (enc.tones.constData (), enc.tones.size (), kNsps,
                                                                      float (kRate), float (f));
          float const amp = float (std::sqrt (2.0 * 2500.0 / 6000.0) * std::pow (10.0, snr / 20.0));
          std::size_t const i0 = std::size_t (t0 * kRate);
          for (int i = 0; i < w.size () && i0 + std::size_t (i) < total; ++i) buf[i0 + std::size_t (i)] += amp * w[i];
          char line[200];
          std::snprintf (line, sizeof line, "%.4f %.1f %.1f %s\n", t0, f, snr, msg.c_str ());
          tf << line;
          ++present;
        }
      ++n;
      // prossimo scambio: dopo la risposta (o il suo posto) e un po' di pausa
      t = t0 + kTxSeconds + 0.5 + U (rng) * 1.5;
    }
  std::vector<short> pcm (total);
  for (std::size_t i = 0; i < total; ++i)
    pcm[i] = short (std::lround (std::max (-32767.0f, std::min (32767.0f, 1000.0f * buf[i]))));
  write_wav (a.pos[0], pcm);
  std::printf ("genqso: %d scambi, %d risposte presenti\n", n, present);
  return 0;
}

struct Expect
{
  double t_end {0};
  double freq {0};
  std::string call;
  bool present {false};
};

std::vector<Expect> read_expect (std::string const& path)
{
  std::vector<Expect> v;
  std::ifstream f (path);
  Expect e;
  int p;
  while (f >> e.t_end >> e.freq >> e.call >> p)
    {
      e.present = p != 0;
      v.push_back (e);
    }
  return v;
}

// ------------------------------------------------------------------ simulazione

struct Shown
{
  double t_show {0};       // istante (s) in cui la riga compare nella lista
  double freq {0};
  int snr {0};
  float dt {0};
  std::string msg;
  int truth {-1};          // indice della trasmissione vera, -1 = falso
};

std::string canon (char const* p, int n)
{
  std::string s (p, std::size_t (n));
  // simplified(): spazi compressi, niente in testa e in coda.
  std::string out;
  bool sp = false;
  for (char c : s)
    {
      if (c == ' ' || c == '\t')
        {
          sp = !out.empty ();
          continue;
        }
      if (sp) out += ' ';
      sp = false;
      out += c;
    }
  return out;
}

std::vector<Tx> read_truth (std::string const& path)
{
  std::vector<Tx> v;
  std::ifstream f (path);
  std::string line;
  while (std::getline (f, line))
    {
      std::istringstream is (line);
      Tx t;
      if (!(is >> t.t0 >> t.freq >> t.snr)) continue;
      std::getline (is, t.msg);
      t.msg = canon (t.msg.c_str (), int (t.msg.size ()));
      v.push_back (t);
    }
  return v;
}

int run (Args const& a)
{
  if (a.pos.empty ())
    {
      std::fprintf (stderr, "run in.wav [truth.txt] [--depth=3] [--tick=100] [--mycall=IU8LMC] [--hiscall=]\n"
                            "    [--nfa=200] [--nfb=4000] [--nfqso=1500] [--speed=1] (moltiplica i tempi CPU)\n"
                            "    [--rows=file] (tutte le righe mostrate) [--label=nome]\n");
      return 2;
    }
  std::vector<short> audio;
  if (!read_wav (a.pos[0], audio))
    {
      std::fprintf (stderr, "non leggo %s\n", a.pos[0].c_str ());
      return 1;
    }
  std::vector<Tx> truth;
  if (a.pos.size () > 1) truth = read_truth (a.pos[1]);

  int ndepth = int (a.num ("depth", 3));
  int const tick_ms = int (a.num ("tick", 100));
  double const cpu_scale = a.num ("speed", 1.0);
  int nfa = int (a.num ("nfa", 200)), nfb = int (a.num ("nfb", 4000)), nfqso = int (a.num ("nfqso", 1500));
  QByteArray my = QByteArray::fromStdString (a.get ("mycall", "IU8LMC")).leftJustified (12, ' ', true);
  QByteArray his = QByteArray::fromStdString (a.get ("hiscall", "")).leftJustified (12, ' ', true);
  // --registro=1: doppioni riconosciuti per istante assoluto e frequenza (F2)
  // invece che per slot di 3,75 s.
  bool const registro = a.num ("registro", 0) != 0;
  decodium::ft2::AsyncRegistry<std::string> registry;
  // --incr=1: F3, a ogni giro solo gli inizi di frame completati dall'ultimo
  // giro. --incr-lead=s ammette frame ancora mancanti degli ultimi s secondi.
  bool const incremental = a.num ("incr", 0) != 0;
  double const incr_lead = a.num ("incr-lead", 0.0);
  double last_dispatch_t = -1.0;
  // --expect=file: QSO ASYMX. Durante ogni scambio hiscall e' il corrispondente
  // (AP come nell'app); con --f5=1 si indica anche la finestra di tempo in cui
  // la risposta deve cominciare (fine della nostra TX + lat-min..lat-max).
  std::vector<Expect> expects;
  if (!a.get ("expect", "").empty ()) expects = read_expect (a.get ("expect", ""));
  bool const f5 = a.num ("f5", 0) != 0;
  // --dispatch-log=file: fine finestra di ogni giro (secondi), per le analisi
  std::ofstream dispatch_log;
  if (!a.get ("dispatch-log", "").empty ()) dispatch_log.open (a.get ("dispatch-log", ""));
  double const f5_lo = a.num ("f5-lo", 0.2), f5_hi = a.num ("f5-hi", 1.0);
  int const qso_progress = int (a.num ("qsoprog", 3));
  // --avanti=1: F4, sottrazione dei segnali gia' decodificati che la finestra
  // taglia (Detector/Ft2AsyncSottrazione.hpp, lo stesso codice del worker).
  bool const avanti = a.num ("avanti", 0) != 0;
  decodium::ft2::AsyncSottrazione sottrazione;

  ftx_ft2_stage7_clravg_c ();

  double const total_s = double (audio.size ()) / kRate;
  double busy_until = -1.0;
  std::int64_t last_pos = -1;
  int ticks = 0, dispatched = 0, skipped_busy = 0;
  std::vector<double> cpu_ms;
  std::set<std::string> dedup;
  std::vector<Shown> shown;
  int raw_rows = 0;

  std::vector<short> window (kWindow);
  for (double t = tick_ms / 1000.0; t <= total_s; t += tick_ms / 1000.0)
    {
      ++ticks;
      if (t < busy_until)
        {
          ++skipped_busy;
          continue;
        }
      std::int64_t const pos = std::int64_t (t * kRate);
      if (pos < kWindow || pos == last_pos) continue;
      last_pos = pos;
      std::copy (audio.begin () + (pos - kWindow), audio.begin () + pos, window.begin ());

      // Il token di slot del bridge: finestra che inizia 3,75 s fa, arrotondata.
      std::int64_t const now_ms = std::int64_t (std::llround (t * 1000.0));
      std::int64_t const slot = (now_ms - kPeriodMs + kPeriodMs / 2) / kPeriodMs;

      decodium::ft2::AsyncDecodeOut dec;
      int nqso = 0, ncontest = 0;
      int& nout = dec.nout;
      int* const snrs = dec.snrs;
      float* const dts = dec.dts;
      float* const freqs = dec.freqs;
      char* const decodeds = dec.decodeds;
      QByteArray his_now = his;
      int nfqso_now = nfqso;
      ftx_ft2_set_async_expected_c (0.0f, 0, -1);
      if (!expects.empty ())
        {
          // lo scambio in corso: l'ultimo la cui TX e' finita e la cui
          // risposta puo' ancora essere nella finestra
          for (auto const& e : expects)
            if (e.t_end <= t && t <= e.t_end + f5_hi + kTxSeconds + 4.0)
              {
                his_now = QByteArray::fromStdString (e.call).leftJustified (12, ' ', true);
                // come nell'app in QSO: RX sul corrispondente, progresso del
                // QSO avanzato (abilita l'AP con mycall+hiscall)
                nfqso_now = int (std::lround (e.freq));
                nqso = qso_progress;
                if (f5)
                  {
                    double const S = t - kWindow / double (kRate);
                    int const lo = std::max (-688, int (std::floor ((e.t_end + f5_lo - S) * 1333.33)));
                    int const hi = std::min (2024, int (std::ceil ((e.t_end + f5_hi - S) * 1333.33)));
                    if (lo <= hi) ftx_ft2_set_async_expected_c (float (e.freq), lo, hi);
                  }
              }
        }
      if (incremental)
        {
          double const tau_max = kWindow / double (kRate) - kTxSeconds + incr_lead;
          double const delta = last_dispatch_t < 0 ? 1.8 : std::min (1.8, t - last_dispatch_t);
          int const lo = std::max (-688, int (std::floor ((tau_max - delta - 0.02) * 1333.33)));
          int const hi = std::min (2024, int (std::ceil ((tau_max + 0.02) * 1333.33)));
          ftx_ft2_set_async_ib_range_c (lo, hi);
        }
      last_dispatch_t = t;
      if (dispatch_log.is_open ()) dispatch_log << t << '\n';
      auto const c0 = std::chrono::steady_clock::now ();
      auto chiama = [&] (short* iw, decodium::ft2::AsyncDecodeOut& o) {
        ftx_ft2_async_decode_stage7_c (iw, &nqso, &nfqso_now, &nfa, &nfb, &ndepth, &ncontest, my.constData (),
                                       his_now.constData (), o.snrs, o.dts, o.freqs, o.naps, o.quals, o.bits77,
                                       o.decodeds, &o.nout);
      };
      if (avanti)
        sottrazione.decodifica (window.data (), pos, dec, chiama);
      else
        chiama (window.data (), dec);
      double const ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now () - c0).count ()
          * cpu_scale;
      if (incremental) ftx_ft2_set_async_ib_range_c (0, -1);
      cpu_ms.push_back (ms);
      ++dispatched;
      busy_until = t + ms / 1000.0;
      double const t_ready = busy_until;
      raw_rows += nout;
      for (int i = 0; i < nout; ++i)
        {
          std::string const msg = canon (decodeds + i * 37, 37);
          if (msg.empty ()) continue;
          if (registro)
            {
              // Inizio assoluto: la finestra comincia 3,75 s prima del dispatch
              // e il DT della riga ha un decimale, come nel bridge.
              double const dt1 = std::round (double (dts[i]) * 10.0) / 10.0;
              double const start = t - kWindow / double (kRate) + dt1;
              if (!registry.admit (msg, std::round (freqs[i]), start, t)) continue;
            }
          else
            {
              int const fbucket = (int (freqs[i]) / 20) * 20;
              std::string const key = std::to_string (slot) + "|" + std::to_string (fbucket) + "|" + msg;
              if (!dedup.insert (key).second) continue;
            }
          Shown s;
          s.t_show = t_ready;
          s.freq = freqs[i];
          s.snr = snrs[i];
          s.dt = dts[i];
          s.msg = msg;
          shown.push_back (s);
        }
    }

  // Abbinamento alla verita': stesso testo, +/-10 Hz, mostrato fra la fine
  // della trasmissione e 6 s dopo.
  std::vector<int> hits (truth.size (), 0);
  std::vector<double> first_latency (truth.size (), -1.0);
  for (auto& s : shown)
    {
      int best = -1;
      double best_d = 1e9;
      for (std::size_t k = 0; k < truth.size (); ++k)
        {
          Tx const& t = truth[k];
          if (t.msg != s.msg || std::abs (t.freq - s.freq) > 10.0) continue;
          double const end = t.t0 + kTxSeconds;
          if (s.t_show < end - 0.05 || s.t_show > end + 6.0) continue;
          double const d = s.t_show - end;
          if (d < best_d)
            {
              best_d = d;
              best = int (k);
            }
        }
      s.truth = best;
      if (best >= 0)
        {
          ++hits[std::size_t (best)];
          if (first_latency[std::size_t (best)] < 0 || best_d < first_latency[std::size_t (best)])
            first_latency[std::size_t (best)] = best_d;
        }
    }

  int decoded = 0, duplicates = 0, falses = 0, lost_repeats = 0, repeats = 0;
  std::vector<double> lat;
  for (std::size_t k = 0; k < truth.size (); ++k)
    {
      if (hits[k] > 0)
        {
          ++decoded;
          duplicates += hits[k] - 1;
          lat.push_back (first_latency[k]);
        }
      // Ripetizione: stesso testo e frequenza di una trasmissione precedente vicina.
      for (std::size_t j = 0; j < k; ++j)
        if (truth[j].msg == truth[k].msg && std::abs (truth[j].freq - truth[k].freq) < 1.0
            && truth[k].t0 - truth[j].t0 < 6.0)
          {
            ++repeats;
            if (hits[k] == 0 && hits[j] > 0) ++lost_repeats;
          }
    }
  for (auto const& s : shown)
    if (s.truth < 0) ++falses;

  // Curva P(decode) per SNR a passi di 1 dB e soglia al 50%.
  std::map<int, std::pair<int, int>> curve;
  for (std::size_t k = 0; k < truth.size (); ++k)
    {
      int const b = int (std::floor (truth[k].snr + 0.5));
      curve[b].second++;
      if (hits[k] > 0) curve[b].first++;
    }
  double threshold = std::nan ("");
  {
    // interpolazione lineare sul primo attraversamento del 50% scendendo in SNR
    std::vector<std::pair<int, double>> pts;
    for (auto const& c : curve)
      if (c.second.second > 0) pts.push_back ({c.first, double (c.second.first) / c.second.second});
    for (std::size_t i = pts.size (); i-- > 1;)
      if (pts[i].second >= 0.5 && pts[i - 1].second < 0.5)
        {
          double const x0 = pts[i - 1].first, y0 = pts[i - 1].second, x1 = pts[i].first, y1 = pts[i].second;
          threshold = x0 + (0.5 - y0) * (x1 - x0) / (y1 - y0);
          break;
        }
  }
  std::sort (lat.begin (), lat.end ());
  auto pct = [] (std::vector<double> v, double p) {
    if (v.empty ()) return std::nan ("");
    std::sort (v.begin (), v.end ());
    return v[std::min (v.size () - 1, std::size_t (p * double (v.size () - 1) + 0.5))];
  };
  double cpu_mean = 0, cpu_max = 0;
  for (double c : cpu_ms)
    {
      cpu_mean += c;
      cpu_max = std::max (cpu_max, c);
    }
  if (!cpu_ms.empty ()) cpu_mean /= double (cpu_ms.size ());
  double const hours = total_s / 3600.0;

  std::string const label = a.get ("label", "");
  std::printf ("== %s %s depth=%d tick=%dms doppioni=%s  audio %.0f s\n", label.c_str (), a.pos[0].c_str (), ndepth,
               tick_ms, registro ? "registro" : "slot", total_s);
  std::printf ("tick %d, dispatch %d, saltati (worker occupato) %d\n", ticks, dispatched, skipped_busy);
  std::printf ("CPU per dispatch: media %.0f ms, p95 %.0f ms, max %.0f ms\n", cpu_mean, pct (cpu_ms, 0.95), cpu_max);
  if (avanti) std::printf ("F4 avanti: segnali presottratti %ld\n", sottrazione.presottratti ());
  std::printf ("righe grezze %d, mostrate %zu\n", raw_rows, shown.size ());
  if (!truth.empty ())
    {
      std::printf ("trasmissioni %zu, decodificate %d (%.1f%%), doppioni mostrati %d, falsi %d (%.1f/ora)\n",
                   truth.size (), decoded, 100.0 * decoded / double (truth.size ()), duplicates, falses,
                   hours > 0 ? falses / hours : 0.0);
      std::printf ("ripetizioni legittime %d, perse %d\n", repeats, lost_repeats);
      std::printf ("latenza (mostrata - fine TX): media %.2f s, p50 %.2f s, p95 %.2f s\n",
                   lat.empty () ? std::nan ("") : [&] { double s = 0; for (double x : lat) s += x; return s / lat.size (); } (),
                   pct (lat, 0.5), pct (lat, 0.95));
      std::printf ("soglia 50%%: %.1f dB\n", threshold);
      std::printf ("  SNR  prese/tot\n");
      for (auto const& c : curve)
        std::printf ("  %4d  %3d/%-3d %s\n", c.first, c.second.first, c.second.second,
                     std::string (std::size_t (20.0 * c.second.first / std::max (1, c.second.second)), '#').c_str ());
    }
  else
    std::printf ("senza verita': %zu righe mostrate (%.1f/ora) — su solo rumore sono tutte false\n", shown.size (),
                 hours > 0 ? shown.size () / hours : 0.0);

  std::string const rows_path = a.get ("rows", "");
  if (!rows_path.empty ())
    {
      std::ofstream rf (rows_path);
      for (auto const& s : shown)
        {
          char line[256];
          std::snprintf (line, sizeof line, "%8.3f %7.1f %4d %5.2f %s %s\n", s.t_show, s.freq, s.snr, s.dt,
                         s.truth >= 0 ? "OK   " : (s.truth == -1 ? "FALSO" : "?"), s.msg.c_str ());
          rf << line;
        }
    }
  return 0;
}

}

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};
  if (argc < 2)
    {
      std::fprintf (stderr, "uso: ft2_async_bench gen|run ...\n");
      return 2;
    }
  std::string const cmd = argv[1];
  Args const a = parse (argc, argv, 2);
  if (cmd == "gen") return gen (a);
  if (cmd == "genqso") return genqso (a);
  if (cmd == "genpair") return genpair (a);
  if (cmd == "subres") return subres (a);
  if (cmd == "win") return win (a);
  if (cmd == "dtcheck") return dtcheck (a);
  if (cmd == "danno") return danno (a);
  if (cmd == "deriva") return deriva (a);
  if (cmd == "run") return run (a);
  std::fprintf (stderr, "comando sconosciuto %s\n", cmd.c_str ());
  return 2;
}
