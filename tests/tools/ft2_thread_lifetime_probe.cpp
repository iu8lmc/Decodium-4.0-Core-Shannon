#include <cstdint>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

extern "C" void ftx_ft2_stage7_clravg_c ();
extern "C" void ftx_ft2_set_ap_hash_cache_c (std::uint32_t const*, int);

int main ()
{
  auto rss = [] {
    std::ifstream status ("/proc/self/status");
    std::string key;
    while (status >> key)
      {
        if (key == "VmRSS:") { long value = 0; status >> value; return value; }
        status.ignore (10000, '\n');
      }
    return -1L;
  };
  long const before = rss ();
  for (int i = 0; i < 80; ++i)
    {
      std::thread worker ([] {
        std::vector<std::uint32_t> hashes (4096, 1234);
        ftx_ft2_set_ap_hash_cache_c (hashes.data (), int (hashes.size ()));
        ftx_ft2_stage7_clravg_c ();
      });
      worker.join ();
    }
#if defined(__GLIBC__)
  malloc_trim (0);
#endif
  std::cout << "threads=80 beforeKiB=" << before << " afterKiB=" << rss () << '\n';
}
