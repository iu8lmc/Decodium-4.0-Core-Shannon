#include "wsjtx_config.h"
#include "commons.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{

constexpr int kFt4Bits {77};
constexpr int kFt4K {91};
constexpr int kFt4Nd {87};
constexpr int kFt4Ns {16};
constexpr int kFt4Nn {kFt4Ns + kFt4Nd};
constexpr int kFt4Nsps {576};
constexpr int kFt4Nmax {21 * 3456};
constexpr int kFt4Nfft1 {2304};
constexpr int kFt4Nh1 {kFt4Nfft1 / 2};
constexpr int kFt4Ndown {18};
constexpr int kFt4Nss {kFt4Nsps / kFt4Ndown};
constexpr int kFt4NdMax {kFt4Nmax / kFt4Ndown};
constexpr int kFt4Rows {2 * kFt4Nn};
constexpr int kFt4MaxCand {260};
constexpr int kFt4MaxLines {100};
constexpr int kFt4DecodedChars {37};
constexpr float kFt4FsDown {12000.0f / kFt4Ndown};
constexpr float kFt4HarvestGridMinHz {225.0f};
constexpr float kFt4HarvestGridMaxHz {3000.0f};
constexpr float kFt4HarvestGridStepHz {25.0f};
constexpr float kFt4HarvestGridFineOffsetHz {12.5f};
constexpr float kFt4LdpcFallbackSyncMin {2.15f};
constexpr float kFt4SyncThresholdDefault {0.90f};
constexpr float kFt4SyncThresholdDeep {0.75f};
constexpr float kFt4SnrFloor {-26.0f};
constexpr float kFt4DecodeScale {2.83f};
constexpr float kFt4SamplesPerSecond {666.67f};
constexpr float kFt4LdpcNorder4FallbackSyncMin {1.80f};
constexpr float kFt4LdpcNorder4FallbackStrongSyncMin {2.15f};
constexpr float kFt4LdpcNorder4FallbackMinCandidateSnr {2.5f};
constexpr float kFt4LdpcNorder4FallbackMaxosd3SyncMin {1.75f};
constexpr float kFt4LdpcNorder4FallbackMaxosd3MinCandidateSnr {2.5f};
constexpr float kFt4LdpcNorder4FallbackStrongSyncMaxosd3SyncMin {2.35f};
constexpr float kFt4LdpcNorder4FallbackStrongSyncMaxosd3MinCandidateSnr {0.0f};
constexpr float kFt4LdpcNorder4FallbackGridMaxosd3SyncMin {1.70f};
constexpr float kFt4LdpcNorder4FallbackGridMinCandidateSnr {-0.10f};
constexpr float kFt4LdpcNorder4FallbackGridMaxCandidateSnr {0.25f};
constexpr float kFt4LdpcNorder4FallbackGridEdgeLowHz {360.0f};
constexpr float kFt4LdpcNorder4FallbackGridEdgeHighHz {5000.0f};
constexpr float kFt4LdpcNorder4FallbackLowAudioMinHz {500.0f};
constexpr float kFt4LdpcNorder4FallbackLowAudioMaxHz {700.0f};
constexpr float kFt4LdpcNorder4FallbackLowAudioSyncMin {1.60f};
constexpr int kFt4ExtraLdpcFallbackBudgetMs {1400};
constexpr int kFt4ExtraLdpcFallbackFastBudgetMs {2400};
constexpr int kFt4ExtraLdpcFallbackDeadlineMs {5600};
constexpr int kFt4ExtraLdpcFallbackFastDeadlineMs {6600};

using Complex = std::complex<float>;

extern "C"
{
  void ftx_getcandidates4_c (float const* dd, float fa, float fb, float syncmin, float nfqso,
                             int maxcand, float* savg, float* candidate, int* ncand,
                             float* sbase);
  void ftx_sync4d_c (Complex const* cd0, int np, int i0, Complex const* ctwk, int itwk,
                     float* sync);
  void ftx_ft4_bitmetrics_c (Complex const* cd, float* bitmetrics, int* badsync);
  void ftx_ft4_downsample_c (float const* dd, int* newdata, float f0, Complex* c);
  void ftx_subtract_ft4_c (float* dd0, int const* itone, float f0, float dt);
  void ftx_twkfreq1_c (Complex const* ca, int const* npts, float const* fsample,
                       float const* a, Complex* cb);
  void decode174_91_ (float* llr_in, int* Keff, int* maxosd, int* norder,
                      signed char* apmask_in, signed char* message91_out, signed char* cw_out,
                      int* ntype_out, int* nharderrors_out, float* dmin_out);
  void legacy_pack77_reset_context_c ();
  void legacy_pack77_set_context_c (char const mycall[13], char const dxcall[13]);
  void legacy_pack77_pack_c (char const msg0[37], int* i3, int* n3,
                             char c77[77], char msgsent[38], bool* success, int received);
  int legacy_pack77_unpack77bits_c (signed char const* message77, int received,
                                    char msg37[38], int* i4);
  int ftx_encode174_91_message77_c (signed char const* message77, signed char* codeword_out);
  int ftx_ft2_message77_to_itone_c (signed char const* message77, int* itone_out);
}

template <typename T>
inline T& bm_at (T* bitmetrics, int row, int col)
{
  return bitmetrics[row + kFt4Rows * col];
}

std::string trim_right_spaces (std::string value)
{
  while (!value.empty () && (value.back () == ' ' || value.back () == '\0'))
    {
      value.pop_back ();
    }
  return value;
}

std::string trim_spaces (std::string value)
{
  value = trim_right_spaces (std::move (value));
  while (!value.empty () && value.front () == ' ')
    {
      value.erase (value.begin ());
    }
  return value;
}

std::string fixed_latin1 (std::string const& text, int width)
{
  std::string out = text.substr (0, static_cast<size_t> (width));
  if (static_cast<int> (out.size ()) < width)
    {
      out.append (static_cast<size_t> (width - static_cast<int> (out.size ())), ' ');
    }
  return out;
}

bool is_grid4 (std::string const& word)
{
  return word.size () == 4
         && word[0] >= 'A' && word[0] <= 'R'
         && word[1] >= 'A' && word[1] <= 'R'
         && word[2] >= '0' && word[2] <= '9'
         && word[3] >= '0' && word[3] <= '9';
}

bool is_report_token (std::string const& word)
{
  if (word.empty ())
    {
      return false;
    }
  size_t pos = 0;
  if (word[pos] == 'R')
    {
      ++pos;
    }
  if (pos < word.size () && (word[pos] == '+' || word[pos] == '-'))
    {
      ++pos;
    }
  size_t const digitStart = pos;
  while (pos < word.size () && word[pos] >= '0' && word[pos] <= '9')
    {
      ++pos;
    }
  return pos == word.size () && pos > digitStart && pos - digitStart <= 2;
}

std::vector<std::string> split_words (std::string const& text)
{
  std::vector<std::string> words;
  std::istringstream stream {text};
  std::string word;
  while (stream >> word)
    {
      words.push_back (word);
    }
  return words;
}

bool is_rr73_token (std::string const& word)
{
  return word == "RR73" || word == "73" || word == "RRR";
}

bool looks_like_pack77_hash_call_seed (std::string const& call)
{
  if (call.size () < 3 || call.size () > 13 || call == "...")
    {
      return false;
    }
  if (call == "CQ" || call == "DE" || call == "QRZ" || call == "RRR"
      || call == "RR73" || call == "73" || call == "TU"
      || call == "FT4" || call == "FT8" || call == "FT2")
    {
      return false;
    }
  if (is_grid4 (call) || is_report_token (call))
    {
      return false;
    }

  bool hasLetter = false;
  bool hasDigit = false;
  for (char const ch : call)
    {
      if (ch >= 'A' && ch <= 'Z')
        {
          hasLetter = true;
          continue;
        }
      if (ch >= '0' && ch <= '9')
        {
          hasDigit = true;
          continue;
        }
      if (ch == '/')
        {
          continue;
        }
      return false;
    }
  return hasLetter && hasDigit;
}

std::string normalize_resolved_hash_call_tokens (std::string const& decoded)
{
  std::string const trimmed = trim_right_spaces (decoded);
  std::string normalized;
  normalized.reserve (trimmed.size ());
  for (size_t i = 0; i < trimmed.size ();)
    {
      if (trimmed[i] == '<')
        {
          size_t const end = trimmed.find ('>', i + 1);
          if (end != std::string::npos)
            {
              std::string const inner = trim_spaces (trimmed.substr (i + 1, end - i - 1));
              if (looks_like_pack77_hash_call_seed (inner))
                {
                  normalized += inner;
                  i = end + 1;
                  continue;
                }
            }
        }
      normalized.push_back (trimmed[i]);
      ++i;
    }
  return fixed_latin1 (normalized, kFt4DecodedChars);
}

std::string normalize_expected_target_text (std::string text)
{
  text = trim_spaces (std::move (text));
  std::string out;
  out.reserve (text.size ());
  bool last_space = false;
  for (unsigned char const ch : text)
    {
      if (std::isspace (ch))
        {
          if (!last_space && !out.empty ())
            {
              out.push_back (' ');
            }
          last_space = true;
          continue;
        }
      out.push_back (static_cast<char> (std::toupper (ch)));
      last_space = false;
    }
  return trim_spaces (out);
}

struct Ft4ExpectedTarget
{
  float freq {};
  float dt {};
  std::string message;
};

std::vector<std::string> split_ascii (std::string const& text, char delimiter)
{
  std::vector<std::string> parts;
  std::string current;
  std::istringstream stream {text};
  while (std::getline (stream, current, delimiter))
    {
      parts.push_back (trim_spaces (current));
    }
  return parts;
}

std::vector<Ft4ExpectedTarget> parse_ft4_expected_targets ()
{
  std::vector<Ft4ExpectedTarget> targets;
  char const* const raw = std::getenv ("DECODIUM_FT4_EXPECTED_TARGETS");
  if (!raw || !*raw)
    {
      return targets;
    }

  for (std::string const& entry : split_ascii (raw, ';'))
    {
      std::vector<std::string> parts = split_ascii (entry, '|');
      if (parts.size () < 3)
        {
          continue;
        }

      size_t offset = 0;
      if (parts.size () >= 4)
        {
          offset = 1;
        }
      Ft4ExpectedTarget target;
      target.freq = static_cast<float> (std::atof (parts[offset].c_str ()));
      target.dt = static_cast<float> (std::atof (parts[offset + 1].c_str ()));
      target.message = normalize_expected_target_text (parts[offset + 2]);
      for (size_t i = offset + 3; i < parts.size (); ++i)
        {
          target.message += '|';
          target.message += normalize_expected_target_text (parts[i]);
        }
      if (target.freq > 0.0f && !target.message.empty ())
        {
          targets.push_back (target);
        }
    }
  return targets;
}

