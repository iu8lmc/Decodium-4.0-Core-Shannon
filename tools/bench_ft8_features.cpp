// Bench FT8: misura impatto di Turbo Feedback (LDPC max_iter 30 vs 50)
// e Neural Sync (OSD always-on) su decoded/recovered count e tempo CPU.
//
// Genera N messaggi FT8 a SNR borderline + noise gaussiano in slot 14 s,
// chiama ftx_ft8_async_decode_stage4_c per 4 configurazioni:
//   baseline | +turbo | +neural | +turbo+neural
// e stampa una tabella comparativa.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>

#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"

extern "C"
{
  void ftx_ft8_stage4_reset_c ();
  void ftx_ft8_stage4_set_cancel_c (int cancel);
  void ftx_ft8_stage4_set_deadline_ms_c (long long deadline_ms);
  void ftx_ft8_stage4_set_ldpc_osd_c (int maxosd, int norder);
  void ftx_ft8_stage4_set_supplemental_c (int supplemental);
  void ftx_ft8_stage4_set_ldpc_max_iter_c (int max_iter);
  void ftx_ft8_async_decode_stage4_c (short const* iwave, int* nqsoprogress, int* nfqso, int* nftx,
                                      int* nutc, int* nfa, int* nfb, int* nzhsym, int* ndepth,
                                      float* emedelay, int* ncontest, int* nagain,
                                      int* lft8apon, int* ltry_a8, int* lapcqonly, int* napwid,
                                      char const* mycall, char const* hiscall,
                                      char const* hisgrid, int* ldiskdat, float* syncs, int* snrs,
                                      float* dts, float* freqs, int* naps, float* quals,
                                      signed char* bits77, char* decodeds, int* nout);
}

