#include "Detector/JT9FastDecoder.hpp"

#include "Detector/JT9FastDecodeWorker.hpp"
#include "Detector/JT9FastHelpers.hpp"
#include "Modulator/LegacyJtEncoder.hpp"
#include "widgets/LegacyUiHelpers.hpp"
#include "wsjtx_config.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

extern "C"
{
  int fano (unsigned int* metric, unsigned int* cycles, unsigned int* maxnp,
            unsigned char* data, unsigned char* symbols, unsigned int nbits,
            int mettab[2][256], int delta, unsigned int maxcycles);
}

namespace
{

constexpr int kFanoMaxBits = 103;
constexpr int kFanoMaxBytes = (kFanoMaxBits + 7) / 8;

using SignedMetricTable = std::array<std::array<int, 2>, 256>;

SignedMetricTable const& jt9_metric_table_signed ()
{
  static SignedMetricTable const table = [] {
    std::array<float, 263> const xx0 {{
      1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,
      1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,
      1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,
      1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,
      1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,
      1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,
      0.988f,1.000f,0.991f,0.993f,1.000f,0.995f,1.000f,0.991f,
      1.000f,0.991f,0.992f,0.991f,0.990f,0.990f,0.992f,0.996f,
      0.990f,0.994f,0.993f,0.991f,0.992f,0.989f,0.991f,0.987f,
      0.985f,0.989f,0.984f,0.983f,0.979f,0.977f,0.971f,0.975f,
      0.974f,0.970f,0.970f,0.970f,0.967f,0.962f,0.960f,0.957f,
      0.956f,0.953f,0.942f,0.946f,0.937f,0.933f,0.929f,0.920f,
      0.917f,0.911f,0.903f,0.895f,0.884f,0.877f,0.869f,0.858f,
      0.846f,0.834f,0.821f,0.806f,0.790f,0.775f,0.755f,0.737f,
      0.713f,0.691f,0.667f,0.640f,0.612f,0.581f,0.548f,0.510f,
      0.472f,0.425f,0.378f,0.328f,0.274f,0.212f,0.146f,0.075f,
      0.000f,-0.079f,-0.163f,-0.249f,-0.338f,-0.425f,-0.514f,-0.606f,
      -0.706f,-0.796f,-0.895f,-0.987f,-1.084f,-1.181f,-1.280f,-1.376f,
      -1.473f,-1.587f,-1.678f,-1.790f,-1.882f,-1.992f,-2.096f,-2.201f,
      -2.301f,-2.411f,-2.531f,-2.608f,-2.690f,-2.829f,-2.939f,-3.058f,
      -3.164f,-3.212f,-3.377f,-3.463f,-3.550f,-3.768f,-3.677f,-3.975f,
      -4.062f,-4.098f,-4.186f,-4.261f,-4.472f,-4.621f,-4.623f,-4.608f,
      -4.822f,-4.870f,-4.652f,-4.954f,-5.108f,-5.377f,-5.544f,-5.995f,
      -5.632f,-5.826f,-6.304f,-6.002f,-6.559f,-6.369f,-6.658f,-7.016f,
      -6.184f,-7.332f,-6.534f,-6.152f,-6.113f,-6.288f,-6.426f,-6.313f,
      -9.966f,-6.371f,-9.966f,-7.055f,-9.966f,-6.629f,-6.313f,-9.966f,
      -5.858f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,
      -9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,
      -9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,
      -9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,
      -9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,
      -9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,
      1.43370769e-019f,2.64031087e-006f,6.25548654e+028f,
      2.44565251e+020f,4.74227538e+030f,10497312.0f,7.74079654e-039f
    }};

    SignedMetricTable result {};
    constexpr float bias = 0.5f;
    constexpr int scale = 50;
    constexpr int ib = 160;
    constexpr int slope = 2;

    for (int i = 0; i <= 255; ++i)
      {
        int value = static_cast<int> (std::lround (scale * (xx0[static_cast<std::size_t> (i)] - bias)));
        if (i > ib)
          {
            int const break_value =
                static_cast<int> (std::lround (scale * (xx0[static_cast<std::size_t> (ib)] - bias)));
            value = break_value - slope * (i - ib);
          }
        result[static_cast<std::size_t> (i)][0] = value;
        if (i >= 1)
          {
            result[static_cast<std::size_t> (256 - i)][1] = value;
          }
      }
    result[0][1] = result[1][1];
    return result;
  } ();

  return table;
}

int jt9_ndelta ()
{
  return static_cast<int> (std::lround (3.4 * 50.0));
}

QByteArray decode_jt9fano_impl (std::array<std::int8_t, 207> const& soft, int limit, int* nlim)
{
  QByteArray blank (22, ' ');
  int constexpr encoded_bits = 72 + 31;
  unsigned int const nbits = encoded_bits;
  int mettab_c[2][256] {};
  SignedMetricTable const& mettab_signed = jt9_metric_table_signed ();
  for (int u = 0; u < 256; ++u)
    {
      auto const signed_symbol =
          static_cast<int> (static_cast<std::int8_t> (static_cast<unsigned char> (u)));
      int const idx = signed_symbol + 128;
      mettab_c[0][u] = mettab_signed[static_cast<std::size_t> (idx)][0];
      mettab_c[1][u] = mettab_signed[static_cast<std::size_t> (idx)][1];
    }

  std::array<unsigned char, kFanoMaxBytes> decoded {};
  std::array<unsigned char, 207> symbols {};
  for (int i = 0; i < 207; ++i)
    {
      symbols[static_cast<std::size_t> (i)] = static_cast<unsigned char> (soft[static_cast<std::size_t> (i)]);
    }
  int ndelta = jt9_ndelta ();
  unsigned int metric = 0;
  unsigned int ncycles = 0;
  unsigned int maxnp = 0;
  int const ierr = fano (&metric, &ncycles, &maxnp, decoded.data (), symbols.data (),
                         nbits, mettab_c, ndelta, static_cast<unsigned int> (limit));
  if (nlim)
    {
      *nlim = static_cast<int> (ncycles / encoded_bits);
    }
  if (ncycles >= static_cast<unsigned int> (encoded_bits * limit) || ierr != 0)
    {
      return blank;
    }

  std::array<int, 12> words {};
  int bit_index = 0;
  for (int i = 0; i < 12; ++i)
    {
      int value = 0;
      for (int j = 0; j < 6; ++j)
        {
          int const byte_index = bit_index / 8;
          int const bit_in_byte = 7 - (bit_index % 8);
          unsigned char const byte =
              static_cast<unsigned char> (decoded[static_cast<std::size_t> (byte_index)]);
          value = (value << 1) | ((byte >> bit_in_byte) & 1u);
          ++bit_index;
        }
      words[static_cast<std::size_t> (i)] = value;
    }

  QByteArray msg = decodium::legacy_jt::detail::unpackmsg (words);
  if (msg.contains ("000AAA "))
    {
      return blank;
    }
  return msg.leftJustified (22, ' ', true).left (22);
}

struct Candidate
{
  float ccf {0.0f};
  float t0 {0.0f};
  float t1 {0.0f};
  float freq {0.0f};
  std::array<std::int8_t, 207> soft {};
};

float percentile_value (float const* values, int count, int pct)
{
  if (!values || count <= 0 || pct < 0 || pct > 100)
    {
      return 1.0f;
    }

  std::vector<float> sorted (values, values + count);
  std::sort (sorted.begin (), sorted.end ());
  int index = static_cast<int> (std::lround (static_cast<double> (count) * 0.01 * pct));
  index = std::max (1, std::min (count, index));
  return sorted[static_cast<std::size_t> (index - 1)];
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

QString format_fast9_row (int nutc, int nsnr, float t0, float freq, QByteArray const& msg)
{
  QByteArray const fixed = msg.leftJustified (22, ' ', true).left (22);
  char prefix[64] {};
  std::snprintf (prefix, sizeof prefix, "%06d%4d%5.1f%5d @  ",
                 nutc, nsnr, double (t0), int (std::lround (freq)));
  QByteArray row {prefix};
  row.append (fixed);
  return QString::fromLatin1 (trim_trailing_spaces (row));
}

}

namespace decodium
{
namespace jt9fast
{

QByteArray decode_soft_symbols (std::array<std::int8_t, 207> const& soft, int limit, int* nlim)
{
  return decode_jt9fano_impl (soft, limit, nlim);
}

QStringList decode_fast9 (DecodeRequest const& request, FastDecodeState* state)
{
  QStringList rows;
  if (!state)
    {
      return rows;
    }

  int const nsubmode = request.nsubmode;
  if (nsubmode < 4)
    {
      return rows;
    }

  int const npts = std::min (request.kdone, int (request.audio.size ()));
  if (npts <= 0)
    {
      return rows;
    }

  int const nsps = 60 << (7 - nsubmode);
  int const nfft = 2 * nsps;
  int const nq = nfft / 4;
  int const istep = nsps / 4;
  int const jz = npts / istep;
  float const df = 12000.0f / nfft;
  float const tmid = npts * 0.5f / 12000.0f;
  int const nfa = std::max (200, request.rxfreq - request.ftol);
  int const nfb = std::min (request.rxfreq + request.ftol, 2500);
  int const maxlines = std::max (1, std::min (request.maxlines, 50));

  if (jz <= 0)
    {
      return rows;
    }

  if (request.newdat || nsubmode != state->nsubmode0)
    {
      int jz_cpp = 0;
      int nq_cpp = 0;
      spec9f_compute (request.audio.constData (), npts, nsps, state->s1, &jz_cpp, &nq_cpp);
      if (jz_cpp != jz || nq_cpp != nq)
        {
          return rows;
        }
    }

  state->nsubmode0 = nsubmode;

  std::vector<Candidate> candidates;
  candidates.reserve (500);
  std::array<float, 240 * 340> s2 {};
  int nlen0 = 0;

  for (int ilength = 1; ilength <= 14; ++ilength)
    {
      int nlen = static_cast<int> (std::pow (1.4142136, ilength - 1));
      int const max_fold = jz / 340;
      if (nlen > max_fold)
        {
          nlen = max_fold;
        }
      if (nlen == nlen0 || nlen <= 0)
        {
          continue;
        }
      nlen0 = nlen;

      int const jlen = nlen * 340;
      int jstep = jlen / 4;
      if (nsubmode >= 6)
        {
          jstep = jlen / 2;
        }
      if (jstep <= 0 || jz - jlen < 1)
        {
          continue;
        }

      for (int ja = 1; ja <= jz - jlen; ja += jstep)
        {
          int const jb = ja + jlen - 1;
          foldspec9f_compute (state->s1.data (), nq, jz, ja, jb, s2);

          auto const sync = sync9f_compute (s2, nq, nfa, nfb);
          if (sync.ccfbest < 30.0f)
            {
              continue;
            }

          Candidate candidate;
          candidate.ccf = sync.ccfbest;
          candidate.t0 = (ja - 1) * istep / 12000.0f;
          candidate.t1 = (jb - 1) * istep / 12000.0f;
          candidate.freq = sync.ipk * df;
          softsym9f_compute (sync.ss2, sync.ss3, candidate.soft);

          if (candidates.size () < 500)
            {
              candidates.push_back (candidate);
            }
        }
    }

  std::stable_sort (candidates.begin (), candidates.end (),
                    [] (Candidate const& lhs, Candidate const& rhs) {
                      return lhs.ccf > rhs.ccf;
                    });

  std::array<float, 1500> power_window {};

  for (int iter = 0; iter < 2; ++iter)
    {
      int const limit_count = std::min (int (candidates.size ()), 50);
      for (int isave = 0; isave < limit_count; ++isave)
        {
          Candidate const& candidate = candidates[static_cast<std::size_t> (isave)];
          if (iter == 0 && candidate.t1 < tmid)
            {
              continue;
            }
          if (iter == 1 && candidate.t1 >= tmid)
            {
              continue;
            }

          int limit = 2000;
          int nlim = 0;
          QByteArray const message = trim_trailing_spaces (decode_soft_symbols (candidate.soft, limit, &nlim));
          if (message.isEmpty ())
            {
              continue;
            }

          bool duplicate = false;
          for (QString const& existing : rows)
            {
              if (existing.contains (QString::fromLatin1 (message)))
                {
                  duplicate = true;
                  break;
                }
            }
          if (duplicate)
            {
              continue;
            }

          int i = static_cast<int> (candidate.t0 * 12000.0f);
          int const kz = static_cast<int> ((candidate.t1 - candidate.t0) / 0.02f);
          float smax = 0.0f;
          for (int k = 0; k < kz; ++k)
            {
              float sq = 0.0f;
              for (int n = 0; n < 240; ++n)
                {
                  ++i;
                  if (i < 0 || i >= npts)
                    {
                      break;
                    }
                  float const x = request.audio.at (i);
                  sq += x * x;
                }
              power_window[static_cast<std::size_t> (k)] = sq / 240.0f;
              smax = std::max (smax, power_window[static_cast<std::size_t> (k)]);
            }

          float const base = percentile_value (power_window.data (), kz, 35);
          float const snr = (base > 0.0f) ? (smax / (1.1f * base) - 1.0f) : -1.0f;
          int nsnr = -20;
          if (snr > 0.0f)
            {
              nsnr = static_cast<int> (std::lround (10.0 * std::log10 (snr)));
            }

          rows << format_fast9_row (request.nutc, nsnr, candidate.t0, candidate.freq, message);
          state->ntot += 1;
          if (rows.size () >= maxlines)
            {
              return rows;
            }
        }
    }

  return rows;
}

}
}
