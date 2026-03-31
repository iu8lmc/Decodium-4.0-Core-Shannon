#include "helper_functions.h"

#include <QByteArray>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr std::array<float, 22> kEltab {
    18.0f, 15.0f, 13.0f, 11.0f, 9.0f, 8.0f, 7.0f, 6.0f, 5.3f, 4.7f, 4.0f,
    3.3f, 2.7f, 2.0f, 1.5f, 1.0f, 0.8f, 0.6f, 0.4f, 0.2f, 0.0f, 0.0f};
constexpr std::array<float, 22> kDaztab {
    21.0f, 18.0f, 16.0f, 15.0f, 14.0f, 13.0f, 12.0f, 11.0f, 10.7f, 10.3f, 10.0f,
    10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 9.0f, 9.0f, 9.0f, 8.0f, 8.0f};

int nint_compat (double value)
{
  return static_cast<int> (std::lround (value));
}

std::array<char, 6> normalize_grid (QString const& grid)
{
  std::array<char, 6> normalized {' ', ' ', ' ', ' ', ' ', ' '};
  QByteArray const latin = grid.left (6).toLatin1 ();
  int const copy_count = std::min<int> (static_cast<int> (latin.size ()), 6);
  for (int i = 0; i < copy_count; ++i)
    {
      normalized[static_cast<size_t> (i)] = latin.at (i);
    }

  unsigned char const fifth = static_cast<unsigned char> (normalized[4]);
  if (normalized[4] == ' ' || fifth <= 64 || fifth >= 128)
    {
      normalized[4] = 'm';
      normalized[5] = 'm';
    }

  if (normalized[0] >= 'a' && normalized[0] <= 'z')
    {
      normalized[0] = static_cast<char> (normalized[0] - 'a' + 'A');
    }
  if (normalized[1] >= 'a' && normalized[1] <= 'z')
    {
      normalized[1] = static_cast<char> (normalized[1] - 'a' + 'A');
    }
  if (normalized[4] >= 'A' && normalized[4] <= 'Z')
    {
      normalized[4] = static_cast<char> (normalized[4] - 'A' + 'a');
    }
  if (normalized[5] >= 'A' && normalized[5] <= 'Z')
    {
      normalized[5] = static_cast<char> (normalized[5] - 'A' + 'a');
    }

  return normalized;
}

void grid_to_degrees (QString const& grid, float* dlong, float* dlat)
{
  auto const normalized = normalize_grid (grid);
  char const g1 = normalized[0];
  char const g2 = normalized[1];
  char const g3 = normalized[2];
  char const g4 = normalized[3];
  char const g5 = normalized[4];
  char const g6 = normalized[5];

  int const nlong = 180 - 20 * (g1 - 'A');
  int const n20d = 2 * (g3 - '0');
  float const xminlong = 5.0f * (static_cast<float> (g5 - 'a') + 0.5f);
  *dlong = static_cast<float> (nlong) - static_cast<float> (n20d) - xminlong / 60.0f;

  int const nlat = -90 + 10 * (g2 - 'A') + (g4 - '0');
  float const xminlat = 2.5f * (static_cast<float> (g6 - 'a') + 0.5f);
  *dlat = static_cast<float> (nlat) + xminlat / 60.0f;
}

