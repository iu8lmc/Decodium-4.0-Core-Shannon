// -*- Mode: C++ -*-
#include "Detector/FtxFst4Ldpc.hpp"

#include "wsjtx_config.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <functional>
#include <limits>
#include <numeric>
#include <vector>

#include "Modulator/FtxFst4LdpcData.hpp"

extern "C"
{
  void four2a_ (std::complex<float> a[], int* nfft, int* ndim, int* isign, int* iform);
  void gen_fst4wave_ (int* itone, int* nsym, int* nsps, int* nwave, float* fsample,
                      int* hmod, float* f0, int* icmplx, std::complex<float>* cwave, float* wave);
}

namespace
{
  using Complex = std::complex<float>;

  constexpr int kCodewordBits {240};
  constexpr int kFst4PayloadBits {77};
  constexpr int kFst4MessageBits {101};
  constexpr int kFst4WPayloadBits {50};
  constexpr int kFst4WMessageBits {74};
  constexpr int kFst4TotalSymbols {160};
  constexpr int kFst4SampleRate {12000};

  using Codeword240 = std::array<signed char, kCodewordBits>;
  using MessageBits101 = std::array<signed char, kFst4MessageBits>;
  using MessageBits74 = std::array<signed char, kFst4WMessageBits>;
  using GeneratorRow101 = std::array<unsigned char, kFst4MessageBits>;
  using GeneratorRow74 = std::array<unsigned char, kFst4WMessageBits>;
  using GeneratorMatrix240_101 = std::array<GeneratorRow101, kCodewordBits - kFst4MessageBits>;
  using GeneratorMatrix240_74 = std::array<GeneratorRow74, kCodewordBits - kFst4WMessageBits>;
  using BasisRows101 = std::array<Codeword240, kFst4MessageBits>;
  using BasisRows74 = std::array<Codeword240, kFst4WMessageBits>;

  constexpr std::array<int, 25> kFst4Crc24Poly {{
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1
  }};

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

  GeneratorMatrix240_101 const& parity_matrix_240_101 ()
  {
    static GeneratorMatrix240_101 matrix =
        build_hex_generator_matrix<GeneratorMatrix240_101> (kFst4Ldpc240_101Hex, 1);
    return matrix;
  }

  GeneratorMatrix240_74 const& parity_matrix_240_74 ()
  {
    static GeneratorMatrix240_74 matrix =
        build_hex_generator_matrix<GeneratorMatrix240_74> (kFst4Ldpc240_74Hex, 2);
    return matrix;
  }

  int fst4_crc24_local (signed char const* bits, int len)
  {
    if (!bits || len < 25)
      {
        return 0;
      }

    std::array<int, 25> r {};
    for (int i = 0; i < 25; ++i)
      {
        r[static_cast<size_t> (i)] = bits[i] != 0 ? 1 : 0;
      }

    for (int i = 0; i <= len - 25; ++i)
      {
        r[24] = bits[i + 24] != 0 ? 1 : 0;
        int const lead = r[0];
        for (int j = 0; j < 25; ++j)
          {
            r[static_cast<size_t> (j)] =
                (r[static_cast<size_t> (j)] + lead * kFst4Crc24Poly[static_cast<size_t> (j)]) & 1;
          }
        int const wrapped = r[0];
        for (int j = 0; j < 24; ++j)
          {
            r[static_cast<size_t> (j)] = r[static_cast<size_t> (j + 1)];
          }
        r[24] = wrapped;
      }

    int crc = 0;
    for (int i = 0; i < 24; ++i)
      {
        crc = (crc << 1) | r[static_cast<size_t> (i)];
      }
    return crc;
  }

  Codeword240 encode240_101_full (MessageBits101 const& bits)
  {
    Codeword240 codeword {};
    for (int i = 0; i < kFst4MessageBits; ++i)
      {
        codeword[static_cast<size_t> (i)] = bits[static_cast<size_t> (i)] != 0 ? 1 : 0;
      }

    auto const& parity = parity_matrix_240_101 ();
    for (int row = 0; row < static_cast<int> (parity.size ()); ++row)
      {
        int accum = 0;
        for (int col = 0; col < kFst4MessageBits; ++col)
          {
            accum ^= (bits[static_cast<size_t> (col)] != 0 ? 1 : 0)
                  & static_cast<int> (parity[static_cast<size_t> (row)][static_cast<size_t> (col)]);
          }
        codeword[static_cast<size_t> (kFst4MessageBits + row)] = static_cast<signed char> (accum & 1);
      }
    return codeword;
  }

