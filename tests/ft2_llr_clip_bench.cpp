// ft2_llr_clip_bench.cpp — quanto costa e quanto rende il taglio degli LLR
// (Ft2Config::llr_clip) sul demodulatore 4-GFSK VERO di FT2.
//
// La domanda aperta in Detector/fastldpc/lab/README.md: il taglio e' tarato
// oggi a 2,5 su un canale AWGN/BPSK sintetico, e la nota dice "quale dei due
// (2 o 2,5) sia giusto lo dira' il canale reale". Il banco sintetico non puo'
// rispondere perche' il taglio serve contro l'interferenza IMPULSIVA, che li'
// non c'e': su AWGN puro si vede solo il costo, mai il beneficio.
//
// Questo banco genera il segnale con lo stack TX vero (encodeFt2 +
// generateFt2Wave, come ft2_gate_dump), lo sporca con AWGN e - opzionalmente -
// con impulsi, e lo passa alla catena di decodifica vera (ft2_async_decode_).
// Conta separatamente le decodifiche GIUSTE (testo identico al messaggio
// trasmesso) e quelle SBAGLIATE: sono le due meta' del compromesso, e vanno
// lette insieme.
//
// llr_clip si legge una volta sola, staticamente, al primo decode
// (decodium_bridge.cpp): non si puo' cambiare a meta' processo. Quindi il
// valore si passa con --llr-clip e il banco lo mette nell'ambiente PRIMA di
// decodificare; per confrontare due tagli si lancia due volte con gli STESSI
// semi, cosi' le forme d'onda sono identiche e il confronto e' appaiato.
//
// Uso:
//   ft2_llr_clip_bench --llr-clip 2.5 --snr-list "-20,-18,-16,-14,-12" --seeds 30
//   ft2_llr_clip_bench --llr-clip 2.0 --snr-list "-20,-18,-16,-14,-12" --seeds 30
//   ...e le stesse due righe con --impulse-rate 40, per il caso impulsivo.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
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
}

namespace {

constexpr int kSampleRate {12000};
constexpr int kFrameSamples {45000};
constexpr int kNsps {288};
constexpr int kFt2MaxLines {100};
constexpr int kBitsPerMessage {77};
constexpr int kDecodedChars {37};

struct Conteggio
{
  long long prove {0};
  long long giuste {0};
  long long sbagliate {0};
};

[[noreturn]] void fail (QString const& message)
{
  throw std::runtime_error {message.toStdString ()};
}

QByteArray to_fortran_field (QString const& text, int width)
{
  QByteArray field = text.toLatin1 ();
  field.resize (width, ' ');
  return field;
}

float compute_signal_rms (std::vector<float> const& frame)
{
  double sum2 = 0.0;
  for (float sample : frame) sum2 += static_cast<double> (sample) * sample;
  return static_cast<float> (std::sqrt (sum2 / std::max<size_t> (1, frame.size ())));
}

// Impulsi: brevi picchi di ampiezza alta, il disturbo che il taglio degli LLR
// dovrebbe attutire. Modellati come una manciata di campioni consecutivi con
// segno casuale - abbastanza per saturare i metrici di un simbolo senza
// spostare l'RMS della cornice, che e' esattamente il caso in cui un |LLR|
// enorme e falso trascina l'OSD.
void add_impulses (std::vector<float>& frame, float signal_rms, int rate, float amp,
                   unsigned seed)
{
  if (rate <= 0 || signal_rms <= 0.0f) return;
  std::mt19937 rng {seed};
  std::uniform_int_distribution<int> where {0, kFrameSamples - 16};
  std::uniform_int_distribution<int> width {2, 8};
  std::uniform_int_distribution<int> sign {0, 1};
  for (int k = 0; k < rate; ++k)
    {
      int const start = where (rng);
      int const len = width (rng);
      float const level = amp * signal_rms * (sign (rng) ? 1.0f : -1.0f);
      for (int i = 0; i < len; ++i)
        frame[static_cast<size_t> (start + i)] += level;
    }
}

void add_awgn (std::vector<float>& frame, float signal_rms, float snr_db, unsigned seed)
{
  if (signal_rms <= 0.0f) return;
  double const sigma = static_cast<double> (signal_rms) / std::pow (10.0, static_cast<double> (snr_db) / 20.0);
  std::mt19937 rng {seed};
  std::normal_distribution<float> noise {0.0f, static_cast<float> (sigma)};
  for (float& sample : frame) sample += noise (rng);
}

// L'RMS per la taratura del rumore si misura sul solo segnale, prima degli
// impulsi: altrimenti aggiungendo impulsi si alzerebbe anche il rumore e
// l'SNR nominale non vorrebbe piu' dire niente fra una condizione e l'altra.
// Cornice di SOLO RUMORE: nessun segnale, quindi ogni decodifica che ne esce
// e' un fantasma per costruzione. E' il banco che misura il BENEFICIO del
// filtro di plausibilita', che quello a segnale singolo non puo' dare: i
// fantasmi non nascono dal traffico ma dai candidati di rumore sottoposti
// alla CRC-14. E' lo stesso metodo con cui fu scelta la soglia nd 0,065.
std::vector<qint16> make_noise_samples (unsigned seed)
{
  std::mt19937 rng {seed};
  std::normal_distribution<float> noise {0.0f, 0.05f};
  std::vector<qint16> pcm (static_cast<size_t> (kFrameSamples), 0);
  for (int i = 0; i < kFrameSamples; ++i)
    {
      float const clipped = std::max (-1.0f, std::min (1.0f, noise (rng)));
      pcm[static_cast<size_t> (i)] = static_cast<qint16> (std::lround (static_cast<double> (clipped) * 32767.0));
    }
  return pcm;
}

std::vector<qint16> make_wav_samples (QString const& message, float freq_hz, float gain,
                                      float offset_ms, float snr_db, unsigned seed,
                                      int impulse_rate, float impulse_amp)
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

