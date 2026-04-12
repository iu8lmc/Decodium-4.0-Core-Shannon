#include "wsjtx_config.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

extern "C"
{
void cgen65_ (char* message, int* mode65, double* samfac, int* nsendingsh, char* msgsent,
              std::complex<float> cwave[], int* nwave, fortran_charlen_t len1,
              fortran_charlen_t len2);
void gen_q65_cwave_ (char* msg, int* ntxFreq, int* ntone_spacing, char* msgsent,
                     std::complex<float> cwave[], int* nwave, fortran_charlen_t len1,
                     fortran_charlen_t len2);
float gran_ ();
float rran_ ();
void four2a_ (std::complex<float> a[], int* nfft, int* ndim, int* isign, int* iform);
}

namespace
{
constexpr int kNmax {60 * 96000};
constexpr int kQ65Chars {37};
constexpr int kJt65Chars {22};
constexpr int kDefaultTxFreq {1270};
double const kTwoPi = 8.0 * std::atan (1.0);

[[noreturn]] void fail (std::string const& message)
{
  throw std::runtime_error (message);
}

std::string trim_fixed (char const* data, std::size_t width)
{
  std::size_t used = width;
  while (used > 0 && (data[used - 1] == ' ' || data[used - 1] == '\0'))
    {
      --used;
    }
  return std::string {data, data + used};
}

template <std::size_t N> std::array<char, N> to_fixed (std::string const& value)
{
  std::array<char, N> fixed {};
  std::fill (fixed.begin (), fixed.end (), ' ');
  std::size_t const count = std::min (value.size (), N);
  std::memcpy (fixed.data (), value.data (), count);
  return fixed;
}

std::string format_hhmm_name (int hhmm)
{
  std::ostringstream out;
  out << "000000_" << std::setw (4) << std::setfill ('0') << hhmm;
  return out.str ();
}

void write_binary (std::string const& path, char const* data, std::size_t size)
{
  std::ofstream out {path, std::ios::binary};
  if (!out)
    {
      fail ("unable to open output file " + path);
    }
  out.write (data, static_cast<std::streamsize> (size));
}

void dopspread (std::vector<std::complex<float>>& cwave, float fspread)
{
  if (fspread <= 0.0f || cwave.empty ())
    {
      return;
    }

  int const nfft = static_cast<int> (cwave.size ());
  int const nh = nfft / 2;
  float const df = 96000.0f / static_cast<float> (nfft);
  float const b = 6.0f;
  std::vector<std::complex<float>> cspread (static_cast<std::size_t> (nfft));
  cspread[0] = std::complex<float> {1.0f, 0.0f};
  cspread[static_cast<std::size_t> (nh)] = std::complex<float> {};
  for (int i = 1; i <= nh; ++i)
    {
      float const f = i * df;
      float const x = b * f / fspread;
      float a = 0.0f;
      std::complex<float> z1 {};
      std::complex<float> z2 {};
      if (x < 3.0f)
        {
          a = std::sqrt (1.111f / (1.0f + x * x) - 0.1f);
          float const phi1 = static_cast<float> (kTwoPi) * rran_ ();
          float const phi2 = static_cast<float> (kTwoPi) * rran_ ();
          z1 = std::complex<float> {a * std::cos (phi1), a * std::sin (phi1)};
          z2 = std::complex<float> {a * std::cos (phi2), a * std::sin (phi2)};
        }
      cspread[static_cast<std::size_t> (i)] = z1;
      if (i < nfft)
        {
          cspread[static_cast<std::size_t> (nfft - i)] = z2;
        }
    }

  int ndim = 1;
  int isign = 1;
  int iform = 1;
  int nfft_arg = nfft;
  four2a_ (cspread.data (), &nfft_arg, &ndim, &isign, &iform);

  double sum = 0.0;
  for (std::complex<float> const& value : cspread)
    {
      sum += std::norm (value);
    }
  if (sum <= 0.0)
    {
      return;
    }
  float const fac = std::sqrt (1.0f / static_cast<float> (sum / nfft));
  for (std::size_t i = 0; i < cspread.size (); ++i)
    {
      cwave[i] *= cspread[i] * fac;
    }
}

std::vector<std::string> const kMessageList {
  "CQ DL4KGC JN68", "CQ HB9Q JN47", "CQ G3LTF IO91", "CQ K1JT FN20", "CQ W3SZ FN10",
  "CQ DL3WDG JN68", "W3XEE K4XFF EM12", "W5XGG K6XHH EM13", "W7XII K8XJJ EM14",
  "W9XKK K0XLL EM15", "G0XMM F1XNN JN16", "G2XOO F3XPP JN17", "G4XQQ F5XRR JN18",
  "G6XSS F7XTT JN19", "W1YAA K2YBB EM20", "W2YCC K3YDD EM21", "W3YEE K4YFF EM22",
  "W5YGG K6YHH EM23", "W7YII K8YJJ EM24", "W9YKK K0YLL EM25", "G0YMM F1YNN JN26",
  "G2YOO F3YPP JN27", "G4YQQ F5YRR JN28", "G6YSS F7YTT JN29", "W1ZAA K2ZBB EM30",
  "W2ZCC K3ZDD EM31", "W3ZEE K4ZFF EM32", "W5ZGG K6ZHH EM33", "W7ZII K8ZJJ EM34",
  "W9ZKK K0ZLL EM35", "G0ZMM F1ZNN JN36", "G2ZOO F3ZPP JN37", "G4ZQQ F5ZRR JN38",
  "G6ZSS F7ZTT JN39", "W1AXA K2BXB EM40", "W2CXC K3DXD EM41", "W3EXE K4FXF EM42",
  "W5GXG K6HXH EM43", "W7IXI K8JXJ EM44", "CQ DG2YCB JO42", "CQ ON4AOI JO21",
  "CQ KA1GT FN54", "G6SSS F7TTT JN09", "W1XAA K2XBB EM10", "W2XCC K3XDD EM11",
  "W9KXK K0LXL EM45", "G0MXM F1NXN JN46", "G2OXO F3PXP JN47", "G4QXQ F5RXR JN48",
  "G6SXS F7TXT JN49", "W1AYA K2BYB EM50", "W2CYC K3DYD EM51", "W3EYE K4FYF EM52",
  "W5GYG K6HYH EM53", "W7IYI K8JYJ EM54", "W9KYK K0LYL EM55", "G0MYM F1NYN JN56",
  "G2OYO F3PYP JN57", "G4QYQ F5RYR JN58", "G6SYS F7TYT JN59"
};
}

