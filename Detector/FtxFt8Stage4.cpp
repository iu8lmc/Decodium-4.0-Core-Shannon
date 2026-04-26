// -*- Mode: C++ -*-
#include "wsjtx_config.h"
#include "commons.h"
#include "helper_functions.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <fftw3.h>

#include "lib/superfox/qpc/np_qpc.h"

extern "C" uint32_t nhash2 (void const* key, uint64_t length, uint32_t initval);

namespace
{

constexpr int kFt8NMax {15 * 12000};
constexpr int kFt8Nn {79};
constexpr int kFt8Nh1 {1920};
constexpr int kFt8DefaultMaxCand {1000};
constexpr int kFt8DeepMaxCand {2400};
constexpr int kFt8MaxCand {4800};
constexpr int kFt8StrictHardErrors {36};
constexpr int kFt8MaxHardErrors {58};
constexpr int kFt8MaxEarly {200};
constexpr int kFt8A7MaxRetained {96};
constexpr int kFt8A7MaxAge {6};
constexpr int kFt8MaxLines {200};
constexpr int kFt8Bits {77};
constexpr int kFt8DecodedChars {37};
constexpr int kFt8WordChars {13};
constexpr int kFt8WordCount {19};
constexpr int kFt8SequenceCount {2};
constexpr int kFt8CarrySamples {47 * 3456};
constexpr int kFt8A7Np2 {2812};
constexpr int kFt8A7DownsampleSize {3200};
constexpr int kFt8A7MaxMsg {206};
constexpr float kFt8A7Fs2 {200.0f};
constexpr int kFt8A8Nsym {79};
constexpr int kFt8A8Nsps {32};
constexpr int kFt8A8Nwave {kFt8A8Nsym * kFt8A8Nsps};
constexpr int kFt8A8DphiSize {(kFt8A8Nsym + 2) * kFt8A8Nsps};
constexpr int kFt8A8MaxMsg {206};
constexpr int kFt8PhaseTableSize {65536};
constexpr float kFt8A8Bt {2.0f};
constexpr float kFt8TwoPi {6.28318530717958647692f};

std::atomic<bool>& stage4_cancel_requested ()
{
  static std::atomic<bool> cancel {false};
  return cancel;
}

std::atomic<long long>& stage4_deadline_ms ()
{
  static std::atomic<long long> deadline {0};
  return deadline;
}

std::atomic<int>& stage4_ldpc_maxosd_override ()
{
  static std::atomic<int> value {-1};
  return value;
}

std::atomic<int>& stage4_ldpc_norder_override ()
{
  static std::atomic<int> value {0};
  return value;
}

std::atomic<bool>& stage4_supplemental_requested ()
{
  static std::atomic<bool> value {false};
  return value;
}

long long steady_clock_ms ()
{
  using namespace std::chrono;
  return duration_cast<milliseconds> (steady_clock::now ().time_since_epoch ()).count ();
}

bool stage4_should_cancel ()
{
  if (stage4_cancel_requested ().load (std::memory_order_relaxed))
    {
      return true;
    }

  long long const deadline = stage4_deadline_ms ().load (std::memory_order_relaxed);
  return deadline > 0 && steady_clock_ms () >= deadline;
}

extern "C"
{
  void ftx_sync8_search_c (float const* dd, int npts, float nfa, float nfb,
                           float syncmin, float nfqso, int maxcand,
                           float* candidate, int* ncand, float* sbase);
  void ftx_sync8_search_stage4_c (float const* dd, int npts, float nfa, float nfb,
                                   float syncmin, float nfqso, int maxcand, int ipass,
                                   float* candidate, int* ncand, float* sbase);
  void ftx_subtract_ft8_c (float* dd0, int const* itone, float f0, float dt, int lrefinedt);
  void ftx_prepare_ft8_ap_c (char const mycall[12], char const hiscall[12], int ncontest,
                             int* apsym, int* aph10);
  void ftx_ft8_prepare_pass_c (int ndepth, int ipass, int ndecodes,
                               float* syncmin, int* imetric, int* lsubtract, int* run_pass);
  void ftx_ft8_plan_decode_stage_c (int ndepth, int nzhsym, int ndec_early,
                                    int nagain, int* action_out, int* refine_out);
  int ftx_ft8_should_bail_by_tseq_c (int ldiskdat, double tseq, double limit);
  void ftx_ft8_prepare_candidate_c (float sync_in, float f1_in, float xdt_in,
                                    float const* sbase, int sbase_size,
                                    float* sync_out, float* f1_out,
                                    float* xdt_out, float* xbase_out);
  void ftx_ft8_finalize_main_result_c (float xsnr, float xdt_in, float emedelay,
                                       int nharderrors, float dmin, int* nsnr_out,
                                       float* xdt_out, float* qual_out);
  int ftx_ft8_should_run_a7_c (int lft8apon, int ncontest, int nzhsym, int previous_count);
  int ftx_ft8_should_run_a8_c (int lft8apon, int ncontest, int nzhsym, int la8,
                               int hiscall_len, int hisgrid_len, int ltry_a8);
  int ftx_ft8_prepare_a7_request_c (float previous_f0, float previous_dt,
                                    char const previous_msg37[37],
                                    float const* sbase, int sbase_size,
                                    float* f1_out, float* xdt_out, float* xbase_out,
                                    char call_1_out[12], char call_2_out[12], char grid4_out[4]);
  int ftx_ft8_should_keep_a8_after_a7_c (char const decoded_msg37[37],
                                         char const hiscall12[12]);
  void ftx_ft8_finalize_a7_result_c (float xsnr, int* nsnr_out, int* iaptype_out,
                                     float* qual_out);
  void ftx_ft8_finalize_a8_result_c (float plog, float xsnr, float fbest,
                                     int* nsnr_out, int* iaptype_out,
                                     float* qual_out, float* save_freq_out);
  int ftx_ft8_select_npasses_c (int lapon, int lapcqonly, int ncontest,
                                int nzhsym, int nQSOProgress);
  void ftx_ft8_plan_pass_window_c (int requested_pass, int npasses,
                                   int* pass_first_out, int* pass_last_out);
  int ftx_ft8_prepare_decode_pass_c (int ipass, int nQSOProgress, int lapcqonly, int ncontest,
                                     int nfqso, int nftx, float f1, int napwid,
                                     int const* apsym, int const* aph10,
                                     float const* llra, float const* llrb,
                                     float const* llrc, float const* llrd,
                                     float const* llre, float* llrz,
                                     int* apmask, int* iaptype_out);
  int ftx_ft8_prepare_cq_ap_pass_c (int ipass, int nQSOProgress, int lapcqonly, int ncontest,
                                    int nfqso, int nftx, float f1, int napwid,
                                    int const* apsym, int const* aph10,
                                    float const* llra, float const* llrb, float const* llrc,
                                    float* llrz, int* apmask, int* iaptype_out);
  int ftx_ft8_finalize_decode_pass_c (int nbadcrc, int pass_index, int iaptype_in,
                                      int* ipass_out, int* iaptype_out);
  int ftx_ft8_store_saved_decode_c (int ndecodes, int max_early,
                                    int nsnr, float f1, float xdt,
                                    int const* itone, int nn,
                                    int* allsnrs, float* f1_save,
                                    float* xdt_save, int* itone_save);
  void ftx_ft8_apply_saved_subtractions_c (float* dd, int const* itone_save,
                                           int nn, int ndec_early,
                                           float const* f1_save, float const* xdt_save,
                                           bool* lsubtracted,
                                           int initial_only, int refine);
  void legacy_pack77_split77_c (char const msg[37], int* nwords, int nw[19], char words[247]);
  void ftx_ft2_decode_and_emit_params_c (short const* iwave,
                                         params_block_t const* params,
                                         char const* temp_dir,
                                         int* decoded_count);
  void ftx_ft4_decode_and_emit_params_c (short const* iwave,
                                         params_block_t const* params,
                                         char const* temp_dir,
                                         int* decoded_count);
  void ftx_ft8_a7d_c (float* dd0, int* newdat, char const call_1[12], char const call_2[12],
                      char const grid4[4], float* xdt, float* f1, float* xbase,
                      int* nharderrors, float* dmin, char msg37[37], float* xsnr);
  void ftx_ft8_a8d_c (float* dd, char const mycall[12], char const dxcall[12],
                      char const dxgrid[6], float* f1a, float* xdt, float* fbest,
                      float* xsnr, float* plog, char msgbest[37]);
  void ftx_ft8_downsample_c (float const* dd, int* newdat, float f0, fftwf_complex* c1);
  void ftx_ft8_bitmetrics_scaled_c (std::complex<float> const* cd0, int np2, int ibest, int imetric,
                                    float scale, float* s8_out, int* nsync_out,
                                    float* llra, float* llrb, float* llrc, float* llrd, float* llre);
  int ftx_ft8_a8_search_candidate_c (std::complex<float> const* cd,
                                     std::complex<float> const* cwave,
                                     int nzz, int nwave, float f1,
                                     float* spk_out, float* fpk_out,
                                     float* tpk_out, float* spectrum_out);
  int ftx_ft8_a8_finalize_search_c (float const* spectrum, int size, float f1,
                                    float fbest, float* xsnr_out);
  int ftx_ft8_a8_accept_score_c (int nhard, float plog, float sigobig);
  void ftx_ft8_a8_score_c (std::complex<float> const* cd, int nzz, float tbest,
                           int const* itone_best, float* plog, int* nhard,
                           float* sigobig);
  void ftx_twkfreq1_c (std::complex<float> const* ca, int const* npts, float const* fsample,
                       float const* a, std::complex<float>* cb);
  int ftx_prepare_ft8_a7_candidate_c (int imsg, char const call_1[12], char const call_2[12],
                                      char const grid4[4], char message37[37]);
  int ftx_prepare_ft8_a8_candidate_c (int imsg, char const mycall[12], char const hiscall[12],
                                      char const hisgrid[6], char message37[37]);
  int ftx_encode_ft8_candidate_c (char const* message37, char* msgsent_out,
                                  int* itone_out, signed char* codeword_out);
  void ftx_ft8a7_measure_candidate_c (float const* s8, int rows, int cols,
                                      int const* itone, signed char const* cw,
                                      float const* llra, float const* llrb,
                                      float const* llrc, float const* llrd,
                                      float* pow_out, float* dmin_out,
                                      int* nharderrors_out);
  void ftx_ft8_a7_search_initial_c (std::complex<float> const* cd0, int np2, float fs2,
                                    float xdt_in, int* ibest_out, float* delfbest_out);
  void ftx_ft8_a7_refine_search_c (std::complex<float> const* cd0, int np2, float fs2,
                                   int ibest_in, int* ibest_out, float* sync_out, float* xdt_out);
  int ftx_ft8_a7_finalize_metrics_c (float const* dmm, int count, float pbest, float xbase,
                                     float* dmin_out, float* dmin2_out, float* xsnr_out);
  void ftx_decode174_91_c (float const* llr_in, int Keff, int maxosd, int norder,
                           signed char const* apmask_in, signed char* message91_out,
                           signed char* cw_out, int* ntype_out, int* nharderror_out,
                           float* dmin_out);
  int ftx_ft8_validate_candidate_meta_c (signed char const* message77, signed char const* cw,
                                         int nharderrors, int unpack_ok, int quirky, int ncontest);
  int ftx_ft8_compute_snr_c (float const* s8, int rows, int cols, int const* itone,
                             float xbase, int nagain, int nsync, float* xsnr_out);
  void legacy_pack77_reset_context_c ();
  void legacy_pack77_set_context_c (char const mycall[13], char const hiscall[13]);
  void legacy_pack77_unpack28_c (int n28, char c13[13], bool* success);
  void legacy_pack77_unpacktext77_c (char const c71[71], char c13[13], bool* success);
  int legacy_pack77_unpack77bits_c (signed char const* message77, int received,
                                    char msgsent[37], int* quirky_out);
  int ftx_ft8_message77_to_itone_c (signed char const* message77, int* itone_out);
  void ftx_sfox_remove_ft8_c (float* dd, int npts);
  void sfox_remove_tone_ (std::complex<float>* c0, float* fsync);
  void qpc_decode2_ (std::complex<float>* c0, float* fsync, float* ftol,
                     signed char* xdec, int* ndepth, float* dth, float* damp,
                     int* crc_ok, float* snrsync, float* fbest, float* tbest, float* snr);
  void __ft8_a7_MOD_ft8_a7d (float* dd0, int* newdat, char* call_1, char* call_2,
                             char* grid4, float* xdt, float* f1, float* xbase,
                             int* nharderrors, float* dmin, char* msg37, float* xsnr,
                             size_t, size_t, size_t, size_t);
  void ft8_a8d_ (float* dd, char* mycall, char* dxcall, char* dxgrid,
                 float* f1a, float* xdt, float* fbest, float* xsnr,
                 float* plog, char* msgbest,
                 size_t, size_t, size_t, size_t);
}

template <size_t N>
using FixedChars = std::array<char, N>;

template <size_t N>
FixedChars<N> blank_fixed ()
{
  FixedChars<N> out {};
  out.fill (' ');
  return out;
}

template <size_t N>
FixedChars<N> fixed_from_chars (char const* data)
{
  FixedChars<N> out = blank_fixed<N> ();
  if (data)
    {
      std::copy_n (data, N, out.data ());
    }
  return out;
}

template <size_t N>
FixedChars<N> fixed_from_string (std::string const& text)
{
  FixedChars<N> out = blank_fixed<N> ();
  std::copy_n (text.data (), std::min (static_cast<int>(text.size ()), static_cast<int>(N)), out.data ());
  return out;
}

template <size_t N>
std::string trim_fixed (FixedChars<N> const& value)
{
  size_t end = N;
  while (end > 0 && (value[end - 1] == ' ' || value[end - 1] == '\0'))
    {
      --end;
    }
  return std::string {value.data (), end};
}

std::string trim_block (char const* data, int width)
{
  int end = width;
  while (end > 0 && (data[end - 1] == ' ' || data[end - 1] == '\0'))
    {
      --end;
    }
  return std::string {data, data + end};
}

int trim_length (char const* data, int width)
{
  int end = width;
  while (end > 0 && (data[end - 1] == ' ' || data[end - 1] == '\0'))
    {
      --end;
    }
  return end;
}

float gfsk_pulse (float bt, float t)
{
  float const c = 0.5f * kFt8TwoPi * std::sqrt (2.0f / std::log (2.0f));
  return 0.5f * (std::erf (c * bt * (t + 0.5f)) - std::erf (c * bt * (t - 0.5f)));
}

float wrap_phase (float phase)
{
  phase = std::fmod (phase, kFt8TwoPi);
  if (phase < 0.0f)
    {
      phase += kFt8TwoPi;
    }
  return phase;
}

std::array<std::complex<float>, kFt8PhaseTableSize> const& phase_table ()
{
  static std::array<std::complex<float>, kFt8PhaseTableSize> table = [] {
    std::array<std::complex<float>, kFt8PhaseTableSize> value {};
    for (int i = 0; i < kFt8PhaseTableSize; ++i)
      {
        float const phase = static_cast<float> (i) * kFt8TwoPi
                            / static_cast<float> (kFt8PhaseTableSize);
        value[static_cast<size_t> (i)] = {
            std::cos (phase),
            std::sin (phase)
        };
      }
    return value;
  }();
  return table;
}

void generate_ft8_a8_waveform (int const* itone, float f0,
                               std::array<std::complex<float>, kFt8A8Nwave>& cwave)
{
  cwave.fill (std::complex<float> {});
  if (!itone || !std::isfinite (f0))
    {
      return;
    }

  static std::array<float, 3 * kFt8A8Nsps> const pulse = [] {
    std::array<float, 3 * kFt8A8Nsps> value {};
    for (int i = 0; i < 3 * kFt8A8Nsps; ++i)
      {
        float const tt = (static_cast<float> (i + 1) - 1.5f * static_cast<float> (kFt8A8Nsps))
                         / static_cast<float> (kFt8A8Nsps);
        value[static_cast<size_t> (i)] = gfsk_pulse (kFt8A8Bt, tt);
      }
    return value;
  }();

  std::array<float, kFt8A8DphiSize> dphi {};
  float const dphi_peak = kFt8TwoPi / static_cast<float> (kFt8A8Nsps);
  for (int j = 0; j < kFt8A8Nsym; ++j)
    {
      int const ib = j * kFt8A8Nsps;
      int const tone = std::max (0, std::min (7, itone[j]));
      for (int i = 0; i < 3 * kFt8A8Nsps; ++i)
        {
          dphi[static_cast<size_t> (ib + i)] += dphi_peak * pulse[static_cast<size_t> (i)]
                                                * static_cast<float> (tone);
        }
    }

  int const first_tone = std::max (0, std::min (7, itone[0]));
  int const last_tone = std::max (0, std::min (7, itone[kFt8A8Nsym - 1]));
  for (int i = 0; i < 2 * kFt8A8Nsps; ++i)
    {
      dphi[static_cast<size_t> (i)] += dphi_peak * static_cast<float> (first_tone)
                                       * pulse[static_cast<size_t> (kFt8A8Nsps + i)];
      dphi[static_cast<size_t> (kFt8A8Nsym * kFt8A8Nsps + i)] +=
          dphi_peak * static_cast<float> (last_tone) * pulse[static_cast<size_t> (i)];
    }

  float const carrier_step = kFt8TwoPi * f0 / kFt8A7Fs2;
  auto const& table = phase_table ();
  float phi = 0.0f;
  for (int j = kFt8A8Nsps, k = 0; j < kFt8A8Nsps + kFt8A8Nwave; ++j, ++k)
    {
      int const index = std::max (
          0,
          std::min (kFt8PhaseTableSize - 1,
                    static_cast<int> (phi * static_cast<float> (kFt8PhaseTableSize) / kFt8TwoPi)));
      cwave[static_cast<size_t> (k)] = table[static_cast<size_t> (index)];
      phi = wrap_phase (phi + dphi[static_cast<size_t> (j)] + carrier_step);
    }

  int const nramp = std::max (1, static_cast<int> (std::lround (kFt8A8Nsps / 8.0f)));
  for (int i = 0; i < nramp; ++i)
    {
      float const env = 0.5f * (1.0f - std::cos (kFt8TwoPi * static_cast<float> (i)
                                                 / (2.0f * static_cast<float> (nramp))));
      cwave[static_cast<size_t> (i)] *= env;
    }
  int const tail_start = kFt8A8Nsym * kFt8A8Nsps - nramp;
  for (int i = 0; i < nramp; ++i)
    {
      float const env = 0.5f * (1.0f + std::cos (kFt8TwoPi * static_cast<float> (i)
                                                 / (2.0f * static_cast<float> (nramp))));
      cwave[static_cast<size_t> (tail_start + i)] *= env;
    }
}

template <size_t N>
bool starts_with (FixedChars<N> const& value, char const* prefix)
{
  size_t const length = std::strlen (prefix);
  return length <= N && std::equal (prefix, prefix + length, value.begin ());
}

bool is_standard_call (char const* data, int width)
{
  auto is_digit = [] (char c) {
    return c >= '0' && c <= '9';
  };
  auto is_letter = [] (char c) {
    return c >= 'A' && c <= 'Z';
  };

  std::string const call = trim_block (data, width);
  if (call.empty () || call.size () > 11)
    {
      return false;
    }
  if (call.find ('.') != std::string::npos
      || call.find ('+') != std::string::npos
      || call.find ('-') != std::string::npos
      || call.find ('?') != std::string::npos)
    {
      return false;
    }
  if (call.size () > 6 && call.find ('/') == std::string::npos)
    {
      return false;
    }

  std::string base = call.substr (0, std::min<size_t> (6, call.size ()));
  size_t const slash = call.find ('/');
  if (slash != std::string::npos && slash >= 1 && slash + 1 < call.size ())
    {
      size_t const left = slash;
      size_t const right = call.size () - slash - 1;
      base = left <= right ? call.substr (slash + 1) : call.substr (0, slash);
    }
  if (base.size () > 6)
    {
      return false;
    }
  if ((base.size () < 1 || !is_letter (base[0]))
      && (base.size () < 2 || !is_letter (base[1])))
    {
      return false;
    }
  if (base[0] == 'Q' && base.rfind ("QU1RK", 0) != 0)
    {
      return false;
    }

  int digit_index = -1;
  if (base.size () >= 2 && is_digit (base[1]))
    {
      digit_index = 1;
    }
  if (base.size () >= 3 && is_digit (base[2]))
    {
      digit_index = 2;
    }
  if (digit_index < 0 || digit_index + 1 >= static_cast<int> (base.size ()))
    {
      return false;
    }

  int suffix_letters = 0;
  for (size_t i = static_cast<size_t> (digit_index + 1); i < base.size (); ++i)
    {
      if (!is_letter (base[i]))
        {
          return false;
        }
      ++suffix_letters;
    }
  return suffix_letters >= 1 && suffix_letters <= 3;
}

bool is_grid4 (std::string const& grid)
{
  auto between = [] (char value, char lo, char hi) {
    return value >= lo && value <= hi;
  };

  return grid.size () == 4
      && between (grid[0], 'A', 'R')
      && between (grid[1], 'A', 'R')
      && between (grid[2], '0', '9')
      && between (grid[3], '0', '9');
}

bool is_standard_call_word (std::string const& word)
{
  if (word.empty () || word.size () > 12)
    {
      return false;
    }

  FixedChars<12> call = blank_fixed<12> ();
  std::copy_n (word.data (), word.size (), call.data ());
  return is_standard_call (call.data (), static_cast<int> (call.size ()));
}

bool is_cq_modifier (std::string const& word)
{
  return word == "DX"
      || word == "NA"
      || word == "SA"
      || word == "EU"
      || word == "AF"
      || word == "AS"
      || word == "OC"
      || word == "TEST";
}

bool is_report_token (std::string const& word)
{
  if (word == "RRR" || word == "RR73" || word == "73")
    {
      return true;
    }
  size_t offset = 0;
  if (!word.empty () && word[0] == 'R')
    {
      offset = 1;
    }
  if (word.size () != offset + 3)
    {
      return false;
    }
  return (word[offset] == '-' || word[offset] == '+')
      && word[offset + 1] >= '0' && word[offset + 1] <= '9'
      && word[offset + 2] >= '0' && word[offset + 2] <= '9';
}

bool is_standard_ft8_exchange_tail (std::string const& word)
{
  return is_grid4 (word) || is_report_token (word);
}

std::vector<std::string> split_words (std::string const& text);

bool is_strict_standard_ft8_message (FixedChars<kFt8DecodedChars> const& decoded)
{
  std::vector<std::string> const words = split_words (trim_fixed (decoded));
  if (words.size () < 2 || words.size () > 4)
    {
      return false;
    }

  if (words[0] == "CQ")
    {
      size_t callIndex = 1;
      if (words.size () >= 3 && is_cq_modifier (words[1]))
        {
          callIndex = 2;
        }
      if (callIndex >= words.size () || !is_standard_call_word (words[callIndex]))
        {
          return false;
        }
      return callIndex + 1 == words.size ()
          || (callIndex + 2 == words.size () && is_grid4 (words[callIndex + 1]));
    }

  if (!is_standard_call_word (words[0]) || !is_standard_call_word (words[1]))
    {
      return false;
    }
  if (words.size () == 2)
    {
      return true;
    }
  if (!is_standard_ft8_exchange_tail (words[2]))
    {
      return false;
    }
  return words.size () == 3;
}

std::vector<std::string> split_words (std::string const& text)
{
  std::istringstream stream {text};
  std::vector<std::string> words;
  std::string word;
  while (stream >> word)
    {
      words.push_back (word);
    }
  return words;
}

std::string format_ft8_stdout_line (int nutc, float sync, int snr, float dt, float freq,
                                    std::string const& decoded, int nap, float qual)
{
  (void) sync;
  std::string decoded_copy = decoded;
  if (decoded_copy.size () < kFt8DecodedChars)
    {
      decoded_copy.append (kFt8DecodedChars - decoded_copy.size (), ' ');
    }
  else if (decoded_copy.size () > kFt8DecodedChars)
    {
      decoded_copy.resize (kFt8DecodedChars);
    }

  std::string annot {"  "};
  if (nap != 0)
    {
      annot[0] = 'a';
      annot[1] = static_cast<char> ('0' + std::max (0, std::min (9, nap)));
      if (qual < 0.17f && !decoded_copy.empty ())
        {
          decoded_copy.back () = '?';
        }
    }

  std::ostringstream line;
  line << std::setfill ('0') << std::setw (6) << nutc
       << std::setfill (' ') << std::setw (4) << snr
       << std::fixed << std::setprecision (1) << std::setw (5) << dt
       << std::setw (5) << static_cast<int> (std::lround (freq))
       << " ~ " << ' ' << decoded_copy << ' ' << annot;
  return line.str ();
}

std::string format_ft8_decoded_file_line (int nutc, float sync, int snr, float dt, float freq,
                                          std::string const& decoded, int nap, float qual)
{
  std::string decoded_copy = decoded;
  if (decoded_copy.size () < kFt8DecodedChars)
    {
      decoded_copy.append (kFt8DecodedChars - decoded_copy.size (), ' ');
    }
  else if (decoded_copy.size () > kFt8DecodedChars)
    {
      decoded_copy.resize (kFt8DecodedChars);
    }
  if (nap != 0 && qual < 0.17f && !decoded_copy.empty ())
    {
      decoded_copy.back () = '?';
    }

  std::ostringstream line;
  line << std::setfill ('0') << std::setw (6) << nutc
       << std::setfill (' ') << std::setw (4) << static_cast<int> (std::lround (sync))
       << std::setw (5) << snr
       << std::fixed << std::setprecision (1) << std::setw (6) << dt
       << std::fixed << std::setprecision (0) << std::setw (8) << freq
       << std::setw (4) << 0
       << "   " << decoded_copy << " FT8";
  return line.str ();
}

constexpr int kSuperFoxPackedSymbols {50};
constexpr int kSuperFoxMaxDecodeLines {16};
constexpr int kSuperFoxNqu1Rks {203514677};
constexpr int kSuperFoxNFilt {8000};
constexpr int kSuperFoxToneFrameSamples {50 * 3456};
constexpr int kSuperFoxQpcRows {128};
constexpr int kSuperFoxQpcCols {128};
constexpr int kSuperFoxDemodSymbols {151};
constexpr int kSuperFoxDemodNsps {1024};
constexpr int kSuperFoxDemodSpectrumCols {kSuperFoxDemodSymbols + 1};
constexpr int kSuperFoxQpcSearchCount {100};
constexpr int kSuperFoxSyncSamples {9 * 12000};
constexpr int kSuperFoxSyncDown {16};
constexpr int kSuperFoxSyncNz {kSuperFoxSyncSamples / kSuperFoxSyncDown};
constexpr int kSuperFoxSyncSymbols {24};
constexpr std::array<int, kSuperFoxSyncSymbols> kSuperFoxIsync {
    1, 2, 4, 7, 11, 16, 22, 29, 37, 39, 42, 43,
    45, 48, 52, 57, 63, 70, 78, 80, 83, 84, 86, 89};
constexpr std::array<int, 8> kSuperFoxMaxDither {
    20, 50, 100, 200, 500, 1000, 2000, 5000};
constexpr std::array<int, kSuperFoxQpcSearchCount> kSuperFoxIdf {
    0,  0, -1,  0, -1,  1,  0, -1,  1, -2,  0, -1,  1, -2,  2,
    0, -1,  1, -2,  2, -3,  0, -1,  1, -2,  2, -3,  3,  0, -1,
    1, -2,  2, -3,  3, -4,  0, -1,  1, -2,  2, -3,  3, -4,  4,
    0, -1,  1, -2,  2, -3,  3, -4,  4, -5, -1,  1, -2,  2, -3,
    3, -4,  4, -5,  1, -2,  2, -3,  3, -4,  4, -5, -2,  2, -3,
    3, -4,  4, -5,  2, -3,  3, -4,  4, -5, -3,  3, -4,  4, -5,
    3, -4,  4, -5, -4,  4, -5,  4, -5, -5};
constexpr std::array<int, kSuperFoxQpcSearchCount> kSuperFoxIdt {
    0, -1,  0,  1, -1,  0, -2,  1, -1,  0,  2, -2,  1, -1,  0,
   -3,  2, -2,  1, -1,  0,  3, -3,  2, -2,  1, -1,  0, -4,  3,
   -3,  2, -2,  1, -1,  0,  4, -4,  3, -3,  2, -2,  1, -1,  0,
   -5,  4, -4,  3, -3,  2, -2,  1, -1,  0, -5,  4, -4,  3, -3,
    2, -2,  1, -1, -5,  4, -4,  3, -3,  2, -2,  1, -5,  4, -4,
    3, -3,  2, -2, -5,  4, -4,  3, -3,  2, -5,  4, -4,  3, -3,
   -5,  4, -4,  3, -5,  4, -4, -5,  4, -5};
constexpr char kSuperFoxBase38[] {" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/"};

struct SuperFoxComplexFft
{
  SuperFoxComplexFft ()
    : data (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * kFt8NMax))),
      forward (nullptr),
      inverse (nullptr)
  {
    if (data)
      {
        forward = fftwf_plan_dft_1d (kFt8NMax, data, data, FFTW_FORWARD, FFTW_ESTIMATE);
        inverse = fftwf_plan_dft_1d (kFt8NMax, data, data, FFTW_BACKWARD, FFTW_ESTIMATE);
      }
  }

  ~SuperFoxComplexFft ()
  {
    if (forward)
      {
        fftwf_destroy_plan (forward);
      }
    if (inverse)
      {
        fftwf_destroy_plan (inverse);
      }
    if (data)
      {
        fftwf_free (data);
      }
  }

  bool valid () const
  {
    return data && forward && inverse;
  }

  std::complex<float>* values ()
  {
    return reinterpret_cast<std::complex<float>*> (data);
  }

  fftwf_complex* data;
  fftwf_plan forward;
  fftwf_plan inverse;
};

