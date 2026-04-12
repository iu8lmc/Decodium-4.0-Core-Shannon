#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

#include <fftw3.h>

namespace
{

using Complex = std::complex<float>;

constexpr int kFt8CodewordBits = 174;
constexpr int kFt8MessageBits = 91;
constexpr int kFt8PayloadBits = 77;
constexpr int kFt8Symbols = 79;
constexpr int kFt8CandidateRows = 3;
constexpr int kFt8MaxCandidates = 100;
constexpr int kFt8DownsampleSize = 3200;
constexpr int kFt8DownsampleNp2 = 2812;
constexpr int kFt8SearchNfa = 500;
constexpr int kFt8SearchNfb = 2500;
constexpr int kFt8SearchNfqso = 750;
constexpr float kFt8SearchSyncMin = 1.5f;
constexpr float kFt8DownsampleRate = 12000.0f / 60.0f;
constexpr float kFt8DownsampleDt = 1.0f / kFt8DownsampleRate;
constexpr float kTwoPi = 6.28318530717958647692f;

extern "C"
{
  void ftx_sync8_search_c (float const* dd, int npts, float nfa, float nfb,
                           float syncmin, float nfqso, int maxcand,
                           float* candidate, int* ncand, float* sbase);
  void ftx_ft8_downsample_c (float const* dd, int* newdat, float f0, fftwf_complex* c1);
  void ftx_sync8d_c (Complex const* cd0, int np, int i0,
                     Complex const* ctwk, int itwk, float* sync);
  void ftx_ft8_bitmetrics_scaled_c (Complex const* cd0, int np2, int ibest, int imetric,
                                    float scale, float* s8_out, int* nsync_out,
                                    float* llra, float* llrb, float* llrc,
                                    float* llrd, float* llre);
  void ftx_decode174_91_c (float const* llr_in, int Keff, int maxosd, int norder,
                           signed char const* apmask_in, signed char* message91_out,
                           signed char* cw_out, int* ntype_out, int* nharderror_out,
                           float* dmin_out);
  int legacy_pack77_unpack77bits_c (signed char const* message77, int received,
                                    char msgsent[37], int* quirky_out);
  int ftx_ft8_message77_to_itone_c (signed char const* message77, int* itone_out);
  void ftx_subtract_ft8_c (float* dd0, int const* itone, float f0, float dt, int lrefinedt);
}

inline float candidate_at (float const* candidate, int row, int col)
{
  return candidate[(row - 1) + kFt8CandidateRows * (col - 1)];
}

inline int clamp_index (int value, int lo, int hi)
{
  return std::max (lo, std::min (hi, value));
}

int bits_to_int (signed char const* bits, int count)
{
  int value = 0;
  for (int i = 0; i < count; ++i)
    {
      value = (value << 1) | (bits[i] != 0 ? 1 : 0);
    }
  return value;
}

bool all_zero (std::array<signed char, kFt8CodewordBits> const& cw)
{
  return std::all_of (cw.begin (), cw.end (), [] (signed char bit) { return bit == 0; });
}

void build_frequency_tweak (float delf, std::array<Complex, 32>& ctwk)
{
  float const dphi = kTwoPi * delf * kFt8DownsampleDt;
  float phi = 0.0f;
  for (Complex& sample : ctwk)
    {
      sample = Complex (std::cos (phi), std::sin (phi));
      phi = std::fmod (phi + dphi, kTwoPi);
      if (phi < 0.0f)
        {
          phi += kTwoPi;
        }
    }
}

}

