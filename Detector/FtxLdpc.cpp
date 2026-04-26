#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>

namespace
{

constexpr int kLdpc17491N = 174;
constexpr int kLdpc17491K = 91;
constexpr int kLdpc17491M = kLdpc17491N - kLdpc17491K;
constexpr int kLdpc17491MaxRowWeight = 7;
constexpr int kLdpc17491ColWeight = 3;
constexpr int kLdpc17491MaxNdeep = 6;
constexpr float kAlphaMs = 0.75f;

using Codeword174 = std::array<signed char, kLdpc17491N>;
using Message91 = std::array<signed char, kLdpc17491K>;
using GeneratorRows = std::vector<Codeword174>;

constexpr std::array<char const*, kLdpc17491M> kLdpc17491GeneratorHex {{
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

constexpr std::array<int, 522> kLdpc17491Mn {{
   16,  45,  73,  25,  51,  62,  33,  58,  78,   1,  44,  45,
    2,   7,  61,   3,   6,  54,   4,  35,  48,   5,  13,  21,
    8,  56,  79,   9,  64,  69,  10,  19,  66,  11,  36,  60,
   12,  37,  58,  14,  32,  43,  15,  63,  80,  17,  28,  77,
   18,  74,  83,  22,  53,  81,  23,  30,  34,  24,  31,  40,
   26,  41,  76,  27,  57,  70,  29,  49,  65,   3,  38,  78,
    5,  39,  82,  46,  50,  73,  51,  52,  74,  55,  71,  72,
   44,  67,  72,  43,  68,  78,   1,  32,  59,   2,   6,  71,
    4,  16,  54,   7,  65,  67,   8,  30,  42,   9,  22,  31,
   10,  18,  76,  11,  23,  82,  12,  28,  61,  13,  52,  79,
   14,  50,  51,  15,  81,  83,  17,  29,  60,  19,  33,  64,
   20,  26,  73,  21,  34,  40,  24,  27,  77,  25,  55,  58,
   35,  53,  66,  36,  48,  68,  37,  46,  75,  38,  45,  47,
   39,  57,  69,  41,  56,  62,  20,  49,  53,  46,  52,  63,
   45,  70,  75,  27,  35,  80,   1,  15,  30,   2,  68,  80,
    3,  36,  51,   4,  28,  51,   5,  31,  56,   6,  20,  37,
    7,  40,  82,   8,  60,  69,   9,  10,  49,  11,  44,  57,
   12,  39,  59,  13,  24,  55,  14,  21,  65,  16,  71,  78,
   17,  30,  76,  18,  25,  80,  19,  61,  83,  22,  38,  77,
   23,  41,  50,   7,  26,  58,  29,  32,  81,  33,  40,  73,
   18,  34,  48,  13,  42,  64,   5,  26,  43,  47,  69,  72,
   54,  55,  70,  45,  62,  68,  10,  63,  67,  14,  66,  72,
   22,  60,  74,  35,  39,  79,   1,  46,  64,   1,  24,  66,
    2,   5,  70,   3,  31,  65,   4,  49,  58,   1,   4,   5,
    6,  60,  67,   7,  32,  75,   8,  48,  82,   9,  35,  41,
   10,  39,  62,  11,  14,  61,  12,  71,  74,  13,  23,  78,
   11,  35,  55,  15,  16,  79,   7,   9,  16,  17,  54,  63,
   18,  50,  57,  19,  30,  47,  20,  64,  80,  21,  28,  69,
   22,  25,  43,  13,  22,  37,   2,  47,  51,  23,  54,  74,
   26,  34,  72,  27,  36,  37,  21,  36,  63,  29,  40,  44,
   19,  26,  57,   3,  46,  82,  14,  15,  58,  33,  52,  53,
   30,  43,  52,   6,   9,  52,  27,  33,  65,  25,  69,  73,
   38,  55,  83,  20,  39,  77,  18,  29,  56,  32,  48,  71,
   42,  51,  59,  28,  44,  79,  34,  60,  62,  31,  45,  61,
   46,  68,  77,   6,  24,  76,   8,  10,  78,  40,  41,  70,
   17,  50,  53,  42,  66,  68,   4,  22,  72,  36,  64,  81,
   13,  29,  47,   2,   8,  81,  56,  67,  73,   5,  38,  50,
   12,  38,  64,  59,  72,  80,   3,  26,  79,  45,  76,  81,
    1,  65,  74,   7,  18,  77,  11,  56,  59,  14,  39,  54,
   16,  37,  66,  10,  28,  55,  15,  60,  70,  17,  25,  82,
   20,  30,  31,  12,  67,  68,  23,  75,  80,  27,  32,  62,
   24,  69,  75,  19,  21,  71,  34,  53,  61,  35,  46,  47,
   33,  59,  76,  40,  43,  83,  41,  42,  63,  49,  75,  83,
   20,  44,  48,  42,  49,  57
}};

constexpr std::array<int, 581> kLdpc17491Nm {{
    4,  31,  59,  91,  92,  96, 153,   5,  32,  60,  93, 115,
  146,   0,   6,  24,  61,  94, 122, 151,   0,   7,  33,  62,
   95,  96, 143,   0,   8,  25,  63,  83,  93,  96, 148,   6,
   32,  64,  97, 126, 138,   0,   5,  34,  65,  78,  98, 107,
  154,   9,  35,  66,  99, 139, 146,   0,  10,  36,  67, 100,
  107, 126,   0,  11,  37,  67,  87, 101, 139, 158,  12,  38,
   68, 102, 105, 155,   0,  13,  39,  69, 103, 149, 162,   0,
    8,  40,  70,  82, 104, 114, 145,  14,  41,  71,  88, 102,
  123, 156,  15,  42,  59, 106, 123, 159,   0,   1,  33,  72,
  106, 107, 157,   0,  16,  43,  73, 108, 141, 160,   0,  17,
   37,  74,  81, 109, 131, 154,  11,  44,  75, 110, 121, 166,
    0,  45,  55,  64, 111, 130, 161, 173,   8,  46,  71, 112,
  119, 166,   0,  18,  36,  76,  89, 113, 114, 143,  19,  38,
   77, 104, 116, 163,   0,  20,  47,  70,  92, 138, 165,   0,
    2,  48,  74, 113, 128, 160,   0,  21,  45,  78,  83, 117,
  121, 151,  22,  47,  58, 118, 127, 164,   0,  16,  39,  62,
  112, 134, 158,   0,  23,  43,  79, 120, 131, 145,   0,  19,
   35,  59,  73, 110, 125, 161,  20,  36,  63,  94, 136, 161,
    0,  14,  31,  79,  98, 132, 164,   0,   3,  44,  80, 124,
  127, 169,   0,  19,  46,  81, 117, 135, 167,   0,   7,  49,
   58,  90, 100, 105, 168,  12,  50,  61, 118, 119, 144,   0,
   13,  51,  64, 114, 118, 157,   0,  24,  52,  76, 129, 148,
  149,   0,  25,  53,  69,  90, 101, 130, 156,  20,  46,  65,
   80, 120, 140, 170,  21,  54,  77, 100, 140, 171,   0,  35,
   82, 133, 142, 171, 174,   0,  14,  30,  83, 113, 125, 170,
    0,   4,  29,  68, 120, 134, 173,   0,   1,   4,  52,  57,
   86, 136, 152,  26,  51,  56,  91, 122, 137, 168,  52,  84,
  110, 115, 145, 168,   0,   7,  50,  81,  99, 132, 173,   0,
   23,  55,  67,  95, 172, 174,   0,  26,  41,  77, 109, 141,
  148,   0,   2,  27,  41,  61,  62, 115, 133,  27,  40,  56,
  124, 125, 126,   0,  18,  49,  55, 124, 141, 167,   0,   6,
   33,  85, 108, 116, 156,   0,  28,  48,  70,  85, 105, 129,
  158,   9,  54,  63, 131, 147, 155,   0,  22,  53,  68, 109,
  121, 174,   0,   3,  13,  48,  78,  95, 123,   0,  31,  69,
  133, 150, 155, 169,   0,  12,  43,  66,  89,  97, 135, 159,
    5,  39,  75, 102, 136, 167,   0,   2,  54,  86, 101, 135,
  164,   0,  15,  56,  87, 108, 119, 171,   0,  10,  44,  82,
   91, 111, 144, 149,  23,  34,  71,  94, 127, 153,   0,  11,
   49,  88,  92, 142, 157,   0,  29,  34,  87,  97, 147, 162,
    0,  30,  50,  60,  86, 137, 142, 162,  10,  53,  66,  84,
  112, 128, 165,  22,  57,  85,  93, 140, 159,   0,  28,  32,
   72, 103, 132, 166,   0,  28,  29,  84,  88, 117, 143, 150,
    1,  26,  45,  80, 128, 147,   0,  17,  27,  89, 103, 116,
  153,   0,  51,  57,  98, 163, 165, 172,   0,  21,  37,  73,
  138, 152, 169,   0,  16,  47,  76, 130, 137, 154,   0,   3,
   24,  30,  72, 104, 139,   0,   9,  40,  90, 106, 134, 151,
    0,  15,  58,  60,  74, 111, 150, 163,  18,  42,  79, 144,
  146, 152,   0,  25,  38,  65,  99, 122, 160,   0,  17,  42,
   75, 129, 170, 172,   0
}};

constexpr std::array<int, 83> kLdpc17491Nrw {{
    7,   6,   6,   6,   7,   6,   7,   6,   6,   7,   6,   6,
    7,   7,   6,   6,   6,   7,   6,   7,   6,   7,   6,   6,
    6,   7,   6,   6,   6,   7,   6,   6,   6,   6,   7,   6,
    6,   6,   7,   7,   6,   6,   6,   6,   7,   7,   6,   6,
    6,   6,   7,   6,   6,   6,   7,   6,   6,   6,   6,   7,
    6,   6,   6,   7,   6,   6,   6,   7,   7,   6,   6,   7,
    6,   6,   6,   6,   6,   6,   6,   7,   6,   6,   6
}};

struct Ldpc17491Tables
{
  std::array<int, kLdpc17491ColWeight * kLdpc17491N> mn {};
  std::array<int, kLdpc17491MaxRowWeight * kLdpc17491M> nm {};
  std::array<int, kLdpc17491M> nrw {};
  int ncw {0};
  bool ready {false};
};

extern "C"
{
  void ftx_ldpc174_91_metrics_c (signed char const* cw, float const* llr,
                                 int* nharderrors_out, float* dmin_out);
  int ftx_ldpc174_91_finalize_c (signed char const* cw, float const* llr,
                                 signed char* message91_out, int* nharderrors_out,
                                 float* dmin_out);
  short crc13 (unsigned char const* data, int length);
  short crc14 (unsigned char const* data, int length);
}

int hex_nibble (char ch)
{
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
  if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
  return 0;
}

using ParityGenerator = std::array<std::array<signed char, kLdpc17491K>, kLdpc17491M>;

ParityGenerator const& ldpc17491_parity_generator ()
{
  static ParityGenerator matrix = [] {
    ParityGenerator gen {};
    for (int row = 0; row < kLdpc17491M; ++row)
      {
        char const* hex = kLdpc17491GeneratorHex[static_cast<size_t> (row)];
        for (int nibble = 0; nibble < 23; ++nibble)
          {
            int const value = hex_nibble (hex[nibble]);
            int const ibmax = nibble == 22 ? 3 : 4;
            for (int jj = 1; jj <= ibmax; ++jj)
              {
                int const col = nibble * 4 + (jj - 1);
                if (col >= kLdpc17491K) break;
                if ((value & (1 << (4 - jj))) != 0)
                  {
                    gen[static_cast<size_t> (row)][static_cast<size_t> (col)] = 1;
                  }
              }
          }
      }
    return gen;
  }();
  return matrix;
}

int crc14_bits_local (signed char const* bits, int length)
{
  static constexpr std::array<int, 15> poly {{1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1}};
  std::array<int, 15> r {};
  for (int i = 0; i < 15; ++i)
    {
      r[static_cast<size_t> (i)] = bits[i] != 0 ? 1 : 0;
    }
  for (int i = 0; i <= length - 15; ++i)
    {
      r[14] = bits[i + 14] != 0 ? 1 : 0;
      int const lead = r[0];
      for (int j = 0; j < 15; ++j)
        {
          r[static_cast<size_t> (j)] =
            (r[static_cast<size_t> (j)] + lead * poly[static_cast<size_t> (j)]) % 2;
        }
      std::rotate (r.begin (), r.begin () + 1, r.end ());
    }

  int value = 0;
  for (int i = 0; i < 14; ++i)
    {
      value = (value << 1) | r[static_cast<size_t> (i)];
    }
  return value;
}

template <size_t N>
std::array<unsigned char, N> pack_bits_msb (signed char const* bits, int bit_count)
{
  std::array<unsigned char, N> packed {};
  if (!bits)
    {
      return packed;
    }

  for (int bit = 0; bit < bit_count; ++bit)
    {
      if (bits[bit] != 0)
        {
          packed[static_cast<size_t> (bit / 8)] |=
            static_cast<unsigned char> (1u << (7 - (bit % 8)));
        }
    }
  return packed;
}

int read_bitfield_msb (signed char const* bits, int offset, int bit_count)
{
  int value = 0;
  if (!bits)
    {
      return value;
    }

  for (int bit = 0; bit < bit_count; ++bit)
    {
      value = (value << 1) | (bits[offset + bit] != 0 ? 1 : 0);
    }
  return value;
}

void encode174_91_nocrc_cpp (Message91 const& message, Codeword174& codeword)
{
  auto const& gen = ldpc17491_parity_generator ();
  codeword.fill (0);
  std::copy (message.begin (), message.end (), codeword.begin ());
  for (int row = 0; row < kLdpc17491M; ++row)
    {
      int parity = 0;
      for (int col = 0; col < kLdpc17491K; ++col)
        {
          parity ^= (message[static_cast<size_t> (col)] != 0
                     && gen[static_cast<size_t> (row)][static_cast<size_t> (col)] != 0)
                        ? 1
                        : 0;
        }
      codeword[static_cast<size_t> (kLdpc17491K + row)] = static_cast<signed char> (parity);
    }
}

GeneratorRows build_osd_generator_rows (int k)
{
  GeneratorRows rows (static_cast<size_t> (k));
  for (int i = 0; i < k; ++i)
    {
      Message91 message {};
      message[static_cast<size_t> (i)] = 1;
      if (i < 77)
        {
          std::array<signed char, 96> m96 {};
          std::copy (message.begin (), message.end (), m96.begin ());
          int const ncrc14 = crc14_bits_local (m96.data (), 96);
          for (int bit = 0; bit < 14; ++bit)
            {
              message[static_cast<size_t> (77 + bit)] =
                static_cast<signed char> ((ncrc14 >> (13 - bit)) & 1);
            }
          for (int bit = 77; bit < k; ++bit)
            {
              message[static_cast<size_t> (bit)] = 0;
            }
        }
      encode174_91_nocrc_cpp (message, rows[static_cast<size_t> (i)]);
    }
  return rows;
}

GeneratorRows const& osd_generator_rows (int k)
{
  static std::array<GeneratorRows, kLdpc17491K + 1> cache;
  static std::array<std::once_flag, kLdpc17491K + 1> once;
  std::call_once (once[static_cast<size_t> (k)], [k] {
    cache[static_cast<size_t> (k)] = build_osd_generator_rows (k);
  });
  return cache[static_cast<size_t> (k)];
}

Ldpc17491Tables& ldpc17491_tables ()
{
  static Ldpc17491Tables tables = [] {
    Ldpc17491Tables value;
    value.mn = kLdpc17491Mn;
    value.nm = kLdpc17491Nm;
    value.nrw = kLdpc17491Nrw;
    value.ncw = kLdpc17491ColWeight;
    value.ready = true;
    return value;
  }();
  return tables;
}

inline int mn_at (Ldpc17491Tables const& tables, int edge, int bit)
{
  return tables.mn[static_cast<size_t> (edge + kLdpc17491ColWeight * bit)];
}

inline int nm_at (Ldpc17491Tables const& tables, int row, int check)
{
  return tables.nm[static_cast<size_t> (row + kLdpc17491MaxRowWeight * check)];
}

inline float sign_one (float value)
{
  return value >= 0.0f ? 1.0f : -1.0f;
}

void clear_outputs (signed char* message91_out, signed char* cw_out, int* ntype_out,
                    int* nharderror_out, float* dmin_out)
{
  if (message91_out)
    {
      std::fill_n (message91_out, kLdpc17491K, static_cast<signed char> (0));
    }
  if (cw_out)
    {
      std::fill_n (cw_out, kLdpc17491N, static_cast<signed char> (0));
    }
  if (ntype_out)
    {
      *ntype_out = 0;
    }
  if (nharderror_out)
    {
      *nharderror_out = -1;
    }
  if (dmin_out)
    {
      *dmin_out = 0.0f;
    }
}

template <size_t Size>
int sum_first (std::array<signed char, Size> const& values, int count)
{
  int total = 0;
  for (int i = 0; i < count; ++i)
    {
      total += values[static_cast<size_t> (i)] != 0 ? 1 : 0;
    }
  return total;
}

template <size_t Size>
int sum_weight (std::array<signed char, Size> const& values, int count)
{
  int total = 0;
  for (int i = 0; i < count; ++i)
    {
      total += values[static_cast<size_t> (i)] != 0 ? 1 : 0;
    }
  return total;
}

void mrbencode91_cpp (std::array<signed char, kLdpc17491K> const& message,
                      std::array<signed char, kLdpc17491N>& codeword,
                      std::array<std::array<signed char, kLdpc17491K>, kLdpc17491N> const& g2,
                      int k)
{
  codeword.fill (0);
  for (int bit = 0; bit < k; ++bit)
    {
      if (message[static_cast<size_t> (bit)] == 0) continue;
      for (int pos = 0; pos < kLdpc17491N; ++pos)
        {
          codeword[static_cast<size_t> (pos)] ^=
            g2[static_cast<size_t> (pos)][static_cast<size_t> (bit)];
        }
    }
}

void nextpat91_cpp (std::array<signed char, kLdpc17491K>& mi, int k, int iorder, int& iflag)
{
  int ind = -1;
  for (int i = 0; i < k - 1; ++i)
    {
      if (mi[static_cast<size_t> (i)] == 0 && mi[static_cast<size_t> (i + 1)] == 1)
        {
          ind = i;
        }
    }
  if (ind < 0)
    {
      iflag = ind;
      return;
    }

  std::array<signed char, kLdpc17491K> ms {};
  for (int i = 0; i < ind; ++i)
    {
      ms[static_cast<size_t> (i)] = mi[static_cast<size_t> (i)];
    }
  ms[static_cast<size_t> (ind)] = 1;
  ms[static_cast<size_t> (ind + 1)] = 0;
  if (ind + 1 < k)
    {
      int const nz = iorder - sum_weight (ms, k);
      for (int i = k - nz; i < k; ++i)
        {
          ms[static_cast<size_t> (i)] = 1;
        }
      for (int i = ind + 2; i < k - nz; ++i)
        {
          ms[static_cast<size_t> (i)] = 0;
        }
    }
  mi = ms;
  iflag = -1;
  for (int i = 0; i < k; ++i)
    {
      if (mi[static_cast<size_t> (i)] == 1)
        {
          iflag = i;
          break;
        }
    }
}

void osd174_91_cpp (float const* llr_in, int k, signed char const* apmask_in, int ndeep,
                    signed char* message91_out, signed char* cw_out,
                    int* nharderror_out, float* dmin_out)
{
  if (!llr_in || !apmask_in || k < 77 || k > kLdpc17491K)
    {
      if (nharderror_out) *nharderror_out = -1;
      if (dmin_out) *dmin_out = 0.0f;
      return;
    }

  auto const& generator = osd_generator_rows (k);
  std::array<float, kLdpc17491N> rx {};
  std::array<float, kLdpc17491N> absrx {};
  std::array<signed char, kLdpc17491N> apmask {};
  std::array<signed char, kLdpc17491N> hdec {};
  for (int i = 0; i < kLdpc17491N; ++i)
    {
      rx[static_cast<size_t> (i)] = llr_in[i];
      absrx[static_cast<size_t> (i)] = std::fabs (llr_in[i]);
      apmask[static_cast<size_t> (i)] = apmask_in[i] != 0 ? 1 : 0;
      hdec[static_cast<size_t> (i)] = llr_in[i] >= 0.0f ? 1 : 0;
    }

  std::array<int, kLdpc17491N> indices {};
  for (int i = 0; i < kLdpc17491N; ++i)
    {
      indices[static_cast<size_t> (i)] = i;
    }
  std::sort (indices.begin (), indices.end (), [&absrx] (int lhs, int rhs) {
    return absrx[static_cast<size_t> (lhs)] > absrx[static_cast<size_t> (rhs)];
  });

  std::array<Codeword174, kLdpc17491K> genmrb {};
  for (int row = 0; row < k; ++row)
    {
      for (int col = 0; col < kLdpc17491N; ++col)
        {
          genmrb[static_cast<size_t> (row)][static_cast<size_t> (col)] =
            generator[static_cast<size_t> (row)][static_cast<size_t> (indices[static_cast<size_t> (col)])];
        }
    }

  int const col_limit = std::min (k + 20, kLdpc17491N);
  for (int id = 0; id < k; ++id)
    {
      for (int icol = id; icol < col_limit; ++icol)
        {
          if (genmrb[static_cast<size_t> (id)][static_cast<size_t> (icol)] != 1) continue;
          if (icol != id)
            {
              for (int row = 0; row < k; ++row)
                {
                  std::swap (genmrb[static_cast<size_t> (row)][static_cast<size_t> (id)],
                             genmrb[static_cast<size_t> (row)][static_cast<size_t> (icol)]);
                }
              std::swap (indices[static_cast<size_t> (id)], indices[static_cast<size_t> (icol)]);
            }
          for (int row = 0; row < k; ++row)
            {
              if (row == id || genmrb[static_cast<size_t> (row)][static_cast<size_t> (id)] != 1) continue;
              for (int col = 0; col < kLdpc17491N; ++col)
                {
                  genmrb[static_cast<size_t> (row)][static_cast<size_t> (col)] ^=
                    genmrb[static_cast<size_t> (id)][static_cast<size_t> (col)];
                }
            }
          break;
        }
    }

  std::array<std::array<signed char, kLdpc17491K>, kLdpc17491N> g2 {};
  for (int row = 0; row < k; ++row)
    {
      for (int col = 0; col < kLdpc17491N; ++col)
        {
          g2[static_cast<size_t> (col)][static_cast<size_t> (row)] =
            genmrb[static_cast<size_t> (row)][static_cast<size_t> (col)];
        }
    }

  std::array<float, kLdpc17491N> absrx_sorted {};
  std::array<signed char, kLdpc17491N> apmask_sorted {};
  std::array<signed char, kLdpc17491N> hdec_sorted {};
  for (int i = 0; i < kLdpc17491N; ++i)
    {
      int const index = indices[static_cast<size_t> (i)];
      absrx_sorted[static_cast<size_t> (i)] = absrx[static_cast<size_t> (index)];
      apmask_sorted[static_cast<size_t> (i)] = apmask[static_cast<size_t> (index)];
      hdec_sorted[static_cast<size_t> (i)] = hdec[static_cast<size_t> (index)];
    }

  std::array<signed char, kLdpc17491K> m0 {};
  std::copy_n (hdec_sorted.begin (), k, m0.begin ());

  Codeword174 c0 {};
  mrbencode91_cpp (m0, c0, g2, k);
  Codeword174 cw_sorted = c0;
  int nhardmin = 0;
  float dmin = 0.0f;
  for (int i = 0; i < kLdpc17491N; ++i)
    {
      if ((c0[static_cast<size_t> (i)] ^ hdec_sorted[static_cast<size_t> (i)]) != 0)
        {
          ++nhardmin;
          dmin += absrx_sorted[static_cast<size_t> (i)];
        }
    }

  int nord = 0;
  int npre1 = 0;
  int npre2 = 0;
  int nt = 0;
  int ntheta = 0;
  int ntau = 0;
  int ndeep_local = std::max (0, std::min (ndeep, kLdpc17491MaxNdeep));
  if (ndeep_local == 1)
    {
      nord = 1; npre1 = 0; npre2 = 0; nt = 40; ntheta = 12;
    }
  else if (ndeep_local == 2)
    {
      nord = 1; npre1 = 1; npre2 = 0; nt = 40; ntheta = 10;
    }
  else if (ndeep_local == 3)
    {
      nord = 1; npre1 = 1; npre2 = 1; nt = 40; ntheta = 12; ntau = 14;
    }
  else if (ndeep_local == 4)
    {
      nord = 2; npre1 = 1; npre2 = 1; nt = 40; ntheta = 12; ntau = 17;
    }
  else if (ndeep_local == 5)
    {
      nord = 3; npre1 = 1; npre2 = 1; nt = 40; ntheta = 12; ntau = 15;
    }
  else if (ndeep_local >= 6)
    {
      nord = 4; npre1 = 1; npre2 = 1; nt = 95; ntheta = 12; ntau = 15;
    }

  if (ndeep_local > 0)
    {
      std::array<signed char, kLdpc17491K> misub {};
      std::array<signed char, kLdpc17491K> mi {};
      std::array<signed char, kLdpc17491K> me {};
      std::array<signed char, kLdpc17491M> e2sub {};
      std::array<signed char, kLdpc17491M> e2 {};
      Codeword174 ce {};

      for (int iorder = 1; iorder <= nord; ++iorder)
        {
          misub.fill (0);
          for (int i = k - iorder; i < k; ++i)
            {
              misub[static_cast<size_t> (i)] = 1;
            }
          int iflag = k - iorder;
          while (iflag >= 0)
            {
              int const iend = (iorder == nord && npre1 == 0) ? iflag : 0;
              float d1 = 0.0f;
              for (int n1 = iflag; n1 >= iend; --n1)
                {
                  mi = misub;
                  mi[static_cast<size_t> (n1)] = 1;
                  bool masked = false;
                  for (int i = 0; i < k; ++i)
                    {
                      if (apmask_sorted[static_cast<size_t> (i)] == 1 && mi[static_cast<size_t> (i)] == 1)
                        {
                          masked = true;
                          break;
                        }
                    }
                  if (masked) continue;

                  for (int i = 0; i < k; ++i)
                    {
                      me[static_cast<size_t> (i)] = m0[static_cast<size_t> (i)] ^ mi[static_cast<size_t> (i)];
                    }

                  int nd1kpt = 0;
                  if (n1 == iflag)
                    {
                      mrbencode91_cpp (me, ce, g2, k);
                      d1 = 0.0f;
                      for (int i = 0; i < k; ++i)
                        {
                          if ((me[static_cast<size_t> (i)] ^ hdec_sorted[static_cast<size_t> (i)]) != 0)
                            {
                              d1 += absrx_sorted[static_cast<size_t> (i)];
                            }
                        }
                      for (int i = 0; i < kLdpc17491M; ++i)
                        {
                          e2sub[static_cast<size_t> (i)] =
                            ce[static_cast<size_t> (k + i)] ^ hdec_sorted[static_cast<size_t> (k + i)];
                          e2[static_cast<size_t> (i)] = e2sub[static_cast<size_t> (i)];
                        }
                      nd1kpt = sum_first (e2sub, nt) + 1;
                    }
                  else
                    {
                      for (int i = 0; i < kLdpc17491M; ++i)
                        {
                          e2[static_cast<size_t> (i)] =
                            e2sub[static_cast<size_t> (i)] ^ g2[static_cast<size_t> (k + i)][static_cast<size_t> (n1)];
                        }
                      nd1kpt = sum_first (e2, nt) + 2;
                    }

                  if (nd1kpt <= ntheta)
                    {
                      mrbencode91_cpp (me, ce, g2, k);
                      int nhard = 0;
                      float dd = d1;
                      if (n1 == iflag)
                        {
                          for (int i = 0; i < kLdpc17491M; ++i)
                            {
                              if (e2sub[static_cast<size_t> (i)] != 0)
                                {
                                  dd += absrx_sorted[static_cast<size_t> (k + i)];
                                }
                              if ((ce[static_cast<size_t> (k + i)] ^ hdec_sorted[static_cast<size_t> (k + i)]) != 0)
                                {
                                  ++nhard;
                                }
                            }
                        }
                      else
                        {
                          if ((ce[static_cast<size_t> (n1)] ^ hdec_sorted[static_cast<size_t> (n1)]) != 0)
                            {
                              dd += absrx_sorted[static_cast<size_t> (n1)];
                            }
                          for (int i = 0; i < kLdpc17491M; ++i)
                            {
                              if (e2[static_cast<size_t> (i)] != 0)
                                {
                                  dd += absrx_sorted[static_cast<size_t> (k + i)];
                                }
                              if ((ce[static_cast<size_t> (k + i)] ^ hdec_sorted[static_cast<size_t> (k + i)]) != 0)
                                {
                                  ++nhard;
                                }
                            }
                        }
                      for (int i = 0; i < k; ++i)
                        {
                          if ((ce[static_cast<size_t> (i)] ^ hdec_sorted[static_cast<size_t> (i)]) != 0)
                            {
                              ++nhard;
                            }
                        }
                      if (dd < dmin)
                        {
                          dmin = dd;
                          cw_sorted = ce;
                          nhardmin = nhard;
                        }
                    }
                }
              nextpat91_cpp (misub, k, iorder, iflag);
            }
        }

      if (npre2 == 1)
        {
          int constexpr kMaxPairs = kLdpc17491K * (kLdpc17491K - 1) / 2;
          std::vector<int> box_heads (static_cast<size_t> (1u << ntau), -1);
          std::array<int, kMaxPairs> box_next {};
          std::array<std::pair<int, int>, kMaxPairs> box_pairs {};
          int box_count = 0;
          for (int i1 = k - 1; i1 >= 0; --i1)
            {
              for (int i2 = i1 - 1; i2 >= 0; --i2)
                {
                  int pattern = 0;
                  for (int bit = 0; bit < ntau; ++bit)
                    {
                      int const value =
                        g2[static_cast<size_t> (k + bit)][static_cast<size_t> (i1)] ^
                        g2[static_cast<size_t> (k + bit)][static_cast<size_t> (i2)];
                      pattern = (pattern << 1) | value;
                    }
                  if (box_count < kMaxPairs)
                    {
                      box_pairs[static_cast<size_t> (box_count)] = {i1, i2};
                      box_next[static_cast<size_t> (box_count)] = box_heads[static_cast<size_t> (pattern)];
                      box_heads[static_cast<size_t> (pattern)] = box_count;
                      ++box_count;
                    }
                }
            }

          misub.fill (0);
          for (int i = k - nord; i < k; ++i)
            {
              misub[static_cast<size_t> (i)] = 1;
            }
          int iflag = k - nord;
          std::array<signed char, kLdpc17491K> mi2 {};
          std::array<signed char, kLdpc17491K> me2 {};
          Codeword174 ce2 {};
          std::array<signed char, kLdpc17491M> e2sub2 {};
          while (iflag >= 0)
            {
              for (int i = 0; i < k; ++i)
                {
                  me2[static_cast<size_t> (i)] = m0[static_cast<size_t> (i)] ^ misub[static_cast<size_t> (i)];
                }
              mrbencode91_cpp (me2, ce2, g2, k);
              for (int i = 0; i < kLdpc17491M; ++i)
                {
                  e2sub2[static_cast<size_t> (i)] =
                    ce2[static_cast<size_t> (k + i)] ^ hdec_sorted[static_cast<size_t> (k + i)];
                }

              for (int ui = 0; ui <= ntau; ++ui)
                {
                  int pattern = 0;
                  for (int bit = 0; bit < ntau; ++bit)
                    {
                      int value = e2sub2[static_cast<size_t> (bit)] != 0 ? 1 : 0;
                      if (ui > 0 && bit == ui - 1)
                        {
                          value ^= 1;
                        }
                      pattern = (pattern << 1) | value;
                    }
                  for (int match_index = box_heads[static_cast<size_t> (pattern)];
                       match_index >= 0;
                       match_index = box_next[static_cast<size_t> (match_index)])
                    {
                      auto const& match = box_pairs[static_cast<size_t> (match_index)];
                      mi2 = misub;
                      mi2[static_cast<size_t> (match.first)] = 1;
                      mi2[static_cast<size_t> (match.second)] = 1;
                      if (sum_weight (mi2, k) < nord + npre1 + npre2) continue;
                      bool masked = false;
                      for (int i = 0; i < k; ++i)
                        {
                          if (apmask_sorted[static_cast<size_t> (i)] == 1 && mi2[static_cast<size_t> (i)] == 1)
                            {
                              masked = true;
                              break;
                            }
                        }
                      if (masked) continue;

                      for (int i = 0; i < k; ++i)
                        {
                          me2[static_cast<size_t> (i)] =
                            m0[static_cast<size_t> (i)] ^ mi2[static_cast<size_t> (i)];
                        }
                      mrbencode91_cpp (me2, ce2, g2, k);
                      int nhard = 0;
                      float dd = 0.0f;
                      for (int i = 0; i < kLdpc17491N; ++i)
                        {
                          if ((ce2[static_cast<size_t> (i)] ^ hdec_sorted[static_cast<size_t> (i)]) != 0)
                            {
                              ++nhard;
                              dd += absrx_sorted[static_cast<size_t> (i)];
                            }
                        }
                      if (dd < dmin)
                        {
                          dmin = dd;
                          cw_sorted = ce2;
                          nhardmin = nhard;
                        }
                    }
                }
              nextpat91_cpp (misub, k, nord, iflag);
            }
        }
    }

  Codeword174 cw_unsorted {};
  for (int i = 0; i < kLdpc17491N; ++i)
    {
      cw_unsorted[static_cast<size_t> (indices[static_cast<size_t> (i)])] =
        cw_sorted[static_cast<size_t> (i)];
    }
  Message91 message91 {};
  std::copy_n (cw_unsorted.begin (), kLdpc17491K, message91.begin ());
  std::array<signed char, 96> m96 {};
  std::copy_n (cw_unsorted.begin (), 77, m96.begin ());
  std::copy_n (cw_unsorted.begin () + 77, 14, m96.begin () + 82);
  if (crc14_bits_local (m96.data (), 96) != 0)
    {
      nhardmin = -nhardmin;
    }

  if (message91_out)
    {
      std::copy (message91.begin (), message91.end (), message91_out);
    }
  if (cw_out)
    {
      std::copy (cw_unsorted.begin (), cw_unsorted.end (), cw_out);
    }
  if (nharderror_out)
    {
      *nharderror_out = nhardmin;
    }
  if (dmin_out)
    {
      *dmin_out = dmin;
    }
}

}

extern "C" void ftx_decode174_91_c (float const* llr_in, int Keff, int maxosd, int norder,
                                     signed char const* apmask_in, signed char* message91_out,
                                     signed char* cw_out, int* ntype_out,
                                     int* nharderror_out, float* dmin_out)
{
  clear_outputs (message91_out, cw_out, ntype_out, nharderror_out, dmin_out);
  if (!llr_in || !apmask_in)
    {
      return;
    }

  auto const& tables = ldpc17491_tables ();
  if (!tables.ready)
    {
      return;
    }

  int nosd = 0;
  maxosd = std::min (maxosd, 3);
  std::array<std::array<float, kLdpc17491N>, 3> zsave {};
  std::array<float, kLdpc17491N> llr {};
  std::array<float, kLdpc17491N> zn {};
  std::array<float, kLdpc17491N> zsum {};
  std::array<std::array<float, kLdpc17491ColWeight>, kLdpc17491N> tov {};
  std::array<std::array<float, kLdpc17491MaxRowWeight>, kLdpc17491M> toc {};
  std::array<signed char, kLdpc17491N> cw {};
  std::array<int, kLdpc17491M> synd {};

  std::copy_n (llr_in, kLdpc17491N, llr.begin ());
  if (maxosd == 0)
    {
      nosd = 1;
      zsave[0] = llr;
    }
  else if (maxosd > 0)
    {
      nosd = maxosd;
    }

  for (int check = 0; check < kLdpc17491M; ++check)
    {
      for (int row = 0; row < tables.nrw[static_cast<size_t> (check)]; ++row)
        {
          int const bit = nm_at (tables, row, check) - 1;
          toc[static_cast<size_t> (check)][static_cast<size_t> (row)] = llr[static_cast<size_t> (bit)];
        }
    }

  int ncnt = 0;
  int nclast = 0;
  zsum.fill (0.0f);

  for (int iter = 0; iter <= 30; ++iter)
    {
      for (int bit = 0; bit < kLdpc17491N; ++bit)
        {
          if (apmask_in[bit] != 1)
            {
              zn[static_cast<size_t> (bit)] = llr[static_cast<size_t> (bit)]
                + tov[static_cast<size_t> (bit)][0]
                + tov[static_cast<size_t> (bit)][1]
                + tov[static_cast<size_t> (bit)][2];
            }
          else
            {
              zn[static_cast<size_t> (bit)] = llr[static_cast<size_t> (bit)];
            }
          zsum[static_cast<size_t> (bit)] += zn[static_cast<size_t> (bit)];
        }

      if (iter > 0 && iter <= maxosd)
        {
          zsave[static_cast<size_t> (iter - 1)] = zsum;
        }

      for (int bit = 0; bit < kLdpc17491N; ++bit)
        {
          cw[static_cast<size_t> (bit)] = zn[static_cast<size_t> (bit)] > 0.0f ? 1 : 0;
        }

      int ncheck = 0;
      for (int check = 0; check < kLdpc17491M; ++check)
        {
          int parity = 0;
          for (int row = 0; row < tables.nrw[static_cast<size_t> (check)]; ++row)
            {
              int const bit = nm_at (tables, row, check) - 1;
              parity += cw[static_cast<size_t> (bit)] != 0 ? 1 : 0;
            }
          synd[static_cast<size_t> (check)] = parity;
          if ((parity & 1) != 0)
            {
              ++ncheck;
            }
        }

      if (ncheck == 0)
        {
          int nhard = -1;
          float dmin = 0.0f;
          if (ftx_ldpc174_91_finalize_c (cw.data (), llr.data (), message91_out, &nhard, &dmin))
            {
              if (cw_out)
                {
                  std::copy (cw.begin (), cw.end (), cw_out);
                }
              if (ntype_out)
                {
                  *ntype_out = 1;
                }
              if (nharderror_out)
                {
                  *nharderror_out = nhard;
                }
              if (dmin_out)
                {
                  *dmin_out = dmin;
                }
              return;
            }
        }

      if (iter > 0)
        {
          int const nd = ncheck - nclast;
          if (nd < 0)
            {
              ncnt = 0;
            }
          else
            {
              ++ncnt;
            }
          if (ncnt >= 5 && iter >= 10 && ncheck > 15)
            {
              break;
            }
        }
      nclast = ncheck;

      for (int check = 0; check < kLdpc17491M; ++check)
        {
          for (int row = 0; row < tables.nrw[static_cast<size_t> (check)]; ++row)
            {
              int const bit = nm_at (tables, row, check) - 1;
              float value = zn[static_cast<size_t> (bit)];
              for (int edge = 0; edge < tables.ncw; ++edge)
                {
                  if (mn_at (tables, edge, bit) == check + 1)
                    {
                      value -= tov[static_cast<size_t> (bit)][static_cast<size_t> (edge)];
                      break;
                    }
                }
              toc[static_cast<size_t> (check)][static_cast<size_t> (row)] = value;
            }
        }

      for (int bit = 0; bit < kLdpc17491N; ++bit)
        {
          for (int edge = 0; edge < tables.ncw; ++edge)
            {
              int const check = mn_at (tables, edge, bit) - 1;
              float sign_prod = 1.0f;
              float min_abs = 1.0e30f;
              for (int row = 0; row < tables.nrw[static_cast<size_t> (check)]; ++row)
                {
                  if (nm_at (tables, row, check) != bit + 1)
                    {
                      float const value = toc[static_cast<size_t> (check)][static_cast<size_t> (row)];
                      sign_prod *= sign_one (-value);
                      min_abs = std::min (min_abs, std::fabs (value));
                    }
                }
              tov[static_cast<size_t> (bit)][static_cast<size_t> (edge)] = kAlphaMs * sign_prod * min_abs;
            }
        }
    }

  for (int i = 0; i < nosd; ++i)
    {
      std::array<signed char, kLdpc17491K> message91 {};
      std::array<signed char, kLdpc17491N> cw_osd {};
      std::array<signed char, kLdpc17491N> apmask {};
      std::copy_n (apmask_in, kLdpc17491N, apmask.begin ());
      int nhard = -1;
      float dmin = 0.0f;
      auto zn_osd = zsave[static_cast<size_t> (i)];
      osd174_91_cpp (zn_osd.data (), Keff, apmask.data (), norder, message91.data (),
                     cw_osd.data (), &nhard, &dmin);
      if (nhard >= 0)
        {
          ftx_ldpc174_91_metrics_c (cw_osd.data (), llr.data (), &nhard, &dmin);
          if (message91_out)
            {
              std::copy (message91.begin (), message91.end (), message91_out);
            }
          if (cw_out)
            {
              std::copy (cw_osd.begin (), cw_osd.end (), cw_out);
            }
          if (ntype_out)
            {
              *ntype_out = 2;
            }
          if (nharderror_out)
            {
              *nharderror_out = nhard;
            }
          if (dmin_out)
            {
              *dmin_out = dmin;
            }
          return;
        }
    }
}

extern "C" void ftx_ldpc174_91_tables_c (int* Mn_out, int* Nm_out, int* nrw_out, int* ncw_out)
{
  auto const& tables = ldpc17491_tables ();
  if (!tables.ready)
    {
      if (ncw_out)
        {
          *ncw_out = 0;
        }
      return;
    }
  if (Mn_out)
    {
      std::copy (tables.mn.begin (), tables.mn.end (), Mn_out);
    }
  if (Nm_out)
    {
      std::copy (tables.nm.begin (), tables.nm.end (), Nm_out);
    }
  if (nrw_out)
    {
      std::copy (tables.nrw.begin (), tables.nrw.end (), nrw_out);
    }
  if (ncw_out)
    {
      *ncw_out = tables.ncw;
    }
}

extern "C" void ftx_osd174_91_c (float const* llr_in, int Keff, signed char const* apmask_in,
                                  int norder, signed char* message91_out, signed char* cw_out,
                                  int* nharderror_out, float* dmin_out)
{
  if (message91_out)
    {
      std::fill_n (message91_out, kLdpc17491K, static_cast<signed char> (0));
    }
  if (cw_out)
    {
      std::fill_n (cw_out, kLdpc17491N, static_cast<signed char> (0));
    }
  if (nharderror_out)
    {
      *nharderror_out = -1;
    }
  if (dmin_out)
    {
      *dmin_out = 0.0f;
    }
  if (!llr_in || !apmask_in)
    {
      return;
    }

  osd174_91_cpp (llr_in, Keff, apmask_in, norder, message91_out, cw_out, nharderror_out, dmin_out);
}

extern "C" void encode174_91_nocrc_ (signed char* message_in, signed char* codeword_out)
{
  if (!codeword_out)
    {
      return;
    }

  std::fill_n (codeword_out, kLdpc17491N, static_cast<signed char> (0));
  if (!message_in)
    {
      return;
    }

  Message91 message {};
  for (int i = 0; i < kLdpc17491K; ++i)
    {
      signed char const value = message_in[i];
      if (value != 0 && value != 1)
        {
          return;
        }
      message[static_cast<size_t> (i)] = value != 0 ? 1 : 0;
    }

  Codeword174 codeword {};
  encode174_91_nocrc_cpp (message, codeword);
  std::copy (codeword.begin (), codeword.end (), codeword_out);
}

extern "C" void osd174_91_ (float* llr_in, int* Keff, signed char* apmask_in, int* norder,
                             signed char* message91_out, signed char* cw_out,
                             int* nharderror_out, float* dmin_out)
{
  if (message91_out)
    {
      std::fill_n (message91_out, kLdpc17491K, static_cast<signed char> (0));
    }
  if (cw_out)
    {
      std::fill_n (cw_out, kLdpc17491N, static_cast<signed char> (0));
    }
  if (nharderror_out)
    {
      *nharderror_out = -1;
    }
  if (dmin_out)
    {
      *dmin_out = 0.0f;
    }
  if (!llr_in || !Keff || !apmask_in || !norder)
    {
      return;
    }

  ftx_osd174_91_c (llr_in, *Keff, apmask_in, *norder, message91_out, cw_out,
                   nharderror_out, dmin_out);
}

extern "C" void ftx_decode174_91_legacy_c (float const* llr_in, int Keff, int maxosd, int norder,
                                            signed char const* apmask_in, signed char* message91_out,
                                            signed char* cw_out, int* ntype_out,
                                            int* nharderror_out, float* dmin_out)
{
  ftx_decode174_91_c (llr_in, Keff, maxosd, norder, apmask_in, message91_out, cw_out, ntype_out,
                      nharderror_out, dmin_out);
}

extern "C" void decode174_91_ (float* llr_in, int* Keff, int* maxosd, int* norder,
                                signed char* apmask_in, signed char* message91_out,
                                signed char* cw_out, int* ntype_out,
                                int* nharderror_out, float* dmin_out)
{
  ftx_decode174_91_c (llr_in,
                      Keff ? *Keff : 0,
                      maxosd ? *maxosd : 0,
                      norder ? *norder : 0,
                      apmask_in,
                      message91_out,
                      cw_out,
                      ntype_out,
                      nharderror_out,
                      dmin_out);
}

extern "C" void chkcrc13a_ (signed char* decoded, int* nbadcrc)
{
  if (nbadcrc)
    {
      *nbadcrc = 1;
    }
  if (!decoded || !nbadcrc)
    {
      return;
    }

  auto const data = pack_bits_msb<12> (decoded, 77);
  int const expected = read_bitfield_msb (decoded, 77, 13);
  *nbadcrc = (static_cast<int> (crc13 (data.data (), static_cast<int> (data.size ()))) == expected)
               ? 0
               : 1;
}

extern "C" void chkcrc14a_ (signed char* decoded, int* nbadcrc)
{
  if (nbadcrc)
    {
      *nbadcrc = 1;
    }
  if (!decoded || !nbadcrc)
    {
      return;
    }

  auto const data = pack_bits_msb<12> (decoded, 77);
  int const expected = read_bitfield_msb (decoded, 77, 14);
  *nbadcrc = (static_cast<int> (crc14 (data.data (), static_cast<int> (data.size ()))) == expected)
               ? 0
               : 1;
}

extern "C" void get_crc14_ (signed char* mc, int* len, int* ncrc)
{
  if (ncrc)
    {
      *ncrc = 0;
    }
  if (!mc || !len || !ncrc || *len < 15)
    {
      return;
    }

  static constexpr std::array<int, 15> poly {{1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1}};
  std::array<int, 15> r {};
  for (int i = 0; i < 15; ++i)
    {
      r[static_cast<size_t> (i)] = mc[i] != 0 ? 1 : 0;
    }
  for (int i = 0; i <= *len - 15; ++i)
    {
      r[14] = mc[i + 14] != 0 ? 1 : 0;
      int const lead = r[0];
      for (int j = 0; j < 15; ++j)
        {
          r[static_cast<size_t> (j)] =
            (r[static_cast<size_t> (j)] + lead * poly[static_cast<size_t> (j)]) % 2;
        }
      std::rotate (r.begin (), r.begin () + 1, r.end ());
    }

  int value = 0;
  for (int i = 0; i < 14; ++i)
    {
      value = (value << 1) | r[static_cast<size_t> (i)];
    }
  *ncrc = value;
}
