#include "Detector/JT65Decoder.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"
#include "LegacyDspIoHelpers.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
constexpr int kNzMax = 60 * 12000;
constexpr int kJt65Nsz = 3413;
constexpr int kJt65MaxQsym = 552;
constexpr int kJt65DecodeRows = 512;
constexpr int kJt65DecodeCols = 126;
constexpr int kJt65S3Rows = 64;
constexpr int kJt65S3Cols = 63;
constexpr int kMaxDecodes = 50;
constexpr int kMaxAvgLines = 64;
constexpr int kDecodeBlankWidth = 22;
constexpr int kAvgLineWidth = 80;

struct DecodeRecord
{
  float sync {0.0f};
  int snr {0};
  float dt {0.0f};
  int freq {0};
  int drift {0};
  int nflip {0};
  float width {0.0f};
  QByteArray decoded {kDecodeBlankWidth, ' '};
  int ft {0};
  int qual {0};
  int nsmo {0};
  int nsum {0};
};

struct AvgResult
{
  int nft {0};
  QByteArray message {kDecodeBlankWidth, ' '};
  float qual {0.0f};
  int nsum {0};
  int nsmo {0};
  std::vector<std::string> lines;
};

struct AverageStateView
{
  int& avg_last_utc;
  int& avg_last_freq;
  int& avg_ring_index;
  int& clear_avg65;
  int* avg_iutc;
  int* avg_nfsave;
  int* avg_nflipsave;
  float* avg_s1save;
  float* avg_s3save;
  float* avg_dtsave;
  float* avg_syncsave;

  std::size_t s1_slot_offset (int slot_1based) const
  {
    return static_cast<std::size_t> (slot_1based - 1)
           * static_cast<std::size_t> (kJt65DecodeRows * kJt65DecodeCols);
  }

  std::size_t s3_slot_offset (int slot_1based) const
  {
    return static_cast<std::size_t> (slot_1based - 1)
           * static_cast<std::size_t> (kJt65S3Rows * kJt65S3Cols);
  }
};

QByteArray fixed_field (QByteArray value, int width)
{
  value = value.left (width);
  if (value.size () < width)
    {
      value.append (QByteArray (width - value.size (), ' '));
    }
  return value;
}

QByteArray blank_message ()
{
  return QByteArray (kDecodeBlankWidth, ' ');
}

double db_value (double x)
{
  if (x <= 1.259e-10)
    {
      return -99.0;
    }
  return 10.0 * std::log10 (x);
}

std::size_t s2_index (int row_fortran, int column_fortran)
{
  return static_cast<std::size_t> (row_fortran - 1)
         + static_cast<std::size_t> (66) * static_cast<std::size_t> (column_fortran - 1);
}

std::size_t s3_index (int row_fortran, int column_fortran)
{
  return static_cast<std::size_t> (row_fortran - 1)
         + static_cast<std::size_t> (kJt65S3Rows)
               * static_cast<std::size_t> (column_fortran - 1);
}

void copy_fixed_bytes (char* out, int width, QByteArray const& value)
{
  std::memset (out, ' ', static_cast<std::size_t> (width));
  auto const bytes =
      std::min<std::size_t> (static_cast<std::size_t> (width),
                             static_cast<std::size_t> (value.size ()));
  if (bytes > 0)
    {
      std::memcpy (out, value.constData (), bytes);
    }
}

void copy_fixed_bytes (char* out, int width, std::string const& value)
{
  std::memset (out, ' ', static_cast<std::size_t> (width));
  auto const bytes =
      std::min<std::size_t> (static_cast<std::size_t> (width), value.size ());
  if (bytes > 0)
    {
      std::memcpy (out, value.data (), bytes);
    }
}

QByteArray fixed_from_chars (char const* data, int width)
{
  return fixed_field (QByteArray (data, width), width);
}

std::string format_avg_line (char used, int utc, float sync, float dt, int freq, char csync)
{
  char buffer[kAvgLineWidth + 1] {};
  std::snprintf (buffer, sizeof buffer, "%c%5.4d%6.1f%6.2f%6d %c", used, utc, sync, dt - 1.0f,
                 freq, csync);
  return std::string {buffer};
}

