#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <memory>
#include <utility>
#include <vector>

#include <fftw3.h>

extern "C" void ftx_baseline_fit_c (float const* spectrum, int size, int ia, int ib,
                                    float offset_db, int linear_output, float* baseline);
extern "C" void ftx_sync8var_scan_c (float const* s, int nh1, int nhsym, int iaw, int ibw,
                                     int jzb, int jzt, int ipass, int lagcc, int lagccbail,
                                     float* sync2d, signed char* syncq);

namespace
{

constexpr float kSampleRate = 12000.0f;
constexpr float kPi = 3.14159265358979323846f;
constexpr int kNFFT1 = 3840;
constexpr int kNH1 = 1920;
constexpr int kNSPS = 1920;
constexpr int kNSTEP = 480;
constexpr int kNHSYM = 372;
constexpr int kJZ = 80;
constexpr int kMaxPreCand = 1000;
constexpr int kFt8VarEdgeWindow = 201;

struct RealFftBuffers
{
  RealFftBuffers ()
    : in (static_cast<float*> (fftwf_malloc (sizeof (float) * kNFFT1))),
      out (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * (kNH1 + 1)))),
      plan (nullptr)
  {
    if (in && out)
      {
        plan = fftwf_plan_dft_r2c_1d (kNFFT1, in, out, FFTW_ESTIMATE);
      }
  }

  ~RealFftBuffers ()
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

RealFftBuffers& fft_buffers ()
{
  thread_local RealFftBuffers instance;
  return instance;
}

std::vector<float> make_ft8_window ()
{
  std::vector<float> window (kNFFT1, 0.0f);
  constexpr float a0 = 0.3635819f;
  constexpr float a1 = -0.4891775f;
  constexpr float a2 = 0.1365995f;
  constexpr float a3 = -0.0106411f;

  float sum = 0.0f;
  for (int i = 0; i < kNFFT1; ++i)
    {
      float const x = 2.0f * kPi * static_cast<float> (i) / static_cast<float> (kNFFT1);
      float const value = a0
        + a1 * std::cos (x)
        + a2 * std::cos (2.0f * x)
        + a3 * std::cos (3.0f * x);
      window[static_cast<size_t> (i)] = value;
      sum += value;
    }

  float const scale = static_cast<float> (kNSPS * 2) / (300.0f * sum);
  for (float& item : window)
    {
      item *= scale;
    }
  return window;
}

std::array<float, kFt8VarEdgeWindow> make_ft8_sync8var_edge_window ()
{
  std::array<float, kFt8VarEdgeWindow> window {};
  constexpr float facx = 1.0f / 300.0f;
  for (int i = 0; i < kFt8VarEdgeWindow; ++i)
    {
      window[static_cast<size_t> (i)] =
          facx * (1.0f + std::cos (static_cast<float> (i) * kPi / 200.0f)) * 0.5f;
    }
  return window;
}

template <typename T>
T clamp_value (T value, T lo, T hi)
{
  return std::max (lo, std::min (hi, value));
}

struct Ft8Candidate
{
  float frequency;
  float dt;
  float sync;
};

struct Ft8VarCandidate
{
  float frequency = 0.0f;
  float dt = 0.0f;
  float sync = 0.0f;
  float cq = 0.0f;
  float score = 0.0f;
};

inline float s_at (std::vector<float> const& s, int i, int j)
{
  return s[static_cast<size_t> (i + kNH1 * j)];
}

