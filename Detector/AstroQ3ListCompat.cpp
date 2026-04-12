#include "wsjtx_config.h"

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QTime>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C"
{
  void moondopjpl_(const int* year, const int* month, const int* day_of_month,
                   const float* ut_hours, const float* east_lon_deg, const float* lat_deg,
                   float* ra_rad, float* dec_rad, float* lst_hours, float* ha_deg,
                   float* az_deg, float* el_deg, float* vr_km_s, float* echo_delay_s);
}

namespace
{
  constexpr int kMaxCallers {40};
  constexpr int kEntrySize {22};
  constexpr int kRowSize {36};

  struct Q3ListEntry
  {
    QByteArray call;
    QByteArray grid;
    std::int32_t nsec {};
    std::int32_t nfreq {};
    std::int32_t moonel {};
  };

  struct DisplayEntry
  {
    Q3ListEntry entry;
    double age_hours {};
  };

  QString& q3list_file_name()
  {
    static QString path;
    return path;
  }

  QByteArray trim_fortran_field(char const* src, fortran_charlen_t len)
  {
    QByteArray field {src, static_cast<int> (len)};
    while (!field.isEmpty () && (field.back () == ' ' || field.back () == '\0'))
      {
        field.chop (1);
      }
    return field;
  }

  QByteArray fixed_field(QByteArray field, int width)
  {
    field = field.left (width);
    if (field.size () < width)
      {
        field.append (QByteArray (width - field.size (), ' '));
      }
    return field;
  }

  QByteArray fitted_text_field(char const* text, int width, bool right_aligned)
  {
    QByteArray field {text ? text : ""};
    if (field.size () > width)
      {
        field = right_aligned ? field.right (width) : field.left (width);
      }
    if (field.size () < width)
      {
        QByteArray const pad (width - field.size (), ' ');
        field = right_aligned ? pad + field : field + pad;
      }
    return field;
  }

  std::int32_t read_i32le(char const* src)
  {
    unsigned char const* p = reinterpret_cast<unsigned char const*> (src);
    return static_cast<std::int32_t> (p[0])
         | (static_cast<std::int32_t> (p[1]) << 8)
         | (static_cast<std::int32_t> (p[2]) << 16)
         | (static_cast<std::int32_t> (p[3]) << 24);
  }

  void write_i32le(char* dst, std::int32_t value)
  {
    unsigned char* p = reinterpret_cast<unsigned char*> (dst);
    p[0] = static_cast<unsigned char> (value & 0xff);
    p[1] = static_cast<unsigned char> ((value >> 8) & 0xff);
    p[2] = static_cast<unsigned char> ((value >> 16) & 0xff);
    p[3] = static_cast<unsigned char> ((value >> 24) & 0xff);
  }

  void grid_to_west_lon_lat(QByteArray grid0, float& west_lon_deg, float& lat_deg)
  {
    if (grid0.size () < 6)
      {
        grid0.append (QByteArray (6 - grid0.size (), ' '));
      }
    grid0 = grid0.left (6);

    int const c5 = static_cast<unsigned char> (grid0[4]);
    if (grid0[4] == ' ' || c5 <= 64 || c5 >= 128)
      {
        grid0[4] = 'm';
        grid0[5] = 'm';
      }

    if (grid0[0] >= 'a' && grid0[0] <= 'z') grid0[0] = static_cast<char> (grid0[0] - ('a' - 'A'));
    if (grid0[1] >= 'a' && grid0[1] <= 'z') grid0[1] = static_cast<char> (grid0[1] - ('a' - 'A'));
    if (grid0[4] >= 'A' && grid0[4] <= 'Z') grid0[4] = static_cast<char> (grid0[4] + ('a' - 'A'));
    if (grid0[5] >= 'A' && grid0[5] <= 'Z') grid0[5] = static_cast<char> (grid0[5] + ('a' - 'A'));

    int const nlong = 180 - 20 * (grid0[0] - 'A');
    int const n20d = 2 * (grid0[2] - '0');
    float const xminlong = 5.0f * ((grid0[4] - 'a') + 0.5f);
    west_lon_deg = static_cast<float> (nlong - n20d) - xminlong / 60.0f;

    int const nlat = -90 + 10 * (grid0[1] - 'A') + (grid0[3] - '0');
    float const xminlat = 2.5f * ((grid0[5] - 'a') + 0.5f);
    lat_deg = static_cast<float> (nlat) + xminlat / 60.0f;
  }

