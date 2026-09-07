// ft2_gate_dump.cpp — banco di raccolta dati per il riaddestramento del gate
// appreso di fastldpc (FASTLDPC-AI-SPEC-001 §2b).
//
// Il pacchetto di ricerca originale (Detector/fastldpc/lab/neural/gate/) ha
// generato il dataset attuale su un canale AWGN sintetico modellato a mano
// (train/ft2chan.py): la nota in cima a gate_weights.hpp dice esplicitamente
// che va rifatto sui LLR REALI. Questo banco fa esattamente quello, ma con la
// verita' nota invece che con un demodulatore modellato: genera un WAV FT2
// con un messaggio noto (stesso stack C++ TX di ft2_make_test_wav), lo passa
// per la catena di decodifica VERA (ft2_async_decode_, la stessa che usa il
// traffico in aria), e per ogni candidato che il gate esamina confronta la
// parola prodotta con la parola di codice attesa (calcolata con lo stesso
// scrambling+LDPC di FtxFt2Stage7::encode_codeword77, vedi ftx_ft2_rvec_c).
//
// Uso (un solo comando, niente continuazione di riga):
//   ft2_gate_dump --out gate_train_real.txt --snr-list "-22,-20,-18,-16,-14,-12,-10,-8"
//                 --seeds 40 --relax 0.30
//                 --message "CQ IU8LMC JN70" --message "IU8LMC DL9XYZ -12"
//                 --message "DL9XYZ IU8LMC R-08" --message "RR73 IU8LMC DL9XYZ"
//
// Il formato di output e' quello di gate/make_dataset.sh e gate/train_gate.py:
// f0..f9 label(1=vero) acc(1=il gate compilato oggi accetterebbe). DECODIUM_LDPC_GATE=1
// e DECODIUM_LDPC_GATE_RELAX vengono impostati da questo programma stesso,
// prima del primo decode: non serve settarli a mano nell'ambiente.
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
  void ft2_async_decode_ (short iwave[], int* nqsoprogress, int* nfqso, int* nfa, int* nfb,
                         int* ndepth, int* ncontest, char mycall[], char hiscall[],
                         int snrs[], float dts[], float freqs[], int naps[], float quals[],
                         signed char bits77[], char decodeds[], int* nout,
                         size_t, size_t, size_t);
  void ftx_ft2_cpp_dsp_rollout_stage_override_c (int stage);
  void ftx_ft2_cpp_dsp_rollout_stage_reset_c ();
  void ftx_ft2_stage7_clravg_c ();
  void ftx_ft2_rvec_c (signed char* out77);
  int ftx_encode174_91_message77_c (signed char const* message77, signed char* codeword_out);
  void fastldpc_gate_dump_open_c (char const* path);
  void fastldpc_gate_dump_close_c ();
  void fastldpc_gate_truth_set_c (signed char const* cw174);
  void fastldpc_gate_truth_clear_c ();
}

