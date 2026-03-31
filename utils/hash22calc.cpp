#include <QString>

#include <array>
#include <iomanip>
#include <iostream>

extern "C"
{
int legacy_pack77_hash_call_bits_c (char const c13[13], int bits, int* hash_out);
}

namespace
{

std::array<char, 13> to_fixed_13 (QString const& text)
{
  std::array<char, 13> data {};
  data.fill (' ');
  QByteArray const latin = text.left (13).toLatin1 ();
  std::copy (latin.begin (), latin.end (), data.begin ());
  return data;
}

void print_usage ()
{
  std::cout << "Given a valid callsign, print its 22-bit hash.\n"
            << "Usage: hash22calc <callsign>\n"
            << "  e.g. hash22calc W9ABC\n";
}

}

int main (int argc, char* argv[])
{
  if (argc != 2)
    {
      print_usage ();
      return 0;
    }

  QString const callsign = QString::fromLocal8Bit (argv[1]).trimmed ().toUpper ();
  std::array<char, 13> const fixed = to_fixed_13 (callsign);
  int n22 = -1;
  if (legacy_pack77_hash_call_bits_c (fixed.data (), 22, &n22) == 0 || n22 < 0)
    {
      std::cout << "Invalid callsign\n";
      print_usage ();
      return 0;
    }

  std::cout << callsign.toStdString ()
            << std::setw (7) << std::setfill ('0') << n22
            << std::setfill (' ') << '\n';
  return 0;
}
