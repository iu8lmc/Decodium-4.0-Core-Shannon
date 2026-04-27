// -*- Mode: C++ -*-
#include "wsjtx_config.h"
#include "commons.h"
#include "Detector/FftCompat.hpp"
#include "Detector/FtxQ65Core.hpp"
#include "Detector/FtxQ65Decoder.hpp"
#include "Detector/LegacyDspIoHelpers.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <fftw3.h>

#include <QByteArray>
#include <QMutexLocker>
#include <QString>

#include "Detector/FortranRuntimeGuard.hpp"

extern "C"
{
  void ftx_prepare_ft8_ap_c (char const mycall[12], char const hiscall[12], int ncontest,
                             int apsym[58], int aph10[10]);
  void ftx_q65_set_list_c (char const mycall[12], char const hiscall[12],
                           char const hisgrid[6], int codewords[63 * 206], int* ncw);
  void ftx_q65_set_list2_c (char const mycall[12], char const hiscall[12],
                            char const hisgrid[6], char const caller_calls[6 * 40],
                            char const caller_grids[4 * 40], int* nhist2,
                            int codewords[63 * 206], int* ncw);
  void ftx_q65_ap_c (int* nqsoprogress, int* ipass, int* ncontest, int* lapcqonly,
                     int const apsym0[58], int const aph10[10], int apmask[78],
                     int apsymbols[78], int* iaptype);
  void ftx_q65_loops_c (std::complex<float> const c00[], int* npts2, int* nsps2,
                        int* nsubmode, int* ndepth, int* jpk0, float* xdt0,
                        float* f0, int* iaptype, int* mode_q65, int* ibwa,
                        int* ibwb, float* drift, int apmask[13], int apsymbols[13],
                        int* maxiters, float* xdt1, float* f1, float* snr2,
                        int dat4[13], int* idec, int* idfbest, int* idtbest,
                        int* ndistbest, int* ibwbest, int* nrc);
  void ftx_q65_dec_q3_c (float const s1[], int* iz, int* jz, int* ipk, int* jpk,
                         int* ll, int* mode_q65, int* nsps, float const sync[85],
                         int* ibwa, int* ibwb, int codewords[], int* ncw,
                         float* snr2, int dat4[13], int* idec, char decoded[37],
                         int* ibwbest, int* nrc, float* plog);
  void ftx_q65_dec_q012_c (float s3[], int* ll, int* mode_q65, int* nsps,
                           int* npasses, int apsym0[58], int aph10[10],
                           int* ibwa, int* ibwb, int* maxiters, float* snr2,
                           int dat4[13], int* idec, char decoded[37],
                           int* ibwbest, int* nrc);
  void ftx_q65_s1_to_s3_c (float const s1[], int* iz, int* jz, int* i0, int* j0,
                           int* ipk, int* jpk, int* ll, int* mode_q65,
                           float const sync[85], float s3[]);
  void ftx_q65_sync_curve_c (float ccf1[], int* count, float* rms1);
  void legacy_pack77_unpack_c (char const c77[77], int received, char msgsent[37],
                               bool* success);
}

namespace
{
  using decodium::fft_compat::forward_complex;
  constexpr int kQ65MaxLines {200};
  constexpr int kQ65Bits {77};
  constexpr int kQ65DecodedChars {37};
  constexpr int kQ65PayloadSymbols {13};
  constexpr int kQ65CodewordSymbols {63};
  constexpr int kQ65MaxCodewords {206};
  constexpr int kQ65PayloadColumns {63};
  constexpr int kQ65SyncSymbols {85};
  constexpr int kQ65Step {8};
  constexpr int kQ65ApBits {78};
  constexpr int kQ65ApPacked {13};
  constexpr int kQ65MaxCallers {40};
  constexpr int kQ65MaxHistory {100};
  constexpr int kTempPathChars {500};
  constexpr float kDbFloor {1.259e-10f};

  struct Ana64Workspace
  {
    ~Ana64Workspace ()
    {
      if (forward)
        {
          fftwf_destroy_plan (forward);
        }
      if (inverse)
        {
          fftwf_destroy_plan (inverse);
        }
    }

    void ensure (int npts)
    {
      if (npts == nfft1)
        {
          return;
        }
      if (forward)
        {
          fftwf_destroy_plan (forward);
          forward = nullptr;
        }
      if (inverse)
        {
          fftwf_destroy_plan (inverse);
          inverse = nullptr;
        }
      nfft1 = npts;
      nfft2 = npts / 2;
      buffer.assign (static_cast<size_t> (std::max (1, npts)), std::complex<float> {});
      auto* base = reinterpret_cast<fftwf_complex*> (buffer.data ());
      forward = fftwf_plan_dft_1d (nfft1, base, base, FFTW_FORWARD, FFTW_ESTIMATE);
      inverse = fftwf_plan_dft_1d (nfft2, base, base, FFTW_BACKWARD, FFTW_ESTIMATE);
    }

    int nfft1 {0};
    int nfft2 {0};
    std::vector<std::complex<float>> buffer;
    fftwf_plan forward {nullptr};
    fftwf_plan inverse {nullptr};
  };

  Ana64Workspace& ana64_workspace ()
  {
    static thread_local Ana64Workspace workspace;
    return workspace;
  }

  bool ana64_native (short const* iwave, int npts, std::vector<std::complex<float>>& c0)
  {
    if (!iwave || npts <= 0)
      {
        return false;
      }

    Ana64Workspace& workspace = ana64_workspace ();
    workspace.ensure (npts);
    int const nfft1 = workspace.nfft1;
    int const nfft2 = workspace.nfft2;
    float const fac = 2.0f / (32767.0f * static_cast<float> (nfft1));

    std::fill (workspace.buffer.begin (), workspace.buffer.end (), std::complex<float> {});
    for (int i = 0; i < npts; ++i)
      {
        workspace.buffer[static_cast<size_t> (i)] = {fac * static_cast<float> (iwave[i]), 0.0f};
      }

    fftwf_execute (workspace.forward);
    for (int i = nfft2 / 2 + 1; i < nfft2; ++i)
      {
        workspace.buffer[static_cast<size_t> (i)] = {};
      }
    workspace.buffer[0] *= 0.5f;
    fftwf_execute (workspace.inverse);

    if (static_cast<int> (c0.size ()) < nfft2)
      {
        c0.resize (static_cast<size_t> (nfft2));
      }
    std::copy_n (workspace.buffer.begin (), nfft2, c0.begin ());
    return true;
  }

  struct Q65Candidate
  {
    float snr {};
    float xdt {};
    float freq {};
  };

  struct Q65HistEntry
  {
    int freq {};
    std::string message;
  };

  struct Q65CallerEntry
  {
    std::string call;
    std::string grid;
    int nsec {};
    int freq {};
    int moonel {};
  };

  struct Q65Line
  {
    int syncsnr {};
    int snr {};
    float dt {};
    float freq {};
    int idec {-1};
    int nused {};
    std::array<signed char, kQ65Bits> bits {};
    std::string decoded;
  };

  struct Q65Dec0Result
  {
    float xdt {};
    float f0 {};
    float snr1 {};
    float width {};
    float snr2 {};
    float drift {};
    int idec {-1};
    std::array<int, kQ65PayloadSymbols> dat4 {};
    std::array<char, kQ65DecodedChars> decoded {};
    std::vector<Q65Candidate> candidates;
  };

  struct Q65RuntimeState
  {
    bool sync_initialized {false};
    int ll0 {};
    int iz0 {};
    int jz0 {};
    int navg[2] {};
    int last_iz {};
    int last_jz {};
    int last_j0 {};
    int last_mode {};
    float last_df {};
    float last_dtstep {};
    std::array<float, kQ65SyncSymbols> sync {};
    std::array<int, 58> apsym0 {};
    std::array<int, 10> aph10 {};
    std::array<int, kQ65CodewordSymbols * kQ65MaxCodewords> codewords {};
    int ncw {};
    std::vector<float> s1a;
    std::vector<float> last_s1;
    std::vector<Q65HistEntry> history;
    std::vector<Q65CallerEntry> callers;
  };

  QByteArray to_fortran_field (QByteArray value, int width)
  {
    value = value.left (width);
    if (value.size () < width)
      {
        value.append (QByteArray (width - value.size (), ' '));
      }
    return value;
  }

  QByteArray trim_fortran_field (char const* data, int width)
  {
    QByteArray field {data, width};
    while (!field.isEmpty () && (field.back () == ' ' || field.back () == '\0'))
      {
        field.chop (1);
      }
    return field;
  }

  std::string trim_temp_dir (char const* temp_dir)
  {
    if (!temp_dir)
      {
        return {};
      }
    size_t length = 0;
    while (length < static_cast<size_t> (kTempPathChars) && temp_dir[length] != '\0')
      {
        ++length;
      }
    return std::string {temp_dir, temp_dir + length};
  }

  std::string trim_fixed (char const* data, int width)
  {
    return trim_fortran_field (data, width).toStdString ();
  }

  std::string trimmed_decode (char const* decodeds, int index)
  {
    QByteArray decoded {decodeds + index * kQ65DecodedChars, kQ65DecodedChars};
    while (!decoded.isEmpty () && (decoded.back () == ' ' || decoded.back () == '\0'))
      {
        decoded.chop (1);
      }
    return decoded.toStdString ();
  }

  QString format_decode_utc (int nutc)
  {
    if (nutc <= 0)
      {
        return {};
      }
    int const width = nutc > 9999 ? 6 : 4;
    return QString::number (nutc).rightJustified (width, QLatin1Char {'0'});
  }