std::vector<Ft4ExpectedTarget> const& ft4_expected_targets ()
{
  static std::vector<Ft4ExpectedTarget> const targets = parse_ft4_expected_targets ();
  return targets;
}

float ft4_expected_freq_window ()
{
  static float const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_EXPECTED_FREQ_WINDOW");
    if (!raw) return 12.0f;
    float const parsed = static_cast<float> (std::atof (raw));
    return parsed > 0.0f ? parsed : 12.0f;
  }();
  return value;
}

float ft4_expected_dt_window ()
{
  static float const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_EXPECTED_DT_WINDOW");
    if (!raw) return 1.2f;
    float const parsed = static_cast<float> (std::atof (raw));
    return parsed > 0.0f ? parsed : 1.2f;
  }();
  return value;
}

float ft4_candidate_syncmin_scale ()
{
  static float const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_CANDIDATE_SYNCMIN_SCALE");
    if (!raw) return 1.0f;
    float const parsed = static_cast<float> (std::atof (raw));
    return (parsed > 0.05f && parsed <= 2.0f) ? parsed : 1.0f;
  }();
  return value;
}

bool ft4_harvest_grid_disabled ()
{
  static bool const disabled = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_DISABLE_HARVEST_GRID");
    return raw && std::atoi (raw) != 0;
  }();
  return disabled;
}

unsigned ft4_hardware_threads ()
{
  static unsigned const threads = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_HARDWARE_THREADS_OVERRIDE");
    if (raw)
      {
        int const parsed = std::atoi (raw);
        if (parsed > 0)
          {
            return static_cast<unsigned> (parsed);
          }
      }
    return std::thread::hardware_concurrency ();
  }();
  return threads;
}

bool ft4_harvest_grid_enabled ()
{
  static bool const enabled = [] {
    if (ft4_harvest_grid_disabled ())
      {
        return false;
      }
    char const* const force = std::getenv ("DECODIUM_FT4_FORCE_HARVEST_GRID");
    if (force && std::atoi (force) != 0)
      {
        return true;
      }
    return ft4_hardware_threads () >= 2;
  }();
  return enabled;
}

bool ft4_late_fine_grid_enabled ()
{
  static bool const enabled = [] {
    char const* const disabled = std::getenv ("DECODIUM_FT4_DISABLE_LATE_FINE_GRID");
    if (disabled && std::atoi (disabled) != 0)
      {
        return false;
      }
    char const* const force = std::getenv ("DECODIUM_FT4_FORCE_LATE_FINE_GRID");
    if (force && std::atoi (force) != 0)
      {
        return true;
      }
    return ft4_hardware_threads () >= 4;
  }();
  return enabled;
}

bool ft4_wide_late_dt_enabled ()
{
  static bool const enabled = [] {
    char const* const disabled = std::getenv ("DECODIUM_FT4_DISABLE_WIDE_LATE_DT");
    if (disabled && std::atoi (disabled) != 0)
      {
        return false;
      }
    char const* const force = std::getenv ("DECODIUM_FT4_FORCE_WIDE_LATE_DT");
    if (force && std::atoi (force) != 0)
      {
        return true;
      }
    return ft4_hardware_threads () >= 4;
  }();
  return enabled;
}

bool ft4_ldpc_norder3_fallback_enabled ()
{
  static bool const enabled = [] {
    char const* const disabled = std::getenv ("DECODIUM_FT4_DISABLE_LDPC_NORDER3_FALLBACK");
    if (disabled && std::atoi (disabled) != 0)
      {
        return false;
      }
    char const* const force = std::getenv ("DECODIUM_FT4_FORCE_LDPC_NORDER3_FALLBACK");
    if (force && std::atoi (force) != 0)
      {
        return true;
      }
    return ft4_hardware_threads () >= 8;
  }();
  return enabled;
}

bool ft4_ldpc_norder4_fallback_enabled ()
{
  static bool const enabled = [] {
    char const* const disabled = std::getenv ("DECODIUM_FT4_DISABLE_LDPC_NORDER4_FALLBACK");
    if (disabled && std::atoi (disabled) != 0)
      {
        return false;
      }
    char const* const force = std::getenv ("DECODIUM_FT4_FORCE_LDPC_NORDER4_FALLBACK");
    if (force && std::atoi (force) != 0)
      {
        return true;
      }
    return ft4_hardware_threads () >= 8;
  }();
  return enabled;
}

int ft4_extra_ldpc_fallback_budget_ms ()
{
  static int const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_EXTRA_LDPC_BUDGET_MS");
    if (raw)
      {
        int const parsed = std::atoi (raw);
        if (parsed >= 0 && parsed <= 7000)
          {
            return parsed;
          }
      }
    return ft4_hardware_threads () >= 16
        ? kFt4ExtraLdpcFallbackFastBudgetMs
        : kFt4ExtraLdpcFallbackBudgetMs;
  }();
  return value;
}

int ft4_extra_ldpc_fallback_deadline_ms ()
{
  static int const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_EXTRA_LDPC_DEADLINE_MS");
    if (raw)
      {
        int const parsed = std::atoi (raw);
        if (parsed >= 0 && parsed <= 9000)
          {
            return parsed;
          }
      }
    return ft4_hardware_threads () >= 16
        ? kFt4ExtraLdpcFallbackFastDeadlineMs
        : kFt4ExtraLdpcFallbackDeadlineMs;
  }();
  return value;
}

int ft4_ldpc_maxosd_override ()
{
  static int const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_LDPC_MAXOSD");
    if (!raw)
      {
        return -999;
      }
    int const parsed = std::atoi (raw);
    return (parsed >= -1 && parsed <= 3) ? parsed : -999;
  }();
  return value;
}

int ft4_ldpc_norder_override ()
{
  static int const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_LDPC_NORDER");
    if (!raw)
      {
        return -999;
      }
    int const parsed = std::atoi (raw);
    return (parsed >= 0 && parsed <= 4) ? parsed : -999;
  }();
  return value;
}

float ft4_subtract_min_qual ()
{
  static float const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_SUBTRACT_MIN_QUAL");
    if (!raw)
      {
        return 0.0f;
      }
    float const parsed = static_cast<float> (std::atof (raw));
    return (parsed >= 0.0f && parsed <= 1.0f) ? parsed : 0.0f;
  }();
  return value;
}

float ft4_ldpc_fallback_sync_min ()
{
  static float const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_LDPC_FALLBACK_SYNC_MIN");
    if (!raw)
      {
        return kFt4LdpcFallbackSyncMin;
      }
    float const parsed = static_cast<float> (std::atof (raw));
    return (parsed >= 0.0f && parsed <= 5.0f) ? parsed : kFt4LdpcFallbackSyncMin;
  }();
  return value;
}

float ft4_ldpc_norder4_fallback_sync_min ()
{
  static float const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_LDPC_NORDER4_FALLBACK_SYNC_MIN");
    if (!raw)
      {
        return kFt4LdpcNorder4FallbackSyncMin;
      }
    float const parsed = static_cast<float> (std::atof (raw));
    return (parsed >= 0.0f && parsed <= 5.0f) ? parsed : kFt4LdpcNorder4FallbackSyncMin;
  }();
  return value;
}

float ft4_ldpc_norder4_fallback_strong_sync_min ()
{
  static float const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_LDPC_NORDER4_FALLBACK_STRONG_SYNC_MIN");
    if (!raw)
      {
        return kFt4LdpcNorder4FallbackStrongSyncMin;
      }
    float const parsed = static_cast<float> (std::atof (raw));
    return (parsed >= 0.0f && parsed <= 5.0f) ? parsed : kFt4LdpcNorder4FallbackStrongSyncMin;
  }();
  return value;
}

float ft4_ldpc_norder4_fallback_min_candidate_snr ()
{
  static float const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_LDPC_NORDER4_FALLBACK_MIN_CAND_SNR");
    if (!raw)
      {
        return kFt4LdpcNorder4FallbackMinCandidateSnr;
      }
    float const parsed = static_cast<float> (std::atof (raw));
    return (parsed >= -5.0f && parsed <= 20.0f) ? parsed : kFt4LdpcNorder4FallbackMinCandidateSnr;
  }();
  return value;
}

float ft4_ldpc_norder4_fallback_maxosd3_sync_min ()
{
  static float const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_LDPC_NORDER4_MAXOSD3_SYNC_MIN");
    if (!raw)
      {
        return kFt4LdpcNorder4FallbackMaxosd3SyncMin;
      }
    float const parsed = static_cast<float> (std::atof (raw));
    return (parsed >= 0.0f && parsed <= 5.0f) ? parsed : kFt4LdpcNorder4FallbackMaxosd3SyncMin;
  }();
  return value;
}

float ft4_ldpc_norder4_fallback_maxosd3_min_candidate_snr ()
{
  static float const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_LDPC_NORDER4_MAXOSD3_MIN_CAND_SNR");
    if (!raw)
      {
        return kFt4LdpcNorder4FallbackMaxosd3MinCandidateSnr;
      }
    float const parsed = static_cast<float> (std::atof (raw));
    return (parsed >= -5.0f && parsed <= 20.0f) ? parsed : kFt4LdpcNorder4FallbackMaxosd3MinCandidateSnr;
  }();
  return value;
}

float ft4_ldpc_norder4_fallback_strong_sync_maxosd3_sync_min ()
{
  static float const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_LDPC_NORDER4_STRONG_SYNC_MAXOSD3_SYNC_MIN");
    if (!raw)
      {
        return kFt4LdpcNorder4FallbackStrongSyncMaxosd3SyncMin;
      }
    float const parsed = static_cast<float> (std::atof (raw));
    return (parsed >= 0.0f && parsed <= 5.0f) ? parsed : kFt4LdpcNorder4FallbackStrongSyncMaxosd3SyncMin;
  }();
  return value;
}

