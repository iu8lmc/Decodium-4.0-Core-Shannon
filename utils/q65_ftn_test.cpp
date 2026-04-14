#include "wsjtx_config.h"

#include <array>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

#include <QByteArray>

#include "Modulator/FtxMessageEncoder.hpp"

extern "C"
{
void q65_encode_message_ (char* msg0, char* msgsent, int* payload, int* codeword, int* itone,
                          fortran_charlen_t len1, fortran_charlen_t len2);
void q65_enc_ (int x[], int y[]);
void q65_intrinsics_ff_ (float s3[], int* submode, float* b90ts, int* fading_model,
                         float s3prob[]);
void q65_dec_ (float s3[], float s3prob[], int apmask[], int apsymbols[], int* maxiters,
               float* esnodb, int xdec[], int* rc);
}

namespace
{
constexpr int kPayloadSymbols {13};
constexpr int kCodewordSymbols {63};
constexpr int kMessageBits {77};
constexpr int kMessageChars {37};
constexpr int kLl {192};
constexpr int kNn {63};

std::string trim_right (char const* data, std::size_t size)
{
  std::size_t used = size;
  while (used > 0 && (data[used - 1] == ' ' || data[used - 1] == '\0'))
    {
      --used;
    }
  return std::string {data, data + used};
}

QByteArray bits_from_payload (std::array<int, kPayloadSymbols> const& payload)
{
  QByteArray bits (kMessageBits, '\0');
  int index = 0;
  for (int symbol = 0; symbol < kPayloadSymbols; ++symbol)
    {
      int width = symbol == kPayloadSymbols - 1 ? 5 : 6;
      int value = payload[static_cast<std::size_t> (symbol)];
      for (int bit = width - 1; bit >= 0; --bit)
        {
          bits[index++] = static_cast<char> ((value >> bit) & 1);
        }
    }
  return bits;
}

void print_usage ()
{
  std::cout << "Usage:   q65_ftn_test \"message\"\n";
  std::cout << "Example: q65_ftn_test \"K1ABC W9XYZ EN37\"\n";
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
  std::fill (message.begin (), message.end (), ' ');
  std::fill (msgsent.begin (), msgsent.end (), ' ');
  std::string const input {argv[1]};
  std::memcpy (message.data (), input.data (), std::min (input.size (), message.size ()));

  std::array<int, kPayloadSymbols> payload {};
  std::array<int, kCodewordSymbols> codeword {};
  std::array<int, 85> tones {};
  q65_encode_message_ (message.data (), msgsent.data (), payload.data (), codeword.data (),
                       tones.data (), static_cast<fortran_charlen_t> (message.size ()),
                       static_cast<fortran_charlen_t> (msgsent.size ()));

  if (trim_right (msgsent.data (), msgsent.size ()).empty ())
    {
      std::cerr << "q65_encode_message failed\n";
      return 1;
    }

  std::cout << "User message:\n";
  for (int value : payload)
    {
      std::cout << std::setw (3) << value;
    }
  std::cout << "  " << trim_right (message.data (), message.size ()) << "\n\n";

  std::cout << "Generated codeword:\n";
  for (int i = 0; i < kCodewordSymbols; ++i)
    {
      if (i != 0 && i % 20 == 0)
        {
          std::cout << "\n";
        }
      std::cout << std::setw (3) << codeword[static_cast<std::size_t> (i)];
    }
  std::cout << "\n";

  std::array<float, kLl * kNn> s3 {};
  std::array<float, kLl * kNn> s3prob {};
  std::array<int, kPayloadSymbols> apmask {};
  std::array<int, kPayloadSymbols> apsymbols {};
  for (int j = 0; j < kCodewordSymbols; ++j)
    {
      int const row = codeword[static_cast<std::size_t> (j)] + 64;
      if (row >= 0 && row < kLl)
        {
          s3[static_cast<std::size_t> (row + kLl * j)] = 1.0f;
        }
    }

  int submode = 0;
  float b90 = 1.0f;
  int fading_model = 1;
  q65_intrinsics_ff_ (s3.data (), &submode, &b90, &fading_model, s3prob.data ());

  std::array<int, kPayloadSymbols> decoded_payload {};
  int maxiters = 100;
  float esnodb = 0.0f;
  int irc = -1;
  q65_dec_ (s3.data (), s3prob.data (), apmask.data (), apsymbols.data (), &maxiters,
            &esnodb, decoded_payload.data (), &irc);

  QByteArray bits = bits_from_payload (decoded_payload);
  auto const decoded = decodium::txmsg::decode77 (bits, -1, -1);

  std::cout << "\nDecoded message:\n";
  for (int value : decoded_payload)
    {
      std::cout << std::setw (3) << value;
    }
  std::string const decoded_text =
    decoded.ok ? decoded.msgsent.trimmed ().toStdString () : trim_right (msgsent.data (), msgsent.size ());
  std::cout << "  " << decoded_text;
  if (irc < 0)
    {
      std::cout << "  [decode failed rc=" << irc << "]";
    }
  std::cout << "\n";
  return irc >= 0 ? 0 : 1;
}