  std::string format_stdout_line (int nutc, int snr, float dt, float freq,
                                  char const* decodeds, int index,
                                  int idec, int nused)
  {
    QString cflags = QStringLiteral ("   ");
    if (idec >= 0)
      {
        cflags = QStringLiteral ("q  ");
        cflags[1] = QLatin1Char ('0' + std::max (0, std::min (idec, 9)));
        if (nused >= 2)
          {
            cflags[2] = QLatin1Char ('0' + std::max (0, std::min (nused, 9)));
          }
      }

    QString line = QStringLiteral ("%1%2%3 :  %4 %5")
        .arg (snr, 4)
        .arg (dt, 5, 'f', 1)
        .arg (qRound (freq), 5)
        .arg (QString::fromStdString (trimmed_decode (decodeds, index)).leftJustified (
                 kQ65DecodedChars, QLatin1Char {' '}))
        .arg (cflags);
    QString const utc = format_decode_utc (nutc);
    if (!utc.isEmpty ())
      {
        line.prepend (utc);
      }
    return line.toStdString ();
  }

  std::string format_decoded_file_line (int nutc, int ntrperiod,
                                        int syncsnr, int snr, float dt, float freq,
                                        char const* decodeds, int index)
  {
    QString decoded = QString::fromStdString (trimmed_decode (decodeds, index));
    decoded = decoded.leftJustified (kQ65DecodedChars, QLatin1Char {' '});

    QString line;
    if (ntrperiod < 60)
      {
        line = QStringLiteral ("%1%2%3%4%5%6   %7 Q65")
            .arg (nutc, 6, 10, QLatin1Char {'0'})
            .arg (syncsnr, 4)
            .arg (snr, 5)
            .arg (dt, 6, 'f', 1)
            .arg (freq, 8, 'f', 0)
            .arg (0, 4)
            .arg (decoded);
      }
    else
      {
        line = QStringLiteral ("%1%2%3%4%5%6   %7 Q65")
            .arg (nutc, 4, 10, QLatin1Char {'0'})
            .arg (syncsnr, 4)
            .arg (snr, 5)
            .arg (dt, 6, 'f', 1)
            .arg (freq, 8, 'f', 0)
            .arg (0, 4)
            .arg (decoded);
      }
    return line.toStdString ();
  }

  inline int col_major_index (int row, int column, int rows)
  {
    return row + rows * column;
  }

  float db_like (float value)
  {
    return value > kDbFloor ? 10.0f * std::log10 (value) : -99.0f;
  }

  void pack_q65_ap_bits (int const unpacked[kQ65ApBits], int packed[kQ65ApPacked])
  {
    for (int group = 0; group < kQ65ApPacked; ++group)
      {
        int value = 0;
        for (int bit = 0; bit < 6; ++bit)
          {
            value = (value << 1) | (unpacked[group * 6 + bit] != 0 ? 1 : 0);
          }
        packed[group] = value;
      }
  }

  void dat4_to_bits77 (std::array<int, kQ65PayloadSymbols> const& dat4,
                       std::array<signed char, kQ65Bits>& bits)
  {
    int offset = 0;
    for (int i = 0; i < 12; ++i)
      {
        int value = dat4[static_cast<size_t> (i)];
        for (int bit = 5; bit >= 0; --bit)
          {
            bits[static_cast<size_t> (offset++)] = ((value >> bit) & 1) ? 1 : 0;
          }
      }
    int value = dat4[12] / 2;
    for (int bit = 4; bit >= 0; --bit)
      {
        bits[static_cast<size_t> (offset++)] = ((value >> bit) & 1) ? 1 : 0;
      }
  }

  bool is_grid4 (std::string const& grid)
  {
    if (grid.size () < 4)
      {
        return false;
      }
    return grid[0] >= 'A' && grid[0] <= 'R'
        && grid[1] >= 'A' && grid[1] <= 'R'
        && grid[2] >= '0' && grid[2] <= '9'
        && grid[3] >= '0' && grid[3] <= '9'
        && grid.substr (0, 4) != "RR73";
  }

  void initialize_sync (Q65RuntimeState& state)
  {
    if (state.sync_initialized)
      {
        return;
      }
    state.sync.fill (-22.0f / 63.0f);
    int const isync[22] = {1, 9, 12, 13, 15, 22, 23, 26, 27, 33, 35,
                           38, 46, 50, 55, 60, 62, 66, 69, 74, 76, 85};
    for (int sync_index : isync)
      {
        state.sync[static_cast<size_t> (sync_index - 1)] = 1.0f;
      }
    state.sync_initialized = true;
  }

  int q65_nsps_for_period (int ntrperiod)
  {
    if (ntrperiod == 30) return 3600;
    if (ntrperiod == 60) return 7200;
    if (ntrperiod == 120) return 16000;
    if (ntrperiod == 300) return 41472;
    return 1800;
  }

  int q65_sequence (int nutc, int ntrperiod)
  {
    int n = nutc;
    if (ntrperiod >= 60 && nutc <= 2359)
      {
        n *= 100;
      }
    int hh = n / 10000;
    int mm = (n / 100) % 100;
    int ss = n % 100;
    int nsec = 3600 * hh + 60 * mm + ss;
    return (nsec / ntrperiod) % 2;
  }

  void clear_average (Q65RuntimeState& state, int iseq)
  {
    if (!state.s1a.empty () && state.iz0 > 0 && state.jz0 > 0 && iseq >= 0 && iseq <= 1)
      {
        int const plane = state.iz0 * state.jz0;
        std::fill (state.s1a.begin () + plane * iseq,
                   state.s1a.begin () + plane * (iseq + 1), 0.0f);
      }
    state.navg[iseq] = 0;
  }

  void ensure_shapes (Q65RuntimeState& state, int ll, int iz, int jz, bool clear_average_flag)
  {
    if (state.ll0 != ll || state.iz0 != iz || state.jz0 != jz || clear_average_flag)
      {
        state.s1a.assign (static_cast<size_t> (iz) * jz * 2, 0.0f);
        state.navg[0] = 0;
        state.navg[1] = 0;
        state.ll0 = ll;
        state.iz0 = iz;
        state.jz0 = jz;
      }
  }

  void q65_symspec (Q65RuntimeState& state, std::vector<short> const& iwave,
                    int ntrperiod, int nsps, int istep, int nsmo,
                    bool lnewdat, int iseq, int iz, int jz,
                    std::vector<float>& s1)
  {
    (void) ntrperiod;
    int const nfft = nsps;
    float const fac = 1.0f / 32767.0f;

    std::vector<std::complex<float>> c0 (static_cast<size_t> (nfft));
    std::fill (s1.begin (), s1.end (), 0.0f);

    for (int j = 1; j <= jz; j += 2)
      {
        int const i1 = (j - 1) * istep;
        int const i2 = i1 + nsps - 1;
        int k = -1;
        for (int i = i1; i <= i2 && (i + 1) < static_cast<int> (iwave.size ()); i += 2)
          {
            float const xx = static_cast<float> (iwave[static_cast<size_t> (i)]);
            float const yy = static_cast<float> (iwave[static_cast<size_t> (i + 1)]);
            ++k;
            c0[static_cast<size_t> (k)] = fac * std::complex<float> {xx, yy};
          }
        if (k + 1 < nfft)
          {
            std::fill (c0.begin () + (k + 1), c0.end (), std::complex<float> {});
          }
        forward_complex (c0.data (), nfft);
        for (int i = 1; i <= iz; ++i)
          {
            std::complex<float> const value = c0[static_cast<size_t> (i - 1)];
            s1[static_cast<size_t> (col_major_index (i - 1, j - 1, iz))] =
                value.real () * value.real () + value.imag () * value.imag ();
          }
        int local_nsmo = nsmo <= 1 ? 0 : nsmo;
        for (int i = 0; i < local_nsmo; ++i)
          {
            decodium::legacy::smooth121_inplace (
                &s1[static_cast<size_t> (col_major_index (0, j - 1, iz))], iz);
          }
        if (j >= 3)
          {
            for (int i = 0; i < iz; ++i)
              {
                s1[static_cast<size_t> (col_major_index (i, j - 2, iz))] =
                    0.5f * (s1[static_cast<size_t> (col_major_index (i, j - 3, iz))]
                          + s1[static_cast<size_t> (col_major_index (i, j - 1, iz))]);
              }
          }
      }

    if (lnewdat)
      {
        state.navg[iseq] += 1;
        int const ntc = std::min (state.navg[iseq], 4);
        float const u = 1.0f / static_cast<float> (ntc);
        int const plane = iz * jz;
        float* s1a_plane = state.s1a.data () + plane * iseq;
        for (int i = 0; i < plane; ++i)
          {
            s1a_plane[i] = u * s1[static_cast<size_t> (i)] + (1.0f - u) * s1a_plane[i];
          }
      }

    state.last_s1 = s1;
    state.last_iz = iz;
    state.last_jz = jz;
  }