AvgResult avg65_compute (AverageStateView state, int nutc, int nsave, float snrsync, float dtxx,
                         int nflip, int nfreq, int mode65, int ntol, int ntrials,
                         int naggressive, int ndepth, QByteArray const& mycall,
                         QByteArray const& hiscall, QByteArray const& hisgrid,
                         int nQSOProgress, bool ljt65apon)
{
  AvgResult result;
  result.message = blank_message ();
  auto const shared = decodium::legacy::jt65_shared_state ();

  std::vector<float> s1b (static_cast<std::size_t> (kJt65DecodeRows * kJt65DecodeCols), 0.0f);
  std::vector<float> s2 (static_cast<std::size_t> (66 * 126), 0.0f);
  std::vector<float> s3b (static_cast<std::size_t> (kJt65S3Rows * kJt65S3Cols), 0.0f);
  std::vector<float> s3c (static_cast<std::size_t> (kJt65S3Rows * kJt65S3Cols), 0.0f);
  std::array<char, 64> cused {};
  cused.fill ('.');
  constexpr float dtdiff = 0.2f;

  if (state.clear_avg65 != 0)
    {
      std::fill_n (state.avg_iutc, 64, -1);
      std::fill_n (state.avg_nfsave, 64, 0);
      std::fill_n (state.avg_nflipsave, 64, 0);
      std::fill_n (state.avg_dtsave, 64, 0.0f);
      std::fill_n (state.avg_syncsave, 64, 0.0f);
      std::fill_n (state.avg_s3save, static_cast<std::size_t> (kJt65S3Rows * kJt65S3Cols * 64), 0.0f);
      std::fill_n (state.avg_s1save,
                   static_cast<std::size_t> (kJt65DecodeRows * kJt65DecodeCols * 64), 0.0f);
      state.clear_avg65 = 0;
    }

  bool found_existing = false;
  for (int i = 0; i < 64; ++i)
    {
      if (state.avg_iutc[i] < 0)
        {
          break;
        }
      if (nutc == state.avg_iutc[i] && std::abs (nfreq - state.avg_nfsave[i]) <= ntol)
        {
          found_existing = true;
          break;
        }
    }

  if (!found_existing && nsave >= 1 && nsave <= 64)
    {
      int const slot = nsave - 1;
      state.avg_iutc[slot] = nutc;
      state.avg_syncsave[slot] = snrsync;
      state.avg_dtsave[slot] = dtxx;
      state.avg_nfsave[slot] = nfreq;
      state.avg_nflipsave[slot] = nflip;
      decodium::legacy::jt65_load_last_s1 (state.avg_s1save + state.s1_slot_offset (nsave));
      std::copy_n (shared.s3a, static_cast<std::size_t> (kJt65S3Rows * kJt65S3Cols),
                   state.avg_s3save + state.s3_slot_offset (nsave));
    }

  for (int i = 0; i < 64; ++i)
    {
      if (state.avg_iutc[i] < 0)
        {
          break;
        }
      if ((state.avg_iutc[i] & 1) != (nutc & 1))
        {
          continue;
        }
      if (std::fabs (dtxx - state.avg_dtsave[i]) > dtdiff)
        {
          continue;
        }
      if (std::abs (nfreq - state.avg_nfsave[i]) > ntol)
        {
          continue;
        }
      if (state.avg_nflipsave[i] == 0 || nflip != state.avg_nflipsave[i])
        {
          continue;
        }

      auto const s1_offset = state.s1_slot_offset (i + 1);
      auto const s3_offset = state.s3_slot_offset (i + 1);
      for (std::size_t idx = 0; idx < s1b.size (); ++idx)
        {
          s1b[idx] += state.avg_s1save[s1_offset + idx];
        }
      for (std::size_t idx = 0; idx < s3b.size (); ++idx)
        {
          s3b[idx] += state.avg_s3save[s3_offset + idx];
        }
      cused[static_cast<std::size_t> (i)] = '$';
      ++result.nsum;
    }

  for (int i = 1; i <= std::max (0, std::min (nsave, 64)); ++i)
    {
      char csync = ' ';
      if (state.avg_nflipsave[i - 1] < 0)
        {
          csync = '#';
        }
      else if (state.avg_nflipsave[i - 1] > 0)
        {
          csync = '*';
        }
      result.lines.push_back (format_avg_line (
          cused[static_cast<std::size_t> (i - 1)], state.avg_iutc[i - 1], state.avg_syncsave[i - 1],
          state.avg_dtsave[i - 1], state.avg_nfsave[i - 1], csync));
    }

  if (result.nsum < 2)
    {
      return result;
    }

  float const df = 1378.125f / 512.0f;
  float qualbest = 0.0f;
  int nfttbest = 0;
  int nsmobest = 0;
  QByteArray deepbest = blank_message ();
  int minsmo = 0;
  int maxsmo = 0;
  if (mode65 >= 2)
    {
      minsmo = static_cast<int> (std::lround (shared.width / df));
      maxsmo = 2 * minsmo;
    }
  int nn = 0;

  for (int ismo = minsmo; ismo <= maxsmo; ++ismo)
    {
      if (ismo > 0)
        {
          int nz = kJt65DecodeRows;
          for (int j = 1; j <= 126; ++j)
            {
              float* column =
                  s1b.data () + static_cast<std::ptrdiff_t> ((j - 1) * kJt65DecodeRows);
              decodium::legacy::smooth121_inplace (column, nz);
              if (j == 1)
                {
                  ++nn;
                }
              if (nn >= 4)
                {
                  decodium::legacy::smooth121_inplace (column, nz);
                  if (j == 1)
                    {
                      ++nn;
                    }
                }
            }
        }

      for (int row = 1; row <= 66; ++row)
        {
          int jj = row;
          if (mode65 == 2)
            {
              jj = 2 * row - 1;
            }
          else if (mode65 == 4)
            {
              float const ff = 4.0f * (row - 1) * df - 355.297852f;
              jj = static_cast<int> (std::lround (ff / df)) + 1;
            }

          std::size_t const source_row =
              (jj >= -255 && jj <= 256) ? static_cast<std::size_t> (jj + 255) : 0U;
          for (int col = 1; col <= 126; ++col)
            {
              float value = 0.0f;
              if (jj >= -255 && jj <= 256)
                {
                  value = s1b[source_row
                              + static_cast<std::size_t> (kJt65DecodeRows * (col - 1))];
                }
              s2[s2_index (row, col)] = value;
            }
        }

      for (int j = 1; j <= 63; ++j)
        {
          int k = shared.mdat[static_cast<std::size_t> (j - 1)];
          if (nflip < 0)
            {
              k = shared.mdat2[static_cast<std::size_t> (j - 1)];
            }
          for (int i = 1; i <= 64; ++i)
            {
              s3c[s3_index (i, j)] = 4.0e-5f * s2[s2_index (i + 2, k)];
            }
        }

      int const nadd = result.nsum * ismo;
      auto const extract = decodium::legacy::extract_compute (
          s3c.data (), nadd, mode65, ntrials, naggressive, ndepth, nflip, mycall, hiscall,
          hisgrid, nQSOProgress, ljt65apon);
      int const nftt = extract.nft;
      float const qual = extract.qual;
      QByteArray const avemsg = fixed_field (extract.decoded, kDecodeBlankWidth);
      if (nftt == 1)
        {
          result.nft = nftt;
          result.message = avemsg;
          result.qual = qual;
          result.nsmo = ismo;
          decodium::legacy::jt65_set_decode_smoothing (result.nsmo);
          return result;
        }
      else if (nftt >= 2 && qual > qualbest)
        {
          deepbest = avemsg;
          qualbest = qual;
          nsmobest = ismo;
          nfttbest = nftt;
        }
    }

  if (nfttbest == 2)
    {
      result.nft = nfttbest;
      result.message = deepbest;
      result.qual = qualbest;
      result.nsmo = nsmobest;
      decodium::legacy::jt65_set_decode_smoothing (result.nsmo);
    }

  return result;
}

