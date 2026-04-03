#include "FtxMessageEncoder.hpp"
#include "FtxFst4LdpcData.hpp"
#include "LegacyJtEncoder.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <QStringList>

#include "wsjtx_config.h"

extern "C"
{
short crc13 (unsigned char const * data, int length);
short crc14 (unsigned char const * data, int length);
std::uint32_t nhash (void const* key, size_t length, std::uint32_t initval);
void q65_enc_ (int x[], int y[]);
void legacy_pack77_reset_context_c ();
void legacy_pack77_pack_c (char const msg0[37], int* i3, int* n3,
                           char c77[77], char msgsent[37], bool* success, int received);
void legacy_pack77_unpack_c (char const c77[77], int received,
                             char msgsent[37], bool* success);
void legacy_pack77_save_hash_call_c (char const c13[13], int* n10, int* n12, int* n22);
int legacy_pack77_hash_call_bits_c (char const c13[13], int bits, int* hash_out);
void legacy_pack77_pack28_c (char const c13[13], int* n28, bool* success);
void legacy_pack77_unpack28_c (int n28, char c13[13], bool* success);
void legacy_pack77_split77_c (char const msg[37], int* nwords, int nw[19], char words[247]);
void legacy_pack77_packtext77_c (char const c13[13], char c71[71], bool* success);
void legacy_pack77_unpacktext77_c (char const c71[71], char c13[13], bool* success);
}

namespace
{
using MessageBits77 = std::array<unsigned char, 77>;
using MessageBits74 = std::array<unsigned char, 74>;
using MessageBits101 = std::array<unsigned char, 101>;
using MessageBits90 = std::array<unsigned char, 90>;
using Codeword174 = std::array<unsigned char, 174>;
using Codeword128 = std::array<unsigned char, 128>;
using Codeword240 = std::array<unsigned char, 240>;
using GeneratorRow = std::array<unsigned char, 91>;
using GeneratorMatrix = std::array<GeneratorRow, 83>;
using GeneratorRow128_90 = std::array<unsigned char, 90>;
using GeneratorMatrix128_90 = std::array<GeneratorRow128_90, 38>;
using GeneratorRow240_101 = std::array<unsigned char, 101>;
using GeneratorMatrix240_101 = std::array<GeneratorRow240_101, 139>;
using GeneratorRow240_74 = std::array<unsigned char, 74>;
using GeneratorMatrix240_74 = std::array<GeneratorRow240_74, 166>;
using Uint71 = std::array<unsigned char, 9>;

constexpr int kNumTokens = 2063592;
constexpr int kMax22 = 4194304;
constexpr int kMaxGrid4 = 32400;
constexpr int kQ65PayloadSymbols = 13;
constexpr int kQ65CodewordSymbols = 63;
constexpr int kQ65ChannelSymbols = 85;

std::array<int, 22> const kQ65SyncPositions = {{
  1, 9, 12, 13, 15, 22, 23, 26, 27, 33, 35, 38, 46, 50, 55, 60, 62, 66, 69, 74, 76, 85
}};

std::array<int, 8> const kFst4SyncWord1 = {{0, 1, 3, 2, 1, 0, 2, 3}};
std::array<int, 8> const kFst4SyncWord2 = {{2, 3, 1, 0, 3, 2, 0, 1}};

std::array<char const*, 83> const kLdpcGeneratorHex = {{
  "8329ce11bf31eaf509f27fc", "761c264e25c259335493132", "dc265902fb277c6410a1bdc",
  "1b3f417858cd2dd33ec7f62", "09fda4fee04195fd034783a", "077cccc11b8873ed5c3d48a",
  "29b62afe3ca036f4fe1a9da", "6054faf5f35d96d3b0c8c3e", "e20798e4310eed27884ae90",
  "775c9c08e80e26ddae56318", "b0b811028c2bf997213487c", "18a0c9231fc60adf5c5ea32",
  "76471e8302a0721e01b12b8", "ffbccb80ca8341fafb47b2e", "66a72a158f9325a2bf67170",
  "c4243689fe85b1c51363a18", "0dff739414d1a1b34b1c270", "15b48830636c8b99894972e",
  "29a89c0d3de81d665489b0e", "4f126f37fa51cbe61bd6b94", "99c47239d0d97d3c84e0940",
  "1919b75119765621bb4f1e8", "09db12d731faee0b86df6b8", "488fc33df43fbdeea4eafb4",
  "827423ee40b675f756eb5fe", "abe197c484cb74757144a9a", "2b500e4bc0ec5a6d2bdbdd0",
  "c474aa53d70218761669360", "8eba1a13db3390bd6718cec", "753844673a27782cc42012e",
  "06ff83a145c37035a5c1268", "3b37417858cc2dd33ec3f62", "9a4a5a28ee17ca9c324842c",
  "bc29f465309c977e89610a4", "2663ae6ddf8b5ce2bb29488", "46f231efe457034c1814418",
  "3fb2ce85abe9b0c72e06fbe", "de87481f282c153971a0a2e", "fcd7ccf23c69fa99bba1412",
  "f0261447e9490ca8e474cec", "4410115818196f95cdd7012", "088fc31df4bfbde2a4eafb4",
  "b8fef1b6307729fb0a078c0", "5afea7acccb77bbc9d99a90", "49a7016ac653f65ecdc9076",
  "1944d085be4e7da8d6cc7d0", "251f62adc4032f0ee714002", "56471f8702a0721e00b12b8",
  "2b8e4923f2dd51e2d537fa0", "6b550a40a66f4755de95c26", "a18ad28d4e27fe92a4f6c84",
  "10c2e586388cb82a3d80758", "ef34a41817ee02133db2eb0", "7e9c0c54325a9c15836e000",
  "3693e572d1fde4cdf079e86", "bfb2cec5abe1b0c72e07fbe", "7ee18230c583cccc57d4b08",
  "a066cb2fedafc9f52664126", "bb23725abc47cc5f4cc4cd2", "ded9dba3bee40c59b5609b4",
  "d9a7016ac653e6decdc9036", "9ad46aed5f707f280ab5fc4", "e5921c77822587316d7d3c2",
  "4f14da8242a8b86dca73352", "8b8b507ad467d4441df770e", "22831c9cf1169467ad04b68",
  "213b838fe2ae54c38ee7180", "5d926b6dd71f085181a4e12", "66ab79d4b29ee6e69509e56",
  "958148682d748a38dd68baa", "b8ce020cf069c32a723ab14", "f4331d6d461607e95752746",
  "6da23ba424b9596133cf9c8", "a636bcbc7b30c5fbeae67fe", "5cb0d86a07df654a9089a20",
  "f11f106848780fc9ecdd80a", "1fbb5364fb8d2c9d730d5ba", "fcb86bc70a50c9d02a5d034",
  "a534433029eac15f322e34c", "c989d9c7c3d3b8c55d75130", "7bb38b2f0186d46643ae962",
  "2644ebadeb44b9467d1f42c", "608cc857594bfbb55d69600"
}};

std::array<char const*, 38> const kMsk144LdpcGeneratorHex = {{
  "a08ea80879050a5e94da994", "59f3b48040ca089c81ee880", "e4070262802e31b7b17d3dc",
  "95cbcbaf032dc3d960bacc8", "c4d79b5dcc21161a254ffbc", "93fde9cdbf2622a70868424",
  "e73b888bb1b01167379ba28", "45a0d0a0f39a7ad2439949c", "759acef19444bcad79c4964",
  "71eb4dddf4f5ed9e2ea17e0", "80f0ad76fb247d6b4ca8d38", "184fff3aa1b82dc66640104",
  "ca4e320bb382ed14cbb1094", "52514447b90e25b9e459e28", "dd10c1666e071956bd0df38",
  "99c332a0b792a2da8ef1ba8", "7bd9f688e7ed402e231aaac", "00fcad76eb647d6a0ca8c38",
  "6ac8d0499c43b02eed78d70", "2c2c764baf795b4788db010", "0e907bf9e280d2624823dd0",
  "b857a6e315afd8c1c925e64", "8deb58e22d73a141cae3778", "22d3cb80d92d6ac132dfe08",
  "754763877b28c187746855c", "1d1bb7cf6953732e04ebca4", "2c65e0ea4466ab9f5e1deec",
  "6dc530ca37fc916d1f84870", "49bccbbee152355be7ac984", "e8387f3f4367cf45a150448",
  "8ce25e03d67d51091c81884", "b798012ffa40a93852752c8", "2e43307933adfca37adc3c8",
  "ca06e0a42ca1ec782d6c06c", "c02b762927556a7039e638c", "4a3e9b7d08b6807f8619fac",
  "45e8030f68997bb68544424", "7e79362c16773efc6482e30"
}};

std::array<unsigned short, 16> const kMsk40GeneratorHex = {{
  0x4428, 0x5a6b, 0x1b04, 0x2c12, 0x60c4, 0x1071, 0xbe6a, 0x36dd,
  0xc580, 0xad9a, 0xeca2, 0x7843, 0x332e, 0xa685, 0x5906, 0x1efe
}};

std::array<int, 32> const kMsk40ColumnOrder = {{
  4, 1, 2, 3, 0, 8, 6, 10, 13, 28, 20, 23, 17, 15, 27, 25,
  16, 12, 18, 19, 7, 21, 22, 11, 24, 5, 26, 14, 9, 29, 30, 31
}};

std::array<unsigned char, 77> const kFt2Ft4Rvec = {{
  0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0,
  1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1,
  0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1
}};

GeneratorMatrix const& generator_matrix ()
{
  static GeneratorMatrix matrix = [] {
    GeneratorMatrix result {};
    for (size_t row = 0; row < kLdpcGeneratorHex.size (); ++row)
      {
        for (int nibble_index = 0; nibble_index < 23; ++nibble_index)
          {
            char const ch = kLdpcGeneratorHex[row][nibble_index];
            unsigned value = 0;
            if (ch >= '0' && ch <= '9') value = static_cast<unsigned> (ch - '0');
            else if (ch >= 'a' && ch <= 'f') value = static_cast<unsigned> (10 + ch - 'a');
            else if (ch >= 'A' && ch <= 'F') value = static_cast<unsigned> (10 + ch - 'A');
            int const max_bits = (nibble_index == 22) ? 3 : 4;
            for (int bit_index = 0; bit_index < max_bits; ++bit_index)
              {
                int const column = nibble_index * 4 + bit_index;
                if (value & (1u << (3 - bit_index)))
                  {
                    result[row][column] = 1u;
                  }
              }
          }
      }
    return result;
  }();
  return matrix;
}

GeneratorMatrix128_90 const& msk144_generator_matrix ()
{
  static GeneratorMatrix128_90 matrix = [] {
    GeneratorMatrix128_90 result {};
    for (size_t row = 0; row < kMsk144LdpcGeneratorHex.size (); ++row)
      {
        for (int nibble_index = 0; nibble_index < 23; ++nibble_index)
          {
            char const ch = kMsk144LdpcGeneratorHex[row][nibble_index];
            unsigned value = 0;
            if (ch >= '0' && ch <= '9') value = static_cast<unsigned> (ch - '0');
            else if (ch >= 'a' && ch <= 'f') value = static_cast<unsigned> (10 + ch - 'a');
            else if (ch >= 'A' && ch <= 'F') value = static_cast<unsigned> (10 + ch - 'A');
            int const max_bits = (nibble_index == 22) ? 2 : 4;
            for (int bit_index = 0; bit_index < max_bits; ++bit_index)
              {
                int const column = nibble_index * 4 + bit_index;
                if (value & (1u << (3 - bit_index)))
                  {
                    result[row][column] = 1u;
                  }
              }
          }
      }
    return result;
  }();
  return matrix;
}

template <typename Matrix, size_t Rows>
Matrix build_hex_generator_matrix (std::array<char const*, Rows> const& hex_rows, int tail_bits)
{
  Matrix result {};
  for (size_t row = 0; row < hex_rows.size (); ++row)
    {
      int const nibble_count = static_cast<int> (std::strlen (hex_rows[row]));
      for (int nibble_index = 0; nibble_index < nibble_count; ++nibble_index)
        {
          char const ch = hex_rows[row][nibble_index];
          unsigned value = 0;
          if (ch >= '0' && ch <= '9') value = static_cast<unsigned> (ch - '0');
          else if (ch >= 'a' && ch <= 'f') value = static_cast<unsigned> (10 + ch - 'a');
          else if (ch >= 'A' && ch <= 'F') value = static_cast<unsigned> (10 + ch - 'A');
          int const max_bits = (nibble_index == nibble_count - 1) ? tail_bits : 4;
          for (int bit_index = 0; bit_index < max_bits; ++bit_index)
            {
              int const column = nibble_index * 4 + bit_index;
              if (value & (1u << (3 - bit_index)))
                {
                  result[row][column] = 1u;
                }
            }
        }
    }
  return result;
}

GeneratorMatrix240_101 const& fst4_generator_matrix_240_101 ()
{
  static GeneratorMatrix240_101 matrix =
      build_hex_generator_matrix<GeneratorMatrix240_101> (kFst4Ldpc240_101Hex, 1);
  return matrix;
}

GeneratorMatrix240_74 const& fst4_generator_matrix_240_74 ()
{
  static GeneratorMatrix240_74 matrix =
      build_hex_generator_matrix<GeneratorMatrix240_74> (kFst4Ldpc240_74Hex, 2);
  return matrix;
}

QByteArray trim_or_pad_37 (QString const& message, bool strip_leading_blanks)
{
  QByteArray latin = message.left (37).toLatin1 ();
  if (strip_leading_blanks)
    {
      while (!latin.isEmpty () && latin.front () == ' ')
        {
          latin.remove (0, 1);
        }
    }
  if (latin.size () < 37)
    {
      latin.append (QByteArray (37 - latin.size (), ' '));
    }
  else if (latin.size () > 37)
    {
      latin.truncate (37);
    }
  return latin;
}

int parse_fixed_bits (char const* bits, int offset, int count)
{
  int value = 0;
  for (int i = 0; i < count; ++i)
    {
      value <<= 1;
      if (bits[offset + i] == '1')
        {
          value |= 1;
        }
    }
  return value;
}

bool encode_q65_message_cpp (QByteArray const& fixed_message, QByteArray* msgsent_out,
                             std::array<int, kQ65PayloadSymbols>* payload_out,
                             std::array<int, kQ65CodewordSymbols>* codeword_out,
                             std::array<int, kQ65ChannelSymbols>* tones_out)
{
  if (!msgsent_out || !payload_out || !codeword_out || !tones_out)
    {
      return false;
    }

  msgsent_out->fill (' ', 37);
  payload_out->fill (0);
  codeword_out->fill (0);
  tones_out->fill (0);

  QByteArray msg37 = fixed_message.left (37);
  if (msg37.size () < 37)
    {
      msg37.append (QByteArray (37 - msg37.size (), ' '));
    }

  char c77[77] {};
  char msgsent[37] {};
  bool pack_ok = false;
  int i3 = -1;
  int n3 = -1;
  legacy_pack77_pack_c (msg37.constData (), &i3, &n3, c77, msgsent, &pack_ok, 0);
  if (!pack_ok)
    {
      return false;
    }

  if (parse_fixed_bits (c77, 59, 15) == 32373)
    {
      std::memcpy (c77 + 59, "111111010010011", 15);
    }

  for (int i = 0; i < 12; ++i)
    {
      (*payload_out)[static_cast<size_t> (i)] = parse_fixed_bits (c77, i * 6, 6);
    }
  (*payload_out)[12] = 2 * parse_fixed_bits (c77, 72, 5);

  int payload_raw[kQ65PayloadSymbols] {};
  int codeword_raw[kQ65CodewordSymbols] {};
  for (int i = 0; i < kQ65PayloadSymbols; ++i)
    {
      payload_raw[i] = (*payload_out)[static_cast<size_t> (i)];
    }
  q65_enc_ (payload_raw, codeword_raw);
  for (int i = 0; i < kQ65CodewordSymbols; ++i)
    {
      (*codeword_out)[static_cast<size_t> (i)] = codeword_raw[i];
    }

  int sync_index = 0;
  int codeword_index = 0;
  for (int symbol = 1; symbol <= kQ65ChannelSymbols; ++symbol)
    {
      if (sync_index < static_cast<int> (kQ65SyncPositions.size ())
          && symbol == kQ65SyncPositions[static_cast<size_t> (sync_index)])
        {
          (*tones_out)[static_cast<size_t> (symbol - 1)] = 0;
          ++sync_index;
        }
      else if (codeword_index < kQ65CodewordSymbols)
        {
          (*tones_out)[static_cast<size_t> (symbol - 1)] =
              (*codeword_out)[static_cast<size_t> (codeword_index)] + 1;
          ++codeword_index;
        }
    }

  *msgsent_out = QByteArray (msgsent, 37);
  return true;
}

struct PackedMessage
{
  bool ok {false};
  QByteArray msgsent;
  MessageBits77 bits {};
  int i3 {-1};
  int n3 {-1};
};

template <typename T>
struct Maybe
{
  Maybe () : ok (false), value () {}
  Maybe (bool has_value, T const& current) : ok (has_value), value (current) {}
  bool ok;
  T value;
};

QStringList const& field_day_sections ();
QStringList const& arrl_rtty_multipliers ();
Maybe<QString> unpack_wspr_prefix_suffix_cpp (int npfx, QString const& call);

QByteArray to_fixed_37 (QString const& text)
{
  QByteArray latin = text.left (37).toLatin1 ();
  if (latin.size () < 37)
    {
      latin.append (QByteArray (37 - latin.size (), ' '));
    }
  return latin;
}

bool is_ascii_digit (QChar const c)
{
  return c >= QLatin1Char ('0') && c <= QLatin1Char ('9');
}

bool is_ascii_letter (QChar const c)
{
  return c >= QLatin1Char ('A') && c <= QLatin1Char ('Z');
}

QString normalize_message77 (QString const& message)
{
  QString normalized;
  normalized.reserve (37);
  bool previous_blank = true;
  for (QChar c : message.left (37))
    {
      if (c.unicode () == 0)
        {
          c = QLatin1Char (' ');
        }
      c = c.toUpper ();
      if (c == QLatin1Char (' ') && previous_blank)
        {
          continue;
        }
      normalized.append (c);
      previous_blank = (c == QLatin1Char (' '));
    }
  while (!normalized.isEmpty () && normalized.back () == QLatin1Char (' '))
    {
      normalized.chop (1);
    }
  return normalized;
}

QString normalize_hash_callsign (QString const& call)
{
  QString normalized = call.trimmed ().toUpper ();
  if (normalized.startsWith (QLatin1Char ('<')))
    {
      int const end = normalized.indexOf (QLatin1Char ('>'));
      if (end > 0)
        {
          normalized = normalized.mid (1, end - 1);
        }
    }
  int const strayEnd = normalized.indexOf (QLatin1Char ('>'));
  if (strayEnd > 0)
    {
      normalized = normalized.left (strayEnd);
    }
  return normalized.left (13).trimmed ();
}

QString bracket_hash_call (QString const& call)
{
  QString const normalized = normalize_hash_callsign (call);
  if (normalized.isEmpty ())
    {
      return QStringLiteral ("<...>");
    }
  return QStringLiteral ("<%1>").arg (normalized);
}

struct CheckedCall
{
  bool ok {false};
  QString base;
};

CheckedCall check_call (QString const& word)
{
  CheckedCall checked;
  QString w = word.trimmed ().toUpper ();
  if (w.size () > 11 || w.contains (QLatin1Char ('.')) || w.contains (QLatin1Char ('+'))
      || w.contains (QLatin1Char ('-')) || w.contains (QLatin1Char ('?'))
      || (w.size () > 6 && !w.contains (QLatin1Char ('/'))))
    {
      return checked;
    }

  QString base = w;
  int const slash = w.indexOf (QLatin1Char ('/'));
  if (slash >= 1 && slash <= w.size () - 2)
    {
      int const left = slash;
      int const right = w.size () - slash - 1;
      if (std::max (left, right) > 6)
        {
          return checked;
        }
      base = (left <= right) ? w.mid (slash + 1) : w.left (slash);
    }

  if (base.size () > 6)
    {
      return checked;
    }
  if (base.size () < 3)
    {
      return checked;
    }
  if (!is_ascii_letter (base[0]) && !is_ascii_letter (base[1]))
    {
      return checked;
    }
  if (base[0] == QLatin1Char ('Q') && base != QStringLiteral ("QU1RK"))
    {
      return checked;
    }

  int area_index = -1;
  if (base.size () >= 2 && is_ascii_digit (base[1])) area_index = 1;
  if (base.size () >= 3 && is_ascii_digit (base[2])) area_index = 2;
  if (area_index < 0 || area_index == base.size () - 1)
    {
      return checked;
    }

  int suffix_letters = 0;
  for (int i = area_index + 1; i < base.size (); ++i)
    {
      if (!is_ascii_letter (base[i]))
        {
          return checked;
        }
      ++suffix_letters;
    }
  if (suffix_letters < 1 || suffix_letters > 3)
    {
      return checked;
    }

  checked.ok = true;
  checked.base = base;
  return checked;
}

QStringList split77_cpp (QString const& message)
{
  QStringList words = normalize_message77 (message).split (QLatin1Char (' '), Qt::SkipEmptyParts);
  if (words.size () >= 3 && words[0] == QStringLiteral ("CQ") && check_call (words[2]).ok)
    {
      words[0] = QStringLiteral ("CQ_") + words[1];
      words.removeAt (1);
    }
  return words;
}

bool is_grid4 (QString const& grid)
{
  return grid.size () >= 4
      && grid[0] >= QLatin1Char ('A') && grid[0] <= QLatin1Char ('R')
      && grid[1] >= QLatin1Char ('A') && grid[1] <= QLatin1Char ('R')
      && grid[2] >= QLatin1Char ('0') && grid[2] <= QLatin1Char ('9')
      && grid[3] >= QLatin1Char ('0') && grid[3] <= QLatin1Char ('9');
}

int pack_grid4 (QString const& grid)
{
  return (grid[0].unicode () - 'A') * 18 * 10 * 10
      + (grid[1].unicode () - 'A') * 10 * 10
      + (grid[2].unicode () - '0') * 10
      + (grid[3].unicode () - '0');
}

QString unpack_grid4 (int value)
{
  QString grid (4, QLatin1Char (' '));
  int j1 = value / (18 * 10 * 10);
  value -= j1 * 18 * 10 * 10;
  int j2 = value / (10 * 10);
  value -= j2 * 10 * 10;
  int j3 = value / 10;
  int j4 = value - j3 * 10;
  grid[0] = QChar ('A' + j1);
  grid[1] = QChar ('A' + j2);
  grid[2] = QChar ('0' + j3);
  grid[3] = QChar ('0' + j4);
  return grid;
}

bool is_grid6_24 (QString const& grid)
{
  return grid.size () >= 6
      && grid[0] >= QLatin1Char ('A') && grid[0] <= QLatin1Char ('R')
      && grid[1] >= QLatin1Char ('A') && grid[1] <= QLatin1Char ('R')
      && grid[2] >= QLatin1Char ('0') && grid[2] <= QLatin1Char ('9')
      && grid[3] >= QLatin1Char ('0') && grid[3] <= QLatin1Char ('9')
      && grid[4] >= QLatin1Char ('A') && grid[4] <= QLatin1Char ('X')
      && grid[5] >= QLatin1Char ('A') && grid[5] <= QLatin1Char ('X');
}

int pack_grid6_24 (QString const& grid)
{
  return (grid[0].unicode () - 'A') * 18 * 10 * 10 * 24 * 24
      + (grid[1].unicode () - 'A') * 10 * 10 * 24 * 24
      + (grid[2].unicode () - '0') * 10 * 24 * 24
      + (grid[3].unicode () - '0') * 24 * 24
      + (grid[4].unicode () - 'A') * 24
      + (grid[5].unicode () - 'A');
}

QString unpack_grid6_24 (int value)
{
  QString grid (6, QLatin1Char (' '));
  int j1 = value / (18 * 10 * 10 * 24 * 24);
  if (j1 < 0 || j1 > 17) return {};
  value -= j1 * 18 * 10 * 10 * 24 * 24;
  int j2 = value / (10 * 10 * 24 * 24);
  if (j2 < 0 || j2 > 17) return {};
  value -= j2 * 10 * 10 * 24 * 24;
  int j3 = value / (10 * 24 * 24);
  if (j3 < 0 || j3 > 9) return {};
  value -= j3 * 10 * 24 * 24;
  int j4 = value / (24 * 24);
  if (j4 < 0 || j4 > 9) return {};
  value -= j4 * 24 * 24;
  int j5 = value / 24;
  if (j5 < 0 || j5 > 23) return {};
  int j6 = value - j5 * 24;
  if (j6 < 0 || j6 > 23) return {};
  grid[0] = QChar ('A' + j1);
  grid[1] = QChar ('A' + j2);
  grid[2] = QChar ('0' + j3);
  grid[3] = QChar ('0' + j4);
  grid[4] = QChar ('A' + j5);
  grid[5] = QChar ('A' + j6);
  return grid;
}

bool is_grid6_25_or4 (QString const& grid)
{
  return grid.size () >= 4
      && grid[0] >= QLatin1Char ('A') && grid[0] <= QLatin1Char ('R')
      && grid[1] >= QLatin1Char ('A') && grid[1] <= QLatin1Char ('R')
      && grid[2] >= QLatin1Char ('0') && grid[2] <= QLatin1Char ('9')
      && grid[3] >= QLatin1Char ('0') && grid[3] <= QLatin1Char ('9')
      && (grid.size () == 4
          || (grid.size () >= 6
              && grid[4] >= QLatin1Char ('A') && grid[4] <= QLatin1Char ('X')
              && grid[5] >= QLatin1Char ('A') && grid[5] <= QLatin1Char ('X')));
}

int pack_grid6_25 (QString const& grid)
{
  int value = (grid[0].unicode () - 'A') * 18 * 10 * 10 * 25 * 25
            + (grid[1].unicode () - 'A') * 10 * 10 * 25 * 25
            + (grid[2].unicode () - '0') * 10 * 25 * 25
            + (grid[3].unicode () - '0') * 25 * 25;
  if (grid.size () >= 6)
    {
      value += (grid[4].unicode () - 'A') * 25 + (grid[5].unicode () - 'A');
    }
  else
    {
      value += 24 * 25 + 24;
    }
  return value;
}

QString unpack_grid6_25 (int value)
{
  QString grid (6, QLatin1Char (' '));
  int j1 = value / (18 * 10 * 10 * 25 * 25);
  if (j1 < 0 || j1 > 17) return {};
  value -= j1 * 18 * 10 * 10 * 25 * 25;
  int j2 = value / (10 * 10 * 25 * 25);
  if (j2 < 0 || j2 > 17) return {};
  value -= j2 * 10 * 10 * 25 * 25;
  int j3 = value / (10 * 25 * 25);
  if (j3 < 0 || j3 > 9) return {};
  value -= j3 * 10 * 25 * 25;
  int j4 = value / (25 * 25);
  if (j4 < 0 || j4 > 9) return {};
  value -= j4 * 25 * 25;
  int j5 = value / 25;
  if (j5 < 0 || j5 > 24) return {};
  int j6 = value - j5 * 25;
  if (j6 < 0 || j6 > 24) return {};
  grid[0] = QChar ('A' + j1);
  grid[1] = QChar ('A' + j2);
  grid[2] = QChar ('0' + j3);
  grid[3] = QChar ('0' + j4);
  if (j5 != 24 || j6 != 24)
    {
      grid[4] = QChar ('A' + j5);
      grid[5] = QChar ('A' + j6);
    }
  return grid.trimmed ();
}

bool is_hex_upper (QChar const c)
{
  return (c >= QLatin1Char ('0') && c <= QLatin1Char ('9'))
      || (c >= QLatin1Char ('A') && c <= QLatin1Char ('F'));
}

int hex_value (QChar const c)
{
  if (c >= QLatin1Char ('0') && c <= QLatin1Char ('9'))
    {
      return c.unicode () - '0';
    }
  if (c >= QLatin1Char ('A') && c <= QLatin1Char ('F'))
    {
      return 10 + c.unicode () - 'A';
    }
  return -1;
}

Maybe<int> ihashcall_cpp (QString const& call, int bits)
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
          return {};
        }
      n8 = 38u * n8 + static_cast<std::uint64_t> (index);
    }

  std::uint64_t const mixed = 47055833459ULL * n8;
  return {true, static_cast<int> (mixed >> (64 - bits))};
}

Maybe<std::uint64_t> pack_base38_11 (QString const& text)
{
  static QString const alphabet = QStringLiteral (" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/");
  QString const padded = text.left (11).rightJustified (11, QLatin1Char (' ')).toUpper ();
  std::uint64_t value = 0;
  for (QChar const c : padded)
    {
      int const index = alphabet.indexOf (c);
      if (index < 0)
        {
          return {};
        }
      value = 38u * value + static_cast<std::uint64_t> (index);
    }
  return {true, value};
}

Maybe<QString> unpack_base38_11 (std::uint64_t value)
{
  static QString const alphabet = QStringLiteral (" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/");
  QString text (11, QLatin1Char (' '));
  for (int i = 10; i >= 0; --i)
    {
      int const index = static_cast<int> (value % 38u);
      if (index < 0 || index >= alphabet.size ())
        {
          return {};
        }
      text[i] = alphabet[index];
      value /= 38u;
    }
  return {true, text.trimmed ()};
}

PackedMessage make_packed_message (MessageBits77 const& bits, int i3, int n3, QByteArray const& msgsent)
{
  PackedMessage packed;
  packed.ok = true;
  packed.bits = bits;
  packed.i3 = i3;
  packed.n3 = n3;
  packed.msgsent = msgsent;
  return packed;
}

Maybe<int> encode_pack28_special (QString const& token)
{
  if (token == QStringLiteral ("DE"))
    {
      return {true, 0};
    }
  if (token == QStringLiteral ("QRZ"))
    {
      return {true, 1};
    }
  if (token == QStringLiteral ("CQ"))
    {
      return {true, 2};
    }
  if (token.startsWith (QStringLiteral ("CQ_")))
    {
      QString suffix = token.mid (3);
      if (suffix.size () >= 1 && suffix.size () <= 4)
        {
          bool digits_only = true;
          bool letters_only = true;
          for (QChar const c : suffix)
            {
              digits_only = digits_only && is_ascii_digit (c);
              letters_only = letters_only && is_ascii_letter (c);
            }
          if (digits_only && suffix.size () == 3)
            {
              bool ok = false;
              int value = suffix.toInt (&ok);
              if (ok)
                {
                  return {true, 3 + value};
                }
            }
          if (letters_only)
            {
              QString padded = suffix.rightJustified (4, QLatin1Char (' '));
              int value = 0;
              for (QChar const c : padded)
                {
                  int j = (c == QLatin1Char (' ')) ? 0 : (c.unicode () - 'A' + 1);
                  value = 27 * value + j;
                }
              return {true, 3 + 1000 + value};
            }
        }
    }
  return {};
}