  void q65_ccf_85 (std::vector<float> const& s1, int iz, int jz, int nfqso,
                   int ia, int ia2, int i0, int j0, int lag1, int lag2, float df,
                   float dtstep, int mode_q65, int ncw, int const* codewords,
                   int& ipk, int& jpk, float& f0, float& xdt, int& imsg_best,
                   float& better, std::vector<float>& ccf1)
  {
    std::vector<float> ccf (static_cast<size_t> (2 * ia2 + 1) * (lag2 - lag1 + 1), 0.0f);
    std::vector<float> best (static_cast<size_t> (std::max (1, ncw)), 0.0f);
    int const isync[22] = {1, 9, 12, 13, 15, 22, 23, 26, 27, 33, 35,
                           38, 46, 50, 55, 60, 62, 66, 69, 74, 76, 85};

    ipk = 0;
    jpk = 0;
    float ccf_best = 0.0f;
    imsg_best = -1;
    ccf1.assign (static_cast<size_t> (2 * ia2 + 1), 0.0f);
    int const iia = static_cast<int> (200.0f / df);

    for (int imsg = 1; imsg <= ncw; ++imsg)
      {
        int sync_pos = 0;
        int payload_index = 0;
        int itone[kQ65SyncSymbols] {};
        for (int j = 1; j <= kQ65SyncSymbols; ++j)
          {
            if (sync_pos < 22 && j == isync[sync_pos])
              {
                ++sync_pos;
                itone[j - 1] = 0;
              }
            else
              {
                ++payload_index;
                itone[j - 1] =
                    codewords[(payload_index - 1) + kQ65CodewordSymbols * (imsg - 1)] + 1;
              }
          }

        std::fill (ccf.begin (), ccf.end (), 0.0f);
        for (int lag = lag1; lag <= lag2; ++lag)
          {
            for (int k = 1; k <= kQ65SyncSymbols; ++k)
              {
                int const j = j0 + kQ65Step * (k - 1) + 1 + lag;
                if (j < 1 || j > jz)
                  {
                    continue;
                  }
                for (int i = -ia2; i <= ia2; ++i)
                  {
                    int const ii = i0 + mode_q65 * itone[k - 1] + i;
                    if (ii >= iia && ii <= iz)
                      {
                        int const row = i + ia2;
                        int const col = lag - lag1;
                        ccf[static_cast<size_t> (row + (2 * ia2 + 1) * col)] +=
                            s1[static_cast<size_t> (col_major_index (ii - 1, j - 1, iz))];
                      }
                  }
              }
          }

        float ccfmax = 0.0f;
        int best_i = 0;
        int best_lag = 0;
        for (int lag = lag1; lag <= lag2; ++lag)
          {
            for (int i = -ia; i <= ia; ++i)
              {
                int const row = i + ia2;
                int const col = lag - lag1;
                float const value = ccf[static_cast<size_t> (row + (2 * ia2 + 1) * col)];
                if (value > ccfmax)
                  {
                    ccfmax = value;
                    best_i = i;
                    best_lag = lag;
                  }
              }
          }

        if (ccfmax > ccf_best)
          {
            ccf_best = ccfmax;
            ipk = best_i;
            jpk = best_lag;
            f0 = nfqso + ipk * df;
            xdt = jpk * dtstep;
            imsg_best = imsg;
            for (int i = -ia2; i <= ia2; ++i)
              {
                int const row = i + ia2;
                int const col = jpk - lag1;
                ccf1[static_cast<size_t> (row)] =
                    ccf[static_cast<size_t> (row + (2 * ia2 + 1) * col)];
              }
          }
        best[static_cast<size_t> (imsg - 1)] = ccfmax;
      }

    better = 0.0f;
    if (imsg_best > 0)
      {
        best[static_cast<size_t> (imsg_best - 1)] = 0.0f;
        float const second_best = *std::max_element (best.begin (), best.end ());
        better = second_best > 0.0f ? ccf_best / second_best : 0.0f;
      }
  }

  std::vector<Q65Candidate> q65_ccf_22 (std::vector<float> const& s1, int iz, int jz,
                                        int nfqso, int ntol, int nfa, int nfb,
                                        int max_drift, int mode_q65, int i0, int j0,
                                        int lag1, int lag2, float df, float dtstep,
                                        std::vector<float>& ccf2, int& ipk, int& jpk,
                                        float& f0, float& xdt, float& drift)
  {
    ccf2.assign (static_cast<size_t> (iz), 0.0f);
    std::vector<float> xdt2 (static_cast<size_t> (iz), 0.0f);
    std::vector<float> s1avg (static_cast<size_t> (iz), 0.0f);

    int ia = std::max (nfa, 100) / df;
    int ib = std::min (nfb, 4900) / df;
    if (max_drift != 0)
      {
        ia = std::max (static_cast<int> (std::lround (100.0f / df)),
                       static_cast<int> (std::lround ((nfqso - ntol) / df)));
        ib = std::min (static_cast<int> (std::lround (4900.0f / df)),
                       static_cast<int> (std::lround ((nfqso + ntol) / df)));
      }
    if (ia >= ib)
      {
        ia = ib - static_cast<int> (ntol / df);
      }
    ia = std::max (1, ia);
    ib = std::min (iz, ib);

    for (int i = ia; i <= ib; ++i)
      {
        float sum = 0.0f;
        for (int j = 1; j <= jz; ++j)
          {
            sum += s1[static_cast<size_t> (col_major_index (i - 1, j - 1, iz))];
          }
        s1avg[static_cast<size_t> (i - 1)] = sum;
      }

    float ccfbest = 0.0f;
    int ibest = 0;
    int lagbest = 0;
    int idrift_best = 0;
    int const isync[22] = {1, 9, 12, 13, 15, 22, 23, 26, 27, 33, 35,
                           38, 46, 50, 55, 60, 62, 66, 69, 74, 76, 85};

    for (int i = ia; i <= ib; ++i)
      {
        float ccfmax = 0.0f;
        int lagpk = 0;
        int idrift_max = 0;
        for (int lag = lag1; lag <= lag2; ++lag)
          {
            for (int idrift = -max_drift; idrift <= max_drift; ++idrift)
              {
                float ccft = 0.0f;
                for (int kk = 0; kk < 22; ++kk)
                  {
                    int const k = isync[kk];
                    int const ii = i + static_cast<int> (std::lround (idrift * (k - 43) / 85.0f));
                    if (ii < 1 || ii > iz)
                      {
                        continue;
                      }
                    int const n = kQ65Step * (k - 1) + 1;
                    int const j = n + lag + j0;
                    if (j >= 1 && j <= jz)
                      {
                        ccft += s1[static_cast<size_t> (col_major_index (ii - 1, j - 1, iz))];
                      }
                  }
                ccft -= (22.0f / static_cast<float> (jz)) * s1avg[static_cast<size_t> (i - 1)];
                if (ccft > ccfmax)
                  {
                    ccfmax = ccft;
                    lagpk = lag;
                    idrift_max = idrift;
                  }
              }
          }

        ccf2[static_cast<size_t> (i - 1)] = ccfmax;
        xdt2[static_cast<size_t> (i - 1)] = lagpk * dtstep;
        if (ccfmax > ccfbest && std::fabs (i * df - nfqso) <= static_cast<float> (ntol))
          {
            ccfbest = ccfmax;
            ibest = i;
            lagbest = lagpk;
            idrift_best = idrift_max;
          }
      }

    ipk = ibest - i0;
    jpk = lagbest;
    f0 = nfqso + ipk * df;
    xdt = jpk * dtstep;
    drift = df * idrift_best;
    std::fill (ccf2.begin (), ccf2.begin () + std::max (0, ia), 0.0f);
    if (ib < iz)
      {
        std::fill (ccf2.begin () + std::max (0, ib - 1), ccf2.end (), 0.0f);
      }

    std::vector<float> baseline (ccf2.begin () + (ia - 1), ccf2.begin () + ib);
    if (baseline.empty ())
      {
        return {};
      }
    std::vector<float> sorted = baseline;
    auto percentile_value = [&sorted] (int pct) -> float
    {
      if (sorted.empty ())
        {
          return 0.0f;
        }
      size_t const index = std::min (sorted.size () - 1,
                                     static_cast<size_t> (std::floor ((pct / 100.0) * (sorted.size () - 1))));
      std::nth_element (sorted.begin (), sorted.begin () + static_cast<long> (index), sorted.end ());
      return sorted[index];
    };
    float const ave = percentile_value (50);
    sorted = baseline;
    float const base = percentile_value (84);
    float const rms = base - ave;
    if (rms <= 0.0f)
      {
        return {};
      }

    std::vector<Q65Candidate> candidates;
    for (int j = 1; j <= 20; ++j)
      {
        int const k = static_cast<int> (baseline.size ()) - j;
        if (k < 0)
          {
            break;
          }
        std::vector<int> order (baseline.size ());
        for (size_t idx = 0; idx < order.size (); ++idx)
          {
            order[idx] = static_cast<int> (idx);
          }
        std::sort (order.begin (), order.end (), [&baseline] (int lhs, int rhs)
        {
          return baseline[static_cast<size_t> (lhs)] < baseline[static_cast<size_t> (rhs)];
        });
        int const i = order[static_cast<size_t> (k)] + ia;
        float const f = i * df;
        int const i3 = std::max (1, i - mode_q65);
        int const i4 = std::min (iz, i + mode_q65);
        float biggest = 0.0f;
        for (int ii = i3; ii <= i4; ++ii)
          {
            biggest = std::max (biggest, ccf2[static_cast<size_t> (ii - 1)]);
          }
        if (ccf2[static_cast<size_t> (i - 1)] != biggest)
          {
            continue;
          }
        float const snr = (ccf2[static_cast<size_t> (i - 1)] - ave) / rms;
        if (snr < 6.0f)
          {
            break;
          }
        Q65Candidate candidate;
        candidate.snr = snr;
        candidate.xdt = xdt2[static_cast<size_t> (i - 1)];
        candidate.freq = f;
        candidates.push_back (candidate);
        if (static_cast<int> (candidates.size ()) >= 20)
          {
            break;
          }
      }
    std::sort (candidates.begin (), candidates.end (), [] (Q65Candidate const& lhs, Q65Candidate const& rhs)
    {
      return lhs.freq < rhs.freq;
    });
    return candidates;
  }