int clamp_snr (float sync1, float sync2, float width, bool vhf, int nspecial)
{
  float s2db = 0.0f;
  if (vhf)
    {
      float const xtmp = std::pow (10.0f, (sync1 + 16.0f) / 10.0f);
      s2db = 1.1f * static_cast<float> (db_value (xtmp))
             + 1.4f * static_cast<float> (db_value (width)) - 52.0f;
      if (nspecial > 0)
        {
          s2db = sync2;
        }
    }
  else
    {
      s2db = 10.0f * std::log10 (std::max (sync2, 1.0e-30f)) - 35.0f;
    }
  int nsnr = static_cast<int> (std::lround (s2db));
  nsnr = std::max (-30, std::min (-1, nsnr));
  return nsnr;
}

void append_decode (std::vector<DecodeRecord>& records, float sync1, int nsnr, float dtx,
                    int nfreq, int ndrift, int nflip, float width, QByteArray const& decoded,
                    int ft, int qual, int nsmo, int nsum)
{
  if (static_cast<int> (records.size ()) >= kMaxDecodes)
    {
      return;
    }
  DecodeRecord record;
  record.sync = sync1;
  record.snr = nsnr;
  record.dt = dtx - 1.0f;
  record.freq = nfreq;
  record.drift = ndrift;
  record.nflip = nflip;
  record.width = width;
  record.decoded = fixed_field (decoded, kDecodeBlankWidth);
  record.ft = ft;
  record.qual = qual;
  record.nsmo = nsmo;
  record.nsum = nsum;
  records.push_back (record);
}

