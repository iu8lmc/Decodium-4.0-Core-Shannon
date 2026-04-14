#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <fftw3.h>

extern "C" void ftx_sync8d_c (std::complex<float> const* cd0, int np, int i0,
                               std::complex<float> const* ctwk, int itwk, float* sync);
extern "C" void ftx_twkfreq1_c (fftwf_complex const* ca, int const* npts, float const* fsample,
                                float const* a, fftwf_complex* cb);
extern "C" void ftx_ft8var_chkgrid_c (char const callsign[12], char const grid[12],
                                      int* lchkcall_out, int* lgvalid_out,
                                      int* lwrongcall_out);

namespace
{
constexpr float kTiny = 1.0e-30f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr int kFt8PeakupNp2 = 2812;
constexpr int kFt8A8Nfft = 3200;
constexpr int kFt8A8Nh = kFt8A8Nfft / 2;
constexpr float kFt8A8Df = 200.0f / static_cast<float> (kFt8A8Nfft);
constexpr float kFt8A8Dt = 1.0f / 200.0f;
using Complex = std::complex<float>;

struct Ft8VarDatacorState
{
  std::array<float, 64 * 63> s3 {};
  std::array<int, 63> correct {};
  bool initialized {false};
};

Ft8VarDatacorState& ft8var_datacor_state ()
{
  static Ft8VarDatacorState state;
  return state;
}

constexpr std::array<int, 29> kMcqRaw {{
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0
}};
constexpr std::array<int, 29> kMcqRuRaw {{
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,1,1,0,0,1,1,0,0
}};
constexpr std::array<int, 29> kMcqFdRaw {{
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,0,1,0,0,0,1,0
}};
constexpr std::array<int, 29> kMcqTestRaw {{
  0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,0,1,0,1,1,1,1,1,1,0,0,1,0
}};
constexpr std::array<int, 29> kMcqWwRaw {{
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,1,1,1,1,0
}};
constexpr std::array<int, 19> kRrrRaw {{
  0,1,1,1,1,1,1,0,1,0,0,1,0,0,1,0,0,0,1
}};
constexpr std::array<int, 19> k73Raw {{
  0,1,1,1,1,1,1,0,1,0,0,1,0,1,0,0,0,0,1
}};
constexpr std::array<int, 19> kRr73Raw {{
  0,1,1,1,1,1,1,0,0,1,1,1,0,1,0,1,0,0,1
}};
constexpr std::array<int, 6> kApPassCounts {{2, 2, 2, 4, 4, 3}};
constexpr std::array<std::array<int, 4>, 6> kApTypes {{
  std::array<int, 4> {{1, 2, 0, 0}},
  std::array<int, 4> {{2, 3, 0, 0}},
  std::array<int, 4> {{2, 3, 0, 0}},
  std::array<int, 4> {{3, 4, 5, 6}},
  std::array<int, 4> {{3, 4, 5, 6}},
  std::array<int, 4> {{3, 1, 2, 0}}
}};

struct Ft8A8SearchFft
{
  Ft8A8SearchFft ()
    : data (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * kFt8A8Nfft))),
      forward (nullptr)
  {
    if (data)
      {
        forward = fftwf_plan_dft_1d (kFt8A8Nfft, data, data, FFTW_FORWARD, FFTW_ESTIMATE);
      }
  }

  ~Ft8A8SearchFft ()
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

  fftwf_complex* data;
  fftwf_plan forward;
};

Ft8A8SearchFft& ft8_a8_fft ()
{
  thread_local Ft8A8SearchFft instance;
  return instance;
}

inline void set_value_range (float* dst, int first, int last, float value)
{
  for (int i = first; i <= last; ++i)
    {
      dst[i - 1] = value;
    }
}

inline void set_mask_range (int* dst, int first, int last)
{
  for (int i = first; i <= last; ++i)
    {
      dst[i - 1] = 1;
    }
}

template <size_t N>
void copy_raw_pm1 (float* dst, int first, std::array<int, N> const& raw, float scale)
{
  for (size_t i = 0; i < N; ++i)
    {
      dst[first - 1 + static_cast<int> (i)] = scale * (raw[i] != 0 ? 1.0f : -1.0f);
    }
}

void copy_scaled (float* dst, int first, int const* src, int count, float scale)
{
  for (int i = 0; i < count; ++i)
    {
      dst[first - 1 + i] = scale * static_cast<float> (src[i]);
    }
}

float db_like (float value)
{
  if (value > 1.259e-10f)
    {
      return 10.0f * std::log10 (value);
    }
  return -99.0f;
}

void smooth_121_in_place (float* values, int size)
{
  if (!values || size < 3)
    {
      return;
    }

  float x0 = values[0];
  for (int i = 1; i < size - 1; ++i)
    {
      float const x1 = values[i];
      values[i] = 0.5f * values[i] + 0.25f * (x0 + values[i + 1]);
      x0 = x1;
    }
}

float compute_ft8_xbase (float f1_in, float const* sbase, int sbase_size)
{
  if (!sbase || sbase_size <= 0)
    {
      return 1.0f;
    }

  int index = static_cast<int> (std::lround (f1_in / 3.125f));
  index = std::max (1, std::min (index, sbase_size));
  return std::pow (10.0f, 0.1f * (sbase[index - 1] - 40.0f));
}

float compute_ft8_snr_legacy_like (float const* s8, int rows, int cols, int const* itone)
{
  float xsnr_sum = 0.001f;
  for (int i = 0; i < cols; ++i)
    {
      int const target = std::max (0, std::min (itone[i], rows - 1));
      float const xsig = s8[target + rows * i] * s8[target + rows * i];

      float total_power = 0.0f;
      for (int row = 0; row < rows; ++row)
        {
          float const value = s8[row + rows * i];
          total_power += value * value;
        }

      float xnoi = 0.01f;
      if (rows > 1)
        {
          xnoi = (total_power - xsig) / static_cast<float> (rows - 1);
          if (xnoi < 0.01f)
            {
              xnoi = 0.01f;
            }
        }

      float xsnr_symbol = 1.01f;
      if (xnoi < xsig)
        {
          xsnr_symbol = xsig / xnoi;
        }
      xsnr_sum += xsnr_symbol;
    }

  float xsnr = xsnr_sum / static_cast<float> (cols) - 1.0f;
  xsnr = 10.0f * std::log10 (std::max (xsnr, 0.001f)) - 26.5f;
  if (xsnr > 7.0f)
    {
      xsnr += (xsnr - 7.0f) * 0.5f;
    }
  if (xsnr > 30.0f)
    {
      xsnr -= 1.0f;
      if (xsnr > 40.0f)
        {
          xsnr -= 1.0f;
        }
      if (xsnr > 49.0f)
        {
          xsnr = 49.0f;
        }
    }
  return xsnr;
}

std::string trim_fixed (char const* data, int length)
{
  if (!data || length <= 0)
    {
      return {};
    }

  std::string value (data, data + length);
  size_t begin = 0;
  while (begin < value.size () && (value[begin] == ' ' || value[begin] == '\0'))
    {
      ++begin;
    }

  size_t end = value.size ();
  while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\0'))
    {
      --end;
    }
  return value.substr (begin, end - begin);
}

void fill_fixed (char* out, int length, std::string const& value)
{
  if (!out || length <= 0)
    {
      return;
    }

  std::fill_n (out, length, ' ');
  std::copy_n (value.data (), std::min (static_cast<int>(length), static_cast<int>(static_cast<int> (value.size ()))), out);
}

size_t find_fixed_space (std::string const& value, size_t start)
{
  size_t const pos = value.find (' ', start);
  return pos == std::string::npos ? value.size () : pos;
}

std::string to_ascii_upper (std::string value)
{
  for (char& ch : value)
    {
      ch = static_cast<char> (std::toupper (static_cast<unsigned char> (ch)));
    }
  return value;
}

int ft8_callsign_bucket (char first)
{
  if (first > '0' && first < 'D') return 0;
  if (first > 'C' && first < 'E') return 1;
  if (first > 'D' && first < 'G') return 2;
  if (first > 'F' && first < 'I') return 3;
  if (first > 'H' && first < 'J') return 4;
  if (first > 'I' && first < 'K') return 5;
  if (first > 'J' && first < 'L') return 6;
  if (first > 'K' && first < 'N') return 7;
  if (first > 'M' && first < 'O') return 8;
  if (first > 'N' && first < 'P') return 9;
  if (first > 'O' && first < 'R') return 10;
  if (first > 'Q' && first < 'S') return 11;
  if (first > 'R' && first < 'U') return 12;
  if (first > 'T' && first < 'W') return 13;
  if (first > 'V' && first < 'X') return 14;
  if (first > 'W' && first <= 'Z') return 15;
  return -1;
}

struct Ft8AllCallDb
{
  bool valid = false;
  std::unordered_set<std::string> calls;
};

Ft8AllCallDb load_ft8_allcall_db ()
{
  Ft8AllCallDb db;
  std::array<int, 16> bucket_counts {};
  std::array<char const*, 4> const candidates {{
      "ALLCALL7.TXT",
      "../ALLCALL7.TXT",
      "../../ALLCALL7.TXT",
      "../../../ALLCALL7.TXT"
    }};

  std::ifstream input;
  for (char const* path : candidates)
    {
      input.open (path);
      if (input.is_open ())
        {
          break;
        }
      input.clear ();
    }

  if (!input.is_open ())
    {
      return db;
    }

  std::string line;
  while (std::getline (input, line))
    {
      if (line.rfind ("ZZZZ", 0) == 0)
        {
          break;
        }
      if (line.rfind ("//", 0) == 0)
        {
          continue;
        }

      size_t const comma = line.find (',');
      if (comma < 3 || comma > 7)
        {
          continue;
        }

      std::string callsign = to_ascii_upper (trim_fixed (line.data (), static_cast<int> (comma)));
      if (callsign.empty ())
        {
          continue;
        }

      db.calls.insert (callsign);
      int const bucket = ft8_callsign_bucket (callsign.front ());
      if (bucket >= 0)
        {
          ++bucket_counts[static_cast<size_t> (bucket)];
        }
    }

  db.valid = !db.calls.empty ()
             && std::all_of (bucket_counts.begin (), bucket_counts.end (),
                             [] (int count) { return count > 0; });
  return db;
}

Ft8AllCallDb const& ft8_allcall_db ()
{
  static Ft8AllCallDb const db = load_ft8_allcall_db ();
  return db;
}

bool ft8var_searchcalls_impl (std::string callsign1, std::string callsign2)
{
  Ft8AllCallDb const& db = ft8_allcall_db ();
  if (!db.valid)
    {
      return true;
    }

  callsign1 = to_ascii_upper (trim_fixed (callsign1.data (), static_cast<int> (callsign1.size ())));
  callsign2 = to_ascii_upper (trim_fixed (callsign2.data (), static_cast<int> (callsign2.size ())));

  if (callsign1.size () > 7 && callsign2.empty ())
    {
      return true;
    }
  if (callsign2.size () > 7)
    {
      callsign2.clear ();
    }

  std::array<std::string, 2> const calls {{callsign1, callsign2}};
  int const cycles = callsign2.empty () ? 1 : 2;
  for (int i = 0; i < cycles; ++i)
    {
      if (calls[static_cast<size_t> (i)].empty () || calls[static_cast<size_t> (i)] == "TU;")
        {
          continue;
        }
      if (db.calls.find (calls[static_cast<size_t> (i)]) != db.calls.end ())
        {
          return true;
        }
    }
  return false;
}

bool ft8var_chkflscall_impl (std::string call_a, std::string call_b)
{
  call_a = to_ascii_upper (trim_fixed (call_a.data (), static_cast<int> (call_a.size ())));
  call_b = to_ascii_upper (trim_fixed (call_b.data (), static_cast<int> (call_b.size ())));

  if (call_a.rfind ("<.", 0) == 0)
    {
      return false;
    }

  bool lfound = false;
  if (call_a == "MYCALL" || call_a == "CQ")
    {
      lfound = ft8var_searchcalls_impl (call_b, {});
    }
  else if (!call_b.empty () && call_b.front () == '<')
    {
      lfound = ft8var_searchcalls_impl (call_a, {});
    }
  else
    {
      lfound = ft8var_searchcalls_impl (call_a, call_b);
    }

  return !lfound;
}

bool starts_with_two_digits (std::string const& value)
{
  return value.size () >= 2
         && value[0] >= '0' && value[0] <= '9'
         && value[1] >= '0' && value[1] <= '9';
}

bool is_ascii_digit (char ch)
{
  return ch >= '0' && ch <= '9';
}

bool is_ascii_upper (char ch)
{
  return ch >= 'A' && ch <= 'Z';
}