  float estimate_q65_snr (Q65RuntimeState const& state,
                          std::array<int, kQ65PayloadSymbols> const& dat4,
                          float dtdec, float f0dec, int mode_q65)
  {
    if (state.last_s1.empty () || state.last_iz <= 0 || state.last_jz <= 0
        || state.last_df <= 0.0f || state.last_dtstep <= 0.0f)
      {
        return 0.0f;
      }

    int codeword[kQ65CodewordSymbols] {};
    int dat4_local[kQ65PayloadSymbols] {};
    for (int i = 0; i < kQ65PayloadSymbols; ++i)
      {
        dat4_local[i] = dat4[static_cast<size_t> (i)];
      }
    decodium::q65::encode_payload_symbols (dat4_local, codeword);

    int const isync[22] = {1, 9, 12, 13, 15, 22, 23, 26, 27, 33, 35,
                           38, 46, 50, 55, 60, 62, 66, 69, 74, 76, 85};
    int sync_pos = 0;
    int payload_index = 0;
    int itone[kQ65SyncSymbols] {};
    for (int j = 1; j <= kQ65SyncSymbols; ++j)
      {
        if (sync_pos < 22 && j == isync[sync_pos])
          {
            ++sync_pos;
            itone[j - 1] = 0;
          }
        else
          {
            ++payload_index;
            itone[j - 1] = codeword[payload_index - 1] + 1;
          }
      }

    std::vector<float> spec (static_cast<size_t> (state.last_iz), 0.0f);
    int const lagpk = static_cast<int> (std::lround (dtdec / state.last_dtstep));
    for (int k = 1; k <= kQ65SyncSymbols; ++k)
      {
        int const j = state.last_j0 + kQ65Step * (k - 1) + 1 + lagpk;
        if (j < 1 || j > state.last_jz)
          {
            continue;
          }
        for (int i = 1; i <= state.last_iz; ++i)
          {
            int const ii = i + mode_q65 * itone[k - 1];
            if (ii >= 1 && ii <= state.last_iz)
              {
                spec[static_cast<size_t> (i - 1)] +=
                    state.last_s1[static_cast<size_t> (col_major_index (ii - 1, j - 1, state.last_iz))];
              }
          }
      }

    int const i0 = static_cast<int> (std::lround (f0dec / state.last_df));
    int const nsum = std::max (10 * mode_q65, static_cast<int> (std::lround (50.0f / state.last_df)));
    int const ia = std::max (1, i0 - 2 * nsum);
    int const ib = std::min (state.last_iz, i0 + 2 * nsum);
    if (ia + nsum - 1 > state.last_iz || ib - nsum < 1)
      {
        return 0.0f;
      }
    float sum1 = 0.0f;
    float sum2 = 0.0f;
    for (int i = ia; i <= ia + nsum - 1; ++i)
      {
        sum1 += spec[static_cast<size_t> (i - 1)];
      }
    for (int i = ib - nsum + 1; i <= ib; ++i)
      {
        sum2 += spec[static_cast<size_t> (i - 1)];
      }
    float const avg = (sum1 + sum2) / (2.0f * nsum);
    if (avg <= 0.0f)
      {
        return 0.0f;
      }
    for (float& value : spec)
      {
        value /= avg;
      }
    float smax = 0.0f;
    for (int i = ia; i <= ib; ++i)
      {
        smax = std::max (smax, spec[static_cast<size_t> (i - 1)]);
      }
    float sig_area = 0.0f;
    for (int i = ia + nsum; i <= ib - nsum; ++i)
      {
        sig_area += spec[static_cast<size_t> (i - 1)] - 1.0f;
      }
    (void) smax;
    return db_like (std::max (1.0f, sig_area)) - db_like (2500.0f / state.last_df);
  }

  void q65_hist_lookup (Q65RuntimeState const& state, int if0, std::string& dxcall, std::string& dxgrid)
  {
    if (!dxcall.empty ())
      {
        return;
      }
    dxcall.clear ();
    dxgrid.clear ();
    for (auto it = state.history.rbegin (); it != state.history.rend (); ++it)
      {
        if (std::abs (it->freq - if0) > 10)
          {
            continue;
          }
        std::string const& msg = it->message;
        size_t const i1 = msg.find (' ');
        if (i1 == std::string::npos || i1 < 3 || i1 > 12)
          {
            continue;
          }
        size_t const i2 = msg.find (' ', i1 + 1);
        if (i2 == std::string::npos)
          {
            continue;
          }
        dxcall = msg.substr (i1 + 1, i2 - i1 - 1);
        if (i2 + 4 < msg.size ())
          {
            std::string grid = msg.substr (i2 + 1, 4);
            if (is_grid4 (grid))
              {
                dxgrid = grid;
              }
          }
        break;
      }
  }

  void q65_hist_store (Q65RuntimeState& state, int if0, std::string const& message)
  {
    if (static_cast<int> (state.history.size ()) == kQ65MaxHistory)
      {
        state.history.erase (state.history.begin ());
      }
    Q65HistEntry entry;
    entry.freq = if0;
    entry.message = message;
    state.history.push_back (std::move (entry));
  }

  void q65_hist2_store (Q65RuntimeState& state, int freq, std::string message)
  {
    if (message.find ('/') != std::string::npos)
      {
        return;
      }
    size_t const rpos = message.find (" R ");
    if (rpos != std::string::npos && rpos >= 6)
      {
        message.erase (rpos + 1, 2);
      }
    size_t const i1 = message.find (' ');
    if (i1 == std::string::npos || i1 < 3 || i1 > 12)
      {
        return;
      }
    size_t const i2 = message.find (' ', i1 + 1);
    if (i2 == std::string::npos)
      {
        return;
      }
    std::string const call = message.substr (i1 + 1, i2 - i1 - 1);
    std::string grid;
    if (i2 + 4 < message.size ())
      {
        std::string maybe_grid = message.substr (i2 + 1, 4);
        if (is_grid4 (maybe_grid))
          {
            grid = maybe_grid;
          }
      }
    if (grid.empty ())
      {
        return;
      }

    int const now = static_cast<int> (std::time (nullptr));
    for (auto& caller : state.callers)
      {
        if (caller.call == call)
          {
            caller.nsec = now;
            caller.freq = freq;
            return;
          }
      }
    if (static_cast<int> (state.callers.size ()) == kQ65MaxCallers)
      {
        state.callers.erase (state.callers.begin ());
      }
    Q65CallerEntry entry;
    entry.call = call;
    entry.grid = grid;
    entry.nsec = now;
    entry.freq = freq;
    entry.moonel = 0;
    state.callers.push_back (std::move (entry));
  }

  std::vector<int> build_q65_nqf (std::vector<Q65Candidate> const& candidates,
                                  std::vector<float> const& decoded_freqs,
                                  float baud, int mode_q65)
  {
    std::vector<int> nqf;
    float const bw = baud * mode_q65 * 65.0f;
    for (Q65Candidate const& candidate : candidates)
      {
        bool near_existing = false;
        for (float fj : decoded_freqs)
          {
            if (candidate.freq > fj - 5.0f && candidate.freq < fj + bw + 5.0f)
              {
                near_existing = true;
                break;
              }
          }
        if (!near_existing)
          {
            nqf.push_back (static_cast<int> (std::lround (candidate.freq)));
          }
      }
    while (nqf.size () < 20)
      {
        nqf.push_back (0);
      }
    return nqf;
  }

