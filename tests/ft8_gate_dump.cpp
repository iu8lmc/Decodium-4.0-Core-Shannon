// ft8_gate_dump.cpp — banco di raccolta dati per il gate appreso di fastldpc,
// versione FT8 (FASTLDPC-AI-SPEC-001 SS2b). Fratello di ft2_gate_dump.cpp:
// stesso principio (WAV con messaggio noto attraverso la catena di
// decodifica vera, confronto col codeword atteso), ma per FT8, che ha il
// proprio banco di pesi separato (GATE_*_FT8 in gate_weights.hpp, vedi
// gate.hpp) perche' canale e tipi di messaggio sono diversi da FT2.
//
// A differenza di FT2, FT8 non scrambla i 77 bit del messaggio prima
// dell'LDPC: ftx_encode_ft8_candidate_c dà direttamente msgsent, i toni e il
// codeword a 174 bit, senza bisogno del vettore di scrambling (ftx_ft2_rvec_c)
// che serve invece per FT2.
//
// Uso (un solo comando, come ft2_gate_dump.cpp ma senza --relax/--nfqso):
//   ft8_gate_dump --out gate_train_real_ft8.txt --snr-list "-22,...,-8"
//                 --seeds 40 --message "CQ IU8LMC JN70" ...
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QMutexLocker>
#include <QStringList>
#include <QTextStream>

#include "Detector/FortranRuntimeGuard.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"

extern "C"
{
  void ftx_ft8_async_decode_stage4_c (short const* iwave, int* nqsoprogress, int* nfqso, int* nftx,
                                      int* nutc, int* nfa, int* nfb, int* nzhsym, int* ndepth,
                                      float* emedelay, int* ncontest, int* nagain,
                                      int* lft8apon, int* ltry_a8, int* lapcqonly, int* napwid,
                                      char const* mycall, char const* hiscall,
                                      char const* hisgrid, int* ldiskdat, float* syncs, int* snrs,
                                      float* dts, float* freqs, int* naps, float* quals,
                                      signed char* bits77, char* decodeds, int* nout);
  void ftx_ft8_stage4_reset_c ();
  void ftx_ft8_stage4_set_deadline_ms_c (long long deadline_ms);
  void ftx_ft8_stage4_set_ldpc_osd_c (int maxosd, int norder);
  void ftx_ft8_stage4_set_ldpc_max_iter_c (int max_iter);
  void ftx_ft8_stage4_set_decode_options_c (int low_thresholds, int subpass,
                                            int cycles, int rx_freq_sensitivity,
                                            int candidate_thin);
  void ftx_ft8_stage4_set_supplemental_c (int supplemental);
  int ftx_encode_ft8_candidate_c (char const* message37, char* msgsent_out,
                                  int* itone_out, signed char* codeword_out);
  void fastldpc_gate_dump_open_c (char const* path);
  void fastldpc_gate_dump_close_c ();
  void fastldpc_gate_truth_set_c (signed char const* cw174);
  void fastldpc_gate_truth_clear_c ();
}