SuperFoxComplexFft& superfox_fft ()
{
  static SuperFoxComplexFft fft;
  return fft;
}

struct SuperFoxSyncFft
{
  SuperFoxSyncFft ()
    : data (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * kSuperFoxSyncSamples))),
      forward (nullptr)
  {
    if (data)
      {
        forward = fftwf_plan_dft_1d (kSuperFoxSyncSamples, data, data, FFTW_FORWARD, FFTW_ESTIMATE);
      }
  }

  ~SuperFoxSyncFft ()
  {
    if (forward)
      {
        fftwf_destroy_plan (forward);
      }
    if (data)
      {
        fftwf_free (data);
      }
  }

  bool valid () const
  {
    return data && forward;
  }

  std::complex<float>* values ()
  {
    return reinterpret_cast<std::complex<float>*> (data);
  }

  fftwf_complex* data;
  fftwf_plan forward;
};

SuperFoxSyncFft& superfox_sync_fft ()
{
  static SuperFoxSyncFft fft;
  return fft;
}

struct SuperFoxDemodFft
{
  SuperFoxDemodFft ()
    : data (reinterpret_cast<fftwf_complex*> (
          fftwf_malloc (sizeof (fftwf_complex) * kSuperFoxDemodNsps))),
      forward (nullptr)
  {
    if (data)
      {
        forward = fftwf_plan_dft_1d (kSuperFoxDemodNsps, data, data, FFTW_FORWARD, FFTW_ESTIMATE);
      }
  }

  ~SuperFoxDemodFft ()
  {
    if (forward)
      {
        fftwf_destroy_plan (forward);
      }
    if (data)
      {
        fftwf_free (data);
      }
  }

  bool valid () const
  {
    return data && forward;
  }

  std::complex<float>* values ()
  {
    return reinterpret_cast<std::complex<float>*> (data);
  }

  fftwf_complex* data;
  fftwf_plan forward;
};

SuperFoxDemodFft& superfox_demod_fft ()
{
  static SuperFoxDemodFft fft;
  return fft;
}

std::vector<std::complex<float>> const& superfox_smoothing_window ()
{
  static std::vector<std::complex<float>> cwindow = [] {
    std::vector<std::complex<float>> filter (static_cast<size_t> (kFt8NMax));
    SuperFoxComplexFft& fft = superfox_fft ();
    if (!fft.valid ())
      {
        return filter;
      }

    std::complex<float>* scratch = fft.values ();
    std::fill_n (scratch, kFt8NMax, std::complex<float> {});

    float sumw = 0.0f;
    std::array<float, kSuperFoxNFilt + 1> window {};
    for (int j = -kSuperFoxNFilt / 2; j <= kSuperFoxNFilt / 2; ++j)
      {
        float const value = std::pow (std::cos ((0.5f * kFt8TwoPi) * static_cast<float> (j)
                                                / static_cast<float> (kSuperFoxNFilt)), 2.0f);
        window[static_cast<size_t> (j + kSuperFoxNFilt / 2)] = value;
        sumw += value;
      }

    int const shift = kSuperFoxNFilt / 2 + 1;
    for (int src = 0; src <= kSuperFoxNFilt; ++src)
      {
        int dst = src - shift;
        if (dst < 0)
          {
            dst += kFt8NMax;
          }
        scratch[static_cast<size_t> (dst)] =
            std::complex<float> {window[static_cast<size_t> (src)] / sumw, 0.0f};
      }

    fftwf_execute (fft.forward);

    float const fac = 1.0f / static_cast<float> (kFt8NMax);
    for (int i = 0; i < kFt8NMax; ++i)
      {
        filter[static_cast<size_t> (i)] = scratch[static_cast<size_t> (i)] * fac;
      }
    return filter;
  }();
  return cwindow;
}

bool superfox_analytic_signal (float const* dd, int npts, std::complex<float>* c0)
{
  if (!dd || !c0 || npts <= 0)
    {
      return false;
    }

  std::vector<std::complex<float>> spectrum (static_cast<size_t> (npts));
  float const scale = 2.0f / (32767.0f * static_cast<float> (npts));
  for (int i = 0; i < npts; ++i)
    {
      spectrum[static_cast<size_t> (i)] = std::complex<float> {scale * dd[i], 0.0f};
    }

  auto* fft_data = reinterpret_cast<fftwf_complex*> (spectrum.data ());
  fftwf_plan const forward =
      fftwf_plan_dft_1d (npts, fft_data, fft_data, FFTW_FORWARD, FFTW_ESTIMATE);
  fftwf_plan const inverse =
      fftwf_plan_dft_1d (npts, fft_data, fft_data, FFTW_BACKWARD, FFTW_ESTIMATE);
  if (!forward || !inverse)
    {
      if (forward)
        {
          fftwf_destroy_plan (forward);
        }
      if (inverse)
        {
          fftwf_destroy_plan (inverse);
        }
      return false;
    }

  fftwf_execute (forward);
  for (int i = npts / 2 + 1; i < npts; ++i)
    {
      spectrum[static_cast<size_t> (i)] = std::complex<float> {};
    }
  spectrum[0] *= 0.5f;
  fftwf_execute (inverse);

  for (int i = 0; i < npts; ++i)
    {
      c0[static_cast<size_t> (i)] = spectrum[static_cast<size_t> (i)];
    }

  fftwf_destroy_plan (forward);
  fftwf_destroy_plan (inverse);
  return true;
}

bool superfox_remove_tone (std::complex<float>* c0, float fsync)
{
  if (!c0)
    {
      return false;
    }

  if (fsync > 1400.0f)
    {
      return true;
    }

  SuperFoxComplexFft& fft = superfox_fft ();
  if (!fft.valid ())
    {
      return false;
    }

  std::vector<std::complex<float>> const& cwindow = superfox_smoothing_window ();
  if (cwindow.empty ())
    {
      return false;
    }

  constexpr float kSampleRate = 12000.0f;
  float const baud = kSampleRate / 1024.0f;
  float const df = kSampleRate / static_cast<float> (kFt8NMax);
  float const fac = 1.0f / static_cast<float> (kFt8NMax);
  int const nbaud = static_cast<int> (std::lround (baud / df));

  std::vector<std::complex<float>> cref (static_cast<size_t> (kFt8NMax));
  std::vector<float> s (static_cast<size_t> (kFt8NMax / 4));
  std::complex<float>* cfilt = fft.values ();

  for (int i = 0; i < kFt8NMax; ++i)
    {
      cfilt[static_cast<size_t> (i)] = c0[static_cast<size_t> (i)] * fac;
    }
  fftwf_execute (fft.forward);

  for (int i = 1; i <= kFt8NMax / 4; ++i)
    {
      s[static_cast<size_t> (i - 1)] = std::norm (cfilt[static_cast<size_t> (i - 1)]);
    }

  int ia = static_cast<int> (std::lround ((fsync - 50.0f) / df));
  int ib = static_cast<int> (std::lround ((fsync + 1550.0f) / df));
  ia = std::max (1, ia);
  ib = std::min (kFt8NMax / 4, ib);
  if (ia > ib)
    {
      return true;
    }

  int i0 = ia;
  float peak = s[static_cast<size_t> (ia - 1)];
  for (int i = ia + 1; i <= ib; ++i)
    {
      float const value = s[static_cast<size_t> (i - 1)];
      if (value > peak)
        {
          peak = value;
          i0 = i;
        }
    }

  ia = std::max (1, i0 - nbaud);
  ib = std::min (kFt8NMax / 4, i0 + nbaud);

  float s0 = 0.0f;
  float s1 = 0.0f;
  for (int i = ia; i <= ib; ++i)
    {
      float const value = s[static_cast<size_t> (i - 1)];
      s0 += value;
      s1 += static_cast<float> (i - i0) * value;
    }
  if (s0 <= 0.0f)
    {
      return true;
    }

  float const delta = s1 / s0;
  i0 = static_cast<int> (std::lround (static_cast<float> (i0) + delta));
  float const f2 = static_cast<float> (i0) * df;

  ia = std::max (1, i0 - nbaud);
  ib = std::min (kFt8NMax / 4, i0 + nbaud);

  float s2 = 0.0f;
  for (int i = ia; i <= ib; ++i)
    {
      float const delta_i = static_cast<float> (i - i0);
      s2 += s[static_cast<size_t> (i - 1)] * delta_i * delta_i;
    }

  float const sigma = std::sqrt (s2 / s0) * df;
  if (sigma > 2.5f)
    {
      return true;
    }

  float const dt = 1.0f / kSampleRate;
  for (int i = 0; i < kFt8NMax; ++i)
    {
      float const arg = kFt8TwoPi * f2 * static_cast<float> (i + 1) * dt;
      cref[static_cast<size_t> (i)] = std::complex<float> {std::cos (arg), std::sin (arg)};
      cfilt[static_cast<size_t> (i)] = c0[static_cast<size_t> (i)] * std::conj (cref[static_cast<size_t> (i)]);
    }

  fftwf_execute (fft.forward);
  for (int i = 0; i < kFt8NMax; ++i)
    {
      cfilt[static_cast<size_t> (i)] *= cwindow[static_cast<size_t> (i)];
    }
  fftwf_execute (fft.inverse);

  int const nframe = std::min (kSuperFoxToneFrameSamples, kFt8NMax);
  for (int i = 0; i < nframe; ++i)
    {
      std::complex<float> const tone = cfilt[static_cast<size_t> (i)] * cref[static_cast<size_t> (i)];
      c0[static_cast<size_t> (i)] -= tone;
    }

  return true;
}

