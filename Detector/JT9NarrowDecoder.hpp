#ifndef JT9NARROWDECODER_HPP
#define JT9NARROWDECODER_HPP

#include <QStringList>
#include <complex>
#include <vector>

namespace decodium
{
namespace legacyjt
{
struct DecodeRequest;
}

namespace jt9narrow
{

// Persistent state for downsam9_cpp (mirrors Fortran save variables in downsam9.f90).
// plan_r2c is an fftwf_plan held as void* to avoid including fftw3.h in this header.
struct Downsam9State
{
  static constexpr int kNfft1 = 653184;
  static constexpr int kNfft2 = 1512;
  static constexpr int kNc1   = kNfft1 / 2 + 1;  // 326593

  bool initialized {false};
  void* plan_r2c   {nullptr};                      // fftwf_plan (r2c of kNfft1 floats)
  std::vector<float>                x1;            // kNfft1 floats (FFT input)
  std::vector<std::complex<float>>  c1;            // kNc1 complex (FFT output, persisted)
  std::vector<float>                s;             // 5000 power-spectrum bins

  ~Downsam9State () noexcept;
};

// Per-instance state: sync correlation arrays (ccfred, red2) + downsam9 FFT state.
struct CorrState
{
  static constexpr int kNsmax = 6827;  // NSMAX from constants.f90
  std::vector<float> ccfred;           // sync correlation array
  std::vector<float> red2;             // secondary correlation array
  Downsam9State      downsam9;         // persistent FFT/power state for downsam9_cpp
};

QStringList decode_async_jt9_narrow (legacyjt::DecodeRequest const& request, CorrState* state);

}  // namespace jt9narrow
}  // namespace decodium

#endif  // JT9NARROWDECODER_HPP