bool ft8var_chkspecial_impl (std::string& msg37, std::string& msg37_2,
                             std::string const& mycall, std::string const& hiscall)
{
  size_t const ispc1 = find_fixed_space (msg37, 0);
  size_t const ispc2 = ispc1 < msg37.size () ? find_fixed_space (msg37, ispc1 + 1) : msg37.size ();
  size_t const ispc12 = find_fixed_space (msg37_2, 0);
  if (ispc1 <= 2 || ispc2 <= 6)
    {
      return false;
    }

  std::string const call_a = to_ascii_upper (trim_fixed (msg37.data (), static_cast<int> (ispc1)));
  std::string const call_b = to_ascii_upper (trim_fixed (msg37_2.data (), static_cast<int> (ispc12)));
  std::string const call_c = ispc2 > ispc1 + 1
                                 ? to_ascii_upper (trim_fixed (msg37.data () + ispc1 + 1,
                                                               static_cast<int> (ispc2 - ispc1 - 1)))
                                 : std::string {};
  std::string const mycall_norm = to_ascii_upper (trim_fixed (mycall.data (), static_cast<int> (mycall.size ())));
  std::string const hiscall_norm = to_ascii_upper (trim_fixed (hiscall.data (), static_cast<int> (hiscall.size ())));

  if (call_a != mycall_norm && call_b != mycall_norm && call_c != hiscall_norm)
    {
      bool const invalid_prefix = (!call_a.empty () && (call_a.front () == 'Q' || call_a.front () == '0'))
                                  || (!call_b.empty () && (call_b.front () == 'Q' || call_b.front () == '0'))
                                  || starts_with_two_digits (call_a)
                                  || starts_with_two_digits (call_b);
      if (invalid_prefix || ft8var_chkflscall_impl (call_a, call_b))
        {
          msg37.assign (37, ' ');
          msg37_2.assign (37, ' ');
          return true;
        }
    }

  return false;
}

bool ft8var_chklong8_impl (std::string callsign)
{
  static std::array<std::string, 9> const mask10 {{
      "0011000000", "0011100000", "0011110000",
      "1011000000", "1011100000", "1011110000",
      "0010000000", "1010000000", "0110000000"
    }};
  static std::array<std::string, 7> const mask11 {{
      "00111000000", "00111100000", "10111000000", "10111100000",
      "00100000000", "01100000000", "10100000000"
    }};

  callsign = to_ascii_upper (trim_fixed (callsign.data (), static_cast<int> (callsign.size ())));
  if (callsign.find ('/') != std::string::npos || callsign.rfind ("8J", 0) == 0 || callsign.rfind ("8N", 0) == 0)
    {
      return false;
    }

  auto build_mask = [] (std::string const& value) {
    std::string mask;
    mask.reserve (value.size ());
    for (char ch : value)
      {
        mask.push_back (is_ascii_digit (ch) ? '1' : '0');
      }
    return mask;
  };

  if (callsign.size () == 10)
    {
      std::string const mask = build_mask (callsign);
      return std::find (mask10.begin (), mask10.end (), mask) == mask10.end ();
    }
  if (callsign.size () == 11)
    {
      std::string const mask = build_mask (callsign);
      return std::find (mask11.begin (), mask11.end (), mask) == mask11.end ();
    }

  return false;
}

std::string ft8var_extract_call_impl (std::string const& message)
{
  size_t const ispc1 = find_fixed_space (message, 0);
  size_t const ispc2 = ispc1 < message.size () ? find_fixed_space (message, ispc1 + 1) : message.size ();
  size_t const ispc3 = ispc2 < message.size () ? find_fixed_space (message, ispc2 + 1) : message.size ();
  std::string const part2 = ispc1 < message.size () && ispc2 > ispc1
                                ? trim_fixed (message.data () + ispc1 + 1, static_cast<int> (ispc2 - ispc1 - 1))
                                : std::string {};
  std::string const part3 = ispc2 < message.size () && ispc3 > ispc2
                                ? trim_fixed (message.data () + ispc2 + 1, static_cast<int> (ispc3 - ispc2 - 1))
                                : std::string {};
  int const npart2len = static_cast<int> (part2.size ());

  if (message.rfind ("CQ ", 0) == 0 || message.rfind ("DE ", 0) == 0 || message.rfind ("QRZ ", 0) == 0)
    {
      if (npart2len > 4)
        {
          return part2;
        }
      if (npart2len == 4)
        {
          if ((part2.size () > 1 && is_ascii_digit (part2[1]))
              || (part2.size () > 2 && is_ascii_digit (part2[2])))
            {
              return part2;
            }
          return part3;
        }
      if (npart2len == 3)
        {
          if (is_ascii_upper (part2[0]) && is_ascii_digit (part2[1]))
            {
              return part2;
            }
          return part3;
        }
      if (npart2len == 2)
        {
          return part3;
        }
      return {};
    }

  if (ispc1 > 2 && ispc1 < 12)
    {
      return part2;
    }
  return {};
}

float ft8var_datacor_impl ()
{
  auto const& state = ft8var_datacor_state ();
  if (!state.initialized)
    {
      // The active FT8 stage-4 path no longer feeds the legacy Fortran
      // symbol-spectra cache. When no caller provides explicit spectra
      // through the bridge below, keep the historical datacor gate open.
      return 2.0f;
    }

  auto s3_at = [] (int row_1based, int col_1based) {
    int const row = std::max (1, std::min (row_1based, 64)) - 1;
    int const col = std::max (1, std::min (col_1based, 63)) - 1;
    return ft8var_datacor_state ().s3[static_cast<size_t> (row + 64 * col)];
  };

  float datapwr = 0.0f;
  for (int i = 1; i <= 63; ++i)
    {
      datapwr += s3_at (state.correct[static_cast<size_t> (i - 1)] + 1, i);
    }

  float timeplus = 0.0f;
  float timeminus = 0.0f;
  float freqplus = 0.0f;
  float freqminus = 0.0f;
  for (int i = 1; i <= 63; ++i)
    {
      int k = i + 1;
      int m = i - 1;
      if (i == 1)
        {
          m = 2;
        }
      if (i == 63)
        {
          k = 62;
        }

      int const correct = state.correct[static_cast<size_t> (i - 1)];
      timeplus += s3_at (correct + 1, k);
      timeminus += s3_at (correct + 1, m);
      int const j = correct < 63 ? 1 : -1;
      int const n = correct > 1 ? -1 : 1;
      freqplus += s3_at (correct + 1 + j, i);
      freqminus += s3_at (correct + 1 + n, i);
    }

  float const denom = timeplus + timeminus + freqplus + freqminus;
  if (denom <= 0.0f)
    {
      return 0.0f;
    }
  return datapwr * 4.0f / denom;
}

bool ft8var_filtersfree_impl (std::string const& decoded_raw)
{
  std::string decoded (22, ' ');
  std::copy_n (decoded_raw.data (),
               std::min (static_cast<int>(static_cast<int>(decoded.size ())), static_cast<int>(static_cast<int>(decoded_raw.size ()))),
               decoded.begin ());

  auto char_at = [&decoded] (int index) {
    if (index < 1 || index > static_cast<int> (decoded.size ()))
      {
        return ' ';
      }
    return decoded[static_cast<size_t> (index - 1)];
  };

  auto slice = [&char_at] (int first, int last) {
    std::string out;
    out.reserve (std::max (0, last - first + 1));
    for (int i = first; i <= last; ++i)
      {
        out.push_back (char_at (i));
      }
    return out;
  };

  int ndot = 0;
  int nsign = 0;
  int nother = 0;

  if (slice (11, 12) == "73" || slice (12, 13) == "73")
    {
      return false;
    }
  if (char_at (1) == '/')
    {
      return true;
    }

  for (int i = 1; i <= 13; ++i)
    {
      std::string const d1 = slice (i, i);
      std::string const d2 = slice (i, i + 1);
      std::string const d3 = slice (i, i + 2);
      std::string const d4 = slice (i, i + 3);
      std::string const d5 = slice (i, i + 4);

      if (d2 == "73" || d2 == "TU" || d2 == "GL")
        {
          return false;
        }
      if (i > 4)
        {
          if ((d2 == " -" || d2 == " +") && char_at (i + 2) >= '0' && char_at (i + 2) < '3')
            {
              return false;
            }
          if ((d3 == " R-" || d3 == " R+") && char_at (i + 3) >= '0' && char_at (i + 3) < '3')
            {
              return false;
            }
        }
      if (d3 == "QSL" || d3 == "TNX" || d2 == "RR"
          || d3 == "QSO" || d3 == "CFM" || d4 == "LOTW"
          || d3 == "BND" || d3 == "QSY" || d4 == "BAND")
        {
          return false;
        }
      if (d3 == "MAS" || d3 == "HNY" || d3 == "HPY" || d3 == " DT"
          || d3 == "XMS" || d5 == "EASTE" || d5 == "PIRAT" || d4 == "TEST")
        {
          return false;
        }
      if (d5 == "/HYBR" || d5 == "/HTML" || d5 == "/PHOT" || d5 == "/IMAG" || d5 == "/JOIN"
          || d5 == "QRZ.C" || d4 == "/GIF" || d4 == "/JPG" || d4 == "/AVI"
          || d4 == "/MP4" || d4 == "/CMD")
        {
          return false;
        }
      if (d1 == ".")
        {
          ++ndot;
        }
      if (d1 == "-" || d1 == "+" || d1 == "?")
        {
          ++nsign;
        }
      if (d1 == "/" || d1 == "?")
        {
          ++nother;
        }
    }

  if (ndot >= 2 || nsign >= 2 || nother >= 2)
    {
      return true;
    }
  if (slice (1, 2) == "-0")
    {
      return true;
    }
  if (char_at (1) != ' ' && char_at (2) == ' ')
    {
      return true;
    }

  if (((char_at (13) >= 'A' && char_at (13) <= 'Z')
       || (char_at (13) >= '0' && char_at (13) <= '9'))
      && char_at (12) == ' ')
    {
      return true;
    }

  if (char_at (1) == '.' || char_at (1) == '+' || char_at (1) == '?'
      || char_at (1) == '/' || char_at (13) == '.' || char_at (13) == '+'
      || char_at (13) == '-' || char_at (13) == '/')
    {
      return true;
    }

  if (char_at (2) == '-' || char_at (2) == '+'
      || char_at (12) == '.' || char_at (12) == '+'
      || char_at (12) == '-')
    {
      return true;
    }

  if (char_at (12) >= '0' && char_at (12) <= '9'
      && char_at (13) >= '0' && char_at (13) <= '9')
    {
      std::string const tail = slice (12, 13);
      if (tail != "55" && tail != "73" && tail != "88")
        {
          return true;
        }
    }

  if (char_at (1) == '0' && char_at (2) != '.')
    {
      return true;
    }

  for (int i = 1; i <= 12; ++i)
    {
      if (i < 12 && char_at (i) == '?' && char_at (i + 1) != ' ')
        {
          return true;
        }
      if (i < 12 && char_at (i) != ' ' && char_at (i + 1) == '+'
          && (char_at (i + 2) <= '0' || char_at (i + 2) >= '9'))
        {
          return true;
        }

      if (char_at (i) >= 'A' && char_at (i) <= 'Z'
          && char_at (i + 1) == '.'
          && char_at (i + 2) >= 'A' && char_at (i + 2) <= 'Z')
        {
          return true;
        }

      if (slice (i + 1, i + 2) == "/ " || slice (i + 1, i + 2) == " /")
        {
          return true;
        }

      if (char_at (i) >= '0' && char_at (i) <= '9'
          && char_at (i + 1) == '/'
          && char_at (i + 2) >= '0' && char_at (i + 2) <= '9')
        {
          return true;
        }

      if (char_at (i) == ' '
          && char_at (i + 1) >= 'A' && char_at (i + 1) <= 'Z'
          && char_at (i + 1) != 'F' && char_at (i + 1) != 'G' && char_at (i + 1) != 'I'
          && char_at (i + 2) == '/')
        {
          return true;
        }

      if (char_at (i) == ' '
          && char_at (i + 1) >= 'A' && char_at (i + 1) <= 'Z'
          && (char_at (i + 2) == '.' || char_at (i + 2) == '+'
              || char_at (i + 2) == '-'))
        {
          return true;
        }
    }

  if (char_at (1) == '-' && char_at (2) <= '1' && char_at (2) >= '9')
    {
      return true;
    }

  if (char_at (1) >= '1' && char_at (1) <= '9'
      && char_at (2) >= '0' && char_at (2) <= '9'
      && char_at (3) >= '0' && char_at (3) <= '9'
      && char_at (4) != 'W')
    {
      return true;
    }

  if (char_at (1) >= '1' && char_at (1) <= '9'
      && char_at (2) >= '0' && char_at (2) <= '9'
      && char_at (3) != 'W')
    {
      return true;
    }

  if (char_at (1) >= '1' && char_at (1) <= '9' && slice (2, 3) != "EL")
    {
      return true;
    }

  if (char_at (10) != '/' && char_at (11) != '/'
      && char_at (12) >= 'A' && char_at (12) <= 'Z'
      && char_at (13) >= '0' && char_at (12) <= '9')
    {
      return true;
    }

  if (ft8var_datacor_impl () < 1.55f)
    {
      return true;
    }

  int ncount = 0;
  int nchar = 0;
  for (int i = 1; i <= 13; ++i)
    {
      if (char_at (i) == ' ')
        {
          break;
        }
      if (char_at (i) == '/' || char_at (i) == '-' || char_at (i) == '.')
        {
          ++nchar;
        }
      ++ncount;
    }

  if (ncount == 13 && nchar >= 0)
    {
      return true;
    }

  return false;
}