float db_compat (float x)
{
  if (x <= 1.259e-10f)
    {
      return -99.0f;
    }
  return 10.0f * std::log10 (x);
}

void smooth_121 (float* values, int count)
{
  if (!values || count <= 2)
    {
      return;
    }

  float x0 = values[0];
  for (int i = 1; i < count - 1; ++i)
    {
      float const x1 = values[i];
      values[i] = 0.5f * values[i] + 0.25f * (x0 + values[i + 1]);
      x0 = x1;
    }
}

float superfox_percentile (float const* values, int count, int percentile)
{
  if (!values || count <= 0 || percentile < 0 || percentile > 100)
    {
      return 1.0f;
    }

  std::vector<float> sorted (values, values + count);
  int const one_based = std::max (1, std::min (count,
                                               static_cast<int> (std::lround (
                                                   static_cast<double> (count) * 0.01
                                                   * static_cast<double> (percentile)))));
  std::nth_element (sorted.begin (), sorted.begin () + (one_based - 1), sorted.end ());
  return sorted[static_cast<size_t> (one_based - 1)];
}

void superfox_twkfreq2 (std::complex<float> const* c3, std::complex<float>* c4,
                        int npts, float fsample, float fshift)
{
  if (!c3 || !c4 || npts <= 0 || fsample <= 0.0f)
    {
      return;
    }

  std::complex<float> w {1.0f, 0.0f};
  float const dphi = fshift * kFt8TwoPi / fsample;
  std::complex<float> const wstep {std::cos (dphi), std::sin (dphi)};
  for (int i = 0; i < npts; ++i)
    {
      w *= wstep;
      c4[static_cast<size_t> (i)] = w * c3[static_cast<size_t> (i)];
    }
}

void smooth_121a (float* values, int count, float a, float b)
{
  if (!values || count <= 2)
    {
      return;
    }

  float const fac = 1.0f / (a + 2.0f * b);
  float x0 = values[0];
  for (int i = 1; i < count - 1; ++i)
    {
      float const x1 = values[i];
      values[i] = fac * (a * values[i] + b * (x0 + values[i + 1]));
      x0 = x1;
    }
}

bool superfox_demod (std::complex<float> const* crcvd, float f, float t,
                     float* s2_out, float* s3_out)
{
  if (!crcvd || !s2_out || !s3_out)
    {
      return false;
    }

  SuperFoxDemodFft& fft = superfox_demod_fft ();
  if (!fft.valid ())
    {
      return false;
    }

  std::fill_n (s2_out, kSuperFoxQpcRows * kSuperFoxDemodSpectrumCols, 0.0f);
  std::fill_n (s3_out, kSuperFoxQpcRows * kSuperFoxQpcCols, 0.0f);

  int const j0 = static_cast<int> (std::lround (12000.0f * (t + 0.5f)));
  float const df = 12000.0f / static_cast<float> (kSuperFoxDemodNsps);
  int const i0 = static_cast<int> (std::lround (f / df)) - kSuperFoxQpcRows / 2;
  if (i0 < 0 || i0 + kSuperFoxQpcRows > kSuperFoxDemodNsps)
    {
      return false;
    }

  std::complex<float>* spectrum = fft.values ();
  int k2 = 0;
  for (int n = 1; n <= kSuperFoxDemodSymbols; ++n)
    {
      int const jb = n * kSuperFoxDemodNsps + j0;
      int const ja = jb - kSuperFoxDemodNsps + 1;
      ++k2;
      if (ja < 1 || jb > kFt8NMax)
        {
          continue;
        }

      std::copy_n (crcvd + (ja - 1), kSuperFoxDemodNsps, spectrum);
      fftwf_execute (fft.forward);
      for (int row = 0; row < kSuperFoxQpcRows; ++row)
        {
          s2_out[row + kSuperFoxQpcRows * k2] =
              std::norm (spectrum[static_cast<size_t> (i0 + row)]);
        }
    }

  float const base2 = superfox_percentile (s2_out, kSuperFoxQpcRows * kSuperFoxDemodSymbols, 50);
  if (base2 > 0.0f)
    {
      float const inv = 1.0f / base2;
      for (int i = 0; i < kSuperFoxQpcRows * kSuperFoxDemodSpectrumCols; ++i)
        {
          s2_out[i] *= inv;
        }
    }

  std::array<int, kSuperFoxQpcRows> hist1 {};
  std::array<int, kSuperFoxQpcRows> hist2 {};
  for (int col = 0; col <= kSuperFoxDemodSymbols; ++col)
    {
      int best_row = 1;
      float best = s2_out[1 + kSuperFoxQpcRows * col];
      for (int row = 2; row < kSuperFoxQpcRows; ++row)
        {
          float const value = s2_out[row + kSuperFoxQpcRows * col];
          if (value > best)
            {
              best = value;
              best_row = row;
            }
        }
      ++hist1[static_cast<size_t> (best_row - 1)];
    }

  hist1[0] = 0;
  for (int i = 0; i <= 123; ++i)
    {
      hist2[static_cast<size_t> (i)] = hist1[static_cast<size_t> (i)]
                                       + hist1[static_cast<size_t> (i + 1)]
                                       + hist1[static_cast<size_t> (i + 2)]
                                       + hist1[static_cast<size_t> (i + 3)];
    }

  int i1 = 0;
  int m1 = hist1[0];
  for (int i = 1; i < kSuperFoxQpcRows; ++i)
    {
      if (hist1[static_cast<size_t> (i)] > m1)
        {
          m1 = hist1[static_cast<size_t> (i)];
          i1 = i;
        }
    }
  (void) i1;

  int i2 = 0;
  int m2 = hist2[0];
  for (int i = 1; i < kSuperFoxQpcRows; ++i)
    {
      if (hist2[static_cast<size_t> (i)] > m2)
        {
          m2 = hist2[static_cast<size_t> (i)];
          i2 = i;
        }
    }

  if (m1 > 12)
    {
      for (int row = 0; row < kSuperFoxQpcRows; ++row)
        {
          if (hist1[static_cast<size_t> (row)] > 12)
            {
              for (int col = 0; col <= kSuperFoxDemodSymbols; ++col)
                {
                  s2_out[row + kSuperFoxQpcRows * col] = 1.0f;
                }
            }
        }
    }

  if (m2 > 20)
    {
      if (i2 >= 1)
        {
          --i2;
        }
      if (i2 > 120)
        {
          i2 = 120;
        }
      for (int row = i2; row <= i2 + 7; ++row)
        {
          for (int col = 0; col <= kSuperFoxDemodSymbols; ++col)
            {
              s2_out[row + kSuperFoxQpcRows * col] = 1.0f;
            }
        }
    }

  int k3 = 0;
  for (int n = 1; n <= kSuperFoxDemodSymbols; ++n)
    {
      if (std::find (kSuperFoxIsync.begin (), kSuperFoxIsync.end (), n) != kSuperFoxIsync.end ())
        {
          continue;
        }
      ++k3;
      for (int row = 0; row < kSuperFoxQpcRows; ++row)
        {
          s3_out[row + kSuperFoxQpcRows * k3] = s2_out[row + kSuperFoxQpcRows * n];
        }
    }

  float const base3 = superfox_percentile (s3_out, kSuperFoxQpcRows * kSuperFoxQpcCols, 50);
  if (base3 > 0.0f)
    {
      float const inv = 1.0f / base3;
      for (int i = 0; i < kSuperFoxQpcRows * kSuperFoxQpcCols; ++i)
        {
          s3_out[i] *= inv;
        }
    }

  return true;
}

struct SuperFoxQpcDecodeResult
{
  std::array<unsigned char, kSuperFoxPackedSymbols> xdec {};
  bool crc_ok {false};
  float snrsync {0.0f};
  float fbest {0.0f};
  float tbest {0.0f};
  float snr {0.0f};
};

bool superfox_qpc_sync (std::complex<float> const* crcvd0, float fsample, float fsync, float ftol,
                        float& f2, float& t2, float& snrsync);
void superfox_qpc_likelihoods (float const* s3, int rows, int cols, float EsNo, float No,
                               float* py_out);
float superfox_qpc_snr (float const* s3, int rows, int cols, unsigned char const* y);

bool superfox_qpc_decode2 (std::complex<float> const* c0, float fsync, float ftol,
                           int ndepth, float dth, float damp,
                           SuperFoxQpcDecodeResult& result)
{
  if (!c0)
    {
      return false;
    }

  float f2 = 0.0f;
  float t2 = 0.0f;
  if (!superfox_qpc_sync (c0, 12000.0f, fsync, ftol, f2, t2, result.snrsync))
    {
      return false;
    }

  float const baud = 12000.0f / 1024.0f;
  float const f00 = 1500.0f + f2;
  float const t00 = t2;
  result.fbest = f00;
  result.tbest = t00;
  result.crc_ok = false;
  result.snr = 0.0f;
  result.xdec.fill (0);

  int maxd = 1;
  if (ndepth > 0)
    {
      int const depth_index = std::max (0, std::min (ndepth - 1,
                                                     static_cast<int> (kSuperFoxMaxDither.size ()) - 1));
      maxd = kSuperFoxMaxDither[static_cast<size_t> (depth_index)];
    }

  int maxft = kSuperFoxQpcSearchCount;
  if (result.snrsync < 4.0f || ndepth <= 0)
    {
      maxft = 1;
    }

  std::vector<std::complex<float>> shifted (static_cast<size_t> (kFt8NMax));
  std::vector<float> py (static_cast<size_t> (kSuperFoxQpcRows * kSuperFoxQpcCols));
  std::vector<float> py0 (static_cast<size_t> (kSuperFoxQpcRows * kSuperFoxQpcCols));
  std::vector<float> pyd (static_cast<size_t> (kSuperFoxQpcRows * kSuperFoxQpcCols));
  std::vector<float> s2 (static_cast<size_t> (kSuperFoxQpcRows * kSuperFoxDemodSpectrumCols));
  std::vector<float> s3 (static_cast<size_t> (kSuperFoxQpcRows * kSuperFoxQpcCols));
  std::array<unsigned char, kSuperFoxPackedSymbols> xdec {};
  std::array<unsigned char, kSuperFoxQpcCols> ydec {};

  constexpr std::array<int, 33> kSeed {
      321278106,  -658879006,  1239150429,  -941466001, -698554454,
      1136210962,  1633585627,  1261915021, -1134191465, -487888229,
      2131958895, -1429290834, -1802468092,  1801346659, 1966248904,
      402671397, -1961400750, -1567227835,  1895670987, -286583128,
      -595933665, -1699285543,  1518291336,  1338407128,  838354404,
      -2081343776, -1449416716,  1236537391,  -133197638,  337355509,
      -460640480,  1592689606,          0};
  std::array<uint32_t, kSeed.size ()> seed_data {};
  for (size_t i = 0; i < kSeed.size (); ++i)
    {
      seed_data[i] = static_cast<uint32_t> (kSeed[i]);
    }

  constexpr uint32_t kMask21 = (1u << 21) - 1u;
  for (int idith = 1; idith <= maxft; ++idith)
    {
      if (idith >= 2)
        {
          maxd = 1;
        }

      float const deltaf = static_cast<float> (kSuperFoxIdf[static_cast<size_t> (idith - 1)]) * 0.5f;
      float const deltat = static_cast<float> (kSuperFoxIdt[static_cast<size_t> (idith - 1)])
                           * 8.0f / 1024.0f;
      float const f = f00 + deltaf;
      float const t = t00 + deltat;
      float const fshift = 1500.0f - (f + baud);
      superfox_twkfreq2 (c0, shifted.data (), kFt8NMax, 12000.0f, fshift);

      float const a = 1.0f;
      for (int kk = 1; kk <= 4; ++kk)
        {
          float b = 0.0f;
          if (kk == 2)
            {
              b = 0.4f;
            }
          else if (kk == 3)
            {
              b = 0.5f;
            }
          else if (kk == 4)
            {
              b = 0.6f;
            }

          if (!superfox_demod (shifted.data (), 1500.0f, t, s2.data (), s3.data ()))
            {
              return false;
            }

          if (b > 0.0f)
            {
              for (int col = 0; col < kSuperFoxQpcCols; ++col)
                {
                  smooth_121a (s3.data () + kSuperFoxQpcRows * col, kSuperFoxQpcRows, a, b);
                }
            }

          float const base3 = superfox_percentile (s3.data (), kSuperFoxQpcRows * kSuperFoxQpcCols, 50);
          if (base3 > 0.0f)
            {
              float const inv = 1.0f / base3;
              for (float& value : s3)
                {
                  value *= inv;
                }
            }

          constexpr float kEsNoDec = 3.16f;
          constexpr float kNo = 1.0f;
          py0 = s3;
          superfox_qpc_likelihoods (s3.data (), kSuperFoxQpcRows, kSuperFoxQpcCols, kEsNoDec, kNo,
                                    py.data ());

          std::seed_seq seed_seq (seed_data.begin (), seed_data.end ());
          std::mt19937 engine (seed_seq);
          std::uniform_real_distribution<float> uniform (0.0f, 1.0f);
          for (int kkk = 1; kkk <= maxd; ++kkk)
            {
              if (kkk == 1)
                {
                  pyd = py0;
                }
              else
                {
                  std::fill (pyd.begin (), pyd.end (), 0.0f);
                  if (kkk > 2)
                    {
                      for (float& value : pyd)
                        {
                          value = 2.0f * (uniform (engine) - 0.5f);
                        }
                    }
                  for (size_t i = 0; i < pyd.size (); ++i)
                    {
                      if (py[i] > dth)
                        {
                          pyd[i] = 0.0f;
                        }
                    }
                  for (size_t i = 0; i < pyd.size (); ++i)
                    {
                      pyd[i] = py[i] * (1.0f + damp * pyd[i]);
                    }
                }

              for (int col = 0; col < kSuperFoxQpcCols; ++col)
                {
                  float ss = 0.0f;
                  for (int row = 0; row < kSuperFoxQpcRows; ++row)
                    {
                      ss += pyd[static_cast<size_t> (row + kSuperFoxQpcRows * col)];
                    }
                  if (ss > 0.0f)
                    {
                      float const inv = 1.0f / ss;
                      for (int row = 0; row < kSuperFoxQpcRows; ++row)
                        {
                          pyd[static_cast<size_t> (row + kSuperFoxQpcRows * col)] *= inv;
                        }
                    }
                  else
                    {
                      for (int row = 0; row < kSuperFoxQpcRows; ++row)
                        {
                          pyd[static_cast<size_t> (row + kSuperFoxQpcRows * col)] = 0.0f;
                        }
                    }
                }

              qpc_decode (xdec.data (), ydec.data (), pyd.data ());
              std::reverse (xdec.begin (), xdec.end ());

              uint32_t const crc_chk = nhash2 (xdec.data (), 47, 571u) & kMask21;
              uint32_t const crc_sent =
                  (static_cast<uint32_t> (xdec[47]) << 14)
                  | (static_cast<uint32_t> (xdec[48]) << 7)
                  | static_cast<uint32_t> (xdec[49]);
              result.crc_ok = crc_chk == crc_sent;
              if (result.crc_ok)
                {
                  result.snr = superfox_qpc_snr (s3.data (), kSuperFoxQpcRows,
                                                 kSuperFoxQpcCols, ydec.data ());
                  result.xdec = xdec;
                  result.fbest = f;
                  result.tbest = t;
                  if (result.snr < -16.5f)
                    {
                      result.crc_ok = false;
                    }
                  return true;
                }
            }
        }
    }

  return true;
}

bool superfox_qpc_sync (std::complex<float> const* crcvd0, float fsample, float fsync, float ftol,
                        float& f2, float& t2, float& snrsync)
{
  if (!crcvd0)
    {
      return false;
    }

  SuperFoxSyncFft& fft = superfox_sync_fft ();
  if (!fft.valid ())
    {
      return false;
    }

  constexpr int kSpectrumBins = kSuperFoxSyncSamples / 4;
  float const baud = 12000.0f / 1024.0f;
  float const df2 = fsample / static_cast<float> (kSuperFoxSyncSamples);
  float const fac = 1.0f / static_cast<float> (kSuperFoxSyncSamples);
  std::complex<float>* c0 = fft.values ();
  for (int i = 0; i < kSuperFoxSyncSamples; ++i)
    {
      c0[static_cast<size_t> (i)] = crcvd0[static_cast<size_t> (i)] * fac;
    }
  fftwf_execute (fft.forward);

  std::vector<float> s (static_cast<size_t> (kSpectrumBins));
  for (int i = 1; i <= kSpectrumBins; ++i)
    {
      s[static_cast<size_t> (i - 1)] = std::norm (c0[static_cast<size_t> (i)]);
    }

  for (int i = 0; i < 4; ++i)
    {
      smooth_121 (s.data (), kSpectrumBins);
    }

  int ia = static_cast<int> (std::lround ((fsync - ftol) / df2));
  int ib = static_cast<int> (std::lround ((fsync + ftol) / df2));
  ia = std::max (1, ia);
  ib = std::min (kSpectrumBins, ib);
  if (ia > ib)
    {
      return false;
    }

  int i0 = ia;
  float peak = s[static_cast<size_t> (ia - 1)];
  for (int i = ia + 1; i <= ib; ++i)
    {
      float const value = s[static_cast<size_t> (i - 1)];
      if (value > peak)
        {
          peak = value;
          i0 = i;
        }
    }
  f2 = df2 * static_cast<float> (i0) - 750.0f;

  ia = static_cast<int> (std::lround (static_cast<float> (i0) - baud / df2));
  ib = static_cast<int> (std::lround (static_cast<float> (i0) + baud / df2));
  ia = std::max (1, ia);
  ib = std::min (kSpectrumBins, ib);

  float s1 = 0.0f;
  float s0 = 0.0f;
  for (int i = ia; i <= ib; ++i)
    {
      float const value = s[static_cast<size_t> (i - 1)];
      s0 += value;
      s1 += static_cast<float> (i - i0) * value;
    }
  if (s0 <= 0.0f)
    {
      return false;
    }

  float const delta = s1 / s0;
  i0 = static_cast<int> (std::lround (static_cast<float> (i0) + delta));
  f2 = static_cast<float> (i0) * df2 - 750.0f;

  std::vector<std::complex<float>> c1 (static_cast<size_t> (kSuperFoxSyncNz), std::complex<float> {});
  ia = static_cast<int> (std::lround (static_cast<float> (i0) - baud / df2));
  ib = static_cast<int> (std::lround (static_cast<float> (i0) + baud / df2));
  ia = std::max (0, ia);
  ib = std::min (kSuperFoxSyncSamples - 1, ib);
  for (int i = ia; i <= ib; ++i)
    {
      int const j = i - i0;
      int const dst = j >= 0 ? j : j + kSuperFoxSyncNz;
      if (dst >= 0 && dst < kSuperFoxSyncNz)
        {
          c1[static_cast<size_t> (dst)] = c0[static_cast<size_t> (i)];
        }
    }

  fftwf_plan inverse = fftwf_plan_dft_1d (kSuperFoxSyncNz,
                                          reinterpret_cast<fftwf_complex*> (c1.data ()),
                                          reinterpret_cast<fftwf_complex*> (c1.data ()),
                                          FFTW_BACKWARD, FFTW_ESTIMATE);
  if (!inverse)
    {
      return false;
    }
  fftwf_execute (inverse);
  fftwf_destroy_plan (inverse);

  std::vector<std::complex<float>> c1sum (static_cast<size_t> (kSuperFoxSyncNz));
  c1sum[0] = c1[0];
  for (int i = 1; i < kSuperFoxSyncNz; ++i)
    {
      c1sum[static_cast<size_t> (i)] = c1sum[static_cast<size_t> (i - 1)] + c1[static_cast<size_t> (i)];
    }

  int const nspsd = 1024 / kSuperFoxSyncDown;
  float const dt = static_cast<float> (kSuperFoxSyncDown) / 12000.0f;
  int const lagmax = static_cast<int> (1.5f / dt);
  int const nominal_start = static_cast<int> (std::lround (0.5f * fsample / kSuperFoxSyncDown));
  std::vector<float> p (static_cast<size_t> (2 * lagmax + 1));
  float pmax = 0.0f;
  int lagpk = 0;
  for (int lag = -lagmax; lag <= lagmax; ++lag)
    {
      float sp = 0.0f;
      for (int j = 0; j < kSuperFoxSyncSymbols; ++j)
        {
          int const i1 = nominal_start + (kSuperFoxIsync[static_cast<size_t> (j)] - 1) * nspsd + lag;
          int const i2 = i1 + nspsd;
          if (i1 < 0 || i1 > kSuperFoxSyncNz - 1 || i2 < 0 || i2 > kSuperFoxSyncNz - 1)
            {
              continue;
            }
          std::complex<float> const z = c1sum[static_cast<size_t> (i2)] - c1sum[static_cast<size_t> (i1)];
          sp += std::norm (z);
        }
      if (sp > pmax)
        {
          pmax = sp;
          lagpk = lag;
        }
      p[static_cast<size_t> (lag + lagmax)] = sp;
    }

  t2 = static_cast<float> (lagpk) * dt;
  float sp = 0.0f;
  float sq = 0.0f;
  int nsum = 0;
  float const tsym = 1024.0f / 12000.0f;
  for (int lag = -lagmax; lag <= lagmax; ++lag)
    {
      float const t = static_cast<float> (lag - lagpk) * dt;
      if (std::fabs (t) < tsym)
        {
          continue;
        }
      float const value = p[static_cast<size_t> (lag + lagmax)];
      ++nsum;
      sp += value;
      sq += value * value;
    }
  if (nsum <= 0)
    {
      return false;
    }

  float const ave = sp / static_cast<float> (nsum);
  float const variance = std::max (0.0f, sq / static_cast<float> (nsum) - ave * ave);
  float const rms = std::sqrt (variance);
  snrsync = rms > 0.0f ? (pmax - ave) / rms : 0.0f;
  return true;
}