QString format_async_decode_line (int nutc, int minsync, bool bVHF, DecodeRecord const& record)
{
  QByteArray decoded = record.decoded;
  QByteArray cflags {"   ", 3};
  bool const is_deep = record.ft == 2;
  bool const is_average = record.nsum >= 2;

  if (bVHF && record.ft > 0)
    {
      cflags[0] = 'f';
      if (is_deep)
        {
          cflags[0] = 'd';
          cflags[1] = record.qual >= 10 ? '*' : static_cast<char> ('0' + std::min (record.qual, 9));
          if (record.qual < 3 && !decoded.isEmpty ())
            {
              decoded[decoded.size () - 1] = '?';
            }
        }
      if (is_average)
        {
          cflags[2] = record.nsum >= 10 ? '*' : static_cast<char> ('0' + std::min (record.nsum, 9));
        }
      int const nap = record.ft >> 2;
      if (nap != 0)
        {
          cflags[0] = 'a';
          cflags[1] = static_cast<char> ('0' + std::min (nap, 9));
          cflags[2] = is_average
              ? (record.nsum >= 10 ? '*' : static_cast<char> ('0' + std::min (record.nsum, 9)))
              : ' ';
        }
    }

  QByteArray csync {"# ", 2};
  if (bVHF && record.nflip != 0 && record.sync >= std::max (0.0f, static_cast<float> (minsync)))
    {
      csync = "#*";
      if (record.nflip == -1)
        {
          csync = "##";
          if (decoded.trimmed () != QByteArray {})
            {
              int i = decoded.size () - 1;
              while (i >= 0 && decoded[i] == ' ')
                {
                  --i;
                }
              i = std::min (i, 17);
              if (i >= 0 && i + 4 < decoded.size ())
                {
                  decoded[i + 2] = 'O';
                  decoded[i + 3] = 'O';
                  decoded[i + 4] = 'O';
                }
            }
        }
    }

  int const trimmed_len = decoded.trimmed ().size ();
  if (trimmed_len == 2 || trimmed_len == 3)
    {
      csync = "# ";
    }
  if (cflags[0] == 'f')
    {
      cflags[1] = cflags[2];
      cflags[2] = ' ';
    }

  return QString::asprintf ("%04d%4d%5.1f%5d %2.2s %22.22s %3.3s",
                            nutc, record.snr, record.dt, record.freq, csync.constData (),
                            decoded.constData (), cflags.constData ());
}

void write_avg_lines (QString const& tempDirPath, std::vector<std::string> const& lines)
{
  if (tempDirPath.isEmpty ())
    {
      return;
    }
  QFile file {QDir {tempDirPath}.filePath (QStringLiteral ("avemsg.txt"))};
  if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
      return;
    }
  QTextStream stream {&file};
  for (std::string const& line : lines)
    {
      stream << QString::fromLatin1 (line.data (), static_cast<int> (line.size ())) << '\n';
    }
}

