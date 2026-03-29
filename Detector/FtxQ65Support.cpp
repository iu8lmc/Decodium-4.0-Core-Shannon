#include "commons.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

#include <QByteArray>
#include <QString>

extern "C"
{
void legacy_pack77_reset_context_c ();
void legacy_pack77_pack_c (char const msg0[37], int* i3, int* n3,
                           char c77[77], char msgsent[37], bool* success, int received);
void q65_enc_ (int x[], int y[]);
}

namespace
{
constexpr int kQ65MaxNcw {206};
constexpr int kQ65CodewordSymbols {63};
constexpr int kQ65PayloadSymbols {13};
constexpr int kQ65Bits {77};

using Q65Ap29 = std::array<int, 29>;
using Q65Ap19 = std::array<int, 19>;

std::array<std::array<int, 4>, 6> const kQ65ApTypes {{
    {{1, 2, 0, 0}},
    {{2, 3, 0, 0}},
    {{2, 3, 0, 0}},
    {{3, 4, 5, 6}},
    {{3, 4, 5, 6}},
    {{3, 1, 2, 0}},
}};

Q65Ap29 const kMcq {{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0
}};
Q65Ap29 const kMcqRu {{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0
}};
Q65Ap29 const kMcqFd {{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0
}};
Q65Ap29 const kMcqTest {{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0
}};
Q65Ap29 const kMcqWw {{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0
}};
Q65Ap19 const kMrrr {{
    0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1
}};
Q65Ap19 const kM73 {{
    0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1
}};
Q65Ap19 const kMrr73 {{
    0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 1
}};

QByteArray from_fixed (char const* data, int width)
{
  QByteArray value {data, width};
  while (!value.isEmpty () && (value.back () == ' ' || value.back () == '\0'))
    {
      value.chop (1);
    }
  return value;
}

void to_fixed (QByteArray value, char* out, int width)
{
  value = value.left (width);
  if (value.size () < width)
    {
      value.append (QByteArray (width - value.size (), ' '));
    }
  std::memcpy (out, value.constData (), static_cast<size_t> (width));
}

bool is_digit (char c)
{
  return c >= '0' && c <= '9';
}

bool is_letter (char c)
{
  return c >= 'A' && c <= 'Z';
}

bool is_standard_call (QByteArray const& callsign)
{
  QByteArray const trimmed = callsign.trimmed ().toUpper ();
  int const n = trimmed.size ();
  int iarea = -1;
  for (int i = n - 1; i >= 1; --i)
    {
      if (is_digit (trimmed.at (i)))
        {
          iarea = i + 1;
          break;
        }
    }

  if (iarea < 0)
    {
      return false;
    }

  int npdig = 0;
  int nplet = 0;
  for (int i = 0; i < iarea - 1; ++i)
    {
      char const c = trimmed.at (i);
      if (is_digit (c))
        {
          ++npdig;
        }
      if (is_letter (c))
        {
          ++nplet;
        }
    }

  int nslet = 0;
  for (int i = iarea; i < n; ++i)
    {
      if (is_letter (trimmed.at (i)))
        {
          ++nslet;
        }
    }

  return !(iarea < 2 || iarea > 3 || nplet == 0 || npdig >= iarea - 1 || nslet > 3);
}

bool is_grid4 (QByteArray const& grid)
{
  QByteArray const trimmed = grid.trimmed ().toUpper ();
  if (trimmed.size () < 4)
    {
      return false;
    }

  return trimmed.at (0) >= 'A' && trimmed.at (0) <= 'R'
      && trimmed.at (1) >= 'A' && trimmed.at (1) <= 'R'
      && trimmed.at (2) >= '0' && trimmed.at (2) <= '9'
      && trimmed.at (3) >= '0' && trimmed.at (3) <= '9'
      && trimmed.left (4) != "RR73";
}

QByteArray from_column_fixed (char const* data, int width, int index)
{
  QByteArray value;
  value.reserve (width);
  for (int row = 0; row < width; ++row)
    {
      value.append (data[row + width * index]);
    }
  return from_fixed (value.constData (), width);
}

int bits_to_int (char const* bits, int offset, int count)
{
  int value = 0;
  for (int i = 0; i < count; ++i)
    {
      value = (value << 1) | (bits[offset + i] == '1' ? 1 : 0);
    }
  return value;
}

bool encode_q65_codeword (QString const& message, int* codeword)
{
  char msg0[37];
  char c77[kQ65Bits];
  char msgsent[37];
  int i3 = -1;
  int n3 = -1;
  bool success = false;
  std::array<int, kQ65PayloadSymbols> dgen {};

  legacy_pack77_reset_context_c ();
  to_fixed (message.left (37).toLatin1 (), msg0, 37);
  legacy_pack77_pack_c (msg0, &i3, &n3, c77, msgsent, &success, 0);
  if (!success)
    {
      return false;
    }

  if (bits_to_int (c77, 59, 15) == 32373)
    {
      std::memcpy (c77 + 59, "111111010010011", 15);
    }

  for (int i = 0; i < 12; ++i)
    {
      dgen[static_cast<size_t> (i)] = bits_to_int (c77, i * 6, 6);
    }
  dgen[12] = 2 * bits_to_int (c77, 72, 5);

  q65_enc_ (dgen.data (), codeword);
  return true;
}

QString format_q65_report (int snr, bool prefixed_r)
{
  QString const magnitude = QStringLiteral ("%1").arg (std::abs (snr), 2, 10, QLatin1Char {'0'});
  QString report;
  if (prefixed_r)
    {
      report += QLatin1Char {'R'};
    }
  report += (snr >= 0) ? QLatin1Char {'+'} : QLatin1Char {'-'};
  report += magnitude;
  return report;
}

void fill_with (int* target, int count, std::array<int, 29> const& source)
{
  std::copy_n (source.data (), std::min (count, static_cast<int> (source.size ())), target);
}

void fill_with (int* target, int count, std::array<int, 19> const& source)
{
  std::copy_n (source.data (), std::min (count, static_cast<int> (source.size ())), target);
}

void zero_fill (int* values, int count)
{
  if (values && count > 0)
    {
      std::fill_n (values, count, 0);
    }
}
}