namespace
{

constexpr int kSampleCount = 15 * 12000;     // 180000, slot FT8 (15 s buffer di lavoro)
constexpr int kFt8Nsps = 1920;
constexpr int kMaxLines = 200;
constexpr int kBitsPerMessage = 77;
constexpr int kDecodedChars = 37;

QByteArray to_fortran_field (QByteArray value, int width)
{
  value = value.left (width);
  if (value.size () < width)
    {
      value.append (QByteArray (width - value.size (), ' '));
    }
  return value;
}

// Genera waveform FT8 di un messaggio in un slot 15 s a SNR target,
// posizionata a `start_ms` dall'inizio dello slot. Restituisce float [-1..1].
std::vector<float> make_signal_slot (QString const& message, float frequency_hz,
                                     float start_ms, float snr_db, unsigned seed)
{
  decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeFt8 (message);
  if (!encoded.ok || encoded.tones.size () != 79)
    {
      return {};
    }

  QVector<float> const wave = decodium::txwave::generateFt8Wave (
      encoded.tones.constData (), encoded.tones.size (), kFt8Nsps,
      2.0f, 12000.0f, frequency_hz);
  if (wave.isEmpty ())
    {
      return {};
    }

  std::vector<float> frame (static_cast<size_t> (kSampleCount), 0.0f);
  int const offset = static_cast<int> (std::lround (start_ms * 12.0f));
  for (int i = 0; i < wave.size (); ++i)
    {
      int const idx = offset + i;
      if (idx >= 0 && idx < kSampleCount)
        {
          frame[static_cast<size_t> (idx)] = 0.85f * wave[i];
        }
    }

  // RMS del segnale
  double sum_sq = 0.0;
  long count = 0;
  for (float v : frame)
    {
      if (v != 0.0f)
        {
          sum_sq += static_cast<double> (v) * static_cast<double> (v);
          ++count;
        }
    }
  if (count <= 0)
    {
      return frame;
    }
  double const signal_rms = std::sqrt (sum_sq / static_cast<double> (count));
  double const sigma = signal_rms / std::pow (10.0, static_cast<double> (snr_db) / 20.0);

  std::mt19937 rng {seed};
  std::normal_distribution<float> noise {0.0f, static_cast<float> (sigma)};
  for (float& v : frame)
    {
      v += noise (rng);
    }
  return frame;
}

// Converti slot float [-1..1] in short int16 saturato.
std::vector<short> to_int16 (std::vector<float> const& frame)
{
  std::vector<short> out (frame.size ());
  for (size_t i = 0; i < frame.size (); ++i)
    {
      float const c = std::max (-1.0f, std::min (1.0f, frame[i]));
      out[i] = static_cast<short> (std::lround (32767.0f * c));
    }
  return out;
}

// True se il messaggio decoded contiene il payload del ground-truth (callsign-like match).
bool decoded_matches (QString const& decoded, QString const& truth)
{
  // Standard FT8 decoded line: già solo testo decodificato (pad spazi a destra).
  // Confronto liberale: trimmed, normalizzato a maiuscole.
  QString d = decoded.trimmed ().toUpper ();
  QString t = truth.trimmed ().toUpper ();
  if (d.isEmpty () || t.isEmpty ())
    {
      return false;
    }
  // Possiamo avere padding del decoded: cerca "t" come substring di "d"
  // perché il decoder a volte inserisce '?' o trailing chars.
  return d.contains (t.left (qMin (10, t.size ())));
}

struct DecodeResult
{
  int decoded_count = 0;
  int recovered = 0;
  long long elapsed_us = 0;
};

DecodeResult run_decode (std::vector<short> const& iwave_in,
                         std::vector<QString> const& truths,
                         bool turbo, bool neural)
{
  // Setup parametri statici tramite setter C
  ftx_ft8_stage4_set_cancel_c (0);
  ftx_ft8_stage4_set_deadline_ms_c (0);
  ftx_ft8_stage4_set_supplemental_c (neural ? 1 : 0);
  ftx_ft8_stage4_set_ldpc_max_iter_c (turbo ? 50 : 30);
  if (neural)
    {
      ftx_ft8_stage4_set_ldpc_osd_c (3, 3);
    }
  else
    {
      ftx_ft8_stage4_set_ldpc_osd_c (-1, 0);
    }

  // Buffer locale (decoder modifica in-place a volte)
  std::vector<short> iwave = iwave_in;

  int nqsoprogress = 0;
  int nfqso = 1500;
  int nftx = 1500;
  int nutc = 100000;
  int nfa = 200;
  int nfb = 3000;
  int nzhsym = 50;
  int ndepth = 3;
  float emedelay = 0.0f;
  int ncontest = 0;
  int nagain = 0;
  int lft8apon = 0;
  int ltry_a8 = 0;
  int lapcqonly = 0;
  int napwid = 50;
  int ldiskdat = 0;

  QByteArray mycall = to_fortran_field ("K1ABC", 12);
  QByteArray hiscall = to_fortran_field ("W9XYZ", 12);
  QByteArray hisgrid = to_fortran_field ("EN37", 6);

  std::array<int, kMaxLines> snrs {};
  std::array<float, kMaxLines> dts {};
  std::array<float, kMaxLines> freqs {};
  std::array<int, kMaxLines> naps {};
  std::array<float, kMaxLines> quals {};
  std::array<signed char, kMaxLines * kBitsPerMessage> bits77 {};
  std::array<char, kMaxLines * kDecodedChars> decodeds {};
  int nout = 0;

  ftx_ft8_stage4_reset_c ();

  auto const t0 = std::chrono::steady_clock::now ();
  ftx_ft8_async_decode_stage4_c (iwave.data (), &nqsoprogress, &nfqso, &nftx, &nutc,
                                 &nfa, &nfb, &nzhsym, &ndepth, &emedelay, &ncontest,
                                 &nagain, &lft8apon, &ltry_a8, &lapcqonly, &napwid,
                                 mycall.constData (), hiscall.constData (),
                                 hisgrid.constData (), &ldiskdat,
                                 nullptr, snrs.data (), dts.data (), freqs.data (),
                                 naps.data (), quals.data (), bits77.data (),
                                 decodeds.data (), &nout);
  auto const t1 = std::chrono::steady_clock::now ();

  DecodeResult result;
  result.elapsed_us =
      std::chrono::duration_cast<std::chrono::microseconds> (t1 - t0).count ();
  result.decoded_count = std::max (0, std::min (nout, kMaxLines));

  for (int i = 0; i < result.decoded_count; ++i)
    {
      QByteArray slice (decodeds.data () + i * kDecodedChars, kDecodedChars);
      QString const dec = QString::fromLatin1 (slice).trimmed ();
      for (QString const& truth : truths)
        {
          if (decoded_matches (dec, truth))
            {
              ++result.recovered;
              break;
            }
        }
    }

  // reset per next run
  ftx_ft8_stage4_set_supplemental_c (0);
  ftx_ft8_stage4_set_ldpc_max_iter_c (30);
  ftx_ft8_stage4_set_ldpc_osd_c (-1, 0);

  return result;
}

}  // namespace

