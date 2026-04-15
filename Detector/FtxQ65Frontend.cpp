#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

#include "Detector/FftCompat.hpp"

extern "C"
{
void fftbig_ (float dd[], int* nmax);
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

extern std::complex<float> cacb_[5376000];

struct QmapDecodesCommon
{
  int ndecodes;
  int ncand2;
  int nQDecoderDone;
  int nWDecoderBusy;
  int nWTransmitting;
  int kHzRequested;
  char result[50][72];
};

struct QmapDecodes2Common
{
  char result2[50][8];
};

struct QmapDatcomCommon
{
  float d4[2 * 5760000];
  float ss[400 * 32768];
  float savg[32768];
  double fcenter;
  int nutc;
  float fselected;
  int mousedf;
  int mousefqso;
  int nagain;
  int ndepth;
  int ndiskdat;
  int ntx60;
  int newdat;
  int nfa;
  int nfb;
  int nfcal;
  int nfshift;
  int ntx30a;
  int ntx30b;
  int ntol;
  int n60;
  int junk4;
  int nfsample;
  int ndop58;
  int nBaseSubmode;
  int ndop00;
  int nsave;
  int max_drift;
  int offset;
  int nhsym;
  char mycall[12];
  char mygrid[6];
  char hiscall[12];
  char hisgrid[6];
  char datetime[20];
  int junk1;
  int junk2;
  bool bAlso30;
};

struct QmapSavecomCommon
{
  char revision[22];
  char saveFileName[120];
};

extern QmapDecodesCommon decodes_;
extern QmapDecodes2Common decodes2_;
extern QmapDatcomCommon& datcom2_;
extern QmapSavecomCommon savecom_;

void ftx_q65_getcand2_c (float const ss[400 * 32768], float savg0[32768],
                         int* nts_q65, int* nagain, int* nhsym,
                         int* ntx30a, int* ntx30b, int* ntol,
                         float* f0_selected, int* balso30,
                         float cand_snr[50], float cand_f[50], float cand_xdt[50],
                         int cand_ntrperiod[50], int cand_iseq[50], int* ncand2);
void ftx_qmap_q65b_c (int* nutc, int* nqd, double* fcenter, int* nfcal,
                      int* nfsample, int* ikhz, int* mousedf, int* ntol,
                      int* ntrperiod, int* iseq, char const mycall0[12],
                      char const hiscall0[12], char const hisgrid[6],
                      int* mode_q65, float* f0, float* fqso,
                      int* nkhz_center, int* newdat, int* nagain,
                      int* bclickdecode, int* max_drift, int* offset,
                      int* ndepth, char const datetime[20], int* ncfom,
                      int* ndop00, int* nhsym, int* idec);
}

extern "C"
{
QmapDecodesCommon decodes_ {};
QmapDecodes2Common decodes2_ {};
// Allocazione dinamica: evita 95MB nel .obj (d4+ss sono ~96MB di zeri)
static QmapDatcomCommon* datcom2_heap_ = nullptr;
static QmapDatcomCommon& datcom2_ref_() {
    if (!datcom2_heap_) datcom2_heap_ = new QmapDatcomCommon{};
    return *datcom2_heap_;
}
QmapDatcomCommon& datcom2_ = datcom2_ref_();
QmapSavecomCommon savecom_ {};
}