namespace {

constexpr int kSampleRate {12000};
constexpr int kFrameSamples {180000};   // 15 s a 12000 Hz, come ft8_stage_compare
constexpr int kNsps {1920};
constexpr float kBt {2.0f};
constexpr int kFt8Codeword {174};
constexpr int kFt8MaxLines {200};
constexpr int kBitsPerMessage {77};
constexpr int kDecodedChars {37};

[[noreturn]] void fail (QString const& message)
{
  throw std::runtime_error {message.toStdString ()};
}

QByteArray to_fortran_field (QByteArray value, int width)
{
  value = value.left (width);
  if (value.size () < width) value.append (QByteArray (width - value.size (), ' '));
  return value;
}

float compute_signal_rms (std::vector<float> const& frame)
{
  double sum_sq = 0.0;
  int count = 0;
  for (float sample : frame)
    {
      if (sample == 0.0f) continue;
      sum_sq += static_cast<double> (sample) * static_cast<double> (sample);
      ++count;
    }
  return count == 0 ? 0.0f : static_cast<float> (std::sqrt (sum_sq / static_cast<double> (count)));
}

// A differenza di FT2 (ftx_encode174_91_message77_c + scrambling con
// ftx_ft2_rvec_c), FT8 non scrambla: ftx_encode_ft8_candidate_c da' gia' il
// codeword a 174 bit pronto per il confronto diretto con l'uscita del
// decoder.
std::array<signed char, kFt8Codeword> expected_codeword (QString const& message,
                                                         std::array<char, kDecodedChars>* msgsent_out,
                                                         std::array<int, 79>* tones_out)
{
  QByteArray const field = to_fortran_field (message.toUpper ().toLatin1 (), kDecodedChars);
  std::array<signed char, kFt8Codeword> codeword {};
  if (ftx_encode_ft8_candidate_c (field.constData (), msgsent_out->data (),
                                  tones_out->data (), codeword.data ()) == 0)
    fail (QStringLiteral ("impossibile codificare il messaggio FT8 \"%1\"").arg (message));
  return codeword;
}

void place_ft8_wave (std::vector<float>& frame, int const* tones, float freq_hz, float gain,
                     int offset_samples)
{
  QVector<float> const wave = decodium::txwave::generateFt8Wave (
      tones, 79, kNsps, kBt, static_cast<float> (kSampleRate), freq_hz);
  if (wave.isEmpty ()) fail (QStringLiteral ("generazione forma d'onda FT8 fallita"));
  if (offset_samples < 0 || offset_samples + wave.size () > static_cast<int> (frame.size ()))
    fail (QStringLiteral ("la forma d'onda non entra nella cornice FT8"));
  for (int i = 0; i < wave.size (); ++i)
    frame[static_cast<size_t> (offset_samples + i)] += gain * wave[i];
}

// Un interferente da mescolare nella stessa cornice del messaggio principale,
// per riprodurre un pileup: stessi 15 s, frequenza vicina, ampiezza propria
// (relativa al messaggio principale, puo' essere piu' debole o piu' forte),
// piccolo scarto temporale indipendente. Il rumore resta unico e condiviso,
// aggiunto dopo la somma di tutti i segnali -- vedi make_wav_samples.
struct Interferer
{
  std::array<int, 79> tones {};
  float freq_hz {0.0f};
  float snr_delta_db {0.0f};   // rispetto allo SNR del messaggio principale
  int offset_samples {6000};
};

std::vector<qint16> make_wav_samples (int const* tones, float freq_hz, float gain,
                                      int offset_samples, float snr_db, bool has_noise, unsigned seed,
                                      std::vector<Interferer> const& interferers = {})
{
  std::vector<float> primary_only (static_cast<size_t> (kFrameSamples), 0.0f);
  place_ft8_wave (primary_only, tones, freq_hz, gain, offset_samples);
  // Lo SNR del messaggio principale si calibra sulla SUA sola energia, non su
  // quella della cornice piena di interferenti: altrimenti --snr-list smette
  // di significare "SNR del messaggio che sto etichettando" e il dataset non
  // e' piu' confrontabile con quello a segnale singolo.
  float const primary_rms = compute_signal_rms (primary_only);

  std::vector<float> frame = primary_only;
  for (Interferer const& interf : interferers)
    {
      float const interf_gain = gain * std::pow (10.0f, interf.snr_delta_db / 20.0f);
      place_ft8_wave (frame, interf.tones.data (), interf.freq_hz, interf_gain, interf.offset_samples);
    }

  if (has_noise && primary_rms > 0.0f)
    {
      double const sigma = static_cast<double> (primary_rms) / std::pow (10.0, static_cast<double> (snr_db) / 20.0);
      std::mt19937 rng {seed};
      std::normal_distribution<float> noise {0.0f, static_cast<float> (sigma)};
      for (float& sample : frame) sample += noise (rng);
    }

  std::vector<qint16> pcm (static_cast<size_t> (kFrameSamples), 0);
  for (int i = 0; i < kFrameSamples; ++i)
    {
      float const clipped = std::max (-1.0f, std::min (1.0f, frame[static_cast<size_t> (i)]));
      pcm[static_cast<size_t> (i)] = static_cast<qint16> (std::lround (static_cast<double> (clipped) * 32767.0));
    }
  return pcm;
}

// Un solo passo di decodifica FT8, alla profondita' indicata: basta per
// esercitare il gate su candidati OSD realistici, non serve replicare tutta
// la sequenza multi-passata di ft8_stage_compare.cpp per questo scopo.
int run_decode (std::vector<qint16> const& pcm, int depth, float nfqso)
{
  std::vector<short> iwave (pcm.begin (), pcm.end ());
  std::array<int, kFt8MaxLines> snrs {};
  std::array<float, kFt8MaxLines> syncs {};
  std::array<float, kFt8MaxLines> dts {};
  std::array<float, kFt8MaxLines> freqs {};
  std::array<int, kFt8MaxLines> naps {};
  std::array<float, kFt8MaxLines> quals {};
  std::array<signed char, kBitsPerMessage * kFt8MaxLines> bits77 {};
  std::array<char, kFt8MaxLines * kDecodedChars> decodeds {};
  int nout = 0;
  int nqsoprogress = 0, nftx = 0, nutc = 0, nfa = 200, nfb = 4000, nzhsym = 50;
  int ndepth = depth, ncontest = 0, nagain = 0, lft8apon = 0, ltry_a8 = 0, lapcqonly = 0;
  int napwid = 0, ldiskdat = 0;
  float emedelay = 0.0f;
  int nfqso_i = static_cast<int> (nfqso);
  QByteArray mycall_field = to_fortran_field ("", 12);
  QByteArray hiscall_field = to_fortran_field ("", 12);
  QByteArray hisgrid_field = to_fortran_field ("", 6);

  QMutexLocker locker {&decodium::fortran::runtime_mutex ()};
  ftx_ft8_stage4_reset_c ();
  ftx_ft8_stage4_set_decode_options_c (0, 0, 1, 1, 100);
  ftx_ft8_stage4_set_supplemental_c (0);
  ftx_ft8_stage4_set_ldpc_osd_c (3, 4);
  ftx_ft8_stage4_set_ldpc_max_iter_c (30);
  ftx_ft8_stage4_set_deadline_ms_c (0);
  ftx_ft8_async_decode_stage4_c (iwave.data (), &nqsoprogress, &nfqso_i, &nftx, &nutc, &nfa, &nfb,
                                 &nzhsym, &ndepth, &emedelay, &ncontest, &nagain, &lft8apon,
                                 &ltry_a8, &lapcqonly, &napwid, mycall_field.constData (),
                                 hiscall_field.constData (), hisgrid_field.constData (), &ldiskdat,
                                 syncs.data (), snrs.data (), dts.data (), freqs.data (),
                                 naps.data (), quals.data (), bits77.data (), decodeds.data (),
                                 &nout);
  ftx_ft8_stage4_set_ldpc_osd_c (-1, 0);
  ftx_ft8_stage4_reset_c ();
  locker.unlock ();
  return std::max (0, nout);
}

QList<double> parse_double_list (QString const& raw, QString const& optionName)
{
  QList<double> values;
  for (QString part : raw.split (QLatin1Char {','}, Qt::SkipEmptyParts))
    {
      bool ok = false;
      double const v = part.trimmed ().toDouble (&ok);
      if (!ok) fail (QStringLiteral ("valore non valido in --%1: \"%2\"").arg (optionName, part));
      values.append (v);
    }
  if (values.isEmpty ()) fail (QStringLiteral ("--%1 e' vuoto").arg (optionName));
  return values;
}

}  // namespace