float ft4_ldpc_norder4_fallback_strong_sync_maxosd3_min_candidate_snr ()
{
  static float const value = [] {
    char const* const raw = std::getenv ("DECODIUM_FT4_LDPC_NORDER4_STRONG_SYNC_MAXOSD3_MIN_CAND_SNR");
    if (!raw)
      {
        return kFt4LdpcNorder4FallbackStrongSyncMaxosd3MinCandidateSnr;
      }
    float const parsed = static_cast<float> (std::atof (raw));
    return (parsed >= -5.0f && parsed <= 20.0f)
        ? parsed
        : kFt4LdpcNorder4FallbackStrongSyncMaxosd3MinCandidateSnr;
  }();
  return value;
}

bool ft4_ldpc_norder4_fallback_strong_sync_candidate (float candidate_snr, float sync)
{
  return candidate_snr >= ft4_ldpc_norder4_fallback_strong_sync_maxosd3_min_candidate_snr ()
         && sync >= ft4_ldpc_norder4_fallback_strong_sync_maxosd3_sync_min ();
}

bool ft4_ldpc_norder4_grid_edge_fallback_enabled ()
{
  static bool const enabled = [] {
    char const* const disabled = std::getenv ("DECODIUM_FT4_DISABLE_GRID_EDGE_FALLBACK");
    if (disabled && std::atoi (disabled) != 0)
      {
        return false;
      }
    char const* const force = std::getenv ("DECODIUM_FT4_FORCE_GRID_EDGE_FALLBACK");
    if (force && std::atoi (force) != 0)
      {
        return true;
      }
    return ft4_hardware_threads () >= 16;
  }();
  return enabled;
}

bool ft4_ldpc_norder4_fallback_grid_candidate (float candidate_snr, float sync, float freq)
{
  if (!ft4_ldpc_norder4_grid_edge_fallback_enabled ())
    {
      return false;
    }
  bool const edge_candidate = freq <= kFt4LdpcNorder4FallbackGridEdgeLowHz
                              || freq >= kFt4LdpcNorder4FallbackGridEdgeHighHz;
  return candidate_snr >= kFt4LdpcNorder4FallbackGridMinCandidateSnr
         && candidate_snr <= kFt4LdpcNorder4FallbackGridMaxCandidateSnr
         && sync >= kFt4LdpcNorder4FallbackGridMaxosd3SyncMin
         && edge_candidate;
}

bool ft4_ldpc_norder4_low_audio_retry_candidate (float candidate_snr, float sync, float freq)
{
  return ft4_hardware_threads () >= 8
         && freq >= kFt4LdpcNorder4FallbackLowAudioMinHz
         && freq <= kFt4LdpcNorder4FallbackLowAudioMaxHz
         && candidate_snr >= kFt4LdpcNorder4FallbackGridMinCandidateSnr
         && candidate_snr <= kFt4LdpcNorder4FallbackGridMaxCandidateSnr
         && sync >= kFt4LdpcNorder4FallbackLowAudioSyncMin;
}

bool ft4_ldpc_norder4_fallback_candidate_allowed (float candidate_snr, float sync, int maxosd,
                                                  float freq)
{
  if (candidate_snr >= ft4_ldpc_norder4_fallback_min_candidate_snr ())
    {
      return true;
    }
  if (maxosd == 2
      && (ft4_ldpc_norder4_fallback_strong_sync_candidate (candidate_snr, sync)
          || ft4_ldpc_norder4_fallback_grid_candidate (candidate_snr, sync, freq)))
    {
      return true;
    }
  return maxosd >= 3 && sync >= ft4_ldpc_norder4_fallback_strong_sync_min ();
}

int ft4_ldpc_norder4_fallback_maxosd (int maxosd, float candidate_snr, float sync, float freq)
{
  if (maxosd != 2)
    {
      return maxosd;
    }
  if ((candidate_snr >= ft4_ldpc_norder4_fallback_maxosd3_min_candidate_snr ()
      && sync >= ft4_ldpc_norder4_fallback_maxosd3_sync_min ())
      || ft4_ldpc_norder4_fallback_strong_sync_candidate (candidate_snr, sync)
      || ft4_ldpc_norder4_fallback_grid_candidate (candidate_snr, sync, freq))
    {
      return 3;
    }
  return maxosd;
}

void append_ft4_harvest_grid_candidates (int depth, int nfa, int nfb, bool include_fine_grid,
                                         float* candidate, int* ncand, int maxcand)
{
  if (!candidate || !ncand || *ncand < 0)
    {
      return;
    }
  // The exhaustive 25 Hz grid is a depth-4 feature. Enabling it at depth 3
  // makes the nominal live pass exceed FT4's 7.5 s slot on otherwise healthy
  // 8-thread systems. The bridge only selects depth 4 on machines with enough
  // execution headroom, while depth 3 retains its lower sync threshold and OSD.
  if (!ft4_harvest_grid_enabled () || depth < 4)
    {
      return;
    }

  float const first = std::max (kFt4HarvestGridMinHz, static_cast<float> (nfa));
  float const last = std::min (kFt4HarvestGridMaxHz, static_cast<float> (nfb));
  if (first > last)
    {
      return;
    }

  auto append_frequency = [&] (float frequency) {
    bool exists = false;
    for (int i = 0; i < *ncand; ++i)
      {
        if (std::fabs (candidate[static_cast<size_t> (2 * i)] - frequency) <= 1.0f)
          {
            exists = true;
            break;
          }
      }
    if (exists)
      {
        return;
      }
    candidate[static_cast<size_t> (2 * *ncand)] = frequency;
    candidate[static_cast<size_t> (2 * *ncand + 1)] = 1.0f;
    ++(*ncand);
  };

  for (float frequency = first; frequency <= last && *ncand < maxcand;
       frequency += kFt4HarvestGridStepHz)
    {
      append_frequency (frequency);
    }

  if (!include_fine_grid || !ft4_late_fine_grid_enabled ())
    {
      return;
    }

  for (float frequency = first + kFt4HarvestGridFineOffsetHz;
       frequency <= last && *ncand < maxcand;
       frequency += kFt4HarvestGridStepHz)
    {
      append_frequency (frequency);
    }
}

std::mutex& ft4_expected_target_trace_mutex ()
{
  static std::mutex mutex;
  return mutex;
}

template<typename Writer>
void trace_ft4_expected_target (Ft4ExpectedTarget const& target, char const* stage,
                                Writer writer)
{
  std::lock_guard<std::mutex> lock {ft4_expected_target_trace_mutex ()};
  std::cerr << "[FT4TARGET] stage=" << stage
            << " target_freq=" << target.freq
            << " target_dt=" << target.dt
            << " target_msg=\"" << target.message << '"';
  writer (std::cerr);
  std::cerr << '\n';
}

Ft4ExpectedTarget const* find_ft4_expected_target_by_freq (float freq)
{
  for (Ft4ExpectedTarget const& target : ft4_expected_targets ())
    {
      if (std::fabs (freq - target.freq) <= ft4_expected_freq_window ())
        {
          return &target;
        }
    }
  return nullptr;
}

Ft4ExpectedTarget const* find_ft4_expected_target (float freq, float candidate_dt)
{
  for (Ft4ExpectedTarget const& target : ft4_expected_targets ())
    {
      if (std::fabs (freq - target.freq) <= ft4_expected_freq_window ()
          && std::fabs (candidate_dt - target.dt) <= ft4_expected_dt_window ())
        {
          return &target;
        }
    }
  return nullptr;
}

bool ft4_expected_message_matches (Ft4ExpectedTarget const& target,
                                   std::string const& decoded)
{
  return normalize_expected_target_text (decoded) == target.message;
}

void trace_ft4_expected_candidate_list (int isp, float syncmin, float const* candidate,
                                        int ncand)
{
  if (!candidate || ft4_expected_targets ().empty ())
    {
      return;
    }
  for (Ft4ExpectedTarget const& target : ft4_expected_targets ())
    {
      int in_window = 0;
      int best_index = -1;
      float best_df = 99999.0f;
      float best_freq = 0.0f;
      float best_strength = 0.0f;
      for (int i = 0; i < ncand; ++i)
        {
          float const freq = candidate[static_cast<size_t> (2 * i)];
          float const df = std::fabs (freq - target.freq);
          if (df <= ft4_expected_freq_window ())
            {
              ++in_window;
            }
          if (df < best_df)
            {
              best_df = df;
              best_index = i;
              best_freq = freq;
              best_strength = candidate[static_cast<size_t> (2 * i + 1)];
            }
        }
      trace_ft4_expected_target (target, "candidate-list", [&] (std::ostream& out) {
        out << " isp=" << isp
            << " syncmin=" << syncmin
            << " ncand=" << ncand
            << " in_window=" << in_window
            << " best_index=" << best_index;
        if (best_index >= 0)
          {
            out << " best_freq=" << best_freq
                << " best_df=" << (best_freq - target.freq)
                << " best_strength=" << best_strength;
          }
      });
    }
}

std::string fixed_from_chars (char const* data, int width)
{
  return std::string (data, static_cast<size_t> (width));
}

std::string trim_block (char const* data, size_t width)
{
  if (!data)
    {
      return {};
    }
  return trim_right_spaces (std::string (data, width));
}

void fill_fixed_chars (char* dest, int width, std::string const& source)
{
  std::fill_n (dest, width, ' ');
  int const count = std::min (static_cast<int>(width), static_cast<int>(static_cast<int> (source.size ())));
  if (count > 0)
    {
      std::memcpy (dest, source.data (), static_cast<size_t> (count));
    }
}

void fill_c_string_13 (char dest[13], std::string const& text)
{
  std::fill_n (dest, 13, '\0');
  int const count = std::min (static_cast<int>(12), static_cast<int>(static_cast<int> (text.size ())));
  if (count > 0)
    {
      std::memcpy (dest, text.data (), static_cast<size_t> (count));
    }
}

std::array<signed char, kFt4Bits> bits_from_c77 (char const c77[77])
{
  std::array<signed char, kFt4Bits> bits {};
  for (int i = 0; i < kFt4Bits; ++i)
    {
      bits[static_cast<size_t> (i)] = (c77[i] == '1') ? 1 : 0;
    }
  return bits;
}

constexpr std::array<int, kFt4Bits> kFt4Rvec {{
  0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0,
  1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1,
  0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1
}};

constexpr std::array<int, 29> kMcq {{
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0
}};

constexpr std::array<int, 29> kMcqRu {{
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,1,1,0,0,1,1,0,0
}};

