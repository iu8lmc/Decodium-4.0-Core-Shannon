#include <QCoreApplication>
#include <QFile>
#include <QString>
#include <QVector>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <vector>

#include "Detector/JT9FastDecoder.hpp"
#include "Detector/JT9WideDecoder.hpp"
#include "Detector/LegacyDspIoHelpers.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "commons.h"
#include "wsjtx_config.h"

extern "C"
{
  void sync9w_ (float ss[], int* nzhsym, int* lag1, int* lag2, int* ia, int* ib,
                float ccfred[], float ccfblue[], int* ipkbest, int* lagpk, int* nadd);
  void softsym9w_ (short id2[], int* npts, float* xdt0, float* f0, float* width,
                   int* nsubmode, float* xdt1, float* snrdb, std::int8_t i1softsymbols[]);
  void symspec_ (dec_data_t* shared_data, int* k, double* trperiod, int* nsps,
                 int* ingain, signed char* low_sidelobes, int* nminw,
                 float* pxdb, float* spectrum, float* df3, int* ihsym,
                 int* npts8, float* pxdbmax, float* npct);
  void jt9fano_ (std::int8_t i1SoftSymbols[], int* limit, int* nlim, char msg[],
                 fortran_charlen_t);
}

namespace
{

constexpr int kSampleRate = RX_SAMPLE_RATE;
constexpr int kAudioCount = 60 * kSampleRate;
constexpr int kNsps = 6912;
constexpr int kJstep = kNsps / 2;
constexpr int kWideLimit = 10000;

struct FortranSyncPass
{
  int ipkbest {0};
  int lagpk {0};
  std::vector<float> ccfred;
  std::array<float, 28> ccfblue {};
};

struct WavData
{
  int sampleRate {};
  int channels {};
  int bitsPerSample {};
  QVector<short> samples;
};

[[noreturn]] void fail (QString const& message)
{
  throw std::runtime_error {message.toStdString ()};
}

int env_int (char const* name, int fallback)
{
  if (char const* raw = std::getenv (name))
    {
      bool ok = false;
      int const value = QString::fromLocal8Bit (raw).toInt (&ok);
      if (ok)
        {
          return value;
        }
    }
  return fallback;
}

float env_float (char const* name, float fallback)
{
  if (char const* raw = std::getenv (name))
    {
      bool ok = false;
      float const value = QString::fromLocal8Bit (raw).toFloat (&ok);
      if (ok)
        {
          return value;
        }
    }
  return fallback;
}

QByteArray trim_trailing_spaces (QByteArray bytes)
{
  while (!bytes.isEmpty ())
    {
      char const c = bytes.back ();
      if (c == ' ' || c == '\0' || c == '\r' || c == '\n' || c == '\t')
        {
          bytes.chop (1);
          continue;
        }
      break;
    }
  return bytes;
}

QByteArray decode_jt9fano_fortran (std::array<std::int8_t, 207> const& soft, int limit, int* nlim_out)
{
  std::array<std::int8_t, 207> work = soft;
  std::array<char, 22> decoded {};
  int nlim = 0;
  jt9fano_ (work.data (), &limit, &nlim, decoded.data (), static_cast<fortran_charlen_t> (decoded.size ()));
  if (nlim_out)
    {
      *nlim_out = nlim;
    }
  return trim_trailing_spaces (QByteArray {decoded.data (), int (decoded.size ())});
}

QString env_string (char const* name, QString const& fallback)
{
  if (char const* raw = std::getenv (name))
    {
      return QString::fromLocal8Bit (raw);
    }
  return fallback;
}

WavData read_wav_file (QString const& fileName)
{
  QFile file {fileName};
  if (!file.open (QIODevice::ReadOnly))
    {
      fail (QStringLiteral ("cannot open WAV file \"%1\": %2").arg (fileName, file.errorString ()));
    }

  QByteArray const blob = file.readAll ();
  if (blob.size () < 12)
    {
      fail (QStringLiteral ("WAV file \"%1\" is too short").arg (fileName));
    }
  if (blob.mid (0, 4) != "RIFF" || blob.mid (8, 4) != "WAVE")
    {
      fail (QStringLiteral ("WAV file \"%1\" is not RIFF/WAVE").arg (fileName));
    }
  quint32 const riffSize =
      qFromLittleEndian<quint32> (reinterpret_cast<uchar const*> (blob.constData () + 4));
  int const riffEnd = std::min<int> (blob.size (), static_cast<int> (riffSize) + 8);

  bool haveFmt = false;
  bool haveData = false;
  quint16 audioFormat = 0;
  quint16 channels = 0;
  quint32 sampleRate = 0;
  quint16 bitsPerSample = 0;
  QByteArray dataChunk;

  int pos = 12;
  while (pos + 8 <= riffEnd)
    {
      QByteArray const chunkId = blob.mid (pos, 4);
      quint32 const chunkSize = qFromLittleEndian<quint32> (
          reinterpret_cast<uchar const*> (blob.constData () + pos + 4));
      pos += 8;
      if (pos + static_cast<int> (chunkSize) > riffEnd)
        {
          fail (QStringLiteral ("WAV file \"%1\" has a truncated chunk").arg (fileName));
        }

      if (chunkId == "fmt ")
        {
          if (chunkSize < 16)
            {
              fail (QStringLiteral ("WAV file \"%1\" has an invalid fmt chunk").arg (fileName));
            }
          auto const* fmt = reinterpret_cast<uchar const*> (blob.constData () + pos);
          audioFormat = qFromLittleEndian<quint16> (fmt);
          channels = qFromLittleEndian<quint16> (fmt + 2);
          sampleRate = qFromLittleEndian<quint32> (fmt + 4);
          bitsPerSample = qFromLittleEndian<quint16> (fmt + 14);
          haveFmt = true;
        }
      else if (chunkId == "data")
        {
          dataChunk = blob.mid (pos, static_cast<int> (chunkSize));
          haveData = true;
        }

      pos += static_cast<int> ((chunkSize + 1u) & ~1u);
    }

  if (!haveFmt || !haveData)
    {
      fail (QStringLiteral ("WAV file \"%1\" is missing fmt or data chunks").arg (fileName));
    }
  if (audioFormat != 1u || channels != 1u || bitsPerSample != 16u)
    {
      fail (QStringLiteral ("WAV file \"%1\" is not PCM mono 16-bit").arg (fileName));
    }

  int sampleCount = dataChunk.size () / 2;
  QVector<short> samples (sampleCount);
  auto const* raw = reinterpret_cast<uchar const*> (dataChunk.constData ());
  for (int i = 0; i < sampleCount; ++i)
    {
      samples[i] = static_cast<short> (qFromLittleEndian<qint16> (raw + 2 * i));
    }

  WavData wav;
  wav.sampleRate = int (sampleRate);
  wav.channels = int (channels);
  wav.bitsPerSample = int (bitsPerSample);
  wav.samples = std::move (samples);
  return wav;
}

QVector<short> synthesize_wide_audio (QString const& message, int nsubmode, float base_freq)
{
  auto const encoded = decodium::txmsg::encodeJt9 (message, false);
  QVector<short> audio (kAudioCount, 0);
  if (!encoded.ok)
    {
      return audio;
    }

  struct DeterministicRng
  {
    quint64 state {0x9e3779b97f4a7c15ULL};

    float uniform ()
    {
      state = state * 6364136223846793005ULL + 1ULL;
      quint64 const bits = (state >> 11) & ((1ULL << 53) - 1ULL);
      return static_cast<float> (bits * (1.0 / 9007199254740992.0));
    }

    float normal ()
    {
      float u1 = std::max (uniform (), 1.0e-12f);
      float const u2 = uniform ();
      return std::sqrt (-2.0f * std::log (u1)) * std::cos (float (2.0 * M_PI) * u2);
    }
  } rng;

  float constexpr rms = 100.0f;
  float constexpr bandwidth_ratio = 2500.0f / 6000.0f;
  float const snrdb = env_float ("JT9_WIDE_SYNTH_SNR", 0.0f);
  float const sig = std::sqrt (2.0f * bandwidth_ratio) * std::pow (10.0f, 0.05f * snrdb);
  std::vector<float> work (static_cast<std::size_t> (audio.size ()), 0.0f);
  for (float& sample : work)
    {
      sample = rng.normal ();
    }

  float const tone_spacing = (static_cast<float> (kSampleRate) / kNsps) * static_cast<float> (1 << nsubmode);
  float phase = 0.0f;
  int pos = kSampleRate;
  for (int tone : encoded.tones)
    {
      float const freq = base_freq + tone * tone_spacing;
      float const delta = float (2.0 * M_PI) * freq / kSampleRate;
      for (int i = 0; i < kNsps && pos < audio.size (); ++i, ++pos)
        {
          work[static_cast<std::size_t> (pos)] += sig * std::sin (phase);
          phase += delta;
          if (phase > float (2.0 * M_PI))
            {
              phase = std::fmod (phase, float (2.0 * M_PI));
            }
        }
    }

  for (int i = 0; i < audio.size (); ++i)
    {
      float const sample = rms * work[static_cast<std::size_t> (i)];
      float const clamped = std::max (-32767.0f, std::min (32767.0f, sample));
      audio[i] = static_cast<short> (std::lround (clamped));
    }

  return audio;
}

void fill_request_common (QVector<short> const& audio, int nutc, int nfqso, int ntol, int nsubmode,
                          decodium::legacyjt::DecodeRequest* request_out)
{
  request_out->serial = 1;
  request_out->mode = QStringLiteral ("JT9");
  request_out->audio = audio;
  request_out->nutc = nutc;
  request_out->nfqso = nfqso;
  request_out->ntol = ntol;
  request_out->nsubmode = nsubmode;
}

void build_request_spectra_cpp (QVector<short> const& audio, int nutc, int nfqso, int ntol, int nsubmode,
                                decodium::legacyjt::DecodeRequest* request_out)
{
  std::unique_ptr<dec_data_t> shared {new dec_data_t {}};
  std::fill_n (shared->ipc, 3, 0);
  std::fill_n (shared->ss, 184 * NSMAX, 0.0f);
  std::fill_n (shared->savg, NSMAX, 0.0f);
  std::fill_n (shared->sred, 5760, 0.0f);
  std::fill_n (shared->d2, NTMAX * RX_SAMPLE_RATE, short {0});

  std::copy_n (audio.constBegin (), std::min<int> (audio.size (), NTMAX * RX_SAMPLE_RATE), shared->d2);
  shared->params.ndiskdat = true;

  float pxdb = 0.0f;
  std::vector<float> spectrum (NSMAX, 0.0f);
  float df3 = 0.0f;
  float pxdbmax = 0.0f;
  int ihsym = 0;
  int npts8 = 0;
  int const max_samples = std::min (audio.size (), NTMAX * RX_SAMPLE_RATE);
  for (int k = kJstep; k <= max_samples && ihsym < 184; k += kJstep)
    {
      decodium::legacy::symspec_update (shared.get (), k, kNsps, 0, false, 0,
                                        &pxdb, spectrum.data (), &df3, &ihsym, &npts8, &pxdbmax);
    }

  fill_request_common (audio, nutc, nfqso, ntol, nsubmode, request_out);
  request_out->ss.resize (184 * NSMAX);
  std::copy_n (shared->ss, request_out->ss.size (), request_out->ss.begin ());
  request_out->npts8 = npts8;
  request_out->nzhsym = ihsym;
}