void fill_ft8_stage4_sbase (float const* dd, int npts, float nfa, float nfb, float* sbase)
{
  if (!dd || !sbase || npts < kNSPS)
    {
      return;
    }

  auto& fft = fft_buffers ();
  if (!fft.valid ())
    {
      return;
    }

  std::vector<float> const window = make_ft8_window ();
  float const df = kSampleRate / static_cast<float> (kNFFT1);
  int ia = std::max (1, static_cast<int> (std::lround (nfa / df)));
  int ib = static_cast<int> (std::lround (nfb / df));
  int const nwin = static_cast<int> (nfb - nfa);
  if (nfa < 100.0f)
    {
      nfa = 100.0f;
      ia = std::max (1, static_cast<int> (std::lround (nfa / df)));
      if (nwin < 100)
        {
          nfb = nfa + static_cast<float> (nwin);
          ib = static_cast<int> (std::lround (nfb / df));
        }
    }
  if (nfb > 4910.0f)
    {
      nfb = 4910.0f;
      ib = static_cast<int> (std::lround (nfb / df));
      if (nwin < 100)
        {
          nfa = nfb - static_cast<float> (nwin);
          ia = std::max (1, static_cast<int> (std::lround (nfa / df)));
        }
    }
  ia = clamp_value (ia, 1, kNH1);
  ib = clamp_value (ib, 1, kNH1);
  if (ib < ia)
    {
      return;
    }

  constexpr int kNST = kNFFT1 / 2;
  constexpr int kNF = 93;
  std::vector<float> savg_base (kNH1, 0.0f);
  for (int j = 0; j < kNF; ++j)
    {
      int const ofs = j * kNST;
      if (ofs + kNFFT1 > npts)
        {
          break;
        }
      for (int i = 0; i < kNFFT1; ++i)
        {
          fft.in[i] = dd[ofs + i] * window[static_cast<size_t> (i)];
        }
      fftwf_execute (fft.plan);
      for (int i = 0; i < kNH1; ++i)
        {
          float const re = fft.out[i + 1][0];
          float const im = fft.out[i + 1][1];
          savg_base[static_cast<size_t> (i)] += re * re + im * im;
        }
    }
  for (float& item : savg_base)
    {
      item /= static_cast<float> (kNF);
    }

  ftx_baseline_fit_c (savg_base.data (), kNH1, ia, ib, 0.65f, 0, sbase);
}

}

