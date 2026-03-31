// -*- Mode: C++ -*-
#include "Detector/MSK144DecodeWorker.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <limits>
#include <numeric>
#include <vector>

#include <fftw3.h>

#include <QDateTime>
#include <QDir>
#include <QMutexLocker>

#include "commons.h"
#include "Detector/FortranRuntimeGuard.hpp"
#include "Modulator/FtxMessageEncoder.hpp"

extern "C"
{
  using fortran_charlen_local = size_t;

  short crc13 (unsigned char const* data, int length);
  int legacy_pack77_unpack77bits_c (signed char const* message77, int received,
                                    char msgsent[37], int* quirky_out);
  std::uint32_t nhash (void const* key, size_t length, std::uint32_t initval);
}

namespace
{

using Complex = std::complex<float>;

constexpr int kMsk144SampleCount {360000};
constexpr int kMsk144SampleRate {12000};
constexpr int kMsk144BlockSize {7168};
constexpr int kMsk144StepSize {kMsk144BlockSize / 2};
constexpr int kMsk144WorkerMaxLines {50};
constexpr int kMsk144Nspm {864};
constexpr int kMsk144Nfft1 {8192};
constexpr int kMsk144SpdMaxCand {5};
constexpr int kMsk144SpdPatterns {6};
constexpr int kMsk144AvgPatterns {4};
constexpr int kMsk144RecentShmem {50};
constexpr int kMaxRecentCalls {10};
constexpr int kMsk144Bits {144};
constexpr int kMsk144MessageBits {77};
constexpr int kMsk144CodewordBits {128};
constexpr int kMsk144BpK {90};
constexpr int kMsk144BpM {38};
constexpr int kMsk144BpColWeight {3};
constexpr int kMsk144BpMaxRowWeight {11};
constexpr int kMsk40Nspm {240};
constexpr int kMsk40Bits {40};
constexpr int kMsk40MessageBits {16};
constexpr int kMsk40CodewordBits {32};
constexpr int kMsk40BpM {16};
constexpr int kMsk40BpColWeight {3};
constexpr int kMsk40BpMaxRowWeight {7};
constexpr float kTwoPi {6.28318530717958647692f};

template <size_t N>
using FixedChars = std::array<char, N>;

template <size_t N>
FixedChars<N> blank_fixed ()
{
  FixedChars<N> value {};
  value.fill (' ');
  return value;
}

template <size_t N>
FixedChars<N> fixed_from_latin (QByteArray const& input)
{
  FixedChars<N> value = blank_fixed<N> ();
  std::copy_n (input.constData (), std::min<int> (input.size (), static_cast<int> (N)), value.data ());
  return value;
}

template <size_t N>
FixedChars<N> fixed_from_string (std::string const& input)
{
  FixedChars<N> value = blank_fixed<N> ();
  std::copy_n (input.data (), std::min<size_t> (input.size (), N), value.data ());
  return value;
}

template <size_t N>
std::string trim_fixed (FixedChars<N> const& value)
{
  size_t used = N;
  while (used > 0 && (value[used - 1] == ' ' || value[used - 1] == '\0'))
    {
      --used;
    }
  return std::string {value.data (), used};
}

std::string trim_block (char const* data, int width)
{
  int used = width;
  while (used > 0 && (data[used - 1] == ' ' || data[used - 1] == '\0'))
    {
      --used;
    }
  return std::string {data, data + used};
}

std::string normalize_fmtmsg_like (std::string value)
{
  std::transform (value.begin (), value.end (), value.begin (), [] (unsigned char ch) {
    return static_cast<char> (std::toupper (ch));
  });

  std::string normalized;
  normalized.reserve (value.size ());
  bool last_space = false;
  for (char ch : value)
    {
      bool const is_space = ch == ' ';
      if (is_space && last_space)
        {
          continue;
        }
      normalized.push_back (ch);
      last_space = is_space;
    }

  while (!normalized.empty () && normalized.back () == ' ')
    {
      normalized.pop_back ();
    }
  return normalized;
}

FixedChars<37> fixed37_from_text (std::string const& value)
{
  return fixed_from_string<37> (value);
}

std::uint32_t msk_hash12 (std::string const& text)
{
  QByteArray field (37, ' ');
  QByteArray const latin = QByteArray::fromStdString (normalize_fmtmsg_like (text));
  std::copy_n (latin.constData (), std::min<int> (latin.size (), field.size ()), field.data ());
  return nhash (field.constData (), static_cast<size_t> (field.size ()), 146u) & 0x0fffu;
}

void update_recent_shmsgs_native (FixedChars<37> const& message,
                                  std::array<FixedChars<37>, kMsk144RecentShmem>& msgs)
{
  bool seen = false;
  for (auto const& item : msgs)
    {
      if (item == message)
        {
          seen = true;
          break;
        }
    }
  if (seen)
    {
      return;
    }
  for (int i = kMsk144RecentShmem - 1; i >= 1; --i)
    {
      msgs[static_cast<size_t> (i)] = msgs[static_cast<size_t> (i - 1)];
    }
  msgs[0] = message;
}

bool looks_like_callsign_token (std::string const& token)
{
  if (token.size () < 3 || token.size () > 12 || token.front () == '<')
    {
      return false;
    }
  bool has_alpha = false;
  bool has_digit = false;
  for (char ch : token)
    {
      unsigned char const uch = static_cast<unsigned char> (ch);
      if (std::isdigit (uch))
        {
          has_digit = true;
        }
      else if (std::isalpha (uch))
        {
          has_alpha = true;
        }
      else if (ch != '/')
        {
          return false;
        }
    }
  return has_alpha && has_digit;
}

void update_recent_call_native (std::string const& call, std::array<FixedChars<13>, kMaxRecentCalls>& recent_calls)
{
  if (call.empty ())
    {
      return;
    }
  FixedChars<13> const fixed_call = fixed_from_string<13> (call);
  for (auto const& item : recent_calls)
    {
      if (item == fixed_call)
        {
          return;
        }
    }
  for (int i = kMaxRecentCalls - 1; i >= 1; --i)
    {
      recent_calls[static_cast<size_t> (i)] = recent_calls[static_cast<size_t> (i - 1)];
    }
  recent_calls[0] = fixed_call;
}

void maybe_update_recent_calls_from_message (FixedChars<37> const& message,
                                             std::array<FixedChars<13>, kMaxRecentCalls>& recent_calls)
{
  QStringList const tokens = QString::fromLatin1 (trim_fixed (message).c_str ())
                               .simplified ()
                               .split (QLatin1Char (' '), Qt::SkipEmptyParts);
  if (tokens.isEmpty ())
    {
      return;
    }

  auto const normalized = [&] (int index) {
    return index >= 0 && index < tokens.size ()
      ? normalize_fmtmsg_like (tokens.at (index).toStdString ())
      : std::string {};
  };

  std::string const first = normalized (0);
  std::string const second = normalized (1);

  if ((first == "CQ" || first == "QRZ" || first == "DE") && looks_like_callsign_token (second))
    {
      update_recent_call_native (second, recent_calls);
      return;
    }

  if (looks_like_callsign_token (first))
    {
      update_recent_call_native (first, recent_calls);
    }
  if (looks_like_callsign_token (second))
    {
      update_recent_call_native (second, recent_calls);
    }
}

void update_msk40_hasharray_native (std::array<FixedChars<13>, kMaxRecentCalls> const& recent_calls,
                                    std::array<std::array<int, kMaxRecentCalls>, kMaxRecentCalls>& nhasharray)
{
  for (auto& row : nhasharray)
    {
      row.fill (-1);
    }

  for (int i = 0; i < kMaxRecentCalls; ++i)
    {
      std::string const call_i = trim_fixed (recent_calls[static_cast<size_t> (i)]);
      if (call_i.empty ())
        {
          continue;
        }
      for (int j = i + 1; j < kMaxRecentCalls; ++j)
        {
          std::string const call_j = trim_fixed (recent_calls[static_cast<size_t> (j)]);
          if (call_j.empty ())
            {
              continue;
            }
          nhasharray[static_cast<size_t> (i)][static_cast<size_t> (j)] =
              static_cast<int> (msk_hash12 (call_i + ' ' + call_j));
          nhasharray[static_cast<size_t> (j)][static_cast<size_t> (i)] =
              static_cast<int> (msk_hash12 (call_j + ' ' + call_i));
        }
    }
}

QByteArray to_fortran_field (QByteArray value, int width)
{
  value = value.left (width);
  if (value.size () < width)
    {
      value.append (QByteArray (width - value.size (), ' '));
    }
  return value;
}

int nint_compat (double value)
{
  return static_cast<int> (std::lround (value));
}

template <typename T>
T clamp_value (T value, T low, T high)
{
  return std::min (high, std::max (low, value));
}

float safe_abs (Complex const& value)
{
  return std::sqrt (std::norm (value));
}

template <size_t N>
std::array<Complex, N> cshift_left (std::array<Complex, N> const& in, int shift)
{
  std::array<Complex, N> out {};
  int const normalized = ((shift % static_cast<int> (N)) + static_cast<int> (N)) % static_cast<int> (N);
  for (size_t i = 0; i < N; ++i)
    {
      out[i] = in[(i + static_cast<size_t> (normalized)) % N];
    }
  return out;
}

template <size_t N>
float power_sum (std::array<Complex, N> const& data, int begin, int end)
{
  float sum = 0.0f;
  for (int i = begin; i < end; ++i)
    {
      sum += std::norm (data[static_cast<size_t> (i)]);
    }
  return sum;
}

template <int N>
struct ComplexFftWorkspace
{
  ComplexFftWorkspace ()
  {
    auto* buffer = reinterpret_cast<fftwf_complex*> (data.data ());
    forward = fftwf_plan_dft_1d (N, buffer, buffer, FFTW_FORWARD, FFTW_ESTIMATE);
    inverse = fftwf_plan_dft_1d (N, buffer, buffer, FFTW_BACKWARD, FFTW_ESTIMATE);
  }

  ~ComplexFftWorkspace ()
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