void superfox_qpc_likelihoods (float const* s3, int rows, int cols, float EsNo, float No,
                               float* py_out)
{
  float const norm = (EsNo / (EsNo + 1.0f)) / No;
  for (int col = 0; col < cols; ++col)
    {
      float normpwrmax = 0.0f;
      for (int row = 0; row < rows; ++row)
        {
          int const index = row + rows * col;
          float const normpwr = norm * s3[index];
          py_out[index] = normpwr;
          normpwrmax = std::max (normpwrmax, normpwr);
        }
      float pynorm = 0.0f;
      for (int row = 0; row < rows; ++row)
        {
          int const index = row + rows * col;
          py_out[index] = std::exp (py_out[index] - normpwrmax);
          pynorm += py_out[index];
        }
      if (pynorm > 0.0f)
        {
          for (int row = 0; row < rows; ++row)
            {
              int const index = row + rows * col;
              py_out[index] /= pynorm;
            }
        }
    }
}

float superfox_qpc_snr (float const* s3, int rows, int cols, unsigned char const* y)
{
  float p = 0.0f;
  for (int col = 1; col < cols; ++col)
    {
      int const row = y[col];
      if (row >= 0 && row < rows)
        {
          p += s3[row + rows * col];
        }
    }
  return db_compat (p / 127.0f) - db_compat (127.0f) - 4.0f;
}

std::string format_superfox_stdout_line (int nutc, int snr, float dt, float freq,
                                         std::string const& decoded)
{
  std::ostringstream line;
  line << std::setfill ('0') << std::setw (6) << nutc
       << std::setfill (' ') << std::setw (4) << snr
       << std::fixed << std::setprecision (1) << std::setw (5) << dt
       << std::setw (5) << static_cast<int> (std::lround (freq))
       << " ~  " << decoded;
  return line.str ();
}

std::uint64_t superfox_read_bits (std::array<unsigned char, kSuperFoxPackedSymbols> const& xdec,
                                  int bit_offset, int bit_count)
{
  std::uint64_t value = 0;
  for (int i = 0; i < bit_count; ++i)
    {
      int const bit_index = bit_offset + i;
      int const symbol = bit_index / 7;
      int const shift = 6 - (bit_index % 7);
      value = (value << 1)
              | ((static_cast<std::uint64_t> (xdec[static_cast<size_t> (symbol)]) >> shift) & 1u);
    }
  return value;
}

std::string superfox_unpack_text71 (std::array<unsigned char, kSuperFoxPackedSymbols> const& xdec,
                                    int bit_offset)
{
  std::array<char, 71> bits {};
  for (int i = 0; i < 71; ++i)
    {
      bits[static_cast<size_t> (i)] =
          superfox_read_bits (xdec, bit_offset + i, 1) != 0 ? '1' : '0';
    }

  std::array<char, 13> unpacked {};
  bool success = false;
  legacy_pack77_unpacktext77_c (bits.data (), unpacked.data (), &success);
  return success ? trim_block (unpacked.data (), static_cast<int> (unpacked.size ())) : std::string {};
}

std::string superfox_unpack_call28 (int n28)
{
  std::array<char, 13> unpacked {};
  bool success = false;
  legacy_pack77_unpack28_c (n28, unpacked.data (), &success);
  return success ? trim_block (unpacked.data (), static_cast<int> (unpacked.size ())) : std::string {};
}

std::string superfox_unpack_grid15 (int n15)
{
  constexpr int kGridBase = 180 * 180;
  if (n15 < 0)
    {
      return {};
    }
  if (n15 < 32400)
    {
      int const dlat = (n15 % 180) - 90;
      int const dlong = (n15 / 180) * 2 - 180 + 2;
      int const nlong = static_cast<int> (60.0 * (180.0 - static_cast<double> (dlong)) / 5.0);
      int const long_a = nlong / 240;
      int const long_b = (nlong - 240 * long_a) / 24;
      int const nlat = static_cast<int> (60.0 * (static_cast<double> (dlat) + 90.0) / 2.5);
      int const lat_a = nlat / 240;
      int const lat_b = (nlat - 240 * lat_a) / 24;
      std::string grid (4, ' ');
      grid[0] = static_cast<char> ('A' + long_a);
      grid[1] = static_cast<char> ('A' + lat_a);
      grid[2] = static_cast<char> ('0' + long_b);
      grid[3] = static_cast<char> ('0' + lat_b);
      if (grid.rfind ("KA", 0) == 0 || grid.rfind ("LA", 0) == 0)
        {
          bool const prefixed = grid[0] == 'L';
          int value = std::stoi (grid.substr (2, 2)) - 50;
          std::ostringstream report;
          if (prefixed)
            {
              report << 'R';
            }
          if (value >= 0)
            {
              report << '+';
            }
          report << std::setw (2) << std::setfill ('0') << std::abs (value);
          return report.str ();
        }
      return grid;
    }

  int value = n15 - kGridBase - 1;
  std::ostringstream report;
  if (value >= 1 && value <= 30)
    {
      report << '-' << std::setw (2) << std::setfill ('0') << value;
      return report.str ();
    }
  if (value >= 31 && value <= 60)
    {
      report << "R-" << std::setw (2) << std::setfill ('0') << (value - 30);
      return report.str ();
    }
  if (value == 61)
    {
      return "RO";
    }
  if (value == 62)
    {
      return "RRR";
    }
  if (value == 63)
    {
      return "73";
    }
  return {};
}

std::string superfox_decode_compound_call (std::uint64_t n58)
{
  std::array<char, 13> foxcall {};
  foxcall.fill (' ');
  for (int i = 10; i >= 0; --i)
    {
      std::uint64_t const index = (n58 % 38u);
      foxcall[static_cast<size_t> (i)] = kSuperFoxBase38[index];
      n58 /= 38u;
    }
  return trim_block (foxcall.data (), static_cast<int> (foxcall.size ()));
}

std::string superfox_trim_period_padding (std::string text)
{
  for (int i = static_cast<int> (text.size ()) - 1; i >= 0; --i)
    {
      if (text[static_cast<size_t> (i)] != '.')
        {
          break;
        }
      text[static_cast<size_t> (i)] = ' ';
    }
  while (!text.empty () && text.back () == ' ')
    {
      text.pop_back ();
    }
  return text;
}

std::string superfox_format_report (int raw)
{
  if (raw == 31)
    {
      return "RR73";
    }

  int const value = raw - 18;
  std::ostringstream report;
  report << (value >= 0 ? '+' : '-')
         << std::setw (2) << std::setfill ('0') << std::abs (value);
  return report.str ();
}

std::vector<std::string> superfox_unpack_lines (
    std::array<unsigned char, kSuperFoxPackedSymbols> const& xdec, bool use_otp)
{
  std::vector<std::string> lines;
  lines.reserve (kSuperFoxMaxDecodeLines);

  int const i3 = static_cast<int> (superfox_read_bits (xdec, 326, 3));
  std::string foxcall = superfox_unpack_call28 (static_cast<int> (superfox_read_bits (xdec, 0, 28)));
  int ncq = 0;

  if (i3 == 2)
    {
      std::string free_text = superfox_unpack_text71 (xdec, 160);
      free_text += superfox_unpack_text71 (xdec, 231);
      lines.push_back (superfox_trim_period_padding (free_text));
    }
  else if (i3 == 3)
    {
      foxcall = superfox_decode_compound_call (superfox_read_bits (xdec, 0, 58));
      std::string const grid4 = superfox_unpack_grid15 (static_cast<int> (superfox_read_bits (xdec, 58, 15)));
      lines.push_back ("CQ " + foxcall + (grid4.empty () ? std::string {} : " " + grid4));

      std::uint64_t const n32 = superfox_read_bits (xdec, 73, 32);
      if (static_cast<int> (n32) != kSuperFoxNqu1Rks)
        {
          std::string free_text = superfox_unpack_text71 (xdec, 73);
          free_text += superfox_unpack_text71 (xdec, 144);
          free_text = superfox_trim_period_padding (free_text);
          if (!free_text.empty ())
            {
              lines.push_back (free_text);
            }
        }
    }

  if (i3 != 3)
    {
      int report_offset = (i3 == 2) ? 140 : 280;
      int report_count = 4;
      std::array<std::string, 5> reports {};
      for (int i = 0; i < report_count; ++i)
        {
          reports[static_cast<size_t> (i)] =
              superfox_format_report (static_cast<int> (superfox_read_bits (xdec, report_offset + 5 * i, 5)));
        }

      int const max_calls = (i3 == 2 || i3 == 3) ? 4 : 9;
      for (int i = 0; i < max_calls; ++i)
        {
          int const n28 = static_cast<int> (superfox_read_bits (xdec, 28 * i, 28));
          if (n28 == 0 || n28 == kSuperFoxNqu1Rks)
            {
              continue;
            }

          std::string const hound = superfox_unpack_call28 (n28);
          if (hound.empty ())
            {
              continue;
            }

          std::string message = hound + " " + foxcall;
          bool const is_cq = message.rfind ("CQ ", 0) == 0;
          if (is_cq)
            {
              ++ncq;
            }
          else if (i3 == 2)
            {
              message += " " + reports[static_cast<size_t> (i)];
            }
          else if (i < 5)
            {
              message += " RR73";
            }
          else
            {
              message += " " + reports[static_cast<size_t> (i - 5)];
            }

          if (ncq <= 1 || !is_cq)
            {
              lines.push_back (message);
            }
        }

      if (superfox_read_bits (xdec, 305, 1) != 0 && ncq < 1)
        {
          lines.push_back ("CQ " + foxcall);
        }
    }

  if (use_otp)
    {
      int const signature = static_cast<int> (superfox_read_bits (xdec, 306, 20));
      std::ostringstream verify;
      verify << "$VERIFY$ " << foxcall << ' '
             << std::setfill ('0') << std::setw (6) << signature;
      lines.push_back (verify.str ());
    }

  return lines;
}

bool superfox_decode_lines_from_wave (short const* iwave, int nfqso, int ntol,
                                      std::vector<std::string>& lines_out,
                                      int& nsnr_out, float& freq_out, float& dt_out)
{
  lines_out.clear ();
  nsnr_out = 0;
  freq_out = 0.0f;
  dt_out = 0.0f;

  if (!iwave)
    {
      return false;
    }

  std::array<float, kFt8NMax> dd {};
  for (int i = 0; i < kFt8NMax; ++i)
    {
      dd[static_cast<size_t> (i)] = static_cast<float> (iwave[i]);
    }
  ftx_sfox_remove_ft8_c (dd.data (), kFt8NMax);

  std::array<std::complex<float>, kFt8NMax> c0 {};
  if (!superfox_analytic_signal (dd.data (), kFt8NMax, c0.data ()))
    {
      return false;
    }

  float fsync = static_cast<float> (nfqso);
  if (!superfox_remove_tone (c0.data (), fsync))
    {
      return false;
    }

  SuperFoxQpcDecodeResult decode;
  if (!superfox_qpc_decode2 (c0.data (), fsync, static_cast<float> (ntol),
                             3, 0.5f, 1.0f, decode)
      || !decode.crc_ok)
    {
      return false;
    }

  std::array<unsigned char, kSuperFoxPackedSymbols> payload {};
  for (int i = 0; i < kSuperFoxPackedSymbols; ++i)
    {
      payload[static_cast<size_t> (i)] = decode.xdec[static_cast<size_t> (i)];
    }

  lines_out = superfox_unpack_lines (payload, true);
  nsnr_out = static_cast<int> (std::lround (decode.snr));
  freq_out = decode.fbest - 750.0f;
  dt_out = decode.tbest;
  return !lines_out.empty ();
}

void copy_superfox_lines (std::vector<std::string> const& lines, char* lines_out,
                          int line_stride, int max_lines)
{
  if (!lines_out || line_stride <= 0 || max_lines <= 0)
    {
      return;
    }

  int const count = std::min (max_lines, static_cast<int> (lines.size ()));
  for (int i = 0; i < count; ++i)
    {
      char* destination = lines_out + i * line_stride;
      std::fill_n (destination, line_stride, ' ');
      std::string const& line = lines[static_cast<size_t> (i)];
      std::copy_n (line.data (), std::min (line_stride, static_cast<int> (line.size ())), destination);
    }
}

struct Ft8FoxEntry
{
  FixedChars<12> callsign {blank_fixed<12> ()};
  FixedChars<4> grid {blank_fixed<4> ()};
  int snr {0};
  int freq {0};
  int n30 {0};
};

struct Ft8EmitState
{
  std::vector<Ft8FoxEntry> fox_entries;
  int n30z {0};
  int nwrap {0};

  void reset ()
  {
    fox_entries.clear ();
    n30z = 0;
    nwrap = 0;
  }
};

Ft8EmitState& ft8_emit_state ()
{
  static Ft8EmitState state;
  return state;
}

int update_ft8_n30_state (Ft8EmitState& state, int nutc)
{
  int const n30 = (3600 * (nutc / 10000)
                   + 60 * ((nutc / 100) % 100)
                   + nutc % 100) / 30;
  if (n30 < state.n30z)
    {
      state.nwrap += 2880;
    }
  state.n30z = n30;
  return n30 + state.nwrap;
}

bool should_collect_fox_entry (std::string const& decoded, std::string const& mycall,
                               bool superfox, int freq, Ft8FoxEntry& entry)
{
  std::vector<std::string> const words = split_words (decoded);
  if (words.size () < 2)
    {
      return false;
    }

  std::string const& c1 = words[0];
  std::string const& c2 = words[1];
  std::string const g2 = words.size () >= 3 ? words[2] : std::string {};
  bool b0 = c1 == mycall;
  if (c1 == "DE" && c2.find ('/') != std::string::npos && c2.find ('/') >= 1)
    {
      b0 = true;
    }
  if (c1.size () != mycall.size ())
    {
      if (c1.find (mycall) != std::string::npos || mycall.find (c1) != std::string::npos)
        {
          b0 = true;
        }
    }

  bool const b1 = is_grid4 (g2);
  bool const b2 = g2.empty ();
  if (!(b0 && (b1 || b2) && (freq >= 1000 || superfox)))
    {
      return false;
    }

  entry.callsign = fixed_from_string<12> (c2);
  entry.grid = fixed_from_string<4> (b1 ? g2 : std::string {});
  entry.freq = freq;
  return true;
}

int grid_distance_km (FixedChars<6> const& mygrid, FixedChars<4> const& hisgrid4)
{
  std::string const hisgrid_trimmed = trim_fixed (hisgrid4);
  if (!is_grid4 (hisgrid_trimmed))
    {
      return 9999;
    }

  FixedChars<6> hisgrid = blank_fixed<6> ();
  std::copy_n (hisgrid4.begin (), 4, hisgrid.begin ());
  QString const my_grid = QString::fromLatin1 (mygrid.data (), static_cast<int> (mygrid.size ()));
  QString const his_grid = QString::fromLatin1 (hisgrid.data (), static_cast<int> (hisgrid.size ()));
  return geo_distance (my_grid, his_grid, 0.0).km;
}

void write_houndcallers_file (std::string const& path, Ft8EmitState& state,
                              FixedChars<6> const& mygrid, int current_n30)
{
  std::ofstream file {path, std::ios::out | std::ios::trunc};
  if (!file.is_open ())
    {
      return;
    }

  std::vector<Ft8FoxEntry> retained;
  retained.reserve (state.fox_entries.size ());
  for (Ft8FoxEntry const& entry : state.fox_entries)
    {
      int const age = std::min (99, (current_n30 - entry.n30 + 288000) % 2880);
      if (age > 4)
        {
          continue;
        }

      retained.push_back (entry);
      int const dkm = grid_distance_km (mygrid, entry.grid);
      file << std::left << std::setw (12) << trim_fixed (entry.callsign)
           << ' '
           << std::setw (4) << trim_fixed (entry.grid)
           << std::right << std::setw (5) << entry.snr
           << std::setw (6) << entry.freq
           << std::setw (7) << dkm
           << std::setw (3) << age
           << '\n';
    }
  file.flush ();
  state.fox_entries = std::move (retained);
}

void copy_audio_to_float (short const* source, std::array<float, kFt8NMax>& dest)
{
  for (int i = 0; i < kFt8NMax; ++i)
    {
      dest[static_cast<size_t> (i)] = static_cast<float> (source[i]);
    }
}

void restore_carried_audio (std::array<float, kFt8NMax>& dd,
                            std::array<float, kFt8NMax> const& dd1,
                            short const* iwave)
{
  std::copy_n (dd1.begin (), kFt8CarrySamples, dd.begin ());
  for (int i = kFt8CarrySamples; i < kFt8NMax; ++i)
    {
      dd[static_cast<size_t> (i)] = static_cast<float> (iwave[i]);
    }
}

double current_sequence_seconds ()
{
  using namespace std::chrono;

  system_clock::time_point const now = system_clock::now ();
  auto const millis = duration_cast<milliseconds> (now.time_since_epoch ());
  std::time_t const tt = system_clock::to_time_t (now);
  std::tm local_tm {};
#if defined(_WIN32)
  localtime_s (&local_tm, &tt);
#else
  localtime_r (&tt, &local_tm);
#endif
  double const sec = static_cast<double> (local_tm.tm_sec) + (millis.count () % 1000) / 1000.0;
  double tseq = std::fmod (sec, 15.0);
  if (tseq < 10.0)
    {
      tseq += 15.0;
    }
  return tseq;
}