  Q65Dec0Result q65_dec0 (Q65RuntimeState& state, std::vector<short> const& iwave,
                          int iavg, int ntrperiod, int nfqso, int ntol, bool clear_average_flag,
                          bool lnewdat, float emedelay, int nsps, int mode_q65,
                          int nfa, int nfb, int ibwa, int ibwb, int npasses, int maxiters,
                          int max_drift, int stageno, int iseq)
  {
    Q65Dec0Result result;
    initialize_sync (state);
    int const ll = 64 * (2 + mode_q65);
    float const df = 12000.0f / nsps;
    int const istep = nsps / kQ65Step;
    int iz = static_cast<int> (5000.0f / df);
    float const txt = 85.0f * nsps / 12000.0f;
    int jz = static_cast<int> (((txt + 1.0f) * 12000.0f) / istep);
    if (nsps >= 6912)
      {
        jz = static_cast<int> (((txt + 2.0f) * 12000.0f) / istep);
      }
    int const ia = static_cast<int> (ntol / df);
    int const ia2 = std::max ({ia, 10 * mode_q65, static_cast<int> (std::lround (100.0f / df))});
    int nsmo = static_cast<int> (0.5f * mode_q65 * mode_q65);
    if (nsmo < 1)
      {
        nsmo = 1;
      }
    ensure_shapes (state, ll, iz, jz, clear_average_flag);

    float const dtstep = nsps / (static_cast<float> (kQ65Step) * 12000.0f);
    int const lag1 = static_cast<int> (-1.0f / dtstep);
    int lag2 = static_cast<int> (1.0f / dtstep + 0.9999f);
    if (nsps >= 3600 && emedelay > 0.0f)
      {
        lag2 = static_cast<int> (5.5f / dtstep + 0.9999f);
      }
    if (ntrperiod == 15 && nsps >= 900 && emedelay > 0.0f)
      {
        lag2 = static_cast<int> (4.0f / dtstep + 0.9999f);
      }
    int j0 = static_cast<int> (0.5f / dtstep);
    if (nsps >= 7200)
      {
        j0 = static_cast<int> (1.0f / dtstep);
      }
    int const i0 = static_cast<int> (std::lround (nfqso / df));

    std::vector<float> s1 (static_cast<size_t> (iz) * jz, 0.0f);
    if (iavg == 0)
      {
        q65_symspec (state, iwave, ntrperiod, nsps, istep, nsmo, lnewdat, iseq, iz, jz, s1);
      }
    else
      {
        int const plane = iz * jz;
        std::copy_n (state.s1a.data () + plane * iseq, plane, s1.data ());
        state.last_s1 = s1;
        state.last_iz = iz;
        state.last_jz = jz;
      }

    state.last_df = df;
    state.last_dtstep = dtstep;
    state.last_j0 = j0;
    state.last_mode = mode_q65;

    std::vector<float> ccf1;
    int ipk = 0;
    int jpk = 0;
    int imsg_best = -1;
    float better = 0.0f;
    result.idec = -1;
    if (state.ncw > 0 && iavg <= 1)
      {
        q65_ccf_85 (s1, iz, jz, nfqso, ia, ia2, i0, j0, lag1, lag2, df, dtstep,
                    mode_q65, state.ncw, state.codewords.data (),
                    ipk, jpk, result.f0, result.xdt, imsg_best, better, ccf1);
        if (better >= 1.10f || mode_q65 >= 8)
          {
            int ll_local = ll;
            int mode_local = mode_q65;
            int nsps_local = nsps;
            int ibwa_local = ibwa;
            int ibwb_local = ibwb;
            int ncw_local = state.ncw;
            int ibwbest = 0;
            int nrc = -2;
            float plog = 0.0f;
            ftx_q65_dec_q3_c (s1.data (), &iz, &jz, &ipk, &jpk, &ll_local, &mode_local,
                              &nsps_local, state.sync.data (), &ibwa_local, &ibwb_local,
                              state.codewords.data (), &ncw_local, &result.snr2,
                              result.dat4.data (), &result.idec, result.decoded.data (),
                              &ibwbest, &nrc, &plog);
            (void) ibwbest;
            (void) nrc;
            (void) plog;
          }
      }

    std::vector<float> ccf2_single;
    if (iavg == 0)
      {
        result.candidates = q65_ccf_22 (s1, iz, jz, nfqso, ntol, nfa, nfb, max_drift,
                                        mode_q65, i0, j0, lag1, lag2, df, dtstep,
                                        ccf2_single, ipk, jpk, result.f0, result.xdt,
                                        result.drift);
      }
    std::vector<float> ccf2_avg;
    if (iavg >= 1)
      {
        result.candidates = q65_ccf_22 (s1, iz, jz, nfqso, ntol, nfa, nfb, max_drift,
                                        mode_q65, i0, j0, lag1, lag2, df, dtstep,
                                        ccf2_avg, ipk, jpk, result.f0, result.xdt,
                                        result.drift);
      }
    if (result.idec < 0)
      {
        // result.f0 and result.xdt already updated by q65_ccf_22.
      }

    std::vector<float> curve = (iavg >= 1 && !ccf2_avg.empty ()) ? ccf2_avg : ccf2_single;
    float rms2 = 0.0f;
    if (!curve.empty ())
      {
        int count = iz;
        ftx_q65_sync_curve_c (curve.data (), &count, &rms2);
        float const smax = *std::max_element (curve.begin (), curve.end ());
        result.snr1 = rms2 > 0.0f ? smax / rms2 : 0.0f;
      }

    if (result.idec <= 0)
      {
        std::vector<float> s3 (static_cast<size_t> (ll) * kQ65PayloadColumns, 0.0f);
        int i0_local = i0;
        int j0_local = j0;
        int ll_local = ll;
        int mode_local = mode_q65;
        ftx_q65_s1_to_s3_c (s1.data (), &iz, &jz, &i0_local, &j0_local, &ipk, &jpk,
                            &ll_local, &mode_local, state.sync.data (), s3.data ());
        if (result.idec < 0 && (iavg == 0 || iavg == 2))
          {
            int mode_local2 = mode_q65;
            int nsps_local2 = nsps;
            int npasses_local = npasses;
            int ibwa_local2 = ibwa;
            int ibwb_local2 = ibwb;
            int maxiters_local = maxiters;
            int ibwbest = 0;
            int nrc = -2;
            ftx_q65_dec_q012_c (s3.data (), &ll_local, &mode_local2, &nsps_local2,
                                &npasses_local, state.apsym0.data (), state.aph10.data (),
                                &ibwa_local2, &ibwb_local2, &maxiters_local,
                                &result.snr2, result.dat4.data (), &result.idec,
                                result.decoded.data (), &ibwbest, &nrc);
          }
      }

    if (result.idec < 0 && max_drift == 50 && stageno == 5)
      {
        std::vector<float> s1w = s1;
        for (int w3t = 1; w3t <= jz; ++w3t)
          {
            for (int w3f = 1; w3f <= iz; ++w3f)
              {
                int const mm = w3f + static_cast<int> (std::lround (result.drift * w3t / (jz * df)));
                if (mm >= 1 && mm <= iz)
                  {
                    s1w[static_cast<size_t> (col_major_index (w3f - 1, w3t - 1, iz))] =
                        s1[static_cast<size_t> (col_major_index (mm - 1, w3t - 1, iz))];
                  }
              }
          }

        if (state.ncw > 0 && iavg <= 1)
          {
            q65_ccf_85 (s1w, iz, jz, nfqso, ia, ia2, i0, j0, lag1, lag2, df, dtstep,
                        mode_q65, state.ncw, state.codewords.data (),
                        ipk, jpk, result.f0, result.xdt, imsg_best, better, ccf1);
            if (better >= 1.10f)
              {
                int ll_local = ll;
                int mode_local = mode_q65;
                int nsps_local = nsps;
                int ibwa_local = ibwa;
                int ibwb_local = ibwb;
                int ncw_local = state.ncw;
                int ibwbest = 0;
                int nrc = -2;
                float plog = 0.0f;
                ftx_q65_dec_q3_c (s1w.data (), &iz, &jz, &ipk, &jpk, &ll_local, &mode_local,
                                  &nsps_local, state.sync.data (), &ibwa_local, &ibwb_local,
                                  state.codewords.data (), &ncw_local, &result.snr2,
                                  result.dat4.data (), &result.idec, result.decoded.data (),
                                  &ibwbest, &nrc, &plog);
                if (result.idec == 3)
                  {
                    result.idec = 5;
                  }
              }
          }
      }

    if (result.idec < 0)
      {
        std::fill (result.decoded.begin (), result.decoded.end (), ' ');
      }
    return result;
  }

  void prepare_q65_ap0 (Q65RuntimeState& state, std::string const& mycall,
                        std::string const& hiscall, int ncontest)
  {
    QByteArray mycall_field = to_fortran_field (QByteArray::fromStdString (mycall), 12);
    QByteArray hiscall_field = to_fortran_field (QByteArray::fromStdString (hiscall), 12);
    ftx_prepare_ft8_ap_c (mycall_field.constData (), hiscall_field.constData (), ncontest,
                          state.apsym0.data (), state.aph10.data ());
    for (int& value : state.apsym0)
      {
        if (value == -1)
          {
            value = 0;
          }
      }
  }

  void build_q65_list (Q65RuntimeState& state, int nqd0, bool lagain, int ncontest,
                       std::string const& mycall, std::string const& hiscall,
                       std::string const& hisgrid)
  {
    state.ncw = 0;
    QByteArray mycall_field = to_fortran_field (QByteArray::fromStdString (mycall), 12);
    QByteArray hiscall_field = to_fortran_field (QByteArray::fromStdString (hiscall), 12);
    QByteArray hisgrid_field = to_fortran_field (QByteArray::fromStdString (hisgrid), 6);
    if (nqd0 == 1 || lagain || ncontest == 1)
      {
        if (ncontest == 1)
          {
            char caller_calls[6 * kQ65MaxCallers] {};
            char caller_grids[4 * kQ65MaxCallers] {};
            int nhist2 = std::min (static_cast<int> (state.callers.size ()), kQ65MaxCallers);
            for (int j = 0; j < nhist2; ++j)
              {
                QByteArray call_field = to_fortran_field (
                    QByteArray::fromStdString (state.callers[static_cast<size_t> (j)].call), 6);
                QByteArray grid_field = to_fortran_field (
                    QByteArray::fromStdString (state.callers[static_cast<size_t> (j)].grid), 4);
                for (int i = 0; i < 6; ++i)
                  {
                    caller_calls[i + 6 * j] = call_field.at (i);
                  }
                for (int i = 0; i < 4; ++i)
                  {
                    caller_grids[i + 4 * j] = grid_field.at (i);
                  }
              }
            ftx_q65_set_list2_c (mycall_field.constData (), hiscall_field.constData (),
                                 hisgrid_field.constData (), caller_calls, caller_grids,
                                 &nhist2, state.codewords.data (), &state.ncw);
          }
        else
          {
            ftx_q65_set_list_c (mycall_field.constData (), hiscall_field.constData (),
                                hisgrid_field.constData (), state.codewords.data (), &state.ncw);
          }
      }
    std::array<int, kQ65PayloadSymbols> dgen {};
    int init_codeword[kQ65CodewordSymbols] {};
    decodium::q65::encode_payload_symbols (dgen.data (), init_codeword);
  }

  bool append_q65_line (std::vector<Q65Line>& lines, int nutc, float syncsnr, float snr2,
                        float dt, float freq, std::array<int, kQ65PayloadSymbols> const& dat4,
                        std::string const& decoded, int idec, int nused)
  {
    if (idec < 0 || lines.size () >= kQ65MaxLines)
      {
        return false;
      }
    Q65Line line;
    line.syncsnr = static_cast<int> (std::lround (syncsnr));
    line.snr = static_cast<int> (std::lround (snr2));
    line.dt = dt;
    line.freq = freq;
    line.idec = idec;
    line.nused = nused;
    line.decoded = decoded;
    dat4_to_bits77 (dat4, line.bits);
    lines.push_back (line);
    (void) nutc;
    return true;
  }

