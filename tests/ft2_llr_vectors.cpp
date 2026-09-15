// ft2_llr_vectors.cpp — vettori .llr dal demodulatore 4-GFSK VERO di FT2.
//
// Tutti i vettori in Detector/fastldpc/lab/data/*.llr sono sintetici: li
// genera tools/gen_test.py con un canale BPSK/AWGN e LLR esatti
// (llr = 2*y/sigma^2). La roadmap del laboratorio chiede da tempo la stessa
// misura sui LLR VERI, perche' il demodulatore a 4 toni non produce affatto
// quella distribuzione: i suoi metrici nascono da max1-max0 su quattro
// correlatori, normalizzati in RMS, e non sono LLR calibrati.
//
// Questo programma percorre la catena vera fino al punto esatto in cui il
// decoder riceve gli LLR:
//   encodeFt2 -> generateFt2Wave  (stack TX di produzione)
//   AWGN al rapporto voluto
//   ftx_ft2_downsample_c + ricerca di sincronismo (ftx_sync2d_c), come Stage7
//   ftx_ft2_bitmetrics_c          (demodulatore 4-GFSK di produzione)
//   build_llr_sets                (scalefac 2,83 e mappatura di FtxFt2Stage7)
// e scrive il risultato nel formato binario che i banchi del laboratorio
// (ml_gap2, gate_stats, bench*) gia' leggono.
//
// Convenzione dei segni: dentro Decodium LLR positivo = bit 1, in fastldpc
// positivo = bit 0 (decodium_bridge.cpp nega all'ingresso). I file .llr sono
// letti da programmi che includono ft2_decoder.hpp, quindi qui si scrive
// nella convenzione di fastldpc, gia' negata.
//
// Le passate cieche di Stage7 usano cinque insiemi di metrici (llra..llre,
// ipass 1..5): --set sceglie quale scrivere. "a" e' la prima passata.
//
// Uso:
//   ft2_llr_vectors --out reale_-16.llr --snr-list "-16" --seeds 400
//   ft2_llr_vectors --out reale_mix.llr --snr-list "-18,-17,-16,-15" --seeds 200 --set d
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
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

