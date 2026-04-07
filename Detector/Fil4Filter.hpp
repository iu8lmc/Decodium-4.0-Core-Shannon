#ifndef FIL4_FILTER_HPP__
#define FIL4_FILTER_HPP__

#include <cstdint>

// C++ replacement for lib/fil4.f90
// FIR low-pass filter (49 taps, 4x downsample, fc=4500 Hz, fs_in=48000 Hz, fs_out=12000 Hz)
// Stateful: retains filter delay line across calls (equivalent to Fortran SAVE).
void fil4_cpp (int16_t const* id1, int32_t n1, int16_t* id2, int32_t& n2);

#endif
