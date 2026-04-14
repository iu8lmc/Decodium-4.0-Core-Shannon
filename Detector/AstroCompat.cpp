#include "wsjtx_config.h"

#include <QByteArray>
#include <QFile>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C"
{
  void tmoonsub_(double* day, double* glat, double* glong, double* moonalt,
                 double* mrv, double* l, double* b, double* paxis);
}

namespace
{
  constexpr double kPi {3.141592653589793238462643383279502884};
  constexpr double kTwoPi {2.0 * kPi};
  constexpr double kDegPerRad {57.29577951308232087679815481410517};
  constexpr double kRadPerDeg {1.0 / kDegPerRad};
  constexpr double kLightKmPerSec {2.99792458e5};
  constexpr double kDegreesToHours {1.0 / 15.0};

  std::array<int, 180> const kSkyTemp144 {{
    234, 246, 257, 267, 275, 280, 283, 286, 291, 298,
    305, 313, 322, 331, 341, 351, 361, 369, 376, 381,
    383, 382, 379, 374, 370, 366, 363, 361, 363, 368,
    376, 388, 401, 415, 428, 440, 453, 467, 487, 512,
    544, 579, 607, 618, 609, 588, 563, 539, 512, 482,
    450, 422, 398, 379, 363, 349, 334, 319, 302, 282,
    262, 242, 226, 213, 205, 200, 198, 197, 196, 197,
    200, 202, 204, 205, 204, 203, 202, 201, 203, 206,
    212, 218, 223, 227, 231, 236, 240, 243, 247, 257,
    276, 301, 324, 339, 346, 344, 339, 331, 323, 316,
    312, 310, 312, 317, 327, 341, 358, 375, 392, 407,
    422, 437, 451, 466, 480, 494, 511, 530, 552, 579,
    612, 653, 702, 768, 863, 1008, 1232, 1557, 1966, 2385,
    2719, 2924, 3018, 3038, 2986, 2836, 2570, 2213, 1823, 1461,
    1163, 939, 783, 677, 602, 543, 494, 452, 419, 392,
    373, 360, 353, 350, 350, 350, 350, 350, 350, 348,
    344, 337, 329, 319, 307, 295, 284, 276, 272, 272,
    273, 274, 274, 271, 266, 260, 252, 245, 238, 231
  }};

  struct SunResult
  {
    double ra_deg {};
    double dec_deg {};
    double lst_hours {};
    double az_deg {};
    double el_deg {};
    int mjd {};
    double day {};
  };

  struct MoonResult
  {
    double ra_deg {};
    double dec_deg {};
    double top_ra_deg {};
    double top_dec_deg {};
    double lst_hours {};
    double ha_deg {};
    double az_deg {};
    double el_deg {};
    double dist_km {};
    double vr_km_s {};
    double echo_delay_s {};
  };

  struct StationResult
  {
    double east_lon_deg {};
    double lat_deg {};
    double azsun_deg {};
    double elsun_deg {};
    double azmoon_deg {};
    double elmoon_deg {};
    double ramoon_deg {};
    double decmoon_deg {};
    double ha_deg {};
    double lst_hours {};
    double day {};
    double techo_s {};
    double doppler_hz {};
    double db_moon {};
    double sd {};
    double pol_angle_deg {};
    double xnr {};
    double dgrd {};
    int ntsky {};
    double xl {};
    double b {};
    double xl_next {};
    double b_next {};
  };

  struct AstroResult
  {
    double azsun {};
    double elsun {};
    double azmoon {};
    double elmoon {};
    double azmoon_dx {};
    double elmoon_dx {};
    double ramoon_hours {};
    double decmoon_deg {};
    double dgrd {};
    double poloffset {};
    double xnr {};
    double techo {};
    double width1 {};
    double width2 {};
    int ntsky {};
    int ndop {};
    int ndop00 {};
    double dfdt {};
    double dfdt0 {};
  };

  QByteArray trim_fortran_field(char const* src, fortran_charlen_t len)
  {
    QByteArray field {src, static_cast<int> (len)};
    while (!field.isEmpty () && (field.back () == ' ' || field.back () == '\0'))
      {
        field.chop (1);
      }
    return field;
  }

  QByteArray fixed_field(char const* src, fortran_charlen_t len, int width)
  {
    QByteArray field {src, std::min (static_cast<int> (len), width)};
    if (field.size () < width)
      {
        field.append (QByteArray (width - field.size (), ' '));
      }
    return field.left (width);
  }

  bool has_grid(QByteArray grid, int min_width)
  {
    if (grid.size () < min_width)
      {
        return false;
      }
    QByteArray const trimmed = grid.left (min_width).trimmed ();
    return trimmed.size () == min_width;
  }

  double wrap_positive(double value, double modulus)
  {
    double wrapped = std::fmod (value, modulus);
    if (wrapped < 0.0)
      {
        wrapped += modulus;
      }
    return wrapped;
  }

  void grid_to_west_lon_lat(QByteArray grid0, double& west_lon_deg, double& lat_deg)
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
    double const xminlong = 5.0 * ((grid0[4] - 'a') + 0.5);
    west_lon_deg = static_cast<double> (nlong - n20d) - xminlong / 60.0;