extern "C"
{
  void ftx_getcandidates2_c (float const* dd, float fa, float fb, float syncmin, float nfqso,
                             int maxcand, float* savg, float* candidate, int* ncand, float* sbase);
  void ftx_sync2d_c (std::complex<float> const* cd0, int np, int i0,
                     std::complex<float> const* ctwk, int itwk, float* sync);
  void ftx_ft2_downsample_c (float const* dd, int* newdata, float f0, std::complex<float>* c);
  void ftx_twkfreq1_c (std::complex<float> const* ca, int const* npts, float const* fsample,
                       float const* a, std::complex<float>* cb);
  void ftx_ft2_bitmetrics_c (std::complex<float> const* cd, float* bitmetrics, int* badsync);
  void ftx_ft2_rvec_c (signed char* out77);
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

[[noreturn]] void fail (QString const& message)
{
  throw std::runtime_error {message.toStdString ()};
}

float signal_rms (std::vector<float> const& frame)
{
  double sum2 = 0.0;
  for (float s : frame) sum2 += static_cast<double> (s) * s;
  return static_cast<float> (std::sqrt (sum2 / std::max<size_t> (1, frame.size ())));
}

void normalize_complex (Complex* data, int count)
{
  float sum2 = 0.0f;
  for (int i = 0; i < count; ++i) sum2 += std::norm (data[i]);
  sum2 /= static_cast<float> (count);
  if (sum2 <= 0.0f) return;
  float const scale = 1.0f / std::sqrt (sum2);
  for (int i = 0; i < count; ++i) data[i] *= scale;
}

void extract_window (std::array<Complex, kNSeg>& cd, std::array<Complex, kNdMax> const& cb,
                     int istart)
{
  std::fill (cd.begin (), cd.end (), Complex {});
  if (istart >= 0)
    {
      int const it = std::min (kNdMax - 1, istart + kNSeg - 1);
      int const np = it - istart + 1;
      if (np > 0) std::copy_n (cb.begin () + istart, np, cd.begin ());
    }
  else
    {
      int const start = -istart;
      int const count = kNSeg + 2 * istart;
      if (start >= 0 && count > 0 && start + count <= kNSeg)
        std::copy_n (cb.begin (), count, cd.begin () + start);
    }
}

std::array<std::array<Complex, 2 * kNss>, 33> make_tweaks ()
{
  std::array<std::array<Complex, 2 * kNss>, 33> tweaks {};
  std::array<Complex, 2 * kNss> ones {};
  std::fill (ones.begin (), ones.end (), Complex {1.0f, 0.0f});
  int const npts = static_cast<int> (ones.size ());
  float const fsample = kFsDown / 2.0f;
  for (int idf = -16; idf <= 16; ++idf)
    {
      std::array<float, 5> a {};
      a[0] = static_cast<float> (idf);
      ftx_twkfreq1_c (ones.data (), &npts, &fsample, a.data (), tweaks[static_cast<size_t> (idf + 16)].data ());
    }
  return tweaks;
}

// Stessa mappatura e stesso scalefac di FtxFt2Stage7::build_llr_sets: i tre
// blocchi di 58 bit saltano gli 8 simboli di sincronismo in testa e i due
// blocchi intermedi. llrd e' il piu' forte dei tre per bit, llre il secondo.
void build_llr_sets (float const* metrics,
                     std::array<float, kCodeword>& llra, std::array<float, kCodeword>& llrb,
                     std::array<float, kCodeword>& llrc, std::array<float, kCodeword>& llrd,
                     std::array<float, kCodeword>& llre)
{
  float const scalefac = 2.83f;
  for (int i = 0; i < 58; ++i)
    {
      llra[static_cast<size_t> (i)] = metrics[8 + i] * scalefac;
      llra[static_cast<size_t> (58 + i)] = metrics[74 + i] * scalefac;
      llra[static_cast<size_t> (116 + i)] = metrics[140 + i] * scalefac;
      llrb[static_cast<size_t> (i)] = metrics[8 + i + kRows] * scalefac;
      llrb[static_cast<size_t> (58 + i)] = metrics[74 + i + kRows] * scalefac;
      llrb[static_cast<size_t> (116 + i)] = metrics[140 + i + kRows] * scalefac;
      llrc[static_cast<size_t> (i)] = metrics[8 + i + 2 * kRows] * scalefac;
      llrc[static_cast<size_t> (58 + i)] = metrics[74 + i + 2 * kRows] * scalefac;
      llrc[static_cast<size_t> (116 + i)] = metrics[140 + i + 2 * kRows] * scalefac;
    }
  for (int i = 0; i < kCodeword; ++i)
    {
      float const a = llra[static_cast<size_t> (i)];
      float const b = llrb[static_cast<size_t> (i)];
      float const c = llrc[static_cast<size_t> (i)];
      float first = a, second = b;
      if (std::fabs (b) > std::fabs (first)) { second = first; first = b; }
      else if (std::fabs (b) > std::fabs (second)) { second = b; }
      if (std::fabs (c) > std::fabs (first)) { second = first; first = c; }
      else if (std::fabs (c) > std::fabs (second)) { second = c; }
      llrd[static_cast<size_t> (i)] = first;
      llre[static_cast<size_t> (i)] = second;
    }
}

std::array<uint8_t, kCodeword> expected_codeword (QByteArray const& msgbits77)
{
  if (msgbits77.size () != kBits77) fail (QStringLiteral ("encodeFt2 non ha prodotto 77 bit"));
  std::array<signed char, kBits77> rvec {};
  ftx_ft2_rvec_c (rvec.data ());
  std::array<signed char, kBits77> scrambled {};
  for (int i = 0; i < kBits77; ++i)
    scrambled[static_cast<size_t> (i)] =
        static_cast<signed char> ((static_cast<int> (msgbits77.at (i) != 0) + rvec[static_cast<size_t> (i)]) & 1);
  std::array<signed char, kCodeword> cw {};
  if (ftx_encode174_91_message77_c (scrambled.data (), cw.data ()) == 0)
    fail (QStringLiteral ("ftx_encode174_91_message77_c ha fallito"));
  std::array<uint8_t, kCodeword> out {};
  for (int i = 0; i < kCodeword; ++i) out[static_cast<size_t> (i)] = static_cast<uint8_t> (cw[static_cast<size_t> (i)] & 1);
  return out;
}

QList<double> parse_double_list (QString const& raw, QString const& name)
{
  QList<double> values;
  for (QString part : raw.split (QLatin1Char {','}, Qt::SkipEmptyParts))
    {
      bool ok = false;
      double const v = part.trimmed ().toDouble (&ok);
      if (!ok) fail (QStringLiteral ("valore non valido in --%1: \"%2\"").arg (name, part));
      values.append (v);
    }
  if (values.isEmpty ()) fail (QStringLiteral ("--%1 e' vuoto").arg (name));
  return values;
}

}  // namespace

