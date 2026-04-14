#include "wsjtx_config.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <numeric>
#include <string>
#include <vector>

extern "C"
{
void four2a_ (std::complex<float> a[], int* nfft, int* ndim, int* isign, int* iform);
void polfit_ (float y[], int* npts, float a[]);
void ftx_q65_async_decode_latest_c_api (short const* iwave, int* nqd0, int* nutc,
                                        int* ntrperiod, int* nsubmode, int* nfqso,
                                        int* ntol, int* ndepth, int* nfa, int* nfb,
                                        int* nclearave, int* single_decode,
                                        int* nagain, int* max_drift, int* lnewdat,
                                        float* emedelay, char const mycall[12],
                                        char const hiscall[12], char const hisgrid[6],
                                        int* nqsoprogress, int* ncontest,
                                        int* lapcqonly, int* snr0, float* dt0,
                                        int* freq0, char cq0[3], char msg0[37],
                                        int* found);

extern std::complex<float> cacb_[2 * 5376000];
}

namespace
{
constexpr int kNfft {32768};
constexpr int kSsRows {322};
constexpr int kSyncCount {22};
constexpr int kLagMax {30};
constexpr int kMaxFft1 {5376000};
constexpr int kMaxFft2 {336000};
constexpr int kWaveSamples {60 * 12000};
constexpr float kSyncThreshold {4.5f};
constexpr float kRad {57.2957795f};

struct SyncInfo
{
  float ccfmax {};
  float xdt {};
  float pol {};
  int ipol {1};
  bool birdie {false};
};

struct Candidate
{
  float snr {};
  float f {};
  float xdt {};
  float pol {};
  int ipol {1};
  int indx {0};
};

std::array<unsigned char, kNfft + 1> g_decoded_bins {};
int g_last_quick_count {0};
int g_last_decode_count {0};
int g_last_nutc {-1};
double g_last_freq1 {0.0};
std::string g_last_msg;
std::array<int, kSyncCount> const kQ65SyncSymbols {{
    1, 9, 12, 13, 15, 22, 23, 26, 27, 33, 35, 38,
    46, 50, 55, 60, 62, 66, 69, 74, 76, 85
}};

inline std::size_t ss_index (int pol1, int row1, int column1)
{
  return static_cast<std::size_t> (pol1 - 1)
      + 4u * (static_cast<std::size_t> (row1 - 1)
              + static_cast<std::size_t> (kSsRows) * static_cast<std::size_t> (column1 - 1));
}

inline std::size_t savg_index (int pol1, int column1)
{
  return static_cast<std::size_t> (pol1 - 1) + 4u * static_cast<std::size_t> (column1 - 1);
}

inline int clamp_int (int value, int lo, int hi)
{
  return std::max (lo, std::min (value, hi));
}

float percentile_value (float const* values, int count, int pct)
{
  if (!values || count <= 0)
    {
      return 0.0f;
    }

  std::vector<float> sorted (values, values + count);
  std::size_t const index = std::min (
      sorted.size () - 1,
      static_cast<std::size_t> (
          std::floor ((static_cast<float> (pct) / 100.0f) * static_cast<float> (sorted.size () - 1))));
  std::nth_element (sorted.begin (), sorted.begin () + static_cast<long> (index), sorted.end ());
  return sorted[index];
}

std::string trim_fixed (char const* data, std::size_t width)
{
  std::size_t length = width;
  while (length > 0 && (data[length - 1] == ' ' || data[length - 1] == '\0'))
    {
      --length;
    }
  return std::string {data, data + length};
}

template <std::size_t N> void to_fixed (std::string const& src, std::array<char, N>& dest)
{
  std::fill (dest.begin (), dest.end (), ' ');
  std::size_t const count = std::min (static_cast<int>(src.size ()), static_cast<int>(N));
  std::memcpy (dest.data (), src.data (), count);
}

void append_line (char const* path, std::string const& line)
{
  std::ofstream out {path, std::ios::app};
  if (!out) return;
  out << line << '\n';
}

std::array<int, kSyncCount> expanded_sync_positions ()
{
  static std::array<int, kSyncCount> expanded {};
  static bool initialized = false;
  if (!initialized)
    {
      float const tstep = 2048.0f / 11025.0f;
      float const fac = 0.6f / tstep;
      for (int i = 0; i < kSyncCount; ++i)
        {
          expanded[static_cast<std::size_t> (i)]
              = static_cast<int> (std::lround ((kQ65SyncSymbols[static_cast<std::size_t> (i)] - 1) * fac)) + 1;
        }
      initialized = true;
    }
  return expanded;
}

SyncInfo q65_sync_at_bin (float const* ss, float const* savg, std::array<float, 4> const& savg_med,
                          bool xpol, int jz, int i)
{
  SyncInfo info;
  int const npol = xpol ? 4 : 1;
  auto const isync = expanded_sync_positions ();

  float ccfmax = 0.0f;
  int lagbest = 0;
  int ipolbest = 1;
  std::array<float, 4> ccf4best {};
  for (int lag = 0; lag <= kLagMax; ++lag)
    {
      std::array<float, 4> ccf4 {};
      for (int j = 0; j < kSyncCount; ++j)
        {
          int const k = isync[static_cast<std::size_t> (j)] + lag;
          for (int pol = 1; pol <= npol; ++pol)
            {
              ccf4[static_cast<std::size_t> (pol - 1)]
                  += ss[ss_index (pol, k, i + 1)] + ss[ss_index (pol, k + 1, i + 1)]
                   + ss[ss_index (pol, k + 2, i + 1)];
            }
        }
      for (int pol = 1; pol <= npol; ++pol)
        {
          ccf4[static_cast<std::size_t> (pol - 1)]
              -= savg[savg_index (pol, i + 1)] * 3.0f * static_cast<float> (kSyncCount)
                 / static_cast<float> (jz);
        }
      auto const it = std::max_element (ccf4.begin (), ccf4.begin () + npol);
      float const ccf = *it;
      int const ipol = static_cast<int> (std::distance (ccf4.begin (), it)) + 1;
      if (ccf > ccfmax)
        {
          ccfmax = ccf;
          lagbest = lag;
          ipolbest = ipol;
          ccf4best = ccf4;
        }
    }

  float poldeg = 0.0f;
  if (xpol && ccfmax >= kSyncThreshold)
    {
      float y[4] {};
      float a[3] {};
      for (int idx = 0; idx < 4; ++idx) y[idx] = ccf4best[static_cast<std::size_t> (idx)];
      int npts = 4;
      polfit_ (y, &npts, a);
      poldeg = a[2];
    }

  info.ccfmax = ccfmax;
  info.xdt = lagbest * (2048.0f / 11025.0f) - 1.0f;
  info.pol = poldeg;
  info.ipol = ipolbest;
  float const denom = savg[savg_index (ipolbest, i)] / std::max (savg_med[static_cast<std::size_t> (ipolbest - 1)], 1.0e-6f);
  info.birdie = denom <= 0.0f || (ccfmax / denom) < 3.0f;
  return info;
}

std::vector<Candidate> find_q65_candidates (float const* ss, float const* savg, bool xpol,
                                            int jz, int nfa, int nfb, int nts_q65)
{
  std::vector<Candidate> result;
  if (!ss || !savg || nts_q65 <= 0 || jz <= 0) return result;

  float const df3 = 96000.0f / static_cast<float> (kNfft);
  int ia = static_cast<int> (std::lround (1000.0f * nfa / df3)) + 1;
  int ib = static_cast<int> (std::lround (1000.0f * nfb / df3)) + 1;
  ia = std::max (1, ia);
  ib = std::min (kNfft - 1, ib);
  if (ib <= ia) return result;

  int const npol = xpol ? 4 : 1;
  std::array<float, 4> savg_med {};
  for (int pol = 1; pol <= npol; ++pol)
    {
      std::vector<float> span;
      span.reserve (static_cast<std::size_t> (ib - ia + 1));
      for (int col = ia; col <= ib; ++col)
        {
          span.push_back (savg[savg_index (pol, col)]);
        }
      savg_med[static_cast<std::size_t> (pol - 1)] = percentile_value (span.data (), ib - ia + 1, 50);
    }

  std::vector<SyncInfo> sync (static_cast<std::size_t> (kNfft + 1));
  for (int i = ia; i <= ib; ++i)
    {
      sync[static_cast<std::size_t> (i)] = q65_sync_at_bin (ss, savg, savg_med, xpol, jz, i);
    }

  std::vector<float> metric;
  metric.reserve (static_cast<std::size_t> (ib - ia + 1));
  for (int i = ia; i <= ib; ++i) metric.push_back (sync[static_cast<std::size_t> (i)].ccfmax);
  float const base = percentile_value (metric.data (), ib - ia + 1, 50);
  if (base > 0.0f)
    {
      for (int i = ia; i <= ib; ++i)
        {
          sync[static_cast<std::size_t> (i)].ccfmax /= base;
        }
    }

  float const bw_blank = 65.0f * 4.0f * 1.66666667f;
  int const nbw_blank = static_cast<int> (bw_blank / df3) + 1;
  int const nguard = 10;
  for (int i = ia; i <= ib; ++i)
    {
      if (sync[static_cast<std::size_t> (i)].ccfmax < 2.0f) continue;
      int jmax = i;
      float spk = sync[static_cast<std::size_t> (i)].ccfmax;
      int upper = std::min (ib, i + nbw_blank);
      for (int j = i; j <= upper; ++j)
        {
          if (sync[static_cast<std::size_t> (j)].ccfmax > spk)
            {
              spk = sync[static_cast<std::size_t> (j)].ccfmax;
              jmax = j;
            }
        }
      int const ja = std::max (ia, std::min (i, jmax - nguard));
      int const jb = std::min (ib, jmax + nbw_blank + nguard);
      for (int j = ja; j <= jb; ++j)
        {
          sync[static_cast<std::size_t> (j)].ccfmax = 0.0f;
        }
      sync[static_cast<std::size_t> (jmax)].ccfmax = spk;
    }

  std::vector<int> order;
  order.reserve (static_cast<std::size_t> (ib - ia + 1));
  for (int i = ia; i <= ib; ++i) order.push_back (i);
  std::sort (order.begin (), order.end (),
             [&sync] (int lhs, int rhs) {
               return sync[static_cast<std::size_t> (lhs)].ccfmax > sync[static_cast<std::size_t> (rhs)].ccfmax;
             });

  float const tstep = 2048.0f / 11025.0f;
  for (int n : order)
    {
      SyncInfo const& item = sync[static_cast<std::size_t> (n)];
      if (item.ccfmax < kSyncThreshold) break;
      if (item.birdie) continue;

      int j1 = static_cast<int> ((item.xdt + 1.0f) / tstep - 1.0f);
      int j2 = static_cast<int> ((item.xdt + 52.0f) / tstep + 1.0f);
      j1 = std::max (1, j1);
      j2 = std::min (jz + 1, j2);

      std::array<float, 41> pavg {};
      for (int j = 1; j <= j1; ++j)
        {
          for (int off = -20; off <= 20; ++off)
            {
              pavg[static_cast<std::size_t> (off + 20)]
                  += ss[ss_index (item.ipol, j, clamp_int (n + off, 1, kNfft))];
            }
        }
      for (int j = j2; j <= jz; ++j)
        {
          for (int off = -20; off <= 20; ++off)
            {
              pavg[static_cast<std::size_t> (off + 20)]
                  += ss[ss_index (item.ipol, j, clamp_int (n + off, 1, kNfft))];
            }
        }

      int const jsum = j1 + std::max (0, jz - j2 + 1);
      if (jsum <= 0) continue;
      float pmax = *std::max_element (pavg.begin () + 18, pavg.begin () + 23);
      float const total = std::accumulate (pavg.begin (), pavg.end (), 0.0f);
      float const local_base = (total - pmax) / static_cast<float> (jsum);
      if (local_base <= 0.0f || (pmax / local_base) > 5.0f) continue;

      float const f0 = 0.001f * static_cast<float> (n - 1) * df3;
      bool skip = false;
      for (Candidate const& prior : result)
        {
          float const diffhz = 1000.0f * (f0 - prior.f);
          float const bw = static_cast<float> (nts_q65) * 110.0f;
          if (diffhz > -0.03f * bw && diffhz < 1.03f * bw)
            {
              skip = true;
              break;
            }
        }
      if (skip) continue;

      Candidate candidate;
      candidate.snr = item.ccfmax;
      candidate.f = f0;
      candidate.xdt = item.xdt;
      candidate.pol = item.pol;
      candidate.ipol = item.ipol;
      candidate.indx = n;
      result.push_back (candidate);
      if (static_cast<int> (result.size ()) >= 50) break;
    }

  return result;
}

void write_stdout_line (int ikhz1, int ndf, int npol, int nutc, float xdt0, int nsnr0,
                        std::string const& msg, std::string const& cq0, int ntxpol, char cp)
{
  char line[160];
  std::snprintf (line, sizeof (line), "!%03d%5d%4d%06d%5.1f%5d : %-28.28s%-3.3s%4d %c",
                 ikhz1, ndf, npol, nutc, xdt0, nsnr0, msg.c_str (), cq0.c_str (), ntxpol, cp);
  std::fputs (line, stdout);
  std::fputc ('\n', stdout);
  std::fflush (stdout);
}

void write_tmp26_line (double freq1, int ndf, float xdt0, int npol, int nsnr0, int nutc,
                       std::string const& msg, char cp, int mode_q65)
{
  char cmode[3] {':', static_cast<char> ('A' + mode_q65 - 1), '\0'};
  char line[128];
  std::snprintf (line, sizeof (line), "%8.3f%5d%3d%3d%3d%5.1f%4d%3d%4d%5.4d    %-28.28s :%c  %2.2s",
                 freq1, ndf, 0, 0, 0, xdt0, npol, 0, nsnr0, nutc, msg.c_str (), cp, cmode);
  append_line ("tmp26.txt", line);
}

void write_map65_rx_line (double freq1, int ndf, float xdt0, int npol, int nsnr0, int nutc,
                          std::string const& msg, std::string const& cq0)
{
  char line[128];
  char submode = cq0.size () >= 2 ? cq0[1] : ' ';
  std::snprintf (line, sizeof (line), "%8.3f%5d%5.1f%4d%4d%5.4d  %-28.28s: %c  %-3.3s",
                 freq1, ndf, xdt0, npol, nsnr0, nutc, msg.c_str (), submode, cq0.c_str ());
  append_line ("map65_rx.log", line);
}

void write_wb_ftx_line (int nutc, float fsked, float xdt0, int nsnr0, std::string const& msg)
{
  char line[128];
  std::snprintf (line, sizeof (line), "%04d%9.3f%7.2f%5d  %s", nutc, fsked, xdt0, nsnr0, msg.c_str ());
  append_line ("wb_ftx.txt", line);
}

int parse_idec (std::string const& cq0)
{
  if (cq0.size () >= 2 && std::isdigit (static_cast<unsigned char> (cq0[1])))
    {
      return cq0[1] - '0';
    }
  return -1;
}

int decode_q65_candidate (Candidate const& candidate, int nqd, double fcenter, int nfcal,
                          int nfsample, int ikhz, int mousedf, int ntol, bool xpol,
                          std::string const& mycall, std::string const& mygrid,
                          std::string const& hiscall, std::string const& hisgrid,
                          int mode_q65, float fqso, int nkhz_center, int newdat, int nagain,
                          int max_drift, int nutc, int nxant, int ndop00)
{
  (void) mygrid;
  if (candidate.indx <= 0 || candidate.indx > kNfft) return -1;
  if (g_decoded_bins[static_cast<std::size_t> (candidate.indx)] != 0) return 0;
  if (candidate.snr < 1.5f) return -1;

  int nfft1 = kMaxFft1;
  int nfft2 = kMaxFft2;
  float df = 96000.0f / static_cast<float> (nfft1);
  if (nfsample == 95238)
    {
      nfft1 = 5120000;
      nfft2 = 322560;
      df = 96000.0f / static_cast<float> (nfft1);
    }
  int const nh = nfft2 / 2;
  float const df3 = 96000.0f / static_cast<float> (kNfft);
  float const f_mouse = 1000.0f * (fqso + 48.0f) + static_cast<float> (mousedf);
  int k0 = static_cast<int> (std::lround ((candidate.indx * df3 - 1000.0f) / df));
  if (nagain == 1)
    {
      k0 = static_cast<int> (std::lround ((f_mouse - 1000.0f) / df));
    }
  if (k0 < nh || k0 > nfft1 - nfft2 + 1) return -1;

  std::complex<float>* ca = cacb_;
  std::complex<float>* cb = cacb_ + kMaxFft1;
  std::vector<std::complex<float>> cz (static_cast<std::size_t> (nfft2 + 1), {0.0f, 0.0f});
  float const fac = 1.0f / static_cast<float> (nfft2);
  float const c = std::cos (candidate.pol / kRad);
  float const s = std::sin (candidate.pol / kRad);
  for (int i = 0; i < nfft2; ++i)
    {
      std::complex<float> value = fac * ca[static_cast<std::size_t> (k0 - 1 + i)];
      if (xpol)
        {
          value = c * value + s * (fac * cb[static_cast<std::size_t> (k0 - 1 + i)]);
        }
      cz[static_cast<std::size_t> (i)] = value;
    }

  int const ja = static_cast<int> (std::lround (500.0f / df));
  int const jb = static_cast<int> (std::lround (2500.0f / df));
  for (int i = 0; i <= ja; ++i)
    {
      float const r = 0.5f * (1.0f + std::cos (i * 3.14159f / ja));
      cz[static_cast<std::size_t> (ja - i)] *= r;
      cz[static_cast<std::size_t> (jb + i)] *= r;
    }
  for (int i = ja + jb + 1; i <= nfft2; ++i)
    {
      cz[static_cast<std::size_t> (i)] = {0.0f, 0.0f};
    }

  int nfft_arg = 2 * nfft2;
  int ndim = 1;
  int isign = 1;
  int iform = -1;
  four2a_ (cz.data (), &nfft_arg, &ndim, &isign, &iform);

  std::vector<short> iwave (kWaveSamples, 0);
  for (int i = 0; i < nfft2; ++i)
    {
      int const j = nfft2 - 1 - i;
      iwave[static_cast<std::size_t> (2 * i + 1)] =
          static_cast<short> (std::lround (std::real (cz[static_cast<std::size_t> (j)])));
      iwave[static_cast<std::size_t> (2 * i)] =
          static_cast<short> (std::lround (std::imag (cz[static_cast<std::size_t> (j)])));
    }

  int const nsubmode = mode_q65 - 1;
  int nfa = 990;
  int nfb = 1010;
  if (nagain == 1)
    {
      nfa = std::max (100, 1000 - ntol);
      nfb = std::min (2500, 1000 + ntol);
    }

  std::array<char, 12> mycall_c {};
  std::array<char, 12> hiscall_c {};
  std::array<char, 6> hisgrid_c {};
  to_fixed (mycall, mycall_c);
  to_fixed (hiscall, hiscall_c);
  to_fixed (hisgrid, hisgrid_c);

  int nsnr0 = -99;
  int nfreq0 = 0;
  float xdt0 = 0.0f;
  std::array<char, 37> msg0 {};
  std::array<char, 3> cq0 {};
  int found = 0;
  int nqd_c = nqd;
  int nutc_c = nutc;
  int ntrperiod_c = 60;
  int nsubmode_c = nsubmode;
  int nfqso_c = 1000;
  int ntol_c = ntol;
  int ndepth_c = 3;
  int nfa_c = nfa;
  int nfb_c = nfb;
  int nclearave_c = 0;
  int single_decode_c = 0;
  int nagain_c = 0;
  int max_drift_c = max_drift;
  int lnewdat_c = newdat;
  float emedelay_c = 2.5f;
  int nqsoprogress_c = 0;
  int ncontest_c = 0;
  int lapcqonly_c = 0;
  ftx_q65_async_decode_latest_c_api (
      iwave.data (), &nqd_c, &nutc_c, &ntrperiod_c, &nsubmode_c, &nfqso_c, &ntol_c,
      &ndepth_c, &nfa_c, &nfb_c, &nclearave_c, &single_decode_c, &nagain_c, &max_drift_c,
      &lnewdat_c, &emedelay_c, mycall_c.data (), hiscall_c.data (), hisgrid_c.data (),
      &nqsoprogress_c, &ncontest_c, &lapcqonly_c, &nsnr0, &xdt0, &nfreq0,
      cq0.data (), msg0.data (), &found);
  if (found == 0 || nsnr0 <= -99) return -1;

  g_decoded_bins[static_cast<std::size_t> (candidate.indx)] = 1;
  std::string const msg = trim_fixed (msg0.data (), 28);
  std::string const cq0s = trim_fixed (cq0.data (), 3);
  int const idec = parse_idec (cq0s);

  int nq65df = static_cast<int> (std::lround (
      1000.0f * (0.001f * k0 * df + nkhz_center - 48.0f + 1.0f - 1.27046f - static_cast<float> (ikhz))))
      - nfcal;
  nq65df += nfreq0 - 1000;
  int npol = static_cast<int> (std::lround (candidate.pol));
  if (nxant != 0)
    {
      npol -= 45;
      if (npol < 0) npol += 180;
    }
  int ikhz1 = ikhz;
  int ndf = nq65df;
  if (ndf > 500) ikhz1 = ikhz + (nq65df + 500) / 1000;
  if (ndf < -500) ikhz1 = ikhz + (nq65df - 500) / 1000;
  ndf = nq65df - 1000 * (ikhz1 - ikhz);

  if (nqd == 1 && std::abs (nq65df - mousedf) < ntol)
    {
      write_stdout_line (ikhz1, ndf, npol, nutc, xdt0, nsnr0, msg, cq0s, 0, ' ');
      ++g_last_quick_count;
    }

  double const freq0 = fcenter + 0.001 * static_cast<double> (ikhz);
  double const freq1 = freq0 + 0.001 * static_cast<double> (ikhz1 - ikhz);
  write_tmp26_line (freq1, ndf, xdt0, npol, nsnr0, nutc, msg, ' ', mode_q65);

  if (nutc != g_last_nutc || msg != g_last_msg || freq1 != g_last_freq1)
    {
      write_map65_rx_line (freq1, ndf, xdt0, npol, nsnr0, nutc, msg, cq0s);
      float const frx = 0.001f * k0 * df + nkhz_center - 48.0f + 1.0f - 0.001f * nfcal;
      float const fsked = frx - 0.001f * ndop00 / 2.0f - 1.5f;
      write_wb_ftx_line (nutc, fsked, xdt0, nsnr0, msg);
      g_last_nutc = nutc;
      g_last_msg = msg;
      g_last_freq1 = freq1;
    }

  ++g_last_decode_count;
  return idec;
}
} // namespace

