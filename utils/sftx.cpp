#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "Modulator/FtxWaveformGenerator.hpp"
#include "commons.h"

namespace
{
QString normalize_otp_key (QString const& raw)
{
  QString trimmed = raw.trimmed ();
  if (trimmed.startsWith (QStringLiteral ("OTP:"), Qt::CaseInsensitive))
    {
      return trimmed;
    }

  QString digits;
  for (QChar const ch : trimmed)
    {
      if (ch.isDigit ())
        {
          digits.append (ch);
        }
    }
  if (digits.size () < 6)
    {
      digits = digits.rightJustified (6, QLatin1Char {'0'});
    }
  else
    {
      digits = digits.right (6);
    }
  return QStringLiteral ("OTP:%1").arg (digits);
}

void clear_fox_state ()
{
  std::fill_n (foxcom_.wave, static_cast<int> (sizeof (foxcom_.wave) / sizeof (foxcom_.wave[0])), 0.0f);
  std::fill_n (&foxcom_.cmsg[0][0], static_cast<int> (sizeof (foxcom_.cmsg)), ' ');
  std::fill_n (foxcom_.textMsg, static_cast<int> (sizeof (foxcom_.textMsg)), ' ');
  std::fill_n (&foxcom3_.cmsg2[0][0], static_cast<int> (sizeof (foxcom3_.cmsg2)), ' ');
  std::fill_n (foxcom3_.itone3, 151, 0);
  foxcom_.nslots = 0;
  foxcom_.nfreq = 750;
  foxcom_.bMoreCQs = false;
  foxcom_.bSendMsg = false;
}

QString output_path_for (QString const& input_path)
{
  QString out = input_path;
  int const pos = out.indexOf (QStringLiteral ("sfox_1"));
  if (pos >= 0)
    {
      out.replace (pos, QStringLiteral ("sfox_1").size (), QStringLiteral ("sfox_2.dat"));
      return out;
    }
  return QFileInfo {input_path}.dir ().absoluteFilePath (QStringLiteral ("sfox_2.dat"));
}

void print_usage ()
{
  std::cout << "Usage: sftx <message_file_name> <foxcall> <ckey>\n";
}
}

int main (int argc, char* argv[])
{
  if (argc != 4)
    {
      print_usage ();
      return 0;
    }

  QString const input_path = QFileInfo {QString::fromLocal8Bit (argv[1])}.absoluteFilePath ();
  QFile input {input_path};
  if (!input.open (QIODevice::ReadOnly | QIODevice::Text))
    {
      std::cerr << "Unable to open " << input_path.toStdString () << '\n';
      return 1;
    }

  QStringList raw_lines;
  while (!input.atEnd ())
    {
      raw_lines << QString::fromLocal8Bit (input.readLine ()).remove (QLatin1Char {'\n'}).remove (QLatin1Char {'\r'});
    }
  if (raw_lines.isEmpty ())
    {
      std::cerr << "Message file is empty: " << input_path.toStdString () << '\n';
      return 1;
    }

  clear_fox_state ();

  int nslots = std::min (raw_lines.size (), 5);
  std::array<QByteArray, 5> fixed {};
  for (int i = 0; i < nslots; ++i)
    {
      fixed[static_cast<size_t> (i)] = raw_lines.at (i).leftJustified (40, QLatin1Char {' '}, true).left (40).toLatin1 ();
    }

  foxcom_.bMoreCQs = !fixed[0].isEmpty () && fixed[0].size () >= 40 && fixed[0].at (39) == '1';
  foxcom_.bSendMsg = !fixed[static_cast<size_t> (nslots - 1)].isEmpty ()
                     && fixed[static_cast<size_t> (nslots - 1)].size () >= 39
                     && fixed[static_cast<size_t> (nslots - 1)].at (38) == '1';
  if (foxcom_.bSendMsg)
    {
      QByteArray const free_text = fixed[static_cast<size_t> (nslots - 1)].left (26).leftJustified (26, ' ', true);
      std::copy_n (free_text.constData (), 26, foxcom_.textMsg);
      if (nslots > 2)
        {
          nslots = 2;
        }
    }

  foxcom_.nslots = nslots;
  for (int i = 0; i < nslots; ++i)
    {
      std::copy_n (fixed[static_cast<size_t> (i)].constData (), 40, foxcom_.cmsg[i]);
    }

  QString const otp_key = normalize_otp_key (QString::fromLocal8Bit (argv[3]));
  if (!decodium::txwave::generateSuperFoxTx (otp_key))
    {
      std::cerr << "Failed to assemble/encode SuperFox message set\n";
      return 1;
    }

  QString const output_path = output_path_for (input_path);
  std::ofstream out {output_path.toStdString ()};
  if (!out)
    {
      std::cerr << "Unable to write " << output_path.toStdString () << '\n';
      return 1;
    }

  for (int i = 0; i < 151; ++i)
    {
      out << std::setw (4) << foxcom3_.itone3[i];
      if ((i + 1) % 20 == 0)
        {
          out << '\n';
        }
    }
  if (151 % 20 != 0)
    {
      out << '\n';
    }

  return 0;
}
