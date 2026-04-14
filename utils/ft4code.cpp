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

constexpr std::array<unsigned char, 77> kFt4Rvec {{
  0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0,
  1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1,
  0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1
}};

std::array<char const*, 47> const kTestMessages {{
  "TNX BOB 73 GL",
  "PA9XYZ 590003 IO91NP",
  "G4ABC/P R 570007 JO22DB",
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
    case 4: return "Nonstandard calls";
    case 5: return "EU VHF Contest";
    default: return "Undefined msg type";
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

void print_channel_symbols (QVector<int> const& tones)
{
  std::cout << "\nChannel symbols (105 tones):\n";
  std::cout << "0 ";
  for (int i = 0; i < tones.size (); ++i)
    {
      std::cout << tones.at (i);
      if (i == 3 || i == 32 || i == 36 || i == 65 || i == 69 || i == 98)
        {
          std::cout << ' ';
        }
    }
  std::cout << " 0\n";
}

void print_usage ()
{
  std::cout << "\nProgram ft4code: Provides examples of FT4 message packing and encoding.\n\n"
            << "Usage: ft4code \"message\"\n"
            << "       ft4code -t\n";
}

int print_one (int index, QString const& message)
{
  EncodedMessage const encoded = decodium::txmsg::encodeFt4 (message);
  if (!encoded.ok || encoded.msgbits.size () != 77 || encoded.tones.size () != 103)
    {
      std::cerr << "Encoding failed for: " << message.toStdString () << '\n';
      return 1;
    }

  std::array<unsigned char, 77> source_bits {};
  std::array<signed char, 77> msgbits {};
  for (int i = 0; i < 77; ++i)
    {
      unsigned char const bit = static_cast<unsigned char> (encoded.msgbits.at (i) != 0);
      msgbits[static_cast<size_t> (i)] = static_cast<signed char> (bit);
      source_bits[static_cast<size_t> (i)] = static_cast<unsigned char> (bit ^ kFt4Rvec[static_cast<size_t> (i)]);
    }

  std::array<signed char, 174> codeword {};
  encode174_91_ (msgbits.data (), codeword.data ());

  std::string const original = message.toStdString ();
  std::string const normalized = QString::fromLatin1 (encoded.msgsent).toStdString ();
  std::string const type = msg_type (encoded);
  char const bad = original == normalized ? ' ' : '*';

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

  if (index == 1)
    {
      std::cout << "\nSource-encoded message before scrambling, 77 bits:\n";
      for (unsigned char bit : source_bits)
        {
          std::cout << (bit != 0 ? '1' : '0');
        }
      std::cout << "\n\nScrambling vector, 77 bits:\n";
      for (unsigned char bit : kFt4Rvec)
        {
          std::cout << (bit != 0 ? '1' : '0');
        }
      std::cout << "\n\nSource-encoded message after scrambling, 77 bits:\n";
      print_bits (encoded.msgbits);
      std::cout << "\n14-bit CRC:\n";
      print_codeword_slice (codeword, 77, 91);
      std::cout << "\n83 Parity bits:\n";
      print_codeword_slice (codeword, 91, 174);
      print_channel_symbols (encoded.tones);
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

  std::cout << "    Message                            Decoded                             Err i3.n3\n"
            << std::string (100, '-') << '\n';

  QString const arg = QString::fromLocal8Bit (argv[1]);
  if (arg == QStringLiteral ("-t"))
    {
      int index = 1;
      for (auto const& message : kTestMessages)
        {
          if (print_one (index++, QString::fromLatin1 (message)) != 0)
            {
              return 1;
            }
        }
      return 0;
    }

  QString message = arg.toUpper ().simplified ();
  return print_one (1, message);
}
