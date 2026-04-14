#include <QCoreApplication>
#include <QFile>
#include <QString>
#include <QVector>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

#include "Detector/LegacyDspIoHelpers.hpp"

extern "C"
{
  void jt65_init_tables_ ();
  void symspec65_ (float dd[], int* npts, int* nqsym, float savg[]);
  void decode65a_ (float dd[], int* npts, int* newdat, int* nqd, float* f0, int* nflip,
                   int* mode65, int* ntrials, int* naggressive, int* ndepth, int* ntol,
                   char const* mycall, char const* hiscall, char const* hisgrid, int* nQSOProgress,
                   int* ljt65apon, int* bVHF, float* sync2, float a[], float* dt, int* nft,
                   int* nspecial, float* qual, int* nhist, int* nsmo, char decoded[],
                   size_t mycall_len, size_t hiscall_len, size_t hisgrid_len, size_t decoded_len);
  extern struct {
    float thresh0;
  } steve_;
}

namespace
{

constexpr int kExpectedSamples = 52 * 12000;

[[noreturn]] void fail (QString const& message)
{
  throw std::runtime_error {message.toStdString ()};
}

QString env_string (char const* name, QString const& fallback)
{
  if (char const* raw = std::getenv (name))
    {
      return QString::fromLocal8Bit (raw);
    }
  return fallback;
}

QVector<short> read_wav_mono16 (QString const& fileName)
{
  QFile file {fileName};
  if (!file.open (QIODevice::ReadOnly))
    {
      fail (QStringLiteral ("cannot open WAV file \"%1\": %2").arg (fileName, file.errorString ()));
    }

  QByteArray const blob = file.readAll ();
  if (blob.size () < 12 || blob.mid (0, 4) != "RIFF" || blob.mid (8, 4) != "WAVE")
    {
      fail (QStringLiteral ("WAV file \"%1\" is not RIFF/WAVE").arg (fileName));
    }

  bool have_fmt = false;
  bool have_data = false;
  quint16 audioFormat = 0;
  quint16 channels = 0;
  quint32 sampleRate = 0;
  quint16 bitsPerSample = 0;
  QByteArray dataChunk;

  int pos = 12;
  while (pos + 8 <= blob.size ())
    {
      QByteArray const chunkId = blob.mid (pos, 4);
      quint32 const chunkSize = qFromLittleEndian<quint32> (
          reinterpret_cast<uchar const*> (blob.constData () + pos + 4));
      pos += 8;
      if (pos + static_cast<int> (chunkSize) > blob.size ())
        {
          fail (QStringLiteral ("WAV file \"%1\" has a truncated chunk").arg (fileName));
        }

      if (chunkId == "fmt ")
        {
          auto const* fmt = reinterpret_cast<uchar const*> (blob.constData () + pos);
          audioFormat = qFromLittleEndian<quint16> (fmt);
          channels = qFromLittleEndian<quint16> (fmt + 2);
          sampleRate = qFromLittleEndian<quint32> (fmt + 4);
          bitsPerSample = qFromLittleEndian<quint16> (fmt + 14);
          have_fmt = true;
        }
      else if (chunkId == "data")
        {
          dataChunk = blob.mid (pos, static_cast<int> (chunkSize));
          have_data = true;
        }

      pos += static_cast<int> ((chunkSize + 1u) & ~1u);
    }

  if (!have_fmt || !have_data || audioFormat != 1u || channels != 1u || sampleRate != 12000u
      || bitsPerSample != 16u)
    {
      fail (QStringLiteral ("WAV file \"%1\" must be PCM mono 16-bit 12000 Hz").arg (fileName));
    }

  QVector<short> samples (dataChunk.size () / 2);
  auto const* raw = reinterpret_cast<uchar const*> (dataChunk.constData ());
  for (int i = 0; i < samples.size (); ++i)
    {
      samples[i] = static_cast<short> (qFromLittleEndian<qint16> (raw + 2 * i));
    }
  return samples;
}

QByteArray fixed_field (QByteArray value, int width)
{
  value = value.left (width);
  if (value.size () < width)
    {
      value.append (QByteArray (width - value.size (), ' '));
    }
  return value;
}

bool nearly_equal (float lhs, float rhs, float abs_tol = 1.0e-5f, float rel_tol = 1.0e-5f)
{
  float const scale = std::max ({1.0f, std::fabs (lhs), std::fabs (rhs)});
  return std::fabs (lhs - rhs) <= abs_tol + rel_tol * scale;
}

}  // namespace