extern "C" void ftx_sync8_search_c (float const* dd, int npts, float nfa, float nfb,
                                    float syncmin, float nfqso, int maxcand,
                                    float* candidate, int* ncand, float* sbase)
{
  if (!dd || !candidate || !ncand || !sbase || maxcand <= 0 || npts < kNSPS)
    {
      return;
    }

  std::fill (candidate, candidate + 3 * maxcand, 0.0f);
  std::fill (sbase, sbase + kNH1, 0.0f);
  *ncand = 0;

  auto& fft = fft_buffers ();
  if (!fft.valid ())
    {
      return;
    }

  std::vector<float> const window = make_ft8_window ();
  std::vector<float> s (static_cast<size_t> (kNH1) * kNHSYM, 0.0f);

  for (int j = 0; j < kNHSYM; ++j)
    {
      int const ia = j * kNSTEP;
      if (ia + kNSPS > npts)
        {
          break;
        }

      std::fill (fft.in, fft.in + kNFFT1, 0.0f);
      for (int i = 0; i < kNSPS; ++i)
        {
          fft.in[i] = dd[ia + i] * window[static_cast<size_t> (i)];
        }
      fftwf_execute (fft.plan);

      for (int i = 0; i < kNH1; ++i)
        {
          float const re = fft.out[i + 1][0];
          float const im = fft.out[i + 1][1];
          float const value = re * re + im * im;
          s[static_cast<size_t> (i + kNH1 * j)] = value;
        }
    }

  float const df = kSampleRate / static_cast<float> (kNFFT1);
  int ia = std::max (1, static_cast<int> (std::lround (nfa / df)));
  int ib = static_cast<int> (std::lround (nfb / df));
  int const nwin = static_cast<int> (nfb - nfa);
  if (nfa < 100.0f)
    {
      nfa = 100.0f;
      ia = std::max (1, static_cast<int> (std::lround (nfa / df)));
      if (nwin < 100)
        {
          nfb = nfa + static_cast<float> (nwin);
          ib = static_cast<int> (std::lround (nfb / df));
        }
    }
  if (nfb > 4910.0f)
    {
      nfb = 4910.0f;
      ib = static_cast<int> (std::lround (nfb / df));
      if (nwin < 100)
        {
          nfa = nfb - static_cast<float> (nwin);
          ia = std::max (1, static_cast<int> (std::lround (nfa / df)));
        }
    }
  ia = clamp_value (ia, 1, kNH1);
  ib = clamp_value (ib, 1, kNH1);
  if (ib < ia)
    {
      return;
    }

  constexpr int kNST = kNFFT1 / 2;
  constexpr int kNF = 93;
  std::vector<float> savg_base (kNH1, 0.0f);
  for (int j = 0; j < kNF; ++j)
    {
      int const ofs = j * kNST;
      if (ofs + kNFFT1 > npts)
        {
          break;
        }
      for (int i = 0; i < kNFFT1; ++i)
        {
          fft.in[i] = dd[ofs + i] * window[static_cast<size_t> (i)];
        }
      fftwf_execute (fft.plan);
      for (int i = 0; i < kNH1; ++i)
        {
          float const re = fft.out[i + 1][0];
          float const im = fft.out[i + 1][1];
          savg_base[static_cast<size_t> (i)] += re * re + im * im;
        }
    }
  for (float& item : savg_base)
    {
      item /= static_cast<float> (kNF);
    }

  ftx_baseline_fit_c (savg_base.data (), kNH1, ia, ib, 0.65f, 0, sbase);

  constexpr std::array<int, 7> icos7 {{3, 1, 4, 0, 6, 5, 2}};
  constexpr int nssy = kNSPS / kNSTEP;
  constexpr int nfos = kNFFT1 / kNSPS;
  constexpr int jstrt = 12;
  float const tstep = static_cast<float> (kNSTEP) / kSampleRate;

  std::vector<float> sync2d (static_cast<size_t> (kNH1) * (2 * kJZ + 1), 0.0f);
  auto sync2d_at = [&sync2d](int i, int j) -> float& {
    return sync2d[static_cast<size_t> (i + kNH1 * (j + kJZ))];
  };

  for (int i = ia - 1; i <= ib - 1; ++i)
    {
      for (int j = -kJZ; j <= kJZ; ++j)
        {
          float ta = 0.0f, tb = 0.0f, tc = 0.0f;
          float t0a = 0.0f, t0b = 0.0f, t0c = 0.0f;
          for (int n = 0; n < 7; ++n)
            {
              int const m = j + jstrt + nssy * n;
              if (m >= 1 && m <= kNHSYM)
                {
                  ta += s_at (s, i + nfos * icos7[static_cast<size_t> (n)], m - 1);
                  for (int bin = i; bin <= i + nfos * 6; bin += nfos)
                    {
                      t0a += s_at (s, bin, m - 1);
                    }
                }

              int const mb = m + nssy * 36;
              if (mb >= 1 && mb <= kNHSYM)
                {
                  tb += s_at (s, i + nfos * icos7[static_cast<size_t> (n)], mb - 1);
                  for (int bin = i; bin <= i + nfos * 6; bin += nfos)
                    {
                      t0b += s_at (s, bin, mb - 1);
                    }
                }

              int const mc = m + nssy * 72;
              if (mc >= 1 && mc <= kNHSYM)
                {
                  tc += s_at (s, i + nfos * icos7[static_cast<size_t> (n)], mc - 1);
                  for (int bin = i; bin <= i + nfos * 6; bin += nfos)
                    {
                      t0c += s_at (s, bin, mc - 1);
                    }
                }
            }

          float const t = ta + tb + tc;
          float t0 = (t0a + t0b + t0c - t) / 6.0f;
          float sync_abc = 0.0f;
          if (t0 > 0.0f) sync_abc = t / t0;

          float sync_bc = 0.0f;
          t0 = (t0b + t0c - (tb + tc)) / 6.0f;
          if (t0 > 0.0f) sync_bc = (tb + tc) / t0;

          float sync_ab = 0.0f;
          t0 = (t0a + t0b - (ta + tb)) / 6.0f;
          if (t0 > 0.0f) sync_ab = (ta + tb) / t0;

          float sync_ac = 0.0f;
          t0 = (t0a + t0c - (ta + tc)) / 6.0f;
          if (t0 > 0.0f) sync_ac = (ta + tc) / t0;

          sync2d_at (i, j) = std::max (std::max (sync_abc, sync_bc), std::max (sync_ab, sync_ac));
        }
    }

  std::vector<float> red (kNH1, 0.0f);
  std::vector<float> red2 (kNH1, 0.0f);
  std::vector<int> jpeak (kNH1, 0);
  std::vector<int> jpeak2 (kNH1, 0);
  constexpr int mlag = 13;
  constexpr int mlag2 = kJZ;
  for (int i = ia - 1; i <= ib - 1; ++i)
    {
      float best = -1.0e30f;
      int bestj = 0;
      for (int j = -mlag; j <= mlag; ++j)
        {
          float const value = sync2d_at (i, j);
          if (value > best)
            {
              best = value;
              bestj = j;
            }
        }
      jpeak[static_cast<size_t> (i)] = bestj;
      red[static_cast<size_t> (i)] = best;

      best = -1.0e30f;
      bestj = 0;
      for (int j = -mlag2; j <= mlag2; ++j)
        {
          float const value = sync2d_at (i, j);
          if (value > best)
            {
              best = value;
              bestj = j;
            }
        }
      jpeak2[static_cast<size_t> (i)] = bestj;
      red2[static_cast<size_t> (i)] = best;
    }

  int const iz = ib - ia + 1;
  int const npctile = static_cast<int> (std::lround (0.40f * static_cast<float> (iz)));
  if (npctile < 1)
    {
      return;
    }

  std::vector<float> red_slice (red.begin () + (ia - 1), red.begin () + ib);
  std::nth_element (red_slice.begin (), red_slice.begin () + (npctile - 1), red_slice.end ());
  float const base = red_slice[static_cast<size_t> (npctile - 1)];
  if (!(base > 0.0f))
    {
      return;
    }
  for (float& item : red)
    {
      item /= base;
    }

  std::vector<float> red2_slice (red2.begin () + (ia - 1), red2.begin () + ib);
  std::nth_element (red2_slice.begin (), red2_slice.begin () + (npctile - 1), red2_slice.end ());
  float const base2 = red2_slice[static_cast<size_t> (npctile - 1)];
  if (!(base2 > 0.0f))
    {
      return;
    }
  for (float& item : red2)
    {
      item /= base2;
    }

  std::vector<int> order;
  order.reserve (static_cast<size_t> (iz));
  for (int i = ia - 1; i <= ib - 1; ++i)
    {
      order.push_back (i);
    }
  std::sort (order.begin (), order.end (), [&red](int left, int right) {
    return red[static_cast<size_t> (left)] > red[static_cast<size_t> (right)];
  });

  std::vector<Ft8Candidate> precandidates;
  precandidates.reserve (kMaxPreCand);
  for (int idx : order)
    {
      if (static_cast<int> (precandidates.size ()) >= kMaxPreCand)
        {
          break;
        }
      if (red[static_cast<size_t> (idx)] >= syncmin && !std::isnan (red[static_cast<size_t> (idx)]))
        {
          precandidates.push_back ({
            (static_cast<float> (idx + 1)) * df,
            (static_cast<float> (jpeak[static_cast<size_t> (idx)]) - 0.5f) * tstep,
            red[static_cast<size_t> (idx)]
          });
        }
      if (static_cast<int> (precandidates.size ()) >= kMaxPreCand)
        {
          break;
        }
      if (std::abs (jpeak2[static_cast<size_t> (idx)] - jpeak[static_cast<size_t> (idx)]) == 0)
        {
          continue;
        }
      if (red2[static_cast<size_t> (idx)] >= syncmin && !std::isnan (red2[static_cast<size_t> (idx)]))
        {
          precandidates.push_back ({
            (static_cast<float> (idx + 1)) * df,
            (static_cast<float> (jpeak2[static_cast<size_t> (idx)]) - 0.5f) * tstep,
            red2[static_cast<size_t> (idx)]
          });
        }
    }

  for (size_t i = 0; i < precandidates.size (); ++i)
    {
      if (precandidates[i].sync <= 0.0f)
        {
          continue;
        }
      for (size_t j = 0; j < i; ++j)
        {
          if (precandidates[j].sync <= 0.0f)
            {
              continue;
            }
          float const fdiff = std::fabs (precandidates[i].frequency) - std::fabs (precandidates[j].frequency);
          float const tdiff = std::fabs (precandidates[i].dt - precandidates[j].dt);
          if (std::fabs (fdiff) < 4.0f && tdiff < 0.04f)
            {
              if (precandidates[i].sync >= precandidates[j].sync) precandidates[j].sync = 0.0f;
              else precandidates[i].sync = 0.0f;
            }
        }
    }

  int k = 0;
  for (Ft8Candidate& item : precandidates)
    {
      if (std::fabs (item.frequency - nfqso) <= 10.0f && item.sync >= syncmin)
        {
          candidate[k * 3 + 0] = item.frequency;
          candidate[k * 3 + 1] = item.dt;
          candidate[k * 3 + 2] = item.sync;
          item.sync = 0.0f;
          ++k;
          if (k >= maxcand)
            {
              break;
            }
        }
    }

  if (k < maxcand)
    {
      std::vector<Ft8Candidate> remaining;
      remaining.reserve (precandidates.size ());
      for (Ft8Candidate const& item : precandidates)
        {
          if (item.sync >= syncmin)
            {
              remaining.push_back (item);
            }
        }
      std::sort (remaining.begin (), remaining.end (), [] (Ft8Candidate const& left, Ft8Candidate const& right) {
        return left.sync > right.sync;
      });

      for (Ft8Candidate const& item : remaining)
        {
          if (k >= maxcand)
            {
              break;
            }
          candidate[k * 3 + 0] = std::fabs (item.frequency);
          candidate[k * 3 + 1] = item.dt;
          candidate[k * 3 + 2] = item.sync;
          ++k;
        }
    }

  *ncand = k;
}

