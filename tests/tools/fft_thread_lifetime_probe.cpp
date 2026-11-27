#include "Detector/FftCompat.hpp"

#include <cmath>
#include <complex>
#include <atomic>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

#if defined(__GLIBC__)
#include <malloc.h>
#endif

int main ()
{
  auto residentKiB = [] {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX counters {};
    counters.cb = sizeof counters;
    if (!GetProcessMemoryInfo (GetCurrentProcess (),
                               reinterpret_cast<PROCESS_MEMORY_COUNTERS *> (&counters),
                               sizeof counters)) return -1L;
    return static_cast<long> (counters.PrivateUsage / 1024);
#else
    std::ifstream status ("/proc/self/status");
    std::string key;
    while (status >> key)
      {
        if (key == "VmRSS:") { long value = 0; status >> value; return value; }
        status.ignore (10000, '\n');
      }
    return -1L;
#endif
  };
  auto const before = residentKiB ();
  for (int iteration = 0; iteration < 300; ++iteration)
    {
      bool ok = false;
      std::thread worker ([&] {
        std::vector<std::complex<float>> values (4096);
        values[0] = {1.0f, 0.0f};
        decodium::fft_compat::legacy_fft_execute (values.data (), 4096, -1, 1);
        ok = true;
        for (auto const& value : values)
          if (std::fabs (value.real () - 1.0f) > 1e-5f || std::fabs (value.imag ()) > 1e-5f)
            ok = false;
      });
      worker.join ();
      if (!ok)
        {
          std::cerr << "sequential transform failed at iteration=" << iteration << '\n';
          return 1;
        }
    }
  std::atomic<int> failures {0};
  std::vector<std::thread> workers;
  for (int thread = 0; thread < 8; ++thread)
    workers.emplace_back ([&, thread] {
      for (int iteration = 0; iteration < 50; ++iteration)
        {
          int const n = (iteration % 2) ? 256 : 512;
          std::vector<std::complex<float>> values (n);
          values[0] = {1.0f, 0.0f};
          decodium::fft_compat::legacy_fft_execute (values.data (), n, -1, 1);
          for (auto const& value : values)
            if (std::fabs (value.real () - 1.0f) > 1e-5f || std::fabs (value.imag ()) > 1e-5f)
              ++failures;
          decodium::fft_compat::legacy_fft_execute (values.data (), n, 1, 1);
          if (std::fabs (values[0].real () - n) > 1e-3f) ++failures;
          for (int i = 1; i < n; ++i)
            if (std::abs (values[i]) > 1e-3f) ++failures;
          if ((iteration + thread) % 3 == 0) decodium::fft_compat::legacy_fft_cleanup ();
        }
    });
  for (auto& worker : workers) worker.join ();
  if (failures) { std::cerr << "transform failures=" << failures << '\n'; return 1; }
#if defined(__GLIBC__)
  malloc_trim (0);
#endif
  std::cout << "threads=300 concurrent=8 metric="
#if defined(_WIN32)
            << "private"
#else
            << "rss"
#endif
            << " beforeKiB=" << before << " afterKiB=" << residentKiB () << '\n';
}
