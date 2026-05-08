#ifndef FFT_COMPAT_HPP
#define FFT_COMPAT_HPP

#include <fftw3.h>

#include <complex>
#include <mutex>
#include <vector>

namespace decodium
{
namespace fft_compat
{

void legacy_fft_execute (std::complex<float>* buffer, int nfft, int isign, int iform);
void legacy_fft_cleanup ();

inline std::mutex& planner_mutex ()
{
  static std::mutex mutex;
  return mutex;
}

inline void initialize_planner_thread_safety ()
{
  static bool const initialized = [] {
    fftwf_init_threads ();
    fftwf_make_planner_thread_safe ();
    return true;
  } ();
  (void) initialized;
}

inline fftwf_plan plan_dft_1d (int n, fftwf_complex* in, fftwf_complex* out,
                               int sign, unsigned flags)
{
  initialize_planner_thread_safety ();
  std::lock_guard<std::mutex> lock {planner_mutex ()};
  return fftwf_plan_dft_1d (n, in, out, sign, flags);
}

inline fftwf_plan plan_dft_r2c_1d (int n, float* in, fftwf_complex* out, unsigned flags)
{
  initialize_planner_thread_safety ();
  std::lock_guard<std::mutex> lock {planner_mutex ()};
  return fftwf_plan_dft_r2c_1d (n, in, out, flags);
}

inline fftwf_plan plan_dft_c2r_1d (int n, fftwf_complex* in, float* out, unsigned flags)
{
  initialize_planner_thread_safety ();
  std::lock_guard<std::mutex> lock {planner_mutex ()};
  return fftwf_plan_dft_c2r_1d (n, in, out, flags);
}

inline void destroy_plan (fftwf_plan plan)
{
  if (!plan) return;
  initialize_planner_thread_safety ();
  std::lock_guard<std::mutex> lock {planner_mutex ()};
  fftwf_destroy_plan (plan);
}

inline int import_wisdom_from_filename (char const* filename)
{
  initialize_planner_thread_safety ();
  std::lock_guard<std::mutex> lock {planner_mutex ()};
  return fftwf_import_wisdom_from_filename (filename);
}

inline int export_wisdom_to_filename (char const* filename)
{
  initialize_planner_thread_safety ();
  std::lock_guard<std::mutex> lock {planner_mutex ()};
  return fftwf_export_wisdom_to_filename (filename);
}

void forward_real (std::vector<std::complex<float>>& buffer, int nfft);
void inverse_real (std::vector<std::complex<float>>& buffer, int nfft);
void forward_real_buffer (float* buffer, int nfft);
void inverse_real_buffer (float* buffer, int nfft);
void forward_complex (std::complex<float>* buffer, int nfft);
void inverse_complex (std::complex<float>* buffer, int nfft);
void transform_complex (std::complex<float>* buffer, int nfft, int isign);
void cleanup ();

}
}

#endif // FFT_COMPAT_HPP