    int const nlat = -90 + 10 * (grid0[1] - 'A') + (grid0[3] - '0');
    double const xminlat = 2.5 * ((grid0[5] - 'a') + 0.5);
    lat_deg = static_cast<double> (nlat) + xminlat / 60.0;
  }

  void dcoord(double a0, double b0, double ap, double bp,
              double a1, double b1, double& a2, double& b2)
  {
    double const sb0 = std::sin (b0);
    double const cb0 = std::cos (b0);
    double const sbp = std::sin (bp);
    double const cbp = std::cos (bp);
    double const sb1 = std::sin (b1);
    double const cb1 = std::cos (b1);
    double const sb2 = sbp * sb1 + cbp * cb1 * std::cos (ap - a1);
    double const cb2 = std::sqrt (std::max (0.0, 1.0 - sb2 * sb2));
    b2 = std::atan2 (sb2, cb2);
    double const saa = std::sin (ap - a1) * cb1 / std::max (cb2, 1e-12);
    double const caa = (sb1 - sb2 * sbp) / std::max (cb2 * cbp, 1e-12);
    double const cbb = sb0 / std::max (cbp, 1e-12);
    double const sbb = std::sin (ap - a0) * cb0;
    double const sa2 = saa * cbb - caa * sbb;
    double const ca2 = caa * cbb + saa * sbb;
    double ta2o2 = 0.0;
    if (ca2 <= 0.0)
      {
        ta2o2 = (1.0 - ca2) / sa2;
      }
    else
      {
        ta2o2 = sa2 / (1.0 + ca2);
      }
    a2 = 2.0 * std::atan (ta2o2);
    if (a2 < 0.0)
      {
        a2 += kTwoPi;
      }
  }

  void geocentric(double alat_rad, double elev_m, double& hlt_rad, double& erad_km)
  {
    double const f = 1.0 / 298.257;
    double const a = 6378140.0;
    double const c = 1.0 / std::sqrt (1.0 + (-2.0 + f) * f * std::sin (alat_rad) * std::sin (alat_rad));
    double const arcf = (a * c + elev_m) * std::cos (alat_rad);
    double const arsf = (a * (1.0 - f) * (1.0 - f) * c + elev_m) * std::sin (alat_rad);
    hlt_rad = std::atan2 (arsf, arcf);
    erad_km = 0.001 * std::sqrt (arcf * arcf + arsf * arsf);
  }

  void toxyz(double alpha_rad, double delta_rad, double r, double vec[3])
  {
    vec[0] = r * std::cos (delta_rad) * std::cos (alpha_rad);
    vec[1] = r * std::cos (delta_rad) * std::sin (alpha_rad);
    vec[2] = r * std::sin (delta_rad);
  }

  void fromxyz(double const vec[3], double& alpha_rad, double& delta_rad, double& r)
  {
    r = std::sqrt (vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2]);
    alpha_rad = std::atan2 (vec[1], vec[0]);
    if (alpha_rad < 0.0)
      {
        alpha_rad += kTwoPi;
      }
    delta_rad = std::asin (vec[2] / r);
  }

  double dot3(double const a[3], double const b[3])
  {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  }

  SunResult compute_sun(int year, int month, int day_of_month, double ut_hours,
                        double east_lon_deg, double lat_deg)
  {
    SunResult result;
    double const d = 367.0 * year - 7.0 * (year + (month + 9) / 12) / 4.0
        + 275.0 * month / 9.0 + day_of_month - 730530.0 + ut_hours / 24.0;
    double const ecl = 23.4393 - 3.563e-7 * d;
    double const w = 282.9404 + 4.70935e-5 * d;
    double const e = 0.016709 - 1.151e-9 * d;
    double const mm = wrap_positive (356.0470 + 0.9856002585 * d + 360000.0, 360.0);
    double const ls = wrap_positive (w + mm + 720.0, 360.0);

    double ee = mm + e * kDegPerRad * std::sin (mm * kRadPerDeg) * (1.0 + e * std::cos (mm * kRadPerDeg));
    ee = ee - (ee - e * kDegPerRad * std::sin (ee * kRadPerDeg) - mm) / (1.0 - e * std::cos (ee * kRadPerDeg));

    double const xv = std::cos (ee * kRadPerDeg) - e;
    double const yv = std::sqrt (1.0 - e * e) * std::sin (ee * kRadPerDeg);
    double const v = kDegPerRad * std::atan2 (yv, xv);
    double const r = std::sqrt (xv * xv + yv * yv);
    double const lonsun = wrap_positive (v + w + 720.0, 360.0);
    double const xs = r * std::cos (lonsun * kRadPerDeg);
    double const ys = r * std::sin (lonsun * kRadPerDeg);
    double const xe = xs;
    double const ye = ys * std::cos (ecl * kRadPerDeg);
    double const ze = ys * std::sin (ecl * kRadPerDeg);

    result.ra_deg = kDegPerRad * std::atan2 (ye, xe);
    result.dec_deg = kDegPerRad * std::atan2 (ze, std::sqrt (xe * xe + ye * ye));

    double const gmst0 = (ls + 180.0) / 15.0;
    result.lst_hours = wrap_positive (gmst0 + ut_hours + east_lon_deg / 15.0 + 48.0, 24.0);
    double const ha_deg = 15.0 * result.lst_hours - result.ra_deg;
    double const xx = std::cos (ha_deg * kRadPerDeg) * std::cos (result.dec_deg * kRadPerDeg);
    double const yy = std::sin (ha_deg * kRadPerDeg) * std::cos (result.dec_deg * kRadPerDeg);
    double const zz = std::sin (result.dec_deg * kRadPerDeg);
    double const xhor = xx * std::sin (lat_deg * kRadPerDeg) - zz * std::cos (lat_deg * kRadPerDeg);
    double const yhor = yy;
    double const zhor = xx * std::cos (lat_deg * kRadPerDeg) + zz * std::sin (lat_deg * kRadPerDeg);

    result.az_deg = wrap_positive (kDegPerRad * std::atan2 (yhor, xhor) + 540.0, 360.0);
    result.el_deg = kDegPerRad * std::asin (zhor);
    result.mjd = static_cast<int> (d + 51543.0);
    result.day = d - 1.5;
    return result;
  }

  MoonResult compute_moon(int year, int month, int day_of_month, double ut_hours,
                          double east_lon_deg, double lat_deg)
  {
    MoonResult result;

    double const d = 367.0 * year - 7.0 * (year + (month + 9) / 12) / 4.0
        + 275.0 * month / 9.0 + day_of_month - 730530.0 + ut_hours / 24.0;
    double const ecl = 23.4393 - 3.563e-7 * d;

    double const nn = 125.1228 - 0.0529538083 * d;
    double const incl = 5.1454;
    double const w = wrap_positive (318.0634 + 0.1643573223 * d + 360000.0, 360.0);
    double const a = 60.2666;
    double const e = 0.054900;
    double const mm = wrap_positive (115.3654 + 13.0649929509 * d + 360000.0, 360.0);

    double ee = mm + e * kDegPerRad * std::sin (mm * kRadPerDeg) * (1.0 + e * std::cos (mm * kRadPerDeg));
    ee = ee - (ee - e * kDegPerRad * std::sin (ee * kRadPerDeg) - mm) / (1.0 - e * std::cos (ee * kRadPerDeg));
    ee = ee - (ee - e * kDegPerRad * std::sin (ee * kRadPerDeg) - mm) / (1.0 - e * std::cos (ee * kRadPerDeg));

    double const xv = a * (std::cos (ee * kRadPerDeg) - e);
    double const yv = a * std::sqrt (1.0 - e * e) * std::sin (ee * kRadPerDeg);
    double const v = wrap_positive (kDegPerRad * std::atan2 (yv, xv) + 720.0, 360.0);
    double r = std::sqrt (xv * xv + yv * yv);

    double const xg0 = r * (std::cos (nn * kRadPerDeg) * std::cos ((v + w) * kRadPerDeg)
                            - std::sin (nn * kRadPerDeg) * std::sin ((v + w) * kRadPerDeg) * std::cos (incl * kRadPerDeg));
    double const yg0 = r * (std::sin (nn * kRadPerDeg) * std::cos ((v + w) * kRadPerDeg)
                            + std::cos (nn * kRadPerDeg) * std::sin ((v + w) * kRadPerDeg) * std::cos (incl * kRadPerDeg));
    double const zg0 = r * std::sin ((v + w) * kRadPerDeg) * std::sin (incl * kRadPerDeg);

    double lonecl = wrap_positive (kDegPerRad * std::atan2 (yg0, xg0) + 720.0, 360.0);
    double latecl = kDegPerRad * std::atan2 (zg0, std::sqrt (xg0 * xg0 + yg0 * yg0));

    double const ms = wrap_positive (356.0470 + 0.9856002585 * d + 3600000.0, 360.0);
    double const ws = 282.9404 + 4.70935e-5 * d;
    double const ls = wrap_positive (ms + ws + 720.0, 360.0);
    double const lm = wrap_positive (mm + w + nn + 720.0, 360.0);
    double const dd = wrap_positive (lm - ls + 360.0, 360.0);
    double const ff = wrap_positive (lm - nn + 360.0, 360.0);

    lonecl = lonecl
        - 1.274 * std::sin ((mm - 2.0 * dd) * kRadPerDeg)
        + 0.658 * std::sin (2.0 * dd * kRadPerDeg)
        - 0.186 * std::sin (ms * kRadPerDeg)
        - 0.059 * std::sin ((2.0 * mm - 2.0 * dd) * kRadPerDeg)
        - 0.057 * std::sin ((mm - 2.0 * dd + ms) * kRadPerDeg)
        + 0.053 * std::sin ((mm + 2.0 * dd) * kRadPerDeg)
        + 0.046 * std::sin ((2.0 * dd - ms) * kRadPerDeg)
        + 0.041 * std::sin ((mm - ms) * kRadPerDeg)
        - 0.035 * std::sin (dd * kRadPerDeg)
        - 0.031 * std::sin ((mm + ms) * kRadPerDeg)
        - 0.015 * std::sin ((2.0 * ff - 2.0 * dd) * kRadPerDeg)
        + 0.011 * std::sin ((mm - 4.0 * dd) * kRadPerDeg);

    latecl = latecl
        - 0.173 * std::sin ((ff - 2.0 * dd) * kRadPerDeg)
        - 0.055 * std::sin ((mm - ff - 2.0 * dd) * kRadPerDeg)
        - 0.046 * std::sin ((mm + ff - 2.0 * dd) * kRadPerDeg)
        + 0.033 * std::sin ((ff + 2.0 * dd) * kRadPerDeg)
        + 0.017 * std::sin ((2.0 * mm + ff) * kRadPerDeg);

    r = 60.36298
        - 3.27746 * std::cos (mm * kRadPerDeg)
        - 0.57994 * std::cos ((mm - 2.0 * dd) * kRadPerDeg)
        - 0.46357 * std::cos (2.0 * dd * kRadPerDeg)
        - 0.08904 * std::cos (2.0 * mm * kRadPerDeg)
        + 0.03865 * std::cos ((2.0 * mm - 2.0 * dd) * kRadPerDeg)
        - 0.03237 * std::cos ((2.0 * dd - ms) * kRadPerDeg)
        - 0.02688 * std::cos ((mm + 2.0 * dd) * kRadPerDeg)
        - 0.02358 * std::cos ((mm - 2.0 * dd + ms) * kRadPerDeg)
        - 0.02030 * std::cos ((mm - ms) * kRadPerDeg)
        + 0.01719 * std::cos (dd * kRadPerDeg)
        + 0.01671 * std::cos ((mm + ms) * kRadPerDeg);

    result.dist_km = r * 6378.140;

    double const xg = r * std::cos (lonecl * kRadPerDeg) * std::cos (latecl * kRadPerDeg);
    double const yg = r * std::sin (lonecl * kRadPerDeg) * std::cos (latecl * kRadPerDeg);
    double const zg = r * std::sin (latecl * kRadPerDeg);
    double const xe = xg;
    double const ye = yg * std::cos (ecl * kRadPerDeg) - zg * std::sin (ecl * kRadPerDeg);
    double const ze = yg * std::sin (ecl * kRadPerDeg) + zg * std::cos (ecl * kRadPerDeg);

    result.ra_deg = wrap_positive (kDegPerRad * std::atan2 (ye, xe) + 360.0, 360.0);
    result.dec_deg = kDegPerRad * std::atan2 (ze, std::sqrt (xe * xe + ye * ye));

    double const mpar = kDegPerRad * std::asin (1.0 / r);
    double const gclat = lat_deg - 0.1924 * std::sin (2.0 * lat_deg * kRadPerDeg);
    double const rho = 0.99883 + 0.00167 * std::cos (2.0 * lat_deg * kRadPerDeg);
    double const gmst0 = (ls + 180.0) / 15.0;
    result.lst_hours = wrap_positive (gmst0 + ut_hours + east_lon_deg / 15.0 + 48.0, 24.0);
    double ha_deg = 15.0 * result.lst_hours - result.ra_deg;
    double const g = kDegPerRad * std::atan (std::tan (gclat * kRadPerDeg) / std::cos (ha_deg * kRadPerDeg));
    result.top_ra_deg = result.ra_deg - mpar * rho * std::cos (gclat * kRadPerDeg)
        * std::sin (ha_deg * kRadPerDeg) / std::cos (result.dec_deg * kRadPerDeg);
    result.top_dec_deg = result.dec_deg - mpar * rho * std::sin (gclat * kRadPerDeg)
        * std::sin ((g - result.dec_deg) * kRadPerDeg) / std::sin (g * kRadPerDeg);

    ha_deg = 15.0 * result.lst_hours - result.top_ra_deg;
    if (ha_deg > 180.0) ha_deg -= 360.0;
    if (ha_deg < -180.0) ha_deg += 360.0;
    result.ha_deg = ha_deg;

    double az_rad {};
    double el_rad {};
    dcoord(0.5 * kPi, 0.5 * kPi - lat_deg * kRadPerDeg, 0.0, lat_deg * kRadPerDeg,
           ha_deg * kRadPerDeg, result.top_dec_deg * kRadPerDeg, az_rad, el_rad);
    result.az_deg = az_rad * kDegPerRad;
    result.el_deg = el_rad * kDegPerRad;
    result.echo_delay_s = 2.0 * result.dist_km / kLightKmPerSec;
    return result;
  }

  MoonResult compute_moondop(int year, int month, int day_of_month, double ut_hours,
                             double east_lon_deg, double lat_deg)
  {
    MoonResult current = compute_moon(year, month, day_of_month, ut_hours, east_lon_deg, lat_deg);

    double const dt = 100.0;
    MoonResult previous = compute_moon(year, month, day_of_month, ut_hours - dt / 3600.0, east_lon_deg, lat_deg);

    double dlat1 {};
    double erad1 {};
    geocentric(lat_deg * kRadPerDeg, 200.0, dlat1, erad1);

    double rme0[3] {};
    double rme[3] {};
    toxyz(previous.ra_deg * kRadPerDeg, previous.dec_deg * kRadPerDeg, previous.dist_km, rme0);
    toxyz(current.ra_deg * kRadPerDeg, current.dec_deg * kRadPerDeg, current.dist_km, rme);

    double rae[6] {};
    double const phi = current.lst_hours * kTwoPi / 24.0;
    toxyz(phi, dlat1, erad1, rae);
    double const radps = kTwoPi / (86400.0 / 1.002737909);
    rae[3] = -rae[1] * radps;
    rae[4] =  rae[0] * radps;
    rae[5] = 0.0;

    double rma[6] {};
    double velocity[3] {};
    for (int i = 0; i < 3; ++i)
      {
        velocity[i] = (rme[i] - rme0[i]) / dt;
        rma[i] = rme[i] - rae[i];
        rma[i + 3] = velocity[i] - rae[i + 3];
      }

    double alpha1 {};
    double delta1 {};
    double dtopo0 {};
    fromxyz(rma, alpha1, delta1, dtopo0);
    double const vel[3] {rma[3], rma[4], rma[5]};
    double const pos[3] {rma[0], rma[1], rma[2]};
    current.vr_km_s = dot3(vel, pos) / dtopo0;
    current.echo_delay_s = 2.0 * current.dist_km / kLightKmPerSec;
    return current;
  }

  double qmap_doppler_frequency_hz(int nfreq)
  {
    if (nfreq == 2) return 1.8e6;
    if (nfreq == 4) return 3.5e6;
    return static_cast<double> (nfreq) * 1.0e6;
  }

  double station_polarization_angle(double lat_deg, double az_deg, double el_deg)
  {
    double const xx = std::sin (lat_deg * kRadPerDeg) * std::cos (el_deg * kRadPerDeg)
        - std::cos (lat_deg * kRadPerDeg) * std::cos (az_deg * kRadPerDeg) * std::sin (el_deg * kRadPerDeg);
    double const yy = std::cos (lat_deg * kRadPerDeg) * std::sin (az_deg * kRadPerDeg);
    return kDegPerRad * std::atan2 (yy, xx);
  }

  int sky_temperature(double top_ra_deg, double top_dec_deg, double sky_freq_hz)
  {
    double el_rad {};
    double eb_rad {};
    dcoord(0.0, 0.0, -1.570796, 1.161639, top_ra_deg * kRadPerDeg, top_dec_deg * kRadPerDeg,
           el_rad, eb_rad);
    int longecl_half = static_cast<int> (std::lround (kDegPerRad * el_rad / 2.0));
    longecl_half = std::max (1, std::min (180, longecl_half));
    double const t144 = static_cast<double> (kSkyTemp144[static_cast<size_t> (longecl_half - 1)]);
    double const tsky = (t144 - 2.7) * std::pow (144.0e6 / sky_freq_hz, 2.6) + 2.7;
    return static_cast<int> (std::lround (tsky));
  }

  bool compute_station(double doppler_freq_hz, double sky_freq_hz, int year, int month, int day_of_month,
                       double ut_hours, QByteArray const& grid, StationResult& result)
  {
    double west_lon_deg {};
    double lat_deg {};
    grid_to_west_lon_lat(grid, west_lon_deg, lat_deg);
    result.east_lon_deg = -west_lon_deg;
    result.lat_deg = lat_deg;

    SunResult const sun = compute_sun(year, month, day_of_month, ut_hours, result.east_lon_deg, lat_deg);
    MoonResult const moon = compute_moondop(year, month, day_of_month, ut_hours, result.east_lon_deg, lat_deg);

    result.azsun_deg = sun.az_deg;
    result.elsun_deg = sun.el_deg;
    result.azmoon_deg = moon.az_deg;
    result.elmoon_deg = moon.el_deg;
    result.ramoon_deg = moon.top_ra_deg;
    result.decmoon_deg = moon.top_dec_deg;
    result.ha_deg = moon.ha_deg;
    result.lst_hours = moon.lst_hours;
    result.day = sun.day;
    result.techo_s = moon.echo_delay_s;
    result.doppler_hz = -doppler_freq_hz * moon.vr_km_s / kLightKmPerSec;
    result.db_moon = -40.0 * std::log10 (moon.dist_km / 356903.0);
    result.sd = 16.23 * 370152.0 / moon.dist_km;
    result.pol_angle_deg = station_polarization_angle(lat_deg, moon.az_deg, moon.el_deg);
    result.ntsky = sky_temperature(moon.top_ra_deg, moon.top_dec_deg, sky_freq_hz);

    double const tr = 80.0;
    double const tsky = static_cast<double> (result.ntsky);
    double const tskymin = 13.0 * std::pow (408.0e6 / sky_freq_hz, 2.6);
    double const tsysmin = tskymin + tr;
    double const tsys = tsky + tr;
    result.dgrd = -10.0 * std::log10 (tsys / tsysmin) + result.db_moon;

    double tm_el {};
    double tm_rv {};
    double tm_paxis {};
    double day = result.day;
    double glat = lat_deg * kRadPerDeg;
    double glong = result.east_lon_deg * kRadPerDeg;
    tmoonsub_(&day, &glat, &glong, &tm_el, &tm_rv, &result.xl, &result.b, &tm_paxis);
    day = result.day + 1.0 / 1440.0;
    tmoonsub_(&day, &glat, &glong, &tm_el, &tm_rv, &result.xl_next, &result.b_next, &tm_paxis);

    return true;
  }

  AstroResult compute_wsjtx_astro(int year, int month, int day_of_month, double ut_hours, double freq_hz,
                                  QByteArray const& mygrid, QByteArray const& hisgrid)
  {
    AstroResult result;

    QByteArray const mygrid6 = fixed_field(mygrid.constData(), static_cast<fortran_charlen_t> (mygrid.size ()), 6);
    bool const dx_present = has_grid(hisgrid, 4);
    QByteArray const hisgrid6 = dx_present
        ? fixed_field(hisgrid.constData(), static_cast<fortran_charlen_t> (hisgrid.size ()), 6)
        : mygrid6;

    StationResult dx {};
    StationResult self {};
    compute_station(freq_hz, freq_hz, year, month, day_of_month, ut_hours, hisgrid6, dx);
    compute_station(freq_hz, freq_hz, year, month, day_of_month, ut_hours, mygrid6, self);

    double const dldt1 = kDegPerRad * (self.xl_next - self.xl);
    double const dbdt1 = kDegPerRad * (self.b_next - self.b);
    double const dldt2 = kDegPerRad * (dx.xl_next - dx.xl);
    double const dbdt2 = kDegPerRad * (dx.b_next - dx.b);
    double const fghz = 1.0e-9 * freq_hz;
    double const rate1 = 2.0 * std::sqrt (dldt1 * dldt1 + dbdt1 * dbdt1);
    double width1 = 0.5 * 6741.0 * fghz * rate1;
    double width2 = 0.5 * 6741.0 * fghz * std::sqrt ((dldt1 + dldt2) * (dldt1 + dldt2)
                                                     + (dbdt1 + dbdt2) * (dbdt1 + dbdt2));
    if (!dx_present)
      {
        width2 = 0.0;
      }

    double poloffset = 0.0;
    double xnr = 0.0;
    if (dx_present)
      {
        poloffset = std::fmod (dx.pol_angle_deg - self.pol_angle_deg + 720.0, 180.0);
        if (poloffset > 90.0)
          {
            poloffset -= 180.0;
          }
        double x1 = std::abs (std::cos (2.0 * poloffset * kRadPerDeg));
        if (x1 < 0.056234)
          {
            x1 = 0.056234;
          }
        xnr = -20.0 * std::log10 (x1);
        char const c0 = hisgrid6[0];
        if (c0 < 'A' || c0 > 'R')
          {
            xnr = 0.0;
          }
      }

    result.azsun = self.azsun_deg;
    result.elsun = self.elsun_deg;
    result.azmoon = self.azmoon_deg;
    result.elmoon = self.elmoon_deg;
    result.azmoon_dx = dx_present ? dx.azmoon_deg : 0.0;
    result.elmoon_dx = dx_present ? dx.elmoon_deg : 0.0;
    result.ramoon_hours = self.ramoon_deg * kDegreesToHours;
    result.decmoon_deg = self.decmoon_deg;
    result.dgrd = self.dgrd;
    result.poloffset = poloffset;
    result.xnr = xnr;
    result.techo = self.techo_s;
    result.width1 = width1;
    result.width2 = width2;
    result.ntsky = self.ntsky;
    double const doppler = dx_present ? (self.doppler_hz + dx.doppler_hz) : 0.0;
    result.ndop = static_cast<int> (std::lround (doppler));
    result.ndop00 = static_cast<int> (std::lround (2.0 * self.doppler_hz));

    static bool initialized {false};
    static double uth_prev {};
    static double doppler_prev {};
    static double doppler00_prev {};
    if (!initialized)
      {
        uth_prev = ut_hours - 1.0 / 3600.0;
        doppler_prev = doppler;
        doppler00_prev = 2.0 * self.doppler_hz;
        initialized = true;
      }
    double dt = 60.0 * (ut_hours - uth_prev);
    if (dt <= 0.0)
      {
        dt = 1.0 / 60.0;
      }
    result.dfdt = (doppler - doppler_prev) / dt;
    result.dfdt0 = ((2.0 * self.doppler_hz) - doppler00_prev) / dt;
    uth_prev = ut_hours;
    doppler_prev = doppler;
    doppler00_prev = 2.0 * self.doppler_hz;
    return result;
  }

  AstroResult compute_legacy_astro(int year, int month, int day_of_month, double ut_hours, int nfreq,
                                   QByteArray const& mygrid, QByteArray const& hisgrid)
  {
    AstroResult result;

    QByteArray const mygrid6 = fixed_field(mygrid.constData(), static_cast<fortran_charlen_t> (mygrid.size ()), 6);
    bool const dx_present = has_grid(hisgrid, 4);
    QByteArray const hisgrid6 = dx_present
        ? fixed_field(hisgrid.constData(), static_cast<fortran_charlen_t> (hisgrid.size ()), 6)
        : mygrid6;

    double const doppler_freq_hz = qmap_doppler_frequency_hz(nfreq);
    double const sky_freq_hz = static_cast<double> (nfreq) * 1.0e6;
    StationResult dx {};
    StationResult self {};
    compute_station(doppler_freq_hz, sky_freq_hz, year, month, day_of_month, ut_hours, hisgrid6, dx);
    compute_station(doppler_freq_hz, sky_freq_hz, year, month, day_of_month, ut_hours, mygrid6, self);

    double const dldt1 = kDegPerRad * (self.xl_next - self.xl);
    double const dbdt1 = kDegPerRad * (self.b_next - self.b);
    double const dldt2 = kDegPerRad * (dx.xl_next - dx.xl);
    double const dbdt2 = kDegPerRad * (dx.b_next - dx.b);
    double const fghz = 0.001 * static_cast<double> (nfreq);
    result.width1 = 0.5 * 6741.0 * fghz * (2.0 * std::sqrt (dldt1 * dldt1 + dbdt1 * dbdt1));
    result.width2 = 0.5 * 6741.0 * fghz * std::sqrt ((dldt1 + dldt2) * (dldt1 + dldt2)
                                                     + (dbdt1 + dbdt2) * (dbdt1 + dbdt2));

    double poloffset = 0.0;
    double xnr = 0.0;
    if (dx_present)
      {
        poloffset = std::fmod (dx.pol_angle_deg - self.pol_angle_deg + 720.0, 180.0);
        if (poloffset > 90.0)
          {
            poloffset -= 180.0;
          }
        double x1 = std::abs (std::cos (2.0 * poloffset * kRadPerDeg));
        if (x1 < 0.056234)
          {
            x1 = 0.056234;
          }
        xnr = -20.0 * std::log10 (x1);
        char const c0 = hisgrid6[0];
        if (c0 < 'A' || c0 > 'R')
          {
            xnr = 0.0;
          }
      }

    result.azsun = self.azsun_deg;
    result.elsun = self.elsun_deg;
    result.azmoon = self.azmoon_deg;
    result.elmoon = self.elmoon_deg;
    result.azmoon_dx = dx.azmoon_deg;
    result.elmoon_dx = dx.elmoon_deg;
    result.ramoon_hours = self.ramoon_deg * kDegreesToHours;
    result.decmoon_deg = self.decmoon_deg;
    result.dgrd = self.dgrd;
    result.poloffset = poloffset;
    result.xnr = xnr;
    result.techo = self.techo_s;
    result.ntsky = self.ntsky;
    double const doppler = self.doppler_hz + dx.doppler_hz;
    result.ndop = static_cast<int> (std::lround (doppler));
    result.ndop00 = static_cast<int> (std::lround (2.0 * self.doppler_hz));

    static bool initialized {false};
    static double uth_prev {};
    static double doppler_prev {};
    static double doppler00_prev {};
    if (!initialized)
      {
        uth_prev = ut_hours - 1.0 / 3600.0;
        doppler_prev = doppler;
        doppler00_prev = 2.0 * self.doppler_hz;
        initialized = true;
      }
    double dt = 60.0 * (ut_hours - uth_prev);
    if (dt <= 0.0)
      {
        dt = 1.0 / 60.0;
      }
    result.dfdt = (doppler - doppler_prev) / dt;
    result.dfdt0 = ((2.0 * self.doppler_hz) - doppler00_prev) / dt;
    uth_prev = ut_hours;
    doppler_prev = doppler;
    doppler00_prev = 2.0 * self.doppler_hz;
    return result;
  }

  int extra_azel_lines()
  {
    static int value = [] {
      char const* env = std::getenv ("WSJT_AZEL_EXTRA_LINES");
      if (!env || !*env)
        {
          return 0;
        }
      return std::atoi (env);
    }();
    return value;
  }

  void write_azel_file(char const* path, double uth, double freq_hz, AstroResult const& result,
                       bool extraazel, bool bTx)
  {
    if (!path || !*path)
      {
        return;
      }

    QFile file {QString::fromLocal8Bit (path)};
    if (!file.open (QIODevice::WriteOnly | QIODevice::Text))
      {
        return;
      }

    int const imin = static_cast<int> (60.0 * uth);
    int const isec = static_cast<int> (3600.0 * uth);
    int const ih = static_cast<int> (uth);
    int const im = imin % 60;
    int const is = isec % 60;
    int const nfreq = static_cast<int> (freq_hz / 1000000.0);

    char buffer[512] {};
    int const n = std::snprintf(
        buffer, sizeof buffer,
        "%02d:%02d:%02d,%5.1f,%5.1f,Moon\n"
        "%02d:%02d:%02d,%5.1f,%5.1f,Sun\n"
        "%02d:%02d:%02d,%5.1f,%5.1f,Source\n"
        "%5d,%8.1f,%8.2f,%8.1f,%8.2f,Doppler, %c\n",
        ih, im, is, result.azmoon, result.elmoon,
        ih, im, is, result.azsun, result.elsun,
        ih, im, is, 0.0, 0.0,
        nfreq, static_cast<double> (result.ndop), result.dfdt,
        static_cast<double> (result.ndop00), result.dfdt0,
        bTx ? 'T' : 'R');
    file.write (buffer, n);

    if (extraazel || extra_azel_lines () >= 1)
      {
        int const n2 = std::snprintf(buffer, sizeof buffer, "%8.1f,%8.1f,%8.1f,%8.1f,%8.1f,Pol\n",
                                     result.poloffset, result.xnr, result.dgrd,
                                     result.width1, result.width2);
        file.write (buffer, n2);
      }
  }
}