void decode_batch_impl (float const* dd0, int npts, bool newdat, int nutc, int nf1, int nf2,
                        int nfqso, int ntol, int nsubmode, int minsync, bool nagain,
                        int n2pass, bool nrobust, int ntrials, int naggressive, int ndepth,
                        bool clearave, QByteArray const& mycall, QByteArray const& hiscall,
                        QByteArray const& hisgrid, int nexp_decode, int nQSOProgress,
                        bool ljt65apon, AverageStateView state,
                        std::vector<DecodeRecord>& out_records,
                        std::vector<std::string>& out_avg_lines)
{
  (void) newdat;
  (void) nrobust;
  std::vector<float> dd (static_cast<std::size_t> (kNzMax), 0.0f);
  if (dd0 && npts > 0)
    {
      std::copy_n (dd0, static_cast<std::size_t> (std::min (npts, kNzMax)), dd.begin ());
    }

  int ndecoded = 0;
  int ndecoded0 = 0;
  QByteArray decoded0;
  bool const single_decode = ((nexp_decode & 32) != 0) || nagain;
  bool const bVHF = (nexp_decode & 64) != 0;

  int nvec = 1000;
  int npass = 1;
  if (bVHF)
    {
      nvec = ntrials;
      npass = n2pass > 1 ? 2 : 1;
    }
  else
    {
      if (ndepth == 1)
        {
          npass = 2;
          nvec = 100;
        }
      else if (ndepth == 2)
        {
          npass = 2;
          nvec = 1000;
        }
      else
        {
          npass = 4;
          nvec = 1000;
        }
    }

  struct AcceptedDecode
  {
    float freq {0.0f};
    float dt {0.0f};
    float sync {0.0f};
    QByteArray decoded {blank_message ()};
  };
  std::array<AcceptedDecode, 50> dec {};

  for (int ipass = 1; ipass <= npass; ++ipass)
    {
      float thresh0 = 2.0f;
      int nsubtract = 1;
      int nrob = 0;
      if (ipass == 1)
        {
          thresh0 = 2.5f;
        }
      else if (ipass == 4)
        {
          nsubtract = 0;
          nrob = 1;
        }
      if (npass == 1)
        {
          nsubtract = 0;
          thresh0 = 2.0f;
        }

      decodium::legacy::Jt65SymspecResult const spec =
          decodium::legacy::symspec65_compute (dd.data (), npts);
      if (!spec.ok)
        {
          break;
        }
      decodium::legacy::jt65_store_symspec_state (spec);

      int nfa = nf1;
      int nfb = nf2;
      if (single_decode || (bVHF && ntol < 1000))
        {
          nfa = std::max (200, nfqso - ntol);
          nfb = std::min (4000, nfqso + ntol);
          thresh0 = 1.0f;
        }

      float width = decodium::legacy::jt65_mode_width ();
      float baseline = 0.0f;
      float amp = 0.0f;
      float const df = 12000.0f / 8192.0f;
      if (bVHF)
        {
          int const ia = std::max (1, static_cast<int> (std::lround ((nfa - 100) / df)));
          int const ib = std::min (kJt65Nsz, static_cast<int> (std::lround ((nfb + 100) / df)));
          int const nz = ib - ia + 1;
          if (nz < 50)
            {
              break;
            }
          double total = 0.0;
          for (int i = ia; i <= ib; ++i)
            {
              total += spec.savg[static_cast<std::size_t> (i - 1)];
            }
          if (std::isnan (total))
            {
              break;
            }
          std::array<float, 5> lorentz {};
          decodium::legacy::lorentzian_fit (
              spec.savg.data () + static_cast<std::size_t> (ia - 1), nz, &lorentz);
          baseline = lorentz[0];
          amp = lorentz[1];
          width = lorentz[3] * df;
          (void) baseline;
          (void) amp;
        }
      decodium::legacy::jt65_set_mode_width (width);

      decodium::legacy::jt65_set_sync_threshold (thresh0);
      auto candidates =
          decodium::legacy::sync65_compute (nfa, nfb, ntol, spec.nqsym, nrob != 0, bVHF, thresh0);
      int ncand = std::min<int> (static_cast<int> (candidates.size ()), 50 / ipass);
      candidates.resize (static_cast<std::size_t> (std::max (0, ncand)));

      int mode65 = 1 << nsubmode;
      int ntol_local = ntol;
      int nflip = 1;
      int nqd = 0;
      if (clearave)
        {
          state.avg_ring_index = 0;
          state.avg_last_utc = -999;
          state.avg_last_freq = -999;
          state.clear_avg65 = 1;
        }

      if (bVHF && static_cast<int> (candidates.size ()) < kJt65MaxQsym)
        {
          decodium::legacy::Jt65SyncCandidate shorthand;
          shorthand.sync = 5.0f;
          shorthand.dt = 2.5f;
          shorthand.freq = static_cast<float> (nfqso);
          shorthand.flip = 0.0f;
          candidates.push_back (shorthand);
        }

      QByteArray previous_decoded;
      float previous_freq = 0.0f;
      bool prtavg = false;

      for (auto const& candidate : candidates)
        {
          float const sync1 = candidate.sync;
          float dtx = candidate.dt;
          float freq = candidate.freq;
          nflip = 1;
          if (bVHF)
            {
              nflip = static_cast<int> (candidate.flip);
            }
          if (sync1 < static_cast<float> (minsync))
            {
              nflip = 0;
            }
          std::array<float, 5> a {};
          float sync2 = 0.0f;
          int nft = 0;
          int nspecial = 0;
          float qual = 0.0f;
          int nsmo = 0;
          QByteArray decoded = blank_message ();
          auto const decode = decodium::legacy::decode65a_compute (
              dd.data (), npts, 1, nqd, freq, nflip, mode65, nvec, naggressive, ndepth,
              ntol_local, mycall, hiscall, hisgrid, nQSOProgress, ljt65apon, bVHF, dtx);
          sync2 = decode.sync2;
          a = decode.a;
          dtx = decode.dt;
          nft = decode.nft;
          nspecial = decode.nspecial;
          qual = decode.qual;
          nsmo = decode.nsmo;
          decoded = fixed_field (decode.decoded, kDecodeBlankWidth);

          if (!bVHF)
            {
              if (std::fabs (a[0]) > 10.0f / ipass)
                {
                  continue;
                }
              int ibad = 0;
              if (std::fabs (a[0]) > 5.0f) ++ibad;
              if (std::fabs (a[1]) > 2.0f) ++ibad;
              if (std::fabs (dtx - 1.0f) > 2.5f) ++ibad;
              if (ibad >= 2)
                {
                  continue;
                }
            }

          if (nspecial == 0 && sync1 == 5.0f && dtx == 2.5f)
            {
              continue;
            }
          if (nspecial == 2)
            {
              decoded = fixed_field (QByteArray ("RO"), 22);
            }
          else if (nspecial == 3)
            {
              decoded = fixed_field (QByteArray ("RRR"), 22);
            }
          else if (nspecial == 4)
            {
              decoded = fixed_field (QByteArray ("73"), 22);
            }
          if (sync1 < static_cast<float> (minsync) && decoded == blank_message ())
            {
              nflip = 0;
            }
          auto const shared_state = decodium::legacy::jt65_shared_state ();
          int const nhard_min = shared_state.param[1];
          int const nrtt1000 = shared_state.param[4];
          int const ntotal_min = shared_state.param[5];
          nsmo = decodium::legacy::jt65_decode_smoothing ();
          int const nfreq = static_cast<int> (std::lround (freq + a[0]));
          int const ndrift = static_cast<int> (std::lround (2.0f * a[1]));
          int const nsnr = clamp_snr (sync1, sync2, width, bVHF, nspecial);

          int nftt = 0;
          if (nft != 1 && (ndepth & 16) == 16 && sync1 >= static_cast<float> (minsync) && !prtavg)
            {
              if (nutc != state.avg_last_utc || std::abs (nfreq - state.avg_last_freq) > ntol)
                {
                  state.avg_last_utc = nutc;
                  state.avg_last_freq = nfreq;
                  ++state.avg_ring_index;
                  state.avg_ring_index = (state.avg_ring_index - 1) % 64 + 1;
                  AvgResult const avg =
                      avg65_compute (state, nutc, state.avg_ring_index, sync1, dtx, nflip, nfreq,
                                     mode65, ntol, ntrials, naggressive, ndepth, mycall, hiscall,
                                     hisgrid, nQSOProgress, ljt65apon);
                  out_avg_lines.insert (out_avg_lines.end (), avg.lines.begin (), avg.lines.end ());
                  nsmo = decodium::legacy::jt65_decode_smoothing ();
                  if (avg.nft >= 1 && avg.nsum >= 2)
                    {
                      append_decode (out_records, sync1, nsnr, dtx, nfreq, ndrift, nflip, width,
                                     avg.message, avg.nft, static_cast<int> (avg.qual), nsmo,
                                     avg.nsum);
                      prtavg = true;
                    }
                  nftt = avg.nft;
                }
            }

          if (nftt != 0)
            {
              int const n = naggressive;
              float const rtt = 0.001f * nrtt1000;
              if (nft < 2 && minsync >= 0 && nspecial == 0 && !bVHF)
                {
                  static int const h0[12] {41, 42, 43, 43, 44, 45, 46, 47, 48, 48, 49, 49};
                  static int const d0[12] {71, 72, 73, 74, 76, 77, 78, 80, 81, 82, 83, 83};
                  static float const r0[12] {0.70f, 0.72f, 0.74f, 0.76f, 0.78f, 0.80f,
                                             0.82f, 0.84f, 0.86f, 0.88f, 0.90f, 0.90f};
                  if (nhard_min > 50 || nhard_min > h0[n] || ntotal_min > d0[n] || rtt > r0[n])
                    {
                      continue;
                    }
                }
            }

          if (decoded == previous_decoded && std::fabs (freq - previous_freq) < 3.0f
              && minsync >= 0)
            {
              continue;
            }

          if (decoded != blank_message () || bVHF)
            {
              if (nsubtract == 1)
                {
                  decodium::legacy::subtract65_inplace (dd.data (), npts, freq, dtx);
                }

              bool dupe = false;
              for (int i = 0; i < ndecoded; ++i)
                {
                  if (decoded == dec[static_cast<std::size_t> (i)].decoded)
                    {
                      dupe = true;
                      break;
                    }
                }
              if (!dupe && (sync1 >= static_cast<float> (minsync) || bVHF))
                {
                  if (ndecoded < 50)
                    {
                      ++ndecoded;
                      dec[static_cast<std::size_t> (ndecoded - 1)].freq = freq + a[0];
                      dec[static_cast<std::size_t> (ndecoded - 1)].dt = dtx;
                      dec[static_cast<std::size_t> (ndecoded - 1)].sync = sync2;
                      dec[static_cast<std::size_t> (ndecoded - 1)].decoded = decoded;
                    }
                  append_decode (out_records, sync1, nsnr, dtx, nfreq, ndrift, nflip, width,
                                 decoded, nft, std::min (static_cast<int> (qual), 9999), nsmo,
                                 1);
                }
              previous_decoded = decoded;
              previous_freq = freq;
              if (previous_decoded == blank_message ())
                {
                  previous_decoded = fixed_field (QByteArray ("*"), 22);
                }
              if (single_decode && ndecoded > 0)
                {
                  break;
                }
            }
        }

      if (single_decode && ndecoded > 0)
        {
          break;
        }
      if (ipass > 1 && ndecoded == ndecoded0)
        {
          break;
        }
      ndecoded0 = ndecoded;
    }
}

}  // namespace