int main (int argc, char** argv)
{
  try
    {
      QCoreApplication app {argc, argv};
      QString const wavPath = env_string ("JT65_WAV", QStringLiteral ("build/JT65.wav"));
      QVector<short> audio = read_wav_mono16 (wavPath);
      if (audio.size () > kExpectedSamples)
        {
          audio.resize (kExpectedSamples);
        }

      std::vector<float> dd (static_cast<std::size_t> (audio.size ()), 0.0f);
      for (int i = 0; i < audio.size (); ++i)
        {
          dd[static_cast<std::size_t> (i)] = static_cast<float> (audio.at (i));
        }

      jt65_init_tables_ ();

      int npts = static_cast<int> (dd.size ());
      int nqsym = 0;
      std::vector<float> savg (3413, 0.0f);
      symspec65_ (dd.data (), &npts, &nqsym, savg.data ());
      steve_.thresh0 = 2.0f;
      auto const candidates = decodium::legacy::sync65_compute (200, 4000, 1000, nqsym, false,
                                                                false, 2.0f);
      if (candidates.empty ())
        {
          fail (QStringLiteral ("no JT65 candidate found in %1").arg (wavPath));
        }
      auto const best = *std::max_element (
          candidates.begin (), candidates.end (),
          [] (decodium::legacy::Jt65SyncCandidate const& lhs,
              decodium::legacy::Jt65SyncCandidate const& rhs) { return lhs.sync < rhs.sync; });

      int newdat = 1;
      int nqd = 0;
      float f0 = best.freq;
      int nflip = 1;
      int mode65 = 1;
      int ntrials = 1000;
      int naggressive = 0;
      int ndepth = 0;
      int ntol = 1000;
      QByteArray mycall (12, ' ');
      QByteArray hiscall (12, ' ');
      QByteArray hisgrid (6, ' ');
      int nQSOProgress = 0;
      int ljt65apon = 0;
      int bVHF = 0;
      float sync2_wrapped = 0.0f;
      std::array<float, 5> a_wrapped {};
      float dt_wrapped = best.dt;
      int nft_wrapped = 0;
      int nspecial_wrapped = 0;
      float qual_wrapped = 0.0f;
      int nhist_wrapped = 0;
      int nsmo_wrapped = 0;
      QByteArray decoded_wrapped (22, ' ');

      decode65a_ (dd.data (), &npts, &newdat, &nqd, &f0, &nflip, &mode65, &ntrials,
                  &naggressive, &ndepth, &ntol, mycall.constData (), hiscall.constData (),
                  hisgrid.constData (), &nQSOProgress, &ljt65apon, &bVHF, &sync2_wrapped,
                  a_wrapped.data (), &dt_wrapped, &nft_wrapped, &nspecial_wrapped, &qual_wrapped,
                  &nhist_wrapped, &nsmo_wrapped, decoded_wrapped.data (),
                  static_cast<size_t> (mycall.size ()), static_cast<size_t> (hiscall.size ()),
                  static_cast<size_t> (hisgrid.size ()), static_cast<size_t> (decoded_wrapped.size ()));

      auto const direct = decodium::legacy::decode65a_compute (
          dd.data (), npts, 1, 0, best.freq, 1, 1, 1000, 0, 0, 1000, mycall, hiscall, hisgrid, 0,
          false, false, best.dt);

      bool ok = nearly_equal (sync2_wrapped, direct.sync2)
                && nearly_equal (dt_wrapped, direct.dt)
                && nft_wrapped == direct.nft
                && nspecial_wrapped == direct.nspecial
                && nearly_equal (qual_wrapped, direct.qual)
                && nhist_wrapped == direct.nhist
                && nsmo_wrapped == direct.nsmo
                && decoded_wrapped == fixed_field (direct.decoded, 22);
      for (int i = 0; ok && i < 5; ++i)
        {
          ok = nearly_equal (a_wrapped[static_cast<std::size_t> (i)], direct.a[static_cast<std::size_t> (i)]);
        }

      if (!ok)
        {
          std::fprintf (stderr,
                        "JT65 decode65a compare failed for %s\n"
                        "  wrapped sync2=%.9g dt=%.9g nft=%d nspecial=%d qual=%.9g nhist=%d nsmo=%d decoded='%.*s'\n"
                        "  direct  sync2=%.9g dt=%.9g nft=%d nspecial=%d qual=%.9g nhist=%d nsmo=%d decoded='%.*s'\n",
                        wavPath.toLocal8Bit ().constData (),
                        sync2_wrapped, dt_wrapped, nft_wrapped, nspecial_wrapped, qual_wrapped,
                        nhist_wrapped, nsmo_wrapped, decoded_wrapped.size (), decoded_wrapped.constData (),
                        direct.sync2, direct.dt, direct.nft, direct.nspecial, direct.qual,
                        direct.nhist, direct.nsmo, direct.decoded.size (), direct.decoded.constData ());
          return 1;
        }

      std::printf ("JT65 decode65a compare passed for %s at %.2f Hz dt %.3f\n",
                   wavPath.toLocal8Bit ().constData (), best.freq, best.dt);
      return 0;
    }
  catch (std::exception const& e)
    {
      std::fprintf (stderr, "jt65_decode65a_compare failed: %s\n", e.what ());
      return 1;
    }
}
