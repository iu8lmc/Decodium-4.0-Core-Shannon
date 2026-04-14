#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cstdio>

#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/LegacyJtEncoder.hpp"
#include "wsjtx_config.h"

extern "C" void gen9_ (char* msg, int* ichk, char* msgsent, int itone[],
                       int* itype, fortran_charlen_t, fortran_charlen_t);
extern "C" int __packjt_MOD_jt_itype;
extern "C" int __packjt_MOD_jt_nc1;
extern "C" int __packjt_MOD_jt_nc2;
extern "C" int __packjt_MOD_jt_ng;
extern "C" int __packjt_MOD_jt_k1;
extern "C" int __packjt_MOD_jt_k2;

namespace
{

QByteArray fixed_22 (QString const& text)
{
  QByteArray latin = text.left (22).toLatin1 ();
  if (latin.size () < 22)
    {
      latin.append (QByteArray (22 - latin.size (), ' '));
    }
  return latin;
}

QByteArray fixed_22 (QByteArray text)
{
  if (text.size () < 22)
    {
      text.append (QByteArray (22 - text.size (), ' '));
    }
  else if (text.size () > 22)
    {
      text.truncate (22);
    }
  return text;
}

bool compare_message (QString const& message, bool check_only)
{
  QByteArray input = fixed_22 (message);
  std::array<char, 22> msg {};
  std::array<char, 22> msgsent {};
  std::array<int, 85> tones {};
  int itype = 0;
  int ichk = check_only ? 1 : 0;
  std::copy_n (input.constData (), 22, msg.data ());

  gen9_ (msg.data (), &ichk, msgsent.data (), tones.data (), &itype,
         static_cast<fortran_charlen_t> (msg.size ()),
         static_cast<fortran_charlen_t> (msgsent.size ()));

  decodium::txmsg::EncodedMessage const encoded =
      decodium::txmsg::encodeJt9 (QString::fromLatin1 (msg.data (), 22), check_only);
  QByteArray const cpp_input = decodium::legacy_jt::detail::fixed_ascii (
      QString::fromLatin1 (msg.data (), 22).toLatin1 (), 22);
  QByteArray trimmed = cpp_input;
  while (!trimmed.isEmpty () && trimmed.at (0) == ' ')
    {
      trimmed.remove (0, 1);
      trimmed = decodium::legacy_jt::detail::fixed_ascii (trimmed, 22);
    }
  decodium::legacy_jt::detail::PackedJtMessage const packed =
      decodium::legacy_jt::detail::packmsg (trimmed);

  if (!encoded.ok)
    {
      std::fprintf (stderr, "C++ JT9 encoder failed for '%s'\n", input.constData ());
      return false;
    }

  QByteArray const fortran_msgsent = fixed_22 (QByteArray (msgsent.data (), 22));
  QByteArray const cpp_msgsent = fixed_22 (encoded.msgsent.left (22));
  if (fortran_msgsent != cpp_msgsent)
    {
      std::fprintf (stderr,
                    "msgsent mismatch for '%s'\n  fortran='%.*s'\n  cxx    ='%.*s'\n"
                    "  fortran state: itype=%d nc1=%d nc2=%d ng=%d k1=%d k2=%d\n"
                    "  cxx state    : itype=%d nc1=%d nc2=%d ng=%d\n",
                    input.constData (),
                    fortran_msgsent.size (), fortran_msgsent.constData (),
                    cpp_msgsent.size (), cpp_msgsent.constData (),
                    __packjt_MOD_jt_itype, __packjt_MOD_jt_nc1, __packjt_MOD_jt_nc2,
                    __packjt_MOD_jt_ng, __packjt_MOD_jt_k1, __packjt_MOD_jt_k2,
                    packed.itype, packed.nc1, packed.nc2, packed.ng);
      return false;
    }

  if (itype != encoded.messageType)
    {
      std::fprintf (stderr,
                    "messageType mismatch for '%s': fortran=%d cxx=%d\n"
                    "  fortran state: itype=%d nc1=%d nc2=%d ng=%d k1=%d k2=%d\n"
                    "  cxx state    : itype=%d nc1=%d nc2=%d ng=%d\n",
                    input.constData (), itype, encoded.messageType,
                    __packjt_MOD_jt_itype, __packjt_MOD_jt_nc1, __packjt_MOD_jt_nc2,
                    __packjt_MOD_jt_ng, __packjt_MOD_jt_k1, __packjt_MOD_jt_k2,
                    packed.itype, packed.nc1, packed.nc2, packed.ng);
      return false;
    }

  if (check_only)
    {
      return true;
    }

  if (encoded.tones.size () != 85)
    {
      std::fprintf (stderr,
                    "tone count mismatch for '%s': expected 85 got %d\n",
                    input.constData (), encoded.tones.size ());
      return false;
    }

  for (int i = 0; i < 85; ++i)
    {
      if (tones[static_cast<size_t> (i)] != encoded.tones.at (i))
        {
          std::fprintf (stderr,
                        "tone mismatch for '%s' at %d: fortran=%d cxx=%d\n"
                        "  fortran state: itype=%d nc1=%d nc2=%d ng=%d k1=%d k2=%d\n"
                        "  cxx state    : itype=%d nc1=%d nc2=%d ng=%d\n",
                        input.constData (), i, tones[static_cast<size_t> (i)], encoded.tones.at (i),
                        __packjt_MOD_jt_itype, __packjt_MOD_jt_nc1, __packjt_MOD_jt_nc2,
                        __packjt_MOD_jt_ng, __packjt_MOD_jt_k1, __packjt_MOD_jt_k2,
                        packed.itype, packed.nc1, packed.nc2, packed.ng);
          return false;
        }
    }

  return true;
}

}  // namespace

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};

  QStringList const kMessages {
    QStringLiteral ("CQ WB9XYZ EN34"),
    QStringLiteral ("CQ DX WB9XYZ EN34"),
    QStringLiteral ("QRZ WB9XYZ EN34"),
    QStringLiteral ("KA1ABC WB9XYZ EN34"),
    QStringLiteral ("KA1ABC WB9XYZ RO"),
    QStringLiteral ("KA1ABC WB9XYZ -21"),
    QStringLiteral ("KA1ABC WB9XYZ R-19"),
    QStringLiteral ("KA1ABC WB9XYZ RRR"),
    QStringLiteral ("KA1ABC WB9XYZ 73"),
    QStringLiteral ("KA1ABC WB9XYZ"),
    QStringLiteral ("CQ 010 WB9XYZ EN34"),
    QStringLiteral ("CQ 999 WB9XYZ EN34"),
    QStringLiteral ("CQ EU WB9XYZ EN34"),
    QStringLiteral ("CQ WY WB9XYZ EN34"),
    QStringLiteral ("1A/KA1ABC WB9XYZ"),
    QStringLiteral ("E5/KA1ABC WB9XYZ"),
    QStringLiteral ("KA1ABC 1A/WB9XYZ"),
    QStringLiteral ("KA1ABC E5/WB9XYZ"),
    QStringLiteral ("KA1ABC/P WB9XYZ"),
    QStringLiteral ("KA1ABC/A WB9XYZ"),
    QStringLiteral ("KA1ABC WB9XYZ/P"),
    QStringLiteral ("KA1ABC WB9XYZ/A"),
    QStringLiteral ("CQ KA1ABC/P"),
    QStringLiteral ("CQ WB9XYZ/A"),
    QStringLiteral ("QRZ KA1ABC/P"),
    QStringLiteral ("QRZ WB9XYZ/A"),
    QStringLiteral ("DE KA1ABC/P"),
    QStringLiteral ("DE WB9XYZ/A"),
    QStringLiteral ("CQ 1A/KA1ABC"),
    QStringLiteral ("CQ E5/KA1ABC"),
    QStringLiteral ("DE 1A/KA1ABC"),
    QStringLiteral ("DE E5/KA1ABC"),
    QStringLiteral ("QRZ 1A/KA1ABC"),
    QStringLiteral ("QRZ E5/KA1ABC"),
    QStringLiteral ("CQ WB9XYZ/1A"),
    QStringLiteral ("CQ WB9XYZ/E5"),
    QStringLiteral ("QRZ WB9XYZ/1A"),
    QStringLiteral ("QRZ WB9XYZ/E5"),
    QStringLiteral ("DE WB9XYZ/1A"),
    QStringLiteral ("DE WB9XYZ/E5"),
    QStringLiteral ("CQ A000/KA1ABC FM07"),
    QStringLiteral ("CQ ZZZZ/KA1ABC FM07"),
    QStringLiteral ("QRZ W4/KA1ABC FM07"),
    QStringLiteral ("DE W4/KA1ABC FM07"),
    QStringLiteral ("CQ W4/KA1ABC -22"),
    QStringLiteral ("DE W4/KA1ABC -22"),
    QStringLiteral ("QRZ W4/KA1ABC -22"),
    QStringLiteral ("CQ W4/KA1ABC R-22"),
    QStringLiteral ("DE W4/KA1ABC R-22"),
    QStringLiteral ("QRZ W4/KA1ABC R-22"),
    QStringLiteral ("DE W4/KA1ABC 73"),
    QStringLiteral ("CQ KA1ABC FM07"),
    QStringLiteral ("QRZ KA1ABC FM07"),
    QStringLiteral ("DE KA1ABC/VE6 FM07"),
    QStringLiteral ("CQ KA1ABC/VE6 -22"),
    QStringLiteral ("DE KA1ABC/VE6 -22"),
    QStringLiteral ("QRZ KA1ABC/VE6 -22"),
    QStringLiteral ("CQ KA1ABC/VE6 R-22"),
    QStringLiteral ("DE KA1ABC/VE6 R-22"),
    QStringLiteral ("QRZ KA1ABC/VE6 R-22"),
    QStringLiteral ("DE KA1ABC 73"),
    QStringLiteral ("HELLO WORLD"),
    QStringLiteral ("ZL4/KA1ABC 73"),
    QStringLiteral ("KA1ABC XL/WB9XYZ"),
    QStringLiteral ("KA1ABC WB9XYZ/W4"),
    QStringLiteral ("DE KA1ABC/QRP 2W"),
    QStringLiteral ("KA1ABC/1 WB9XYZ/1"),
    QStringLiteral ("123456789ABCDEFGH"),
    QStringLiteral ("KA1ABC WB9XYZ EN34 OOO"),
    QStringLiteral ("KA1ABC WB9XYZ OOO"),
    QStringLiteral ("RO"),
    QStringLiteral ("RRR"),
    QStringLiteral ("73"),
  };

  int failures = 0;
  for (QString const& message : kMessages)
    {
      if (!compare_message (message, false)) ++failures;
      if (!compare_message (message, true)) ++failures;
    }

  if (failures != 0)
    {
      std::fprintf (stderr, "JT9 compare failed: %d mismatches\n", failures);
      return 1;
    }

  std::printf ("JT9 compare passed for %d messages\n", kMessages.size ());
  return 0;
}