constexpr std::array<int, 29> kMcqFd {{
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,0,1,0,0,0,1,0
}};

constexpr std::array<int, 29> kMcqTest {{
  0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,0,1,0,1,1,1,1,1,1,0,0,1,0
}};

constexpr std::array<int, 29> kMcqWw {{
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,1,1,1,1,0
}};

constexpr std::array<int, 6> kNapPasses {{2, 2, 2, 2, 2, 3}};

constexpr std::array<std::array<int, 4>, 6> kNapTypes {{
  std::array<int, 4> {{1, 2, 0, 0}},
  std::array<int, 4> {{2, 3, 0, 0}},
  std::array<int, 4> {{2, 3, 0, 0}},
  std::array<int, 4> {{3, 6, 0, 0}},
  std::array<int, 4> {{3, 6, 0, 0}},
  std::array<int, 4> {{3, 1, 2, 0}}
}};

struct Ft4State
{
  bool tweak_ready {false};
  std::array<std::array<Complex, 2 * kFt4Nss>, 33> ctwk2 {};
};

struct Ft4ApSetup
{
  std::array<int, 2 * kFt4Nd> apbits {};
  std::array<int, 28> apmy_ru {};
  std::array<int, 28> aphis_fd {};
};

struct Ft4DecodeLine
{
  float sync {0.0f};
  int snr {0};
  float dt {0.0f};
  float freq {0.0f};
  int nap {0};
  float qual {0.0f};
  std::array<signed char, kFt4Bits> bits {};
  std::string decoded {std::string (kFt4DecodedChars, ' ')};
};

Ft4State& ft4_state ()
{
  static Ft4State state;
  return state;
}

int tweak_index (int idf)
{
  return idf + 16;
}

void ensure_tweaks (Ft4State& state)
{
  if (state.tweak_ready)
    {
      return;
    }

  std::array<Complex, 2 * kFt4Nss> ones {};
  std::fill (ones.begin (), ones.end (), Complex {1.0f, 0.0f});
  int const npts = static_cast<int> (ones.size ());
  float const fsample = kFt4FsDown / 2.0f;

  for (int idf = -16; idf <= 16; ++idf)
    {
      std::array<float, 5> a {};
      a[0] = static_cast<float> (idf);
      ftx_twkfreq1_c (ones.data (), &npts, &fsample, a.data (),
                      state.ctwk2[static_cast<size_t> (tweak_index (idf))].data ());
    }

  state.tweak_ready = true;
}

std::array<int, 29> scramble_partial (std::array<int, 29> const& source, int offset)
{
  std::array<int, 29> out {};
  for (int i = 0; i < 29; ++i)
    {
      out[static_cast<size_t> (i)] = 2 * ((source[static_cast<size_t> (i)]
                                           + kFt4Rvec[static_cast<size_t> (offset + i)]) & 1) - 1;
    }
  return out;
}

std::array<signed char, kFt4Bits> scramble_bits (std::array<signed char, kFt4Bits> const& bits)
{
  std::array<signed char, kFt4Bits> out {};
  for (int i = 0; i < kFt4Bits; ++i)
    {
      out[static_cast<size_t> (i)] =
          static_cast<signed char> ((bits[static_cast<size_t> (i)] + kFt4Rvec[static_cast<size_t> (i)]) & 1);
    }
  return out;
}

bool pack_message77_with_context (std::string const& message,
                                  std::array<signed char, kFt4Bits>* bits_out,
                                  std::string* canonical_out,
                                  int* i3_out, int* n3_out)
{
  char msg0[kFt4DecodedChars];
  char c77[kFt4Bits];
  char msgsent[kFt4DecodedChars + 1];
  int i3 = -1;
  int n3 = -1;
  bool success = false;

  fill_fixed_chars (msg0, kFt4DecodedChars, fixed_latin1 (message, kFt4DecodedChars));
  std::fill_n (c77, kFt4Bits, '0');
  std::fill_n (msgsent, kFt4DecodedChars + 1, ' ');
  msgsent[kFt4DecodedChars] = '\0';

  legacy_pack77_pack_c (msg0, &i3, &n3, c77, msgsent, &success, 0);
  if (!success)
    {
      return false;
    }

  if (bits_out)
    {
      *bits_out = bits_from_c77 (c77);
    }
  if (canonical_out)
    {
      *canonical_out = fixed_from_chars (msgsent, kFt4DecodedChars);
    }
  if (i3_out)
    {
      *i3_out = i3;
    }
  if (n3_out)
    {
      *n3_out = n3;
    }
  return true;
}

bool unpack_message77_with_context (std::array<signed char, kFt4Bits> const& bits,
                                    std::string* message_out)
{
  char msgsent[kFt4DecodedChars + 1];
  int quirky = 0;
  std::fill_n (msgsent, kFt4DecodedChars + 1, ' ');
  msgsent[kFt4DecodedChars] = '\0';
  if (legacy_pack77_unpack77bits_c (bits.data (), 1, msgsent, &quirky) == 0)
    {
      return false;
    }
  if (message_out)
    {
      *message_out = normalize_resolved_hash_call_tokens (
          fixed_from_chars (msgsent, kFt4DecodedChars));
    }
  return true;
}

bool encode_codeword77 (std::array<signed char, kFt4Bits> const& bits,
                        std::array<signed char, 2 * kFt4Nd>* codeword_out)
{
  if (!codeword_out)
    {
      return false;
    }
  if (ftx_encode174_91_message77_c (bits.data (), codeword_out->data ()) == 0)
    {
      codeword_out->fill (0);
      return false;
    }
  return true;
}

bool message77_to_ft4_tones (std::array<signed char, kFt4Bits> const& bits,
                             std::array<int, kFt4Nn>* tones_out)
{
  if (!tones_out)
    {
      return false;
    }
  if (ftx_ft2_message77_to_itone_c (bits.data (), tones_out->data ()) == 0)
    {
      tones_out->fill (0);
      return false;
    }
  return true;
}

Ft4ApSetup build_ap_setup (std::string const& mycall, std::string const& hiscall)
{
  Ft4ApSetup setup;
  setup.apbits.fill (0);
  setup.apbits[0] = 99;
  setup.apbits[29] = 99;
  setup.apmy_ru.fill (0);
  setup.aphis_fd.fill (0);

  std::string const my = trim_right_spaces (mycall);
  if (my.size () < 3)
    {
      return setup;
    }

  std::string his = trim_right_spaces (hiscall);
  bool const no_hiscall = his.size () < 3;
  if (no_hiscall)
    {
      his = my;
    }

  std::string const message = my + " " + his + " RR73";
  std::array<signed char, kFt4Bits> bits {};
  std::string canonical;
  int i3 = -1;
  int n3 = -1;
  if (!pack_message77_with_context (message, &bits, &canonical, &i3, &n3))
    {
      return setup;
    }
  if (i3 != 1 || canonical != fixed_latin1 (message, kFt4DecodedChars))
    {
      return setup;
    }

  for (int i = 0; i < 28; ++i)
    {
      setup.apmy_ru[static_cast<size_t> (i)] =
          2 * ((bits[static_cast<size_t> (i)] + kFt4Rvec[static_cast<size_t> (i + 1)]) & 1) - 1;
      setup.aphis_fd[static_cast<size_t> (i)] =
          2 * ((bits[static_cast<size_t> (i + 29)] + kFt4Rvec[static_cast<size_t> (i + 28)]) & 1) - 1;
    }

  auto const scrambled = scramble_bits (bits);
  std::array<signed char, 2 * kFt4Nd> codeword {};
  if (!encode_codeword77 (scrambled, &codeword))
    {
      return setup;
    }
  for (int i = 0; i < 2 * kFt4Nd; ++i)
    {
      setup.apbits[static_cast<size_t> (i)] = 2 * static_cast<int> (codeword[static_cast<size_t> (i)]) - 1;
    }
  if (no_hiscall)
    {
      setup.apbits[29] = 99;
    }
  return setup;
}

std::array<int, 29> contest_mcq (int ncontest)
{
  switch (ncontest)
    {
    case 1:
    case 2:
      return scramble_partial (kMcqTest, 0);
    case 3:
      return scramble_partial (kMcqFd, 0);
    case 4:
      return scramble_partial (kMcqRu, 0);
    case 5:
      return scramble_partial (kMcqWw, 0);
    default:
      return scramble_partial (kMcq, 0);
    }
}

void clear_outputs (float* syncs, int* snrs, float* dts, float* freqs, int* naps, float* quals,
                    signed char* bits77, char* decodeds, int* nout)
{
  if (syncs) std::fill_n (syncs, kFt4MaxLines, 0.0f);
  if (snrs) std::fill_n (snrs, kFt4MaxLines, 0);
  if (dts) std::fill_n (dts, kFt4MaxLines, 0.0f);
  if (freqs) std::fill_n (freqs, kFt4MaxLines, 0.0f);
  if (naps) std::fill_n (naps, kFt4MaxLines, 0);
  if (quals) std::fill_n (quals, kFt4MaxLines, 0.0f);
  if (bits77) std::fill_n (bits77, kFt4MaxLines * kFt4Bits, static_cast<signed char> (0));
  if (decodeds) std::fill_n (decodeds, kFt4MaxLines * kFt4DecodedChars, ' ');
  if (nout) *nout = 0;
}

void store_line (int index, Ft4DecodeLine const& line, float* syncs, int* snrs, float* dts,
                 float* freqs, int* naps, float* quals, signed char* bits77, char* decodeds)
{
  if (syncs) syncs[index] = line.sync;
  if (snrs) snrs[index] = line.snr;
  if (dts) dts[index] = line.dt;
  if (freqs) freqs[index] = line.freq;
  if (naps) naps[index] = line.nap;
  if (quals) quals[index] = line.qual;
  if (bits77)
    {
      std::copy_n (line.bits.begin (), kFt4Bits, bits77 + index * kFt4Bits);
    }
  if (decodeds)
    {
      fill_fixed_chars (decodeds + index * kFt4DecodedChars, kFt4DecodedChars, line.decoded);
    }
}

