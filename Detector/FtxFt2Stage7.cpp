#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <QByteArray>
#include <QRegularExpression>
#include <QString>

#include "commons.h"
#include "helper_functions.h"
#include "Modulator/FtxMessageEncoder.hpp"

namespace
{

using Complex = std::complex<float>;
using fortran_charlen_t = size_t;

constexpr int kFt2MaxLines = 100;
constexpr int kFt2Bits = 77;
constexpr int kFt2Codeword = 174;
constexpr int kFt2Message91 = 91;
constexpr int kFt2DecodedChars = 37;
constexpr int kFt2OutlineChars = 80;
constexpr int kFt2NMax = 45000;
constexpr int kFt2Nd = 87;
constexpr int kFt2Ns = 16;
constexpr int kFt2Nn = kFt2Ns + kFt2Nd;
constexpr int kFt2Rows = 2 * kFt2Nn;
constexpr int kFt2Nsps = 288;
constexpr int kFt2NDown = 9;
constexpr int kFt2Nss = kFt2Nsps / kFt2NDown;
constexpr int kFt2NSeg = kFt2Nn * kFt2Nss;
constexpr int kFt2NdMax = kFt2NMax / kFt2NDown;
constexpr int kFt2MaxCand = 300;
constexpr int kFt2Nh1 = 1152 / 2;
constexpr float kFt2FsDown = 12000.0f / static_cast<float> (kFt2NDown);
// Legacy FT2 uses a rounded 1333.33 samples/s when converting ibest -> dt.
constexpr float kFt2FreqDtScale = 1.0f / 1333.33f;
constexpr int kFt2FoxMax = 1000;

extern "C"
{
  void ftx_getcandidates2_c (float const* dd, float fa, float fb, float syncmin, float nfqso,
                             int maxcand, float* savg, float* candidate, int* ncand,
                             float* sbase);
  void ftx_sync2d_c (Complex const* cd0, int np, int i0, Complex const* ctwk,
                     int itwk, float* sync);
  void ftx_ft2_bitmetrics_c (Complex const* cd, float* bitmetrics, int* badsync);
  void ftx_ft2_bitmetrics_diag_c (Complex const* cd,
                                  float* bitmetrics_final,
                                  float* bitmetrics_base,
                                  float* bmet_eq_raw,
                                  float* bmet_eq,
                                  Complex* cd_eq,
                                  float* ch_snr,
                                  int* badsync,
                                  int* use_cheq,
                                  float* snr_min,
                                  float* snr_max,
                                  float* snr_mean,
                                  float* fading_depth,
                                  float* noise_var,
                                  float* noise_var_eq);
  void ftx_ft2_downsample_c (float const* dd, int* newdata, float f0, Complex* c);
  void ftx_subtract_ft2_c (float* dd, int const* itone, float f0, float dt);
  void ftx_decode174_91_c (float const* llr, int Keff, int maxosd, int norder,
                           signed char const* apmask, signed char* message91, signed char* cw,
                           int* ntype, int* nharderror, float* dmin);
  void ftx_twkfreq1_c (Complex const* ca, int const* npts, float const* fsample,
                       float const* a, Complex* cb);
  int ftx_ft2_message77_to_itone_c (signed char const* message77, int* itone_out);
  int ftx_encode174_91_message77_c (signed char const* message77, signed char* codeword_out);
}

template <typename T>
inline T& bm_at (T* bitmetrics, int row, int col)
{
  return bitmetrics[row + kFt2Rows * col];
}

QByteArray trim_right_spaces (QByteArray value)
{
  while (!value.isEmpty () && (value.back () == ' ' || value.back () == '\0'))
    {
      value.chop (1);
    }
  return value;
}

QByteArray fixed_latin1 (QString const& text, int width)
{
  QByteArray latin = text.left (width).toLatin1 ();
  if (latin.size () < width)
    {
      latin.append (QByteArray (width - latin.size (), ' '));
    }
  return latin;
}

QByteArray fixed_from_chars (char const* data, int width)
{
  return QByteArray (data, width);
}

QString qt_trimmed_string (char const* data, int width)
{
  return QString::fromLatin1 (trim_right_spaces (fixed_from_chars (data, width)));
}

bool is_ft2_ascii_digit (QChar const c)
{
  return c >= QLatin1Char ('0') && c <= QLatin1Char ('9');
}

bool is_ft2_ascii_letter (QChar const c)
{
  return c >= QLatin1Char ('A') && c <= QLatin1Char ('Z');
}

QString normalize_ft2_token (QString const& token)
{
  QString normalized = token.trimmed ().toUpper ();
  if (normalized.startsWith (QLatin1Char ('<')) && normalized.endsWith (QLatin1Char ('>'))
      && normalized.size () >= 3)
    {
      normalized = normalized.mid (1, normalized.size () - 2).trimmed ();
    }
  return normalized;
}

bool is_bracket_hash_token (QString const& token)
{
  QString const normalized = token.trimmed ().toUpper ();
  return normalized.startsWith (QLatin1Char ('<')) && normalized.endsWith (QLatin1Char ('>'))
      && normalized.size () >= 3;
}

bool is_plausible_ft2_call_no_slash (QString const& token)
{
  static QRegularExpression const call_re {
      QStringLiteral (
          "^(?:[A-Z]{1,4}[0-9]{1,4}[A-Z]{1,4}|[0-9]{1,2}[A-Z]{1,4}[0-9]{1,4}[A-Z]{1,4})$")
  };
  QString const normalized = normalize_ft2_token (token);
  return !normalized.isEmpty ()
      && normalized.size () >= 3
      && normalized.size () <= 11
      && call_re.match (normalized).hasMatch ();
}

bool is_short_ft2_affix (QString const& token)
{
  QString const normalized = normalize_ft2_token (token);
  if (normalized.isEmpty () || normalized.size () > 4)
    {
      return false;
    }

  bool has_letter = false;
  for (QChar const c : normalized)
    {
      if (!is_ft2_ascii_letter (c) && !is_ft2_ascii_digit (c))
        {
          return false;
        }
      has_letter = has_letter || is_ft2_ascii_letter (c);
    }
  return has_letter;
}

bool is_plausible_ft2_callsignish (QString const& token)
{
  if (is_bracket_hash_token (token))
    {
      return true;
    }

  QString const normalized = normalize_ft2_token (token);
  if (normalized.isEmpty ())
    {
      return false;
    }

  int const slash = normalized.indexOf (QLatin1Char ('/'));
  if (slash < 0)
    {
      return is_plausible_ft2_call_no_slash (normalized);
    }
  if (slash != normalized.lastIndexOf (QLatin1Char ('/')))
    {
      return false;
    }

  QString const left = normalized.left (slash);
  QString const right = normalized.mid (slash + 1);
  if (left.isEmpty () || right.isEmpty ())
    {
      return false;
    }

  bool const left_call = is_plausible_ft2_call_no_slash (left);
  bool const right_call = is_plausible_ft2_call_no_slash (right);
  if (left_call && right_call)
    {
      return true;
    }
  if (left_call && is_short_ft2_affix (right))
    {
      return true;
    }
  if (right_call && is_short_ft2_affix (left))
    {
      return true;
    }
  return false;
}

bool is_ft2_ack_token (QString const& token)
{
  return token == QStringLiteral ("RRR")
      || token == QStringLiteral ("RR73")
      || token == QStringLiteral ("73");
}

bool looks_like_ft2_free_text_callsign_ghost (QStringList const& words)
{
  static QRegularExpression const alnum_slash_re {
      QStringLiteral ("^[A-Z0-9/]+$")
  };

  int callsignish_tokens = 0;
  int implausible_callsignish_tokens = 0;
  for (QString const& raw_word : words)
    {
      QString const word = normalize_ft2_token (raw_word);
      if (word.isEmpty () || is_ft2_ack_token (word))
        {
          continue;
        }

      if (!alnum_slash_re.match (word).hasMatch ())
        {
          return false;
        }

      if (!word.contains (QRegularExpression {QStringLiteral ("[0-9/]")}))
        {
          return false;
        }

      ++callsignish_tokens;
      if (!is_plausible_ft2_callsignish (word))
        {
          ++implausible_callsignish_tokens;
        }
    }

  return callsignish_tokens >= 2 && implausible_callsignish_tokens > 0;
}

// FT2 type-4 decodes can map arbitrary base38 payloads into callsign-looking
// text. That is useful for genuine long/special calls, but it also lets random
// noise surface as bogus callsigns like "CAYOBTYZCV0". Reject only the clearly
// implausible type-4 forms so standard, contest, and hash-assisted traffic
// continue to pass untouched.
bool is_plausible_ft2_decoded_message (QByteArray const& decoded)
{
  QString const message = QString::fromLatin1 (decoded).trimmed ().toUpper ();
  if (message.isEmpty ())
    {
      return false;
    }

  decodium::txmsg::EncodedMessage const encoded =
      decodium::txmsg::encodeFt2 (message, true);
  if (!encoded.ok)
    {
      return false;
    }

  QStringList const words = message.split (QLatin1Char (' '), Qt::SkipEmptyParts);
  if (encoded.i3 == 0 && encoded.n3 == 0)
    {
      return !looks_like_ft2_free_text_callsign_ghost (words);
    }

  if (encoded.i3 != 4)
    {
      return true;
    }

  if (words.size () == 2 && words[0] == QStringLiteral ("CQ"))
    {
      return is_plausible_ft2_callsignish (words[1]);
    }

  if (words.size () == 2 || words.size () == 3)
    {
      if (!is_plausible_ft2_callsignish (words[0])
          || !is_plausible_ft2_callsignish (words[1]))
        {
          return false;
        }
      if (words.size () == 3 && !is_ft2_ack_token (words[2]))
        {
          return false;
        }
      return true;
    }

  return false;
}

extern "C" int ftx_ft2_message_is_plausible_c (char const decoded37[37])
{
  if (!decoded37)
    {
      return 0;
    }

  return is_plausible_ft2_decoded_message (QByteArray {decoded37, kFt2DecodedChars}) ? 1 : 0;
}

void fill_fixed_chars (char* dest, int width, QByteArray const& source)
{
  std::fill_n (dest, width, ' ');
  int const count = std::min (width, static_cast<int>(source.size ()));
  if (count > 0)
    {
      std::memcpy (dest, source.constData (), static_cast<size_t> (count));
    }
}

void fill_outline_row (char* dest, int snr, float dt, float freq,
                       QByteArray const& decoded_fixed, int nap)
{
  QString annot = QStringLiteral ("T ");
  if (nap != 0)
    {
      annot = QStringLiteral ("a%1").arg (nap).leftJustified (2, QLatin1Char {' '});
    }
  QByteArray const row = QStringLiteral ("%1%2%3 ~ %4 %5")
                             .arg (snr, 4)
                             .arg (dt, 5, 'f', 1)
                             .arg (static_cast<int> (std::lround (freq)), 5)
                             .arg (QString::fromLatin1 (decoded_fixed.constData (),
                                                        decoded_fixed.size ()))
                             .arg (annot)
                             .toLatin1 ();
  fill_fixed_chars (dest, kFt2OutlineChars, row);
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

std::string format_ft2_decoded_text (std::string const& decoded, int nap, float qual,
                                     std::string* annot_out)
{
  std::string decoded_copy = decoded;
  std::string annot = "  ";
  if (nap != 0)
    {
      annot = "a" + std::to_string (nap);
      if (annot.size () < 2)
        {
          annot.push_back (' ');
        }
      else if (annot.size () > 2)
        {
          annot.resize (2);
        }
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

std::string format_ft2_stdout_line (int nutc, int snr, float dt, float freq,
                                    std::string const& decoded, int nap, float qual)
{
  std::string annot;
  std::string const decoded_copy = format_ft2_decoded_text (decoded, nap, qual, &annot);

  std::ostringstream line;
  line << std::setfill ('0') << std::setw (6) << nutc
       << std::setfill (' ') << std::setw (4) << snr
       << std::fixed << std::setprecision (1) << std::setw (5) << dt
       << std::setw (5) << static_cast<int> (std::lround (freq))
       << " + " << ' ' << decoded_copy << ' ' << annot;
  return line.str ();
}

std::string format_ft2_decoded_file_line (int nutc, float sync, int snr, float dt, float freq,
                                          std::string const& decoded, int nap, float qual)
{
  std::string const decoded_copy = format_ft2_decoded_text (decoded, nap, qual, nullptr);

  std::ostringstream line;
  line << std::setfill ('0') << std::setw (6) << nutc
       << std::setfill (' ') << std::setw (4) << static_cast<int> (std::lround (sync))
       << std::setw (5) << snr
       << std::fixed << std::setprecision (1) << std::setw (6) << dt
       << std::fixed << std::setprecision (0) << std::setw (8) << freq
       << std::setw (4) << 0
       << "   " << decoded_copy << " FT2";
  return line.str ();
}

bool is_grid4 (std::string const& grid)
{
  if (grid.size () != 4)
    {
      return false;
    }
  return grid[0] >= 'A' && grid[0] <= 'R'
      && grid[1] >= 'A' && grid[1] <= 'R'
      && grid[2] >= '0' && grid[2] <= '9'
      && grid[3] >= '0' && grid[3] <= '9';
}

struct Ft2FoxEntry
{
  std::string callsign;
  std::string grid;
  int snr {0};
  int freq {0};
  int n30 {0};
};

struct Ft2EmitState
{
  int n30z {0};
  int nwrap {0};
  std::vector<Ft2FoxEntry> fox_entries;

  void reset ()
  {
    n30z = 0;
    nwrap = 0;
    fox_entries.clear ();
  }
};

QByteArray byte_bits_from_array (std::array<signed char, kFt2Bits> const& bits)
{
  QByteArray byte_bits (kFt2Bits, '\0');
  for (int i = 0; i < kFt2Bits; ++i)
    {
      byte_bits[i] = bits[static_cast<size_t> (i)] != 0 ? 1 : 0;
    }
  return byte_bits;
}

bool array_from_byte_bits (QByteArray const& byte_bits,
                           std::array<signed char, kFt2Bits>* bits_out)
{
  if (!bits_out || byte_bits.size () < kFt2Bits)
    {
      return false;
    }

  for (int i = 0; i < kFt2Bits; ++i)
    {
      char const value = byte_bits.at (i);
      (*bits_out)[static_cast<size_t> (i)] =
          static_cast<signed char> ((value == 1 || value == '1') ? 1 : 0);
    }
  return true;
}

decodium::txmsg::Decode77Context make_decode77_context (QString const& mycall,
                                                        QString const& hiscall)
{
  decodium::txmsg::Decode77Context context;
  context.setMyCall (mycall);
  context.setDxCall (hiscall);
  return context;
}

struct DecodePassResult
{
  bool ok {false};
  bool stop_candidate {false};
  QByteArray message_fixed {QByteArray (kFt2DecodedChars, ' ')};
  std::array<signed char, kFt2Bits> bits {};
  int iaptype {0};
  int selected_pass {0};
  int nharderror {-1};
  int maxosd {0};
  float dmin {0.0f};
};

enum Ft2SearchStatus
{
  kFt2SearchAccept = 0,
  kFt2SearchRejectSmax = 1,
  kFt2SearchRejectSegment1 = 2,
  kFt2SearchRejectFreq = 3,
  kFt2SearchRejectBadsync = 4,
  kFt2SearchRejectNsync = 5
};

struct SegmentSearchTrace
{
  float smax {-99.0f};
  float smaxthresh {0.0f};
  float segment1_smax {-99.0f};
  float f1 {0.0f};
  int ibest {-1};
  int idfbest {0};
  int nsync {0};
  int badsync {0};
  int status {kFt2SearchRejectSmax};
};

struct ApSetup
{
  std::array<int, kFt2Codeword> apbits {};
  std::array<int, 28> apmy_ru {};
  std::array<int, 28> aphis_fd {};

  ApSetup ()
  {
    apbits.fill (0);
    apbits[0] = 99;
    apbits[29] = 99;
    apmy_ru.fill (0);
    aphis_fd.fill (0);
  }
};

struct Stage7State
{
  bool tweak_ready {false};
  bool symbol_ready {false};
  std::array<std::array<Complex, 2 * kFt2Nss>, 33> ctwk2 {};
  std::array<int, kFt2Bits> rvec {};
  std::array<int, 29> mcq {};
  std::array<int, 29> mcqru {};
  std::array<int, 29> mcqfd {};
  std::array<int, 29> mcqtest {};
  std::array<int, 29> mcqww {};
  std::array<int, 6> nappasses {};
  std::array<std::array<int, 4>, 6> naptypes {};
  std::array<float, kFt2Rows * 3> bm_avg {};
  int navg {0};
  float f_avg {0.0f};
  float dt_avg {0.0f};
  bool last_avg_decode_valid {false};
  int last_avg_navg {0};
  float last_avg_freq {0.0f};
  float last_avg_dt {0.0f};
  std::array<char, kFt2DecodedChars> last_avg_decoded {};
};

Stage7State& stage7_state ()
{
  thread_local Stage7State state;
  return state;
}

Ft2EmitState& ft2_emit_state ()
{
  thread_local Ft2EmitState state;
  return state;
}

std::atomic<bool>& stage7_cancel_requested ()
{
  static std::atomic<bool> requested {false};
  return requested;
}

bool stage7_should_cancel ()
{
  return stage7_cancel_requested ().load (std::memory_order_relaxed);
}

int idf_index (int idf)
{
  return idf + 16;
}

bool stage7_debug_enabled ()
{
  static int cached = -1;
  if (cached < 0)
    {
      char const* value = std::getenv ("DECODIUM_FT2_STAGE7_DEBUG");
      cached = (value && value[0] != '\0' && value[0] != '0') ? 1 : 0;
    }
  return cached != 0;
}

int stage7_local_rescue_budget ()
{
  static bool loaded = false;
  static int cached = 0;
  if (!loaded)
    {
      loaded = true;
      char const* value = std::getenv ("DECODIUM_FT2_STAGE7_LOCAL_RESCUE_BUDGET");
      if (value && value[0] != '\0')
        {
          cached = std::atoi (value);
          if (cached < 0)
            {
              cached = 0;
            }
        }
    }
  return cached;
}

int stage7_trace_target_pass ()
{
  static bool loaded = false;
  static int cached = 0;
  if (!loaded)
    {
      loaded = true;
      char const* value = std::getenv ("DECODIUM_FT2_TRACE_PASS");
      if (value && value[0] != '\0')
        {
          cached = std::atoi (value);
          if (cached < 0) cached = 0;
        }
    }
  return cached;
}

float stage7_trace_target_freq ()
{
  static bool loaded = false;
  static float cached = -1.0f;
  if (!loaded)
    {
      loaded = true;
      char const* value = std::getenv ("DECODIUM_FT2_TRACE_FREQ");
      if (value && value[0] != '\0')
        {
          cached = std::strtof (value, nullptr);
        }
    }
  return cached;
}

float stage7_trace_window ()
{
  static bool loaded = false;
  static float cached = 25.0f;
  if (!loaded)
    {
      loaded = true;
      char const* value = std::getenv ("DECODIUM_FT2_TRACE_WINDOW");
      if (value && value[0] != '\0')
        {
          cached = std::strtof (value, nullptr);
          if (!(cached > 0.0f))
            {
              cached = 25.0f;
            }
        }
    }
  return cached;
}

bool stage7_trace_active ()
{
  return stage7_trace_target_pass () > 0 || stage7_trace_target_freq () >= 0.0f;
}

bool stage7_trace_pass_only (int pass_index)
{
  if (!stage7_debug_enabled ())
    {
      return false;
    }
  int const target_pass = stage7_trace_target_pass ();
  if (target_pass > 0 && pass_index != target_pass)
    {
      return false;
    }
  return true;
}

bool stage7_trace_match (int pass_index, float freq)
{
  if (!stage7_debug_enabled ())
    {
      return false;
    }
  int const target_pass = stage7_trace_target_pass ();
  if (target_pass > 0 && pass_index != target_pass)
    {
      return false;
    }
  float const target_freq = stage7_trace_target_freq ();
  if (target_freq >= 0.0f && std::fabs (freq - target_freq) > stage7_trace_window ())
    {
      return false;
    }
  return true;
}

template <typename... Args>
void stage7_debug_logf (char const* format, Args... args)
{
  if (!stage7_debug_enabled ())
    {
      return;
    }
  std::fprintf (stderr, "[ft2-stage7] ");
  std::fprintf (stderr, format, args...);
  std::fprintf (stderr, "\n");
}

void stage7_debug_log_dd_state (char const* tag, float const* dd, int count)
{
  if (!stage7_debug_enabled () || !dd || count <= 0)
    {
      return;
    }

  double sum = 0.0;
  double energy = 0.0;
  double weighted = 0.0;
  for (int i = 0; i < count; ++i)
    {
      double const v = static_cast<double> (dd[i]);
      sum += v;
      energy += v * v;
      weighted += v * static_cast<double> ((i % 257) + 1);
    }

  stage7_debug_logf ("%s dd_sum=%.6f dd_energy=%.6f dd_weighted=%.6f",
                    tag, sum, energy, weighted);
}

template <typename T, size_t N>
float max_abs_diff (std::array<T, N> const& lhs,
                    std::array<T, N> const& rhs,
                    int* index_out,
                    T* lhs_value_out = nullptr,
                    T* rhs_value_out = nullptr)
{
  float max_diff = 0.0f;
  int max_index = -1;
  T lhs_value {};
  T rhs_value {};
  for (size_t i = 0; i < N; ++i)
    {
      float const diff = std::fabs (static_cast<float> (lhs[i] - rhs[i]));
      if (diff > max_diff)
        {
          max_diff = diff;
          max_index = static_cast<int> (i);
          lhs_value = lhs[i];
          rhs_value = rhs[i];
        }
    }
  if (index_out) *index_out = max_index;
  if (lhs_value_out) *lhs_value_out = lhs_value;
  if (rhs_value_out) *rhs_value_out = rhs_value;
  return max_diff;
}

void stage7_debug_log_llr_state (int ipass, int iaptype, float f_for_ap,
                                 std::array<float, kFt2Codeword> const& llr,
                                 std::array<signed char, kFt2Codeword> const& apmask)
{
  if (!stage7_debug_enabled ())
    {
      return;
    }

  double llr_sum = 0.0;
  double llr_abs = 0.0;
  int mask_sum = 0;
  for (size_t i = 0; i < llr.size (); ++i)
    {
      llr_sum += static_cast<double> (llr[i]);
      llr_abs += std::fabs (static_cast<double> (llr[i]));
      mask_sum += static_cast<int> (apmask[i]);
    }

  stage7_debug_logf ("pass=%d iaptype=%d f=%.3f llr_sum=%.6f llr_abs=%.6f mask_sum=%d",
                    ipass, iaptype, f_for_ap, llr_sum, llr_abs, mask_sum);
}

void ensure_tweaks (Stage7State& state)
{
  if (state.tweak_ready)
    {
      return;
    }

  std::array<Complex, 2 * kFt2Nss> ones {};
  std::fill (ones.begin (), ones.end (), Complex {1.0f, 0.0f});
  int const npts = static_cast<int> (ones.size ());
  float const fsample = kFt2FsDown / 2.0f;

  for (int idf = -16; idf <= 16; ++idf)
    {
      std::array<float, 5> a {};
      a[0] = static_cast<float> (idf);
      ftx_twkfreq1_c (ones.data (), &npts, &fsample, a.data (),
                      state.ctwk2[static_cast<size_t> (idf_index (idf))].data ());
    }

  state.tweak_ready = true;
}

void fill_scrambled_symbol (std::array<int, 29>& dest,
                            int const* raw,
                            std::array<int, kFt2Bits> const& rvec,
                            int rvec_offset)
{
  for (int i = 0; i < 29; ++i)
    {
      dest[static_cast<size_t> (i)] = 2 * ((raw[i] + rvec[static_cast<size_t> (rvec_offset + i)]) & 1) - 1;
    }
}

void ensure_symbol_tables (Stage7State& state)
{
  if (state.symbol_ready)
    {
      return;
    }

  int const raw_rvec[kFt2Bits] = {
    0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0,
    1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1,
    0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1
  };
  int const raw_mcq[29] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0
  };
  int const raw_mcqru[29] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,1,1,0,0,1,1,0,0
  };
  int const raw_mcqfd[29] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,0,1,0,0,0,1,0
  };
  int const raw_mcqtest[29] = {
    0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,0,1,0,1,1,1,1,1,1,0,0,1,0
  };
  int const raw_mcqww[29] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,1,1,1,1,0
  };