  void decode_q65_session (Q65RuntimeState& state, std::vector<short> const& audio,
                           int nqd0, int nutc, int ntrperiod, int nsubmode, int nfqso,
                           int ntol, int ndepth, int nfa, int nfb, bool lclearave,
                           bool single_decode, bool lagain, int max_drift, bool lnewdat,
                           float emedelay, std::string mycall, std::string hiscall,
                           std::string hisgrid, int nqsoprogress, int ncontest,
                           bool lapcqonly, std::vector<Q65Line>& out_lines,
                           std::vector<int>& nqf)
  {
    int const iseq = q65_sequence (nutc, ntrperiod);
    if (lclearave)
      {
        clear_average (state, iseq);
      }
    if (lagain)
      {
        q65_hist_lookup (state, nfqso, hiscall, hisgrid);
      }

    int const mode_q65 = 1 << nsubmode;
    int const nsps = q65_nsps_for_period (ntrperiod);
    float const baud = 12000.0f / static_cast<float> (nsps);
    int ibwa = 1;
    if (mode_q65 == 2) ibwa = 3;
    if (mode_q65 >= 4) ibwa = 8;
    int ibwb = std::min (15, ibwa + 6);
    int maxiters = 40;
    if ((ndepth & 3) == 2) maxiters = 60;
    if ((ndepth & 3) == 3)
      {
        ibwa = std::max (1, ibwa - 2);
        ibwb = std::min (15, ibwb + 2);
        maxiters = 100;
      }

    build_q65_list (state, nqd0, lagain, ncontest, mycall, hiscall, hisgrid);
    prepare_q65_ap0 (state, mycall, hiscall, ncontest);
    int npasses = (nqsoprogress == 5) ? 3 : 2;
    int nused = 1;
    int iavg = 0;
    std::vector<std::string> session_decodes;
    std::vector<float> decoded_freqs;

    auto emit_decode = [&] (Q65Dec0Result const& dec, float dtdec, float f0dec, int idec_value, int used)
    {
      std::string decoded = trim_fixed (dec.decoded.data (), kQ65DecodedChars);
      if (decoded.empty ())
        {
          return false;
        }
      if (std::find (session_decodes.begin (), session_decodes.end (), decoded) != session_decodes.end ())
        {
          return false;
        }
      float snr_est = estimate_q65_snr (state, dec.dat4, dtdec, f0dec, mode_q65);
      append_q65_line (out_lines, nutc, dec.snr1, snr_est, dtdec, f0dec, dec.dat4, decoded,
                       idec_value, used);
      session_decodes.push_back (decoded);
      decoded_freqs.push_back (f0dec);
      if (ncontest == 1)
        {
          q65_hist2_store (state, static_cast<int> (std::lround (f0dec)), decoded);
        }
      else
        {
          q65_hist_store (state, static_cast<int> (std::lround (f0dec)), decoded);
        }
      if ((ndepth & 128) != 0 && !lagain && std::abs (static_cast<int> (std::lround (f0dec - nfqso))) <= ntol)
        {
          clear_average (state, iseq);
        }
      return true;
    };

    Q65Dec0Result dec0 = q65_dec0 (state, audio, 0, ntrperiod, nfqso, ntol, lclearave,
                                   lnewdat, emedelay, nsps, mode_q65, nfa, nfb,
                                   ibwa, ibwb, npasses, maxiters, max_drift, 0, iseq);
    if (dec0.idec >= 0)
      {
        emit_decode (dec0, dec0.xdt, dec0.f0, dec0.idec, nused);
        nqf.assign (20, 0);
        return;
      }

    if (!(ncontest == 1 && lagain && (ndepth & 16) == 16))
      {
        if (!(ncontest == 1 && lagain && (ndepth & 16) == 0))
          {
            int jpk0 = static_cast<int> ((dec0.xdt + 1.0f) * 6000.0f);
            if (ntrperiod <= 30)
              {
                jpk0 = static_cast<int> ((dec0.xdt + 0.5f) * 6000.0f);
              }
            jpk0 = std::max (0, jpk0);
            std::vector<std::complex<float>> c00 (static_cast<size_t> (std::max (1, ntrperiod * 6000)));
            int npts = ntrperiod * 12000;
            if (ana64_native (audio.data (), npts, c00))
              {
                int passes = lapcqonly ? 1 : npasses;
                for (int ipass = 0; ipass <= passes; ++ipass)
                  {
                    int apmask_bits[kQ65ApBits] {};
                    int apsymbols_bits[kQ65ApBits] {};
                    int apmask[kQ65ApPacked] {};
                    int apsymbols[kQ65ApPacked] {};
                    int iaptype = 0;
                    if (ipass >= 1)
                      {
                        int lapcq_int = lapcqonly ? 1 : 0;
                        int qso = nqsoprogress;
                        int contest = ncontest;
                        ftx_q65_ap_c (&qso, &ipass, &contest, &lapcq_int, state.apsym0.data (),
                                      state.aph10.data (), apmask_bits, apsymbols_bits, &iaptype);
                        pack_q65_ap_bits (apmask_bits, apmask);
                        pack_q65_ap_bits (apsymbols_bits, apsymbols);
                      }
                    float xdt1 = dec0.xdt;
                    float f1 = dec0.f0;
                    float snr2 = 0.0f;
                    std::array<int, kQ65PayloadSymbols> dat4 {};
                    int idec = -1;
                    int idfbest = 0;
                    int idtbest = 0;
                    int ndistbest = 0;
                    int ibw = 0;
                    int nrc = -2;
                    int npts2 = npts / 2;
                    int nsps2 = nsps / 2;
                    int nsubmode_local = nsubmode;
                    int ndepth_local = ndepth;
                    int mode_local = mode_q65;
                    int ibwa_local = ibwa;
                    int ibwb_local = ibwb;
                    float drift = dec0.drift;
                    int maxiters_local = maxiters;
                    ftx_q65_loops_c (c00.data (), &npts2, &nsps2, &nsubmode_local, &ndepth_local,
                                     &jpk0, &dec0.xdt, &dec0.f0, &iaptype, &mode_local,
                                     &ibwa_local, &ibwb_local, &drift, apmask, apsymbols,
                                     &maxiters_local, &xdt1, &f1, &snr2, dat4.data (),
                                     &idec, &idfbest, &idtbest, &ndistbest, &ibw, &nrc);
                    if (idec >= 0)
                      {
                        char decoded_chars[kQ65DecodedChars] {};
                        bool success = false;
                        std::array<signed char, kQ65Bits> bits {};
                        dat4_to_bits77 (dat4, bits);
                        char c77[kQ65Bits];
                        for (int i = 0; i < kQ65Bits; ++i)
                          {
                            c77[i] = bits[static_cast<size_t> (i)] != 0 ? '1' : '0';
                          }
                        legacy_pack77_unpack_c (c77, 0, decoded_chars, &success);
                        if (success)
                          {
                            Q65Dec0Result loop_dec;
                            loop_dec.snr1 = dec0.snr1;
                            loop_dec.snr2 = snr2;
                            loop_dec.dat4 = dat4;
                            std::copy_n (decoded_chars, kQ65DecodedChars, loop_dec.decoded.begin ());
                            emit_decode (loop_dec, xdt1, f1, idec, nused);
                            nqf.assign (20, 0);
                            return;
                          }
                      }
                  }
              }
          }
      }

    if ((ndepth & 16) != 0 && state.navg[iseq] >= 2)
      {
        iavg = 1;
        Q65Dec0Result avg_list = q65_dec0 (state, audio, 1, ntrperiod, nfqso, ntol, false,
                                           lnewdat, emedelay, nsps, mode_q65, nfa, nfb,
                                           ibwa, ibwb, npasses, maxiters, max_drift, 0, iseq);
        if (avg_list.idec >= 0)
          {
            nused = state.navg[iseq];
            emit_decode (avg_list, avg_list.xdt, avg_list.f0, avg_list.idec, nused);
            nqf.assign (20, 0);
            return;
          }

        iavg = 2;
        Q65Dec0Result avg_q012 = q65_dec0 (state, audio, 2, ntrperiod, nfqso, ntol, false,
                                           lnewdat, emedelay, nsps, mode_q65, nfa, nfb,
                                           ibwa, ibwb, npasses, maxiters, max_drift, 0, iseq);
        if (avg_q012.idec >= 0)
          {
            nused = state.navg[iseq];
            emit_decode (avg_q012, avg_q012.xdt, avg_q012.f0, avg_q012.idec, nused);
            nqf.assign (20, 0);
            return;
          }
      }

    if (max_drift == 50)
      {
        Q65Dec0Result drift_dec = q65_dec0 (state, audio, iavg, ntrperiod, nfqso, ntol, false,
                                            lnewdat, emedelay, nsps, mode_q65, nfa, nfb,
                                            ibwa, ibwb, npasses, maxiters, max_drift, 5, iseq);
        if (drift_dec.idec >= 0)
          {
            emit_decode (drift_dec, drift_dec.xdt, drift_dec.f0, drift_dec.idec, nused);
          }
      }

    if (single_decode || lagain)
      {
        nqf.assign (20, 0);
        return;
      }

    for (Q65Candidate const& candidate : dec0.candidates)
      {
        bool dupe = false;
        for (float decoded_freq : decoded_freqs)
          {
            float const fdiff = candidate.freq - decoded_freq;
            if (fdiff > -baud * mode_q65 && fdiff < 65.0f * baud * mode_q65)
              {
                dupe = true;
                break;
              }
          }
        if (dupe)
          {
            continue;
          }

        int jpk0 = static_cast<int> ((candidate.xdt + 1.0f) * 6000.0f);
        if (ntrperiod <= 30)
          {
            jpk0 = static_cast<int> ((candidate.xdt + 0.5f) * 6000.0f);
          }
        jpk0 = std::max (0, jpk0);
        std::vector<std::complex<float>> c00 (static_cast<size_t> (std::max (1, ntrperiod * 6000)));
        int npts = ntrperiod * 12000;
        if (!ana64_native (audio.data (), npts, c00))
          {
            continue;
          }
        prepare_q65_ap0 (state, mycall, hiscall, ncontest);
        int passes = lapcqonly ? 1 : ((nqsoprogress == 5) ? 3 : 2);
        for (int ipass = 0; ipass <= passes; ++ipass)
          {
            int apmask_bits[kQ65ApBits] {};
            int apsymbols_bits[kQ65ApBits] {};
            int apmask[kQ65ApPacked] {};
            int apsymbols[kQ65ApPacked] {};
            int iaptype = 0;
            if (ipass >= 1)
              {
                int lapcq_int = lapcqonly ? 1 : 0;
                int qso = nqsoprogress;
                int contest = ncontest;
                ftx_q65_ap_c (&qso, &ipass, &contest, &lapcq_int, state.apsym0.data (),
                              state.aph10.data (), apmask_bits, apsymbols_bits, &iaptype);
                pack_q65_ap_bits (apmask_bits, apmask);
                pack_q65_ap_bits (apsymbols_bits, apsymbols);
              }
            float xdt1 = candidate.xdt;
            float f1 = candidate.freq;
            float snr2 = 0.0f;
            std::array<int, kQ65PayloadSymbols> dat4 {};
            int idec = -1;
            int idfbest = 0;
            int idtbest = 0;
            int ndistbest = 0;
            int ibw = 0;
            int nrc = -2;
            int npts2 = npts / 2;
            int nsps2 = nsps / 2;
            int nsubmode_local = nsubmode;
            int ndepth_local = ndepth;
            int mode_local = mode_q65;
            int ibwa_local = ibwa;
            int ibwb_local = ibwb;
            float drift = dec0.drift;
            int maxiters_local = maxiters;
            float xdt0_candidate = candidate.xdt;
            float f0_candidate = candidate.freq;
            ftx_q65_loops_c (c00.data (), &npts2, &nsps2, &nsubmode_local, &ndepth_local,
                             &jpk0, &xdt0_candidate, &f0_candidate, &iaptype, &mode_local,
                             &ibwa_local, &ibwb_local, &drift, apmask, apsymbols,
                             &maxiters_local, &xdt1, &f1, &snr2, dat4.data (), &idec,
                             &idfbest, &idtbest, &ndistbest, &ibw, &nrc);
            if (idec >= 0)
              {
                char decoded_chars[kQ65DecodedChars] {};
                bool success = false;
                std::array<signed char, kQ65Bits> bits {};
                dat4_to_bits77 (dat4, bits);
                char c77[kQ65Bits];
                for (int i = 0; i < kQ65Bits; ++i)
                  {
                    c77[i] = bits[static_cast<size_t> (i)] != 0 ? '1' : '0';
                  }
                legacy_pack77_unpack_c (c77, 0, decoded_chars, &success);
                if (success)
                  {
                    Q65Dec0Result loop_dec;
                    loop_dec.snr1 = candidate.snr;
                    loop_dec.snr2 = snr2;
                    loop_dec.dat4 = dat4;
                    std::copy_n (decoded_chars, kQ65DecodedChars, loop_dec.decoded.begin ());
                    emit_decode (loop_dec, xdt1, f1, idec, nused);
                    break;
                  }
              }
          }
      }

    if (iavg == 0 && state.navg[iseq] >= 2 && (ndepth & 16) != 0)
      {
        nqf.assign (20, 0);
        return;
      }

    if (ncontest == 1 && !lagain && ntrperiod == 60 && nsubmode == 0)
      {
        nqf = build_q65_nqf (dec0.candidates, decoded_freqs, baud, mode_q65);
      }
    else
      {
        nqf.assign (20, 0);
      }
  }