void copy_llr_column (float const* bitmetrics, int column, std::array<float, 2 * kFt4Nd>& llr)
{
  for (int i = 0; i < 58; ++i)
    {
      llr[static_cast<size_t> (i)] = bitmetrics[(8 + i) + kFt4Rows * column];
      llr[static_cast<size_t> (58 + i)] = bitmetrics[(74 + i) + kFt4Rows * column];
      llr[static_cast<size_t> (116 + i)] = bitmetrics[(140 + i) + kFt4Rows * column];
    }
  for (float& value : llr)
    {
      value *= kFt4DecodeScale;
    }
}

std::string fixed_to_string (char const* data, size_t len)
{
  return trim_right_spaces (std::string (data, len));
}

std::string format_ft4_decoded_text (std::string const& decoded, int nap, float qual,
                                     std::string* annot_out)
{
  std::string decoded_copy = decoded;
  if (decoded_copy.size () < static_cast<size_t> (kFt4DecodedChars))
    {
      decoded_copy.append (static_cast<size_t> (kFt4DecodedChars) - decoded_copy.size (), ' ');
    }
  else if (decoded_copy.size () > static_cast<size_t> (kFt4DecodedChars))
    {
      decoded_copy.resize (static_cast<size_t> (kFt4DecodedChars));
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

  if (annot_out)
    {
      *annot_out = annot;
    }
  return decoded_copy;
}

std::string format_ft4_stdout_line (int nutc, int snr, float dt, float freq,
                                    std::string const& decoded, int nap, float qual)
{
  std::string annot;
  std::string const decoded_copy = format_ft4_decoded_text (decoded, nap, qual, &annot);

  std::ostringstream line;
  line << std::setfill ('0') << std::setw (6) << nutc
       << std::setfill (' ') << std::setw (4) << snr
       << std::fixed << std::setprecision (1) << std::setw (5) << dt
       << std::setw (5) << static_cast<int> (std::lround (freq))
       << " + " << ' ' << decoded_copy << ' ' << annot;
  return line.str ();
}

std::string format_ft4_decoded_file_line (int nutc, float sync, int snr, float dt, float freq,
                                          std::string const& decoded, int nap, float qual)
{
  std::string const decoded_copy = format_ft4_decoded_text (decoded, nap, qual, nullptr);

  std::ostringstream line;
  line << std::setfill ('0') << std::setw (6) << nutc
       << std::setfill (' ') << std::setw (4) << static_cast<int> (std::lround (sync))
       << std::setw (5) << snr
       << std::fixed << std::setprecision (1) << std::setw (6) << dt
       << std::fixed << std::setprecision (0) << std::setw (8) << freq
       << std::setw (4) << 0
       << "   " << decoded_copy << " FT4";
  return line.str ();
}

bool is_duplicate (std::vector<std::string> const& seen, std::string const& decoded)
{
  return std::find (seen.begin (), seen.end (), decoded) != seen.end ();
}

std::vector<Ft4DecodeLine> expand_ft4_compound_line (Ft4DecodeLine const& line)
{
  std::string const decoded = normalize_expected_target_text (line.decoded);
  size_t const semicolon = decoded.find (';');
  if (semicolon == std::string::npos || decoded.find (';', semicolon + 1) != std::string::npos)
    {
      return {};
    }

  std::vector<std::string> const left = split_words (decoded.substr (0, semicolon));
  std::vector<std::string> const right = split_words (decoded.substr (semicolon + 1));
  if (left.size () != 2 || right.size () < 3)
    {
      return {};
    }
  if (!looks_like_pack77_hash_call_seed (left[0])
      || !is_rr73_token (left[1])
      || !looks_like_pack77_hash_call_seed (right[0])
      || !looks_like_pack77_hash_call_seed (right[1])
      || (!is_report_token (right[2]) && !is_rr73_token (right[2])))
    {
      return {};
    }

  Ft4DecodeLine first = line;
  Ft4DecodeLine second = line;
  first.decoded = fixed_latin1 (left[0] + " " + right[1] + " " + left[1] + " 1",
                                kFt4DecodedChars);
  second.decoded = fixed_latin1 (right[0] + " " + right[1] + " " + right[2] + " 1",
                                 kFt4DecodedChars);
  return {first, second};
}

bool append_line (Ft4DecodeLine const& line, std::vector<Ft4DecodeLine>& lines,
                  std::vector<std::string>& seen)
{
  std::vector<Ft4DecodeLine> const expanded = expand_ft4_compound_line (line);
  if (!expanded.empty ())
    {
      bool appended = false;
      for (Ft4DecodeLine const& expanded_line : expanded)
        {
          appended = append_line (expanded_line, lines, seen) || appended;
        }
      return appended;
    }
  if (is_duplicate (seen, line.decoded))
    {
      return false;
    }
  if (static_cast<int> (lines.size ()) >= kFt4MaxLines)
    {
      return false;
    }
  lines.push_back (line);
  seen.push_back (line.decoded);
  return true;
}

void run_ft4_decode (short const* iwave,
                     int nqsoprogress, int nfqso, int nfa, int nfb,
                     int ndepth, bool lapcqonly, int ncontest,
                     std::string const& mycall, std::string const& hiscall,
                     std::vector<Ft4DecodeLine>& output)
{
  output.clear ();
  if (!iwave)
    {
      return;
    }

  Ft4State& state = ft4_state ();
  ensure_tweaks (state);
  auto const decode_started_at = std::chrono::steady_clock::now ();
  int extra_ldpc_elapsed_ms = 0;
  int const extra_ldpc_budget_ms = ft4_extra_ldpc_fallback_budget_ms ();
  int const extra_ldpc_deadline_ms = ft4_extra_ldpc_fallback_deadline_ms ();
  auto decode_elapsed_ms = [&] {
    return static_cast<int> (std::chrono::duration_cast<std::chrono::milliseconds> (
        std::chrono::steady_clock::now () - decode_started_at).count ());
  };
  auto extra_ldpc_budget_available = [&] {
    return extra_ldpc_budget_ms > 0
           && extra_ldpc_elapsed_ms < extra_ldpc_budget_ms
           && extra_ldpc_deadline_ms > 0
           && decode_elapsed_ms () < extra_ldpc_deadline_ms;
  };

  char mycall_c[13];
  char hiscall_c[13];
  fill_c_string_13 (mycall_c, mycall);
  fill_c_string_13 (hiscall_c, hiscall);
  // Keep the pack77 hash table alive across FT4 candidates/slots. Each hash is
  // inserted directly by the worker, so replaying the complete cache here is
  // redundant and can block the shared runtime for seconds on Windows.
  legacy_pack77_set_context_c (mycall_c, hiscall_c);

  Ft4ApSetup const ap_setup = build_ap_setup (mycall, hiscall);
  std::array<int, 29> const mcq = contest_mcq (ncontest);

  std::array<float, kFt4Nmax> dd {};
  for (int i = 0; i < kFt4Nmax; ++i)
    {
      dd[static_cast<size_t> (i)] = static_cast<float> (iwave[i]);
    }

  int const qso_progress = std::max (0, std::min (5, nqsoprogress));
  int const depth = std::max (1, std::min (4, ndepth));

  float syncmin = 1.18f;
  if (depth >= 3)
    {
      syncmin = 1.0f;
    }

  bool dosubtract = true;
  bool doosd = true;
  int nsp = 4;
  if (depth == 2)
    {
      nsp = 3;
      doosd = false;
    }
  else if (depth <= 1)
    {
      nsp = 1;
      dosubtract = false;
      doosd = false;
    }

  int ndecodes = 0;
  int nd1 = 0;
  int nd2 = 0;
  std::vector<std::string> seen;
  seen.reserve (kFt4MaxLines);

  std::array<float, kFt4Nh1> savg {};
  std::array<float, kFt4Nh1> sbase {};
  std::array<float, 2 * kFt4MaxCand> candidate {};
  std::array<Complex, kFt4NdMax> cd2 {};
  std::array<Complex, kFt4NdMax> cb {};
  std::array<Complex, kFt4Nn * kFt4Nss> cd {};
  std::array<float, kFt4Rows * 3> bitmetrics {};

  for (int isp = 1; isp <= nsp; ++isp)
    {
      if (isp == 2)
        {
          if (ndecodes == 0)
            {
              break;
            }
          nd1 = ndecodes;
        }
      else if (isp == 3)
        {
          nd2 = ndecodes - nd1;
          if (nd2 == 0
              && !(depth >= 4 && ft4_late_fine_grid_enabled ()
                   && ft4_hardware_threads () >= 8))
            {
              break;
            }
        }

	      float syncmin_pass = syncmin;
	      if (isp >= 2) syncmin_pass *= 0.88f;
	      if (isp >= 3) syncmin_pass *= 0.76f;
	      syncmin_pass *= ft4_candidate_syncmin_scale ();

      candidate.fill (0.0f);
      int ncand = 0;
      ftx_getcandidates4_c (dd.data (), static_cast<float> (nfa), static_cast<float> (nfb),
                            syncmin_pass, static_cast<float> (nfqso), kFt4MaxCand,
                            savg.data (), candidate.data (), &ncand, sbase.data ());
      append_ft4_harvest_grid_candidates (depth, nfa, nfb, isp == nsp,
                                          candidate.data (), &ncand, kFt4MaxCand);
      trace_ft4_expected_candidate_list (isp, syncmin_pass, candidate.data (), ncand);
      bool dobigfft = true;

      for (int icand = 0; icand < ncand; ++icand)
        {
          float const f0 = candidate[static_cast<size_t> (2 * icand)];
          float const snr = candidate[static_cast<size_t> (2 * icand + 1)] - 1.0f;

          int newdata = dobigfft ? 1 : 0;
          ftx_ft4_downsample_c (dd.data (), &newdata, f0, cd2.data ());
          dobigfft = (newdata != 0);
          if (dobigfft)
            {
              dobigfft = false;
            }

          float sum2 = 0.0f;
          for (Complex const& sample : cd2)
            {
              sum2 += std::norm (sample);
            }
          sum2 /= (static_cast<float> (kFt4Nmax) / static_cast<float> (kFt4Ndown));
          if (sum2 > 0.0f)
            {
              float const scale = 1.0f / std::sqrt (sum2);
              for (Complex& sample : cd2)
                {
                  sample *= scale;
                }
            }

          float smax1 = -99.0f;
          bool decoded_candidate = false;
          for (int iseg = 1; iseg <= 3 && !decoded_candidate; ++iseg)
            {
              float smax = -99.0f;
              int ibest = -1;
              int idfbest = 0;

              for (int isync = 1; isync <= 2; ++isync)
                {
                  int idfmin = 0;
                  int idfmax = 0;
                  int idfstp = 1;
                  int ibmin = 0;
                  int ibmax = 0;
                  int ibstp = 1;

                  if (isync == 1)
                    {
                      idfmin = -12;
                      idfmax = 12;
                      idfstp = 3;
                      ibmin = -344;
                      ibmax = 1012;
                      if (iseg == 1)
                        {
                          ibmin = 108;
                          ibmax = 560;
                        }
                      else if (iseg == 2)
                        {
                          ibmin = 560;
                          ibmax = 1012;
                          if (ft4_wide_late_dt_enabled ())
                            {
                              ibmax = 1120;
                            }
                        }
                      else
                        {
                          ibmin = -344;
                          ibmax = 108;
                        }
                      ibstp = 4;
                    }
                  else
                    {
                      idfmin = idfbest - 4;
                      idfmax = idfbest + 4;
                      idfstp = 1;
                      ibmin = ibest - 5;
                      ibmax = ibest + 5;
                      ibstp = 1;
                    }

                  ibest = -1;
                  idfbest = 0;
                  smax = -99.0f;
                  for (int idf = idfmin; idf <= idfmax; idf += idfstp)
                    {
                      for (int istart = ibmin; istart <= ibmax; istart += ibstp)
                        {
                          float sync = 0.0f;
                          ftx_sync4d_c (cd2.data (), kFt4NdMax, istart,
                                        state.ctwk2[static_cast<size_t> (tweak_index (idf))].data (),
                                        1, &sync);
                          if (sync > smax)
                            {
                              smax = sync;
                              ibest = istart;
                              idfbest = idf;
                            }
                        }
                    }
                }

	              if (iseg == 1)
	                {
	                  smax1 = smax;
	                }

	              float const f1 = f0 + static_cast<float> (idfbest);
	              float const callback_dt = static_cast<float> (ibest) / kFt4SamplesPerSecond - 0.5f;
	              Ft4ExpectedTarget const* trace_target = find_ft4_expected_target (f1, callback_dt);
	              if (!trace_target)
	                {
	                  trace_target = find_ft4_expected_target_by_freq (f1);
	                }
	              if (trace_target)
	                {
	                  trace_ft4_expected_target (*trace_target, "sync-eval",
	                                             [&] (std::ostream& out) {
	                    out << " isp=" << isp
	                        << " icand=" << icand
	                        << " iseg=" << iseg
	                        << " f0=" << f0
	                        << " f1=" << f1
	                        << " dt=" << callback_dt
	                        << " smax=" << smax;
	                  });
	                }

	              float smaxthresh = (depth >= 3) ? kFt4SyncThresholdDeep : kFt4SyncThresholdDefault;
	              if (isp >= 2) smaxthresh *= 0.88f;
	              if (isp >= 3) smaxthresh *= 0.76f;
	              if (smax < smaxthresh)
	                {
	                  if (trace_target)
	                    {
	                      trace_ft4_expected_target (*trace_target, "sync-reject",
	                                                 [&] (std::ostream& out) {
	                        out << " isp=" << isp
	                            << " icand=" << icand
	                            << " iseg=" << iseg
	                            << " f1=" << f1
	                            << " dt=" << callback_dt
	                            << " smax=" << smax
	                            << " threshold=" << smaxthresh;
	                      });
	                    }
	                  continue;
	                }
	              if (iseg > 1 && smax < smax1)
	                {
	                  if (trace_target)
	                    {
	                      trace_ft4_expected_target (*trace_target, "segment-reject",
	                                                 [&] (std::ostream& out) {
	                        out << " isp=" << isp
	                            << " icand=" << icand
	                            << " iseg=" << iseg
	                            << " f1=" << f1
	                            << " dt=" << callback_dt
	                            << " smax=" << smax
	                            << " smax1=" << smax1;
	                      });
	                    }
	                  continue;
	                }

	              if (f1 <= 10.0f || f1 >= 4990.0f)
	                {
	                  if (trace_target)
	                    {
	                      trace_ft4_expected_target (*trace_target, "freq-reject",
	                                                 [&] (std::ostream& out) {
	                        out << " isp=" << isp
	                            << " icand=" << icand
	                            << " iseg=" << iseg
	                            << " f1=" << f1
	                            << " dt=" << callback_dt;
	                      });
	                    }
	                  continue;
	                }

              int newdata_final = dobigfft ? 1 : 0;
              ftx_ft4_downsample_c (dd.data (), &newdata_final, f1, cb.data ());
              dobigfft = (newdata_final != 0);

              float sum_cb = 0.0f;
              for (Complex const& sample : cb)
                {
                  sum_cb += std::norm (sample);
                }
              sum_cb /= static_cast<float> (kFt4Nss * kFt4Nn);
              if (sum_cb > 0.0f)
                {
                  float const scale = 1.0f / std::sqrt (sum_cb);
                  for (Complex& sample : cb)
                    {
                      sample *= scale;
                    }
                }

              cd.fill (Complex {});
              if (ibest >= 0)
                {
                  int const it = std::min (kFt4NdMax - 1, ibest + kFt4Nn * kFt4Nss - 1);
                  int const np = it - ibest + 1;
                  if (np > 0)
                    {
                      std::copy_n (cb.begin () + ibest, np, cd.begin ());
                    }
                }
              else
                {
                  int const start_dest = -ibest;
                  int const count = kFt4Nn * kFt4Nss + 2 * ibest;
                  if (count > 0 && start_dest < static_cast<int> (cd.size ()))
                    {
                      std::copy_n (cb.begin (), std::min (static_cast<int>(count), static_cast<int>(static_cast<int> (cd.size ())) - start_dest),
                                   cd.begin () + start_dest);
                    }
                }

              int badsync = 0;
	              bitmetrics.fill (0.0f);
	              ftx_ft4_bitmetrics_c (cd.data (), bitmetrics.data (), &badsync);
	              if (badsync != 0)
	                {
	                  if (trace_target)
	                    {
	                      trace_ft4_expected_target (*trace_target, "bitmetrics-reject",
	                                                 [&] (std::ostream& out) {
	                        out << " isp=" << isp
	                            << " icand=" << icand
	                            << " iseg=" << iseg
	                            << " f1=" << f1
	                            << " dt=" << callback_dt
	                            << " badsync=" << badsync;
	                      });
	                    }
	                  continue;
	                }

              std::array<int, kFt4Rows> hbits {};
              for (int row = 0; row < kFt4Rows; ++row)
                {
                  hbits[static_cast<size_t> (row)] = bm_at (bitmetrics.data (), row, 0) >= 0.0f ? 1 : 0;
                }
              int ns1 = 0;
              int ns2 = 0;
              int ns3 = 0;
              int ns4 = 0;
              int const expect1[8] = {0,0,0,1,1,0,1,1};
              int const expect2[8] = {0,1,0,0,1,1,1,0};
              int const expect3[8] = {1,1,1,0,0,1,0,0};
              int const expect4[8] = {1,0,1,1,0,0,0,1};
              for (int i = 0; i < 8; ++i)
                {
                  ns1 += (hbits[static_cast<size_t> (i)] == expect1[i]) ? 1 : 0;
                  ns2 += (hbits[static_cast<size_t> (66 + i)] == expect2[i]) ? 1 : 0;
                  ns3 += (hbits[static_cast<size_t> (132 + i)] == expect3[i]) ? 1 : 0;
                  ns4 += (hbits[static_cast<size_t> (198 + i)] == expect4[i]) ? 1 : 0;
                }
	              int const nsync_qual = ns1 + ns2 + ns3 + ns4;
	              if (nsync_qual < 16)
	                {
	                  if (trace_target)
	                    {
	                      trace_ft4_expected_target (*trace_target, "nsync-reject",
	                                                 [&] (std::ostream& out) {
	                        out << " isp=" << isp
	                            << " icand=" << icand
	                            << " iseg=" << iseg
	                            << " f1=" << f1
	                            << " dt=" << callback_dt
	                            << " nsync=" << nsync_qual;
	                      });
	                    }
	                  continue;
	                }

              std::array<float, 2 * kFt4Nd> llra {};
              std::array<float, 2 * kFt4Nd> llrb {};
              std::array<float, 2 * kFt4Nd> llrc {};
              std::array<float, 2 * kFt4Nd> llrd {};
              std::array<float, 2 * kFt4Nd> llr {};
              copy_llr_column (bitmetrics.data (), 0, llra);
              copy_llr_column (bitmetrics.data (), 1, llrb);
              copy_llr_column (bitmetrics.data (), 2, llrc);

              float apmag = 0.0f;
              for (float value : llra)
                {
                  apmag = std::max (apmag, std::fabs (value));
                }
              apmag *= 1.1f;

              int npasses = 3 + kNapPasses[static_cast<size_t> (qso_progress)];
              if (lapcqonly) npasses = 4;
              if (depth == 1) npasses = 3;
              if (ncontest >= 6) npasses = 3;

              int nharderror = -1;
              for (int ipass = 1; ipass <= npasses; ++ipass)
                {
                  std::array<signed char, 2 * kFt4Nd> apmask {};
                  int iaptype = 0;
                  if (ipass == 1) llr = llra;
                  if (ipass == 2) llr = llrb;
                  if (ipass == 3) llr = llrc;

                  if (ipass > 3)
                    {
                      llrd = llrc;
                      iaptype = kNapTypes[static_cast<size_t> (qso_progress)][static_cast<size_t> (ipass - 4)];
                      if (lapcqonly)
                        {
                          iaptype = 1;
                        }

                      int const napwid = 50;
                      if (ncontest <= 5 && iaptype >= 3 && (std::fabs (f1 - static_cast<float> (nfqso)) > napwid))
                        {
                          continue;
                        }
                      if (iaptype >= 2 && ap_setup.apbits[0] > 1)
                        {
                          continue;
                        }
                      if (iaptype >= 3 && ap_setup.apbits[29] > 1)
                        {
                          continue;
                        }

                      if (iaptype == 1)
                        {
                          for (int i = 0; i < 29; ++i)
                            {
                              apmask[static_cast<size_t> (i)] = 1;
                              llrd[static_cast<size_t> (i)] = apmag * static_cast<float> (mcq[static_cast<size_t> (i)]);
                            }
                        }
                      else if (iaptype == 2)
                        {
                          if (ncontest == 0 || ncontest == 1 || ncontest == 5)
                            {
                              for (int i = 0; i < 29; ++i)
                                {
                                  apmask[static_cast<size_t> (i)] = 1;
                                  llrd[static_cast<size_t> (i)] = apmag * static_cast<float> (ap_setup.apbits[static_cast<size_t> (i)]);
                                }
                            }
                          else if (ncontest == 2 || ncontest == 3)
                            {
                              for (int i = 0; i < 28; ++i)
                                {
                                  apmask[static_cast<size_t> (i)] = 1;
                                  llrd[static_cast<size_t> (i)] = apmag * static_cast<float> (ap_setup.apbits[static_cast<size_t> (i)]);
                                }
                            }
                          else if (ncontest == 4)
                            {
                              for (int i = 0; i < 28; ++i)
                                {
                                  apmask[static_cast<size_t> (1 + i)] = 1;
                                  llrd[static_cast<size_t> (1 + i)] = apmag * static_cast<float> (ap_setup.apmy_ru[static_cast<size_t> (i)]);
                                }
                            }
                        }
                      else if (iaptype == 3)
                        {
                          if (ncontest == 0 || ncontest == 1 || ncontest == 2 || ncontest == 5)
                            {
                              for (int i = 0; i < 58; ++i)
                                {
                                  apmask[static_cast<size_t> (i)] = 1;
                                  llrd[static_cast<size_t> (i)] = apmag * static_cast<float> (ap_setup.apbits[static_cast<size_t> (i)]);
                                }
                            }
                          else if (ncontest == 3)
                            {
                              for (int i = 0; i < 28; ++i)
                                {
                                  apmask[static_cast<size_t> (i)] = 1;
                                  llrd[static_cast<size_t> (i)] = apmag * static_cast<float> (ap_setup.apbits[static_cast<size_t> (i)]);
                                  apmask[static_cast<size_t> (28 + i)] = 1;
                                  llrd[static_cast<size_t> (28 + i)] =
                                      apmag * static_cast<float> (ap_setup.aphis_fd[static_cast<size_t> (i)]);
                                }
                            }
                          else if (ncontest == 4)
                            {
                              for (int i = 0; i < 28; ++i)
                                {
                                  apmask[static_cast<size_t> (1 + i)] = 1;
                                  llrd[static_cast<size_t> (1 + i)] =
                                      apmag * static_cast<float> (ap_setup.apmy_ru[static_cast<size_t> (i)]);
                                  apmask[static_cast<size_t> (29 + i)] = 1;
                                  llrd[static_cast<size_t> (29 + i)] =
                                      apmag * static_cast<float> (ap_setup.apbits[static_cast<size_t> (29 + i)]);
                                }
                            }
                        }
                      else if (iaptype == 4 || iaptype == 5 || iaptype == 6)
                        {
                          if (ncontest <= 5)
                            {
                              for (int i = 0; i < kFt4Bits; ++i)
                                {
                                  apmask[static_cast<size_t> (i)] = 1;
                                  if (iaptype == 6)
                                    {
                                      llrd[static_cast<size_t> (i)] =
                                          apmag * static_cast<float> (ap_setup.apbits[static_cast<size_t> (i)]);
                                    }
                                }
                            }
                        }

                      llr = llrd;
                    }

                  std::array<signed char, kFt4K> message91 {};
                  std::array<signed char, 2 * kFt4Nd> cw {};
                  float dmin = 0.0f;
                  int Keff = 91;
                  int ndeep = 2;
                  int maxosd = (std::fabs (static_cast<float> (nfqso) - f1) <= 50.0f) ? 3 : 2;
                  if (!doosd)
                    {
                      maxosd = -1;
                    }
                  int const maxosd_override = ft4_ldpc_maxosd_override ();
                  if (maxosd_override != -999)
                    {
                      maxosd = maxosd_override;
                    }
                  int const norder_override = ft4_ldpc_norder_override ();
                  if (norder_override != -999)
                    {
                      ndeep = norder_override;
                    }
                  int ntype = 0;
                  decode174_91_ (llr.data (), &Keff, &maxosd, &ndeep, apmask.data (),
                                 message91.data (), cw.data (), &ntype, &nharderror, &dmin);
                  if (trace_target)
                    {
                      trace_ft4_expected_target (*trace_target, "ldpc-result",
                                                 [&] (std::ostream& out) {
                        out << " isp=" << isp
                            << " icand=" << icand
                            << " iseg=" << iseg
                            << " ipass=" << ipass
                            << " iaptype=" << iaptype
                            << " f1=" << f1
                            << " dt=" << callback_dt
                            << " ntype=" << ntype
                            << " nharderror=" << nharderror
                            << " dmin=" << dmin
                            << " maxosd=" << maxosd
                            << " norder=" << ndeep;
                      });
                    }

                  if (ntype == 0
                      && norder_override == -999
                      && ft4_ldpc_norder3_fallback_enabled ()
                      && depth >= 3
                      && isp >= 2
                      && iaptype == 0
                      && ipass <= 3
                      && ndeep < 3
                      && maxosd >= 0
                      && smax >= ft4_ldpc_fallback_sync_min ())
                    {
                      std::array<signed char, kFt4K> fallback_message91 {};
                      std::array<signed char, 2 * kFt4Nd> fallback_cw {};
                      float fallback_dmin = 0.0f;
                      int fallback_Keff = Keff;
                      int fallback_maxosd = maxosd;
                      int fallback_norder = 3;
                      int fallback_ntype = 0;
                      int fallback_nharderror = -1;
                      decode174_91_ (llr.data (), &fallback_Keff, &fallback_maxosd,
                                     &fallback_norder, apmask.data (),
                                     fallback_message91.data (), fallback_cw.data (),
                                     &fallback_ntype, &fallback_nharderror,
                                     &fallback_dmin);
                      if (trace_target)
                        {
                          trace_ft4_expected_target (*trace_target, "ldpc-fallback-result",
                                                     [&] (std::ostream& out) {
                            out << " isp=" << isp
                                << " icand=" << icand
                                << " iseg=" << iseg
                                << " ipass=" << ipass
                                << " iaptype=" << iaptype
                                << " f1=" << f1
                                << " dt=" << callback_dt
                                << " ntype=" << fallback_ntype
                                << " nharderror=" << fallback_nharderror
                                << " dmin=" << fallback_dmin
                                << " maxosd=" << fallback_maxosd
                                << " norder=" << fallback_norder;
                          });
                        }
                      if (fallback_ntype != 0 || fallback_nharderror >= 0)
                        {
                          message91 = fallback_message91;
                          cw = fallback_cw;
                          ntype = fallback_ntype;
                          nharderror = fallback_nharderror;
                          dmin = fallback_dmin;
                          Keff = fallback_Keff;
                          maxosd = fallback_maxosd;
                          ndeep = fallback_norder;
                        }
                    }

                  if (ntype == 0
                      && norder_override == -999
                      && depth >= 4
                      && iaptype == 0
                      && ipass <= 3
                      && maxosd >= 0
                      && extra_ldpc_deadline_ms > 0
                      && decode_elapsed_ms () < extra_ldpc_deadline_ms
                      && ft4_ldpc_norder4_low_audio_retry_candidate (snr, smax, f1))
                    {
                      std::array<signed char, kFt4K> fallback_message91 {};
                      std::array<signed char, 2 * kFt4Nd> fallback_cw {};
                      float fallback_dmin = 0.0f;
                      int fallback_Keff = Keff;
                      int fallback_maxosd = 3;
                      int fallback_norder = 4;
                      int fallback_ntype = 0;
                      int fallback_nharderror = -1;
                      auto const extra_ldpc_started_at = std::chrono::steady_clock::now ();
                      decode174_91_ (llr.data (), &fallback_Keff, &fallback_maxosd,
                                     &fallback_norder, apmask.data (),
                                     fallback_message91.data (), fallback_cw.data (),
                                     &fallback_ntype, &fallback_nharderror,
                                     &fallback_dmin);
                      extra_ldpc_elapsed_ms += static_cast<int> (
                          std::chrono::duration_cast<std::chrono::milliseconds> (
                              std::chrono::steady_clock::now () - extra_ldpc_started_at).count ());
                      if (trace_target)
                        {
                          trace_ft4_expected_target (*trace_target, "ldpc-low-audio-retry-result",
                                                     [&] (std::ostream& out) {
                            out << " isp=" << isp
                                << " icand=" << icand
                                << " iseg=" << iseg
                                << " ipass=" << ipass
                                << " iaptype=" << iaptype
                                << " f1=" << f1
                                << " dt=" << callback_dt
                                << " ntype=" << fallback_ntype
                                << " nharderror=" << fallback_nharderror
                                << " dmin=" << fallback_dmin
                                << " maxosd=" << fallback_maxosd
                                << " norder=" << fallback_norder;
                          });
                        }
                      if (fallback_ntype != 0 || fallback_nharderror >= 0)
                        {
                          message91 = fallback_message91;
                          cw = fallback_cw;
                          ntype = fallback_ntype;
                          nharderror = fallback_nharderror;
                          dmin = fallback_dmin;
                          Keff = fallback_Keff;
                          maxosd = fallback_maxosd;
                          ndeep = fallback_norder;
                        }
                    }

                  if (ntype == 0
                      && norder_override == -999
                      && ft4_ldpc_norder4_fallback_enabled ()
                      && extra_ldpc_budget_available ()
                      // Some weak FT4 finals decode only with OSD order 4 even
                      // when the UI profile is the normal deep depth-3 path.
                      // Keep this fast-machine only via
                      // ft4_ldpc_norder4_fallback_enabled() and the live budget.
                      && depth >= 3
                      && iaptype == 0
                      && ipass <= 3
                      && maxosd >= 0
                      && ft4_ldpc_norder4_fallback_candidate_allowed (snr, smax, maxosd, f1)
                      && smax >= ft4_ldpc_norder4_fallback_sync_min ())
                    {
                      std::array<signed char, kFt4K> fallback_message91 {};
                      std::array<signed char, 2 * kFt4Nd> fallback_cw {};
                      float fallback_dmin = 0.0f;
                      int fallback_Keff = Keff;
                      int fallback_maxosd = ft4_ldpc_norder4_fallback_maxosd (maxosd, snr, smax, f1);
                      int fallback_norder = 4;
                      int fallback_ntype = 0;
                      int fallback_nharderror = -1;
                      auto const extra_ldpc_started_at = std::chrono::steady_clock::now ();
                      decode174_91_ (llr.data (), &fallback_Keff, &fallback_maxosd,
                                     &fallback_norder, apmask.data (),
                                     fallback_message91.data (), fallback_cw.data (),
                                     &fallback_ntype, &fallback_nharderror,
                                     &fallback_dmin);
                      extra_ldpc_elapsed_ms += static_cast<int> (
                          std::chrono::duration_cast<std::chrono::milliseconds> (
                              std::chrono::steady_clock::now () - extra_ldpc_started_at).count ());
                      if (trace_target)
                        {
                          trace_ft4_expected_target (*trace_target, "ldpc-norder4-fallback-result",
                                                     [&] (std::ostream& out) {
                            out << " isp=" << isp
                                << " icand=" << icand
                                << " iseg=" << iseg
                                << " ipass=" << ipass
                                << " iaptype=" << iaptype
                                << " f1=" << f1
                                << " dt=" << callback_dt
                                << " ntype=" << fallback_ntype
                                << " nharderror=" << fallback_nharderror
                                << " dmin=" << fallback_dmin
                                << " maxosd=" << fallback_maxosd
                                << " norder=" << fallback_norder;
                          });
                        }
                      if (fallback_ntype != 0 || fallback_nharderror >= 0)
                        {
                          message91 = fallback_message91;
                          cw = fallback_cw;
                          ntype = fallback_ntype;
                          nharderror = fallback_nharderror;
                          dmin = fallback_dmin;
                          Keff = fallback_Keff;
                          maxosd = fallback_maxosd;
                          ndeep = fallback_norder;
                        }
                    }

                  std::array<signed char, kFt4Bits> message77 {};
                  std::copy_n (message91.begin (), kFt4Bits, message77.begin ());
                  bool all_zero = true;
                  for (signed char bit : message77)
                    {
                      if (bit != 0)
                        {
                          all_zero = false;
                          break;
                        }
                    }
                  if (all_zero)
                    {
                      if (trace_target)
                        {
                          trace_ft4_expected_target (*trace_target, "ldpc-zero",
                                                     [&] (std::ostream& out) {
                            out << " isp=" << isp
                                << " icand=" << icand
                                << " iseg=" << iseg
                                << " ipass=" << ipass
                                << " iaptype=" << iaptype
                                << " f1=" << f1
                                << " dt=" << callback_dt;
                          });
                        }
                      continue;
                    }

                  if (nharderror >= 0)
                    {
                      for (int i = 0; i < kFt4Bits; ++i)
                        {
                          message77[static_cast<size_t> (i)] =
                              static_cast<signed char> ((message77[static_cast<size_t> (i)]
                                                         + kFt4Rvec[static_cast<size_t> (i)]) & 1);
                        }

                      std::string message_fixed;
                      if (!unpack_message77_with_context (message77, &message_fixed))
                        {
                          if (trace_target)
                            {
                              trace_ft4_expected_target (*trace_target, "unpack-fail",
                                                         [&] (std::ostream& out) {
                                out << " isp=" << isp
                                    << " icand=" << icand
                                    << " iseg=" << iseg
                                    << " ipass=" << ipass
                                    << " iaptype=" << iaptype
                                    << " f1=" << f1
                                    << " dt=" << callback_dt;
                              });
                            }
                          break;
                        }
                      if (trace_target)
                        {
                          trace_ft4_expected_target (*trace_target, "decode-success",
                                                     [&] (std::ostream& out) {
                            out << " isp=" << isp
                                << " icand=" << icand
                                << " iseg=" << iseg
                                << " ipass=" << ipass
                                << " iaptype=" << iaptype
                                << " f1=" << f1
                                << " dt=" << callback_dt
                                << " decoded=\""
                                << normalize_expected_target_text (message_fixed)
                                << "\" expected_match="
                                << (ft4_expected_message_matches (*trace_target, message_fixed) ? 1 : 0);
                          });
                        }

                      Ft4DecodeLine line;
                      line.sync = smax;
                      if (snr > 0.0f)
                        {
                          line.snr = static_cast<int> (std::lround (std::max (kFt4SnrFloor,
                              10.0f * std::log10 (snr) - 14.8f)));
                        }
                      else
                        {
                          line.snr = static_cast<int> (kFt4SnrFloor);
                        }
                      line.dt = static_cast<float> (ibest) / kFt4SamplesPerSecond - 0.5f;
                      line.freq = f1;
                      line.nap = iaptype;
                      line.qual = 1.0f - (static_cast<float> (nharderror) + dmin) / 60.0f;
                      line.bits = message77;
                      line.decoded = message_fixed;

                      bool const accepted = append_line (line, output, seen);
                      if (accepted && dosubtract && line.qual >= ft4_subtract_min_qual ())
                        {
                          std::array<int, kFt4Nn> itone {};
                          if (message77_to_ft4_tones (message77, &itone))
                            {
                              float const dt = static_cast<float> (ibest) / kFt4SamplesPerSecond;
                              ftx_subtract_ft4_c (dd.data (), itone.data (), f1, dt);
                            }
                        }
                      ndecodes = static_cast<int> (output.size ());
                      decoded_candidate = true;
                      break;
                    }
                }
            }
        }
    }
}

void decode_ft4_into_outputs (short const* iwave,
                              int nqsoprogress, int nfqso, int nfa, int nfb,
                              int ndepth, bool lapcqonly, int ncontest,
                              char const* mycall, size_t mycall_len,
                              char const* hiscall, size_t hiscall_len,
                              float* syncs, int* snrs, float* dts, float* freqs,
                              int* naps, float* quals, signed char* bits77,
                              char* decodeds, int* nout)
{
  clear_outputs (syncs, snrs, dts, freqs, naps, quals, bits77, decodeds, nout);
  if (!iwave || !snrs || !dts || !freqs || !naps || !quals || !bits77 || !decodeds || !nout)
    {
      return;
    }

  std::vector<Ft4DecodeLine> rows;
  rows.reserve (kFt4MaxLines);
  run_ft4_decode (iwave, nqsoprogress, nfqso, nfa, nfb, ndepth, lapcqonly, ncontest,
                  fixed_to_string (mycall, mycall_len),
                  fixed_to_string (hiscall, hiscall_len),
                  rows);

  int const count = std::min (static_cast<int> (rows.size ()), kFt4MaxLines);
  for (int i = 0; i < count; ++i)
    {
      store_line (i, rows[static_cast<size_t> (i)], syncs, snrs, dts, freqs, naps, quals,
                  bits77, decodeds);
    }
  *nout = count;
}

void emit_ft4_results (int nutc, int nagain, char const* temp_dir,
                       float const* syncs, int const* snrs, float const* dts,
                       float const* freqs, int const* naps, float const* quals,
                       char const* decodeds, int nout, int* decoded_count)
{
  int const count = std::max (0, std::min (nout, kFt4MaxLines));
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
      std::ios::out | (nagain != 0 ? std::ios::app : std::ios::trunc)
  };

  for (int index = 0; index < count; ++index)
    {
      std::string decoded {decodeds + index * kFt4DecodedChars, static_cast<size_t> (kFt4DecodedChars)};
      std::cout << format_ft4_stdout_line (nutc, snrs[index], dts[index], freqs[index],
                                           decoded, naps[index], quals[index])
                << '\n';

      if (decoded_file.is_open ())
        {
          decoded_file << format_ft4_decoded_file_line (nutc, syncs[index], snrs[index],
                                                        dts[index], freqs[index], decoded,
                                                        naps[index], quals[index])
                       << '\n';
        }
    }

  std::cout.flush ();
  if (decoded_file.is_open ())
    {
      decoded_file.flush ();
    }
}

}