bool ft8_invalid_callsign_shape (std::string callsign)
{
  callsign = to_ascii_upper (trim_fixed (callsign.data (), static_cast<int> (callsign.size ())));
  auto char_at = [&callsign] (size_t index) {
    return index < callsign.size () ? callsign[index] : ' ';
  };

  char const c1 = char_at (0);
  if (c1 == 'Q' || c1 == '0' || c1 == '/')
    {
      return true;
    }

  if (is_ascii_digit (c1))
    {
      char const c2 = char_at (1);
      char const c3 = char_at (2);
      char const c4 = char_at (3);
      if (is_ascii_digit (c2))
        {
          return true;
        }
      if (is_ascii_upper (c2) && is_ascii_upper (c3) && is_ascii_upper (c4))
        {
          return true;
        }
    }

  if (is_ascii_upper (c1))
    {
      char const c2 = char_at (1);
      char const c3 = char_at (2);
      if (is_ascii_upper (c2) && is_ascii_upper (c3))
        {
          return true;
        }
    }

  if (is_ascii_digit (c1))
    {
      char const c2 = char_at (1);
      char const c3 = char_at (2);
      if (is_ascii_upper (c2) && is_ascii_upper (c3))
        {
          std::string const prefix = callsign.substr (0, std::min<size_t> (3, callsign.size ()));
          if (prefix != "3DA" && prefix != "3XY")
            {
              return true;
            }
        }
    }

  return false;
}

bool ft8_invalid_call_pair_shape (std::string call_a, std::string call_b)
{
  call_a = to_ascii_upper (trim_fixed (call_a.data (), static_cast<int> (call_a.size ())));
  call_b = to_ascii_upper (trim_fixed (call_b.data (), static_cast<int> (call_b.size ())));
  auto char_at = [] (std::string const& value, size_t index) {
    return index < value.size () ? value[index] : ' ';
  };

  if (char_at (call_a, 0) == 'Q' || char_at (call_b, 0) == 'Q')
    {
      return true;
    }
  if (char_at (call_a, 0) == '0' || char_at (call_b, 0) == '0')
    {
      return true;
    }
  if (is_ascii_digit (char_at (call_a, 0)) && is_ascii_digit (char_at (call_a, 1)))
    {
      return true;
    }
  if (is_ascii_digit (char_at (call_b, 0)) && is_ascii_digit (char_at (call_b, 1)))
    {
      return true;
    }
  return false;
}

bool ft8_is_grid_token (std::string const& token)
{
  return token.size () == 4
         && token[0] > '@' && token[0] < 'S'
         && token[1] > '@' && token[1] < 'S'
         && token[2] < ':'
         && token[3] < ':';
}

void ft8var_chkgrid_bridge (std::string const& callsign, std::string const& grid,
                            bool& lchkcall, bool& lgvalid, bool& lwrongcall)
{
  char callsign_c[12];
  char grid_c[12];
  int lchkcall_out = 0;
  int lgvalid_out = 0;
  int lwrongcall_out = 0;
  fill_fixed (callsign_c, 12, callsign);
  fill_fixed (grid_c, 12, grid);
  ftx_ft8var_chkgrid_c (callsign_c, grid_c, &lchkcall_out, &lgvalid_out, &lwrongcall_out);
  lchkcall = lchkcall_out != 0;
  lgvalid = lgvalid_out != 0;
  lwrongcall = lwrongcall_out != 0;
}

