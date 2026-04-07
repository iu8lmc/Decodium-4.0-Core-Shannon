#include "Detector/FanoSequentialDecoder.hpp"
#include "Modulator/LegacyJtEncoder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{

constexpr int kConv232MaxBytes = 13;
constexpr int kConv232EncodedBits = 206;
constexpr int kConv232TailBits = 31;
constexpr int kConv232MetricRows = 256;

void encode232_bytes (signed char const* dat, int nbytes, signed char symbol[], int maxsym)
{
  if (!dat || !symbol || nbytes <= 0 || maxsym <= 0)
    {
      return;
    }

  std::uint32_t state = 0;
  int k = 0;
  for (int j = 0; j < nbytes && k < maxsym; ++j)
    {
      unsigned int const byte = static_cast<unsigned char> (dat[j]);
      for (int i = 7; i >= 0 && k < maxsym; --i)
        {
          state = (state << 1) | static_cast<std::uint32_t> ((byte >> i) & 1U);
          symbol[k++] = static_cast<signed char> (
              decodium::legacy_jt::detail::parity32 (state & decodium::legacy_jt::detail::kConv232Poly1));
          if (k < maxsym)
            {
              symbol[k++] = static_cast<signed char> (
                  decodium::legacy_jt::detail::parity32 (state & decodium::legacy_jt::detail::kConv232Poly2));
            }
        }
    }
}

void build_signed_metric_table (int const* mettab_flat, int mettab_c[2][256])
{
  for (int u = 0; u < kConv232MetricRows; ++u)
    {
      int const signed_offset =
          static_cast<int> (static_cast<std::int8_t> (static_cast<unsigned char> (u))) + 128;
      mettab_c[0][u] = mettab_flat[signed_offset];
      mettab_c[1][u] = mettab_flat[signed_offset + kConv232MetricRows];
    }
}

void build_unsigned_metric_table (int const* mettab_flat, int mettab_c[2][256])
{
  for (int u = 0; u < kConv232MetricRows; ++u)
    {
      mettab_c[0][u] = mettab_flat[u];
      mettab_c[1][u] = mettab_flat[u + kConv232MetricRows];
    }
}

}

extern "C" void encode232_ (signed char dat[], int* nsym, signed char symbol[])
{
  if (!dat || !nsym || !symbol)
    {
      return;
    }

  std::array<signed char, kConv232MaxBytes> input {};
  std::copy_n (dat, static_cast<int> (input.size ()), input.begin ());

  int const output_bits = std::max (0, std::min (*nsym, kConv232EncodedBits));
  encode232_bytes (input.data (), static_cast<int> (input.size ()), symbol, output_bits);
}

extern "C" void encode232_wspr_ (signed char dat[], int* nbytes, signed char symbol[],
                                 int* maxsym)
{
  if (!dat || !nbytes || !symbol || !maxsym)
    {
      return;
    }

  encode232_bytes (dat, *nbytes, symbol, *maxsym);
}

extern "C" void fano232_ (signed char symbol[], int* nbits, int mettab[], int* ndelta,
                          int* maxcycles, signed char dat[], int* ncycles, int* metric,
                          int* ierr)
{
  if (ierr) { *ierr = -1; }
  if (ncycles) { *ncycles = 0; }
  if (metric) { *metric = 0; }
  if (!symbol || !nbits || !mettab || !ndelta || !maxcycles || !dat || !ncycles || !metric || !ierr)
    {
      return;
    }

  int const bit_count = *nbits;
  if (bit_count < kConv232TailBits || *maxcycles <= 0)
    {
      return;
    }

  std::vector<unsigned char> symbols (static_cast<std::size_t> (bit_count * 2));
  for (int i = 0; i < bit_count * 2; ++i)
    {
      symbols[static_cast<std::size_t> (i)] = static_cast<unsigned char> (symbol[i]);
    }

  int mettab_c[2][256] {};
  build_signed_metric_table (mettab, mettab_c);

  std::size_t const byte_count = static_cast<std::size_t> ((bit_count + 7) / 8);
  std::vector<unsigned char> decoded (byte_count, 0);
  unsigned int decoded_metric = 0;
  unsigned int decoded_cycles = 0;
  unsigned int max_depth = 0;
  int const status = decodium::fano::sequential_decode (&decoded_metric, &decoded_cycles, &max_depth,
                                                        decoded.data (), symbols.data (),
                                                        static_cast<unsigned int> (bit_count),
                                                        mettab_c, *ndelta,
                                                        static_cast<unsigned int> (*maxcycles));

  std::fill_n (dat, byte_count, 0);
  for (std::size_t i = 0; i < byte_count; ++i)
    {
      dat[i] = static_cast<signed char> (decoded[i]);
    }
  *metric = static_cast<int> (decoded_metric);
  *ncycles = static_cast<int> (decoded_cycles);
  *ierr = status;
}