  for (int i = 0; i < kFt2Bits; ++i)
    {
      state.rvec[static_cast<size_t> (i)] = raw_rvec[i];
    }

  fill_scrambled_symbol (state.mcq, raw_mcq, state.rvec, 0);
  fill_scrambled_symbol (state.mcqru, raw_mcqru, state.rvec, 0);
  fill_scrambled_symbol (state.mcqfd, raw_mcqfd, state.rvec, 0);
  fill_scrambled_symbol (state.mcqtest, raw_mcqtest, state.rvec, 0);
  fill_scrambled_symbol (state.mcqww, raw_mcqww, state.rvec, 0);

  state.nappasses = {{3, 3, 3, 4, 4, 4}};
  state.naptypes[0] = {{1, 2, 0, 0}};
  state.naptypes[1] = {{2, 3, 0, 0}};
  state.naptypes[2] = {{2, 3, 0, 0}};
  state.naptypes[3] = {{3, 4, 5, 6}};
  state.naptypes[4] = {{3, 4, 5, 6}};
  state.naptypes[5] = {{3, 1, 2, 0}};
  state.symbol_ready = true;
}

void reset_average_state (Stage7State& state)
{
  state.navg = 0;
  state.f_avg = 0.0f;
  state.dt_avg = 0.0f;
  state.bm_avg.fill (0.0f);
}