int main (int argc, char* argv[])
{
  try
    {
      QCoreApplication app {argc, argv};
      QCoreApplication::setApplicationName (QStringLiteral ("ft8_gate_dump"));

      QCommandLineParser parser;
      parser.setApplicationDescription (
          QStringLiteral ("Raccoglie feature+etichetta REALI del gate FT8 su messaggi noti, per il riaddestramento."));
      parser.addHelpOption ();

      QCommandLineOption out_option {QStringList {"o", "out"}, "File di output (append).", "path",
                                     "gate_train_real_ft8.txt"};
      QCommandLineOption message_option {"message", "Messaggio FT8 da provare (ripetibile).", "text"};
      QCommandLineOption snr_option {"snr-list", "SNR in dB separati da virgola.", "list",
                                     "-22,-20,-18,-16,-14,-12,-10,-8"};
      QCommandLineOption seeds_option {"seeds", "Semi di rumore per (messaggio,SNR).", "n", "30"};
      QCommandLineOption freq_option {"freq", "Frequenza audio in Hz.", "hz", "1500.0"};
      QCommandLineOption depth_option {"depth", "Profondita' di decodifica FT8.", "n", "3"};
      QCommandLineOption clean_option {"clean-too", "Include anche una prova pulita (senza rumore) per messaggio."};
      QCommandLineOption relax_option {"relax", "DECODIUM_LDPC_GATE_RELAX (soglia nd allargata).", "value", "0.30"};
      QCommandLineOption pileup_option {"pileup",
          "Interferenti da mescolare per prova, per riprodurre un pileup (0 = disattivato, come prima).",
          "n", "0"};
      QCommandLineOption pileup_pool_option {"pileup-message",
          "Messaggio da usare come interferente (ripetibile; default: gli stessi --message).", "text"};
      QCommandLineOption pileup_freq_option {"pileup-freq-range",
          "Scarto in Hz degli interferenti dal messaggio principale, min,max (segno casuale).",
          "hz,hz", "5,50"};
      QCommandLineOption pileup_snr_option {"pileup-snr-delta",
          "SNR dell'interferente relativo al messaggio principale, min,max in dB "
          "(negativo = interferente piu' debole, positivo = piu' forte, come nei pileup veri).",
          "db,db", "-6,12"};
      QCommandLineOption pileup_seed_option {"pileup-seed",
          "Seme base per la scelta di interferenti/frequenze/SNR (indipendente dal seme del rumore).",
          "n", "9000"};

      parser.addOption (out_option);
      parser.addOption (message_option);
      parser.addOption (snr_option);
      parser.addOption (seeds_option);
      parser.addOption (freq_option);
      parser.addOption (depth_option);
      parser.addOption (clean_option);
      parser.addOption (relax_option);
      parser.addOption (pileup_option);
      parser.addOption (pileup_pool_option);
      parser.addOption (pileup_freq_option);
      parser.addOption (pileup_snr_option);
      parser.addOption (pileup_seed_option);
      parser.process (app);

      QStringList messages = parser.values (message_option);
      if (messages.isEmpty ())
        messages = {QStringLiteral ("CQ IU8LMC JN70"), QStringLiteral ("IU8LMC DL9XYZ -12"),
                    QStringLiteral ("DL9XYZ IU8LMC R-08"), QStringLiteral ("RR73 IU8LMC DL9XYZ"),
                    QStringLiteral ("IU8LMC DL9XYZ 73"), QStringLiteral ("CQ DX IU8LMC JN70"),
                    QStringLiteral ("IU8LMC W1AW FN31"), QStringLiteral ("W1AW IU8LMC RR73")};

      QList<double> const snr_list = parse_double_list (parser.value (snr_option), "snr-list");
      bool ok = false;
      int const seeds = parser.value (seeds_option).toInt (&ok);
      if (!ok || seeds <= 0) fail (QStringLiteral ("--seeds non valido"));
      float const freq = parser.value (freq_option).toFloat (&ok);
      int const depth = parser.value (depth_option).toInt (&ok);
      bool const clean_too = parser.isSet (clean_option);
      QString const out_path = parser.value (out_option);
      float const relax = parser.value (relax_option).toFloat (&ok);
      if (!ok || relax <= 0.0f) fail (QStringLiteral ("--relax non valido"));

      int const pileup_n = parser.value (pileup_option).toInt (&ok);
      if (!ok || pileup_n < 0) fail (QStringLiteral ("--pileup non valido"));
      QStringList pileup_pool = parser.values (pileup_pool_option);
      if (pileup_pool.isEmpty ()) pileup_pool = messages;
      QList<double> const pileup_freq_range = parse_double_list (parser.value (pileup_freq_option), "pileup-freq-range");
      QList<double> const pileup_snr_range = parse_double_list (parser.value (pileup_snr_option), "pileup-snr-delta");
      if (pileup_n > 0 && (pileup_freq_range.size () != 2 || pileup_snr_range.size () != 2))
        fail (QStringLiteral ("--pileup-freq-range e --pileup-snr-delta vogliono esattamente due valori, min,max"));
      unsigned const pileup_seed_base = static_cast<unsigned> (parser.value (pileup_seed_option).toUInt (&ok));
      if (pileup_n > 0 && !ok) fail (QStringLiteral ("--pileup-seed non valido"));

      // Toni degli interferenti, codificati una sola volta (come per i messaggi principali).
      QList<std::array<int, 79>> pileup_tones;
      for (QString const& m : pileup_pool)
        {
          std::array<char, kDecodedChars> sent {};
          std::array<int, 79> t {};
          expected_codeword (m, &sent, &t);
          pileup_tones.append (t);
        }

      // Impostate PRIMA del primo decode: decodium_bridge.cpp le legge una
      // sola volta, in modo statico, al primo uso (come ft2_gate_dump.cpp).
      // gate_relax allarga la soglia nd dell'OSD cosi' emergono anche i
      // candidati che il gate scarterebbe, per avere sia veri che falsi nel
      // dataset -- esattamente come lab/neural/gate/make_dataset.sh.
      qputenv ("DECODIUM_LDPC_GATE", "1");
      qputenv ("DECODIUM_LDPC_GATE_RELAX", QByteArray::number (relax));

      fastldpc_gate_dump_open_c (out_path.toLocal8Bit ().constData ());

      QTextStream out {stdout};
      long long trials = 0, decodes = 0, pileup_trials = 0;
      int message_index = 0;
      for (QString const& message : messages)
        {
          std::array<char, kDecodedChars> msgsent {};
          std::array<int, 79> tones {};
          std::array<signed char, kFt8Codeword> const truth =
              expected_codeword (message, &msgsent, &tones);

          // Indici del pool interferenti che non sono QUESTO messaggio: se il
          // pool coincide con --message (caso di default), evita di mescolare
          // il segnale principale con una copia di se' stesso.
          QList<int> pileup_candidates;
          for (int i = 0; i < pileup_pool.size (); ++i)
            if (pileup_pool.at (i) != message) pileup_candidates.append (i);

          auto run_one = [&] (bool has_noise, double snr_db, unsigned seed) {
            std::vector<Interferer> interferers;
            if (pileup_n > 0 && !pileup_candidates.isEmpty ())
              {
                // Seme deterministico per prova: base + indice del messaggio +
                // seme del rumore, cosi' ogni combinazione ha il suo pileup
                // riproducibile ma diverso dalle altre.
                std::mt19937 pileup_rng {pileup_seed_base + 97u * static_cast<unsigned> (message_index) + seed};
                std::uniform_int_distribution<int> pick_msg (0, pileup_candidates.size () - 1);
                std::uniform_real_distribution<float> pick_freq (
                    static_cast<float> (pileup_freq_range.at (0)), static_cast<float> (pileup_freq_range.at (1)));
                std::uniform_real_distribution<float> pick_snr_delta (
                    static_cast<float> (pileup_snr_range.at (0)), static_cast<float> (pileup_snr_range.at (1)));
                std::uniform_int_distribution<int> pick_sign (0, 1);
                std::uniform_int_distribution<int> pick_dt (-300, 300);   // +-25 ms di scarto temporale
                for (int k = 0; k < pileup_n; ++k)
                  {
                    int const pool_index = pileup_candidates.at (pick_msg (pileup_rng));
                    float const delta_hz = pick_freq (pileup_rng) * (pick_sign (pileup_rng) ? 1.0f : -1.0f);
                    Interferer interf;
                    interf.tones = pileup_tones.at (pool_index);
                    interf.freq_hz = freq + delta_hz;
                    interf.snr_delta_db = pick_snr_delta (pileup_rng);
                    interf.offset_samples = std::max (0, 6000 + pick_dt (pileup_rng));
                    interferers.push_back (interf);
                  }
                ++pileup_trials;
              }
            std::vector<qint16> const pcm =
                make_wav_samples (tones.data (), freq, 0.85f, 6000, static_cast<float> (snr_db),
                                  has_noise, seed, interferers);
            fastldpc_gate_truth_set_c (truth.data ());
            int const n = run_decode (pcm, depth, freq);
            fastldpc_gate_truth_clear_c ();
            ++trials;
            decodes += n;
          };

          if (clean_too) run_one (false, 0.0, 0);
          for (double snr_db : snr_list)
            for (int seed = 0; seed < seeds; ++seed)
              run_one (true, snr_db, static_cast<unsigned> (2000000 + seed));

          out << "messaggio \"" << message << "\": fatto (" << trials << " prove finora, "
              << decodes << " decodifiche totali, " << pileup_trials << " con pileup)\n";
          out.flush ();
          ++message_index;
        }

      fastldpc_gate_dump_close_c ();
      out << "totale: " << trials << " prove -> " << out_path << '\n';
      return 0;
    }
  catch (std::exception const& error)
    {
      QTextStream err {stderr};
      err << "ft8_gate_dump: " << error.what () << '\n';
      err.flush ();
      return 1;
    }
}
