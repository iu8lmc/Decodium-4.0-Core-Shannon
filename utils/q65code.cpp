#include "wsjtx_config.h"

#include <array>
#include <bitset>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

extern "C"
{
void q65_encode_message_ (char* msg0, char* msgsent, int* payload, int* codeword, int* itone,
                          fortran_charlen_t len1, fortran_charlen_t len2);
}

namespace
{
constexpr int kPayloadSymbols {13};
constexpr int kCodewordSymbols {63};
constexpr int kChannelSymbols {85};
constexpr int kMessageChars {37};

std::string trim_right (char const* data, std::size_t size)
{
  std::size_t used = size;
  while (used > 0 && (data[used - 1] == ' ' || data[used - 1] == '\0'))
    {
      --used;
    }
  return std::string {data, data + used};
}

void print_usage ()
{
  std::cout << "Usage: q65code \"msg\"\n";
}
}

int main (int argc, char* argv[])
{
  if (argc != 2)
    {
      print_usage ();
      return 0;
    }

  std::array<char, kMessageChars> message {};
  std::array<char, kMessageChars> msgsent {};
  std::array<int, kPayloadSymbols> payload {};
  std::array<int, kCodewordSymbols> codeword {};
  std::array<int, kChannelSymbols> tones {};
  std::fill (message.begin (), message.end (), ' ');
  std::fill (msgsent.begin (), msgsent.end (), ' ');

  std::string const input {argv[1]};
  std::memcpy (message.data (), input.data (), std::min (input.size (), message.size ()));

  q65_encode_message_ (message.data (), msgsent.data (), payload.data (), codeword.data (),
                       tones.data (), static_cast<fortran_charlen_t> (message.size ()),
                       static_cast<fortran_charlen_t> (msgsent.size ()));

  std::cout << " Canonical message\n";
  std::cout << "  " << trim_right (msgsent.data (), msgsent.size ()) << "\n \n";
  std::cout << " Q65 payload (13 symbols / 78 bits)\n";
  std::cout << "6 bit : ";
  for (int value : payload)
    {
      std::cout << std::setw (4) << value;
    }
  std::cout << "\n";
  std::cout << "binary: ";
  for (int value : payload)
    {
      std::cout << std::bitset<6> (static_cast<unsigned long long> (value & 0x3f)) << ' ';
    }
  std::cout << "\n \n";
  std::cout << " Q65 codeword (63 symbols)\n";
  for (int i = 0; i < kCodewordSymbols; ++i)
    {
      if (i != 0 && i % 20 == 0)
        {
          std::cout << "\n";
        }
      std::cout << std::setw (3) << codeword[static_cast<std::size_t> (i)];
    }
  std::cout << "\n \n";
  std::cout << " Channel symbols (85 total)\n";
  for (int i = 0; i < kChannelSymbols; ++i)
    {
      if (i != 0 && i % 20 == 0)
        {
          std::cout << "\n";
        }
      std::cout << std::setw (3) << tones[static_cast<std::size_t> (i)];
    }
  std::cout << "\n";
  return 0;
}
