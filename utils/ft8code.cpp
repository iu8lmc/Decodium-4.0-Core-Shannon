#include <QByteArray>
#include <QString>

#include <array>
#include <iomanip>
#include <iostream>

#include "Modulator/FtxMessageEncoder.hpp"

extern "C" void encode174_91_ (signed char* message77, signed char* codeword_out);

namespace
{

using decodium::txmsg::EncodedMessage;

constexpr std::array<char const*, 49> kTestMessages {{
  "TNX BOB 73 GL",
  "K1ABC RR73; W9XYZ <KH1/KH7Z> -08",
  "K1ABC W9XYZ 6A WI",
  "W9XYZ K1ABC R 17B EMA",
  "123456789ABCDEF012",
  "CQ K1ABC FN42",
  "K1ABC W9XYZ EN37",
  "W9XYZ K1ABC -11",
  "K1ABC W9XYZ R-09",
  "W9XYZ K1ABC RRR",
  "K1ABC W9XYZ 73",
  "K1ABC W9XYZ RR73",
  "CQ FD K1ABC FN42",
  "CQ TEST K1ABC/R FN42",
  "K1ABC/R W9XYZ EN37",
  "W9XYZ K1ABC/R R FN42",
  "K1ABC/R W9XYZ RR73",
  "CQ TEST K1ABC FN42",
  "W9XYZ <PJ4/K1ABC> -11",
  "<PJ4/K1ABC> W9XYZ R-09",
  "CQ W9XYZ EN37",
  "<YW18FIFA> W9XYZ -11",
  "W9XYZ <YW18FIFA> R-09",
  "<YW18FIFA> KA1ABC",
  "KA1ABC <YW18FIFA> -11",
  "<YW18FIFA> KA1ABC R-17",
  "<YW18FIFA> KA1ABC 73",
  "CQ G4ABC/P IO91",
  "CQ TEST G4ABC/P IO91",
  "<PA9XYZ> <G4ABC/P> 570123 IO91NP",
  "<G4ABC/P> <PA9XYZ> R 580071 JO22DB",
  "G4ABC/P PA9XYZ JO22",
  "PA9XYZ G4ABC/P RR73",
  "K1ABC W9XYZ 579 WI",
  "W9XYZ K1ABC R 589 MA",
  "K1ABC KA0DEF 559 MO",
  "TU; KA0DEF K1ABC R 569 MA",
  "KA1ABC G3AAA 529 0013",
  "TU; G3AAA K1ABC R 559 MA",
  "CQ KH1/KH7Z",
  "CQ PJ4/K1ABC",
  "PJ4/K1ABC <W9XYZ>",
  "<W9XYZ> PJ4/K1ABC RRR",
  "PJ4/K1ABC <W9XYZ> 73",
  "<W9XYZ> YW18FIFA",
  "YW18FIFA <W9XYZ> RRR",
  "<W9XYZ> YW18FIFA 73",
  "CQ YW18FIFA",
  "<KA1ABC> YW18FIFA RR73",
}};

std::string msg_type (EncodedMessage const& encoded)
{
  if (encoded.i3 == 0)
    {
      switch (encoded.n3)
        {
        case 0: return "Free text";
        case 1: return "DXpedition mode";
        case 2: return "EU VHF Contest";
        case 3: return "ARRL Field Day";
        case 4: return "ARRL Field Day";
        case 5: return "Telemetry";
        default: return "Undefined type";
        }
    }

  switch (encoded.i3)
    {
    case 1: return "Standard msg";
    case 2: return "EU VHF Contest";
    case 3: return "ARRL RTTY Roundup";
    case 4: return "Nonstandard call";
    case 5: return "EU VHF Contest";
    default: return "Undefined type";
    }
}

void print_bits (QByteArray const& bits)
{
  for (char bit : bits)
    {
      std::cout << (bit != 0 ? '1' : '0');
    }
  std::cout << '\n';
}

void print_codeword_slice (std::array<signed char, 174> const& codeword, int begin, int end)
{
  for (int i = begin; i < end; ++i)
    {
      std::cout << (codeword[static_cast<size_t> (i)] != 0 ? '1' : '0');
    }
  std::cout << '\n';
}

void print_tones (QVector<int> const& tones)
{
  for (int i = 0; i < tones.size (); ++i)
    {
      std::cout << tones.at (i);
    }
  std::cout << '\n';
}

void print_usage ()
{
  std::cout << "\nProgram ft8code: Provides examples of FT8 message packing and encoding.\n\n"
            << "Usage: ft8code \"message\"\n"
            << "       ft8code -T\n"
            << "       ft8code -t\n";
}

int print_one (int index, QString const& message, bool short_mode)
{
  EncodedMessage const encoded = decodium::txmsg::encodeFt8 (message);
  if (!encoded.ok || encoded.msgbits.size () != 77 || encoded.tones.size () != 79)
    {
      std::cerr << "Encoding failed for: " << message.toStdString () << '\n';
      return 1;
    }

  std::array<signed char, 77> msgbits {};
  for (int i = 0; i < 77; ++i)
    {
      msgbits[static_cast<size_t> (i)] = static_cast<signed char> (encoded.msgbits.at (i) != 0);
    }

  std::array<signed char, 174> codeword {};
  encode174_91_ (msgbits.data (), codeword.data ());

  std::string const original = message.toStdString ();
  std::string const normalized = QString::fromLatin1 (encoded.msgsent).toStdString ();
  std::string const type = msg_type (encoded);
  char const bad = original == normalized ? ' ' : '*';

  if (short_mode)
    {
      std::cout << encoded.i3 << '.';
      if (encoded.i3 == 0)
        {
          std::cout << encoded.n3;
        }
      else
        {
          std::cout << ' ';
        }
      std::cout << "  " << std::setw (37) << std::left << original
                << ' ' << bad << ' ' << type << '\n';
    }
  else
    {
      std::cout << std::setw (2) << std::right << index << ". "
                << std::setw (37) << std::left << original << ' '
                << std::setw (37) << std::left << normalized << ' '
                << bad << "  " << encoded.i3 << '.';
      if (encoded.i3 == 0)
        {
          std::cout << encoded.n3;
        }
      else
        {
          std::cout << ' ';
        }
      std::cout << ' ' << type << '\n';
    }

  if (!short_mode && index == 1)
    {
      std::cout << "\nSource-encoded message, 77 bits:\n";
      print_bits (encoded.msgbits);
      std::cout << "\n14-bit CRC:\n";
      print_codeword_slice (codeword, 77, 91);
      std::cout << "\n83 Parity bits:\n";
      print_codeword_slice (codeword, 91, 174);
      std::cout << "\nChannel symbols (79 tones):\n";
      print_tones (encoded.tones);
    }

  return 0;
}

}

int main (int argc, char* argv[])
{
  if (argc != 2)
    {
      print_usage ();
      return 0;
    }

  std::string const arg {argv[1]};
  bool const short_mode = arg == "-t";
  bool const test_mode = short_mode || arg == "-T";

  if (!short_mode)
    {
      std::cout << "    Message                            Decoded                             Err i3.n3\n"
                << std::string (100, '-') << '\n';
    }

  if (test_mode)
    {
      int index = 1;
      for (auto const& message : kTestMessages)
        {
          if (print_one (index++, QString::fromLatin1 (message), short_mode) != 0)
            {
              return 1;
            }
        }
      return 0;
    }

  return print_one (1, QString::fromLocal8Bit (argv[1]), false);
}