  Codeword240 encode240_74_full (MessageBits74 const& bits)
  {
    Codeword240 codeword {};
    for (int i = 0; i < kFst4WMessageBits; ++i)
      {
        codeword[static_cast<size_t> (i)] = bits[static_cast<size_t> (i)] != 0 ? 1 : 0;
      }

    auto const& parity = parity_matrix_240_74 ();
    for (int row = 0; row < static_cast<int> (parity.size ()); ++row)
      {
        int accum = 0;
        for (int col = 0; col < kFst4WMessageBits; ++col)
          {
            accum ^= (bits[static_cast<size_t> (col)] != 0 ? 1 : 0)
                  & static_cast<int> (parity[static_cast<size_t> (row)][static_cast<size_t> (col)]);
          }
        codeword[static_cast<size_t> (kFst4WMessageBits + row)] = static_cast<signed char> (accum & 1);
      }
    return codeword;
  }

  BasisRows101 const& basis_rows_240_101 (int keff)
  {
    static std::array<BasisRows101, kFst4MessageBits + 1> cache {};
    static std::array<bool, kFst4MessageBits + 1> initialized {};
    int const k = std::max (kFst4PayloadBits, std::min (keff, kFst4MessageBits));
    if (!initialized[static_cast<size_t> (k)])
      {
        BasisRows101 rows {};
        for (int i = 0; i < k; ++i)
          {
            MessageBits101 message {};
            message[static_cast<size_t> (i)] = 1;
            if (i < kFst4PayloadBits)
              {
                int const crc = fst4_crc24_local (message.data (), kFst4MessageBits);
                for (int bit = 0; bit < 24; ++bit)
                  {
                    message[static_cast<size_t> (kFst4PayloadBits + bit)] =
                        static_cast<signed char> ((crc >> (23 - bit)) & 1);
                  }
                for (int bit = kFst4PayloadBits; bit < k; ++bit)
                  {
                    message[static_cast<size_t> (bit)] = 0;
                  }
              }
            rows[static_cast<size_t> (i)] = encode240_101_full (message);
          }
        cache[static_cast<size_t> (k)] = rows;
        initialized[static_cast<size_t> (k)] = true;
      }
    return cache[static_cast<size_t> (k)];
  }

  BasisRows74 const& basis_rows_240_74 (int keff)
  {
    static std::array<BasisRows74, kFst4WMessageBits + 1> cache {};
    static std::array<bool, kFst4WMessageBits + 1> initialized {};
    int const k = std::max (kFst4WPayloadBits, std::min (keff, kFst4WMessageBits));
    if (!initialized[static_cast<size_t> (k)])
      {
        BasisRows74 rows {};
        for (int i = 0; i < k; ++i)
          {
            MessageBits74 message {};
            message[static_cast<size_t> (i)] = 1;
            if (i < kFst4WPayloadBits)
              {
                int const crc = fst4_crc24_local (message.data (), kFst4WMessageBits);
                for (int bit = 0; bit < 24; ++bit)
                  {
                    message[static_cast<size_t> (kFst4WPayloadBits + bit)] =
                        static_cast<signed char> ((crc >> (23 - bit)) & 1);
                  }
                for (int bit = kFst4WPayloadBits; bit < k; ++bit)
                  {
                    message[static_cast<size_t> (bit)] = 0;
                  }
              }
            rows[static_cast<size_t> (i)] = encode240_74_full (message);
          }
        cache[static_cast<size_t> (k)] = rows;
        initialized[static_cast<size_t> (k)] = true;
      }
    return cache[static_cast<size_t> (k)];
  }