void geodist_cpp (float eplat, float eplon, float stlat, float stlon,
                  float* az, float* baz, float* dist)
{
  if (std::abs (eplat - stlat) < 0.02 && std::abs (eplon - stlon) < 0.02)
    {
      *az = 0.0;
      *baz = 180.0;
      *dist = 0.0;
      return;
    }

  constexpr float al = 6378206.4f;              // Clarke 1866 ellipsoid
  constexpr float bl = 6356583.8f;
  constexpr float d2r = 0.01745329251994f;
  constexpr float pi2 = 6.28318530718f;

  float const boa = bl / al;
  float const f = 1.0f - boa;
  float const p1r = eplat * d2r;
  float const p2r = stlat * d2r;
  float const l1r = eplon * d2r;
  float const l2r = stlon * d2r;
  float const dlr = l2r - l1r;
  float const t1r = std::atan (boa * std::tan (p1r));
  float const t2r = std::atan (boa * std::tan (p2r));
  float const tm = (t1r + t2r) / 2.0f;
  float const dtm = (t2r - t1r) / 2.0f;
  float const stm = std::sin (tm);
  float const ctm = std::cos (tm);
  float const sdtm = std::sin (dtm);
  float const cdtm = std::cos (dtm);
  float const kl = stm * cdtm;
  float const kk = sdtm * ctm;
  float const sdlmr = std::sin (dlr / 2.0f);
  float const l = sdtm * sdtm + sdlmr * sdlmr * (cdtm * cdtm - stm * stm);
  float const cd = 1.0f - 2.0f * l;
  float const dl = std::acos (cd);
  float const sd = std::sin (dl);
  float const t = dl / sd;
  float const u = 2.0f * kl * kl / (1.0f - l);
  float const v = 2.0f * kk * kk / l;
  float const d = 4.0f * t * t;
  float const x = u + v;
  float const e = -2.0f * cd;
  float const y = u - v;
  float const a = -d * e;
  float const ff64 = f * f / 64.0f;
  *dist =
      al * sd
      * (t - (f / 4.0f) * (t * x - y)
         + ff64 * (x * (a + (t - (a + e) / 2.0f) * x) + y * (-2.0f * d + e * y) + d * x * y))
      / 1000.0f;
  float const tdlpm =
      std::tan ((dlr + (-((e * (4.0f - x) + 2.0f * y)
                           * ((f / 2.0f) * t + ff64 * (32.0f * t + (a - 20.0f * t) * x
                                                      - 2.0f * (d + 2.0f) * y))
                           / 4.0f)
                         * std::tan (dlr)))
                / 2.0f);
  float const hapbr = std::atan2 (sdtm, ctm * tdlpm);
  float const hambr = std::atan2 (cdtm, stm * tdlpm);
  float a1m2 = pi2 + hambr - hapbr;
  float a2m1 = pi2 - hambr - hapbr;

  while (a1m2 < 0.0 || a1m2 >= pi2)
    {
      if (a1m2 >= pi2)
        {
          a1m2 -= pi2;
        }
      else
        {
          a1m2 += pi2;
        }
    }

  while (a2m1 < 0.0 || a2m1 >= pi2)
    {
      if (a2m1 >= pi2)
        {
          a2m1 -= pi2;
        }
      else
        {
          a2m1 += pi2;
        }
    }

  *az = 360.0f - a1m2 / d2r;
  *baz = 360.0f - a2m1 / d2r;
}
}

double tx_duration(QString mode, double trPeriod, int nsps, bool bFast9)
{
  double txt=0.0;
  if(mode=="FT2")  txt=0.2 + 105*288/12000.0;      // FT2: +100 ms margin on top of waveform duration
  if(mode=="FT4")  txt=1.0 + 105*576/12000.0;      // FT4
  if(mode=="FT8")  txt=1.0 + 79*1920/12000.0;      // FT8
  if(mode=="JT4")  txt=1.0 + 207.0*2520/11025.0;   // JT4
  if(mode=="JT9")  txt=1.0 + 85.0*nsps/12000.0;  // JT9
  if(mode=="JT65") txt=1.0 + 126*4096/11025.0;     // JT65
  if(mode=="Q65")  {                                      // Q65
    if(trPeriod==15) txt=0.5 + 85*1800/12000.0;
    if(trPeriod==30) txt=0.5 + 85*3600/12000.0;
    if(trPeriod==60) txt=1.0 + 85*7200/12000.0;
    if(trPeriod==120) txt=1.0 + 85*16000/12000.0;
    if(trPeriod==300) txt=1.0 + 85*41472/12000.0;
  }
  if(mode=="WSPR") txt=2.0 + 162*8192/12000.0;     // WSPR
  if(mode=="FST4" or mode=="FST4W") {               //FST4, FST4W
    if(trPeriod==15)  txt=1.0 + 160*720/12000.0;
    if(trPeriod==30)  txt=1.0 + 160*1680/12000.0;
    if(trPeriod==60)  txt=1.0 + 160*3888/12000.0;
    if(trPeriod==120) txt=1.0 + 160*8200/12000.0;
    if(trPeriod==300) txt=1.0 + 160*21504/12000.0;
    if(trPeriod==900) txt=1.0 + 160*66560/12000.0;
    if(trPeriod==1800) txt=1.0 + 160*134400/12000.0;
  }
  if(mode=="MSK144" or bFast9) {
    txt=trPeriod-0.25; // JT9-fast, MSK144
  }
  if(mode=="Echo") txt=2.6;
  return txt;
}

