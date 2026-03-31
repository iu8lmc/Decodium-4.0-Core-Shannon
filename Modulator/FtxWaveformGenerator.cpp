#include "FtxWaveformGenerator.hpp"

#include "FtxMessageEncoder.hpp"
#include "commons.h"

#include <QStringList>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <fftw3.h>

#include "wsjtx_config.h"

extern "C" {
void legacy_pack77_pack28_c (char const c13[13], int* n28, bool* success);
void legacy_pack77_split77_c (char const msg[37], int* nwords, int nw[19], char words[247]);
void legacy_pack77_packtext77_c (char const c13[13], char c71[71], bool* success);
std::uint32_t nhash2 (void const* key, std::uint64_t length, std::uint32_t initval);
void qpc_encode (unsigned char* y, unsigned char const* x);
foxcom_block_t foxcom_ {};
foxcom3_block_t foxcom3_ {};
}

namespace
{
constexpr double kTwoPi = 6.28318530717958647692;
constexpr int kFt2FoxNsym = 103;
constexpr int kFt2FoxNsps = 4 * 288;
constexpr int kFt2FoxNwave = (kFt2FoxNsym + 2) * kFt2FoxNsps;
constexpr int kFt2FoxNfft = 614400;
constexpr int kFt8FoxNsym = 79;
constexpr int kFt8FoxNsps = 4 * 1920;
constexpr int kFt8FoxNwave = kFt8FoxNsym * kFt8FoxNsps;
constexpr int kFt8FoxNfft = 614400;
constexpr int kSuperFoxNsym = 151;
constexpr int kSuperFoxNsps = 4 * 1024;
constexpr int kSuperFoxWaveSamples = (kSuperFoxNsym + 2) * kSuperFoxNsps;
constexpr int kSuperFoxNqu1Rks = 203514677;
constexpr char kSuperFoxBase38[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/";

std::array<int, 24> const kSuperFoxSyncPositions = {{
  1, 2, 4, 7, 11, 16, 22, 29, 37, 39, 42, 43, 45, 48,
  52, 57, 63, 70, 78, 80, 83, 84, 86, 89
}};

struct Split77Words
{
  int nwords {0};
  std::array<int, 19> lengths {};
  std::array<std::array<char, 13>, 19> words {};
};

struct SuperFoxAssembled
{
  QString line;
  QString foxCall;
};

int clamp_int (int value, int low, int high)
{
  if (value < low)
    {
      return low;
    }
  if (value > high)
    {
      return high;
    }
  return value;
}

double gfsk_pulse (double bt, double t)
{
  double const c = 0.5 * kTwoPi * std::sqrt (2.0 / std::log (2.0));
  return 0.5 * (std::erf (c * bt * (t + 0.5)) - std::erf (c * bt * (t - 0.5)));
}

double wrap_phase (double phase)
{
  phase = std::fmod (phase, kTwoPi);
  if (phase < 0.0)
    {
      phase += kTwoPi;
    }
  return phase;
}

QByteArray to_fixed_latin (QString const& text, int width)
{
  QByteArray latin = text.left (width).toLatin1 ();
  if (latin.size () < width)
    {
      latin.append (QByteArray (width - latin.size (), ' '));
    }
  return latin;
}

Split77Words split77_words (QString const& message)
{
  Split77Words result {};
  std::array<char, 37> fixed_message {};
  std::fill (fixed_message.begin (), fixed_message.end (), ' ');
  QByteArray const latin = to_fixed_latin (message, 37);
  std::copy_n (latin.constData (), 37, fixed_message.data ());
  legacy_pack77_split77_c (fixed_message.data (), &result.nwords, result.lengths.data (),
                           reinterpret_cast<char*> (result.words.data ()));
  result.nwords = clamp_int (result.nwords, 0, 19);
  return result;
}

QString split_word (Split77Words const& words, int index)
{
  if (index < 0 || index >= words.nwords)
    {
      return {};
    }
  int const length = clamp_int (words.lengths[static_cast<size_t> (index)], 0, 13);
  return QString::fromLatin1 (words.words[static_cast<size_t> (index)].data (), length);
}

bool is_signed_report_word (QString const& word)
{
  return !word.isEmpty () && (word.at (0) == QLatin1Char {'+'} || word.at (0) == QLatin1Char {'-'});
}

int base38_index (QChar ch)
{
  ushort const code = ch.toLatin1 ();
  for (int i = 0; kSuperFoxBase38[i] != '\0'; ++i)
    {
      if (ushort (kSuperFoxBase38[i]) == code)
        {
          return i;
        }
    }
  return 0;
}

std::uint64_t pack_base38_11 (QString const& text)
{
  QString padded = text.leftJustified (11, QLatin1Char {' '}, true).left (11);
  std::uint64_t value = 0;
  for (QChar const ch : padded)
    {
      value = value * 38u + static_cast<std::uint64_t> (base38_index (ch));
    }
  return value;
}

void write_integer_bits (std::array<unsigned char, 329>& bits, int start, int width, std::uint64_t value)
{
  for (int i = 0; i < width; ++i)
    {
      int const shift = width - 1 - i;
      bits[static_cast<size_t> (start + i)] = static_cast<unsigned char> ((value >> shift) & 1u);
    }
}

std::uint64_t read_integer_bits (std::array<unsigned char, 329> const& bits, int start, int width)
{
  std::uint64_t value = 0;
  for (int i = 0; i < width; ++i)
    {
      value = (value << 1u) | bits[static_cast<size_t> (start + i)];
    }
  return value;
}

bool pack28_local (QString const& call, int* value_out)
{
  if (value_out)
    {
      *value_out = 0;
    }
  std::array<char, 13> fixed_call {};
  std::fill (fixed_call.begin (), fixed_call.end (), ' ');
  QByteArray const latin = to_fixed_latin (call, 13);
  std::copy_n (latin.constData (), 13, fixed_call.data ());
  bool success = false;
  int value = 0;
  legacy_pack77_pack28_c (fixed_call.data (), &value, &success);
  if (!success)
    {
      return false;
    }
  if (value_out)
    {
      *value_out = value;
    }
  return true;
}

bool packtext13_local (QString const& text, std::array<unsigned char, 71>* bits_out)
{
  if (!bits_out)
    {
      return false;
    }
  bits_out->fill (0);
  std::array<char, 13> fixed_text {};
  std::fill (fixed_text.begin (), fixed_text.end (), ' ');
  QByteArray const latin = to_fixed_latin (text, 13);
  std::copy_n (latin.constData (), 13, fixed_text.data ());
  std::array<char, 71> packed {};
  bool success = false;
  legacy_pack77_packtext77_c (fixed_text.data (), packed.data (), &success);
  if (!success)
    {
      return false;
    }
  for (int i = 0; i < 71; ++i)
    {
      (*bits_out)[static_cast<size_t> (i)] = packed[static_cast<size_t> (i)] == '1' ? 1u : 0u;
    }
  return true;
}

int packgrid_superfox (QString const& grid_in, bool* text_out)
{
  if (text_out)
    {
      *text_out = false;
    }

  constexpr int kNgBase = 180 * 180;
  QString grid = grid_in.leftJustified (4, QLatin1Char {' '}, true).left (4).toUpper ();
  if (grid == QStringLiteral ("    "))
    {
      return kNgBase + 1;
    }

  if (grid.startsWith (QLatin1Char {'-'}))
    {
      bool ok = false;
      int const report = grid.mid (1, 2).toInt (&ok);
      if (ok && report >= 1 && report <= 30)
        {
          return kNgBase + 1 + report;
        }
    }
  else if (grid.startsWith (QStringLiteral ("R-")))
    {
      bool ok = false;
      int const report = grid.mid (2, 2).toInt (&ok);
      if (ok && report >= 1 && report <= 30)
        {
          return kNgBase + 31 + report;
        }
    }
  else if (grid == QStringLiteral ("RO  "))
    {
      return kNgBase + 62;
    }
  else if (grid == QStringLiteral ("RRR "))
    {
      return kNgBase + 63;
    }
  else if (grid == QStringLiteral ("73  "))
    {
      return kNgBase + 64;
    }

  {
    bool ok = false;
    int report = grid.toInt (&ok);
    QChar const c1 = grid.at (0);
    if (!ok)
      {
        report = grid.mid (1, 3).toInt (&ok);
      }
    if (ok && report >= -50 && report <= 49)
      {
        return packgrid_superfox ((c1 == QLatin1Char {'R'})
                                    ? QStringLiteral ("LA%1").arg (report + 50, 2, 10, QLatin1Char {'0'})
                                    : QStringLiteral ("KA%1").arg (report + 50, 2, 10, QLatin1Char {'0'}),
                                  text_out);
      }
  }

  bool text = false;
  text = text || grid.at (0) < QLatin1Char {'A'} || grid.at (0) > QLatin1Char {'R'};
  text = text || grid.at (1) < QLatin1Char {'A'} || grid.at (1) > QLatin1Char {'R'};
  text = text || grid.at (2) < QLatin1Char {'0'} || grid.at (2) > QLatin1Char {'9'};
  text = text || grid.at (3) < QLatin1Char {'0'} || grid.at (3) > QLatin1Char {'9'};
  if (text)
    {
      if (text_out)
        {
          *text_out = true;
        }
      return 0;
    }

  char const g1 = grid.at (0).toLatin1 ();
  char const g2 = grid.at (1).toLatin1 ();
  char const g3 = grid.at (2).toLatin1 ();
  char const g4 = grid.at (3).toLatin1 ();
  float const dlong = float (180 - 20 * (g1 - 'A')) - float (2 * (g3 - '0')) - (5.0f * (12.0f + 0.5f)) / 60.0f;
  float const dlat = float (-90 + 10 * (g2 - 'A') + (g4 - '0')) + (2.5f * (12.0f + 0.5f)) / 60.0f;
  int const longitude = static_cast<int> (dlong);
  int const latitude = static_cast<int> (dlat + 90.0f);
  return ((longitude + 180) / 2) * 180 + latitude;
}

void copy_bits (std::array<unsigned char, 329>& dst, int start, std::array<unsigned char, 71> const& src)
{
  for (int i = 0; i < 71; ++i)
    {
      dst[static_cast<size_t> (start + i)] = src[static_cast<size_t> (i)];
    }
}

QString normalize_free_text26 (QString const& text)
{
  QString normalized = text.leftJustified (26, QLatin1Char {' '}, true).left (26);
  int last_non_space = -1;
  for (int i = 0; i < normalized.size (); ++i)
    {
      if (normalized.at (i) != QLatin1Char {' '})
        {
          last_non_space = i;
        }
    }
  for (int i = last_non_space + 1; i < normalized.size (); ++i)
    {
      normalized[i] = QLatin1Char {'.'};
    }
  return normalized;
}

void append_superfox_piece (int ntype, QString const& msg26, QString const& mycall0, QString const& mygrid0,
                            int* nmsg, int* nbits, int* nb_mycall, QString* mycall, QString* mygrid,
                            QString* msg1, std::array<QString, 10>* hiscall, std::array<QString, 5>* rpt2)
{
  if (!mycall0.trimmed ().isEmpty ())
    {
      *mycall = mycall0.leftJustified (13, QLatin1Char {' '}, true).left (13);
    }
  if (!mygrid0.trimmed ().isEmpty ())
    {
      *mygrid = mygrid0.leftJustified (4, QLatin1Char {' '}, true).left (4);
    }
  if (ntype >= 1)
    {
      *nb_mycall = 28;
    }

  if (ntype == 0)
    {
      if (*nbits + *nb_mycall <= 191)
        {
          ++nmsg[0];
          *nbits += 142;
        }
      return;
    }

  if (ntype == 1)
    {
      if (*nbits + *nb_mycall <= 318)
        {
          ++nmsg[1];
          *nbits += 15;
          *msg1 = msg26;
        }
      return;
    }

  QString const trimmed_msg = msg26.trimmed ();
  int const first_space = trimmed_msg.indexOf (QLatin1Char {' '});
  QString const first_word = first_space > 0 ? trimmed_msg.left (first_space) : trimmed_msg;
  if (ntype == 2)
    {
      if (*nbits + *nb_mycall <= 305)
        {
          ++nmsg[2];
          *nbits += 28;
          int const index = nmsg[2] - 1;
          (*hiscall)[static_cast<size_t> (index + 5)] = first_word.left (6);
        }
      return;
    }

  if (ntype == 3)
    {
      if (*nbits + *nb_mycall <= 300)
        {
          ++nmsg[3];
          *nbits += 33;
          int const index = nmsg[3] - 1;
          (*hiscall)[static_cast<size_t> (index)] = first_word.left (6);
          int const plus_index = trimmed_msg.lastIndexOf (QLatin1Char {'+'});
          int const minus_index = trimmed_msg.lastIndexOf (QLatin1Char {'-'});
          int const report_index = std::max (plus_index, minus_index);
          if (report_index >= 0)
            {
              (*rpt2)[static_cast<size_t> (index)] = trimmed_msg.mid (report_index, 4).leftJustified (
                  4, QLatin1Char {' '}, true);
            }
        }
    }
}

SuperFoxAssembled assemble_superfox_message ()
{
  int const slots = clamp_int (foxcom_.nslots, 0, 5);
  int nmsg[4] {0, 0, 0, 0};
  int nbits = 0;
  int nb_mycall = 0;
  QString mycall;
  QString mygrid;
  QString msg1;
  std::array<QString, 10> hiscall {};
  std::array<QString, 5> rpt2 {};

  for (int slot = 0; slot < slots; ++slot)
    {
      QString const msg37 = QString::fromLatin1 (foxcom_.cmsg[slot], 40).left (37);
      Split77Words const split = split77_words (msg37);
      QString const w1 = split_word (split, 0);
      QString const w2 = split_word (split, 1);
      QString const w3 = split_word (split, 2);
      QString const w4 = split_word (split, 3);
      QString const w5 = split_word (split, 4);

      if (msg37.startsWith (QStringLiteral ("CQ ")))
        {
          append_superfox_piece (1, msg37.left (26), w2.left (12), w3.left (4), nmsg, &nbits,
                                 &nb_mycall, &mycall, &mygrid, &msg1, &hiscall, &rpt2);
          continue;
        }

      if (msg37.contains (QLatin1Char {';'}))
        {
          QString const bracket_call = (w4.startsWith (QLatin1Char {'<'}) && w4.endsWith (QLatin1Char {'>'})
                                        && w4.size () >= 2)
                                           ? w4.mid (1, w4.size () - 2)
                                           : w4;
          QString const msg_rr73 = QStringLiteral ("%1 %2 RR73").arg (w1.left (6), bracket_call);
          append_superfox_piece (2, msg_rr73.left (26), bracket_call.left (12), QString {}, nmsg,
                                 &nbits, &nb_mycall, &mycall, &mygrid, &msg1, &hiscall, &rpt2);
          QString const msg_report = QStringLiteral ("%1 %2 %3").arg (w3.left (6), bracket_call, w5.left (3));
          append_superfox_piece (3, msg_report.left (26), bracket_call.left (12), QString {}, nmsg,
                                 &nbits, &nb_mycall, &mycall, &mygrid, &msg1, &hiscall, &rpt2);
          continue;
        }

      if (msg37.contains (QStringLiteral (" RR73")))
        {
          append_superfox_piece (2, msg37.left (26), w2.left (12), QString {}, nmsg, &nbits,
                                 &nb_mycall, &mycall, &mygrid, &msg1, &hiscall, &rpt2);
          continue;
        }

      if (split.nwords == 3 && split.lengths[2] == 3 && is_signed_report_word (w3))
        {
          append_superfox_piece (3, msg37.left (26), w2.left (12), QString {}, nmsg, &nbits,
                                 &nb_mycall, &mycall, &mygrid, &msg1, &hiscall, &rpt2);
          continue;
        }

      append_superfox_piece (0, msg37.left (26), QString {}, QString {}, nmsg, &nbits, &nb_mycall,
                             &mycall, &mygrid, &msg1, &hiscall, &rpt2);
    }

  nbits += nb_mycall;
  QString line;
  if (nmsg[1] >= 1)
    {
      line = msg1.trimmed ();
    }
  else
    {
      line = mycall.trimmed ();
      for (int i = 0; i < nmsg[3]; ++i)
        {
          line = line.trimmed () + QStringLiteral ("  %1 %2")
                                     .arg (hiscall[static_cast<size_t> (i)].trimmed (),
                                           rpt2[static_cast<size_t> (i)]);
        }
      for (int i = 0; i < nmsg[2]; ++i)
        {
          line = line.trimmed () + QStringLiteral ("  %1")
                                     .arg (hiscall[static_cast<size_t> (i + 5)].trimmed ());
        }
    }

  SuperFoxAssembled result;
  result.line = line.left (120);
  result.foxCall = mycall.left (11);
  return result;
}

bool pack_superfox_message (QString const& line_in, QString const& ckey_in, bool bMoreCQs, bool bSendMsg,
                            QString const& freeTextMsg_in, std::array<unsigned char, 50>* xin_out)
{
  if (!xin_out)
    {
      return false;
    }

  xin_out->fill (0);
  std::array<unsigned char, 329> msgbits {};
  QString const line = line_in.left (120);
  QStringList words = line.split (QLatin1Char {' '}, Qt::SkipEmptyParts);
  if (words.size () > 16)
    {
      words = words.mid (0, 16);
    }

  QString const ckey = ckey_in.leftJustified (10, QLatin1Char {' '}, true).left (10);
  bool otp_ok = false;
  int const otp = ckey.mid (4, 6).trimmed ().toInt (&otp_ok);
  write_integer_bits (msgbits, 306, 20, otp_ok ? static_cast<std::uint64_t> (otp) : 0u);

  int i3 = 0;
  int nh1 = 0;
  int nh2 = 0;

  if (line.startsWith (QStringLiteral ("CQ ")) && words.size () >= 3)
    {
      i3 = 3;
      write_integer_bits (msgbits, 0, 58, pack_base38_11 (words.at (1)));
      bool grid_is_text = false;
      int const packed_grid = packgrid_superfox (words.at (2).left (4), &grid_is_text);
      if (grid_is_text)
        {
          return false;
        }
      write_integer_bits (msgbits, 58, 15, static_cast<std::uint64_t> (packed_grid));
      write_integer_bits (msgbits, 326, 3, 3u);
    }
  else
    {
      if (words.isEmpty ())
        {
          return false;
        }
      int fox_call = 0;
      if (!pack28_local (words.at (0), &fox_call))
        {
          return false;
        }
      write_integer_bits (msgbits, 0, 28, static_cast<std::uint64_t> (fox_call));

      if (bSendMsg)
        {
          std::fill_n (msgbits.begin () + 140, 20, 1u);
        }

      int bit_offset = 28;
      for (int i = 1; i < words.size (); ++i)
        {
          if (is_signed_report_word (words.at (i)))
            {
              continue;
            }
          int const next_index = std::min (i + 1, words.size () - 1);
          if (next_index > i && is_signed_report_word (words.at (next_index)))
            {
              continue;
            }
          int packed_call = 0;
          if (!pack28_local (words.at (i), &packed_call))
            {
              return false;
            }
          write_integer_bits (msgbits, bit_offset, 28, static_cast<std::uint64_t> (packed_call));
          bit_offset += 28;
          ++nh1;
          if (nh1 >= 5)
            {
              break;
            }
        }

      int call_bit_offset = 168;
      int report_bit_offset = 280;
      if (bSendMsg)
        {
          i3 = 2;
          call_bit_offset = 28 + 28 * nh1;
          report_bit_offset = 140 + 5 * nh1;
        }

      for (int i = 1; i + 1 < words.size (); ++i)
        {
          if (!is_signed_report_word (words.at (i + 1)))
            {
              continue;
            }
          int packed_call = 0;
          if (!pack28_local (words.at (i), &packed_call))
            {
              return false;
            }
          write_integer_bits (msgbits, call_bit_offset, 28, static_cast<std::uint64_t> (packed_call));
          bool ok = false;
          int report = words.at (i + 1).toInt (&ok);
          if (!ok)
            {
              report = 0;
            }
          report = clamp_int (report, -18, 12);
          write_integer_bits (msgbits, report_bit_offset, 5, static_cast<std::uint64_t> (report + 18));
          ++nh2;
          if (nh2 >= 4 || nh1 + nh2 >= 9)
            {
              break;
            }
          call_bit_offset += 28;
          report_bit_offset += 5;
        }
    }

  if (bSendMsg)
    {
      QString const free_text = normalize_free_text26 (freeTextMsg_in);
      std::array<unsigned char, 71> bits_a {};
      std::array<unsigned char, 71> bits_b {};
      if (!packtext13_local (free_text.left (13), &bits_a) || !packtext13_local (free_text.mid (13, 13), &bits_b))
        {
          return false;
        }
      if (i3 == 3)
        {
          copy_bits (msgbits, 73, bits_a);
          copy_bits (msgbits, 144, bits_b);
        }
      else if (i3 == 2)
        {
          copy_bits (msgbits, 160, bits_a);
          copy_bits (msgbits, 231, bits_b);
        }
      write_integer_bits (msgbits, 326, 3, static_cast<std::uint64_t> (i3));
    }

  if (bMoreCQs)
    {
      msgbits[305] = 1u;
    }

  i3 = static_cast<int> (read_integer_bits (msgbits, 326, 3));
  if (i3 == 0)
    {
      for (int i = 0; i < 9; ++i)
        {
          int const offset = 28 + i * 28;
          if (read_integer_bits (msgbits, offset, 28) == 0u)
            {
              write_integer_bits (msgbits, offset, 28, static_cast<std::uint64_t> (kSuperFoxNqu1Rks));
            }
        }
    }
  else if (i3 == 3)
    {
      bool all_zero = true;
      for (int i = 0; i < 7; ++i)
        {
          if (read_integer_bits (msgbits, 73 + i * 32, 32) != 0u)
            {
              all_zero = false;
              break;
            }
        }
      if (all_zero)
        {
          for (int i = 0; i < 7; ++i)
            {
              write_integer_bits (msgbits, 73 + i * 32, 32, static_cast<std::uint64_t> (kSuperFoxNqu1Rks));
            }
        }
    }

  for (int i = 0; i < 47; ++i)
    {
      (*xin_out)[static_cast<size_t> (i)] = static_cast<unsigned char> (read_integer_bits (msgbits, i * 7, 7));
    }

  std::uint32_t const crc21 = nhash2 (xin_out->data (), 47u, 571u) & ((1u << 21) - 1u);
  (*xin_out)[47] = static_cast<unsigned char> (crc21 / 16384u);
  (*xin_out)[48] = static_cast<unsigned char> ((crc21 / 128u) & 127u);
  (*xin_out)[49] = static_cast<unsigned char> (crc21 & 127u);
  std::reverse (xin_out->begin (), xin_out->end ());
  return true;
}

void build_superfox_channel_symbols (std::array<unsigned char, 50> const& xin, int tones[151])
{
  std::array<unsigned char, 128> codeword {};
  qpc_encode (codeword.data (), xin.data ());
  std::rotate (codeword.begin (), codeword.begin () + 1, codeword.end ());
  codeword[127] = 0u;

  int sync_index = 0;
  int data_index = 0;
  for (int symbol = 1; symbol <= kSuperFoxNsym; ++symbol)
    {
      if (sync_index < static_cast<int> (kSuperFoxSyncPositions.size ())
          && symbol == kSuperFoxSyncPositions[static_cast<size_t> (sync_index)])
        {
          tones[symbol - 1] = 0;
          if (sync_index + 1 < static_cast<int> (kSuperFoxSyncPositions.size ()))
            {
              ++sync_index;
            }
        }
      else
        {
          tones[symbol - 1] = static_cast<int> (codeword[static_cast<size_t> (data_index)]) + 1;
          ++data_index;
        }
    }
}

void generate_superfox_waveform (int const* itone, float* wave, int wave_capacity)
{
  if (!itone || !wave || wave_capacity <= 0)
    {
      return;
    }

  std::fill_n (wave, wave_capacity, 0.0f);
  std::vector<double> dphi (kSuperFoxWaveSamples, 0.0);
  std::array<double, 3 * kSuperFoxNsps> pulse {};
  double const dphi_peak = kTwoPi / double (kSuperFoxNsps);

  for (int i = 0; i < 3 * kSuperFoxNsps; ++i)
    {
      double const tt = (double (i + 1) - 1.5 * double (kSuperFoxNsps)) / double (kSuperFoxNsps);
      pulse[static_cast<size_t> (i)] = gfsk_pulse (8.0, tt);
    }

  for (int j = 0; j < kSuperFoxNsym; ++j)
    {
      int const ib = j * kSuperFoxNsps;
      for (int i = 0; i < 3 * kSuperFoxNsps; ++i)
        {
          dphi[static_cast<size_t> (ib + i)] += dphi_peak * pulse[static_cast<size_t> (i)]
              * double (itone[j]);
        }
    }

  for (int i = 0; i < 2 * kSuperFoxNsps; ++i)
    {
      dphi[static_cast<size_t> (i)] += dphi_peak * double (itone[0])
          * pulse[static_cast<size_t> (kSuperFoxNsps + i)];
      dphi[static_cast<size_t> (kSuperFoxNsym * kSuperFoxNsps + i)] += dphi_peak
          * double (itone[kSuperFoxNsym - 1]) * pulse[static_cast<size_t> (i)];
    }

  double const carrier_step = kTwoPi * 750.0 / 48000.0;
  for (double& value : dphi)
    {
      value += carrier_step;
    }

  double phi = 0.0;
  int const generated = std::min (wave_capacity, kSuperFoxWaveSamples);
  for (int sample = 0; sample < generated - 1; ++sample)
    {
      wave[sample] = static_cast<float> (std::sin (phi));
      phi += dphi[static_cast<size_t> (sample + 1)];
    }

  int const nramp = kSuperFoxNsps / 8;
  for (int i = 0; i < kSuperFoxNsps - nramp && i < generated; ++i)
    {
      wave[i] = 0.0f;
    }
  for (int i = 0; i < nramp && (kSuperFoxNsps - nramp + i) < generated; ++i)
    {
      double const env = 0.5 * (1.0 - std::cos (kTwoPi * double (i) / (2.0 * double (nramp))));
      int const index = kSuperFoxNsps - nramp + i;
      wave[index] = static_cast<float> (wave[index] * env);
    }

  int const tail_start = (kSuperFoxNsym + 1) * kSuperFoxNsps;
  for (int i = 0; i < nramp && (tail_start + i) < generated; ++i)
    {
      double const env = 0.5 * (1.0 + std::cos (kTwoPi * double (i) / (2.0 * double (nramp))));
      wave[tail_start + i] = static_cast<float> (wave[tail_start + i] * env);
    }
  for (int i = tail_start + nramp; i < generated; ++i)
    {
      wave[i] = 0.0f;
    }
}

void generate_superfox_complex_waveform (int const* idat, float f0, int const* isync,
                                         int* itone_out, std::complex<float>* cdat, int capacity)
{
  if (!idat || !isync || !itone_out || !cdat || capacity <= 0)
    {
      return;
    }

  std::fill_n (itone_out, kSuperFoxNsym, 0);
  std::fill_n (cdat, capacity, std::complex<float> {});

  int sync_index = 0;
  int data_index = 0;
  for (int symbol = 1; symbol <= kSuperFoxNsym; ++symbol)
    {
      if (sync_index < 24 && symbol == isync[sync_index])
        {
          itone_out[symbol - 1] = 0;
          if (sync_index + 1 < 24)
            {
              ++sync_index;
            }
        }
      else
        {
          itone_out[symbol - 1] = idat[data_index] + 1;
          ++data_index;
        }
    }

  constexpr int kComplexNsps = 1024;
  constexpr int kComplexNpts = (kSuperFoxNsym + 2) * kComplexNsps;
  std::vector<double> dphi (static_cast<size_t> (kComplexNpts), 0.0);
  std::array<double, 3 * kComplexNsps> pulse {};
  double const dphi_peak = kTwoPi / double (kComplexNsps);
  for (int i = 0; i < static_cast<int> (pulse.size ()); ++i)
    {
      double const tt = (double (i + 1) - 1.5 * double (kComplexNsps)) / double (kComplexNsps);
      pulse[static_cast<size_t> (i)] = gfsk_pulse (8.0, tt);
    }

  for (int j = 0; j < kSuperFoxNsym; ++j)
    {
      int const ib = j * kComplexNsps;
      for (int i = 0; i < static_cast<int> (pulse.size ()); ++i)
        {
          dphi[static_cast<size_t> (ib + i)] += dphi_peak * pulse[static_cast<size_t> (i)]
              * double (itone_out[j]);
        }
    }
  for (int i = 0; i < 2 * kComplexNsps; ++i)
    {
      dphi[static_cast<size_t> (i)] += dphi_peak * double (itone_out[0])
          * pulse[static_cast<size_t> (kComplexNsps + i)];
      dphi[static_cast<size_t> (kSuperFoxNsym * kComplexNsps + i)] += dphi_peak
          * double (itone_out[kSuperFoxNsym - 1]) * pulse[static_cast<size_t> (i)];
    }

  double phi = 0.0;
  double const carrier_step = kTwoPi * double (f0) / 12000.0;
  int const generated = std::min (capacity, kComplexNpts);
  for (int sample = 0; sample < generated - 1; ++sample)
    {
      cdat[sample] = std::complex<float> {float (std::cos (phi)), float (std::sin (phi))};
      phi += dphi[static_cast<size_t> (sample + 1)] + carrier_step;
    }

  int const nramp = kComplexNsps / 8;
  for (int i = 0; i < kComplexNsps - nramp && i < generated; ++i)
    {
      cdat[i] = std::complex<float> {};
    }
  for (int i = 0; i < nramp && (kComplexNsps - nramp + i) < generated; ++i)
    {
      double const env = 0.5 * (1.0 - std::cos (kTwoPi * double (i) / (2.0 * double (nramp))));
      cdat[kComplexNsps - nramp + i] *= float (env);
    }

  int const tail_start = (kSuperFoxNsym + 1) * kComplexNsps;
  for (int i = 0; i < nramp && (tail_start + i) < generated; ++i)
    {
      double const env = 0.5 * (1.0 + std::cos (kTwoPi * double (i) / (2.0 * double (nramp))));
      cdat[tail_start + i] *= float (env);
    }
  for (int i = tail_start + nramp; i < generated; ++i)
    {
      cdat[i] = std::complex<float> {};
    }
}

QVector<float> generate_ft2_ft4_wave (int const* itone, int nsym, int nsps, float fsample, float f0)
{
  if (!itone || nsym <= 0 || nsps <= 0 || fsample <= 0.0f)
    {
      return {};
    }

  QVector<double> pulse (3 * nsps);
  for (int i = 0; i < pulse.size (); ++i)
    {
      double const tt = (double (i + 1) - 1.5 * double (nsps)) / double (nsps);
      pulse[i] = gfsk_pulse (1.0, tt);
    }

  QVector<double> dphi ((nsym + 2) * nsps, 0.0);
  double const dphi_peak = kTwoPi / double (nsps);
  for (int j = 0; j < nsym; ++j)
    {
      int const ib = j * nsps;
      for (int i = 0; i < 3 * nsps; ++i)
        {
          dphi[ib + i] += dphi_peak * pulse[i] * double (itone[j]);
        }
    }

  double const carrier_step = kTwoPi * double (f0) / double (fsample);
  QVector<float> wave ((nsym + 2) * nsps, 0.0f);
  double phi = 0.0;
  for (int i = 0; i < wave.size (); ++i)
    {
      wave[i] = std::sin (phi);
      phi = wrap_phase (phi + dphi[i] + carrier_step);
    }

  for (int i = 0; i < nsps; ++i)
    {
      double const env = 0.5 * (1.0 - std::cos (kTwoPi * double (i) / (2.0 * double (nsps))));
      wave[i] = float (wave[i] * env);
    }

  int const tail_start = (nsym + 1) * nsps;
  for (int i = 0; i < nsps; ++i)
    {
      double const env = 0.5 * (1.0 + std::cos (kTwoPi * double (i) / (2.0 * double (nsps))));
      wave[tail_start + i] = float (wave[tail_start + i] * env);
    }

  return wave;
}

void generate_ft2_ft4_complex_wave (int const* itone, int nsym, int nsps, float fsample, float f0,
                                    std::complex<float>* cwave, int nwave)
{
  if (!cwave || nwave <= 0)
    {
      return;
    }

  std::fill_n (cwave, nwave, std::complex<float> {0.0f, 0.0f});
  if (!itone || nsym <= 0 || nsps <= 0 || fsample <= 0.0f)
    {
      return;
    }

  QVector<double> pulse (3 * nsps);
  for (int i = 0; i < pulse.size (); ++i)
    {
      double const tt = (double (i + 1) - 1.5 * double (nsps)) / double (nsps);
      pulse[i] = gfsk_pulse (1.0, tt);
    }

  QVector<double> dphi ((nsym + 2) * nsps, 0.0);
  double const dphi_peak = kTwoPi / double (nsps);
  for (int j = 0; j < nsym; ++j)
    {
      int const ib = j * nsps;
      for (int i = 0; i < 3 * nsps; ++i)
        {
          dphi[ib + i] += dphi_peak * pulse[i] * double (itone[j]);
        }
    }

  double const carrier_step = kTwoPi * double (f0) / double (fsample);
  double phi = 0.0;
  int const samples = std::min (nwave, (nsym + 2) * nsps);
  for (int i = 0; i < samples; ++i)
    {
      cwave[i] = std::complex<float> {float (std::cos (phi)), float (std::sin (phi))};
      phi = wrap_phase (phi + dphi[i] + carrier_step);
    }

  for (int i = 0; i < std::min (nsps, samples); ++i)
    {
      double const env = 0.5 * (1.0 - std::cos (kTwoPi * double (i) / (2.0 * double (nsps))));
      cwave[i] *= float (env);
    }

  int const tail_start = (nsym + 1) * nsps;
  if (tail_start < samples)
    {
      for (int i = 0; i < nsps && tail_start + i < samples; ++i)
        {
          double const env = 0.5 * (1.0 + std::cos (kTwoPi * double (i) / (2.0 * double (nsps))));
          cwave[tail_start + i] *= float (env);
        }
    }
}

QVector<float> generate_ft8_wave (int const* itone, int nsym, int nsps, float bt, float fsample, float f0)
{
  if (!itone || nsym <= 0 || nsps <= 0 || fsample <= 0.0f)
    {
      return {};
    }

  QVector<double> pulse (3 * nsps);
  for (int i = 0; i < pulse.size (); ++i)
    {
      double const tt = (double (i + 1) - 1.5 * double (nsps)) / double (nsps);
      pulse[i] = gfsk_pulse (double (bt), tt);
    }

  QVector<double> dphi ((nsym + 2) * nsps, 0.0);
  double const dphi_peak = kTwoPi / double (nsps);
  for (int j = 0; j < nsym; ++j)
    {
      int const ib = j * nsps;
      for (int i = 0; i < 3 * nsps; ++i)
        {
          dphi[ib + i] += dphi_peak * pulse[i] * double (itone[j]);
        }
    }

  for (int i = 0; i < 2 * nsps; ++i)
    {
      dphi[i] += dphi_peak * double (itone[0]) * pulse[nsps + i];
      dphi[nsym * nsps + i] += dphi_peak * double (itone[nsym - 1]) * pulse[i];
    }

  double const carrier_step = kTwoPi * double (f0) / double (fsample);
  QVector<float> wave (nsym * nsps, 0.0f);
  double phi = 0.0;
  for (int i = 0; i < wave.size (); ++i)
    {
      wave[i] = std::sin (phi);
      phi = wrap_phase (phi + dphi[nsps + i] + carrier_step);
    }

  int const nramp = std::max (1, int (std::lround (double (nsps) / 8.0)));
  for (int i = 0; i < nramp; ++i)
    {
      double const env = 0.5 * (1.0 - std::cos (kTwoPi * double (i) / (2.0 * double (nramp))));
      wave[i] = float (wave[i] * env);
    }

  int const tail_start = nsym * nsps - nramp;
  for (int i = 0; i < nramp; ++i)
    {
      double const env = 0.5 * (1.0 + std::cos (kTwoPi * double (i) / (2.0 * double (nramp))));
      wave[tail_start + i] = float (wave[tail_start + i] * env);
    }

  return wave;
}

void generate_q65_complex_wave (int const* itone, int nsym, double fsample, double f0,
                                double dfgen, std::complex<float>* cwave, int nwave)
{
  if (!itone || !cwave || nsym <= 0 || fsample <= 0.0 || nwave <= 0)
    {
      return;
    }

  double const dt = 1.0 / fsample;
  double const tsym = 7200.0 / 12000.0;
  double phi = 0.0;
  double dphi = kTwoPi * dt * f0;
  double t = 0.0;
  int current_symbol = 0;
  for (int i = 0; i < nwave; ++i)
    {
      t += dt;
      int symbol = static_cast<int> (t / tsym + 1.0);
      symbol = clamp_int (symbol, 1, nsym);
      if (symbol != current_symbol)
        {
          dphi = kTwoPi * dt * (f0 + double (itone[symbol - 1]) * dfgen);
          current_symbol = symbol;
        }
      phi = wrap_phase (phi + dphi);
      cwave[i] = std::complex<float> {float (std::cos (phi)), float (-std::sin (phi))};
    }
}

extern "C" {
struct FoxCom2
{
  int itone2[kFt8FoxNsym];
  signed char msgbits2[77];
};

FoxCom2 foxcom2_;

void genq65_ (char* msg0, int* ichk, char* msgsent, int* itone, int* i3, int* n3,
              fortran_charlen_t len1, fortran_charlen_t len2);
}

void normalize_wave (QVector<float>& wave)
{
  float peak = 0.0f;
  for (float sample : wave)
    {
      peak = std::max (peak, std::abs (sample));
    }
  if (peak <= 0.0f)
    {
      return;
    }
  for (float& sample : wave)
    {
      sample /= peak;
    }
}

void fox_bandlimit_inplace (QVector<float>& wave, int nslots, int nfreq, float width)
{
  if (wave.isEmpty () || nslots <= 0 || width <= 0.0f)
    {
      return;
    }

  std::vector<float> time_domain (static_cast<size_t> (kFt8FoxNfft), 0.0f);
  int const copy_samples = std::min (wave.size (), kFt8FoxNwave);
  for (int i = 0; i < copy_samples; ++i)
    {
      time_domain[static_cast<size_t> (i)] = wave.at (i);
    }

  int const nh = kFt8FoxNfft / 2;
  std::vector<std::array<float, 2>> spectrum (static_cast<size_t> (nh + 1));
  auto* fft_spectrum = reinterpret_cast<fftwf_complex*> (spectrum.data ());
  fftwf_plan forward = fftwf_plan_dft_r2c_1d (kFt8FoxNfft, time_domain.data (), fft_spectrum, FFTW_ESTIMATE);
  fftwf_plan inverse = fftwf_plan_dft_c2r_1d (kFt8FoxNfft, fft_spectrum, time_domain.data (), FFTW_ESTIMATE);
  if (!forward || !inverse)
    {
      if (forward) fftwf_destroy_plan (forward);
      if (inverse) fftwf_destroy_plan (inverse);
      return;
    }

  fftwf_execute (forward);

  float const df = 48000.0f / static_cast<float> (kFt8FoxNfft);
  float const fa = static_cast<float> (nfreq) - 0.5f * 6.25f;
  float const fb = static_cast<float> (nfreq) + 7.5f * 6.25f + static_cast<float> (nslots - 1) * 60.0f;
  int const ia2 = clamp_int (static_cast<int> (std::lround (fa / df)), 0, nh);
  int const ib1 = clamp_int (static_cast<int> (std::lround (fb / df)), 0, nh);
  int const ia1 = clamp_int (static_cast<int> (std::lround ((fa - width) / df)), 0, ia2);
  int const ib2 = clamp_int (static_cast<int> (std::lround ((fb + width) / df)), ib1, nh);

  float const pi = static_cast<float> (std::acos (-1.0));
  for (int i = ia1; i <= ia2; ++i)
    {
      float const fil = (1.0f + std::cos (pi * df * static_cast<float> (i - ia2) / width)) * 0.5f;
      spectrum[static_cast<size_t> (i)][0] *= fil;
      spectrum[static_cast<size_t> (i)][1] *= fil;
    }
  for (int i = ib1; i <= ib2; ++i)
    {
      float const fil = (1.0f + std::cos (pi * df * static_cast<float> (i - ib1) / width)) * 0.5f;
      spectrum[static_cast<size_t> (i)][0] *= fil;
      spectrum[static_cast<size_t> (i)][1] *= fil;
    }
  for (int i = 0; i < ia1; ++i)
    {
      spectrum[static_cast<size_t> (i)][0] = 0.0f;
      spectrum[static_cast<size_t> (i)][1] = 0.0f;
    }
  for (int i = ib2 + 1; i <= nh; ++i)
    {
      spectrum[static_cast<size_t> (i)][0] = 0.0f;
      spectrum[static_cast<size_t> (i)][1] = 0.0f;
    }

  fftwf_execute (inverse);
  fftwf_destroy_plan (forward);
  fftwf_destroy_plan (inverse);

  float const scale = 1.0f / static_cast<float> (kFt8FoxNfft);
  for (int i = 0; i < copy_samples; ++i)
    {
      wave[i] = time_domain[static_cast<size_t> (i)] * scale;
    }
}

void ft2_fox_bandlimit_inplace (QVector<float>& wave, int nslots, int nfreq, float width)
{
  if (wave.isEmpty () || nslots <= 0 || width <= 0.0f)
    {
      return;
    }

  std::vector<float> time_domain (static_cast<size_t> (kFt2FoxNfft), 0.0f);
  int const copy_samples = std::min (wave.size (), kFt2FoxNwave);
  for (int i = 0; i < copy_samples; ++i)
    {
      time_domain[static_cast<size_t> (i)] = wave.at (i);
    }

  int const nh = kFt2FoxNfft / 2;
  std::vector<std::array<float, 2>> spectrum (static_cast<size_t> (nh + 1));
  auto* fft_spectrum = reinterpret_cast<fftwf_complex*> (spectrum.data ());
  fftwf_plan forward = fftwf_plan_dft_r2c_1d (kFt2FoxNfft, time_domain.data (), fft_spectrum, FFTW_ESTIMATE);
  fftwf_plan inverse = fftwf_plan_dft_c2r_1d (kFt2FoxNfft, fft_spectrum, time_domain.data (), FFTW_ESTIMATE);
  if (!forward || !inverse)
    {
      if (forward) fftwf_destroy_plan (forward);
      if (inverse) fftwf_destroy_plan (inverse);
      return;
    }

  fftwf_execute (forward);

  float const df = 48000.0f / static_cast<float> (kFt2FoxNfft);
  float const baud = 48000.0f / static_cast<float> (kFt2FoxNsps);
  float const fa = static_cast<float> (nfreq) - 0.5f * baud;
  float const fb = static_cast<float> (nfreq) + 3.5f * baud + static_cast<float> (nslots - 1) * 500.0f;
  int const ia2 = clamp_int (static_cast<int> (std::lround (fa / df)), 0, nh);
  int const ib1 = clamp_int (static_cast<int> (std::lround (fb / df)), 0, nh);
  int const ia1 = clamp_int (static_cast<int> (std::lround ((fa - width) / df)), 0, ia2);
  int const ib2 = clamp_int (static_cast<int> (std::lround ((fb + width) / df)), ib1, nh);

  float const pi = static_cast<float> (std::acos (-1.0));
  for (int i = ia1; i <= ia2; ++i)
    {
      float const fil = (1.0f + std::cos (pi * df * static_cast<float> (i - ia2) / width)) * 0.5f;
      spectrum[static_cast<size_t> (i)][0] *= fil;
      spectrum[static_cast<size_t> (i)][1] *= fil;
    }
  for (int i = ib1; i <= ib2; ++i)
    {
      float const fil = (1.0f + std::cos (pi * df * static_cast<float> (i - ib1) / width)) * 0.5f;
      spectrum[static_cast<size_t> (i)][0] *= fil;
      spectrum[static_cast<size_t> (i)][1] *= fil;
    }
  for (int i = 0; i < ia1; ++i)
    {
      spectrum[static_cast<size_t> (i)][0] = 0.0f;
      spectrum[static_cast<size_t> (i)][1] = 0.0f;
    }
  for (int i = ib2 + 1; i <= nh; ++i)
    {
      spectrum[static_cast<size_t> (i)][0] = 0.0f;
      spectrum[static_cast<size_t> (i)][1] = 0.0f;
    }

  fftwf_execute (inverse);
  fftwf_destroy_plan (forward);
  fftwf_destroy_plan (inverse);

  float const scale = 1.0f / static_cast<float> (kFt2FoxNfft);
  for (int i = 0; i < copy_samples; ++i)
    {
      wave[i] = time_domain[static_cast<size_t> (i)] * scale;
    }
}

bool fst4_shaping_enabled ()
{
  static bool const enabled = [] {
    char const* value = std::getenv ("FST4_NOSHAPING");
    return !(value && value[0] == '1');
  }();
  return enabled;
}

std::array<std::complex<float>, 65536> const& fst4_ctab ()
{
  static std::array<std::complex<float>, 65536> table = [] {
    std::array<std::complex<float>, 65536> result {};
    for (size_t i = 0; i < result.size (); ++i)
      {
        double const phi = static_cast<double> (i) * kTwoPi / static_cast<double> (result.size ());
        result[i] = std::complex<float> {static_cast<float> (std::cos (phi)),
                                         static_cast<float> (std::sin (phi))};
      }
    return result;
  }();
  return table;
}

void generate_fst4_wave_internal (int const* itone, int nsym, int nsps, float fsample, int hmod, float f0,
                                  std::complex<float>* cwave, float* wave, int icmplx, int nwave)
{
  if (!itone || nsym <= 0 || nsps <= 0 || fsample <= 0.0f || nwave <= 0)
    {
      return;
    }

  std::vector<float> pulse (static_cast<size_t> (3 * nsps), 0.0f);
  for (int i = 0; i < 3 * nsps; ++i)
    {
      double const tt = (static_cast<double> (i) + 1.0 - 1.5 * static_cast<double> (nsps))
                      / static_cast<double> (nsps);
      pulse[static_cast<size_t> (i)] = static_cast<float> (gfsk_pulse (2.0, tt));
    }

  std::vector<double> dphi (static_cast<size_t> ((nsym + 2) * nsps), 0.0);
  double const dphi_peak = kTwoPi * static_cast<double> (hmod) / static_cast<double> (nsps);
  for (int j = 0; j < nsym; ++j)
    {
      int const start = j * nsps;
      for (int i = 0; i < 3 * nsps; ++i)
        {
          dphi[static_cast<size_t> (start + i)] += dphi_peak
              * static_cast<double> (pulse[static_cast<size_t> (i)])
              * static_cast<double> (itone[j]);
        }
    }

  double const tsym = static_cast<double> (nsps) / static_cast<double> (fsample);
  double const dt = 1.0 / static_cast<double> (fsample);
  double const carrier_step =
      kTwoPi * (static_cast<double> (f0) - 1.5 * static_cast<double> (hmod) / tsym) * dt;

  auto const& table = fst4_ctab ();
  double phi = 0.0;
  int k = 0;
  int const limit = std::min (nwave, nsym * nsps);
  for (int j = nsps; j < (nsym + 1) * nsps && k < limit; ++j)
    {
      int index = static_cast<int> (phi * static_cast<double> (table.size ()) / kTwoPi);
      index &= static_cast<int> (table.size () - 1);
      if (icmplx == 0)
        {
          if (wave) wave[k] = table[static_cast<size_t> (index)].imag ();
        }
      else
        {
          if (cwave) cwave[k] = table[static_cast<size_t> (index)];
        }
      ++k;
      phi = wrap_phase (phi + dphi[static_cast<size_t> (j)] + carrier_step);
    }

  if (!fst4_shaping_enabled ())
    {
      return;
    }

  int const rise = std::min (limit, nsps / 4);
  for (int i = 0; i < rise; ++i)
    {
      double const env = (1.0 - std::cos (kTwoPi * static_cast<double> (i)
                                          / static_cast<double> (nsps / 2))) / 2.0;
      if (icmplx == 0)
        {
          if (wave) wave[i] = static_cast<float> (wave[i] * env);
        }
      else
        {
          if (cwave) cwave[i] *= static_cast<float> (env);
        }
    }

  int const tail_start = (nsym - 1) * nsps + 3 * nsps / 4;
  int const tail_count = std::min (nsps / 4 + 1, std::max (0, limit - tail_start));
  for (int i = 0; i < tail_count; ++i)
    {
      double const env = (1.0 + std::cos (kTwoPi * static_cast<double> (i)
                                          / static_cast<double> (nsps / 2))) / 2.0;
      int const index = tail_start + i;
      if (icmplx == 0)
        {
          if (wave) wave[index] = static_cast<float> (wave[index] * env);
        }
      else
        {
          if (cwave) cwave[index] *= static_cast<float> (env);
        }
    }
}
}

namespace decodium
{
namespace txwave
{

QVector<float> generateFt2Wave (int const* itone, int nsym, int nsps, float fsample, float f0)
{
  return generate_ft2_ft4_wave (itone, nsym, nsps, fsample, f0);
}

QVector<float> generateFt4Wave (int const* itone, int nsym, int nsps, float fsample, float f0)
{
  return generate_ft2_ft4_wave (itone, nsym, nsps, fsample, f0);
}

QVector<float> generateFt8Wave (int const* itone, int nsym, int nsps, float bt, float fsample, float f0)
{
  return generate_ft8_wave (itone, nsym, nsps, bt, fsample, f0);
}

QVector<float> generateFst4Wave (int const* itone, int nsym, int nsps, float fsample, int hmod, float f0)
{
  int const output_samples = std::max (0, (nsym + 2) * nsps);
  QVector<float> wave (output_samples, 0.0f);
  if (output_samples == 0)
    {
      return wave;
    }
  generate_fst4_wave_internal (itone, nsym, nsps, fsample, hmod, f0, nullptr, wave.data (), 0, output_samples);
  return wave;
}

bool packSuperFoxMessage (QString const& line, QString const& otpKey, bool bMoreCQs, bool bSendMsg,
                          QString const& freeTextMsg, std::array<unsigned char, 50>* xinOut)
{
  return pack_superfox_message (line, otpKey, bMoreCQs, bSendMsg, freeTextMsg, xinOut);
}

bool generateSuperFoxTx (QString const& otpKey)
{
  std::fill_n (foxcom_.wave, static_cast<int> (sizeof (foxcom_.wave) / sizeof (foxcom_.wave[0])), 0.0f);
  std::fill_n (foxcom3_.itone3, kSuperFoxNsym, 0);
  foxcom3_.nslots2 = clamp_int (foxcom_.nslots, 0, 5);
  std::memset (foxcom3_.cmsg2, ' ', sizeof (foxcom3_.cmsg2));
  for (int slot = 0; slot < foxcom3_.nslots2; ++slot)
    {
      std::memcpy (foxcom3_.cmsg2[slot], foxcom_.cmsg[slot], 40);
    }

  SuperFoxAssembled const assembled = assemble_superfox_message ();
  if (assembled.line.trimmed ().isEmpty ())
    {
      return false;
    }

  std::array<unsigned char, 50> xin {};
  QString const free_text = QString::fromLatin1 (foxcom_.textMsg, 26);
  if (!packSuperFoxMessage (assembled.line, otpKey, foxcom_.bMoreCQs, foxcom_.bSendMsg, free_text, &xin))
    {
      return false;
    }

  build_superfox_channel_symbols (xin, foxcom3_.itone3);
  generate_superfox_waveform (foxcom3_.itone3, foxcom_.wave,
                              static_cast<int> (sizeof (foxcom_.wave) / sizeof (foxcom_.wave[0])));
  return true;
}

}
}

extern "C" int ftx_gen_ft8wave_complex_c (int const* itone, int nsym, int nsps, float bt,
                                           float fsample, float f0, float* real_out,
                                           float* imag_out, int nwave)
{
  constexpr int kNTab = 65536;
  if (!itone || !real_out || !imag_out || nsym <= 0 || nsps <= 0 || nwave <= 0 || fsample <= 0.0f)
    {
      return 0;
    }

  std::vector<double> pulse (static_cast<size_t> (3 * nsps), 0.0);
  for (int i = 0; i < 3 * nsps; ++i)
    {
      double const tt = (double (i + 1) - 1.5 * double (nsps)) / double (nsps);
      pulse[static_cast<size_t> (i)] = gfsk_pulse (double (bt), tt);
    }

  std::vector<double> dphi (static_cast<size_t> ((nsym + 2) * nsps), 0.0);
  double const dphi_peak = kTwoPi / double (nsps);
  for (int j = 0; j < nsym; ++j)
    {
      int const ib = j * nsps;
      for (int i = 0; i < 3 * nsps; ++i)
        {
          dphi[static_cast<size_t> (ib + i)] += dphi_peak * pulse[static_cast<size_t> (i)]
            * double (itone[j]);
        }
    }

  for (int i = 0; i < 2 * nsps; ++i)
    {
      dphi[static_cast<size_t> (i)] += dphi_peak * double (itone[0])
        * pulse[static_cast<size_t> (nsps + i)];
      dphi[static_cast<size_t> (nsym * nsps + i)] += dphi_peak * double (itone[nsym - 1])
        * pulse[static_cast<size_t> (i)];
    }

  double const carrier_step = kTwoPi * double (f0) / double (fsample);
  double phi = 0.0;
  int const samples = std::min (nwave, nsym * nsps);
  for (int i = 0; i < samples; ++i)
    {
      int index = static_cast<int> (phi * double (kNTab) / kTwoPi);
      if (index < 0)
        {
          index = 0;
        }
      else if (index >= kNTab)
        {
          index = kNTab - 1;
        }
      double const phi_quantized = double (index) * kTwoPi / double (kNTab);
      real_out[i] = float (std::cos (phi_quantized));
      imag_out[i] = float (std::sin (phi_quantized));
      phi = wrap_phase (phi + dphi[static_cast<size_t> (nsps + i)] + carrier_step);
    }
  for (int i = samples; i < nwave; ++i)
    {
      real_out[i] = 0.0f;
      imag_out[i] = 0.0f;
    }

  int const nramp = std::max (1, int (std::lround (double (nsps) / 8.0)));
  for (int i = 0; i < std::min (nramp, samples); ++i)
    {
      double const env = 0.5 * (1.0 - std::cos (kTwoPi * double (i) / (2.0 * double (nramp))));
      real_out[i] = float (real_out[i] * env);
      imag_out[i] = float (imag_out[i] * env);
    }

  int const tail_start = std::max (0, samples - nramp);
  for (int i = 0; i < nramp && tail_start + i < samples; ++i)
    {
      double const env = 0.5 * (1.0 + std::cos (kTwoPi * double (i) / (2.0 * double (nramp))));
      real_out[tail_start + i] = float (real_out[tail_start + i] * env);
      imag_out[tail_start + i] = float (imag_out[tail_start + i] * env);
    }

  return 1;
}

extern "C" void foxgen_ (bool* bSuperFox, char const* /*fname*/, fortran_charlen_t /*len*/)
{
  if (bSuperFox && *bSuperFox)
    {
      foxcom3_.nslots2 = foxcom_.nslots;
      std::memset (foxcom3_.cmsg2, ' ', sizeof (foxcom3_.cmsg2));
      for (int slot = 0; slot < std::min (foxcom_.nslots, 5); ++slot)
        {
          std::memcpy (foxcom3_.cmsg2[slot], foxcom_.cmsg[slot], 40);
        }
      std::fill_n (foxcom3_.itone3, 151, 0);
      return;
    }

  QVector<float> wave (kFt8FoxNwave, 0.0f);
  std::fill_n (foxcom2_.itone2, kFt8FoxNsym, 0);
  std::fill_n (foxcom2_.msgbits2, 77, static_cast<signed char> (0));

  int const slots = clamp_int (foxcom_.nslots, 0, 5);
  for (int slot = 0; slot < slots; ++slot)
    {
      QString const message = QString::fromLatin1 (foxcom_.cmsg[slot], 40).left (37);
      auto const encoded = decodium::txmsg::encodeFt8 (message);
      if (!encoded.ok || encoded.tones.size () < kFt8FoxNsym)
        {
          continue;
        }

      float const f0 = static_cast<float> (foxcom_.nfreq + slot * 60);
      QVector<float> const slot_wave = decodium::txwave::generateFt8Wave (encoded.tones.constData (),
                                                                          kFt8FoxNsym,
                                                                          kFt8FoxNsps,
                                                                          2.0f,
                                                                          48000.0f,
                                                                          f0);
      int const samples = std::min (wave.size (), slot_wave.size ());
      for (int i = 0; i < samples; ++i)
        {
          wave[i] += slot_wave.at (i);
        }

      if (slot == slots - 1)
        {
          for (int i = 0; i < kFt8FoxNsym; ++i)
            {
              foxcom2_.itone2[i] = encoded.tones.at (i);
            }
          for (int i = 0; i < 77 && i < encoded.msgbits.size (); ++i)
            {
              foxcom2_.msgbits2[i] = static_cast<signed char> (encoded.msgbits.at (i) != 0);
            }
        }
    }

  normalize_wave (wave);
  fox_bandlimit_inplace (wave, slots, foxcom_.nfreq, 50.0f);
  normalize_wave (wave);

  int const max_samples = static_cast<int> (sizeof (foxcom_.wave) / sizeof (foxcom_.wave[0]));
  int const copy_samples = std::min (max_samples, wave.size ());
  std::copy_n (wave.constBegin (), copy_samples, foxcom_.wave);
  if (copy_samples < max_samples)
    {
      std::fill (foxcom_.wave + copy_samples, foxcom_.wave + max_samples, 0.0f);
    }
}

extern "C" void sftx_sub_ (char const* otp_key, fortran_charlen_t len)
{
  QString const otp = QString::fromLatin1 (otp_key ? otp_key : "", static_cast<int> (len)).trimmed ();
  decodium::txwave::generateSuperFoxTx (otp);
}

extern "C" void sfox_wave_gfsk_ ()
{
  generate_superfox_waveform (foxcom3_.itone3, foxcom_.wave,
                              static_cast<int> (sizeof (foxcom_.wave) / sizeof (foxcom_.wave[0])));
}

extern "C" void sfox_gen_gfsk_ (int* idat, float* f0, int* isync, int* itone, std::complex<float>* cdat)
{
  generate_superfox_complex_waveform (idat, f0 ? *f0 : 750.0f, isync, itone, cdat, 15 * 12000);
}

extern "C" void foxgenft2_ ()
{
  QVector<float> wave (kFt2FoxNwave, 0.0f);

  int const slots = clamp_int (foxcom_.nslots, 0, 5);
  for (int slot = 0; slot < slots; ++slot)
    {
      QString const message = QString::fromLatin1 (foxcom_.cmsg[slot], 40).left (37);
      auto const encoded = decodium::txmsg::encodeFt2 (message);
      if (!encoded.ok || encoded.tones.size () < kFt2FoxNsym)
        {
          continue;
        }

      float const f0 = static_cast<float> (foxcom_.nfreq + slot * 500);
      QVector<float> const slot_wave = decodium::txwave::generateFt2Wave (encoded.tones.constData (),
                                                                          kFt2FoxNsym,
                                                                          kFt2FoxNsps,
                                                                          48000.0f,
                                                                          f0);
      int const samples = std::min (wave.size (), slot_wave.size ());
      for (int i = 0; i < samples; ++i)
        {
          wave[i] += slot_wave.at (i);
        }
    }

  normalize_wave (wave);
  ft2_fox_bandlimit_inplace (wave, slots, foxcom_.nfreq, 50.0f);
  normalize_wave (wave);

  int const max_samples = static_cast<int> (sizeof (foxcom_.wave) / sizeof (foxcom_.wave[0]));
  int const copy_samples = std::min (max_samples, wave.size ());
  std::copy_n (wave.constBegin (), copy_samples, foxcom_.wave);
  if (copy_samples < max_samples)
    {
      std::fill (foxcom_.wave + copy_samples, foxcom_.wave + max_samples, 0.0f);
    }
}

extern "C" void gen_ft8wave_ (int* itone, int* nsym, int* nsps, float* bt, float* fsample,
                               float* f0, std::complex<float>* cwave, float* wave,
                               int* icmplx, int* nwave)
{
  int const symbol_count = nsym ? *nsym : 0;
  int const samples_per_symbol = nsps ? *nsps : 0;
  int const output_samples = nwave ? *nwave : 0;
  if (output_samples <= 0)
    {
      return;
    }

  if (icmplx && *icmplx != 0)
    {
      if (cwave)
        {
          std::fill_n (cwave, output_samples, std::complex<float> {0.0f, 0.0f});
        }
      if (!itone || !nsym || !nsps || !bt || !fsample || !f0 || !cwave)
        {
          return;
        }

      std::vector<float> real_out (static_cast<size_t> (output_samples), 0.0f);
      std::vector<float> imag_out (static_cast<size_t> (output_samples), 0.0f);
      if (!ftx_gen_ft8wave_complex_c (itone, symbol_count, samples_per_symbol, *bt, *fsample, *f0,
                                      real_out.data (), imag_out.data (), output_samples))
        {
          return;
        }
      for (int i = 0; i < output_samples; ++i)
        {
          cwave[i] = std::complex<float> {real_out[static_cast<size_t> (i)],
                                          imag_out[static_cast<size_t> (i)]};
        }
      return;
    }

  if (wave)
    {
      std::fill_n (wave, output_samples, 0.0f);
    }
  if (!itone || !nsym || !nsps || !bt || !fsample || !f0 || !wave)
    {
      return;
    }

  QVector<float> const generated = decodium::txwave::generateFt8Wave (itone, symbol_count,
                                                                      samples_per_symbol, *bt,
                                                                      *fsample, *f0);
  int const samples = std::min (output_samples, generated.size ());
  for (int i = 0; i < samples; ++i)
    {
      wave[i] = generated.at (i);
    }
}

extern "C" void gen_ft4wave_ (int* itone, int* nsym, int* nsps, float* fsample, float* f0,
                               std::complex<float>* cwave, float* wave,
                               int* icmplx, int* nwave)
{
  int const symbol_count = nsym ? *nsym : 0;
  int const samples_per_symbol = nsps ? *nsps : 0;
  int const output_samples = nwave ? *nwave : 0;
  if (output_samples <= 0)
    {
      return;
    }

  if (icmplx && *icmplx != 0)
    {
      if (cwave)
        {
          std::fill_n (cwave, output_samples, std::complex<float> {0.0f, 0.0f});
        }
      if (!itone || !nsym || !nsps || !fsample || !f0 || !cwave)
        {
          return;
        }

      generate_ft2_ft4_complex_wave (itone, symbol_count, samples_per_symbol, *fsample, *f0,
                                     cwave, output_samples);
      return;
    }

  if (wave)
    {
      std::fill_n (wave, output_samples, 0.0f);
    }
  if (!itone || !nsym || !nsps || !fsample || !f0 || !wave)
    {
      return;
    }

  QVector<float> const generated = decodium::txwave::generateFt4Wave (itone, symbol_count,
                                                                      samples_per_symbol,
                                                                      *fsample, *f0);
  int const samples = std::min (output_samples, generated.size ());
  for (int i = 0; i < samples; ++i)
    {
      wave[i] = generated.at (i);
    }
}

extern "C" void gen_fst4wave_ (int* itone, int* nsym, int* nsps, int* nwave, float* fsample,
                                int* hmod, float* f0, int* icmplx,
                                std::complex<float>* cwave, float* wave)
{
  int const symbol_count = nsym ? *nsym : 0;
  int const samples_per_symbol = nsps ? *nsps : 0;
  int const output_samples = nwave ? *nwave : 0;
  int const modulation_index = hmod ? *hmod : 1;
  int const complex_output = icmplx ? *icmplx : 0;
  if (output_samples <= 0)
    {
      return;
    }

  if (wave)
    {
      std::fill_n (wave, output_samples, 0.0f);
    }
  if (cwave)
    {
      std::fill_n (cwave, output_samples, std::complex<float> {0.0f, 0.0f});
    }
  if (!itone || !nsym || !nsps || !fsample || !f0)
    {
      return;
    }

  generate_fst4_wave_internal (itone, symbol_count, samples_per_symbol, *fsample,
                               modulation_index, *f0, cwave, wave, complex_output,
                               output_samples);
}

extern "C" void gen_ft2wave_ (int* itone, int* nsym, int* nsps, float* fsample, float* f0,
                               std::complex<float>* cwave, float* wave,
                               int* icmplx, int* nwave)
{
  int const symbol_count = nsym ? *nsym : 0;
  int const samples_per_symbol = nsps ? *nsps : 0;
  int const output_samples = nwave ? *nwave : 0;
  if (output_samples <= 0)
    {
      return;
    }

  if (icmplx && *icmplx != 0)
    {
      if (cwave)
        {
          std::fill_n (cwave, output_samples, std::complex<float> {0.0f, 0.0f});
        }
      if (!itone || !nsym || !nsps || !fsample || !f0 || !cwave)
        {
          return;
        }

      generate_ft2_ft4_complex_wave (itone, symbol_count, samples_per_symbol, *fsample, *f0,
                                     cwave, output_samples);
      return;
    }

  if (wave)
    {
      std::fill_n (wave, output_samples, 0.0f);
    }
  if (!itone || !nsym || !nsps || !fsample || !f0 || !wave)
    {
      return;
    }

  QVector<float> const generated = decodium::txwave::generateFt2Wave (itone, symbol_count,
                                                                      samples_per_symbol,
                                                                      *fsample, *f0);
  int const samples = std::min (output_samples, generated.size ());
  for (int i = 0; i < samples; ++i)
    {
      wave[i] = generated.at (i);
    }
}

extern "C" void gen_q65_wave_ (char* msg, int* ntxFreq, int* mode64,
                               char* msgsent, short iwave[], int* nwave,
                               int len1, int len2)
{
  if (nwave) *nwave = 0;
  if (!msg || !ntxFreq || !mode64 || !msgsent || !iwave || !nwave)
    {
      return;
    }

  std::array<char, 37> msg37 {};
  std::array<char, 37> msgsent37 {};
  std::array<int, 85> itone {};
  std::fill (msg37.begin (), msg37.end (), ' ');
  std::fill (msgsent37.begin (), msgsent37.end (), ' ');
  std::copy_n (msg, std::min (len1, 37), msg37.data ());

  int ichk = 0;
  int i3 = -1;
  int n3 = -1;
  genq65_ (msg37.data (), &ichk, msgsent37.data (), itone.data (), &i3, &n3,
           static_cast<fortran_charlen_t> (37), static_cast<fortran_charlen_t> (37));

  std::fill_n (msgsent, len2, ' ');
  std::copy_n (msgsent37.data (), std::min (len2, 37), msgsent);

  double const fsample = 11025.0;
  double const dt = 1.0 / fsample;
  double const tsym = 7200.0 / 12000.0;
  double const f0 = double (*ntxFreq);
  int const ndf = 1 << std::max (0, *mode64 - 1);
  double const dfgen = double (ndf) * 12000.0 / 7200.0;
  int const samples = static_cast<int> (85.0 * 7200.0 * fsample / 12000.0);
  *nwave = 2 * samples;

  double phi = 0.0;
  double dphi = kTwoPi * dt * f0;
  double t = 0.0;
  int current_symbol = 0;
  for (int i = 0; i < samples; ++i)
    {
      t += dt;
      int symbol = clamp_int (static_cast<int> (t / tsym + 1.0), 1, 85);
      if (symbol != current_symbol)
        {
          dphi = kTwoPi * dt * (f0 + double (itone[static_cast<size_t> (symbol - 1)]) * dfgen);
          current_symbol = symbol;
        }
      phi = wrap_phase (phi + dphi);
      iwave[2 * i] = static_cast<short> (std::lround (32767.0 * std::cos (phi)));
      iwave[2 * i + 1] = static_cast<short> (std::lround (32767.0 * std::sin (phi)));
    }
}

extern "C" void gen_q65_cwave_ (char* msg, int* ntxFreq, int* ntone_spacing,
                                char* msgsent, std::complex<float>* cwave, int* nwave,
                                fortran_charlen_t len1, fortran_charlen_t len2)
{
  if (nwave) *nwave = 0;
  if (!msg || !ntxFreq || !ntone_spacing || !msgsent || !cwave || !nwave)
    {
      return;
    }

  std::array<char, 37> msg37 {};
  std::array<char, 37> msgsent37 {};
  std::array<int, 85> itone {};
  std::fill (msg37.begin (), msg37.end (), ' ');
  std::fill (msgsent37.begin (), msgsent37.end (), ' ');
  std::copy_n (msg, std::min (static_cast<int> (len1), 37), msg37.data ());

  int ichk = 0;
  int i3 = -1;
  int n3 = -1;
  genq65_ (msg37.data (), &ichk, msgsent37.data (), itone.data (), &i3, &n3,
           static_cast<fortran_charlen_t> (37), static_cast<fortran_charlen_t> (37));

  std::fill_n (msgsent, static_cast<size_t> (len2), ' ');
  std::copy_n (msgsent37.data (), std::min (static_cast<int> (len2), 37), msgsent);

  int const samples = static_cast<int> (85.0 * 7200.0 * 96000.0 / 12000.0);
  *nwave = samples;
  std::fill_n (cwave, static_cast<size_t> (samples), std::complex<float> {0.0f, 0.0f});
  generate_q65_complex_wave (itone.data (), 85, 96000.0, double (*ntxFreq),
                             double (*ntone_spacing) * 12000.0 / 7200.0, cwave, samples);
}