namespace {

constexpr int kSampleRate {12000};
constexpr int kFrameSamples {45000};
constexpr int kNsps {288};
constexpr int kFt2Bits {77};
constexpr int kFt2Codeword {174};
constexpr int kFt2MaxLines {100};
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

void add_awgn (std::vector<float>& frame, float snr_db, unsigned seed)
{
  float const signal_rms = compute_signal_rms (frame);
  if (signal_rms <= 0.0f) return;
  double const sigma = static_cast<double> (signal_rms) / std::pow (10.0, static_cast<double> (snr_db) / 20.0);
  std::mt19937 rng {seed};
  std::normal_distribution<float> noise {0.0f, static_cast<float> (sigma)};
  for (float& sample : frame) sample += noise (rng);
}

// Parola di codice VERA attesa (dominio scrambled+LDPC), stesso calcolo di
// FtxFt2Stage7::encode_codeword77 per il tipo 8: scrambling con rvec fisso,
// poi CRC-14+LDPC. Se il decoder VERO produce esattamente questi 174 bit su
// un candidato, quel candidato e' un vero; qualunque altra cosa e' un falso.
std::array<signed char, kFt2Codeword> expected_codeword (QByteArray const& msgbits77)
{
  if (msgbits77.size () != kFt2Bits)
    fail (QStringLiteral ("encodeFt2 non ha prodotto 77 bit di messaggio"));
  std::array<signed char, kFt2Bits> rvec {};
  ftx_ft2_rvec_c (rvec.data ());
  std::array<signed char, kFt2Bits> scrambled {};
  for (int i = 0; i < kFt2Bits; ++i)
    scrambled[static_cast<size_t> (i)] =
        static_cast<signed char> ((static_cast<int> (msgbits77.at (i) != 0) + rvec[static_cast<size_t> (i)]) & 1);
  std::array<signed char, kFt2Codeword> codeword {};
  if (ftx_encode174_91_message77_c (scrambled.data (), codeword.data ()) == 0)
    fail (QStringLiteral ("ftx_encode174_91_message77_c ha fallito"));
  return codeword;
}

std::vector<qint16> make_wav_samples (QString const& message, float freq_hz, float gain,
                                      float offset_ms, float snr_db, bool has_noise, unsigned seed)
{
  decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeFt2 (message);
  if (!encoded.ok || encoded.tones.isEmpty ())
    fail (QStringLiteral ("impossibile codificare il messaggio FT2 \"%1\"").arg (message));

  QVector<float> const wave = decodium::txwave::generateFt2Wave (
      encoded.tones.constData (), encoded.tones.size (), kNsps,
      static_cast<float> (kSampleRate), freq_hz);
  if (wave.isEmpty ()) fail (QStringLiteral ("generazione forma d'onda FT2 fallita"));

  int const offset_samples = static_cast<int> (std::lround (static_cast<double> (offset_ms) * kSampleRate / 1000.0));
  if (offset_samples < 0 || offset_samples + wave.size () > kFrameSamples)
    fail (QStringLiteral ("la forma d'onda non entra nella cornice FT2"));

  std::vector<float> frame (static_cast<size_t> (kFrameSamples), 0.0f);
  for (int i = 0; i < wave.size (); ++i)
    frame[static_cast<size_t> (offset_samples + i)] = gain * wave[i];

  if (has_noise) add_awgn (frame, snr_db, seed);

  std::vector<qint16> pcm (static_cast<size_t> (kFrameSamples), 0);
  for (int i = 0; i < kFrameSamples; ++i)
    {
      float const clipped = std::max (-1.0f, std::min (1.0f, frame[static_cast<size_t> (i)]));
      pcm[static_cast<size_t> (i)] = static_cast<qint16> (std::lround (static_cast<double> (clipped) * 32767.0));
    }
  return pcm;
}

// Decodifica una cornice gia' in memoria per lo stage FT2 indicato: stesso
// codice di ft2_stage_compare.cpp, senza il giro dal/al file WAV.
int run_decode (std::vector<qint16> const& pcm, int stage, float nfqso)
{
  std::vector<short> iwave (pcm.begin (), pcm.end ());
  std::array<int, kFt2MaxLines> snrs {};
  std::array<float, kFt2MaxLines> dts {};
  std::array<float, kFt2MaxLines> freqs {};
  std::array<int, kFt2MaxLines> naps {};
  std::array<float, kFt2MaxLines> quals {};
  std::array<signed char, kBitsPerMessage * kFt2MaxLines> bits77 {};
  std::array<char, kFt2MaxLines * kDecodedChars> decodeds {};
  int nout = 0;
  int nqsoprogress = 0, nfa = 200, nfb = 5000, ndepth = 3, ncontest = 0;
  int nfqso_i = static_cast<int> (nfqso);
  QByteArray mycall_field = to_fortran_field ("", 12);
  QByteArray hiscall_field = to_fortran_field ("", 12);

  QMutexLocker locker {&decodium::fortran::runtime_mutex ()};
  if (stage >= 7) ftx_ft2_stage7_clravg_c ();
  ftx_ft2_cpp_dsp_rollout_stage_override_c (stage);
  ft2_async_decode_ (iwave.data (), &nqsoprogress, &nfqso_i, &nfa, &nfb, &ndepth, &ncontest,
                     mycall_field.data (), hiscall_field.data (), snrs.data (), dts.data (),
                     freqs.data (), naps.data (), quals.data (), bits77.data (),
                     decodeds.data (), &nout, static_cast<size_t> (mycall_field.size ()),
                     static_cast<size_t> (hiscall_field.size ()), static_cast<size_t> (decodeds.size ()));
  ftx_ft2_cpp_dsp_rollout_stage_reset_c ();
  if (stage >= 7) ftx_ft2_stage7_clravg_c ();
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
      QCoreApplication::setApplicationName (QStringLiteral ("ft2_gate_dump"));

      QCommandLineParser parser;
      parser.setApplicationDescription (
          QStringLiteral ("Raccoglie feature+etichetta REALI del gate FT2 su messaggi noti, per il riaddestramento."));
      parser.addHelpOption ();

      QCommandLineOption out_option {QStringList {"o", "out"}, "File di output (append).", "path",
                                     "gate_train_real.txt"};
      QCommandLineOption message_option {"message", "Messaggio FT2 da provare (ripetibile).", "text"};
      QCommandLineOption snr_option {"snr-list", "SNR in dB separati da virgola.", "list",
                                     "-24,-22,-20,-18,-16,-14,-12,-10,-8"};
      QCommandLineOption seeds_option {"seeds", "Semi di rumore per (messaggio,SNR).", "n", "30"};
      QCommandLineOption relax_option {"relax", "DECODIUM_LDPC_GATE_RELAX (soglia nd allargata).", "value", "0.30"};
      QCommandLineOption freq_option {"freq", "Frequenza audio in Hz.", "hz", "1000.0"};
      QCommandLineOption nfqso_option {"nfqso", "Frequenza QSO attesa in Hz.", "hz", "1000"};
      QCommandLineOption stage_option {"stage", "Stage FT2 DSP.", "n", "7"};
      QCommandLineOption clean_option {"clean-too", "Include anche una prova pulita (senza rumore) per messaggio."};

      parser.addOption (out_option);
      parser.addOption (message_option);
      parser.addOption (snr_option);
      parser.addOption (seeds_option);
      parser.addOption (relax_option);
      parser.addOption (freq_option);
      parser.addOption (nfqso_option);
      parser.addOption (stage_option);
      parser.addOption (clean_option);
      parser.process (app);

      QStringList messages = parser.values (message_option);
      if (messages.isEmpty ())
        messages = {QStringLiteral ("CQ IU8LMC JN70"), QStringLiteral ("IU8LMC DL9XYZ -12"),
                    QStringLiteral ("DL9XYZ IU8LMC R-08"), QStringLiteral ("RR73 IU8LMC DL9XYZ"),
                    QStringLiteral ("IU8LMC DL9XYZ 73")};

      QList<double> const snr_list = parse_double_list (parser.value (snr_option), "snr-list");
      bool ok = false;
      int const seeds = parser.value (seeds_option).toInt (&ok);
      if (!ok || seeds <= 0) fail (QStringLiteral ("--seeds non valido"));
      float const relax = parser.value (relax_option).toFloat (&ok);
      if (!ok || relax <= 0.0f) fail (QStringLiteral ("--relax non valido"));
      float const freq = parser.value (freq_option).toFloat (&ok);
      float const nfqso = parser.value (nfqso_option).toFloat (&ok);
      int const stage = parser.value (stage_option).toInt (&ok);
      bool const clean_too = parser.isSet (clean_option);
      QString const out_path = parser.value (out_option);

      // Impostate PRIMA del primo decode: decodium_bridge.cpp le legge una
      // sola volta, in modo statico, al primo uso.
      qputenv ("DECODIUM_LDPC_GATE", "1");
      qputenv ("DECODIUM_LDPC_GATE_RELAX", QByteArray::number (relax));

      fastldpc_gate_dump_open_c (out_path.toLocal8Bit ().constData ());

      QTextStream out {stdout};
      long long trials = 0, decodes = 0;
      for (QString const& message : messages)
        {
          decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeFt2 (message);
          if (!encoded.ok)
            {
              out << "skip (encode fallito): " << message << '\n';
              continue;
            }
          std::array<signed char, kFt2Codeword> const truth = expected_codeword (encoded.msgbits);

          auto run_one = [&] (bool has_noise, double snr_db, unsigned seed) {
            std::vector<qint16> const pcm =
                make_wav_samples (message, freq, 0.85f, 600.0f, static_cast<float> (snr_db), has_noise, seed);
            fastldpc_gate_truth_set_c (truth.data ());
            int const n = run_decode (pcm, stage, nfqso);
            fastldpc_gate_truth_clear_c ();
            ++trials;
            decodes += n;
          };

          if (clean_too) run_one (false, 0.0, 0);
          for (double snr_db : snr_list)
            for (int seed = 0; seed < seeds; ++seed)
              run_one (true, snr_db, static_cast<unsigned> (1000000 + seed));

          out << "messaggio \"" << message << "\": fatto (" << trials << " prove finora, "
              << decodes << " decodifiche totali)\n";
          out.flush ();
        }

      fastldpc_gate_dump_close_c ();
      out << "totale: " << trials << " prove -> " << out_path << '\n';
      return 0;
    }
  catch (std::exception const& error)
    {
      QTextStream err {stderr};
      err << "ft2_gate_dump: " << error.what () << '\n';
      err.flush ();
      return 1;
    }
}