struct AsyncCollector
{
  float* syncs {};
  int* snrs {};
  float* dts {};
  float* freqs {};
  int* naps {};
  float* quals {};
  signed char* bits77 {};
  char* decodeds {};
  int* nout {};
  int count {0};

  void reset ()
  {
    if (syncs) std::fill_n (syncs, kFt8MaxLines, 0.0f);
    if (snrs) std::fill_n (snrs, kFt8MaxLines, 0);
    if (dts) std::fill_n (dts, kFt8MaxLines, 0.0f);
    if (freqs) std::fill_n (freqs, kFt8MaxLines, 0.0f);
    if (naps) std::fill_n (naps, kFt8MaxLines, 0);
    if (quals) std::fill_n (quals, kFt8MaxLines, 0.0f);
    if (bits77) std::fill_n (bits77, kFt8MaxLines * kFt8Bits, static_cast<signed char> (0));
    if (decodeds) std::fill_n (decodeds, kFt8MaxLines * kFt8DecodedChars, ' ');
    count = 0;
    if (nout) *nout = 0;
  }

  void append (float sync, int snr, float dt, float freq,
               FixedChars<kFt8DecodedChars> const& decoded, int nap, float qual,
               signed char const* message77)
  {
    (void) sync;
    if (count >= kFt8MaxLines)
      {
        return;
      }

    int const index = count++;
    if (syncs)
      {
        syncs[index] = sync;
      }
    snrs[index] = snr;
    dts[index] = dt;
    freqs[index] = freq;
    naps[index] = nap;
    quals[index] = qual;
    std::copy (decoded.begin (), decoded.end (),
               decodeds + index * kFt8DecodedChars);
    if (message77)
      {
        std::copy_n (message77, kFt8Bits, bits77 + index * kFt8Bits);
      }
    if (nout)
      {
        *nout = count;
      }
  }
};

struct Ft8A7Entry
{
  float dt {0.0f};
  float freq {0.0f};
  int age {0};
  FixedChars<kFt8DecodedChars> message {blank_fixed<kFt8DecodedChars> ()};
};

struct Ft8A7Slot
{
  int previous_count {0};
  int current_count {0};
  std::array<Ft8A7Entry, kFt8MaxEarly> previous {};
  std::array<Ft8A7Entry, kFt8MaxEarly> current {};
};

struct Ft8A7HistoryState
{
  std::array<Ft8A7Slot, kFt8SequenceCount> a7slots {};
  int nutc0 {-1};

  void reset ()
  {
    a7slots = {};
    nutc0 = -1;
  }
};

struct Ft8Stage4State
{
  std::array<float, kFt8NMax> dd {};
  std::array<float, kFt8NMax> dd1 {};
  std::array<FixedChars<kFt8DecodedChars>, kFt8MaxEarly> allmessages {};
  std::array<int, kFt8MaxEarly> allsnrs {};
  std::array<int, kFt8Nn * kFt8MaxEarly> itone_save {};
  std::array<float, kFt8MaxEarly> f1_save {};
  std::array<float, kFt8MaxEarly> xdt_save {};
  std::array<bool, kFt8MaxEarly> lsubtracted {};
  std::array<float, kFt8Nh1> early_sbase {};
  std::array<Ft8A7Slot, kFt8SequenceCount> a7 {};
  int nutc0 {-1};
  int early_nutc {-1};
  int ndec_early {0};
  bool early_sbase_valid {false};

  void resetEarlySlotState ()
  {
    allmessages.fill (blank_fixed<kFt8DecodedChars> ());
    allsnrs.fill (0);
    itone_save.fill (0);
    f1_save.fill (0.0f);
    xdt_save.fill (0.0f);
    lsubtracted.fill (false);
    early_sbase.fill (0.0f);
    early_sbase_valid = false;
    ndec_early = 0;
  }

  void reset ()
  {
    dd.fill (0.0f);
    dd1.fill (0.0f);
    resetEarlySlotState ();
    a7 = {};
    nutc0 = -1;
    early_nutc = -1;
  }
};

struct Ft8Request
{
  short const* iwave {};
  int nqsoprogress {};
  int nfqso {};
  int nftx {};
  int nutc {};
  int nfa {};
  int nfb {};
  int nzhsym {};
  int ndepth {};
  float emedelay {};
  int ncontest {};
  int nagain {};
  int lft8apon {};
  int ltry_a8 {};
  int lapcqonly {};
  int napwid {};
  int ldiskdat {};
  FixedChars<12> mycall {blank_fixed<12> ()};
  FixedChars<12> hiscall {blank_fixed<12> ()};
  FixedChars<6> hisgrid {blank_fixed<6> ()};
};

Ft8Stage4State& stage4_state ()
{
  static Ft8Stage4State state;
  return state;
}

Ft8A7HistoryState& global_a7_history ()
{
  static Ft8A7HistoryState state;
  return state;
}

int sequence_index_for_utc (int nutc)
{
  return std::abs ((nutc / 5) % 2);
}

bool a7_entries_match (Ft8A7Entry const& lhs, Ft8A7Entry const& rhs)
{
  return std::fabs (lhs.freq - rhs.freq) <= 3.0f
         && std::equal (lhs.message.begin (), lhs.message.end (), rhs.message.begin ());
}

void append_a7_retained_entry (std::array<Ft8A7Entry, kFt8MaxEarly>& retained,
                               int& retained_count, Ft8A7Entry entry)
{
  if (entry.freq <= -98.0f)
    {
      return;
    }

  for (int i = 0; i < retained_count; ++i)
    {
      if (a7_entries_match (retained[static_cast<size_t> (i)], entry))
        {
          retained[static_cast<size_t> (i)] = entry;
          return;
        }
    }

  if (retained_count >= kFt8A7MaxRetained || retained_count >= kFt8MaxEarly)
    {
      return;
    }

  retained[static_cast<size_t> (retained_count)] = entry;
  ++retained_count;
}

void prepare_a7_tables (std::array<Ft8A7Slot, kFt8SequenceCount>& a7slots, int& nutc0,
                        int nutc, int nzhsym, int jseq)
{
  if (nzhsym != 41 && nutc == nutc0)
    {
      return;
    }

  Ft8A7Slot& slot = a7slots[static_cast<size_t> (jseq)];
  std::array<Ft8A7Entry, kFt8MaxEarly> retained {};
  int retained_count = 0;

  for (int i = 0; i < slot.current_count && i < kFt8MaxEarly; ++i)
    {
      Ft8A7Entry entry = slot.current[static_cast<size_t> (i)];
      entry.age = 0;
      append_a7_retained_entry (retained, retained_count, entry);
    }

  for (int i = 0; i < slot.previous_count && i < kFt8MaxEarly; ++i)
    {
      Ft8A7Entry entry = slot.previous[static_cast<size_t> (i)];
      if (entry.age >= kFt8A7MaxAge)
        {
          continue;
        }
      ++entry.age;
      append_a7_retained_entry (retained, retained_count, entry);
    }

  slot.previous = retained;
  slot.previous_count = retained_count;
  slot.current_count = 0;
  nutc0 = nutc;
}

void save_a7_entry (std::array<Ft8A7Slot, kFt8SequenceCount>& a7slots, int jseq, float dt, float freq,
                    FixedChars<kFt8DecodedChars> const& decoded)
{
  std::string const decoded_trimmed = trim_fixed (decoded);
  if (decoded_trimmed.find ('/') != std::string::npos
      || decoded_trimmed.find ('<') != std::string::npos)
    {
      return;
    }

  int nwords = 0;
  std::array<int, kFt8WordCount> nw {};
  std::array<char, kFt8WordCount * kFt8WordChars> words {};
  legacy_pack77_split77_c (decoded.data (), &nwords, nw.data (), words.data ());
  if (nwords < 1)
    {
      return;
    }

  auto word_at = [&] (int index) {
    return trim_block (words.data () + index * kFt8WordChars, kFt8WordChars);
  };

  std::string const word1 = word_at (0);
  if (word1.rfind ("CQ_", 0) == 0)
    {
      return;
    }

  Ft8A7Slot& slot = a7slots[static_cast<size_t> (jseq)];
  if (slot.current_count >= kFt8MaxEarly)
    {
      return;
    }

  std::string const word2 = nwords >= 2 ? word_at (1) : std::string {};
  std::string saved = word1 + (word2.empty () ? std::string {} : " " + word2);
  if (word1 == "CQ" && nwords >= 3 && nw[1] <= 2)
    {
      saved = "CQ " + word2 + " " + word_at (2);
    }

  std::string const last_word = word_at (std::max (0, nwords - 1));
  if (is_grid4 (last_word))
    {
      saved += " " + last_word;
    }

  Ft8A7Entry entry;
  entry.dt = dt;
  entry.freq = freq;
  entry.age = 0;
  entry.message = fixed_from_string<kFt8DecodedChars> (saved);
  slot.current[static_cast<size_t> (slot.current_count)] = entry;
  ++slot.current_count;

  int saved_words = 0;
  std::array<int, kFt8WordCount> saved_nw {};
  std::array<char, kFt8WordCount * kFt8WordChars> saved_split {};
  legacy_pack77_split77_c (entry.message.data (), &saved_words, saved_nw.data (), saved_split.data ());
  if (saved_words < 2)
    {
      return;
    }
  std::string const peer = trim_block (saved_split.data () + kFt8WordChars, kFt8WordChars);

  for (int i = 0; i < slot.previous_count && i < kFt8MaxEarly; ++i)
    {
      Ft8A7Entry& previous = slot.previous[static_cast<size_t> (i)];
      if (previous.freq <= -98.0f)
        {
          continue;
        }
      std::string const previous_trimmed = trim_fixed (previous.message);
      std::string const needle = " " + peer;
      if (std::fabs (freq - previous.freq) <= 3.0f
          && previous_trimmed.find (needle) != std::string::npos
          && previous_trimmed.find (needle) >= 2)
        {
          previous.freq = -98.0f;
        }
    }
}

bool messages_equal (FixedChars<kFt8DecodedChars> const& lhs,
                     FixedChars<kFt8DecodedChars> const& rhs)
{
  return std::equal (lhs.begin (), lhs.end (), rhs.begin ());
}

FixedChars<13> widen_call_for_pack77 (FixedChars<12> const& value)
{
  FixedChars<13> out = blank_fixed<13> ();
  std::copy_n (value.begin (), 12, out.begin ());
  return out;
}

int ft8_candidate_sync_threshold (int imetric, int ndepth)
{
  int syncmin = 6;
  if (imetric == 2)
    {
      syncmin = 7;
    }
  if (ndepth >= 4)
    {
      syncmin = std::min (syncmin, 5);
    }
  else if (ndepth >= 3)
    {
      syncmin = std::min (syncmin, 6);
    }
  if (ndepth <= 2)
    {
      syncmin = 8;
    }
  if (ndepth == 1)
    {
      syncmin = 7;
    }
  if (ndepth >= 4 && stage4_supplemental_requested ().load (std::memory_order_relaxed))
    {
      syncmin = std::min (syncmin, 4);
    }
  return syncmin;
}

int ft8_candidate_budget (Ft8Request const& request)
{
  bool const supplemental = stage4_supplemental_requested ().load (std::memory_order_relaxed);
  if (request.ndepth >= 4 && supplemental)
    {
      return kFt8MaxCand;
    }
  if (request.ndepth >= 4)
    {
      return 3200;
    }
  if (request.ndepth >= 3)
    {
      return kFt8DeepMaxCand;
    }
  return kFt8DefaultMaxCand;
}

float ft8_cq_signature_score (float const* s8, int rows)
{
  if (!s8 || rows < 8)
    {
      return 0.0f;
    }

  auto strongest_tone = [s8, rows] (int symbol) {
    int best = 0;
    float best_value = s8[rows * symbol];
    for (int tone = 1; tone < 8; ++tone)
      {
        float const value = s8[tone + rows * symbol];
        if (value > best_value)
          {
            best_value = value;
            best = tone;
          }
      }
    return best;
  };

  float score = 0.0f;
  for (int symbol = 7; symbol <= 15; ++symbol)
    {
      int const tone = strongest_tone (symbol);
      if ((symbol < 15 && tone == 0) || (symbol == 15 && tone == 1))
        {
          score += 1.0f;
        }
    }
  int const tone17 = strongest_tone (16);
  int const tone27 = strongest_tone (26);
  int const tone33 = strongest_tone (32);
  if (tone17 == 0 || tone17 == 1) score += 0.5f;
  if (tone27 == 0 || tone27 == 1) score += 0.5f;
  if (tone33 == 2 || tone33 == 3) score += 0.5f;
  return score;
}

void plan_ft8_ldpc_decode (Ft8Request const& request, float f1, int pass_iaptype,
                           int& Keff, int& maxosd, int& norder)
{
  norder = 2;
  maxosd = 2;
  if (request.ndepth == 1)
    {
      maxosd = -1;
    }
  else
    {
      int const overrideMaxOsd = stage4_ldpc_maxosd_override ().load (std::memory_order_relaxed);
      int const overrideNOrder = stage4_ldpc_norder_override ().load (std::memory_order_relaxed);
      if (overrideMaxOsd >= 0)
        {
          maxosd = overrideMaxOsd;
        }
      if (overrideNOrder > 0)
        {
          norder = overrideNOrder;
        }
    }
  if (request.ndepth >= 4
      && pass_iaptype > 0
	      && request.nzhsym < 47
      && !stage4_supplemental_requested ().load (std::memory_order_relaxed))
    {
      // JTDX gets many of its weakest live decodes through AP-assisted OSD.
      // Spend the extra saved BP state only on AP passes; applying maxosd=3
      // to every normal pass runs into the live deadline.
      maxosd = std::max (maxosd, 3);
      norder = std::max (norder, 3);
    }
  if (request.ndepth == 3
      && (std::fabs (static_cast<float> (request.nfqso) - f1) <= static_cast<float> (request.napwid)
          || std::fabs (static_cast<float> (request.nftx) - f1) <= static_cast<float> (request.napwid)
          || request.ncontest == 7))
    {
      maxosd = 2;
    }
  Keff = 91;
}

bool decode_main_candidate_cpp (float* dd0, int* newdat, Ft8Request const& request, int imetric,
                                int lsubtract, std::array<int, 58> const& apsym,
                                std::array<int, 10> const& aph10,
                                float const* candidate_values, float candidate_cq_flag, float const* sbase,
                                int sbase_size, float& sync, float& f1, float& xdt,
                                float& xbase, int& nharderrors, float& dmin, int& nbadcrc,
                                int& ipass, int& iaptype,
                                FixedChars<kFt8DecodedChars>& msg37, float& xsnr,
                                std::array<int, kFt8Nn>& itone,
                                std::array<signed char, kFt8Bits>& message77)
{
  sync = 0.0f;
  f1 = 0.0f;
  xdt = 0.0f;
  xbase = 0.0f;
  nharderrors = -1;
  dmin = 0.0f;
  nbadcrc = 1;
  ipass = 0;
  iaptype = 0;
  msg37 = blank_fixed<kFt8DecodedChars> ();
  xsnr = 0.0f;
  itone.fill (0);
  message77.fill (0);

  if (!dd0 || !newdat || !candidate_values || !sbase)
    {
      return false;
    }
  if (stage4_should_cancel ())
    {
      return false;
    }

  auto const mycall13 = widen_call_for_pack77 (request.mycall);
  auto const hiscall13 = widen_call_for_pack77 (request.hiscall);
  legacy_pack77_reset_context_c ();
  legacy_pack77_set_context_c (mycall13.data (), hiscall13.data ());

  ftx_ft8_prepare_candidate_c (candidate_values[2], candidate_values[0], candidate_values[1],
                               sbase, sbase_size, &sync, &f1, &xdt, &xbase);

  std::array<std::complex<float>, kFt8A7DownsampleSize> cd0 {};
  std::array<float, 8 * kFt8Nn> s8 {};
  std::array<float, 174> llra {};
  std::array<float, 174> llrb {};
  std::array<float, 174> llrc {};
  std::array<float, 174> llrd {};
  std::array<float, 174> llre {};

  int local_newdat = *newdat;
  ftx_ft8_downsample_c (dd0, &local_newdat, f1,
                        reinterpret_cast<fftwf_complex*> (cd0.data ()));
  if (stage4_should_cancel ())
    {
      return false;
    }

  int ibest = 0;
  float delfbest = 0.0f;
  ftx_ft8_a7_search_initial_c (cd0.data (), kFt8A7Np2, kFt8A7Fs2, xdt, &ibest, &delfbest);
  f1 += delfbest;

  int second_pass_newdat = 0;
  ftx_ft8_downsample_c (dd0, &second_pass_newdat, f1,
                        reinterpret_cast<fftwf_complex*> (cd0.data ()));
  if (stage4_should_cancel ())
    {
      return false;
    }

  ftx_ft8_a7_refine_search_c (cd0.data (), kFt8A7Np2, kFt8A7Fs2, ibest,
                              &ibest, &sync, &xdt);
  xdt += 0.5f;

  int nsync = 0;
  ftx_ft8_bitmetrics_scaled_c (cd0.data (), kFt8A7Np2, ibest, imetric, 3.2f,
                               s8.data (), &nsync, llra.data (), llrb.data (),
                               llrc.data (), llrd.data (), llre.data ());
  *newdat = local_newdat;

  float const cq_signature_score = ft8_cq_signature_score (s8.data (), 8);
  int const sync_threshold = ft8_candidate_sync_threshold (imetric, request.ndepth);
  (void) candidate_cq_flag;
  bool const cq_sync_override =
      request.nzhsym >= 50
      && request.ndepth >= 4
      && request.lft8apon != 0
      && !stage4_supplemental_requested ().load (std::memory_order_relaxed)
      && ((nsync == 4 && cq_signature_score >= 6.6f)
          || (nsync == 5 && cq_signature_score >= 6.1f)
          || (nsync == 6 && cq_signature_score >= 5.6f));

  if (nsync <= sync_threshold && !cq_sync_override)
    {
      return false;
    }
  bool const cq_only_decode = cq_sync_override && nsync <= sync_threshold;

  int npasses = ftx_ft8_select_npasses_c (request.lft8apon, request.lapcqonly,
                                          request.ncontest, request.nzhsym,
                                          request.nqsoprogress);
  bool const live_full_ap =
      request.nzhsym >= 47
      && request.ndepth >= 4
      && request.lft8apon != 0
      && !stage4_supplemental_requested ().load (std::memory_order_relaxed);
  if (live_full_ap)
    {
      npasses = std::min (npasses, 8);
    }
  int pass_first = 1;
  int pass_last = std::max (0, npasses);
  ftx_ft8_plan_pass_window_c (0, npasses, &pass_first, &pass_last);

  std::array<float, 174> llrz {};
  std::array<int, 174> apmask {};
  std::array<signed char, 174> apmask_bits {};
  std::array<signed char, 91> message91 {};
  std::array<signed char, 174> cw {};

  for (int pass_index = pass_first; pass_index <= pass_last; ++pass_index)
    {
      if (stage4_should_cancel ())
        {
          return false;
        }
      int pass_iaptype = 0;
      int pass_ready = 0;
      if (live_full_ap && pass_index >= 6 && pass_index <= 8)
        {
          pass_ready = ftx_ft8_prepare_cq_ap_pass_c (pass_index, request.nqsoprogress,
                                                     request.lapcqonly, request.ncontest,
                                                     request.nfqso, request.nftx, f1,
                                                     request.napwid, apsym.data (), aph10.data (),
                                                     llra.data (), llrb.data (), llrc.data (),
                                                     llrz.data (), apmask.data (), &pass_iaptype);
        }
      else
        {
          pass_ready = ftx_ft8_prepare_decode_pass_c (pass_index, request.nqsoprogress,
                                                      request.lapcqonly, request.ncontest,
                                                      request.nfqso, request.nftx, f1,
                                                      request.napwid, apsym.data (), aph10.data (),
                                                      llra.data (), llrb.data (), llrc.data (),
                                                      llrd.data (), llre.data (), llrz.data (),
                                                      apmask.data (), &pass_iaptype);
        }
      if (pass_ready == 0)
        {
          continue;
        }
      if (cq_only_decode && pass_iaptype != 1)
        {
          continue;
        }
      if (request.nzhsym >= 50
          && pass_iaptype == 1
          && !stage4_supplemental_requested ().load (std::memory_order_relaxed)
          && cq_signature_score < 3.1f)
        {
          continue;
        }

      std::transform (apmask.begin (), apmask.end (), apmask_bits.begin (),
                      [] (int value) {
                        return static_cast<signed char> (value != 0 ? 1 : 0);
                      });

      int Keff = 91;
      int maxosd = 2;
      int norder = 2;
      plan_ft8_ldpc_decode (request, f1, pass_iaptype, Keff, maxosd, norder);

      int ntype = 0;
      int pass_nharderrors = -1;
      float pass_dmin = 0.0f;
      ftx_decode174_91_c (llrz.data (), Keff, maxosd, norder, apmask_bits.data (),
                          message91.data (), cw.data (), &ntype,
                          &pass_nharderrors, &pass_dmin);
      if (stage4_should_cancel ())
        {
          return false;
        }
      if (pass_nharderrors < 0 || pass_nharderrors > kFt8MaxHardErrors)
        {
          continue;
        }

      std::copy_n (message91.begin (), kFt8Bits, message77.begin ());

      FixedChars<kFt8DecodedChars> candidate_msg = blank_fixed<kFt8DecodedChars> ();
      int quirky = 0;
      int const unpack_ok = legacy_pack77_unpack77bits_c (message77.data (), 1,
                                                          candidate_msg.data (), &quirky);
      if (ftx_ft8_validate_candidate_meta_c (message77.data (), cw.data (),
                                             pass_nharderrors, unpack_ok, quirky,
                                             request.ncontest) == 0)
        {
          continue;
        }
      if (pass_nharderrors > kFt8StrictHardErrors
          && !is_strict_standard_ft8_message (candidate_msg))
        {
          continue;
        }

      std::array<int, kFt8Nn> candidate_itone {};
      if (ftx_ft8_message77_to_itone_c (message77.data (), candidate_itone.data ()) == 0)
        {
          continue;
        }

      if (lsubtract != 0)
        {
          ftx_subtract_ft8_c (dd0, candidate_itone.data (), f1, xdt, 0);
        }

      float pass_xsnr = 0.0f;
      if (ftx_ft8_compute_snr_c (s8.data (), 8, kFt8Nn, candidate_itone.data (),
                                 xbase, request.nagain, nsync, &pass_xsnr) == 0)
        {
          continue;
        }

      int selected_pass = 0;
      int selected_iaptype = 0;
      if (ftx_ft8_finalize_decode_pass_c (0, pass_index, pass_iaptype,
                                          &selected_pass, &selected_iaptype) == 0)
        {
          continue;
        }

      nharderrors = pass_nharderrors;
      dmin = pass_dmin;
      nbadcrc = 0;
      ipass = selected_pass;
      iaptype = selected_iaptype;
      msg37 = candidate_msg;
      xsnr = pass_xsnr;
      itone = candidate_itone;
      return true;
    }

  return false;
}