  template <size_t MaxK>
  Codeword240 encode_from_basis_rows (std::array<Codeword240, MaxK> const& rows,
                                      std::array<signed char, MaxK> const& message,
                                      int k)
  {
    Codeword240 codeword {};
    for (int bit = 0; bit < k; ++bit)
      {
        if (message[static_cast<size_t> (bit)] == 0)
          {
            continue;
          }
        for (int col = 0; col < kCodewordBits; ++col)
          {
            codeword[static_cast<size_t> (col)] ^=
                rows[static_cast<size_t> (bit)][static_cast<size_t> (col)];
          }
      }
    return codeword;
  }

  float weighted_distance (Codeword240 const& codeword,
                           std::array<signed char, kCodewordBits> const& hard,
                           std::array<float, kCodewordBits> const& absllr)
  {
    float distance = 0.0f;
    for (int i = 0; i < kCodewordBits; ++i)
      {
        if (codeword[static_cast<size_t> (i)] != hard[static_cast<size_t> (i)])
          {
            distance += absllr[static_cast<size_t> (i)];
          }
      }
    return distance;
  }

  template <size_t MaxK>
  struct MrbBasis
  {
    std::array<Codeword240, MaxK> rows {};
    std::array<int, kCodewordBits> indices {};
    std::array<signed char, kCodewordBits> hard {};
    std::array<float, kCodewordBits> absllr {};
    std::array<signed char, kCodewordBits> apmask {};
  };

  template <size_t MaxK>
  MrbBasis<MaxK> build_mrb_basis (std::array<Codeword240, MaxK> const& source_rows,
                                  std::array<float, kCodewordBits> const& llr,
                                  std::array<signed char, kCodewordBits> const& apmask,
                                  int k)
  {
    MrbBasis<MaxK> basis;
    std::array<int, kCodewordBits> order {};
    std::iota (order.begin (), order.end (), 0);
    std::sort (order.begin (), order.end (), [&llr] (int lhs, int rhs) {
      float const al = std::abs (llr[static_cast<size_t> (lhs)]);
      float const ar = std::abs (llr[static_cast<size_t> (rhs)]);
      if (al == ar)
        {
          return lhs < rhs;
        }
      return al > ar;
    });

    for (int col = 0; col < kCodewordBits; ++col)
      {
        basis.indices[static_cast<size_t> (col)] = order[static_cast<size_t> (col)];
      }

    for (int row = 0; row < k; ++row)
      {
        for (int col = 0; col < kCodewordBits; ++col)
          {
            basis.rows[static_cast<size_t> (row)][static_cast<size_t> (col)] =
                source_rows[static_cast<size_t> (row)]
                           [static_cast<size_t> (basis.indices[static_cast<size_t> (col)])];
          }
      }

    std::array<int, kCodewordBits> indices2 {};
    indices2.fill (-1);
    int icol = 0;
    int nskipped = 0;
    for (int id = 0; id < k; ++id)
      {
        bool found = false;
        while (!found && icol < kCodewordBits)
          {
            if (basis.rows[static_cast<size_t> (id)][static_cast<size_t> (icol)] == 1)
              {
                found = true;
              }
            else
              {
                for (int j = id + 1; j < k; ++j)
                  {
                    if (basis.rows[static_cast<size_t> (j)][static_cast<size_t> (icol)] == 1)
                      {
                        std::swap (basis.rows[static_cast<size_t> (id)],
                                   basis.rows[static_cast<size_t> (j)]);
                        found = true;
                        break;
                      }
                  }
              }
            if (!found)
              {
                indices2[static_cast<size_t> (k + nskipped)] = icol;
                ++nskipped;
                ++icol;
              }
          }
        if (!found || icol >= kCodewordBits)
          {
            break;
          }

        indices2[static_cast<size_t> (id)] = icol;
        for (int j = 0; j < k; ++j)
          {
            if (j == id
                || basis.rows[static_cast<size_t> (j)][static_cast<size_t> (icol)] == 0)
              {
                continue;
              }
            for (int col = 0; col < kCodewordBits; ++col)
              {
                basis.rows[static_cast<size_t> (j)][static_cast<size_t> (col)] ^=
                    basis.rows[static_cast<size_t> (id)][static_cast<size_t> (col)];
              }
          }
        ++icol;
      }
    for (int i = k + nskipped; i < kCodewordBits; ++i)
      {
        indices2[static_cast<size_t> (i)] = i;
      }
    for (int i = 0; i < kCodewordBits; ++i)
      {
        if (indices2[static_cast<size_t> (i)] < 0)
          {
            indices2[static_cast<size_t> (i)] = i;
          }
      }

    std::array<int, kCodewordBits> remapped_indices {};
    for (int i = 0; i < kCodewordBits; ++i)
      {
        remapped_indices[static_cast<size_t> (i)] =
            basis.indices[static_cast<size_t> (indices2[static_cast<size_t> (i)])];
      }
    basis.indices = remapped_indices;

    for (int row = 0; row < k; ++row)
      {
        Codeword240 reordered {};
        for (int col = 0; col < kCodewordBits; ++col)
          {
            reordered[static_cast<size_t> (col)] =
                basis.rows[static_cast<size_t> (row)]
                          [static_cast<size_t> (indices2[static_cast<size_t> (col)])];
          }
        basis.rows[static_cast<size_t> (row)] = reordered;
      }

    for (int col = 0; col < kCodewordBits; ++col)
      {
        int const original = basis.indices[static_cast<size_t> (col)];
        basis.hard[static_cast<size_t> (col)] =
            llr[static_cast<size_t> (original)] >= 0.0f ? static_cast<signed char> (1)
                                                        : static_cast<signed char> (0);
        basis.absllr[static_cast<size_t> (col)] = std::abs (llr[static_cast<size_t> (original)]);
        basis.apmask[static_cast<size_t> (col)] = apmask[static_cast<size_t> (original)];
      }
    return basis;
  }