namespace
{
constexpr int kQmapRows {400};
constexpr int kQmapNfft {32768};
constexpr int kQmapNsmax {60 * 96000};
constexpr int kQmapLagMax {33};
constexpr int kQmapMaxCandidates {50};
constexpr int kQmapMaxFft1 {5376000};
constexpr int kQmapMaxFft2 {336000};
constexpr int kQmapWaveSamples {60 * 12000};

inline std::size_t qmap_index (int row1, int column1)
{
  return static_cast<std::size_t> (row1 - 1)
      + static_cast<std::size_t> (kQmapRows) * static_cast<std::size_t> (column1 - 1);
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
      static_cast<std::size_t> (std::floor ((static_cast<float> (pct) / 100.0f)
                                            * static_cast<float> (sorted.size () - 1))));
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

template <std::size_t N> void to_fixed (std::string const& src, char (&dest)[N])
{
  std::fill_n (dest, N, ' ');
  std::size_t const count = std::min (src.size (), N);
  std::memcpy (dest, src.data (), count);
}

template <std::size_t N> void to_fixed (std::string const& src, std::array<char, N>& dest)
{
  std::fill (dest.begin (), dest.end (), ' ');
  std::size_t const count = std::min (src.size (), N);
  std::memcpy (dest.data (), src.data (), count);
}

template <std::size_t N> std::string from_fixed (char const (&src)[N])
{
  return trim_fixed (src, N);
}

void store_c_string (char* dest, std::size_t width, std::string const& value)
{
  if (!dest || width == 0)
    {
      return;
    }
  std::fill_n (dest, width, ' ');
  std::size_t const count = std::min (value.size (), width);
  std::memcpy (dest, value.data (), count);
  if (count < width)
    {
      dest[count] = '\0';
    }
}

void write_qmap_debug_wav (std::vector<short> const& iwave, int nagain)
{
  static int ifile = 0;
  ++ifile;

  char name[32];
  std::snprintf (name, sizeof (name), "000000_%06d.wav", ifile);

  int sample_rate = 12000;
  int start = 0;
  int frames = 60 * 12000;
  if (nagain == 3 || nagain == 4)
    {
      frames = 30 * 12000;
    }
  if (nagain == 4)
    {
      start = 30 * 12000;
    }

  std::ofstream out {name, std::ios::binary};
  if (!out)
    {
      return;
    }

  int bytes_per_sample = 2;
  int channels = 1;
  int data_bytes = frames * bytes_per_sample * channels;
  int riff_size = 36 + data_bytes;
  short audio_format = 1;
  short block_align = static_cast<short> (channels * bytes_per_sample);
  int byte_rate = sample_rate * block_align;
  short bits_per_sample = 16;

  out.write ("RIFF", 4);
  out.write (reinterpret_cast<char*> (&riff_size), 4);
  out.write ("WAVE", 4);
  out.write ("fmt ", 4);
  int fmt_size = 16;
  out.write (reinterpret_cast<char*> (&fmt_size), 4);
  out.write (reinterpret_cast<char*> (&audio_format), 2);
  out.write (reinterpret_cast<char*> (&channels), 2);
  out.write (reinterpret_cast<char*> (&sample_rate), 4);
  out.write (reinterpret_cast<char*> (&byte_rate), 4);
  out.write (reinterpret_cast<char*> (&block_align), 2);
  out.write (reinterpret_cast<char*> (&bits_per_sample), 2);
  out.write ("data", 4);
  out.write (reinterpret_cast<char*> (&data_bytes), 4);
  out.write (reinterpret_cast<char const*> (iwave.data () + start), data_bytes);
}

bool q65_sync_candidate (float const* ss, int i0, int nts_q65, int ntrperiod, int iseq,
                         float& snr, float& xdt)
{
  int const isync0[22] = {
      1, 9, 12, 13, 15, 22, 23, 26, 27, 33, 35, 38, 46, 50, 55, 60, 62, 66, 69, 74, 76, 85};
  int isync[22] {};
  float ccf[kQmapLagMax + 1] {};

  float const tstep = 0.15f;
  int nfac = 4;
  if (ntrperiod == 30)
    {
      nfac = 2;
    }
  for (int i = 0; i < 22; ++i)
    {
      isync[i] = nfac * (isync0[i] - 1) + 1;
    }

  int m = nts_q65 / 2;
  if (ntrperiod == 30)
    {
      m = nts_q65 / 4;
    }
  int const i1 = std::max (1, i0 - m);
  int const i2 = std::min (kQmapNfft, i0 + m);

  for (int lag = 0; lag <= kQmapLagMax; ++lag)
    {
      for (int j = 0; j < 22; ++j)
        {
          int const k = isync[j] + lag + iseq * 200;
          if (k >= 400)
            {
              continue;
            }

          if (ntrperiod == 60)
            {
              for (int freq = i1; freq <= i2; ++freq)
                {
                  ccf[lag] += ss[qmap_index (k, freq)];
                  ccf[lag] += ss[qmap_index (k + 1, freq)];
                  ccf[lag] += ss[qmap_index (k + 2, freq)];
                  ccf[lag] += ss[qmap_index (k + 3, freq)];
                }
            }
          else
            {
              for (int freq = i1; freq <= i2; ++freq)
                {
                  ccf[lag] += ss[qmap_index (k, freq)];
                  ccf[lag] += ss[qmap_index (k + 1, freq)];
                }
            }
        }
    }

  float ccfmax = ccf[0];
  int lagbest = 0;
  for (int i = 1; i <= kQmapLagMax; ++i)
    {
      if (ccf[i] > ccfmax)
        {
          ccfmax = ccf[i];
          lagbest = i;
        }
    }
  xdt = lagbest * tstep - 1.0f;

  float xsum = 0.0f;
  float sq = 0.0f;
  int nsum = 0;
  for (int i = 0; i <= kQmapLagMax; ++i)
    {
      if (std::abs (i - lagbest) <= 2)
        {
          continue;
        }
      xsum += ccf[i];
      sq += ccf[i] * ccf[i];
      ++nsum;
    }
  if (nsum <= 0)
    {
      snr = 0.0f;
      return false;
    }

  float const ave = xsum / static_cast<float> (nsum);
  float const variance = sq / static_cast<float> (nsum) - ave * ave;
  if (variance <= 0.0f)
    {
      snr = 0.0f;
      return false;
    }

  float const rms = std::sqrt (variance);
  snr = (ccfmax - ave) / rms;
  return snr >= 5.0f;
}

void append_qmap_log_line (std::string const& datetime, std::string const& payload)
{
  std::ofstream out {"all_qmap.txt", std::ios::app};
  if (!out)
    {
      return;
    }
  out << datetime.substr (0, std::min<std::size_t> (13, datetime.size ())) << ' '
      << payload << '\n';
}

float qmap_db (float value)
{
  return value > 0.0f ? 10.0f * std::log10 (value) : -99.0f;
}

bool any_nonzero (float const* values, std::size_t count)
{
  for (std::size_t i = 0; i < count; ++i)
    {
      if (values[i] != 0.0f)
        {
          return true;
        }
    }
  return false;
}

void qmap_chkstat (float const* dd, float pdb[4])
{
  static constexpr int kIa[4] = {23 * 96000, 27 * 96000, 53 * 96000, 57 * 96000};
  static constexpr int kIb[4] = {24 * 96000, 28 * 96000, 54 * 96000, 58 * 96000};

  for (int section = 0; section < 4; ++section)
    {
      double sq = 0.0;
      for (int sample = kIa[section]; sample < kIb[section]; ++sample)
        {
          float const i = dd[2 * static_cast<std::size_t> (sample)];
          float const q = dd[2 * static_cast<std::size_t> (sample) + 1];
          sq += i * i + q * q;
        }
      pdb[section] = qmap_db (1.0f + static_cast<float> (sq / (2.0 * 96000.0)));
    }
}

void qmap_zero_tx_half (int row_from, int row_to)
{
  for (int column = 1; column <= kQmapNfft; ++column)
    {
      float sum = 0.0f;
      for (int row = 1; row <= kQmapRows; ++row)
        {
          std::size_t const idx = qmap_index (row, column);
          if (row >= row_from && row <= row_to)
            {
              datcom2_.ss[idx] = 0.0f;
            }
          else
            {
              sum += datcom2_.ss[idx];
            }
        }
      datcom2_.savg[static_cast<std::size_t> (column - 1)] = sum;
    }
}

void qmap_save_qm ()
{
  int ia = 1;
  int ib = kQmapNsmax;
  if (datcom2_.ntx30a > 5)
    {
      ia = kQmapNsmax / 2 + 1;
    }
  if (datcom2_.ntx30b > 5)
    {
      ib = kQmapNsmax / 2;
    }

  double sq = 0.0;
  for (int sample = ia - 1; sample < ib; ++sample)
    {
      float const x = datcom2_.d4[2 * static_cast<std::size_t> (sample)];
      float const y = datcom2_.d4[2 * static_cast<std::size_t> (sample) + 1];
      sq += x * x + y * y;
    }
  int const nsum = 2 * (ib - ia + 1);
  float const rms = nsum > 0 ? std::sqrt (static_cast<float> (sq / nsum)) : 1.0f;
  float const fac0 = rms > 0.0f ? 10.0f / rms : 0.0f;

  std::vector<std::int8_t> id1 (2 * static_cast<std::size_t> (kQmapNsmax), 0);
  for (int sample = ia - 1; sample < ib; ++sample)
    {
      float x = fac0 * datcom2_.d4[2 * static_cast<std::size_t> (sample)];
      float y = fac0 * datcom2_.d4[2 * static_cast<std::size_t> (sample) + 1];
      if (std::fabs (x) > 127.0f)
        {
          x = 0.0f;
        }
      if (std::fabs (y) > 127.0f)
        {
          y = 0.0f;
        }
      id1[2 * static_cast<std::size_t> (sample)] = static_cast<std::int8_t> (std::lround (x));
      id1[2 * static_cast<std::size_t> (sample) + 1]
          = static_cast<std::int8_t> (std::lround (y));
    }

  std::string const fname = trim_fixed (savecom_.saveFileName, sizeof (savecom_.saveFileName));
  if (fname.empty ())
    {
      return;
    }

  std::ofstream out {fname, std::ios::binary};
  if (!out)
    {
      return;
    }

  std::array<char, 24> revision {};
  std::fill (revision.begin (), revision.end (), ' ');
  std::memcpy (revision.data (), savecom_.revision,
               std::min<std::size_t> (22, revision.size ()));
  std::array<int, 15> nxtra {};

  out.write (revision.data (), revision.size ());
  out.write (datcom2_.mycall, sizeof (datcom2_.mycall));
  out.write (datcom2_.mygrid, sizeof (datcom2_.mygrid));
  out.write (reinterpret_cast<char const*> (&datcom2_.fcenter), sizeof (datcom2_.fcenter));
  out.write (reinterpret_cast<char const*> (&datcom2_.nutc), sizeof (datcom2_.nutc));
  out.write (reinterpret_cast<char const*> (&datcom2_.ntx30a), sizeof (datcom2_.ntx30a));
  out.write (reinterpret_cast<char const*> (&datcom2_.ntx30b), sizeof (datcom2_.ntx30b));
  out.write (reinterpret_cast<char const*> (&datcom2_.ndop00), sizeof (datcom2_.ndop00));
  out.write (reinterpret_cast<char const*> (&datcom2_.ndop58), sizeof (datcom2_.ndop58));
  out.write (reinterpret_cast<char const*> (&ia), sizeof (ia));
  out.write (reinterpret_cast<char const*> (&ib), sizeof (ib));
  out.write (reinterpret_cast<char const*> (&fac0), sizeof (fac0));
  out.write (reinterpret_cast<char const*> (nxtra.data ()), sizeof (nxtra));
  out.write (reinterpret_cast<char const*> (id1.data () + 2 * static_cast<std::size_t> (ia - 1)),
             2 * static_cast<std::size_t> (ib - ia + 1));
}

void qmap_decode0_native ()
{
  datcom2_.newdat = 1;
  datcom2_.ndepth = std::max (datcom2_.ndepth, datcom2_.nagain >= 1 ? 3 : datcom2_.ndepth);
  decodes_.nQDecoderDone = 0;

  if (!any_nonzero (datcom2_.d4, 2 * static_cast<std::size_t> (kQmapNsmax))
      || !any_nonzero (datcom2_.ss, static_cast<std::size_t> (kQmapRows) * kQmapNfft)
      || !any_nonzero (datcom2_.savg, kQmapNfft))
    {
      return;
    }

  int nkhz_center = static_cast<int> (
      std::lround (1000.0 * (datcom2_.fcenter - std::floor (datcom2_.fcenter))));
  int mode_q65 = datcom2_.nBaseSubmode;
  int nts_q65 = 1 << std::max (0, mode_q65 - 1);
  float f0_selected = datcom2_.fselected - nkhz_center + 48.0f;

  struct Candidate
  {
    float snr {};
    float f {};
    float xdt {};
    int ntrperiod {};
    int iseq {};
  };

  std::vector<Candidate> candidates;
  if (datcom2_.nagain <= 1)
    {
      std::array<float, kQmapMaxCandidates> cand_snr {};
      std::array<float, kQmapMaxCandidates> cand_f {};
      std::array<float, kQmapMaxCandidates> cand_xdt {};
      std::array<int, kQmapMaxCandidates> cand_ntrperiod {};
      std::array<int, kQmapMaxCandidates> cand_iseq {};
      int balso30 = datcom2_.bAlso30 ? 1 : 0;
      int ncand2 = 0;
      ftx_q65_getcand2_c (datcom2_.ss, datcom2_.savg, &nts_q65, &datcom2_.nagain, &datcom2_.nhsym,
                          &datcom2_.ntx30a, &datcom2_.ntx30b, &datcom2_.ntol, &f0_selected,
                          &balso30, cand_snr.data (), cand_f.data (), cand_xdt.data (),
                          cand_ntrperiod.data (), cand_iseq.data (), &ncand2);
      decodes_.ncand2 = ncand2;
      for (int i = 0; i < ncand2; ++i)
        {
          Candidate candidate;
          candidate.snr = cand_snr[static_cast<std::size_t> (i)];
          candidate.f = cand_f[static_cast<std::size_t> (i)];
          candidate.xdt = cand_xdt[static_cast<std::size_t> (i)];
          candidate.ntrperiod = cand_ntrperiod[static_cast<std::size_t> (i)];
          candidate.iseq = cand_iseq[static_cast<std::size_t> (i)];
          candidates.push_back (candidate);
        }
    }

  int nmax = kQmapNsmax;
  fftbig_ (datcom2_.d4, &nmax);

  float const foffset = 0.001f * static_cast<float> (1270 + datcom2_.nfcal);
  float fqso = static_cast<float> (datcom2_.mousefqso) + foffset
      - 0.5f * static_cast<float> (datcom2_.nfa + datcom2_.nfb)
      + static_cast<float> (datcom2_.nfshift);
  int nqd = 0;
  bool const click_decode = datcom2_.nagain >= 1;

  if (datcom2_.nagain >= 2)
    {
      candidates.clear ();
      Candidate candidate;
      candidate.snr = 0.0f;
      candidate.f = f0_selected;
      candidate.xdt = 0.0f;
      candidate.ntrperiod = datcom2_.nagain == 2 ? 60 : 30;
      candidate.iseq = datcom2_.nagain == 4 ? 1 : 0;
      candidates.push_back (candidate);
      decodes_.ncand2 = 1;
      fqso = datcom2_.fselected;
    }

  using Clock = std::chrono::steady_clock;
  Clock::time_point const started = Clock::now ();
  std::vector<std::tuple<float, int, int>> found;
  for (Candidate const& cand : candidates)
    {
      if (datcom2_.ndiskdat == 0)
        {
          double const elapsed
              = std::chrono::duration<double> (Clock::now () - started).count ();
          if ((datcom2_.nhsym == 130 && elapsed > 6.0)
              || (datcom2_.nhsym == 200 && elapsed > 10.0)
              || (datcom2_.nhsym == 330 && elapsed > 6.0)
              || (datcom2_.nhsym == 390 && elapsed > 16.0))
            {
              break;
            }
        }

      if (datcom2_.nagain == 0)
        {
          bool already_done = false;
          for (auto const& item : found)
            {
              if (std::fabs (cand.f - std::get<0> (item)) < 0.005f
                  && cand.ntrperiod == std::get<1> (item)
                  && cand.iseq == std::get<2> (item))
                {
                  already_done = true;
                  break;
                }
            }
          if (already_done)
            {
              continue;
            }
        }

      int mode_q65_tmp = mode_q65;
      if (cand.ntrperiod == 30)
        {
          mode_q65_tmp = std::max (1, mode_q65 - 1);
        }
      float freq = cand.f + nkhz_center - 48.0f - 1.27046f;
      int ikhz = static_cast<int> (std::lround (freq));
      int idec = -1;
      int bclickdecode = click_decode ? 1 : 0;
      float f0 = cand.f;
      int ntrperiod = cand.ntrperiod;
      int iseq = cand.iseq;
      int ncfom = 0;
      ftx_qmap_q65b_c (&datcom2_.nutc, &nqd, &datcom2_.fcenter, &datcom2_.nfcal,
                       &datcom2_.nfsample, &ikhz, &datcom2_.mousedf, &datcom2_.ntol,
                       &ntrperiod, &iseq, datcom2_.mycall, datcom2_.hiscall,
                       datcom2_.hisgrid, &mode_q65_tmp, &f0, &fqso, &nkhz_center,
                       &datcom2_.newdat, &datcom2_.nagain, &bclickdecode,
                       &datcom2_.max_drift, &datcom2_.offset, &datcom2_.ndepth,
                       datcom2_.datetime, &ncfom, &datcom2_.ndop00, &datcom2_.nhsym, &idec);
      if (click_decode && idec >= 0)
        {
          break;
        }
      if (idec >= 0)
        {
          found.emplace_back (cand.f, cand.ntrperiod, cand.iseq);
        }
    }
}
}