void ft8var_chkfalse8_impl (std::string& msg37, int i3, int n3, int& nbadcrc, int iaptype,
                            bool lcall1hash, std::string const& mycall_in,
                            std::string const& hiscall_in, std::string const& hisgrid4_in)
{
  std::string const mycall = to_ascii_upper (trim_fixed (mycall_in.data (), static_cast<int> (mycall_in.size ())));
  std::string const hiscall = to_ascii_upper (trim_fixed (hiscall_in.data (), static_cast<int> (hiscall_in.size ())));
  std::string const hisgrid4 = to_ascii_upper (trim_fixed (hisgrid4_in.data (), static_cast<int> (hisgrid4_in.size ())));

  auto invalidate = [&] {
    nbadcrc = 1;
    msg37.assign (37, ' ');
  };
  auto token_between = [&msg37] (size_t start, size_t end) {
    if (start >= msg37.size () || end <= start)
      {
        return std::string {};
      }
    return to_ascii_upper (trim_fixed (msg37.data () + start, static_cast<int> (end - start)));
  };
  auto token_after = [&msg37] (size_t start) {
    if (start >= msg37.size ())
      {
        return std::string {};
      }
    return to_ascii_upper (trim_fixed (msg37.data () + start, static_cast<int> (msg37.size () - start)));
  };
  auto maybe_grid_token = [&msg37] (size_t start, size_t end) {
    if (end <= start)
      {
        return std::string {};
      }
    std::string const token = msg37.substr (start, end - start);
    return ft8_is_grid_token (token) ? token : std::string {};
  };

  msg37.resize (37, ' ');

  if (msg37.rfind ("CQ ", 0) == 0)
    {
      std::string call_a;
      std::string call_b;
      std::string callsign;
      std::string grid;
      std::string callmask6;
      size_t const ispc1 = find_fixed_space (msg37, 0);
      size_t const ispc2 = ispc1 < msg37.size () ? find_fixed_space (msg37, ispc1 + 1) : msg37.size ();

      if (i3 == 4)
        {
          size_t const islash_pos = msg37.find ('/');
          int const islash = islash_pos == std::string::npos ? -1 : static_cast<int> (islash_pos);
          if (islash < 0)
            {
              callsign = token_after (ispc1 + 1);
              if (callsign.empty ())
                {
                  return;
                }
              size_t const ispccall = callsign.find (' ');
              if (ispccall < callsign.size () || is_ascii_digit (callsign.back ()))
                {
                  invalidate ();
                  return;
                }
            }
          else if (static_cast<size_t> (islash) > ispc1 && static_cast<size_t> (islash) < ispc2)
            {
              if (static_cast<size_t> (islash) - ispc1 <= ispc2 - static_cast<size_t> (islash))
                {
                  callsign = token_between (static_cast<size_t> (islash) + 1, ispc2);
                }
              else
                {
                  callsign = token_between (ispc1 + 1, static_cast<size_t> (islash));
                }
            }

          if (ft8_invalid_callsign_shape (callsign))
            {
              invalidate ();
              return;
            }

          if (islash >= 0)
            {
              if (ft8var_chkflscall_impl ("CQ", callsign))
                {
                  invalidate ();
                  return;
                }
            }
          else
            {
              int const nlen = static_cast<int> (callsign.size ());
              if (nlen >= 10)
                {
                  if (ft8var_chklong8_impl (callsign))
                    {
                      invalidate ();
                      return;
                    }
                }
              else if (nlen == 6 && callsign.find ('/') == std::string::npos)
                {
                  callmask6.reserve (6);
                  for (int i = 0; i < 6; ++i)
                    {
                      char const ch = callsign[static_cast<size_t> (i)];
                      callmask6.push_back (ch > '/' && ch < ':' ? '1' : '0');
                    }
                  if (callmask6 == "001000" || callmask6 == "101000" || callmask6 == "011000")
                    {
                      invalidate ();
                      return;
                    }
                }
            }
        }
      else if (iaptype == 1)
        {
          size_t const ispc3 = ispc2 < msg37.size () ? find_fixed_space (msg37, ispc2 + 1) : msg37.size ();
          if (ispc2 >= 2 && (msg37.substr (ispc2 - 2, 2) == "/R" || msg37.substr (ispc2 - 2, 2) == "/P"))
            {
              callsign = token_between (ispc1 + 1, ispc2 - 2);
            }
          else
            {
              callsign = token_between (ispc1 + 1, ispc2);
            }
          if (ft8_invalid_callsign_shape (callsign))
            {
              invalidate ();
              return;
            }
          grid = maybe_grid_token (ispc2 + 1, ispc3);
          if (grid.size () == 4)
            {
              bool lchkcall = false;
              bool lgvalid = false;
              bool lwrongcall = false;
              ft8var_chkgrid_bridge (callsign, grid, lchkcall, lgvalid, lwrongcall);
              if (lwrongcall || !lgvalid)
                {
                  invalidate ();
                  return;
                }
            }
        }

      if (i3 == 4)
        {
          callsign = token_after (3);
          size_t const islash_pos = callsign.find ('/');
          size_t const islash2_pos = islash_pos == std::string::npos ? std::string::npos : callsign.find ('/', islash_pos + 1);
          int const islash = islash_pos == std::string::npos ? -1 : static_cast<int> (islash_pos) + 1;
          int const islash2 = islash2_pos == std::string::npos ? -1 : static_cast<int> (islash2_pos - islash_pos);
          int const nlencall = static_cast<int> (callsign.size ());
          if (nlencall == 11 && islash > 0 && islash2 < 1)
            {
              if (islash < 7)
                {
                  if (((callsign[static_cast<size_t> (islash)] < ':'
                        && callsign[static_cast<size_t> (islash + 1)] < ':')
                       || callsign[static_cast<size_t> (islash)] == 'Q'
                       || is_ascii_digit (callsign.back ())))
                    {
                      invalidate ();
                      return;
                    }
                }
              if (islash > 5)
                {
                  if (is_ascii_digit (callsign[static_cast<size_t> (islash - 2)])
                      || (is_ascii_digit (callsign[0]) && is_ascii_digit (callsign[1]))
                      || callsign[0] == 'Q')
                    {
                      invalidate ();
                      return;
                    }
                }
              if (islash > 4 && islash < 8)
                {
                  call_a = callsign.substr (0, static_cast<size_t> (islash - 1));
                  call_b = callsign.substr (static_cast<size_t> (islash));
                  if (ft8var_chkflscall_impl (call_a, call_b))
                    {
                      invalidate ();
                      return;
                    }
                }
            }
        }

      if (msg37[3] > '/' && msg37[3] < ':' && msg37[6] == ' ' && (i3 == 1 || i3 == 2))
        {
          size_t const islash_pos = msg37.find ('/');
          int const islash = islash_pos == std::string::npos ? -1 : static_cast<int> (islash_pos) + 1;
          if (islash > 10 && islash < 15)
            {
              callsign = token_between (7, static_cast<size_t> (islash - 1));
              grid = msg37.substr (static_cast<size_t> (islash + 2), 4);
              if (ft8_is_grid_token (grid))
                {
                  bool lchkcall = false;
                  bool lgvalid = false;
                  bool lwrongcall = false;
                  ft8var_chkgrid_bridge (callsign, grid, lchkcall, lgvalid, lwrongcall);
                  if (lwrongcall || !lgvalid)
                    {
                      invalidate ();
                      return;
                    }
                }
              if (ft8var_chkflscall_impl ("MYCALL", callsign))
                {
                  invalidate ();
                  return;
                }
            }
        }

      if (i3 == 1 && iaptype == 0)
        {
          int islash = -1;
          size_t const islash_r = msg37.find ("/R ");
          if (islash_r != std::string::npos)
            {
              islash = static_cast<int> (islash_r) + 1;
              callsign = token_between (3, static_cast<size_t> (islash - 1));
              grid = msg37.substr (static_cast<size_t> (islash + 2), 4);
            }
          else
            {
              size_t const ispc3 = ispc2 < msg37.size () ? find_fixed_space (msg37, ispc2 + 1) : msg37.size ();
              if (trim_fixed (msg37.data (), static_cast<int> (msg37.size ())).size () == ispc3)
                {
                  callsign = token_between (ispc1 + 1, ispc2);
                  grid = token_between (ispc2 + 1, ispc3);
                }
              else
                {
                  return;
                }
            }

          if (ft8_is_grid_token (grid))
            {
              bool lchkcall = false;
              bool lgvalid = false;
              bool lwrongcall = false;
              ft8var_chkgrid_bridge (callsign, grid, lchkcall, lgvalid, lwrongcall);
              if (lwrongcall || !lgvalid)
                {
                  invalidate ();
                  return;
                }
            }

          if (islash > 6 && ft8var_chkflscall_impl ("MYCALL", callsign))
            {
              invalidate ();
              return;
            }
        }

      return;
    }

  if (iaptype == 2)
    {
      size_t const ispc1 = find_fixed_space (msg37, 0);
      size_t const ispc2 = ispc1 < msg37.size () ? find_fixed_space (msg37, ispc1 + 1) : msg37.size ();
      size_t const ispc3 = ispc2 < msg37.size () ? find_fixed_space (msg37, ispc2 + 1) : msg37.size ();
      std::string callsign = token_between (ispc1 + 1, ispc2);
      std::string grid;
      size_t const slash_r = callsign.find ("/R");
      if (slash_r != std::string::npos && slash_r > 2)
        {
          callsign = callsign.substr (0, slash_r);
        }
      if (ft8_invalid_callsign_shape (callsign))
        {
          invalidate ();
          return;
        }

      size_t const indxr = msg37.find (" R ");
      if (indxr != std::string::npos)
        {
          size_t const ispc4 = ispc3 < msg37.size () ? find_fixed_space (msg37, ispc3 + 1) : msg37.size ();
          if (ispc4 == ispc3 + 5)
            {
              grid = maybe_grid_token (ispc3 + 1, ispc4);
              if (grid.empty ())
                {
                  invalidate ();
                  return;
                }
            }
        }
      else if (ispc3 == ispc2 + 5)
        {
          grid = maybe_grid_token (ispc2 + 1, ispc3);
          if (grid.empty ())
            {
              invalidate ();
              return;
            }
        }

      if (grid.size () == 4)
        {
          bool lchkcall = false;
          bool lgvalid = false;
          bool lwrongcall = false;
          ft8var_chkgrid_bridge (callsign, grid, lchkcall, lgvalid, lwrongcall);
          if (lwrongcall || !lgvalid)
            {
              invalidate ();
              return;
            }
        }
      return;
    }

  if (iaptype == 40)
    {
      size_t const ispc1 = find_fixed_space (msg37, 0);
      size_t const ispc2 = ispc1 < msg37.size () ? find_fixed_space (msg37, ispc1 + 1) : msg37.size ();
      size_t const ispc3 = ispc2 < msg37.size () ? find_fixed_space (msg37, ispc2 + 1) : msg37.size ();
      std::string const callsign = token_between (ispc1 + 1, ispc2);
      std::string grid;
      if (ft8_invalid_callsign_shape (callsign))
        {
          invalidate ();
          return;
        }
      size_t const indxr = msg37.find (" R ");
      if (indxr != std::string::npos)
        {
          size_t const ispc4 = ispc3 < msg37.size () ? find_fixed_space (msg37, ispc3 + 1) : msg37.size ();
          if (ispc4 == ispc3 + 5)
            {
              grid = maybe_grid_token (ispc3 + 1, ispc4);
            }
        }
      else if (ispc3 == ispc2 + 5)
        {
          grid = maybe_grid_token (ispc2 + 1, ispc3);
        }
      if (grid.size () == 4)
        {
          bool lchkcall = false;
          bool lgvalid = false;
          bool lwrongcall = false;
          ft8var_chkgrid_bridge (callsign, grid, lchkcall, lgvalid, lwrongcall);
          if (lwrongcall || !lgvalid)
            {
              invalidate ();
              return;
            }
        }
      return;
    }

  if (iaptype == 3 || iaptype == 11 || iaptype == 21 || iaptype == 41)
    {
      size_t const indxr = msg37.find (" R ");
      if (iaptype == 21 && indxr != std::string::npos)
        {
          invalidate ();
          return;
        }

      size_t const islash1_pos = msg37.find ('/');
      int const islash1 = islash1_pos == std::string::npos ? -1 : static_cast<int> (islash1_pos) + 1;
      size_t const ispc1 = find_fixed_space (msg37, 0);
      size_t const ispc2 = ispc1 < msg37.size () ? find_fixed_space (msg37, ispc1 + 1) : msg37.size ();
      if (ispc1 > 2 && ispc2 > 6)
        {
          if (islash1 < static_cast<int> (ispc1) + 1
              || (islash1 == static_cast<int> (ispc2) - 1 && is_ascii_digit (msg37[ispc2 - 1])))
            {
              std::string const first = token_between (0, ispc1);
              std::string const second = token_between (ispc1 + 1, ispc2);
              if (first == mycall && second == hiscall)
                {
                  size_t const ispc3 = ispc2 < msg37.size () ? find_fixed_space (msg37, ispc2 + 1) : msg37.size ();
                  std::string grid;
                  if (indxr != std::string::npos)
                    {
                      size_t const ispc4 = ispc3 < msg37.size () ? find_fixed_space (msg37, ispc3 + 1) : msg37.size ();
                      if (ispc4 == ispc3 + 5)
                        {
                          grid = maybe_grid_token (ispc3 + 1, ispc4);
                        }
                    }
                  else if (ispc3 == ispc2 + 5)
                    {
                      grid = maybe_grid_token (ispc2 + 1, ispc3);
                    }

                  if (grid.size () == 4 && grid != hisgrid4)
                    {
                      if (iaptype == 21)
                        {
                          if (grid == "RR73")
                            {
                              return;
                            }
                          invalidate ();
                          return;
                        }
                      bool lchkcall = false;
                      bool lgvalid = false;
                      bool lwrongcall = false;
                      ft8var_chkgrid_bridge (hiscall, grid, lchkcall, lgvalid, lwrongcall);
                      if (grid != "RR73" && !lgvalid)
                        {
                          invalidate ();
                          return;
                        }
                    }
                }
            }
        }
      return;
    }

  if ((i3 >= 1 && i3 <= 3)
      && (msg37.find (" R ") != std::string::npos || msg37.find ("/R ") != std::string::npos
          || msg37.find ("/P ") != std::string::npos))
    {
      size_t const islash1_pos = msg37.find ('/');
      size_t const ispc1 = find_fixed_space (msg37, 0);
      size_t const ispc2 = ispc1 < msg37.size () ? find_fixed_space (msg37, ispc1 + 1) : msg37.size ();
      std::string call_a;
      std::string call_b;

      if (islash1_pos == std::string::npos && ispc1 > 2 && ispc2 > 6)
        {
          call_a = token_between (0, ispc1);
          call_b = token_between (ispc1 + 1, ispc2);
          if (call_b != hiscall
              && (ft8_invalid_call_pair_shape (call_a, call_b) || ft8var_chkflscall_impl (call_a, call_b)))
            {
              invalidate ();
              return;
            }
        }

      if (islash1_pos != std::string::npos && ispc1 > 2 && ispc2 > 6)
        {
          size_t const islash2_pos = msg37.find ('/', islash1_pos + 1);
          if (islash1_pos > ispc1)
            {
              call_a = token_between (0, ispc1);
              call_b = token_between (ispc1 + 1, islash1_pos);
            }
          else
            {
              call_a = token_between (0, islash1_pos);
              call_b = islash2_pos != std::string::npos
                         ? token_between (ispc1 + 1, islash2_pos)
                         : token_between (ispc1 + 1, ispc2);
            }
          if (ft8_invalid_call_pair_shape (call_a, call_b))
            {
              invalidate ();
              return;
            }
          if (call_a != mycall && call_b != hiscall && ft8var_chkflscall_impl (call_a, call_b))
            {
              invalidate ();
              return;
            }
        }
    }

  if (lcall1hash && i3 == 1)
    {
      size_t const ispc1 = find_fixed_space (msg37, 0);
      if (token_between (0, ispc1) != mycall)
        {
          size_t const ispc2 = ispc1 < msg37.size () ? find_fixed_space (msg37, ispc1 + 1) : msg37.size ();
          std::string const callsign = token_between (ispc1 + 1, ispc2);
          if (callsign.find ('/') == std::string::npos)
            {
              if (ft8_invalid_callsign_shape (callsign))
                {
                  invalidate ();
                  return;
                }
              if (callsign != hiscall)
                {
                  size_t const ispc3 = ispc2 < msg37.size () ? find_fixed_space (msg37, ispc2 + 1) : msg37.size ();
                  std::string const grid = maybe_grid_token (ispc2 + 1, ispc3);
                  if (grid.size () == 4)
                    {
                      bool lchkcall = false;
                      bool lgvalid = false;
                      bool lwrongcall = false;
                      ft8var_chkgrid_bridge (callsign, grid, lchkcall, lgvalid, lwrongcall);
                      if (lwrongcall || !lgvalid)
                        {
                          invalidate ();
                          return;
                        }
                    }
                }
            }
        }
    }

  if (iaptype != 2)
    {
      bool const plain_dual_call = ((i3 == 1 || i3 == 3) && msg37.find (" R ") == std::string::npos
                                    && msg37.find ('/') == std::string::npos);
      bool const contest_msg = (i3 == 0 && n3 == 3);
      if ((plain_dual_call || contest_msg) && msg37.rfind ("<.", 0) != 0)
        {
          size_t const ispc1 = find_fixed_space (msg37, 0);
          size_t const ispc2 = ispc1 < msg37.size () ? find_fixed_space (msg37, ispc1 + 1) : msg37.size ();
          if (ispc1 > 2 && ispc2 > 6)
            {
              std::string const call_a = token_between (0, ispc1);
              if (call_a != mycall)
                {
                  std::string const call_b = token_between (ispc1 + 1, ispc2);
                  if (ft8_invalid_call_pair_shape (call_a, call_b) || ft8var_chkflscall_impl (call_a, call_b))
                    {
                      invalidate ();
                      return;
                    }
                }
            }
        }
    }

  if (i3 == 4)
    {
      size_t const ibrace1 = msg37.find ("<.");
      if (ibrace1 != std::string::npos && ibrace1 > 3)
        {
          std::string const callsign = token_between (0, ibrace1 - 1);
          size_t const islash_pos = callsign.find ('/');
          if (callsign.empty ())
            {
              goto label4;
            }
          if (callsign.find (' ') != std::string::npos
              || (islash_pos == std::string::npos && is_ascii_digit (callsign.back ())))
            {
              invalidate ();
              return;
            }
          if (ft8_invalid_callsign_shape (callsign) || ft8var_chklong8_impl (callsign))
            {
              invalidate ();
              return;
            }
        }

      if (msg37.rfind ("<.", 0) == 0 || msg37.find (".>") != std::string::npos)
        {
          std::string callsign;
          std::string const trimmed = trim_fixed (msg37.data (), static_cast<int> (msg37.size ()));
          int const nlenmsg = static_cast<int> (trimmed.size ());
          if (msg37.rfind ("<.", 0) == 0)
            {
              if (nlenmsg >= 5 && trimmed.substr (static_cast<size_t> (nlenmsg - 5)) == " RR73")
                {
                  callsign = to_ascii_upper (trim_fixed (trimmed.data () + 6, nlenmsg - 11));
                }
              else if (nlenmsg >= 3 && trimmed.substr (static_cast<size_t> (nlenmsg - 3)) == " 73")
                {
                  callsign = to_ascii_upper (trim_fixed (trimmed.data () + 6, nlenmsg - 9));
                }
              else if (nlenmsg >= 4 && trimmed.substr (static_cast<size_t> (nlenmsg - 4)) == " RRR")
                {
                  callsign = to_ascii_upper (trim_fixed (trimmed.data () + 6, nlenmsg - 10));
                }
              else
                {
                  callsign = to_ascii_upper (trim_fixed (trimmed.data () + 6, nlenmsg - 6));
                }
            }
          else
            {
              size_t const ibracket = msg37.find ("<.");
              if (ibracket != std::string::npos)
                {
                  callsign = token_between (0, ibracket - 1);
                }
            }

          int const nlencall = static_cast<int> (callsign.size ());
          if (nlencall > 0)
            {
              size_t const ispace = callsign.find (' ');
              size_t const islash_pos = callsign.find ('/');
              if (ispace != std::string::npos
                  || (islash_pos == std::string::npos && is_ascii_digit (callsign.back ()))
                  || islash_pos + 1 == callsign.size ())
                {
                  invalidate ();
                  return;
                }

              if (islash_pos != std::string::npos && static_cast<int> (islash_pos) + 1 > 4)
                {
                  size_t const islash2 = callsign.find ('/', islash_pos + 1);
                  int const islash = static_cast<int> (islash_pos) + 1;
                  if (nlencall == 11 && islash2 == std::string::npos)
                    {
                      if (islash < 7)
                        {
                          if (((callsign[static_cast<size_t> (islash)] < ':'
                                && callsign[static_cast<size_t> (islash + 1)] < ':')
                               || callsign[static_cast<size_t> (islash)] == 'Q'
                               || is_ascii_digit (callsign.back ())))
                            {
                              invalidate ();
                              return;
                            }
                        }
                      if (islash > 5)
                        {
                          if (is_ascii_digit (callsign[static_cast<size_t> (islash - 2)])
                              || (is_ascii_digit (callsign[0]) && is_ascii_digit (callsign[1]))
                              || callsign[0] == 'Q')
                            {
                              invalidate ();
                              return;
                            }
                        }
                      if (islash > 4 && islash < 8)
                        {
                          std::string const call_a = callsign.substr (0, static_cast<size_t> (islash - 1));
                          std::string const call_b = callsign.substr (static_cast<size_t> (islash));
                          if (ft8var_chkflscall_impl (call_a, call_b))
                            {
                              invalidate ();
                              return;
                            }
                        }
                    }
                }

              if (ft8_invalid_callsign_shape (callsign) || ft8var_chklong8_impl (callsign))
                {
                  invalidate ();
                  return;
                }
            }
        }
    }

label4:
  if (i3 == 3 && msg37.rfind ("TU;", 0) == 0)
    {
      size_t const ispc1 = find_fixed_space (msg37, 0);
      size_t const ispc2 = ispc1 < msg37.size () ? find_fixed_space (msg37, ispc1 + 1) : msg37.size ();
      size_t const ispc3 = ispc2 < msg37.size () ? find_fixed_space (msg37, ispc2 + 1) : msg37.size ();
      if (ispc2 > 6 && ispc3 > 10)
        {
          std::string const call_a = token_between (ispc1 + 1, ispc2);
          std::string const call_b = token_between (ispc2 + 1, ispc3);
          if (ft8_invalid_call_pair_shape (call_a, call_b) || ft8var_chkflscall_impl (call_a, call_b))
            {
              invalidate ();
              return;
            }
        }
    }

  if (i3 == 0 && n3 == 0)
    {
      if (msg37.find ("/.") != std::string::npos)
        {
          invalidate ();
          return;
        }
      if (ft8var_filtersfree_impl (msg37.substr (0, 22)))
        {
          invalidate ();
          return;
        }
    }

  if (i3 == 0 && (msg37.rfind ("CQ_", 0) == 0 || msg37.find ('^') != std::string::npos))
    {
      invalidate ();
      return;
    }
}
}

