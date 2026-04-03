#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QStringList>
#include <QVector>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>

#include "Detector/JT9WideDecoder.hpp"
#include "Detector/JT9FastDecoder.hpp"
#include "Detector/LegacyDspIoHelpers.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "commons.h"
#include "wsjtx_config.h"

extern "C" void decode9w_ (int* nfqso, int* ntol, int* nsubmode, float ss[], short id2[],
                           float* sync, int* nsnr, float* xdt1, float* f0, char* decoded,
                           fortran_charlen_t);

namespace
{

constexpr int kSampleRate = RX_SAMPLE_RATE;
constexpr int kAudioCount = 60 * kSampleRate;
constexpr int kNsps = 6912;
constexpr int kJstep = kNsps / 2;

struct WavData
{
  int sampleRate {};
  int channels {};
  int bitsPerSample {};
  QVector<short> samples;
};

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

bool env_flag (char const* name)
{
  if (char const* raw = std::getenv (name))
    {
      return QString::fromLocal8Bit (raw) != QStringLiteral ("0");
    }
  return false;
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

QString env_string (char const* name, QString const& fallback)
{
  if (char const* raw = std::getenv (name))
    {
      return QString::fromLocal8Bit (raw);
    }
  return fallback;
}

[[noreturn]] void fail (QString const& message)
{
  throw std::runtime_error {message.toStdString ()};
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
          audioFormat = qFromLittleEndian<quint16> (fmt + 0);
          channels = qFromLittleEndian<quint16> (fmt + 2);
          sampleRate = qFromLittleEndian<quint32> (fmt + 4);
          bitsPerSample = qFromLittleEndian<quint16> (fmt + 14);
          haveFmt = true;
        }
      else if (chunkId == "data")
        {
          dataChunk = blob.mid (pos, chunkSize);
          haveData = true;
        }

      pos += chunkSize + (chunkSize & 1);
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

void build_request_spectra (QVector<short> const& audio, int nutc, int nfqso, int ntol, int nsubmode,
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

  request_out->serial = 1;
  request_out->mode = QStringLiteral ("JT9");
  request_out->audio = audio;
  request_out->ss.resize (184 * NSMAX);
  std::copy_n (shared->ss, request_out->ss.size (), request_out->ss.begin ());
  request_out->npts8 = npts8;
  request_out->nzhsym = ihsym;
  request_out->nutc = nutc;
  request_out->nfqso = nfqso;
  request_out->ntol = ntol;
  request_out->nsubmode = nsubmode;
}

bool compare_case (QString const& message, int nsubmode, int nutc, int nfqso, int ntol)
{
  QVector<short> audio;
  if (char const* wav_path = std::getenv ("JT9_WIDE_WAV"))
    {
      WavData const wav = read_wav_file (QString::fromLocal8Bit (wav_path));
      if (wav.sampleRate != RX_SAMPLE_RATE)
        {
          std::fprintf (stderr, "JT9 wide compare requires 12000 Hz WAV, got %d\n", wav.sampleRate);
          return false;
        }
      audio = wav.samples;
    }
  else
    {
      audio = synthesize_wide_audio (message, nsubmode, float (nfqso));
    }
  decodium::legacyjt::DecodeRequest request;
  build_request_spectra (audio, nutc, nfqso, ntol, nsubmode, &request);

  auto const estimate = decodium::jt9wide::detail::estimate_wide_jt9 (request);
  if (env_flag ("JT9_WIDE_DUMP"))
    {
      std::fprintf (stderr,
                    "fixture: message='%s' submode=%d nutc=%d nfqso=%d ntol=%d audio=%d nzhsym=%d npts8=%d\n",
                    message.toLatin1 ().constData (), nsubmode, nutc, nfqso, ntol, audio.size (),
                    request.nzhsym, request.npts8);
      std::fprintf (stderr, "  estimate: ok=%d sync=%.6f xdt0=%.6f f0=%.3f width=%.3f lag=%d\n",
                    estimate.ok ? 1 : 0, estimate.sync, estimate.xdt0, estimate.f0, estimate.width, estimate.lagpk);
      if (estimate.ok)
        {
          auto const soft =
              decodium::jt9wide::detail::compute_softsym (request.audio, estimate.xdt0, estimate.f0,
                                                          estimate.width, request.nsubmode);
          std::fprintf (stderr, "  softsym: ok=%d xdt1=%.6f nsnr=%d\n",
                        soft.ok ? 1 : 0, soft.xdt1, soft.nsnr);
          if (soft.ok)
            {
              int nlim = 0;
              QByteArray const decoded = decodium::jt9fast::decode_soft_symbols (soft.soft, 10000, &nlim);
              std::fprintf (stderr, "  fano: nlim=%d decoded='%s'\n",
                            nlim, decoded.constData ());
            }
        }
    }

  QString fortran_row;
  if (std::getenv ("JT9_WIDE_FORTRAN"))
    {
      float sync = 0.0f;
      int nsnr = 0;
      float xdt1 = 0.0f;
      float f0 = 0.0f;
      std::array<char, 22> decoded {};
      decode9w_ (&nfqso, &ntol, &nsubmode, request.ss.data (), request.audio.data (),
                 &sync, &nsnr, &xdt1, &f0, decoded.data (), static_cast<fortran_charlen_t> (decoded.size ()));
      fortran_row = QStringLiteral ("%1 %2 %3 %4")
          .arg (nsnr)
          .arg (xdt1, 0, 'f', 1)
          .arg (f0, 0, 'f', 1)
          .arg (QString::fromLatin1 (decoded.data (), decoded.size ()).trimmed ());
    }

  QStringList const rows = decodium::jt9wide::decode_wide_jt9 (request);
  if (env_flag ("JT9_WIDE_DUMP"))
    {
      std::fprintf (stderr, "  rows=%d\n", rows.size ());
      if (!fortran_row.isEmpty ())
        {
          std::fprintf (stderr, "  Fortran: %s\n", fortran_row.toLatin1 ().constData ());
        }
      for (QString const& row : rows)
        {
          std::fprintf (stderr, "  %s\n", row.toLatin1 ().constData ());
        }
      return true;
    }
  if (rows.isEmpty ())
    {
      auto const estimate = decodium::jt9wide::detail::estimate_wide_jt9 (request);
      std::fprintf (stderr, "JT9 wide decoder produced no rows for '%s'\n",
                    message.toLatin1 ().constData ());
      std::fprintf (stderr, "  C++ estimate: ok=%d sync=%.6f xdt0=%.6f f0=%.3f width=%.3f lag=%d\n",
                    estimate.ok ? 1 : 0, estimate.sync, estimate.xdt0, estimate.f0, estimate.width, estimate.lagpk);
      if (!fortran_row.isEmpty ())
        {
          std::fprintf (stderr, "  Fortran: %s\n", fortran_row.toLatin1 ().constData ());
        }
      return false;
    }

  if (!rows.front ().contains (message))
    {
      auto const estimate = decodium::jt9wide::detail::estimate_wide_jt9 (request);
      std::fprintf (stderr, "JT9 wide decoder row mismatch for '%s'\n",
                    message.toLatin1 ().constData ());
      std::fprintf (stderr, "  C++ estimate: ok=%d sync=%.6f xdt0=%.6f f0=%.3f width=%.3f lag=%d\n",
                    estimate.ok ? 1 : 0, estimate.sync, estimate.xdt0, estimate.f0, estimate.width, estimate.lagpk);
      if (!fortran_row.isEmpty ())
        {
          std::fprintf (stderr, "  Fortran: %s\n", fortran_row.toLatin1 ().constData ());
        }
      for (QString const& row : rows)
        {
          std::fprintf (stderr, "  %s\n", row.toLatin1 ().constData ());
        }
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

      bool ok = true;
      ok = compare_case (message, nsubmode, nutc, nfqso, ntol) && ok;

      if (!ok)
        {
          return 1;
        }

      std::printf ("JT9 wide decode compare passed\n");
      return 0;
    }
  catch (std::exception const& error)
    {
      std::fprintf (stderr, "jt9_wide_compare exception: %s\n", error.what ());
      return 1;
    }
}