extern "C" void ftx_q65_getcand2_c (float const ss[400 * 32768], float savg0[32768],
                                    int* nts_q65, int* nagain, int* nhsym,
                                    int* ntx30a, int* ntx30b, int* ntol,
                                    float* f0_selected, int* balso30,
                                    float cand_snr[kQmapMaxCandidates],
                                    float cand_f[kQmapMaxCandidates],
                                    float cand_xdt[kQmapMaxCandidates],
                                    int cand_ntrperiod[kQmapMaxCandidates],
                                    int cand_iseq[kQmapMaxCandidates], int* ncand2)
{
  if (ncand2)
    {
      *ncand2 = 0;
    }
  if (!ss || !savg0 || !nts_q65 || !nagain || !nhsym || !ntx30a || !ntx30b || !ntol
      || !f0_selected || !balso30 || !cand_snr || !cand_f || !cand_xdt
      || !cand_ntrperiod || !cand_iseq || !ncand2)
    {
      return;
    }

  std::fill_n (cand_snr, kQmapMaxCandidates, 0.0f);
  std::fill_n (cand_f, kQmapMaxCandidates, 0.0f);
  std::fill_n (cand_xdt, kQmapMaxCandidates, 0.0f);
  std::fill_n (cand_ntrperiod, kQmapMaxCandidates, 0);
  std::fill_n (cand_iseq, kQmapMaxCandidates, 0);

  if (savg0[kQmapNfft - 1] == 0.0f)
    {
      return;
    }

  std::vector<float> savg (savg0, savg0 + kQmapNfft);
  int const nseg = 16;
  int const npct = 40;
  int const nlen = kQmapNfft / nseg;
  for (int iseg = 0; iseg < nseg; ++iseg)
    {
      int const ja = iseg * nlen;
      int const jb = ja + nlen;
      float const base = percentile_value (savg.data () + ja, nlen, npct);
      float const scale = 1.015f * base;
      if (scale <= 0.0f)
        {
          continue;
        }
      for (int idx = ja; idx < jb; ++idx)
        {
          savg[idx] /= scale;
          savg0[idx] /= scale;
        }
    }

  float const df = 96000.0f / static_cast<float> (kQmapNfft);
  float const bw = 65.0f * static_cast<float> (*nts_q65) * 1.666666667f;
  int const nbw = static_cast<int> (bw / df) + 1;
  int const nb0 = 2 * (*nts_q65);
  float const smin = 1.4f;
  int const nguard = 5;
  int i1 = 1;
  int i2 = kQmapNfft - nbw - nguard;
  if (*nagain >= 1)
    {
      i1 = static_cast<int> (std::lround ((1000.0f * (*f0_selected) - *ntol) / df));
      i2 = static_cast<int> (std::lround ((1000.0f * (*f0_selected) + *ntol) / df));
    }
  i1 = std::max (1, i1);
  i2 = std::min (kQmapNfft - nb0, i2);

  int count = 0;
  for (int i = i1; i <= i2; ++i)
    {
      if (savg[static_cast<std::size_t> (i - 1)] < smin)
        {
          continue;
        }

      float spk = savg[static_cast<std::size_t> (i - 1)];
      int ipk = 1;
      for (int j = 1; j <= nb0; ++j)
        {
          float const value = savg[static_cast<std::size_t> (i - 1 + j)];
          if (value > spk)
            {
              spk = value;
              ipk = j + 1;
            }
        }
      (void) spk;

      int const i0 = ipk + i - 1;
      float const fpk = 0.001f * static_cast<float> (i0) * df;

      auto store_candidate = [&] (int ntrperiod, int iseq) -> bool
      {
        float snr_sync = 0.0f;
        float xdt = 0.0f;
        if (!q65_sync_candidate (ss, i0, *nts_q65, ntrperiod, iseq, snr_sync, xdt))
          {
            return false;
          }
        if (count >= kQmapMaxCandidates)
          {
            return true;
          }

        cand_f[count] = fpk;
        cand_xdt[count] = xdt;
        cand_snr[count] = snr_sync;
        cand_ntrperiod[count] = ntrperiod;
        cand_iseq[count] = iseq;
        ++count;

        int const ia = std::max (1, std::min (i, i0 - nguard));
        int const ib = std::min (i0 + nbw + nguard, kQmapNfft);
        for (int idx = ia; idx <= ib; ++idx)
          {
            savg[static_cast<std::size_t> (idx - 1)] = 0.0f;
          }
        return count >= kQmapMaxCandidates;
      };

      if (*nhsym >= 200 && store_candidate (60, 0))
        {
          break;
        }

      if (*balso30 == 0)
        {
          continue;
        }

      if (*nhsym <= 200 && *ntx30a <= 5 && store_candidate (30, 0))
        {
          break;
        }

      if (*nhsym >= 330 && *ntx30b <= 5 && store_candidate (30, 1))
        {
          break;
        }
    }

  *ncand2 = count;
}

