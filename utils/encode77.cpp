#include <QString>

#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

extern "C"
{
void legacy_pack77_reset_context_c ();
void legacy_pack77_pack_c (char const msg0[37], int* i3, int* n3,
                           char c77[77], char msgsent[37], bool* success, int received);
void legacy_pack77_split77_c (char const msg[37], int* nwords, int nw[19], char words[247]);
}

namespace
{

std::array<char, 37> to_fixed_37 (QString const& text)
{
  std::array<char, 37> data {};
  data.fill (' ');
  QByteArray const latin = text.left (37).toLatin1 ();
  std::copy (latin.begin (), latin.end (), data.begin ());
  return data;
}

QString normalize_message (QString const& message)
{
  return message.toUpper ().simplified ();
}

bool is_blank_line (QString const& line)
{
  return line.trimmed ().isEmpty ();
}

bool is_skipped_line (QString const& line)
{
  if (line.startsWith (QStringLiteral ("---")))
    {
      return true;
    }
  if (line.size () > 1 && line.at (1) == QLatin1Char ('.'))
    {
      return true;
    }
  if (line.size () > 2 && line.at (2) == QLatin1Char ('.'))
    {
      return true;
    }
  return false;
}

void print_usage ()
{
  std::cout << "Usage: encode77 \"message\"\n"
            << "       encode77 -f <infile>\n";
}

void print_header ()
{
  std::cout << "i3.n3 Err Message to be encoded                 Decoded message\n"
            << std::string (80, '-') << '\n';
}

void print_standard_line (int i3, int n3, char cerr,
                          std::string const& original, std::string const& decoded)
{
  std::cout << std::setw (2) << std::right << i3 << '.'
            << n3 << "    " << cerr << ' '
            << std::setw (37) << std::left << original << ' '
            << std::setw (37) << std::left << decoded << '\n';
}

void print_type6_line (int i3, int n3, int j2, char cerr,
                       std::string const& original, std::string const& decoded)
{
  std::cout << std::setw (2) << std::right << i3 << '.'
            << n3 << '.' << j2 << "  " << cerr << ' '
            << std::setw (37) << std::left << original << ' '
            << std::setw (37) << std::left << decoded << '\n';
}

void print_nonstandard_line (int i3, char cerr,
                             std::string const& original, std::string const& decoded)
{
  std::cout << std::setw (2) << std::right << i3 << ".     "
            << cerr << ' '
            << std::setw (37) << std::left << original << ' '
            << std::setw (37) << std::left << decoded << '\n';
}

int process_message (QString const& raw_message)
{
  QString const normalized = normalize_message (raw_message);
  std::array<char, 37> const fixed_message = to_fixed_37 (normalized);
  std::array<char, 77> bits {};
  std::array<char, 37> msgsent {};
  int i3 = -1;
  int n3 = -1;
  bool success = false;

  legacy_pack77_reset_context_c ();
  legacy_pack77_pack_c (fixed_message.data (), &i3, &n3, bits.data (), msgsent.data (), &success, 1);
  if (!success || i3 < 0)
    {
      return 0;
    }

  QString const original = QString::fromLatin1 (fixed_message.data (), 37);
  QString const decoded = QString::fromLatin1 (msgsent.data (), 37);
  char const cerr = original == decoded ? ' ' : '*';
  std::string const original_std = original.toStdString ();
  std::string const decoded_std = decoded.toStdString ();

  if (i3 == 0 && n3 != 6)
    {
      print_standard_line (i3, n3, cerr, original_std, decoded_std);
      return 0;
    }

  if (i3 == 0 && n3 == 6)
    {
      std::array<int, 19> lengths {};
      std::array<char, 19 * 13> words {};
      int nwords = 0;
      legacy_pack77_split77_c (msgsent.data (), &nwords, lengths.data (), words.data ());

      int j2 = 0;
      if (nwords == 2 && lengths[1] <= 2)
        {
          j2 = 1;
        }
      if (nwords == 2 && lengths[1] == 6)
        {
          j2 = 2;
        }
      print_type6_line (i3, n3, j2, cerr, original_std, decoded_std);
      return 0;
    }

  if (i3 >= 1)
    {
      print_nonstandard_line (i3, cerr, original_std, decoded_std);
    }

  return 0;
}

}

int main (int argc, char* argv[])
{
  if (argc != 2 && argc != 3)
    {
      print_usage ();
      return 0;
    }

  if (argc == 2)
    {
      return process_message (QString::fromLocal8Bit (argv[1]));
    }

  if (QString::fromLocal8Bit (argv[1]) != QStringLiteral ("-f"))
    {
      print_usage ();
      return 0;
    }

  std::ifstream input {argv[2]};
  if (!input)
    {
      std::cerr << "Unable to open input file\n";
      return 1;
    }

  print_header ();
  for (std::string line; std::getline (input, line); )
    {
      QString const qline = QString::fromLocal8Bit (line.c_str ());
      if (!qline.isEmpty () && qline.at (0) == QLatin1Char ('$'))
        {
          break;
        }
      if (is_blank_line (qline) || is_skipped_line (qline))
        {
          continue;
        }
      process_message (qline);
    }
  return 0;
}