extern "C" void ftx_sync8var_c (float const* dd8, float const* windowx, float facx,
                                int nfa, int nfb, float syncmin, int nfqso,
                                float* candidate, int* ncand,
                                int jzb, int jzt, int ipass, int lqsothread,
                                int ncandthin, int ndtcenter,
                                int lagcc, int lagccbail, int nfawide, int nfbwide)
{
  if (!dd8 || !windowx || !candidate || !ncand)
    {
      return;
    }

  *ncand = 0;
  std::fill (candidate, candidate + 4 * 460, 0.0f);

  auto& fft = fft_buffers ();
  if (!fft.valid ())
    {
      return;
    }

  std::vector<float> s (static_cast<size_t> (kNH1) * kNHSYM, 0.0f);
  std::vector<float> sync2d (static_cast<size_t> (kNH1) * (jzt - jzb + 1), 0.0f);
  std::vector<signed char> syncq_raw (static_cast<size_t> (kNH1) * (jzt - jzb + 1), 0);
  std::vector<float> red (static_cast<size_t> (kNH1 + 1), 0.0f);
  std::vector<int> jpeak (static_cast<size_t> (kNH1 + 1), 0);
  std::vector<unsigned char> redcq (static_cast<size_t> (kNH1 + 1), 0);
  std::vector<Ft8VarCandidate> candidate0 (450);

  auto s_at = [&s] (int i, int j) -> float& {
    return s[static_cast<size_t> ((i - 1) + kNH1 * (j - 1))];
  };
  auto sync2d_at = [&sync2d, jzb] (int i, int j) -> float& {
    return sync2d[static_cast<size_t> ((i - 1) + kNH1 * (j - jzb))];
  };
  auto syncq_at = [&syncq_raw, jzb] (int i, int j) -> signed char& {
    return syncq_raw[static_cast<size_t> ((i - 1) + kNH1 * (j - jzb))];
  };
  auto candidate_at = [candidate] (int row, int col) -> float& {
    return candidate[static_cast<size_t> ((row - 1) + 4 * (col - 1))];
  };

  for (int j = 1; j <= kNHSYM; ++j)
    {
      int const ia = (j - 1) * kNSTEP;
      int const ib = ia + kNSPS - 1;

      std::fill (fft.in, fft.in + kNFFT1, 0.0f);

      if (j != 1)
        {
          for (int k = 0; k < kFt8VarEdgeWindow; ++k)
            {
              fft.in[759 + k] = dd8[ia - kFt8VarEdgeWindow + k] * windowx[kFt8VarEdgeWindow - 1 - k];
            }
        }
      for (int k = 0; k < kNSPS; ++k)
        {
          fft.in[960 + k] = facx * dd8[ia + k];
        }
      fft.in[960] *= 1.9f;
      fft.in[2879] *= 1.9f;
      if (j != kNHSYM)
        {
          for (int k = 0; k < kFt8VarEdgeWindow; ++k)
            {
              fft.in[2880 + k] = dd8[ib + 1 + k] * windowx[k];
            }
        }

      fftwf_execute (fft.plan);
      for (int i = 1; i <= kNH1; ++i)
        {
          float const re = fft.out[i][0];
          float const im = fft.out[i][1];
          if (ipass == 1 || ipass == 4 || ipass == 7)
            {
              s_at (i, j) = std::sqrt (re * re + im * im);
            }
          else if (ipass == 2 || ipass == 5 || ipass == 8)
            {
              s_at (i, j) = re * re + im * im;
            }
          else
            {
              s_at (i, j) = std::fabs (re) + std::fabs (im);
            }
        }
    }

  float const tstep = 0.04f;
  float const df = 3.125f;
  float rcandthin = static_cast<float> (ncandthin) / 100.0f;
  float const dtcenter = static_cast<float> (ndtcenter) / 100.0f;
  int ia = std::max (1, static_cast<int> (std::lround (static_cast<float> (nfa) / df)));
  int ib = std::max (1, static_cast<int> (std::lround (static_cast<float> (nfb) / df)));
  int iaw = std::max (1, static_cast<int> (std::lround (static_cast<float> (nfawide) / df)));
  int ibw = std::max (1, static_cast<int> (std::lround (static_cast<float> (nfbwide) / df)));
  ia = clamp_value (ia, 1, kNH1);
  ib = clamp_value (ib, 1, kNH1);
  iaw = clamp_value (iaw, 1, kNH1);
  ibw = clamp_value (ibw, 1, kNH1);
  if (ib < ia || ibw < iaw)
    {
      return;
    }

  ftx_sync8var_scan_c (s.data (), kNH1, kNHSYM, iaw, ibw, jzb, jzt, ipass,
                       lagcc, lagccbail, sync2d.data (), syncq_raw.data ());

  for (int i = iaw; i <= ibw; ++i)
    {
      float best = -1.0e30f;
      int bestj = jzb;
      for (int j = jzb; j <= jzt; ++j)
        {
          float const value = sync2d_at (i, j);
          if (value > best)
            {
              best = value;
              bestj = j;
            }
        }
      jpeak[static_cast<size_t> (i)] = bestj;
      red[static_cast<size_t> (i)] = best;
      redcq[static_cast<size_t> (i)] = syncq_at (i, bestj) != 0 ? 1 : 0;
    }

  int const iz = ibw - iaw + 1;
  int const npctile = std::max (1, static_cast<int> (std::lround (0.40f * static_cast<float> (iz))));
  std::vector<float> red_wide;
  red_wide.reserve (static_cast<size_t> (iz));
  for (int i = iaw; i <= ibw; ++i)
    {
      red_wide.push_back (red[static_cast<size_t> (i)]);
    }
  std::sort (red_wide.begin (), red_wide.end ());
  float base = red_wide[static_cast<size_t> (npctile - 1)];
  if (base < 1.0e-8f)
    {
      base = 1.0f;
    }
  for (int i = 1; i <= kNH1; ++i)
    {
      red[static_cast<size_t> (i)] /= base;
    }

  bool lpass1 = false;
  bool lpass2 = false;
  if (rcandthin < 0.99f)
    {
      if (ipass == 1 || ipass == 4 || ipass == 7) lpass1 = true;
      else if (ipass == 2 || ipass == 5 || ipass == 8) lpass2 = true;
    }

  std::vector<int> rank_band;
  rank_band.reserve (static_cast<size_t> (ib - ia + 1));
  for (int i = ia; i <= ib; ++i)
    {
      rank_band.push_back (i);
    }
  std::stable_sort (rank_band.begin (), rank_band.end (), [&red] (int left, int right) {
    return red[static_cast<size_t> (left)] < red[static_cast<size_t> (right)];
  });

  int k = 0;
  for (auto it = rank_band.rbegin (); it != rank_band.rend (); ++it)
    {
      int const n = *it;
      float const freq = static_cast<float> (n) * df;
      if (std::fabs (freq - static_cast<float> (nfqso)) > 3.0f)
        {
          if (red[static_cast<size_t> (n)] < syncmin) continue;
        }
      else if (red[static_cast<size_t> (n)] < 1.1f)
        {
          continue;
        }
      if (jpeak[static_cast<size_t> (n)] < -49 || jpeak[static_cast<size_t> (n)] > 76) continue;
      if (k >= 450) break;
      Ft8VarCandidate& item = candidate0[static_cast<size_t> (k)];
      item.frequency = freq;
      item.dt = static_cast<float> (jpeak[static_cast<size_t> (n)] - 1) * tstep;
      item.sync = red[static_cast<size_t> (n)];
      item.cq = redcq[static_cast<size_t> (n)] ? 2.0f : 0.0f;
      item.score = item.sync;
      if (rcandthin < 0.99f)
        {
          float const denom = std::fabs (item.dt - dtcenter) + 1.0f;
          item.score = lpass2 ? item.sync / (denom * denom) : item.sync / denom;
        }
      ++k;
    }
  int ncand0 = k;

  for (int i = 0; i < ncand0; ++i)
    {
      if (candidate0[static_cast<size_t> (i)].sync <= 0.0f) continue;
      for (int j = 0; j < i; ++j)
        {
          if (candidate0[static_cast<size_t> (j)].sync <= 0.0f) continue;
          float const fdiff = std::fabs (candidate0[static_cast<size_t> (i)].frequency
                                         - candidate0[static_cast<size_t> (j)].frequency);
          float const xdtdelta = std::fabs (candidate0[static_cast<size_t> (i)].dt
                                            - candidate0[static_cast<size_t> (j)].dt);
          if (fdiff < 4.0f
              && std::fabs (candidate0[static_cast<size_t> (i)].frequency - static_cast<float> (nfqso)) > 3.0f
              && xdtdelta < 0.1f)
            {
              if (candidate0[static_cast<size_t> (i)].sync >= candidate0[static_cast<size_t> (j)].sync)
                {
                  candidate0[static_cast<size_t> (j)].sync = 0.0f;
                }
              else
                {
                  candidate0[static_cast<size_t> (i)].sync = 0.0f;
                }
            }
        }
    }

  std::vector<int> order_candidates;
  order_candidates.reserve (static_cast<size_t> (ncand0));
  for (int i = 0; i < ncand0; ++i)
    {
      order_candidates.push_back (i);
    }
  std::stable_sort (order_candidates.begin (), order_candidates.end (),
                    [&candidate0, rcandthin] (int left, int right) {
                      float const lhs = rcandthin > 0.99f
                        ? candidate0[static_cast<size_t> (left)].sync
                        : candidate0[static_cast<size_t> (left)].score;
                      float const rhs = rcandthin > 0.99f
                        ? candidate0[static_cast<size_t> (right)].sync
                        : candidate0[static_cast<size_t> (right)].score;
                      return lhs < rhs;
                    });

  int out = 1;
  int ncandfqso = 0;
  float fprev = 5004.0f;
  for (auto it = order_candidates.rbegin (); it != order_candidates.rend (); ++it)
    {
      Ft8VarCandidate const& item = candidate0[static_cast<size_t> (*it)];
      if (std::fabs (item.frequency - static_cast<float> (nfqso)) <= 3.0f
          && item.sync >= 1.1f
          && std::fabs (item.frequency - fprev) > 3.0f)
        {
          candidate_at (1, out) = item.frequency;
          candidate_at (2, out) = item.dt;
          candidate_at (3, out) = item.sync;
          candidate_at (4, out) = item.cq;
          fprev = item.frequency;
          ++out;
          ++ncandfqso;
        }
    }

  if (lqsothread != 0 && out <= 460)
    {
      candidate_at (1, out) = static_cast<float> (nfqso);
      candidate_at (2, out) = 5.0f;
      ++out;
      ++ncandfqso;
    }
  if (lqsothread != 0 && out <= 460)
    {
      candidate_at (1, out) = static_cast<float> (nfqso);
      candidate_at (2, out) = -5.0f;
      ++out;
      ++ncandfqso;
    }

  for (auto it = order_candidates.rbegin (); it != order_candidates.rend (); ++it)
    {
      Ft8VarCandidate const& item = candidate0[static_cast<size_t> (*it)];
      float const syncmin1 = std::fabs (item.frequency - static_cast<float> (nfqso)) > 3.0f ? syncmin : 1.1f;
      if (item.sync >= syncmin1)
        {
          if (out > 460) break;
          candidate_at (1, out) = item.frequency;
          candidate_at (2, out) = item.dt;
          candidate_at (3, out) = item.sync;
          candidate_at (4, out) = item.cq;
          ++out;
        }
    }

  *ncand = out - 1;
  if (*ncand - ncandfqso > 1 && rcandthin < 0.99f)
    {
      if (lpass1)
        {
          rcandthin = std::min (rcandthin * 1.27f, 1.0f);
        }
      else if (lpass2)
        {
          rcandthin = rcandthin > 0.79f ? rcandthin * rcandthin : rcandthin * 0.79f;
        }
      else
        {
          rcandthin = std::min (rcandthin * 5.0f, 1.0f);
        }
      *ncand = ncandfqso + static_cast<int> (std::lround (static_cast<float> (*ncand - ncandfqso) * rcandthin));
    }
}