extern "C" void ftx_ft8_a7_reset_c ()
{
  global_a7_history ().reset ();
}

extern "C" void ftx_ft8_a7_prepare_tables_c (int nutc, int nzhsym, int jseq)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount)
    {
      return;
    }

  Ft8A7HistoryState& history = global_a7_history ();
  prepare_a7_tables (history.a7slots, history.nutc0, nutc, nzhsym, jseq);
}

extern "C" int ftx_ft8_a7_previous_count_c (int jseq)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount)
    {
      return 0;
    }

  Ft8A7Slot const& slot = global_a7_history ().a7slots[static_cast<size_t> (jseq)];
  return std::max (0, std::min (slot.previous_count, kFt8MaxEarly));
}

extern "C" int ftx_ft8_a7_get_previous_entry_c (int jseq, int index, float* dt_out,
                                                 float* freq_out, char* msg37_out)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount || index <= 0 || !dt_out || !freq_out || !msg37_out)
    {
      return 0;
    }

  Ft8A7Slot const& slot = global_a7_history ().a7slots[static_cast<size_t> (jseq)];
  if (index > slot.previous_count || index > kFt8MaxEarly)
    {
      return 0;
    }

  Ft8A7Entry const& entry = slot.previous[static_cast<size_t> (index - 1)];
  *dt_out = entry.dt;
  *freq_out = entry.freq;
  std::copy (entry.message.begin (), entry.message.end (), msg37_out);
  return 1;
}

extern "C" void ftx_ft8_a7_save_c (int jseq, float dt, float freq, char const* msg37)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount || !msg37)
    {
      return;
    }

  save_a7_entry (global_a7_history ().a7slots, jseq, dt, freq,
                 fixed_from_chars<kFt8DecodedChars> (msg37));
  save_a7_entry (stage4_state ().a7, jseq, dt, freq,
                 fixed_from_chars<kFt8DecodedChars> (msg37));
}

extern "C" void ftx_ft8_decode_candidate_stage4_c (
    float* dd0, int* newdat, int* nQSOProgress, int* nfqso, int* nftx, int* ndepth,
    int* nzhsym, int* lapon, int* lapcqonly, int* napwid, int* lsubtract,
    int* nagain, int* ncontest, int* imetric, char const* mycall12,
    char const* hiscall12, float const* candidate_values, float const* sbase,
    int* sbase_size, int const* apsym, int const* aph10, float* sync, float* f1,
    float* xdt, float* xbase, int* nharderrors, float* dmin, int* nbadcrc,
    int* ipass, int* iaptype, char* msg37, float* xsnr, int* itone,
    signed char* message77_out)
{
  if (!dd0 || !newdat || !nQSOProgress || !nfqso || !nftx || !ndepth || !nzhsym
      || !lapon || !lapcqonly || !napwid || !lsubtract || !nagain || !ncontest
      || !imetric || !mycall12 || !hiscall12 || !candidate_values || !sbase
      || !sbase_size || !apsym || !aph10 || !sync || !f1 || !xdt || !xbase
      || !nharderrors || !dmin || !nbadcrc || !ipass || !iaptype || !msg37
      || !xsnr || !itone || !message77_out)
    {
      return;
    }

  Ft8Request request;
  request.nqsoprogress = *nQSOProgress;
  request.nfqso = *nfqso;
  request.nftx = *nftx;
  request.nutc = 0;
  request.nfa = 0;
  request.nfb = 0;
  request.nzhsym = *nzhsym;
  request.ndepth = *ndepth;
  request.emedelay = 0.0f;
  request.ncontest = *ncontest;
  request.nagain = *nagain;
  request.lft8apon = *lapon;
  request.lapcqonly = *lapcqonly;
  request.napwid = *napwid;
  request.ldiskdat = 0;
  request.mycall = fixed_from_chars<12> (mycall12);
  request.hiscall = fixed_from_chars<12> (hiscall12);
  request.hisgrid = blank_fixed<6> ();

  std::array<int, 58> apsym_array {};
  std::array<int, 10> aph10_array {};
  std::copy_n (apsym, apsym_array.size (), apsym_array.data ());
  std::copy_n (aph10, aph10_array.size (), aph10_array.data ());

  FixedChars<kFt8DecodedChars> msg37_array = blank_fixed<kFt8DecodedChars> ();
  std::array<int, kFt8Nn> itone_array {};
  std::array<signed char, kFt8Bits> message77_array {};
  float sync_out = 0.0f;
  float f1_out = 0.0f;
  float xdt_out = 0.0f;
  float xbase_out = 0.0f;
  int nharderrors_out = -1;
  float dmin_out = 0.0f;
  int nbadcrc_out = 1;
  int ipass_out = 0;
  int iaptype_out = 0;
  float xsnr_out = 0.0f;

  decode_main_candidate_cpp (dd0, newdat, request, *imetric, *lsubtract,
                             apsym_array, aph10_array, candidate_values, 0.0f, sbase,
                             *sbase_size, sync_out, f1_out, xdt_out, xbase_out,
                             nharderrors_out, dmin_out, nbadcrc_out, ipass_out,
                             iaptype_out, msg37_array, xsnr_out, itone_array,
                             message77_array);

  *sync = sync_out;
  *f1 = f1_out;
  *xdt = xdt_out;
  *xbase = xbase_out;
  *nharderrors = nharderrors_out;
  *dmin = dmin_out;
  *nbadcrc = nbadcrc_out;
  *ipass = ipass_out;
  *iaptype = iaptype_out;
  *xsnr = xsnr_out;
  std::copy (msg37_array.begin (), msg37_array.end (), msg37);
  std::copy (itone_array.begin (), itone_array.end (), itone);
  std::copy (message77_array.begin (), message77_array.end (), message77_out);
}

void run_main_passes (Ft8Stage4State& state, Ft8Request const& request, int jseq,
                      std::array<int, 58> const& apsym, std::array<int, 10> const& aph10,
                      int ifa, int ifb, int& ndecodes, std::array<float, kFt8Nh1>& sbase,
                      AsyncCollector& collector)
{
  std::array<float, 4 * kFt8MaxCand> candidate {};
  std::array<int, kFt8Nn> itone {};
  std::array<signed char, kFt8Bits> message77 {};
  int npass = 4;
  if (request.ndepth <= 2)
    {
      npass = 3;
    }
  if (request.ndepth == 1)
    {
      npass = 2;
    }

  for (int ipass = 1; ipass <= npass; ++ipass)
    {
      if (stage4_should_cancel ())
        {
          return;
        }
      std::array<float, kFt8NMax> dd_before_shift {};
      bool shifted_pass = false;
      if (request.nzhsym >= 50 && request.ndepth >= 4 && ipass == 4)
        {
          dd_before_shift = state.dd;
          for (int i = 0; i < kFt8NMax - 1; ++i)
            {
              state.dd[static_cast<size_t> (i)] =
                  0.5f * (dd_before_shift[static_cast<size_t> (i)]
                          + dd_before_shift[static_cast<size_t> (i + 1)]);
            }
          shifted_pass = true;
        }
      int pass_newdat = 1;
      float syncmin = 0.0f;
      int imetric = 0;
      int lsubtract = 0;
      int run_pass = 0;
      ftx_ft8_prepare_pass_c (request.ndepth, ipass, ndecodes,
                              &syncmin, &imetric, &lsubtract, &run_pass);
      if (run_pass == 0)
        {
          if (shifted_pass)
            {
              state.dd = dd_before_shift;
            }
          continue;
        }

      int ncand = 0;
      ftx_sync8_search_stage4_c (state.dd.data (), kFt8NMax,
                                 static_cast<float> (ifa), static_cast<float> (ifb),
                                 syncmin, static_cast<float> (request.nfqso),
                                 ft8_candidate_budget (request), ipass,
                                 candidate.data (), &ncand, sbase.data ());

      for (int icand = 0; icand < ncand; ++icand)
        {
          if (stage4_should_cancel ())
            {
              if (shifted_pass)
                {
                  state.dd = dd_before_shift;
                }
              return;
            }
          float sync = 0.0f;
          float f1 = 0.0f;
          float xdt = 0.0f;
          float xbase = 0.0f;
          float dmin = 0.0f;
          float xsnr = 0.0f;
          int nharderrors = -1;
          int nbadcrc = 1;
          int candidate_pass = 0;
          int iaptype = 0;
          FixedChars<kFt8DecodedChars> msg37 = blank_fixed<kFt8DecodedChars> ();

          decode_main_candidate_cpp (state.dd.data (), &pass_newdat, request, imetric,
                                     lsubtract, apsym, aph10,
                                     candidate.data () + icand * 4,
                                     candidate[static_cast<size_t> (icand * 4 + 3)],
                                     sbase.data (),
                                     kFt8Nh1, sync, f1, xdt, xbase, nharderrors,
                                     dmin, nbadcrc, candidate_pass, iaptype,
                                     msg37, xsnr, itone, message77);
          if (stage4_should_cancel ())
            {
              if (shifted_pass)
                {
                  state.dd = dd_before_shift;
                }
              return;
            }

          xdt -= 0.5f;

          int nsnr = 0;
          float callback_dt = 0.0f;
          float qual = 0.0f;
          ftx_ft8_finalize_main_result_c (xsnr, xdt, request.emedelay, nharderrors,
                                          dmin, &nsnr, &callback_dt, &qual);

          if (nbadcrc == 0)
            {
              bool duplicate = false;
              for (int id = 0; id < ndecodes; ++id)
                {
                  if (messages_equal (msg37, state.allmessages[static_cast<size_t> (id)]))
                    {
                      duplicate = true;
                      break;
                    }
                }

              if (!duplicate)
                {
                  int const saved_slot =
                      ftx_ft8_store_saved_decode_c (ndecodes, kFt8MaxEarly, nsnr, f1, xdt + 0.5f,
                                                   itone.data (), kFt8Nn, state.allsnrs.data (),
                                                   state.f1_save.data (), state.xdt_save.data (),
                                                   state.itone_save.data ());
                  if (saved_slot == 0)
                    {
                      continue;
                    }
                  ndecodes = saved_slot;
                  state.allmessages[static_cast<size_t> (ndecodes - 1)] = msg37;
                }

              if (!duplicate)
                {
                  collector.append (sync, nsnr, callback_dt, f1, msg37, iaptype, qual,
                                    message77.data ());
                  save_a7_entry (state.a7, jseq, xdt, f1, msg37);
                }
            }

          if (request.nzhsym == 41
              && ftx_ft8_should_bail_by_tseq_c (request.ldiskdat, current_sequence_seconds (), 13.4) != 0)
            {
              if (shifted_pass)
                {
                  state.dd = dd_before_shift;
                }
              return;
            }
        }
      if (shifted_pass)
        {
          state.dd = dd_before_shift;
        }
    }
}

void run_ap_passes (Ft8Stage4State& state, Ft8Request const& request, int jseq,
                    std::array<float, kFt8Nh1> const& sbase, AsyncCollector& collector)
{
  constexpr float kSyncAp = 10.0f;
  std::array<signed char, kFt8Bits> zero_bits {};

  bool la8 = true;
  Ft8A7Slot& slot = state.a7[static_cast<size_t> (jseq)];

  if (ftx_ft8_should_run_a7_c (request.lft8apon, request.ncontest,
                               request.nzhsym, slot.previous_count) != 0)
    {
      int newdat_a7 = 1;
      int previous_limit = std::min (slot.previous_count, kFt8MaxEarly);
      if (request.nzhsym < 50)
        {
          previous_limit = std::min (previous_limit, 64);
        }
      for (int i = 0; i < previous_limit; ++i)
        {
          if (stage4_should_cancel ())
            {
              return;
            }
          Ft8A7Entry const& previous = slot.previous[static_cast<size_t> (i)];

          float f1 = 0.0f;
          float xdt = 0.0f;
          float xbase = 0.0f;
          FixedChars<12> call_1 = blank_fixed<12> ();
          FixedChars<12> call_2 = blank_fixed<12> ();
          FixedChars<4> grid4 = blank_fixed<4> ();

          int const request_status =
              ftx_ft8_prepare_a7_request_c (previous.freq, previous.dt, previous.message.data (),
                                            sbase.data (), kFt8Nh1, &f1, &xdt, &xbase,
                                            call_1.data (), call_2.data (), grid4.data ());
          if (request_status == 2)
            {
              break;
            }
          if (request_status == 0)
            {
              continue;
            }

          int nharderrors = -1;
          float dmin = 0.0f;
          float xsnr = 0.0f;
          FixedChars<kFt8DecodedChars> msg37 = blank_fixed<kFt8DecodedChars> ();
          ftx_ft8_a7d_c (state.dd.data (), &newdat_a7, call_1.data (), call_2.data (), grid4.data (),
                         &xdt, &f1, &xbase, &nharderrors, &dmin, msg37.data (), &xsnr);
          if (stage4_should_cancel ())
            {
              return;
            }

          if (nharderrors >= 0)
            {
              int nsnr = 0;
              int iaptype = 0;
              float qual = 0.0f;
              ftx_ft8_finalize_a7_result_c (xsnr, &nsnr, &iaptype, &qual);
              if (ftx_ft8_should_keep_a8_after_a7_c (msg37.data (), request.hiscall.data ()) == 0)
                {
                  la8 = false;
                }
              collector.append (kSyncAp, nsnr, xdt, f1, msg37, iaptype, qual,
                                zero_bits.data ());
              save_a7_entry (state.a7, jseq, xdt, f1, msg37);
            }
        }
    }

  if (ftx_ft8_should_run_a8_c (request.lft8apon, request.ncontest, request.nzhsym,
                               la8 ? 1 : 0,
                               trim_length (request.hiscall.data (), 12),
                               trim_length (request.hisgrid.data (), 6),
                               request.ltry_a8) == 0)
    {
      return;
    }
  if (stage4_should_cancel ())
    {
      return;
    }

  float f1 = static_cast<float> (request.nfqso);
  float xdt = 0.0f;
  float fbest = 0.0f;
  float xsnr = 0.0f;
  float plog = 0.0f;
  FixedChars<kFt8DecodedChars> msg37 = blank_fixed<kFt8DecodedChars> ();
  ftx_ft8_a8d_c (state.dd.data (), request.mycall.data (), request.hiscall.data (),
                 request.hisgrid.data (), &f1, &xdt, &fbest, &xsnr, &plog, msg37.data ());
  if (stage4_should_cancel ())
    {
      return;
    }

  if (msg37[0] == ' ')
    {
      return;
    }

  int nsnr = 0;
  int iaptype = 0;
  float qual = 0.0f;
  float save_freq = 0.0f;
  ftx_ft8_finalize_a8_result_c (plog, xsnr, fbest, &nsnr, &iaptype, &qual, &save_freq);
  collector.append (kSyncAp, nsnr, xdt, fbest, msg37, iaptype, qual, zero_bits.data ());
  save_a7_entry (state.a7, jseq, xdt, save_freq, msg37);
}

} // namespace