int main (int argc, char* argv[])
{
  try
    {
      QCoreApplication app {argc, argv};
      QCoreApplication::setApplicationName (QStringLiteral ("ft2_llr_vectors"));

      QCommandLineParser parser;
      parser.setApplicationDescription (
          QStringLiteral ("Scrive vettori .llr presi dal demodulatore 4-GFSK vero di FT2."));
      parser.addHelpOption ();

      QCommandLineOption out_option {QStringList {"o", "out"}, "File .llr di uscita.", "path", "reale.llr"};
      QCommandLineOption message_option {"message", "Messaggio FT2 (ripetibile).", "text"};
      QCommandLineOption snr_option {"snr-list", "SNR in dB separati da virgola.", "list", "-16"};
      QCommandLineOption seeds_option {"seeds", "Semi di rumore per (messaggio,SNR).", "n", "200"};
      QCommandLineOption set_option {"set", "Insieme di metrici: a,b,c,d,e.", "lettera", "a"};
      QCommandLineOption freq_option {"freq", "Frequenza audio in Hz.", "hz", "1500.0"};
      QCommandLineOption offset_option {"offset-ms", "Inizio del burst in ms.", "ms", "600.0"};

      parser.addOption (out_option);
      parser.addOption (message_option);
      parser.addOption (snr_option);
      parser.addOption (seeds_option);
      parser.addOption (set_option);
      parser.addOption (freq_option);
      parser.addOption (offset_option);
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
      QString const set = parser.value (set_option).toLower ();
      if (set.size () != 1 || set[0] < QLatin1Char {'a'} || set[0] > QLatin1Char {'e'})
        fail (QStringLiteral ("--set deve essere a, b, c, d o e"));
      float const freq = parser.value (freq_option).toFloat (&ok);
      float const offset_ms = parser.value (offset_option).toFloat (&ok);
      QString const out_path = parser.value (out_option);

      auto const tweaks = make_tweaks ();
      std::vector<float> all_llr;
      std::vector<uint8_t> all_tx;
      long long badsync_count = 0, trials = 0;

      QTextStream out {stdout};
      for (QString const& message : messages)
        {
          decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeFt2 (message);
          if (!encoded.ok || encoded.tones.isEmpty ())
            {
              out << "skip (encode fallito): " << message << '\n';
              continue;
            }
          std::array<uint8_t, kCodeword> const truth = expected_codeword (encoded.msgbits);

          QVector<float> const wave = decodium::txwave::generateFt2Wave (
              encoded.tones.constData (), encoded.tones.size (), kNsps,
              static_cast<float> (kSampleRate), freq);
          if (wave.isEmpty ()) fail (QStringLiteral ("generazione forma d'onda fallita"));

          int const offset_samples = static_cast<int> (std::lround (static_cast<double> (offset_ms) * kSampleRate / 1000.0));
          if (offset_samples < 0 || offset_samples + wave.size () > kFrameSamples)
            fail (QStringLiteral ("la forma d'onda non entra nella cornice"));

          for (double snr_db : snr_list)
            for (int seed = 0; seed < seeds; ++seed)
              {
                std::vector<float> frame (static_cast<size_t> (kFrameSamples), 0.0f);
                for (int i = 0; i < wave.size (); ++i)
                  frame[static_cast<size_t> (offset_samples + i)] = 0.85f * wave[i];
                float const rms = signal_rms (frame);
                double const sigma = static_cast<double> (rms) / std::pow (10.0, snr_db / 20.0);
                std::mt19937 rng {static_cast<unsigned> (1000000 + seed)};
                std::normal_distribution<float> noise {0.0f, static_cast<float> (sigma)};
                for (float& s : frame) s += noise (rng);

                std::vector<float> dd (frame.begin (), frame.end ());
                std::array<Complex, kNdMax> cb {};
                int newdata = 1;
                ftx_ft2_downsample_c (dd.data (), &newdata, freq, cb.data ());
                normalize_complex (cb.data (), kNdMax);

                // Ricerca del sincronismo come Stage7, ristretta al segmento 1:
                // grossolana e poi fine attorno al massimo. Il tempo VERO non si
                // impone: si vuole il LLR che il decoder vede davvero, con
                // l'errore di sincronismo che la ricerca lascia.
                int ibest = -1, idfbest = 0;
                for (int isync = 1; isync <= 2; ++isync)
                  {
                    int idfmin = -12, idfmax = 12, idfstp = 3;
                    int ibmin = 216, ibmax = 1120, ibstp = 4;
                    if (isync == 2)
                      {
                        idfmin = idfbest - 4; idfmax = idfbest + 4; idfstp = 1;
                        ibmin = ibest - 5; ibmax = ibest + 5; ibstp = 1;
                      }
                    int best_i = -1, best_df = 0;
                    float smax = -99.0f;
                    for (int idf = idfmin; idf <= idfmax; idf += idfstp)
                      for (int istart = ibmin; istart <= ibmax; istart += ibstp)
                        {
                          float sync = 0.0f;
                          ftx_sync2d_c (cb.data (), kNdMax, istart,
                                        tweaks[static_cast<size_t> (idf + 16)].data (), 1, &sync);
                          if (sync > smax) { smax = sync; best_i = istart; best_df = idf; }
                        }
                    ibest = best_i; idfbest = best_df;
                  }

                std::array<Complex, kNdMax> cb2 {};
                float const f1 = freq + static_cast<float> (idfbest);
                newdata = 0;
                ftx_ft2_downsample_c (dd.data (), &newdata, f1, cb2.data ());
                normalize_complex (cb2.data (), kNdMax);
                std::array<Complex, kNSeg> cd {};
                extract_window (cd, cb2, ibest);

                std::array<float, kRows * 3> metrics {};
                int badsync = 0;
                ftx_ft2_bitmetrics_c (cd.data (), metrics.data (), &badsync);
                if (badsync) ++badsync_count;

                std::array<float, kCodeword> llra {}, llrb {}, llrc {}, llrd {}, llre {};
                build_llr_sets (metrics.data (), llra, llrb, llrc, llrd, llre);
                std::array<float, kCodeword> const& chosen =
                    set == "a" ? llra : set == "b" ? llrb : set == "c" ? llrc : set == "d" ? llrd : llre;

                // Decodium: positivo = bit 1. fastldpc e i file .llr: = bit 0.
                for (int i = 0; i < kCodeword; ++i) all_llr.push_back (-chosen[static_cast<size_t> (i)]);
                for (int i = 0; i < kCodeword; ++i) all_tx.push_back (truth[static_cast<size_t> (i)]);
                ++trials;
              }
          out << "messaggio \"" << message << "\": " << trials << " parole finora\n";
          out.flush ();
        }

      std::FILE* f = std::fopen (out_path.toLocal8Bit ().constData (), "wb");
      if (!f) fail (QStringLiteral ("non riesco ad aprire \"%1\"").arg (out_path));
      int32_t const count = static_cast<int32_t> (trials);
      int32_t const n = kCodeword;
      std::fwrite (&count, 4, 1, f);
      std::fwrite (&n, 4, 1, f);
      std::fwrite (all_llr.data (), 4, all_llr.size (), f);
      std::fwrite (all_tx.data (), 1, all_tx.size (), f);
      std::fclose (f);

      out << "\nscritte " << count << " parole (insieme " << set << ") in " << out_path
          << "; badsync su " << badsync_count << " prove\n";
      return 0;
    }
  catch (std::exception const& error)
    {
      QTextStream err {stderr};
      err << "ft2_llr_vectors: " << error.what () << '\n';
      return 1;
    }
}