  std::array<Complex, N> data {};
  fftwf_plan forward {nullptr};
  fftwf_plan inverse {nullptr};
};

template <int N>
ComplexFftWorkspace<N>& fft_workspace ()
{
  static thread_local ComplexFftWorkspace<N> workspace;
  return workspace;
}

struct Msk144TrainingState
{
  std::array<Complex, kMsk144Nspm> cross_avg {};
  float wt_avg {0.0f};
  float tlast {0.0f};
  int navg {0};
  bool currently_training {false};
  FixedChars<13> training_dxcall = blank_fixed<13> ();
  FixedChars<13> trained_dxcall = blank_fixed<13> ();
};

struct Msk144RuntimeState
{
  bool initialized {false};
  float tsec0 {0.0f};
  int nutc00 {0};
  float pnoise {-1.0f};
  std::array<Complex, kMsk144Nfft1> cdat {};
  FixedChars<37> msglast = blank_fixed<37> ();
  FixedChars<37> msglastswl = blank_fixed<37> ();
  std::array<FixedChars<37>, kMsk144RecentShmem> recent_shmsgs {};
  std::array<FixedChars<13>, kMaxRecentCalls> recent_calls {};
  std::array<std::array<int, kMaxRecentCalls>, kMaxRecentCalls> nhasharray {};
  int nsnrlast {-99};
  int nsnrlastswl {-99};
  FixedChars<13> last_mycall13 = blank_fixed<13> ();
  FixedChars<13> last_dxcall13 = blank_fixed<13> ();
  Msk144TrainingState training;
};

Msk144RuntimeState& shared_worker_state ()
{
  static Msk144RuntimeState state;
  return state;
}

Msk144RuntimeState& shared_live_state ()
{
  static Msk144RuntimeState state;
  return state;
}

void reset_training_state (Msk144TrainingState& state)
{
  state.cross_avg.fill (Complex {});
  state.wt_avg = 0.0f;
  state.tlast = 0.0f;
  state.navg = 0;
  state.currently_training = false;
  state.training_dxcall = blank_fixed<13> ();
  state.trained_dxcall = blank_fixed<13> ();
}

void reset_runtime_state (Msk144RuntimeState& state)
{
  state.initialized = false;
  state.tsec0 = 0.0f;
  state.nutc00 = 0;
  state.pnoise = -1.0f;
  state.cdat.fill (Complex {});
  state.msglast = blank_fixed<37> ();
  state.msglastswl = blank_fixed<37> ();
  for (auto& item : state.recent_shmsgs)
    {
      item = blank_fixed<37> ();
    }
  for (auto& item : state.recent_calls)
    {
      item = blank_fixed<13> ();
    }
  for (auto& row : state.nhasharray)
    {
      row.fill (0);
    }
  state.nsnrlast = -99;
  state.nsnrlastswl = -99;
  state.last_mycall13 = blank_fixed<13> ();
  state.last_dxcall13 = blank_fixed<13> ();
  reset_training_state (state.training);
}

std::array<float, 12> const& half_sine_pulse ()
{
  static std::array<float, 12> pulse = [] {
    std::array<float, 12> value {};
    for (int i = 0; i < 12; ++i)
      {
        float const angle = static_cast<float> (i) * static_cast<float> (M_PI) / 12.0f;
        value[static_cast<size_t> (i)] = std::sin (angle);
      }
    return value;
  }();
  return pulse;
}

std::array<float, 12> const& raised_cosine_window ()
{
  static std::array<float, 12> window = [] {
    std::array<float, 12> value {};
    for (int i = 0; i < 12; ++i)
      {
        float const angle = static_cast<float> (i) * static_cast<float> (M_PI) / 12.0f;
        value[static_cast<size_t> (i)] = 0.5f * (1.0f - std::cos (angle));
      }
    return value;
  }();
  return window;
}

std::array<int, 8> const& sync_word_bits ()
{
  static std::array<int, 8> const bits {{0, 1, 1, 1, 0, 0, 1, 0}};
  return bits;
}

std::array<int, 8> const& sync_word_bipolar ()
{
  static std::array<int, 8> value = [] {
    std::array<int, 8> bits = sync_word_bits ();
    for (int& bit : bits)
      {
        bit = 2 * bit - 1;
      }
    return bits;
  }();
  return value;
}

std::array<Complex, 42> const& sync_waveform ()
{
  static std::array<Complex, 42> waveform = [] {
    std::array<Complex, 42> value {};
    auto const& pp = half_sine_pulse ();
    auto const& s8 = sync_word_bipolar ();
    std::array<float, 42> cbi {};
    std::array<float, 42> cbq {};

    for (int i = 0; i < 6; ++i)
      {
        cbq[static_cast<size_t> (i)] = pp[static_cast<size_t> (i + 6)] * static_cast<float> (s8[0]);
        cbi[static_cast<size_t> (36 + i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[7]);
      }
    for (int i = 0; i < 12; ++i)
      {
        cbq[static_cast<size_t> (6 + i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[2]);
        cbq[static_cast<size_t> (18 + i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[4]);
        cbq[static_cast<size_t> (30 + i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[6]);
        cbi[static_cast<size_t> (i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[1]);
        cbi[static_cast<size_t> (12 + i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[3]);
        cbi[static_cast<size_t> (24 + i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[5]);
      }

    for (int i = 0; i < 42; ++i)
      {
        value[static_cast<size_t> (i)] = {cbi[static_cast<size_t> (i)], cbq[static_cast<size_t> (i)]};
      }
    return value;
  }();
  return waveform;
}

std::array<int, 8> const& sync40_word_bits ()
{
  static std::array<int, 8> const bits {{1, 0, 1, 1, 0, 0, 0, 1}};
  return bits;
}

std::array<int, 8> const& sync40_word_bipolar ()
{
  static std::array<int, 8> value = [] {
    std::array<int, 8> bits = sync40_word_bits ();
    for (int& bit : bits)
      {
        bit = 2 * bit - 1;
      }
    return bits;
  }();
  return value;
}

std::array<Complex, 42> const& sync40_waveform ()
{
  static std::array<Complex, 42> waveform = [] {
    std::array<Complex, 42> value {};
    auto const& pp = half_sine_pulse ();
    auto const& s8 = sync40_word_bipolar ();
    std::array<float, 42> cbi {};
    std::array<float, 42> cbq {};

    for (int i = 0; i < 6; ++i)
      {
        cbq[static_cast<size_t> (i)] = pp[static_cast<size_t> (i + 6)] * static_cast<float> (s8[0]);
        cbi[static_cast<size_t> (36 + i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[7]);
      }
    for (int i = 0; i < 12; ++i)
      {
        cbq[static_cast<size_t> (6 + i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[2]);
        cbq[static_cast<size_t> (18 + i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[4]);
        cbq[static_cast<size_t> (30 + i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[6]);
        cbi[static_cast<size_t> (i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[1]);
        cbi[static_cast<size_t> (12 + i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[3]);
        cbi[static_cast<size_t> (24 + i)] = pp[static_cast<size_t> (i)] * static_cast<float> (s8[5]);
      }

    for (int i = 0; i < 42; ++i)
      {
        value[static_cast<size_t> (i)] = {cbi[static_cast<size_t> (i)], cbq[static_cast<size_t> (i)]};
      }
    return value;
  }();
  return waveform;
}

std::array<std::array<int, kMsk144BpColWeight>, kMsk144CodewordBits> const kMsk144BpMn = {{
  {{21, 34, 36}},
  {{1, 8, 28}},
  {{2, 9, 37}},
  {{3, 7, 19}},
  {{4, 16, 32}},
  {{2, 5, 22}},
  {{6, 13, 25}},
  {{10, 31, 33}},
  {{11, 24, 27}},
  {{12, 15, 23}},
  {{14, 18, 26}},
  {{17, 20, 29}},
  {{17, 30, 34}},
  {{6, 34, 35}},
  {{1, 10, 30}},
  {{3, 18, 23}},
  {{4, 12, 25}},
  {{5, 28, 36}},
  {{7, 14, 21}},
  {{8, 15, 31}},
  {{9, 27, 32}},
  {{11, 19, 35}},
  {{13, 16, 37}},
  {{20, 24, 38}},
  {{21, 22, 26}},
  {{12, 29, 33}},
  {{1, 17, 35}},
  {{2, 28, 30}},
  {{3, 10, 32}},
  {{4, 8, 36}},
  {{5, 19, 29}},
  {{6, 20, 27}},
  {{7, 22, 37}},
  {{9, 11, 33}},
  {{13, 24, 26}},
  {{14, 31, 34}},
  {{15, 16, 25}},
  {{13, 18, 38}},
  {{8, 20, 23}},
  {{1, 32, 33}},
  {{2, 17, 19}},
  {{3, 24, 34}},
  {{4, 7, 38}},
  {{5, 11, 31}},
  {{6, 18, 21}},
  {{9, 15, 36}},
  {{10, 16, 28}},
  {{12, 26, 30}},
  {{14, 27, 29}},
  {{22, 25, 35}},
  {{23, 30, 32}},
  {{4, 11, 37}},
  {{1, 14, 23}},
  {{2, 8, 25}},
  {{3, 13, 27}},
  {{5, 10, 37}},
  {{6, 16, 31}},
  {{7, 15, 18}},
  {{9, 22, 24}},
  {{12, 19, 36}},
  {{17, 26, 38}},
  {{20, 21, 33}},
  {{20, 28, 35}},
  {{4, 29, 34}},
  {{1, 26, 36}},
  {{2, 23, 34}},
  {{3, 9, 38}},
  {{5, 6, 17}},
  {{7, 27, 35}},
  {{8, 14, 32}},
  {{10, 15, 22}},
  {{11, 18, 29}},
  {{12, 13, 28}},
  {{16, 19, 33}},
  {{21, 25, 31}},
  {{24, 30, 37}},
  {{1, 3, 21}},
  {{2, 18, 31}},
  {{4, 6, 9}},
  {{5, 8, 33}},
  {{7, 29, 32}},
  {{10, 13, 19}},
  {{11, 22, 23}},
  {{12, 27, 34}},
  {{14, 15, 30}},
  {{16, 27, 38}},
  {{17, 28, 37}},
  {{20, 25, 26}},
  {{5, 24, 35}},
  {{3, 6, 36}},
  {{1, 12, 31}},
  {{2, 4, 33}},
  {{3, 16, 30}},
  {{1, 2, 24}},
  {{5, 23, 27}},
  {{6, 28, 32}},
  {{7, 17, 36}},
  {{8, 22, 38}},
  {{9, 18, 20}},
  {{10, 21, 29}},
  {{11, 13, 34}},
  {{4, 14, 20}},
  {{11, 30, 38}},
  {{14, 35, 37}},
  {{15, 19, 26}},
  {{3, 28, 29}},
  {{7, 8, 9}},
  {{5, 18, 34}},
  {{13, 15, 17}},
  {{12, 16, 35}},
  {{10, 23, 25}},
  {{19, 21, 37}},
  {{17, 27, 31}},
  {{24, 25, 36}},
  {{1, 18, 19}},
  {{6, 26, 33}},
  {{22, 31, 32}},
  {{3, 20, 22}},
  {{4, 21, 27}},
  {{2, 13, 29}},
  {{6, 7, 12}},
  {{15, 24, 32}},
  {{9, 25, 30}},
  {{23, 37, 38}},
  {{5, 16, 26}},
  {{11, 14, 28}},
  {{33, 36, 38}},
  {{8, 10, 35}}
}};

std::array<std::array<int, kMsk144BpMaxRowWeight>, kMsk144BpM> const kMsk144BpNm = {{
  {{2, 15, 27, 40, 53, 65, 77, 91, 94, 115, 0}},
  {{3, 6, 28, 41, 54, 66, 78, 92, 94, 120, 0}},
  {{4, 16, 29, 42, 55, 67, 77, 90, 93, 106, 118}},
  {{5, 17, 30, 43, 52, 64, 79, 92, 102, 119, 0}},
  {{6, 18, 31, 44, 56, 68, 80, 89, 95, 108, 125}},
  {{7, 14, 32, 45, 57, 68, 79, 90, 96, 116, 121}},
  {{4, 19, 33, 43, 58, 69, 81, 97, 107, 121, 0}},
  {{2, 20, 30, 39, 54, 70, 80, 98, 107, 128, 0}},
  {{3, 21, 34, 46, 59, 67, 79, 99, 107, 123, 0}},
  {{8, 15, 29, 47, 56, 71, 82, 100, 111, 128, 0}},
  {{9, 22, 34, 44, 52, 72, 83, 101, 103, 126, 0}},
  {{10, 17, 26, 48, 60, 73, 84, 91, 110, 121, 0}},
  {{7, 23, 35, 38, 55, 73, 82, 101, 109, 120, 0}},
  {{11, 19, 36, 49, 53, 70, 85, 102, 104, 126, 0}},
  {{10, 20, 37, 46, 58, 71, 85, 105, 109, 122, 0}},
  {{5, 23, 37, 47, 57, 74, 86, 93, 110, 125, 0}},
  {{12, 13, 27, 41, 61, 68, 87, 97, 109, 113, 0}},
  {{11, 16, 38, 45, 58, 72, 78, 99, 108, 115, 0}},
  {{4, 22, 31, 41, 60, 74, 82, 105, 112, 115, 0}},
  {{12, 24, 32, 39, 62, 63, 88, 99, 102, 118, 0}},
  {{1, 19, 25, 45, 62, 75, 77, 100, 112, 119, 0}},
  {{6, 25, 33, 50, 59, 71, 83, 98, 117, 118, 0}},
  {{10, 16, 39, 51, 53, 66, 83, 95, 111, 124, 0}},
  {{9, 24, 35, 42, 59, 76, 89, 94, 114, 122, 0}},
  {{7, 17, 37, 50, 54, 75, 88, 111, 114, 123, 0}},
  {{11, 25, 35, 48, 61, 65, 88, 105, 116, 125, 0}},
  {{9, 21, 32, 49, 55, 69, 84, 86, 95, 113, 119}},
  {{2, 18, 28, 47, 63, 73, 87, 96, 106, 126, 0}},
  {{12, 26, 31, 49, 64, 72, 81, 100, 106, 120, 0}},
  {{13, 15, 28, 48, 51, 76, 85, 93, 103, 123, 0}},
  {{8, 20, 36, 44, 57, 75, 78, 91, 113, 117, 0}},
  {{5, 21, 29, 40, 51, 70, 81, 96, 117, 122, 0}},
  {{8, 26, 34, 40, 62, 74, 80, 92, 116, 127, 0}},
  {{1, 13, 14, 36, 42, 64, 66, 84, 101, 108, 0}},
  {{14, 22, 27, 50, 63, 69, 89, 104, 110, 128, 0}},
  {{1, 18, 30, 46, 60, 65, 90, 97, 114, 127, 0}},
  {{3, 23, 33, 52, 56, 76, 87, 104, 112, 124, 0}},
  {{24, 38, 43, 61, 67, 86, 98, 103, 124, 127, 0}}
}};

std::array<int, kMsk144BpM> const kMsk144BpNrw = {{
  10, 10, 11, 10, 11, 11, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
  10, 10, 10, 10, 10, 10, 10, 11, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10
}};

std::array<std::array<int, kMsk40BpColWeight>, kMsk40CodewordBits> const kMsk40BpMn = {{
  {{1, 6, 13}}, {{2, 3, 14}}, {{4, 8, 15}}, {{5, 11, 12}},
  {{7, 10, 16}}, {{6, 9, 15}}, {{1, 11, 16}}, {{2, 4, 5}},
  {{3, 7, 9}}, {{8, 10, 12}}, {{8, 13, 14}}, {{1, 4, 12}},
  {{2, 6, 10}}, {{3, 11, 15}}, {{5, 9, 14}}, {{7, 13, 15}},
  {{12, 14, 16}}, {{1, 2, 8}}, {{3, 5, 6}}, {{4, 9, 11}},
  {{1, 7, 14}}, {{5, 10, 13}}, {{3, 4, 16}}, {{2, 15, 16}},
  {{6, 7, 12}}, {{7, 8, 11}}, {{1, 9, 10}}, {{2, 11, 13}},
  {{3, 12, 13}}, {{4, 6, 14}}, {{1, 5, 15}}, {{8, 9, 16}}
}};

std::array<std::array<int, kMsk40BpMaxRowWeight>, kMsk40BpM> const kMsk40BpNm = {{
  {{1, 7, 12, 18, 21, 27, 31}},
  {{2, 8, 13, 18, 24, 28, 0}},
  {{2, 9, 14, 19, 23, 29, 0}},
  {{3, 8, 12, 20, 23, 30, 0}},
  {{4, 8, 15, 19, 22, 31, 0}},
  {{1, 6, 13, 19, 25, 30, 0}},
  {{5, 9, 16, 21, 25, 26, 0}},
  {{3, 10, 11, 18, 26, 32, 0}},
  {{6, 9, 15, 20, 27, 32, 0}},
  {{5, 10, 13, 22, 27, 0, 0}},
  {{4, 7, 14, 20, 26, 28, 0}},
  {{4, 10, 12, 17, 25, 29, 0}},
  {{1, 11, 16, 22, 28, 29, 0}},
  {{2, 11, 15, 17, 21, 30, 0}},
  {{3, 6, 14, 16, 24, 31, 0}},
  {{5, 7, 17, 23, 24, 32, 0}}
}};

std::array<int, kMsk40BpM> const kMsk40BpNrw = {{
  7, 6, 6, 6, 6, 6, 6, 6, 6, 5, 6, 6, 6, 6, 6, 6
}};

std::array<int, kMsk40CodewordBits> const kMsk40ColumnOrder = {{
  4, 1, 2, 3, 0, 8, 6, 10, 13, 28, 20, 23, 17, 15, 27, 25,
  16, 12, 18, 19, 7, 21, 22, 11, 24, 5, 26, 14, 9, 29, 30, 31
}};

std::array<QString, 16> const kMsk40Reports = {{
  QStringLiteral ("-03"), QStringLiteral ("+00"), QStringLiteral ("+03"), QStringLiteral ("+06"),
  QStringLiteral ("+10"), QStringLiteral ("+13"), QStringLiteral ("+16"), QStringLiteral ("R-03"),
  QStringLiteral ("R+00"), QStringLiteral ("R+03"), QStringLiteral ("R+06"), QStringLiteral ("R+10"),
  QStringLiteral ("R+13"), QStringLiteral ("R+16"), QStringLiteral ("RRR"), QStringLiteral ("73")
}};

float platanh_msk144 (float x)
{
  int sign = x < 0.0f ? -1 : 1;
  float const z = std::abs (x);
  if (z <= 0.664f)
    {
      return x / 0.83f;
    }
  if (z <= 0.9217f)
    {
      return sign * (z - 0.4064f) / 0.322f;
    }
  if (z <= 0.9951f)
    {
      return sign * (z - 0.8378f) / 0.0524f;
    }
  if (z <= 0.9998f)
    {
      return sign * (z - 0.9914f) / 0.0012f;
    }
  return sign * 7.0f;
}

bool msk144_crc13_ok (std::array<signed char, kMsk144BpK> const& decoded)
{
  std::array<unsigned char, 12> bytes {};
  for (int bit = 0; bit < 77; ++bit)
    {
      unsigned const value = decoded[static_cast<size_t> (bit)] != 0 ? 1u : 0u;
      bytes[static_cast<size_t> (bit / 8)] |= static_cast<unsigned char> (value << (7 - (bit % 8)));
    }

  int expected = 0;
  for (int bit = 77; bit < kMsk144BpK; ++bit)
    {
      expected = (expected << 1) | (decoded[static_cast<size_t> (bit)] != 0 ? 1 : 0);
    }
  return static_cast<int> (crc13 (bytes.data (), static_cast<int> (bytes.size ()))) == expected;
}

bool bpdecode128_90_native (std::array<float, kMsk144CodewordBits> const& llr,
                            std::array<signed char, kMsk144CodewordBits> const& apmask,
                            int maxiterations,
                            std::array<signed char, kMsk144MessageBits>& message77_out,
                            std::array<signed char, kMsk144CodewordBits>& cw_out,
                            int& nharderror_out, int& iter_out)
{
  std::array<signed char, kMsk144BpK> decoded {};
  std::array<float, kMsk144CodewordBits> zn {};
  std::array<std::array<float, kMsk144BpColWeight>, kMsk144CodewordBits> tov {};
  std::array<std::array<float, kMsk144BpMaxRowWeight>, kMsk144BpM> toc {};
  std::array<std::array<float, kMsk144BpMaxRowWeight>, kMsk144BpM> tanhtoc {};
  std::array<int, kMsk144BpM> synd {};

  nharderror_out = -1;
  iter_out = 0;
  message77_out.fill (0);
  cw_out.fill (0);
  for (auto& row : tov)
    {
      row.fill (0.0f);
    }
  for (auto& row : toc)
    {
      row.fill (0.0f);
    }
  for (auto& row : tanhtoc)
    {
      row.fill (0.0f);
    }

  for (int check = 0; check < kMsk144BpM; ++check)
    {
      for (int row = 0; row < kMsk144BpNrw[static_cast<size_t> (check)]; ++row)
        {
          int const bit = kMsk144BpNm[static_cast<size_t> (check)][static_cast<size_t> (row)] - 1;
          toc[static_cast<size_t> (check)][static_cast<size_t> (row)] = llr[static_cast<size_t> (bit)];
        }
    }

  int ncnt = 0;
  int nclast = 0;

  for (int iter = 0; iter <= maxiterations; ++iter)
    {
      iter_out = iter;

      for (int bit = 0; bit < kMsk144CodewordBits; ++bit)
        {
          if (apmask[static_cast<size_t> (bit)] != 1)
            {
              zn[static_cast<size_t> (bit)] =
                  llr[static_cast<size_t> (bit)]
                + tov[static_cast<size_t> (bit)][0]
                + tov[static_cast<size_t> (bit)][1]
                + tov[static_cast<size_t> (bit)][2];
            }
          else
            {
              zn[static_cast<size_t> (bit)] = llr[static_cast<size_t> (bit)];
            }
        }

      int ncheck = 0;
      for (int bit = 0; bit < kMsk144CodewordBits; ++bit)
        {
          cw_out[static_cast<size_t> (bit)] = zn[static_cast<size_t> (bit)] > 0.0f
                                            ? static_cast<signed char> (1)
                                            : static_cast<signed char> (0);
        }
      for (int check = 0; check < kMsk144BpM; ++check)
        {
          int parity = 0;
          for (int row = 0; row < kMsk144BpNrw[static_cast<size_t> (check)]; ++row)
            {
              int const bit = kMsk144BpNm[static_cast<size_t> (check)][static_cast<size_t> (row)] - 1;
              parity += cw_out[static_cast<size_t> (bit)] != 0 ? 1 : 0;
            }
          synd[static_cast<size_t> (check)] = parity;
          if ((parity & 1) != 0)
            {
              ++ncheck;
            }
        }
      if (ncheck == 0)
        {
          std::copy_n (cw_out.begin (), kMsk144BpK, decoded.begin ());
          if (msk144_crc13_ok (decoded))
            {
              std::copy_n (decoded.begin (), kMsk144MessageBits, message77_out.begin ());
              nharderror_out = 0;
              for (int bit = 0; bit < kMsk144CodewordBits; ++bit)
                {
                  int const hard = cw_out[static_cast<size_t> (bit)] != 0 ? 1 : 0;
                  if ((2 * hard - 1) * llr[static_cast<size_t> (bit)] < 0.0f)
                    {
                      ++nharderror_out;
                    }
                }
              return true;
            }
        }

      if (iter > 0)
        {
          int const nd = ncheck - nclast;
          if (nd < 0)
            {
              ncnt = 0;
            }
          else
            {
              ++ncnt;
            }
          if (ncnt >= 3 && iter >= 5 && ncheck > 10)
            {
              nharderror_out = -1;
              return false;
            }
        }
      nclast = ncheck;

      for (int check = 0; check < kMsk144BpM; ++check)
        {
          for (int row = 0; row < kMsk144BpNrw[static_cast<size_t> (check)]; ++row)
            {
              int const bit = kMsk144BpNm[static_cast<size_t> (check)][static_cast<size_t> (row)] - 1;
              float value = zn[static_cast<size_t> (bit)];
              for (int edge = 0; edge < kMsk144BpColWeight; ++edge)
                {
                  if (kMsk144BpMn[static_cast<size_t> (bit)][static_cast<size_t> (edge)] - 1 == check)
                    {
                      value -= tov[static_cast<size_t> (bit)][static_cast<size_t> (edge)];
                    }
                }
              toc[static_cast<size_t> (check)][static_cast<size_t> (row)] = value;
            }
        }

      for (int check = 0; check < kMsk144BpM; ++check)
        {
          for (int row = 0; row < kMsk144BpNrw[static_cast<size_t> (check)]; ++row)
            {
              tanhtoc[static_cast<size_t> (check)][static_cast<size_t> (row)] =
                  std::tanh (-0.5f * toc[static_cast<size_t> (check)][static_cast<size_t> (row)]);
            }
        }

      for (int bit = 0; bit < kMsk144CodewordBits; ++bit)
        {
          for (int edge = 0; edge < kMsk144BpColWeight; ++edge)
            {
              int const check = kMsk144BpMn[static_cast<size_t> (bit)][static_cast<size_t> (edge)] - 1;
              float tmn = 1.0f;
              for (int row = 0; row < kMsk144BpNrw[static_cast<size_t> (check)]; ++row)
                {
                  int const other_bit = kMsk144BpNm[static_cast<size_t> (check)][static_cast<size_t> (row)] - 1;
                  if (other_bit != bit)
                    {
                      tmn *= tanhtoc[static_cast<size_t> (check)][static_cast<size_t> (row)];
                    }
                }
              tov[static_cast<size_t> (bit)][static_cast<size_t> (edge)] = 2.0f * platanh_msk144 (-tmn);
            }
        }
    }

  nharderror_out = -1;
  return false;
}

std::array<unsigned char, kMsk40CodewordBits> encode_msk40_codeword_native (
    std::array<signed char, kMsk40MessageBits> const& message)
{
  static std::array<unsigned short, kMsk40MessageBits> const generator = {{
    0x4428, 0x5a6b, 0x1b04, 0x2c12, 0x60c4, 0x1071, 0xbe6a, 0x36dd,
    0xc580, 0xad9a, 0xeca2, 0x7843, 0x332e, 0xa685, 0x5906, 0x1efe
  }};

  std::array<unsigned char, kMsk40MessageBits> pchecks {};
  for (int row = 0; row < kMsk40MessageBits; ++row)
    {
      int sum = 0;
      for (int bit = 0; bit < kMsk40MessageBits; ++bit)
        {
          if ((generator[static_cast<size_t> (row)] >> (15 - bit)) & 0x1u)
            {
              sum += message[static_cast<size_t> (bit)] != 0 ? 1 : 0;
            }
        }
      pchecks[static_cast<size_t> (row)] = static_cast<unsigned char> (sum & 0x1);
    }

  std::array<unsigned char, kMsk40CodewordBits> interleaved {};
  for (int i = 0; i < kMsk40MessageBits; ++i)
    {
      interleaved[static_cast<size_t> (i)] = pchecks[static_cast<size_t> (i)];
      interleaved[static_cast<size_t> (kMsk40MessageBits + i)] =
          static_cast<unsigned char> (message[static_cast<size_t> (i)] != 0 ? 1 : 0);
    }

  std::array<unsigned char, kMsk40CodewordBits> codeword {};
  for (int i = 0; i < kMsk40CodewordBits; ++i)
    {
      codeword[static_cast<size_t> (kMsk40ColumnOrder[static_cast<size_t> (i)])] =
          interleaved[static_cast<size_t> (i)];
    }
  return codeword;
}

bool bpdecode40_native (std::array<float, kMsk40CodewordBits> const& llr,
                        std::array<signed char, kMsk40MessageBits>& decoded_out,
                        int& niterations_out)
{
  std::array<signed char, kMsk40CodewordBits> codeword {};
  std::array<float, kMsk40CodewordBits> zn {};
  std::array<std::array<float, kMsk40BpColWeight>, kMsk40CodewordBits> tov {};
  std::array<std::array<float, kMsk40BpMaxRowWeight>, kMsk40BpM> toc {};
  std::array<std::array<float, kMsk40BpMaxRowWeight>, kMsk40BpM> tanhtoc {};

  for (auto& row : tov)
    {
      row.fill (0.0f);
    }
  for (auto& row : toc)
    {
      row.fill (0.0f);
    }
  for (auto& row : tanhtoc)
    {
      row.fill (0.0f);
    }

  for (int check = 0; check < kMsk40BpM; ++check)
    {
      for (int row = 0; row < kMsk40BpNrw[static_cast<size_t> (check)]; ++row)
        {
          int const bit = kMsk40BpNm[static_cast<size_t> (check)][static_cast<size_t> (row)] - 1;
          toc[static_cast<size_t> (check)][static_cast<size_t> (row)] = llr[static_cast<size_t> (bit)];
        }
    }

  for (int iter = 0; iter <= 5; ++iter)
    {
      niterations_out = iter;
      for (int bit = 0; bit < kMsk40CodewordBits; ++bit)
        {
          zn[static_cast<size_t> (bit)] =
              llr[static_cast<size_t> (bit)]
            + tov[static_cast<size_t> (bit)][0]
            + tov[static_cast<size_t> (bit)][1]
            + tov[static_cast<size_t> (bit)][2];
          codeword[static_cast<size_t> (bit)] = zn[static_cast<size_t> (bit)] > 0.0f
                                              ? static_cast<signed char> (1)
                                              : static_cast<signed char> (0);
        }

      int ncheck = 0;
      for (int check = 0; check < kMsk40BpM; ++check)
        {
          int parity = 0;
          for (int row = 0; row < kMsk40BpNrw[static_cast<size_t> (check)]; ++row)
            {
              int const bit = kMsk40BpNm[static_cast<size_t> (check)][static_cast<size_t> (row)] - 1;
              parity += codeword[static_cast<size_t> (bit)] != 0 ? 1 : 0;
            }
          if ((parity & 1) != 0)
            {
              ++ncheck;
            }
        }

      if (ncheck == 0)
        {
          std::array<signed char, kMsk40CodewordBits> reordered {};
          for (int i = 0; i < kMsk40CodewordBits; ++i)
            {
              reordered[static_cast<size_t> (i)] =
                  codeword[static_cast<size_t> (kMsk40ColumnOrder[static_cast<size_t> (i)])];
            }
          std::copy_n (reordered.begin () + kMsk40BpM, kMsk40MessageBits, decoded_out.begin ());
          return true;
        }

      for (int check = 0; check < kMsk40BpM; ++check)
        {
          for (int row = 0; row < kMsk40BpNrw[static_cast<size_t> (check)]; ++row)
            {
              int const bit = kMsk40BpNm[static_cast<size_t> (check)][static_cast<size_t> (row)] - 1;
              float value = zn[static_cast<size_t> (bit)];
              for (int edge = 0; edge < kMsk40BpColWeight; ++edge)
                {
                  if (kMsk40BpMn[static_cast<size_t> (bit)][static_cast<size_t> (edge)] - 1 == check)
                    {
                      value -= tov[static_cast<size_t> (bit)][static_cast<size_t> (edge)];
                    }
                }
              toc[static_cast<size_t> (check)][static_cast<size_t> (row)] = value;
            }
        }

      for (int check = 0; check < kMsk40BpM; ++check)
        {
          for (int row = 0; row < kMsk40BpNrw[static_cast<size_t> (check)]; ++row)
            {
              tanhtoc[static_cast<size_t> (check)][static_cast<size_t> (row)] =
                  std::tanh (-0.5f * toc[static_cast<size_t> (check)][static_cast<size_t> (row)]);
            }
        }

      for (int bit = 0; bit < kMsk40CodewordBits; ++bit)
        {
          for (int edge = 0; edge < kMsk40BpColWeight; ++edge)
            {
              int const check = kMsk40BpMn[static_cast<size_t> (bit)][static_cast<size_t> (edge)] - 1;
              float tmn = 1.0f;
              for (int row = 0; row < kMsk40BpNrw[static_cast<size_t> (check)]; ++row)
                {
                  int const other_bit = kMsk40BpNm[static_cast<size_t> (check)][static_cast<size_t> (row)] - 1;
                  if (other_bit != bit)
                    {
                      tmn *= tanhtoc[static_cast<size_t> (check)][static_cast<size_t> (row)];
                    }
                }
              tov[static_cast<size_t> (bit)][static_cast<size_t> (edge)] = 2.0f * platanh_msk144 (-tmn);
            }
        }
    }

  niterations_out = -1;
  return false;
}

QString format_msk40_message (std::string const& left, std::string const& right, int report_index)
{
  QString const report = kMsk40Reports[static_cast<size_t> (report_index)].leftJustified (4, QLatin1Char (' '));
  return QStringLiteral ("<%1 %2> %3").arg (QString::fromStdString (left),
                                            QString::fromStdString (right),
                                            report);
}

QString format_msk40_hash_message (int hash_value, int report_index)
{
  QString const report = kMsk40Reports[static_cast<size_t> (report_index)].leftJustified (4, QLatin1Char (' '));
  return QStringLiteral ("<%1> %2").arg (hash_value, 4, 10, QLatin1Char ('0')).arg (report);
}

bool decodeframe40_native (std::array<Complex, kMsk40Nspm> const& input_frame,
                           FixedChars<12> const& mycall,
                           FixedChars<12> const& hiscall,
                           float xsnr, bool bswl,
                           std::array<FixedChars<13>, kMaxRecentCalls> const& recent_calls,
                           std::array<std::array<int, kMaxRecentCalls>, kMaxRecentCalls> const& nhasharray,
                           FixedChars<37>& msgreceived_out, int& nsuccess_out)
{
  auto const& cb = sync40_waveform ();
  auto const& pp = half_sine_pulse ();
  auto const& s8r = sync40_word_bipolar ();

  nsuccess_out = 0;
  msgreceived_out = blank_fixed<37> ();

  std::string const mycall_text = trim_fixed (mycall);
  std::string const hiscall_text = trim_fixed (hiscall);
  int ihash = 9999;
  if (!mycall_text.empty () && !hiscall_text.empty ())
    {
      ihash = static_cast<int> (msk_hash12 (mycall_text + ' ' + hiscall_text));
    }

  Complex cca {};
  for (int i = 0; i < 42; ++i)
    {
      cca += input_frame[static_cast<size_t> (i)] * std::conj (cb[static_cast<size_t> (i)]);
    }
  float const phase0 = std::atan2 (std::imag (cca), std::real (cca));
  Complex const cfac {std::cos (phase0), std::sin (phase0)};

  std::array<Complex, kMsk40Nspm> c {};
  for (int i = 0; i < kMsk40Nspm; ++i)
    {
      c[static_cast<size_t> (i)] = input_frame[static_cast<size_t> (i)] * std::conj (cfac);
    }

  std::array<float, kMsk40Bits> softbits {};
  softbits[0] = 0.0f;
  for (int i = 0; i < 6; ++i)
    {
      softbits[0] += std::imag (c[static_cast<size_t> (i)]) * pp[static_cast<size_t> (i + 6)];
      softbits[1] += std::real (c[static_cast<size_t> (i)]) * pp[static_cast<size_t> (i)];
      softbits[1] += std::real (c[static_cast<size_t> (6 + i)]) * pp[static_cast<size_t> (6 + i)];
      softbits[0] += std::imag (c[static_cast<size_t> (kMsk40Nspm - 6 + i)]) * pp[static_cast<size_t> (i)];
    }
  for (int i = 2; i <= 20; ++i)
    {
      float odd = 0.0f;
      float even = 0.0f;
      int const odd_begin = (i - 1) * 12 - 6;
      int const even_begin = (i - 1) * 12;
      for (int j = 0; j < 12; ++j)
        {
          odd += std::imag (c[static_cast<size_t> (odd_begin + j)]) * pp[static_cast<size_t> (j)];
          even += std::real (c[static_cast<size_t> (even_begin + j)]) * pp[static_cast<size_t> (j)];
        }
      softbits[static_cast<size_t> (2 * i - 2)] = odd;
      softbits[static_cast<size_t> (2 * i - 1)] = even;
    }

  std::array<int, kMsk40Bits> hardbits {};
  for (int i = 0; i < kMsk40Bits; ++i)
    {
      hardbits[static_cast<size_t> (i)] = softbits[static_cast<size_t> (i)] >= 0.0f ? 1 : 0;
    }
  int sync_score = 0;
  for (int i = 0; i < 8; ++i)
    {
      sync_score += (2 * hardbits[static_cast<size_t> (i)] - 1) * s8r[static_cast<size_t> (i)];
    }
  int const nbadsync = (8 - sync_score) / 2;
  if (nbadsync > 3)
    {
      return false;
    }

  float sav = std::accumulate (softbits.begin (), softbits.end (), 0.0f) / static_cast<float> (kMsk40Bits);
  float s2av = 0.0f;
  for (float value : softbits)
    {
      s2av += value * value;
    }
  s2av /= static_cast<float> (kMsk40Bits);
  float const ssig = std::sqrt (std::max (1.0e-6f, s2av - sav * sav));
  for (float& value : softbits)
    {
      value /= ssig;
    }

  float sigma = 0.75f;
  if (xsnr < 0.0f)
    {
      sigma = 0.75f - 0.11f * xsnr;
    }

  std::array<float, kMsk40CodewordBits> llr {};
  for (int i = 0; i < kMsk40CodewordBits; ++i)
    {
      llr[static_cast<size_t> (i)] = 2.0f * softbits[static_cast<size_t> (i + 8)] / (sigma * sigma);
    }

  std::array<signed char, kMsk40MessageBits> decoded {};
  int niterations = -1;
  if (!bpdecode40_native (llr, decoded, niterations) || niterations < 0)
    {
      return false;
    }

  std::array<unsigned char, kMsk40CodewordBits> const codeword = encode_msk40_codeword_native (decoded);
  int nhammd = 0;
  float cord = 0.0f;
  for (int i = 0; i < kMsk40CodewordBits; ++i)
    {
      if (codeword[static_cast<size_t> (i)] != static_cast<unsigned char> (hardbits[static_cast<size_t> (i + 8)]))
        {
          ++nhammd;
          cord += std::abs (softbits[static_cast<size_t> (i + 8)]);
        }
    }

  int imsg = 0;
  for (int i = 0; i < kMsk40MessageBits; ++i)
    {
      imsg = (imsg << 1) | (decoded[static_cast<size_t> (kMsk40MessageBits - 1 - i)] != 0 ? 1 : 0);
    }
  int const nrxrpt = imsg & 15;
  int const nrxhash = (imsg - nrxrpt) / 16;

  if (nhammd <= 4 && cord < 0.65f && nrxhash == ihash && nrxrpt >= 7)
    {
      nsuccess_out = 1;
      msgreceived_out = fixed37_from_text (format_msk40_message (mycall_text, hiscall_text, nrxrpt).toStdString ());
      return true;
    }

  if (bswl && nhammd <= 4 && cord < 0.65f && nrxrpt >= 7)
    {
      for (int i = 0; i < kMaxRecentCalls; ++i)
        {
          std::string const left = trim_fixed (recent_calls[static_cast<size_t> (i)]);
          if (left.empty ())
            {
              continue;
            }
          for (int j = i + 1; j < kMaxRecentCalls; ++j)
            {
              std::string const right = trim_fixed (recent_calls[static_cast<size_t> (j)]);
              if (right.empty ())
                {
                  continue;
                }
              if (nrxhash == nhasharray[static_cast<size_t> (i)][static_cast<size_t> (j)])
                {
                  nsuccess_out = 2;
                  msgreceived_out = fixed37_from_text (format_msk40_message (left, right, nrxrpt).toStdString ());
                  return true;
                }
              if (nrxhash == nhasharray[static_cast<size_t> (j)][static_cast<size_t> (i)])
                {
                  nsuccess_out = 2;
                  msgreceived_out = fixed37_from_text (format_msk40_message (right, left, nrxrpt).toStdString ());
                  return true;
                }
            }
        }
      nsuccess_out = 3;
      msgreceived_out = fixed37_from_text (format_msk40_hash_message (nrxhash, nrxrpt).toStdString ());
      return true;
    }

  return false;
}

struct Sync40Result
{
  bool ok {false};
  float fest {0.0f};
  float xmax {0.0f};
  std::array<int, 10> npklocs {};
  std::array<Complex, kMsk40Nspm> c {};
};

Sync40Result sync40_native (Complex const* cdat, int nframes, int ntol, float delf,
                            int const* navmask, int npeaks, float fc)
{
  Sync40Result result;
  result.npklocs.fill (1);

  int const navg = std::accumulate (navmask, navmask + nframes, 0);
  if (navg <= 0)
    {
      return result;
    }

  int const n = nframes * kMsk40Nspm;
  float const fac = 1.0f / (24.0f * std::sqrt (static_cast<float> (navg)));
  int const span = nint_compat (static_cast<float> (ntol) / delf);
  std::array<float, kMsk40Nspm> best_xcc {};

  std::vector<Complex> shifted (static_cast<size_t> (n));
  auto const& cb = sync40_waveform ();
  for (int ifr = -span; ifr <= span; ++ifr)
    {
      float const ferr = static_cast<float> (ifr) * delf;
      float const dphi = kTwoPi * (-(fc + ferr)) / static_cast<float> (kMsk144SampleRate);
      Complex const wstep {std::cos (dphi), std::sin (dphi)};
      Complex w {1.0f, 0.0f};
      for (int i = 0; i < n; ++i)
        {
          w *= wstep;
          shifted[static_cast<size_t> (i)] = w * cdat[static_cast<size_t> (i)];
        }

      std::array<Complex, kMsk40Nspm> averaged {};
      averaged.fill (Complex {});
      for (int frame = 0; frame < nframes; ++frame)
        {
          if (navmask[frame] == 0)
            {
              continue;
            }
          int const start = frame * kMsk40Nspm;
          for (int i = 0; i < kMsk40Nspm; ++i)
            {
              averaged[static_cast<size_t> (i)] += shifted[static_cast<size_t> (start + i)];
            }
        }

      std::array<Complex, 2 * kMsk40Nspm> duplicated {};
      for (int i = 0; i < kMsk40Nspm; ++i)
        {
          duplicated[static_cast<size_t> (i)] = averaged[static_cast<size_t> (i)];
          duplicated[static_cast<size_t> (kMsk40Nspm + i)] = averaged[static_cast<size_t> (i)];
        }

      std::array<float, kMsk40Nspm> xcc {};
      for (int shift = 0; shift < kMsk40Nspm; ++shift)
        {
          Complex accum {};
          for (int i = 0; i < 42; ++i)
            {
              accum += duplicated[static_cast<size_t> (shift + i)] * std::conj (cb[static_cast<size_t> (i)]);
            }
          xcc[static_cast<size_t> (shift)] = safe_abs (accum);
        }

      float const peak = *std::max_element (xcc.begin (), xcc.end ()) * fac;
      if (peak > result.xmax)
        {
          result.xmax = peak;
          result.fest = fc + ferr;
          result.c = averaged;
          best_xcc = xcc;
        }
    }

  if (result.xmax < 1.3f)
    {
      return result;
    }

  for (int peak_index = 0; peak_index < npeaks; ++peak_index)
    {
      auto it = std::max_element (best_xcc.begin (), best_xcc.end ());
      int const location = static_cast<int> (std::distance (best_xcc.begin (), it)) + 1;
      result.npklocs[static_cast<size_t> (peak_index)] = location;
      int const lo = std::max (0, location - 1 - 7);
      int const hi = std::min (kMsk40Nspm - 1, location - 1 + 7);
      for (int i = lo; i <= hi; ++i)
        {
          best_xcc[static_cast<size_t> (i)] = 0.0f;
        }
    }
  result.ok = true;
  return result;
}

struct Msk40SpdResult
{
  bool ok {false};
  int nsuccess {0};
  FixedChars<37> msgreceived = blank_fixed<37> ();
  float fret {0.0f};
  float tret {0.0f};
  int navg {0};
};

Msk40SpdResult spd40_native (Complex const* cbig, int n, int ntol,
                             FixedChars<12> const& mycall, FixedChars<12> const& hiscall,
                             bool bswl,
                             std::array<FixedChars<13>, kMaxRecentCalls> const& recent_calls,
                             std::array<std::array<int, kMaxRecentCalls>, kMaxRecentCalls> const& nhasharray,
                             float fc)
{
  Msk40SpdResult result;

  auto const& rcw = raised_cosine_window ();
  auto& fft = fft_workspace<kMsk40Nspm> ();
  int const nstep = (n - kMsk40Nspm) / 60;
  if (nstep <= 0)
    {
      return result;
    }

  std::vector<float> detmet (static_cast<size_t> (nstep), 0.0f);
  std::vector<float> detmet2 (static_cast<size_t> (nstep), 0.0f);
  std::vector<float> detfer (static_cast<size_t> (nstep), -999.99f);
  float const df = static_cast<float> (kMsk144SampleRate) / static_cast<float> (kMsk40Nspm);
  float const nfhi = 2.0f * (fc + 500.0f);
  float const nflo = 2.0f * (fc - 500.0f);
  int const ihlo = clamp_value (nint_compat ((nfhi - 2.0f * ntol) / df), 1, kMsk40Nspm - 2);
  int const ihhi = clamp_value (nint_compat ((nfhi + 2.0f * ntol) / df), 1, kMsk40Nspm - 2);
  int const illo = clamp_value (nint_compat ((nflo - 2.0f * ntol) / df), 1, kMsk40Nspm - 2);
  int const ilhi = clamp_value (nint_compat ((nflo + 2.0f * ntol) / df), 1, kMsk40Nspm - 2);
  int const i2000 = clamp_value (nint_compat (nflo / df), 1, kMsk40Nspm - 2);
  int const i4000 = clamp_value (nint_compat (nfhi / df), 1, kMsk40Nspm - 2);

  for (int step = 0; step < nstep; ++step)
    {
      int const ns = step * 60;
      if (ns + kMsk40Nspm > n)
        {
          break;
        }

      fft.data.fill (Complex {});
      for (int i = 0; i < kMsk40Nspm; ++i)
        {
          fft.data[static_cast<size_t> (i)] = cbig[static_cast<size_t> (ns + i)];
        }

      for (Complex& value : fft.data)
        {
          value *= value;
        }
      for (int i = 0; i < 12; ++i)
        {
          fft.data[static_cast<size_t> (i)] *= rcw[static_cast<size_t> (i)];
          fft.data[static_cast<size_t> (kMsk40Nspm - 12 + i)] *= rcw[static_cast<size_t> (11 - i)];
        }
      fftwf_execute (fft.forward);

      std::array<float, kMsk40Nspm> tonespec {};
      for (int i = 0; i < kMsk40Nspm; ++i)
        {
          tonespec[static_cast<size_t> (i)] = std::norm (fft.data[static_cast<size_t> (i)]);
        }

      auto best_in_range = [&tonespec] (int lo, int hi) {
        int best = lo;
        for (int i = lo + 1; i <= hi; ++i)
          {
            if (tonespec[static_cast<size_t> (i)] > tonespec[static_cast<size_t> (best)])
              {
                best = i;
              }
          }
        return best;
      };
      auto range_average = [&tonespec] (int lo, int hi, int peak) {
        float sum = 0.0f;
        int count = 0;
        for (int i = lo; i <= hi; ++i)
          {
            if (i == peak)
              {
                continue;
              }
            sum += tonespec[static_cast<size_t> (i)];
            ++count;
          }
        return count > 0 ? sum / static_cast<float> (count) : 0.0f;
      };
      auto parabola_delta = [&fft] (int peak) {
        Complex const numerator = fft.data[static_cast<size_t> (peak - 1)] - fft.data[static_cast<size_t> (peak + 1)];
        Complex const denominator = 2.0f * fft.data[static_cast<size_t> (peak)]
                                  - fft.data[static_cast<size_t> (peak - 1)]
                                  - fft.data[static_cast<size_t> (peak + 1)];
        if (std::norm (denominator) < 1.0e-12f)
          {
            return 0.0f;
          }
        return -std::real (numerator / denominator);
      };

      int const ihpk = best_in_range (ihlo, ihhi);
      int const ilpk = best_in_range (illo, ilhi);
      float const deltah = parabola_delta (ihpk);
      float const deltal = parabola_delta (ilpk);
      float const ah = tonespec[static_cast<size_t> (ihpk)];
      float const al = tonespec[static_cast<size_t> (ilpk)];
      float const ahavp = range_average (ihlo, ihhi, ihpk);
      float const alavp = range_average (illo, ilhi, ilpk);
      float const trath = ah / (ahavp + 0.01f);
      float const tratl = al / (alavp + 0.01f);
      float const ferrh = (static_cast<float> (ihpk) + deltah - static_cast<float> (i4000)) * df / 2.0f;
      float const ferrl = (static_cast<float> (ilpk) + deltal - static_cast<float> (i2000)) * df / 2.0f;
      detmet[static_cast<size_t> (step)] = std::max (ah, al);
      detmet2[static_cast<size_t> (step)] = std::max (trath, tratl);
      detfer[static_cast<size_t> (step)] = ah >= al ? ferrh : ferrl;
    }

  std::vector<float> ordered = detmet;
  std::sort (ordered.begin (), ordered.end ());
  int const rank = std::max (1, nstep / 4);
  float const xmed = ordered[static_cast<size_t> (rank - 1)] > 0.0f ? ordered[static_cast<size_t> (rank - 1)] : 1.0f;
  for (float& value : detmet)
    {
      value /= xmed;
    }

  std::array<int, kMsk144SpdMaxCand> nstart {};
  std::array<float, kMsk144SpdMaxCand> ferrs {};
  std::array<float, kMsk144SpdMaxCand> snrs {};
  int ndet = 0;
  for (int pass = 0; pass < kMsk144SpdMaxCand; ++pass)
    {
      auto it = std::max_element (detmet.begin (), detmet.end ());
      if (it == detmet.end () || *it < 3.5f)
        {
          break;
        }
      int const index = static_cast<int> (std::distance (detmet.begin (), it));
      if (std::abs (detfer[static_cast<size_t> (index)]) <= ntol)
        {
          nstart[static_cast<size_t> (ndet)] = index * 60 + 2;
          ferrs[static_cast<size_t> (ndet)] = detfer[static_cast<size_t> (index)];
          snrs[static_cast<size_t> (ndet)] =
              6.0f * std::log10 (std::max (*it, 1.0e-6f)) - 9.0f;
          ++ndet;
        }
      *it = 0.0f;
      if (ndet >= kMsk144SpdMaxCand)
        {
          break;
        }
    }

  if (ndet < 3)
    {
      for (int pass = 0; pass < kMsk144SpdMaxCand - ndet; ++pass)
        {
          auto it = std::max_element (detmet2.begin (), detmet2.end ());
          if (it == detmet2.end () || *it < 12.0f)
            {
              break;
            }
          int const index = static_cast<int> (std::distance (detmet2.begin (), it));
          if (std::abs (detfer[static_cast<size_t> (index)]) <= ntol)
            {
              nstart[static_cast<size_t> (ndet)] = index * 60 + 2;
              ferrs[static_cast<size_t> (ndet)] = detfer[static_cast<size_t> (index)];
              snrs[static_cast<size_t> (ndet)] =
                  6.0f * std::log10 (std::max (*it, 1.0e-6f)) - 9.0f;
              ++ndet;
            }
          *it = 0.0f;
          if (ndet >= kMsk144SpdMaxCand)
            {
              break;
            }
        }
    }

  static std::array<std::array<int, 3>, 6> const navpatterns {{
      {{0, 1, 0}},
      {{1, 0, 0}},
      {{0, 0, 1}},
      {{1, 1, 0}},
      {{0, 1, 1}},
      {{1, 1, 1}},
  }};

  for (int candidate = 0; candidate < ndet; ++candidate)
    {
      int ib1 = std::max (1, nstart[static_cast<size_t> (candidate)] - kMsk40Nspm);
      int ie1 = ib1 - 1 + 3 * kMsk40Nspm;
      if (ie1 > n)
        {
          ie1 = n;
          ib1 = ie1 - 3 * kMsk40Nspm + 1;
        }
      int const ib0 = ib1 - 1;
      std::array<Complex, 3 * kMsk40Nspm> cdat {};
      std::copy_n (cbig + ib0, cdat.size (), cdat.begin ());

      float const fo = fc + ferrs[static_cast<size_t> (candidate)];
      float const xsnr = snrs[static_cast<size_t> (candidate)];
      for (auto const& navmask : navpatterns)
        {
          Sync40Result const sync = sync40_native (cdat.data (), 3, 29, 7.2f, navmask.data (), 2, fo);
          if (!sync.ok)
            {
              continue;
            }

          for (int ipk = 0; ipk < 2; ++ipk)
            {
              for (int is = 0; is < 3; ++is)
                {
                  int ic0 = sync.npklocs[static_cast<size_t> (ipk)];
                  if (is == 1)
                    {
                      ic0 = std::max (1, ic0 - 1);
                    }
                  else if (is == 2)
                    {
                      ic0 = std::min (kMsk40Nspm, ic0 + 1);
                    }
                  auto shifted = cshift_left (sync.c, ic0 - 1);

                  FixedChars<37> message = blank_fixed<37> ();
                  int nsuccess = 0;
                  if (decodeframe40_native (shifted, mycall, hiscall, xsnr, bswl,
                                            recent_calls, nhasharray, message, nsuccess))
                    {
                      result.ok = true;
                      result.nsuccess = nsuccess;
                      result.msgreceived = message;
                      result.tret = static_cast<float> (nstart[static_cast<size_t> (candidate)] + kMsk40Nspm / 2)
                                  / static_cast<float> (kMsk144SampleRate);
                      result.fret = sync.fest;
                      result.navg = std::accumulate (navmask.begin (), navmask.end (), 0);
                      return result;
                    }
                }
            }
        }
    }

  return result;
}

template <size_t N>
void apply_constant_tweak (Complex const* in, Complex* out, float f0)
{
  float const dphi = kTwoPi * f0 / static_cast<float> (kMsk144SampleRate);
  Complex const wstep {std::cos (dphi), std::sin (dphi)};
  Complex w {1.0f, 0.0f};
  for (size_t i = 0; i < N; ++i)
    {
      w *= wstep;
      out[i] = static_cast<Complex> (w * in[i]);
    }
}

template <size_t N>
void apply_constant_tweak (std::array<Complex, N> const& in, std::array<Complex, N>& out, float f0)
{
  apply_constant_tweak<N> (in.data (), out.data (), f0);
}

template <int NFFT>
bool analytic_signal (float const* d, int npts, std::array<double, 5> const& pc,
                      bool beq, std::array<Complex, NFFT>& out)
{
  if (!d || npts <= 0 || npts > NFFT)
    {
      return false;
    }

  auto& fft = fft_workspace<NFFT> ();
  auto const df = static_cast<float> (kMsk144SampleRate) / static_cast<float> (NFFT);
  auto const nh = NFFT / 2;
  auto const fac = 2.0f / static_cast<float> (NFFT);

  for (int i = 0; i < npts; ++i)
    {
      fft.data[static_cast<size_t> (i)] = {fac * d[i], 0.0f};
    }
  for (int i = npts; i < NFFT; ++i)
    {
      fft.data[static_cast<size_t> (i)] = {};
    }

  fftwf_execute (fft.forward);

  for (int i = 0; i <= nh; ++i)
    {
      float const ff = static_cast<float> (i) * df;
      float const f = ff - 1500.0f;
      float h = 1.0f;
      float const t = 1.0f / 2000.0f;
      float const beta = 0.1f;
      if (std::abs (f) > (1.0f - beta) / (2.0f * t) && std::abs (f) <= (1.0f + beta) / (2.0f * t))
        {
          h *= 0.5f * (1.0f + std::cos ((static_cast<float> (M_PI) * t / beta)
                                        * (std::abs (f) - (1.0f - beta) / (2.0f * t))));
        }
      else if (std::abs (f) > (1.0f + beta) / (2.0f * t))
        {
          h = 0.0f;
        }

      Complex correction {1.0f, 0.0f};
      if (beq)
        {
          double const fp = static_cast<double> (f) / 1000.0;
          double const pd = fp * fp * (pc[2] + fp * (pc[3] + fp * pc[4]));
          correction = {std::cos (static_cast<float> (pd)), std::sin (static_cast<float> (pd))};
        }
      fft.data[static_cast<size_t> (i)] *= h * correction;
    }

  fft.data[0] *= 0.5f;
  for (int i = nh + 1; i < NFFT; ++i)
    {
      fft.data[static_cast<size_t> (i)] = {};
    }

  fftwf_execute (fft.inverse);
  out = fft.data;
  return true;
}

QString format_msk144_line (int nutc, int nsnr, float tdec, float fest, char const* decsym,
                            std::string const& message)
{
  std::array<char, 96> buffer {};
  std::snprintf (buffer.data (), buffer.size (), "%06d%4d%5.1f%5d%s%-37.37s",
                 nutc, nsnr, tdec, nint_compat (fest), decsym, message.c_str ());
  QByteArray line {buffer.data ()};
  while (!line.isEmpty () && line.back () == ' ')
    {
      line.chop (1);
    }
  return QString::fromLatin1 (line);
}

bool msk144_row_in_pick_window (QString const& row, decodium::msk144::DecodeRequest const& request)
{
  if (request.npick <= 0)
    {
      return true;
    }

  if (row.size () < 15)
    {
      return false;
    }

  bool ok = false;
  double const tdec = row.mid (10, 5).trimmed ().toDouble (&ok);
  if (!ok)
    {
      return false;
    }

  double const t0 = std::max (0, request.t0_ms) * 0.001;
  double const t1 = std::max (request.t0_ms, request.t1_ms) * 0.001;
  return tdec >= t0 && tdec <= t1;
}

bool decodeframe_native (std::array<Complex, kMsk144Nspm> const& input_frame,
                         std::array<float, kMsk144Bits>& softbits_out,
                         FixedChars<37>& msgreceived_out, int& nharderror_out)
{
  auto const& pp = half_sine_pulse ();
  auto const& cb = sync_waveform ();
  auto const& s8 = sync_word_bits ();

  msgreceived_out = blank_fixed<37> ();
  nharderror_out = -1;

  std::array<Complex, kMsk144Nspm> frame = input_frame;
  Complex cca {};
  Complex ccb {};
  for (int i = 0; i < 42; ++i)
    {
      cca += frame[static_cast<size_t> (i)] * std::conj (cb[static_cast<size_t> (i)]);
      ccb += frame[static_cast<size_t> (336 + i)] * std::conj (cb[static_cast<size_t> (i)]);
    }
  float const phase0 = std::atan2 (std::imag (cca + ccb), std::real (cca + ccb));
  Complex const cfac {std::cos (phase0), std::sin (phase0)};
  for (Complex& sample : frame)
    {
      sample *= std::conj (cfac);
    }

  softbits_out[0] = 0.0f;
  for (int i = 0; i < 6; ++i)
    {
      softbits_out[0] += std::imag (frame[static_cast<size_t> (i)]) * pp[static_cast<size_t> (i + 6)];
      softbits_out[0] += std::imag (frame[static_cast<size_t> (kMsk144Nspm - 6 + i)]) * pp[static_cast<size_t> (i)];
    }
  softbits_out[1] = 0.0f;
  for (int i = 0; i < 12; ++i)
    {
      softbits_out[1] += std::real (frame[static_cast<size_t> (i)]) * pp[static_cast<size_t> (i)];
    }

  for (int symbol = 1; symbol < 72; ++symbol)
    {
      float odd = 0.0f;
      float even = 0.0f;
      int const odd_start = symbol * 12 - 6;
      int const even_start = symbol * 12;
      for (int i = 0; i < 12; ++i)
        {
          odd += std::imag (frame[static_cast<size_t> (odd_start + i)]) * pp[static_cast<size_t> (i)];
          even += std::real (frame[static_cast<size_t> (even_start + i)]) * pp[static_cast<size_t> (i)];
        }
      softbits_out[static_cast<size_t> (2 * symbol)] = odd;
      softbits_out[static_cast<size_t> (2 * symbol + 1)] = even;
    }

  std::array<int, kMsk144Bits> hardbits {};
  for (int i = 0; i < kMsk144Bits; ++i)
    {
      hardbits[static_cast<size_t> (i)] = softbits_out[static_cast<size_t> (i)] >= 0.0f ? 1 : 0;
    }

  int sum1 = 0;
  int sum2 = 0;
  for (int i = 0; i < 8; ++i)
    {
      sum1 += (2 * hardbits[static_cast<size_t> (i)] - 1) * s8[static_cast<size_t> (i)];
      sum2 += (2 * hardbits[static_cast<size_t> (56 + i)] - 1) * s8[static_cast<size_t> (i)];
    }
  int const nbadsync1 = (8 - sum1) / 2;
  int const nbadsync2 = (8 - sum2) / 2;
  if (nbadsync1 + nbadsync2 > 4)
    {
      return false;
    }

  float mean = std::accumulate (softbits_out.begin (), softbits_out.end (), 0.0f)
               / static_cast<float> (softbits_out.size ());
  float sq = 0.0f;
  for (float value : softbits_out)
    {
      sq += value * value;
    }
  float const sigma_soft = std::sqrt (std::max (0.0f, sq / static_cast<float> (softbits_out.size ()) - mean * mean));
  if (!std::isfinite (sigma_soft) || sigma_soft <= 1.0e-6f)
    {
      return false;
    }
  for (float& value : softbits_out)
    {
      value /= sigma_soft;
    }

  std::array<float, kMsk144CodewordBits> llr {};
  for (int i = 0; i < 48; ++i)
    {
      llr[static_cast<size_t> (i)] = softbits_out[static_cast<size_t> (8 + i)];
    }
  for (int i = 0; i < 80; ++i)
    {
      llr[static_cast<size_t> (48 + i)] = softbits_out[static_cast<size_t> (64 + i)];
    }
  float const llr_scale = 2.0f / (0.60f * 0.60f);
  for (float& value : llr)
    {
      value *= llr_scale;
    }

  std::array<signed char, kMsk144CodewordBits> apmask {};
  std::array<signed char, kMsk144MessageBits> decoded77 {};
  std::array<signed char, kMsk144CodewordBits> cw {};
  int max_iterations = 10;
  int niterations = 0;
  int nharderror = -1;
  if (!bpdecode128_90_native (llr, apmask, max_iterations, decoded77, cw, nharderror, niterations)
      || nharderror < 0 || nharderror >= 18)
    {
      return false;
    }

  auto bits_to_int = [&decoded77] (int offset) {
    int value = 0;
    for (int i = 0; i < 3; ++i)
      {
        value = (value << 1) | (decoded77[static_cast<size_t> (offset + i)] != 0 ? 1 : 0);
      }
    return value;
  };
  int const n3 = bits_to_int (71);
  int const i3 = bits_to_int (74);
  if ((i3 == 0 && (n3 == 1 || n3 == 3 || n3 == 4 || n3 > 5)) || i3 == 3 || i3 > 5)
    {
      return false;
    }

  std::array<char, 37> msg37 {};
  int quirky = 0;
  if (legacy_pack77_unpack77bits_c (decoded77.data (), 1, msg37.data (), &quirky) == 0)
    {
      return false;
    }

  msgreceived_out = fixed_from_string<37> (trim_block (msg37.data (), 37));
  nharderror_out = nharderror;
  return true;
}

struct SyncResult
{
  bool ok {false};
  float fest {0.0f};
  float xmax {0.0f};
  std::array<int, 10> npklocs {};
  std::array<Complex, kMsk144Nspm> c {};
};

SyncResult sync_native (Complex const* cdat, int nframes, int ntol, float delf,
                        int const* navmask, int npeaks, float fc)
{
  SyncResult result;
  result.npklocs.fill (1);

  int const navg = std::accumulate (navmask, navmask + nframes, 0);
  if (navg <= 0)
    {
      return result;
    }

  int const n = nframes * kMsk144Nspm;
  float const fac = 1.0f / (48.0f * std::sqrt (static_cast<float> (navg)));
  int const span = nint_compat (static_cast<float> (ntol) / delf);
  std::array<float, kMsk144Nspm> best_xcc {};

  std::vector<Complex> shifted (static_cast<size_t> (n));
  for (int ifr = -span; ifr <= span; ++ifr)
    {
      float const ferr = static_cast<float> (ifr) * delf;
      float const dphi = kTwoPi * (-(fc + ferr)) / static_cast<float> (kMsk144SampleRate);
      Complex const wstep {std::cos (dphi), std::sin (dphi)};
      Complex w {1.0f, 0.0f};
      for (int i = 0; i < n; ++i)
        {
          w *= wstep;
          shifted[static_cast<size_t> (i)] = w * cdat[static_cast<size_t> (i)];
        }

      std::array<Complex, kMsk144Nspm> averaged {};
      averaged.fill (Complex {});
      for (int frame = 0; frame < nframes; ++frame)
        {
          if (navmask[frame] == 0)
            {
              continue;
            }
          int const start = frame * kMsk144Nspm;
          for (int i = 0; i < kMsk144Nspm; ++i)
            {
              averaged[static_cast<size_t> (i)] += shifted[static_cast<size_t> (start + i)];
            }
        }

      std::array<Complex, 2 * kMsk144Nspm> duplicated {};
      for (int i = 0; i < kMsk144Nspm; ++i)
        {
          duplicated[static_cast<size_t> (i)] = averaged[static_cast<size_t> (i)];
          duplicated[static_cast<size_t> (kMsk144Nspm + i)] = averaged[static_cast<size_t> (i)];
        }

      std::array<float, kMsk144Nspm> xcc {};
      auto const& cb = sync_waveform ();
      for (int shift = 0; shift < kMsk144Nspm; ++shift)
        {
          Complex accum {};
          for (int i = 0; i < 42; ++i)
            {
              accum += (duplicated[static_cast<size_t> (shift + i)]
                        + duplicated[static_cast<size_t> (shift + 336 + i)])
                       * std::conj (cb[static_cast<size_t> (i)]);
            }
          xcc[static_cast<size_t> (shift)] = safe_abs (accum);
        }

      float const peak = *std::max_element (xcc.begin (), xcc.end ()) * fac;
      if (peak > result.xmax)
        {
          result.xmax = peak;
          result.fest = fc + ferr;
          result.c = averaged;
          best_xcc = xcc;
        }
    }

  if (result.xmax < 1.3f)
    {
      return result;
    }

  for (int peak_index = 0; peak_index < npeaks; ++peak_index)
    {
      auto it = std::max_element (best_xcc.begin (), best_xcc.end ());
      int const location = static_cast<int> (std::distance (best_xcc.begin (), it)) + 1;
      result.npklocs[static_cast<size_t> (peak_index)] = location;
      int const lo = std::max (0, location - 1 - 7);
      int const hi = std::min (kMsk144Nspm - 1, location - 1 + 7);
      for (int i = lo; i <= hi; ++i)
        {
          best_xcc[static_cast<size_t> (i)] = 0.0f;
        }
    }
  result.ok = true;
  return result;
}

struct SpdResult
{
  bool ok {false};
  FixedChars<37> msgreceived = blank_fixed<37> ();
  float fret {0.0f};
  float tret {0.0f};
  int navg {0};
  std::array<Complex, kMsk144Nspm> ct {};
  std::array<float, kMsk144Bits> softbits {};
};

SpdResult spd_native (Complex const* cbig, int n, int ntol, float fc)
{
  SpdResult result;

  auto const& rcw = raised_cosine_window ();
  auto& fft = fft_workspace<kMsk144Nspm> ();
  int const nstep = (n - kMsk144Nspm) / 216;
  if (nstep <= 0)
    {
      return result;
    }

  std::vector<float> detmet (static_cast<size_t> (nstep), 0.0f);
  std::vector<float> detmet2 (static_cast<size_t> (nstep), 0.0f);
  std::vector<float> detfer (static_cast<size_t> (nstep), -999.99f);
  float const df = static_cast<float> (kMsk144SampleRate) / static_cast<float> (kMsk144Nspm);
  float const nfhi = 2.0f * (fc + 500.0f);
  float const nflo = 2.0f * (fc - 500.0f);
  int const ihlo = clamp_value (nint_compat ((nfhi - 2.0f * ntol) / df), 1, kMsk144Nspm - 2);
  int const ihhi = clamp_value (nint_compat ((nfhi + 2.0f * ntol) / df), 1, kMsk144Nspm - 2);
  int const illo = clamp_value (nint_compat ((nflo - 2.0f * ntol) / df), 1, kMsk144Nspm - 2);
  int const ilhi = clamp_value (nint_compat ((nflo + 2.0f * ntol) / df), 1, kMsk144Nspm - 2);
  int const i2000 = clamp_value (nint_compat (nflo / df), 1, kMsk144Nspm - 2);
  int const i4000 = clamp_value (nint_compat (nfhi / df), 1, kMsk144Nspm - 2);

  for (int step = 0; step < nstep; ++step)
    {
      int const ns = step * 216;
      if (ns + kMsk144Nspm > n)
        {
          break;
        }

      fft.data.fill (Complex {});
      for (int i = 0; i < kMsk144Nspm; ++i)
        {
          fft.data[static_cast<size_t> (i)] = cbig[static_cast<size_t> (ns + i)];
        }

      for (Complex& value : fft.data)
        {
          value *= value;
        }
      for (int i = 0; i < 12; ++i)
        {
          fft.data[static_cast<size_t> (i)] *= rcw[static_cast<size_t> (i)];
          fft.data[static_cast<size_t> (kMsk144Nspm - 12 + i)] *= rcw[static_cast<size_t> (11 - i)];
        }
      fftwf_execute (fft.forward);

      std::array<float, kMsk144Nspm> tonespec {};
      for (int i = 0; i < kMsk144Nspm; ++i)
        {
          tonespec[static_cast<size_t> (i)] = std::norm (fft.data[static_cast<size_t> (i)]);
        }

      auto best_in_range = [&tonespec] (int lo, int hi) {
        int best = lo;
        for (int i = lo + 1; i <= hi; ++i)
          {
            if (tonespec[static_cast<size_t> (i)] > tonespec[static_cast<size_t> (best)])
              {
                best = i;
              }
          }
        return best;
      };
      auto range_average = [&tonespec] (int lo, int hi, int peak) {
        float sum = 0.0f;
        int count = 0;
        for (int i = lo; i <= hi; ++i)
          {
            if (i == peak)
              {
                continue;
              }
            sum += tonespec[static_cast<size_t> (i)];
            ++count;
          }
        return count > 0 ? sum / static_cast<float> (count) : 0.0f;
      };
      auto parabola_delta = [&fft] (int peak) {
        Complex const numerator = fft.data[static_cast<size_t> (peak - 1)] - fft.data[static_cast<size_t> (peak + 1)];
        Complex const denominator = 2.0f * fft.data[static_cast<size_t> (peak)]
                                  - fft.data[static_cast<size_t> (peak - 1)]
                                  - fft.data[static_cast<size_t> (peak + 1)];
        if (std::norm (denominator) < 1.0e-12f)
          {
            return 0.0f;
          }
        return -std::real (numerator / denominator);
      };

      int const ihpk = best_in_range (ihlo, ihhi);
      int const ilpk = best_in_range (illo, ilhi);
      float const deltah = parabola_delta (ihpk);
      float const deltal = parabola_delta (ilpk);
      float const ah = tonespec[static_cast<size_t> (ihpk)];
      float const al = tonespec[static_cast<size_t> (ilpk)];
      float const ahavp = range_average (ihlo, ihhi, ihpk);
      float const alavp = range_average (illo, ilhi, ilpk);
      float const trath = ah / (ahavp + 0.01f);
      float const tratl = al / (alavp + 0.01f);
      float const ferrh = (static_cast<float> (ihpk) + deltah - static_cast<float> (i4000)) * df / 2.0f;
      float const ferrl = (static_cast<float> (ilpk) + deltal - static_cast<float> (i2000)) * df / 2.0f;
      detmet[static_cast<size_t> (step)] = std::max (ah, al);
      detmet2[static_cast<size_t> (step)] = std::max (trath, tratl);
      detfer[static_cast<size_t> (step)] = ah >= al ? ferrh : ferrl;
    }

  std::vector<float> ordered = detmet;
  std::sort (ordered.begin (), ordered.end ());
  int const rank = std::max (1, nstep / 4);
  float const xmed = ordered[static_cast<size_t> (rank - 1)] > 0.0f ? ordered[static_cast<size_t> (rank - 1)] : 1.0f;
  for (float& value : detmet)
    {
      value /= xmed;
    }

  std::array<int, kMsk144SpdMaxCand> nstart {};
  std::array<float, kMsk144SpdMaxCand> ferrs {};
  int ndet = 0;
  for (int pass = 0; pass < kMsk144SpdMaxCand; ++pass)
    {
      auto it = std::max_element (detmet.begin (), detmet.end ());
      if (it == detmet.end () || *it < 3.0f)
        {
          break;
        }
      int const index = static_cast<int> (std::distance (detmet.begin (), it));
      if (std::abs (detfer[static_cast<size_t> (index)]) <= ntol)
        {
          nstart[static_cast<size_t> (ndet)] = index * 216 + 2;
          ferrs[static_cast<size_t> (ndet)] = detfer[static_cast<size_t> (index)];
          ++ndet;
        }
      *it = 0.0f;
      if (ndet >= kMsk144SpdMaxCand)
        {
          break;
        }
    }

  if (ndet < 3)
    {
      for (int pass = 0; pass < kMsk144SpdMaxCand - ndet; ++pass)
        {
          auto it = std::max_element (detmet2.begin (), detmet2.end ());
          if (it == detmet2.end () || *it < 12.0f)
            {
              break;
            }
          int const index = static_cast<int> (std::distance (detmet2.begin (), it));
          if (std::abs (detfer[static_cast<size_t> (index)]) <= ntol)
            {
              nstart[static_cast<size_t> (ndet)] = index * 216 + 2;
              ferrs[static_cast<size_t> (ndet)] = detfer[static_cast<size_t> (index)];
              ++ndet;
            }
          *it = 0.0f;
          if (ndet >= kMsk144SpdMaxCand)
            {
              break;
            }
        }
    }

  static std::array<std::array<int, 3>, kMsk144SpdPatterns> const navpatterns {{
      {{0, 1, 0}},
      {{1, 0, 0}},
      {{0, 0, 1}},
      {{1, 1, 0}},
      {{0, 1, 1}},
      {{1, 1, 1}},
  }};

  for (int candidate = 0; candidate < ndet; ++candidate)
    {
      int ib1 = std::max (1, nstart[static_cast<size_t> (candidate)] - kMsk144Nspm);
      int ie1 = ib1 - 1 + 3 * kMsk144Nspm;
      if (ie1 > n)
        {
          ie1 = n;
          ib1 = ie1 - 3 * kMsk144Nspm + 1;
        }
      int const ib0 = ib1 - 1;
      std::array<Complex, 3 * kMsk144Nspm> cdat {};
      std::copy_n (cbig + ib0, cdat.size (), cdat.begin ());

      float const fo = fc + ferrs[static_cast<size_t> (candidate)];
      for (auto const& navmask : navpatterns)
        {
          SyncResult const sync = sync_native (cdat.data (), 3, 8, 2.0f, navmask.data (), 2, fo);
          if (!sync.ok)
            {
              continue;
            }

          for (int ipk = 0; ipk < 2; ++ipk)
            {
              for (int is = 0; is < 3; ++is)
                {
                  int ic0 = sync.npklocs[static_cast<size_t> (ipk)];
                  if (is == 1)
                    {
                      ic0 = std::max (1, ic0 - 1);
                    }
                  else if (is == 2)
                    {
                      ic0 = std::min (kMsk144Nspm, ic0 + 1);
                    }
                  auto shifted = cshift_left (sync.c, ic0 - 1);

                  int nharderror = -1;
                  if (decodeframe_native (shifted, result.softbits, result.msgreceived, nharderror))
                    {
                      result.tret = static_cast<float> (nstart[static_cast<size_t> (candidate)] + kMsk144Nspm / 2)
                                    / static_cast<float> (kMsk144SampleRate);
                      result.fret = sync.fest;
                      result.navg = std::accumulate (navmask.begin (), navmask.end (), 0);
                      result.ct = shifted;
                      result.ok = true;
                      return result;
                    }
                }
            }
        }
    }

  return result;
}

template <int N>
void fft_forward_copy (std::array<Complex, N> const& in, std::array<Complex, N>& out)
{
  auto& fft = fft_workspace<N> ();
  fft.data = in;
  fftwf_execute (fft.forward);
  out = fft.data;
}

bool weighted_polyfit_order4 (std::array<double, 145> const& x,
                              std::array<double, 145> const& y,
                              std::array<double, 145> const& sigma,
                              std::array<double, 5>& coeffs,
                              double& chisqr)
{
  std::array<std::array<double, 6>, 5> a {};
  for (size_t i = 0; i < x.size (); ++i)
    {
      double const s = std::max (1.0e-12, sigma[i]);
      double const w = 1.0 / (s * s);
      std::array<double, 9> xp {};
      xp[0] = 1.0;
      for (size_t p = 1; p < xp.size (); ++p)
        {
          xp[p] = xp[p - 1] * x[i];
        }
      for (int row = 0; row < 5; ++row)
        {
          for (int col = 0; col < 5; ++col)
            {
              a[static_cast<size_t> (row)][static_cast<size_t> (col)] += w * xp[static_cast<size_t> (row + col)];
            }
          a[static_cast<size_t> (row)][5] += w * y[i] * xp[static_cast<size_t> (row)];
        }
    }

  for (int pivot = 0; pivot < 5; ++pivot)
    {
      int best = pivot;
      for (int row = pivot + 1; row < 5; ++row)
        {
          if (std::abs (a[static_cast<size_t> (row)][static_cast<size_t> (pivot)])
              > std::abs (a[static_cast<size_t> (best)][static_cast<size_t> (pivot)]))
            {
              best = row;
            }
        }
      if (std::abs (a[static_cast<size_t> (best)][static_cast<size_t> (pivot)]) < 1.0e-18)
        {
          coeffs.fill (0.0);
          chisqr = std::numeric_limits<double>::infinity ();
          return false;
        }
      if (best != pivot)
        {
          std::swap (a[static_cast<size_t> (best)], a[static_cast<size_t> (pivot)]);
        }
      double const scale = a[static_cast<size_t> (pivot)][static_cast<size_t> (pivot)];
      for (int col = pivot; col < 6; ++col)
        {
          a[static_cast<size_t> (pivot)][static_cast<size_t> (col)] /= scale;
        }
      for (int row = 0; row < 5; ++row)
        {
          if (row == pivot)
            {
              continue;
            }
          double const factor = a[static_cast<size_t> (row)][static_cast<size_t> (pivot)];
          for (int col = pivot; col < 6; ++col)
            {
              a[static_cast<size_t> (row)][static_cast<size_t> (col)] -=
                  factor * a[static_cast<size_t> (pivot)][static_cast<size_t> (col)];
            }
        }
    }

  for (int i = 0; i < 5; ++i)
    {
      coeffs[static_cast<size_t> (i)] = a[static_cast<size_t> (i)][5];
    }

  chisqr = 0.0;
  for (size_t i = 0; i < x.size (); ++i)
    {
      double const fit = coeffs[0] + x[i] * (coeffs[1] + x[i] * (coeffs[2] + x[i] * (coeffs[3] + x[i] * coeffs[4])));
      double const s = std::max (1.0e-12, sigma[i]);
      double const residual = (y[i] - fit) / s;
      chisqr += residual * residual;
    }
  return true;
}

void signal_quality_native (std::array<Complex, kMsk144Nspm> const& cframe, float snr, float freq, float t0,
                            std::array<float, kMsk144Bits> const& softbits, FixedChars<37> const& msg,
                            FixedChars<12> const& dxcall, bool btrain, QString const& datadir,
                            std::array<double, 5>& pcoeffs, Msk144TrainingState& training,
                            int& nbiterrors, float& eyeopening, bool* training_completed_out)
{
  if (training_completed_out)
    {
      *training_completed_out = false;
    }

  std::array<int, kMsk144Bits> hardbits {};
  for (int i = 0; i < kMsk144Bits; ++i)
    {
      hardbits[static_cast<size_t> (i)] = softbits[static_cast<size_t> (i)] > 0.0f ? 1 : 0;
    }

  QString const decoded_message = QString::fromLatin1 (trim_fixed (msg).c_str ());
  auto const encoded = decodium::txmsg::encodeMsk144 (decoded_message);
  if (!encoded.ok || encoded.tones.size () < kMsk144Bits)
    {
      nbiterrors = 0;
      eyeopening = 0.0f;
      return;
    }

  std::array<int, kMsk144Bits> msgbits {};
  msgbits[0] = 0;
  for (int i = 0; i < kMsk144Bits - 1; ++i)
    {
      int const tone = encoded.tones.at (i);
      if (tone == 0)
        {
          msgbits[static_cast<size_t> (i + 1)] = (i % 2 == 0) ? msgbits[static_cast<size_t> (i)]
                                                               : (msgbits[static_cast<size_t> (i)] + 1) % 2;
        }
      else
        {
          msgbits[static_cast<size_t> (i + 1)] = (i % 2 == 0) ? (msgbits[static_cast<size_t> (i)] + 1) % 2
                                                               : msgbits[static_cast<size_t> (i)];
        }
    }

  nbiterrors = 0;
  float eyetop = 1.0f;
  float eyebot = -1.0f;
  for (int i = 0; i < kMsk144Bits; ++i)
    {
      if (hardbits[static_cast<size_t> (i)] != msgbits[static_cast<size_t> (i)])
        {
          ++nbiterrors;
        }
      if (msgbits[static_cast<size_t> (i)] == 1)
        {
          eyetop = std::min (eyetop, softbits[static_cast<size_t> (i)]);
        }
      else
        {
          eyebot = std::max (eyebot, softbits[static_cast<size_t> (i)]);
        }
    }
  eyeopening = eyetop - eyebot;

  std::string const dxcall_text = trim_fixed (dxcall);
  QString const dxcall_q = QString::fromStdString (dxcall_text);
  bool const msg_has_dxcall = !dxcall_text.empty () && decoded_message.indexOf (dxcall_q) >= 3;

  if ((training.currently_training && fixed_from_string<13> (dxcall_text) != training.training_dxcall)
      || training.navg > 10)
    {
      reset_training_state (training);
    }

  if (btrain && msg_has_dxcall && !training.currently_training)
    {
      training.currently_training = true;
      training.training_dxcall = fixed_from_string<13> (dxcall_text);
      training.trained_dxcall = blank_fixed<13> ();
    }

  if (msg_has_dxcall && training.currently_training)
    {
      training.trained_dxcall = blank_fixed<13> ();
      training.training_dxcall = fixed_from_string<13> (dxcall_text);
    }

  bool const is_training_frame = snr > 5.0f
                              && nbiterrors < 7
                              && std::abs (t0 - training.tlast) > 0.072f
                              && msg_has_dxcall;
  if (!training.currently_training || !is_training_frame)
    {
      return;
    }

  std::array<float, 1024> d {};
  float phi = -kTwoPi / 8.0f;
  int nsym = 144;
  if (encoded.tones.size () > 40 && encoded.tones.at (40) < 0)
    {
      nsym = 40;
    }
  int index = 0;
  for (int i = 0; i < nsym && i < encoded.tones.size (); ++i)
    {
      float const dphi = kTwoPi * (freq + (encoded.tones.at (i) == 0 ? -500.0f : 500.0f))
                       / static_cast<float> (kMsk144SampleRate);
      for (int j = 0; j < 6 && index < static_cast<int> (d.size ()); ++j)
        {
          d[static_cast<size_t> (index++)] = std::cos (phi);
          phi = std::fmod (phi + dphi, kTwoPi);
          if (phi < 0.0f)
            {
              phi += kTwoPi;
            }
        }
    }

  std::array<Complex, 1024> canalytic {};
  std::array<Complex, 1024> cmodel {};
  if (!analytic_signal<1024> (d.data (), 864, pcoeffs, false, canalytic))
    {
      return;
    }
  apply_constant_tweak<1024> (canalytic, cmodel, -freq);

  std::array<Complex, kMsk144Nspm> cframe_fft {};
  std::array<Complex, kMsk144Nspm> cmodel_fft {};
  fft_forward_copy<kMsk144Nspm> (cframe, cframe_fft);
  std::copy_n (cmodel.begin (), kMsk144Nspm, cmodel_fft.begin ());
  fft_forward_copy<kMsk144Nspm> (cmodel_fft, cmodel_fft);

  std::array<Complex, kMsk144Nspm> cross {};
  for (int i = 0; i < kMsk144Nspm; ++i)
    {
      cross[static_cast<size_t> (i)] =
          cmodel_fft[static_cast<size_t> (i)] * std::conj (cframe_fft[static_cast<size_t> (i)]) / 1000.0f;
    }
  cross = cshift_left (cross, kMsk144Nspm / 2);

  float const weight = std::pow (10.0f, snr / 20.0f);
  for (int i = 0; i < kMsk144Nspm; ++i)
    {
      training.cross_avg[static_cast<size_t> (i)] += weight * cross[static_cast<size_t> (i)];
    }
  training.wt_avg += weight;
  ++training.navg;
  training.tlast = t0;

  constexpr int kPolyPoints = 145;
  std::array<double, kPolyPoints> x {};
  std::array<double, kPolyPoints> y {};
  std::array<double, kPolyPoints> sigmay {};
  float const df = static_cast<float> (kMsk144SampleRate) / static_cast<float> (kMsk144Nspm);
  int const center = kMsk144Nspm / 2;
  int const half = kPolyPoints / 2;
  for (int i = 0; i < kPolyPoints; ++i)
    {
      x[static_cast<size_t> (i)] = static_cast<double> (i - half) * static_cast<double> (df) / 1000.0;
      Complex const sample = training.cross_avg[static_cast<size_t> (center - half + i)];
      y[static_cast<size_t> (i)] = std::atan2 (std::imag (sample), std::real (sample));
      double const mag = std::max<double> (1.0e-12, safe_abs (sample));
      sigmay[static_cast<size_t> (i)] = std::max<double> (1.0e-12, training.wt_avg / mag);
    }

  std::array<double, 5> coeffs {};
  double chisqr = 0.0;
  if (!weighted_polyfit_order4 (x, y, sigmay, coeffs, chisqr))
    {
      return;
    }

  double rmsdiff = 0.0;
  for (int i = 0; i < kPolyPoints; ++i)
    {
      double const fit = coeffs[0] + x[static_cast<size_t> (i)]
                       * (coeffs[1] + x[static_cast<size_t> (i)]
                       * (coeffs[2] + x[static_cast<size_t> (i)]
                       * (coeffs[3] + x[static_cast<size_t> (i)] * coeffs[4])));
      double const diff = fit - y[static_cast<size_t> (i)];
      rmsdiff += diff * diff;
    }
  rmsdiff /= static_cast<double> (kPolyPoints);

  if (std::sqrt (chisqr) < 1.8 && rmsdiff < 0.5 && training.navg >= 5)
    {
      training.trained_dxcall = fixed_from_string<13> (dxcall_text);
      QString const timestamp = QDateTime::currentDateTime ().toString (QStringLiteral ("yyMMdd_HHmmss"));
      QString const path = QDir {datadir}.filePath (
          QStringLiteral ("%1_%2.pcoeff").arg (QString::fromStdString (dxcall_text), timestamp));
      std::ofstream out {path.toLocal8Bit ().constData (), std::ios::out | std::ios::trunc};
      if (out)
        {
          out.setf (std::ios::scientific);
          out.precision (16);
          out << training.navg << ' ' << std::sqrt (chisqr) << ' ' << rmsdiff
              << ' ' << 500 << ' ' << 2500 << ' ' << 5;
          for (double coeff : coeffs)
            {
              out << ' ' << coeff;
            }
          out << '\n';
          for (int i = 0; i < kPolyPoints; ++i)
            {
              double const fit = coeffs[0] + x[static_cast<size_t> (i)]
                               * (coeffs[1] + x[static_cast<size_t> (i)]
                               * (coeffs[2] + x[static_cast<size_t> (i)]
                               * (coeffs[3] + x[static_cast<size_t> (i)] * coeffs[4])));
              out << x[static_cast<size_t> (i)] << ' ' << fit << ' '
                  << y[static_cast<size_t> (i)] << ' ' << sigmay[static_cast<size_t> (i)] << '\n';
            }
          for (int i = 0; i < kMsk144Nspm; ++i)
            {
              Complex const sample = cframe[static_cast<size_t> (i)];
              Complex const avg = training.cross_avg[static_cast<size_t> (i)];
              out << (i + 1) << ' ' << std::real (sample) << ' ' << std::imag (sample)
                  << ' ' << std::real (avg) << ' ' << std::imag (avg) << '\n';
            }
        }
      reset_training_state (training);
      if (training_completed_out)
        {
          *training_completed_out = true;
        }
    }
}

bool decode_block_native (short const* id2, int nutc0, float tsec, int ntol, int nrxfreq, int ndepth,
                          FixedChars<12> const& mycall, FixedChars<12> const& hiscall,
                          bool bshmsg, bool btrain, bool bswl,
                          std::array<double, 5> const& pcoeffs,
                          QString const& datadir, Msk144RuntimeState& state, QString* row_out,
                          bool* training_completed_out)
{
  if (!id2 || !row_out)
    {
      return false;
    }

  FixedChars<13> requested_mycall13 = blank_fixed<13> ();
  FixedChars<13> requested_dxcall13 = blank_fixed<13> ();
  std::copy_n (mycall.data (), 12, requested_mycall13.data ());
  std::copy_n (hiscall.data (), 12, requested_dxcall13.data ());

  if (!state.initialized || state.last_mycall13 != requested_mycall13 || state.last_dxcall13 != requested_dxcall13)
    {
      reset_runtime_state (state);
      state.tsec0 = tsec;
      state.nutc00 = nutc0;
      state.pnoise = -1.0f;
      state.last_mycall13 = requested_mycall13;
      state.last_dxcall13 = requested_dxcall13;
      state.initialized = true;
    }

  float const fc = static_cast<float> (nrxfreq);
  if (state.nutc00 != nutc0 || tsec < state.tsec0)
    {
      state.msglast = blank_fixed<37> ();
      state.msglastswl = blank_fixed<37> ();
      state.nsnrlast = -99;
      state.nsnrlastswl = -99;
      state.nutc00 = nutc0;
    }

  std::array<float, kMsk144Nfft1> d {};
  for (int i = 0; i < kMsk144BlockSize; ++i)
    {
      d[static_cast<size_t> (i)] = static_cast<float> (id2[i]);
    }
  float sq = 0.0f;
  for (int i = 0; i < kMsk144BlockSize; ++i)
    {
      sq += d[static_cast<size_t> (i)] * d[static_cast<size_t> (i)];
    }
  float const rms = std::sqrt (sq / static_cast<float> (kMsk144BlockSize));
  if (rms < 1.0f)
    {
      state.tsec0 = tsec;
      return false;
    }
  float const scale = 1.0f / rms;
  for (int i = 0; i < kMsk144BlockSize; ++i)
    {
      d[static_cast<size_t> (i)] *= scale;
    }
  for (int i = kMsk144BlockSize; i < kMsk144Nfft1; ++i)
    {
      d[static_cast<size_t> (i)] = 0.0f;
    }

  if (!analytic_signal<kMsk144Nfft1> (d.data (), kMsk144BlockSize, pcoeffs, !btrain, state.cdat))
    {
      state.tsec0 = tsec;
      return false;
    }

  std::array<float, 8> pow {};
  float pmax = -99.0f;
  for (int frame = 0; frame < 8; ++frame)
    {
      int const begin = frame * kMsk144Nspm;
      int const end = begin + kMsk144Nspm;
      pow[static_cast<size_t> (frame)] = power_sum (state.cdat, begin, end) * rms * rms;
      pmax = std::max (pmax, pow[static_cast<size_t> (frame)]);
    }
  float const pavg = std::accumulate (pow.begin (), pow.end (), 0.0f) / 8.0f;

  SpdResult short_ping = spd_native (state.cdat.data (), 8 * kMsk144Nspm, ntol, fc);
  bool bshdecode = false;
  int ndecodesuccess = short_ping.ok ? 1 : 0;
  FixedChars<37> msgreceived = short_ping.msgreceived;
  float fest = short_ping.fret;
  float tdec = short_ping.tret;
  int navg = short_ping.navg;
  std::array<Complex, kMsk144Nspm> ct = short_ping.ct;
  std::array<float, kMsk144Bits> softbits = short_ping.softbits;

  if (!short_ping.ok)
    {
      if (bshmsg || bswl)
        {
          Msk40SpdResult const shorthand = spd40_native (state.cdat.data (), 8 * kMsk144Nspm, ntol,
                                                         mycall, hiscall, bswl,
                                                         state.recent_calls, state.nhasharray, fc);
          if (shorthand.ok)
            {
              bshdecode = true;
              ndecodesuccess = shorthand.nsuccess;
              msgreceived = shorthand.msgreceived;
              fest = shorthand.fret;
              tdec = tsec + shorthand.tret;
              navg = shorthand.navg;
            }
        }
    }

  if (!short_ping.ok && !bshdecode)
    {
      static std::array<std::array<int, 8>, kMsk144AvgPatterns> const iavpatterns {{
          {{1, 1, 1, 1, 0, 0, 0, 0}},
          {{0, 0, 1, 1, 1, 1, 0, 0}},
          {{1, 1, 1, 1, 1, 0, 0, 0}},
          {{1, 1, 1, 1, 1, 1, 1, 0}},
      }};
      static std::array<float, kMsk144AvgPatterns> const xmc {{2.0f, 4.5f, 2.5f, 3.5f}};
      int npat = kMsk144AvgPatterns;
      if (ndepth == 1)
        {
          npat = 0;
        }
      else if (ndepth == 2)
        {
          npat = 2;
        }

      float const tframe = static_cast<float> (kMsk144Nspm) / static_cast<float> (kMsk144SampleRate);
      for (int iavg = 0; iavg < npat; ++iavg)
        {
          int const navmask_sum = std::accumulate (iavpatterns[static_cast<size_t> (iavg)].begin (),
                                                   iavpatterns[static_cast<size_t> (iavg)].end (), 0);
          if (navmask_sum <= 0)
            {
              continue;
            }
          SyncResult const sync = sync_native (state.cdat.data (), 8, ntol,
                                               10.0f / static_cast<float> (navmask_sum),
                                               iavpatterns[static_cast<size_t> (iavg)].data (), 2, fc);
          if (!sync.ok)
            {
              continue;
            }
          for (int ipk = 0; ipk < 2; ++ipk)
            {
              for (int is = 0; is < 3; ++is)
                {
                  int ic0 = sync.npklocs[static_cast<size_t> (ipk)];
                  if (is == 1)
                    {
                      ic0 = std::max (1, ic0 - 1);
                    }
                  else if (is == 2)
                    {
                      ic0 = std::min (kMsk144Nspm, ic0 + 1);
                    }

                  auto shifted = cshift_left (sync.c, ic0 - 1);
                  int nharderror = -1;
                  if (decodeframe_native (shifted, softbits, msgreceived, nharderror))
                    {
                      ct = shifted;
                      tdec = tsec + xmc[static_cast<size_t> (iavg)] * tframe;
                      fest = sync.fest;
                      navg = navmask_sum;
                      ndecodesuccess = 1;
                      break;
                    }
                }
              if (ndecodesuccess > 0)
                {
                  break;
                }
            }
          if (ndecodesuccess > 0)
            {
              break;
            }
        }
    }
  else if (short_ping.ok)
    {
      tdec = tsec + tdec;
    }

  if (ndecodesuccess == 0)
    {
      if (state.pnoise < 0.0f)
        {
          state.pnoise = pavg;
        }
      else if (pavg > state.pnoise)
        {
          state.pnoise = 0.9f * state.pnoise + 0.1f * pavg;
        }
      else if (pavg < state.pnoise)
        {
          state.pnoise = pavg;
        }
      state.tsec0 = tsec;
      return false;
    }

  float snr0 = 0.0f;
  if (state.pnoise > 0.0f)
    {
      float const ratio = pmax / state.pnoise - 1.0f;
      if (ratio > 1.0e-6f)
        {
          snr0 = 10.0f * std::log10 (ratio);
        }
    }
  int nsnr = nint_compat (snr0);

  int ncorrected = 0;
  float eyeopening = 0.0f;
  bool training_completed = false;
  if (!bshdecode)
    {
      auto pcoeffs_copy = pcoeffs;
      signal_quality_native (ct, snr0, fest, tdec, softbits, msgreceived, hiscall,
                             btrain, datadir, pcoeffs_copy, state.training,
                             ncorrected, eyeopening, &training_completed);
    }
  else
    {
      ncorrected = 0;
      eyeopening = 0.0f;
    }
  if (training_completed_out)
    {
      *training_completed_out = training_completed;
    }
  Q_UNUSED (ncorrected);
  Q_UNUSED (eyeopening);
  Q_UNUSED (navg);

  if (nsnr < -8)
    {
      nsnr = -8;
    }
  else if (nsnr > 24)
    {
      nsnr = 24;
    }

  char const* decsym = btrain ? " ^  " : " &  ";
  bool const emit_primary = ndecodesuccess == 1
                            && (msgreceived != state.msglast || nsnr > state.nsnrlast || tsec < state.tsec0);
  if (emit_primary)
    {
      state.msglast = msgreceived;
      state.nsnrlast = nsnr;
      if (!bshdecode)
        {
          maybe_update_recent_calls_from_message (msgreceived, state.recent_calls);
          update_msk40_hasharray_native (state.recent_calls, state.nhasharray);
        }
      *row_out = format_msk144_line (nutc0, nsnr, tdec, fest, decsym,
                                     trim_fixed (msgreceived));
      state.tsec0 = tsec;
      return !row_out->isEmpty ();
    }

  if (bswl && ndecodesuccess >= 2)
    {
      bool seenb4 = false;
      for (auto const& item : state.recent_shmsgs)
        {
          if (item == msgreceived)
            {
              seenb4 = true;
              break;
            }
        }
      update_recent_shmsgs_native (msgreceived, state.recent_shmsgs);
      bool const emit_swl = seenb4
                         && (msgreceived != state.msglastswl || nsnr > state.nsnrlastswl || tsec < state.tsec0)
                         && nsnr > -6;
      if (emit_swl)
        {
          state.msglastswl = msgreceived;
          state.nsnrlastswl = nsnr;
          *row_out = format_msk144_line (nutc0, nsnr, tdec, fest, decsym, trim_fixed (msgreceived));
          state.tsec0 = tsec;
          return !row_out->isEmpty ();
        }
    }

  state.tsec0 = tsec;
  return false;
}

} // namespace

namespace decodium
{
namespace msk144
{

MSK144DecodeWorker::MSK144DecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void resetMsk144DecoderState ()
{
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};
  reset_runtime_state (shared_worker_state ());
  reset_runtime_state (shared_live_state ());
}

QStringList decodeMsk144Rows (DecodeRequest const& request)
{
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};

  QStringList rows;
  rows.reserve (kMsk144WorkerMaxLines);

  int sample_count = request.audio.size ();
  if (request.kdone > 0)
    {
      sample_count = std::min (sample_count, request.kdone);
    }
  if (request.trperiod > 0.0)
    {
      int const tr_samples = qMax (0, qRound (request.trperiod * kMsk144SampleRate));
      if (tr_samples > 0)
        {
          sample_count = std::min (sample_count, tr_samples);
        }
    }
  sample_count = qBound (0, sample_count, kMsk144SampleCount);
  if (sample_count < kMsk144BlockSize)
    {
      return rows;
    }

  FixedChars<12> const mycall = fixed_from_latin<12> (to_fortran_field (request.mycall, 12));
  FixedChars<12> const hiscall = fixed_from_latin<12> (to_fortran_field (request.hiscall, 12));
  QString const data_dir = QDir::currentPath ().isEmpty () ? QStringLiteral (".") : QDir::currentPath ();
  std::array<double, 5> pcoeffs {{0.0, 0.0, 0.0, 0.0, 0.0}};
  int const max_lines = qBound (1, request.maxlines, kMsk144WorkerMaxLines);
  int const ntol = qMax (0, request.ftol);
  int const nrxfreq = qBound (0, request.rxfreq, 5000);
  int const ndepth = qBound (0, request.aggressive, 10);

  Msk144RuntimeState& state = shared_worker_state ();
  for (int position = 0; position + kMsk144BlockSize <= sample_count; position += kMsk144StepSize)
    {
	      float const tsec = static_cast<float> (position + 1) / static_cast<float> (kMsk144SampleRate);
	      QString row;
	      bool training_completed = false;
	      if (!decode_block_native (request.audio.constData () + position, qMax (0, request.nutc), tsec,
	                                ntol, nrxfreq, ndepth, mycall, hiscall,
	                                request.shorthandEnabled, request.trainingEnabled, request.swlEnabled,
	                                pcoeffs, data_dir, state, &row, &training_completed))
	        {
	          continue;
	        }
      if (row.isEmpty () || !msk144_row_in_pick_window (row, request))
        {
          continue;
        }
      rows << row;
      if (rows.size () >= max_lines)
        {
          break;
        }
    }

  return rows;
}

QString decodeMsk144RealtimeBlock (RealtimeDecodeRequest const& request)
{
  if (!request.audio)
    {
      return {};
    }

  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};

  FixedChars<12> const mycall = fixed_from_latin<12> (to_fortran_field (request.mycall, 12));
  FixedChars<12> const hiscall = fixed_from_latin<12> (to_fortran_field (request.hiscall, 12));
  QString const data_dir = request.dataDir.isEmpty () ? QDir::currentPath () : request.dataDir;
  Msk144RuntimeState& state = shared_live_state ();
  QString row;
  bool training_completed = false;
  if (!decode_block_native (request.audio, qMax (0, request.nutc), request.tsec,
                            qMax (0, request.ftol), qBound (0, request.rxfreq, 5000),
                            qBound (0, request.aggressive, 10), mycall, hiscall,
                            request.shorthandEnabled, request.trainingEnabled, request.swlEnabled,
                            request.phaseEqCoefficients, data_dir, state, &row, &training_completed))
    {
      return {};
    }
  if (training_completed && request.trainingEnabledState)
    {
      *request.trainingEnabledState = false;
    }
  return row;
}

extern "C" void mskrtd_ (short id2[], int* nutc0, float* tsec, int* ntol, int* nrxfreq, int* ndepth,
                         char* mycall, char* hiscall, bool* bshmsg, bool* btrain,
                         double const pcoeffs[], bool* bswl, char* datadir, char* line,
                         fortran_charlen_local mycall_len, fortran_charlen_local hiscall_len,
                         fortran_charlen_local datadir_len, fortran_charlen_local line_len)
{
  if (line)
    {
      std::fill_n (line, line_len, ' ');
    }
  if (!id2 || !nutc0 || !tsec || !ntol || !nrxfreq || !ndepth || !line)
    {
      return;
    }

  bool training_enabled = btrain ? *btrain : false;
  RealtimeDecodeRequest request;
  request.audio = id2;
  request.nutc = *nutc0;
  request.tsec = *tsec;
  request.rxfreq = *nrxfreq;
  request.ftol = *ntol;
  request.aggressive = *ndepth;
  request.mycall = QByteArray (mycall ? mycall : "", static_cast<int> (mycall_len));
  request.hiscall = QByteArray (hiscall ? hiscall : "", static_cast<int> (hiscall_len));
  request.shorthandEnabled = bshmsg ? *bshmsg : false;
  request.trainingEnabled = training_enabled;
  request.swlEnabled = bswl ? *bswl : false;
  request.trainingEnabledState = &training_enabled;
  request.dataDir = QString::fromLocal8Bit (datadir ? datadir : "", static_cast<int> (datadir_len)).trimmed ();
  if (request.dataDir.isEmpty ())
    {
      request.dataDir = QDir::currentPath ();
    }
  if (pcoeffs)
    {
      for (int i = 0; i < 5; ++i)
        {
          request.phaseEqCoefficients[static_cast<size_t> (i)] = pcoeffs[i];
        }
    }

  QString const row = decodeMsk144RealtimeBlock (request);
  if (btrain)
    {
      *btrain = training_enabled;
    }
  QByteArray const latin = row.toLatin1 ();
  std::copy_n (latin.constData (), std::min<size_t> (latin.size (), line_len), line);
}

void MSK144DecodeWorker::decode (DecodeRequest const& request)
{
  QStringList const rows = decodeMsk144Rows (request);
  Q_EMIT decodeReady (request.serial, rows);
}

}
}