  Q65RuntimeState& q65_runtime_state ()
  {
    static Q65RuntimeState state;
    return state;
  }

  void decode_q65_core_native (short const* iwave,
                               int* nqd0, int* nutc, int* ntrperiod, int* nsubmode,
                               int* nfqso, int* ntol, int* ndepth, int* nfa, int* nfb,
                               int* nclearave, int* single_decode, int* nagain,
                               int* max_drift, int* lnewdat, float* emedelay,
                               char const* mycall, char const* hiscall, char const* hisgrid,
                               int* nqsoprogress, int* ncontest, int* lapcqonly,
                               int* syncsnrs, int* snrs, float* dts, float* freqs, int* idecs,
                               int* nuseds, signed char* bits77, char* decodeds, int* nout)
  {
    std::fill_n (syncsnrs, kQ65MaxLines, 0);
    std::fill_n (snrs, kQ65MaxLines, 0);
    std::fill_n (dts, kQ65MaxLines, 0.0f);
    std::fill_n (freqs, kQ65MaxLines, 0.0f);
    std::fill_n (idecs, kQ65MaxLines, -1);
    std::fill_n (nuseds, kQ65MaxLines, 0);
    std::fill_n (bits77, kQ65Bits * kQ65MaxLines, 0);
    std::fill_n (decodeds, kQ65DecodedChars * kQ65MaxLines, ' ');
    *nout = 0;

    int const expected_samples = std::max (1, *ntrperiod) * RX_SAMPLE_RATE;
    std::vector<short> audio (static_cast<size_t> (expected_samples), 0);
    std::copy_n (iwave, expected_samples, audio.data ());

    Q65RuntimeState& state = q65_runtime_state ();
    std::vector<Q65Line> lines;
    std::vector<int> nqf;

    decode_q65_session (state, audio, *nqd0, *nutc, *ntrperiod, *nsubmode, *nfqso, *ntol,
                        *ndepth, *nfa, *nfb, *nclearave != 0, *single_decode != 0,
                        *nagain != 0, *max_drift, *lnewdat != 0, *emedelay,
                        trim_fixed (mycall, 12), trim_fixed (hiscall, 12), trim_fixed (hisgrid, 6),
                        *nqsoprogress, *ncontest, *lapcqonly != 0, lines, nqf);

    if (!(*nagain))
      {
        for (int freq : nqf)
          {
            if (freq == 0)
              {
                break;
              }
            decode_q65_session (state, audio, 1, *nutc, *ntrperiod, *nsubmode, freq, 5,
                                *ndepth, *nfa, *nfb, false, true, true, *max_drift, false,
                                *emedelay, trim_fixed (mycall, 12), trim_fixed (hiscall, 12),
                                trim_fixed (hisgrid, 6), *nqsoprogress, *ncontest,
                                *lapcqonly != 0, lines, nqf);
          }
      }

    int const count = std::min (static_cast<int> (lines.size ()), kQ65MaxLines);
    *nout = count;
    for (int i = 0; i < count; ++i)
      {
        Q65Line const& line = lines[static_cast<size_t> (i)];
        syncsnrs[i] = line.syncsnr;
        snrs[i] = line.snr;
        dts[i] = line.dt;
        freqs[i] = line.freq;
        idecs[i] = line.idec;
        nuseds[i] = line.nused;
        std::copy_n (line.bits.data (), kQ65Bits, bits77 + i * kQ65Bits);
        std::fill_n (decodeds + i * kQ65DecodedChars, kQ65DecodedChars, ' ');
        std::copy_n (line.decoded.data (),
                     std::min (static_cast<int> (line.decoded.size ()), kQ65DecodedChars),
                     decodeds + i * kQ65DecodedChars);
      }
  }

  void decode_q65_into_outputs (short const* iwave,
                                int* nqd0, int* nutc, int* ntrperiod, int* nsubmode,
                                int* nfqso, int* ntol, int* ndepth, int* nfa, int* nfb,
                                int* nclearave, int* single_decode, int* nagain,
                                int* max_drift, int* lnewdat, float* emedelay,
                                char const* mycall, char const* hiscall, char const* hisgrid,
                                int* nqsoprogress, int* ncontest, int* lapcqonly,
                                int* syncsnrs, int* snrs, float* dts, float* freqs, int* idecs,
                                int* nuseds, signed char* bits77, char* decodeds, int* nout)
  {
    if (!iwave || !nqd0 || !nutc || !ntrperiod || !nsubmode || !nfqso || !ntol || !ndepth
        || !nfa || !nfb || !nclearave || !single_decode || !nagain || !max_drift
        || !lnewdat || !emedelay || !mycall || !hiscall || !hisgrid || !nqsoprogress
        || !ncontest || !lapcqonly || !syncsnrs || !snrs || !dts || !freqs || !idecs
        || !nuseds || !bits77 || !decodeds || !nout)
      {
        if (nout)
          {
            *nout = 0;
          }
        return;
      }

    QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};
    decode_q65_core_native (iwave, nqd0, nutc, ntrperiod, nsubmode, nfqso, ntol, ndepth,
                            nfa, nfb, nclearave, single_decode, nagain, max_drift,
                            lnewdat, emedelay, mycall, hiscall, hisgrid, nqsoprogress,
                            ncontest, lapcqonly, syncsnrs, snrs, dts, freqs, idecs,
                            nuseds, bits77, decodeds, nout);
  }
}

extern "C" int ftx_q65_ana64_c (short const* iwave, int npts, float* real_out, float* imag_out)
{
  if (!iwave || npts <= 0 || (npts & 1) != 0 || !real_out || !imag_out)
    {
      return 0;
    }

  std::vector<std::complex<float>> c0;
  if (!decodium::q65::ana64_transform (iwave, npts, c0))
    {
      return 0;
    }

  int const count = npts / 2;
  for (int i = 0; i < count; ++i)
    {
      std::complex<float> const value = c0[static_cast<size_t> (i)];
      real_out[i] = value.real ();
      imag_out[i] = value.imag ();
    }
  return 1;
}

extern "C" void ana64_ (short iwave[], int* npts, std::complex<float> c0[])
{
  if (!iwave || !npts || *npts <= 0 || (*npts & 1) != 0 || !c0)
    {
      return;
    }

  std::vector<std::complex<float>> native;
  if (!ana64_native (iwave, *npts, native))
    {
      return;
    }

  int const count = *npts / 2;
  for (int i = 0; i < count; ++i)
    {
      c0[i] = native[static_cast<size_t> (i)];
    }
}

