#ifndef JT4DECODER_HPP
#define JT4DECODER_HPP

#include <QStringList>
#include <array>
#include <string>
#include <vector>

namespace decodium
{
namespace legacyjt
{
struct DecodeRequest;
}

namespace jt4
{

// Cache for deep4 template matching: rebuilt only when mycall/hiscall/hisgrid/neme change.
struct Deep4Entry
{
  std::array<float, 206> code;  // ±1 encoded bits
  std::string msg;              // 22-char message text
};

struct Deep4Cache
{
  std::string mycall0, hiscall0, hisgrid0;
  int neme0 {-99};
  std::vector<Deep4Entry> entries;
};

struct AverageState
{
  static constexpr int kMaxAve = 64;
  static constexpr int kNsymMax = 207;
  static constexpr int kNsubMax = 7;

  bool wsjt4_initialized {false};
  int avg_last_utc {-999};
  int avg_last_freq {-999999};
  int nsave {0};  // 1-indexed ring counter (0 = nothing saved yet)
  int ich1 {1};
  int ich2 {1};

  std::array<int, kMaxAve> avg_iutc;
  std::array<int, kMaxAve> avg_nfsave;
  std::array<float, kMaxAve> avg_syncsave;
  std::array<float, kMaxAve> avg_dtsave;
  std::array<float, kMaxAve> avg_flipsave;
  // avg_ppsave(207, 7, 64): Fortran column-major layout
  std::array<float, kNsymMax * kNsubMax * kMaxAve> avg_ppsave;
  // rsymbol(207, 7): soft symbols from last decode4 call
  std::array<float, kNsymMax * kNsubMax> rsymbol;

  // wav11 output buffer (reused when newdat=false)
  std::vector<float> cached_dd;

  // Template cache for deep4 (rebuilt when mycall/hiscall/hisgrid/neme changes)
  Deep4Cache deep4_cache;

  AverageState ()
  {
    avg_iutc.fill (-1);
    avg_nfsave.fill (0);
    avg_syncsave.fill (0.0f);
    avg_dtsave.fill (0.0f);
    avg_flipsave.fill (0.0f);
    avg_ppsave.fill (0.0f);
    rsymbol.fill (0.0f);
  }
};

QStringList decode_async_jt4 (legacyjt::DecodeRequest const& request, AverageState* state);

}  // namespace jt4
}  // namespace decodium

#endif  // JT4DECODER_HPP