void clear_last_average_emit_state (Stage7State& state)
{
  state.last_avg_decode_valid = false;
  state.last_avg_navg = 0;
  state.last_avg_freq = 0.0f;
  state.last_avg_dt = 0.0f;
  state.last_avg_decoded.fill (' ');
}

int update_ft2_n30_state (Ft2EmitState& state, int nutc)
{
  int n30 = (3600 * (nutc / 10000) + 60 * ((nutc / 100) % 100) + (nutc % 100)) / 30;
  if (n30 < state.n30z)
    {
      state.nwrap += 2880;
    }
  state.n30z = n30;
  return n30 + state.nwrap;
}

int grid_distance_km (std::string const& mygrid, std::string const& hisgrid4)
{
  if (mygrid.empty () || hisgrid4.size () != 4)
    {
      return 9999;
    }

  QString const my_grid = QString::fromStdString (mygrid);
  QString const his_grid = QString::fromStdString (hisgrid4);
  return geo_distance (my_grid, his_grid, 0.0).km;
}

bool should_collect_ft2_fox_entry (std::string const& decoded,
                                   std::string const& mycall,
                                   int freq,
                                   Ft2FoxEntry& entry)
{
  std::istringstream stream {decoded};
  std::string c1;
  std::string c2;
  std::string g2;
  if (!(stream >> c1 >> c2))
    {
      return false;
    }
  stream >> g2;

  bool b0 = c1 == mycall;
  if (c1.rfind ("DE ", 0) == 0 && c2.find ('/') != std::string::npos)
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
  if (!b0 || (!(b1 || b2)) || freq < 1000)
    {
      return false;
    }

  entry.callsign = c2;
  entry.grid = g2;
  entry.freq = freq;
  return true;
}

void write_ft2_houndcallers_file (std::string const& path, Ft2EmitState& state,
                                  std::string const& mygrid, int current_n30)
{
  std::ofstream file {path, std::ios::out | std::ios::trunc};
  if (!file.is_open ())
    {
      return;
    }

  std::vector<Ft2FoxEntry> retained;
  retained.reserve (state.fox_entries.size ());
  for (Ft2FoxEntry const& entry : state.fox_entries)
    {
      int const age = std::min (99, (current_n30 - entry.n30 + 288000) % 2880);
      if (age > 4)
        {
          continue;
        }

      retained.push_back (entry);
      int const dkm = grid_distance_km (mygrid, entry.grid);
      file << std::left << std::setw (12) << entry.callsign
           << ' '
           << std::setw (4) << entry.grid
           << std::right << std::setw (5) << entry.snr
           << std::setw (6) << entry.freq
           << std::setw (7) << dkm
           << std::setw (3) << age
           << '\n';
    }
  file.flush ();
  state.fox_entries = std::move (retained);
}

bool pack_message77_with_context (QString const& message,
                                  decodium::txmsg::Decode77Context* context,
                                  int received,
                                  std::array<signed char, kFt2Bits>* bits_out,
                                  QByteArray* canonical_out,
                                  int* i3_out, int* n3_out)
{
  decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeFt2 (message);
  if (!encoded.ok || encoded.msgbits.size () < kFt2Bits)
    {
      return false;
    }

  if (bits_out)
    {
      if (!array_from_byte_bits (encoded.msgbits, bits_out))
        {
          return false;
        }
    }

  QByteArray canonical = encoded.msgsent;
  if (context)
    {
      decodium::txmsg::DecodedMessage const decoded =
          decodium::txmsg::decode77 (encoded.msgbits, encoded.i3, encoded.n3,
                                     context, received != 0);
      if (decoded.ok)
        {
          canonical = decoded.msgsent;
        }
    }

  if (canonical_out)
    {
      *canonical_out = canonical;
    }
  if (i3_out)
    {
      *i3_out = encoded.i3;
    }
  if (n3_out)
    {
      *n3_out = encoded.n3;
    }
  return true;
}

bool unpack_message77_with_context (std::array<signed char, kFt2Bits> const& bits,
                                    decodium::txmsg::Decode77Context* context,
                                    QByteArray* message_out)
{
  decodium::txmsg::DecodedMessage const decoded =
      decodium::txmsg::decode77 (byte_bits_from_array (bits), context, true);
  if (!decoded.ok)
    {
      return false;
    }
  if (message_out)
    {
      *message_out = decoded.msgsent;
    }
  return true;
}

bool encode_codeword77 (std::array<signed char, kFt2Bits> const& bits,
                        std::array<signed char, kFt2Codeword>* codeword_out)
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

bool message77_to_ft2_tones (std::array<signed char, kFt2Bits> const& bits,
                             std::array<int, kFt2Nn>* tones_out)
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

ApSetup build_ap_setup (Stage7State const& state, QString const& mycall, QString const& hiscall,
                        decodium::txmsg::Decode77Context* context)
{
  ApSetup setup;
  QString const my = mycall.trimmed ();
  if (my.size () < 3)
    {
      return setup;
    }

  QString dx = hiscall.trimmed ();
  bool no_hiscall = false;
  if (dx.size () < 3)
    {
      dx = my;
      no_hiscall = true;
    }

  QString const message = QStringLiteral ("%1 %2 RR73").arg (my, dx);
  std::array<signed char, kFt2Bits> packed_bits {};
  QByteArray canonical;
  int i3 = -1;
  int n3 = -1;
  if (!pack_message77_with_context (message, context, 1, &packed_bits, &canonical, &i3, &n3))
    {
      return setup;
    }
  if (i3 != 1 || canonical != fixed_latin1 (message, kFt2DecodedChars))
    {
      return setup;
    }

  for (int i = 0; i < 28; ++i)
    {
      int const lhs = static_cast<int> (packed_bits[static_cast<size_t> (i)]);
      int const rhs = state.rvec[static_cast<size_t> (i + 1)];
      setup.apmy_ru[static_cast<size_t> (i)] = 2 * ((lhs + rhs) & 1) - 1;
    }
  for (int i = 0; i < 28; ++i)
    {
      int const lhs = static_cast<int> (packed_bits[static_cast<size_t> (29 + i)]);
      int const rhs = state.rvec[static_cast<size_t> (28 + i)];
      setup.aphis_fd[static_cast<size_t> (i)] = 2 * ((lhs + rhs) & 1) - 1;
    }

  std::array<signed char, kFt2Bits> scrambled {};
  for (int i = 0; i < kFt2Bits; ++i)
    {
      scrambled[static_cast<size_t> (i)] =
          static_cast<signed char> ((packed_bits[static_cast<size_t> (i)] + state.rvec[static_cast<size_t> (i)]) & 1);
    }

  std::array<signed char, kFt2Codeword> codeword {};
  if (!encode_codeword77 (scrambled, &codeword))
    {
      return setup;
    }

  for (int i = 0; i < kFt2Codeword; ++i)
    {
      setup.apbits[static_cast<size_t> (i)] = 2 * static_cast<int> (codeword[static_cast<size_t> (i)]) - 1;
    }
  if (no_hiscall)
    {
      setup.apbits[29] = 99;
    }
  return setup;
}