extern "C" void jt65_decode_batch_c (
    float dd0[], int* npts, int* newdat, int* nutc, int* nf1, int* nf2, int* nfqso, int* ntol,
    int* nsubmode, int* minsync, int* nagain, int* n2pass, int* nrobust, int* ntrials,
    int* naggressive, int* ndepth, float* emedelay, int* clearave, char mycall[12],
    char hiscall[12], char hisgrid[6], int* nexp_decode, int* nQSOProgress, int* ljt65apon,
    int* avg_last_utc, int* avg_last_freq, int* avg_ring_index, int* clear_avg65,
    int avg_iutc[64], int avg_nfsave[64], int avg_nflipsave[64],
    float avg_s1save[kJt65DecodeRows][kJt65DecodeCols][64],
    float avg_s3save[kJt65S3Rows][kJt65S3Cols][64], float avg_dtsave[64], float avg_syncsave[64],
    float record_sync[kMaxDecodes], int record_snr[kMaxDecodes], float record_dt[kMaxDecodes],
    int record_freq[kMaxDecodes], int record_drift[kMaxDecodes], int record_nflip[kMaxDecodes],
    float record_width[kMaxDecodes], char record_decoded[kMaxDecodes][kDecodeBlankWidth],
    int record_ft[kMaxDecodes], int record_qual[kMaxDecodes], int record_nsmo[kMaxDecodes],
    int record_nsum[kMaxDecodes], int* nrecords, char avg_lines[kMaxAvgLines][kAvgLineWidth],
    int* navg_lines)
{
  if (!dd0 || !npts || !newdat || !nutc || !nf1 || !nf2 || !nfqso || !ntol || !nsubmode
      || !minsync || !nagain || !n2pass || !nrobust || !ntrials || !naggressive || !ndepth
      || !clearave || !mycall || !hiscall || !hisgrid || !nexp_decode || !nQSOProgress
      || !ljt65apon || !avg_last_utc || !avg_last_freq || !avg_ring_index || !clear_avg65
      || !avg_iutc || !avg_nfsave || !avg_nflipsave || !avg_s1save || !avg_s3save
      || !avg_dtsave || !avg_syncsave || !record_sync || !record_snr || !record_dt
      || !record_freq || !record_drift || !record_nflip || !record_width || !record_decoded
      || !record_ft || !record_qual || !record_nsmo || !record_nsum || !nrecords
      || !avg_lines || !navg_lines)
    {
      return;
    }
  (void) emedelay;

  std::fill_n (record_sync, kMaxDecodes, 0.0f);
  std::fill_n (record_snr, kMaxDecodes, 0);
  std::fill_n (record_dt, kMaxDecodes, 0.0f);
  std::fill_n (record_freq, kMaxDecodes, 0);
  std::fill_n (record_drift, kMaxDecodes, 0);
  std::fill_n (record_nflip, kMaxDecodes, 0);
  std::fill_n (record_width, kMaxDecodes, 0.0f);
  std::fill_n (record_ft, kMaxDecodes, 0);
  std::fill_n (record_qual, kMaxDecodes, 0);
  std::fill_n (record_nsmo, kMaxDecodes, 0);
  std::fill_n (record_nsum, kMaxDecodes, 0);
  std::memset (record_decoded, ' ', static_cast<std::size_t> (kDecodeBlankWidth * kMaxDecodes));
  std::memset (avg_lines, ' ', static_cast<std::size_t> (kAvgLineWidth * kMaxAvgLines));
  *nrecords = 0;
  *navg_lines = 0;

  AverageStateView state {*avg_last_utc,
                          *avg_last_freq,
                          *avg_ring_index,
                          *clear_avg65,
                          avg_iutc,
                          avg_nfsave,
                          avg_nflipsave,
                          &avg_s1save[0][0][0],
                          &avg_s3save[0][0][0],
                          avg_dtsave,
                          avg_syncsave};

  std::vector<DecodeRecord> records;
  std::vector<std::string> lines;
  decode_batch_impl (dd0, *npts, *newdat != 0, *nutc, *nf1, *nf2, *nfqso, *ntol, *nsubmode,
                     *minsync, *nagain != 0, *n2pass, *nrobust != 0, *ntrials, *naggressive,
                     *ndepth, *clearave != 0, fixed_from_chars (mycall, 12),
                     fixed_from_chars (hiscall, 12), fixed_from_chars (hisgrid, 6), *nexp_decode,
                     *nQSOProgress, *ljt65apon != 0, state, records, lines);

  *nrecords = std::min<int> (static_cast<int> (records.size ()), kMaxDecodes);
  for (int i = 0; i < *nrecords; ++i)
    {
      DecodeRecord const& record = records[static_cast<std::size_t> (i)];
      record_sync[i] = record.sync;
      record_snr[i] = record.snr;
      record_dt[i] = record.dt;
      record_freq[i] = record.freq;
      record_drift[i] = record.drift;
      record_nflip[i] = record.nflip;
      record_width[i] = record.width;
      record_ft[i] = record.ft;
      record_qual[i] = record.qual;
      record_nsmo[i] = record.nsmo;
      record_nsum[i] = record.nsum;
      copy_fixed_bytes (record_decoded[i], kDecodeBlankWidth, record.decoded);
    }

  *navg_lines = std::min<int> (static_cast<int> (lines.size ()), kMaxAvgLines);
  for (int i = 0; i < *navg_lines; ++i)
    {
      copy_fixed_bytes (avg_lines[i], kAvgLineWidth, lines[static_cast<std::size_t> (i)]);
    }
}

