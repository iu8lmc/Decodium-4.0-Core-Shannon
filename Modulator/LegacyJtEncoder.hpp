#ifndef LEGACY_JT_ENCODER_HPP
#define LEGACY_JT_ENCODER_HPP

#include "FtxMessageEncoder.hpp"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>

#include <QByteArray>
#include <QString>
#include <QVector>

extern "C" void rs_encode_ (int* dgen, int* sent);
extern "C" std::uint32_t nhash (void const* key, size_t length, std::uint32_t initval);

namespace decodium
{
namespace legacy_jt
{
namespace detail
{

constexpr int kJtNBase = 37 * 36 * 10 * 27 * 27 * 27;
constexpr int kJtNgBase = 180 * 180;
constexpr int kJt65ToneCount = 126;
constexpr int kJt65DataSymbolCount = 63;
constexpr int kJt4ToneCount = 206;
constexpr int kJt9ToneCount = 85;
constexpr int kJt9EncodedBits = 207;
constexpr int kJt9DataSymbolCount = 69;
constexpr int kWsprToneCount = 162;
constexpr std::uint32_t kConv232Poly1 = static_cast<std::uint32_t> (-221228207);
constexpr std::uint32_t kConv232Poly2 = static_cast<std::uint32_t> (-463389625);

std::array<int, kJt65ToneCount> const kJt65SyncMask = {{
  1,0,0,1,1,0,0,0,1,1,1,1,1,1,0,1,0,1,0,0,
  0,1,0,1,1,0,0,1,0,0,0,1,1,1,0,0,1,1,1,1,
  0,1,1,0,1,1,1,1,0,0,0,1,1,0,1,0,1,0,1,1,
  0,0,1,1,0,1,0,1,0,1,0,0,1,0,0,0,0,0,0,1,
  1,0,0,0,0,0,0,0,1,1,0,1,0,0,1,0,1,1,0,1,
  0,1,0,1,0,0,1,1,0,0,1,0,0,1,0,0,0,0,1,1,
  1,1,1,1,1,1
}};

std::array<int, kJt9ToneCount> const kJt9SyncMask = {{
  1,1,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,
  0,0,1,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,0,0,1,
  0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
  0,0,1,0,1
}};

std::array<int, 207> const kJt4SyncPattern = {{
  0,0,0,0,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,0,0,0,0,0,0,0,1,1,0,0,
  0,0,0,0,0,0,0,0,0,0,1,0,1,1,0,1,1,0,1,0,1,1,1,1,1,0,1,0,0,0,
  1,0,0,1,0,0,1,1,1,1,1,0,0,0,1,0,1,0,0,0,1,1,1,1,0,1,1,0,0,1,
  0,0,0,1,1,0,1,0,1,0,1,0,1,0,1,1,1,1,1,0,1,0,1,0,1,1,0,1,0,1,
  0,1,1,1,0,0,1,0,1,1,0,1,1,1,1,0,0,0,0,1,1,0,1,1,0,0,0,1,1,1,
  0,1,1,1,0,1,1,1,0,0,1,0,0,0,1,1,0,1,1,0,0,1,0,0,0,1,1,1,1,1,
  1,0,0,1,1,0,0,0,0,1,1,0,0,0,1,0,1,1,0,1,1,1,1,0,1,0,1
}};

std::array<int, 10> const kWsprNu = {{
  0, -1, 1, 0, -1, 2, 1, 0, -1, 1
}};

std::array<int, kWsprToneCount> const kWsprSyncMask = {{
  1,1,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,1,0,
  0,1,0,1,1,1,1,0,0,0,0,0,0,0,1,0,0,1,0,1,
  0,0,0,0,0,0,1,0,1,1,0,0,1,1,0,1,0,0,0,1,
  1,0,1,0,0,0,0,1,1,0,1,0,1,0,1,0,1,0,0,1,
  0,0,1,0,1,1,0,0,0,1,1,0,1,0,1,0,0,0,1,0,
  0,0,0,0,1,0,0,1,0,0,1,1,1,0,1,1,0,0,1,1,
  0,1,0,0,0,1,1,1,0,0,0,0,0,1,0,1,0,0,1,1,
  0,0,0,0,0,0,0,1,1,0,1,0,1,1,0,0,0,1,1,0,
  0,0
}};

std::array<char const*, 339> const kJtPfx = {{
  "1A   ", "1S   ", "3A   ", "3B6  ", "3B8  ", "3B9  ", "3C   ", "3C0  ",
  "3D2  ", "3D2C ", "3D2R ", "3DA  ", "3V   ", "3W   ", "3X   ", "3Y   ",
  "3YB  ", "3YP  ", "4J   ", "4L   ", "4S   ", "4U1I ", "4U1U ", "4W   ",
  "4X   ", "5A   ", "5B   ", "5H   ", "5N   ", "5R   ", "5T   ", "5U   ",
  "5V   ", "5W   ", "5X   ", "5Z   ", "6W   ", "6Y   ", "7O   ", "7P   ",
  "7Q   ", "7X   ", "8P   ", "8Q   ", "8R   ", "9A   ", "9G   ", "9H   ",
  "9J   ", "9K   ", "9L   ", "9M2  ", "9M6  ", "9N   ", "9Q   ", "9U   ",
  "9V   ", "9X   ", "9Y   ", "A2   ", "A3   ", "A4   ", "A5   ", "A6   ",
  "A7   ", "A9   ", "AP   ", "BS7  ", "BV   ", "BV9  ", "BY   ", "C2   ",
  "C3   ", "C5   ", "C6   ", "C9   ", "CE   ", "CE0X ", "CE0Y ", "CE0Z ",
  "CE9  ", "CM   ", "CN   ", "CP   ", "CT   ", "CT3  ", "CU   ", "CX   ",
  "CY0  ", "CY9  ", "D2   ", "D4   ", "D6   ", "DL   ", "DU   ", "E3   ",
  "E4   ", "EA   ", "EA6  ", "EA8  ", "EA9  ", "EI   ", "EK   ", "EL   ",
  "EP   ", "ER   ", "ES   ", "ET   ", "EU   ", "EX   ", "EY   ", "EZ   ",
  "F    ", "FG   ", "FH   ", "FJ   ", "FK   ", "FKC  ", "FM   ", "FO   ",
  "FOA  ", "FOC  ", "FOM  ", "FP   ", "FR   ", "FRG  ", "FRJ  ", "FRT  ",
  "FT5W ", "FT5X ", "FT5Z ", "FW   ", "FY   ", "M    ", "MD   ", "MI   ",
  "MJ   ", "MM   ", "MU   ", "MW   ", "H4   ", "H40  ", "HA   ", "HB   ",
  "HB0  ", "HC   ", "HC8  ", "HH   ", "HI   ", "HK   ", "HK0  ", "HK0M ",
  "HL   ", "HM   ", "HP   ", "HR   ", "HS   ", "HV   ", "HZ   ", "I    ",
  "IS   ", "IS0  ", "J2   ", "J3   ", "J5   ", "J6   ", "J7   ", "J8   ",
  "JA   ", "JDM  ", "JDO  ", "JT   ", "JW   ", "JX   ", "JY   ", "K    ",
  "KG4  ", "KH0  ", "KH1  ", "KH2  ", "KH3  ", "KH4  ", "KH5  ", "KH5K ",
  "KH6  ", "KH7  ", "KH8  ", "KH9  ", "KL   ", "KP1  ", "KP2  ", "KP4  ",
  "KP5  ", "LA   ", "LU   ", "LX   ", "LY   ", "LZ   ", "OA   ", "OD   ",
  "OE   ", "OH   ", "OH0  ", "OJ0  ", "OK   ", "OM   ", "ON   ", "OX   ",
  "OY   ", "OZ   ", "P2   ", "P4   ", "PA   ", "PJ2  ", "PJ7  ", "PY   ",
  "PY0F ", "PT0S ", "PY0T ", "PZ   ", "R1F  ", "R1M  ", "S0   ", "S2   ",
  "S5   ", "S7   ", "S9   ", "SM   ", "SP   ", "ST   ", "SU   ", "SV   ",
  "SVA  ", "SV5  ", "SV9  ", "T2   ", "T30  ", "T31  ", "T32  ", "T33  ",
  "T5   ", "T7   ", "T8   ", "T9   ", "TA   ", "TF   ", "TG   ", "TI   ",
  "TI9  ", "TJ   ", "TK   ", "TL   ", "TN   ", "TR   ", "TT   ", "TU   ",
  "TY   ", "TZ   ", "UA   ", "UA2  ", "UA9  ", "UK   ", "UN   ", "UR   ",
  "V2   ", "V3   ", "V4   ", "V5   ", "V6   ", "V7   ", "V8   ", "VE   ",
  "VK   ", "VK0H ", "VK0M ", "VK9C ", "VK9L ", "VK9M ", "VK9N ", "VK9W ",
  "VK9X ", "VP2E ", "VP2M ", "VP2V ", "VP5  ", "VP6  ", "VP6D ", "VP8  ",
  "VP8G ", "VP8H ", "VP8O ", "VP8S ", "VP9  ", "VQ9  ", "VR   ", "VU   ",
  "VU4  ", "VU7  ", "XE   ", "XF4  ", "XT   ", "XU   ", "XW   ", "XX9  ",
  "XZ   ", "YA   ", "YB   ", "YI   ", "YJ   ", "YK   ", "YL   ", "YN   ",
  "YO   ", "YS   ", "YU   ", "YV   ", "YV0  ", "Z2   ", "Z3   ", "ZA   ",
  "ZB   ", "ZC4  ", "ZD7  ", "ZD8  ", "ZD9  ", "ZF   ", "ZK1N ", "ZK1S ",
  "ZK2  ", "ZK3  ", "ZL   ", "ZL7  ", "ZL8  ", "ZL9  ", "ZP   ", "ZS   ",
  "ZS8  ", "KC4  ", "E5   ",
}};

std::array<char const*, 12> const kJtSfx = {{"P", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A"}};

inline QByteArray fixed_ascii (QByteArray text, int width)
{
  if (text.size () < width)
    {
      text.append (QByteArray (width - text.size (), ' '));
    }
  else if (text.size () > width)
    {
      text.truncate (width);
    }
  return text;
}

inline QByteArray fixed_ascii (QString const& text, int width)
{
  return fixed_ascii (text.left (width).toLatin1 (), width);
}

inline bool is_digit (char c)
{
  return c >= '0' && c <= '9';
}

inline bool is_upper_letter (char c)
{
  return c >= 'A' && c <= 'Z';
}

inline QByteArray upper_ascii (QByteArray text)
{
  for (char& c : text)
    {
      if (c >= 'a' && c <= 'z')
        {
          c = static_cast<char> (c - 'a' + 'A');
        }
    }
  return text;
}

inline QByteArray trim_right_ascii (QByteArray text)
{
  while (!text.isEmpty () && text.back () == ' ')
    {
      text.chop (1);
    }
  return text;
}

inline QByteArray trim_left_ascii (QByteArray text)
{
  int i = 0;
  while (i < text.size () && text.at (i) == ' ')
    {
      ++i;
    }
  return text.mid (i);
}

inline bool starts_with_ascii (QByteArray const& text, char const* prefix)
{
  int const size = static_cast<int> (std::strlen (prefix));
  return text.size () >= size && std::memcmp (text.constData (), prefix, size) == 0;
}

inline QByteArray fmtmsg (QByteArray msg)
{
  msg = fixed_ascii (upper_ascii (msg), 22);
  int iz = 0;
  for (int i = 0; i < msg.size (); ++i)
    {
      if (msg.at (i) != ' ')
        {
          iz = i + 1;
        }
    }

  QByteArray normalized;
  normalized.reserve (22);
  bool previous_blank = false;
  for (int i = 0; i < iz; ++i)
    {
      char const c = msg.at (i);
      if (c == ' ' && previous_blank)
        {
          continue;
        }
      normalized.append (c);
      previous_blank = (c == ' ');
    }
  return fixed_ascii (normalized, 22);
}

inline int nchar (char c)
{
  if (c >= '0' && c <= '9')
    {
      return c - '0';
    }
  if (c >= 'A' && c <= 'Z')
    {
      return c - 'A' + 10;
    }
  if (c >= 'a' && c <= 'z')
    {
      return c - 'a' + 10;
    }
  return 36;
}

inline QByteArray format_signed_report (int value)
{
  int const abs_value = std::abs (value);
  QByteArray result (3, '0');
  result[0] = value < 0 ? '-' : '+';
  result[1] = static_cast<char> ('0' + ((abs_value / 10) % 10));
  result[2] = static_cast<char> ('0' + (abs_value % 10));
  return result;
}

inline QByteArray format_reply_report (int value)
{
  QByteArray result {"R+00"};
  int const abs_value = std::abs (value);
  result[1] = value < 0 ? '-' : '+';
  result[2] = static_cast<char> ('0' + ((abs_value / 10) % 10));
  result[3] = static_cast<char> ('0' + (abs_value % 10));
  return result;
}

struct CheckMessageResult
{
  QByteArray message;
  QByteArray cok;
  int nspecial {0};
  bool flip {false};
};

inline CheckMessageResult chkmsg (QByteArray const& message_in)
{
  CheckMessageResult result;
  result.message = fixed_ascii (message_in, 22);
  result.cok = QByteArray ("   ", 3);

  int last_non_blank = -1;
  for (int i = 21; i >= 0; --i)
    {
      if (result.message.at (i) != ' ')
        {
          last_non_blank = i;
          break;
        }
    }
  if (last_non_blank < 0)
    {
      last_non_blank = 21;
    }

  if (last_non_blank >= 10)
    {
      bool const has_short_ooo = result.message.mid (19, 3) == QByteArray (" OO", 3);
      bool const has_full_ooo =
          last_non_blank >= 3 && result.message.mid (last_non_blank - 3, 4) == QByteArray (" OOO", 4);
      if (has_short_ooo || has_full_ooo)
        {
          result.cok = QByteArray ("OOO", 3);
          result.flip = true;
          if (has_short_ooo)
            {
              result.message = fixed_ascii (result.message.left (19), 22);
            }
          else
            {
              result.message = fixed_ascii (result.message.left (last_non_blank - 3), 22);
            }
        }
    }

  if (result.message == fixed_ascii (QByteArray ("RO", 2), 22)) result.nspecial = 2;
  if (result.message == fixed_ascii (QByteArray ("RRR", 3), 22)) result.nspecial = 3;
  if (result.message == fixed_ascii (QByteArray ("73", 2), 22)) result.nspecial = 4;
  return result;
}

inline void grid2deg (QByteArray grid0, double& dlong, double& dlat)
{
  QByteArray grid = fixed_ascii (grid0, 6);
  unsigned char const fifth = static_cast<unsigned char> (grid.at (4));
  if (grid.at (4) == ' ' || fifth <= 64 || fifth >= 128)
    {
      grid[4] = 'm';
      grid[5] = 'm';
    }

  if (grid[0] >= 'a' && grid[0] <= 'z') grid[0] = static_cast<char> (grid[0] - 'a' + 'A');
  if (grid[1] >= 'a' && grid[1] <= 'z') grid[1] = static_cast<char> (grid[1] - 'a' + 'A');
  if (grid[4] >= 'A' && grid[4] <= 'Z') grid[4] = static_cast<char> (grid[4] - 'A' + 'a');
  if (grid[5] >= 'A' && grid[5] <= 'Z') grid[5] = static_cast<char> (grid[5] - 'A' + 'a');

  int const nlong = 180 - 20 * (grid.at (0) - 'A');
  int const n20d = 2 * (grid.at (2) - '0');
  double const xminlong = 5.0 * (grid.at (4) - 'a' + 0.5);
  dlong = static_cast<double> (nlong) - static_cast<double> (n20d) - xminlong / 60.0;
  int const nlat = -90 + 10 * (grid.at (1) - 'A') + (grid.at (3) - '0');
  double const xminlat = 2.5 * (grid.at (5) - 'a' + 0.5);
  dlat = static_cast<double> (nlat) + xminlat / 60.0;
}

inline QByteArray deg2grid (double dlong0, double dlat)
{
  double dlong = dlong0;
  if (dlong < -180.0) dlong += 360.0;
  if (dlong > 180.0) dlong -= 360.0;

  QByteArray grid (6, ' ');
  int const nlong = static_cast<int> (60.0 * (180.0 - dlong) / 5.0);
  int const n1 = nlong / 240;
  int const n2 = (nlong - 240 * n1) / 24;
  int const n3 = nlong - 240 * n1 - 24 * n2;
  grid[0] = static_cast<char> ('A' + n1);
  grid[2] = static_cast<char> ('0' + n2);
  grid[4] = static_cast<char> ('a' + n3);

  int const nlat = static_cast<int> (60.0 * (dlat + 90.0) / 2.5);
  int const m1 = nlat / 240;
  int const m2 = (nlat - 240 * m1) / 24;
  int const m3 = nlat - 240 * m1 - 24 * m2;
  grid[1] = static_cast<char> ('A' + m1);
  grid[3] = static_cast<char> ('0' + m2);
  grid[5] = static_cast<char> ('a' + m3);
  return grid;
}

inline int grid2k (QByteArray const& grid)
{
  double xlong = 0.0;
  double xlat = 0.0;
  grid2deg (grid + QByteArray ("55", 2), xlong, xlat);
  int const nlong = qRound (xlong);
  int const nlat = qRound (xlat);
  if (nlat >= 85)
    {
      return 5 * ((nlong + 179) / 2) + nlat - 84;
    }
  return 0;
}

inline QByteArray k2grid (int k)
{
  int nlong = 2 * (((k - 1) / 5) % 90) - 179;
  if (k > 450)
    {
      nlong += 180;
    }
  int const nlat = ((k - 1) % 5) + 85;
  return deg2grid (static_cast<double> (nlong), static_cast<double> (nlat));
}

struct PackCallResult
{
  int ncall {0};
  bool text {false};
  QByteArray normalized;
};

inline PackCallResult packcall (QByteArray callsign_in)
{
  PackCallResult result;
  QByteArray callsign = fixed_ascii (callsign_in, 6);

  if (callsign.left (4) == QByteArray ("3DA0", 4))
    {
      callsign = QByteArray ("3D0", 3) + callsign.mid (4, 2);
    }
  if (callsign.left (2) == QByteArray ("3X", 2)
      && callsign.at (2) >= 'A' && callsign.at (2) <= 'Z')
    {
      callsign = QByteArray ("Q", 1) + callsign.mid (2, 4);
    }

  if (callsign.left (3) == QByteArray ("CQ ", 3))
    {
      result.ncall = kJtNBase + 1;
      if (is_digit (callsign.at (3)) && is_digit (callsign.at (4)) && is_digit (callsign.at (5)))
        {
          bool ok = false;
          int const nfreq = QString::fromLatin1 (callsign.mid (3, 3).constData (), 3).toInt (&ok);
          if (ok)
            {
              result.ncall = kJtNBase + 3 + nfreq;
            }
        }
      result.normalized = callsign;
      return result;
    }
  if (callsign.left (4) == QByteArray ("QRZ ", 4))
    {
      result.ncall = kJtNBase + 2;
      result.normalized = callsign;
      return result;
    }
  if (callsign.left (3) == QByteArray ("DE ", 3))
    {
      result.ncall = 267796945;
      result.normalized = callsign;
      return result;
    }

  QByteArray tmp (6, ' ');
  if (is_digit (callsign.at (2)))
    {
      tmp = callsign;
    }
  else if (is_digit (callsign.at (1)))
    {
      if (callsign.at (5) != ' ')
        {
          result.text = true;
          return result;
        }
      tmp = QByteArray (" ", 1) + callsign.left (5);
    }
  else
    {
      result.text = true;
      return result;
    }

  tmp = upper_ascii (tmp);
  bool const n1 = (is_upper_letter (tmp.at (0)) || tmp.at (0) == ' ' || is_digit (tmp.at (0)));
  bool const n2 = (is_upper_letter (tmp.at (1)) || is_digit (tmp.at (1)));
  bool const n3 = is_digit (tmp.at (2));
  bool const n4 = (is_upper_letter (tmp.at (3)) || tmp.at (3) == ' ');
  bool const n5 = (is_upper_letter (tmp.at (4)) || tmp.at (4) == ' ');
  bool const n6 = (is_upper_letter (tmp.at (5)) || tmp.at (5) == ' ');
  if (!(n1 && n2 && n3 && n4 && n5 && n6))
    {
      result.text = true;
      return result;
    }

  int ncall = nchar (tmp.at (0));
  ncall = 36 * ncall + nchar (tmp.at (1));
  ncall = 10 * ncall + nchar (tmp.at (2));
  ncall = 27 * ncall + nchar (tmp.at (3)) - 10;
  ncall = 27 * ncall + nchar (tmp.at (4)) - 10;
  ncall = 27 * ncall + nchar (tmp.at (5)) - 10;

  result.ncall = ncall;
  result.normalized = tmp;
  return result;
}

inline QByteArray unpack_standard_call (int ncall)
{
  static char const kAlphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
  QByteArray word {"......", 6};
  int n = ncall;
  word[5] = kAlphabet[(n % 27) + 10];
  n /= 27;
  word[4] = kAlphabet[(n % 27) + 10];
  n /= 27;
  word[3] = kAlphabet[(n % 27) + 10];
  n /= 27;
  word[2] = kAlphabet[n % 10];
  n /= 10;
  word[1] = kAlphabet[n % 36];
  n /= 36;
  word[0] = kAlphabet[n];

  int first = 0;
  while (first < 4 && word.at (first) == ' ')
    {
      ++first;
    }
  word = fixed_ascii (word.mid (first), 12);
  if (word.left (3) == QByteArray ("3D0", 3))
    {
      word = fixed_ascii (QByteArray ("3DA0", 4) + word.mid (3), 12);
    }
  if (word.at (0) == 'Q' && word.at (1) >= 'A' && word.at (1) <= 'Z')
    {
      word = fixed_ascii (QByteArray ("3X", 2) + word.mid (1), 12);
    }
  return word;
}

struct UnpackCallResult
{
  QByteArray word;
  int iv2 {0};
  QByteArray psfx;
};

inline UnpackCallResult unpackcall (int ncall)
{
  static char const kAlphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

  UnpackCallResult result;
  result.word = fixed_ascii (QByteArray ("......", 6), 12);
  result.psfx = QByteArray (4, ' ');
  int n = ncall;

  if (n < 262177560)
    {
      result.word = unpack_standard_call (n);
      return result;
    }
  if (n >= 267796946)
    {
      return result;
    }

  auto decode_prefix = [&] (int count, int base, int iv2) {
    result.iv2 = iv2;
    n -= base;
    for (int index = count - 1; index >= 0; --index)
      {
        result.psfx[index] = kAlphabet[(n % 37)];
        n /= 37;
      }
  };

  if (n >= 262178563 && n <= 264002071)
    {
      decode_prefix (4, 262178563, 1);
    }
  else if (n >= 264002072 && n <= 265825580)
    {
      decode_prefix (4, 264002072, 2);
    }
  else if (n >= 265825581 && n <= 267649089)
    {
      decode_prefix (4, 265825581, 3);
    }
  else if (n >= 267649090 && n <= 267698374)
    {
      decode_prefix (3, 267649090, 4);
    }
  else if (n >= 267698375 && n <= 267747659)
    {
      decode_prefix (3, 267698375, 5);
    }
  else if (n >= 267747660 && n <= 267796944)
    {
      decode_prefix (3, 267747660, 6);
    }
  else if (n == 267796945)
    {
      result.iv2 = 7;
    }

  if (result.word.left (3) == QByteArray ("3D0", 3))
    {
      result.word = fixed_ascii (QByteArray ("3DA0", 4) + result.word.mid (3), 12);
    }
  if (result.word.at (0) == 'Q' && result.word.at (1) >= 'A' && result.word.at (1) <= 'Z')
    {
      result.word = fixed_ascii (QByteArray ("3X", 2) + result.word.mid (1), 12);
    }
  return result;
}

struct PrefixResult
{
  QByteArray callsign;
  int k {0};
  int nv2 {1};
};

inline PrefixResult getpfx1 (QByteArray const& callsign0_in)
{
  PrefixResult result;
  QByteArray callsign0 = fixed_ascii (upper_ascii (callsign0_in), 12);
  QByteArray callsign = callsign0;
  int iz = callsign.indexOf (' ');
  if (iz < 0)
    {
      iz = 12;
    }
  int const islash = callsign.left (iz).indexOf ('/');

  if (islash >= 0 && islash <= (iz - 4))
    {
      QByteArray const prefix = fixed_ascii (callsign.left (islash), 4);
      callsign = fixed_ascii (callsign.mid (islash + 1, iz - islash - 1), 12);
      for (size_t i = 0; i < kJtPfx.size (); ++i)
        {
          if (std::memcmp (kJtPfx[i], prefix.constData (), 4) == 0)
            {
              result.callsign = callsign;
              result.k = static_cast<int> (i) + 1;
              result.nv2 = 2;
              return result;
            }
        }
    }
  else if (islash == (iz - 2))
    {
      char const suffix = callsign.at (islash + 1);
      callsign = fixed_ascii (callsign.left (islash), 12);
      for (size_t i = 0; i < kJtSfx.size (); ++i)
        {
          if (kJtSfx[i][0] == suffix)
            {
              result.callsign = callsign;
              result.k = 401 + static_cast<int> (i);
              result.nv2 = 3;
              return result;
            }
        }
    }

  if (islash >= 0 && result.k == 0)
    {
      QByteArray const lof = fixed_ascii (callsign0.left (islash), 12);
      QByteArray const rof = fixed_ascii (callsign0.mid (islash + 1), 12);
      int const llof = trim_right_ascii (lof).size ();
      int const lrof = trim_right_ascii (rof).size ();
      bool ispfx = llof > 0 && llof <= 4;
      bool issfx = lrof > 0 && lrof <= 3;
      bool const invalid = !(ispfx || issfx);
      if (ispfx && issfx)
        {
          if (llof < 3) issfx = false;
          if (lrof < 3) ispfx = false;
          if (ispfx && issfx)
            {
              char const c = callsign0.at (islash - 1);
              if (c >= '0' && c <= '9')
                {
                  issfx = false;
                }
              else
                {
                  ispfx = false;
                }
            }
        }

      if (invalid)
        {
          result.callsign = callsign0;
          result.k = -1;
          return result;
        }

      if (ispfx)
        {
          QByteArray const tpfx = fixed_ascii (lof.left (4), 4);
          int k = nchar (tpfx.at (0));
          k = 37 * k + nchar (tpfx.at (1));
          k = 37 * k + nchar (tpfx.at (2));
          k = 37 * k + nchar (tpfx.at (3));
          result.callsign = fixed_ascii (callsign0.mid (islash + 1), 12);
          result.k = k;
          result.nv2 = 4;
          return result;
        }

      if (issfx)
        {
          QByteArray const tsfx = fixed_ascii (rof.left (3), 3);
          int k = nchar (tsfx.at (0));
          k = 37 * k + nchar (tsfx.at (1));
          k = 37 * k + nchar (tsfx.at (2));
          result.callsign = fixed_ascii (callsign0.left (islash), 12);
          result.k = k;
          result.nv2 = 5;
          return result;
        }
    }

  result.callsign = callsign0;
  return result;
}

inline QByteArray getpfx2 (int k0, QByteArray callsign)
{
  callsign = fixed_ascii (trim_right_ascii (callsign), 12);
  int k = k0;
  if (k > 450)
    {
      k -= 450;
    }

  if (k >= 1 && k <= static_cast<int> (kJtPfx.size ()))
    {
      QByteArray prefix = trim_right_ascii (QByteArray (kJtPfx[static_cast<size_t> (k - 1)], 5));
      return fixed_ascii (prefix + QByteArray ("/", 1) + trim_right_ascii (callsign), 12);
    }
  if (k >= 401 && k <= 400 + static_cast<int> (kJtSfx.size ()))
    {
      QByteArray suffix (kJtSfx[static_cast<size_t> (k - 401)], 1);
      return fixed_ascii (trim_right_ascii (callsign) + QByteArray ("/", 1) + suffix, 12);
    }
  return callsign;
}

struct PackGridResult
{
  int ng {0};
  bool text {false};
};

inline PackGridResult packgrid (QByteArray grid_in)
{
  PackGridResult result;
  QByteArray grid = fixed_ascii (upper_ascii (grid_in), 4);
  if (grid == QByteArray ("    ", 4))
    {
      result.ng = kJtNgBase + 1;
      return result;
    }

  if (grid.at (0) == '-')
    {
      bool ok = false;
      int const n = QString::fromLatin1 (grid.mid (1, 2)).toInt (&ok);
      if (ok && n >= 1 && n <= 30)
        {
          result.ng = kJtNgBase + 1 + n;
          return result;
        }
    }
  else if (grid.left (2) == QByteArray ("R-", 2))
    {
      bool ok = false;
      int const n = QString::fromLatin1 (grid.mid (2, 2)).toInt (&ok);
      if (ok && n >= 1 && n <= 30)
        {
          result.ng = kJtNgBase + 31 + n;
          return result;
        }
    }
  else if (grid == QByteArray ("RO  ", 4))
    {
      result.ng = kJtNgBase + 62;
      return result;
    }
  else if (grid == QByteArray ("RRR ", 4))
    {
      result.ng = kJtNgBase + 63;
      return result;
    }
  else if (grid == QByteArray ("73  ", 4))
    {
      result.ng = kJtNgBase + 64;
      return result;
    }

  int n = 99;
  char const c1 = grid.at (0);
  bool ok = false;
  n = QString::fromLatin1 (grid).trimmed ().toInt (&ok);
  if (!ok)
    {
      n = QString::fromLatin1 (grid.mid (1, 3)).trimmed ().toInt (&ok);
    }
  if (ok && n >= -50 && n <= 49)
    {
      if (c1 == 'R')
        {
          grid = fixed_ascii (QByteArray ("LA", 2)
                              + QByteArray::number (n + 50).rightJustified (2, '0'), 4);
        }
      else
        {
          grid = fixed_ascii (QByteArray ("KA", 2)
                              + QByteArray::number (n + 50).rightJustified (2, '0'), 4);
        }
    }

  if (grid.at (0) < 'A' || grid.at (0) > 'R'
      || grid.at (1) < 'A' || grid.at (1) > 'R'
      || grid.at (2) < '0' || grid.at (2) > '9'
      || grid.at (3) < '0' || grid.at (3) > '9')
    {
      result.text = true;
      return result;
    }

  double dlong = 0.0;
  double dlat = 0.0;
  grid2deg (grid + QByteArray ("mm", 2), dlong, dlat);
  int const longitude = static_cast<int> (dlong);
  int const latitude = static_cast<int> (dlat + 90.0);
  result.ng = ((longitude + 180) / 2) * 180 + latitude;
  return result;
}

inline QByteArray unpackgrid (int ng)
{
  QByteArray grid (4, ' ');
  if (ng < kJtNgBase)
    {
      double const dlat = static_cast<double> (ng % 180 - 90);
      double const dlong = static_cast<double> ((ng / 180) * 2 - 180 + 2);
      grid = deg2grid (dlong, dlat).left (4);
      if (grid.left (2) == QByteArray ("KA", 2))
        {
          bool ok = false;
          int const n = QString::fromLatin1 (grid.mid (2, 2)).toInt (&ok) - 50;
          if (ok)
            {
              grid = format_signed_report (n);
            }
        }
      else if (grid.left (2) == QByteArray ("LA", 2))
        {
          bool ok = false;
          int const n = QString::fromLatin1 (grid.mid (2, 2)).toInt (&ok) - 50;
          if (ok)
            {
              grid = format_reply_report (n);
            }
        }
      return fixed_ascii (grid, 4);
    }

  int n = ng - kJtNgBase - 1;
  if (n >= 1 && n <= 30)
    {
      grid = format_signed_report (-n);
    }
  else if (n >= 31 && n <= 60)
    {
      grid = format_reply_report (-(n - 30));
    }
  else if (n == 61)
    {
      grid = QByteArray ("RO", 2);
    }
  else if (n == 62)
    {
      grid = QByteArray ("RRR", 3);
    }
  else if (n == 63)
    {
      grid = QByteArray ("73", 2);
    }
  return fixed_ascii (grid, 4);
}

inline void packtext (QByteArray const& msg_in, int& nc1, int& nc2, int& nc3)
{
  static QByteArray const chars {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ +-./?"};
  QByteArray const msg = fixed_ascii (upper_ascii (msg_in), 22);
  nc1 = 0;
  nc2 = 0;
  nc3 = 0;

  auto char_code = [&] (char c) {
    int index = chars.indexOf (c);
    return index >= 0 ? index : 36;
  };

  for (int i = 0; i < 5; ++i) nc1 = 42 * nc1 + char_code (msg.at (i));
  for (int i = 5; i < 10; ++i) nc2 = 42 * nc2 + char_code (msg.at (i));
  for (int i = 10; i < 13; ++i) nc3 = 42 * nc3 + char_code (msg.at (i));

  nc1 *= 2;
  if ((nc3 & 32768) != 0) ++nc1;
  nc2 *= 2;
  if ((nc3 & 65536) != 0) ++nc2;
  nc3 &= 32767;
}

inline QByteArray unpacktext (int nc1a, int nc2a, int nc3a)
{
  static QByteArray const chars {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ +-./?"};
  QByteArray msg (22, ' ');
  int nc1 = nc1a;
  int nc2 = nc2a;
  int nc3 = nc3a & 32767;
  if ((nc1 & 1) != 0) nc3 += 32768;
  nc1 /= 2;
  if ((nc2 & 1) != 0) nc3 += 65536;
  nc2 /= 2;

  for (int i = 4; i >= 0; --i)
    {
      msg[i] = chars.at ((nc1 % 42));
      nc1 /= 42;
    }
  for (int i = 9; i >= 5; --i)
    {
      msg[i] = chars.at ((nc2 % 42));
      nc2 /= 42;
    }
  for (int i = 12; i >= 10; --i)
    {
      msg[i] = chars.at ((nc3 % 42));
      nc3 /= 42;
    }
  return msg;
}

struct PackedJtMessage
{
  bool ok {true};
  int itype {1};
  int nc1 {0};
  int nc2 {0};
  int ng {0};
  std::array<int, 12> dat {{}};
};

inline PackedJtMessage packmsg (QByteArray const& msg0_in)
{
  PackedJtMessage result;
  QByteArray msg = fmtmsg (fixed_ascii (msg0_in, 22));
  auto finalize = [&result] {
    result.dat[0] = (result.nc1 >> 22) & 63;
    result.dat[1] = (result.nc1 >> 16) & 63;
    result.dat[2] = (result.nc1 >> 10) & 63;
    result.dat[3] = (result.nc1 >> 4) & 63;
    result.dat[4] = 4 * (result.nc1 & 15) + ((result.nc2 >> 26) & 3);
    result.dat[5] = (result.nc2 >> 20) & 63;
    result.dat[6] = (result.nc2 >> 14) & 63;
    result.dat[7] = (result.nc2 >> 8) & 63;
    result.dat[8] = (result.nc2 >> 2) & 63;
    result.dat[9] = 16 * (result.nc2 & 3) + ((result.ng >> 12) & 15);
    result.dat[10] = (result.ng >> 6) & 63;
    result.dat[11] = result.ng & 63;
    return result;
  };

  if (msg.left (3) == QByteArray ("CQ ", 3)
      && is_digit (msg.at (3)) && msg.at (4) == ' ')
    {
      msg = fixed_ascii (QByteArray ("CQ 00", 5) + msg.mid (3), 22);
    }
  if (msg.left (6) == QByteArray ("CQ DX ", 6))
    {
      msg[2] = '9';
    }
  if (msg.left (3) == QByteArray ("CQ ", 3)
      && is_upper_letter (msg.at (3)) && is_upper_letter (msg.at (4)) && msg.at (5) == ' ')
    {
      msg = fixed_ascii (QByteArray ("E9", 2) + msg.mid (3), 22);
    }

  int ia = -1;
  if (msg.left (3) == QByteArray ("CQ ", 3))
    {
      ia = 3;
      if (is_digit (msg.at (3)) && is_digit (msg.at (4)) && is_digit (msg.at (5)))
        {
          ia = 7;
        }
    }
  else
    {
      for (int i = 0; i < 22; ++i)
        {
          if (msg.at (i) == ' ')
            {
              ia = i + 1;
              break;
            }
        }
    }

  auto pack_as_text = [&] {
    result.itype = 6;
    packtext (msg, result.nc1, result.nc2, result.ng);
    result.ng += 32768;
  };

  if (ia < 0)
    {
      pack_as_text ();
      return finalize ();
    }

  QByteArray c1 = fixed_ascii (msg.left (ia - 1), 12);
  int ib = -1;
  for (int i = ia; i < 22; ++i)
    {
      if (msg.at (i) == ' ')
        {
          ib = i + 1;
          break;
        }
    }
  if (ib < 0)
    {
      pack_as_text ();
      return finalize ();
    }

  QByteArray c2 = fixed_ascii (msg.mid (ia, ib - ia - 1), 12);
  int ic = -1;
  for (int i = ib; i < 22; ++i)
    {
      if (msg.at (i) == ' ')
        {
          ic = i + 1;
          break;
        }
    }
  if (ic < 0)
    {
      pack_as_text ();
      return finalize ();
    }

  QByteArray c3 (4, ' ');
  if (ic >= ib + 1)
    {
      c3 = fixed_ascii (msg.mid (ib, ic - ib), 4);
    }
  if (c3 == QByteArray ("OOO ", 4))
    {
      c3 = QByteArray ("    ", 4);
    }

  PrefixResult pfx1 = getpfx1 (c1);
  if (pfx1.nv2 >= 4)
    {
      pack_as_text ();
      return finalize ();
    }
  PackCallResult call1 = packcall (pfx1.callsign);
  PrefixResult pfx2 = getpfx1 (c2);
  PackCallResult call2 = packcall (pfx2.callsign);

  int k1 = pfx1.k;
  int k2 = pfx2.k;
  if (pfx1.nv2 == 2 || pfx1.nv2 == 3 || pfx2.nv2 == 2 || pfx2.nv2 == 3)
    {
      if (k1 < 0 || k2 < 0 || (k1 * k2) != 0)
        {
          pack_as_text ();
          return finalize ();
        }
      if (k2 > 0)
        {
          k2 += 450;
        }
      int const k = std::max (k1, k2);
      if (k > 0)
        {
          c3 = k2grid (k).left (4);
        }
    }

  PackGridResult grid = packgrid (c3);
  if (pfx1.nv2 < 4 && pfx2.nv2 < 4 && !call1.text && !call2.text && !grid.text)
    {
      result.nc1 = call1.ncall;
      result.nc2 = call2.ncall;
      result.ng = grid.ng;
    }
  else
    {
      result.nc1 = 0;
      if (pfx2.nv2 == 4)
        {
          if (c1.left (3) == QByteArray ("CQ ", 3) && !grid.text) result.nc1 = 262178563 + k2;
          if (c1.left (4) == QByteArray ("QRZ ", 4) && !grid.text) result.nc1 = 264002072 + k2;
          if (c1.left (3) == QByteArray ("DE ", 3) && !grid.text) result.nc1 = 265825581 + k2;
        }
      else if (pfx2.nv2 == 5)
        {
          if (c1.left (3) == QByteArray ("CQ ", 3) && !grid.text) result.nc1 = 267649090 + k2;
          if (c1.left (4) == QByteArray ("QRZ ", 4) && !grid.text) result.nc1 = 267698375 + k2;
          if (c1.left (3) == QByteArray ("DE ", 3) && !grid.text) result.nc1 = 267747660 + k2;
        }

      if (result.nc1 == 0)
        {
          pack_as_text ();
          return finalize ();
        }

      result.nc2 = call2.ncall;
      result.ng = grid.ng;
    }

  result.itype = std::max (pfx1.nv2, pfx2.nv2);
  return finalize ();
}

inline QByteArray unpackmsg (std::array<int, 12> const& dat)
{
  int const nc1 =
      (dat[0] << 22) + (dat[1] << 16) + (dat[2] << 10) + (dat[3] << 4) + ((dat[4] >> 2) & 15);
  int const nc2 =
      ((dat[4] & 3) << 26) + (dat[5] << 20) + (dat[6] << 14) + (dat[7] << 8) + (dat[8] << 2)
      + ((dat[9] >> 4) & 3);
  int const ng = ((dat[9] & 15) << 12) + (dat[10] << 6) + dat[11];

  if (ng >= 32768)
    {
      return unpacktext (nc1, nc2, ng);
    }

  UnpackCallResult c1_unpack = unpackcall (nc1);
  QByteArray c1 = c1_unpack.word;
  bool cqnnn = false;
  if (c1_unpack.iv2 == 0)
    {
      if (nc1 == kJtNBase + 1) c1 = fixed_ascii (QByteArray ("CQ", 2), 12);
      if (nc1 == kJtNBase + 2) c1 = fixed_ascii (QByteArray ("QRZ", 3), 12);
      int const nfreq = nc1 - kJtNBase - 3;
      if (nfreq >= 0 && nfreq <= 999)
        {
          c1 = fixed_ascii (QByteArray ("CQ ", 3) + QByteArray::number (nfreq).rightJustified (3, '0'), 12);
          cqnnn = true;
        }
    }

  QByteArray c2 = unpackcall (nc2).word;
  QByteArray grid = unpackgrid (ng);

  QByteArray msg (22, ' ');
  if (c1_unpack.iv2 > 0)
    {
      QByteArray const psfx = trim_right_ascii (c1_unpack.psfx);
      QByteArray const call = trim_right_ascii (c2);
      QByteArray const grid_text = trim_right_ascii (grid);
      if (c1_unpack.iv2 == 1) msg = fixed_ascii (QByteArray ("CQ ", 3) + psfx + '/' + call + ' ' + grid_text, 22);
      if (c1_unpack.iv2 == 2) msg = fixed_ascii (QByteArray ("QRZ ", 4) + psfx + '/' + call + ' ' + grid_text, 22);
      if (c1_unpack.iv2 == 3) msg = fixed_ascii (QByteArray ("DE ", 3) + psfx + '/' + call + ' ' + grid_text, 22);
      if (c1_unpack.iv2 == 4) msg = fixed_ascii (QByteArray ("CQ ", 3) + call + '/' + psfx + ' ' + grid_text, 22);
      if (c1_unpack.iv2 == 5) msg = fixed_ascii (QByteArray ("QRZ ", 4) + call + '/' + psfx + ' ' + grid_text, 22);
      if (c1_unpack.iv2 == 6) msg = fixed_ascii (QByteArray ("DE ", 3) + call + '/' + psfx + ' ' + grid_text, 22);
      if (c1_unpack.iv2 == 7)
        {
          int const k = grid2k (grid + QByteArray ("ma", 2));
          if (k >= 451 && k <= 900)
            {
              msg = fixed_ascii (QByteArray ("DE ", 3) + trim_right_ascii (getpfx2 (k, c2)), 22);
            }
          else
            {
              msg = fixed_ascii (QByteArray ("DE ", 3) + trim_right_ascii (c2) + ' ' + grid_text, 22);
            }
        }
    }
  else
    {
      int const k = grid2k (grid + QByteArray ("ma", 2));
      if (k >= 1 && k <= 450) c1 = getpfx2 (k, c1);
      if (k >= 451 && k <= 900) c2 = getpfx2 (k, c2);

      QByteArrayList parts;
      if (!trim_right_ascii (c1).isEmpty ()) parts.append (trim_right_ascii (c1));
      if (!trim_right_ascii (c2).isEmpty ()) parts.append (trim_right_ascii (c2));
      if (k == 0 && !trim_right_ascii (grid).isEmpty ()) parts.append (trim_right_ascii (grid));
      msg = fixed_ascii (parts.join (' '), 22);

      if (cqnnn)
        {
          msg = fixed_ascii ((QByteArrayList {trim_right_ascii (c1), trim_right_ascii (c2), trim_right_ascii (grid)}
                              ).join (' '), 22);
        }
    }

  if (msg.left (6) == QByteArray ("CQ9DX ", 6))
    {
      msg[2] = ' ';
    }
  if (msg.left (2) == QByteArray ("E9", 2)
      && is_upper_letter (msg.at (2)) && is_upper_letter (msg.at (3)) && msg.at (4) == ' ')
    {
      msg = fixed_ascii (QByteArray ("CQ ", 3) + msg.mid (2), 22);
    }
  if (msg.left (5) == QByteArray ("CQ 00", 5) && is_digit (msg.at (5)))
    {
      msg = fixed_ascii (QByteArray ("CQ ", 3) + msg.mid (5), 22);
    }
  return msg;
}

inline void interleave63 (std::array<int, kJt65DataSymbolCount>& data)
{
  std::array<int, kJt65DataSymbolCount> transposed {};
  for (int i = 0; i < 7; ++i)
    {
      for (int j = 0; j < 9; ++j)
        {
          transposed[static_cast<size_t> (i * 9 + j)] = data[static_cast<size_t> (j * 7 + i)];
        }
    }
  data = transposed;
}

inline int gray_encode (int value)
{
  return value ^ (value >> 1);
}

inline void entail (std::array<int, 12> const& words, std::array<signed char, 13>& bytes)
{
  int accumulator = 0;
  int bit_count = 0;
  int byte_index = 0;
  bytes.fill (0);
  for (int i = 0; i < 12; ++i)
    {
      int const word = words[static_cast<size_t> (i)];
      for (int j = 0; j < 6; ++j)
        {
          accumulator <<= 1;
          accumulator |= (word >> (5 - j)) & 1;
          accumulator &= 0xff;
          ++bit_count;
          if (bit_count == 8)
            {
              int value = accumulator;
              if (value > 127)
                {
                  value -= 256;
                }
              bytes[static_cast<size_t> (byte_index)] = static_cast<signed char> (value);
              ++byte_index;
              bit_count = 0;
            }
        }
    }
}

inline unsigned char parity32 (std::uint32_t value)
{
  return static_cast<unsigned char> (__builtin_popcount (value) & 1U);
}

inline void encode232 (std::array<signed char, 13> const& data,
                       std::array<unsigned char, 206>& encoded)
{
  std::uint32_t state = 0;
  int k = 0;
  for (int j = 0; j < 13 && k < 206; ++j)
    {
      unsigned int const byte = static_cast<unsigned char> (data[static_cast<size_t> (j)]);
      for (int i = 7; i >= 0 && k < 206; --i)
        {
          state = (state << 1) | static_cast<std::uint32_t> ((byte >> i) & 1U);
          encoded[static_cast<size_t> (k++)] = parity32 (state & kConv232Poly1);
          if (k < 206)
            {
              encoded[static_cast<size_t> (k++)] = parity32 (state & kConv232Poly2);
            }
        }
    }
}

inline std::array<int, 206> interleave9_order ()
{
  std::array<int, 206> order {};
  int k = 0;
  for (int i = 0; i < 256; ++i)
    {
      int m = i;
      int n = m & 1;
      n = 2 * n + ((m >> 1) & 1);
      n = 2 * n + ((m >> 2) & 1);
      n = 2 * n + ((m >> 3) & 1);
      n = 2 * n + ((m >> 4) & 1);
      n = 2 * n + ((m >> 5) & 1);
      n = 2 * n + ((m >> 6) & 1);
      n = 2 * n + ((m >> 7) & 1);
      if (n <= 205)
        {
          order[static_cast<size_t> (k++)] = n;
        }
    }
  return order;
}

inline void interleave9 (std::array<unsigned char, 206> const& input,
                         std::array<unsigned char, 206>& output)
{
  static std::array<int, 206> const order = interleave9_order ();
  for (int i = 0; i < 206; ++i)
    {
      output[static_cast<size_t> (order[static_cast<size_t> (i)])] = input[static_cast<size_t> (i)];
    }
}

inline void packbits3 (std::array<unsigned char, kJt9EncodedBits> const& bits,
                       std::array<int, kJt9DataSymbolCount>& symbols)
{
  int k = 0;
  for (int i = 0; i < kJt9DataSymbolCount; ++i)
    {
      int value = 0;
      for (int j = 0; j < 3; ++j)
        {
          value = (value << 1) | bits[static_cast<size_t> (k++)];
        }
      symbols[static_cast<size_t> (i)] = value;
    }
}

inline int hash15 (QByteArray const& string, int length = -1)
{
  int const hash_length = (length >= 0 && length <= string.size ()) ? length : string.size ();
  return static_cast<int> (nhash (string.constData (),
                                  static_cast<size_t> (hash_length),
                                  static_cast<std::uint32_t> (146)) & 32767U);
}

inline signed char to_signed_byte (int value)
{
  value &= 255;
  if (value > 127)
    {
      value -= 256;
    }
  return static_cast<signed char> (value);
}

inline std::array<signed char, 11> pack50 (int n1, int n2)
{
  std::array<signed char, 11> dat {};
  dat[0] = to_signed_byte ((n1 >> 20) & 255);
  dat[1] = to_signed_byte ((n1 >> 12) & 255);
  dat[2] = to_signed_byte ((n1 >> 4) & 255);
  dat[3] = to_signed_byte (16 * (n1 & 15) + ((n2 >> 18) & 15));
  dat[4] = to_signed_byte ((n2 >> 10) & 255);
  dat[5] = to_signed_byte ((n2 >> 2) & 255);
  dat[6] = to_signed_byte (64 * (n2 & 3));
  return dat;
}

struct PackPrefixResult
{
  bool text {false};
  int ncall {0};
  int ng {0};
  int nadd {0};
};

inline PackPrefixResult packpfx (QByteArray const& call1_in)
{
  PackPrefixResult result;
  QByteArray call1 = fixed_ascii (call1_in, 12);
  int const islash = call1.indexOf ('/');
  if (islash < 1)
    {
      result.text = true;
      return result;
    }

  auto const pack_call0 = [&] (QByteArray const& call0_in) {
    PackCallResult const packed = packcall (fixed_ascii (call0_in, 12));
    result.text = packed.text;
    result.ncall = packed.ncall;
  };

  if (islash + 2 < call1.size () && call1.at (islash + 2) == ' ')
    {
      pack_call0 (call1.left (islash));
      int const nc = static_cast<unsigned char> (call1.at (islash + 1));
      int n = 38;
      if (nc >= '0' && nc <= '9')
        {
          n = nc - '0';
        }
      else if (nc >= 'A' && nc <= 'Z')
        {
          n = nc - 'A' + 10;
        }
      result.nadd = 1;
      result.ng = 60000 - 32768 + n;
      return result;
    }

  if (islash + 3 < call1.size () && call1.at (islash + 3) == ' ')
    {
      pack_call0 (call1.left (islash));
      int const tens = (islash + 1 < call1.size () && call1.at (islash + 1) >= '0' && call1.at (islash + 1) <= '9')
          ? call1.at (islash + 1) - '0'
          : 0;
      int const ones = (islash + 2 < call1.size () && call1.at (islash + 2) >= '0' && call1.at (islash + 2) <= '9')
          ? call1.at (islash + 2) - '0'
          : 0;
      result.nadd = 1;
      result.ng = 60000 + 26 + 10 * tens + ones;
      return result;
    }

  QByteArray pfx = fixed_ascii (call1.left (islash), 3);
  if (pfx.at (2) == ' ')
    {
      pfx = QByteArray (" ", 1) + pfx.left (2);
    }
  if (pfx.at (2) == ' ')
    {
      pfx = QByteArray (" ", 1) + pfx.left (2);
    }

  pack_call0 (call1.mid (islash + 1));
  for (int i = 0; i < 3; ++i)
    {
      int const nc = static_cast<unsigned char> (pfx.at (i));
      int n = 36;
      if (nc >= '0' && nc <= '9')
        {
          n = nc - '0';
        }
      else if (nc >= 'A' && nc <= 'Z')
        {
          n = nc - 'A' + 10;
        }
      result.ng = 37 * result.ng + n;
    }
  if (result.ng >= 32768)
    {
      result.ng -= 32768;
      result.nadd = 1;
    }
  return result;
}

inline int normalize_wspr_dbm (QByteArray const& dbm_text)
{
  bool ok = false;
  int ndbm = QString::fromLatin1 (dbm_text).trimmed ().toInt (&ok);
  if (!ok)
    {
      ndbm = 0;
    }
  ndbm = std::max (0, std::min (60, ndbm));
  ndbm += kWsprNu[static_cast<size_t> (ndbm % 10)];
  return ndbm;
}

struct WsprSourceEncoding
{
  bool ok {false};
  int ntype {0};
  std::array<signed char, 11> data {};
};

inline WsprSourceEncoding encode_wspr_source (QByteArray const& msg_in)
{
  WsprSourceEncoding result;
  QByteArray msg = fixed_ascii (msg_in, 22);
  int const i1 = msg.indexOf (' ');
  int const i2 = msg.indexOf ('/');
  int const i3 = msg.indexOf ('<');

  if (i1 >= 2 && i1 <= 6 && i2 < 0 && i3 < 0)
    {
      PackCallResult const packed_call = packcall (fixed_ascii (msg.left (i1), 12));
      PackGridResult const packed_grid = packgrid (msg.mid (i1 + 1, 4));
      if (!packed_call.text && !packed_grid.text)
        {
          int const ndbm = normalize_wspr_dbm (msg.mid (i1 + 5));
          result.ok = true;
          result.ntype = ndbm;
          result.data = pack50 (packed_call.ncall, 128 * packed_grid.ng + (ndbm + 64));
          return result;
        }
    }

  if (i2 >= 1 && i3 < 0 && i1 >= 0)
    {
      PackPrefixResult const packed = packpfx (fixed_ascii (msg.left (i1), 12));
      if (!packed.text)
        {
          int const ndbm = normalize_wspr_dbm (msg.mid (i1 + 1));
          result.ok = true;
          result.ntype = ndbm + 1 + packed.nadd;
          result.data = pack50 (packed.ncall, 128 * packed.ng + result.ntype + 64);
          return result;
        }
    }

  if (i3 == 0 && i1 >= 0)
    {
      int const i4 = msg.indexOf ('>');
      if (i4 > 1)
        {
          QByteArray const call1 = msg.mid (1, i4 - 1);
          QByteArray const grid_and_dbm = msg.mid (i1 + 1).trimmed ();
          int const i5 = grid_and_dbm.indexOf (' ');
          if (i5 > 0)
            {
              QByteArray call2 = msg.mid (i1 + 2, i5 - 1) + msg.mid (i1 + 1, 1);
              PackCallResult const packed_call = packcall (fixed_ascii (call2, 12));
              if (!packed_call.text)
                {
                  int const ndbm = normalize_wspr_dbm (msg.mid (i1 + i5 + 2));
                  int const ih = hash15 (call1, i4 - 1);
                  result.ok = true;
                  result.ntype = -(ndbm + 1);
                  result.data = pack50 (packed_call.ncall, 128 * ih + result.ntype + 64);
                  return result;
                }
            }
        }
    }

  return result;
}

inline std::array<int, kWsprToneCount> interleave_wspr_order ()
{
  std::array<int, kWsprToneCount> order {};
  int k = 0;
  for (int i = 0; i < 256 && k < kWsprToneCount; ++i)
    {
      int n = 0;
      int ii = i;
      for (int j = 0; j < 8; ++j)
        {
          n += n;
          if ((ii & 1) != 0)
            {
              ++n;
            }
          ii >>= 1;
        }
      if (n <= 161)
        {
          order[static_cast<size_t> (k++)] = n;
        }
    }
  return order;
}

inline void interleave_wspr (std::array<unsigned char, kWsprToneCount> const& input,
                             std::array<unsigned char, kWsprToneCount>& output)
{
  static std::array<int, kWsprToneCount> const order = interleave_wspr_order ();
  for (int i = 0; i < kWsprToneCount; ++i)
    {
      output[static_cast<size_t> (order[static_cast<size_t> (i)])] = input[static_cast<size_t> (i)];
    }
}

}  // namespace detail

inline txmsg::EncodedMessage encodeJt65 (QString const& message, bool check_only = false)
{
  txmsg::EncodedMessage encoded;
  encoded.msgbits = QByteArray (77, '\0');

  QByteArray msg22 = detail::fixed_ascii (message.left (22).toLatin1 (), 22);
  if (!msg22.isEmpty () && msg22.at (0) == '@')
    {
      bool ok = false;
      int tone = QString::fromLatin1 (msg22.mid (1, 4)).toInt (&ok);
      if (!ok)
        {
          tone = 1000;
        }

      encoded.ok = true;
      encoded.messageType = 1;
      encoded.msgsent = detail::fixed_ascii (msg22, 37);
      if (!check_only)
        {
          encoded.tones = QVector<int> (detail::kJt65ToneCount, 0);
          encoded.tones[0] = tone;
        }
      return encoded;
    }

  while (!msg22.isEmpty () && msg22.at (0) == ' ')
    {
      msg22.remove (0, 1);
      msg22 = detail::fixed_ascii (msg22, 22);
    }

  detail::CheckMessageResult const checked = detail::chkmsg (msg22);
  if (checked.nspecial == 0)
    {
      detail::PackedJtMessage const packed = detail::packmsg (checked.message);
      QByteArray msgsent22 = detail::unpackmsg (packed.dat);
      if (checked.cok != QByteArray ("   ", 3))
        {
          msgsent22[19] = checked.cok.at (0);
          msgsent22[20] = checked.cok.at (1);
          msgsent22[21] = checked.cok.at (2);
          msgsent22 = detail::fmtmsg (msgsent22);
        }

      encoded.ok = true;
      encoded.messageType = packed.itype;
      encoded.msgsent = detail::fixed_ascii (msgsent22, 37);
      if (!check_only)
        {
          int dgen[13] {};
          int sent_raw[63] {};
          for (int i = 0; i < 12; ++i)
            {
              dgen[i] = packed.dat[static_cast<size_t> (i)];
            }
          rs_encode_ (dgen, sent_raw);

          std::array<int, detail::kJt65DataSymbolCount> sent {};
          for (int i = 0; i < detail::kJt65DataSymbolCount; ++i)
            {
              sent[static_cast<size_t> (i)] = sent_raw[i];
            }
          detail::interleave63 (sent);
          for (int i = 0; i < detail::kJt65DataSymbolCount; ++i)
            {
              sent[static_cast<size_t> (i)] = detail::gray_encode (sent[static_cast<size_t> (i)]);
            }

          int const ntest = checked.flip ? 1 : 0;
          encoded.tones = QVector<int> (detail::kJt65ToneCount, 0);
          int k = 0;
          for (int j = 0; j < detail::kJt65ToneCount; ++j)
            {
              if (detail::kJt65SyncMask[static_cast<size_t> (j)] == ntest)
                {
                  encoded.tones[j] = sent[static_cast<size_t> (k)] + 2;
                  ++k;
                }
            }
        }
      return encoded;
    }

  encoded.ok = true;
  encoded.messageType = 7;
  encoded.msgsent = detail::fixed_ascii (checked.message, 37);
  if (!check_only)
    {
      encoded.tones = QVector<int> (detail::kJt65ToneCount, 0);
      int k = 0;
      for (int j = 1; j <= 32 && k < detail::kJt65ToneCount; ++j)
        {
          for (int n = 0; n < 4 && k < detail::kJt65ToneCount; ++n)
            {
              encoded.tones[k] = (j & 1) ? 0 : 10 * checked.nspecial;
              ++k;
            }
        }
  }
  return encoded;
}

inline txmsg::EncodedMessage encodeJt9 (QString const& message, bool check_only = false)
{
  txmsg::EncodedMessage encoded;
  encoded.msgbits = QByteArray (77, '\0');

  QByteArray msg22 = detail::fixed_ascii (message.left (22).toLatin1 (), 22);
  if (!msg22.isEmpty () && msg22.at (0) == '@')
    {
      bool ok = false;
      int tone = QString::fromLatin1 (msg22.mid (1, 4)).toInt (&ok);
      if (!ok)
        {
          tone = 1000;
        }

      encoded.ok = true;
      encoded.messageType = 1;
      encoded.msgsent = detail::fixed_ascii (msg22, 37);
      if (!check_only)
        {
          encoded.tones = QVector<int> (detail::kJt9ToneCount, 0);
          encoded.tones[0] = tone;
        }
      return encoded;
    }

  while (!msg22.isEmpty () && msg22.at (0) == ' ')
    {
      msg22.remove (0, 1);
      msg22 = detail::fixed_ascii (msg22, 22);
    }

  detail::PackedJtMessage const packed = detail::packmsg (msg22);
  QByteArray const msgsent22 = detail::unpackmsg (packed.dat);

  encoded.ok = true;
  encoded.messageType = packed.itype;
  encoded.msgsent = detail::fixed_ascii (msgsent22, 37);
  if (!check_only)
    {
      std::array<signed char, 13> tail_bytes {};
      std::array<unsigned char, 206> encoded_bits {};
      std::array<unsigned char, 206> scrambled_bits206 {};
      std::array<unsigned char, detail::kJt9EncodedBits> scrambled_bits {};
      std::array<int, detail::kJt9DataSymbolCount> data_symbols {};
      std::array<int, detail::kJt9DataSymbolCount> gray_symbols {};

      detail::entail (packed.dat, tail_bytes);
      detail::encode232 (tail_bytes, encoded_bits);
      detail::interleave9 (encoded_bits, scrambled_bits206);
      scrambled_bits.fill (0);
      for (int i = 0; i < 206; ++i)
        {
          scrambled_bits[static_cast<size_t> (i)] = scrambled_bits206[static_cast<size_t> (i)];
        }
      scrambled_bits[206] = 0;
      detail::packbits3 (scrambled_bits, data_symbols);
      for (int i = 0; i < detail::kJt9DataSymbolCount; ++i)
        {
          gray_symbols[static_cast<size_t> (i)] = detail::gray_encode (data_symbols[static_cast<size_t> (i)]);
        }

      encoded.tones = QVector<int> (detail::kJt9ToneCount, 0);
      int j = 0;
      for (int i = 0; i < detail::kJt9ToneCount; ++i)
        {
          if (detail::kJt9SyncMask[static_cast<size_t> (i)] == 1)
            {
              encoded.tones[i] = 0;
            }
          else
            {
              encoded.tones[i] = gray_symbols[static_cast<size_t> (j++)] + 1;
            }
        }
  }
  return encoded;
}

inline txmsg::EncodedMessage encodeJt4 (QString const& message, bool check_only = false)
{
  txmsg::EncodedMessage encoded;
  encoded.msgbits = QByteArray (77, '\0');

  QByteArray msg22 = detail::fixed_ascii (message.left (22).toLatin1 (), 22);
  if (!msg22.isEmpty () && msg22.at (0) == '@')
    {
      bool ok = false;
      int tone = QString::fromLatin1 (msg22.mid (1, 4)).toInt (&ok);
      if (!ok)
        {
          tone = 1000;
        }

      encoded.ok = true;
      encoded.messageType = 1;
      encoded.msgsent = detail::fixed_ascii (msg22, 37);
      if (!check_only)
        {
          encoded.tones = QVector<int> (detail::kJt4ToneCount, 0);
          encoded.tones[0] = tone;
        }
      return encoded;
    }

  QByteArray const normalized = detail::fmtmsg (msg22);
  detail::PackedJtMessage const packed = detail::packmsg (normalized);
  QByteArray const msgsent22 = detail::unpackmsg (packed.dat);

  encoded.ok = true;
  encoded.messageType = packed.itype;
  encoded.msgsent = detail::fixed_ascii (msgsent22, 37);
  if (!check_only)
    {
      std::array<signed char, 13> tail_bytes {};
      std::array<unsigned char, 206> encoded_bits {};
      std::array<unsigned char, 206> interleaved_bits {};

      detail::entail (packed.dat, tail_bytes);
      detail::encode232 (tail_bytes, encoded_bits);
      detail::interleave9 (encoded_bits, interleaved_bits);

      encoded.tones = QVector<int> (detail::kJt4ToneCount, 0);
      int dash_index = normalized.indexOf ('-');
      bool const invert_sync =
          dash_index >= 8
          && dash_index + 1 < normalized.size ()
          && normalized.at (dash_index + 1) >= '0'
          && normalized.at (dash_index + 1) <= '3';
      for (int i = 0; i < detail::kJt4ToneCount; ++i)
        {
          int const sync = invert_sync
              ? (1 - detail::kJt4SyncPattern[static_cast<size_t> (i + 1)])
              : detail::kJt4SyncPattern[static_cast<size_t> (i + 1)];
          encoded.tones[i] = 2 * interleaved_bits[static_cast<size_t> (i)] + sync;
        }
    }
  return encoded;
}

inline txmsg::EncodedMessage encodeWspr (QString const& message, bool check_only = false)
{
  txmsg::EncodedMessage encoded;
  encoded.msgbits = QByteArray (77, '\0');

  QByteArray const msg22 = detail::fixed_ascii (message.left (22).toLatin1 (), 22);
  detail::WsprSourceEncoding const source = detail::encode_wspr_source (msg22);

  encoded.ok = source.ok;
  encoded.messageType = source.ntype;
  encoded.msgsent = detail::fixed_ascii (msg22, 37);
  if (!encoded.ok || check_only)
    {
      return encoded;
    }

  std::array<signed char, 13> tail_bytes {};
  std::copy_n (source.data.begin (), source.data.size (), tail_bytes.begin ());

  std::array<unsigned char, 206> encoded_bits206 {};
  std::array<unsigned char, detail::kWsprToneCount> encoded_bits {};
  std::array<unsigned char, detail::kWsprToneCount> interleaved_bits {};

  detail::encode232 (tail_bytes, encoded_bits206);
  for (int i = 0; i < detail::kWsprToneCount; ++i)
    {
      encoded_bits[static_cast<size_t> (i)] = encoded_bits206[static_cast<size_t> (i)];
    }
  detail::interleave_wspr (encoded_bits, interleaved_bits);

  encoded.tones = QVector<int> (detail::kWsprToneCount, 0);
  for (int i = 0; i < detail::kWsprToneCount; ++i)
    {
      encoded.tones[i] = detail::kWsprSyncMask[static_cast<size_t> (i)]
          + 2 * interleaved_bits[static_cast<size_t> (i)];
    }
  return encoded;
}

}  // namespace legacy_jt
}  // namespace decodium

#endif // LEGACY_JT_ENCODER_HPP
