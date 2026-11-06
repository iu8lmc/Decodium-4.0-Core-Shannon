#ifndef DECODIUM_FTX_FAST_MATH_HPP_
#define DECODIUM_FTX_FAST_MATH_HPP_

#include <cstdint>
#include <cstring>

namespace decodium
{

// Single-precision exp() for the soft-decision sums of the FT8/FT4/FT2 demodulators.
//
// std::exp(float) on MinGW goes through expf -> the x87 f2xm1 path of the C runtime: about a hundred
// cycles per call plus the call itself, and the log-sum-exp loops of the bit-metric stage call it
// for every symbol of every candidate (measured: ~48 % of the FT2 decoder CPU). This is the Cephes
// expf polynomial: range reduction by ln2, degree-5 polynomial, exponent put back with integer
// arithmetic. It has no branches on the hot path, so the compiler can vectorise the loops that use it.
// Maximum relative error is about 2e-7 (1-2 ulp of float), well under the noise of an LLR.
//
// Domain: x <= 88. Below -87 it returns 0 (the exact result is a denormal below 1.2e-38, which can
// not change a sum that contains the term exp(0) = 1). Not meant for NaN inputs.
inline float ftx_expf (float x)
{
  x = x > 88.0f ? 88.0f : x;
  x = x < -87.0f ? -87.0f : x;
  // n = round (x / ln2) with the 1.5 * 2^23 trick: float add/sub rounds to nearest, no libm call.
  float const t = x * 1.44269504088896341f;
  float const n = (t + 12582912.0f) - 12582912.0f;
  // r = x - n ln2, ln2 split in a high part exact in float and a low correction
  float const r = (x - n * 0.693359375f) + n * 2.12194440e-4f;
  float const z = r * r;
  float p = 1.9875691500e-4f;
  p = p * r + 1.3981999507e-3f;
  p = p * r + 8.3334519073e-3f;
  p = p * r + 4.1665795894e-2f;
  p = p * r + 1.6666665459e-1f;
  p = p * r + 5.0000001201e-1f;
  p = p * z + r + 1.0f;
  // 2^n as a float built from its exponent field
  std::int32_t const bits = (static_cast<std::int32_t> (n) + 127) << 23;
  float scale;
  std::memcpy (&scale, &bits, sizeof scale);
  float const result = p * scale;
  return x <= -87.0f ? 0.0f : result;
}

} // namespace decodium

#endif