extern "C" void fano232_wspr_ (signed char symbol[], int* nbits, int mettab[], int* ndelta,
                               int* maxcycles, signed char dat[], int* ncycles, int* metric,
                               int* ierr)
{
  if (ierr) { *ierr = -1; }
  if (ncycles) { *ncycles = 0; }
  if (metric) { *metric = 0; }
  if (!symbol || !nbits || !mettab || !ndelta || !maxcycles || !dat || !ncycles || !metric || !ierr)
    {
      return;
    }

  int const bit_count = *nbits;
  if (bit_count < kConv232TailBits || *maxcycles <= 0)
    {
      return;
    }

  std::vector<unsigned char> symbols (static_cast<std::size_t> (bit_count * 2));
  for (int i = 0; i < bit_count * 2; ++i)
    {
      symbols[static_cast<std::size_t> (i)] = static_cast<unsigned char> (symbol[i]);
    }

  int mettab_c[2][256] {};
  build_unsigned_metric_table (mettab, mettab_c);
  std::vector<unsigned long> nstate (static_cast<std::size_t> (bit_count), 0UL);
  std::vector<int> gamma (static_cast<std::size_t> (bit_count), 0);
  std::vector<std::array<int, 4>> metrics (static_cast<std::size_t> (bit_count));
  std::vector<std::array<int, 2>> tm (static_cast<std::size_t> (bit_count));
  std::vector<int> ii (static_cast<std::size_t> (bit_count), 0);

  for (int np = 0; np < bit_count; ++np)
    {
      int const j = 2 * np;
      int const i4a = symbols[static_cast<std::size_t> (j)];
      int const i4b = symbols[static_cast<std::size_t> (j + 1)];
      metrics[static_cast<std::size_t> (np)][0] = mettab_c[0][i4a] + mettab_c[0][i4b];
      metrics[static_cast<std::size_t> (np)][1] = mettab_c[0][i4a] + mettab_c[1][i4b];
      metrics[static_cast<std::size_t> (np)][2] = mettab_c[1][i4a] + mettab_c[0][i4b];
      metrics[static_cast<std::size_t> (np)][3] = mettab_c[1][i4a] + mettab_c[1][i4b];
    }

  auto wspr_lsym = [] (unsigned long state) {
    unsigned int const s = static_cast<unsigned int> (state);
    unsigned int lsym =
        decodium::legacy_jt::detail::parity32 (s & decodium::legacy_jt::detail::kConv232Poly1);
    lsym = (lsym << 1) |
           decodium::legacy_jt::detail::parity32 (s & decodium::legacy_jt::detail::kConv232Poly2);
    return static_cast<int> (lsym);
  };

  int const ntail = bit_count - 31;
  int np = 0;
  nstate[0] = 0;
  int lsym = wspr_lsym (nstate[0]);
  int m0 = metrics[0][static_cast<std::size_t> (lsym)];
  int m1 = metrics[0][static_cast<std::size_t> (3 ^ lsym)];
  if (m0 > m1) { tm[0][0] = m0; tm[0][1] = m1; }
  else { tm[0][0] = m1; tm[0][1] = m0; nstate[0] += 1UL; }
  ii[0] = 0;
  gamma[0] = 0;
  int nt = 0;
  int i = 0;

  for (i = 1; i <= bit_count * (*maxcycles); ++i)
    {
      int const ngamma = gamma[static_cast<std::size_t> (np)] +
                         tm[static_cast<std::size_t> (np)][static_cast<std::size_t> (ii[static_cast<std::size_t> (np)])];
      if (ngamma >= nt)
        {
          if (gamma[static_cast<std::size_t> (np)] < (nt + *ndelta))
            nt = nt + *ndelta * ((ngamma - nt) / *ndelta);

          gamma[static_cast<std::size_t> (np + 1)] = ngamma;
          nstate[static_cast<std::size_t> (np + 1)] = nstate[static_cast<std::size_t> (np)] << 1;
          ++np;
          if (np == bit_count - 1) break;

          lsym = wspr_lsym (nstate[static_cast<std::size_t> (np)]);
          if (np >= ntail)
            {
              tm[static_cast<std::size_t> (np)][0] =
                  metrics[static_cast<std::size_t> (np)][static_cast<std::size_t> (lsym)];
            }
          else
            {
              m0 = metrics[static_cast<std::size_t> (np)][static_cast<std::size_t> (lsym)];
              m1 = metrics[static_cast<std::size_t> (np)][static_cast<std::size_t> (3 ^ lsym)];
              if (m0 > m1) { tm[static_cast<std::size_t> (np)][0] = m0; tm[static_cast<std::size_t> (np)][1] = m1; }
              else { tm[static_cast<std::size_t> (np)][0] = m1; tm[static_cast<std::size_t> (np)][1] = m0; nstate[static_cast<std::size_t> (np)] += 1UL; }
            }
          ii[static_cast<std::size_t> (np)] = 0;
          continue;
        }

      for (;;)
        {
          bool noback = (np == 0);
          if (np > 0 && gamma[static_cast<std::size_t> (np - 1)] < nt) noback = true;

          if (noback)
            {
              nt -= *ndelta;
              if (ii[static_cast<std::size_t> (np)] != 0)
                { ii[static_cast<std::size_t> (np)] = 0; nstate[static_cast<std::size_t> (np)] ^= 1UL; }
              break;
            }

          --np;
          if (np < ntail && ii[static_cast<std::size_t> (np)] != 1)
            { ++ii[static_cast<std::size_t> (np)]; nstate[static_cast<std::size_t> (np)] ^= 1UL; break; }
        }
    }

  *metric = gamma[static_cast<std::size_t> (np)];
  std::size_t const byte_count = static_cast<std::size_t> ((bit_count + 7) / 8);
  std::fill_n (dat, byte_count, 0);
  int state_index = 7;
  for (std::size_t j = 0; j + 1 < byte_count; ++j)
    {
      dat[j] = static_cast<signed char> (nstate[static_cast<std::size_t> (state_index)]);
      state_index += 8;
    }

  *ncycles = i + 1;
  *ierr = (i >= bit_count * (*maxcycles)) ? -1 : 0;
}