namespace decodium
{
namespace jt65
{

QStringList decode_async_jt65 (legacyjt::DecodeRequest const& request, AverageState* state)
{
  if (!state)
    {
      return {};
    }

  if (state->avg_iutc.size () != 64)
    {
      state->avg_iutc.assign (64, -1);
      state->avg_nfsave.assign (64, 0);
      state->avg_nflipsave.assign (64, 0);
      state->avg_s1save.assign (static_cast<std::size_t> (kJt65DecodeRows * kJt65DecodeCols * 64), 0.0f);
      state->avg_s3save.assign (static_cast<std::size_t> (kJt65S3Rows * kJt65S3Cols * 64), 0.0f);
      state->avg_dtsave.assign (64, 0.0f);
      state->avg_syncsave.assign (64, 0.0f);
      state->clear_avg65 = 1;
    }
  if (request.nclearave != 0)
    {
      state->clear_avg65 = 1;
    }

  std::vector<float> dd (52 * 12000, 0.0f);
  int const copyCount = std::min<int> (request.audio.size (), static_cast<int> (dd.size ()));
  if (request.newdat != 0 && copyCount > 0)
    {
      for (int i = 0; i < copyCount; ++i)
        {
          dd[static_cast<std::size_t> (i)] = static_cast<float> (request.audio.at (i));
        }
    }

  AverageStateView view {state->avg_last_utc,
                         state->avg_last_freq,
                         state->avg_ring_index,
                         state->clear_avg65,
                         state->avg_iutc.data (),
                         state->avg_nfsave.data (),
                         state->avg_nflipsave.data (),
                         state->avg_s1save.data (),
                         state->avg_s3save.data (),
                         state->avg_dtsave.data (),
                         state->avg_syncsave.data ()};

  std::vector<DecodeRecord> records;
  std::vector<std::string> avg_lines;
  decodium::legacy::set_jt65_data_dir_hint (request.dataDir);
  decode_batch_impl (
      dd.data (), static_cast<int> (dd.size ()), request.newdat != 0, request.nutc, request.nfa,
      request.nfb, request.nfqso, request.ntol, request.nsubmode, request.minsync,
      request.nagain != 0, request.n2pass, request.nrobust != 0, request.ntrials,
      request.naggressive, request.ndepth, request.nclearave != 0,
      fixed_field (request.mycall, 12), fixed_field (request.hiscall, 12),
      fixed_field (request.hisgrid, 6), request.nexp_decode, request.nqsoprogress,
      request.ljt65apon != 0, view, records, avg_lines);
  decodium::legacy::set_jt65_data_dir_hint ({});

  write_avg_lines (QString::fromLocal8Bit (request.tempDir), avg_lines);

  QStringList rows;
  rows.reserve (static_cast<int> (records.size ()));
  bool const bVHF = (request.nexp_decode & 64) != 0;
  for (DecodeRecord const& record : records)
    {
      rows << format_async_decode_line (request.nutc, request.minsync, bVHF, record);
    }
  return rows;
}

}
}