int main (int argc, char* argv[])
{
  QCoreApplication app (argc, argv);

  std::printf ("FT8 features benchmark — Decodium 1.0.67\n");
  std::printf ("Slot 15s @ 12kHz mono int16, ndepth=3, single-shot per config.\n\n");

  // 5 messaggi FT8 distinti, frequenze separate, dt simulato.
  // Tutti CQ standard per encoding sicuro.
  struct Item
  {
    QString message;
    float freq_hz;
    float start_ms;
  };
  std::vector<Item> items {
      {"CQ K1ABC FN42",  650.0f,   500.0f},
      {"CQ W9XYZ EN37",  1100.0f,  500.0f},
      {"CQ DL9ABC JN58", 1550.0f,  500.0f},
      {"CQ JA1XYZ PM95", 2000.0f,  500.0f},
      {"CQ G3ABC IO91",  2450.0f,  500.0f},
  };

  // Range SNR borderline + above-threshold per stressare LDPC/OSD.
  std::vector<float> snrs_db {-19.0f, -18.0f, -17.0f, -16.0f, -15.0f, -14.0f, -13.0f};
  // Multi-seed: rumore diverso a parità di SNR per ridurre varianza statistica.
  constexpr int kSeedRepeats = 5;

  // Costruzione: sommo i 5 segnali in un unico slot per ogni SNR,
  // eseguo il decode 4 volte (una per config).
  struct Config { const char* name; bool turbo; bool neural; };
  std::vector<Config> configs {
      {"baseline      ", false, false},
      {"+turbo         ", true,  false},
      {"+neural        ", false, true },
      {"+turbo+neural  ", true,  true },
  };

  std::printf ("%-7s | %-15s | dec | rec | rec%% | time(ms)\n", "SNR", "config");
  std::printf ("--------+-----------------+-----+-----+------+---------\n");

  // Aggregati per config su tutti gli SNR
  struct Agg { int decoded = 0; int recovered = 0; long long us = 0; };
  std::vector<Agg> agg (configs.size ());
  // Aggregati per (config, snr) per breakdown
  std::vector<std::vector<Agg>> per_snr (snrs_db.size (), std::vector<Agg> (configs.size ()));

  unsigned seed = 0x600D5EED;
  for (size_t s_idx = 0; s_idx < snrs_db.size (); ++s_idx)
    {
      float const snr = snrs_db[s_idx];
      // Aggregato del livello SNR (su tutti i seed)
      std::vector<Agg> level (configs.size ());

      for (int rep = 0; rep < kSeedRepeats; ++rep)
        {
          std::vector<float> slot (kSampleCount, 0.0f);
          std::vector<QString> truths;
          truths.reserve (items.size ());
          for (Item const& it : items)
            {
              std::vector<float> sig = make_signal_slot (it.message, it.freq_hz,
                                                        it.start_ms, snr, seed++);
              if (sig.empty ())
                {
                  std::fprintf (stderr, "ERROR: failed to encode %s\n",
                                qPrintable (it.message));
                  return 2;
                }
              for (size_t i = 0; i < slot.size (); ++i)
                {
                  slot[i] += sig[i];
                }
              truths.push_back (it.message);
            }
          // Normalizza per evitare clipping (5 segnali sommati)
          float peak = 0.0f;
          for (float v : slot)
            {
              peak = std::max (peak, std::fabs (v));
            }
          if (peak > 0.95f)
            {
              float const scale = 0.95f / peak;
              for (float& v : slot) v *= scale;
            }
          std::vector<short> iwave = to_int16 (slot);

          for (size_t c = 0; c < configs.size (); ++c)
            {
              DecodeResult r = run_decode (iwave, truths, configs[c].turbo, configs[c].neural);
              agg[c].decoded += r.decoded_count;
              agg[c].recovered += r.recovered;
              agg[c].us += r.elapsed_us;
              level[c].decoded += r.decoded_count;
              level[c].recovered += r.recovered;
              level[c].us += r.elapsed_us;
              per_snr[s_idx][c] = level[c];
            }
        }
      // Stampa per livello SNR, riga per config
      int const snr_truths = static_cast<int> (kSeedRepeats * items.size ());
      for (size_t c = 0; c < configs.size (); ++c)
        {
          double const recpct = snr_truths > 0
              ? 100.0 * level[c].recovered / static_cast<double> (snr_truths)
              : 0.0;
          std::printf ("%5.0f dB | %s | %3d | %3d | %4.1f | %7.1f\n",
                       snr, configs[c].name,
                       level[c].decoded, level[c].recovered,
                       recpct, level[c].us / 1000.0);
        }
      std::printf ("--------+-----------------+-----+-----+------+---------\n");
    }

  std::printf ("\nTOTALI (su %zu SNR × %d seed × %zu segnali = %zu ground-truth):\n",
               snrs_db.size (), kSeedRepeats, items.size (),
               snrs_db.size () * kSeedRepeats * items.size ());
  std::printf ("%-15s | decoded | recovered | rec%%   | time(ms)\n", "config");
  std::printf ("----------------+---------+-----------+--------+---------\n");
  int const total_truths = static_cast<int> (snrs_db.size () * kSeedRepeats * items.size ());
  for (size_t c = 0; c < configs.size (); ++c)
    {
      double const recpct = total_truths > 0
          ? 100.0 * agg[c].recovered / static_cast<double> (total_truths)
          : 0.0;
      std::printf ("%s | %7d | %9d | %5.1f%% | %7.1f\n",
                   configs[c].name, agg[c].decoded, agg[c].recovered, recpct,
                   agg[c].us / 1000.0);
    }

  // === SCENARIO LDPC-BORDERLINE ===
  // Segnali a SNR moderato (-15 dB) con interferenza co-canale ravvicinata
  // (delta freq 50..100 Hz) per generare codeword con bit-flip residui che
  // l'iterazione LDPC standard fatica a recuperare. Qui Turbo Feedback dovrebbe
  // mostrare il proprio valore reale.
  std::printf ("\n=== SCENARIO LDPC-borderline (interferenza co-canale) ===\n");
  std::printf ("%-7s | %-15s | dec | rec | time(ms)\n", "trial", "config");
  std::printf ("--------+-----------------+-----+-----+---------\n");

  std::vector<Agg> agg_ldpc (configs.size ());
  constexpr int kLdpcTrials = 8;
  unsigned ldpc_seed = 0xDEADC0DE;
  for (int trial = 0; trial < kLdpcTrials; ++trial)
    {
      std::vector<float> slot (kSampleCount, 0.0f);
      std::vector<QString> truths;
      // Coppia interferente: vittima a 1500 Hz, QRM a 1550 Hz, entrambe SNR -15
      std::vector<Item> pair {
        {"CQ K1ABC FN42",  1500.0f, 500.0f},
        {"CQ W9XYZ EN37",  1500.0f + 50.0f * (trial % 3 + 1), 500.0f + 80.0f},
      };
      for (Item const& it : pair)
        {
          std::vector<float> sig = make_signal_slot (it.message, it.freq_hz,
                                                    it.start_ms, -15.0f, ldpc_seed++);
          if (sig.empty ()) continue;
          for (size_t i = 0; i < slot.size (); ++i) slot[i] += sig[i];
          truths.push_back (it.message);
        }
      float peak = 0.0f;
      for (float v : slot) peak = std::max (peak, std::fabs (v));
      if (peak > 0.95f)
        {
          float const scale = 0.95f / peak;
          for (float& v : slot) v *= scale;
        }
      std::vector<short> iwave = to_int16 (slot);
      for (size_t c = 0; c < configs.size (); ++c)
        {
          DecodeResult r = run_decode (iwave, truths, configs[c].turbo, configs[c].neural);
          agg_ldpc[c].decoded += r.decoded_count;
          agg_ldpc[c].recovered += r.recovered;
          agg_ldpc[c].us += r.elapsed_us;
          std::printf ("%6d  | %s | %3d | %3d | %7.1f\n",
                       trial, configs[c].name, r.decoded_count, r.recovered,
                       r.elapsed_us / 1000.0);
        }
      std::printf ("--------+-----------------+-----+-----+---------\n");
    }
  std::printf ("\nLDPC-borderline TOTALI (%d trial × 2 segnali = %d GT):\n",
               kLdpcTrials, kLdpcTrials * 2);
  std::printf ("%-15s | decoded | recovered | rec%%   | time(ms)\n", "config");
  std::printf ("----------------+---------+-----------+--------+---------\n");
  int const ldpc_truths = kLdpcTrials * 2;
  for (size_t c = 0; c < configs.size (); ++c)
    {
      double const recpct = ldpc_truths > 0
          ? 100.0 * agg_ldpc[c].recovered / static_cast<double> (ldpc_truths)
          : 0.0;
      std::printf ("%s | %7d | %9d | %5.1f%% | %7.1f\n",
                   configs[c].name, agg_ldpc[c].decoded, agg_ldpc[c].recovered, recpct,
                   agg_ldpc[c].us / 1000.0);
    }

  // Delta vs baseline
  std::printf ("\nDELTA vs baseline:\n");
  std::printf ("%-15s | Δrecovered | Δtime(ms) | overhead\n", "config");
  std::printf ("----------------+------------+-----------+----------\n");
  for (size_t c = 1; c < configs.size (); ++c)
    {
      int const drec = agg[c].recovered - agg[0].recovered;
      double const dt = (agg[c].us - agg[0].us) / 1000.0;
      double const ov = agg[0].us > 0
          ? 100.0 * (agg[c].us - agg[0].us) / static_cast<double> (agg[0].us)
          : 0.0;
      std::printf ("%s | %+10d | %+9.1f | %+6.1f%%\n",
                   configs[c].name, drec, dt, ov);
    }
  std::printf ("\n");
  return 0;
}