extern "C" void ftx_ft4_decode_c (short const* iwave,
                                   int* nqsoprogress, int* nfqso, int* nfa, int* nfb,
                                   int* ndepth, int* lapcqonly, int* ncontest,
                                   char const* mycall, char const* hiscall,
                                   float* syncs, int* snrs, float* dts, float* freqs,
                                   int* naps, float* quals, signed char* bits77,
                                   char* decodeds, int* nout,
                                   fortran_charlen_t mycall_len,
                                   fortran_charlen_t hiscall_len,
                                   fortran_charlen_t)
{
  if (!nqsoprogress || !nfqso || !nfa || !nfb || !ndepth || !lapcqonly || !ncontest)
    {
      clear_outputs (syncs, snrs, dts, freqs, naps, quals, bits77, decodeds, nout);
      return;
    }

  decode_ft4_into_outputs (iwave, *nqsoprogress, *nfqso, *nfa, *nfb, *ndepth,
                           *lapcqonly != 0, *ncontest, mycall,
                           static_cast<size_t> (mycall_len), hiscall,
                           static_cast<size_t> (hiscall_len),
                           syncs, snrs, dts, freqs, naps, quals, bits77, decodeds, nout);
}

extern "C" void ftx_ft4_decode_and_emit_params_c (short const* iwave,
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

  std::array<float, kFt4MaxLines> syncs {};
  std::array<int, kFt4MaxLines> snrs {};
  std::array<float, kFt4MaxLines> dts {};
  std::array<float, kFt4MaxLines> freqs {};
  std::array<int, kFt4MaxLines> naps {};
  std::array<float, kFt4MaxLines> quals {};
  std::array<signed char, kFt4Bits * kFt4MaxLines> bits77 {};
  std::array<char, kFt4DecodedChars * kFt4MaxLines> decodeds {};
  int nout = 0;

  decode_ft4_into_outputs (iwave,
                           params->nQSOProgress,
                           params->nfqso,
                           params->nfa,
                           params->nfb,
                           params->ndepth,
                           params->lapcqonly,
                           params->nexp_decode & 7,
                           params->mycall,
                           sizeof (params->mycall),
                           params->hiscall,
                           sizeof (params->hiscall),
                           syncs.data (),
                           snrs.data (),
                           dts.data (),
                           freqs.data (),
                           naps.data (),
                           quals.data (),
                           bits77.data (),
                           decodeds.data (),
                           &nout);

  emit_ft4_results (params->nutc,
                    params->nagain ? 1 : 0,
                    temp_dir,
                    syncs.data (),
                    snrs.data (),
                    dts.data (),
                    freqs.data (),
                    naps.data (),
                    quals.data (),
                    decodeds.data (),
                    nout,
                    decoded_count);
}
