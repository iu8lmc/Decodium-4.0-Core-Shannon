/* c0com.c — provides storage for Fortran COMMON /c0com/ complex c0(120*1500)
 *
 * Previously emitted by savec2.f90 / wspr_downsample.f90.
 * LegacyDspIoHelpers.cpp accesses it as:
 *   extern struct { std::complex<float> c0[120*1500]; } c0com_;
 */
#include <complex.h>

struct { float _Complex c0[120 * 1500]; } c0com_;
