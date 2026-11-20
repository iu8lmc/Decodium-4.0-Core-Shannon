// Pins the saturating integer truncation of the FT2 legacy noise estimate
// (ft2_legacy_trunc, ft2_legacy_base_noise_var, ft2_legacy_equalized_beta in
// Detector/FtxBitmetrics.cpp). Before the fix an out-of-range power was
// converted to int (undefined behaviour) and 48 terms were summed in an int,
// which wrapped. The functions are file-local, so the source is included.
#include "../Detector/FtxBitmetrics.cpp"

#include <iostream>

namespace
{

int failures = 0;

void expect (bool ok, char const* what)
{
  if (!ok)
    {
      std::cerr << "FAIL: " << what << '\n';
      ++failures;
    }
}

std::array<std::array<float, 4>, 103> filled (float value)
{
  std::array<std::array<float, 4>, 103> pwr {};
  for (auto& row : pwr)
    {
      row.fill (value);
    }
  return pwr;
}

}

int main ()
{
  std::int64_t const imax = std::numeric_limits<std::int32_t>::max ();
  std::int64_t const imin = std::numeric_limits<std::int32_t>::min ();
  float const inf = std::numeric_limits<float>::infinity ();

  // Representable values truncate toward zero exactly as before.
  expect (ft2_legacy_trunc (0.0f) == 0, "trunc 0");
  expect (ft2_legacy_trunc (123.9f) == 123, "trunc 123.9");
  expect (ft2_legacy_trunc (-123.9f) == -123, "trunc -123.9");
  expect (ft2_legacy_trunc (2147483520.0f) == 2147483520, "largest float below 2^31");
  expect (ft2_legacy_trunc (-2147483520.0f) == -2147483520, "smallest float above -2^31");
  // Out of range saturates, NaN gives zero.
  expect (ft2_legacy_trunc (2147483648.0f) == imax, "trunc 2^31");
  expect (ft2_legacy_trunc (3.0e9f) == imax, "trunc 3e9");
  expect (ft2_legacy_trunc (inf) == imax, "trunc +inf");
  expect (ft2_legacy_trunc (-2147483648.0f) == imin, "trunc -2^31");
  expect (ft2_legacy_trunc (-3.0e9f) == imin, "trunc -3e9");
  expect (ft2_legacy_trunc (-inf) == imin, "trunc -inf");
  expect (ft2_legacy_trunc (std::numeric_limits<float>::quiet_NaN ()) == 0, "trunc NaN");

  // 48 noise tones: small values as before, the sum no longer wraps.
  expect (ft2_legacy_base_noise_var (filled (100.7f)) == 100.0f, "noise var 100.7");
  expect (ft2_legacy_base_noise_var (filled (1.0e9f)) == 1.0e9f, "noise var, sum of 48 x 1e9");
  expect (ft2_legacy_base_noise_var (filled (3.0e9f)) == 2147483648.0f, "noise var, saturated terms");
  expect (ft2_legacy_base_noise_var (filled (std::numeric_limits<float>::quiet_NaN ())) == 1.0e-10f,
          "noise var, NaN");

  float noise_var_eq = -1.0f;
  expect (ft2_legacy_equalized_beta (filled (100.7f), &noise_var_eq) == 0.01f
          && noise_var_eq == 100.0f, "equalized beta 100.7");
  expect (ft2_legacy_equalized_beta (filled (1.0e9f), &noise_var_eq) == 0.01f
          && noise_var_eq == 1.0e9f, "equalized beta, sum of 48 x 1e9");
  expect (ft2_legacy_equalized_beta (filled (3.0e9f), &noise_var_eq) == 0.01f
          && noise_var_eq == 2147483648.0f, "equalized beta, saturated terms");

  if (failures != 0)
    {
      return 1;
    }
  std::cout << "FT2 legacy noise estimate saturates\n";
  return 0;
}