bool decodium::q65::ana64_transform (short const* iwave, int npts,
                                     std::vector<std::complex<float>>& c0)
{
  return ana64_native (iwave, npts, c0);
}

extern "C" void ftx_q65_async_decode_c (short const* iwave,
                                        int* nqd0, int* nutc, int* ntrperiod, int* nsubmode,
                                        int* nfqso, int* ntol, int* ndepth, int* nfa, int* nfb,
                                        int* nclearave, int* single_decode, int* nagain,
                                        int* max_drift, int* lnewdat, float* emedelay,
                                        char const* mycall, char const* hiscall,
                                        char const* hisgrid, int* nqsoprogress,
                                        int* ncontest, int* lapcqonly,
                                        int syncsnrs[], int snrs[], float dts[],
                                        float freqs[], int idecs[], int nuseds[],
                                        signed char bits77[], char decodeds[], int* nout,
                                        fortran_charlen_t, fortran_charlen_t,
                                        fortran_charlen_t, fortran_charlen_t)
{
  decode_q65_into_outputs (iwave, nqd0, nutc, ntrperiod, nsubmode, nfqso, ntol, ndepth,
                           nfa, nfb, nclearave, single_decode, nagain, max_drift,
                           lnewdat, emedelay, mycall, hiscall, hisgrid, nqsoprogress,
                           ncontest, lapcqonly, syncsnrs, snrs, dts, freqs, idecs,
                           nuseds, bits77, decodeds, nout);
}

extern "C" void ftx_q65_async_decode_c_api (short const* iwave,
                                            int* nqd0, int* nutc, int* ntrperiod, int* nsubmode,
                                            int* nfqso, int* ntol, int* ndepth, int* nfa, int* nfb,
                                            int* nclearave, int* single_decode, int* nagain,
                                            int* max_drift, int* lnewdat, float* emedelay,
                                            char const mycall[12], char const hiscall[12],
                                            char const hisgrid[6], int* nqsoprogress,
                                            int* ncontest, int* lapcqonly,
                                            int syncsnrs[], int snrs[], float dts[],
                                            float freqs[], int idecs[], int nuseds[],
                                            signed char bits77[], char decodeds[], int* nout)
{
  decode_q65_into_outputs (iwave, nqd0, nutc, ntrperiod, nsubmode, nfqso, ntol, ndepth,
                           nfa, nfb, nclearave, single_decode, nagain, max_drift,
                           lnewdat, emedelay, mycall, hiscall, hisgrid, nqsoprogress,
                           ncontest, lapcqonly, syncsnrs, snrs, dts, freqs, idecs,
                           nuseds, bits77, decodeds, nout);
}

extern "C" void ftx_q65_async_decode_latest_c_api (short const* iwave,
                                                    int* nqd0, int* nutc, int* ntrperiod,
                                                    int* nsubmode, int* nfqso, int* ntol,
                                                    int* ndepth, int* nfa, int* nfb,
                                                    int* nclearave, int* single_decode,
                                                    int* nagain, int* max_drift, int* lnewdat,
                                                    float* emedelay,
                                                    char const mycall[12],
                                                    char const hiscall[12],
                                                    char const hisgrid[6],
                                                    int* nqsoprogress, int* ncontest,
                                                    int* lapcqonly, int* snr0, float* dt0,
                                                    int* freq0, char cq0[3], char msg0[37],
                                                    int* found)
{
  if (snr0)
    {
      *snr0 = -99;
    }
  if (dt0)
    {
      *dt0 = 0.0f;
    }
  if (freq0)
    {
      *freq0 = 0;
    }
  if (cq0)
    {
      std::fill_n (cq0, 3, ' ');
    }
  if (msg0)
    {
      std::fill_n (msg0, 37, ' ');
    }
  if (found)
    {
      *found = 0;
    }
  if (!iwave || !nqd0 || !nutc || !ntrperiod || !nsubmode || !nfqso || !ntol || !ndepth
      || !nfa || !nfb || !nclearave || !single_decode || !nagain || !max_drift
      || !lnewdat || !emedelay || !mycall || !hiscall || !hisgrid || !nqsoprogress
      || !ncontest || !lapcqonly || !snr0 || !dt0 || !freq0 || !cq0 || !msg0 || !found)
    {
      return;
    }

  std::array<int, kQ65MaxLines> syncsnrs {};
  std::array<int, kQ65MaxLines> snrs {};
  std::array<float, kQ65MaxLines> dts {};
  std::array<float, kQ65MaxLines> freqs {};
  std::array<int, kQ65MaxLines> idecs {};
  std::array<int, kQ65MaxLines> nuseds {};
  std::array<signed char, kQ65Bits * kQ65MaxLines> bits77 {};
  std::array<char, kQ65DecodedChars * kQ65MaxLines> decodeds {};
  int nout = 0;

  decode_q65_into_outputs (iwave, nqd0, nutc, ntrperiod, nsubmode, nfqso, ntol, ndepth,
                           nfa, nfb, nclearave, single_decode, nagain, max_drift,
                           lnewdat, emedelay, mycall, hiscall, hisgrid, nqsoprogress,
                           ncontest, lapcqonly, syncsnrs.data (), snrs.data (), dts.data (),
                           freqs.data (), idecs.data (), nuseds.data (), bits77.data (),
                           decodeds.data (), &nout);

  int latest = -1;
  for (int i = 0; i < nout; ++i)
    {
      if (idecs[static_cast<size_t> (i)] >= 0)
        {
          latest = i;
        }
    }

  if (latest < 0)
    {
      return;
    }

  *found = 1;
  *snr0 = snrs[static_cast<size_t> (latest)];
  *dt0 = dts[static_cast<size_t> (latest)];
  *freq0 = qRound (freqs[static_cast<size_t> (latest)]);
  cq0[0] = 'q';
  cq0[1] = static_cast<char> ('0' + std::max (0, std::min (idecs[static_cast<size_t> (latest)], 9)));
  if (nuseds[static_cast<size_t> (latest)] >= 2)
    {
      cq0[2] = static_cast<char> ('0' + std::max (0, std::min (nuseds[static_cast<size_t> (latest)], 9)));
    }

  std::string const decoded = trimmed_decode (decodeds.data (), latest);
  std::memcpy (msg0, decoded.data (), std::min<size_t> (decoded.size (), 37));
}

extern "C" void ftx_q65_decode_and_emit_params_c (short const* iwave,
                                                  params_block_t const* params,
                                                  char const* temp_dir,
                                                  int* decoded_count)
{
  if (decoded_count)
    {
      *decoded_count = 0;
    }
  if (!iwave || !params || !temp_dir)
    {
      return;
    }

  std::array<int, kQ65MaxLines> syncsnrs {};
  std::array<int, kQ65MaxLines> snrs {};
  std::array<float, kQ65MaxLines> dts {};
  std::array<float, kQ65MaxLines> freqs {};
  std::array<int, kQ65MaxLines> idecs {};
  std::array<int, kQ65MaxLines> nuseds {};
  std::array<signed char, kQ65Bits * kQ65MaxLines> bits77 {};
  std::array<char, kQ65DecodedChars * kQ65MaxLines> decodeds {};
  int nout = 0;

  int nqd0 = 1;
  int nutc = params->nutc;
  int ntrperiod = params->ntrperiod;
  int nsubmode = params->nsubmode;
  int nfqso = params->nfqso;
  int ntol = params->ntol;
  int ndepth = params->ndepth;
  int nfa = params->nfa;
  int nfb = params->nfb;
  int nclearave = params->nclearave ? 1 : 0;
  int single_decode = (params->nexp_decode & 32) ? 1 : 0;
  int nagain = params->nagain ? 1 : 0;
  int max_drift = params->max_drift;
  int lnewdat = params->newdat ? 1 : 0;
  float emedelay = params->emedelay;
  int nqsoprogress = params->nQSOProgress;
  int ncontest = params->nexp_decode & 7;
  int lapcqonly = params->lapcqonly ? 1 : 0;

  decode_q65_into_outputs (iwave, &nqd0, &nutc, &ntrperiod, &nsubmode, &nfqso, &ntol, &ndepth,
                           &nfa, &nfb, &nclearave, &single_decode, &nagain, &max_drift,
                           &lnewdat, &emedelay, params->mycall, params->hiscall,
                           params->hisgrid, &nqsoprogress, &ncontest, &lapcqonly,
                           syncsnrs.data (), snrs.data (), dts.data (), freqs.data (),
                           idecs.data (), nuseds.data (), bits77.data (), decodeds.data (),
                           &nout);

  int const count = std::max (0, std::min (nout, kQ65MaxLines));
  if (decoded_count)
    {
      *decoded_count = count;
    }

  std::string const temp_dir_path = trim_temp_dir (temp_dir);
  std::string const decoded_path = temp_dir_path.empty ()
      ? "decoded.txt"
      : temp_dir_path + "/decoded.txt";
  std::ofstream decoded_file {
      decoded_path,
      std::ios::out | (nagain != 0 ? std::ios::app : std::ios::trunc)
  };

  for (int index = 0; index < count; ++index)
    {
      std::cout << format_stdout_line (nutc, snrs[index], dts[index], freqs[index],
                                       decodeds.data (), index, idecs[index], nuseds[index])
                << '\n';
      if (decoded_file.is_open ())
        {
          decoded_file << format_decoded_file_line (nutc, ntrperiod, syncsnrs[index],
                                                    snrs[index], dts[index], freqs[index],
                                                    decodeds.data (), index)
                       << '\n';
        }
    }

  std::cout.flush ();
  if (decoded_file.is_open ())
    {
      decoded_file.flush ();
    }
}