  bool read_q3list_file(QString const& path, std::vector<Q3ListEntry>& entries)
  {
    entries.clear ();
    QFile file {path};
    if (!file.exists ())
      {
        return true;
      }
    if (!file.open (QIODevice::ReadOnly))
      {
        return false;
      }

    QByteArray const data = file.readAll ();
    if (data.isEmpty ())
      {
        return true;
      }
    if (data.size () < 20)
      {
        return false;
      }

    char const* p = data.constData ();
    std::int32_t const header_len = read_i32le (p);
    std::int32_t const count = read_i32le (p + 4);
    std::int32_t const header_end_len = read_i32le (p + 8);
    if (header_len != 4 || header_end_len != 4 || count < 0 || count > kMaxCallers)
      {
        return false;
      }

    std::int32_t const body_len = read_i32le (p + 12);
    if (body_len != count * kEntrySize || data.size () != 20 + body_len)
      {
        return false;
      }
    if (read_i32le (p + 16 + body_len) != body_len)
      {
        return false;
      }

    entries.reserve (static_cast<std::size_t> (count));
    p += 16;
    for (int i = 0; i < count; ++i, p += kEntrySize)
      {
        Q3ListEntry entry;
        entry.call = QByteArray {p, 6}.trimmed ();
        entry.grid = QByteArray {p + 6, 4}.trimmed ();
        entry.nsec = read_i32le (p + 10);
        entry.nfreq = read_i32le (p + 14);
        entry.moonel = read_i32le (p + 18);
        entries.push_back (std::move (entry));
      }
    return true;
  }

  bool write_q3list_file(QString const& path, std::vector<Q3ListEntry> const& entries)
  {
    QFile file {path};
    if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate))
      {
        return false;
      }

    int const count = std::min (static_cast<int> (entries.size ()), kMaxCallers);
    std::int32_t const body_len = count * kEntrySize;
    QByteArray data (20 + body_len, 0);
    char* p = data.data ();
    write_i32le (p, 4);
    write_i32le (p + 4, count);
    write_i32le (p + 8, 4);
    write_i32le (p + 12, body_len);

    char* body = p + 16;
    for (int i = 0; i < count; ++i, body += kEntrySize)
      {
        QByteArray const call = fixed_field (entries[static_cast<std::size_t> (i)].call, 6);
        QByteArray const grid = fixed_field (entries[static_cast<std::size_t> (i)].grid, 4);
        std::memcpy (body, call.constData (), 6);
        std::memcpy (body + 6, grid.constData (), 4);
        write_i32le (body + 10, entries[static_cast<std::size_t> (i)].nsec);
        write_i32le (body + 14, entries[static_cast<std::size_t> (i)].nfreq);
        write_i32le (body + 18, entries[static_cast<std::size_t> (i)].moonel);
      }

    write_i32le (p + 16 + body_len, body_len);
    return file.write (data) == data.size ();
  }

  bool update_entry_moonel(Q3ListEntry& entry, QDateTime const& now_utc)
  {
    QByteArray grid6 = fixed_field (entry.grid, 4);
    grid6.append ("mm");

    float west_lon_deg {};
    float lat_deg {};
    grid_to_west_lon_lat (grid6, west_lon_deg, lat_deg);

    int const year = now_utc.date ().year ();
    int const month = now_utc.date ().month ();
    int const day = now_utc.date ().day ();
    float const uth = static_cast<float> (
        now_utc.time ().hour ()
        + now_utc.time ().minute () / 60.0
        + (now_utc.time ().second () + 0.001 * now_utc.time ().msec ()) / 3600.0);
    float const east_lon_deg = -west_lon_deg;

    float ra_rad {};
    float dec_rad {};
    float lst_hours {};
    float ha_deg {};
    float az_deg {};
    float el_deg {};
    float vr_km_s {};
    float echo_delay_s {};
    moondopjpl_(&year, &month, &day, &uth, &east_lon_deg, &lat_deg,
                &ra_rad, &dec_rad, &lst_hours, &ha_deg,
                &az_deg, &el_deg, &vr_km_s, &echo_delay_s);
    entry.moonel = static_cast<std::int32_t> (std::lround (el_deg));
    return el_deg >= -5.0f;
  }

  void write_display_row(char* dst, int index, Q3ListEntry const& entry, double age_hours)
  {
    QByteArray const call = fixed_field (entry.call, 6);
    QByteArray const grid = fixed_field (entry.grid, 4);
    char temp[64] {};
    QByteArray row (kRowSize - 1, ' ');

    std::snprintf (temp, sizeof (temp), "%d", index);
    row.replace (0, 2, fitted_text_field (temp, 2, true));
    row[2] = '.';

    std::snprintf (temp, sizeof (temp), "%d", static_cast<int> (entry.nfreq));
    row.replace (3, 6, fitted_text_field (temp, 6, true));

    row.replace (11, 6, call);
    row.replace (19, 4, grid);

    std::snprintf (temp, sizeof (temp), "%d", static_cast<int> (entry.moonel));
    row.replace (23, 5, fitted_text_field (temp, 5, true));

    std::snprintf (temp, sizeof (temp), "%.1f", age_hours);
    row.replace (28, 7, fitted_text_field (temp, 7, true));

    std::memset (dst, 0, kRowSize);
    std::memcpy (dst, row.constData (), static_cast<std::size_t> (row.size ()));
  }
}