  void build_request_spectra_fortran (QVector<short> const& audio, int nutc, int nfqso, int ntol, int nsubmode,
                                      decodium::legacyjt::DecodeRequest* request_out)
  {
    std::unique_ptr<dec_data_t> shared {new dec_data_t {}};
    shared->params.ndiskdat = true;

  int const copyCount = std::min (audio.size (), NTMAX * RX_SAMPLE_RATE);
  if (copyCount > 0)
    {
      std::copy_n (audio.constBegin (), copyCount, shared->d2);
    }

  signed char lowSidelobes = 0;
  int ingain = 0;
  int nminw = 1;
  int nsps = kNsps;
  float pxdb = 0.0f;
  QVector<float> spectrum (NSMAX);
  float df3 = 0.0f;
  int ihsym = 0;
  int npts8 = 0;
  float pxdbmax = 0.0f;
  float npct = 0.0f;
  int nhsym = 0;
  int nhsym0 = -999;
  double trPeriod = std::max (60.0, audio.size () / static_cast<double> (RX_SAMPLE_RATE));
  int const npts = std::min (audio.size (), NTMAX * RX_SAMPLE_RATE);

  for (int block = 1; block <= npts / kJstep; ++block)
    {
      int k = block * kJstep;
      symspec_ (shared.get (), &k, &trPeriod, &nsps,
                &ingain, &lowSidelobes, &nminw, &pxdb, spectrum.data (),
                &df3, &ihsym, &npts8, &pxdbmax, &npct);
      nhsym = (k - 2048) / kJstep;
      if (nhsym >= 1 && nhsym != nhsym0)
        {
          nhsym0 = nhsym;
          if (nhsym >= 181)
            {
              break;
            }
        }
    }

  fill_request_common (audio, nutc, nfqso, ntol, nsubmode, request_out);
  request_out->ss.resize (184 * NSMAX);
  std::copy_n (shared->ss, request_out->ss.size (), request_out->ss.begin ());
  request_out->npts8 = npts8;
  request_out->nzhsym = std::max (0, nhsym0);
}

FortranSyncPass first_sync_fortran (decodium::legacyjt::DecodeRequest const& request)
{
  FortranSyncPass result;
  float const df = static_cast<float> (RX_SAMPLE_RATE) / 16384.0f;
  int const nzhsym = 184;
  int const ia = std::max (1, static_cast<int> (std::lround ((request.nfqso - request.ntol) / df)));
  int const ib = std::min (NSMAX, static_cast<int> (std::lround ((request.nfqso + request.ntol) / df)));
  int const lag1 = -static_cast<int> (2.5 / (0.5 * kNsps / static_cast<double> (RX_SAMPLE_RATE)) + 0.9999);
  int const lag2 = static_cast<int> (5.0 / (0.5 * kNsps / static_cast<double> (RX_SAMPLE_RATE)) + 0.9999);

  std::vector<float> ss (request.ss.constBegin (), request.ss.constEnd ());
  result.ccfred.assign (NSMAX, 0.0f);
  int nadd = 3;
  sync9w_ (ss.data (), const_cast<int*> (&nzhsym), const_cast<int*> (&lag1), const_cast<int*> (&lag2),
           const_cast<int*> (&ia), const_cast<int*> (&ib), result.ccfred.data (),
           result.ccfblue.data (), &result.ipkbest, &result.lagpk, &nadd);

  return result;
}

bool nearly_equal (float lhs, float rhs, float abs_tol, float rel_tol)
{
  float const scale = std::max ({1.0f, std::fabs (lhs), std::fabs (rhs)});
  return std::fabs (lhs - rhs) <= std::max (abs_tol, rel_tol * scale);
}

bool compare_case (QString const& message, int nsubmode, int nutc, int nfqso, int ntol)
{
  QVector<short> audio;
  if (char const* wav_path = std::getenv ("JT9_WIDE_WAV"))
    {
      WavData const wav = read_wav_file (QString::fromLocal8Bit (wav_path));
      if (wav.sampleRate != RX_SAMPLE_RATE)
        {
          std::fprintf (stderr, "JT9 wide stage compare requires 12000 Hz WAV, got %d\n", wav.sampleRate);
          return false;
        }
      audio = wav.samples;
    }
  else
    {
      audio = synthesize_wide_audio (message, nsubmode, float (nfqso));
    }
  decodium::legacyjt::DecodeRequest request_cpp;
  decodium::legacyjt::DecodeRequest request_ftn;
  build_request_spectra_cpp (audio, nutc, nfqso, ntol, nsubmode, &request_cpp);
  build_request_spectra_fortran (audio, nutc, nfqso, ntol, nsubmode, &request_ftn);

  if (request_cpp.ss.size () != request_ftn.ss.size ())
    {
      std::fprintf (stderr, "JT9 wide request size mismatch for '%s'\n",
                    message.toLatin1 ().constData ());
      return false;
    }

  if (qEnvironmentVariableIsSet ("JT9_WIDE_DUMP"))
    {
      int worst_index = -1;
      float worst_abs = 0.0f;
      for (int i = 0; i < request_cpp.ss.size (); ++i)
        {
          float const diff = std::fabs (request_cpp.ss[i] - request_ftn.ss[i]);
          if (diff > worst_abs)
            {
              worst_abs = diff;
              worst_index = i;
            }
        }
      std::fprintf (stderr,
                    "spectra: cpp nzhsym=%d npts8=%d, ftn nzhsym=%d npts8=%d, worst_ss[%d]=%.6g\n",
                    request_cpp.nzhsym, request_cpp.npts8, request_ftn.nzhsym, request_ftn.npts8,
                    worst_index, worst_abs);
    }

  bool const use_fortran_spectra = qEnvironmentVariableIsSet ("JT9_WIDE_USE_FTN_SPECTRA");
  auto const& request = use_fortran_spectra ? request_ftn : request_cpp;

  float const df = static_cast<float> (RX_SAMPLE_RATE) / 16384.0f;
  int const ia = std::max (1, static_cast<int> (std::lround ((request.nfqso - request.ntol) / df)));
  int const ib = std::min (NSMAX, static_cast<int> (std::lround ((request.nfqso + request.ntol) / df)));
  int const lag1 = -static_cast<int> (2.5 / (0.5 * kNsps / static_cast<double> (RX_SAMPLE_RATE)) + 0.9999);
  int const lag2 = static_cast<int> (5.0 / (0.5 * kNsps / static_cast<double> (RX_SAMPLE_RATE)) + 0.9999);
  auto const sync_cpp = decodium::jt9wide::detail::run_sync_pass (request.ss, 184, lag1, lag2, ia, ib, 3);
  auto const sync_ref = first_sync_fortran (request);

  if (sync_cpp.lagpk != sync_ref.lagpk)
    {
      std::fprintf (stderr,
                    "JT9 wide sync lag mismatch for '%s'\n"
                    "  cpp lag=%d\n"
                    "  ftn lag=%d ipkbest=%d\n",
                    message.toLatin1 ().constData (),
                    sync_cpp.lagpk, sync_ref.lagpk, sync_ref.ipkbest);
      return false;
    }

  for (int i = ia; i <= ib; ++i)
    {
      float const lhs = sync_cpp.ccfred[static_cast<std::size_t> (i)];
      float const rhs = sync_ref.ccfred[static_cast<std::size_t> (i - 1)];
      if (!nearly_equal (lhs, rhs, 2.0e-3f, 2.0e-3f))
        {
          std::fprintf (stderr,
                        "JT9 wide sync spectrum mismatch for '%s' at bin %d: cpp=%.6f ftn=%.6f\n",
                        message.toLatin1 ().constData (), i, lhs, rhs);
          return false;
        }
    }

  for (int i = 0; i < 28; ++i)
    {
      if (!nearly_equal (sync_cpp.ccfblue[static_cast<std::size_t> (i)],
                         sync_ref.ccfblue[static_cast<std::size_t> (i)], 2.0e-3f, 2.0e-3f))
        {
          std::fprintf (stderr,
                        "JT9 wide sync time mismatch for '%s' at lag slot %d: cpp=%.6f ftn=%.6f\n",
                        message.toLatin1 ().constData (), i - 9,
                        sync_cpp.ccfblue[static_cast<std::size_t> (i)],
                        sync_ref.ccfblue[static_cast<std::size_t> (i)]);
          return false;
        }
    }

  std::array<std::int8_t, 207> soft_ref {};
  int npts = std::min (kAudioCount, request.audio.size ());
  float xdt1_ref = 0.0f;
  float snrdb_ref = 0.0f;
  float xdt0 = 1.0f;
  float f0 = static_cast<float> (nfqso);
  float width = 2.0f * (static_cast<float> (RX_SAMPLE_RATE) / 16384.0f);
  int submode = request.nsubmode;
  softsym9w_ (const_cast<short*> (request.audio.constData ()), &npts, &xdt0, &f0, &width, &submode,
              &xdt1_ref, &snrdb_ref, soft_ref.data ());

  auto const soft_cpp = decodium::jt9wide::detail::compute_softsym (request.audio, xdt0, f0, width, request.nsubmode);
  if (!soft_cpp.ok)
    {
      std::fprintf (stderr, "JT9 wide softsym failed for '%s'\n", message.toLatin1 ().constData ());
      return false;
    }

  if (!nearly_equal (soft_cpp.xdt1, xdt1_ref, 1.0e-3f, 1.0e-3f)
      || !nearly_equal (static_cast<float> (soft_cpp.nsnr), std::round (snrdb_ref), 1.0f, 0.0f))
    {
      std::fprintf (stderr,
                    "JT9 wide softsym summary mismatch for '%s'\n"
                    "  cpp: xdt1=%.6f nsnr=%d\n"
                    "  ftn: xdt1=%.6f nsnr=%d\n",
                    message.toLatin1 ().constData (),
                    soft_cpp.xdt1, soft_cpp.nsnr,
                    xdt1_ref, static_cast<int> (std::lround (snrdb_ref)));
      return false;
    }

  for (int i = 0; i < 207; ++i)
    {
      if (soft_cpp.soft[static_cast<std::size_t> (i)] != soft_ref[static_cast<std::size_t> (i)])
        {
          std::fprintf (stderr,
                        "JT9 wide soft symbol mismatch for '%s' at %d: cpp=%d ftn=%d\n",
                        message.toLatin1 ().constData (), i,
                        static_cast<int> (soft_cpp.soft[static_cast<std::size_t> (i)]),
                        static_cast<int> (soft_ref[static_cast<std::size_t> (i)]));
          return false;
        }
    }

  int nlim_ref = 0;
  int nlim_cpp = 0;
  QByteArray const msg_ref = trim_trailing_spaces (decode_jt9fano_fortran (soft_ref, kWideLimit, &nlim_ref));
  QByteArray const msg_cpp = trim_trailing_spaces (decodium::jt9fast::decode_soft_symbols (soft_cpp.soft, kWideLimit, &nlim_cpp));
  if (msg_ref != msg_cpp)
    {
      std::fprintf (stderr,
                    "JT9 wide decoded soft message mismatch for '%s'\n"
                    "  cpp='%.*s' nlim=%d\n"
                    "  ftn='%.*s' nlim=%d\n",
                    message.toLatin1 ().constData (),
                    msg_cpp.size (), msg_cpp.constData (), nlim_cpp,
                    msg_ref.size (), msg_ref.constData (), nlim_ref);
      return false;
    }

  return true;
}

}

int main (int argc, char** argv)
{
  try
    {
      QCoreApplication app {argc, argv};
      QString const message = env_string ("JT9_WIDE_MESSAGE", QStringLiteral ("CQ WB9XYZ EN34"));
      int const nsubmode = env_int ("JT9_WIDE_SUBMODE", 1);
      int const nutc = env_int ("JT9_WIDE_NUTC", 1234);
      int const nfqso = env_int ("JT9_WIDE_NFQSO", 1200);
      int const ntol = env_int ("JT9_WIDE_NTOL", 100);

      bool ok = compare_case (message, nsubmode, nutc, nfqso, ntol);
      if (!ok)
        {
          return 1;
        }

      std::printf ("JT9 wide stage compare passed\n");
      return 0;
    }
  catch (std::exception const& error)
    {
      std::fprintf (stderr, "jt9_wide_stage_compare exception: %s\n", error.what ());
      return 1;
    }
}