void stage7_debug_compare_with_reference (std::array<float, kFt2Codeword> const& llra,
                                          std::array<float, kFt2Codeword> const& llrb,
                                          std::array<float, kFt2Codeword> const& llrc,
                                          std::array<float, kFt2Codeword> const& llrd,
                                          std::array<float, kFt2Codeword> const& llre,
                                          DecodePassResult const& cpp_result,
                                          int ndepth0, int ncontest, int qso_progress,
                                          bool lapcqonly, int nfqso, float f_for_ap,
                                          bool averaged, bool doosd, float apmag,
                                          QString const& mycall, QString const& hiscall)
{
  static_cast<void> (llra);
  static_cast<void> (llrb);
  static_cast<void> (llrc);
  static_cast<void> (llrd);
  static_cast<void> (llre);
  static_cast<void> (cpp_result);
  static_cast<void> (ndepth0);
  static_cast<void> (ncontest);
  static_cast<void> (qso_progress);
  static_cast<void> (lapcqonly);
  static_cast<void> (nfqso);
  static_cast<void> (f_for_ap);
  static_cast<void> (averaged);
  static_cast<void> (doosd);
  static_cast<void> (apmag);
  static_cast<void> (mycall);
  static_cast<void> (hiscall);
}

void build_llr_sets (float const* metrics,
                     std::array<float, kFt2Codeword>& llra,
                     std::array<float, kFt2Codeword>& llrb,
                     std::array<float, kFt2Codeword>& llrc,
                     std::array<float, kFt2Codeword>& llrd,
                     std::array<float, kFt2Codeword>& llre)
{
  float const scalefac = 2.83f;
  for (int i = 0; i < 58; ++i)
    {
      llra[static_cast<size_t> (i)] = metrics[8 + i] * scalefac;
      llra[static_cast<size_t> (58 + i)] = metrics[74 + i] * scalefac;
      llra[static_cast<size_t> (116 + i)] = metrics[140 + i] * scalefac;

      llrb[static_cast<size_t> (i)] = metrics[8 + i + kFt2Rows] * scalefac;
      llrb[static_cast<size_t> (58 + i)] = metrics[74 + i + kFt2Rows] * scalefac;
      llrb[static_cast<size_t> (116 + i)] = metrics[140 + i + kFt2Rows] * scalefac;

      llrc[static_cast<size_t> (i)] = metrics[8 + i + 2 * kFt2Rows] * scalefac;
      llrc[static_cast<size_t> (58 + i)] = metrics[74 + i + 2 * kFt2Rows] * scalefac;
      llrc[static_cast<size_t> (116 + i)] = metrics[140 + i + 2 * kFt2Rows] * scalefac;
    }

  for (int i = 0; i < kFt2Codeword; ++i)
    {
      float const a = llra[static_cast<size_t> (i)];
      float const b = llrb[static_cast<size_t> (i)];
      float const c = llrc[static_cast<size_t> (i)];
      if (std::fabs (a) >= std::fabs (b) && std::fabs (a) >= std::fabs (c))
        {
          llrd[static_cast<size_t> (i)] = a;
        }
      else if (std::fabs (b) >= std::fabs (c))
        {
          llrd[static_cast<size_t> (i)] = b;
        }
      else
        {
          llrd[static_cast<size_t> (i)] = c;
        }
      llre[static_cast<size_t> (i)] = (a + b + c) / 3.0f;
    }
}

void stage7_debug_compare_bitmetrics_with_reference (
    std::array<Complex, kFt2NSeg> const& cd,
    std::array<float, kFt2Rows * 3> const& stage7_bitmetrics,
    int isp, int icand, int iseg, int ibest, float f1)
{
  static_cast<void> (cd);
  static_cast<void> (stage7_bitmetrics);
  static_cast<void> (isp);
  static_cast<void> (icand);
  static_cast<void> (iseg);
  static_cast<void> (ibest);
  static_cast<void> (f1);
}

void stage7_debug_compare_search_with_reference (
    std::array<float, kFt2NMax> const& dd,
    float f0,
    int isp,
    int icand,
    int ndepth0,
    std::array<SegmentSearchTrace, 3> const& cpp_segments)
{
  static_cast<void> (dd);
  static_cast<void> (f0);
  static_cast<void> (isp);
  static_cast<void> (icand);
  static_cast<void> (ndepth0);
  static_cast<void> (cpp_segments);
}

int count_matches (int const* actual, int const* expected, int count);

void extract_ft2_window (std::array<Complex, kFt2NSeg>& cd,
                         std::array<Complex, kFt2NdMax> const& cb,
                         int istart)
{
  std::fill (cd.begin (), cd.end (), Complex {});
  if (istart >= 0)
    {
      int const it = std::min (kFt2NdMax - 1, istart + kFt2NSeg - 1);
      int const np = it - istart + 1;
      if (np > 0)
        {
          std::copy_n (cb.begin () + istart, np, cd.begin ());
        }
    }
  else
    {
      int const start = -istart;
      int const count = kFt2NSeg + 2 * istart;
      if (start >= 0 && count > 0 && start + count <= kFt2NSeg)
        {
          std::copy_n (cb.begin (), count, cd.begin () + start);
        }
    }
}

int ft2_sync_quality (float const* bitmetrics)
{
  int const costas1[8] = {0,0,0,1,1,0,1,1};
  int const costas2[8] = {0,1,0,0,1,1,1,0};
  int const costas3[8] = {1,1,1,0,0,1,0,0};
  int const costas4[8] = {1,0,1,1,0,0,0,1};
  int hbits[kFt2Rows] {};
  for (int i = 0; i < kFt2Rows; ++i)
    {
      hbits[i] = bm_at (bitmetrics, i, 0) >= 0.0f ? 1 : 0;
    }
  return count_matches (hbits + 0, costas1, 8)
       + count_matches (hbits + 66, costas2, 8)
       + count_matches (hbits + 132, costas3, 8)
       + count_matches (hbits + 198, costas4, 8);
}

bool any_message_bits (std::array<signed char, kFt2Message91> const& message91)
{
  for (int i = 0; i < kFt2Bits; ++i)
    {
      if (message91[static_cast<size_t> (i)] != 0)
        {
          return true;
        }
    }
  return false;
}

bool prepare_ap_pass (Stage7State const& state, ApSetup const& setup,
                      std::array<float, kFt2Codeword> const& llrc, int ncontest,
                      int qso_progress, bool lapcqonly, int nfqso, float f_for_ap,
                      float apmag, int pass_index, std::array<float, kFt2Codeword>& llr,
                      std::array<signed char, kFt2Codeword>& apmask, int* iaptype_out)
{
  llr = llrc;

  int iaptype = state.naptypes[static_cast<size_t> (qso_progress)]
                            [static_cast<size_t> (pass_index - 6)];
  if (lapcqonly)
    {
      iaptype = 1;
    }
  if (iaptype_out)
    {
      *iaptype_out = iaptype;
    }

  int const napwid = 75;
  if (ncontest <= 5 && iaptype >= 3
      && std::fabs (f_for_ap - static_cast<float> (nfqso)) > static_cast<float> (napwid))
    {
      return false;
    }
  if (iaptype >= 2 && setup.apbits[0] > 1)
    {
      return false;
    }
  if (iaptype >= 3 && setup.apbits[29] > 1)
    {
      return false;
    }

  if (iaptype == 1)
    {
      apmask.fill (0);
      for (int i = 0; i < 29; ++i)
        {
          apmask[static_cast<size_t> (i)] = 1;
        }
      std::array<int, 29> const* source = &state.mcq;
      if (ncontest == 1 || ncontest == 2) source = &state.mcqtest;
      else if (ncontest == 3) source = &state.mcqfd;
      else if (ncontest == 4) source = &state.mcqru;
      else if (ncontest == 5) source = &state.mcqww;
      for (int i = 0; i < 29; ++i)
        {
          llr[static_cast<size_t> (i)] = apmag * static_cast<float> ((*source)[static_cast<size_t> (i)]);
        }
    }

  if (iaptype == 2)
    {
      apmask.fill (0);
      if (ncontest == 0 || ncontest == 1 || ncontest == 5)
        {
          for (int i = 0; i < 29; ++i)
            {
              apmask[static_cast<size_t> (i)] = 1;
              llr[static_cast<size_t> (i)] = apmag * static_cast<float> (setup.apbits[static_cast<size_t> (i)]);
            }
        }
      else if (ncontest == 2 || ncontest == 3)
        {
          for (int i = 0; i < 28; ++i)
            {
              apmask[static_cast<size_t> (i)] = 1;
              llr[static_cast<size_t> (i)] = apmag * static_cast<float> (setup.apbits[static_cast<size_t> (i)]);
            }
        }
      else if (ncontest == 4)
        {
          for (int i = 0; i < 28; ++i)
            {
              apmask[static_cast<size_t> (i + 1)] = 1;
              llr[static_cast<size_t> (i + 1)] = apmag * static_cast<float> (setup.apmy_ru[static_cast<size_t> (i)]);
            }
        }
    }

  if (iaptype == 3)
    {
      apmask.fill (0);
      if (ncontest == 0 || ncontest == 1 || ncontest == 2 || ncontest == 5)
        {
          for (int i = 0; i < 58; ++i)
            {
              apmask[static_cast<size_t> (i)] = 1;
              llr[static_cast<size_t> (i)] = apmag * static_cast<float> (setup.apbits[static_cast<size_t> (i)]);
            }
        }
      else if (ncontest == 3)
        {
          for (int i = 0; i < 28; ++i)
            {
              apmask[static_cast<size_t> (i)] = 1;
              llr[static_cast<size_t> (i)] = apmag * static_cast<float> (setup.apbits[static_cast<size_t> (i)]);
            }
          for (int i = 0; i < 28; ++i)
            {
              apmask[static_cast<size_t> (28 + i)] = 1;
              llr[static_cast<size_t> (28 + i)] = apmag * static_cast<float> (setup.aphis_fd[static_cast<size_t> (i)]);
            }
        }
      else if (ncontest == 4)
        {
          for (int i = 0; i < 28; ++i)
            {
              apmask[static_cast<size_t> (i + 1)] = 1;
              llr[static_cast<size_t> (i + 1)] = apmag * static_cast<float> (setup.apmy_ru[static_cast<size_t> (i)]);
            }
          for (int i = 0; i < 28; ++i)
            {
              apmask[static_cast<size_t> (29 + i)] = 1;
              llr[static_cast<size_t> (29 + i)] = apmag * static_cast<float> (setup.apbits[static_cast<size_t> (29 + i)]);
            }
        }
    }

  if (iaptype == 4 || iaptype == 5 || iaptype == 6)
    {
      if (ncontest <= 5)
        {
          apmask.fill (0);
          for (int i = 0; i < 77; ++i)
            {
              apmask[static_cast<size_t> (i)] = 1;
              if (iaptype == 6)
                {
                  llr[static_cast<size_t> (i)] = apmag * static_cast<float> (setup.apbits[static_cast<size_t> (i)]);
                }
            }
        }
    }

  return true;
}

