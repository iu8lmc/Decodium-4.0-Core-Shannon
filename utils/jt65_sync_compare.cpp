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
#include <tuple>
#include <vector>

#include "Detector/LegacyDspIoHelpers.hpp"

extern "C"
{
  void symspec65_ (float dd[], int* npts, int* nqsym, float savg[]);
  void sync65_ (int* nfa, int* nfb, int* ntol, int* nqsym, void* ca, int* ncand, int* nrobust,
                int* bVHF);
  extern struct {
    float thresh0;
  } steve_;
}

namespace
{

constexpr int kExpectedSamples = 52 * 12000;
constexpr int kMaxCand = 300;

struct LegacyCandidate
{
  float freq;
  float dt;
  float sync;
  float flip;
};

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

bool nearly_equal (float lhs, float rhs, float abs_tol = 1.0e-4f, float rel_tol = 1.0e-5f)
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

      int npts = static_cast<int> (dd.size ());
      int nqsym = 0;
      std::vector<float> savg (3413, 0.0f);
      symspec65_ (dd.data (), &npts, &nqsym, savg.data ());

      std::array<std::tuple<int, int, int, int, int, float>, 4> const cases {{
          std::make_tuple (200, 4000, 1000, 0, 0, 2.0f),
          std::make_tuple (200, 4000, 1000, 1, 0, 2.0f),
          std::make_tuple (1200, 1800, 200, 0, 0, 1.0f),
          std::make_tuple (1200, 1800, 200, 0, 1, 1.0f),
      }};

      bool ok = true;
      for (auto const& entry : cases)
        {
          int const nfa = std::get<0> (entry);
          int const nfb = std::get<1> (entry);
          int const ntol = std::get<2> (entry);
          int const robust = std::get<3> (entry);
          int const vhf = std::get<4> (entry);
          float const thresh0 = std::get<5> (entry);
          steve_.thresh0 = thresh0;

          std::array<LegacyCandidate, kMaxCand> wrapped {};
          int ncand = 0;
          int nfa_copy = nfa;
          int nfb_copy = nfb;
          int ntol_copy = ntol;
          int nqsym_copy = nqsym;
          int robust_copy = robust;
          int vhf_copy = vhf;
          sync65_ (&nfa_copy, &nfb_copy, &ntol_copy, &nqsym_copy, wrapped.data (), &ncand,
                   &robust_copy, &vhf_copy);

          auto const direct = decodium::legacy::sync65_compute (nfa, nfb, ntol, nqsym,
                                                                robust != 0, vhf != 0, thresh0);
          if (ncand != static_cast<int> (direct.size ()))
            {
              std::fprintf (stderr,
                            "JT65 sync candidate count mismatch for case nfa=%d nfb=%d ntol=%d robust=%d vhf=%d thresh=%.2f: wrapped=%d direct=%d\n",
                            nfa, nfb, ntol, robust, vhf, thresh0, ncand, int (direct.size ()));
              ok = false;
              continue;
            }

          for (int i = 0; i < ncand; ++i)
            {
              auto const& lhs = wrapped[static_cast<std::size_t> (i)];
              auto const& rhs = direct[static_cast<std::size_t> (i)];
              if (!nearly_equal (lhs.freq, rhs.freq) || !nearly_equal (lhs.dt, rhs.dt)
                  || !nearly_equal (lhs.sync, rhs.sync) || !nearly_equal (lhs.flip, rhs.flip))
                {
                  std::fprintf (stderr,
                                "JT65 sync mismatch case nfa=%d nfb=%d ntol=%d robust=%d vhf=%d thresh=%.2f cand=%d\n"
                                "  wrapped freq=%.9g dt=%.9g sync=%.9g flip=%.9g\n"
                                "  direct  freq=%.9g dt=%.9g sync=%.9g flip=%.9g\n",
                                nfa, nfb, ntol, robust, vhf, thresh0, i,
                                lhs.freq, lhs.dt, lhs.sync, lhs.flip,
                                rhs.freq, rhs.dt, rhs.sync, rhs.flip);
                  ok = false;
                  break;
                }
            }
        }

      if (!ok)
        {
          return 1;
        }

      std::printf ("JT65 sync compare passed for %s with nqsym=%d\n",
                   wavPath.toLocal8Bit ().constData (), nqsym);
      return 0;
    }
  catch (std::exception const& e)
    {
      std::fprintf (stderr, "jt65_sync_compare failed: %s\n", e.what ());
      return 1;
    }
}
