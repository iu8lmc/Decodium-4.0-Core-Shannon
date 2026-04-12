#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "Detector/JT9FastHelpers.hpp"

extern "C"
{
  void spec9f_ (short id2[], int* npts, int* nsps, float s1[], int* jz, int* nq);
  void foldspec9f_ (float s1[], int* nq, int* jz, int* ja, int* jb, float s2[]);
  void sync9f_ (float s2[], int* nq, int* nfa, int* nfb, float ss2[], float ss3[],
                int* lagpk, int* ipk, float* ccfbest);
  void softsym9f_ (float ss2[], float ss3[], std::int8_t soft[]);
}

namespace
{

bool close_float (float lhs, float rhs, float abs_tol = 1.0e-4f, float rel_tol = 1.0e-4f)
{
  float const scale = std::max (1.0f, std::max (std::fabs (lhs), std::fabs (rhs)));
  return std::fabs (lhs - rhs) <= abs_tol + rel_tol * scale;
}

bool compare_spec9f ()
{
  int const nsps = 240;
  int const npts = 30000;
  std::vector<short> audio (static_cast<std::size_t> (npts + 1), 0);
  double const pi = 4.0 * std::atan (1.0);
  for (int i = 0; i <= npts; ++i)
    {
      double const sample = 12000.0 * std::sin (2.0 * pi * 1510.0 * i / 12000.0)
                          + 4000.0 * std::sin (2.0 * pi * 1680.0 * i / 12000.0);
      audio[static_cast<std::size_t> (i)] = static_cast<short> (std::lround (sample));
    }

  int jz_fortran = npts / (nsps / 4);
  int nq_fortran = (2 * nsps) / 4;
  std::vector<float> s1_fortran (static_cast<std::size_t> (jz_fortran * nq_fortran), 0.0f);
  int npts_mut = npts;
  int nsps_mut = nsps;
  spec9f_ (audio.data (), &npts_mut, &nsps_mut, s1_fortran.data (), &jz_fortran, &nq_fortran);

  std::vector<float> s1_cpp;
  int jz_cpp = 0;
  int nq_cpp = 0;
  decodium::jt9fast::spec9f_compute (audio.data (), npts, nsps, s1_cpp, &jz_cpp, &nq_cpp);
  if (jz_cpp != jz_fortran || nq_cpp != nq_fortran)
    {
      std::fprintf (stderr, "spec9f shape mismatch: jz %d/%d nq %d/%d\n",
                    jz_fortran, jz_cpp, nq_fortran, nq_cpp);
      return false;
    }

  for (int i = 0; i < jz_fortran * nq_fortran; ++i)
    {
      if (!close_float (s1_fortran[static_cast<std::size_t> (i)], s1_cpp[static_cast<std::size_t> (i)]))
        {
          std::fprintf (stderr, "spec9f mismatch at %d: %g vs %g\n",
                        i, s1_fortran[static_cast<std::size_t> (i)], s1_cpp[static_cast<std::size_t> (i)]);
          return false;
        }
    }
  return true;
}

bool compare_fold_sync_soft ()
{
  int const nsps = 240;
  int const npts = 30000;
  std::vector<short> audio (static_cast<std::size_t> (npts + 1), 0);
  double const pi = 4.0 * std::atan (1.0);
  for (int i = 0; i <= npts; ++i)
    {
      double const sample = 12000.0 * std::sin (2.0 * pi * 1500.0 * i / 12000.0)
                          + 2500.0 * std::sin (2.0 * pi * 1625.0 * i / 12000.0);
      audio[static_cast<std::size_t> (i)] = static_cast<short> (std::lround (sample));
    }

  int jz = npts / (nsps / 4);
  int nq = (2 * nsps) / 4;
  std::vector<float> s1 (static_cast<std::size_t> (jz * nq), 0.0f);
  int npts_mut = npts;
  int nsps_mut = nsps;
  spec9f_ (audio.data (), &npts_mut, &nsps_mut, s1.data (), &jz, &nq);

  int ja = 17;
  int jb = std::min (jz, 356);
  std::array<float, 240 * 340> s2_fortran {};
  foldspec9f_ (s1.data (), &nq, &jz, &ja, &jb, s2_fortran.data ());

  std::array<float, 240 * 340> s2_cpp {};
  decodium::jt9fast::foldspec9f_compute (s1.data (), nq, jz, ja, jb, s2_cpp);
  for (int i = 0; i < int (s2_fortran.size ()); ++i)
    {
      if (!close_float (s2_fortran[static_cast<std::size_t> (i)], s2_cpp[static_cast<std::size_t> (i)]))
        {
          std::fprintf (stderr, "foldspec9f mismatch at %d: %g vs %g\n",
                        i, s2_fortran[static_cast<std::size_t> (i)], s2_cpp[static_cast<std::size_t> (i)]);
          return false;
        }
    }

  int nfa = 1000;
  int nfb = 2000;
  std::array<float, 9 * 85> ss2_fortran {};
  std::array<float, 8 * 69> ss3_fortran {};
  int lagpk_fortran = 0;
  int ipk_fortran = 0;
  float ccfbest_fortran = 0.0f;
  sync9f_ (s2_fortran.data (), &nq, &nfa, &nfb, ss2_fortran.data (), ss3_fortran.data (),
           &lagpk_fortran, &ipk_fortran, &ccfbest_fortran);

  auto const sync_cpp = decodium::jt9fast::sync9f_compute (s2_cpp, nq, nfa, nfb);
  if (lagpk_fortran != sync_cpp.lagpk || ipk_fortran != sync_cpp.ipk
      || !close_float (ccfbest_fortran, sync_cpp.ccfbest))
    {
      std::fprintf (stderr, "sync9f scalar mismatch: lag %d/%d ipk %d/%d ccf %g/%g\n",
                    lagpk_fortran, sync_cpp.lagpk, ipk_fortran, sync_cpp.ipk,
                    ccfbest_fortran, sync_cpp.ccfbest);
      return false;
    }
  for (int i = 0; i < int (ss2_fortran.size ()); ++i)
    {
      if (!close_float (ss2_fortran[static_cast<std::size_t> (i)], sync_cpp.ss2[static_cast<std::size_t> (i)]))
        {
          std::fprintf (stderr, "sync9f ss2 mismatch at %d: %g vs %g\n",
                        i, ss2_fortran[static_cast<std::size_t> (i)],
                        sync_cpp.ss2[static_cast<std::size_t> (i)]);
          return false;
        }
    }
  for (int i = 0; i < int (ss3_fortran.size ()); ++i)
    {
      if (!close_float (ss3_fortran[static_cast<std::size_t> (i)], sync_cpp.ss3[static_cast<std::size_t> (i)]))
        {
          std::fprintf (stderr, "sync9f ss3 mismatch at %d: %g vs %g\n",
                        i, ss3_fortran[static_cast<std::size_t> (i)],
                        sync_cpp.ss3[static_cast<std::size_t> (i)]);
          return false;
        }
    }

  std::array<std::int8_t, 207> soft_fortran {};
  std::array<std::int8_t, 207> soft_cpp {};
  softsym9f_ (ss2_fortran.data (), ss3_fortran.data (), soft_fortran.data ());
  decodium::jt9fast::softsym9f_compute (sync_cpp.ss2, sync_cpp.ss3, soft_cpp);
  for (int i = 0; i < 207; ++i)
    {
      if (soft_fortran[static_cast<std::size_t> (i)] != soft_cpp[static_cast<std::size_t> (i)])
        {
          std::fprintf (stderr, "softsym9f mismatch at %d: %d vs %d\n",
                        i, int (soft_fortran[static_cast<std::size_t> (i)]),
                        int (soft_cpp[static_cast<std::size_t> (i)]));
          return false;
        }
    }
  return true;
}

}

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};
  bool const spec_ok = compare_spec9f ();
  bool const rest_ok = compare_fold_sync_soft ();
  if (!spec_ok || !rest_ok)
    {
      std::fprintf (stderr, "spec9f=%s fold_sync_soft=%s\n",
                    spec_ok ? "ok" : "fail",
                    rest_ok ? "ok" : "fail");
      return 1;
    }
  std::printf ("Fast9 stage compare passed for spec9f, foldspec9f, sync9f and softsym9f\n");
  return 0;
}
