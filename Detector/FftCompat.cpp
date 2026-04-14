#include "Detector/FftCompat.hpp"
#include "Detector/FtxQ65Core.hpp"

// ── Backward-compatibility shims ────────────────────────────────────────────
// libwsjt_fort.a e libwsjt_qt.a (pre-compilati) usano ancora i simboli legacy.
// Questi shim li reindirizzano ai nuovi equivalenti C++.

extern "C"
{
  // four2a_: vecchio simbolo Fortran → ora mappato su wsjt_fft_compat_ (via FtxSharedDsp)
  // ma poiché wsjt_fft_compat_ è definito altrove, definiamo four2a_ come alias
  void wsjt_fft_compat_ (std::complex<float> a[], int* nfft, int* ndim,
                         int* isign, int* iform, int);

  void four2a_ (std::complex<float> a[], int* nfft, int* ndim,
                int* isign, int* iform, int pad)
  {
    // Reindirizza al nuovo simbolo rinominato
    wsjt_fft_compat_ (a, nfft, ndim, isign, iform, pad);
  }

  // q65_enc_: vecchio simbolo Fortran per codifica Q65 → nuovo C++
  void q65_enc_ (int x[], int y[])
  {
    decodium::q65::encode_payload_symbols (x, y);
  }
}

namespace decodium
{
namespace fft_compat
{

void forward_real (std::vector<std::complex<float>>& buffer, int nfft)
{
  legacy_fft_execute (buffer.data (), nfft, -1, 0);
}

void inverse_real (std::vector<std::complex<float>>& buffer, int nfft)
{
  legacy_fft_execute (buffer.data (), nfft, 1, -1);
}

void forward_real_buffer (float* buffer, int nfft)
{
  legacy_fft_execute (reinterpret_cast<std::complex<float>*> (buffer), nfft, -1, 0);
}

void inverse_real_buffer (float* buffer, int nfft)
{
  legacy_fft_execute (reinterpret_cast<std::complex<float>*> (buffer), nfft, 1, -1);
}

void forward_complex (std::complex<float>* buffer, int nfft)
{
  legacy_fft_execute (buffer, nfft, -1, 1);
}

void inverse_complex (std::complex<float>* buffer, int nfft)
{
  legacy_fft_execute (buffer, nfft, 1, 1);
}

void transform_complex (std::complex<float>* buffer, int nfft, int isign)
{
  legacy_fft_execute (buffer, nfft, isign, 1);
}

void cleanup ()
{
  legacy_fft_cleanup ();
}

}
}
