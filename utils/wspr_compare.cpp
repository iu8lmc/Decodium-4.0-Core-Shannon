#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cstdio>

#include "Modulator/FtxMessageEncoder.hpp"
#include "wsjtx_config.h"

extern "C" void genwspr_ (char* msg, char* msgsent, int itone[],
                          fortran_charlen_t, fortran_charlen_t);

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

bool compare_message (QString const& message)
{
  QByteArray input = fixed_22 (message);
  std::array<char, 22> msg {};
  std::array<char, 22> msgsent {};
  std::array<int, 162> tones {};
  std::copy_n (input.constData (), 22, msg.data ());

  genwspr_ (msg.data (), msgsent.data (), tones.data (),
            static_cast<fortran_charlen_t> (msg.size ()),
            static_cast<fortran_charlen_t> (msgsent.size ()));

  decodium::txmsg::EncodedMessage const encoded =
      decodium::txmsg::encodeWspr (QString::fromLatin1 (msg.data (), 22), false);
  if (!encoded.ok)
    {
      std::fprintf (stderr, "C++ WSPR encoder failed for '%s'\n", input.constData ());
      return false;
    }

  QByteArray const fortran_msgsent = fixed_22 (QByteArray (msgsent.data (), 22));
  QByteArray const cpp_msgsent = fixed_22 (encoded.msgsent.left (22));
  if (fortran_msgsent != cpp_msgsent)
    {
      std::fprintf (stderr,
                    "msgsent mismatch for '%s'\n  fortran='%.*s'\n  cxx    ='%.*s'\n",
                    input.constData (),
                    fortran_msgsent.size (), fortran_msgsent.constData (),
                    cpp_msgsent.size (), cpp_msgsent.constData ());
      return false;
    }

  if (encoded.tones.size () != 162)
    {
      std::fprintf (stderr,
                    "tone count mismatch for '%s': expected 162 got %d\n",
                    input.constData (), encoded.tones.size ());
      return false;
    }

  for (int i = 0; i < 162; ++i)
    {
      if (tones[static_cast<size_t> (i)] != encoded.tones.at (i))
        {
          std::fprintf (stderr,
                        "tone mismatch for '%s' at %d: fortran=%d cxx=%d\n",
                        input.constData (), i, tones[static_cast<size_t> (i)], encoded.tones.at (i));
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
    QStringLiteral ("K1ABC FN42 00"),
    QStringLiteral ("K1ABC FN42 01"),
    QStringLiteral ("K1ABC FN42 10"),
    QStringLiteral ("K1ABC FN42 37"),
    QStringLiteral ("K1ABC FN42 60"),
    QStringLiteral ("EA8/K1ABC 20"),
    QStringLiteral ("K1ABC/P 23"),
    QStringLiteral ("K1ABC/10 27"),
    QStringLiteral ("1A/K1ABC 30"),
    QStringLiteral ("<K1ABC> FN42AB 30"),
    QStringLiteral ("<K1ABC/P> FN42AB 07"),
    QStringLiteral ("<EA8/K1ABC> FN42AB 37")
  };

  for (QString const& message : kMessages)
    {
      if (!compare_message (message))
        {
          return 1;
        }
    }

  std::printf ("WSPR compare passed for %d messages\n", kMessages.size ());
  return 0;
}