DecodePassResult run_decode_passes (Stage7State const& state, ApSetup const& setup,
                                    decodium::txmsg::Decode77Context* context,
                                    std::array<float, kFt2Codeword> const& llra,
                                    std::array<float, kFt2Codeword> const& llrb,
                                    std::array<float, kFt2Codeword> const& llrc,
                                    std::array<float, kFt2Codeword> const& llrd_base,
                                    std::array<float, kFt2Codeword> const& llre,
                                    int ndepth0, int ncontest, int qso_progress,
                                    bool doosd, bool lapcqonly, int nfqso, float f_for_ap,
                                    bool averaged, float apmag)
{
  DecodePassResult result;

  int npasses = 5 + state.nappasses[static_cast<size_t> (qso_progress)];
  if (lapcqonly)
    {
      npasses = 6;
    }
  if (ndepth0 == 1)
    {
      npasses = 5;
    }
  if (ncontest == 6)
    {
      npasses = averaged ? 5 : 6;
    }

  // The legacy FT2 decoder intentionally reuses the previous AP mask when an
  // AP pass resolves to iaptype=0. Weak-signal cases rely on that quirk, so
  // the stage-7 port keeps the same mask state within a single decode attempt.
  std::array<signed char, kFt2Codeword> apmask {};
  for (int ipass = 1; ipass <= npasses; ++ipass)
    {
      if (stage7_should_cancel ())
        {
          return result;
        }
      std::array<float, kFt2Codeword> llr {};
      int iaptype = 0;
      if (ipass == 1)
        {
          llr = llra;
          apmask.fill (0);
        }
      else if (ipass == 2)
        {
          llr = llrb;
          apmask.fill (0);
        }
      else if (ipass == 3)
        {
          llr = llrc;
          apmask.fill (0);
        }
      else if (ipass == 4)
        {
          llr = llrd_base;
          apmask.fill (0);
        }
      else if (ipass == 5)
        {
          llr = llre;
          apmask.fill (0);
        }
      else if (!prepare_ap_pass (state, setup, llrc, ncontest, qso_progress, lapcqonly,
                                 nfqso, f_for_ap, apmag, ipass, llr, apmask, &iaptype))
        {
          continue;
        }

      std::array<signed char, kFt2Message91> message91 {};
      std::array<signed char, kFt2Codeword> cw {};
      int ntype = 0;
      int nharderror = -1;
      float dmin = 0.0f;

      int maxosd = 3;
      if (!doosd)
        {
          maxosd = -1;
        }

      stage7_debug_log_llr_state (ipass, iaptype, f_for_ap, llr, apmask);
      ftx_decode174_91_c (llr.data (), 91, maxosd, 3, apmask.data (), message91.data (),
                          cw.data (), &ntype, &nharderror, &dmin);
      stage7_debug_logf ("pass=%d iaptype=%d maxosd=%d nharderror=%d dmin=%.3f",
                        ipass, iaptype, maxosd, nharderror, dmin);
      if (!any_message_bits (message91))
        {
          continue;
        }
      if (nharderror < 0)
        {
          continue;
        }

      result.stop_candidate = true;

      std::array<signed char, kFt2Bits> message77 {};
      for (int i = 0; i < kFt2Bits; ++i)
        {
          message77[static_cast<size_t> (i)] =
              static_cast<signed char> ((message91[static_cast<size_t> (i)] + state.rvec[static_cast<size_t> (i)]) & 1);
        }

      QByteArray decoded;
      if (!unpack_message77_with_context (message77, context, &decoded))
        {
          stage7_debug_logf ("pass=%d unpack failed after nharderror=%d", ipass, nharderror);
          break;
        }

      if (!is_plausible_ft2_decoded_message (decoded))
        {
          stage7_debug_logf ("pass=%d rejected implausible decode=\"%s\"",
                             ipass, decoded.constData ());
          result.stop_candidate = false;
          continue;
        }

      stage7_debug_logf ("pass=%d decoded=\"%s\" nharderror=%d dmin=%.3f",
                         ipass, decoded.constData (), nharderror, dmin);
      result.ok = true;
      result.message_fixed = decoded;
      result.bits = message77;
      result.iaptype = iaptype;
      result.selected_pass = ipass;
      result.nharderror = nharderror;
      result.maxosd = maxosd;
      result.dmin = dmin;
      return result;
    }

  return result;
}

int count_matches (int const* actual, int const* expected, int count)
{
  int matches = 0;
  for (int i = 0; i < count; ++i)
    {
      if (actual[i] == expected[i])
        {
          ++matches;
        }
    }
  return matches;
}

void copy_decode_row (int index, float sync, int snr, float dt, float freq, int nap, float qual,
                      std::array<signed char, kFt2Bits> const& bits,
                      QByteArray const& decoded,
                      float* syncs, int* snrs, float* dts, float* freqs, int* naps, float* quals,
                      signed char* bits77, char* decodeds)
{
  if (syncs) syncs[index] = sync;
  snrs[index] = snr;
  dts[index] = dt;
  freqs[index] = freq;
  naps[index] = nap;
  quals[index] = qual;
  std::copy (bits.begin (), bits.end (), bits77 + index * kFt2Bits);
  fill_fixed_chars (decodeds + index * kFt2DecodedChars, kFt2DecodedChars, decoded);
}

void clear_outputs (float* syncs, int* snrs, float* dts, float* freqs, int* naps, float* quals,
                    signed char* bits77, char* decodeds, int* nout)
{
  if (syncs) std::fill_n (syncs, kFt2MaxLines, 0.0f);
  if (snrs) std::fill_n (snrs, kFt2MaxLines, 0);
  if (dts) std::fill_n (dts, kFt2MaxLines, 0.0f);
  if (freqs) std::fill_n (freqs, kFt2MaxLines, 0.0f);
  if (naps) std::fill_n (naps, kFt2MaxLines, 0);
  if (quals) std::fill_n (quals, kFt2MaxLines, 0.0f);
  if (bits77) std::fill_n (bits77, kFt2MaxLines * kFt2Bits, static_cast<signed char> (0));
  if (decodeds) std::fill_n (decodeds, kFt2MaxLines * kFt2DecodedChars, ' ');
  if (nout) *nout = 0;
}