  Codeword240 restore_codeword_order (Codeword240 const& reordered,
                                      std::array<int, kCodewordBits> const& indices)
  {
    Codeword240 restored {};
    for (int i = 0; i < kCodewordBits; ++i)
      {
        restored[static_cast<size_t> (indices[static_cast<size_t> (i)])] =
            reordered[static_cast<size_t> (i)];
      }
    return restored;
  }

  int hard_error_count (Codeword240 const& codeword,
                        std::array<float, kCodewordBits> const& llr)
  {
    int count = 0;
    for (int i = 0; i < kCodewordBits; ++i)
      {
        int const hard = llr[static_cast<size_t> (i)] >= 0.0f ? 1 : 0;
        if ((codeword[static_cast<size_t> (i)] != 0 ? 1 : 0) != hard)
          {
            ++count;
          }
      }
    return count;
  }

  template <size_t Bits>
  bool crc_ok (std::array<signed char, Bits> const& message)
  {
    return fst4_crc24_local (message.data (), static_cast<int> (Bits)) == 0;
  }

  template <size_t MaxK, size_t Bits>
  bool decode_mrb_osd_generic (std::array<Codeword240, MaxK> const& generator_rows,
                               std::array<float, kCodewordBits> const& llr,
                               std::array<signed char, kCodewordBits> const& apmask,
                               int k, int maxosd, int norder, int search_span,
                               std::array<signed char, Bits>& message_out,
                               Codeword240& cw_out,
                               int& ntype_out, int& nharderror_out, float& dmin_out)
  {
    message_out.fill (0);
    cw_out.fill (0);
    ntype_out = 0;
    nharderror_out = -1;
    dmin_out = 0.0f;

    MrbBasis<MaxK> const basis = build_mrb_basis (generator_rows, llr, apmask, k);
    std::array<signed char, MaxK> m0 {};
    for (int i = 0; i < k; ++i)
      {
        m0[static_cast<size_t> (i)] = basis.hard[static_cast<size_t> (i)];
      }

    bool best_valid = false;
    Codeword240 best_codeword {};
    float best_distance = std::numeric_limits<float>::max ();
    int best_order = -1;

    auto try_message = [&] (std::array<signed char, MaxK> const& message, int order) {
      Codeword240 const reordered = encode_from_basis_rows (basis.rows, message, k);
      float const distance = weighted_distance (reordered, basis.hard, basis.absllr);
      if (distance >= best_distance)
        {
          return;
        }
      Codeword240 const restored = restore_codeword_order (reordered, basis.indices);
      std::array<signed char, Bits> decoded {};
      for (size_t i = 0; i < Bits; ++i)
        {
          decoded[i] = restored[i];
        }
      if (!crc_ok (decoded))
        {
          return;
        }
      best_valid = true;
      best_codeword = restored;
      best_distance = distance;
      best_order = order;
      message_out = decoded;
    };

    try_message (m0, 0);
    int const order_limit = (maxosd < 0) ? 0 : std::max (0, std::min (norder, 4));
    int const span = std::max (0, std::min (search_span, k));

    if (!best_valid && order_limit > 0 && span > 0)
      {
        std::array<int, 4> positions {};
        std::function<void (int, int, int, std::array<signed char, MaxK>&)> search;
        search = [&] (int start, int depth, int target_order,
                      std::array<signed char, MaxK>& working) {
          if (depth == target_order)
            {
              try_message (working, target_order);
              return;
            }
          for (int pos = start; pos < span; ++pos)
            {
              if (basis.apmask[static_cast<size_t> (pos)] == 1)
                {
                  continue;
                }
              working[static_cast<size_t> (pos)] ^= 1;
              positions[static_cast<size_t> (depth)] = pos;
              search (pos + 1, depth + 1, target_order, working);
              working[static_cast<size_t> (pos)] ^= 1;
            }
        };

        for (int order = 1; order <= order_limit && !best_valid; ++order)
          {
            std::array<signed char, MaxK> working = m0;
            search (0, 0, order, working);
          }
      }

    if (!best_valid)
      {
        return false;
      }

    cw_out = best_codeword;
    dmin_out = best_distance;
    ntype_out = best_order <= 0 ? 1 : 2;
    nharderror_out = hard_error_count (best_codeword, llr);
    return true;
  }