Maybe<int> pack28_cpp (QString const& token)
{
  QString const trimmed = token.trimmed ().toUpper ();
  Maybe<int> special = encode_pack28_special (trimmed);
  if (special.ok)
    {
      return special;
    }

  bool const bracketed = trimmed.startsWith (QLatin1Char ('<'))
      && trimmed.endsWith (QLatin1Char ('>')) && trimmed.size () >= 3;
  if (bracketed)
    {
      Maybe<int> n22 = ihashcall_cpp (trimmed, 22);
      if (!n22.ok)
        {
          return {};
        }
      return {true, kNumTokens + n22.value};
    }

  if (trimmed.contains (QLatin1Char ('<')) || trimmed.contains (QLatin1Char ('>')))
    {
      return {};
    }

  CheckedCall checked = check_call (trimmed);
  QString callsign = checked.ok ? checked.base : trimmed;
  if (trimmed.startsWith (QStringLiteral ("3DA0")) && trimmed.size () >= 7)
    {
      callsign = QStringLiteral ("3D0") + trimmed.mid (4, 3);
    }
  if (trimmed.startsWith (QStringLiteral ("3X")) && trimmed.size () >= 6 && is_ascii_letter (trimmed[2]))
    {
      callsign = QStringLiteral ("Q") + trimmed.mid (2, 4);
    }

  if (!checked.ok)
    {
      Maybe<int> n22 = ihashcall_cpp (callsign, 22);
      if (!n22.ok)
        {
          return {};
        }
      return {true, kNumTokens + n22.value};
    }

  int area = -1;
  if (callsign.size () >= 2 && is_ascii_digit (callsign[1])) area = 2;
  if (callsign.size () >= 3 && is_ascii_digit (callsign[2])) area = 3;
  if (area < 0)
    {
      return {};
    }

  QString six = (area == 2) ? QStringLiteral (" ") + callsign.left (5) : callsign.leftJustified (6, QLatin1Char (' '));
  static QString const a1 = QStringLiteral (" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  static QString const a2 = QStringLiteral ("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  static QString const a4 = QStringLiteral (" ABCDEFGHIJKLMNOPQRSTUVWXYZ");

  int const i1 = a1.indexOf (six[0]);
  int const i2 = a2.indexOf (six[1]);
  int const i3 = six[2].unicode () - '0';
  int const i4 = a4.indexOf (six[3]);
  int const i5 = a4.indexOf (six[4]);
  int const i6 = a4.indexOf (six[5]);
  if (i1 < 0 || i2 < 0 || i3 < 0 || i3 > 9 || i4 < 0 || i5 < 0 || i6 < 0)
    {
      return {};
    }

  int n28 = 36 * 10 * 27 * 27 * 27 * i1
          + 10 * 27 * 27 * 27 * i2
          + 27 * 27 * 27 * i3
          + 27 * 27 * i4
          + 27 * i5
          + i6;
  return {true, n28 + kNumTokens + kMax22};
}

Maybe<QString> unpack28_cpp (int n28, decodium::txmsg::Decode77Context* context = nullptr)
{
  if (n28 < kNumTokens)
    {
      if (n28 == 0) return {true, QStringLiteral ("DE")};
      if (n28 == 1) return {true, QStringLiteral ("QRZ")};
      if (n28 == 2) return {true, QStringLiteral ("CQ")};
      if (n28 <= 1002)
        {
          return {true, QStringLiteral ("CQ_%1").arg (n28 - 3, 3, 10, QLatin1Char ('0'))};
        }
      if (n28 <= 532443)
        {
          int value = n28 - 1003;
          QString suffix (4, QLatin1Char (' '));
          for (int i = 3; i >= 0; --i)
            {
              int j = value % 27;
              suffix[i] = (j == 0) ? QLatin1Char (' ') : QChar ('A' + j - 1);
              value /= 27;
            }
          return {true, QStringLiteral ("CQ_") + suffix.trimmed ()};
        }
      return {};
    }

  n28 -= kNumTokens;
  if (n28 < kMax22)
    {
      if (context)
        {
          return {true, context->lookupHash22 (n28)};
        }
      return {true, QStringLiteral ("<...>")};
    }

  static QString const c1 = QStringLiteral (" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  static QString const c2 = QStringLiteral ("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  static QString const c3 = QStringLiteral ("0123456789");
  static QString const c4 = QStringLiteral (" ABCDEFGHIJKLMNOPQRSTUVWXYZ");

  int value = n28 - kMax22;
  int const i1 = value / (36 * 10 * 27 * 27 * 27);
  value -= 36 * 10 * 27 * 27 * 27 * i1;
  int const i2 = value / (10 * 27 * 27 * 27);
  value -= 10 * 27 * 27 * 27 * i2;
  int const i3 = value / (27 * 27 * 27);
  value -= 27 * 27 * 27 * i3;
  int const i4 = value / (27 * 27);
  value -= 27 * 27 * i4;
  int const i5 = value / 27;
  int const i6 = value - 27 * i5;
  QString decoded;
  decoded += c1[i1];
  decoded += c2[i2];
  decoded += c3[i3];
  decoded += c4[i4];
  decoded += c4[i5];
  decoded += c4[i6];
  decoded = decoded.trimmed ();
  if (!check_call (decoded).ok)
    {
      return {};
    }
  return {true, decoded};
}

void write_bits (MessageBits77& bits, int offset, int width, std::uint64_t value)
{
  for (int i = 0; i < width; ++i)
    {
      int const shift = width - 1 - i;
      bits[offset + i] = static_cast<unsigned char> ((value >> shift) & 0x1u);
    }
}

std::uint64_t read_bits (MessageBits77 const& bits, int offset, int width)
{
  std::uint64_t value = 0;
  for (int i = 0; i < width; ++i)
    {
      value = (value << 1) | bits[offset + i];
    }
  return value;
}

void write_uint71_bits (MessageBits77& bits, int offset, Uint71 const& value)
{
  int bit_index = offset;
  for (int i = 6; i >= 0; --i)
    {
      bits[bit_index++] = static_cast<unsigned char> ((value[0] >> i) & 0x1u);
    }
  for (int byte = 1; byte < 9; ++byte)
    {
      for (int i = 7; i >= 0; --i)
        {
          bits[bit_index++] = static_cast<unsigned char> ((value[byte] >> i) & 0x1u);
        }
    }
}

Uint71 read_uint71_bits (MessageBits77 const& bits, int offset)
{
  Uint71 value {};
  int bit_index = offset;
  for (int i = 6; i >= 0; --i)
    {
      value[0] |= static_cast<unsigned char> (bits[bit_index++] << i);
    }
  for (int byte = 1; byte < 9; ++byte)
    {
      for (int i = 7; i >= 0; --i)
        {
          value[byte] |= static_cast<unsigned char> (bits[bit_index++] << i);
        }
    }
  return value;
}

void mp_short_mult (Uint71& value, int multiplier)
{
  int carry = 0;
  for (int i = 8; i >= 0; --i)
    {
      int const next = value[i] * multiplier + carry;
      value[i] = static_cast<unsigned char> (next & 0xff);
      carry = next >> 8;
    }
}

void mp_short_add (Uint71& value, int addend)
{
  int carry = addend;
  for (int i = 8; i >= 0 && carry > 0; --i)
    {
      int const next = value[i] + carry;
      value[i] = static_cast<unsigned char> (next & 0xff);
      carry = next >> 8;
    }
}

int mp_short_div (Uint71& value, int divisor)
{
  int remainder = 0;
  for (int i = 0; i < 9; ++i)
    {
      int const next = remainder * 256 + value[i];
      value[i] = static_cast<unsigned char> (next / divisor);
      remainder = next % divisor;
    }
  return remainder;
}

Maybe<Uint71> packtext71_cpp (QString const& text)
{
  static QString const alphabet = QStringLiteral (" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?");
  QString w = text.left (13).toUpper ().rightJustified (13, QLatin1Char (' '));
  Uint71 value {};
  for (QChar const c : w)
    {
      int idx = alphabet.indexOf (c);
      if (idx < 0)
        {
          return {};
        }
      mp_short_mult (value, 42);
      mp_short_add (value, idx);
    }
  return {true, value};
}

Maybe<QString> unpacktext71_cpp (Uint71 value)
{
  static QString const alphabet = QStringLiteral (" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?");
  QString text (13, QLatin1Char (' '));
  for (int i = 12; i >= 0; --i)
    {
      int const remainder = mp_short_div (value, 42);
      text[i] = alphabet[remainder];
    }
  text = text.trimmed ();
  if (text.isEmpty ())
    {
      return {};
    }
  return {true, text};
}

QString format_report (int isnr)
{
  QChar sign = isnr < 0 ? QLatin1Char ('-') : QLatin1Char ('+');
  int const magnitude = std::abs (isnr);
  return QStringLiteral ("%1%2").arg (sign).arg (magnitude, 2, 10, QLatin1Char ('0'));
}

bool has_supported_standard_compound (QString const& word)
{
  int slash = word.indexOf (QLatin1Char ('/'));
  return slash < 0 || word.endsWith (QStringLiteral ("/P")) || word.endsWith (QStringLiteral ("/R"));
}

struct PackedStandard
{
  MessageBits77 bits {};
  int i3 {1};
  int n3 {0};
};

struct ParsedReport
{
  int ir {0};
  int encoded_report {0};
};

Maybe<ParsedReport> parse_report_token (QString const& token)
{
  ParsedReport parsed;
  QString work = token;
  if (work.startsWith (QStringLiteral ("R+")) || work.startsWith (QStringLiteral ("R-")))
    {
      parsed.ir = 1;
      work.remove (0, 1);
    }
  else if (!(work.startsWith (QLatin1Char ('+')) || work.startsWith (QLatin1Char ('-'))))
    {
      return {};
    }

  bool ok = false;
  int report = work.toInt (&ok);
  if (!ok)
    {
      return {};
    }
  if (report >= -50 && report <= -31)
    {
      report += 101;
    }
  parsed.encoded_report = report + 35;
  return {true, parsed};
}

Maybe<PackedStandard> pack_standard_cpp (QString const& message)
{
  QStringList words = split77_cpp (message);
  if (words.size () < 2 || words.size () > 4)
    {
      return {};
    }
  for (QString const& word : words)
    {
      if (word.size () > 13)
        {
          return {};
        }
    }

  bool const hashed1 = words[0].startsWith (QLatin1Char ('<'))
      && words[0].contains (QLatin1Char ('>'));
  bool const hashed2 = words[1].startsWith (QLatin1Char ('<'))
      && words[1].contains (QLatin1Char ('>'));

  CheckedCall checked1 = check_call (words[0]);
  CheckedCall checked2 = check_call (words[1]);
  bool ok1 = checked1.ok;
  bool ok2 = checked2.ok;

  if (words[0] == QStringLiteral ("DE")
      || words[0] == QStringLiteral ("QRZ")
      || words[0] == QStringLiteral ("CQ")
      || words[0].startsWith (QStringLiteral ("CQ_")))
    {
      ok1 = true;
    }
  if (hashed1 && words[0].indexOf (QLatin1Char ('>')) >= 4)
    {
      ok1 = true;
    }
  if (hashed2 && words[1].indexOf (QLatin1Char ('>')) >= 4)
    {
      ok2 = true;
    }

  if (!ok1 || !ok2)
    {
      return {};
    }
  if ((!hashed1 && !has_supported_standard_compound (words[0]))
      || (!hashed2 && !has_supported_standard_compound (words[1])))
    {
      return {};
    }
  if (hashed1 && words[1].contains (QLatin1Char ('/')))
    {
      return {};
    }
  if (hashed2 && words[0].contains (QLatin1Char ('/')))
    {
      return {};
    }
  if (words.size () == 2 && (!ok2 || words[1].indexOf (QLatin1Char ('/')) >= 1))
    {
      return {};
    }

  Maybe<int> n28a = pack28_cpp (words[0]);
  Maybe<int> n28b = pack28_cpp (words[1]);
  if (!n28a.ok || !n28b.ok)
    {
      return {};
    }

  int ipa = (words[0].endsWith (QStringLiteral ("/P")) || words[0].endsWith (QStringLiteral ("/R"))) ? 1 : 0;
  int ipb = (words[1].endsWith (QStringLiteral ("/P")) || words[1].endsWith (QStringLiteral ("/R"))) ? 1 : 0;
  int i3 = (words[0].endsWith (QStringLiteral ("/P")) || words[1].endsWith (QStringLiteral ("/P"))) ? 2 : 1;
  int ir = 0;
  int igrid4 = kMaxGrid4 + 1;

  if (words.size () == 4 && words[3] == QStringLiteral ("TU"))
    {
      Maybe<ParsedReport> report = parse_report_token (words[2]);
      if (!report.ok)
        {
          return {};
        }
      ir = report.value.ir;
      igrid4 = kMaxGrid4 + report.value.encoded_report + 101;
    }
  else if (words.size () > 2)
    {
      QString const tail = words.back ();
      if (is_grid4 (tail))
        {
          if (words.size () == 4)
            {
              if (words[2] != QStringLiteral ("R"))
                {
                  return {};
                }
              ir = 1;
            }
          igrid4 = pack_grid4 (tail.left (4));
        }
      else if (tail == QStringLiteral ("RRR"))
        {
          igrid4 = kMaxGrid4 + 2;
        }
      else if (tail == QStringLiteral ("RR73"))
        {
          igrid4 = kMaxGrid4 + 3;
        }
      else if (tail == QStringLiteral ("73"))
        {
          igrid4 = kMaxGrid4 + 4;
        }
      else
        {
          Maybe<ParsedReport> report = parse_report_token (tail);
          if (!report.ok)
            {
              return {};
            }
          ir = report.value.ir;
          igrid4 = kMaxGrid4 + report.value.encoded_report;
        }
    }

  PackedStandard packed;
  packed.i3 = i3;
  packed.n3 = 0;
  write_bits (packed.bits, 0, 28, static_cast<std::uint64_t> (n28a.value));
  write_bits (packed.bits, 28, 1, static_cast<std::uint64_t> (ipa));
  write_bits (packed.bits, 29, 28, static_cast<std::uint64_t> (n28b.value));
  write_bits (packed.bits, 57, 1, static_cast<std::uint64_t> (ipb));
  write_bits (packed.bits, 58, 1, static_cast<std::uint64_t> (ir));
  write_bits (packed.bits, 59, 15, static_cast<std::uint64_t> (igrid4));
  write_bits (packed.bits, 74, 3, static_cast<std::uint64_t> (i3));
  return {true, packed};
}

Maybe<QByteArray> unpack77_cpp (MessageBits77 const& bits, int i3, int n3,
                                decodium::txmsg::Decode77Context* context = nullptr,
                                bool received = false)
{
  if (i3 == 0 && n3 == 0)
    {
      Maybe<QString> text = unpacktext71_cpp (read_uint71_bits (bits, 0));
      if (!text.ok)
        {
          return {};
        }
      return {true, to_fixed_37 (text.value)};
    }

  if (i3 == 0 && n3 == 1)
    {
      Maybe<QString> call1 = unpack28_cpp (static_cast<int> (read_bits (bits, 0, 28)), context);
      Maybe<QString> call2 = unpack28_cpp (static_cast<int> (read_bits (bits, 28, 28)), context);
      if (!call1.ok || !call2.ok)
        {
          return {};
        }
      int const n5 = static_cast<int> (read_bits (bits, 66, 5));
      int const n10 = static_cast<int> (read_bits (bits, 56, 10));
      QString const report = format_report (2 * n5 - 30);
      QString call3 = context ? context->lookupHash10 (n10) : QStringLiteral ("<...>");
      if (context && received && context->hasDxCall () && context->hashDx10 () == n10
          && context->dxCall ().size () >= 3)
        {
          call3 = bracket_hash_call (context->dxCall ());
        }
      if (context && !received && context->hasMyCall () && context->hashMy10 () == n10)
        {
          call3 = bracket_hash_call (context->myCall ());
        }
      QString const msg = QStringLiteral ("%1 RR73; %2 %3 %4").arg (call1.value, call2.value, call3, report);
      return {true, to_fixed_37 (msg)};
    }

  if (i3 == 0 && (n3 == 3 || n3 == 4))
    {
      Maybe<QString> call1 = unpack28_cpp (static_cast<int> (read_bits (bits, 0, 28)), context);
      Maybe<QString> call2 = unpack28_cpp (static_cast<int> (read_bits (bits, 28, 28)), context);
      if (!call1.ok || !call2.ok)
        {
          return {};
        }
      int const ir = static_cast<int> (read_bits (bits, 56, 1));
      int const intx = static_cast<int> (read_bits (bits, 57, 4));
      int const nclass = static_cast<int> (read_bits (bits, 61, 3));
      int const isec = static_cast<int> (read_bits (bits, 64, 7));
      if (nclass < 0 || nclass > 25 || isec < 1 || isec > field_day_sections ().size ())
        {
          return {};
        }
      int const ntx = intx + ((n3 == 4) ? 17 : 1);
      QString const cntx = QString::number (ntx) + QChar ('A' + nclass);
      QString const msg = ir == 0
          ? QStringLiteral ("%1 %2 %3 %4").arg (call1.value, call2.value, cntx, field_day_sections ().at (isec - 1))
          : QStringLiteral ("%1 %2 R %3 %4").arg (call1.value, call2.value, cntx, field_day_sections ().at (isec - 1));
      return {true, to_fixed_37 (msg)};
    }

  if (i3 == 0 && n3 == 5)
    {
      QString hex = QStringLiteral ("%1%2%3")
          .arg (static_cast<unsigned int> (read_bits (bits, 0, 23)), 6, 16, QLatin1Char ('0'))
          .arg (static_cast<unsigned int> (read_bits (bits, 23, 24)), 6, 16, QLatin1Char ('0'))
          .arg (static_cast<unsigned int> (read_bits (bits, 47, 24)), 6, 16, QLatin1Char ('0'))
          .toUpper ();
      int first_non_zero = 0;
      while (first_non_zero < hex.size () - 1 && hex[first_non_zero] == QLatin1Char ('0'))
        {
          ++first_non_zero;
        }
      return {true, to_fixed_37 (hex.mid (first_non_zero))};
    }

  if (i3 == 0 && n3 == 6)
    {
      int const j48 = static_cast<int> (read_bits (bits, 47, 1));
      int const j49 = static_cast<int> (read_bits (bits, 48, 1));
      int const j50 = static_cast<int> (read_bits (bits, 49, 1));
      int itype = -1;
      if (j50 == 1) itype = 2;
      else if (j49 == 0) itype = 1;
      else if (j48 == 0) itype = 3;
      if (itype == 1)
        {
          Maybe<QString> call = unpack28_cpp (static_cast<int> (read_bits (bits, 0, 28)), context);
          QString const grid4 = unpack_grid4 (static_cast<int> (read_bits (bits, 28, 15)));
          int const idbm = static_cast<int> (std::lround (read_bits (bits, 43, 5) * 10.0 / 3.0));
          if (!call.ok || grid4.isEmpty ())
            {
              return {};
            }
          if (context)
            {
              context->saveHashCall (call.value);
            }
          return {true, to_fixed_37 (QStringLiteral ("%1 %2 %3").arg (call.value, grid4).arg (idbm))};
        }
      if (itype == 2)
        {
          Maybe<QString> call = unpack28_cpp (static_cast<int> (read_bits (bits, 0, 28)), context);
          Maybe<QString> call_with_affix = unpack_wspr_prefix_suffix_cpp (static_cast<int> (read_bits (bits, 28, 16)), call.ok ? call.value : QString {});
          int const idbm = static_cast<int> (std::lround (read_bits (bits, 44, 5) * 10.0 / 3.0));
          if (!call.ok || !call_with_affix.ok)
            {
              return {};
            }
          if (context)
            {
              context->saveHashCall (call_with_affix.value);
            }
          return {true, to_fixed_37 (QStringLiteral ("%1 %2").arg (call_with_affix.value).arg (idbm))};
        }
      if (itype == 3)
        {
          int const n22 = static_cast<int> (read_bits (bits, 0, 22));
          QString const grid = unpack_grid6_25 (static_cast<int> (read_bits (bits, 22, 25)));
          if (grid.isEmpty ())
            {
              return {};
            }
          QString const call = context ? context->lookupHash22 (n22) : QStringLiteral ("<...>");
          return {true, to_fixed_37 (QStringLiteral ("%1 %2").arg (call, grid))};
        }
      return {};
    }

  if (i3 == 1 || i3 == 2)
    {
      int const n28a = static_cast<int> (read_bits (bits, 0, 28));
      int const n28b = static_cast<int> (read_bits (bits, 29, 28));
      Maybe<QString> call1 = unpack28_cpp (n28a, context);
      Maybe<QString> call2 = unpack28_cpp (n28b, context);
      if (!call1.ok || !call2.ok)
        {
          return {};
        }

      int const ipa = static_cast<int> (read_bits (bits, 28, 1));
      int const ipb = static_cast<int> (read_bits (bits, 57, 1));
      int const ir = static_cast<int> (read_bits (bits, 58, 1));
      int const igrid4 = static_cast<int> (read_bits (bits, 59, 15));

      if (context && received
          && n28a >= kNumTokens && n28a < kNumTokens + kMax22
          && context->hasMyCall () && context->hashMy22 () == (n28a - kNumTokens))
        {
          call1.value = bracket_hash_call (context->myCall ());
        }

      if (call1.value.startsWith (QStringLiteral ("CQ_")))
        {
          call1.value[2] = QLatin1Char (' ');
        }
      if (ipa)
        {
          call1.value += (i3 == 2) ? QStringLiteral ("/P") : QStringLiteral ("/R");
        }
      if (ipb)
        {
          call2.value += (i3 == 2) ? QStringLiteral ("/P") : QStringLiteral ("/R");
        }
      if (context && !call1.value.startsWith (QLatin1Char ('<')))
        {
          context->addRecentCall (call1.value);
        }
      if (context && !call2.value.startsWith (QLatin1Char ('<')))
        {
          context->addRecentCall (call2.value);
          context->saveHashCall (call2.value);
        }

      QString msg;
      if (igrid4 <= kMaxGrid4)
        {
          QString const grid = unpack_grid4 (igrid4);
          msg = ir == 0
              ? QStringLiteral ("%1 %2 %3").arg (call1.value, call2.value, grid)
              : QStringLiteral ("%1 %2 R %3").arg (call1.value, call2.value, grid);
        }
      else
        {
          int const irpt = igrid4 - kMaxGrid4;
          if (irpt == 1)
            {
              msg = QStringLiteral ("%1 %2").arg (call1.value, call2.value);
            }
          else if (irpt == 2)
            {
              msg = QStringLiteral ("%1 %2 RRR").arg (call1.value, call2.value);
            }
          else if (irpt == 3)
            {
              msg = QStringLiteral ("%1 %2 RR73").arg (call1.value, call2.value);
            }
          else if (irpt == 4)
            {
              msg = QStringLiteral ("%1 %2 73").arg (call1.value, call2.value);
            }
          else if (irpt >= 106)
            {
              int isnr = (irpt - 101) - 35;
              if (isnr > 50) isnr -= 101;
              QString const report = format_report (isnr);
              msg = ir == 0
                  ? QStringLiteral ("%1 %2 %3 TU").arg (call1.value, call2.value, report)
                  : QStringLiteral ("%1 %2 R%3 TU").arg (call1.value, call2.value, report);
            }
          else if (irpt >= 5)
            {
              int isnr = irpt - 35;
              if (isnr > 50) isnr -= 101;
              QString const report = format_report (isnr);
              msg = ir == 0
                  ? QStringLiteral ("%1 %2 %3").arg (call1.value, call2.value, report)
                  : QStringLiteral ("%1 %2 R%3").arg (call1.value, call2.value, report);
            }
          else
            {
              return {};
            }
        }
      return {true, to_fixed_37 (msg)};
    }

  if (i3 == 3)
    {
      Maybe<QString> call1 = unpack28_cpp (static_cast<int> (read_bits (bits, 1, 28)), context);
      Maybe<QString> call2 = unpack28_cpp (static_cast<int> (read_bits (bits, 29, 28)), context);
      if (!call1.ok || !call2.ok)
        {
          return {};
        }
      int const itu = static_cast<int> (read_bits (bits, 0, 1));
      int const ir = static_cast<int> (read_bits (bits, 57, 1));
      int const irpt = static_cast<int> (read_bits (bits, 58, 3));
      int const nexch = static_cast<int> (read_bits (bits, 61, 13));
      QString msg = itu == 1 ? QStringLiteral ("TU; ") : QString ();
      msg += QStringLiteral ("%1 %2 ").arg (call1.value, call2.value);
      if (ir == 1)
        {
          msg += QStringLiteral ("R ");
        }
      msg += QStringLiteral ("5%19 ").arg (irpt + 2, 1, 10, QLatin1Char ('0'));
      if (nexch > 8000)
        {
          int const imult = nexch - 8000;
          if (imult < 1 || imult > arrl_rtty_multipliers ().size ())
            {
              return {};
            }
          msg += arrl_rtty_multipliers ().at (imult - 1);
        }
      else if (nexch >= 1)
        {
          msg += QStringLiteral ("%1").arg (nexch, 4, 10, QLatin1Char ('0'));
        }
      else
        {
          return {};
        }
      return {true, to_fixed_37 (msg)};
    }

  if (i3 == 4)
    {
      Maybe<QString> nonstandard = unpack_base38_11 (read_bits (bits, 12, 58));
      if (!nonstandard.ok)
        {
          return {};
        }
      int const n12 = static_cast<int> (read_bits (bits, 0, 12));
      int const iflip = static_cast<int> (read_bits (bits, 70, 1));
      int const nrpt = static_cast<int> (read_bits (bits, 71, 2));
      int const icq = static_cast<int> (read_bits (bits, 73, 1));
      if (icq == 1)
        {
          if (context)
            {
              context->addRecentCall (nonstandard.value);
              context->saveHashCall (nonstandard.value);
            }
          return {true, to_fixed_37 (QStringLiteral ("CQ %1").arg (nonstandard.value))};
        }
      QString call3 = context ? context->lookupHash12 (n12) : QStringLiteral ("<...>");
      QString call1 = (iflip == 0) ? call3 : nonstandard.value;
      QString call2 = (iflip == 0) ? nonstandard.value : call3;
      if (context && iflip == 0)
        {
          context->addRecentCall (call2);
          context->saveHashCall (call2);
          if (received
              && context->hasDxCall () && context->hasMyCall ()
              && normalize_hash_callsign (call2) == normalize_hash_callsign (context->dxCall ())
              && n12 == context->hashMy12 ())
            {
              call1 = bracket_hash_call (context->myCall ());
            }
          if (received
              && context->hasMyCall ()
              && call1.contains (QStringLiteral ("<...>"))
              && n12 == context->hashMy12 ())
            {
              call1 = bracket_hash_call (context->myCall ());
            }
        }
      else if (context && iflip == 1)
        {
          context->addRecentCall (call1);
          context->saveHashCall (call1);
          if (!received && context->hasMyCall () && n12 == context->hashMy12 ())
            {
              call2 = bracket_hash_call (context->myCall ());
            }
        }
      QString msg = QStringLiteral ("%1 %2").arg (call1, call2);
      if (nrpt == 1) msg += QStringLiteral (" RRR");
      else if (nrpt == 2) msg += QStringLiteral (" RR73");
      else if (nrpt == 3) msg += QStringLiteral (" 73");
      return {true, to_fixed_37 (msg)};
    }

  if (i3 == 5)
    {
      int const n12 = static_cast<int> (read_bits (bits, 0, 12));
      int const n22 = static_cast<int> (read_bits (bits, 12, 22));
      int const ir = static_cast<int> (read_bits (bits, 34, 1));
      int const irpt = static_cast<int> (read_bits (bits, 35, 3));
      int const iserial = static_cast<int> (read_bits (bits, 38, 11));
      QString const grid6 = unpack_grid6_24 (static_cast<int> (read_bits (bits, 49, 25)));
      if (grid6.isEmpty ())
        {
          return {};
        }
      QString call1 = context ? context->lookupHash12 (n12) : QStringLiteral ("<...>");
      if (context && context->hasMyCall () && n12 == context->hashMy12 ())
        {
          call1 = bracket_hash_call (context->myCall ());
        }
      QString const call2 = context ? context->lookupHash22 (n22) : QStringLiteral ("<...>");
      QString msg = QStringLiteral ("%1 %2").arg (call1, call2);
      if (ir == 1)
        {
          msg += QStringLiteral (" R");
        }
      QString const exchange = QStringLiteral ("%1%2")
          .arg (52 + irpt, 2, 10, QLatin1Char ('0'))
          .arg (iserial, 4, 10, QLatin1Char ('0'));
      msg += QStringLiteral (" %1 %2").arg (exchange, grid6);
      return {true, to_fixed_37 (msg)};
    }

  return {};
}

QStringList const& field_day_sections ()
{
  static QStringList const sections {
    QStringLiteral ("AB"),  QStringLiteral ("AK"),  QStringLiteral ("AL"),  QStringLiteral ("AR"),
    QStringLiteral ("AZ"),  QStringLiteral ("BC"),  QStringLiteral ("CO"),  QStringLiteral ("CT"),
    QStringLiteral ("DE"),  QStringLiteral ("EB"),  QStringLiteral ("EMA"), QStringLiteral ("ENY"),
    QStringLiteral ("EPA"), QStringLiteral ("EWA"), QStringLiteral ("GA"),  QStringLiteral ("GH"),
    QStringLiteral ("IA"),  QStringLiteral ("ID"),  QStringLiteral ("IL"),  QStringLiteral ("IN"),
    QStringLiteral ("KS"),  QStringLiteral ("KY"),  QStringLiteral ("LA"),  QStringLiteral ("LAX"),
    QStringLiteral ("NS"),  QStringLiteral ("MB"),  QStringLiteral ("MDC"), QStringLiteral ("ME"),
    QStringLiteral ("MI"),  QStringLiteral ("MN"),  QStringLiteral ("MO"),  QStringLiteral ("MS"),
    QStringLiteral ("MT"),  QStringLiteral ("NC"),  QStringLiteral ("ND"),  QStringLiteral ("NE"),
    QStringLiteral ("NFL"), QStringLiteral ("NH"),  QStringLiteral ("NL"),  QStringLiteral ("NLI"),
    QStringLiteral ("NM"),  QStringLiteral ("NNJ"), QStringLiteral ("NNY"), QStringLiteral ("TER"),
    QStringLiteral ("NTX"), QStringLiteral ("NV"),  QStringLiteral ("OH"),  QStringLiteral ("OK"),
    QStringLiteral ("ONE"), QStringLiteral ("ONN"), QStringLiteral ("ONS"), QStringLiteral ("OR"),
    QStringLiteral ("ORG"), QStringLiteral ("PAC"), QStringLiteral ("PR"),  QStringLiteral ("QC"),
    QStringLiteral ("RI"),  QStringLiteral ("SB"),  QStringLiteral ("SC"),  QStringLiteral ("SCV"),
    QStringLiteral ("SD"),  QStringLiteral ("SDG"), QStringLiteral ("SF"),  QStringLiteral ("SFL"),
    QStringLiteral ("SJV"), QStringLiteral ("SK"),  QStringLiteral ("SNJ"), QStringLiteral ("STX"),
    QStringLiteral ("SV"),  QStringLiteral ("TN"),  QStringLiteral ("UT"),  QStringLiteral ("VA"),
    QStringLiteral ("VI"),  QStringLiteral ("VT"),  QStringLiteral ("WCF"), QStringLiteral ("WI"),
    QStringLiteral ("WMA"), QStringLiteral ("WNY"), QStringLiteral ("WPA"), QStringLiteral ("WTX"),
    QStringLiteral ("WV"),  QStringLiteral ("WWA"), QStringLiteral ("WY"),  QStringLiteral ("DX"),
    QStringLiteral ("PE"),  QStringLiteral ("NB")
  };
  return sections;
}

QStringList const& arrl_rtty_multipliers ()
{
  static QStringList const mults {
    QStringLiteral ("AL"),  QStringLiteral ("AK"),  QStringLiteral ("AZ"),  QStringLiteral ("AR"),
    QStringLiteral ("CA"),  QStringLiteral ("CO"),  QStringLiteral ("CT"),  QStringLiteral ("DE"),
    QStringLiteral ("FL"),  QStringLiteral ("GA"),  QStringLiteral ("HI"),  QStringLiteral ("ID"),
    QStringLiteral ("IL"),  QStringLiteral ("IN"),  QStringLiteral ("IA"),  QStringLiteral ("KS"),
    QStringLiteral ("KY"),  QStringLiteral ("LA"),  QStringLiteral ("ME"),  QStringLiteral ("MD"),
    QStringLiteral ("MA"),  QStringLiteral ("MI"),  QStringLiteral ("MN"),  QStringLiteral ("MS"),
    QStringLiteral ("MO"),  QStringLiteral ("MT"),  QStringLiteral ("NE"),  QStringLiteral ("NV"),
    QStringLiteral ("NH"),  QStringLiteral ("NJ"),  QStringLiteral ("NM"),  QStringLiteral ("NY"),
    QStringLiteral ("NC"),  QStringLiteral ("ND"),  QStringLiteral ("OH"),  QStringLiteral ("OK"),
    QStringLiteral ("OR"),  QStringLiteral ("PA"),  QStringLiteral ("RI"),  QStringLiteral ("SC"),
    QStringLiteral ("SD"),  QStringLiteral ("TN"),  QStringLiteral ("TX"),  QStringLiteral ("UT"),
    QStringLiteral ("VT"),  QStringLiteral ("VA"),  QStringLiteral ("WA"),  QStringLiteral ("WV"),
    QStringLiteral ("WI"),  QStringLiteral ("WY"),  QStringLiteral ("NB"),  QStringLiteral ("NS"),
    QStringLiteral ("QC"),  QStringLiteral ("ON"),  QStringLiteral ("MB"),  QStringLiteral ("SK"),
    QStringLiteral ("AB"),  QStringLiteral ("BC"),  QStringLiteral ("NWT"), QStringLiteral ("NF"),
    QStringLiteral ("LB"),  QStringLiteral ("NU"),  QStringLiteral ("YT"),  QStringLiteral ("PEI"),
    QStringLiteral ("DC"),  QStringLiteral ("DR"),  QStringLiteral ("FR"),  QStringLiteral ("GD"),
    QStringLiteral ("GR"),  QStringLiteral ("OV"),  QStringLiteral ("ZH"),  QStringLiteral ("ZL"),
    QStringLiteral ("X01"), QStringLiteral ("X02"), QStringLiteral ("X03"), QStringLiteral ("X04"),
    QStringLiteral ("X05"), QStringLiteral ("X06"), QStringLiteral ("X07"), QStringLiteral ("X08"),
    QStringLiteral ("X09"), QStringLiteral ("X10"), QStringLiteral ("X11"), QStringLiteral ("X12"),
    QStringLiteral ("X13"), QStringLiteral ("X14"), QStringLiteral ("X15"), QStringLiteral ("X16"),
    QStringLiteral ("X17"), QStringLiteral ("X18"), QStringLiteral ("X19"), QStringLiteral ("X20"),
    QStringLiteral ("X21"), QStringLiteral ("X22"), QStringLiteral ("X23"), QStringLiteral ("X24"),
    QStringLiteral ("X25"), QStringLiteral ("X26"), QStringLiteral ("X27"), QStringLiteral ("X28"),
    QStringLiteral ("X29"), QStringLiteral ("X30"), QStringLiteral ("X31"), QStringLiteral ("X32"),
    QStringLiteral ("X33"), QStringLiteral ("X34"), QStringLiteral ("X35"), QStringLiteral ("X36"),
    QStringLiteral ("X37"), QStringLiteral ("X38"), QStringLiteral ("X39"), QStringLiteral ("X40"),
    QStringLiteral ("X41"), QStringLiteral ("X42"), QStringLiteral ("X43"), QStringLiteral ("X44"),
    QStringLiteral ("X45"), QStringLiteral ("X46"), QStringLiteral ("X47"), QStringLiteral ("X48"),
    QStringLiteral ("X49"), QStringLiteral ("X50"), QStringLiteral ("X51"), QStringLiteral ("X52"),
    QStringLiteral ("X53"), QStringLiteral ("X54"), QStringLiteral ("X55"), QStringLiteral ("X56"),
    QStringLiteral ("X57"), QStringLiteral ("X58"), QStringLiteral ("X59"), QStringLiteral ("X60"),
    QStringLiteral ("X61"), QStringLiteral ("X62"), QStringLiteral ("X63"), QStringLiteral ("X64"),
    QStringLiteral ("X65"), QStringLiteral ("X66"), QStringLiteral ("X67"), QStringLiteral ("X68"),
    QStringLiteral ("X69"), QStringLiteral ("X70"), QStringLiteral ("X71"), QStringLiteral ("X72"),
    QStringLiteral ("X73"), QStringLiteral ("X74"), QStringLiteral ("X75"), QStringLiteral ("X76"),
    QStringLiteral ("X77"), QStringLiteral ("X78"), QStringLiteral ("X79"), QStringLiteral ("X80"),
    QStringLiteral ("X81"), QStringLiteral ("X82"), QStringLiteral ("X83"), QStringLiteral ("X84"),
    QStringLiteral ("X85"), QStringLiteral ("X86"), QStringLiteral ("X87"), QStringLiteral ("X88"),
    QStringLiteral ("X89"), QStringLiteral ("X90"), QStringLiteral ("X91"), QStringLiteral ("X92"),
    QStringLiteral ("X93"), QStringLiteral ("X94"), QStringLiteral ("X95"), QStringLiteral ("X96"),
    QStringLiteral ("X97"), QStringLiteral ("X98"), QStringLiteral ("X99")
  };
  return mults;
}

Maybe<PackedMessage> pack_telemetry_cpp (QString const& message)
{
  QString const normalized = normalize_message77 (message);
  if (normalized.isEmpty () || normalized.contains (QLatin1Char (' ')) || normalized.size () > 18)
    {
      return {};
    }
  for (QChar const c : normalized)
    {
      if (!is_hex_upper (c))
        {
          return {};
        }
    }

  QString const padded = normalized.rightJustified (18, QLatin1Char ('0'));
  std::array<int, 3> words {};
  for (int section = 0; section < 3; ++section)
    {
      int value = 0;
      for (int i = 0; i < 6; ++i)
        {
          int const nibble = hex_value (padded[section * 6 + i]);
          if (nibble < 0)
            {
              return {};
            }
          value = (value << 4) | nibble;
        }
      words[section] = value;
    }
  if (words[0] >= (1 << 23))
    {
      return {};
    }

  int first_non_zero = 0;
  while (first_non_zero < padded.size () - 1 && padded[first_non_zero] == QLatin1Char ('0'))
    {
      ++first_non_zero;
    }

  MessageBits77 bits {};
  write_bits (bits, 0, 23, static_cast<std::uint64_t> (words[0]));
  write_bits (bits, 23, 24, static_cast<std::uint64_t> (words[1]));
  write_bits (bits, 47, 24, static_cast<std::uint64_t> (words[2]));
  write_bits (bits, 71, 3, 5);
  write_bits (bits, 74, 3, 0);
  return {true, make_packed_message (bits, 0, 5, to_fixed_37 (padded.mid (first_non_zero)))};
}

Maybe<int> pack_wspr_suffix_prefix_cpp (QString const& call_with_affix)
{
  static QString const alphabet = QStringLiteral ("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  int const slash = call_with_affix.indexOf (QLatin1Char ('/'));
  if (slash < 1 || slash == call_with_affix.size () - 1)
    {
      return {};
    }

  if (slash <= 3)
    {
      QString const prefix = call_with_affix.left (slash);
      int value = 0;
      for (QChar const c : prefix)
        {
          int const index = alphabet.indexOf (c);
          if (index < 0)
            {
              return {};
            }
          value = 36 * value + index;
        }
      return {true, value};
    }

  QString const suffix = call_with_affix.mid (slash + 1);
  if (suffix.size () == 1)
    {
      int const index = alphabet.indexOf (suffix[0]);
      return index < 0 ? Maybe<int> {} : Maybe<int> {true, index + 46656};
    }
  if (suffix.size () == 2)
    {
      int const a = alphabet.indexOf (suffix[0]);
      int const b = alphabet.indexOf (suffix[1]);
      if (a < 0 || b < 0)
        {
          return {};
        }
      return {true, 46656 + 36 * a + b};
    }
  if (suffix.size () == 3)
    {
      int const a = alphabet.indexOf (suffix[0]);
      int const b = alphabet.indexOf (suffix[1]);
      int const c = suffix[2].unicode () - '0';
      if (a < 0 || b < 0 || c < 0 || c > 9)
        {
          return {};
        }
      return {true, 46656 + 36 * 10 * a + 10 * b + c};
    }
  return {};
}

Maybe<QString> unpack_wspr_prefix_suffix_cpp (int npfx, QString const& call)
{
  static QString const alphabet = QStringLiteral ("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  QString affix;
  if (npfx < 46656)
    {
      affix = QString (3, QLatin1Char (' '));
      int value = npfx;
      for (int i = 2; i >= 0; --i)
        {
          affix[i] = alphabet[value % 36];
          value /= 36;
          if (value == 0)
            {
              break;
            }
        }
      return {true, affix.trimmed () + QLatin1Char ('/') + call};
    }

  int value = npfx - 46656;
  if (value <= 35)
    {
      affix = QString (alphabet[value]);
    }
  else if (value <= 1295)
    {
      affix += alphabet[value / 36];
      affix += alphabet[value % 36];
    }
  else if (value <= 12959)
    {
      affix += alphabet[value / 360];
      affix += alphabet[(value / 10) % 36];
      affix += QChar ('0' + (value % 10));
    }
  else
    {
      return {};
    }
  return {true, call + QLatin1Char ('/') + affix.trimmed ()};
}

Maybe<PackedMessage> pack_wspr_cpp (QString const& message)
{
  QStringList const words = split77_cpp (message);
  if (words.size () == 3 && is_grid4 (words[1]))
    {
      CheckedCall checked = check_call (words[0]);
      if (!checked.ok)
        {
          return {};
        }
      bool dbm_ok = false;
      int dbm = words[2].toInt (&dbm_ok);
      Maybe<int> n28 = pack28_cpp (checked.base);
      if (!dbm_ok || !n28.ok)
        {
          return {};
        }
      dbm = std::max (0, std::min (60, dbm));

      MessageBits77 bits {};
      write_bits (bits, 0, 28, static_cast<std::uint64_t> (n28.value));
      write_bits (bits, 28, 15, static_cast<std::uint64_t> (pack_grid4 (words[1])));
      write_bits (bits, 43, 5, static_cast<std::uint64_t> (std::lround (0.3 * dbm)));
      write_bits (bits, 48, 1, 0);
      write_bits (bits, 49, 1, 0);
      write_bits (bits, 50, 21, 0);
      write_bits (bits, 71, 3, 6);
      write_bits (bits, 74, 3, 0);
      return {true, make_packed_message (bits, 0, 6, to_fixed_37 (normalize_message77 (message)))};
    }

  if (words.size () == 2 && words[0].contains (QLatin1Char ('/')))
    {
      CheckedCall checked = check_call (words[0]);
      Maybe<int> npfx = pack_wspr_suffix_prefix_cpp (words[0]);
      if (!checked.ok || !npfx.ok)
        {
          return {};
        }

      bool dbm_ok = false;
      int dbm = words[1].toInt (&dbm_ok);
      Maybe<int> n28 = pack28_cpp (checked.base);
      if (!dbm_ok || !n28.ok)
        {
          return {};
        }
      dbm = std::max (0, std::min (60, dbm));

      MessageBits77 bits {};
      write_bits (bits, 0, 28, static_cast<std::uint64_t> (n28.value));
      write_bits (bits, 28, 16, static_cast<std::uint64_t> (npfx.value));
      write_bits (bits, 44, 5, static_cast<std::uint64_t> (std::lround (0.3 * dbm)));
      write_bits (bits, 49, 1, 1);
      write_bits (bits, 50, 21, 0);
      write_bits (bits, 71, 3, 6);
      write_bits (bits, 74, 3, 0);
      return {true, make_packed_message (bits, 0, 6, to_fixed_37 (normalize_message77 (message)))};
    }

  if (words.size () == 2
      && words[0].startsWith (QLatin1Char ('<')) && words[0].endsWith (QLatin1Char ('>'))
      && is_grid6_25_or4 (words[1]))
    {
      Maybe<int> n22 = ihashcall_cpp (words[0], 22);
      if (!n22.ok)
        {
          return {};
        }

      QString const grid = words[1].left (6);
      MessageBits77 bits {};
      write_bits (bits, 0, 22, static_cast<std::uint64_t> (n22.value));
      write_bits (bits, 22, 25, static_cast<std::uint64_t> (pack_grid6_25 (grid)));
      write_bits (bits, 47, 3, 2);
      write_bits (bits, 50, 21, 0);
      write_bits (bits, 71, 3, 6);
      write_bits (bits, 74, 3, 0);
      return {true, make_packed_message (bits, 0, 6, to_fixed_37 (normalize_message77 (message)))};
    }

  return {};
}

Maybe<PackedMessage> pack_dxped_cpp (QString const& message)
{
  QStringList const words = split77_cpp (message);
  if (words.size () != 5 || words[1] != QStringLiteral ("RR73;")
      || !words[3].startsWith (QLatin1Char ('<')) || !words[3].endsWith (QLatin1Char ('>')))
    {
      return {};
    }

  CheckedCall first = check_call (words[0]);
  CheckedCall second = check_call (words[2]);
  Maybe<int> n28a = pack28_cpp (words[0]);
  Maybe<int> n28b = pack28_cpp (words[2]);
  if (!first.ok || !second.ok || !n28a.ok || !n28b.ok)
    {
      return {};
    }

  bool report_ok = false;
  int report = words[4].toInt (&report_ok);
  if (!report_ok)
    {
      return {};
    }
  int n5 = (report + 30) / 2;
  n5 = std::max (0, std::min (31, n5));

  Maybe<int> n10 = ihashcall_cpp (words[3], 10);
  if (!n10.ok)
    {
      return {};
    }

  MessageBits77 bits {};
  write_bits (bits, 0, 28, static_cast<std::uint64_t> (n28a.value));
  write_bits (bits, 28, 28, static_cast<std::uint64_t> (n28b.value));
  write_bits (bits, 56, 10, static_cast<std::uint64_t> (n10.value));
  write_bits (bits, 66, 5, static_cast<std::uint64_t> (n5));
  write_bits (bits, 71, 3, 1);
  write_bits (bits, 74, 3, 0);

  QString const canonical = QStringLiteral ("%1 RR73; %2 %3 %4")
      .arg (words[0], words[2], words[3], format_report (2 * n5 - 30));
  return {true, make_packed_message (bits, 0, 1, to_fixed_37 (canonical))};
}

Maybe<PackedMessage> pack_field_day_cpp (QString const& message)
{
  QStringList const words = split77_cpp (message);
  if (words.size () < 4 || words.size () > 5)
    {
      return {};
    }

  CheckedCall first = check_call (words[0]);
  CheckedCall second = check_call (words[1]);
  if (!first.ok || !second.ok)
    {
      return {};
    }

  int const isec = field_day_sections ().indexOf (words.back ()) + 1;
  if (isec <= 0)
    {
      return {};
    }
  if (words.size () == 5 && words[2] != QStringLiteral ("R"))
    {
      return {};
    }

  QString const ntxClass = words[words.size () - 2];
  if (ntxClass.size () < 2)
    {
      return {};
    }
  bool ntx_ok = false;
  int ntx = ntxClass.left (ntxClass.size () - 1).toInt (&ntx_ok);
  int nclass = ntxClass.back ().unicode () - 'A';
  if (!ntx_ok || ntx < 1 || ntx > 32 || nclass < 0 || nclass > 25)
    {
      return {};
    }

  int n3 = 3;
  int intx = ntx - 1;
  if (intx >= 16)
    {
      n3 = 4;
      intx = ntx - 17;
    }

  Maybe<int> n28a = pack28_cpp (words[0]);
  Maybe<int> n28b = pack28_cpp (words[1]);
  if (!n28a.ok || !n28b.ok)
    {
      return {};
    }

  int ir = (words.size () == 5) ? 1 : 0;
  MessageBits77 bits {};
  write_bits (bits, 0, 28, static_cast<std::uint64_t> (n28a.value));
  write_bits (bits, 28, 28, static_cast<std::uint64_t> (n28b.value));
  write_bits (bits, 56, 1, static_cast<std::uint64_t> (ir));
  write_bits (bits, 57, 4, static_cast<std::uint64_t> (intx));
  write_bits (bits, 61, 3, static_cast<std::uint64_t> (nclass));
  write_bits (bits, 64, 7, static_cast<std::uint64_t> (isec));
  write_bits (bits, 71, 3, static_cast<std::uint64_t> (n3));
  write_bits (bits, 74, 3, 0);
  return {true, make_packed_message (bits, 0, n3, to_fixed_37 (words.join (QStringLiteral (" "))))};
}

Maybe<PackedMessage> pack_rtty_contest_cpp (QString const& message)
{
  QStringList const words = split77_cpp (message);
  if (words.size () < 4 || words.size () > 6)
    {
      return {};
    }

  int itu = 0;
  if (words[0] == QStringLiteral ("TU;"))
    {
      itu = 1;
    }
  if (words.size () < 4 + itu)
    {
      return {};
    }

  CheckedCall first = check_call (words[itu]);
  CheckedCall second = check_call (words[itu + 1]);
  if (!first.ok || !second.ok)
    {
      return {};
    }

  QString const crpt = words[words.size () - 2].left (3);
  if (crpt.contains (QLatin1Char ('-')) || crpt.contains (QLatin1Char ('+')) || crpt.size () != 3
      || crpt[0] != QLatin1Char ('5') || crpt[2] != QLatin1Char ('9'))
    {
      return {};
    }

  bool serial_ok = false;
  int serial = words.back ().toInt (&serial_ok);
  int imult = arrl_rtty_multipliers ().indexOf (words.back ()) + 1;
  int nexch = 0;
  if (serial_ok && serial > 0)
    {
      nexch = serial;
    }
  else if (imult > 0)
    {
      nexch = 8000 + imult;
    }
  else
    {
      return {};
    }

  Maybe<int> n28a = pack28_cpp (words[itu]);
  Maybe<int> n28b = pack28_cpp (words[itu + 1]);
  if (!n28a.ok || !n28b.ok)
    {
      return {};
    }

  int ir = 0;
  int rpt_index = 2 + itu;
  if (words.value (2 + itu) == QStringLiteral ("R"))
    {
      ir = 1;
      rpt_index = 3 + itu;
    }
  bool rpt_ok = false;
  int rpt_val = words.value (rpt_index).toInt (&rpt_ok);
  if (!rpt_ok)
    {
      return {};
    }
  int irpt = (rpt_val - 509) / 10 - 2;
  irpt = std::max (0, std::min (7, irpt));

  MessageBits77 bits {};
  write_bits (bits, 0, 1, static_cast<std::uint64_t> (itu));
  write_bits (bits, 1, 28, static_cast<std::uint64_t> (n28a.value));
  write_bits (bits, 29, 28, static_cast<std::uint64_t> (n28b.value));
  write_bits (bits, 57, 1, static_cast<std::uint64_t> (ir));
  write_bits (bits, 58, 3, static_cast<std::uint64_t> (irpt));
  write_bits (bits, 61, 13, static_cast<std::uint64_t> (nexch));
  write_bits (bits, 74, 3, 3);
  return {true, make_packed_message (bits, 3, 0, to_fixed_37 (words.join (QStringLiteral (" "))))};
}

Maybe<PackedMessage> pack_type4_cpp (QString const& message)
{
  QStringList const words = split77_cpp (message);
  if (words.size () < 2 || words.size () > 3)
    {
      return {};
    }

  QString call1 = words[0];
  QString call2 = words[1];
  QString stripped1 = (call1.startsWith (QLatin1Char ('<')) && call1.endsWith (QLatin1Char ('>'))) ? call1.mid (1, call1.size () - 2) : call1;
  QString stripped2 = (call2.startsWith (QLatin1Char ('<')) && call2.endsWith (QLatin1Char ('>'))) ? call2.mid (1, call2.size () - 2) : call2;
  CheckedCall ok1 = check_call (stripped1);
  CheckedCall ok2 = check_call (stripped2);
  if (ok1.ok && ok2.ok && stripped1 == ok1.base && stripped2 == ok2.base)
    {
      return {};
    }

  int icq = (words[0] == QStringLiteral ("CQ")) ? 1 : 0;
  if (icq == 1 && words[1].size () <= 4)
    {
      return {};
    }

  int iflip = 0;
  int nrpt = 0;
  int n12 = 0;
  QString nonstandard;
  if (icq == 1)
    {
      nonstandard = words[1];
    }
  else if (words[0].startsWith (QLatin1Char ('<')) && words[0].endsWith (QLatin1Char ('>')))
    {
      Maybe<int> hash = ihashcall_cpp (words[0], 12);
      if (!hash.ok)
        {
          return {};
        }
      n12 = hash.value;
      nonstandard = stripped2;
    }
  else if (words[1].startsWith (QLatin1Char ('<')) && words[1].endsWith (QLatin1Char ('>')))
    {
      Maybe<int> hash = ihashcall_cpp (words[1], 12);
      if (!hash.ok)
        {
          return {};
        }
      n12 = hash.value;
      iflip = 1;
      nonstandard = stripped1;
    }
  else
    {
      return {};
    }

  Maybe<std::uint64_t> n58 = pack_base38_11 (nonstandard);
  if (!n58.ok)
    {
      return {};
    }

  if (words.size () == 3)
    {
      if (words[2] == QStringLiteral ("RRR")) nrpt = 1;
      else if (words[2] == QStringLiteral ("RR73")) nrpt = 2;
      else if (words[2] == QStringLiteral ("73")) nrpt = 3;
      else return {};
    }

  MessageBits77 bits {};
  write_bits (bits, 0, 12, static_cast<std::uint64_t> (n12));
  write_bits (bits, 12, 58, n58.value);
  write_bits (bits, 70, 1, static_cast<std::uint64_t> (iflip));
  write_bits (bits, 71, 2, static_cast<std::uint64_t> (nrpt));
  write_bits (bits, 73, 1, static_cast<std::uint64_t> (icq));
  write_bits (bits, 74, 3, 4);
  return {true, make_packed_message (bits, 4, 0, to_fixed_37 (words.join (QStringLiteral (" "))))};
}

Maybe<PackedMessage> pack_type5_cpp (QString const& message)
{
  QStringList const words = split77_cpp (message);
  if (words.size () < 4 || words.size () > 5)
    {
      return {};
    }
  if (!words[0].startsWith (QLatin1Char ('<')) || !words[0].endsWith (QLatin1Char ('>'))
      || !words[1].startsWith (QLatin1Char ('<')) || !words[1].endsWith (QLatin1Char ('>')))
    {
      return {};
    }

  bool exchange_ok = false;
  int exchange = words[words.size () - 2].toInt (&exchange_ok);
  if (!exchange_ok || exchange < 520001 || exchange > 594095 || !is_grid6_24 (words.back ()))
    {
      return {};
    }

  Maybe<int> n12 = ihashcall_cpp (words[0], 12);
  Maybe<int> n22 = ihashcall_cpp (words[1], 22);
  if (!n12.ok || !n22.ok)
    {
      return {};
    }

  int ir = (words.size () == 5 && words[2] == QStringLiteral ("R")) ? 1 : 0;
  int irpt = exchange / 10000 - 52;
  int iserial = exchange % 10000;
  if (iserial > 2047) iserial = 2047;
  int igrid6 = pack_grid6_24 (words.back ());

  MessageBits77 bits {};
  write_bits (bits, 0, 12, static_cast<std::uint64_t> (n12.value));
  write_bits (bits, 12, 22, static_cast<std::uint64_t> (n22.value));
  write_bits (bits, 34, 1, static_cast<std::uint64_t> (ir));
  write_bits (bits, 35, 3, static_cast<std::uint64_t> (irpt));
  write_bits (bits, 38, 11, static_cast<std::uint64_t> (iserial));
  write_bits (bits, 49, 25, static_cast<std::uint64_t> (igrid6));
  write_bits (bits, 74, 3, 5);
  return {true, make_packed_message (bits, 5, 0, to_fixed_37 (words.join (QStringLiteral (" "))))};
}

Maybe<PackedMessage> pack_message77_cpp (QString const& message)
{
  QString const normalized = normalize_message77 (message);

  Maybe<PackedMessage> telemetry = pack_telemetry_cpp (normalized);
  if (telemetry.ok) return telemetry;

  Maybe<PackedMessage> wspr = pack_wspr_cpp (normalized);
  if (wspr.ok) return wspr;

  Maybe<PackedStandard> standard = pack_standard_cpp (normalized);
  if (standard.ok)
    {
      PackedMessage packed;
      packed.ok = true;
      packed.bits = standard.value.bits;
      packed.i3 = standard.value.i3;
      packed.n3 = standard.value.n3;
      Maybe<QByteArray> msgsent = unpack77_cpp (packed.bits, packed.i3, packed.n3);
      if (!msgsent.ok)
        {
          return {};
        }
      packed.msgsent = msgsent.value;
      return {true, packed};
    }

  Maybe<PackedMessage> dxped = pack_dxped_cpp (normalized);
  if (dxped.ok) return dxped;

  Maybe<PackedMessage> fieldDay = pack_field_day_cpp (normalized);
  if (fieldDay.ok) return fieldDay;

  Maybe<PackedMessage> rttyContest = pack_rtty_contest_cpp (normalized);
  if (rttyContest.ok) return rttyContest;

  Maybe<PackedMessage> type4 = pack_type4_cpp (normalized);
  if (type4.ok) return type4;

  Maybe<PackedMessage> type5 = pack_type5_cpp (normalized);
  if (type5.ok) return type5;

  if (!normalized.isEmpty ())
    {
      Maybe<Uint71> text_bits = packtext71_cpp (normalized.left (13));
      if (text_bits.ok)
        {
          PackedMessage packed;
          packed.ok = true;
          packed.i3 = 0;
          packed.n3 = 0;
          write_uint71_bits (packed.bits, 0, text_bits.value);
          Maybe<QByteArray> msgsent = unpack77_cpp (packed.bits, packed.i3, packed.n3);
          if (!msgsent.ok)
            {
              return {};
            }
          packed.msgsent = msgsent.value;
          return {true, packed};
        }
    }

  return {};
}

PackedMessage pack_message77_with_hint (QString const& message, bool strip_leading_blanks,
                                        int i3_hint, int n3_hint)
{
  Maybe<PackedMessage> cpp_packed = pack_message77_cpp (QString::fromLatin1 (trim_or_pad_37 (message, strip_leading_blanks)));
  if (cpp_packed.ok)
    {
      if (i3_hint == 0 && n3_hint == 6 && !(cpp_packed.value.i3 == 0 && cpp_packed.value.n3 == 6))
        {
          // Fall through to the legacy path, which still understands the explicit WSPR tie-break hint.
        }
      else
        {
      return cpp_packed.value;
        }
    }

  auto pack77_tx_cpp = [] (char const msg0[37], int* i3, int* n3, signed char bits77[77],
                           char msgsent[38], bool* success) {
    if (i3)
      {
        *i3 = -1;
      }
    if (n3)
      {
        *n3 = -1;
      }
    if (bits77)
      {
        std::fill_n (bits77, 77, static_cast<signed char> (0));
      }
    if (msgsent)
      {
        std::fill_n (msgsent, 38, '\0');
      }
    if (success)
      {
        *success = false;
      }
    if (!msg0 || !i3 || !n3 || !bits77 || !msgsent || !success)
      {
        return;
      }

    std::array<char, 77> c77 {};
    std::array<char, 37> unpacked {};
    std::fill (c77.begin (), c77.end (), '0');
    std::fill (unpacked.begin (), unpacked.end (), ' ');

    legacy_pack77_reset_context_c ();
    bool pack_ok = false;
    legacy_pack77_pack_c (msg0, i3, n3, c77.data (), unpacked.data (), &pack_ok, 0);
    *success = pack_ok;
    if (!pack_ok)
      {
        return;
      }

    for (int i = 0; i < 77; ++i)
      {
        bits77[i] = c77[static_cast<size_t> (i)] == '1' ? static_cast<signed char> (1)
                                                         : static_cast<signed char> (0);
      }
    std::copy_n (unpacked.data (), 37, msgsent);
    msgsent[37] = '\0';
  };

  PackedMessage packed;
  QByteArray input = trim_or_pad_37 (message, strip_leading_blanks);
  std::array<char, 38> msg {};
  std::copy_n (input.constData (), 37, msg.data ());

  std::array<signed char, 77> bits {};
  std::array<char, 38> msgsent {};
  bool success = false;
  packed.i3 = i3_hint;
  packed.n3 = n3_hint;
  pack77_tx_cpp (msg.data (), &packed.i3, &packed.n3, bits.data (), msgsent.data (), &success);

  packed.ok = success;
  packed.msgsent = QByteArray (msgsent.data ());
  for (int i = 0; i < 77; ++i)
    {
      packed.bits[i] = static_cast<unsigned char> (bits[i] != 0);
    }
  return packed;
}

PackedMessage pack_message77 (QString const& message, bool strip_leading_blanks)
{
  return pack_message77_with_hint (message, strip_leading_blanks, -1, -1);
}

template <size_t N>
int compute_crc24_cpp (std::array<unsigned char, N> const& message)
{
  static std::array<unsigned char, 25> const polynomial {{
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,0,1,1,0,1,1
  }};

  std::array<unsigned char, 25> remainder {};
  std::copy_n (message.begin (), 25, remainder.begin ());
  for (size_t i = 0; i <= N - 25; ++i)
    {
      remainder[24] = message[i + 24];
      unsigned char const lead = remainder[0];
      for (size_t j = 0; j < remainder.size (); ++j)
        {
          remainder[j] = static_cast<unsigned char> ((remainder[j] + lead * polynomial[j]) & 0x1u);
        }
      std::rotate (remainder.begin (), remainder.begin () + 1, remainder.end ());
    }

  int crc = 0;
  for (int i = 0; i < 24; ++i)
    {
      crc = (crc << 1) | remainder[static_cast<size_t> (i)];
    }
  return crc;
}

template <size_t K, size_t M, typename Matrix>
std::array<unsigned char, K + M> encode_linear_block_code (std::array<unsigned char, K> const& message,
                                                           Matrix const& generator)
{
  std::array<unsigned char, K + M> codeword {};
  std::copy (message.begin (), message.end (), codeword.begin ());
  for (size_t row = 0; row < M; ++row)
    {
      unsigned parity = 0;
      for (size_t col = 0; col < K; ++col)
        {
          parity ^= (message[col] & generator[row][col]);
        }
      codeword[K + row] = static_cast<unsigned char> (parity & 0x1u);
    }
  return codeword;
}

Codeword240 encode240_101_cpp (MessageBits101 const& bits)
{
  return encode_linear_block_code<101, 139> (bits, fst4_generator_matrix_240_101 ());
}

Codeword240 encode240_74_cpp (MessageBits74 const& bits)
{
  return encode_linear_block_code<74, 166> (bits, fst4_generator_matrix_240_74 ());
}

QVector<int> map_fst4_tones_from_codeword (Codeword240 const& codeword)
{
  QVector<int> tones (160, 0);
  std::array<int, 120> payload_tones {};
  for (int i = 0; i < 120; ++i)
    {
      int const symbol = codeword[static_cast<size_t> (2 * i + 1)]
                       + 2 * codeword[static_cast<size_t> (2 * i)];
      payload_tones[static_cast<size_t> (i)] =
          symbol <= 1 ? symbol : (symbol == 2 ? 3 : 2);
    }

  std::copy (kFst4SyncWord1.begin (), kFst4SyncWord1.end (), tones.begin () + 0);
  std::copy (payload_tones.begin () + 0, payload_tones.begin () + 30, tones.begin () + 8);
  std::copy (kFst4SyncWord2.begin (), kFst4SyncWord2.end (), tones.begin () + 38);
  std::copy (payload_tones.begin () + 30, payload_tones.begin () + 60, tones.begin () + 46);
  std::copy (kFst4SyncWord1.begin (), kFst4SyncWord1.end (), tones.begin () + 76);
  std::copy (payload_tones.begin () + 60, payload_tones.begin () + 90, tones.begin () + 84);
  std::copy (kFst4SyncWord2.begin (), kFst4SyncWord2.end (), tones.begin () + 114);
  std::copy (payload_tones.begin () + 90, payload_tones.begin () + 120, tones.begin () + 122);
  std::copy (kFst4SyncWord1.begin (), kFst4SyncWord1.end (), tones.begin () + 152);
  return tones;
}

decodium::txmsg::EncodedMessage build_bad_fst4_message ()
{
  decodium::txmsg::EncodedMessage encoded;
  encoded.ok = false;
  encoded.msgsent = QByteArray ("*** bad message ***                  ");
  encoded.msgbits = QByteArray (101, '\0');
  encoded.tones = QVector<int> (160, 0);
  return encoded;
}

decodium::txmsg::EncodedMessage encode_fst4_cpp (QString const& message, bool check_only,
                                                 bool wspr_hint)
{
  auto const packed = pack_message77_with_hint (message, true, wspr_hint ? 0 : -1, wspr_hint ? 6 : -1);
  if (!packed.ok)
    {
      return build_bad_fst4_message ();
    }

  decodium::txmsg::EncodedMessage encoded;
  encoded.ok = true;
  encoded.msgsent = packed.msgsent;
  encoded.i3 = packed.i3;
  encoded.n3 = packed.n3;
  encoded.messageType = (packed.i3 == 0 && packed.n3 == 6) ? 1 : 0;
  encoded.msgbits = QByteArray (101, '\0');

  if (encoded.messageType == 1)
    {
      MessageBits74 bits {};
      for (int i = 0; i < 50; ++i)
        {
          bits[static_cast<size_t> (i)] = packed.bits[static_cast<size_t> (i)];
          encoded.msgbits[i] = static_cast<char> (bits[static_cast<size_t> (i)]);
        }
      int const crc24 = compute_crc24_cpp (bits);
      for (int i = 0; i < 24; ++i)
        {
          bits[static_cast<size_t> (50 + i)] = static_cast<unsigned char> ((crc24 >> (23 - i)) & 0x1);
          encoded.msgbits[50 + i] = static_cast<char> (bits[static_cast<size_t> (50 + i)]);
        }
      if (!check_only)
        {
          encoded.tones = map_fst4_tones_from_codeword (encode240_74_cpp (bits));
        }
      return encoded;
    }

  MessageBits101 bits {};
  for (int i = 0; i < 77; ++i)
    {
      bits[static_cast<size_t> (i)] =
          static_cast<unsigned char> ((packed.bits[static_cast<size_t> (i)] + kFt2Ft4Rvec[static_cast<size_t> (i)]) & 0x1u);
      encoded.msgbits[i] = static_cast<char> (bits[static_cast<size_t> (i)]);
    }
  int const crc24 = compute_crc24_cpp (bits);
  for (int i = 0; i < 24; ++i)
    {
      bits[static_cast<size_t> (77 + i)] = static_cast<unsigned char> ((crc24 >> (23 - i)) & 0x1);
      encoded.msgbits[77 + i] = static_cast<char> (bits[static_cast<size_t> (77 + i)]);
    }
  if (!check_only)
    {
      encoded.tones = map_fst4_tones_from_codeword (encode240_101_cpp (bits));
    }
  return encoded;
}

Codeword174 encode174_91_cpp (MessageBits77 const& message77)
{
  std::array<unsigned char, 12> crc_input {};
  for (int bit = 0; bit < 80; ++bit)
    {
      unsigned const value = (bit < 77) ? message77[bit] : 0u;
      crc_input[bit / 8] |= static_cast<unsigned char> (value << (7 - (bit % 8)));
    }

  short const crc = crc14 (crc_input.data (), static_cast<int> (crc_input.size ()));

  std::array<unsigned char, 91> message {};
  for (int i = 0; i < 77; ++i)
    {
      message[i] = message77[i];
    }
  for (int i = 0; i < 14; ++i)
    {
      message[77 + i] = static_cast<unsigned char> ((crc >> (13 - i)) & 0x1);
    }

  Codeword174 codeword {};
  for (int i = 0; i < 91; ++i)
    {
      codeword[i] = message[i];
    }

  auto const& gen = generator_matrix ();
  for (int row = 0; row < 83; ++row)
    {
      unsigned parity = 0;
      for (int col = 0; col < 91; ++col)
        {
          parity ^= (message[col] & gen[row][col]);
        }
      codeword[91 + row] = static_cast<unsigned char> (parity & 0x1);
    }

  return codeword;
}

QVector<int> map_ft2_ft4_tones (MessageBits77 bits)
{
  for (int i = 0; i < 77; ++i)
    {
      bits[i] = static_cast<unsigned char> ((bits[i] + kFt2Ft4Rvec[i]) & 0x1);
    }

  auto const codeword = encode174_91_cpp (bits);
  std::array<int, 87> data_tones {};
  for (int i = 0; i < 87; ++i)
    {
      int const value = codeword[2 * i + 1] + 2 * codeword[2 * i];
      if (value <= 1) data_tones[i] = value;
      else if (value == 2) data_tones[i] = 3;
      else data_tones[i] = 2;
    }

  QVector<int> tones (103, 0);
  int const icos4a[4] = {0,1,3,2};
  int const icos4b[4] = {1,0,2,3};
  int const icos4c[4] = {2,3,1,0};
  int const icos4d[4] = {3,2,0,1};

  for (int i = 0; i < 4; ++i) tones[i] = icos4a[i];
  for (int i = 0; i < 29; ++i) tones[4 + i] = data_tones[i];
  for (int i = 0; i < 4; ++i) tones[33 + i] = icos4b[i];
  for (int i = 0; i < 29; ++i) tones[37 + i] = data_tones[29 + i];
  for (int i = 0; i < 4; ++i) tones[66 + i] = icos4c[i];
  for (int i = 0; i < 29; ++i) tones[70 + i] = data_tones[58 + i];
  for (int i = 0; i < 4; ++i) tones[99 + i] = icos4d[i];
  return tones;
}

QVector<int> map_ft8_tones (MessageBits77 const& bits)
{
  auto const codeword = encode174_91_cpp (bits);
  int const icos7[7] = {3,1,4,0,6,5,2};
  int const graymap[8] = {0,1,3,2,5,6,4,7};

  QVector<int> tones (79, 0);
  for (int i = 0; i < 7; ++i) {
    tones[i] = icos7[i];
    tones[36 + i] = icos7[i];
    tones[72 + i] = icos7[i];
  }

  int k = 6;
  for (int j = 0; j < 58; ++j)
    {
      int const start = 3 * j;
      ++k;
      if (j == 29) k += 7;
      int const index = codeword[start] * 4 + codeword[start + 1] * 2 + codeword[start + 2];
      tones[k] = graymap[index];
    }
  return tones;
}

bool is_msk144_special_tone_request (QString const& message)
{
  QByteArray const fixed = trim_or_pad_37 (message, true);
  return !fixed.isEmpty () && fixed.front () == '@';
}

bool is_msk144_shorthand_request (QString const& message)
{
  QString const trimmed = QString::fromLatin1 (trim_or_pad_37 (message, true)).trimmed ();
  if (!trimmed.startsWith (QLatin1Char ('<')))
    {
      return false;
    }
  int const closing = trimmed.indexOf (QLatin1Char ('>'));
  if (closing <= 0)
    {
      return false;
    }
  return trimmed.left (closing + 1).indexOf (QLatin1Char (' ')) > 0;
}

std::array<unsigned char, 12> msk144_crc13_input (MessageBits77 const& bits)
{
  std::array<unsigned char, 12> bytes {};
  for (int bit = 0; bit < 77; ++bit)
    {
      if (bits[static_cast<size_t> (bit)] != 0)
        {
          bytes[static_cast<size_t> (bit / 8)] |=
              static_cast<unsigned char> (1u << (7 - (bit % 8)));
        }
    }
  return bytes;
}

Codeword128 encode128_90_cpp (MessageBits77 const& bits)
{
  MessageBits90 message90 {};
  std::copy_n (bits.begin (), bits.size (), message90.begin ());

  std::array<unsigned char, 12> const crc_input = msk144_crc13_input (bits);
  unsigned short const crc = static_cast<unsigned short> (crc13 (crc_input.data (),
                                                                  static_cast<int> (crc_input.size ())));
  for (int i = 0; i < 13; ++i)
    {
      int const shift = 12 - i;
      message90[77 + i] = static_cast<unsigned char> ((crc >> shift) & 0x1u);
    }

  Codeword128 codeword {};
  std::copy_n (message90.begin (), message90.size (), codeword.begin ());

  GeneratorMatrix128_90 const& generator = msk144_generator_matrix ();
  for (size_t row = 0; row < generator.size (); ++row)
    {
      int parity = 0;
      for (size_t col = 0; col < message90.size (); ++col)
        {
          parity ^= (message90[col] & generator[row][col]);
        }
      codeword[90 + static_cast<int> (row)] = static_cast<unsigned char> (parity & 0x1);
    }
  return codeword;
}

QVector<int> map_msk144_tones (MessageBits77 const& bits)
{
  static std::array<unsigned char, 8> const sync_word = {{0, 1, 1, 1, 0, 0, 1, 0}};

  Codeword128 const codeword = encode128_90_cpp (bits);
  std::array<int, 144> bipolar {};
  for (int i = 0; i < 8; ++i)
    {
      bipolar[i] = sync_word[static_cast<size_t> (i)] ? 1 : -1;
      bipolar[56 + i] = sync_word[static_cast<size_t> (i)] ? 1 : -1;
    }
  for (int i = 0; i < 48; ++i)
    {
      bipolar[8 + i] = codeword[static_cast<size_t> (i)] ? 1 : -1;
    }
  for (int i = 0; i < 80; ++i)
    {
      bipolar[64 + i] = codeword[static_cast<size_t> (48 + i)] ? 1 : -1;
    }

  QVector<int> tones (144, 0);
  for (int symbol = 0; symbol < 72; ++symbol)
    {
      int const even_bit = bipolar[2 * symbol + 1];
      int const odd_bit = bipolar[2 * symbol];
      int const next_odd = bipolar[(2 * symbol + 2) % 144];
      int const odd_tone = (even_bit * odd_bit + 1) / 2;
      int const even_tone = -(even_bit * next_odd - 1) / 2;
      tones[2 * symbol] = 1 - odd_tone;
      tones[2 * symbol + 1] = 1 - even_tone;
    }
  return tones;
}

Maybe<int> parse_msk40_report_index (QString const& token)
{
  static std::array<QString, 16> const reports = {{
    QStringLiteral ("-03"), QStringLiteral ("+00"), QStringLiteral ("+03"), QStringLiteral ("+06"),
    QStringLiteral ("+10"), QStringLiteral ("+13"), QStringLiteral ("+16"), QStringLiteral ("R-03"),
    QStringLiteral ("R+00"), QStringLiteral ("R+03"), QStringLiteral ("R+06"), QStringLiteral ("R+10"),
    QStringLiteral ("R+13"), QStringLiteral ("R+16"), QStringLiteral ("RRR"), QStringLiteral ("73")
  }};

  QString const normalized = token.trimmed ().toUpper ();
  for (int i = 0; i < static_cast<int> (reports.size ()); ++i)
    {
      if (normalized == reports[static_cast<size_t> (i)])
        {
          return {true, i};
        }
    }
  return {};
}

std::array<unsigned char, 32> encode_msk40_codeword_cpp (std::array<unsigned char, 16> const& message)
{
  std::array<unsigned char, 16> pchecks {};
  for (size_t row = 0; row < kMsk40GeneratorHex.size (); ++row)
    {
      int sum = 0;
      for (int bit = 0; bit < 16; ++bit)
        {
          if ((kMsk40GeneratorHex[row] >> (15 - bit)) & 0x1u)
            {
              sum += message[static_cast<size_t> (bit)];
            }
        }
      pchecks[row] = static_cast<unsigned char> (sum & 0x1);
    }

  std::array<unsigned char, 32> interleaved {};
  for (int i = 0; i < 16; ++i)
    {
      interleaved[i] = pchecks[static_cast<size_t> (i)];
      interleaved[16 + i] = message[static_cast<size_t> (i)];
    }

  std::array<unsigned char, 32> codeword {};
  for (int i = 0; i < 32; ++i)
    {
      codeword[static_cast<size_t> (kMsk40ColumnOrder[static_cast<size_t> (i)])] =
          interleaved[static_cast<size_t> (i)];
    }
  return codeword;
}

decodium::txmsg::EncodedMessage encode_msk144_bad_message ()
{
  decodium::txmsg::EncodedMessage encoded;
  encoded.ok = false;
  encoded.msgsent = QByteArray ("*** bad message ***                  ");
  encoded.msgbits = QByteArray (77, '\0');
  encoded.tones = QVector<int> (144, 0);
  return encoded;
}

decodium::txmsg::EncodedMessage encode_msk144_shorthand_cpp (QString const& message, bool check_only)
{
  QString const trimmed = QString::fromLatin1 (trim_or_pad_37 (message, true)).trimmed ();
  int const closing = trimmed.indexOf (QLatin1Char ('>'));
  if (!trimmed.startsWith (QLatin1Char ('<')) || closing < 0)
    {
      return encode_msk144_bad_message ();
    }

  QString const hashText = trimmed.mid (1, closing - 1).trimmed ();
  Maybe<int> const reportIndex = parse_msk40_report_index (trimmed.mid (closing + 1).trimmed ());
  if (hashText.isEmpty () || !reportIndex.ok)
    {
      return encode_msk144_bad_message ();
    }

  QByteArray hashBytes (37, ' ');
  QByteArray const hashField = hashText.left (37).toLatin1 ();
  std::copy (hashField.begin (), hashField.end (), hashBytes.begin ());
  std::uint32_t const hashValue =
      nhash (hashBytes.constData (), static_cast<size_t> (hashBytes.size ()), 146u) & 0x0fffu;
  int const payload = static_cast<int> (16u * hashValue) + reportIndex.value;

  std::array<unsigned char, 16> messageBits {};
  for (int i = 0; i < 16; ++i)
    {
      messageBits[static_cast<size_t> (i)] =
          static_cast<unsigned char> ((payload >> i) & 0x1);
    }
  std::array<unsigned char, 32> const codeword = encode_msk40_codeword_cpp (messageBits);

  static std::array<unsigned char, 8> const sync_word = {{1, 0, 1, 1, 0, 0, 0, 1}};
  std::array<int, 40> bipolar {};
  for (int i = 0; i < 8; ++i)
    {
      bipolar[i] = sync_word[static_cast<size_t> (i)] ? 1 : -1;
    }
  for (int i = 0; i < 32; ++i)
    {
      bipolar[8 + i] = codeword[static_cast<size_t> (i)] ? 1 : -1;
    }

  decodium::txmsg::EncodedMessage encoded;
  encoded.ok = true;
  encoded.msgsent = to_fixed_37 (normalize_message77 (message));
  encoded.msgbits = QByteArray (77, '\0');
  encoded.messageType = 7;
  if (!check_only)
    {
      encoded.tones = QVector<int> (144, 0);
      for (int symbol = 0; symbol < 20; ++symbol)
        {
          int const even_bit = bipolar[2 * symbol + 1];
          int const odd_bit = bipolar[2 * symbol];
          int const next_odd = bipolar[(2 * symbol + 2) % 40];
          int const odd_tone = (even_bit * odd_bit + 1) / 2;
          int const even_tone = -(even_bit * next_odd - 1) / 2;
          encoded.tones[2 * symbol] = 1 - odd_tone;
          encoded.tones[2 * symbol + 1] = 1 - even_tone;
        }
      encoded.tones[40] = -40;
    }
  return encoded;
}

decodium::txmsg::EncodedMessage encode_msk144_fixed_tone_cpp (QString const& message, bool check_only)
{
  QString const trimmed = QString::fromLatin1 (trim_or_pad_37 (message, true)).trimmed ();
  bool ok = false;
  int tone = trimmed.mid (1, 4).toInt (&ok);
  if (!ok)
    {
      tone = 1000;
    }

  decodium::txmsg::EncodedMessage encoded;
  encoded.ok = true;
  encoded.msgsent = to_fixed_37 (trimmed);
  encoded.msgbits = QByteArray (77, '\0');
  encoded.messageType = 1;
  if (!check_only)
    {
      encoded.tones = QVector<int> (144, 0);
      encoded.tones[0] = tone;
    }
  return encoded;
}

decodium::txmsg::EncodedMessage build_bad_message (int tone_count)
{
  decodium::txmsg::EncodedMessage encoded;
  encoded.ok = false;
  encoded.msgsent = QByteArray ("*** bad message ***                  ");
  encoded.msgbits = QByteArray (77, '\0');
  encoded.tones = QVector<int> (tone_count, 0);
  return encoded;
}

decodium::txmsg::EncodedMessage encode_ftx_common (QString const& message,
                                                   bool strip_leading_blanks,
                                                   int tone_count,
                                                   bool is_ft8,
                                                   bool check_only)
{
  auto const packed = pack_message77 (message, strip_leading_blanks);
  if (!packed.ok)
    {
      return build_bad_message (tone_count);
    }

  decodium::txmsg::EncodedMessage encoded;
  encoded.ok = true;
  encoded.msgsent = packed.msgsent;
  encoded.msgbits.resize (77);
  for (int i = 0; i < 77; ++i)
    {
      encoded.msgbits[i] = static_cast<char> (packed.bits[i]);
    }
  encoded.i3 = packed.i3;
  encoded.n3 = packed.n3;
  if (!check_only)
    {
      encoded.tones = is_ft8 ? map_ft8_tones (packed.bits) : map_ft2_ft4_tones (packed.bits);
    }
  return encoded;
}
}

extern "C" int ftx_encode_ft8_candidate_c (char const* message37, char* msgsent_out,
                                            int* itone_out, signed char* codeword_out)
{
  if (!message37 || !msgsent_out || !itone_out || !codeword_out)
    {
      return 0;
    }

  std::fill_n (msgsent_out, 37, ' ');
  std::fill_n (itone_out, 79, 0);
  std::fill_n (codeword_out, 174, 0);

  QString const message = QString::fromLatin1 (message37, 37);
  auto const encoded = decodium::txmsg::encodeFt8 (message);
  if (!encoded.ok || encoded.msgbits.size () < 77 || encoded.tones.size () < 79)
    {
      return 0;
    }

  MessageBits77 bits {};
  for (int i = 0; i < 77; ++i)
    {
      bits[static_cast<size_t> (i)] = encoded.msgbits.at (i) != 0 ? 1u : 0u;
    }
  Codeword174 const codeword = encode174_91_cpp (bits);

  QByteArray msgsent = encoded.msgsent.left (37);
  if (msgsent.size () < 37)
    {
      msgsent.append (QByteArray (37 - msgsent.size (), ' '));
    }

  for (int i = 0; i < 37; ++i)
    {
      msgsent_out[i] = msgsent.at (i);
    }
  for (int i = 0; i < 79; ++i)
    {
      itone_out[i] = encoded.tones.at (i);
    }
  for (int i = 0; i < 174; ++i)
    {
      codeword_out[i] = static_cast<signed char> (codeword[static_cast<size_t> (i)] != 0 ? 1 : 0);
    }

  return 1;
}

namespace decodium
{
namespace txmsg
{

Decode77Context::Decode77Context ()
  : m_calls10 (1024)
  , m_calls12 (4096)
{
}

void Decode77Context::clear ()
{
  std::fill (m_calls10.begin (), m_calls10.end (), QString {});
  std::fill (m_calls12.begin (), m_calls12.end (), QString {});
  m_hash22.clear ();
  m_calls22.clear ();
  m_recentCalls.clear ();
  m_mycall13.clear ();
  m_dxcall13.clear ();
  m_mycall13_0.clear ();
  m_dxcall13_0.clear ();
  m_hashmy10 = -1;
  m_hashmy12 = -1;
  m_hashmy22 = -1;
  m_hashdx10 = -1;
  m_hashdx12 = -1;
  m_hashdx22 = -1;
  m_mycallSet = false;
  m_dxcallSet = false;
}

void Decode77Context::setMyCall (QString const& call)
{
  QString const normalized = normalize_hash_callsign (call);
  if (normalized == m_mycall13_0)
    {
      return;
    }

  m_mycall13_0 = normalized;
  m_mycall13 = normalized;
  if (normalized.size () > 2)
    {
      m_mycallSet = true;
      Maybe<int> h10 = ihashcall_cpp (normalized, 10);
      Maybe<int> h12 = ihashcall_cpp (normalized, 12);
      Maybe<int> h22 = ihashcall_cpp (normalized, 22);
      m_hashmy10 = h10.ok ? h10.value : -1;
      m_hashmy12 = h12.ok ? h12.value : -1;
      m_hashmy22 = h22.ok ? h22.value : -1;
      saveHashCall (normalized);
    }
  else
    {
      m_mycallSet = false;
      m_hashmy10 = -1;
      m_hashmy12 = -1;
      m_hashmy22 = -1;
    }
}

void Decode77Context::setDxCall (QString const& call)
{
  QString const normalized = normalize_hash_callsign (call);
  if (normalized == m_dxcall13_0)
    {
      return;
    }

  m_dxcall13_0 = normalized;
  m_dxcall13 = normalized;
  if (normalized.size () > 2)
    {
      m_dxcallSet = true;
      Maybe<int> h10 = ihashcall_cpp (normalized, 10);
      Maybe<int> h12 = ihashcall_cpp (normalized, 12);
      Maybe<int> h22 = ihashcall_cpp (normalized, 22);
      m_hashdx10 = h10.ok ? h10.value : -1;
      m_hashdx12 = h12.ok ? h12.value : -1;
      m_hashdx22 = h22.ok ? h22.value : -1;
    }
  else
    {
      m_dxcallSet = false;
      m_hashdx10 = -1;
      m_hashdx12 = -1;
      m_hashdx22 = -1;
    }
}

void Decode77Context::saveHashCall (QString const& call)
{
  if (call.trimmed ().startsWith (QStringLiteral ("<...>")))
    {
      return;
    }
  QString const cw = normalize_hash_callsign (call);
  if (cw.isEmpty () || cw == QStringLiteral ("...") || cw.size () < 3)
    {
      return;
    }

  Maybe<int> h10 = ihashcall_cpp (cw, 10);
  if (h10.ok && h10.value >= 0 && h10.value < m_calls10.size () && cw != m_mycall13)
    {
      m_calls10[h10.value] = cw;
    }

  Maybe<int> h12 = ihashcall_cpp (cw, 12);
  if (h12.ok && h12.value >= 0 && h12.value < m_calls12.size () && cw != m_mycall13)
    {
      m_calls12[h12.value] = cw;
    }

  Maybe<int> h22 = ihashcall_cpp (cw, 22);
  if (!h22.ok)
    {
      return;
    }
  int const existingIndex = m_hash22.indexOf (h22.value);
  if (existingIndex >= 0)
    {
      m_calls22[existingIndex] = cw;
      return;
    }
  m_hash22.prepend (h22.value);
  m_calls22.prepend (cw);
  while (m_hash22.size () > 1000)
    {
      m_hash22.removeLast ();
      m_calls22.removeLast ();
    }
}

void Decode77Context::addRecentCall (QString const& callsign)
{
  if (callsign.trimmed ().startsWith (QLatin1Char ('<')))
    {
      return;
    }
  QString const normalized = normalize_hash_callsign (callsign);
  if (normalized.size () < 3)
    {
      return;
    }
  m_recentCalls.removeAll (normalized);
  m_recentCalls.prepend (normalized);
  while (m_recentCalls.size () > 10)
    {
      m_recentCalls.removeLast ();
    }
}

QString Decode77Context::lookupHash10 (int hash) const
{
  if (hash < 0 || hash >= m_calls10.size () || m_calls10.at (hash).trimmed ().isEmpty ())
    {
      return QStringLiteral ("<...>");
    }
  return bracket_hash_call (m_calls10.at (hash));
}

QString Decode77Context::lookupHash12 (int hash) const
{
  if (hash < 0 || hash >= m_calls12.size () || m_calls12.at (hash).trimmed ().isEmpty ())
    {
      return QStringLiteral ("<...>");
    }
  return bracket_hash_call (m_calls12.at (hash));
}

QString Decode77Context::lookupHash22 (int hash) const
{
  int const index = m_hash22.indexOf (hash);
  if (index < 0 || index >= m_calls22.size () || m_calls22.at (index).trimmed ().isEmpty ())
    {
      return QStringLiteral ("<...>");
    }
  return bracket_hash_call (m_calls22.at (index));
}

QStringList Decode77Context::recentCalls () const
{
  return m_recentCalls;
}

bool Decode77Context::hasMyCall () const { return m_mycallSet; }
bool Decode77Context::hasDxCall () const { return m_dxcallSet; }
QString Decode77Context::myCall () const { return m_mycall13; }
QString Decode77Context::dxCall () const { return m_dxcall13; }
int Decode77Context::hashMy10 () const { return m_hashmy10; }
int Decode77Context::hashMy12 () const { return m_hashmy12; }
int Decode77Context::hashMy22 () const { return m_hashmy22; }
int Decode77Context::hashDx10 () const { return m_hashdx10; }
int Decode77Context::hashDx12 () const { return m_hashdx12; }
int Decode77Context::hashDx22 () const { return m_hashdx22; }

Decode77Context& sharedDecode77Context ()
{
  static Decode77Context context;
  return context;
}

EncodedMessage encodeFt2 (QString const& message, bool check_only)
{
  return encode_ftx_common (message, true, 103, false, check_only);
}

EncodedMessage encodeFt4 (QString const& message, bool check_only)
{
  return encode_ftx_common (message, true, 103, false, check_only);
}

EncodedMessage encodeFst4 (QString const& message, bool check_only)
{
  return encode_fst4_cpp (message, check_only, false);
}

EncodedMessage encodeFst4WithHint (QString const& message, bool wspr_hint, bool check_only)
{
  return encode_fst4_cpp (message, check_only, wspr_hint);
}

EncodedMessage encodeFt8 (QString const& message)
{
  return encode_ftx_common (message, false, 79, true, false);
}

EncodedMessage encodeJt4 (QString const& message, bool check_only)
{
  return decodium::legacy_jt::encodeJt4 (message, check_only);
}

EncodedMessage encodeJt65 (QString const& message, bool check_only)
{
  return decodium::legacy_jt::encodeJt65 (message, check_only);
}

EncodedMessage encodeJt9 (QString const& message, bool check_only)
{
  return decodium::legacy_jt::encodeJt9 (message, check_only);
}

EncodedMessage encodeQ65 (QString const& message, bool check_only)
{
  EncodedMessage encoded {};
  QByteArray raw = decodium::legacy_jt::detail::fixed_ascii (message, 37);
  if (!raw.isEmpty () && raw.front () == '@')
    {
      bool ok = false;
      int const frequency = raw.mid (1, 4).trimmed ().toInt (&ok);
      int const tone = ok ? frequency : 1000;
      encoded.ok = true;
      encoded.msgsent = QByteArray::number (tone).rightJustified (5, ' ') + " Hz";
      encoded.msgsent = decodium::legacy_jt::detail::fixed_ascii (encoded.msgsent, 37);
      encoded.tones = QVector<int> (kQ65ChannelSymbols, 0);
      encoded.tones[0] = tone;
      encoded.i3 = -1;
      encoded.n3 = -1;
      encoded.messageType = 0;
      if (check_only)
        {
          encoded.tones.clear ();
        }
      return encoded;
    }

  std::array<int, kQ65PayloadSymbols> payload_values {};
  std::array<int, kQ65CodewordSymbols> codeword_values {};
  std::array<int, kQ65ChannelSymbols> tone_values {};
  QByteArray packed_msgsent;
  if (!encode_q65_message_cpp (raw, &packed_msgsent, &payload_values, &codeword_values, &tone_values))
    {
      return encoded;
    }

  encoded.ok = true;
  encoded.msgsent = packed_msgsent;
  encoded.i3 = -1;
  encoded.n3 = -1;
  encoded.messageType = 0;
  if (!check_only)
    {
      encoded.tones = QVector<int> (kQ65ChannelSymbols, 0);
      for (int i = 0; i < kQ65ChannelSymbols; ++i)
        {
          encoded.tones[i] = tone_values[static_cast<size_t> (i)];
        }
    }
  return encoded;
}

EncodedMessage encodeWspr (QString const& message, bool check_only)
{
  return decodium::legacy_jt::encodeWspr (message, check_only);
}

EncodedMessage encodeMsk144 (QString const& message, bool check_only)
{
  if (is_msk144_special_tone_request (message))
    {
      return encode_msk144_fixed_tone_cpp (message, check_only);
    }
  if (is_msk144_shorthand_request (message))
    {
      return encode_msk144_shorthand_cpp (message, check_only);
    }

  auto const packed = pack_message77 (message, true);
  if (!packed.ok)
    {
      return build_bad_message (144);
    }

  EncodedMessage encoded;
  encoded.ok = true;
  encoded.msgsent = packed.msgsent;
  encoded.msgbits.resize (77);
  for (int i = 0; i < 77; ++i)
    {
      encoded.msgbits[i] = static_cast<char> (packed.bits[i]);
    }
  encoded.i3 = packed.i3;
  encoded.n3 = packed.n3;
  encoded.messageType = 1;
  if (!check_only)
    {
      encoded.tones = map_msk144_tones (packed.bits);
    }
  return encoded;
}

DecodedMessage decode77 (QByteArray const& msgbits, int i3, int n3)
{
  return decode77 (msgbits, i3, n3, nullptr, false);
}

DecodedMessage decode77 (QByteArray const& msgbits, Decode77Context* context, bool received)
{
  if (msgbits.size () < 77)
    {
      return {};
    }

  MessageBits77 bits {};
  for (int i = 0; i < 77; ++i)
    {
      char const value = msgbits.at (i);
      bits[i] = static_cast<unsigned char> ((value == 1 || value == '1') ? 1 : 0);
    }

  int const n3 = static_cast<int> (read_bits (bits, 71, 3));
  int const i3 = static_cast<int> (read_bits (bits, 74, 3));
  return decode77 (msgbits, i3, n3, context, received);
}

DecodedMessage decode77 (QByteArray const& msgbits, int i3, int n3,
                         Decode77Context* context, bool received)
{
  DecodedMessage decoded;
  if (msgbits.size () < 77)
    {
      return decoded;
    }

  MessageBits77 bits {};
  for (int i = 0; i < 77; ++i)
    {
      char const value = msgbits.at (i);
      bits[i] = static_cast<unsigned char> ((value == 1 || value == '1') ? 1 : 0);
    }

  Maybe<QByteArray> unpacked = unpack77_cpp (bits, i3, n3, context, received);
  if (!unpacked.ok)
    {
      return decoded;
    }

  decoded.ok = true;
  decoded.msgsent = unpacked.value;
  return decoded;
}

}
}

namespace
{
thread_local decodium::txmsg::Decode77Context g_legacy_pack77_context;
thread_local QString g_legacy_pack77_dxbase;

QString from_fixed_chars (char const* data, int length)
{
  return QString::fromLatin1 (data, length);
}

void to_fixed_chars (QString const& text, char* out, int length)
{
  QByteArray latin = text.left (length).toLatin1 ();
  std::fill_n (out, length, ' ');
  std::copy_n (latin.constData (), std::min (length, latin.size ()), out);
}

void to_fixed_chars (QByteArray const& text, char* out, int length)
{
  std::fill_n (out, length, ' ');
  std::copy_n (text.constData (), std::min (length, text.size ()), out);
}

MessageBits77 bits_from_chars (char const* data, int length)
{
  MessageBits77 bits {};
  for (int i = 0; i < std::min (77, length); ++i)
    {
      char const value = data[i];
      bits[static_cast<size_t> (i)] = static_cast<unsigned char> ((value == 1 || value == '1') ? 1 : 0);
    }
  return bits;
}

bool checked_bits_from_int8 (signed char const* data, int length, MessageBits77* bits_out)
{
  if (!data || !bits_out)
    {
      return false;
    }

  MessageBits77 bits {};
  for (int i = 0; i < std::min (77, length); ++i)
    {
      signed char const value = data[i];
      if (value != 0 && value != 1)
        {
          return false;
        }
      bits[static_cast<size_t> (i)] = static_cast<unsigned char> (value);
    }
  *bits_out = bits;
  return true;
}

QByteArray bytes_from_bits (MessageBits77 const& bits)
{
  QByteArray result (77, '\0');
  for (int i = 0; i < 77; ++i)
    {
      result[i] = static_cast<char> (bits[static_cast<size_t> (i)]);
    }
  return result;
}

void bits_to_fixed_chars (MessageBits77 const& bits, char* out, int length)
{
  std::fill_n (out, length, '0');
  for (int i = 0; i < std::min (77, length); ++i)
    {
      out[i] = bits[static_cast<size_t> (i)] != 0 ? '1' : '0';
    }
}

QString apply_dxbase (QString const& message)
{
  QString normalized = message;
  if (!normalized.startsWith (QStringLiteral ("$DX")))
    {
      return normalized;
    }

  int const first_blank = normalized.indexOf (QLatin1Char (' '));
  if (first_blank < 0)
    {
      return g_legacy_pack77_dxbase.trimmed ();
    }
  return QStringLiteral ("%1 %2")
      .arg (g_legacy_pack77_dxbase.trimmed (),
            normalized.mid (first_blank + 1).trimmed ());
}

QString normalized_call_from_fixed (char const* data, int length)
{
  return normalize_hash_callsign (from_fixed_chars (data, length));
}

bool is_standard_callsign_ftx (QString const& call)
{
  QString const normalized = normalize_hash_callsign (call);
  int const n = normalized.size ();
  if (n < 3)
    {
      return false;
    }

  int iarea = -1;
  for (int i = n - 1; i >= 1; --i)
    {
      if (is_ascii_digit (normalized.at (i)))
        {
          iarea = i + 1; // Preserve the original 1-based Fortran logic.
          break;
        }
    }
  if (iarea < 2 || iarea > 3)
    {
      return false;
    }

  int npdig = 0;
  int nplet = 0;
  for (int i = 0; i < iarea - 1; ++i)
    {
      if (is_ascii_digit (normalized.at (i))) ++npdig;
      if (is_ascii_letter (normalized.at (i))) ++nplet;
    }

  int nslet = 0;
  for (int i = iarea; i < n; ++i)
    {
      if (is_ascii_letter (normalized.at (i))) ++nslet;
    }

  if (nplet == 0 || npdig >= iarea - 1 || nslet > 3)
    {
      return false;
    }
  return true;
}

QString format_ft8_report_text (int snr, bool prefixed_reply)
{
  QString report;
  if (snr >= 0)
    {
      report = QStringLiteral ("+%1").arg (snr, 2, 10, QLatin1Char ('0'));
    }
  else
    {
      report = QStringLiteral ("-%1").arg (std::abs (snr), 2, 10, QLatin1Char ('0'));
    }
  if (!prefixed_reply)
    {
      return report;
    }
  return QStringLiteral ("R%1").arg (report);
}

QString build_ft8_a7_candidate_message (int imsg, QString const& call1,
                                        QString const& call2, QString const& grid4)
{
  bool std1 = is_standard_callsign_ftx (call1);
  bool const std2 = is_standard_callsign_ftx (call2);
  if (call1 == QStringLiteral ("CQ"))
    {
      std1 = true;
    }

  QString msg = call1 + QLatin1Char (' ') + call2;
  if (call1 == QStringLiteral ("CQ") && imsg != 5)
    {
      msg = QStringLiteral ("QU1RK ") + call2;
    }
  if (!std1)
    {
      if (imsg == 1 || imsg >= 6)
        {
          msg = bracket_hash_call (call1) + QLatin1Char (' ') + call2;
        }
      if (imsg >= 2 && imsg <= 4)
        {
          msg = call1 + QLatin1Char (' ') + bracket_hash_call (call2);
        }
    }
  else if (!std2)
    {
      if (imsg <= 4 || imsg == 6)
        {
          msg = bracket_hash_call (call1) + QLatin1Char (' ') + call2;
        }
      if (imsg >= 7)
        {
          msg = call1 + QLatin1Char (' ') + bracket_hash_call (call2);
        }
    }

  if (imsg == 2) msg = msg.trimmed () + QStringLiteral (" RRR");
  if (imsg == 3) msg = msg.trimmed () + QStringLiteral (" RR73");
  if (imsg == 4) msg = msg.trimmed () + QStringLiteral (" 73");
  if (imsg == 5)
    {
      if (std2)
        {
          msg = QStringLiteral ("CQ ") + call2;
          if (call1.size () >= 3 && call1.at (2) == QLatin1Char ('_'))
            {
              msg = call1 + QLatin1Char (' ') + call2;
            }
          if (grid4 != QStringLiteral ("RR73"))
            {
              msg = msg.trimmed () + QLatin1Char (' ') + grid4.left (4);
            }
        }
      if (!std2)
        {
          msg = QStringLiteral ("CQ ") + call2;
        }
    }
  if (imsg == 6 && std2 && !grid4.trimmed ().isEmpty ())
    {
      msg = msg.trimmed () + QLatin1Char (' ') + grid4.left (4);
    }
  if (imsg >= 7)
    {
      int const isnr = -50 + (imsg - 7) / 2;
      msg = msg.trimmed () + QLatin1Char (' ')
            + format_ft8_report_text (isnr, (imsg & 1) == 0);
    }
  return normalize_message77 (msg);
}

QString build_ft8_a8_candidate_message (int imsg, QString const& mycall,
                                        QString const& hiscall, QString const& hisgrid)
{
  bool const my_std = is_standard_callsign_ftx (mycall);
  bool const his_std = is_standard_callsign_ftx (hiscall);

  int isnr = 0;
  QString msg = mycall + QLatin1Char (' ') + hiscall;
  if (!my_std)
    {
      if (imsg == 1 || imsg >= 6)
        {
          msg = bracket_hash_call (mycall) + QLatin1Char (' ') + hiscall;
        }
      if (imsg >= 2 && imsg <= 4)
        {
          msg = mycall + QLatin1Char (' ') + bracket_hash_call (hiscall);
        }
    }
  else if (!his_std)
    {
      if (imsg <= 4 || imsg == 6)
        {
          msg = bracket_hash_call (mycall) + QLatin1Char (' ') + hiscall;
        }
      if (imsg >= 7)
        {
          msg = mycall + QLatin1Char (' ') + bracket_hash_call (hiscall);
        }
    }

  if (imsg == 2) msg = msg.trimmed () + QStringLiteral (" RRR");
  if (imsg == 3) msg = msg.trimmed () + QStringLiteral (" RR73");
  if (imsg == 4) msg = msg.trimmed () + QStringLiteral (" 73");
  if (imsg == 5)
    {
      if (his_std)
        {
          msg = QStringLiteral ("CQ %1 %2").arg (hiscall, hisgrid.left (4));
        }
      else
        {
          msg = QStringLiteral ("CQ ") + hiscall;
        }
    }
  if (imsg == 6 && his_std && !hisgrid.left (4).trimmed ().isEmpty ())
    {
      msg = msg.trimmed () + QLatin1Char (' ') + hisgrid.left (4);
    }
  if (imsg >= 7 && imsg <= 206)
    {
      isnr = -50 + (imsg - 7) / 2;
      msg = msg.trimmed () + QLatin1Char (' ')
            + format_ft8_report_text (isnr, (imsg & 1) == 0);
    }

  if (std::abs (isnr) > 30)
    {
      return {};
    }
  return normalize_message77 (msg);
}
}

extern "C"
{
int ftx_prepare_ft8_a7_candidate_c (int imsg, char const call_1[12], char const call_2[12],
                                    char const grid4[4], char message37[37])
{
  if (!call_1 || !call_2 || !grid4 || !message37 || imsg < 1 || imsg > 206)
    {
      return 0;
    }

  std::fill_n (message37, 37, ' ');
  QString const call1 = from_fixed_chars (call_1, 12).trimmed ().toUpper ();
  QString const call2 = from_fixed_chars (call_2, 12).trimmed ().toUpper ();
  QString const grid = from_fixed_chars (grid4, 4).toUpper ();
  QString const msg = build_ft8_a7_candidate_message (imsg, call1, call2, grid);
  if (msg.isEmpty ())
    {
      return 0;
    }

  to_fixed_chars (msg, message37, 37);
  return 1;
}

int ftx_prepare_ft8_a8_candidate_c (int imsg, char const mycall[12], char const hiscall[12],
                                    char const hisgrid[6], char message37[37])
{
  if (!mycall || !hiscall || !hisgrid || !message37 || imsg < 1 || imsg > 206)
    {
      return 0;
    }

  std::fill_n (message37, 37, ' ');
  QString const my = normalized_call_from_fixed (mycall, 12);
  QString const his = normalized_call_from_fixed (hiscall, 12);
  QString const grid = from_fixed_chars (hisgrid, 6).trimmed ().toUpper ();
  QString const msg = build_ft8_a8_candidate_message (imsg, my, his, grid);
  if (msg.isEmpty ())
    {
      return 0;
    }

  to_fixed_chars (msg, message37, 37);
  return 1;
}

void ftx_prepare_ft8_ap_c (char const mycall[12], char const hiscall[12], int ncontest,
                           int apsym[58], int aph10[10])
{
  if (apsym) std::fill_n (apsym, 58, 0);
  if (aph10) std::fill_n (aph10, 10, 0);
  if (apsym) apsym[0] = 99;
  if (apsym) apsym[29] = 99;
  if (aph10) aph10[0] = 99;

  QString const my = normalized_call_from_fixed (mycall, 12);
  if (my.size () < 3)
    {
      return;
    }

  bool no_hiscall = false;
  QString his = normalized_call_from_fixed (hiscall, 12);
  if (his.size () < 3)
    {
      his = QStringLiteral ("KA1ABC");
      no_hiscall = true;
    }
  else
    {
      Maybe<int> h10 = ihashcall_cpp (his, 10);
      if (h10.ok && aph10)
        {
          for (int i = 0; i < 10; ++i)
            {
              int const shift = 9 - i;
              aph10[i] = (h10.value & (1 << shift)) ? 1 : -1;
            }
        }
    }

  QString msg = QStringLiteral ("%1 %2 RRR").arg (my, his);
  if (!is_standard_callsign_ftx (my))
    {
      msg = QStringLiteral ("<%1> %2 RRR").arg (my, his);
    }

  decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeFt8 (msg);
  if (!encoded.ok)
    {
      return;
    }

  if (ncontest == 7)
    {
      if (encoded.i3 != 1)
        {
          return;
        }
    }
  else if (ncontest <= 5)
    {
      if (encoded.i3 != 1 || trim_or_pad_37 (msg, false) != encoded.msgsent)
        {
          return;
        }
    }

  if (apsym)
    {
      for (int i = 0; i < 58 && i < encoded.msgbits.size (); ++i)
        {
          apsym[i] = encoded.msgbits.at (i) ? 1 : -1;
        }
    }
  if (no_hiscall)
    {
      if (apsym) apsym[29] = 99;
      if (aph10) aph10[0] = 99;
    }
}

void legacy_pack77_reset_context_c ()
{
  g_legacy_pack77_context.clear ();
  g_legacy_pack77_dxbase.clear ();
}

void legacy_pack77_set_context_c (char const mycall[13], char const dxcall[13])
{
  g_legacy_pack77_context.setMyCall (normalized_call_from_fixed (mycall, 13));
  g_legacy_pack77_context.setDxCall (normalized_call_from_fixed (dxcall, 13));
}

void legacy_pack77_set_dxbase_c (char const value[6])
{
  g_legacy_pack77_dxbase = from_fixed_chars (value, 6).trimmed ();
}

namespace
{

void copy_fortran_input_chars (char const* src, fortran_charlen_t len, char* dst, int width)
{
  if (!dst || width <= 0)
    {
      return;
    }
  std::fill_n (dst, width, ' ');
  if (!src || len <= 0)
    {
      return;
    }
  std::copy_n (src, std::min<size_t> (static_cast<size_t> (width), static_cast<size_t> (len)), dst);
}

void copy_fixed_to_fortran_output (char const* src, int src_width, char* dst, fortran_charlen_t len)
{
  if (!dst || len <= 0)
    {
      return;
    }
  std::fill_n (dst, static_cast<size_t> (len), ' ');
  if (!src || src_width <= 0)
    {
      return;
    }
  std::copy_n (src, std::min<size_t> (static_cast<size_t> (src_width), static_cast<size_t> (len)), dst);
}

void copy_int_words_to_fortran_output (char const* src, int word_width, int word_count,
                                       char* dst, fortran_charlen_t len)
{
  if (!dst || len <= 0)
    {
      return;
    }
  std::fill_n (dst, static_cast<size_t> (word_count) * static_cast<size_t> (len), ' ');
  if (!src || word_width <= 0)
    {
      return;
    }
  for (int i = 0; i < word_count; ++i)
    {
      std::copy_n (src + i * word_width,
                   std::min<size_t> (static_cast<size_t> (word_width), static_cast<size_t> (len)),
                   dst + static_cast<size_t> (i) * static_cast<size_t> (len));
    }
}

void set_fortran_logical (int* value, bool state)
{
  if (value)
    {
      *value = state ? 1 : 0;
    }
}

}  // namespace

extern "C" void ftx_pack77_reset_context_ ()
{
  legacy_pack77_reset_context_c ();
}

extern "C" void ftx_pack77_set_context_ (char const* mycall, char const* hiscall,
                                         fortran_charlen_t len1, fortran_charlen_t len2)
{
  char mycall_fixed[13] {};
  char hiscall_fixed[13] {};
  copy_fortran_input_chars (mycall, len1, mycall_fixed, 13);
  copy_fortran_input_chars (hiscall, len2, hiscall_fixed, 13);
  legacy_pack77_set_context_c (mycall_fixed, hiscall_fixed);
}

extern "C" void ftx_pack77_set_dxbase_ (char const* value, fortran_charlen_t len)
{
  char fixed[6] {};
  copy_fortran_input_chars (value, len, fixed, 6);
  legacy_pack77_set_dxbase_c (fixed);
}

extern "C" void ftx_pack77_pack_ (char const* msg0, int* i3, int* n3, char* c77,
                                  char* msgsent, int* success, int* received,
                                  fortran_charlen_t len1, fortran_charlen_t len2,
                                  fortran_charlen_t len3)
{
  char msg0_fixed[37] {};
  char c77_fixed[77] {};
  char msgsent_fixed[37] {};
  bool ok = false;
  int received_value = received ? *received : 1;

  copy_fortran_input_chars (msg0, len1, msg0_fixed, 37);
  legacy_pack77_pack_c (msg0_fixed, i3, n3, c77_fixed, msgsent_fixed, &ok, received_value);
  copy_fixed_to_fortran_output (c77_fixed, 77, c77, len2);
  copy_fixed_to_fortran_output (msgsent_fixed, 37, msgsent, len3);
  set_fortran_logical (success, ok);
}

extern "C" void ftx_pack77_unpack_ (char const* c77, int* received, char* msgsent, int* success,
                                    fortran_charlen_t len1, fortran_charlen_t len2)
{
  char c77_fixed[77] {};
  char msgsent_fixed[37] {};
  bool ok = false;
  copy_fortran_input_chars (c77, len1, c77_fixed, 77);
  legacy_pack77_unpack_c (c77_fixed, received ? *received : 1, msgsent_fixed, &ok);
  copy_fixed_to_fortran_output (msgsent_fixed, 37, msgsent, len2);
  set_fortran_logical (success, ok);
}

extern "C" void ftx_pack77_save_hash_call_ (char const* c13, int* n10, int* n12, int* n22,
                                            fortran_charlen_t len)
{
  char fixed[13] {};
  copy_fortran_input_chars (c13, len, fixed, 13);
  legacy_pack77_save_hash_call_c (fixed, n10, n12, n22);
}

extern "C" int ftx_pack77_hash_call_ (char const* c13, int* bits, fortran_charlen_t len)
{
  char fixed[13] {};
  int hash_out = -1;
  copy_fortran_input_chars (c13, len, fixed, 13);
  if (!bits)
    {
      return -1;
    }
  return legacy_pack77_hash_call_bits_c (fixed, *bits, &hash_out) ? hash_out : -1;
}

extern "C" void ftx_pack77_pack28_ (char const* c13, int* n28, fortran_charlen_t len)
{
  char fixed[13] {};
  bool success = false;
  copy_fortran_input_chars (c13, len, fixed, 13);
  legacy_pack77_pack28_c (fixed, n28, &success);
}

extern "C" void ftx_pack77_unpack28_ (int* n28, char* c13, int* success, fortran_charlen_t len)
{
  char fixed[13] {};
  bool ok = false;
  legacy_pack77_unpack28_c (n28 ? *n28 : 0, fixed, &ok);
  copy_fixed_to_fortran_output (fixed, 13, c13, len);
  set_fortran_logical (success, ok);
}

extern "C" void ftx_pack77_split77_ (char const* msg, int* nwords, int* nw, char* w,
                                     fortran_charlen_t len1, fortran_charlen_t len2)
{
  char msg_fixed[37] {};
  int local_nwords = 0;
  int local_nw[19] {};
  char local_w[19 * 13] {};
  copy_fortran_input_chars (msg, len1, msg_fixed, 37);
  legacy_pack77_split77_c (msg_fixed, &local_nwords, local_nw, local_w);
  if (nwords)
    {
      *nwords = local_nwords;
    }
  if (nw)
    {
      std::copy_n (local_nw, 19, nw);
    }
  copy_int_words_to_fortran_output (local_w, 13, 19, w, len2);
}

extern "C" void ftx_pack77_packtext77_ (char const* c13, char* c71,
                                        fortran_charlen_t len1, fortran_charlen_t len2)
{
  char c13_fixed[13] {};
  char c71_fixed[71] {};
  bool success = false;
  copy_fortran_input_chars (c13, len1, c13_fixed, 13);
  legacy_pack77_packtext77_c (c13_fixed, c71_fixed, &success);
  copy_fixed_to_fortran_output (c71_fixed, 71, c71, len2);
}

extern "C" void ftx_pack77_unpacktext77_ (char const* c71, char* c13,
                                          fortran_charlen_t len1, fortran_charlen_t len2)
{
  char c71_fixed[71] {};
  char c13_fixed[13] {};
  bool success = false;
  copy_fortran_input_chars (c71, len1, c71_fixed, 71);
  legacy_pack77_unpacktext77_c (c71_fixed, c13_fixed, &success);
  copy_fixed_to_fortran_output (c13_fixed, 13, c13, len2);
}

extern "C" void save_dxbase_ (char* dxbase, fortran_charlen_t len)
{
  char fixed[6] {};
  std::fill_n (fixed, 6, ' ');
  if (dxbase && len > 0)
    {
      std::copy_n (dxbase, std::min<size_t> (6, len), fixed);
    }
  legacy_pack77_set_dxbase_c (fixed);
}

void legacy_pack77_pack_c (char const msg0[37], int* i3, int* n3,
                           char c77[77], char msgsent[37], bool* success, int received)
{
  if (i3) *i3 = -1;
  if (n3) *n3 = -1;
  if (success) *success = false;
  if (c77) std::fill_n (c77, 77, '0');
  if (msgsent) std::fill_n (msgsent, 37, ' ');

  Maybe<PackedMessage> packed = pack_message77_cpp (apply_dxbase (from_fixed_chars (msg0, 37)));
  if (!packed.ok)
    {
      return;
    }

  if (i3) *i3 = packed.value.i3;
  if (n3) *n3 = packed.value.n3;
  if (c77) bits_to_fixed_chars (packed.value.bits, c77, 77);

  QByteArray bit_bytes = bytes_from_bits (packed.value.bits);
  decodium::txmsg::DecodedMessage decoded =
      decodium::txmsg::decode77 (bit_bytes, packed.value.i3, packed.value.n3,
                                 &g_legacy_pack77_context, received != 0);
  QByteArray const canonical = decoded.ok ? decoded.msgsent : packed.value.msgsent;
  if (msgsent) to_fixed_chars (canonical, msgsent, 37);
  if (success) *success = true;
}

extern "C" bool stdmsg_ (char const* msg0, fortran_charlen_t len)
{
  char fixed_msg[37] {};
  char c77[77] {};
  char msgsent[37] {};
  int i3 = -1;
  int n3 = -1;
  bool success = false;

  std::fill_n (fixed_msg, 37, ' ');
  if (msg0 && len > 0)
    {
      std::copy_n (msg0, std::min<size_t> (37, len), fixed_msg);
    }

  legacy_pack77_reset_context_c ();
  legacy_pack77_pack_c (fixed_msg, &i3, &n3, c77, msgsent, &success, 0);
  return i3 > 0 || n3 > 0;
}

void legacy_pack77_unpack_c (char const c77[77], int received,
                             char msgsent[37], bool* success)
{
  if (success) *success = false;
  if (msgsent) std::fill_n (msgsent, 37, ' ');

  MessageBits77 bits = bits_from_chars (c77, 77);
  QByteArray const bit_bytes = bytes_from_bits (bits);
  decodium::txmsg::DecodedMessage decoded =
      decodium::txmsg::decode77 (bit_bytes, &g_legacy_pack77_context, received != 0);
  if (!decoded.ok)
    {
      return;
    }

  if (msgsent) to_fixed_chars (decoded.msgsent, msgsent, 37);
  if (success) *success = true;
}

extern "C" void genmsk_128_90_ (char* msg0, int* ichk, char* msgsent, int* i4tone, int* itype,
                                fortran_charlen_t len1, fortran_charlen_t len2)
{
  if (msgsent)
    {
      std::fill_n (msgsent, static_cast<size_t> (len2), ' ');
    }
  if (i4tone)
    {
      std::fill_n (i4tone, 144, 0);
    }
  if (itype)
    {
      *itype = -1;
    }

  QString const message = QString::fromLatin1 (msg0 ? msg0 : "", static_cast<int> (len1));
  bool const check_only = ichk && *ichk == 1;
  decodium::txmsg::EncodedMessage const encoded = decodium::txmsg::encodeMsk144 (message, check_only);
  if (!encoded.ok)
    {
      if (msgsent)
        {
          QByteArray const bad = QByteArrayLiteral ("*** bad message ***                  ");
          std::copy_n (bad.constData (), std::min<size_t> (bad.size (), len2), msgsent);
        }
      return;
    }

  if (itype)
    {
      *itype = encoded.messageType;
    }
  if (msgsent)
    {
      QByteArray fixed = encoded.msgsent.leftJustified (static_cast<int> (len2), ' ', true);
      std::copy_n (fixed.constData (), std::min<size_t> (fixed.size (), len2), msgsent);
    }
  if (i4tone && !encoded.tones.isEmpty ())
    {
      int const tone_count = std::min (144, encoded.tones.size ());
      for (int i = 0; i < tone_count; ++i)
        {
          i4tone[i] = encoded.tones.at (i);
        }
    }
}

namespace
{

void init_ft8var_ap_field (int* values, int count)
{
  if (!values || count <= 0)
    {
      return;
    }

  std::fill_n (values, count, 0);
  values[0] = 99;
  if (count >= 30)
    {
      values[29] = 99;
    }
}

void copy_ft8var_bits (char const c77[77], int* values, int count)
{
  if (!c77 || !values || count <= 0)
    {
      return;
    }

  for (int i = 0; i < count; ++i)
    {
      values[i] = c77[i] == '1' ? 1 : -1;
    }
}

bool fixed_prefix_matches_through_lt (QString const& expected, char const actual[37])
{
  int const lt_index = expected.indexOf ('<');
  if (lt_index < 14)
    {
      return false;
    }

  char expected_fixed[37];
  to_fixed_chars (expected, expected_fixed, 37);
  return std::equal (expected_fixed, expected_fixed + lt_index + 1, actual);
}

bool encode_ft8var_ap_message (QString const& message, int expected_i3,
                               int* values, int count, bool prefix_through_lt)
{
  char msg0[37];
  char c77[77];
  char msgchk[37];
  char msgsent[37];
  int i3 = -1;
  int n3 = -1;
  bool pack_ok = false;
  bool unpack_ok = false;

  to_fixed_chars (message, msg0, 37);
  legacy_pack77_pack_c (msg0, &i3, &n3, c77, msgsent, &pack_ok, 0);
  if (!pack_ok || i3 != expected_i3)
    {
      return false;
    }

  legacy_pack77_unpack_c (c77, 1, msgchk, &unpack_ok);
  if (!unpack_ok)
    {
      return false;
    }

  if (prefix_through_lt)
    {
      if (!fixed_prefix_matches_through_lt (message, msgchk))
        {
          return false;
        }
    }
  else
    {
      char expected_fixed[37];
      to_fixed_chars (message, expected_fixed, 37);
      if (!std::equal (expected_fixed, expected_fixed + 37, msgchk))
        {
          return false;
        }
    }

  copy_ft8var_bits (c77, values, count);
  return true;
}

}  // namespace

void ftx_prepare_ft8var_ap_c (char const mycall[12], char const hiscall[12],
                              char const mybcall[12], char const hisbcall[12],
                              char const hisgrid4[4], int lhound, int lmycallstd,
                              int lhiscallstd, int apsym[58], int apsymsp[66],
                              int apsymdxns1[58], int apsymdxnsrrr[77],
                              int apcqsym[77], int apsymdxnsrr73[77],
                              int apsymdxns73[77], int apsymmyns1[29],
                              int apsymmyns2[58], int apsymmynsrr73[77],
                              int apsymmyns73[77], int apsymdxstd[58],
                              int apsymdxnsr73[77], int apsymdxns732[77],
                              int apsymmynsrrr[77])
{
  init_ft8var_ap_field (apsym, 58);
  init_ft8var_ap_field (apsymsp, 66);
  init_ft8var_ap_field (apsymdxns1, 58);
  init_ft8var_ap_field (apsymdxnsrrr, 77);
  init_ft8var_ap_field (apcqsym, 77);
  init_ft8var_ap_field (apsymdxnsrr73, 77);
  init_ft8var_ap_field (apsymdxns73, 77);
  init_ft8var_ap_field (apsymmyns1, 29);
  init_ft8var_ap_field (apsymmyns2, 58);
  init_ft8var_ap_field (apsymmynsrr73, 77);
  init_ft8var_ap_field (apsymmyns73, 77);
  init_ft8var_ap_field (apsymdxstd, 58);
  init_ft8var_ap_field (apsymdxnsr73, 77);
  init_ft8var_ap_field (apsymdxns732, 77);
  init_ft8var_ap_field (apsymmynsrrr, 77);

  QString const my = normalized_call_from_fixed (mycall, 12);
  QString const his = normalized_call_from_fixed (hiscall, 12);
  QString const my_base = normalized_call_from_fixed (mybcall, 12);
  QString const his_base = normalized_call_from_fixed (hisbcall, 12);
  QString const grid4 = from_fixed_chars (hisgrid4, 4).trimmed ().toUpper ();
  bool const hound = lhound != 0;
  bool const my_standard = lmycallstd != 0;
  bool const his_standard = lhiscallstd != 0;
  bool const no_hiscall = his.size () < 3;
  QString const his_target = no_hiscall ? my : his;

  std::array<char, 13> my_context {};
  std::array<char, 13> his_context {};
  to_fixed_chars (my, my_context.data (), 13);
  to_fixed_chars (his, his_context.data (), 13);
  legacy_pack77_set_context_c (my_context.data (), his_context.data ());

  if (his.size () >= 3)
    {
      QString cq_message = his_standard && grid4.size () == 4
                               ? QStringLiteral ("CQ %1 %2").arg (his, grid4)
                               : QStringLiteral ("CQ %1").arg (his);
      encode_ft8var_ap_message (cq_message, his_standard ? 1 : 4, apcqsym, 77, false);

      if (his_standard && !my_standard)
        {
          encode_ft8var_ap_message (QStringLiteral ("%1 %1 RRR").arg (his),
                                    1, apsymdxstd, 58, false);
        }

      if (!hound && !his_standard)
        {
          encode_ft8var_ap_message (QStringLiteral ("<W9XYZ> %1 RR73").arg (his),
                                    4, apsymdxnsrr73, 77, false);
          encode_ft8var_ap_message (QStringLiteral ("<W9XYZ> %1 73").arg (his),
                                    4, apsymdxns73, 77, false);
        }
    }

  if (!hound && !my_standard && my.size () >= 3)
    {
      if (his_standard)
        {
          encode_ft8var_ap_message (QStringLiteral ("<%1> %2 -15").arg (my, his),
                                    1, apsymmyns2, 58, false);
          encode_ft8var_ap_message (QStringLiteral ("%1 <%2> RR73").arg (my, his),
                                    4, apsymmynsrr73, 77, false);
          encode_ft8var_ap_message (QStringLiteral ("%1 <%2> 73").arg (my, his),
                                    4, apsymmyns73, 77, false);
          encode_ft8var_ap_message (QStringLiteral ("%1 <%2> RRR").arg (my, his),
                                    4, apsymmynsrrr, 77, false);
        }
      else if (no_hiscall)
        {
          encode_ft8var_ap_message (QStringLiteral ("<%1> ZZ1ZZZ -15").arg (my),
                                    1, apsymmyns1, 29, false);
        }
    }

  if (my.size () < 3)
    {
      return;
    }

  if (!hound && my_standard && (his_standard || no_hiscall))
    {
      if (encode_ft8var_ap_message (QStringLiteral ("%1 %2 RRR").arg (my, his_target),
                                    1, apsym, 58, false)
          && no_hiscall)
        {
          apsym[29] = 99;
        }
    }

  if (hound)
    {
      if (my_base.size () >= 3 && his_base.size () >= 3)
        {
          encode_ft8var_ap_message (QStringLiteral ("%1 %2 -15").arg (my_base, his_base),
                                    1, apsym, 58, false);
        }
      else if (apsym)
        {
          apsym[29] = 99;
        }

      encode_ft8var_ap_message (QStringLiteral ("%1 RR73; %1 <%2> -16").arg (my, his_target),
                                0, apsymsp, 66, true);
    }

  if (!hound && my_standard && !his_standard && his.size () >= 3)
    {
      encode_ft8var_ap_message (QStringLiteral ("%1 <%2> -16").arg (my, his),
                                1, apsymdxns1, 58, false);
      encode_ft8var_ap_message (QStringLiteral ("<%1> %2 RRR").arg (my, his),
                                4, apsymdxnsrrr, 77, false);
      encode_ft8var_ap_message (QStringLiteral ("<%1> %2 RR73").arg (my, his),
                                4, apsymdxnsr73, 77, false);
      encode_ft8var_ap_message (QStringLiteral ("<%1> %2 73").arg (my, his),
                                4, apsymdxns732, 77, false);
    }
}

int ftx_encode_ft8var_message_c (char const message37[37], char const mycall[12],
                                 char const hiscall[12], int* i3_out, int* n3_out,
                                 char msgsent_out[37], signed char msgbits_out[77],
                                 int itone_out[79])
{
  if (i3_out) *i3_out = -1;
  if (n3_out) *n3_out = -1;
  if (msgsent_out) std::fill_n (msgsent_out, 37, ' ');
  if (msgbits_out) std::fill_n (msgbits_out, 77, 0);
  if (itone_out) std::fill_n (itone_out, 79, 0);
  if (!message37 || !mycall || !hiscall || !msgsent_out || !msgbits_out || !itone_out)
    {
      return 0;
    }

  std::array<char, 13> my_context {};
  std::array<char, 13> his_context {};
  QString const my = normalized_call_from_fixed (mycall, 12);
  QString const his = normalized_call_from_fixed (hiscall, 12);
  to_fixed_chars (my, my_context.data (), 13);
  to_fixed_chars (his, his_context.data (), 13);
  legacy_pack77_set_context_c (my_context.data (), his_context.data ());

  char c77[77];
  char packed_msgsent[37];
  int i3 = -1;
  int n3 = -1;
  bool pack_ok = false;
  legacy_pack77_pack_c (message37, &i3, &n3, c77, packed_msgsent, &pack_ok, 0);
  if (!pack_ok)
    {
      to_fixed_chars (QStringLiteral ("*** bad message ***"), msgsent_out, 37);
      return 0;
    }

  if (i3_out) *i3_out = i3;
  if (n3_out) *n3_out = n3;

  bool unpack_ok = false;
  legacy_pack77_unpack_c (c77, 0, msgsent_out, &unpack_ok);
  if (!unpack_ok)
    {
      std::fill_n (msgbits_out, 77, 0);
      std::fill_n (itone_out, 79, 0);
      to_fixed_chars (QStringLiteral ("*** bad message ***"), msgsent_out, 37);
      return 0;
    }

  MessageBits77 bits {};
  for (int i = 0; i < 77; ++i)
    {
      unsigned char const bit = c77[i] == '1' ? 1u : 0u;
      bits[static_cast<size_t> (i)] = bit;
      msgbits_out[i] = static_cast<signed char> (bit);
    }

  QVector<int> const tones = map_ft8_tones (bits);
  if (tones.size () < 79)
    {
      std::fill_n (msgbits_out, 77, 0);
      std::fill_n (itone_out, 79, 0);
      to_fixed_chars (QStringLiteral ("*** bad message ***"), msgsent_out, 37);
      return 0;
    }

  for (int i = 0; i < 79; ++i)
    {
      itone_out[i] = tones.at (i);
  }
  return 1;
}

int ftx_encode_ft8var_short_message_c (char const message37[37], int* i3_out,
                                       int* n3_out, char msgsent_out[37],
                                       signed char msgbits_out[77], int itone_out[79])
{
  if (i3_out) *i3_out = -1;
  if (n3_out) *n3_out = -1;
  if (msgsent_out) std::fill_n (msgsent_out, 37, ' ');
  if (msgbits_out) std::fill_n (msgbits_out, 77, 0);
  if (itone_out) std::fill_n (itone_out, 79, 0);
  if (!message37 || !msgsent_out || !msgbits_out || !itone_out)
    {
      return 0;
    }

  legacy_pack77_reset_context_c ();

  char c77[77];
  char packed_msgsent[37];
  int i3 = -1;
  int n3 = -1;
  bool pack_ok = false;
  legacy_pack77_pack_c (message37, &i3, &n3, c77, packed_msgsent, &pack_ok, 0);
  if (!pack_ok)
    {
      to_fixed_chars (QStringLiteral ("*** bad message ***"), msgsent_out, 37);
      return 0;
    }

  if (i3_out) *i3_out = i3;
  if (n3_out) *n3_out = n3;

  bool unpack_ok = false;
  legacy_pack77_unpack_c (c77, 0, msgsent_out, &unpack_ok);
  if (!unpack_ok)
    {
      std::fill_n (msgbits_out, 77, 0);
      std::fill_n (itone_out, 79, 0);
      to_fixed_chars (QStringLiteral ("*** bad message ***"), msgsent_out, 37);
      return 0;
    }

  MessageBits77 bits {};
  for (int i = 0; i < 77; ++i)
    {
      unsigned char const bit = c77[i] == '1' ? 1u : 0u;
      bits[static_cast<size_t> (i)] = bit;
      msgbits_out[i] = static_cast<signed char> (bit);
    }

  QVector<int> const tones = map_ft8_tones (bits);
  if (tones.size () < 79)
    {
      std::fill_n (msgbits_out, 77, 0);
      std::fill_n (itone_out, 79, 0);
      to_fixed_chars (QStringLiteral ("*** bad message ***"), msgsent_out, 37);
      return 0;
    }

  for (int i = 0; i < 79; ++i)
    {
      itone_out[i] = tones.at (i);
    }
  return 1;
}

int ftx_prepare_ft8var_tone8_c (char const mycall[12], char const hiscall[12],
                                char const mybcall[12], char const hisbcall[12],
                                int lhound, int lmycallstd, int lhiscallstd,
                                char msg_out[56 * 37], int idtone56_out[56 * 58],
                                int itone56_out[56 * 79], int idtonecqdxcns_out[58],
                                int idtonedxcns73_out[58], int idtonefox73_out[58],
                                int idtonespec_out[58], int first_itone_out[79],
                                int* has_synce_out)
{
  if (has_synce_out) *has_synce_out = 0;
  if (msg_out) std::fill_n (msg_out, 56 * 37, ' ');
  if (idtone56_out) std::fill_n (idtone56_out, 56 * 58, 0);
  if (itone56_out) std::fill_n (itone56_out, 56 * 79, 0);
  if (idtonecqdxcns_out) std::fill_n (idtonecqdxcns_out, 58, 0);
  if (idtonedxcns73_out) std::fill_n (idtonedxcns73_out, 58, 0);
  if (idtonefox73_out) std::fill_n (idtonefox73_out, 58, 0);
  if (idtonespec_out) std::fill_n (idtonespec_out, 58, 0);
  if (first_itone_out) std::fill_n (first_itone_out, 79, 0);
  if (!mycall || !hiscall || !mybcall || !hisbcall || !msg_out || !idtone56_out || !itone56_out
      || !idtonecqdxcns_out || !idtonedxcns73_out || !idtonefox73_out || !idtonespec_out
      || !first_itone_out || !has_synce_out)
    {
      return 0;
    }

  static std::array<char const*, 56> const rpt {{
    "-01 ", "-02 ", "-03 ", "-04 ", "-05 ", "-06 ", "-07 ", "-08 ", "-09 ", "-10 ",
    "-11 ", "-12 ", "-13 ", "-14 ", "-15 ", "-16 ", "-17 ", "-18 ", "-19 ", "-20 ",
    "-21 ", "-22 ", "-23 ", "-24 ", "-25 ", "-26 ", "R-01", "R-02", "R-03", "R-04",
    "R-05", "R-06", "R-07", "R-08", "R-09", "R-10", "R-11", "R-12", "R-13", "R-14",
    "R-15", "R-16", "R-17", "R-18", "R-19", "R-20", "R-21", "R-22", "R-23", "R-24",
    "R-25", "R-26", "AA00", "RRR ", "RR73", "73  "
  }};

  QString const my = normalized_call_from_fixed (mycall, 12);
  QString const his = normalized_call_from_fixed (hiscall, 12);
  QString const my_base = normalized_call_from_fixed (mybcall, 12);
  QString const his_base = normalized_call_from_fixed (hisbcall, 12);
  QString const my_bracketed = QStringLiteral ("<%1>").arg (my);
  QString const his_bracketed = QStringLiteral ("<%1>").arg (his);
  bool const hound = lhound != 0;
  bool const my_standard = lmycallstd != 0;
  bool const his_standard = lhiscallstd != 0;

  std::array<char, 12> my_fixed {};
  std::array<char, 12> his_fixed {};
  std::fill (my_fixed.begin (), my_fixed.end (), ' ');
  std::fill (his_fixed.begin (), his_fixed.end (), ' ');
  to_fixed_chars (my, my_fixed.data (), 12);
  to_fixed_chars (his, his_fixed.data (), 12);

  auto encode_with_context = [&] (QString const& text, std::array<int, 79>& tones) {
    QByteArray const fixed = to_fixed_37 (text);
    std::array<char, 37> msgsent {};
    std::array<signed char, 77> msgbits {};
    int i3 = -1;
    int n3 = -1;
    tones.fill (0);
    return ftx_encode_ft8var_message_c (fixed.constData (), my_fixed.data (), his_fixed.data (),
                                        &i3, &n3, msgsent.data (), msgbits.data (), tones.data ()) != 0;
  };

  auto store_sparse = [] (std::array<int, 79> const& tones, int* sparse) {
    for (int col = 0; col < 29; ++col)
      {
        sparse[col] = tones[7 + col];
        sparse[29 + col] = tones[43 + col];
      }
  };

  auto store_row = [&] (int row, QString const& text, std::array<int, 79> const& tones) {
    QByteArray const fixed = to_fixed_37 (text);
    std::copy_n (fixed.constData (), 37, msg_out + row * 37);
    for (int col = 0; col < 29; ++col)
      {
        idtone56_out[row + 56 * col] = tones[7 + col];
        idtone56_out[row + 56 * (29 + col)] = tones[43 + col];
      }
    for (int col = 0; col < 79; ++col)
      {
        itone56_out[row + 56 * col] = tones[col];
      }
  };

  if (hound && my_base.size () >= 3 && his_base.size () >= 3)
    {
      std::array<int, 79> tones {};
      encode_with_context (QStringLiteral ("%1 %2 RR73").arg (my_base, his_base), tones);
      store_sparse (tones, idtonefox73_out);
      encode_with_context (QStringLiteral ("%1 RR73; %1 <%2> -12").arg (my, his), tones);
      store_sparse (tones, idtonespec_out);
    }

  if (!his_standard && his.size () >= 3)
    {
      std::array<int, 79> tones {};
      encode_with_context (QStringLiteral ("CQ %1").arg (his), tones);
      store_sparse (tones, idtonecqdxcns_out);
      encode_with_context (QStringLiteral ("<AA1AAA> %1 73").arg (his), tones);
      store_sparse (tones, idtonedxcns73_out);
    }

  if (!his_standard && !my_standard)
    {
      return 1;
    }

  auto capture_first = [&] (std::array<int, 79> const& tones) {
    std::copy (tones.begin (), tones.end (), first_itone_out);
    *has_synce_out = 1;
  };

  if (his_standard && my_standard)
    {
      for (int i = 0; i < 56; ++i)
        {
          std::array<int, 79> tones {};
          QString const text = QStringLiteral ("%1 %2 %3").arg (my, his, QLatin1String (rpt[static_cast<size_t> (i)]));
          encode_with_context (text, tones);
          if (i == 0) capture_first (tones);
          store_row (i, text, tones);
        }
      return 1;
    }

  if (!his_standard && my_standard)
    {
      for (int i = 0; i < 52; ++i)
        {
          std::array<int, 79> tones {};
          encode_with_context (QStringLiteral ("%1 %2 %3").arg (my, his_bracketed, QLatin1String (rpt[static_cast<size_t> (i)])),
                               tones);
          if (i == 0) capture_first (tones);
          store_row (i, QStringLiteral ("%1 %2 %3").arg (my, his, QLatin1String (rpt[static_cast<size_t> (i)])), tones);
        }
      {
        std::array<int, 79> tones {};
        QString const display = QStringLiteral ("%1 %2").arg (my_bracketed, his);
        encode_with_context (display, tones);
        store_row (52, display, tones);
      }
      for (int i = 53; i < 56; ++i)
        {
          std::array<int, 79> tones {};
          encode_with_context (QStringLiteral ("%1 %2 %3").arg (my_bracketed, his, QLatin1String (rpt[static_cast<size_t> (i)])),
                               tones);
          store_row (i, QStringLiteral ("%1 %2 %3").arg (my, his, QLatin1String (rpt[static_cast<size_t> (i)])), tones);
        }
      return 1;
    }

  for (int i = 0; i < 52; ++i)
    {
      std::array<int, 79> tones {};
      encode_with_context (QStringLiteral ("%1 %2 %3").arg (my_bracketed, his, QLatin1String (rpt[static_cast<size_t> (i)])),
                           tones);
      if (i == 0) capture_first (tones);
      store_row (i, QStringLiteral ("%1 %2 %3").arg (my, his, QLatin1String (rpt[static_cast<size_t> (i)])), tones);
    }
  {
    std::array<int, 79> tones {};
    QString const display = QStringLiteral ("%1 %2").arg (my_bracketed, his);
    encode_with_context (display, tones);
    store_row (52, display, tones);
  }
  for (int i = 53; i < 56; ++i)
    {
      std::array<int, 79> tones {};
      encode_with_context (QStringLiteral ("%1 %2 %3").arg (my, his_bracketed, QLatin1String (rpt[static_cast<size_t> (i)])),
                           tones);
      store_row (i, QStringLiteral ("%1 %2 %3").arg (my, his, QLatin1String (rpt[static_cast<size_t> (i)])), tones);
    }

  return 1;
}

int ftx_prepare_ft8var_mycall_tone_c (char const mycall[12], int idtonemyc_out[58])
{
  if (idtonemyc_out) std::fill_n (idtonemyc_out, 58, 0);
  if (!mycall || !idtonemyc_out)
    {
      return 0;
    }

  QString const my = normalized_call_from_fixed (mycall, 12);
  std::array<char, 12> my_fixed {};
  std::array<char, 12> his_fixed {};
  std::fill (my_fixed.begin (), my_fixed.end (), ' ');
  std::fill (his_fixed.begin (), his_fixed.end (), ' ');
  to_fixed_chars (my, my_fixed.data (), 12);

  QByteArray const fixed = to_fixed_37 (QStringLiteral ("%1 AA1AAA FN25").arg (my));
  std::array<char, 37> msgsent {};
  std::array<signed char, 77> msgbits {};
  std::array<int, 79> tones {};
  int i3 = -1;
  int n3 = -1;
  if (ftx_encode_ft8var_message_c (fixed.constData (), my_fixed.data (), his_fixed.data (),
                                   &i3, &n3, msgsent.data (), msgbits.data (), tones.data ()) == 0)
    {
      return 0;
    }

  for (int col = 0; col < 29; ++col)
    {
      idtonemyc_out[col] = tones[7 + col];
      idtonemyc_out[29 + col] = tones[43 + col];
  }
  return 1;
}

int ftx_prepare_ft8var_tonesd_c (char const msgd[37], int lcq,
                                 char msgsd76_out[76 * 37], int idtone76_out[76 * 58],
                                 int itone76_out[76 * 79], int build_itone_out[79],
                                 int* build_cq_out)
{
  if (msgsd76_out) std::fill_n (msgsd76_out, 76 * 37, ' ');
  if (idtone76_out) std::fill_n (idtone76_out, 76 * 58, 0);
  if (itone76_out) std::fill_n (itone76_out, 76 * 79, 0);
  if (build_itone_out) std::fill_n (build_itone_out, 79, 0);
  if (build_cq_out) *build_cq_out = lcq != 0 ? 1 : 0;
  if (!msgd || !msgsd76_out || !idtone76_out || !itone76_out || !build_itone_out || !build_cq_out)
    {
      return 0;
    }

  static std::array<char const*, 75> const rpt {{
    "+09 ", "+08 ", "+07 ", "+06 ", "+05 ", "+04 ", "+03 ", "+02 ", "+01 ", "+00 ",
    "-01 ", "-02 ", "-03 ", "-04 ", "-05 ", "-06 ", "-07 ", "-08 ", "-09 ", "-10 ",
    "-11 ", "-12 ", "-13 ", "-14 ", "-15 ", "-16 ", "-17 ", "-18 ", "-19 ", "-20 ",
    "-21 ", "-22 ", "-23 ", "-24 ", "-25 ", "-26 ", "R+09", "R+08", "R+07", "R+06",
    "R+05", "R+04", "R+03", "R+02", "R+01", "R+00", "R-01", "R-02", "R-03", "R-04",
    "R-05", "R-06", "R-07", "R-08", "R-09", "R-10", "R-11", "R-12", "R-13", "R-14",
    "R-15", "R-16", "R-17", "R-18", "R-19", "R-20", "R-21", "R-22", "R-23", "R-24",
    "R-25", "R-26", "RRR ", "RR73", "73  "
  }};

  auto encode_short = [&] (QString const& text, std::array<int, 79>& tones) {
    QByteArray const fixed = to_fixed_37 (text);
    std::array<char, 37> msgsent {};
    std::array<signed char, 77> msgbits {};
    int i3 = -1;
    int n3 = -1;
    tones.fill (0);
    return ftx_encode_ft8var_short_message_c (fixed.constData (), &i3, &n3, msgsent.data (),
                                              msgbits.data (), tones.data ()) != 0;
  };

  auto store_sparse = [] (std::array<int, 79> const& tones, int row, int* idtone76, int* itone76) {
    for (int col = 0; col < 29; ++col)
      {
        idtone76[row + 76 * col] = tones[7 + col];
        idtone76[row + 76 * (29 + col)] = tones[43 + col];
      }
    for (int col = 0; col < 79; ++col)
      {
        itone76[row + 76 * col] = tones[col];
      }
  };

  QString const text = from_fixed_chars (msgd, 37).trimmed ().toUpper ();
  if (lcq != 0)
    {
      std::array<int, 79> tones {};
      encode_short (text, tones);
      std::copy (tones.begin (), tones.end (), build_itone_out);
      *build_cq_out = 1;
      return 1;
    }

  QString c1;
  QString c2;
  QString grid = QStringLiteral ("AA00");
  QStringList const words = text.split (QLatin1Char (' '), Qt::SkipEmptyParts);
  if (words.size () >= 2 && words[0].size () <= 12 && words[1].size () <= 12)
    {
      c1 = words[0];
      c2 = words[1];
    }

  bool const lr73 = text.contains (QStringLiteral (" RR73")) || text.contains (QStringLiteral (" 73"));
  if (!lr73 && words.size () >= 3)
    {
      QString const token = words[2].toUpper ();
      if (token.size () == 4 && !token.startsWith (QStringLiteral ("R+"))
          && !token.startsWith (QStringLiteral ("R-")))
        {
          grid = token;
        }
    }

  for (int i = 0; i < 75; ++i)
    {
      std::array<int, 79> tones {};
      QString const message = QStringLiteral ("%1 %2 %3").arg (c1, c2, QLatin1String (rpt[static_cast<size_t> (i)]));
      QByteArray const fixed = to_fixed_37 (message);
      std::copy_n (fixed.constData (), 37, msgsd76_out + i * 37);
      encode_short (message, tones);
      if (i == 0)
        {
          std::copy (tones.begin (), tones.end (), build_itone_out);
        }
      store_sparse (tones, i, idtone76_out, itone76_out);
    }

  {
    std::array<int, 79> tones {};
    QString const message = QStringLiteral ("%1 %2 %3").arg (c1, c2, grid);
    QByteArray const fixed = to_fixed_37 (message);
    std::copy_n (fixed.constData (), 37, msgsd76_out + 75 * 37);
    encode_short (message, tones);
    store_sparse (tones, 75, idtone76_out, itone76_out);
  }

  *build_cq_out = 0;
  return 1;
}

int ftx_ft8sd1var_c (float const* s8, int const* itone_in, char const msgd[37],
                     char const mycall[12], int lcq, char msg37_out[37], int itone_out[79])
{
  if (msg37_out) std::fill_n (msg37_out, 37, ' ');
  if (itone_out) std::fill_n (itone_out, 79, 0);
  if (!s8 || !itone_in || !msgd || !mycall || !msg37_out || !itone_out)
    {
      return 0;
    }

  auto s8_at = [&] (int row, int col_1based) {
    return s8[row + 8 * (col_1based - 1)];
  };
  auto set_s8 = [&] (std::array<float, 8 * 79>& buffer, int row, int col_1based, float value) {
    buffer[static_cast<size_t> (row + 8 * (col_1based - 1))] = value;
  };
  auto get_s8 = [&] (std::array<float, 8 * 79> const& buffer, int row, int col_1based) {
    return buffer[static_cast<size_t> (row + 8 * (col_1based - 1))];
  };
  auto best_row = [&] (std::array<float, 8 * 79> const& buffer, int col_1based, bool zero_after,
                       std::array<float, 8 * 79>* writable) {
    int best = 0;
    float best_value = get_s8 (buffer, 0, col_1based);
    for (int row = 1; row < 8; ++row)
      {
        float const value = get_s8 (buffer, row, col_1based);
        if (value > best_value)
          {
            best_value = value;
            best = row;
          }
      }
    if (zero_after && writable)
      {
        set_s8 (*writable, best, col_1based, 0.0f);
      }
    return best;
  };
  auto encode_short = [&] (QString const& text, std::array<int, 79>& tones) {
    QByteArray const fixed = to_fixed_37 (text);
    std::array<char, 37> msgsent {};
    std::array<signed char, 77> msgbits {};
    int i3 = -1;
    int n3 = -1;
    tones.fill (0);
    return ftx_encode_ft8var_short_message_c (fixed.constData (), &i3, &n3, msgsent.data (),
                                              msgbits.data (), tones.data ()) != 0;
  };
  auto sparse_tones = [] (std::array<int, 79> const& tones) {
    std::array<int, 58> sparse {};
    for (int i = 0; i < 29; ++i)
      {
        sparse[static_cast<size_t> (i)] = tones[7 + i];
        sparse[static_cast<size_t> (29 + i)] = tones[43 + i];
      }
    return sparse;
  };
  auto match_counts = [] (std::array<int, 58> const& expected, std::array<int, 58> const& observed) {
    std::pair<int, int> counts {0, 0};
    for (int i = 0; i < 58; ++i)
      {
        if (expected[static_cast<size_t> (i)] == observed[static_cast<size_t> (i)])
          {
            ++counts.first;
            if (i >= 25)
              {
                ++counts.second;
              }
          }
      }
    return counts;
  };

  QString const text = from_fixed_chars (msgd, 37).trimmed ().toUpper ();
  QString const my = normalized_call_from_fixed (mycall, 12);
  QStringList const words = text.split (QLatin1Char (' '), Qt::SkipEmptyParts);

  bool lr73 = false;
  bool lgrid = false;
  std::array<QString, 4> variants {};
  std::array<std::array<int, 58>, 4> sparse_variants {};
  std::array<std::array<int, 79>, 4> tones_variants {};
  std::array<int, 58> sparse {};

  if (lcq == 0)
    {
      if (words.size () < 2 || words[0].size () > 12 || words[1].size () > 12)
        {
          return 0;
        }
      QString const c1 = words[0];
      QString const c2 = words[1];
      if (c1.size () < 3 || c2.size () < 3 || c2 == my)
        {
          return 0;
        }
      lr73 = text.contains (QStringLiteral (" RR73")) || text.contains (QStringLiteral (" 73"));
      if (!lr73 && words.size () >= 3)
        {
          QString const token = words[2];
          if (token.size () == 4 && !token.startsWith (QStringLiteral ("R+"))
              && !token.startsWith (QStringLiteral ("R-")))
            {
              lgrid = true;
            }
        }
      if (!lgrid && !lr73)
        {
          variants[0] = text;
          variants[1] = QStringLiteral ("%1 %2 RRR").arg (c1, c2);
          variants[2] = QStringLiteral ("%1 %2 RR73").arg (c1, c2);
          variants[3] = QStringLiteral ("%1 %2 73").arg (c1, c2);
          for (int i = 0; i < 4; ++i)
            {
              if (!encode_short (variants[static_cast<size_t> (i)], tones_variants[static_cast<size_t> (i)]))
                {
                  return 0;
                }
              sparse_variants[static_cast<size_t> (i)] = sparse_tones (tones_variants[static_cast<size_t> (i)]);
            }
        }
    }

  if (lcq != 0 || lgrid || lr73)
    {
      std::array<int, 79> tones {};
      if (!encode_short (text, tones))
        {
          return 0;
        }
      sparse = sparse_tones (tones);
      std::copy_n (itone_in, 79, itone_out);
    }

  std::array<float, 8 * 79> s8_copy {};
  for (int col = 1; col <= 79; ++col)
    {
      for (int row = 0; row < 8; ++row)
        {
          s8_copy[static_cast<size_t> (row + 8 * (col - 1))] = s8_at (row, col);
        }
    }

  std::array<int, 58> observed {};
  std::array<bool, 58> matched {};
  for (int i = 1; i <= 58; ++i)
    {
      int const col = i <= 29 ? i + 7 : i + 14;
      int const best = best_row (s8_copy, col, true, &s8_copy);
      observed[static_cast<size_t> (i - 1)] = best;
    }

  int best_variant = -1;
  int nmatch1 = 0;
  int ncrcpaty1 = 0;
  if (lcq != 0 || lgrid || lr73)
    {
      auto const counts = match_counts (sparse, observed);
      nmatch1 = counts.first;
      ncrcpaty1 = counts.second;
      if (nmatch1 > 29 && ncrcpaty1 > 10)
        {
          to_fixed_chars (text, msg37_out, 37);
          return 1;
        }
    }
  else
    {
      int best_match = -1;
      int best_crc = -1;
      for (int i = 0; i < 4; ++i)
        {
          auto const counts = match_counts (sparse_variants[static_cast<size_t> (i)], observed);
          if (counts.first > best_match)
            {
              best_match = counts.first;
              best_crc = counts.second;
              best_variant = i;
            }
        }
      if (best_variant < 0)
        {
          return 0;
        }
      nmatch1 = best_match;
      ncrcpaty1 = best_crc;
      sparse = sparse_variants[static_cast<size_t> (best_variant)];
      if (nmatch1 > 29 && ncrcpaty1 > 10)
        {
          to_fixed_chars (variants[static_cast<size_t> (best_variant)], msg37_out, 37);
          std::copy (tones_variants[static_cast<size_t> (best_variant)].begin (),
                     tones_variants[static_cast<size_t> (best_variant)].end (),
                     itone_out);
          return 1;
        }
    }

  if (nmatch1 < 22)
    {
      return 0;
    }

  for (int i = 0; i < 58; ++i)
    {
      matched[static_cast<size_t> (i)] = sparse[static_cast<size_t> (i)] == observed[static_cast<size_t> (i)];
    }
  for (int i = 1; i <= 58; ++i)
    {
      if (matched[static_cast<size_t> (i - 1)])
        {
          continue;
        }
      int const col = i <= 29 ? i + 7 : i + 14;
      observed[static_cast<size_t> (i - 1)] = best_row (s8_copy, col, false, nullptr);
    }

  auto const counts2 = match_counts (sparse, observed);
  if (counts2.first <= 41 || counts2.second <= 19)
    {
      return 0;
    }

  if (lcq != 0 || lgrid || lr73)
    {
      to_fixed_chars (text, msg37_out, 37);
      return 1;
    }

  to_fixed_chars (variants[static_cast<size_t> (best_variant)], msg37_out, 37);
  std::copy (tones_variants[static_cast<size_t> (best_variant)].begin (),
             tones_variants[static_cast<size_t> (best_variant)].end (),
             itone_out);
  return 1;
}

int ftx_ft8sdvar_c (float const* s8, float srr, int const* itone_in, char const msgd[37],
                    char const mycall[12], int lcq, char msg37_out[37], int itone_out[79])
{
  if (msg37_out) std::fill_n (msg37_out, 37, ' ');
  if (itone_out) std::fill_n (itone_out, 79, 0);
  if (!s8 || !itone_in || !msgd || !mycall || !msg37_out || !itone_out)
    {
      return 0;
    }

  auto s8_at = [&] (int row, int col_1based) {
    return s8[row + 8 * (col_1based - 1)];
  };
  auto set_s8 = [&] (std::array<float, 8 * 79>& buffer, int row, int col_1based, float value) {
    buffer[static_cast<size_t> (row + 8 * (col_1based - 1))] = value;
  };
  auto get_s8 = [&] (std::array<float, 8 * 79> const& buffer, int row, int col_1based) {
    return buffer[static_cast<size_t> (row + 8 * (col_1based - 1))];
  };
  auto best_row = [&] (std::array<float, 8 * 79> const& buffer, int col_1based,
                       std::array<float, 8 * 79>* writable) {
    int best = 0;
    float best_value = get_s8 (buffer, 0, col_1based);
    for (int row = 1; row < 8; ++row)
      {
        float const value = get_s8 (buffer, row, col_1based);
        if (value > best_value)
          {
            best_value = value;
            best = row;
          }
      }
    if (writable)
      {
        set_s8 (*writable, best, col_1based, 0.0f);
      }
    return best;
  };
  auto encode_short = [&] (QString const& text, std::array<int, 79>& tones) {
    QByteArray const fixed = to_fixed_37 (text);
    std::array<char, 37> msgsent {};
    std::array<signed char, 77> msgbits {};
    int i3 = -1;
    int n3 = -1;
    tones.fill (0);
    return ftx_encode_ft8var_short_message_c (fixed.constData (), &i3, &n3, msgsent.data (),
                                              msgbits.data (), tones.data ()) != 0;
  };
  auto sparse_tones = [] (std::array<int, 79> const& tones) {
    std::array<int, 58> sparse {};
    for (int i = 0; i < 29; ++i)
      {
        sparse[static_cast<size_t> (i)] = tones[7 + i];
        sparse[static_cast<size_t> (29 + i)] = tones[43 + i];
      }
    return sparse;
  };
  auto match_counts = [] (std::array<int, 58> const& expected, std::array<int, 58> const& observed) {
    std::pair<int, int> counts {0, 0};
    for (int i = 0; i < 58; ++i)
      {
        if (expected[static_cast<size_t> (i)] == observed[static_cast<size_t> (i)])
          {
            ++counts.first;
            if (i >= 25)
              {
                ++counts.second;
              }
          }
      }
    return counts;
  };

  QString const text = from_fixed_chars (msgd, 37).trimmed ().toUpper ();
  QString const my = normalized_call_from_fixed (mycall, 12);
  if (text.contains (QStringLiteral (" RR73")) || text.contains (QStringLiteral (" 73")))
    {
      return 0;
    }
  if (!text.startsWith (QStringLiteral ("CQ ")) && !text.contains (QLatin1Char ('-'))
      && !text.contains (QLatin1Char ('+')) && !text.contains (QStringLiteral (" RRR")))
    {
      return 0;
    }

  std::array<QString, 4> variants {};
  std::array<std::array<int, 58>, 4> sparse_variants {};
  std::array<std::array<int, 79>, 4> tone_variants {};
  std::array<int, 58> sparse {};
  std::copy_n (itone_in, 79, itone_out);

  if (lcq == 0)
    {
      QStringList const words = text.split (QLatin1Char (' '), Qt::SkipEmptyParts);
      if (words.size () < 2 || words[0].size () > 12 || words[1].size () > 12)
        {
          return 0;
        }
      QString const c1 = words[0];
      QString const c2 = words[1];
      if (c1.size () < 3 || c2.size () < 3 || c2 == my)
        {
          return 0;
        }
      variants[0] = text;
      variants[1] = QStringLiteral ("%1 %2 RRR").arg (c1, c2);
      variants[2] = QStringLiteral ("%1 %2 RR73").arg (c1, c2);
      variants[3] = QStringLiteral ("%1 %2 73").arg (c1, c2);
      for (int i = 0; i < 4; ++i)
        {
          if (!encode_short (variants[static_cast<size_t> (i)], tone_variants[static_cast<size_t> (i)]))
            {
              return 0;
            }
          sparse_variants[static_cast<size_t> (i)] = sparse_tones (tone_variants[static_cast<size_t> (i)]);
        }
    }
  else
    {
      std::array<int, 79> tones {};
      if (!encode_short (text, tones))
        {
          return 0;
        }
      sparse = sparse_tones (tones);
    }

  std::array<float, 8 * 79> s8_copy {};
  for (int col = 1; col <= 79; ++col)
    {
      for (int row = 0; row < 8; ++row)
        {
          s8_copy[static_cast<size_t> (row + 8 * (col - 1))] = s8_at (row, col);
        }
    }

  std::array<int, 58> observed {};
  std::array<bool, 58> matched {};
  for (int i = 1; i <= 58; ++i)
    {
      int const col = i <= 29 ? i + 7 : i + 14;
      observed[static_cast<size_t> (i - 1)] = best_row (s8_copy, col, &s8_copy);
    }

  int best_variant = -1;
  int nmatch1 = 0;
  int ncrcpaty1 = 0;
  int nbase1 = 0;
  if (lcq != 0)
    {
      auto const counts = match_counts (sparse, observed);
      nmatch1 = counts.first;
      ncrcpaty1 = counts.second;
      if (nmatch1 > 26)
        {
          to_fixed_chars (text, msg37_out, 37);
          return 1;
        }
    }
  else
    {
      int best_match = -1;
      int best_crc = -1;
      int best_base = -1;
      for (int i = 0; i < 4; ++i)
        {
          int matches = 0;
          int crc = 0;
          int base = 0;
          for (int k = 0; k < 58; ++k)
            {
              if (sparse_variants[static_cast<size_t> (i)][static_cast<size_t> (k)] == observed[static_cast<size_t> (k)])
                {
                  ++matches;
                  if (k < 22)
                    {
                      ++base;
                    }
                  if (k >= 25)
                    {
                      ++crc;
                    }
                }
            }
          if (matches > best_match)
            {
              best_match = matches;
              best_crc = crc;
              best_base = base;
              best_variant = i;
            }
        }
      if (best_variant < 0)
        {
          return 0;
        }
      nmatch1 = best_match;
      ncrcpaty1 = best_crc;
      nbase1 = best_base;
      sparse = sparse_variants[static_cast<size_t> (best_variant)];
      if (srr > 3.0f && nbase1 < 12)
        {
          return 0;
        }
      if (nmatch1 > 26 && ncrcpaty1 > 10)
        {
          to_fixed_chars (variants[static_cast<size_t> (best_variant)], msg37_out, 37);
          std::copy (tone_variants[static_cast<size_t> (best_variant)].begin (),
                     tone_variants[static_cast<size_t> (best_variant)].end (),
                     itone_out);
          return 1;
        }
    }

  for (int i = 0; i < 58; ++i)
    {
      matched[static_cast<size_t> (i)] = sparse[static_cast<size_t> (i)] == observed[static_cast<size_t> (i)];
    }

  auto finish = [&] (QString const& message, std::array<int, 79> const* tones) {
    to_fixed_chars (message, msg37_out, 37);
    if (tones)
      {
        std::copy (tones->begin (), tones->end (), itone_out);
      }
    return 1;
  };
  auto run_extra_pass = [&] (int current_match, int current_crc) {
    for (int i = 1; i <= 58; ++i)
      {
        if (matched[static_cast<size_t> (i - 1)])
          {
            continue;
          }
        int const col = i <= 29 ? i + 7 : i + 14;
        observed[static_cast<size_t> (i - 1)] = best_row (s8_copy, col, &s8_copy);
      }
    int next_match = current_match;
    int next_crc = current_crc;
    for (int i = 0; i < 58; ++i)
      {
        if (matched[static_cast<size_t> (i)])
          {
            continue;
          }
        if (sparse[static_cast<size_t> (i)] == observed[static_cast<size_t> (i)])
          {
            matched[static_cast<size_t> (i)] = true;
            ++next_match;
            if (i >= 25)
              {
                ++next_crc;
              }
          }
      }
    return std::pair<int, int> {next_match, next_crc};
  };

  if (nmatch1 < 16)
    {
      return 0;
    }

  auto const result_message = [&] () -> QString const& {
    return (lcq != 0) ? text : variants[static_cast<size_t> (best_variant)];
  };
  std::array<int, 79> const* result_tones = (lcq != 0) ? nullptr : &tone_variants[static_cast<size_t> (best_variant)];

  auto const counts2 = run_extra_pass (nmatch1, ncrcpaty1);
  int const nmatch2 = counts2.first;
  int const ncrcpaty2 = counts2.second;
  if (nmatch2 > 38 && ncrcpaty2 > 19)
    {
      return finish (result_message (), result_tones);
    }

  auto const counts3 = run_extra_pass (nmatch2, ncrcpaty2);
  int const nmatch3 = counts3.first;
  int const ncrcpaty3 = counts3.second;
  if (nmatch3 > 44 && ncrcpaty3 > 21)
    {
      return finish (result_message (), result_tones);
    }

  auto const counts4 = run_extra_pass (nmatch3, ncrcpaty3);
  int const nmatch4 = counts4.first;
  int const ncrcpaty4 = counts4.second;
  if (nmatch4 > 47 && ncrcpaty4 > 23)
    {
      return finish (result_message (), result_tones);
    }

  auto const counts5 = run_extra_pass (nmatch4, ncrcpaty4);
  int const nmatch5 = counts5.first;
  int const ncrcpaty5 = counts5.second;
  if (nmatch5 > 50 && (nmatch1 > 21 || nmatch2 > 31 || nmatch3 > 38 || nmatch4 > 46)
      && ncrcpaty5 > 25)
    {
      return finish (result_message (), result_tones);
    }

  auto const counts6 = run_extra_pass (nmatch5, ncrcpaty5);
  int const nmatch6 = counts6.first;
  int const ncrcpaty6 = counts6.second;
  if (nmatch6 > 54
      && (nmatch1 > 22 || nmatch2 > 27 || nmatch3 > 35 || (nmatch2 - nmatch1) > 9
          || (nmatch3 - nmatch2) > 10)
      && ncrcpaty6 > 29)
    {
      return finish (result_message (), result_tones);
    }

  return 0;
}

int ftx_ft8svar_c (float const* s8, float srr, int const* idtone56, int const* itone56,
                   char const* msg56, int nlasttx, char const lastrcvdmsg[37],
                   int lastrcvd_lstate, char const mycall[12], char const hiscall[12],
                   int nft8rxfsens, int stophint, char msg37_out[37], int itone_out[79])
{
  if (msg37_out) std::fill_n (msg37_out, 37, ' ');
  if (itone_out) std::fill_n (itone_out, 79, 0);
  if (!s8 || !idtone56 || !itone56 || !msg56 || !lastrcvdmsg || !mycall || !hiscall
      || !msg37_out || !itone_out)
    {
      return 0;
    }

  auto s8_at = [&] (int row, int col_1based) {
    return s8[row + 8 * (col_1based - 1)];
  };
  auto set_s8 = [&] (std::array<float, 8 * 79>& buffer, int row, int col_1based, float value) {
    buffer[static_cast<size_t> (row + 8 * (col_1based - 1))] = value;
  };
  auto get_s8 = [&] (std::array<float, 8 * 79> const& buffer, int row, int col_1based) {
    return buffer[static_cast<size_t> (row + 8 * (col_1based - 1))];
  };
  auto best_row = [&] (std::array<float, 8 * 79> const& buffer, int col_1based,
                       std::array<float, 8 * 79>* writable) {
    int best = 0;
    float best_value = get_s8 (buffer, 0, col_1based);
    for (int row = 1; row < 8; ++row)
      {
        float const value = get_s8 (buffer, row, col_1based);
        if (value > best_value)
          {
            best_value = value;
            best = row;
          }
      }
    if (writable)
      {
        set_s8 (*writable, best, col_1based, 0.0f);
      }
    return best;
  };
  auto sparse_at = [&] (std::array<int, 56 * 58> const& table, int row_1based, int col_1based) {
    return table[static_cast<size_t> ((row_1based - 1) + 56 * (col_1based - 1))];
  };
  auto tone_at = [&] (std::array<int, 56 * 79> const& table, int row_1based, int col_1based) {
    return table[static_cast<size_t> ((row_1based - 1) + 56 * (col_1based - 1))];
  };
  auto set_sparse = [&] (std::array<int, 56 * 58>& table, int row_1based, int col_1based, int value) {
    table[static_cast<size_t> ((row_1based - 1) + 56 * (col_1based - 1))] = value;
  };
  auto set_tone = [&] (std::array<int, 56 * 79>& table, int row_1based, int col_1based, int value) {
    table[static_cast<size_t> ((row_1based - 1) + 56 * (col_1based - 1))] = value;
  };
  auto encode_short = [&] (QString const& text, std::array<int, 79>& tones) {
    QByteArray const fixed = to_fixed_37 (text);
    std::array<char, 37> msgsent {};
    std::array<signed char, 77> msgbits {};
    int i3 = -1;
    int n3 = -1;
    tones.fill (0);
    return ftx_encode_ft8var_short_message_c (fixed.constData (), &i3, &n3, msgsent.data (),
                                              msgbits.data (), tones.data ()) != 0;
  };
  auto read_message = [&] (std::array<char, 56 * 37> const& table, int row_1based) {
    std::array<char, 37> fixed {};
    for (int i = 0; i < 37; ++i)
      {
        fixed[static_cast<size_t> (i)] = table[static_cast<size_t> (i + 37 * (row_1based - 1))];
      }
    return from_fixed_chars (fixed.data (), 37);
  };
  auto write_message = [&] (std::array<char, 56 * 37>& table, int row_1based, QString const& text) {
    QByteArray const fixed = to_fixed_37 (text);
    std::copy_n (fixed.constData (), 37, table.data () + 37 * (row_1based - 1));
  };
  auto median_triplet = [] (float a, float b, float c) {
    if ((a > b && a < c) || (a < b && a > c)) return a;
    if ((b > a && b < c) || (b < a && b > c)) return b;
    if ((c > a && c < b) || (c < a && c > b)) return c;
    return a;
  };

  QString const my = normalized_call_from_fixed (mycall, 12);
  QString const his = normalized_call_from_fixed (hiscall, 12);
  if (stophint != 0 || his.size () < 3 || nlasttx == 6 || nlasttx == 0)
    {
      return 0;
    }

  std::array<int, 56 * 58> sparse_table {};
  std::array<int, 56 * 79> tone_table {};
  std::array<char, 56 * 37> message_table {};
  std::copy_n (idtone56, sparse_table.size (), sparse_table.data ());
  std::copy_n (itone56, tone_table.size (), tone_table.data ());
  std::copy_n (msg56, message_table.size (), message_table.data ());

  QString const last_text = from_fixed_chars (lastrcvdmsg, 37);
  bool const has_last = lastrcvd_lstate != 0;
  bool lmycall = false;
  bool lhiscall = false;
  bool lrrr = false;
  bool lr73 = false;
  bool lcallingrprt = false;
  bool lastrrprt = false;
  bool lastreport = false;
  bool lgrid = false;
  int nft8rxfslow = nft8rxfsens;
  int ntresh1 = 26;
  int ntresh2 = 38;
  if (srr > 7.0f)
    {
      ntresh1 = 29;
      ntresh2 = 41;
    }

  if (has_last && last_text.trimmed () != QLatin1String (""))
    {
      lhiscall = last_text.contains (his);
      lmycall = last_text.startsWith (my);
      if (lmycall && lhiscall)
        {
          lrrr = last_text.contains (QStringLiteral (" RRR"));
          lr73 = last_text.contains (QStringLiteral (" RR73")) || last_text.contains (QStringLiteral (" 73"));
        }
    }
  else if (nlasttx == 2)
    {
      lcallingrprt = true;
    }

  if (lmycall && lhiscall)
    {
      lastreport = last_text.contains (QStringLiteral (" +")) || last_text.contains (QStringLiteral (" -"));
      lastrrprt = last_text.contains (QStringLiteral (" R+")) || last_text.contains (QStringLiteral (" R-"));
    }
  if (has_last && lmycall && lhiscall && !lastreport && !lastrrprt && !lrrr && !lr73)
    {
      lgrid = true;
    }
  if (has_last && !lmycall && lhiscall && nlasttx == 2)
    {
      lcallingrprt = true;
    }

  if (lgrid)
    {
      QString const row53 = read_message (message_table, 53);
      QString const expected = QStringLiteral ("%1 %2 AA00").arg (my, his);
      if (row53.startsWith (expected))
        {
          std::array<int, 79> tones {};
          if (!encode_short (last_text, tones))
            {
              return 0;
            }
          write_message (message_table, 53, last_text);
          for (int i = 0; i < 29; ++i)
            {
              set_sparse (sparse_table, 53, i + 1, tones[7 + i]);
              set_sparse (sparse_table, 53, i + 30, tones[43 + i]);
            }
          for (int i = 0; i < 79; ++i)
            {
              set_tone (tone_table, 53, i + 1, tones[static_cast<size_t> (i)]);
            }
        }
    }

  std::array<float, 8 * 79> s8_copy {};
  for (int col = 1; col <= 79; ++col)
    {
      for (int row = 0; row < 8; ++row)
        {
          s8_copy[static_cast<size_t> (row + 8 * (col - 1))] = s8_at (row, col);
        }
    }

  std::array<int, 58> observed {};
  std::array<bool, 58> matched {};
  for (int i = 1; i <= 58; ++i)
    {
      int const col = i <= 29 ? i + 7 : i + 14;
      observed[static_cast<size_t> (i - 1)] = best_row (s8_copy, col, &s8_copy);
    }

  int nmycall1 = 0;
  int nbase1 = 0;
  for (int k = 1; k <= 19; ++k)
    {
      if (sparse_at (sparse_table, 1, k) == observed[static_cast<size_t> (k - 1)])
        {
          if (k < 10)
            {
              ++nmycall1;
            }
          ++nbase1;
        }
    }

  int ilow = 1;
  int ihigh = 56;
  if (nlasttx == 1)
    {
      ilow = 1;
      ihigh = 26;
    }
  else if (lgrid)
    {
      ilow = 27;
      ihigh = 53;
    }
  else if (lcallingrprt)
    {
      ilow = 27;
      ihigh = 52;
    }
  else if (lastrrprt)
    {
      ilow = 27;
      ihigh = 56;
    }
  else if (lr73 || lrrr)
    {
      ilow = 54;
      ihigh = 56;
    }

  int imax = 0;
  int nmatch1 = 0;
  int ncrcpaty1 = 0;
  for (int candidate = ilow; candidate <= ihigh; ++candidate)
    {
      if (lastreport && candidate > 26 && candidate < 54)
        {
          continue;
        }
      int matches = 0;
      int crc = 0;
      for (int k = 1; k <= 58; ++k)
        {
          if (sparse_at (sparse_table, candidate, k) == observed[static_cast<size_t> (k - 1)])
            {
              ++matches;
              if (k > 25)
                {
                  ++crc;
                }
            }
        }
      if (matches > nmatch1)
        {
          imax = candidate;
          nmatch1 = matches;
          ncrcpaty1 = crc;
        }
    }
  if (imax == 0)
    {
      return 0;
    }
  if (srr > 3.0f && (nmycall1 < 5 || nbase1 < 10))
    {
      return 0;
    }
  if ((nlasttx == 1 || lcallingrprt) && nmycall1 == 0)
    {
      return 0;
    }
  if (lr73 && (imax == 53 || imax == 54))
    {
      return 0;
    }

  std::array<int, 58> sparse {};
  for (int k = 1; k <= 58; ++k)
    {
      sparse[static_cast<size_t> (k - 1)] = sparse_at (sparse_table, imax, k);
      matched[static_cast<size_t> (k - 1)] = sparse[static_cast<size_t> (k - 1)] == observed[static_cast<size_t> (k - 1)];
    }
  if (ncrcpaty1 < 8)
    {
      return 0;
    }

  std::array<float, 8 * 58> s8d {};
  auto s8d_at = [&] (int row, int col_1based) -> float& {
    return s8d[static_cast<size_t> (row + 8 * (col_1based - 1))];
  };
  for (int i = 1; i <= 58; ++i)
    {
      int const col = i <= 29 ? i + 7 : i + 14;
      for (int row = 0; row < 8; ++row)
        {
          s8d_at (row, i) = s8_at (row, col);
        }
    }

  std::array<int, 58> mrs {};
  std::array<int, 58> mrs2 {};
  std::array<int, 58> mrs3 {};
  std::array<int, 58> mrs4 {};
  for (int j = 1; j <= 58; ++j)
    {
      float s1 = -1.0e6f, s2 = -1.0e6f, s3 = -1.0e6f, s4 = -1.0e6f;
      int i1 = 0, i2 = 0, i3 = 0, i4 = 0;
      for (int row = 0; row < 8; ++row)
        {
          float const value = s8d_at (row, j);
          if (value > s1)
            {
              s1 = value;
              i1 = row;
            }
        }
      for (int row = 0; row < 8; ++row)
        {
          float const value = s8d_at (row, j);
          if (row != i1 && value > s2)
            {
              s2 = value;
              i2 = row;
            }
        }
      for (int row = 0; row < 8; ++row)
        {
          float const value = s8d_at (row, j);
          if (row != i1 && row != i2 && value > s3)
            {
              s3 = value;
              i3 = row;
            }
        }
      for (int row = 0; row < 8; ++row)
        {
          float const value = s8d_at (row, j);
          if (row != i1 && row != i2 && row != i3 && value > s4)
            {
              s4 = value;
              i4 = row;
            }
        }
      mrs[static_cast<size_t> (j - 1)] = i1;
      mrs2[static_cast<size_t> (j - 1)] = i2;
      mrs3[static_cast<size_t> (j - 1)] = i3;
      mrs4[static_cast<size_t> (j - 1)] = i4;
    }

  float ref0 = 0.0f;
  float ref0paty = 0.0f;
  float ref0mycl = 0.0f;
  for (int i = 1; i <= 58; ++i)
    {
      ref0 += s8d_at (mrs[static_cast<size_t> (i - 1)], i);
      if (i > 26)
        {
          ref0paty += s8d_at (mrs[static_cast<size_t> (i - 1)], i);
        }
      else if (i < 10)
        {
          ref0mycl += s8d_at (mrs[static_cast<size_t> (i - 1)], i);
        }
    }
  float const ref0oth = ref0mycl + ref0paty;

  int ipk = 0;
  float u1 = 0.0f;
  float u2 = 0.0f;
  float u1paty = 0.0f;
  float u2paty = 0.0f;
  float u1oth = 0.0f;
  float u2oth = 0.0f;
  for (int candidate = ilow; candidate <= ihigh; ++candidate)
    {
      if (lastreport && candidate > 26 && candidate < 54)
        {
          continue;
        }
      float psum = 0.0f;
      float psumpaty = 0.0f;
      float psumoth = 0.0f;
      float ref = ref0;
      float refpaty = ref0paty;
      float refoth = ref0oth;
      for (int j = 1; j <= 58; ++j)
        {
          int const tone = sparse_at (sparse_table, candidate, j);
          psum += s8d_at (tone, j);
          if (j > 26)
            {
              psumpaty += s8d_at (tone, j);
              psumoth += s8d_at (tone, j);
            }
          else if (j < 10)
            {
              psumoth += s8d_at (tone, j);
            }

          if (tone == mrs[static_cast<size_t> (j - 1)])
            {
              float const stmp = s8d_at (mrs2[static_cast<size_t> (j - 1)], j)
                                 - s8d_at (tone, j);
              ref += stmp;
              if (j > 26)
                {
                  refpaty += stmp;
                  refoth += stmp;
                }
              else if (j < 10)
                {
                  refoth += stmp;
                }
            }
          if (tone == mrs2[static_cast<size_t> (j - 1)])
            {
              float const stmp = s8d_at (mrs3[static_cast<size_t> (j - 1)], j)
                                 - s8d_at (mrs2[static_cast<size_t> (j - 1)], j);
              ref += stmp;
              if (j > 26)
                {
                  refpaty += stmp;
                  refoth += stmp;
                }
              else if (j < 10)
                {
                  refoth += stmp;
                }
            }
          if (tone == mrs3[static_cast<size_t> (j - 1)])
            {
              float const stmp = s8d_at (mrs4[static_cast<size_t> (j - 1)], j)
                                 - s8d_at (mrs3[static_cast<size_t> (j - 1)], j);
              ref += stmp;
              if (j > 26)
                {
                  refpaty += stmp;
                  refoth += stmp;
                }
              else if (j < 10)
                {
                  refoth += stmp;
                }
            }
        }
      float const p = psum / ref;
      float const ppaty = psumpaty / refpaty;
      float const poth = psumoth / refoth;
      if (p > u1)
        {
          u2 = u1;
          u2paty = u1paty;
          u2oth = u1oth;
          u1 = p;
          u1paty = ppaty;
          u1oth = poth;
          ipk = candidate;
        }
      else if (p > u2)
        {
          u2 = p;
          u2paty = ppaty;
          u2oth = poth;
        }
    }

  if (lastrrprt && ipk == 53)
    {
      return 0;
    }
  if (lr73 && ipk < 55)
    {
      return 0;
    }
  if (lcallingrprt || nlasttx == 1)
    {
      nft8rxfslow = 1;
    }

  float thresh = 0.0f;
  if (ipk != 0)
    {
      float const qual = 100.0f * (u1 - u2);
      float const qualp = 100.0f * (u1paty - u2paty);
      float const qualo = 100.0f * (u1oth - u2oth);
      thresh = (qual + 10.0f) * (u1 - 0.6f);
      if (thresh < 1.5f)
        {
          return 0;
        }
      float const threshp = (qualp + 10.0f) * (u1paty - 0.6f);
      float const thresho = (qualo + 10.0f) * (u1oth - 0.6f);
      if ((lcallingrprt || nlasttx == 1) && thresho < 3.43f)
        {
          return 0;
        }
      if (thresho < 2.63f || threshp < 2.45f)
        {
          return 0;
        }
      if (((nft8rxfslow == 1 && thresh > 4.0f)
           || (nft8rxfslow == 2 && thresh > 3.55f)
           || (nft8rxfslow == 3 && thresh > 3.0f))
          && qual > 2.6f && u1 > 0.77f)
        {
          std::copy_n (message_table.data () + 37 * (ipk - 1), 37, msg37_out);
          for (int i = 1; i <= 79; ++i)
            {
              itone_out[static_cast<size_t> (i - 1)] = tone_at (tone_table, ipk, i);
            }
          return 1;
        }
    }

  auto accept_candidate = [&] (int candidate) {
    std::copy_n (message_table.data () + 37 * (candidate - 1), 37, msg37_out);
    for (int i = 1; i <= 79; ++i)
      {
        itone_out[static_cast<size_t> (i - 1)] = tone_at (tone_table, candidate, i);
      }
    return 1;
  };
  if (imax == ipk && (nft8rxfslow > 1 || (nft8rxfslow == 1 && thresh > 2.7f))
      && srr < 7.0f && (ncrcpaty1 > 14 || (nmatch1 > 22 && ncrcpaty1 > 13)))
    {
      return accept_candidate (imax);
    }
  if (imax == ipk && nmatch1 > ntresh1 && ncrcpaty1 > 10)
    {
      return accept_candidate (imax);
    }
  if (nmatch1 < 16)
    {
      return 0;
    }

  auto run_extra_pass = [&] (int current_match, int current_crc) {
    for (int i = 1; i <= 58; ++i)
      {
        if (matched[static_cast<size_t> (i - 1)])
          {
            continue;
          }
        int const col = i <= 29 ? i + 7 : i + 14;
        observed[static_cast<size_t> (i - 1)] = best_row (s8_copy, col, &s8_copy);
      }
    int next_match = current_match;
    int next_crc = current_crc;
    for (int i = 0; i < 58; ++i)
      {
        if (matched[static_cast<size_t> (i)])
          {
            continue;
          }
        if (sparse[static_cast<size_t> (i)] == observed[static_cast<size_t> (i)])
          {
            matched[static_cast<size_t> (i)] = true;
            ++next_match;
            if (i >= 25)
              {
                ++next_crc;
              }
          }
      }
    return std::pair<int, int> {next_match, next_crc};
  };

  auto const counts2 = run_extra_pass (nmatch1, ncrcpaty1);
  int const nmatch2 = counts2.first;
  int const ncrcpaty2 = counts2.second;
  if (nmatch2 > ntresh2 && ncrcpaty2 > 19)
    {
      return accept_candidate (imax);
    }

  if (srr > 7.0f)
    {
      return 0;
    }

  auto const counts3 = run_extra_pass (nmatch2, ncrcpaty2);
  int const nmatch3 = counts3.first;
  int const ncrcpaty3 = counts3.second;
  if (nmatch3 > 44 && ncrcpaty3 > 21)
    {
      return accept_candidate (imax);
    }

  auto const counts4 = run_extra_pass (nmatch3, ncrcpaty3);
  int const nmatch4 = counts4.first;
  int const ncrcpaty4 = counts4.second;
  if ((nft8rxfslow == 3 || (nft8rxfslow == 2 && thresh > 2.2f))
      && nmatch4 > 47 && ncrcpaty4 > 23)
    {
      return accept_candidate (imax);
    }

  auto const counts5 = run_extra_pass (nmatch4, ncrcpaty4);
  int const nmatch5 = counts5.first;
  int const ncrcpaty5 = counts5.second;
  if ((nft8rxfslow == 3 || (nft8rxfslow == 1 && thresh > 3.4f)
       || (nft8rxfslow == 2 && thresh > 3.25f))
      && nmatch5 > 50
      && (nmatch1 > 21 || nmatch2 > 31 || nmatch3 > 38 || nmatch4 > 46)
      && ncrcpaty5 > 25)
    {
      return accept_candidate (imax);
    }

  auto const counts6 = run_extra_pass (nmatch5, ncrcpaty5);
  int const nmatch6 = counts6.first;
  int const ncrcpaty6 = counts6.second;
  if (((nft8rxfslow == 2 && thresh > 2.6f) || (nft8rxfslow == 3 && thresh > 2.22f))
      && imax == ipk && ncrcpaty6 > 26)
    {
      return accept_candidate (imax);
    }
  if (nft8rxfslow == 1)
    {
      if (nmatch6 > 54
          && (nmatch1 > 22 || nmatch2 > 27 || nmatch3 > 35 || (nmatch2 - nmatch1) > 9
              || (nmatch3 - nmatch2) > 10)
          && ncrcpaty6 > 29 && thresh > 3.15f)
        {
          return accept_candidate (imax);
        }
    }
  if (ncrcpaty6 > 29)
    {
      if (nft8rxfslow == 3 && imax == ipk && (ncrcpaty2 - ncrcpaty1) > 3
          && (ncrcpaty3 - ncrcpaty2) > 3 && (ncrcpaty4 - ncrcpaty3) > 3
          && (ncrcpaty6 - ncrcpaty5) < 6)
        {
          return accept_candidate (imax);
        }
    }

  if (msg37_out[0] == ' ')
    {
      return 0;
    }

  float snr = 0.0f;
  float snrbase = 0.0f;
  float snrpaty = 0.0f;
  float snrmycall = 0.0f;
  for (int i = 1; i <= 79; ++i)
    {
      float const xsig = s8_at (itone_out[static_cast<size_t> (i - 1)], i);
      float xsum = 0.0f;
      for (int row = 0; row < 8; ++row)
        {
          xsum += s8_at (row, i);
        }
      float const xnoi = (xsum - xsig) / 7.0f;
      float const snr1 = xsig / (xnoi + 1.0e-6f);
      snr += snr1;
      if (i > 7 && i < 34)
        {
          snrbase += snr1;
          if (i < 17)
            {
              snrmycall += snr1;
            }
        }
      if ((i > 43 && i < 73) || (i > 33 && i < 37))
        {
          snrpaty += snr1;
        }
    }
  float snrsync = snr - (snrbase + snrpaty);
  float const snrother = (snrmycall + snrpaty) / 48.0f;
  snrsync /= 21.0f;
  snrpaty /= 32.0f;
  if (lcallingrprt || nlasttx == 1)
    {
      float const soratio = snrsync / snrother;
      if (soratio > 1.29f)
        {
          std::fill_n (msg37_out, 37, ' ');
          return 0;
        }
    }
  float const spratio = snrsync / snrpaty;
  if (spratio < 0.6f || spratio > 1.25f)
    {
      std::fill_n (msg37_out, 37, ' ');
      return 0;
    }

  std::array<float, 21> xsync {};
  std::array<float, 21> xmsync {};
  std::array<float, 58> xdata {};
  std::array<float, 58> xmdata {};
  std::array<float, 79> xnoise {};
  std::array<float, 79> xmnoise {};
  for (int i = 1; i <= 7; ++i) xsync[static_cast<size_t> (i - 1)] = s8_at (itone_out[static_cast<size_t> (i - 1)], i);
  for (int i = 8; i <= 14; ++i)
    {
      int const k = i + 29;
      xsync[static_cast<size_t> (i - 1)] = s8_at (itone_out[static_cast<size_t> (k - 1)], k);
    }
  for (int i = 15; i <= 21; ++i)
    {
      int const k = i + 58;
      xsync[static_cast<size_t> (i - 1)] = s8_at (itone_out[static_cast<size_t> (k - 1)], k);
    }
  for (int i = 1; i <= 29; ++i)
    {
      int const k = i + 7;
      xdata[static_cast<size_t> (i - 1)] = s8_at (itone_out[static_cast<size_t> (k - 1)], k);
    }
  for (int i = 30; i <= 58; ++i)
    {
      int const k = i + 14;
      xdata[static_cast<size_t> (i - 1)] = s8_at (itone_out[static_cast<size_t> (k - 1)], k);
    }
  for (int i = 1; i <= 79; ++i)
    {
      int const k = (itone_out[static_cast<size_t> (i - 1)] + 4) % 8;
      xnoise[static_cast<size_t> (i - 1)] = s8_at (k, i);
    }
  for (int i = 1; i <= 19; ++i)
    {
      xmsync[static_cast<size_t> (i - 1)] = median_triplet (xsync[static_cast<size_t> (i - 1)],
                                                            xsync[static_cast<size_t> (i)],
                                                            xsync[static_cast<size_t> (i + 1)]);
    }
  xmsync[19] = xmsync[17];
  xmsync[20] = xmsync[18];
  for (int i = 1; i <= 56; ++i)
    {
      xmdata[static_cast<size_t> (i - 1)] = median_triplet (xdata[static_cast<size_t> (i - 1)],
                                                            xdata[static_cast<size_t> (i)],
                                                            xdata[static_cast<size_t> (i + 1)]);
    }
  xmdata[56] = xmdata[54];
  xmdata[57] = xmdata[55];
  for (int i = 1; i <= 77; ++i)
    {
      xmnoise[static_cast<size_t> (i - 1)] = median_triplet (xnoise[static_cast<size_t> (i - 1)],
                                                             xnoise[static_cast<size_t> (i)],
                                                             xnoise[static_cast<size_t> (i + 1)]);
    }
  xmnoise[77] = xmnoise[75];
  xmnoise[78] = xmnoise[76];

  float ssync = 0.0f;
  for (float value : xmsync) ssync += value;
  ssync /= 21.0f;
  float spaty = 0.0f;
  for (int i = 26; i < 58; ++i) spaty += xmdata[static_cast<size_t> (i)];
  spaty /= 32.0f;
  float spnoise = 0.0f;
  for (int i = 33; i <= 35; ++i) spnoise += xmnoise[static_cast<size_t> (i)];
  for (int i = 43; i <= 71; ++i) spnoise += xmnoise[static_cast<size_t> (i)];
  spnoise /= 32.0f;
  float spother = 0.0f;
  for (int i = 0; i <= 8; ++i) spother += xmdata[static_cast<size_t> (i)];
  for (int i = 26; i < 58; ++i) spother += xmdata[static_cast<size_t> (i)];
  spother /= 41.0f;
  float const spratiom = ssync / (spaty + 1.0e-6f);
  float const spnratiom = spaty / (spnoise + 1.0e-6f);
  float const sporatiom = ssync / (spother + 1.0e-6f);
  if (spnratiom > 2.3f)
    {
      if (lcallingrprt || nlasttx == 1)
        {
          if (sporatiom > 1.35f)
            {
              std::fill_n (msg37_out, 37, ' ');
              return 0;
            }
        }
      else if (spratiom > 1.35f)
        {
          std::fill_n (msg37_out, 37, ' ');
          return 0;
        }
    }

  return 1;
}

int ftx_ft8mf1var_c (float const* s8, int const* idtone76, int const* itone76,
                     char const* msgsd76, char const msgd[37], char msg37_out[37], int itone_out[79])
{
  if (msg37_out) std::fill_n (msg37_out, 37, ' ');
  if (itone_out) std::fill_n (itone_out, 79, 0);
  if (!s8 || !idtone76 || !itone76 || !msgsd76 || !msgd || !msg37_out || !itone_out)
    {
      return 0;
    }

  auto s8d_at = [&] (int row, int col_1based) {
    int const source_col = col_1based <= 29 ? col_1based + 7 : col_1based + 14;
    return s8[row + 8 * (source_col - 1)];
  };
  auto sparse_at = [&] (int row, int col_1based) {
    return idtone76[row + 76 * (col_1based - 1)];
  };
  auto tone_at = [&] (int row, int col_1based) {
    return itone76[row + 76 * (col_1based - 1)];
  };
  auto row_text = [&] (int row) {
    return from_fixed_chars (msgsd76 + row * 37, 37).trimmed ().toUpper ();
  };

  QString const text = from_fixed_chars (msgd, 37).trimmed ().toUpper ();
  QStringList const words = text.split (QLatin1Char (' '), Qt::SkipEmptyParts);
  if (words.size () < 3 || words[0].size () > 12 || words[1].size () > 12
      || words[0].size () < 3 || words[1].size () < 3)
    {
      return 0;
    }

  QString const token = words[2];
  bool const lr73 = token == QStringLiteral ("RR73") || token == QStringLiteral ("73");
  bool const lgrid = !lr73 && token.size () == 4
                     && !token.startsWith (QStringLiteral ("R+"))
                     && !token.startsWith (QStringLiteral ("R-"));
  bool const lreport = !token.isEmpty () && (token[0] == QLatin1Char ('+') || token[0] == QLatin1Char ('-'));
  bool const lrreport = token.startsWith (QStringLiteral ("R+")) || token.startsWith (QStringLiteral ("R-"));
  bool const lrrr = token == QStringLiteral ("RRR");

  std::array<int, 58> mrs {};
  std::array<int, 58> mrs2 {};
  float ref0 = 0.0f;
  for (int j = 1; j <= 58; ++j)
    {
      int best = 0;
      int second = 0;
      float s1 = -1.0e6f;
      float s2 = -1.0e6f;
      for (int row = 0; row < 8; ++row)
        {
          float const value = s8d_at (row, j);
          if (value > s1)
            {
              s2 = s1;
              second = best;
              s1 = value;
              best = row;
            }
          else if (row != best && value > s2)
            {
              s2 = value;
              second = row;
            }
        }
      mrs[static_cast<size_t> (j - 1)] = best;
      mrs2[static_cast<size_t> (j - 1)] = second;
      ref0 += s8d_at (best, j);
    }

  int ipk = -1;
  float u1 = 0.0f;
  float u2 = 0.0f;
  for (int row = 0; row < 76; ++row)
    {
      float psum = 0.0f;
      float ref = ref0;
      for (int j = 1; j <= 58; ++j)
        {
          int const i3 = sparse_at (row, j);
          psum += s8d_at (i3, j);
          if (i3 == mrs[static_cast<size_t> (j - 1)])
            {
              ref = ref - s8d_at (i3, j) + s8d_at (mrs2[static_cast<size_t> (j - 1)], j);
            }
        }
      float const p = psum / ref;
      if (p > u1)
        {
          u2 = u1;
          u1 = p;
          ipk = row;
        }
      else if (p > u2)
        {
          u2 = p;
        }
    }

  if (ipk < 0)
    {
      return 0;
    }

  if (lgrid)
    {
      if (ipk == 75)
        {
          QString const msggrid = row_text (75);
          QStringList const grid_words = msggrid.split (QLatin1Char (' '), Qt::SkipEmptyParts);
          if (grid_words.size () >= 3 && grid_words[2] == QStringLiteral ("AA00"))
            {
              return 0;
            }
        }
      if (ipk < 36 || (ipk > 71 && ipk < 75))
        {
          return 0;
        }
    }
  if (lreport && ((ipk > 35 && ipk < 72) || ipk == 75)) return 0;
  if (lrreport && (ipk < 36 || ipk == 72 || ipk == 75)) return 0;
  if (lrrr && (ipk < 72 || ipk == 75)) return 0;
  if (lr73 && (ipk < 73 || ipk == 75)) return 0;

  float const qual = 100.0f * (u1 - u2);
  float const thresh = (qual + 10.0f) * (u1 - 0.6f);
  if (thresh <= 4.0f || qual <= 2.6f || u1 <= 0.77f)
    {
      return 0;
    }

  to_fixed_chars (row_text (ipk), msg37_out, 37);
  for (int col = 1; col <= 79; ++col)
    {
      itone_out[col - 1] = tone_at (ipk, col);
    }
  return 1;
}

int ftx_ft8mfcqvar_c (float const* s8, char const msgd[37], int const* idtone25, char msg37_out[37])
{
  if (msg37_out) std::fill_n (msg37_out, 37, ' ');
  if (!s8 || !msgd || !idtone25 || !msg37_out)
    {
      return 0;
    }

  auto s8d_at = [&] (int row, int col_1based) {
    int const source_col = col_1based <= 29 ? col_1based + 7 : col_1based + 14;
    return s8[row + 8 * (source_col - 1)];
  };
  auto sparse_at = [&] (int row, int col_1based) {
    return idtone25[row + 25 * (col_1based - 1)];
  };
  auto encode_short = [&] (QString const& text, std::array<int, 79>& tones) {
    QByteArray const fixed = to_fixed_37 (text);
    std::array<char, 37> msgsent {};
    std::array<signed char, 77> msgbits {};
    int i3 = -1;
    int n3 = -1;
    tones.fill (0);
    return ftx_encode_ft8var_short_message_c (fixed.constData (), &i3, &n3, msgsent.data (),
                                              msgbits.data (), tones.data ()) != 0;
  };

  QString const text = from_fixed_chars (msgd, 37).trimmed ().toUpper ();
  if (text.size () < 6)
    {
      return 0;
    }

  std::array<int, 79> tones {};
  if (!encode_short (text, tones))
    {
      return 0;
    }

  std::array<int, 58> candidate {};
  for (int i = 0; i < 29; ++i)
    {
      candidate[static_cast<size_t> (i)] = tones[7 + i];
      candidate[static_cast<size_t> (29 + i)] = tones[43 + i];
    }

  std::array<int, 58> mrs {};
  std::array<int, 58> mrs2 {};
  float ref0 = 0.0f;
  for (int j = 1; j <= 58; ++j)
    {
      int best = 0;
      int second = 0;
      float s1 = -1.0e6f;
      float s2 = -1.0e6f;
      for (int row = 0; row < 8; ++row)
        {
          float const value = s8d_at (row, j);
          if (value > s1)
            {
              s2 = s1;
              second = best;
              s1 = value;
              best = row;
            }
          else if (row != best && value > s2)
            {
              s2 = value;
              second = row;
            }
        }
      mrs[static_cast<size_t> (j - 1)] = best;
      mrs2[static_cast<size_t> (j - 1)] = second;
      ref0 += s8d_at (best, j);
    }

  int ipk = -1;
  float u1 = 0.0f;
  float u2 = 0.0f;
  for (int row = 0; row < 25; ++row)
    {
      float psum = 0.0f;
      float ref = ref0;
      for (int j = 1; j <= 58; ++j)
        {
          int const i3 = row == 0 ? candidate[static_cast<size_t> (j - 1)] : sparse_at (row, j);
          psum += s8d_at (i3, j);
          if (i3 == mrs[static_cast<size_t> (j - 1)])
            {
              ref = ref - s8d_at (i3, j) + s8d_at (mrs2[static_cast<size_t> (j - 1)], j);
            }
        }
      float const p = psum / ref;
      if (p > u1)
        {
          u2 = u1;
          u1 = p;
          ipk = row;
        }
      else if (p > u2)
        {
          u2 = p;
        }
    }

  float const qual = 100.0f * (u1 - u2);
  float const thresh = (qual + 10.0f) * (u1 - 0.6f);
  if (ipk == 0 && thresh > 4.0f && qual > 2.6f && u1 > 0.77f)
    {
      to_fixed_chars (text, msg37_out, 37);
      return 1;
    }
  return 0;
}

int legacy_pack77_unpack77bits_c (signed char const* message77, int received,
                                  char msgsent[37], int* quirky_out)
{
  if (quirky_out) *quirky_out = 0;
  if (msgsent) std::fill_n (msgsent, 37, ' ');
  if (!message77 || !msgsent)
    {
      return 0;
    }

  MessageBits77 bits {};
  if (!checked_bits_from_int8 (message77, 77, &bits))
    {
      return 0;
    }

  QByteArray const bit_bytes = bytes_from_bits (bits);
  decodium::txmsg::DecodedMessage decoded =
      decodium::txmsg::decode77 (bit_bytes, &g_legacy_pack77_context, received != 0);
  if (!decoded.ok)
    {
      return 0;
    }

  if (quirky_out)
    {
      *quirky_out = (decoded.msgsent.contains (QByteArrayLiteral ("/R"))
                     || decoded.msgsent.startsWith (QByteArrayLiteral ("TU; "))) ? 1 : 0;
    }
  to_fixed_chars (decoded.msgsent, msgsent, 37);
  return 1;
}

int ftx_ft8_message77_to_itone_c (signed char const* message77, int* itone_out)
{
  if (!message77 || !itone_out)
    {
      return 0;
    }

  std::fill_n (itone_out, 79, 0);

  MessageBits77 bits {};
  if (!checked_bits_from_int8 (message77, 77, &bits))
    {
      return 0;
    }

  QVector<int> const tones = map_ft8_tones (bits);
  if (tones.size () < 79)
    {
      return 0;
    }

  for (int i = 0; i < 79; ++i)
    {
      itone_out[i] = tones.at (i);
  }
  return 1;
}

int ftx_ft2_message77_to_itone_c (signed char const* message77, int* itone_out)
{
  if (!message77 || !itone_out)
    {
      return 0;
    }

  std::fill_n (itone_out, 103, 0);

  MessageBits77 bits {};
  if (!checked_bits_from_int8 (message77, 77, &bits))
    {
      return 0;
    }

  QVector<int> const tones = map_ft2_ft4_tones (bits);
  if (tones.size () < 103)
    {
      return 0;
    }

  for (int i = 0; i < 103; ++i)
    {
      itone_out[i] = tones.at (i);
    }
  return 1;
}

void get_ft2_tones_from_77bits_ (signed char* msgbits, int* i4tone)
{
  if (i4tone)
    {
      std::fill_n (i4tone, 103, 0);
    }
  if (!msgbits || !i4tone)
    {
      return;
    }

  MessageBits77 bits {};
  if (!checked_bits_from_int8 (msgbits, 77, &bits))
    {
      return;
    }

  QVector<int> const tones = map_ft2_ft4_tones (bits);
  if (tones.size () < 103)
    {
      return;
    }

  for (int i = 0; i < 103; ++i)
    {
      i4tone[i] = tones.at (i);
    }
}

int ftx_encode174_91_message77_c (signed char const* message77, signed char* codeword_out)
{
  if (!message77 || !codeword_out)
    {
      return 0;
    }

  MessageBits77 bits {};
  if (!checked_bits_from_int8 (message77, 77, &bits))
    {
      return 0;
    }

  Codeword174 const codeword = encode174_91_cpp (bits);
  for (int i = 0; i < 174; ++i)
    {
      codeword_out[i] = static_cast<signed char> (codeword[static_cast<size_t> (i)] != 0);
    }
  return 1;
}

void encode174_91_ (signed char* message77, signed char* codeword_out)
{
  if (!codeword_out)
    {
      return;
    }

  std::fill_n (codeword_out, 174, static_cast<signed char> (0));
  if (!message77)
    {
      return;
    }

  MessageBits77 bits {};
  if (!checked_bits_from_int8 (message77, 77, &bits))
    {
      return;
    }

  Codeword174 const codeword = encode174_91_cpp (bits);
  for (int i = 0; i < 174; ++i)
    {
      codeword_out[i] = static_cast<signed char> (codeword[static_cast<size_t> (i)] != 0);
    }
}

void genft8_ (char* msg, int* i3, int* n3, char* msgsent, signed char* msgbits, int* itone)
{
  if (i3) *i3 = -1;
  if (n3) *n3 = -1;
  if (msgsent) std::fill_n (msgsent, 37, ' ');
  if (msgbits) std::fill_n (msgbits, 77, static_cast<signed char> (0));
  if (itone) std::fill_n (itone, 79, 0);
  if (!msg || !msgsent || !msgbits || !itone)
    {
      return;
    }

  QString const message = QString::fromLatin1 (msg, 37);
  auto const encoded = decodium::txmsg::encodeFt8 (message);

  QByteArray msgsent_bytes = encoded.msgsent.left (37);
  if (msgsent_bytes.size () < 37)
    {
      msgsent_bytes.append (QByteArray (37 - msgsent_bytes.size (), ' '));
    }
  std::memcpy (msgsent, msgsent_bytes.constData (), 37);

  if (i3) *i3 = encoded.i3;
  if (n3) *n3 = encoded.n3;

  for (int idx = 0; idx < 77 && idx < encoded.msgbits.size (); ++idx)
    {
      msgbits[idx] = static_cast<signed char> (encoded.msgbits.at (idx) != 0);
    }
  for (int idx = 0; idx < 79 && idx < encoded.tones.size (); ++idx)
    {
      itone[idx] = encoded.tones.at (idx);
    }
}

void genfst4_ (char* msg, int* ichk, char* msgsent, signed char* msgbits, int* itone, int* iwspr,
               fortran_charlen_t len1, fortran_charlen_t len2)
{
  bool const check_only = ichk && *ichk == 1;
  if (msgsent) std::fill_n (msgsent, static_cast<size_t> (len2), ' ');
  if (msgbits) std::fill_n (msgbits, 101, static_cast<signed char> (0));
  if (itone) std::fill_n (itone, 160, 0);
  if (iwspr) *iwspr = 0;
  if (!msg || !msgsent || !msgbits || !itone)
    {
      return;
    }

  QString const message = QString::fromLatin1 (msg, static_cast<int> (len1));
  auto const encoded = decodium::txmsg::encodeFst4 (message, check_only);
  to_fixed_chars (encoded.msgsent, msgsent, static_cast<int> (len2));
  if (iwspr)
    {
      *iwspr = (encoded.i3 == 0 && encoded.n3 == 6) ? 1 : 0;
    }

  if (check_only || !encoded.ok)
    {
      return;
    }

  for (int idx = 0; idx < 101 && idx < encoded.msgbits.size (); ++idx)
    {
      msgbits[idx] = static_cast<signed char> (encoded.msgbits.at (idx) != 0);
    }
  for (int idx = 0; idx < 160 && idx < encoded.tones.size (); ++idx)
    {
      itone[idx] = encoded.tones.at (idx);
    }
}

void get_fst4_tones_from_bits_ (signed char* msgbits, int* i4tone, int* iwspr)
{
  if (i4tone)
    {
      std::fill_n (i4tone, 160, 0);
    }
  if (!msgbits || !i4tone)
    {
      return;
    }

  bool const wspr = iwspr && *iwspr != 0;
  QVector<int> tones;
  if (wspr)
    {
      MessageBits74 bits {};
      for (int i = 0; i < 74; ++i)
        {
          bits[static_cast<size_t> (i)] = static_cast<unsigned char> (msgbits[i] != 0);
        }
      tones = map_fst4_tones_from_codeword (encode240_74_cpp (bits));
    }
  else
    {
      MessageBits101 bits {};
      for (int i = 0; i < 101; ++i)
        {
          bits[static_cast<size_t> (i)] = static_cast<unsigned char> (msgbits[i] != 0);
        }
      tones = map_fst4_tones_from_codeword (encode240_101_cpp (bits));
    }

  for (int i = 0; i < 160 && i < tones.size (); ++i)
    {
      i4tone[i] = tones.at (i);
    }
}

void genft4_ (char* msg, int* ichk, char* msgsent, signed char* msgbits, int* itone)
{
  bool const check_only = ichk && *ichk == 1;
  if (msgsent) std::fill_n (msgsent, 37, ' ');
  if (msgbits) std::fill_n (msgbits, 77, static_cast<signed char> (0));
  if (itone) std::fill_n (itone, 103, 0);
  if (!msg || !msgsent || !msgbits || !itone)
    {
      return;
    }

  QString const message = QString::fromLatin1 (msg, 37);
  auto const encoded = decodium::txmsg::encodeFt4 (message, check_only);

  QByteArray msgsent_bytes = encoded.msgsent.left (37);
  if (msgsent_bytes.size () < 37)
    {
      msgsent_bytes.append (QByteArray (37 - msgsent_bytes.size (), ' '));
    }
  std::memcpy (msgsent, msgsent_bytes.constData (), 37);

  if (check_only || !encoded.ok)
    {
      return;
    }

  for (int idx = 0; idx < 77 && idx < encoded.msgbits.size (); ++idx)
    {
      msgbits[idx] = static_cast<signed char> (encoded.msgbits.at (idx) != 0);
    }
  for (int idx = 0; idx < 103 && idx < encoded.tones.size (); ++idx)
    {
      itone[idx] = encoded.tones.at (idx);
    }
}

void genft2_ (char* msg, int* ichk, char* msgsent, signed char* msgbits, int* itone)
{
  bool const check_only = ichk && *ichk == 1;
  if (msgsent) std::fill_n (msgsent, 37, ' ');
  if (msgbits) std::fill_n (msgbits, 77, static_cast<signed char> (0));
  if (itone) std::fill_n (itone, 103, 0);
  if (!msg || !msgsent || !msgbits || !itone)
    {
      return;
    }

  auto const encoded = decodium::txmsg::encodeFt2 (from_fixed_chars (msg, 37), check_only);
  to_fixed_chars (encoded.msgsent, msgsent, 37);

  if (check_only || !encoded.ok)
    {
      return;
    }

  for (int idx = 0; idx < 77 && idx < encoded.msgbits.size (); ++idx)
    {
      msgbits[idx] = static_cast<signed char> (encoded.msgbits.at (idx) != 0);
    }
  for (int idx = 0; idx < 103 && idx < encoded.tones.size (); ++idx)
    {
      itone[idx] = encoded.tones.at (idx);
    }
}

void legacy_pack77_save_hash_call_c (char const c13[13], int* n10, int* n12, int* n22)
{
  QString const call = normalized_call_from_fixed (c13, 13);
  if (n10) *n10 = -1;
  if (n12) *n12 = -1;
  if (n22) *n22 = -1;
  if (call.size () < 3)
    {
      return;
    }

  g_legacy_pack77_context.saveHashCall (call);
  Maybe<int> h10 = ihashcall_cpp (call, 10);
  Maybe<int> h12 = ihashcall_cpp (call, 12);
  Maybe<int> h22 = ihashcall_cpp (call, 22);
  if (n10 && h10.ok) *n10 = h10.value;
  if (n12 && h12.ok) *n12 = h12.value;
  if (n22 && h22.ok) *n22 = h22.value;
}

int legacy_pack77_hash_call_bits_c (char const c13[13], int bits, int* hash_out)
{
  if (hash_out) *hash_out = -1;
  if (bits != 10 && bits != 12 && bits != 22)
    {
      return 0;
    }

  QString const call = normalized_call_from_fixed (c13, 13);
  CheckedCall const checked = check_call (call);
  if (!checked.ok)
    {
      return 0;
    }

  Maybe<int> hash = ihashcall_cpp (call, bits);
  if (!hash.ok)
    {
      return 0;
    }

  if (hash_out) *hash_out = hash.value;
  return 1;
}

void legacy_pack77_pack28_c (char const c13[13], int* n28, bool* success)
{
  if (n28) *n28 = -1;
  if (success) *success = false;

  QString const token = from_fixed_chars (c13, 13).trimmed ();
  Maybe<int> const packed = pack28_cpp (token);
  if (!packed.ok)
    {
      return;
    }

  int const packed_value = packed.value;
  if (n28) *n28 = packed_value;
  if (success) *success = true;
}

void legacy_pack77_unpack28_c (int n28, char c13[13], bool* success)
{
  if (c13) std::fill_n (c13, 13, ' ');
  if (success) *success = false;

  Maybe<QString> unpacked = unpack28_cpp (n28, &g_legacy_pack77_context);
  if (!unpacked.ok)
    {
      return;
    }

  if (c13) to_fixed_chars (unpacked.value, c13, 13);
  if (success) *success = true;
}

void legacy_pack77_split77_c (char const msg[37], int* nwords, int nw[19], char words[247])
{
  if (nwords) *nwords = 0;
  if (nw) std::fill_n (nw, 19, 0);
  if (words) std::fill_n (words, 19 * 13, ' ');

  QStringList const split = split77_cpp (from_fixed_chars (msg, 37));
  int const count = std::min (19, split.size ());
  if (nwords) *nwords = count;
  for (int i = 0; i < count; ++i)
    {
      QByteArray latin = split.at (i).left (13).toLatin1 ();
      if (nw) nw[i] = latin.size ();
      std::fill_n (words + i * 13, 13, ' ');
      std::copy_n (latin.constData (), std::min (13, latin.size ()), words + i * 13);
    }
}

void legacy_pack77_packtext77_c (char const c13[13], char c71[71], bool* success)
{
  if (c71) std::fill_n (c71, 71, '0');
  if (success) *success = false;

  Maybe<Uint71> packed = packtext71_cpp (from_fixed_chars (c13, 13));
  if (!packed.ok)
    {
      return;
    }

  MessageBits77 bits {};
  write_uint71_bits (bits, 0, packed.value);
  if (c71) bits_to_fixed_chars (bits, c71, 71);
  if (success) *success = true;
}

void legacy_pack77_unpacktext77_c (char const c71[71], char c13[13], bool* success)
{
  if (c13) std::fill_n (c13, 13, ' ');
  if (success) *success = false;

  MessageBits77 bits = bits_from_chars (c71, 71);
  Maybe<QString> unpacked = unpacktext71_cpp (read_uint71_bits (bits, 0));
  if (!unpacked.ok)
    {
      return;
    }

  if (c13) to_fixed_chars (unpacked.value, c13, 13);
  if (success) *success = true;
}

void q65_encode_message_ (char* msg0, char* msgsent, int* payload, int* codeword, int* itone,
                          fortran_charlen_t len1, fortran_charlen_t len2)
{
  if (msgsent)
    {
      std::fill_n (msgsent, static_cast<size_t> (len2), ' ');
    }
  if (payload)
    {
      std::fill_n (payload, kQ65PayloadSymbols, 0);
    }
  if (codeword)
    {
      std::fill_n (codeword, kQ65CodewordSymbols, 0);
    }
  if (itone)
    {
      std::fill_n (itone, kQ65ChannelSymbols, 0);
    }
  if (!msg0 || !msgsent || !payload || !codeword || !itone)
    {
      return;
    }

  QByteArray fixed = QByteArray (msg0, static_cast<int> (len1)).left (37);
  if (fixed.size () < 37)
    {
      fixed.append (QByteArray (37 - fixed.size (), ' '));
    }

  QByteArray packed_msgsent;
  std::array<int, kQ65PayloadSymbols> payload_values {};
  std::array<int, kQ65CodewordSymbols> codeword_values {};
  std::array<int, kQ65ChannelSymbols> tone_values {};
  if (!encode_q65_message_cpp (fixed, &packed_msgsent, &payload_values, &codeword_values,
                               &tone_values))
    {
      return;
    }

  std::copy_n (payload_values.begin (), kQ65PayloadSymbols, payload);
  std::copy_n (codeword_values.begin (), kQ65CodewordSymbols, codeword);
  std::copy_n (tone_values.begin (), kQ65ChannelSymbols, itone);
  int const copy_len = std::min (static_cast<int> (len2), packed_msgsent.size ());
  std::copy_n (packed_msgsent.constData (), copy_len, msgsent);
}

void genq65_ (char* msg0, int* ichk, char* msgsent, int* itone, int* i3, int* n3,
              fortran_charlen_t len1, fortran_charlen_t len2)
{
  if (i3) *i3 = -1;
  if (n3) *n3 = -1;
  if (msgsent)
    {
      std::fill_n (msgsent, static_cast<size_t> (len2), ' ');
    }
  if (itone)
    {
      std::fill_n (itone, kQ65ChannelSymbols, 0);
    }
  if (!msg0 || !msgsent || !itone)
    {
      return;
    }

  QByteArray raw = QByteArray (msg0, static_cast<int> (len1));
  if (!raw.isEmpty () && raw.front () == '@')
    {
      bool ok = false;
      int const frequency = raw.mid (1, 4).trimmed ().toInt (&ok);
      int const tone = ok ? frequency : 1000;
      itone[0] = tone;
      QByteArray text = QByteArray::number (tone).rightJustified (5, ' ') + " Hz";
      int const copy_len = std::min (static_cast<int> (len2), text.size ());
      std::copy_n (text.constData (), copy_len, msgsent);
      return;
    }

  QByteArray fixed = raw.left (37);
  if (fixed.size () < 37)
    {
      fixed.append (QByteArray (37 - fixed.size (), ' '));
    }

  QByteArray packed_msgsent;
  std::array<int, kQ65PayloadSymbols> payload_values {};
  std::array<int, kQ65CodewordSymbols> codeword_values {};
  std::array<int, kQ65ChannelSymbols> tone_values {};
  if (!encode_q65_message_cpp (fixed, &packed_msgsent, &payload_values, &codeword_values,
                               &tone_values))
    {
      return;
    }

  int const copy_len = std::min (static_cast<int> (len2), packed_msgsent.size ());
  std::copy_n (packed_msgsent.constData (), copy_len, msgsent);

  if (ichk && *ichk == 1)
    {
      return;
    }

  std::copy_n (tone_values.begin (), kQ65ChannelSymbols, itone);
}
}