void decode_ft2_stage7 (short const* iwave, int nqsoprogress, int nfqso, int nfa, int nfb,
                        int ndepth, int ncontest, QString const& mycall, QString const& hiscall,
                        float* syncs, int* snrs, float* dts, float* freqs, int* naps, float* quals,
                        signed char* bits77, char* decodeds, int* nout)
{
  clear_outputs (syncs, snrs, dts, freqs, naps, quals, bits77, decodeds, nout);
  if (!iwave || !snrs || !dts || !freqs || !naps || !quals || !bits77 || !decodeds || !nout)
    {
      return;
    }

  Stage7State& state = stage7_state ();
  ensure_tweaks (state);
  ensure_symbol_tables (state);
  decodium::txmsg::Decode77Context context = make_decode77_context (mycall, hiscall);
  auto abort_if_cancelled = [&state, nout] () {
    if (!stage7_should_cancel ())
      {
        return false;
      }
    reset_average_state (state);
    if (nout)
      {
        *nout = 0;
      }
    return true;
  };
  if (abort_if_cancelled ())
    {
      return;
    }

  ApSetup const ap_setup = build_ap_setup (state, mycall, hiscall, &context);

  std::array<float, kFt2NMax> dd {};
  for (int i = 0; i < kFt2NMax; ++i)
    {
      dd[static_cast<size_t> (i)] = static_cast<float> (iwave[i]);
    }

  int const qso_progress = std::max (0, std::min (5, nqsoprogress));
  int const ndepth0 = ndepth & 7;
  bool dosubtract = true;
  bool doosd = true;
  int nsp = 4;
  if (ndepth0 == 2)
    {
      nsp = 3;
      doosd = false;
    }
  else if (ndepth0 <= 1)
    {
      nsp = 1;
      dosubtract = false;
      doosd = false;
    }

  float syncmin = 0.80f;
  if (ndepth0 >= 2) syncmin = 0.78f;
  if (ndepth0 >= 3) syncmin = 0.70f;

  int ndecodes = 0;
  int nwork_decodes = 0;
  int nd1 = 0;
  int nd2 = 0;
  float best_sync_avg = -99.0f;
  float best_f1_avg = 0.0f;
  int best_ibest_avg = 0;
  bool got_candidate = false;
  std::array<float, kFt2Rows * 3> best_bm {};
  std::vector<QByteArray> seen_decodes;
  seen_decodes.reserve (kFt2MaxLines);

  std::array<float, kFt2Nh1> savg {};
  std::array<float, kFt2Nh1> sbase {};
  std::array<float, 2 * kFt2MaxCand> candidate {};
  std::array<Complex, kFt2NdMax> cd2 {};
  std::array<Complex, kFt2NdMax> cb {};
  std::array<Complex, kFt2NSeg> cd {};
  std::array<float, kFt2Rows * 3> bitmetrics {};

  for (int isp = 1; isp <= nsp; ++isp)
    {
      int local_rescue_budget = stage7_local_rescue_budget ();
      if (abort_if_cancelled ())
        {
          return;
        }
      if (isp == 2)
        {
          if (nwork_decodes == 0)
            {
              break;
            }
          nd1 = nwork_decodes;
        }
      else if (isp == 3)
        {
          nd2 = nwork_decodes - nd1;
          if (nd2 == 0)
            {
              break;
            }
        }
      else if (isp == 4)
        {
          int const nd3 = nwork_decodes - nd1 - nd2;
          if (nd3 == 0)
            {
              break;
            }
        }

      float syncmin_pass = syncmin;
      if (isp >= 2) syncmin_pass *= 0.88f;
      if (isp >= 3) syncmin_pass *= 0.76f;

      if (stage7_trace_pass_only (isp))
        {
          char tag[64];
          std::snprintf (tag, sizeof(tag), "pass=%d before getcand", isp);
          stage7_debug_log_dd_state (tag, dd.data (), kFt2NMax);
        }

      int ncand = 0;
      ftx_getcandidates2_c (dd.data (), static_cast<float> (nfa), static_cast<float> (nfb),
                            syncmin_pass, static_cast<float> (nfqso), kFt2MaxCand,
                            savg.data (), candidate.data (), &ncand, sbase.data ());
      if (stage7_trace_pass_only (isp))
        {
          stage7_debug_logf ("pass=%d ncand=%d", isp, ncand);
        }

      int newdata = 1;
      for (int icand = 0; icand < ncand; ++icand)
        {
          if (abort_if_cancelled ())
            {
              return;
            }
          float const f0 = candidate[static_cast<size_t> (icand * 2)];
          float const snr = candidate[static_cast<size_t> (icand * 2 + 1)] - 1.0f;
          std::array<SegmentSearchTrace, 3> search_trace {};
          std::array<float, kFt2NMax> const dd_before_candidate = dd;
          if (!stage7_trace_active () || stage7_trace_match (isp, f0))
            {
              stage7_debug_logf ("pass=%d cand=%d/%d f0=%.1f snr_est=%.3f",
                                isp, icand + 1, ncand, f0, snr);
            }

          ftx_ft2_downsample_c (dd.data (), &newdata, f0, cd2.data ());
          newdata = 0;

          float sum2 = 0.0f;
          for (size_t i = 0; i < cd2.size (); ++i)
            {
              sum2 += std::norm (cd2[i]);
            }
          sum2 /= static_cast<float> (kFt2NdMax);
          if (sum2 > 0.0f)
            {
              float const scale = 1.0f / std::sqrt (sum2);
              for (size_t i = 0; i < cd2.size (); ++i)
                {
                  cd2[i] *= scale;
                }
            }

          float segment1_smax = -99.0f;
          for (int iseg = 1; iseg <= 3; ++iseg)
            {
              if (abort_if_cancelled ())
                {
                  return;
                }
              SegmentSearchTrace& segment_trace = search_trace[static_cast<size_t> (iseg - 1)];
              int ibest = -1;
              int idfbest = 0;
              float smax = -99.0f;

              for (int isync = 1; isync <= 2; ++isync)
                {
                  if (abort_if_cancelled ())
                    {
                      return;
                    }
                  int idfmin = -12;
                  int idfmax = 12;
                  int idfstp = 3;
                  int ibmin = -688;
                  int ibmax = 2024;
                  int ibstp = 4;

                  if (isync == 1)
                    {
                      if (iseg == 1)
                        {
                          ibmin = 216;
                          ibmax = 1120;
                        }
                      else if (iseg == 2)
                        {
                          ibmin = 1120;
                          ibmax = 2024;
                        }
                      else
                        {
                          ibmin = -688;
                          ibmax = 216;
                        }
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
                      if (abort_if_cancelled ())
                        {
                          return;
                        }
                      for (int istart = ibmin; istart <= ibmax; istart += ibstp)
                        {
                          float sync = 0.0f;
                          ftx_sync2d_c (cd2.data (), kFt2NdMax, istart,
                                        state.ctwk2[static_cast<size_t> (idf_index (idf))].data (),
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
                  segment1_smax = smax;
                }

              float smaxthresh = 0.60f;
              if (ndepth0 >= 3) smaxthresh = 0.50f;
              if (isp >= 2) smaxthresh *= 0.88f;
              if (isp >= 3) smaxthresh *= 0.76f;
              float const trace_f1 = f0 + static_cast<float> (idfbest);
              segment_trace.smax = smax;
              segment_trace.smaxthresh = smaxthresh;
              segment_trace.segment1_smax = segment1_smax;
              segment_trace.f1 = trace_f1;
              segment_trace.ibest = ibest;
              segment_trace.idfbest = idfbest;
              if (smax < smaxthresh)
                {
                  segment_trace.status = kFt2SearchRejectSmax;
                  if (stage7_trace_match (isp, trace_f1))
                    {
                      stage7_debug_logf (
                          "pass=%d cand=%d seg=%d rejected by smax %.3f < %.3f f1=%.3f",
                          isp, icand + 1, iseg, smax, smaxthresh, trace_f1);
                    }
                  continue;
                }
              if (iseg > 1 && smax < segment1_smax)
                {
                  segment_trace.status = kFt2SearchRejectSegment1;
                  if (stage7_trace_match (isp, trace_f1))
                    {
                      stage7_debug_logf (
                          "pass=%d cand=%d seg=%d rejected by segment1 %.3f < %.3f f1=%.3f",
                          isp, icand + 1, iseg, smax, segment1_smax, trace_f1);
                    }
                  continue;
                }

              float const f1 = f0 + static_cast<float> (idfbest);
              segment_trace.f1 = f1;
              if (f1 <= 10.0f || f1 >= 4990.0f)
                {
                  segment_trace.status = kFt2SearchRejectFreq;
                  continue;
                }

              ftx_ft2_downsample_c (dd.data (), &newdata, f1, cb.data ());

              sum2 = 0.0f;
              for (size_t i = 0; i < cb.size (); ++i)
                {
                  sum2 += std::norm (cb[i]);
                }
              sum2 /= static_cast<float> (kFt2NSeg);
              if (sum2 > 0.0f)
                {
                  float const scale = 1.0f / std::sqrt (sum2);
                  for (size_t i = 0; i < cb.size (); ++i)
                    {
                      cb[i] *= scale;
                    }
                }

              extract_ft2_window (cd, cb, ibest);

              int badsync = 0;
              bitmetrics.fill (0.0f);
              ftx_ft2_bitmetrics_c (cd.data (), bitmetrics.data (), &badsync);
              stage7_debug_compare_bitmetrics_with_reference (
                  cd, bitmetrics, isp, icand + 1, iseg, ibest, f1);
              segment_trace.badsync = badsync;
              if (badsync != 0)
                {
                  segment_trace.status = kFt2SearchRejectBadsync;
                  if (stage7_trace_match (isp, f1))
                    {
                      stage7_debug_logf ("pass=%d cand=%d seg=%d f1=%.1f badsync",
                                        isp, icand + 1, iseg, f1);
                    }
                  continue;
                }

              if ((ndepth & 16) != 0
                  && std::fabs (f1 - static_cast<float> (nfqso)) < 100.0f
                  && smax > best_sync_avg)
                {
                  best_bm = bitmetrics;
                  best_sync_avg = smax;
                  best_f1_avg = f1;
                  best_ibest_avg = ibest;
                  got_candidate = true;
                }

              int nsync_qual = ft2_sync_quality (bitmetrics.data ());
              segment_trace.nsync = nsync_qual;
              int nsync_qual_min = 13;
              if (ndepth0 >= 3) nsync_qual_min = 10;
              if (nsync_qual < nsync_qual_min)
                {
                  segment_trace.status = kFt2SearchRejectNsync;
                  if (stage7_trace_match (isp, f1))
                    {
                      stage7_debug_logf ("pass=%d cand=%d seg=%d f1=%.1f nsync=%d < %d",
                                        isp, icand + 1, iseg, f1, nsync_qual, nsync_qual_min);
                    }
                  continue;
                }
              segment_trace.status = kFt2SearchAccept;

              if (!stage7_trace_active () || stage7_trace_match (isp, f1))
                {
                  stage7_debug_logf ("pass=%d cand=%d seg=%d f1=%.1f smax=%.3f nsync=%d ibest=%d",
                                    isp, icand + 1, iseg, f1, smax, nsync_qual, ibest);
                }

              std::array<float, kFt2Codeword> llra {};
              std::array<float, kFt2Codeword> llrb {};
              std::array<float, kFt2Codeword> llrc {};
              std::array<float, kFt2Codeword> llrd {};
              std::array<float, kFt2Codeword> llre {};
              build_llr_sets (bitmetrics.data (), llra, llrb, llrc, llrd, llre);

              float apmag = 0.0f;
              for (size_t i = 0; i < llra.size (); ++i)
                {
                  apmag = std::max (apmag, std::fabs (llra[i]));
                }
              apmag *= 1.1f;

              DecodePassResult const decoded = run_decode_passes (
                  state, ap_setup, &context, llra, llrb, llrc, llrd, llre, ndepth0, ncontest,
                  qso_progress, doosd, false, nfqso, f1, false, apmag);
              stage7_debug_compare_with_reference (
                  llra, llrb, llrc, llrd, llre, decoded, ndepth0, ncontest, qso_progress,
                  false, nfqso, f1, false, doosd, apmag, mycall, hiscall);

              DecodePassResult decoded_best = decoded;
              int ibest_best = ibest;
              float qual_best = nsync_qual;
              float f1_best = f1;
              float const freq_deltas[] = {
                  -0.500f, -0.375f, -0.250f, -0.1875f, -0.125f, -0.0625f,
                   0.0625f, 0.125f,  0.1875f, 0.250f,  0.375f,  0.500f
              };
              int const ibest_deltas[] = {0, -1, 1, -2, 2, -3, 3};

              if (!decoded_best.ok)
                {
                  bool const enable_local_rescue =
                      local_rescue_budget > 0
                      && isp >= 2
                      && nsync_qual >= 18
                      && smax >= smaxthresh + 0.10f;
                  if (!enable_local_rescue)
                    {
                      if (decoded_best.stop_candidate)
                        {
                          break;
                        }
                      continue;
                    }
                  --local_rescue_budget;
                  bool rescued = false;
                  for (float rescue_freq_delta : freq_deltas)
                    {
                      if (abort_if_cancelled ())
                        {
                          return;
                        }
                      float const f1_try = f1 + rescue_freq_delta;
                      if (f1_try <= 10.0f || f1_try >= 4990.0f)
                        {
                          continue;
                        }

                      int rescue_newdata = 0;
                      ftx_ft2_downsample_c (dd.data (), &rescue_newdata, f1_try, cb.data ());

                      sum2 = 0.0f;
                      for (size_t i = 0; i < cb.size (); ++i)
                        {
                          sum2 += std::norm (cb[i]);
                        }
                      sum2 /= static_cast<float> (kFt2NSeg);
                      if (sum2 > 0.0f)
                        {
                          float const scale = 1.0f / std::sqrt (sum2);
                          for (size_t i = 0; i < cb.size (); ++i)
                            {
                              cb[i] *= scale;
                            }
                        }

                      for (int ibest_delta : ibest_deltas)
                        {
                          if (abort_if_cancelled ())
                            {
                              return;
                            }
                          int const ibest_try = ibest_best + ibest_delta;
                          extract_ft2_window (cd, cb, ibest_try);

                          int badsync_try = 0;
                          bitmetrics.fill (0.0f);
                          ftx_ft2_bitmetrics_c (cd.data (), bitmetrics.data (), &badsync_try);
                          stage7_debug_compare_bitmetrics_with_reference (
                              cd, bitmetrics, isp, icand + 1, iseg, ibest_try, f1_try);
                          if (badsync_try != 0)
                            {
                              continue;
                            }

                          int const nsync_try = ft2_sync_quality (bitmetrics.data ());
                          int nsync_try_min = 13;
                          if (ndepth0 >= 3) nsync_try_min = 10;
                          if (nsync_try < nsync_try_min)
                            {
                              continue;
                            }

                          std::array<float, kFt2Codeword> llra_try {};
                          std::array<float, kFt2Codeword> llrb_try {};
                          std::array<float, kFt2Codeword> llrc_try {};
                          std::array<float, kFt2Codeword> llrd_try {};
                          std::array<float, kFt2Codeword> llre_try {};
                          build_llr_sets (bitmetrics.data (), llra_try, llrb_try, llrc_try,
                                          llrd_try, llre_try);

                          float apmag_try = 0.0f;
                          for (size_t i = 0; i < llra_try.size (); ++i)
                            {
                              apmag_try = std::max (apmag_try, std::fabs (llra_try[i]));
                            }
                          apmag_try *= 1.1f;

                          DecodePassResult const decoded_try = run_decode_passes (
                              state, ap_setup, &context, llra_try, llrb_try, llrc_try, llrd_try, llre_try,
                              ndepth0, ncontest, qso_progress, doosd, false, nfqso, f1_try,
                              false, apmag_try);
                          if (!decoded_try.ok)
                            {
                              if (decoded_try.stop_candidate)
                                {
                                  decoded_best = decoded_try;
                                }
                              continue;
                            }

                          bool const better_rescue =
                              !rescued
                              || decoded_try.nharderror < decoded_best.nharderror
                              || (decoded_try.nharderror == decoded_best.nharderror
                                  && decoded_try.dmin + 1.0e-4f < decoded_best.dmin)
                              || (decoded_try.nharderror == decoded_best.nharderror
                                  && std::fabs (decoded_try.dmin - decoded_best.dmin) <= 1.0e-4f
                                  && nsync_try > qual_best);
                          if (!better_rescue)
                            {
                              continue;
                            }

                          rescued = true;
                          decoded_best = decoded_try;
                          ibest_best = ibest_try;
                          qual_best = static_cast<float> (nsync_try);
                          f1_best = f1_try;
                          stage7_debug_logf (
                              "rescued cand=%d seg=%d f1 %.3f->%.3f ibest %d->%d with \"%s\"",
                              icand + 1, iseg, f1, f1_best, ibest, ibest_best,
                              decoded_best.message_fixed.constData ());
                        }
                    }

                  if (decoded_best.stop_candidate)
                    {
                      break;
                    }
                  if (decoded_best.ok)
                    {
                      nsync_qual = static_cast<int> (qual_best);
                    }
                  else
                    {
                      continue;
                    }
                }

              ibest = ibest_best;

              if (dosubtract)
                {
                  std::array<int, kFt2Nn> tones {};
                  if (message77_to_ft2_tones (decoded_best.bits, &tones))
                    {
                      float const dt = static_cast<float> (ibest) * kFt2FreqDtScale;
                      ftx_subtract_ft2_c (dd.data (), tones.data (), f1_best, dt);
                      if (stage7_trace_pass_only (isp))
                        {
                          char tag[96];
                          stage7_debug_logf ("subtract msg=\"%s\" f1=%.3f dt=%.6f",
                                            decoded_best.message_fixed.constData (), f1_best, dt);
                          std::snprintf (tag, sizeof(tag), "pass=%d after subtract", isp);
                          stage7_debug_log_dd_state (tag, dd.data (), kFt2NMax);
                        }
                    }
                }

              if (std::find (seen_decodes.begin (), seen_decodes.end (), decoded_best.message_fixed)
                  != seen_decodes.end ())
                {
                  break;
                }

              seen_decodes.push_back (decoded_best.message_fixed);
              ++nwork_decodes;
              float xsnr = -21.0f;
              if (snr > 0.0f)
                {
                  xsnr = 10.0f * std::log10 (snr) - 13.0f;
                }
              int const nsnr = static_cast<int> (std::lround (std::max (-21.0f, xsnr)));
              float const xdt = static_cast<float> (ibest) * kFt2FreqDtScale - 0.5f;
              float const qual = 1.0f - (static_cast<float> (decoded_best.nharderror) + decoded_best.dmin) / 60.0f;

              if (decoded_best.nharderror > 48)
                {
                  stage7_debug_logf ("pass=%d cand=%d provisional decode=\"%s\" nharderror=%d dmin=%.3f",
                                     isp, icand + 1, decoded_best.message_fixed.constData (),
                                     decoded_best.nharderror, decoded_best.dmin);
                  break;
                }

              if (ndecodes < kFt2MaxLines)
                {
                  copy_decode_row (ndecodes, smax, nsnr, xdt, f1_best, decoded_best.iaptype, qual,
                                   decoded_best.bits, decoded_best.message_fixed, syncs, snrs, dts, freqs,
                                   naps, quals, bits77, decodeds);
                }
              ++ndecodes;
              break;
            }

          stage7_debug_compare_search_with_reference (
              dd_before_candidate, f0, isp, icand + 1, ndepth0, search_trace);
        }
    }

  if ((ndepth & 16) != 0 && got_candidate)
    {
      if (abort_if_cancelled ())
        {
          return;
        }
      float const candidate_dt = static_cast<float> (best_ibest_avg) * kFt2FreqDtScale;
      if (state.navg == 0 || std::fabs (best_f1_avg - state.f_avg) > 10.0f)
        {
          state.bm_avg = best_bm;
          state.navg = 1;
          state.f_avg = best_f1_avg;
          state.dt_avg = candidate_dt;
        }
      else
        {
          state.navg += 1;
          int const ntc = std::min (state.navg, 6);
          float const u = 1.0f / static_cast<float> (ntc);
          for (size_t i = 0; i < state.bm_avg.size (); ++i)
            {
              state.bm_avg[i] = u * best_bm[i] + (1.0f - u) * state.bm_avg[i];
            }
          state.f_avg = u * best_f1_avg + (1.0f - u) * state.f_avg;
          state.dt_avg = u * candidate_dt + (1.0f - u) * state.dt_avg;
        }

      if (nwork_decodes == 0 && state.navg >= 2)
        {
          std::array<float, kFt2Codeword> llra {};
          std::array<float, kFt2Codeword> llrb {};
          std::array<float, kFt2Codeword> llrc {};
          std::array<float, kFt2Codeword> llrd {};
          std::array<float, kFt2Codeword> llre {};
          build_llr_sets (state.bm_avg.data (), llra, llrb, llrc, llrd, llre);

          float apmag = 0.0f;
          for (size_t i = 0; i < llra.size (); ++i)
            {
              apmag = std::max (apmag, std::fabs (llra[i]));
            }
          apmag *= 1.1f;

          if (abort_if_cancelled ())
            {
              return;
            }

          DecodePassResult const decoded = run_decode_passes (
              state, ap_setup, &context, llra, llrb, llrc, llrd, llre, ndepth0, ncontest,
              qso_progress, doosd, false, nfqso, state.f_avg, true, apmag);
          stage7_debug_compare_with_reference (
              llra, llrb, llrc, llrd, llre, decoded, ndepth0, ncontest, qso_progress,
              false, nfqso, state.f_avg, true, doosd, apmag, mycall, hiscall);
          if (decoded.ok && ndecodes < kFt2MaxLines)
            {
              float const qual =
                  1.0f - (static_cast<float> (decoded.nharderror) + decoded.dmin) / 60.0f;
              state.last_avg_decode_valid = true;
              state.last_avg_navg = state.navg;
              state.last_avg_freq = state.f_avg;
              state.last_avg_dt = state.dt_avg - 0.5f;
              state.last_avg_decoded.fill (' ');
              fill_fixed_chars (state.last_avg_decoded.data (), kFt2DecodedChars,
                                decoded.message_fixed);
              copy_decode_row (ndecodes, best_sync_avg, -21, state.dt_avg - 0.5f, state.f_avg,
                               decoded.iaptype, qual, decoded.bits, decoded.message_fixed,
                               syncs, snrs, dts, freqs, naps, quals, bits77, decodeds);
              ++ndecodes;
              reset_average_state (state);
            }
          else if (decoded.ok)
            {
              reset_average_state (state);
            }
        }
    }

  *nout = std::min (ndecodes, kFt2MaxLines);
}

}

extern "C" void ftx_ft2_stage7_set_cancel_c (int cancel)
{
  stage7_cancel_requested ().store (cancel != 0, std::memory_order_relaxed);
}

extern "C" void ftx_ft2_async_decode_stage7_c (short const* iwave, int* nqsoprogress, int* nfqso,
                                                int* nfa, int* nfb, int* ndepth, int* ncontest,
                                                char const* mycall, char const* hiscall,
                                                int* snrs, float* dts, float* freqs, int* naps,
                                                float* quals, signed char* bits77, char* decodeds,
                                                int* nout)
{
  QString const my = mycall ? qt_trimmed_string (mycall, 12) : QString {};
  QString const his = hiscall ? qt_trimmed_string (hiscall, 12) : QString {};
  decode_ft2_stage7 (iwave,
                     nqsoprogress ? *nqsoprogress : 0,
                     nfqso ? *nfqso : 0,
                     nfa ? *nfa : 0,
                     nfb ? *nfb : 0,
                     ndepth ? *ndepth : 3,
                     ncontest ? *ncontest : 0,
                     my, his, nullptr,
                     snrs, dts, freqs, naps, quals, bits77, decodeds, nout);
}

extern "C" void ftx_ft2_decode_candidate_stage7_c (float const* llra, float const* llrb,
                                                    float const* llrc, float const* llrd,
                                                    float const* llre, int ndepth0,
                                                    int ncontest, int qso_progress,
                                                    int lapcqonly, int nfqso, float f1,
                                                    float apmag, int averaged, int doosd,
                                                    char const* mycall, char const* hiscall,
                                                    int* ok_out, int* stop_candidate_out,
                                                    int* selected_pass_out, int* iaptype_out,
                                                    int* nharderror_out, float* dmin_out,
                                                    int* maxosd_out, signed char* message77_out,
                                                    char* decoded_out)
{
  if (ok_out) *ok_out = 0;
  if (stop_candidate_out) *stop_candidate_out = 0;
  if (selected_pass_out) *selected_pass_out = 0;
  if (iaptype_out) *iaptype_out = 0;
  if (nharderror_out) *nharderror_out = -1;
  if (dmin_out) *dmin_out = 0.0f;
  if (maxosd_out) *maxosd_out = 0;
  if (message77_out) std::fill_n (message77_out, kFt2Bits, static_cast<signed char> (0));
  if (decoded_out) std::fill_n (decoded_out, kFt2DecodedChars, ' ');
  if (!llra || !llrb || !llrc || !llrd || !llre)
    {
      return;
    }

  std::array<float, kFt2Codeword> a {};
  std::array<float, kFt2Codeword> b {};
  std::array<float, kFt2Codeword> c {};
  std::array<float, kFt2Codeword> d {};
  std::array<float, kFt2Codeword> e {};
  std::copy_n (llra, kFt2Codeword, a.begin ());
  std::copy_n (llrb, kFt2Codeword, b.begin ());
  std::copy_n (llrc, kFt2Codeword, c.begin ());
  std::copy_n (llrd, kFt2Codeword, d.begin ());
  std::copy_n (llre, kFt2Codeword, e.begin ());

  Stage7State& state = stage7_state ();
  ensure_tweaks (state);
  QString const my = mycall ? qt_trimmed_string (mycall, 12) : QString {};
  QString const his = hiscall ? qt_trimmed_string (hiscall, 12) : QString {};
  decodium::txmsg::Decode77Context context = make_decode77_context (my, his);
  ApSetup const ap_setup = build_ap_setup (state, my, his, &context);
  DecodePassResult const result = run_decode_passes (
      state, ap_setup, &context, a, b, c, d, e, ndepth0, ncontest, qso_progress,
      doosd != 0, lapcqonly != 0, nfqso, f1, averaged != 0, apmag);

  if (ok_out) *ok_out = result.ok ? 1 : 0;
  if (stop_candidate_out) *stop_candidate_out = result.stop_candidate ? 1 : 0;
  if (selected_pass_out) *selected_pass_out = result.selected_pass;
  if (iaptype_out) *iaptype_out = result.iaptype;
  if (nharderror_out) *nharderror_out = result.nharderror;
  if (dmin_out) *dmin_out = result.dmin;
  if (maxosd_out) *maxosd_out = result.maxosd;
  if (message77_out)
    {
      std::copy (result.bits.begin (), result.bits.end (), message77_out);
    }
  if (decoded_out)
    {
      fill_fixed_chars (decoded_out, kFt2DecodedChars, result.message_fixed);
    }
}

extern "C" void ftx_ft2_stage7_clravg_c ()
{
  Stage7State& state = stage7_state ();
  reset_average_state (state);
  clear_last_average_emit_state (state);
}

extern "C" int ftx_ft2_cpp_dsp_rollout_stage_c ()
{
  return 7;
}

extern "C" void ftx_ft2_cpp_dsp_rollout_stage_reset_c ()
{
}

extern "C" void ftx_ft2_cpp_dsp_rollout_stage_override_c (int)
{
}

extern "C" void ftx_ft2_emit_results_c (int* nutc, int* ncontest, int* nagain,
                                        char const* mycall, char const* mygrid,
                                        char const* temp_dir, float const* syncs,
                                        int const* snrs, float const* dts,
                                        float const* freqs, int const* naps,
                                        float const* quals, char const* decodeds,
                                        int* nout, int* decoded_count)
{
  if (!nutc || !ncontest || !nagain || !mycall || !mygrid || !temp_dir
      || !syncs || !snrs || !dts || !freqs || !naps || !quals || !decodeds || !nout)
    {
      if (decoded_count)
        {
          *decoded_count = 0;
        }
      return;
    }

  int const count = std::max (0, std::min (*nout, kFt2MaxLines));
  if (decoded_count)
    {
      *decoded_count = count;
    }

  std::string const temp_dir_path = trim_block (temp_dir, 500);
  std::string const decoded_path = temp_dir_path.empty ()
      ? "decoded.txt"
      : temp_dir_path + "/decoded.txt";
  std::string const avemsg_path = temp_dir_path.empty ()
      ? "avemsg.txt"
      : temp_dir_path + "/avemsg.txt";

  std::ofstream decoded_file {
      decoded_path,
      std::ios::out | (*nagain != 0 ? std::ios::app : std::ios::trunc)
  };
  std::ofstream avg_file {avemsg_path, std::ios::out | std::ios::trunc};

  std::string const mycall_trimmed = trim_block (mycall, 12);
  std::string const mygrid_trimmed = trim_block (mygrid, 6);
  bool const fox_mode = *ncontest == 6;
  Ft2EmitState& emit_state = ft2_emit_state ();
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
      current_n30 = update_ft2_n30_state (emit_state, *nutc);
    }

  for (int index = 0; index < count; ++index)
    {
      std::string decoded {decodeds + index * kFt2DecodedChars,
                           static_cast<size_t> (kFt2DecodedChars)};
      std::cout << format_ft2_stdout_line (*nutc, snrs[index], dts[index], freqs[index],
                                           decoded, naps[index], quals[index])
                << '\n';

      if (decoded_file.is_open ())
        {
          decoded_file << format_ft2_decoded_file_line (*nutc, syncs[index], snrs[index],
                                                        dts[index], freqs[index], decoded,
                                                        naps[index], quals[index])
                       << '\n';
        }

      if (fox_mode)
        {
          Ft2FoxEntry entry;
          if (should_collect_ft2_fox_entry (decoded, mycall_trimmed,
                                            static_cast<int> (std::lround (freqs[index])),
                                            entry))
            {
              entry.snr = snrs[index];
              entry.n30 = current_n30;
              emit_state.fox_entries.push_back (entry);
              if (static_cast<int> (emit_state.fox_entries.size ()) > kFt2FoxMax)
                {
                  emit_state.fox_entries.erase (emit_state.fox_entries.begin ());
                }
            }
        }
    }

  Stage7State& state = stage7_state ();
  if (avg_file.is_open ())
    {
      if (state.last_avg_decode_valid)
        {
          avg_file << "FT2 avg:  navg=" << std::setw (3) << state.last_avg_navg
                   << "  f=" << std::setw (5) << static_cast<int> (std::lround (state.last_avg_freq))
                   << "  dt=" << std::fixed << std::setprecision (2) << std::setw (6) << state.last_avg_dt
                   << "  " << trim_block (state.last_avg_decoded.data (), kFt2DecodedChars)
                   << '\n';
        }
      else if (state.navg > 0)
        {
          avg_file << "FT2 avg:  navg=" << std::setw (3) << state.navg
                   << "  f=" << std::setw (5) << static_cast<int> (std::lround (state.f_avg))
                   << " Hz  dt=" << std::fixed << std::setprecision (2) << std::setw (6) << state.dt_avg
                   << " s" << '\n';
        }
      avg_file.flush ();
    }
  clear_last_average_emit_state (state);

  std::cout.flush ();
  if (decoded_file.is_open ())
    {
      decoded_file.flush ();
    }
  if (fox_mode)
    {
      write_ft2_houndcallers_file (hounds_path, emit_state, mygrid_trimmed, current_n30);
    }
}

extern "C" void ftx_ft2_decode_and_emit_params_c (short const* iwave,
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

  if (params->nclearave)
    {
      ftx_ft2_stage7_clravg_c ();
    }

  int ncontest = params->nexp_decode & 7;
  std::array<float, kFt2MaxLines> syncs {};
  std::array<int, kFt2MaxLines> snrs {};
  std::array<float, kFt2MaxLines> dts {};
  std::array<float, kFt2MaxLines> freqs {};
  std::array<int, kFt2MaxLines> naps {};
  std::array<float, kFt2MaxLines> quals {};
  std::array<signed char, kFt2Bits * kFt2MaxLines> bits77 {};
  std::array<char, kFt2DecodedChars * kFt2MaxLines> decodeds {};
  int nout = 0;

  QString const my = qt_trimmed_string (reinterpret_cast<char const*> (params->mycall), 12);
  QString const his = qt_trimmed_string (reinterpret_cast<char const*> (params->hiscall), 12);
  decode_ft2_stage7 (iwave, params->nQSOProgress, params->nfqso, params->nfa, params->nfb,
                     params->ndepth, ncontest, my, his, syncs.data (), snrs.data (),
                     dts.data (), freqs.data (), naps.data (), quals.data (),
                     bits77.data (), decodeds.data (), &nout);

  int nutc = params->nutc;
  int nagain = params->nagain ? 1 : 0;
  ftx_ft2_emit_results_c (&nutc, &ncontest, &nagain,
                          reinterpret_cast<char const*> (params->mycall),
                          reinterpret_cast<char const*> (params->mygrid),
                          temp_dir, syncs.data (), snrs.data (), dts.data (), freqs.data (),
                          naps.data (), quals.data (), decodeds.data (), &nout, decoded_count);
}

extern "C" void ft2_triggered_decode_ (short iwave[], int* nqsoprogress, int* nfqso, int* nfa,
                                        int* nfb, int* ndepth, int* ncontest, char mycall[],
                                        char hiscall[], char outlines[], int* nout,
                                        size_t, size_t, size_t)
{
  std::array<int, kFt2MaxLines> snrs {};
  std::array<float, kFt2MaxLines> dts {};
  std::array<float, kFt2MaxLines> freqs {};
  std::array<int, kFt2MaxLines> naps {};
  std::array<float, kFt2MaxLines> quals {};
  std::array<signed char, kFt2Bits * kFt2MaxLines> bits77 {};
  std::array<char, kFt2DecodedChars * kFt2MaxLines> decodeds {};
  int local_nout = 0;

  ftx_ft2_async_decode_stage7_c (iwave, nqsoprogress, nfqso, nfa, nfb, ndepth, ncontest,
                                 mycall, hiscall, snrs.data (), dts.data (),
                                 freqs.data (), naps.data (), quals.data (),
                                 bits77.data (), decodeds.data (), &local_nout);

  if (outlines)
    {
      std::fill_n (outlines, kFt2MaxLines * kFt2OutlineChars, ' ');
      for (int i = 0; i < std::min (local_nout, kFt2MaxLines); ++i)
        {
          QByteArray const decoded_fixed {
            decodeds.data () + i * kFt2DecodedChars,
            kFt2DecodedChars
          };
          fill_outline_row (outlines + i * kFt2OutlineChars,
                            snrs[static_cast<size_t> (i)],
                            dts[static_cast<size_t> (i)],
                            freqs[static_cast<size_t> (i)],
                            decoded_fixed,
                            naps[static_cast<size_t> (i)]);
        }
    }

  if (nout)
    {
      *nout = std::min (local_nout, kFt2MaxLines);
    }
}

extern "C" void ft2_async_decode_ (short iwave[], int* nqsoprogress, int* nfqso, int* nfa,
                                    int* nfb, int* ndepth, int* ncontest, char mycall[],
                                    char hiscall[], int snrs[], float dts[], float freqs[],
                                    int naps[], float quals[], signed char bits77[],
                                    char decodeds[], int* nout, size_t, size_t, size_t)
{
  ftx_ft2_async_decode_stage7_c (iwave, nqsoprogress, nfqso, nfa, nfb, ndepth, ncontest,
                                 mycall, hiscall, snrs, dts, freqs, naps, quals,
                                 bits77, decodeds, nout);
}
