#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <fftw3.h>

extern "C" void ftx_baseline_fit_c (float const* spectrum, int size, int ia, int ib,
                                    float offset_db, int linear_output, float* baseline);

namespace
{

constexpr float kSampleRate = 12000.0f;
constexpr float kScale = 1.0f / 300.0f;
constexpr float kPi = 3.14159265358979323846f;

struct Candidate
{
  float frequency;
  float strength;
};

struct ModeConfig
{
  int nmax;
  int nfft1;
  int nh1;
  int nhsym;
  int nstep;
  int nsps;
  float max_frequency_hz;
  float baseline_offset_db;
  bool sort_groups;
};

struct FftBuffers
{
  explicit FftBuffers (int nfft)
    : in (static_cast<float*> (fftwf_malloc (sizeof (float) * static_cast<size_t> (nfft)))),
      out (static_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * static_cast<size_t> (nfft / 2 + 1)))),
      plan (nullptr)
  {
    if (in && out)
      {
        plan = fftwf_plan_dft_r2c_1d (nfft, in, out, FFTW_ESTIMATE);
      }
  }

  ~FftBuffers ()
  {
    if (plan)
      {
        fftwf_destroy_plan (plan);
      }
    if (out)
      {
        fftwf_free (out);
      }
    if (in)
      {
        fftwf_free (in);
      }
  }

  bool valid () const
  {
    return in && out && plan;
  }

  float* in;
  fftwf_complex* out;
  fftwf_plan plan;
};

std::vector<float> make_nuttall_window (int size)
{
  std::vector<float> window (static_cast<size_t> (size), 0.0f);
  constexpr float a0 = 0.3635819f;
  constexpr float a1 = -0.4891775f;
  constexpr float a2 = 0.1365995f;
  constexpr float a3 = -0.0106411f;

  for (int i = 0; i < size; ++i)
    {
      float const x = 2.0f * kPi * static_cast<float> (i) / static_cast<float> (size);
      window[static_cast<size_t> (i)] = a0
        + a1 * std::cos (x)
        + a2 * std::cos (2.0f * x)
        + a3 * std::cos (3.0f * x);
    }

  return window;
}

void sort_group_desc (float* candidate, int begin_index, int end_index)
{
  if (end_index - begin_index <= 1)
    {
      return;
    }

  std::vector<Candidate> group;
  group.reserve (static_cast<size_t> (end_index - begin_index));
  for (int i = begin_index; i < end_index; ++i)
    {
      group.push_back ({candidate[i * 2], candidate[i * 2 + 1]});
    }

  std::sort (group.begin (), group.end (), [](Candidate const& left, Candidate const& right) {
    return left.strength > right.strength;
  });

  for (int i = begin_index; i < end_index; ++i)
    {
      Candidate const& entry = group[static_cast<size_t> (i - begin_index)];
      candidate[i * 2] = entry.frequency;
      candidate[i * 2 + 1] = entry.strength;
    }
}

