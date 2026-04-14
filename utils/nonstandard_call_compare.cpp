#include <QCoreApplication>
#include <QString>

#include <cstdint>
#include <cstdio>

#include "Modulator/FtxMessageEncoder.hpp"

namespace
{

int hash_call (QString const& call, int bits)
{
  static QString const alphabet = QStringLiteral (" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/");
  QString cw = call.trimmed ().toUpper ();
  if (cw.startsWith (QLatin1Char ('<')) && cw.endsWith (QLatin1Char ('>')) && cw.size () >= 2)
    {
      cw = cw.mid (1, cw.size () - 2);
    }
  cw = cw.left (11).leftJustified (11, QLatin1Char (' '));

  std::uint64_t n8 = 0;
  for (QChar const c : cw)
    {
      int const index = alphabet.indexOf (c);
      if (index < 0)
        {
          return -1;
        }
      n8 = 38u * n8 + static_cast<std::uint64_t> (index);
    }

  std::uint64_t const mixed = 47055833459ULL * n8;
  return static_cast<int> (mixed >> (64 - bits));
}

QString trim_fixed (QByteArray const& fixed)
{
  return QString::fromLatin1 (fixed).trimmed ();
}

bool expect_equal (char const* label, QString const& actual, QString const& expected)
{
  if (actual == expected)
    {
      return true;
    }

  std::fprintf (stderr, "%s mismatch\n  actual  : %s\n  expected: %s\n",
                label,
                actual.toLocal8Bit ().constData (),
                expected.toLocal8Bit ().constData ());
  return false;
}

bool expect_decode (char const* label,
                    decodium::txmsg::EncodedMessage const& encoded,
                    decodium::txmsg::Decode77Context* context,
                    bool received,
                    QString const& expected)
{
  if (!encoded.ok)
    {
      std::fprintf (stderr, "%s failed to encode\n", label);
      return false;
    }

  auto const decoded = decodium::txmsg::decode77 (encoded.msgbits, encoded.i3, encoded.n3, context, received);
  if (!decoded.ok)
    {
      std::fprintf (stderr, "%s failed to decode\n", label);
      return false;
    }

  return expect_equal (label, trim_fixed (decoded.msgsent), expected);
}

}  // namespace

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};

  QString const my_call = QStringLiteral ("9H1SR");
  QString const dx_call = QStringLiteral ("HF10SQ8W");
  QString const hashed_dx = QStringLiteral ("<%1>").arg (dx_call);

  decodium::txmsg::Decode77Context context;
  context.clear ();
  context.setMyCall (my_call);

  auto const cq = decodium::txmsg::encodeFt8 (QStringLiteral ("CQ %1").arg (dx_call));
  if (!expect_decode ("cq_special", cq, &context, true, QStringLiteral ("CQ %1").arg (dx_call)))
    {
      return 1;
    }

  int const h12 = hash_call (dx_call, 12);
  int const h22 = hash_call (dx_call, 22);
  if (h12 < 0 || h22 < 0)
    {
      std::fprintf (stderr, "hash computation failed for %s\n", dx_call.toLocal8Bit ().constData ());
      return 1;
    }
  if (context.lookupHash12 (h12) != hashed_dx || context.lookupHash22 (h22) != hashed_dx)
    {
      std::fprintf (stderr, "decoded CQ did not populate hash tables for %s\n",
                    dx_call.toLocal8Bit ().constData ());
      return 1;
    }

  auto const report_reply = decodium::txmsg::encodeFt8 (
      QStringLiteral ("%1 %2 -10").arg (hashed_dx, my_call));
  if (!expect_decode ("reply_report", report_reply, &context, false,
                      QStringLiteral ("%1 %2 -10").arg (hashed_dx, my_call)))
    {
      return 1;
    }

  auto const report_reply_reverse = decodium::txmsg::encodeFt8 (
      QStringLiteral ("%1 %2 R-10").arg (my_call, hashed_dx));
  if (!expect_decode ("reply_report_reverse", report_reply_reverse, &context, false,
                      QStringLiteral ("%1 %2 R-10").arg (my_call, hashed_dx)))
    {
      return 1;
    }

  auto const end_reply = decodium::txmsg::encodeFt8 (
      QStringLiteral ("%1 %2 RR73").arg (hashed_dx, my_call));
  if (!expect_decode ("reply_rr73", end_reply, &context, false,
                      QStringLiteral ("%1 %2 RR73").arg (hashed_dx, my_call)))
    {
      return 1;
    }

  std::puts ("Nonstandard callsign compare passed");
  return 0;
}
