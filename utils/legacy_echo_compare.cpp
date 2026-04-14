#include <QCoreApplication>
#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "commons.h"
#include "Detector/LegacyDspIoHelpers.hpp"

#if __GNUC__ > 7 || defined(__clang__)
using fortran_charlen_t_local = size_t;
#else
using fortran_charlen_t_local = int;
#endif

extern "C"
{
  void save_echo_params_ (int* ndoptotal, int* ndop, int* nfrit, float* f1, float* fspread,
                          int* toneSpacing, volatile int itone[], short id2[], int* idir);
  void avecho_ (short id2[], int* dop, int* nfrit, int* nauto, int* ndf, int* navg, int* nqual,
                float* f1, float* level, float* sigdb, float* snr, float* dfreq, float* width,
                bool* bDiskData, bool* bEchoCall, char const* txcall, char rxcall[], float* xdt,
                fortran_charlen_t_local, fortran_charlen_t_local);
  extern struct {
    float fspread_self;
    float fspread_dx;
  } echocom2_;
}

namespace
{
constexpr int kEchoNsps = 4096;
constexpr int kEchoTxSamples = 6 * kEchoNsps;
constexpr int kEchoPadding = 4096;
constexpr int kEchoNz = 4096;

struct EchoSnapshot
{
  int nqual {0};
  float xlevel {0.0f};
  float sigdb {0.0f};
  float db_err {0.0f};
  float dfreq {0.0f};
  float width {0.0f};
  float xdt {0.0f};
  QString rxcall;
  int nsum {0};
  std::array<float, kEchoNz> blue {};
  std::array<float, kEchoNz> red {};
};

bool close_enough (float lhs, float rhs, float abs_tol = 2.0e-2f, float rel_tol = 5.0e-4f)
{
  float const scale = std::max (1.0f, std::max (std::fabs (lhs), std::fabs (rhs)));
  return std::fabs (lhs - rhs) <= abs_tol + rel_tol * scale;
}

void clear_echo_common ()
{
  echocom_.nclearave = 0;
  echocom_.nsum = 0;
  std::fill_n (echocom_.blue, kEchoNz, 0.0f);
  std::fill_n (echocom_.red, kEchoNz, 0.0f);
}

void gen_echocall_local (QString const& callsign, int itone[6])
{
  std::fill (itone, itone + 6, 0);
  QByteArray const latin = callsign.left (6).toLatin1 ();
  for (int i = 0; i < latin.size () && i < 6; ++i)
    {
      int const ch = static_cast<unsigned char> (latin.at (i));
      if (ch >= '0' && ch <= '9')
        {
          itone[i] = ch - 47;
        }
      else if (ch >= 'A' && ch <= 'Z')
        {
          itone[i] = ch - 54;
        }
      else if (ch >= 'a' && ch <= 'z')
        {
          itone[i] = ch - 86;
        }
    }
}

std::vector<short> make_echo_waveform (QString const& txcall, float f1, int ndf)
{
  std::vector<short> wave (static_cast<std::size_t> (kEchoTxSamples + kEchoPadding), 0);
  int itone[6] {};
  gen_echocall_local (txcall, itone);
  double const twopi = 8.0 * std::atan (1.0);
  double const dt = 1.0 / 12000.0;
  double phi = 0.0;
  int k = 0;
  for (int j = 0; j < 6; ++j)
    {
      double const freq = f1 + itone[j] * ndf;
      double const dphi = twopi * freq * dt;
      for (int i = 0; i < kEchoNsps; ++i)
        {
          phi += dphi;
          if (phi > twopi)
            {
              phi -= twopi;
            }
          wave[static_cast<std::size_t> (k++)] = static_cast<short> (std::lround (12000.0 * std::sin (phi)));
        }
    }

  decodium::legacy::save_echo_params_inplace (0, 0, 0, f1, 10.0f, ndf, itone, wave.data ());
  return wave;
}

EchoSnapshot capture_fortran (std::vector<short> wave, QString const& txcall, int navg,
                              bool clear_average)
{
  EchoSnapshot snapshot;
  int ndop = 0;
  int nfrit = 0;
  int nauto = 0;
  int ndf = 20;
  int nqual = 0;
  float f1 = 1500.0f;
  float xlevel = 0.0f;
  float sigdb = 0.0f;
  float db_err = 0.0f;
  float dfreq = 0.0f;
  float width = 10.0f;
  bool disk_data = false;
  bool echo_call = true;
  char rxcall[7] {};
  float xdt = 0.0f;
  echocom_.nclearave = clear_average ? 1 : 0;
  QByteArray const txcall_latin = txcall.leftJustified (6, QLatin1Char {' '}).toLatin1 ();
  avecho_ (wave.data (), &ndop, &nfrit, &nauto, &ndf, &navg, &nqual, &f1, &xlevel, &sigdb,
           &db_err, &dfreq, &width, &disk_data, &echo_call, txcall_latin.constData (), rxcall,
           &xdt, static_cast<fortran_charlen_t_local> (6), static_cast<fortran_charlen_t_local> (6));
  rxcall[6] = '\0';

  snapshot.nqual = nqual;
  snapshot.xlevel = xlevel;
  snapshot.sigdb = sigdb;
  snapshot.db_err = db_err;
  snapshot.dfreq = dfreq;
  snapshot.width = width;
  snapshot.xdt = xdt;
  snapshot.rxcall = QString::fromLatin1 (rxcall);
  snapshot.nsum = echocom_.nsum;
  std::copy_n (echocom_.blue, kEchoNz, snapshot.blue.begin ());
  std::copy_n (echocom_.red, kEchoNz, snapshot.red.begin ());
  return snapshot;
}

EchoSnapshot capture_cpp (std::vector<short> const& wave, QString const& txcall, int navg,
                          bool clear_average)
{
  EchoSnapshot snapshot;
  echocom_.nclearave = clear_average ? 1 : 0;
  auto const result = decodium::legacy::avecho_update (wave.data (), 0, 0, 0, 20, navg, 1500.0f,
                                                       10.0f, false, true, txcall);
  snapshot.nqual = result.nqual;
  snapshot.xlevel = result.xlevel;
  snapshot.sigdb = result.sigdb;
  snapshot.db_err = result.db_err;
  snapshot.dfreq = result.dfreq;
  snapshot.width = result.width;
  snapshot.xdt = result.xdt;
  snapshot.rxcall = result.rxcall;
  snapshot.nsum = echocom_.nsum;
  std::copy_n (echocom_.blue, kEchoNz, snapshot.blue.begin ());
  std::copy_n (echocom_.red, kEchoNz, snapshot.red.begin ());
  return snapshot;
}

bool compare_save_echo_params ()
{
  int ndop_total = 1234;
  int ndop_audio = -12;
  int nfrit = 45;
  float f1 = 1512.5f;
  float fspread = 7.5f;
  int tone_spacing = 20;
  int itone[6] {1, 12, 5, 22, 3, 0};
  short header_cpp[15] {};
  short header_fortran[15] {};
  decodium::legacy::save_echo_params_inplace (ndop_total, ndop_audio, nfrit, f1, fspread,
                                              tone_spacing, itone, header_cpp);
  int idir = 1;
  save_echo_params_ (&ndop_total, &ndop_audio, &nfrit, &f1, &fspread, &tone_spacing,
                     reinterpret_cast<volatile int*> (itone), header_fortran, &idir);

  for (int i = 0; i < 15; ++i)
    {
      if (header_cpp[i] != header_fortran[i])
        {
          std::fprintf (stderr, "save_echo_params mismatch at word %d\n", i);
          return false;
        }
    }

  int ndop_total_out = 0;
  int ndop_audio_out = 0;
  int nfrit_out = 0;
  float f1_out = 0.0f;
  float fspread_out = 0.0f;
  int tone_spacing_out = 0;
  int itone_out[6] {};
  decodium::legacy::load_echo_params (header_fortran, &ndop_total_out, &ndop_audio_out,
                                      &nfrit_out, &f1_out, &fspread_out, &tone_spacing_out,
                                      itone_out);
  return ndop_total_out == ndop_total
      && ndop_audio_out == ndop_audio
      && nfrit_out == nfrit
      && close_enough (f1_out, f1, 1.0e-4f, 1.0e-6f)
      && close_enough (fspread_out, fspread, 1.0e-4f, 1.0e-6f)
      && tone_spacing_out == tone_spacing
      && std::equal (itone, itone + 6, itone_out);
}

bool compare_avecho ()
{
  QString const txcall = QStringLiteral ("K1JT");
  std::vector<short> const wave = make_echo_waveform (txcall, 1500.0f, 20);
  echocom2_.fspread_self = 10.0f;
  echocom2_.fspread_dx = 10.0f;

  std::array<EchoSnapshot, 3> fortran_runs {};
  std::array<EchoSnapshot, 3> cpp_runs {};

  clear_echo_common ();
  for (int i = 0; i < 3; ++i)
    {
      fortran_runs[static_cast<std::size_t> (i)] = capture_fortran (wave, txcall, 3, i == 0);
    }

  clear_echo_common ();
  for (int i = 0; i < 3; ++i)
    {
      cpp_runs[static_cast<std::size_t> (i)] = capture_cpp (wave, txcall, 3, i == 0);
    }

  for (int run = 0; run < 3; ++run)
    {
      EchoSnapshot const& lhs = fortran_runs[static_cast<std::size_t> (run)];
      EchoSnapshot const& rhs = cpp_runs[static_cast<std::size_t> (run)];
      if (lhs.nqual != rhs.nqual
          || !close_enough (lhs.xlevel, rhs.xlevel, 5.0e-2f, 5.0e-4f)
          || !close_enough (lhs.sigdb, rhs.sigdb, 5.0e-2f, 5.0e-4f)
          || !close_enough (lhs.db_err, rhs.db_err, 5.0e-2f, 5.0e-4f)
          || !close_enough (lhs.dfreq, rhs.dfreq, 5.0e-2f, 5.0e-4f)
          || !close_enough (lhs.width, rhs.width, 1.0e-4f, 1.0e-6f)
          || !close_enough (lhs.xdt, rhs.xdt, 1.0e-4f, 1.0e-6f)
          || lhs.rxcall.trimmed () != rhs.rxcall.trimmed ()
          || lhs.nsum != rhs.nsum)
        {
          std::fprintf (stderr,
                        "avecho scalar mismatch on run %d\n"
                        "  nqual: %d vs %d\n"
                        "  xlevel: %.6f vs %.6f\n"
                        "  sigdb: %.6f vs %.6f\n"
                        "  db_err: %.6f vs %.6f\n"
                        "  dfreq: %.6f vs %.6f\n"
                        "  width: %.6f vs %.6f\n"
                        "  xdt: %.6f vs %.6f\n"
                        "  rxcall: '%s' vs '%s'\n"
                        "  nsum: %d vs %d\n",
                        run,
                        lhs.nqual, rhs.nqual,
                        lhs.xlevel, rhs.xlevel,
                        lhs.sigdb, rhs.sigdb,
                        lhs.db_err, rhs.db_err,
                        lhs.dfreq, rhs.dfreq,
                        lhs.width, rhs.width,
                        lhs.xdt, rhs.xdt,
                        lhs.rxcall.toLatin1 ().constData (),
                        rhs.rxcall.toLatin1 ().constData (),
                        lhs.nsum, rhs.nsum);
          return false;
        }
      for (int i = 0; i < kEchoNz; ++i)
        {
          if (!close_enough (lhs.blue[static_cast<std::size_t> (i)],
                             rhs.blue[static_cast<std::size_t> (i)], 7.5e-2f, 1.0e-3f)
              || !close_enough (lhs.red[static_cast<std::size_t> (i)],
                                rhs.red[static_cast<std::size_t> (i)], 7.5e-2f, 1.0e-3f))
            {
              std::fprintf (stderr, "avecho spectrum mismatch on run %d at bin %d\n", run, i);
              return false;
            }
        }
    }

  return true;
}

}

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};
  bool const save_ok = compare_save_echo_params ();
  bool const avecho_ok = compare_avecho ();
  std::printf ("save_echo_params=%s avecho=%s\n",
               save_ok ? "ok" : "fail",
               avecho_ok ? "ok" : "fail");
  if (!(save_ok && avecho_ok))
    {
      return 1;
    }

  std::printf ("Legacy echo compare passed for save_echo_params and avecho\n");
  return 0;
}
