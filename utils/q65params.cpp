#include <cmath>
#include <iomanip>
#include <iostream>

int main ()
{
  int const ntrp[5] {15, 30, 60, 120, 300};
  int const nsps[5] {1800, 3600, 7200, 16000, 41472};

  std::cout << "T/R   tsym   baud    BW     TxT    SNR\n"
            << "---------------------------------------\n";
  for (int i = 0; i < 5; ++i)
    {
      float const baud = 12000.0f / static_cast<float> (nsps[i]);
      float const bw = 65.0f * baud;
      float const tsym = 1.0f / baud;
      float const txt = 85.0f * tsym;
      float const snr = -27.0f + 10.0f * std::log10 (7200.0f / static_cast<float> (nsps[i]));
      std::cout << std::setw (3) << ntrp[i]
                << std::fixed << std::setprecision (3)
                << std::setw (7) << tsym
                << std::setw (7) << baud
                << std::setprecision (1)
                << std::setw (7) << bw
                << std::setw (7) << txt
                << std::setw (7) << snr << '\n';
    }

  for (int j = 0; j < 5; ++j)
    {
      char const mode = static_cast<char> ('A' + j);
      std::cout << "\n" << mode << "  T/R   baud    BW\n"
                << "--------------------\n";
      for (int i = 0; i < 5; ++i)
        {
          float const baud = 12000.0f / static_cast<float> (nsps[i]);
          float const spacing = baud * static_cast<float> (1 << j);
          float const bw = 65.0f * spacing;
          std::cout << std::setw (6) << ntrp[i]
                    << std::fixed << std::setprecision (2)
                    << std::setw (7) << spacing
                    << std::setw (6) << static_cast<int> (std::lround (bw)) << '\n';
        }
    }

  return 0;
}