extern "C"
{
  void get_q3list_(char* fname, bool* bDiskData, int* nlist, char* list,
                   fortran_charlen_t len1, fortran_charlen_t len2)
  {
    if (nlist)
      {
        *nlist = 0;
      }
    if (list && len2 > 0)
      {
        std::memset (list, 0, static_cast<std::size_t> (len2));
      }
    if (!fname || !nlist)
      {
        return;
      }

    QString const path = QString::fromLocal8Bit (trim_fortran_field (fname, len1));
    q3list_file_name () = path;

    std::vector<Q3ListEntry> entries;
    if (!read_q3list_file (path, entries))
      {
        write_q3list_file (path, {});
        return;
      }

    QDateTime const now_utc = QDateTime::currentDateTimeUtc ();
    qint64 const now_epoch = now_utc.toSecsSinceEpoch ();
    std::vector<DisplayEntry> filtered;
    filtered.reserve (entries.size ());

    for (Q3ListEntry& entry : entries)
      {
        double const age_hours = (now_epoch - static_cast<qint64> (entry.nsec)) / 3600.0;
        if (age_hours > 24.0)
          {
            continue;
          }

        bool const moon_visible = update_entry_moonel (entry, now_utc);
        if (!moon_visible && !(bDiskData && *bDiskData))
          {
            continue;
          }

        DisplayEntry item;
        item.entry = entry;
        item.age_hours = age_hours;
        filtered.push_back (item);
      }

    std::vector<Q3ListEntry> persisted;
    persisted.reserve (filtered.size ());
    for (DisplayEntry const& item : filtered)
      {
        persisted.push_back (item.entry);
      }
    write_q3list_file (path, persisted);

    std::stable_sort (filtered.begin (), filtered.end (),
                      [] (DisplayEntry const& lhs, DisplayEntry const& rhs)
                      {
                        return lhs.entry.nfreq < rhs.entry.nfreq;
                      });

    int const row_capacity = list ? std::max (0, static_cast<int> (len2) / kRowSize) : 0;
    int const count = std::min ({static_cast<int> (filtered.size ()), kMaxCallers, row_capacity});
    for (int i = 0; i < count; ++i)
      {
        write_display_row (list + i * kRowSize, i + 1, filtered[static_cast<std::size_t> (i)].entry,
                           filtered[static_cast<std::size_t> (i)].age_hours);
      }
    *nlist = count;
  }

  void rm_q3list_(char* callsign, fortran_charlen_t len)
  {
    QString const path = q3list_file_name ();
    if (path.isEmpty () || !callsign)
      {
        return;
      }

    std::vector<Q3ListEntry> entries;
    if (!read_q3list_file (path, entries))
      {
        return;
      }

    QByteArray const wanted = trim_fortran_field (callsign, len).trimmed ();
    entries.erase (std::remove_if (entries.begin (), entries.end (),
                                   [&wanted] (Q3ListEntry const& entry)
                                   {
                                     return entry.call.trimmed () == wanted;
                                   }),
                   entries.end ());
    write_q3list_file (path, entries);
  }
}