extern "C"
{
  void ftx_subtract_ft8_c (float* dd0, int const* itone, float f0, float dt, int lrefinedt);
}

extern "C" void ftx_ft8_prepare_candidate_c (float sync_in, float f1_in, float xdt_in,
                                              float const* sbase, int sbase_size,
                                              float* sync_out, float* f1_out,
                                              float* xdt_out, float* xbase_out)
{
  if (sync_out)
    {
      *sync_out = sync_in;
    }
  if (f1_out)
    {
      *f1_out = f1_in;
    }
  if (xdt_out)
    {
      *xdt_out = xdt_in;
    }
  if (xbase_out)
    {
      *xbase_out = compute_ft8_xbase (f1_in, sbase, sbase_size);
    }
}

extern "C" void ftx_ft8var_partintft8_c (float* dd8, int* ndelay, int const* nutc)
{
  if (!dd8 || !ndelay)
    {
      return;
    }

  int delay = *ndelay;
  delay = std::max (0, std::min (delay, 120));
  *ndelay = delay;

  int const numsamp = static_cast<int> (std::lround (static_cast<float> (delay) * 1200.0f));
  if (numsamp <= 0)
    {
      return;
    }

  std::memmove (dd8 + numsamp, dd8, static_cast<size_t> (180000 - numsamp) * sizeof (float));

  thread_local std::mt19937 rng {0x5a17c3u};
  std::uniform_real_distribution<float> dist (-5.0f, 5.0f);
  for (int i = 0; i < numsamp; ++i)
    {
      dd8[i] = dist (rng);
    }

  std::printf ("%06d  %-20s                     d\n",
               nutc ? *nutc : 0,
               "partial loss of data");
  std::fflush (stdout);
}

extern "C" void ftx_ft8var_msgparser_c (char const msg37_in[37],
                                         char msg37_out[37],
                                         char msg37_2_out[37])
{
  std::string const msg = msg37_in ? std::string (msg37_in, msg37_in + 37) : std::string (37, ' ');
  fill_fixed (msg37_out, 37, msg);
  fill_fixed (msg37_2_out, 37, std::string {});

  size_t const ispc1 = find_fixed_space (msg, 0);
  size_t const ispc2 = ispc1 < msg.size () ? find_fixed_space (msg, ispc1 + 1) : msg.size ();
  size_t const ispc3 = ispc2 < msg.size () ? find_fixed_space (msg, ispc2 + 1) : msg.size ();
  size_t const ispc4 = ispc3 < msg.size () ? find_fixed_space (msg, ispc3 + 1) : msg.size ();
  size_t const ispc5 = ispc4 < msg.size () ? find_fixed_space (msg, ispc4 + 1) : msg.size ();
  if (ispc5 < 20 || ispc1 == 0 || ispc2 <= ispc1 + 1 || ispc3 <= ispc2 + 1 || ispc4 <= ispc3 + 1)
    {
      return;
    }

  std::string const call1 = trim_fixed (msg.data (), static_cast<int> (ispc1));
  std::string const call2 = trim_fixed (msg.data () + ispc2 + 1, static_cast<int> (ispc3 - ispc2 - 1));

  std::string call3;
  if (ispc3 + 2 < msg.size () && msg[ispc3 + 1] == '<' && msg[ispc3 + 2] == '.')
    {
      call3 = "<...>";
    }
  else
    {
      call3 = trim_fixed (msg.data () + ispc3 + 1, static_cast<int> (ispc4 - ispc3 - 1));
    }

  std::string report;
  if (call3.size () < 11)
    {
      report = trim_fixed (msg.data () + ispc4 + 1, static_cast<int> (ispc5 - ispc4 - 1));
    }
  else
    {
      report = trim_fixed (msg.data () + ispc4 + 1, static_cast<int> (msg.size () - ispc4 - 1));
    }

  fill_fixed (msg37_out, 37, call1 + " RR73; " + call2 + " " + call3 + " " + report);
  fill_fixed (msg37_2_out, 37, call2 + " " + call3 + " " + report);
}

extern "C" int ftx_ft8var_searchcalls_c (char const callsign1[12], char const callsign2[12])
{
  std::string const call1 = callsign1 ? std::string (callsign1, callsign1 + 12) : std::string {};
  std::string const call2 = callsign2 ? std::string (callsign2, callsign2 + 12) : std::string {};
  return ft8var_searchcalls_impl (call1, call2) ? 1 : 0;
}

extern "C" int ftx_ft8var_chkflscall_c (char const call_a[12], char const call_b[12])
{
  std::string const first = call_a ? std::string (call_a, call_a + 12) : std::string {};
  std::string const second = call_b ? std::string (call_b, call_b + 12) : std::string {};
  return ft8var_chkflscall_impl (first, second) ? 1 : 0;
}

extern "C" int ftx_ft8var_chkspecial8_c (char msg37[37], char msg37_2[37],
                                          char const mycall12[12], char const hiscall12[12])
{
  std::string message = msg37 ? std::string (msg37, msg37 + 37) : std::string (37, ' ');
  std::string secondary = msg37_2 ? std::string (msg37_2, msg37_2 + 37) : std::string (37, ' ');
  std::string const mycall = mycall12 ? std::string (mycall12, mycall12 + 12) : std::string {};
  std::string const hiscall = hiscall12 ? std::string (hiscall12, hiscall12 + 12) : std::string {};
  bool const bad = ft8var_chkspecial_impl (message, secondary, mycall, hiscall);
  fill_fixed (msg37, 37, message);
  fill_fixed (msg37_2, 37, secondary);
  return bad ? 1 : 0;
}

extern "C" int ftx_ft8var_chklong8_c (char const callsign[12])
{
  std::string const value = callsign ? std::string (callsign, callsign + 12) : std::string {};
  return ft8var_chklong8_impl (value) ? 1 : 0;
}

extern "C" void ftx_ft8var_extract_call_c (char const msg37[37], char call2_out[12])
{
  std::string const message = msg37 ? std::string (msg37, msg37 + 37) : std::string (37, ' ');
  fill_fixed (call2_out, 12, ft8var_extract_call_impl (message));
}

extern "C" int ftx_ft8var_filtersfree_c (char const decoded[22])
{
  std::string const message = decoded ? std::string (decoded, decoded + 22) : std::string (22, ' ');
  return ft8var_filtersfree_impl (message) ? 1 : 0;
}

extern "C" void ftx_ft8var_reset_datacor_state_c ()
{
  auto& state = ft8var_datacor_state ();
  state.s3.fill (0.0f);
  state.correct.fill (0);
  state.initialized = false;
}

extern "C" void ftx_ft8var_set_datacor_state_c (float const* s3, int const* correct)
{
  auto& state = ft8var_datacor_state ();
  if (!s3 || !correct)
    {
      ftx_ft8var_reset_datacor_state_c ();
      return;
    }

  std::copy_n (s3, static_cast<int> (state.s3.size ()), state.s3.begin ());
  std::copy_n (correct, static_cast<int> (state.correct.size ()), state.correct.begin ());
  state.initialized = true;
}

extern "C" void ftx_ft8var_chkfalse8_c (char msg37[37], int const* i3_in, int const* n3_in,
                                         int* nbadcrc_io, int const* iaptype_in,
                                         int const* lcall1hash_in, char const mycall12[12],
                                         char const hiscall12[12], char const hisgrid4[4])
{
  std::string message = msg37 ? std::string (msg37, msg37 + 37) : std::string (37, ' ');
  int nbadcrc = nbadcrc_io ? *nbadcrc_io : 0;
  ft8var_chkfalse8_impl (message,
                         i3_in ? *i3_in : 0,
                         n3_in ? *n3_in : 0,
                         nbadcrc,
                         iaptype_in ? *iaptype_in : 0,
                         lcall1hash_in && *lcall1hash_in != 0,
                         mycall12 ? std::string (mycall12, mycall12 + 12) : std::string {},
                         hiscall12 ? std::string (hiscall12, hiscall12 + 12) : std::string {},
                         hisgrid4 ? std::string (hisgrid4, hisgrid4 + 4) : std::string {});
  fill_fixed (msg37, 37, message);
  if (nbadcrc_io)
    {
      *nbadcrc_io = nbadcrc;
    }
}

extern "C" void ftx_ft8_finalize_main_result_c (float xsnr, float xdt_in, float emedelay,
                                                 int nharderrors, float dmin,
                                                 int* nsnr_out, float* xdt_out,
                                                 float* qual_out)
{
  if (nsnr_out)
    {
      *nsnr_out = static_cast<int> (std::lround (xsnr));
    }
  if (xdt_out)
    {
      *xdt_out = xdt_in + (emedelay != 0.0f ? 2.0f : 0.0f);
    }
  if (qual_out)
    {
      *qual_out = 1.0f - (static_cast<float> (nharderrors) + dmin) / 60.0f;
    }
}

extern "C" int ftx_ft8_should_run_a7_c (int lft8apon, int ncontest, int nzhsym, int previous_count)
{
  return lft8apon != 0 && ncontest != 6 && ncontest != 7 && nzhsym == 50 && previous_count >= 1;
}

extern "C" int ftx_ft8_should_run_a8_c (int lft8apon, int ncontest, int nzhsym, int la8,
                                         int hiscall_len, int hisgrid_len, int ltry_a8)
{
  return lft8apon != 0 && ncontest != 6 && ncontest != 7 && nzhsym == 50
         && la8 != 0 && hiscall_len >= 3 && hisgrid_len >= 4 && ltry_a8 != 0;
}

