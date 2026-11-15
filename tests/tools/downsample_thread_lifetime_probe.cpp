#include <fftw3.h>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

extern "C" void ftx_ft8_downsample_c (float const*, int*, float, fftwf_complex*);
extern "C" void ftx_ft2_downsample_c (float const*, int*, float, fftwf_complex*);
extern "C" void ftx_ft4_downsample_c (float const*, int*, float, fftwf_complex*);
extern "C" void ftx_filt8_c (float const*, int const*, float const*, float*);
extern "C" void ftx_ft8var_downsample_c (float const*, int*, float const*, int const*,
                                           fftwf_complex*, fftwf_complex*, fftwf_complex*,
                                           int const*, int*, int*, float const*);

int main ()
{
  std::vector<float> input (180000);
  for (int i = 0; i < int (input.size ()); ++i)
    input[i] = std::sin (float (i) * 0.193f) + 0.2f * std::cos (float (i) * 0.117f);
  std::atomic<std::uint64_t> expected {0};
  std::atomic<int> failures {0};
  auto run = [&] {
    std::vector<std::complex<float>> a (5000), b (5000), c (5000), d (5000), e (5000), f (5000);
    auto ptr = [] (auto& v) { return reinterpret_cast<fftwf_complex*> (v.data ()); };
    int fresh = 1; ftx_ft8_downsample_c (input.data (), &fresh, 1500, ptr (a));
    fresh = 1; ftx_ft2_downsample_c (input.data (), &fresh, 1500, ptr (b));
    fresh = 1; ftx_ft4_downsample_c (input.data (), &fresh, 1500, ptr (c));
    // A live worker must reuse its cached input FFT without changing output.
    std::vector<std::complex<float>> reused (5000);
    fresh = 0; ftx_ft8_downsample_c (input.data (), &fresh, 1500, ptr (reused));
    if (std::memcmp (a.data (), reused.data (), 3200 * sizeof (a[0]))) ++failures;
    std::fill (reused.begin (), reused.end (), std::complex<float> {});
    fresh = 0; ftx_ft2_downsample_c (input.data (), &fresh, 1500, ptr (reused));
    if (std::memcmp (b.data (), reused.data (), 5000 * sizeof (b[0]))) ++failures;
    std::fill (reused.begin (), reused.end (), std::complex<float> {});
    fresh = 0; ftx_ft4_downsample_c (input.data (), &fresh, 1500, ptr (reused));
    if (std::memcmp (c.data (), reused.data (), 4032 * sizeof (c[0]))) ++failures;
    fresh = 1; float freq = 1500; int nqso = 1, high = 0, sub = 0, pos = 0;
    ftx_ft8var_downsample_c (input.data (), &fresh, &freq, &nqso, ptr (d), ptr (e), ptr (f),
                             &high, &sub, &pos, nullptr);
    std::vector<float> filtered = input;
    int slots = 2; float width = 20;
    ftx_filt8_c (&freq, &slots, &width, filtered.data ());
    std::uint64_t hash = 1469598103934665603ULL;
    for (auto const* v : {&a, &b, &c, &d, &e, &f})
      for (auto const& value : *v)
        for (unsigned char byte : *reinterpret_cast<unsigned char const (*)[sizeof value]> (&value))
          hash = (hash ^ byte) * 1099511628211ULL;
    for (float const value : filtered)
      for (unsigned char byte : *reinterpret_cast<unsigned char const (*)[sizeof value]> (&value))
        hash = (hash ^ byte) * 1099511628211ULL;
    auto zero = std::uint64_t {0}; expected.compare_exchange_strong (zero, hash);
    if (expected != hash) ++failures;
  };
  for (int i = 0; i < 25; ++i) { std::thread worker (run); worker.join (); }
  std::vector<std::thread> threads;
  for (int i = 0; i < 8; ++i) threads.emplace_back (run);
  for (auto& t : threads) t.join ();
  if (failures) return 1;
#if defined(__GLIBC__)
  malloc_trim (0);
#endif
  std::ifstream status ("/proc/self/status");
  std::string key; long rss = -1;
  while (status >> key) { if (key == "VmRSS:") { status >> rss; break; } status.ignore (10000, '\n'); }
  std::cout << "sequential=25 concurrent=8 hash=" << expected << " rssKiB=" << rss << '\n';
}