extern "C" void ftx_sync8_search_stage4_c (float const* dd, int npts, float nfa, float nfb,
                                            float syncmin, float nfqso, int maxcand, int ipass,
                                            float* candidate, int* ncand, float* sbase)
{
  if (!dd || !candidate || !ncand || !sbase || maxcand <= 0 || npts < kNSPS)
    {
      return;
    }

  std::fill (candidate, candidate + 4 * maxcand, 0.0f);
  std::fill (sbase, sbase + kNH1, 0.0f);
  *ncand = 0;

  fill_ft8_stage4_sbase (dd, npts, nfa, nfb, sbase);

  std::array<float, kFt8VarEdgeWindow> const edge_window = make_ft8_sync8var_edge_window ();
  std::array<float, 4 * 460> var_candidate {};
  int var_ncand = 0;
  int const nfa_i = static_cast<int> (std::lround (nfa));
  int const nfb_i = static_cast<int> (std::lround (nfb));
  int const nfqso_i = static_cast<int> (std::lround (nfqso));
  int const pass = std::max (1, std::min (ipass, 9));

  ftx_sync8var_c (dd, edge_window.data (), 1.0f / 300.0f,
                  nfa_i, nfb_i, syncmin, nfqso_i,
                  var_candidate.data (), &var_ncand,
                  -62, 62, pass, 0, 100, 0, 0, 0, nfa_i, nfb_i);

  int const out_count = std::min (std::max (0, var_ncand), std::min (maxcand, 460));
  for (int i = 0; i < out_count; ++i)
    {
      candidate[i * 4 + 0] = var_candidate[4 * i + 0];
      candidate[i * 4 + 1] = var_candidate[4 * i + 1];
      candidate[i * 4 + 2] = var_candidate[4 * i + 2];
      candidate[i * 4 + 3] = var_candidate[4 * i + 3];
    }
  *ncand = out_count;
}

extern "C" void sync8_ (float* dd, int* npts, int* nfa, int* nfb,
                        float* syncmin, int* nfqso, int* maxcand,
                        float* candidate, int* ncand, float* sbase)
{
  if (!npts || !nfa || !nfb || !syncmin || !nfqso || !maxcand)
    {
      if (ncand)
        {
          *ncand = 0;
        }
      return;
    }

  ftx_sync8_search_c (dd, *npts, static_cast<float> (*nfa), static_cast<float> (*nfb),
                      *syncmin, static_cast<float> (*nfqso), *maxcand, candidate, ncand, sbase);
}