GeoDistanceInfo geo_distance (QString const& myGrid, QString const& hisGrid, double utch)
{
  GeoDistanceInfo result;

  auto const my_grid0 = myGrid.leftJustified (6, ' ', true).left (6);
  auto const his_grid0 = hisGrid.leftJustified (6, ' ', true).left (6);
  if (my_grid0 == his_grid0)
    {
      result.hotABetter = true;
      return result;
    }

  QString my_grid = my_grid0;
  QString his_grid = his_grid0;
  if (my_grid.size () >= 6 && my_grid.at (4) == QChar {' '})
    {
      my_grid[4] = QChar {'m'};
      my_grid[5] = QChar {'m'};
    }
  if (his_grid.size () >= 6 && his_grid.at (4) == QChar {' '})
    {
      his_grid[4] = QChar {'m'};
      his_grid[5] = QChar {'m'};
    }

  if (my_grid == his_grid)
    {
      result.hotABetter = true;
      return result;
    }

  float dlong1 = 0.0f;
  float dlat1 = 0.0f;
  float dlong2 = 0.0f;
  float dlat2 = 0.0f;
  grid_to_degrees (my_grid, &dlong1, &dlat1);
  grid_to_degrees (his_grid, &dlong2, &dlat2);

  constexpr float eps = 1.0e-6f;
  float az = 0.0f;
  float baz = 0.0f;
  float dkm = 0.0f;
  float el = 0.0f;
  float hotA = 0.0f;
  float hotB = 0.0f;
  bool hotABetter = true;

  if (std::abs (dlat1 - dlat2) >= eps || std::abs (dlong1 - dlong2) >= eps)
    {
      float const difflong = std::fmod (dlong1 - dlong2 + 720.0f, 360.0f);
      if (std::abs (dlat1 + dlat2) < eps && std::abs (difflong - 180.0) < eps)
        {
          dkm = 20400.0f;
        }
      else
        {
          geodist_cpp (dlat1, dlong1, dlat2, dlong2, &az, &baz, &dkm);

          int const ndkm = static_cast<int> (dkm / 100.0);
          int j = ndkm - 4;
          j = std::max (1, std::min (21, j));
          float const u = (dkm - 100.0f * static_cast<float> (ndkm)) / 100.0f;
          if (dkm < 500.0f)
            {
              el = 18.0f;
            }
          else
            {
              el = (1.0f - u) * kEltab[static_cast<size_t> (j - 1)]
                   + u * kEltab[static_cast<size_t> (j)];
            }

          double const daz =
              kDaztab[static_cast<size_t> (j - 1)]
              + u * (kDaztab[static_cast<size_t> (j)] - kDaztab[static_cast<size_t> (j - 1)]);

          float const tmid = std::fmod (static_cast<float> (utch) - 0.5f * (dlong1 + dlong2) / 15.0f + 48.0f, 24.0f);
          bool iamEast = false;
          if (dlong1 < dlong2)
            {
              iamEast = true;
            }
          if (dlong1 == dlong2 && dlat1 > dlat2)
            {
              iamEast = false;
            }
          float azEast = baz;
          if (iamEast)
            {
              azEast = az;
            }
          if ((azEast >= 45.0f && azEast < 135.0f) || (azEast >= 225.0f && azEast < 315.0f))
            {
              hotABetter = true;
              if (std::abs (tmid - 6.0f) < 6.0f)
                {
                  hotABetter = false;
                }
              if ((dlat1 + dlat2) / 2.0f < 0.0f)
                {
                  hotABetter = !hotABetter;
                }
            }
          else
            {
              hotABetter = false;
              if (std::abs (tmid - 12.0f) < 6.0f)
                {
                  hotABetter = true;
                }
            }
          if (iamEast)
            {
              hotA = az - daz;
              hotB = az + daz;
            }
          else
            {
              hotA = az + daz;
              hotB = az - daz;
            }
          if (hotA < 0.0f)
            {
              hotA += 360.0f;
            }
          if (hotA > 360.0f)
            {
              hotA -= 360.0f;
            }
          if (hotB < 0.0f)
            {
              hotB += 360.0f;
            }
          if (hotB > 360.0f)
            {
              hotB -= 360.0f;
            }
        }
    }

  float const dmiles = dkm / 1.609344f;
  result.az = nint_compat (az);
  result.el = nint_compat (el);
  result.miles = nint_compat (dmiles);
  result.km = nint_compat (dkm);
  result.hotAz = nint_compat (hotABetter ? hotA : hotB);
  result.hotABetter = hotABetter;
  return result;
}
