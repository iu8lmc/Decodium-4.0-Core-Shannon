#include <QByteArray>
#include <QString>

#include <array>
#include <iomanip>
#include <iostream>
#include <string>

#include "Modulator/FtxMessageEncoder.hpp"

namespace
{

using decodium::txmsg::EncodedMessage;

constexpr std::array<char const*, 43> kTestMessages {{
  "CQ K1ABC FN42",
  "K1ABC W9XYZ EN37",
  "W9XYZ K1ABC -11",
  "K1ABC W9XYZ R-09",
  "W9XYZ K1ABC RRR",
  "K1ABC W9XYZ 73",
  "K1ABC W9XYZ RR73",
  "CQ KH1/KH7Z",
  "CQ TEST K1ABC/R FN42",
  "K1ABC/R W9XYZ EN37",
  "W9XYZ K1ABC/R R FN42",
  "K1ABC/R W9XYZ RR73",
  "CQ TEST K1ABC FN42",
  "K1ABC/R W9XYZ/R R FN42",
  "CQ G4ABC/P IO91",
  "G4ABC/P PA9XYZ JO22",
  "PA9XYZ 590003 IO91NP",
  "G4ABC/P R 570007 JO22DB",
  "PA9XYZ G4ABC/P RR73",
  "CQ PJ4/K1ABC",
  "PJ4/K1ABC <W9XYZ>",
  "W9XYZ <PJ4/K1ABC> -11",
  "<PJ4/K1ABC> W9XYZ R-09",
  "<W9XYZ> PJ4/K1ABC RRR",
  "PJ4/K1ABC <W9XYZ> 73",
  "CQ W9XYZ EN37",
  "<W9XYZ> YW18FIFA",
  "<YW18FIFA> W9XYZ -11",
  "W9XYZ <YW18FIFA> R-09",
  "YW18FIFA <W9XYZ> RRR",
  "<W9XYZ> YW18FIFA 73",
  "TNX BOB 73 GL",
  "CQ YW18FIFA",
  "<YW18FIFA> KA1ABC",
  "KA1ABC <YW18FIFA> -11",
  "<YW18FIFA> KA1ABC R-17",
  "<KA1ABC> YW18FIFA RR73",
  "<YW18FIFA> KA1ABC 73",
  "123456789ABCDEF012",
  "<KA1ABC WB9XYZ> -03",
  "<KA1ABC WB9XYZ> R+03",
  "<KA1ABC WB9XYZ> RRR",
  "<KA1ABC WB9XYZ> 73",
}};

std::string trim_right (QByteArray const& value)
{
  QByteArray trimmed = value;
  while (!trimmed.isEmpty () && (trimmed.endsWith (' ') || trimmed.endsWith ('\0')))
    {
      trimmed.chop (1);
    }
  return trimmed.toStdString ();
}

std::string msg_type (QString const& message, EncodedMessage const& encoded)
{
  if (message.trimmed ().startsWith (QLatin1Char ('@')))
    {
      return "Fixed tone";
    }
  if (encoded.messageType == 7)
    {
      return "Sh msg";
    }
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

void print_channel_symbols (QString const& message, EncodedMessage const& encoded)
{
  if (encoded.tones.isEmpty ())
    {
      return;
    }

  std::cout << "\nChannel symbols\n";
  if (message.trimmed ().startsWith (QLatin1Char ('@')))
    {
      std::cout << encoded.tones.first () << "\n";
      return;
    }

  int tone_count = encoded.tones.size ();
  if (encoded.messageType == 7 && tone_count >= 41 && encoded.tones.at (40) < 0)
    {
      tone_count = 40;
    }

  for (int i = 0; i < tone_count; ++i)
    {
      std::cout << encoded.tones.at (i);
      if ((i + 1) % 72 == 0)
        {
          std::cout << '\n';
        }
    }
  if (tone_count % 72 != 0)
    {
      std::cout << '\n';
    }
}

void print_usage ()
{
  std::cout << "Program msk144code: Provides examples of MSK144 message packing and encoding.\n\n"
            << "Usage: msk144code \"message\"\n"
            << "       msk144code -t\n";
}

int print_one (int index, QString const& raw_message)
{
  QString const message = raw_message.toUpper ().simplified ();
  EncodedMessage const encoded = decodium::txmsg::encodeMsk144 (message);
  if (!encoded.ok)
    {
      std::cerr << "Encoding failed for: " << message.toStdString () << '\n';
      return 1;
    }

  std::string const original = message.toStdString ();
  std::string const normalized = trim_right (encoded.msgsent);
  std::string const type = msg_type (message, encoded);
  char const bad = original == normalized ? ' ' : '*';

  std::cout << std::setw (2) << std::right << index << ". "
            << std::setw (37) << std::left << original << ' '
            << std::setw (37) << std::left << normalized << ' '
            << bad << "  ";
  if (encoded.messageType == 7 || message.trimmed ().startsWith (QLatin1Char ('@')))
    {
      std::cout << "    ";
    }
  else
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
      std::cout << ' ';
    }
  std::cout << type << '\n';

  if (index == 1)
    {
      print_channel_symbols (message, encoded);
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
      for (char const* message : kTestMessages)
        {
          if (print_one (index++, QString::fromLatin1 (message)) != 0)
            {
              return 1;
            }
        }
      return 0;
    }

  return print_one (1, arg);
}