extern "C" int ftx_ft8_prepare_a7_request_c (float previous_f0, float previous_dt,
                                              char const previous_msg37[37],
                                              float const* sbase, int sbase_size,
                                              float* f1_out, float* xdt_out,
                                              float* xbase_out,
                                              char call_1_out[12],
                                              char call_2_out[12],
                                              char grid4_out[4])
{
  if (f1_out) *f1_out = previous_f0;
  if (xdt_out) *xdt_out = previous_dt;
  if (xbase_out) *xbase_out = compute_ft8_xbase (previous_f0, sbase, sbase_size);
  fill_fixed (call_1_out, 12, std::string {});
  fill_fixed (call_2_out, 12, std::string {});
  fill_fixed (grid4_out, 4, std::string {});

  if (!previous_msg37)
    {
      return 0;
    }
  if (previous_f0 == -99.0f)
    {
      return 2;
    }
  if (previous_f0 == -98.0f)
    {
      return 0;
    }

  std::string const message (previous_msg37, previous_msg37 + 37);
  if (message.find ('<') != std::string::npos)
    {
      return 0;
    }

  size_t const first_blank = message.find (' ');
  if (first_blank == std::string::npos)
    {
      return 0;
    }
  size_t const second_blank = message.find (' ', first_blank + 1);
  if (second_blank == std::string::npos || second_blank <= first_blank + 1)
    {
      return 0;
    }

  std::string const call_1 = trim_fixed (message.data (), static_cast<int> (first_blank));
  std::string const call_2 = trim_fixed (message.data () + first_blank + 1,
                                         static_cast<int> (second_blank - first_blank - 1));
  std::string grid4 (4, ' ');
  for (int i = 0; i < 4; ++i)
    {
      size_t const index = second_blank + 1 + static_cast<size_t> (i);
      if (index < message.size ())
        {
          grid4[static_cast<size_t> (i)] = message[index];
        }
    }

  if (grid4 == "RR73" || grid4.find ('+') != std::string::npos
      || grid4.find ('-') != std::string::npos)
    {
      grid4.assign (4, ' ');
    }

  if (call_1.empty () || call_2.empty ())
    {
      return 0;
    }

  fill_fixed (call_1_out, 12, call_1);
  fill_fixed (call_2_out, 12, call_2);
  fill_fixed (grid4_out, 4, grid4);
  return 1;
}

extern "C" int ftx_ft8_should_keep_a8_after_a7_c (char const decoded_msg37[37],
                                                   char const hiscall12[12])
{
  std::string const message = trim_fixed (decoded_msg37, 37);
  std::string const hiscall = trim_fixed (hiscall12, 12);
  if (hiscall.empty ())
    {
      return 1;
    }
  return message.find (hiscall) == std::string::npos ? 1 : 0;
}

extern "C" void ftx_ft8_finalize_a7_result_c (float xsnr, int* nsnr_out, int* iaptype_out,
                                               float* qual_out)
{
  if (nsnr_out)
    {
      *nsnr_out = static_cast<int> (std::lround (xsnr));
    }
  if (iaptype_out)
    {
      *iaptype_out = 7;
    }
  if (qual_out)
    {
      *qual_out = 1.0f;
    }
}

extern "C" void ftx_ft8_finalize_a8_result_c (float plog, float xsnr, float fbest,
                                               int* nsnr_out, int* iaptype_out,
                                               float* qual_out, float* save_freq_out)
{
  if (nsnr_out)
    {
      *nsnr_out = static_cast<int> (std::lround (xsnr));
    }
  if (iaptype_out)
    {
      *iaptype_out = 8;
    }
  if (qual_out)
    {
      *qual_out = plog < -147.0f ? 0.16f : 1.0f;
    }
  if (save_freq_out)
    {
      *save_freq_out = fbest;
    }
}

extern "C" void ftx_ft8_a7_search_initial_c (Complex const* cd0, int np2, float fs2, float xdt_in,
                                              int* ibest_out, float* delfbest_out)
{
  int ibest = 0;
  float delfbest = 0.0f;
  if (!cd0 || np2 <= 0 || fs2 == 0.0f)
    {
      if (ibest_out) *ibest_out = ibest;
      if (delfbest_out) *delfbest_out = delfbest;
      return;
    }

  int const i0 = static_cast<int> (std::lround ((xdt_in + 0.5f) * fs2));
  float smax = 0.0f;
  std::array<Complex, 32> tweak {};
  std::fill (tweak.begin (), tweak.end (), Complex {});

  for (int idt = i0 - 10; idt <= i0 + 10; ++idt)
    {
      float sync = 0.0f;
      ftx_sync8d_c (cd0, np2, idt, tweak.data (), 0, &sync);
      if (sync > smax)
        {
          smax = sync;
          ibest = idt;
        }
    }

  smax = 0.0f;
  float const dt2 = 1.0f / fs2;
  for (int ifr = -5; ifr <= 5; ++ifr)
    {
      float const delf = static_cast<float> (ifr) * 0.5f;
      float const dphi = kTwoPi * delf * dt2;
      float phi = 0.0f;
      for (Complex& item : tweak)
        {
          item = Complex (std::cos (phi), std::sin (phi));
          phi = std::fmod (phi + dphi, kTwoPi);
        }

      float sync = 0.0f;
      ftx_sync8d_c (cd0, np2, ibest, tweak.data (), 1, &sync);
      if (sync > smax)
        {
          smax = sync;
          delfbest = delf;
        }
    }

  if (ibest_out) *ibest_out = ibest;
  if (delfbest_out) *delfbest_out = delfbest;
}

extern "C" void ftx_ft8_a7_refine_search_c (Complex const* cd0, int np2, float fs2, int ibest_in,
                                             int* ibest_out, float* sync_out, float* xdt_out)
{
  int ibest = ibest_in;
  float sync_best = 0.0f;
  float xdt = -0.5f;
  if (!cd0 || np2 <= 0 || fs2 == 0.0f)
    {
      if (ibest_out) *ibest_out = ibest;
      if (sync_out) *sync_out = sync_best;
      if (xdt_out) *xdt_out = xdt;
      return;
    }

  std::array<Complex, 32> tweak {};
  std::fill (tweak.begin (), tweak.end (), Complex {});
  for (int idt = -4; idt <= 4; ++idt)
    {
      float sync = 0.0f;
      ftx_sync8d_c (cd0, np2, ibest_in + idt, tweak.data (), 0, &sync);
      if (sync > sync_best)
        {
          sync_best = sync;
          ibest = ibest_in + idt;
        }
    }

  xdt = static_cast<float> (ibest - 1) / fs2 - 0.5f;
  if (ibest_out) *ibest_out = ibest;
  if (sync_out) *sync_out = sync_best;
  if (xdt_out) *xdt_out = xdt;
}

extern "C" void __ft8_search_support_MOD_ft8_search_initial_peak (Complex const* cd0,
                                                                   float* xdt, float* fs2,
                                                                   float* dt2, float* twopi,
                                                                   int* ibest, float* delfbest)
{
  (void) dt2;

  if (!ibest || !delfbest)
    {
      return;
    }

  *ibest = 0;
  *delfbest = 0.0f;
  if (!cd0 || !xdt || !fs2 || *fs2 == 0.0f)
    {
      return;
    }

  int const i0 = static_cast<int> (std::lround ((*xdt + 0.5f) * *fs2));
  float smax = 0.0f;
  std::array<Complex, 32> tweak {};
  std::fill (tweak.begin (), tweak.end (), Complex {});

  for (int idt = i0 - 10; idt <= i0 + 10; ++idt)
    {
      float sync = 0.0f;
      ftx_sync8d_c (cd0, kFt8PeakupNp2, idt, tweak.data (), 0, &sync);
      if (sync > smax)
        {
          smax = sync;
          *ibest = idt;
        }
    }

  smax = 0.0f;
  float const phase_scale = (twopi ? *twopi : kTwoPi) / *fs2;
  for (int ifr = -5; ifr <= 5; ++ifr)
    {
      float const delf = static_cast<float> (ifr) * 0.5f;
      float const dphi = phase_scale * delf;
      float phi = 0.0f;
      for (Complex& item : tweak)
        {
          item = Complex (std::cos (phi), std::sin (phi));
          phi = std::fmod (phi + dphi, twopi ? *twopi : kTwoPi);
        }

      float sync = 0.0f;
      ftx_sync8d_c (cd0, kFt8PeakupNp2, *ibest, tweak.data (), 1, &sync);
      if (sync > smax)
        {
          smax = sync;
          *delfbest = delf;
        }
    }
}

extern "C" void __ft8_search_support_MOD_ft8_apply_frequency_peakup (Complex* cd0,
                                                                      float* fs2,
                                                                      float* delfbest,
                                                                      float* f1)
{
  if (!cd0 || !fs2 || !delfbest || !f1)
    {
      return;
    }

  std::array<float, 5> tweak {};
  tweak[0] = -*delfbest;
  int const npts = kFt8PeakupNp2;
  std::array<Complex, kFt8A8Nfft> adjusted {};
  std::copy_n (cd0, kFt8A8Nfft, adjusted.data ());
  ftx_twkfreq1_c (reinterpret_cast<fftwf_complex const*> (cd0), &npts, fs2, tweak.data (),
                  reinterpret_cast<fftwf_complex*> (adjusted.data ()));
  std::copy (adjusted.begin (), adjusted.end (), cd0);
  *f1 += *delfbest;
}

extern "C" void __ft8_search_support_MOD_ft8_refine_time_peak (Complex const* cd0,
                                                                int* ibest,
                                                                float* dt2,
                                                                float* xdt_bias,
                                                                float* xdt,
                                                                float* sync)
{
  if (!ibest || !dt2 || !xdt_bias || !xdt || !sync)
    {
      return;
    }

  *sync = 0.0f;
  if (!cd0)
    {
      return;
    }

  std::array<Complex, 32> tweak {};
  std::fill (tweak.begin (), tweak.end (), Complex {});
  int best_index = *ibest;
  for (int idt = -4; idt <= 4; ++idt)
    {
      float candidate_sync = 0.0f;
      ftx_sync8d_c (cd0, kFt8PeakupNp2, *ibest + idt, tweak.data (), 0, &candidate_sync);
      if (candidate_sync > *sync)
        {
          *sync = candidate_sync;
          best_index = *ibest + idt;
        }
    }

  *ibest = best_index;
  *xdt = static_cast<float> (best_index - 1) * *dt2 + *xdt_bias;
}

extern "C" int ftx_ft8_a7_finalize_metrics_c (float const* dmm, int count, float pbest, float xbase,
                                               float* dmin_out, float* dmin2_out, float* xsnr_out)
{
  float dmin = 1.0e30f;
  float dmin2 = 1.0e30f;
  if (!dmm || count <= 0)
    {
      if (dmin_out) *dmin_out = dmin;
      if (dmin2_out) *dmin2_out = dmin2;
      if (xsnr_out) *xsnr_out = -25.0f;
      return 0;
    }

  for (int i = 0; i < count; ++i)
    {
      float const value = dmm[i];
      if (value < dmin)
        {
          dmin2 = dmin;
          dmin = value;
        }
      else if (value < dmin2)
        {
          dmin2 = value;
        }
    }

  float xsnr = -25.0f;
  float const denom = std::max (xbase * 3.0e6f, kTiny);
  float const arg = pbest / denom - 1.0f;
  if (arg > 0.0f)
    {
      xsnr = std::max (-25.0f, db_like (arg) - 27.0f);
    }

  if (dmin_out) *dmin_out = dmin;
  if (dmin2_out) *dmin2_out = dmin2;
  if (xsnr_out) *xsnr_out = xsnr;

  if (dmin > 100.0f)
    {
      return 0;
    }
  if (dmin > 0.0f && dmin2 / dmin < 1.3f)
    {
      return 0;
    }
  return 1;
}

extern "C" int ftx_ft8_a8_search_candidate_c (Complex const* cd, Complex const* cwave,
                                               int nzz, int nwave, float f1,
                                               float* spk_out, float* fpk_out,
                                               float* tpk_out, float* spectrum_out)
{
  if (spk_out) *spk_out = 0.0f;
  if (fpk_out) *fpk_out = 0.0f;
  if (tpk_out) *tpk_out = 0.0f;
  if (spectrum_out)
    {
      std::fill_n (spectrum_out, kFt8A8Nfft + 1, 0.0f);
    }
  if (!cd || !cwave || !spectrum_out || nzz != kFt8A8Nfft || nwave < 0 || nwave > nzz)
    {
      return 0;
    }

  auto& fft = ft8_a8_fft ();
  if (!fft.valid ())
    {
      return 0;
    }

  std::vector<float> spectrum (static_cast<size_t> (kFt8A8Nfft + 1), 0.0f);
  float spk = 0.0f;
  float fpk = 0.0f;
  float tpk = 0.0f;
  int lagpk = 0;

  for (int iter = 0; iter < 2; ++iter)
    {
      int lag1 = -200;
      int lag2 = 200;
      int lagstep = 4;
      if (iter == 1)
        {
          lag1 = lagpk - 8;
          lag2 = lagpk + 8;
          lagstep = 1;
        }

      for (int lag = lag1; lag <= lag2; lag += lagstep)
        {
          Complex* data = reinterpret_cast<Complex*> (fft.data);
          std::fill_n (data, nzz, Complex {});
          for (int i = 0; i < nwave; ++i)
            {
              int const j = i + lag + 100;
              if (j >= 0 && j <= nwave - 1)
                {
                  data[i] = cd[j] * std::conj (cwave[i]);
                }
            }

          fftwf_execute (fft.forward);
          std::fill (spectrum.begin (), spectrum.end (), 0.0f);
          for (int i = 0; i < nzz; ++i)
            {
              int j = i;
              if (i > kFt8A8Nh)
                {
                  j -= nzz;
                }
              float const re = fft.data[i][0];
              float const im = fft.data[i][1];
              spectrum[static_cast<size_t> (j + kFt8A8Nh)] = 1.0e-6f * (re * re + im * im);
            }

          smooth_121_in_place (spectrum.data (), static_cast<int> (spectrum.size ()));
          for (int j = -kFt8A8Nh; j <= kFt8A8Nh; ++j)
            {
              float const value = spectrum[static_cast<size_t> (j + kFt8A8Nh)];
              if (value > spk)
                {
                  spk = value;
                  fpk = static_cast<float> (j) * kFt8A8Df + f1;
                  lagpk = lag;
                  tpk = static_cast<float> (lag) * kFt8A8Dt;
                  std::copy (spectrum.begin (), spectrum.end (), spectrum_out);
                }
            }
        }
    }

  if (spk_out) *spk_out = spk;
  if (fpk_out) *fpk_out = fpk;
  if (tpk_out) *tpk_out = tpk;
  return 1;
}