void run_get_candidates (ModeConfig const& config, float const* dd,
                         float fa, float fb, float syncmin, float nfqso,
                         int maxcand, float* savg, float* candidate, int* ncand,
                         float* sbase)
{
  if (!dd || !savg || !candidate || !ncand || !sbase || maxcand <= 0)
    {
      return;
    }

  std::fill (savg, savg + config.nh1, 0.0f);
  std::fill (sbase, sbase + config.nh1, 0.0f);
  std::fill (candidate, candidate + 2 * maxcand, 0.0f);
  *ncand = 0;

  FftBuffers fft (config.nfft1);
  if (!fft.valid ())
    {
      return;
    }

  std::vector<float> const window = make_nuttall_window (config.nfft1);

  for (int j = 0; j < config.nhsym; ++j)
    {
      int const ia = j * config.nstep;
      int const ib = ia + config.nfft1;
      if (ib > config.nmax)
        {
          break;
        }

      for (int i = 0; i < config.nfft1; ++i)
        {
          fft.in[i] = kScale * dd[ia + i] * window[static_cast<size_t> (i)];
        }

      fftwf_execute (fft.plan);
      for (int i = 0; i < config.nh1; ++i)
        {
          float const re = fft.out[i + 1][0];
          float const im = fft.out[i + 1][1];
          savg[i] += re * re + im * im;
        }
    }

  for (int i = 0; i < config.nh1; ++i)
    {
      savg[i] /= static_cast<float> (config.nhsym);
    }

  std::vector<float> savsm (static_cast<size_t> (config.nh1), 0.0f);
  for (int i = 7; i <= config.nh1 - 8; ++i)
    {
      float sum = 0.0f;
      for (int j = i - 7; j <= i + 7; ++j)
        {
          sum += savg[j];
        }
      savsm[static_cast<size_t> (i)] = sum / 15.0f;
    }

  float const df = kSampleRate / static_cast<float> (config.nfft1);
  int const min_bin = static_cast<int> (std::lround (200.0f / df));
  int nfa = static_cast<int> (fa / df);
  int nfb = static_cast<int> (fb / df);
  nfa = std::max (nfa, min_bin);
  nfb = std::min (nfb, static_cast<int> (std::lround (config.max_frequency_hz / df)));

  ftx_baseline_fit_c (savg, config.nh1, nfa, nfb, config.baseline_offset_db, 1, sbase);

  for (int i = nfa - 1; i <= nfb - 1; ++i)
    {
      if (sbase[i] <= 0.0f)
        {
          return;
        }
      savsm[static_cast<size_t> (i)] /= sbase[i];
    }

  std::vector<Candidate> raw_candidates;
  raw_candidates.reserve (static_cast<size_t> (maxcand));

  float const f_offset = -1.5f * kSampleRate / static_cast<float> (config.nsps);
  for (int i = nfa + 1; i <= nfb - 1; ++i)
    {
      float const left = savsm[static_cast<size_t> (i - 2)];
      float const center = savsm[static_cast<size_t> (i - 1)];
      float const right = savsm[static_cast<size_t> (i)];
      if (center < left || center < right || center < syncmin)
        {
          continue;
        }

      float const den = left - 2.0f * center + right;
      float del = 0.0f;
      if (den != 0.0f)
        {
          del = 0.5f * (left - right) / den;
        }

      float const fpeak = (static_cast<float> (i) + del) * df + f_offset;
      if (fpeak < 200.0f || fpeak > config.max_frequency_hz)
        {
          continue;
        }

      float const speak = center - 0.25f * (left - right) * del;
      raw_candidates.push_back ({fpeak, speak});
      if (static_cast<int> (raw_candidates.size ()) == maxcand)
        {
          break;
        }
    }

  *ncand = static_cast<int> (raw_candidates.size ());
  if (*ncand == 0)
    {
      return;
    }

  int nq = 0;
  for (Candidate const& item : raw_candidates)
    {
      if (std::fabs (item.frequency - nfqso) <= 20.0f)
        {
          ++nq;
        }
    }

  int near_index = 0;
  int far_index = nq;
  for (Candidate const& item : raw_candidates)
    {
      int target = far_index;
      if (std::fabs (item.frequency - nfqso) <= 20.0f)
        {
          target = near_index;
          ++near_index;
        }
      else
        {
          ++far_index;
        }
      candidate[target * 2] = item.frequency;
      candidate[target * 2 + 1] = item.strength;
    }

  if (config.sort_groups)
    {
      sort_group_desc (candidate, 0, nq);
      sort_group_desc (candidate, nq, *ncand);
    }
}

}

extern "C" void ftx_getcandidates2_c (float const* dd, float fa, float fb, float syncmin,
                                      float nfqso, int maxcand, float* savg,
                                      float* candidate, int* ncand, float* sbase)
{
  static ModeConfig const config {
    45000, 1152, 576, 152, 288, 288, 5500.0f, 0.50f, true
  };
  run_get_candidates (config, dd, fa, fb, syncmin, nfqso, maxcand, savg, candidate, ncand, sbase);
}

extern "C" void ftx_getcandidates4_c (float const* dd, float fa, float fb, float syncmin,
                                      float nfqso, int maxcand, float* savg,
                                      float* candidate, int* ncand, float* sbase)
{
  static ModeConfig const config {
    72576, 2304, 1152, 122, 576, 576, 4910.0f, 0.65f, false
  };
  run_get_candidates (config, dd, fa, fb, syncmin, nfqso, maxcand, savg, candidate, ncand, sbase);
}