extern "C" void ftx_map65_native_decode_c (float const* ss, float const* savg, int* reset_state,
                                           double* fcenter, int* nutc, int* nfa, int* nfb,
                                           int* nfshift, int* ntol, int* mousedf, int* mousefqso,
                                           int* nagain, int* nfcal, int* nfsample, int* nxant,
                                           int* nxpol, int* nmode, int* max_drift, int* newdat,
                                           int* ndiskdat, int* nhsym, int* ndop00,
                                           char const mycall[12], char const mygrid[6],
                                           char const hiscall[12], char const hisgrid[6],
                                           char const datetime[20])
{
  g_last_quick_count = 0;
  g_last_decode_count = 0;
  if (newdat && *newdat == 1)
    {
      g_last_nutc = -1;
      g_last_freq1 = 0.0;
      g_last_msg.clear ();
    }
  if (!ss || !savg || !fcenter || !nutc || !nfa || !nfb || !nfshift || !ntol || !mousedf
      || !mousefqso || !nagain || !nfcal || !nfsample || !nxant || !nxpol || !nmode
      || !max_drift || !newdat || !ndiskdat || !nhsym || !ndop00 || !mycall || !mygrid
      || !hiscall || !hisgrid || !datetime)
    {
      return;
    }
  if (reset_state && *reset_state != 0)
    {
      g_decoded_bins.fill (0);
    }

  int const mode_q65 = *nmode / 10;
  if (mode_q65 <= 0) return;

  int const nkhz_center = static_cast<int> (std::lround (1000.0 * (*fcenter - std::floor (*fcenter))));
  int const mfa = *nfa - nkhz_center + 48;
  int const mfb = *nfb - nkhz_center + 48;
  int const nts_q65 = 1 << (mode_q65 - 1);
  bool const xpol = *nxpol != 0;
  auto const candidates = find_q65_candidates (ss, savg, xpol, *nhsym, mfa, mfb, nts_q65);

  float const foffset = 0.001f * static_cast<float> (1270 + *nfcal);
  float const fqso = static_cast<float> (*mousefqso) + foffset
      - 0.5f * static_cast<float> (*nfa + *nfb) + static_cast<float> (*nfshift);
  std::string const mycall_s = trim_fixed (mycall, 12);
  std::string const mygrid_s = trim_fixed (mygrid, 6);
  std::string const hiscall_s = trim_fixed (hiscall, 12);
  std::string const hisgrid_s = trim_fixed (hisgrid, 6);

  using Clock = std::chrono::steady_clock;
  Clock::time_point const started = Clock::now ();
  bool quick_called = false;
  for (Candidate const& cand : candidates)
    {
      float const freq = cand.f + nkhz_center - 48.0f - 1.27046f;
      int const nhzdiff = static_cast<int> (std::lround (1000.0f * (freq - *mousefqso) - *mousedf)) - *nfcal;
      if (std::abs (nhzdiff) > *ntol) continue;
      quick_called = true;
      Candidate quick_cand = cand;
      decode_q65_candidate (quick_cand, 1, *fcenter, *nfcal, *nfsample, *mousefqso, *mousedf,
                            *ntol, xpol, mycall_s, mygrid_s, hiscall_s, hisgrid_s, mode_q65,
                            fqso, nkhz_center, *newdat, *nagain, *max_drift, *nutc, *nxant,
                            *ndop00);
    }
  if (!quick_called)
    {
      Candidate fallback;
      fallback.f = static_cast<float> (*mousefqso) + 0.001f * static_cast<float> (*mousedf)
          - (static_cast<float> (nkhz_center) - 48.0f - 1.27046f);
      fallback.indx = static_cast<int> (std::lround ((1000.0f * fallback.f) / (96000.0f / kNfft))) + 1;
      fallback.indx = clamp_int (fallback.indx, 1, kNfft);
      int const ia = std::max (1, fallback.indx - 1);
      int const ib = std::min (kNfft - 1, fallback.indx + 1);
      std::array<float, 4> savg_med {};
      int const npol = xpol ? 4 : 1;
      for (int pol = 1; pol <= npol; ++pol)
        {
          std::vector<float> span;
          span.reserve (static_cast<std::size_t> (ib - ia + 1));
          for (int col = ia; col <= ib; ++col)
            {
              span.push_back (savg[savg_index (pol, col)]);
            }
          savg_med[static_cast<std::size_t> (pol - 1)] = percentile_value (span.data (), ib - ia + 1, 50);
        }
      SyncInfo const info = q65_sync_at_bin (ss, savg, savg_med, xpol, *nhsym, fallback.indx);
      fallback.snr = info.ccfmax;
      fallback.xdt = info.xdt;
      fallback.pol = info.pol;
      fallback.ipol = info.ipol;
      decode_q65_candidate (fallback, 1, *fcenter, *nfcal, *nfsample, *mousefqso, *mousedf,
                            *ntol, xpol, mycall_s, mygrid_s, hiscall_s, hisgrid_s, mode_q65,
                            fqso, nkhz_center, *newdat, *nagain, *max_drift, *nutc, *nxant,
                            *ndop00);
    }

  if (*nagain == 1) return;
  double const elapsed = std::chrono::duration<double> (Clock::now () - started).count ();
  if (*nhsym == 280 && elapsed > 3.0) return;

  for (Candidate const& cand : candidates)
    {
      float const freq = cand.f + nkhz_center - 48.0f - 1.27046f;
      int const ikhz = static_cast<int> (std::lround (freq));
      decode_q65_candidate (cand, 0, *fcenter, *nfcal, *nfsample, ikhz, *mousedf, *ntol,
                            xpol, mycall_s, mygrid_s, hiscall_s, hisgrid_s, mode_q65, fqso,
                            nkhz_center, *newdat, *nagain, *max_drift, *nutc, *nxant,
                            *ndop00);
    }
}