extern "C" void ftx_qmap_q65b_c (int* nutc, int* nqd, double* fcenter, int* nfcal,
                                 int* nfsample, int* ikhz, int* mousedf, int* ntol,
                                 int* ntrperiod, int* iseq, char const mycall0[12],
                                 char const hiscall0[12], char const hisgrid[6],
                                 int* mode_q65, float* f0, float* fqso,
                                 int* nkhz_center, int* newdat, int* nagain,
                                 int* bclickdecode, int* max_drift, int* offset,
                                 int* ndepth, char const datetime[20], int* ncfom,
                                 int* ndop00, int* nhsym, int* idec)
{
  static std::string saved_mycall;
  static std::string saved_hiscall;

  if (idec)
    {
      *idec = -1;
    }
  if (!nutc || !nqd || !fcenter || !nfcal || !nfsample || !ikhz || !mousedf || !ntol
      || !ntrperiod || !iseq || !mycall0 || !hiscall0 || !hisgrid || !mode_q65 || !f0
      || !fqso || !nkhz_center || !newdat || !nagain || !bclickdecode || !max_drift
      || !offset || !ndepth || !datetime || !ncfom || !ndop00 || !nhsym || !idec)
    {
      return;
    }

  (void) mousedf;
  (void) nfsample;
  (void) ncfom;

  std::string const mycall_in = trim_fixed (mycall0, 12);
  std::string const hiscall_in = trim_fixed (hiscall0, 12);
  std::string const hisgrid_in = trim_fixed (hisgrid, 6);
  if (!mycall_in.empty ())
    {
      saved_mycall = mycall_in;
    }
  if (!hiscall_in.empty ())
    {
      saved_hiscall = hiscall_in;
    }

  float const df3 = 96000.0f / static_cast<float> (kQmapNfft);
  int ipk = static_cast<int> ((1000.0f * (*f0) - 1.0f) / df3);
  if (*nagain >= 2)
    {
      ipk = static_cast<int> (std::lround (1000.0f * (*fqso - *nkhz_center + 48.0f) / df3));
    }

  int const nfft1 = kQmapMaxFft1;
  int const nfft2 = kQmapMaxFft2;
  float const df = 96000.0f / static_cast<float> (nfft1);
  int const nh = nfft2 / 2;
  int const k0 = static_cast<int> (std::lround ((ipk * df3 - 1000.0f) / df));
  if (k0 < nh || k0 > kQmapMaxFft1 - nfft2 + 1)
    {
      return;
    }

  std::vector<std::complex<float>> cz (static_cast<std::size_t> (nfft2 + 1), {0.0f, 0.0f});
  float const fac = 1.0f / static_cast<float> (nfft2);
  for (int i = 0; i < nfft2; ++i)
    {
      cz[static_cast<std::size_t> (i)] = fac * cacb_[static_cast<std::size_t> (k0 - 1 + i)];
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

  int const nfft_arg = 2 * nfft2;
  decodium::fft_compat::inverse_real (cz, nfft_arg);

  std::vector<short> iwave (kQmapWaveSamples, 0);
  for (int i = 0; i < nfft2; ++i)
    {
      int const j = nfft2 - 1 - i;
      iwave[static_cast<std::size_t> (2 * i + 1)] =
          static_cast<short> (std::lround (std::real (cz[static_cast<std::size_t> (j)])));
      iwave[static_cast<std::size_t> (2 * i)] =
          static_cast<short> (std::lround (std::imag (cz[static_cast<std::size_t> (j)])));
    }

  if (*iseq == 1)
    {
      std::copy (iwave.begin () + 360000, iwave.begin () + 720000, iwave.begin ());
    }

  int nsubmode = *mode_q65 - 1;
  int nfa = 990;
  int nfb = 1010;
  if (*nagain >= 1)
    {
      nfa = std::max (100, 1000 - *ntol);
      nfb = std::min (2500, 1000 + *ntol);
    }

  int nsnr0 = -99;
  int nfreq0 = 0;
  float xdt0 = 0.0f;
  std::array<char, 37> msg0 {};
  std::array<char, 3> cq0 {};

  if (*nagain >= 2)
    {
      write_qmap_debug_wav (iwave, *nagain);
      return;
    }

  std::array<char, 12> mycall_c {};
  std::array<char, 12> hiscall_c {};
  std::array<char, 6> hisgrid_c {};
  to_fixed (saved_mycall, mycall_c);
  to_fixed (saved_hiscall, hiscall_c);
  to_fixed (hisgrid_in, hisgrid_c);

  int nqd_c = *nqd;
  int nutc_c = *nutc;
  if (*ntrperiod == 30)
    {
      nutc_c = 100 * *nutc + *iseq * 30;
    }
  int ntrperiod_c = *ntrperiod;
  int nsubmode_c = nsubmode;
  int nfqso_c = 1000;
  int ntol_c = *ntol;
  int ndepth_c = *ndepth;
  int nfa_c = nfa;
  int nfb_c = nfb;
  int nclearave_c = 0;
  int single_decode_c = 0;
  int nagain_c = 0;
  int max_drift_c = *max_drift;
  int lnewdat_c = *newdat;
  float emedelay_c = 2.5f;
  int nqsoprogress_c = 0;
  int ncontest_c = 0;
  int lapcqonly_c = 0;
  int found_c = 0;
  ftx_q65_async_decode_latest_c_api (
      iwave.data (), &nqd_c, &nutc_c, &ntrperiod_c, &nsubmode_c, &nfqso_c, &ntol_c,
      &ndepth_c, &nfa_c, &nfb_c, &nclearave_c, &single_decode_c, &nagain_c,
      &max_drift_c, &lnewdat_c, &emedelay_c, mycall_c.data (), hiscall_c.data (),
      hisgrid_c.data (), &nqsoprogress_c, &ncontest_c, &lapcqonly_c, &nsnr0, &xdt0,
      &nfreq0, cq0.data (), msg0.data (), &found_c);
  if (found_c == 0 || nsnr0 <= -99)
    {
      return;
    }

  std::string const msg = trim_fixed (msg0.data (), msg0.size ());
  for (int i = 0; i < decodes_.ndecodes; ++i)
    {
      std::string const existing = trim_fixed (decodes_.result[i], 72);
      if (existing.size () >= 41 && existing.find (msg, 41) != std::string::npos
          && (!*bclickdecode || *nhsym == 390))
        {
          *idec = 0;
          return;
        }
    }

  int nq65df = static_cast<int> (std::lround (
      1000.0f * (0.001f * k0 * df + *nkhz_center - 48.0f + 1.000f - 1.27046f - *ikhz)))
      - *nfcal;
  nq65df += nfreq0 - 1000;
  int ikhz1 = *ikhz;
  int ndf = nq65df;
  if (ndf > 500)
    {
      ikhz1 = *ikhz + (nq65df + 500) / 1000;
    }
  if (ndf < -500)
    {
      ikhz1 = *ikhz + (nq65df - 500) / 1000;
    }
  ndf = nq65df - 1000 * (ikhz1 - *ikhz);

  float const frx = 0.001f * k0 * df + *nkhz_center - 48.0f + 1.0f - 0.001f * *nfcal;
  float const fsked = frx - 0.001f * *ndop00 / 2.0f - 0.001f * *offset;

  char csubmode[4];
  std::snprintf (csubmode, sizeof (csubmode), "%02d%c", *ntrperiod, 'A' + nsubmode);
  char result[96];
  std::snprintf (result, sizeof (result), "%06d%9.3f%7.1f%7.2f%5d  %s  %s",
                 nutc_c, frx, fsked, xdt0, nsnr0, csubmode, msg.c_str ());

  char result2[16];
  std::snprintf (result2, sizeof (result2), "%7.3f", fsked);

  int const slot = std::min (decodes_.ndecodes, 49);
  decodes_.ndecodes = slot + 1;
  store_c_string (decodes_.result[slot], 72, result);
  store_c_string (decodes2_.result2[slot], 8, result2);

  std::string datetime1 = trim_fixed (datetime, 20);
  if (datetime1.size () < 20)
    {
      datetime1.resize (20, ' ');
    }
  datetime1.replace (11, 2, (*ntrperiod == 30 && *iseq == 1) ? "30" : "00");
  std::string const payload = trim_fixed (decodes_.result[slot], 72).substr (6);
  append_qmap_log_line (datetime1, payload);
  *idec = 0;
}

extern "C" void q65c_ ()
{
  if (sizeof (datcom2_.datetime) >= 12)
    {
      for (std::size_t i = 11; i < sizeof (datcom2_.datetime); ++i)
        {
          datcom2_.datetime[i] = ' ';
        }
      datcom2_.datetime[11] = '0';
      if (sizeof (datcom2_.datetime) >= 13)
        {
          datcom2_.datetime[12] = '0';
        }
    }

  if (datcom2_.ndiskdat == 1)
    {
      float pdb[4] {};
      qmap_chkstat (datcom2_.d4, pdb);
      if ((std::fabs (pdb[0] - pdb[1]) > 3.0f && pdb[0] > 1.0f) || pdb[0] < 1.0f)
        {
          datcom2_.ntx30a = 20;
        }
      if ((std::fabs (pdb[2] - pdb[3]) > 3.0f && pdb[2] > 1.0f) || pdb[2] < 1.0f)
        {
          datcom2_.ntx30b = 20;
        }
      if (pdb[3] < 0.04f)
        {
          datcom2_.ntx30a = 0;
          datcom2_.ntx30b = 0;
        }
    }

  if (datcom2_.ntx30a > 5)
    {
      std::fill_n (datcom2_.d4, 2 * 30 * 96000, 0.0f);
      qmap_zero_tx_half (1, 200);
    }
  if (datcom2_.ntx30b > 5)
    {
      std::fill_n (datcom2_.d4 + 2 * 30 * 96000, 2 * 30 * 96000, 0.0f);
      qmap_zero_tx_half (201, 400);
    }

  if (!(datcom2_.nhsym > 200 && datcom2_.ntx30b > 5))
    {
      qmap_decode0_native ();
    }

  if (datcom2_.ndiskdat == 0 && datcom2_.nhsym == 390
      && (datcom2_.nsave == 2 || (datcom2_.nsave == 1 && decodes_.ndecodes >= 1)))
    {
      qmap_save_qm ();
    }
}

extern "C" void all_done_ () {}
