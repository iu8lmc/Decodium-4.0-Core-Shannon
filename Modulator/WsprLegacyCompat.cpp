#include "LegacyJtEncoder.hpp"

#include <QByteArray>
#include <QString>

#include <algorithm>
#include <array>
#include <cstddef>

#include "wsjtx_config.h"

namespace
{

QByteArray fixed_from_fortran (char const* src, fortran_charlen_t len, int width)
{
  QByteArray result = src ? QByteArray (src, static_cast<int> (len)) : QByteArray {};
  if (result.size () > width)
    result.truncate (width);
  else if (result.size () < width)
    result.append (QByteArray (width - result.size (), ' '));
  return result;
}

void copy_fixed_to_fortran (QByteArray const& src, char* dst, fortran_charlen_t len)
{
  if (!dst)
    return;

  std::fill_n (dst, static_cast<std::size_t> (len), ' ');
  int const copy_len = std::min (static_cast<int> (len), static_cast<int> (src.size ()));
  if (copy_len > 0)
    std::copy_n (src.constData (), copy_len, dst);
}

}

extern "C" void wqencode_ (char* msg, int* ntype, signed char data0[],
                           fortran_charlen_t len)
{
  if (ntype) { *ntype = 0; }
  if (data0) { std::fill_n (data0, 11, 0); }
  if (!msg || !ntype || !data0)
    return;

  QByteArray const fixed = fixed_from_fortran (msg, len, 22);
  auto const encoded = decodium::legacy_jt::detail::encode_wspr_source (fixed);
  if (!encoded.ok)
    return;

  *ntype = encoded.ntype;
  std::copy_n (encoded.data.begin (), static_cast<int> (encoded.data.size ()), data0);
}

extern "C" void inter_wspr_ (signed char id[], int* ndir)
{
  if (!id || !ndir)
    return;

  static std::array<int, decodium::legacy_jt::detail::kWsprToneCount> const order =
      decodium::legacy_jt::detail::interleave_wspr_order ();

  std::array<signed char, decodium::legacy_jt::detail::kWsprToneCount> input {};
  std::array<signed char, decodium::legacy_jt::detail::kWsprToneCount> output {};
  std::copy_n (id, static_cast<int> (input.size ()), input.begin ());

  if (*ndir == 1)
    {
      for (int i = 0; i < static_cast<int> (input.size ()); ++i)
        output[static_cast<std::size_t> (order[static_cast<std::size_t> (i)])] =
            input[static_cast<std::size_t> (i)];
    }
  else
    {
      for (int i = 0; i < static_cast<int> (input.size ()); ++i)
        output[static_cast<std::size_t> (i)] =
            input[static_cast<std::size_t> (order[static_cast<std::size_t> (i)])];
    }

  std::copy (output.begin (), output.end (), id);
}

extern "C" void inter_mept_ (signed char id[], int* ndir)
{
  inter_wspr_ (id, ndir);
}

extern "C" void genwspr_ (char* msg, char* msgsent, int itone[],
                          fortran_charlen_t len1, fortran_charlen_t len2)
{
  if (msgsent)
    std::fill_n (msgsent, static_cast<std::size_t> (len2), ' ');
  if (itone)
    std::fill_n (itone, decodium::legacy_jt::detail::kWsprToneCount, 0);
  if (!msg || !msgsent || !itone)
    return;

  QByteArray const fixed = fixed_from_fortran (msg, len1, 22);
  auto const encoded = decodium::legacy_jt::encodeWspr (QString::fromLatin1 (fixed.constData (), fixed.size ()));
  if (!encoded.ok || encoded.tones.size () != decodium::legacy_jt::detail::kWsprToneCount)
    return;

  copy_fixed_to_fortran (fixed, msgsent, len2);
  std::copy_n (encoded.tones.constData (), encoded.tones.size (), itone);
}
