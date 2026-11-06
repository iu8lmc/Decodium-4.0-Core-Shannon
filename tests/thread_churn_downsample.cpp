// Riproduce fuori dall'applicazione il difetto che tiene spenta la fase
// profonda di FT8: la corruzione dello heap che si manifesta quando i thread
// di decodifica muoiono.
//
// L'ipotesi da falsificare e' che basti il ricambio di thread — ognuno con i
// propri spazi di lavoro thread_local pieni di buffer FFTW — a rovinare lo
// heap, senza bisogno della GUI, di Qt o della radio. Se il difetto compare
// qui, si puo' passarci sopra Dr. Memory in pochi secondi invece che sull'app
// intera, dove lo strumento non arriva nemmeno a decodificare.
//
// Uso:  thread_churn_downsample [tornate] [thread per tornata]

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

#include <fftw3.h>

namespace
{
constexpr int kFt8NMax = 15 * 12000;
constexpr int kFt8Nfft2 = 3200;
constexpr int kFiltNfft = 180000;
// c0/c2/c3 partono da un offset di 800 dentro il buffer: la dimensione
// giusta e' kFt8VarDownsampleSize di FtxFt8Stage4.cpp, non kFt8Nfft2.
constexpr int kFt8VarSize = 4801;

// fftwf_complex is a C array typedef (float[2]).  It cannot be the element
// type of std::vector with libc++, because arrays are not destructible objects.
// Keep FFTW's required allocation/alignment and make ownership exception-safe.
using FftwComplexBuffer =
  std::unique_ptr<fftwf_complex, decltype (&fftwf_free)>;

FftwComplexBuffer make_complex_buffer (int size)
{
  return FftwComplexBuffer {fftwf_alloc_complex (size), &fftwf_free};
}
}

extern "C"
{
  void ftx_ft8_downsample_c (float const* dd, int* newdat, float f0, fftwf_complex* c1);
  void ftx_ft8var_downsample_c (float const* dd, int* newdat, float const* f0,
                                int const* nqso, fftwf_complex* c0, fftwf_complex* c2,
                                fftwf_complex* c3, int const* lhighsens,
                                int* lsubtracted, int* npos, float const* freqsub);
  void ftx_filt8_c (float const* f0, int const* nslots, float const* width, float* wave);
}

int main (int argc, char* argv[])
{
  int const tornate = argc > 1 ? std::atoi (argv[1]) : 40;
  int const per_tornata = argc > 2 ? std::atoi (argv[2]) : 12;

  // Rumore deterministico: non serve un segnale vero, serve che il percorso
  // di downsample venga eseguito per intero.
  std::vector<float> dd (static_cast<size_t> (kFt8NMax));
  for (size_t i = 0; i < dd.size (); ++i)
    {
      dd[i] = std::sin (static_cast<float> (i) * 0.017f) * 1000.0f;
    }

  std::atomic<long> eseguiti {0};

  // Bisezione: SOLO=1 downsample FT8, 2 downsample var, 3 filtro, 0 tutte
  char const* solo_env = std::getenv ("SOLO");
  int const solo = solo_env ? std::atoi (solo_env) : 0;
  std::printf ("funzioni sotto prova: %s\n",
               solo == 0 ? "tutte" : solo == 1 ? "ft8_downsample"
               : solo == 2 ? "ft8var_downsample" : "filt8");

  for (int t = 0; t < tornate; ++t)
    {
      std::vector<std::thread> squadra;
      squadra.reserve (static_cast<size_t> (per_tornata));

      for (int k = 0; k < per_tornata; ++k)
        {
          squadra.emplace_back ([&dd, &eseguiti, k, solo] {
            // Ogni thread crea i propri spazi di lavoro thread_local alla prima
            // chiamata, e li porta con se' quando muore: e' esattamente la
            // sequenza che nell'applicazione precede la corruzione.
            auto c1 = make_complex_buffer (kFt8Nfft2);
            if (!c1)
              std::abort ();
            int newdat = 1;
            float const f0 = 1500.0f + static_cast<float> (k % 7) * 50.0f;

            if (solo == 0 || solo == 1)
              ftx_ft8_downsample_c (dd.data (), &newdat, f0, c1.get ());

            auto c0 = make_complex_buffer (kFt8VarSize);
            auto c2 = make_complex_buffer (kFt8VarSize);
            auto c3 = make_complex_buffer (kFt8VarSize);
            if (!c0 || !c2 || !c3)
              std::abort ();
            int const nqso = 0;
            int const lhighsens = 0;
            int lsubtracted = 0;
            int npos = 0;
            float const freqsub = 0.0f;
            newdat = 1;
            if (solo == 0 || solo == 2)
              ftx_ft8var_downsample_c (dd.data (), &newdat, &f0, &nqso, c0.get (),
                                       c2.get (), c3.get (), &lhighsens,
                                       &lsubtracted, &npos, &freqsub);

            // Il filtro tiene due vettori thread_local a cui punta un piano FFTW
            std::vector<float> wave (static_cast<size_t> (kFiltNfft), 0.0f);
            int const nslots = 1;
            float const width = 50.0f;
            if (solo == 0 || solo == 3)
              ftx_filt8_c (&f0, &nslots, &width, wave.data ());

            ++eseguiti;
          });
        }

      for (std::thread& th : squadra)
        {
          th.join ();          // i thread muoiono qui: i distruttori partono
        }

      if ((t + 1) % 5 == 0)
        {
          std::printf ("tornata %d/%d — thread completati: %ld\n",
                       t + 1, tornate, eseguiti.load ());
          std::fflush (stdout);
        }
    }

  std::printf ("FINITO senza cadere: %ld thread nati e morti\n", eseguiti.load ());
  return 0;
}
