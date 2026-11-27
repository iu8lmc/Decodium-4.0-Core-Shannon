// Include the implementation so this lab can inspect the private per-thread
// accumulation table without adding a diagnostic entry point to the decoder.
#include "../../Detector/FtxFt2Stage7.cpp"

#include <atomic>
#include <iostream>
#include <thread>

int main ()
{
  std::atomic<int> failures {0};
  auto check = [&] {
    auto& first = stage7_state ();
    std::array<Complex, kFt2NSeg> signal {};
    for (size_t i = 0; i < signal.size (); ++i)
      signal[i] = Complex {std::sin (float (i) * 0.1f), std::cos (float (i) * 0.1f)};
    first.accumulo_call = 1;
    accumulo_raccogli (first, 1500.0f, 0.2f, 2.0f, signal.data (), true);
    int const idx = accumulo_trova (first, 1500.0f, 0.2f);
    if (idx < 0 || first.accumulo[idx].nslot != 1) { ++failures; return; }
    first.accumulo_call = 2;
    accumulo_raccogli (stage7_state (), 1501.0f, 0.21f, 1.9f, signal.data (), true);
    auto const& state = stage7_state ();
    if (&first != &state || state.accumulo[idx].nslot != 2
        || state.accumulo[idx].slot[0].call != 1
        || state.accumulo[idx].slot[1].call != 2) ++failures;
  };
  // A second fresh thread must start with its own empty state.
  for (int i = 0; i < 2; ++i) { std::thread worker (check); worker.join (); }
  if (failures) return 1;
  std::cout << "Two slots retained within each of two independent threads\n";
}