extern "C" int ftx_map65_native_last_quick_count_c ()
{
  return g_last_quick_count;
}

extern "C" int ftx_map65_native_last_decode_count_c ()
{
  return g_last_decode_count;
}

extern "C" void ftx_map65_q65_decode_c (float const* ss, float const* savg, int* reset_state,
                                        double* fcenter, int* nutc, int* nfa, int* nfb,
                                        int* nfshift, int* ntol, int* mousedf, int* mousefqso,
                                        int* nagain, int* nfcal, int* nfsample, int* nxant,
                                        int* nxpol, int* nmode, int* max_drift, int* newdat,
                                        int* ndiskdat, int* nhsym, int* ndop00,
                                        char const mycall[12], char const mygrid[6],
                                        char const hiscall[12], char const hisgrid[6],
                                        char const datetime[20])
{
  ftx_map65_native_decode_c (ss, savg, reset_state, fcenter, nutc, nfa, nfb, nfshift, ntol,
                             mousedf, mousefqso, nagain, nfcal, nfsample, nxant, nxpol, nmode,
                             max_drift, newdat, ndiskdat, nhsym, ndop00, mycall, mygrid,
                             hiscall, hisgrid, datetime);
}

extern "C" int ftx_map65_q65_last_quick_count_c ()
{
  return ftx_map65_native_last_quick_count_c ();
}

extern "C" int ftx_map65_q65_last_decode_count_c ()
{
  return ftx_map65_native_last_decode_count_c ();
}