  float const signal_rms = compute_signal_rms (frame);
  add_impulses (frame, signal_rms, impulse_rate, impulse_amp, seed ^ 0x9e3779b9u);
  add_awgn (frame, signal_rms, snr_db, seed);

  std::vector<qint16> pcm (static_cast<size_t> (kFrameSamples), 0);
  for (int i = 0; i < kFrameSamples; ++i)
    {
      float const clipped = std::max (-1.0f, std::min (1.0f, frame[static_cast<size_t> (i)]));
      pcm[static_cast<size_t> (i)] = static_cast<qint16> (std::lround (static_cast<double> (clipped) * 32767.0));
    }
  return pcm;
}

QString normalizza (QString const& testo)
{
  return testo.simplified ().toUpper ();
}

// Ritorna quante righe decodificate coincidono col messaggio trasmesso e
// quante no. Una riga che non e' il messaggio trasmesso e' una falsa: nella
// cornice c'e' un solo segnale, e nient'altro puo' essere legittimo.
void run_decode (std::vector<qint16> const& pcm, int stage, float nfqso,
                 QString const& atteso, Conteggio& conta)
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

  ++conta.prove;
  int const righe = std::max (0, std::min (nout, kFt2MaxLines));
  for (int i = 0; i < righe; ++i)
    {
      QString const riga = normalizza (QString::fromLatin1 (
          decodeds.data () + static_cast<size_t> (i) * kDecodedChars, kDecodedChars));
      if (riga == atteso) ++conta.giuste;
      else ++conta.sbagliate;
    }
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
      QCoreApplication::setApplicationName (QStringLiteral ("ft2_llr_clip_bench"));

      QCommandLineParser parser;
      parser.setApplicationDescription (
          QStringLiteral ("Misura decodifiche giuste e false al variare di llr_clip, sul demodulatore FT2 vero."));
      parser.addHelpOption ();

      QCommandLineOption clip_option {"llr-clip", "Valore di llr_clip (0 = nessun taglio).", "value", "2.5"};
      QCommandLineOption message_option {"message", "Messaggio FT2 da provare (ripetibile).", "text"};
      QCommandLineOption snr_option {"snr-list", "SNR in dB separati da virgola.", "list",
                                     "-20,-18,-16,-14,-12,-10"};
      QCommandLineOption seeds_option {"seeds", "Semi di rumore per (messaggio,SNR).", "n", "30"};
      QCommandLineOption impulse_rate_option {"impulse-rate", "Impulsi per cornice (0 = nessuno).", "n", "0"};
      QCommandLineOption impulse_amp_option {"impulse-amp", "Ampiezza degli impulsi, in RMS di segnale.", "value", "8.0"};
      QCommandLineOption freq_option {"freq", "Frequenza audio in Hz.", "hz", "1000.0"};
      QCommandLineOption nfqso_option {"nfqso", "Frequenza QSO attesa in Hz.", "hz", "1000"};
      QCommandLineOption stage_option {"stage", "Stage FT2 DSP.", "n", "7"};
      QCommandLineOption noise_option {"noise-only", "Cornici di solo rumore: ogni decodifica e' un fantasma."};

      parser.addOption (clip_option);
      parser.addOption (message_option);
      parser.addOption (snr_option);
      parser.addOption (seeds_option);
      parser.addOption (impulse_rate_option);
      parser.addOption (impulse_amp_option);
      parser.addOption (freq_option);
      parser.addOption (nfqso_option);
      parser.addOption (stage_option);
      parser.addOption (noise_option);
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
      double const clip = parser.value (clip_option).toDouble (&ok);
      if (!ok || clip < 0.0) fail (QStringLiteral ("--llr-clip non valido"));
      int const impulse_rate = parser.value (impulse_rate_option).toInt (&ok);
      if (!ok || impulse_rate < 0) fail (QStringLiteral ("--impulse-rate non valido"));
      float const impulse_amp = parser.value (impulse_amp_option).toFloat (&ok);
      float const freq = parser.value (freq_option).toFloat (&ok);
      float const nfqso = parser.value (nfqso_option).toFloat (&ok);
      int const stage = parser.value (stage_option).toInt (&ok);
      bool const noise_only = parser.isSet (noise_option);

      // Prima di qualunque decodifica: decodium_bridge.cpp legge la variabile
      // una volta sola, staticamente, alla costruzione del primo decoder.
      qputenv ("DECODIUM_LDPC_LLR_CLIP", QByteArray::number (clip));

      QTextStream out {stdout};
      out << "llr_clip=" << clip
          << " impulsi=" << impulse_rate
          << (impulse_rate > 0 ? QStringLiteral (" (amp %1x RMS)").arg (impulse_amp) : QString {})
          << " semi=" << seeds
          << " messaggi=" << messages.size ()
          << " stage=" << stage << "\n\n";

      std::map<double, Conteggio> per_snr;
      Conteggio totale;

      for (QString const& message : messages)
        {
          QString const atteso = normalizza (message);
          for (double snr_db : snr_list)
            for (int seed = 0; seed < seeds; ++seed)
              {
                unsigned const s = static_cast<unsigned> (1000000 + seed);
                std::vector<qint16> const pcm =
                    noise_only
                        ? make_noise_samples (s + static_cast<unsigned> (17 * (int) snr_db))
                        : make_wav_samples (message, freq, 0.85f, 600.0f, static_cast<float> (snr_db), s,
                                            impulse_rate, impulse_amp);
                Conteggio prima = per_snr[snr_db];
                run_decode (pcm, stage, nfqso, atteso, per_snr[snr_db]);
                totale.prove += per_snr[snr_db].prove - prima.prove;
                totale.giuste += per_snr[snr_db].giuste - prima.giuste;
                totale.sbagliate += per_snr[snr_db].sbagliate - prima.sbagliate;
              }
          out << "messaggio \"" << message << "\": fatto\n";
          out.flush ();
        }

      out << "\n| SNR dB | prove | giuste | % | false |\n";
      out << "|---:|---:|---:|---:|---:|\n";
      for (auto const& [snr_db, c] : per_snr)
        out << "| " << snr_db << " | " << c.prove << " | " << c.giuste << " | "
            << QString::number (100.0 * static_cast<double> (c.giuste) / std::max<long long> (1, c.prove), 'f', 1)
            << " | " << c.sbagliate << " |\n";
      out << "\ntotale: " << totale.prove << " prove, " << totale.giuste << " giuste ("
          << QString::number (100.0 * static_cast<double> (totale.giuste) / std::max<long long> (1, totale.prove), 'f', 1)
          << "%), " << totale.sbagliate << " false\n";
      return 0;
    }
  catch (std::exception const& error)
    {
      QTextStream err {stderr};
      err << "ft2_llr_clip_bench: " << error.what () << '\n';
      return 1;
    }
}
