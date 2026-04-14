#ifndef FFT_COMPAT_HPP
#define FFT_COMPAT_HPP

#include <complex>
#include <vector>

namespace decodium
{
namespace fft_compat
{

void legacy_fft_execute (std::complex<float>* buffer, int nfft, int isign, int iform);
void legacy_fft_cleanup ();
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