extern "C" void ftx_q65_ap_c (int* nqsoprogress, int* ipass, int* ncontest, int* lapcqonly,
                              int const apsym0[58], int const aph10[10],
                              int apmask[78], int apsymbols[78], int* iaptype)
{
  zero_fill (apmask, 78);
  zero_fill (apsymbols, 78);
  if (!nqsoprogress || !ipass || !ncontest || !lapcqonly || !apsym0 || !aph10 || !iaptype)
    {
      if (iaptype)
        {
          *iaptype = 0;
        }
      return;
    }

  int const qso = std::max (0, std::min (*nqsoprogress, 5));
  int const pass = std::max (1, std::min (*ipass, 4));
  int contest = *ncontest;
  int const apcqonly = *lapcqonly;

  int ap_type = kQ65ApTypes[static_cast<size_t> (qso)][static_cast<size_t> (pass - 1)];
  if (apcqonly)
    {
      ap_type = 1;
    }
  *iaptype = ap_type;

  if (contest >= 6)
    {
      return;
    }
  if (ap_type >= 2 && apsym0[0] > 1)
    {
      return;
    }
  if (contest == 7 && ap_type >= 2 && aph10[0] > 1)
    {
      return;
    }
  if (ap_type >= 3 && apsym0[29] > 1)
    {
      return;
    }

  if (ap_type == 1)
    {
      std::fill_n (apmask, 29, 1);
      if (contest == 0) fill_with (apsymbols, 29, kMcq);
      if (contest == 1) fill_with (apsymbols, 29, kMcqTest);
      if (contest == 2) fill_with (apsymbols, 29, kMcqTest);
      if (contest == 3) fill_with (apsymbols, 29, kMcqFd);
      if (contest == 4) fill_with (apsymbols, 29, kMcqRu);
      if (contest == 5) fill_with (apsymbols, 29, kMcqWw);
      if (contest == 7) fill_with (apsymbols, 29, kMcq);
      std::fill_n (apmask + 74, 4, 1);
      apsymbols[74] = 0;
      apsymbols[75] = 0;
      apsymbols[76] = 1;
      apsymbols[77] = 0;
      return;
    }

  if (ap_type == 2)
    {
      if (contest == 0 || contest == 1 || contest == 5)
        {
          std::fill_n (apmask, 29, 1);
          std::copy_n (apsym0, 29, apsymbols);
          std::fill_n (apmask + 74, 4, 1);
          apsymbols[74] = 0;
          apsymbols[75] = 0;
          apsymbols[76] = 1;
          apsymbols[77] = 0;
        }
      else if (contest == 2)
        {
          std::fill_n (apmask, 28, 1);
          std::copy_n (apsym0, 28, apsymbols);
          std::fill_n (apmask + 71, 3, 1);
          apsymbols[71] = 0;
          apsymbols[72] = 1;
          apsymbols[73] = 0;
          std::fill_n (apmask + 74, 4, 1);
          std::fill_n (apsymbols + 74, 4, 0);
        }
      else if (contest == 3)
        {
          std::fill_n (apmask, 28, 1);
          std::copy_n (apsym0, 28, apsymbols);
          std::fill_n (apmask + 74, 4, 1);
          std::fill_n (apsymbols + 74, 4, 0);
        }
      else if (contest == 4)
        {
          std::fill_n (apmask + 1, 28, 1);
          std::copy_n (apsym0, 28, apsymbols + 1);
          std::fill_n (apmask + 74, 4, 1);
          apsymbols[74] = 0;
          apsymbols[75] = 0;
          apsymbols[76] = 1;
          apsymbols[77] = 0;
        }
      else if (contest == 7)
        {
          std::fill_n (apmask + 28, 28, 1);
          std::copy_n (apsym0, 28, apsymbols + 28);
          std::fill_n (apmask + 56, 10, 1);
          std::copy_n (aph10, 10, apsymbols + 56);
          std::fill_n (apmask + 71, 7, 1);
          apsymbols[71] = 0;
          apsymbols[72] = 0;
          apsymbols[73] = 1;
          std::fill_n (apsymbols + 74, 4, 0);
        }
      return;
    }

  if (ap_type == 3)
    {
      if (contest == 0 || contest == 1 || contest == 2 || contest == 5 || contest == 7)
        {
          std::fill_n (apmask, 58, 1);
          std::copy_n (apsym0, 58, apsymbols);
          std::fill_n (apmask + 74, 4, 1);
          apsymbols[74] = 0;
          apsymbols[75] = 0;
          apsymbols[76] = 1;
          apsymbols[77] = 0;
        }
      else if (contest == 3)
        {
          std::fill_n (apmask, 56, 1);
          std::copy_n (apsym0, 28, apsymbols);
          std::copy_n (apsym0 + 29, 28, apsymbols + 28);
          std::fill_n (apmask + 71, 7, 1);
          std::fill_n (apsymbols + 74, 4, 0);
        }
      else if (contest == 4)
        {
          std::fill_n (apmask + 1, 56, 1);
          std::copy_n (apsym0, 28, apsymbols + 1);
          std::copy_n (apsym0 + 29, 28, apsymbols + 29);
          std::fill_n (apmask + 74, 4, 1);
          apsymbols[74] = 0;
          apsymbols[75] = 0;
          apsymbols[76] = 1;
          apsymbols[77] = 0;
        }
      return;
    }

  if (ap_type == 5 && contest == 7)
    {
      return;
    }

  if (ap_type == 4 || ap_type == 5 || ap_type == 6)
    {
      if (contest <= 5 || (contest == 7 && ap_type == 6))
        {
          std::fill_n (apmask, 78, 1);
          apmask[71] = 0;
          apmask[72] = 0;
          apmask[73] = 0;
          std::copy_n (apsym0, 58, apsymbols);
          if (ap_type == 4) fill_with (apsymbols + 58, 19, kMrrr);
          if (ap_type == 5) fill_with (apsymbols + 58, 19, kM73);
          if (ap_type == 6) fill_with (apsymbols + 58, 19, kMrr73);
        }
      else if (contest == 7 && ap_type == 4)
        {
          std::fill_n (apmask, 28, 1);
          std::copy_n (apsym0, 28, apsymbols);
          std::fill_n (apmask + 56, 10, 1);
          std::copy_n (aph10, 10, apsymbols + 56);
          std::fill_n (apmask + 71, 7, 1);
          apsymbols[71] = 0;
          apsymbols[72] = 0;
          apsymbols[73] = 1;
          std::fill_n (apsymbols + 74, 4, 0);
        }
    }
}