extern "C" void ftx_ft8_a7d_c (float* dd0, int* newdat, char const call_1[12],
                               char const call_2[12], char const grid4[4], float* xdt,
                               float* f1, float* xbase, int* nharderrors,
                               float* dmin, char msg37[37], float* xsnr)
{
  if (msg37)
    {
      std::fill_n (msg37, kFt8DecodedChars, ' ');
    }
  if (nharderrors)
    {
      *nharderrors = -1;
    }
  if (dmin)
    {
      *dmin = 1.0e30f;
    }
  if (xsnr)
    {
      *xsnr = -25.0f;
    }

  if (!dd0 || !xdt || !f1 || !xbase || !nharderrors || !dmin || !msg37 || !xsnr)
    {
      return;
    }
  if (stage4_should_cancel ())
    {
      return;
    }

  int newdat_local = newdat ? *newdat : 0;
  std::array<std::complex<float>, kFt8A7DownsampleSize> cd0 {};
  ftx_ft8_downsample_c (dd0, &newdat_local, *f1,
                        reinterpret_cast<fftwf_complex*> (cd0.data ()));
  if (newdat)
    {
      *newdat = newdat_local;
    }

  int ibest = 0;
  float delfbest = 0.0f;
  ftx_ft8_a7_search_initial_c (cd0.data (), kFt8A7Np2, kFt8A7Fs2, *xdt, &ibest, &delfbest);

  std::array<float, 5> tweak {};
  tweak[0] = -delfbest;
  if (std::fabs (delfbest) > 0.0f)
    {
      int const npts = kFt8A7Np2;
      float const fsample = kFt8A7Fs2;
      std::array<std::complex<float>, kFt8A7DownsampleSize> adjusted {};
      ftx_twkfreq1_c (cd0.data (), &npts, &fsample, tweak.data (), adjusted.data ());
      cd0 = adjusted;
    }
  *f1 += delfbest;

  int second_pass_newdat = 0;
  ftx_ft8_downsample_c (dd0, &second_pass_newdat, *f1,
                        reinterpret_cast<fftwf_complex*> (cd0.data ()));

  float sync = 0.0f;
  ftx_ft8_a7_refine_search_c (cd0.data (), kFt8A7Np2, kFt8A7Fs2, ibest, &ibest, &sync, xdt);

  std::array<float, 8 * kFt8Nn> s8 {};
  std::array<float, 174> llra {};
  std::array<float, 174> llrb {};
  std::array<float, 174> llrc {};
  std::array<float, 174> llrd {};
  std::array<float, 174> llre {};
  int nsync = 0;
  ftx_ft8_bitmetrics_scaled_c (cd0.data (), kFt8A7Np2, ibest, 1, 2.83f, s8.data (), &nsync,
                               llra.data (), llrb.data (), llrc.data (), llrd.data (), llre.data ());

  std::array<float, kFt8A7MaxMsg> dmm;
  dmm.fill (1.0e30f);
  float best_dmin = 1.0e30f;
  float pbest = 0.0f;
  int best_nhard = -1;
  FixedChars<kFt8DecodedChars> best_msg = blank_fixed<kFt8DecodedChars> ();
  std::array<int, kFt8Nn> best_itone {};

  for (int imsg = 1; imsg <= kFt8A7MaxMsg; ++imsg)
    {
      if (stage4_should_cancel ())
        {
          return;
        }
      FixedChars<kFt8DecodedChars> candidate_message = blank_fixed<kFt8DecodedChars> ();
      if (ftx_prepare_ft8_a7_candidate_c (imsg, call_1, call_2, grid4,
                                          candidate_message.data ()) == 0)
        {
          continue;
        }

      FixedChars<kFt8DecodedChars> msgsent = blank_fixed<kFt8DecodedChars> ();
      std::array<int, kFt8Nn> itone {};
      std::array<signed char, 174> cw {};
      if (ftx_encode_ft8_candidate_c (candidate_message.data (), msgsent.data (),
                                      itone.data (), cw.data ()) == 0)
        {
          continue;
        }

      float pow = 0.0f;
      float dm = 0.0f;
      int nhard = 0;
      ftx_ft8a7_measure_candidate_c (s8.data (), 8, kFt8Nn, itone.data (), cw.data (),
                                     llra.data (), llrb.data (), llrc.data (), llrd.data (),
                                     &pow, &dm, &nhard);
      dmm[static_cast<size_t> (imsg - 1)] = dm;
      if (dm < best_dmin)
        {
          best_dmin = dm;
          best_msg = msgsent;
          pbest = pow;
          best_nhard = nhard;
          best_itone = itone;
        }
    }

  float dmin2 = 1.0e30f;
  int const accept = ftx_ft8_a7_finalize_metrics_c (dmm.data (), kFt8A7MaxMsg, pbest, *xbase,
                                                     dmin, &dmin2, xsnr);
  if (accept != 0)
    {
      float snr_estimate = *xsnr;
      ftx_ft8_compute_snr_c (s8.data (), 8, kFt8Nn, best_itone.data (), *xbase, 0,
                             nsync, &snr_estimate);
      *xsnr = snr_estimate;
    }
  *nharderrors = accept != 0 ? best_nhard : -1;
  std::copy (best_msg.begin (), best_msg.end (), msg37);

  if (starts_with (best_msg, "CQ ") && is_standard_call (call_2, 12)
      && trim_length (grid4, 4) == 0)
    {
      *nharderrors = -1;
    }
  if (starts_with (best_msg, "QU1RK "))
    {
      *nharderrors = -1;
    }
}

extern "C" void ftx_ft8_a8d_c (float* dd, char const mycall[12], char const dxcall[12],
                               char const dxgrid[6], float* f1a, float* xdt, float* fbest,
                               float* xsnr, float* plog, char msgbest[37])
{
  if (msgbest)
    {
      std::fill_n (msgbest, kFt8DecodedChars, ' ');
    }
  if (xsnr)
    {
      *xsnr = -134.0f;
    }
  if (plog)
    {
      *plog = 0.0f;
    }
  if (fbest)
    {
      *fbest = 0.0f;
    }

  if (!dd || !mycall || !dxcall || !dxgrid || !f1a || !xdt || !fbest || !xsnr || !plog || !msgbest)
    {
      return;
    }
  if (stage4_should_cancel ())
    {
      return;
    }

  float const f1 = *f1a;
  int newdat = 1;
  std::array<std::complex<float>, kFt8A7DownsampleSize> cd {};
  ftx_ft8_downsample_c (dd, &newdat, f1, reinterpret_cast<fftwf_complex*> (cd.data ()));

  float sbest = 0.0f;
  float tbest = 0.0f;
  FixedChars<kFt8DecodedChars> best_message = blank_fixed<kFt8DecodedChars> ();
  std::array<int, kFt8Nn> best_itone {};
  std::array<float, kFt8A7DownsampleSize + 1> best_spectrum {};

  for (int imsg = 1; imsg <= kFt8A8MaxMsg; ++imsg)
    {
      if (stage4_should_cancel ())
        {
          return;
        }
      FixedChars<kFt8DecodedChars> candidate_message = blank_fixed<kFt8DecodedChars> ();
      if (ftx_prepare_ft8_a8_candidate_c (imsg, mycall, dxcall, dxgrid,
                                          candidate_message.data ()) == 0)
        {
          continue;
        }

      FixedChars<kFt8DecodedChars> msgsent = blank_fixed<kFt8DecodedChars> ();
      std::array<int, kFt8Nn> itone {};
      std::array<signed char, 174> codeword {};
      if (ftx_encode_ft8_candidate_c (candidate_message.data (), msgsent.data (),
                                      itone.data (), codeword.data ()) == 0)
        {
          continue;
        }

      std::array<std::complex<float>, kFt8A8Nwave> cwave {};
      generate_ft8_a8_waveform (itone.data (), 0.0f, cwave);

      float spk = 0.0f;
      float candidate_fbest = 0.0f;
      float candidate_tbest = 0.0f;
      std::array<float, kFt8A7DownsampleSize + 1> spectrum {};
      if (ftx_ft8_a8_search_candidate_c (cd.data (), cwave.data (), kFt8A7DownsampleSize,
                                         kFt8A8Nwave, f1, &spk, &candidate_fbest,
                                         &candidate_tbest, spectrum.data ()) == 0)
        {
          continue;
        }

      if (spk > sbest)
        {
          sbest = spk;
          *fbest = candidate_fbest;
          tbest = candidate_tbest;
          best_message = candidate_message;
          best_itone = itone;
          best_spectrum = spectrum;
        }
    }

  if (ftx_ft8_a8_finalize_search_c (best_spectrum.data (), kFt8A7DownsampleSize + 1,
                                    f1, *fbest, xsnr) == 0)
    {
      return;
    }

  std::array<float, 5> tweak {};
  tweak[0] = f1 - *fbest;
  int const npts = kFt8A7DownsampleSize;
  float const fsample = kFt8A7Fs2;
  std::array<std::complex<float>, kFt8A7DownsampleSize> adjusted {};
  ftx_twkfreq1_c (cd.data (), &npts, &fsample, tweak.data (), adjusted.data ());
  cd = adjusted;
  *xdt = tbest;

  int nhard = 0;
  float sigobig = 0.0f;
  ftx_ft8_a8_score_c (cd.data (), kFt8A7DownsampleSize, tbest, best_itone.data (),
                      plog, &nhard, &sigobig);
  if (ftx_ft8_a8_accept_score_c (nhard, *plog, sigobig) == 0)
    {
      return;
    }

  std::copy (best_message.begin (), best_message.end (), msgbest);
}

extern "C" void ftx_ft8_a7d_ref_c (float* dd0, int* newdat, char const call_1[12],
                                   char const call_2[12], char const grid4[4], float* xdt,
                                   float* f1, float* xbase, int* nharderrors,
                                   float* dmin, char msg37[37], float* xsnr)
{
  if (msg37)
    {
      std::fill_n (msg37, kFt8DecodedChars, ' ');
    }
  if (nharderrors)
    {
      *nharderrors = -1;
    }
  if (dmin)
    {
      *dmin = 1.0e30f;
    }
  if (xsnr)
    {
      *xsnr = -25.0f;
    }

  if (!dd0 || !call_1 || !call_2 || !grid4 || !xdt || !f1 || !xbase
      || !nharderrors || !dmin || !msg37 || !xsnr)
    {
      if (newdat)
        {
          *newdat = 0;
        }
      return;
    }

  int newdat_local = newdat ? (*newdat != 0 ? 1 : 0) : 0;
  FixedChars<12> call1_copy = fixed_from_chars<12> (call_1);
  FixedChars<12> call2_copy = fixed_from_chars<12> (call_2);
  FixedChars<4> grid4_copy = fixed_from_chars<4> (grid4);
  FixedChars<kFt8DecodedChars> msg_copy = blank_fixed<kFt8DecodedChars> ();

  __ft8_a7_MOD_ft8_a7d (dd0, &newdat_local, call1_copy.data (), call2_copy.data (),
                        grid4_copy.data (), xdt, f1, xbase, nharderrors, dmin,
                        msg_copy.data (), xsnr, call1_copy.size (), call2_copy.size (),
                        grid4_copy.size (), msg_copy.size ());

  if (newdat)
    {
      *newdat = newdat_local != 0 ? 1 : 0;
    }
  std::copy (msg_copy.begin (), msg_copy.end (), msg37);
}

extern "C" void ftx_ft8_a8d_ref_c (float* dd, char const mycall[12], char const dxcall[12],
                                   char const dxgrid[6], float* f1a, float* xdt, float* fbest,
                                   float* xsnr, float* plog, char msgbest[37])
{
  if (msgbest)
    {
      std::fill_n (msgbest, kFt8DecodedChars, ' ');
    }
  if (xsnr)
    {
      *xsnr = -134.0f;
    }
  if (plog)
    {
      *plog = 0.0f;
    }
  if (fbest)
    {
      *fbest = 0.0f;
    }

  if (!dd || !mycall || !dxcall || !dxgrid || !f1a || !xdt || !fbest || !xsnr || !plog || !msgbest)
    {
      return;
    }

  FixedChars<12> mycall_copy = fixed_from_chars<12> (mycall);
  FixedChars<12> dxcall_copy = fixed_from_chars<12> (dxcall);
  FixedChars<6> dxgrid_copy = fixed_from_chars<6> (dxgrid);
  FixedChars<kFt8DecodedChars> msg_copy = blank_fixed<kFt8DecodedChars> ();

  ft8_a8d_ (dd, mycall_copy.data (), dxcall_copy.data (), dxgrid_copy.data (),
            f1a, xdt, fbest, xsnr, plog, msg_copy.data (),
            mycall_copy.size (), dxcall_copy.size (), dxgrid_copy.size (), msg_copy.size ());

  std::copy (msg_copy.begin (), msg_copy.end (), msgbest);
}

extern "C" void ft8_a8d_ (float* dd, char* mycall, char* dxcall, char* dxgrid,
                          float* f1a, float* xdt, float* fbest, float* xsnr,
                          float* plog, char* msgbest,
                          size_t mycall_len, size_t dxcall_len,
                          size_t dxgrid_len, size_t msgbest_len)
{
  FixedChars<12> mycall_copy = blank_fixed<12> ();
  FixedChars<12> dxcall_copy = blank_fixed<12> ();
  FixedChars<6> dxgrid_copy = blank_fixed<6> ();
  FixedChars<kFt8DecodedChars> msg_copy = blank_fixed<kFt8DecodedChars> ();

  if (mycall)
    {
      std::copy_n (mycall, std::min (static_cast<int>(mycall_len), static_cast<int>(mycall_copy.size ())), mycall_copy.data ());
    }
  if (dxcall)
    {
      std::copy_n (dxcall, std::min (static_cast<int>(dxcall_len), static_cast<int>(dxcall_copy.size ())), dxcall_copy.data ());
    }
  if (dxgrid)
    {
      std::copy_n (dxgrid, std::min (static_cast<int>(dxgrid_len), static_cast<int>(dxgrid_copy.size ())), dxgrid_copy.data ());
    }

  ftx_ft8_a8d_c (dd, mycall_copy.data (), dxcall_copy.data (), dxgrid_copy.data (),
                 f1a, xdt, fbest, xsnr, plog, msg_copy.data ());

  if (msgbest)
    {
      size_t const copy_size = std::min (static_cast<int>(msgbest_len), static_cast<int>(msg_copy.size ()));
      std::fill_n (msgbest, msgbest_len, ' ');
      std::copy_n (msg_copy.data (), copy_size, msgbest);
    }
}

extern "C" void __ft8_a7_MOD_ft8_a7d (float* dd0, int* newdat, char* call_1, char* call_2,
                                      char* grid4, float* xdt, float* f1, float* xbase,
                                      int* nharderrors, float* dmin, char* msg37, float* xsnr,
                                      size_t call_1_len, size_t call_2_len,
                                      size_t grid4_len, size_t msg37_len)
{
  FixedChars<12> call1_copy = blank_fixed<12> ();
  FixedChars<12> call2_copy = blank_fixed<12> ();
  FixedChars<4> grid4_copy = blank_fixed<4> ();
  FixedChars<kFt8DecodedChars> msg_copy = blank_fixed<kFt8DecodedChars> ();

  if (call_1)
    {
      std::copy_n (call_1, std::min (static_cast<int>(call_1_len), static_cast<int>(call1_copy.size ())), call1_copy.data ());
    }
  if (call_2)
    {
      std::copy_n (call_2, std::min (static_cast<int>(call_2_len), static_cast<int>(call2_copy.size ())), call2_copy.data ());
    }
  if (grid4)
    {
      std::copy_n (grid4, std::min (static_cast<int>(grid4_len), static_cast<int>(grid4_copy.size ())), grid4_copy.data ());
    }

  ftx_ft8_a7d_c (dd0, newdat, call1_copy.data (), call2_copy.data (), grid4_copy.data (),
                 xdt, f1, xbase, nharderrors, dmin, msg_copy.data (), xsnr);

  if (msg37)
    {
      size_t const copy_size = std::min (static_cast<int>(msg37_len), static_cast<int>(msg_copy.size ()));
      std::fill_n (msg37, msg37_len, ' ');
      std::copy_n (msg_copy.data (), copy_size, msg37);
    }
}

extern "C" void ftx_ft8_stage4_reset_c ()
{
  stage4_state ().reset ();
}

extern "C" void ftx_ft8_stage4_set_cancel_c (int cancel)
{
  stage4_cancel_requested ().store (cancel != 0, std::memory_order_relaxed);
}

extern "C" void ftx_ft8_stage4_set_deadline_ms_c (long long deadline_ms)
{
  stage4_deadline_ms ().store (deadline_ms, std::memory_order_relaxed);
}

extern "C" void ftx_ft8_stage4_set_ldpc_osd_c (int maxosd, int norder)
{
  stage4_ldpc_maxosd_override ().store (maxosd, std::memory_order_relaxed);
  stage4_ldpc_norder_override ().store (norder, std::memory_order_relaxed);
}

extern "C" void ftx_ft8_stage4_set_supplemental_c (int supplemental)
{
  stage4_supplemental_requested ().store (supplemental != 0, std::memory_order_relaxed);
}

extern "C" void ftx_ft8_async_decode_stage4_c (short const* iwave,
                                               int* nqsoprogress, int* nfqso, int* nftx,
                                               int* nutc, int* nfa, int* nfb,
                                               int* nzhsym, int* ndepth, float* emedelay,
                                               int* ncontest, int* nagain, int* lft8apon,
                                               int* ltry_a8, int* lapcqonly, int* napwid,
                                               char const* mycall, char const* hiscall,
                                               char const* hisgrid, int* ldiskdat,
                                               float* syncs, int* snrs, float* dts, float* freqs,
                                               int* naps, float* quals, signed char* bits77,
                                               char* decodeds, int* nout)
{
  if (!iwave || !nqsoprogress || !nfqso || !nftx || !nutc || !nfa || !nfb || !nzhsym
      || !ndepth || !emedelay || !ncontest || !nagain || !lft8apon || !lapcqonly
      || !ltry_a8 || !napwid || !mycall || !hiscall || !hisgrid || !ldiskdat
      || !snrs || !dts || !freqs || !naps || !quals || !bits77 || !decodeds || !nout)
    {
      if (nout)
        {
          *nout = 0;
        }
      return;
    }

  AsyncCollector collector;
  collector.syncs = syncs;
  collector.snrs = snrs;
  collector.dts = dts;
  collector.freqs = freqs;
  collector.naps = naps;
  collector.quals = quals;
  collector.bits77 = bits77;
  collector.decodeds = decodeds;
  collector.nout = nout;
  collector.reset ();
  if (stage4_should_cancel ())
    {
      return;
    }

  Ft8Request request;
  request.iwave = iwave;
  request.nqsoprogress = *nqsoprogress;
  request.nfqso = *nfqso;
  request.nftx = *nftx;
  request.nutc = *nutc;
  request.nfa = *nfa;
  request.nfb = *nfb;
  request.nzhsym = *nzhsym;
  request.ndepth = *ndepth;
  request.emedelay = *emedelay;
  request.ncontest = *ncontest;
  request.nagain = *nagain;
  request.lft8apon = *lft8apon;
  request.ltry_a8 = *ltry_a8;
  request.lapcqonly = *lapcqonly;
  request.napwid = *napwid;
  request.ldiskdat = *ldiskdat;
  request.mycall = fixed_from_chars<12> (mycall);
  request.hiscall = fixed_from_chars<12> (hiscall);
  request.hisgrid = fixed_from_chars<6> (hisgrid);

  Ft8Stage4State& state = stage4_state ();
  if (request.nutc != state.early_nutc)
    {
      // Early/full FT8 passes may be interrupted by the live deadline. Never
      // carry saved early decodes, tones, or subtractions into the next UTC
      // slot; otherwise stale AP decodes are emitted with a fresh timestamp.
      state.resetEarlySlotState ();
      state.early_nutc = request.nutc;
    }
  int const jseq = sequence_index_for_utc (request.nutc);
  prepare_a7_tables (state.a7, state.nutc0, request.nutc, request.nzhsym, jseq);

  int stage_action = 0;
  int refine = 0;
  ftx_ft8_plan_decode_stage_c (request.ndepth, request.nzhsym, state.ndec_early,
                               request.nagain, &stage_action, &refine);
  if (stage_action == 1)
    {
      state.ndec_early = 0;
      return;
    }
  if (stage4_should_cancel ())
    {
      return;
    }

  if (request.nzhsym == 50 && stage_action != 4)
    {
      copy_audio_to_float (request.iwave, state.dd);
    }

  std::array<int, 58> apsym {};
  std::array<int, 10> aph10 {};
  ftx_prepare_ft8_ap_c (request.mycall.data (), request.hiscall.data (),
                        request.ncontest, apsym.data (), aph10.data ());

  if (request.nzhsym <= 47)
    {
      copy_audio_to_float (request.iwave, state.dd);
      state.dd1 = state.dd;
    }

  int ndecodes = request.nzhsym == 41 ? 0 : state.ndec_early;
  if (request.nzhsym == 41)
    {
      state.allmessages.fill (blank_fixed<kFt8DecodedChars> ());
      state.allsnrs.fill (0);
    }

  if (stage_action == 2)
    {
      state.dd1 = state.dd;
      state.ndec_early = 0;
      if (request.nzhsym < 50)
        {
          state.ndec_early = ndecodes;
        }
      if (stage4_should_cancel ())
        {
          return;
        }
      std::array<float, kFt8Nh1> blank_sbase {};
      auto const& ap_sbase = state.early_sbase_valid ? state.early_sbase : blank_sbase;
      run_ap_passes (state, request, jseq, ap_sbase, collector);
      return;
    }

  if (stage_action == 3)
    {
      if (stage4_should_cancel ())
        {
          return;
        }
      state.lsubtracted.fill (false);
      ftx_ft8_apply_saved_subtractions_c (state.dd.data (), state.itone_save.data (), kFt8Nn,
                                          state.ndec_early, state.f1_save.data (),
                                          state.xdt_save.data (), state.lsubtracted.data (),
                                          1, refine != 0 ? 1 : 0);
      state.dd1 = state.dd;
      std::array<float, kFt8Nh1> blank_sbase {};
      auto const& ap_sbase = state.early_sbase_valid ? state.early_sbase : blank_sbase;
      run_ap_passes (state, request, jseq, ap_sbase, collector);
      return;
    }

  if (stage_action == 4)
    {
      if (stage4_should_cancel ())
        {
          return;
        }
      restore_carried_audio (state.dd, state.dd1, request.iwave);
      ftx_ft8_apply_saved_subtractions_c (state.dd.data (), state.itone_save.data (), kFt8Nn,
                                          state.ndec_early, state.f1_save.data (),
                                          state.xdt_save.data (), state.lsubtracted.data (),
                                          0, 1);
    }

  int ifa = request.nfa;
  int ifb = request.nfb;
  if (request.nzhsym == 50 && request.nagain != 0)
    {
      copy_audio_to_float (request.iwave, state.dd);
      ifa = request.nfqso - 20;
      ifb = request.nfqso + 20;
    }

  std::array<float, kFt8Nh1> sbase {};
  if (stage4_should_cancel ())
    {
      return;
    }
  run_main_passes (state, request, jseq, apsym, aph10, ifa, ifb, ndecodes, sbase, collector);
  if (stage4_should_cancel ())
    {
      return;
    }

  state.ndec_early = 0;
  if (request.nzhsym < 50)
    {
      state.ndec_early = ndecodes;
      state.early_sbase = sbase;
      state.early_sbase_valid = true;
    }

  run_ap_passes (state, request, jseq, sbase, collector);
}