extern "C"
{
  void sun_(const int* year, const int* month, const int* day_of_month,
            const float* ut_hours, const float* east_lon_deg, const float* lat_deg,
            float* ra_deg, float* dec_deg, float* lst_hours,
            float* az_deg, float* el_deg, float* mjd, float* day)
  {
    SunResult const result = compute_sun(*year, *month, *day_of_month, *ut_hours, *east_lon_deg, *lat_deg);
    *ra_deg = static_cast<float> (result.ra_deg);
    *dec_deg = static_cast<float> (result.dec_deg);
    *lst_hours = static_cast<float> (result.lst_hours);
    *az_deg = static_cast<float> (result.az_deg);
    *el_deg = static_cast<float> (result.el_deg);
    *mjd = static_cast<float> (result.mjd);
    *day = static_cast<float> (result.day);
  }

  void moondopjpl_(const int* year, const int* month, const int* day_of_month,
                   const float* ut_hours, const float* east_lon_deg, const float* lat_deg,
                   float* ra_rad, float* dec_rad, float* lst_hours, float* ha_deg,
                   float* az_deg, float* el_deg, float* vr_km_s, float* echo_delay_s)
  {
    MoonResult const moon = compute_moondop(*year, *month, *day_of_month, *ut_hours, *east_lon_deg, *lat_deg);
    *ra_rad = static_cast<float> (moon.top_ra_deg * kRadPerDeg);
    *dec_rad = static_cast<float> (moon.top_dec_deg * kRadPerDeg);
    *lst_hours = static_cast<float> (moon.lst_hours);
    *ha_deg = static_cast<float> (moon.ha_deg);
    *az_deg = static_cast<float> (moon.az_deg);
    *el_deg = static_cast<float> (moon.el_deg);
    *vr_km_s = static_cast<float> (moon.vr_km_s);
    *echo_delay_s = static_cast<float> (moon.echo_delay_s);
  }

  void astrosub(int nyear, int month, int nday, double uth, double freqMoon,
                char const* mygrid, char const* hisgrid,
                double* azsun, double* elsun, double* azmoon,
                double* elmoon, double* azmoondx, double* elmoondx, int* ntsky,
                int* ndop, int* ndop00, double* ramoon, double* decmoon, double* dgrd,
                double* poloffset, double* xnr, bool extraazel, double* techo,
                double* width1, double* width2, bool bTx, char const* AzElFileName,
                char const* /*jpleph*/)
  {
    AstroResult const result = compute_wsjtx_astro(
        nyear, month, nday, uth, freqMoon,
        QByteArray {mygrid ? mygrid : ""}, QByteArray {hisgrid ? hisgrid : ""});

    *azsun = result.azsun;
    *elsun = result.elsun;
    *azmoon = result.azmoon;
    *elmoon = result.elmoon;
    *azmoondx = result.azmoon_dx;
    *elmoondx = result.elmoon_dx;
    *ntsky = result.ntsky;
    *ndop = result.ndop;
    *ndop00 = result.ndop00;
    *ramoon = result.ramoon_hours;
    *decmoon = result.decmoon_deg;
    *dgrd = result.dgrd;
    *poloffset = result.poloffset;
    *xnr = result.xnr;
    *techo = result.techo;
    *width1 = result.width1;
    *width2 = result.width2;

    write_azel_file(AzElFileName, uth, freqMoon, result, extraazel, bTx);
  }

  void astrosub_(int* nyear, int* month, int* nday, double* uth, int* nfreq,
                 char const* mygrid, char const* hisgrid, double* azsun,
                 double* elsun, double* azmoon, double* elmoon, double* azmoondx,
                 double* elmoondx, int* ntsky, int* ndop, int* ndop00,
                 double* ramoon, double* decmoon, double* dgrd, double* poloffset,
                 double* xnr, fortran_charlen_t mygrid_len, fortran_charlen_t hisgrid_len)
  {
    AstroResult const result = compute_legacy_astro(
        *nyear, *month, *nday, *uth, *nfreq,
        trim_fortran_field(mygrid, mygrid_len),
        trim_fortran_field(hisgrid, hisgrid_len));

    *azsun = result.azsun;
    *elsun = result.elsun;
    *azmoon = result.azmoon;
    *elmoon = result.elmoon;
    *azmoondx = result.azmoon_dx;
    *elmoondx = result.elmoon_dx;
    *ntsky = result.ntsky;
    *ndop = result.ndop;
    *ndop00 = result.ndop00;
    *ramoon = result.ramoon_hours;
    *decmoon = result.decmoon_deg;
    *dgrd = result.dgrd;
    *poloffset = result.poloffset;
    *xnr = result.xnr;
  }

  void astrosub00_(int* nyear, int* month, int* nday, double* uth, int* nfreq,
                   char const* mygrid, int* ndop00, fortran_charlen_t mygrid_len)
  {
    QByteArray const grid = trim_fortran_field(mygrid, mygrid_len);
    AstroResult const result = compute_legacy_astro(*nyear, *month, *nday, *uth, *nfreq, grid, grid);
    *ndop00 = result.ndop00;
  }
}
