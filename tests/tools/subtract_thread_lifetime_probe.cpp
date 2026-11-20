#include <atomic>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

extern "C" void ftx_subtract_ft8_c (float*, int const*, float, float, int);

int main ()
{
  std::vector<float> input (180000);
  for (int i = 0; i < int (input.size ()); ++i)
    input[i] = std::sin (float (i) * 0.193f) + 0.2f * std::cos (float (i) * 0.117f);
  int tones[79];
  for (int i = 0; i < 79; ++i) tones[i] = (i * 7 + 3) % 8;
  std::atomic<std::uint64_t> expected {0};
  std::atomic<int> failures {0};
  auto run = [&] {
    auto wave = input;
    ftx_subtract_ft8_c (wave.data (), tones, 1500.0f, 0.5f, 1);
    std::uint64_t hash = 1469598103934665603ULL;
    for (float const value : wave)
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