extern "C" void ftx_ft8_emit_results_c (int* nutc, int* ncontest, int* nagain,
                                        int* b_superfox, char const* mycall,
                                        char const* mygrid, char const* temp_dir,
                                        float const* syncs, int const* snrs,
                                        float const* dts, float const* freqs,
                                        int const* naps, float const* quals,
                                        char const* decodeds, int* nout,
                                        int* decoded_count)
{
  if (!nutc || !ncontest || !nagain || !b_superfox || !mycall || !mygrid || !temp_dir
      || !syncs || !snrs || !dts || !freqs || !naps || !quals || !decodeds || !nout)
    {
      if (decoded_count)
        {
          *decoded_count = 0;
        }
      return;
    }

  int const count = std::max (0, std::min (*nout, kFt8MaxLines));
  if (decoded_count)
    {
      *decoded_count = count;
    }

  std::string const temp_dir_path = trim_block (temp_dir, 500);
  std::string const decoded_path = temp_dir_path.empty ()
      ? "decoded.txt"
      : temp_dir_path + "/decoded.txt";

  std::ofstream decoded_file {
      decoded_path,
      std::ios::out | (*nagain != 0 ? std::ios::app : std::ios::trunc)
  };

  FixedChars<12> const mycall_fixed = fixed_from_chars<12> (mycall);
  FixedChars<6> const mygrid_fixed = fixed_from_chars<6> (mygrid);
  std::string const mycall_trimmed = trim_fixed (mycall_fixed);
  bool const fox_mode = *ncontest == 6;
  Ft8EmitState& emit_state = ft8_emit_state ();
  std::string const hounds_path = temp_dir_path.empty ()
      ? "houndcallers.txt"
      : temp_dir_path + "/houndcallers.txt";

  if (fox_mode && !std::ifstream {hounds_path}.good ())
    {
      emit_state.reset ();
    }

  int current_n30 = 0;
  if (fox_mode)
    {
      current_n30 = update_ft8_n30_state (emit_state, *nutc);
    }

  for (int index = 0; index < count; ++index)
    {
      std::string decoded {decodeds + index * kFt8DecodedChars, kFt8DecodedChars};
      std::string const stdout_line =
          format_ft8_stdout_line (*nutc, syncs[index], snrs[index], dts[index],
                                  freqs[index], decoded, naps[index], quals[index]);
      std::cout << stdout_line << '\n';

      if (decoded_file.is_open ())
        {
          decoded_file << format_ft8_decoded_file_line (*nutc, syncs[index], snrs[index],
                                                        dts[index], freqs[index], decoded,
                                                        naps[index], quals[index])
                       << '\n';
        }

      if (fox_mode)
        {
          Ft8FoxEntry entry;
          if (should_collect_fox_entry (decoded, mycall_trimmed, *b_superfox != 0,
                                        static_cast<int> (std::lround (freqs[index])), entry))
            {
              entry.snr = snrs[index];
              entry.n30 = current_n30;
              emit_state.fox_entries.push_back (entry);
            }
        }
    }

  std::cout.flush ();
  if (decoded_file.is_open ())
    {
      decoded_file.flush ();
    }

  if (fox_mode)
    {
      write_houndcallers_file (hounds_path, emit_state, mygrid_fixed, current_n30);
    }
}

extern "C" int ftx_superfox_unpack_lines_c (unsigned char const* xdec,
                                            int use_otp,
                                            char* lines_out,
                                            int line_stride,
                                            int max_lines)
{
  if (!xdec)
    {
      return 0;
    }

  std::array<unsigned char, kSuperFoxPackedSymbols> payload {};
  std::copy_n (xdec, kSuperFoxPackedSymbols, payload.data ());
  std::vector<std::string> const lines = superfox_unpack_lines (payload, use_otp != 0);
  copy_superfox_lines (lines, lines_out, line_stride, max_lines);
  return static_cast<int> (lines.size ());
}

extern "C" int ftx_superfox_analytic_c (float const* dd,
                                        int npts,
                                        float* real_out,
                                        float* imag_out)
{
  if (!dd || !real_out || !imag_out || npts <= 0)
    {
      return 0;
    }

  std::vector<std::complex<float>> analytic (static_cast<size_t> (npts));
  if (!superfox_analytic_signal (dd, npts, analytic.data ()))
    {
      return 0;
    }

  for (int i = 0; i < npts; ++i)
    {
      real_out[i] = analytic[static_cast<size_t> (i)].real ();
      imag_out[i] = analytic[static_cast<size_t> (i)].imag ();
    }
  return 1;
}

extern "C" int ftx_superfox_remove_tone_c (float* real_io,
                                           float* imag_io,
                                           int npts,
                                           float fsync)
{
  if (!real_io || !imag_io || npts != kFt8NMax)
    {
      return 0;
    }

  std::vector<std::complex<float>> c0 (static_cast<size_t> (npts));
  for (int i = 0; i < npts; ++i)
    {
      c0[static_cast<size_t> (i)] =
          std::complex<float> {real_io[i], imag_io[i]};
    }

  if (!superfox_remove_tone (c0.data (), fsync))
    {
      return 0;
    }

  for (int i = 0; i < npts; ++i)
    {
      real_io[i] = c0[static_cast<size_t> (i)].real ();
      imag_io[i] = c0[static_cast<size_t> (i)].imag ();
  }
  return 1;
}

extern "C" int ftx_superfox_qpc_sync_c (float const* real_in,
                                        float const* imag_in,
                                        int npts,
                                        float fsync,
                                        float ftol,
                                        float* f2_out,
                                        float* t2_out,
                                        float* snrsync_out)
{
  if (!real_in || !imag_in || !f2_out || !t2_out || !snrsync_out || npts != kFt8NMax)
    {
      return 0;
    }

  std::vector<std::complex<float>> c0 (static_cast<size_t> (npts));
  for (int i = 0; i < npts; ++i)
    {
      c0[static_cast<size_t> (i)] = std::complex<float> {real_in[i], imag_in[i]};
    }

  float f2 = 0.0f;
  float t2 = 0.0f;
  float snrsync = 0.0f;
  if (!superfox_qpc_sync (c0.data (), 12000.0f, fsync, ftol, f2, t2, snrsync))
    {
      return 0;
    }

  *f2_out = f2;
  *t2_out = t2;
  *snrsync_out = snrsync;
  return 1;
}

extern "C" int ftx_superfox_qpc_likelihoods2_c (float const* s3,
                                                int rows,
                                                int cols,
                                                float EsNo,
                                                float No,
                                                float* py_out)
{
  if (!s3 || !py_out || rows <= 0 || cols <= 0)
    {
      return 0;
    }

  superfox_qpc_likelihoods (s3, rows, cols, EsNo, No, py_out);
  return 1;
}

extern "C" int ftx_superfox_qpc_snr_c (float const* s3,
                                       int rows,
                                       int cols,
                                       unsigned char const* y,
                                       float* snr_out)
{
  if (!s3 || !y || !snr_out || rows <= 0 || cols <= 0)
    {
      return 0;
    }

  *snr_out = superfox_qpc_snr (s3, rows, cols, y);
  return 1;
}

extern "C" int ftx_superfox_qpc_decode2_c (float const* real_in,
                                           float const* imag_in,
                                           int npts,
                                           float fsync,
                                           float ftol,
                                           int ndepth,
                                           float dth,
                                           float damp,
                                           unsigned char* xdec_out,
                                           int* crc_ok_out,
                                           float* snrsync_out,
                                           float* fbest_out,
                                           float* tbest_out,
                                           float* snr_out)
{
  if (!real_in || !imag_in || !xdec_out || !crc_ok_out
      || !snrsync_out || !fbest_out || !tbest_out || !snr_out
      || npts != kFt8NMax)
    {
      return 0;
    }

  std::vector<std::complex<float>> c0 (static_cast<size_t> (npts));
  for (int i = 0; i < npts; ++i)
    {
      c0[static_cast<size_t> (i)] = std::complex<float> {real_in[i], imag_in[i]};
    }

  SuperFoxQpcDecodeResult result;
  if (!superfox_qpc_decode2 (c0.data (), fsync, ftol, ndepth, dth, damp, result))
    {
      return 0;
    }

  std::copy (result.xdec.begin (), result.xdec.end (), xdec_out);
  *crc_ok_out = result.crc_ok ? 1 : 0;
  *snrsync_out = result.snrsync;
  *fbest_out = result.fbest;
  *tbest_out = result.tbest;
  *snr_out = result.snr;
  return 1;
}

extern "C" int ftx_superfox_decode_lines_c (short const* iwave,
                                            int nfqso,
                                            int ntol,
                                            char* lines_out,
                                            int line_stride,
                                            int max_lines,
                                            int* nsnr_out,
                                            float* freq_out,
                                            float* dt_out)
{
  std::vector<std::string> lines;
  int nsnr = 0;
  float freq = 0.0f;
  float dt = 0.0f;
  if (!superfox_decode_lines_from_wave (iwave, nfqso, ntol, lines, nsnr, freq, dt))
    {
      if (nsnr_out)
        {
          *nsnr_out = 0;
        }
      if (freq_out)
        {
          *freq_out = 0.0f;
        }
      if (dt_out)
        {
          *dt_out = 0.0f;
        }
      return 0;
    }

  copy_superfox_lines (lines, lines_out, line_stride, max_lines);
  if (nsnr_out)
    {
      *nsnr_out = nsnr;
    }
  if (freq_out)
    {
      *freq_out = freq;
    }
  if (dt_out)
    {
      *dt_out = dt;
    }
  return static_cast<int> (lines.size ());
}

extern "C" void sfox_ana_ (float* dd, int* npts, std::complex<float>* c0, int* npts2)
{
  if (!dd || !npts || !c0)
    {
      return;
    }
  int const count = *npts;
  if (npts2)
    {
      *npts2 = count;
    }
  superfox_analytic_signal (dd, count, c0);
}

extern "C" void sfox_remove_tone_ (std::complex<float>* c0, float* fsync)
{
  if (!c0 || !fsync)
    {
      return;
    }
  superfox_remove_tone (c0, *fsync);
}

extern "C" void qpc_sync_ (std::complex<float>* crcvd0, float* fsample, int* /*isync*/, float* fsync,
                           float* ftol, float* f2, float* t2, float* snrsync)
{
  if (!crcvd0 || !fsync || !ftol || !f2 || !t2 || !snrsync)
    {
      return;
    }

  float f2_local = 0.0f;
  float t2_local = 0.0f;
  float snr_local = 0.0f;
  if (!superfox_qpc_sync (crcvd0, fsample ? *fsample : 12000.0f, *fsync, *ftol,
                          f2_local, t2_local, snr_local))
    {
      return;
    }
  *f2 = f2_local;
  *t2 = t2_local;
  *snrsync = snr_local;
}

extern "C" void qpc_likelihoods2_ (float* py, float* s3, float* EsNo, float* No)
{
  if (!py || !s3 || !EsNo || !No)
    {
      return;
    }
  superfox_qpc_likelihoods (s3, kSuperFoxQpcRows, kSuperFoxQpcCols, *EsNo, *No, py);
}

extern "C" void qpc_snr_ (float* s3, signed char* y, float* snr)
{
  if (!s3 || !y || !snr)
    {
      return;
    }
  std::array<unsigned char, kSuperFoxQpcCols> symbols {};
  for (int i = 0; i < kSuperFoxQpcCols; ++i)
    {
      symbols[static_cast<size_t> (i)] = static_cast<unsigned char> (std::max<int> (0, y[i]));
    }
  *snr = superfox_qpc_snr (s3, kSuperFoxQpcRows, kSuperFoxQpcCols, symbols.data ());
}

extern "C" void qpc_decode2_ (std::complex<float>* c0, float* fsync, float* ftol,
                              signed char* xdec, int* ndepth, float* dth, float* damp,
                              int* crc_ok, float* snrsync, float* fbest, float* tbest, float* snr)
{
  if (!c0 || !fsync || !ftol || !xdec || !ndepth || !dth || !damp
      || !crc_ok || !snrsync || !fbest || !tbest || !snr)
    {
      return;
    }

  SuperFoxQpcDecodeResult result;
  if (!superfox_qpc_decode2 (c0, *fsync, *ftol, *ndepth, *dth, *damp, result))
    {
      *crc_ok = 0;
      return;
    }

  for (int i = 0; i < kSuperFoxPackedSymbols; ++i)
    {
      xdec[i] = static_cast<signed char> (result.xdec[static_cast<size_t> (i)]);
    }
  *crc_ok = result.crc_ok ? 1 : 0;
  *snrsync = result.snrsync;
  *fbest = result.fbest;
  *tbest = result.tbest;
  *snr = result.snr;
}

extern "C" void ftx_ft8_decode_and_emit_params_c (short const* iwave,
                                                  params_block_t const* params,
                                                  char const* temp_dir,
                                                  int* decoded_count)
{
  if (!iwave || !params || !temp_dir)
    {
      if (decoded_count)
        {
          *decoded_count = 0;
        }
      return;
    }

  int const ncontest = params->nexp_decode & 7;
  if (ncontest == 7 && params->b_superfox && params->b_even_seq)
    {
      if (params->nzhsym >= 50)
        {
          std::vector<std::string> lines;
          int nsnr = 0;
          float freq = 0.0f;
          float dt = 0.0f;
          if (superfox_decode_lines_from_wave (iwave, params->nfqso, params->ntol,
                                               lines, nsnr, freq, dt))
            {
              for (std::string const& line : lines)
                {
                  std::cout << format_superfox_stdout_line (params->nutc, nsnr, dt, freq, line)
                            << '\n';
                }
              std::cout.flush ();
            }
        }
      if (decoded_count)
        {
          *decoded_count = 0;
        }
      return;
    }

  int nqsoprogress = params->nQSOProgress;
  int nfqso = params->nfqso;
  int nftx = params->nftx;
  int nutc = params->nutc;
  int nfa = params->nfa;
  int nfb = params->nfb;
  int nzhsym = params->nzhsym;
  int ndepth = params->ndepth;
  float emedelay = params->emedelay;
  int nagain = params->nagain ? 1 : 0;
  int lft8apon = params->lft8apon ? 1 : 0;
  int ltry_a8 = (params->nzhsym == 41 || params->lmultift8) ? 1 : 0;
  int lapcqonly = params->lapcqonly ? 1 : 0;
  int napwid = params->napwid;
  int ldiskdat = params->ndiskdat ? 1 : 0;
  int b_superfox = params->b_superfox ? 1 : 0;
  int nout = 0;

  std::array<float, kFt8MaxLines> syncs {};
  std::array<int, kFt8MaxLines> snrs {};
  std::array<float, kFt8MaxLines> dts {};
  std::array<float, kFt8MaxLines> freqs {};
  std::array<int, kFt8MaxLines> naps {};
  std::array<float, kFt8MaxLines> quals {};
  std::array<signed char, kFt8Bits * kFt8MaxLines> bits77 {};
  std::array<char, kFt8DecodedChars * kFt8MaxLines> decodeds {};

  ftx_ft8_async_decode_stage4_c (iwave, &nqsoprogress, &nfqso, &nftx, &nutc, &nfa, &nfb,
                                 &nzhsym, &ndepth, &emedelay, const_cast<int*> (&ncontest),
                                 &nagain, &lft8apon, &ltry_a8, &lapcqonly, &napwid,
                                 params->mycall, params->hiscall, params->hisgrid, &ldiskdat,
                                 syncs.data (), snrs.data (), dts.data (), freqs.data (),
                                 naps.data (), quals.data (), bits77.data (), decodeds.data (),
                                 &nout);

  int emitted_count = 0;
  ftx_ft8_emit_results_c (&nutc, const_cast<int*> (&ncontest), &nagain, &b_superfox,
                          params->mycall, params->mygrid, temp_dir, syncs.data (),
                          snrs.data (), dts.data (), freqs.data (), naps.data (),
                          quals.data (), decodeds.data (), &nout, &emitted_count);

  if (decoded_count)
    {
      *decoded_count = emitted_count;
    }
}

extern "C" void ftx_q65_decode_and_emit_params_c (short const* iwave,
                                                  params_block_t const* params,
                                                  char const* temp_dir,
                                                  int* decoded_count);
extern "C" void ftx_fst4_decode_and_emit_params_c (short const* iwave,
                                                   params_block_t const* params,
                                                   char const* temp_dir,
                                                   int* decoded_count);

extern "C" void ftx_native_decode_and_emit_params_c (short const* iwave,
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

  switch (params->nmode)
    {
    case 2:
      ftx_ft2_decode_and_emit_params_c (iwave, params, temp_dir, decoded_count);
      break;
    case 8:
      ftx_ft8_decode_and_emit_params_c (iwave, params, temp_dir, decoded_count);
      break;
    case 5:
      ftx_ft4_decode_and_emit_params_c (iwave, params, temp_dir, decoded_count);
      break;
    case 66:
      ftx_q65_decode_and_emit_params_c (iwave, params, temp_dir, decoded_count);
      break;
    case 240:
    case 241:
    case 242:
      ftx_fst4_decode_and_emit_params_c (iwave, params, temp_dir, decoded_count);
      break;
    default:
      break;
    }
}