extern "C" void ftx_sfox_remove_ft8_c (float* dd, int npts)
{
  if (!dd || npts <= 0)
    {
      return;
    }

  std::array<float, kFt8CandidateRows * kFt8MaxCandidates> candidate {};
  std::array<float, 1920> sbase {};
  int ncand = 0;
  ftx_sync8_search_c (dd, npts,
                      static_cast<float> (kFt8SearchNfa),
                      static_cast<float> (kFt8SearchNfb),
                      kFt8SearchSyncMin,
                      static_cast<float> (kFt8SearchNfqso),
                      kFt8MaxCandidates,
                      candidate.data (), &ncand, sbase.data ());

  int newdat = 1;
  std::array<Complex, kFt8DownsampleSize> cd0 {};
  std::array<Complex, 32> ctwk {};
  std::array<float, 8 * kFt8Symbols> s8 {};
  std::array<float, kFt8CodewordBits> llra {};
  std::array<float, kFt8CodewordBits> llrb {};
  std::array<float, kFt8CodewordBits> llrc {};
  std::array<float, kFt8CodewordBits> llrd {};
  std::array<float, kFt8CodewordBits> llre {};
  std::array<signed char, kFt8MessageBits> message91 {};
  std::array<signed char, kFt8PayloadBits> message77 {};
  std::array<signed char, kFt8CodewordBits> cw {};
  std::array<signed char, kFt8CodewordBits> apmask {};
  std::array<int, kFt8Symbols> itone {};
  std::array<char, 37> msg37 {};

  for (int icand = 1; icand <= ncand; ++icand)
    {
      float f1 = candidate_at (candidate.data (), 1, icand);
      float xdt = candidate_at (candidate.data (), 2, icand);
      (void) clamp_index (static_cast<int> (std::lround (f1 / 3.125f)) - 1, 0,
                          static_cast<int> (sbase.size ()) - 1);

      ftx_ft8_downsample_c (dd, &newdat, f1, reinterpret_cast<fftwf_complex*> (cd0.data ()));
      newdat = 0;

      int const i0 = static_cast<int> (std::lround ((xdt + 0.5f) * kFt8DownsampleRate));
      float smax = 0.0f;
      int ibest = i0;
      for (int idt = i0 - 10; idt <= i0 + 10; ++idt)
        {
          float sync = 0.0f;
          ftx_sync8d_c (cd0.data (), kFt8DownsampleNp2, idt, ctwk.data (), 0, &sync);
          if (sync > smax)
            {
              smax = sync;
              ibest = idt;
            }
        }

      smax = 0.0f;
      float delfbest = 0.0f;
      for (int ifr = -5; ifr <= 5; ++ifr)
        {
          float const delf = static_cast<float> (ifr) * 0.5f;
          build_frequency_tweak (delf, ctwk);
          float sync = 0.0f;
          ftx_sync8d_c (cd0.data (), kFt8DownsampleNp2, ibest, ctwk.data (), 1, &sync);
          if (sync > smax)
            {
              smax = sync;
              delfbest = delf;
            }
        }

      f1 += delfbest;
      ftx_ft8_downsample_c (dd, &newdat, f1, reinterpret_cast<fftwf_complex*> (cd0.data ()));

      std::array<float, 9> ss {};
      for (int idt = -4; idt <= 4; ++idt)
        {
          float sync = 0.0f;
          ftx_sync8d_c (cd0.data (), kFt8DownsampleNp2, ibest + idt, ctwk.data (), 0, &sync);
          ss[static_cast<size_t> (idt + 4)] = sync;
        }
      auto const best_sync = std::max_element (ss.begin (), ss.end ());
      ibest += static_cast<int> (best_sync - ss.begin ()) - 4;
      xdt = static_cast<float> (ibest - 1) * kFt8DownsampleDt;

      int nsync = 0;
      ftx_ft8_bitmetrics_scaled_c (cd0.data (), kFt8DownsampleNp2, ibest, 1, 2.83f,
                                   s8.data (), &nsync,
                                   llra.data (), llrb.data (), llrc.data (),
                                   llrd.data (), llre.data ());
      if (nsync <= 6)
        {
          continue;
        }

      std::fill (apmask.begin (), apmask.end (), 0);
      std::fill (message91.begin (), message91.end (), 0);
      std::fill (cw.begin (), cw.end (), 0);
      int ntype = 0;
      int nharderrors = -1;
      float dmin = 0.0f;
      ftx_decode174_91_c (llra.data (), 91, -1, 2, apmask.data (),
                          message91.data (), cw.data (), &ntype,
                          &nharderrors, &dmin);
      if (nharderrors < 0 || all_zero (cw))
        {
          continue;
        }

      std::copy_n (message91.begin (), kFt8PayloadBits, message77.begin ());
      int const n3 = bits_to_int (message77.data () + 71, 3);
      int const i3 = bits_to_int (message77.data () + 74, 3);
      if (i3 > 5 || (i3 == 0 && n3 > 6) || (i3 == 0 && n3 == 2))
        {
          continue;
        }

      int quirky = 0;
      if (legacy_pack77_unpack77bits_c (message77.data (), 1, msg37.data (), &quirky) == 0)
        {
          continue;
        }
      if (ftx_ft8_message77_to_itone_c (message77.data (), itone.data ()) == 0)
        {
          continue;
        }

      ftx_subtract_ft8_c (dd, itone.data (), f1, xdt, 1);
    }
}