  int fst4_search_span_101 (int norder)
  {
    if (norder <= 0) return 0;
    if (norder == 1) return 18;
    if (norder == 2) return 28;
    return 40;
  }

  int fst4_search_span_74 (int keff, int norder)
  {
    if (norder <= 0) return 0;
    if (keff <= 50) return 18;
    if (norder == 1) return 16;
    if (norder == 2) return 22;
    if (norder == 3) return 28;
    return 32;
  }

  void circular_shift_left (std::vector<Complex>& values, int shift)
  {
    if (values.empty ())
      {
        return;
      }
    int const size = static_cast<int> (values.size ());
    int const wrapped = ((shift % size) + size) % size;
    std::rotate (values.begin (), values.begin () + wrapped, values.end ());
  }
}

namespace decodium
{
namespace fst4
{

bool decode240_101_native (std::array<float, 240> const& llr,
                           int keff, int maxosd, int norder,
                           std::array<signed char, 240> const& apmask,
                           std::array<signed char, 101>& message101_out,
                           std::array<signed char, 240>& cw_out,
                           int& ntype_out, int& nharderror_out, float& dmin_out)
{
  int const k = std::max (kFst4PayloadBits, std::min (keff, kFst4MessageBits));
  return decode_mrb_osd_generic<kFst4MessageBits, kFst4MessageBits> (
      basis_rows_240_101 (k), llr, apmask, k, maxosd, norder, fst4_search_span_101 (norder),
      message101_out, cw_out, ntype_out, nharderror_out, dmin_out);
}

bool decode240_74_native (std::array<float, 240> const& llr,
                          int keff, int maxosd, int norder,
                          std::array<signed char, 240> const& apmask,
                          std::array<signed char, 74>& message74_out,
                          std::array<signed char, 240>& cw_out,
                          int& ntype_out, int& nharderror_out, float& dmin_out)
{
  int const k = std::max (kFst4WPayloadBits, std::min (keff, kFst4WMessageBits));
  return decode_mrb_osd_generic<kFst4WMessageBits, kFst4WMessageBits> (
      basis_rows_240_74 (k), llr, apmask, k, maxosd, norder, fst4_search_span_74 (k, norder),
      message74_out, cw_out, ntype_out, nharderror_out, dmin_out);
}

void dopspread_native (std::array<int, 160> const& itone,
                       std::vector<short> const& iwave,
                       int nsps, int nmax, int ndown, int hmod,
                       int i0, float fc, float& fmid, float& w50)
{
  fmid = -999.0f;
  w50 = 0.0f;
  if (nsps <= 0 || nmax <= 0 || ndown <= 0 || iwave.empty ())
    {
      return;
    }

  int nfft = 2 * nmax;
  int nwave = std::max (nmax, (kFst4TotalSymbols + 2) * nsps);
  std::vector<Complex> cwave (static_cast<size_t> (nwave));
  std::vector<Complex> g (static_cast<size_t> (nfft));
  std::vector<float> wave (static_cast<size_t> (nwave), 0.0f);
  std::array<int, kFst4TotalSymbols> tones = itone;
  float fsample = static_cast<float> (kFst4SampleRate);
  int nsym = kFst4TotalSymbols;
  int icmplx = 1;
  gen_fst4wave_ (tones.data (), &nsym, &nsps, &nwave, &fsample, &hmod, &fc, &icmplx,
                 cwave.data (), wave.data ());

  circular_shift_left (cwave, i0 * ndown);
  float const fac = 1.0f / 32768.0f;
  int const copy_count = std::min (nmax, static_cast<int> (iwave.size ()));
  for (int i = 0; i < copy_count; ++i)
    {
      g[static_cast<size_t> (i)] =
          fac * static_cast<float> (iwave[static_cast<size_t> (i)])
        * std::conj (cwave[static_cast<size_t> (i)]);
    }
  for (int i = copy_count; i < nfft; ++i)
    {
      g[static_cast<size_t> (i)] = Complex {};
    }

  int ndim = 1;
  int isign = -1;
  int iform = 1;
  four2a_ (g.data (), &nfft, &ndim, &isign, &iform);

  float const df = static_cast<float> (kFst4SampleRate) / nfft;
  int ia = static_cast<int> (1.0f / df);
  float smax = 0.0f;
  for (int i = -ia; i <= ia; ++i)
    {
      int j = i;
      if (j < 0)
        {
          j += nfft;
        }
      smax = std::max (smax, std::norm (g[static_cast<size_t> (j)]));
    }
  if (!(smax > 0.0f))
    {
      return;
    }

  ia = static_cast<int> (10.1f / df);
  std::vector<float> ss (static_cast<size_t> (2 * ia + 1), 0.0f);
  float sum1 = 0.0f;
  float sum2 = 0.0f;
  int nns = 0;
  for (int i = -ia; i <= ia; ++i)
    {
      int j = i;
      if (j < 0)
        {
          j += nfft;
        }
      float const value = std::norm (g[static_cast<size_t> (j)]) / smax;
      ss[static_cast<size_t> (i + ia)] = value;
      float const f = i * df;
      if (f >= -4.0f && f <= -2.0f)
        {
          sum1 += value;
          ++nns;
        }
      else if (f >= 2.0f && f <= 4.0f)
        {
          sum2 += value;
        }
    }
  if (nns <= 0)
    {
      return;
    }
  float const avg = std::min (sum1 / nns, sum2 / nns);

  sum1 = 0.0f;
  for (int i = -ia; i <= ia; ++i)
    {
      float const f = i * df;
      if (std::abs (f) <= 1.0f)
        {
          sum1 += ss[static_cast<size_t> (i + ia)] - avg;
        }
    }
  if (!(sum1 > 0.0f))
    {
      return;
    }

  ia = static_cast<int> (std::lround (1.0f / df)) + 1;
  float accum = 0.0f;
  float accum_prev = 0.0f;
  float xi1 = -999.0f;
  float xi2 = -999.0f;
  float xi3 = -999.0f;
  for (int i = -ia; i <= ia; ++i)
    {
      accum += ss[static_cast<size_t> (i + static_cast<int> (10.1f / df))] - avg;
      if (accum >= 0.25f * sum1 && xi1 == -999.0f)
        {
          xi1 = i - 1 + (0.25f * sum1 - accum) / (accum - accum_prev);
        }
      if (accum >= 0.50f * sum1 && xi2 == -999.0f)
        {
          xi2 = i - 1 + (accum - 0.50f * sum1) / (accum - accum_prev);
        }
      if (accum >= 0.75f * sum1)
        {
          xi3 = i - 1 + (accum - 0.75f * sum1) / (accum - accum_prev);
          break;
        }
      accum_prev = accum;
    }
  if (xi1 == -999.0f || xi2 == -999.0f || xi3 == -999.0f)
    {
      return;
    }

  float const xdiff = std::sqrt (1.0f + (xi3 - xi1) * (xi3 - xi1));
  w50 = xdiff * df;
  fmid = xi2 * df;
}

}
}