int main (int argc, char* argv[])
{
  try
    {
      if (argc != 13)
        {
          std::cout << "Usage:   mapsim \"message\" mode DT fa fb nsigs pol fDop SNR nfiles fcenter HHmm\n"
                    << "Example: mapsim \"CQ K1ABC FN42\" B 2.5 -20 20 21 45 0.0 -20 1 1296.1 1803\n"
                    << "         mode = A B C for JT65; QA-QE for Q65-60A\n"
                    << "         fa = lowest freq in kHz, relative to center\n"
                    << "         fb = highest freq in kHz, relative to center\n"
                    << "         message = \"list\" to use callsigns from list\n"
                    << "         pol = -1 to generate a range of polarization angles.\n"
                    << "         SNR = 0 to generate a range of SNRs.\n";
          return 0;
        }

      std::string const msg0 = argv[1];
      std::string const mode = argv[2];
      float const dt0 = std::stof (argv[3]);
      float const fa = std::stof (argv[4]);
      float const fb = std::stof (argv[5]);
      int const nsigs = std::stoi (argv[6]);
      int const npol_arg = std::stoi (argv[7]);
      float const fdop = std::stof (argv[8]);
      float const snrdb = std::stof (argv[9]);
      int const nfiles = std::stoi (argv[10]);
      double const fcenter = std::stod (argv[11]);
      int const hhmm = std::stoi (argv[12]);

      if (nsigs < 1)
        {
          fail ("nsigs must be positive");
        }

      bool const bq65 = !mode.empty () && mode[0] == 'Q';
      int ntone_spacing = 1;
      if ((mode.size () >= 1 && mode[0] == 'B') || (mode.size () >= 2 && mode[1] == 'B')) ntone_spacing = 2;
      if ((mode.size () >= 1 && mode[0] == 'C') || (mode.size () >= 2 && mode[1] == 'C')) ntone_spacing = 4;
      if (mode.size () >= 2 && mode[1] == 'D') ntone_spacing = 8;
      if (mode.size () >= 2 && mode[1] == 'E') ntone_spacing = 16;

      double const fsample = 96000.0;
      double const dt = 1.0 / fsample;
      double const rad = 360.0 / kTwoPi;
      double const samfac = 1.0;
      float const fac = 1.0f / 32767.0f;
      int const npts = kNmax;

      std::cout << "File N Mode  DT    freq   pol   fDop   SNR   Message\n"
                << std::string (68, '-') << '\n';

      int ilist = 0;
      for (int ifile = 1; ifile <= nfiles; ++ifile)
        {
          std::vector<float> d4 (static_cast<std::size_t> (4 * npts));
          for (float& sample : d4)
            {
              sample = gran_ ();
            }

          std::vector<std::complex<float>> cwave (static_cast<std::size_t> (kNmax));
          int nwave = 0;
          std::string message = msg0;
          std::string msgsent;
          auto generate_waveform = [&] {
            std::fill (cwave.begin (), cwave.end (), std::complex<float> {});
            if (bq65)
              {
                auto fixed_msg = to_fixed<kQ65Chars> (message);
                std::array<char, kQ65Chars> fixed_sent {};
                std::fill (fixed_sent.begin (), fixed_sent.end (), ' ');
                gen_q65_cwave_ (fixed_msg.data (), const_cast<int*> (&kDefaultTxFreq), &ntone_spacing,
                                fixed_sent.data (), cwave.data (), &nwave,
                                static_cast<fortran_charlen_t> (kQ65Chars),
                                static_cast<fortran_charlen_t> (kQ65Chars));
                msgsent = trim_fixed (fixed_sent.data (), fixed_sent.size ());
              }
            else
              {
                auto fixed_msg = to_fixed<kJt65Chars> (message);
                std::array<char, kJt65Chars> fixed_sent {};
                std::fill (fixed_sent.begin (), fixed_sent.end (), ' ');
                int nsendingsh = 0;
                int mode65 = ntone_spacing;
                double samfac_local = samfac;
                cgen65_ (fixed_msg.data (), &mode65, &samfac_local, &nsendingsh, fixed_sent.data (),
                         cwave.data (), &nwave, static_cast<fortran_charlen_t> (kJt65Chars),
                         static_cast<fortran_charlen_t> (kJt65Chars));
                msgsent = trim_fixed (fixed_sent.data (), fixed_sent.size ());
              }
            if (fdop > 0.0f)
              {
                dopspread (cwave, fdop);
              }
          };

          if (msg0 != "list")
            {
              generate_waveform ();
            }

          for (int isig = 1; isig <= nsigs; ++isig)
            {
              if (msg0 == "list")
                {
                  if (ilist >= static_cast<int> (kMessageList.size ()))
                    {
                      ilist = 0;
                    }
                  message = kMessageList[static_cast<std::size_t> (ilist++)];
                  generate_waveform ();
                }

              float pol = static_cast<float> (npol_arg);
              if (npol_arg < 0)
                {
                  pol = (isig - 1) * 180.0f / static_cast<float> (nsigs);
                }
              double const a = std::cos (pol / rad);
              double const b = std::sin (pol / rad);
              double f = 1000.0 * (fa + fb) / 2.0;
              if (nsigs > 1)
                {
                  f = 1000.0 * (fa + (isig - 1) * (fb - fa) / (nsigs - 1.0));
                }
              double const dphi = kTwoPi * f * dt + 0.5 * kTwoPi;

              float snrdbx = snrdb;
              if (snrdb == 0.0f)
                {
                  snrdbx = -15.0f - 15.0f * (isig - 1.0f) / nsigs;
                }
              float const sig = std::sqrt (2.2f * 2500.0f / 96000.0f) * std::pow (10.0f, 0.05f * snrdbx);
              std::cout << std::setw (3) << ifile
                        << std::setw (3) << isig << "  "
                        << std::setw (2) << mode
                        << std::fixed << std::setprecision (2)
                        << std::setw (6) << dt0
                        << std::setprecision (3)
                        << std::setw (8) << (0.001 * f)
                        << std::setw (5) << std::lround (pol)
                        << std::setprecision (1)
                        << std::setw (7) << fdop
                        << std::setw (7) << snrdbx << "  "
                        << msgsent << '\n';

              double phi = 0.0;
              int const i0 = static_cast<int> (fsample * (1.0 + dt0));
              for (int i = 1; i <= nwave; ++i)
                {
                  phi += dphi;
                  if (phi < -kTwoPi) phi += kTwoPi;
                  if (phi > kTwoPi) phi -= kTwoPi;
                  std::complex<float> const osc {static_cast<float> (std::cos (phi)),
                                                 static_cast<float> (-std::sin (phi))};
                  std::complex<float> const z = sig * cwave[static_cast<std::size_t> (i - 1)] * osc;
                  std::complex<float> const zx = static_cast<float> (a) * z;
                  std::complex<float> const zy = static_cast<float> (b) * z;
                  int const j = i + i0;
                  if (j >= 1 && j <= npts)
                    {
                      std::size_t const base = static_cast<std::size_t> (j - 1);
                      d4[base] += zx.real ();
                      d4[static_cast<std::size_t> (npts + j - 1)] += zx.imag ();
                      d4[static_cast<std::size_t> (2 * npts + j - 1)] += zy.real ();
                      d4[static_cast<std::size_t> (3 * npts + j - 1)] += zy.imag ();
                    }
                }
            }

          std::vector<int16_t> id4 (static_cast<std::size_t> (4 * npts));
          std::vector<int16_t> id2 (static_cast<std::size_t> (2 * npts));
          for (int i = 0; i < npts; ++i)
            {
              int16_t const v1 = static_cast<int16_t> (std::lround (d4[static_cast<std::size_t> (i)] / fac));
              int16_t const v2 = static_cast<int16_t> (std::lround (d4[static_cast<std::size_t> (npts + i)] / fac));
              int16_t const v3 = static_cast<int16_t> (std::lround (d4[static_cast<std::size_t> (2 * npts + i)] / fac));
              int16_t const v4 = static_cast<int16_t> (std::lround (d4[static_cast<std::size_t> (3 * npts + i)] / fac));
              id4[static_cast<std::size_t> (i)] = v1;
              id4[static_cast<std::size_t> (npts + i)] = v2;
              id4[static_cast<std::size_t> (2 * npts + i)] = v3;
              id4[static_cast<std::size_t> (3 * npts + i)] = v4;
              id2[static_cast<std::size_t> (i)] = v1;
              id2[static_cast<std::size_t> (npts + i)] = v2;
            }

          std::string const stem = format_hhmm_name (hhmm);
          std::vector<char> iq_blob (sizeof (double) + id2.size () * sizeof (int16_t));
          std::memcpy (iq_blob.data (), &fcenter, sizeof (double));
          std::memcpy (iq_blob.data () + sizeof (double), id2.data (), id2.size () * sizeof (int16_t));
          write_binary (stem + ".iq", iq_blob.data (), iq_blob.size ());

          std::vector<char> tf2_blob (sizeof (double) + id4.size () * sizeof (int16_t));
          std::memcpy (tf2_blob.data (), &fcenter, sizeof (double));
          std::memcpy (tf2_blob.data () + sizeof (double), id4.data (), id4.size () * sizeof (int16_t));
          write_binary (stem + ".tf2", tf2_blob.data (), tf2_blob.size ());
        }

      return 0;
    }
  catch (std::exception const& error)
    {
      std::cerr << "mapsim: " << error.what () << '\n';
      return 1;
    }
}