extern "C" int ftx_ft8_a8_finalize_search_c (float const* spectrum, int size, float f1, float fbest,
                                               float* xsnr_out)
{
  if (xsnr_out)
    {
      *xsnr_out = -134.0f;
    }
  if (!spectrum || size < kFt8A8Nfft + 1)
    {
      return 0;
    }
  if (std::fabs (f1 - fbest) > 5.0f)
    {
      return 0;
    }

  float ave = 0.0f;
  for (int i = -200; i <= -100; ++i)
    {
      ave += spectrum[static_cast<size_t> (i + kFt8A8Nh)];
    }
  for (int i = 100; i <= 200; ++i)
    {
      ave += spectrum[static_cast<size_t> (i + kFt8A8Nh)];
    }
  ave /= 202.0f;
  if (ave <= kTiny)
    {
      return 0;
    }

  float s1pk = -1.0e30f;
  for (int i = -32; i <= 32; ++i)
    {
      float const value = spectrum[static_cast<size_t> (i + kFt8A8Nh)] / ave - 1.0f;
      s1pk = std::max (s1pk, value);
    }

  float sig = 0.0f;
  int nsig = 0;
  for (int i = -32; i <= 32; ++i)
    {
      float const value = spectrum[static_cast<size_t> (i + kFt8A8Nh)] / ave - 1.0f;
      if (value < 0.5f * s1pk)
        {
          continue;
        }
      sig += value;
      ++nsig;
    }
  if (nsig <= 0)
    {
      return 0;
    }

  if (xsnr_out)
    {
      *xsnr_out = db_like (sig / static_cast<float> (nsig)) - 35.0f;
    }
  return 1;
}

extern "C" int ftx_ft8_a8_accept_score_c (int nhard, float plog, float sigobig)
{
  return nhard <= 54 && plog >= -159.0f && sigobig >= 0.71f;
}

extern "C" void ftx_ft8_prepare_pass_c (int ndepth, int ipass, int ndecodes,
                                         float* syncmin, int* imetric,
                                         int* lsubtract, int* run_pass)
{
  float local_syncmin = 1.0f;
  int local_imetric = 1;
  int local_lsubtract = 1;
  int local_run_pass = 1;

  if (ndepth <= 2)
    {
      local_syncmin = 1.8f;
    }
  if (ipass >= 3)
    {
      local_syncmin *= 0.88f;
    }
  if (ipass >= 4)
    {
      local_syncmin *= 0.76f;
    }

  if (ipass == 1)
    {
      local_imetric = 1;
    }
  else if (ipass == 2)
    {
      local_imetric = 2;
    }
  else
    {
      local_imetric = 2;
      if (ndecodes == 0)
        {
          local_run_pass = 0;
        }
    }

  if (syncmin)
    {
      *syncmin = local_syncmin;
    }
  if (imetric)
    {
      *imetric = local_imetric;
    }
  if (lsubtract)
    {
      *lsubtract = local_lsubtract;
    }
  if (run_pass)
    {
      *run_pass = local_run_pass;
    }
}

extern "C" void ftx_ft8_plan_decode_stage_c (int ndepth, int nzhsym, int ndec_early,
                                             int nagain, int* action_out, int* refine_out)
{
  int action = 0;
  int refine = 1;

  if (ndepth == 1 && nzhsym < 50)
    {
      action = 1;
    }
  else if (nzhsym == 47 && ndec_early == 0)
    {
      action = 2;
    }
  else if (nzhsym == 47 && ndec_early >= 1)
    {
      action = 3;
      refine = ndepth > 2 ? 1 : 0;
    }
  else if (nzhsym == 50 && ndec_early >= 1 && nagain == 0)
    {
      action = 4;
    }

  if (action_out)
    {
      *action_out = action;
    }
  if (refine_out)
    {
      *refine_out = refine;
    }
}

extern "C" int ftx_ft8_should_bail_by_tseq_c (int ldiskdat, double tseq, double limit)
{
  return ldiskdat == 0 && tseq >= limit;
}

extern "C" int ftx_ft8_select_npasses_c (int lapon, int lapcqonly, int ncontest,
                                          int nzhsym, int nQSOProgress)
{
  int const progress = std::max (0, std::min (nQSOProgress, 5));
  int npasses = 5;
  if (lapon != 0 || ncontest == 7)
    {
      if (lapcqonly == 0)
        {
          npasses = 5 + 2 * kApPassCounts[static_cast<size_t> (progress)];
        }
      else
        {
          npasses = 7;
        }
    }
  if (nzhsym < 50)
    {
      npasses = 5;
    }
  return npasses;
}

extern "C" void ftx_ft8_plan_pass_window_c (int requested_pass, int npasses,
                                            int* pass_first_out, int* pass_last_out)
{
  int pass_first = 1;
  int pass_last = std::max (0, npasses);
  if (requested_pass > 0)
    {
      pass_first = requested_pass;
      pass_last = requested_pass;
    }

  if (pass_first_out)
    {
      *pass_first_out = pass_first;
    }
  if (pass_last_out)
    {
      *pass_last_out = pass_last;
    }
}

extern "C" int ftx_ft8_finalize_decode_pass_c (int nbadcrc, int pass_index, int iaptype_in,
                                               int* ipass_out, int* iaptype_out)
{
  if (nbadcrc != 0)
    {
      return 0;
    }

  if (ipass_out)
    {
      *ipass_out = pass_index;
    }
  if (iaptype_out)
    {
      *iaptype_out = iaptype_in;
    }
  return 1;
}

namespace
{
int prepare_ap_pass_impl (int ipass, int nQSOProgress, int lapcqonly, int ncontest,
                          int nfqso, int nftx, float f1, int napwid,
                          int const* apsym, int const* aph10,
                          float const* llra, float const* llrc,
                          float* llrz, int* apmask, int* iaptype_out)
{
  if (!apsym || !aph10 || !llra || !llrc || !llrz || !apmask || !iaptype_out)
    {
      return 0;
    }

  int const progress = std::max (0, std::min (nQSOProgress, 5));
  std::fill_n (apmask, 174, 0);

  float const* source = (((ipass - 5) % 2) == 1) ? llra : llrc;
  std::copy_n (source, 174, llrz);
  float const apmag = std::fabs (*std::max_element (llrz, llrz + 174,
    [] (float lhs, float rhs) { return std::fabs (lhs) < std::fabs (rhs); })) * 1.1f;

  int iaptype = 1;
  if (lapcqonly == 0)
    {
      int const pass_index = std::max (0, std::min (((ipass - 4) / 2) - 1, 3));
      iaptype = kApTypes[static_cast<size_t> (progress)][static_cast<size_t> (pass_index)];
    }
  if (iaptype <= 0)
    {
      return 0;
    }

  if (ncontest <= 5 && iaptype >= 3
      && (std::fabs (f1 - static_cast<float> (nfqso)) > static_cast<float> (napwid))
      && (std::fabs (f1 - static_cast<float> (nftx)) > static_cast<float> (napwid)))
    {
      return 0;
    }
  if (ncontest == 6)
    {
      return 0;
    }
  if (ncontest == 7 && f1 > 950.0f)
    {
      return 0;
    }
  if (iaptype >= 2 && apsym[0] > 1)
    {
      return 0;
    }
  if (ncontest == 7 && iaptype >= 2 && aph10[0] > 1)
    {
      return 0;
    }
  if (iaptype >= 3 && apsym[29] > 1)
    {
      return 0;
    }

  if (iaptype == 1)
    {
      set_mask_range (apmask, 1, 29);
      if (ncontest == 0 || ncontest == 7)
        {
          copy_raw_pm1 (llrz, 1, kMcqRaw, apmag);
        }
      else if (ncontest == 1 || ncontest == 2 || ncontest == 8)
        {
          copy_raw_pm1 (llrz, 1, kMcqTestRaw, apmag);
        }
      else if (ncontest == 3)
        {
          copy_raw_pm1 (llrz, 1, kMcqFdRaw, apmag);
        }
      else if (ncontest == 4)
        {
          copy_raw_pm1 (llrz, 1, kMcqRuRaw, apmag);
        }
      else if (ncontest == 5)
        {
          copy_raw_pm1 (llrz, 1, kMcqWwRaw, apmag);
        }
      set_mask_range (apmask, 75, 77);
      set_value_range (llrz, 75, 76, -apmag);
      set_value_range (llrz, 77, 77, apmag);
    }

  if (iaptype == 2)
    {
      if (ncontest == 0 || ncontest == 1 || ncontest == 5 || ncontest == 8)
        {
          set_mask_range (apmask, 1, 29);
          copy_scaled (llrz, 1, apsym, 29, apmag);
          set_mask_range (apmask, 75, 77);
          set_value_range (llrz, 75, 76, -apmag);
          set_value_range (llrz, 77, 77, apmag);
        }
      else if (ncontest == 2)
        {
          set_mask_range (apmask, 1, 28);
          copy_scaled (llrz, 1, apsym, 28, apmag);
          set_mask_range (apmask, 72, 74);
          llrz[71] = -apmag;
          llrz[72] = apmag;
          llrz[73] = -apmag;
          set_mask_range (apmask, 75, 77);
          set_value_range (llrz, 75, 77, -apmag);
        }
      else if (ncontest == 3)
        {
          set_mask_range (apmask, 1, 28);
          copy_scaled (llrz, 1, apsym, 28, apmag);
          set_mask_range (apmask, 75, 77);
          set_value_range (llrz, 75, 77, -apmag);
        }
      else if (ncontest == 4)
        {
          set_mask_range (apmask, 2, 29);
          copy_scaled (llrz, 2, apsym, 28, apmag);
          set_mask_range (apmask, 75, 77);
          llrz[74] = -apmag;
          llrz[75] = apmag;
          llrz[76] = apmag;
        }
      else if (ncontest == 7)
        {
          set_mask_range (apmask, 29, 56);
          copy_scaled (llrz, 29, apsym, 28, apmag);
          set_mask_range (apmask, 57, 66);
          copy_scaled (llrz, 57, aph10, 10, apmag);
          set_mask_range (apmask, 72, 77);
          llrz[71] = -apmag;
          llrz[72] = -apmag;
          llrz[73] = apmag;
          set_value_range (llrz, 75, 77, -apmag);
        }
      *iaptype_out = iaptype;
      return 1;
    }

  if (iaptype == 3)
    {
      if (ncontest == 0 || ncontest == 1 || ncontest == 2 || ncontest == 5
          || ncontest == 7 || ncontest == 8)
        {
          set_mask_range (apmask, 1, 58);
          copy_scaled (llrz, 1, apsym, 58, apmag);
          set_mask_range (apmask, 75, 77);
          set_value_range (llrz, 75, 76, -apmag);
          set_value_range (llrz, 77, 77, apmag);
        }
      else if (ncontest == 3)
        {
          set_mask_range (apmask, 1, 56);
          copy_scaled (llrz, 1, apsym, 28, apmag);
          copy_scaled (llrz, 29, apsym + 29, 28, apmag);
          set_mask_range (apmask, 72, 74);
          set_mask_range (apmask, 75, 77);
          set_value_range (llrz, 75, 77, -apmag);
        }
      else if (ncontest == 4)
        {
          set_mask_range (apmask, 2, 57);
          copy_scaled (llrz, 2, apsym, 28, apmag);
          copy_scaled (llrz, 30, apsym + 29, 28, apmag);
          set_mask_range (apmask, 75, 77);
          llrz[74] = -apmag;
          llrz[75] = apmag;
          llrz[76] = apmag;
        }
      *iaptype_out = iaptype;
      return 1;
    }

  if (iaptype == 5 && ncontest == 7)
    {
      return 0;
    }

  if (iaptype == 4 || iaptype == 5 || iaptype == 6)
    {
      if (ncontest <= 5 || (ncontest == 7 && iaptype == 6) || ncontest == 8)
        {
          set_mask_range (apmask, 1, 77);
          copy_scaled (llrz, 1, apsym, 58, apmag);
          if (iaptype == 4) copy_raw_pm1 (llrz, 59, kRrrRaw, apmag);
          if (iaptype == 5) copy_raw_pm1 (llrz, 59, k73Raw, apmag);
          if (iaptype == 6) copy_raw_pm1 (llrz, 59, kRr73Raw, apmag);
        }
      else if (ncontest == 7 && iaptype == 4)
        {
          set_mask_range (apmask, 1, 28);
          copy_scaled (llrz, 1, apsym, 28, apmag);
          set_mask_range (apmask, 57, 66);
          copy_scaled (llrz, 57, aph10, 10, apmag);
          set_mask_range (apmask, 72, 77);
          llrz[71] = -apmag;
          llrz[72] = -apmag;
          llrz[73] = apmag;
          set_value_range (llrz, 75, 77, -apmag);
        }
      *iaptype_out = iaptype;
      return 1;
    }

  *iaptype_out = iaptype;
  return 1;
}
}