extern "C" void decode240_101_ (float llr[], int* keff, int* maxosd, int* norder,
                                 signed char apmask[], signed char message101[],
                                 signed char cw[], int* ntype, int* nharderror, float* dmin)
{
  if (ntype) *ntype = 0;
  if (nharderror) *nharderror = -1;
  if (dmin) *dmin = 0.0f;
  if (!llr || !keff || !maxosd || !norder || !apmask || !message101 || !cw)
    {
      return;
    }

  std::array<float, 240> llr_values {};
  std::array<signed char, 240> apmask_values {};
  std::array<signed char, 101> message_values {};
  std::array<signed char, 240> codeword_values {};
  std::copy_n (llr, 240, llr_values.begin ());
  std::copy_n (apmask, 240, apmask_values.begin ());

  int local_ntype = 0;
  int local_nharderror = -1;
  float local_dmin = 0.0f;
  bool const have_codeword = decodium::fst4::decode240_101_native (
      llr_values, *keff, *maxosd, *norder, apmask_values,
      message_values, codeword_values, local_ntype, local_nharderror, local_dmin);

  if (have_codeword)
    {
      std::copy (message_values.begin (), message_values.end (), message101);
      std::copy (codeword_values.begin (), codeword_values.end (), cw);
      if (ntype) *ntype = local_ntype;
      if (nharderror) *nharderror = local_nharderror;
      if (dmin) *dmin = local_dmin;
    }
}

