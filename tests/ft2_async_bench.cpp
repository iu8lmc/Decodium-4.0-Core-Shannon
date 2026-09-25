// Banco del ricevitore asincrono FT2 (ASYMX / Async L2) — fase F0 del
// progetto PROGETTO_ASYMX_JTTY.
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

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>

#include "Detector/Ft2AsyncRegistry.hpp"
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
  double const f5_lo = a.num ("f5-lo", 0.2), f5_hi = a.num ("f5-hi", 1.0);
  int const qso_progress = int (a.num ("qsoprog", 3));

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

      int snrs[kMaxLines] {}, naps[kMaxLines] {}, nout = 0, nqso = 0, ncontest = 0;
      float dts[kMaxLines] {}, freqs[kMaxLines] {}, quals[kMaxLines] {};
      signed char bits77[kMaxLines * 77] {};
      char decodeds[kMaxLines * 37] {};
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
      auto const c0 = std::chrono::steady_clock::now ();
      ftx_ft2_async_decode_stage7_c (window.data (), &nqso, &nfqso_now, &nfa, &nfb, &ndepth, &ncontest,
                                     my.constData (), his_now.constData (), snrs, dts, freqs, naps, quals,
                                     bits77, decodeds, &nout);
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
  if (cmd == "run") return run (a);
  std::fprintf (stderr, "comando sconosciuto %s\n", cmd.c_str ());
  return 2;
}