extern "C" int ftx_ft8_prepare_ap_pass_c (int ipass, int nQSOProgress, int lapcqonly, int ncontest,
                                           int nfqso, int nftx, float f1, int napwid,
                                           int const* apsym, int const* aph10,
                                           float const* llra, float const* llrc,
                                           float* llrz, int* apmask, int* iaptype_out)
{
  return prepare_ap_pass_impl (ipass, nQSOProgress, lapcqonly, ncontest,
                               nfqso, nftx, f1, napwid, apsym, aph10,
                               llra, llrc, llrz, apmask, iaptype_out);
}

extern "C" int ftx_ft8_prepare_decode_pass_c (int ipass, int nQSOProgress, int lapcqonly, int ncontest,
                                               int nfqso, int nftx, float f1, int napwid,
                                               int const* apsym, int const* aph10,
                                               float const* llra, float const* llrb,
                                               float const* llrc, float const* llrd,
                                               float const* llre, float* llrz,
                                               int* apmask, int* iaptype_out)
{
  if (!llra || !llrb || !llrc || !llrd || !llre || !llrz || !apmask || !iaptype_out)
    {
      return 0;
    }

  std::fill_n (apmask, 174, 0);
  *iaptype_out = 0;

  auto copy_pass = [llrz] (float const* src) {
    std::copy_n (src, 174, llrz);
  };

  switch (ipass)
    {
    case 1:
      copy_pass (llra);
      return 1;
    case 2:
      copy_pass (llrb);
      return 1;
    case 3:
      copy_pass (llrc);
      return 1;
    case 4:
      copy_pass (llrd);
      return 1;
    case 5:
      copy_pass (llre);
      return 1;
    default:
      return prepare_ap_pass_impl (ipass, nQSOProgress, lapcqonly, ncontest,
                                   nfqso, nftx, f1, napwid, apsym, aph10,
                                   llra, llrc, llrz, apmask, iaptype_out);
    }
}

extern "C" int ftx_ft8_validate_candidate_c (int nharderrors, int zero_count, int i3, int n3,
                                              int unpack_ok, int quirky, int ncontest)
{
  if (nharderrors < 0 || nharderrors > 36)
    {
      return 0;
    }
  if (zero_count == 174)
    {
      return 0;
    }
  if (i3 > 5 || (i3 == 0 && n3 > 6) || (i3 == 0 && n3 == 2))
    {
      return 0;
    }
  if ((unpack_ok == 0 || quirky != 0) && i3 >= 1 && i3 <= 3 && ncontest == 0)
    {
      return 0;
    }
  if (unpack_ok == 0)
    {
      return 0;
    }
  return 1;
}

extern "C" int ftx_ft8_validate_candidate_meta_c (signed char const* message77, signed char const* cw,
                                                   int nharderrors, int unpack_ok, int quirky, int ncontest)
{
  if (!message77 || !cw)
    {
      return 0;
    }

  for (int i = 0; i < 77; ++i)
    {
      signed char const bit = message77[i];
      if (bit != 0 && bit != 1)
        {
          return 0;
        }
    }

  int zero_count = 0;
  for (int i = 0; i < 174; ++i)
    {
      if (cw[i] == 0)
        {
          ++zero_count;
        }
    }

  int const n3 = 4 * int (message77[71]) + 2 * int (message77[72]) + int (message77[73]);
  int const i3 = 4 * int (message77[74]) + 2 * int (message77[75]) + int (message77[76]);
  return ftx_ft8_validate_candidate_c (nharderrors, zero_count, i3, n3, unpack_ok, quirky, ncontest);
}

extern "C" int ftx_ft8_compute_snr_c (float const* s8, int rows, int cols, int const* itone,
                                       float xbase, int nagain, int nsync, float* xsnr_out)
{
  if (!s8 || !itone || rows <= 0 || cols <= 0 || !xsnr_out)
    {
      return 0;
    }
  (void) xbase;
  (void) nagain;

  float xsnr = compute_ft8_snr_legacy_like (s8, rows, cols, itone);
  if (nsync <= 10 && xsnr < -25.0f)
    {
      *xsnr_out = xsnr;
      return 0;
    }
  if (xsnr < -25.0f)
    {
      xsnr = -25.0f;
    }
  *xsnr_out = xsnr;
  return 1;
}

extern "C" void ftx_ft8c_measure_candidate_c (float const* llra, signed char const* cw,
                                               float* score_out, int* nharderrors_out,
                                               float* dmin_out)
{
  if (!llra || !cw)
    {
      if (score_out) *score_out = 0.0f;
      if (nharderrors_out) *nharderrors_out = 174;
      if (dmin_out) *dmin_out = 0.0f;
      return;
    }

  float score = 0.0f;
  int nharderrors = 0;
  float dmin = 0.0f;
  for (int i = 0; i < 174; ++i)
    {
      int const bit = cw[i] != 0 ? 1 : 0;
      int const rcw = 2 * bit - 1;
      score += llra[i] * static_cast<float> (rcw);
      if (static_cast<float> (rcw) * llra[i] < 0.0f)
        {
          ++nharderrors;
        }
      int const hdec = llra[i] >= 0.0f ? 1 : 0;
      if ((hdec ^ bit) != 0)
        {
          dmin += std::fabs (llra[i]);
        }
    }

  if (score_out) *score_out = score;
  if (nharderrors_out) *nharderrors_out = nharderrors;
  if (dmin_out) *dmin_out = dmin;
}

extern "C" void ftx_ft8a7_measure_candidate_c (float const* s8, int rows, int cols,
                                                int const* itone, signed char const* cw,
                                                float const* llra, float const* llrb,
                                                float const* llrc, float const* llrd,
                                                float* pow_out, float* dmin_out,
                                                int* nharderrors_out)
{
  if (!s8 || !itone || !cw || !llra || !llrb || !llrc || !llrd || rows <= 0 || cols <= 0)
    {
      if (pow_out) *pow_out = 0.0f;
      if (dmin_out) *dmin_out = 0.0f;
      if (nharderrors_out) *nharderrors_out = 174;
      return;
    }

  float pow = 0.0f;
  for (int i = 0; i < cols; ++i)
    {
      int const tone = itone[i];
      if (tone >= 0 && tone < rows)
        {
          float const value = s8[tone + rows * i];
          pow += value * value;
        }
    }

  auto measure = [cw] (float const* llr) {
    float dmin = 0.0f;
    int nharderrors = 0;
    for (int i = 0; i < 174; ++i)
      {
        int const bit = cw[i] != 0 ? 1 : 0;
        int const rcw = 2 * bit - 1;
        if (static_cast<float> (rcw) * llr[i] < 0.0f)
          {
            ++nharderrors;
          }
        int const hdec = llr[i] >= 0.0f ? 1 : 0;
        if ((hdec ^ bit) != 0)
          {
            dmin += std::fabs (llr[i]);
          }
      }
    return std::pair<float, int> {dmin, nharderrors};
  };

  std::pair<float, int> best = measure (llra);
  for (float const* llr : {llrb, llrc, llrd})
    {
      std::pair<float, int> current = measure (llr);
      if (current.first < best.first)
        {
          best = current;
        }
    }

  if (pow_out) *pow_out = pow;
  if (dmin_out) *dmin_out = best.first;
  if (nharderrors_out) *nharderrors_out = best.second;
}

namespace
{
int crc14_bits (signed char const* bits, int length)
{
  static constexpr std::array<int, 15> poly {{1,1,0,0,1,1,1,0,1,0,1,0,1,1,1}};
  std::array<int, 15> r {};
  for (int i = 0; i < 15; ++i)
    {
      r[static_cast<size_t> (i)] = bits[i] != 0 ? 1 : 0;
    }
  for (int i = 0; i <= length - 15; ++i)
    {
      r[14] = bits[i + 14] != 0 ? 1 : 0;
      int const lead = r[0];
      for (int j = 0; j < 15; ++j)
        {
          r[static_cast<size_t> (j)] = (r[static_cast<size_t> (j)] + lead * poly[static_cast<size_t> (j)]) % 2;
        }
      std::rotate (r.begin (), r.begin () + 1, r.end ());
    }

  int value = 0;
  for (int i = 0; i < 14; ++i)
    {
      value = (value << 1) | r[static_cast<size_t> (i)];
    }
  return value;
}
}

extern "C" void ftx_ldpc174_91_metrics_c (signed char const* cw, float const* llr,
                                           int* nharderrors_out, float* dmin_out)
{
  if (!cw || !llr)
    {
      if (nharderrors_out) *nharderrors_out = -1;
      if (dmin_out) *dmin_out = 0.0f;
      return;
    }

  int nharderrors = 0;
  float dmin = 0.0f;
  for (int i = 0; i < 174; ++i)
    {
      int const bit = cw[i] != 0 ? 1 : 0;
      int const rcw = 2 * bit - 1;
      if (static_cast<float> (rcw) * llr[i] < 0.0f)
        {
          ++nharderrors;
        }
      int const hdec = llr[i] >= 0.0f ? 1 : 0;
      if ((hdec ^ bit) != 0)
        {
          dmin += std::fabs (llr[i]);
        }
    }

  if (nharderrors_out) *nharderrors_out = nharderrors;
  if (dmin_out) *dmin_out = dmin;
}

extern "C" int ftx_ldpc174_91_finalize_c (signed char const* cw, float const* llr,
                                           signed char* message91_out, int* nharderrors_out,
                                           float* dmin_out)
{
  if (!cw || !llr)
    {
      if (nharderrors_out) *nharderrors_out = -1;
      if (dmin_out) *dmin_out = 0.0f;
      return 0;
    }

  if (message91_out)
    {
      std::copy_n (cw, 91, message91_out);
    }

  ftx_ldpc174_91_metrics_c (cw, llr, nharderrors_out, dmin_out);

  std::array<signed char, 96> m96 {};
  std::fill (m96.begin (), m96.end (), 0);
  std::copy_n (cw, 77, m96.begin ());
  std::copy_n (cw + 77, 14, m96.begin () + 82);
  return crc14_bits (m96.data (), 96) == 0 ? 1 : 0;
}

extern "C" int ftx_ft8_store_saved_decode_c (int ndecodes, int max_early,
                                              int nsnr, float f1, float xdt,
                                              int const* itone, int nn,
                                              int* allsnrs, float* f1_save,
                                              float* xdt_save, int* itone_save)
{
  if (!itone || !allsnrs || !f1_save || !xdt_save || !itone_save || nn <= 0)
    {
      return 0;
    }

  if (ndecodes < 0 || ndecodes >= max_early)
    {
      return 0;
    }

  int const slot = ndecodes;

  allsnrs[slot] = nsnr;
  f1_save[slot] = f1;
  xdt_save[slot] = xdt;
  std::copy_n (itone, nn, itone_save + slot * nn);

  return slot + 1;
}

extern "C" void ftx_ft8_apply_saved_subtractions_c (float* dd,
                                                     int const* itone_save, int nn, int ndec_early,
                                                     float const* f1_save, float const* xdt_save,
                                                     bool* lsubtracted,
                                                     int initial_only, int refine)
{
  if (!dd || !itone_save || !f1_save || !xdt_save || ndec_early <= 0 || nn <= 0)
    {
      return;
    }

  for (int i = 0; i < ndec_early; ++i)
    {
      int const* tones = itone_save + i * nn;

      if (initial_only != 0)
        {
          if (xdt_save[i] - 0.5f < 0.500f)
            {
              ftx_subtract_ft8_c (dd, tones, f1_save[i], xdt_save[i], refine);
              if (lsubtracted)
                {
                  lsubtracted[i] = true;
                }
            }
          continue;
        }

      if (lsubtracted && lsubtracted[i])
        {
          continue;
        }
      ftx_subtract_ft8_c (dd, tones, f1_save[i], xdt_save[i], refine);
    }
}