extern "C" void decode240_74_ (float llr[], int* keff, int* maxosd, int* norder,
                                signed char apmask[], signed char message74[],
                                signed char cw[], int* ntype, int* nharderror, float* dmin)
{
  if (ntype) *ntype = 0;
  if (nharderror) *nharderror = -1;
  if (dmin) *dmin = 0.0f;
  if (!llr || !keff || !maxosd || !norder || !apmask || !message74 || !cw)
    {
      return;
    }

  std::array<float, 240> llr_values {};
  std::array<signed char, 240> apmask_values {};
  std::array<signed char, 74> message_values {};
  std::array<signed char, 240> codeword_values {};
  std::copy_n (llr, 240, llr_values.begin ());
  std::copy_n (apmask, 240, apmask_values.begin ());

  int local_ntype = 0;
  int local_nharderror = -1;
  float local_dmin = 0.0f;
  bool const have_codeword = decodium::fst4::decode240_74_native (
      llr_values, *keff, *maxosd, *norder, apmask_values,
      message_values, codeword_values, local_ntype, local_nharderror, local_dmin);

  if (have_codeword)
    {
      std::copy (message_values.begin (), message_values.end (), message74);
      std::copy (codeword_values.begin (), codeword_values.end (), cw);
      if (ntype) *ntype = local_ntype;
      if (nharderror) *nharderror = local_nharderror;
      if (dmin) *dmin = local_dmin;
    }
}

extern "C" void osd240_101_ (float llr[], int* keff, signed char apmask[], int* ndeep,
                              signed char message101[], signed char cw[],
                              int* nhardmin, float* dmin)
{
  int maxosd = 0;
  int ntype = 0;
  decode240_101_ (llr, keff, &maxosd, ndeep, apmask, message101, cw, &ntype, nhardmin, dmin);
}

extern "C" void osd240_74_ (float llr[], int* keff, signed char apmask[], int* ndeep,
                             signed char message74[], signed char cw[],
                             int* nhardmin, float* dmin)
{
  int maxosd = 0;
  int ntype = 0;
  decode240_74_ (llr, keff, &maxosd, ndeep, apmask, message74, cw, &ntype, nhardmin, dmin);
}

extern "C" void fastosd240_74_ (float llr[], int* keff, signed char apmask[], int* ndeep,
                                 signed char message74[], signed char cw[],
                                 int* nhardmin, float* dmin)
{
  int maxosd = 0;
  int ntype = 0;
  decode240_74_ (llr, keff, &maxosd, ndeep, apmask, message74, cw, &ntype, nhardmin, dmin);
}