extern "C" void ftx_q65_set_list_c (char const mycall[12], char const hiscall[12],
                                    char const hisgrid[6], int codewords[63 * 206], int* ncw)
{
  if (ncw)
    {
      *ncw = 0;
    }
  if (!mycall || !hiscall || !hisgrid || !codewords || !ncw)
    {
      return;
    }

  QByteArray const my_call = from_fixed (mycall, 12);
  QByteArray const his_call = from_fixed (hiscall, 12);
  QByteArray const his_grid = from_fixed (hisgrid, 6);
  if (his_call.isEmpty ())
    {
      return;
    }

  bool const my_std = is_standard_call (my_call);
  bool const his_std = is_standard_call (his_call);
  std::fill_n (codewords, kQ65CodewordSymbols * kQ65MaxNcw, 0);

  for (int i = 1; i <= kQ65MaxNcw; ++i)
    {
      QString msg = QStringLiteral ("%1 %2").arg (QString::fromLatin1 (my_call),
                                                  QString::fromLatin1 (his_call));
      if (!my_std)
        {
          if (i == 1 || i >= 6)
            {
              msg = QStringLiteral ("<%1> %2").arg (QString::fromLatin1 (my_call),
                                                    QString::fromLatin1 (his_call));
            }
          if (i >= 2 && i <= 4)
            {
              msg = QStringLiteral ("%1 <%2>").arg (QString::fromLatin1 (my_call),
                                                    QString::fromLatin1 (his_call));
            }
        }
      else if (!his_std)
        {
          if (i <= 4 || i == 6)
            {
              msg = QStringLiteral ("<%1> %2").arg (QString::fromLatin1 (my_call),
                                                    QString::fromLatin1 (his_call));
            }
          if (i >= 7)
            {
              msg = QStringLiteral ("%1 <%2>").arg (QString::fromLatin1 (my_call),
                                                    QString::fromLatin1 (his_call));
            }
        }

      if (i == 2) msg += QStringLiteral (" RRR");
      if (i == 3) msg += QStringLiteral (" RR73");
      if (i == 4) msg += QStringLiteral (" 73");
      if (i == 5)
        {
          msg = his_std
                    ? QStringLiteral ("CQ %1 %2")
                          .arg (QString::fromLatin1 (his_call), QString::fromLatin1 (his_grid.left (4)))
                    : QStringLiteral ("CQ %1").arg (QString::fromLatin1 (his_call));
        }
      if (i == 6 && his_std)
        {
          msg += QStringLiteral (" %1").arg (QString::fromLatin1 (his_grid.left (4)));
        }
      if (i >= 7)
        {
          int const snr = -50 + (i - 7) / 2;
          msg += QLatin1Char {' '};
          msg += format_q65_report (snr, (i & 1) == 0);
        }

      int sent[kQ65CodewordSymbols] {};
      if (!encode_q65_codeword (msg, sent))
        {
          *ncw = i - 1;
          return;
        }

      for (int row = 0; row < kQ65CodewordSymbols; ++row)
        {
          codewords[row + kQ65CodewordSymbols * (i - 1)] = sent[row];
        }
    }

  *ncw = kQ65MaxNcw;
}

