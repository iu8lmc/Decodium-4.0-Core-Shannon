// -*- Mode: C++ -*-
#include "wsjtx_config.h"
#include "commons.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <fftw3.h>

namespace
{

constexpr int kFt8NMax {15 * 12000};
constexpr int kFt8Nn {79};
constexpr int kFt8Nh1 {1920};
constexpr int kFt8MaxCand {1000};
constexpr int kFt8MaxEarly {200};
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

extern "C"
{
  void ftx_sync8_search_c (float const* dd, int npts, float nfa, float nfb,
                           float syncmin, float nfqso, int maxcand,
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
  int legacy_pack77_unpack77bits_c (signed char const* message77, int received,
                                    char msgsent[37], int* quirky_out);
  int ftx_ft8_message77_to_itone_c (signed char const* message77, int* itone_out);
  void __ft8_a7_MOD_ft8_a7d (float* dd0, int* newdat, char* call_1, char* call_2,
                             char* grid4, float* xdt, float* f1, float* xbase,
                             int* nharderrors, float* dmin, char* msg37, float* xsnr,
                             size_t, size_t, size_t, size_t);
  void ft8_a8d_ (float* dd, char* mycall, char* dxcall, char* dxgrid,
                 float* f1a, float* xdt, float* fbest, float* xsnr,
                 float* plog, char* msgbest,
                 size_t, size_t, size_t, size_t);
  void azdist_ (char* MyGrid, char* HisGrid, double* utch, int* nAz, int* nEl,
                int* nDmiles, int* nDkm, int* nHotAz, int* nHotABetter,
                fortran_charlen_t, fortran_charlen_t);
  void sfrx_sub_ (int* nyymmdd, int* nutc, int* nfqso, int* ntol, short* iwave);
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
  std::copy_n (text.data (), std::min (text.size (), N), out.data ());
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
  int nAz = 0;
  int nEl = 0;
  int nDmiles = 0;
  int nDkm = 9999;
  int nHotAz = 0;
  int nHotABetter = 0;
  double utch = 0.0;
  azdist_ (const_cast<char*> (mygrid.data ()),
           hisgrid.data (),
           &utch,
           &nAz,
           &nEl,
           &nDmiles,
           &nDkm,
           &nHotAz,
           &nHotABetter,
           static_cast<fortran_charlen_t> (mygrid.size ()),
           static_cast<fortran_charlen_t> (hisgrid.size ()));
  return nDkm;
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
  std::array<Ft8A7Slot, kFt8SequenceCount> slots {};
  int nutc0 {-1};

  void reset ()
  {
    slots = {};
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
  std::array<Ft8A7Slot, kFt8SequenceCount> a7 {};
  int nutc0 {-1};
  int ndec_early {0};

  void reset ()
  {
    dd.fill (0.0f);
    dd1.fill (0.0f);
    allmessages.fill (blank_fixed<kFt8DecodedChars> ());
    allsnrs.fill (0);
    itone_save.fill (0);
    f1_save.fill (0.0f);
    xdt_save.fill (0.0f);
    lsubtracted.fill (false);
    a7 = {};
    nutc0 = -1;
    ndec_early = 0;
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

void prepare_a7_tables (std::array<Ft8A7Slot, kFt8SequenceCount>& slots, int& nutc0,
                        int nutc, int nzhsym, int jseq)
{
  if (nzhsym != 41 && nutc == nutc0)
    {
      return;
    }

  Ft8A7Slot& slot = slots[static_cast<size_t> (jseq)];
  slot.previous_count = slot.current_count;
  for (int i = 0; i < slot.current_count && i < kFt8MaxEarly; ++i)
    {
      slot.previous[static_cast<size_t> (i)] = slot.current[static_cast<size_t> (i)];
    }
  slot.current_count = 0;
  nutc0 = nutc;
}

void save_a7_entry (std::array<Ft8A7Slot, kFt8SequenceCount>& slots, int jseq, float dt, float freq,
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

  Ft8A7Slot& slot = slots[static_cast<size_t> (jseq)];
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
  if (ndepth <= 2)
    {
      syncmin = 8;
    }
  return syncmin;
}

void plan_ft8_ldpc_decode (Ft8Request const& request, float f1,
                           int& Keff, int& maxosd, int& norder)
{
  norder = 2;
  maxosd = 2;
  if (request.ndepth == 1)
    {
      maxosd = -1;
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
                                float const* candidate_values, float const* sbase,
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

  int ibest = 0;
  float delfbest = 0.0f;
  ftx_ft8_a7_search_initial_c (cd0.data (), kFt8A7Np2, kFt8A7Fs2, xdt, &ibest, &delfbest);
  f1 += delfbest;

  int second_pass_newdat = 0;
  ftx_ft8_downsample_c (dd0, &second_pass_newdat, f1,
                        reinterpret_cast<fftwf_complex*> (cd0.data ()));

  ftx_ft8_a7_refine_search_c (cd0.data (), kFt8A7Np2, kFt8A7Fs2, ibest,
                              &ibest, &sync, &xdt);
  xdt += 0.5f;

  int nsync = 0;
  ftx_ft8_bitmetrics_scaled_c (cd0.data (), kFt8A7Np2, ibest, imetric, 3.2f,
                               s8.data (), &nsync, llra.data (), llrb.data (),
                               llrc.data (), llrd.data (), llre.data ());
  *newdat = local_newdat;

  if (nsync <= ft8_candidate_sync_threshold (imetric, request.ndepth))
    {
      return false;
    }

  int const npasses = ftx_ft8_select_npasses_c (request.lft8apon, request.lapcqonly,
                                                request.ncontest, request.nzhsym,
                                                request.nqsoprogress);
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
      int pass_iaptype = 0;
      if (ftx_ft8_prepare_decode_pass_c (pass_index, request.nqsoprogress,
                                         request.lapcqonly, request.ncontest,
                                         request.nfqso, request.nftx, f1,
                                         request.napwid, apsym.data (), aph10.data (),
                                         llra.data (), llrb.data (), llrc.data (),
                                         llrd.data (), llre.data (), llrz.data (),
                                         apmask.data (), &pass_iaptype) == 0)
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
      plan_ft8_ldpc_decode (request, f1, Keff, maxosd, norder);

      int ntype = 0;
      int pass_nharderrors = -1;
      float pass_dmin = 0.0f;
      ftx_decode174_91_c (llrz.data (), Keff, maxosd, norder, apmask_bits.data (),
                          message91.data (), cw.data (), &ntype,
                          &pass_nharderrors, &pass_dmin);
      if (pass_nharderrors < 0 || pass_nharderrors > 36)
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
  prepare_a7_tables (history.slots, history.nutc0, nutc, nzhsym, jseq);
}

extern "C" int ftx_ft8_a7_previous_count_c (int jseq)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount)
    {
      return 0;
    }

  Ft8A7Slot const& slot = global_a7_history ().slots[static_cast<size_t> (jseq)];
  return std::max (0, std::min (slot.previous_count, kFt8MaxEarly));
}

extern "C" int ftx_ft8_a7_get_previous_entry_c (int jseq, int index, float* dt_out,
                                                 float* freq_out, char* msg37_out)
{
  if (jseq < 0 || jseq >= kFt8SequenceCount || index <= 0 || !dt_out || !freq_out || !msg37_out)
    {
      return 0;
    }

  Ft8A7Slot const& slot = global_a7_history ().slots[static_cast<size_t> (jseq)];
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

  save_a7_entry (global_a7_history ().slots, jseq, dt, freq,
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
                             apsym_array, aph10_array, candidate_values, sbase,
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
  std::array<float, 3 * kFt8MaxCand> candidate {};
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
      int pass_newdat = 1;
      float syncmin = 0.0f;
      int imetric = 0;
      int lsubtract = 0;
      int run_pass = 0;
      ftx_ft8_prepare_pass_c (request.ndepth, ipass, ndecodes,
                              &syncmin, &imetric, &lsubtract, &run_pass);
      if (run_pass == 0)
        {
          continue;
        }

      int ncand = 0;
      ftx_sync8_search_c (state.dd.data (), kFt8NMax,
                          static_cast<float> (ifa), static_cast<float> (ifb),
                          syncmin, static_cast<float> (request.nfqso),
                          kFt8MaxCand, candidate.data (), &ncand, sbase.data ());

      for (int icand = 0; icand < ncand; ++icand)
        {
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
                                     candidate.data () + icand * 3, sbase.data (),
                                     kFt8Nh1, sync, f1, xdt, xbase, nharderrors,
                                     dmin, nbadcrc, candidate_pass, iaptype,
                                     msg37, xsnr, itone, message77);

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
              return;
            }
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
      for (int i = 0; i < slot.previous_count && i < kFt8MaxEarly; ++i)
        {
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

  float f1 = static_cast<float> (request.nfqso);
  float xdt = 0.0f;
  float fbest = 0.0f;
  float xsnr = 0.0f;
  float plog = 0.0f;
  FixedChars<kFt8DecodedChars> msg37 = blank_fixed<kFt8DecodedChars> ();
  ftx_ft8_a8d_c (state.dd.data (), request.mycall.data (), request.hiscall.data (),
                 request.hisgrid.data (), &f1, &xdt, &fbest, &xsnr, &plog, msg37.data ());

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

  for (int imsg = 1; imsg <= kFt8A7MaxMsg; ++imsg)
    {
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
        }
    }

  float dmin2 = 1.0e30f;
  int const accept = ftx_ft8_a7_finalize_metrics_c (dmm.data (), kFt8A7MaxMsg, pbest, *xbase,
                                                     dmin, &dmin2, xsnr);
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
      std::copy_n (mycall, std::min (mycall_len, mycall_copy.size ()), mycall_copy.data ());
    }
  if (dxcall)
    {
      std::copy_n (dxcall, std::min (dxcall_len, dxcall_copy.size ()), dxcall_copy.data ());
    }
  if (dxgrid)
    {
      std::copy_n (dxgrid, std::min (dxgrid_len, dxgrid_copy.size ()), dxgrid_copy.data ());
    }

  ftx_ft8_a8d_c (dd, mycall_copy.data (), dxcall_copy.data (), dxgrid_copy.data (),
                 f1a, xdt, fbest, xsnr, plog, msg_copy.data ());

  if (msgbest)
    {
      size_t const copy_size = std::min (msgbest_len, msg_copy.size ());
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
      std::copy_n (call_1, std::min (call_1_len, call1_copy.size ()), call1_copy.data ());
    }
  if (call_2)
    {
      std::copy_n (call_2, std::min (call_2_len, call2_copy.size ()), call2_copy.data ());
    }
  if (grid4)
    {
      std::copy_n (grid4, std::min (grid4_len, grid4_copy.size ()), grid4_copy.data ());
    }

  ftx_ft8_a7d_c (dd0, newdat, call1_copy.data (), call2_copy.data (), grid4_copy.data (),
                 xdt, f1, xbase, nharderrors, dmin, msg_copy.data (), xsnr);

  if (msg37)
    {
      size_t const copy_size = std::min (msg37_len, msg_copy.size ());
      std::fill_n (msg37, msg37_len, ' ');
      std::copy_n (msg_copy.data (), copy_size, msg37);
    }
}

extern "C" void ftx_ft8_stage4_reset_c ()
{
  stage4_state ().reset ();
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

  if (request.ndepth == 1 && request.nzhsym == 50)
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
      run_ap_passes (state, request, jseq, std::array<float, kFt8Nh1> {}, collector);
      return;
    }

  if (stage_action == 3)
    {
      state.lsubtracted.fill (false);
      ftx_ft8_apply_saved_subtractions_c (state.dd.data (), state.itone_save.data (), kFt8Nn,
                                          state.ndec_early, state.f1_save.data (),
                                          state.xdt_save.data (), state.lsubtracted.data (),
                                          1, refine != 0 ? 1 : 0);
      state.dd1 = state.dd;
      run_ap_passes (state, request, jseq, std::array<float, kFt8Nh1> {}, collector);
      return;
    }

  if (stage_action == 4)
    {
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
  run_main_passes (state, request, jseq, apsym, aph10, ifa, ifb, ndecodes, sbase, collector);

  state.ndec_early = 0;
  if (request.nzhsym < 50)
    {
      state.ndec_early = ndecodes;
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
          int yymmdd = params->yymmdd;
          int nutc = params->nutc;
          int nfqso = params->nfqso;
          int ntol = params->ntol;
          sfrx_sub_ (&yymmdd, &nutc, &nfqso, &ntol, const_cast<short*> (iwave));
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
    default:
      break;
    }
}