extern "C" void ftx_q65_set_list2_c (char const mycall[12], char const hiscall[12],
                                     char const hisgrid[6], char const caller_calls[6 * 40],
                                     char const caller_grids[4 * 40], int* nhist2,
                                     int codewords[63 * 206], int* ncw)
{
  if (ncw)
    {
      *ncw = 0;
    }
  if (!mycall || !hiscall || !hisgrid || !caller_calls || !caller_grids || !nhist2
      || !codewords || !ncw)
    {
      return;
    }

  QByteArray const my_call = from_fixed (mycall, 12);
  QByteArray const his_call = from_fixed (hiscall, 12);
  QByteArray const his_grid = from_fixed (hisgrid, 6);

  int history_count = std::max (0, std::min (*nhist2, 40));
  int jmax = history_count;
  if (is_standard_call (his_call) && is_grid4 (his_grid.left (4)))
    {
      jmax = std::min (40, history_count + 1);
      for (int j = 0; j < history_count; ++j)
        {
          if (from_column_fixed (caller_calls, 6, j).trimmed () == his_call.left (6).trimmed ())
            {
              jmax = history_count;
              break;
            }
        }
    }

  std::fill_n (codewords, kQ65CodewordSymbols * kQ65MaxNcw, 0);

  int i = 1;
  for (int j = 0; j < jmax; ++j)
    {
      QByteArray c6 = (j == history_count)
          ? his_call.left (6)
          : from_column_fixed (caller_calls, 6, j);
      QByteArray g4 = (j == history_count)
          ? his_grid.left (4)
          : from_column_fixed (caller_grids, 4, j);
      QString base = QStringLiteral ("%1 %2")
          .arg (QString::fromLatin1 (my_call.trimmed ()),
                QString::fromLatin1 (c6.trimmed ()));

      for (int k = 1; k <= 5 && i < kQ65MaxNcw; ++k)
        {
          ++i;
          QString msg = base;
          if (k == 1)
            {
              msg += QStringLiteral (" %1").arg (QString::fromLatin1 (g4.trimmed ()));
            }
          else if (k == 2)
            {
              msg += QStringLiteral (" R %1").arg (QString::fromLatin1 (g4.trimmed ()));
            }
          else if (k == 3)
            {
              msg += QStringLiteral (" RRR");
            }
          else if (k == 4)
            {
              msg += QStringLiteral (" RR73");
            }
          else if (k == 5)
            {
              msg += QStringLiteral (" 73");
            }

          int sent[kQ65CodewordSymbols] {};
          if (!encode_q65_codeword (msg, sent))
            {
              *ncw = i - 1;
              return;
            }

          for (int row = 0; row < kQ65CodewordSymbols; ++row)
            {
              codewords[row + kQ65CodewordSymbols * (i - 1)] = sent[row];
            }
        }
    }

  *ncw = i;
}
