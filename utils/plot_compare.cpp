#include <QCoreApplication>

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

#include "widgets/PlotLegacyHelpers.hpp"

extern "C" void smo121_ (float x[], int* npts);
extern "C" void flat4_ (float swide[], int* iz, int* nflatten);
extern "C" void plotsave_ (float swide[], int* nw, int* nh, int* irow);

namespace
{

bool compare_smo121 (std::array<float, 16> const& input, int passes)
{
  std::array<float, 16> fortran_values = input;
  std::array<float, 16> cpp_values = input;
  int npts = static_cast<int> (input.size ());

  for (int i = 0; i < passes; ++i)
    {
      smo121_ (fortran_values.data (), &npts);
      decodium::plot::smooth121_inplace (cpp_values.data (), npts);
    }

  for (int i = 0; i < npts; ++i)
    {
      if (fortran_values[static_cast<size_t> (i)] != cpp_values[static_cast<size_t> (i)])
        {
          std::fprintf (stderr, "smo121 mismatch at pass=%d index=%d: fortran=%g cxx=%g\n",
                        passes, i, fortran_values[static_cast<size_t> (i)],
                        cpp_values[static_cast<size_t> (i)]);
          return false;
        }
    }

  return true;
}

bool compare_plotsave ()
{
  int nw = 6;
  int nh = 4;
  int clear_row = -99;
  float dummy = 0.0f;
  plotsave_ (&dummy, &nw, &nh, &clear_row);
  decodium::plot::clear_saved_waterfall ();

  std::array<std::array<float, 6>, 5> const rows {{
    {{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}},
    {{7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f}},
    {{13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f}},
    {{19.0f, 20.0f, 21.0f, 22.0f, 23.0f, 24.0f}},
    {{25.0f, 26.0f, 27.0f, 28.0f, 29.0f, 30.0f}}
  }};

  for (size_t push = 0; push < rows.size (); ++push)
    {
      int push_row = -1;
      std::array<float, 6> fortran_push = rows[push];
      std::array<float, 6> cpp_push = rows[push];
      plotsave_ (fortran_push.data (), &nw, &nh, &push_row);
      decodium::plot::plotsave_row (cpp_push.data (), nw, nh, push_row);

      for (int row = 0; row < nh; ++row)
        {
          std::array<float, 6> fortran_read {};
          std::array<float, 6> cpp_read {};
          int irow = row;
          plotsave_ (fortran_read.data (), &nw, &nh, &irow);
          decodium::plot::plotsave_row (cpp_read.data (), nw, nh, irow);
          for (int i = 0; i < nw; ++i)
            {
              if (fortran_read[static_cast<size_t> (i)] != cpp_read[static_cast<size_t> (i)])
                {
                  std::fprintf (stderr,
                                "plotsave mismatch push=%zu row=%d index=%d: fortran=%g cxx=%g\n",
                                push, row, i, fortran_read[static_cast<size_t> (i)],
                                cpp_read[static_cast<size_t> (i)]);
                  return false;
                }
            }
        }
    }

  clear_row = -99;
  plotsave_ (&dummy, &nw, &nh, &clear_row);
  decodium::plot::clear_saved_waterfall ();

  nw = 3;
  nh = 2;
  int push_row = -1;
  std::array<float, 3> const short_row {{9.0f, 4.0f, 1.0f}};
  std::array<float, 3> fortran_push = short_row;
  std::array<float, 3> cpp_push = short_row;
  plotsave_ (fortran_push.data (), &nw, &nh, &push_row);
  decodium::plot::plotsave_row (cpp_push.data (), nw, nh, push_row);

  std::array<float, 3> fortran_read {};
  std::array<float, 3> cpp_read {};
  int read_row = 0;
  plotsave_ (fortran_read.data (), &nw, &nh, &read_row);
  decodium::plot::plotsave_row (cpp_read.data (), nw, nh, read_row);
  for (int i = 0; i < nw; ++i)
    {
      if (fortran_read[static_cast<size_t> (i)] != cpp_read[static_cast<size_t> (i)])
        {
          std::fprintf (stderr, "plotsave resize mismatch index=%d: fortran=%g cxx=%g\n",
                        i, fortran_read[static_cast<size_t> (i)],
                        cpp_read[static_cast<size_t> (i)]);
          return false;
        }
    }

  clear_row = -99;
  plotsave_ (&dummy, &nw, &nh, &clear_row);
  decodium::plot::clear_saved_waterfall ();
  return true;
}

bool compare_flat4 (std::vector<float> const& input, int nflatten, float tolerance)
{
  std::vector<float> fortran_values = input;
  std::vector<float> cpp_values = input;
  int npts = static_cast<int> (input.size ());
  int flatten_mode = nflatten;

  flat4_ (fortran_values.data (), &npts, &flatten_mode);
  decodium::plot::flat4_inplace (cpp_values.data (), npts, flatten_mode);

  for (int i = 0; i < npts; ++i)
    {
      float const diff = std::fabs (fortran_values[static_cast<size_t> (i)]
                                  - cpp_values[static_cast<size_t> (i)]);
      if (diff > tolerance)
        {
          std::fprintf (stderr,
                        "flat4 mismatch flatten=%d index=%d: fortran=%g cxx=%g diff=%g\n",
                        nflatten, i, fortran_values[static_cast<size_t> (i)],
                        cpp_values[static_cast<size_t> (i)], diff);
          return false;
        }
    }

  return true;
}

}  // namespace

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};

  std::array<float, 16> const kSawtooth {{
    0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 6.0f, 4.0f, 2.0f,
    1.0f, 3.0f, 5.0f, 7.0f, 5.0f, 3.0f, 1.0f, 0.0f
  }};
  std::array<float, 16> const kImpulse {{
    0.0f, 0.0f, 0.0f, 0.0f, 4.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
  }};
  std::vector<float> kFlatInput (512);
  for (size_t i = 0; i < kFlatInput.size (); ++i)
    {
      float const x = static_cast<float> (i + 1);
      float const envelope = 10.0f + 0.005f * x + 0.00002f * x * x;
      float const ripple = 0.6f * std::sin (0.031f * x) + 0.25f * std::cos (0.11f * x);
      kFlatInput[i] = std::pow (10.0f, 0.1f * (envelope + ripple));
    }
  std::vector<float> kBoundaryInput (32, 3.0f);
  kBoundaryInput[0] = 1.0e30f;

  if (!compare_smo121 (kSawtooth, 1) || !compare_smo121 (kSawtooth, 3)
      || !compare_smo121 (kImpulse, 1) || !compare_smo121 (kImpulse, 4)
      || !compare_plotsave ()
      || !compare_flat4 (kFlatInput, 0, 1.0e-4f)
      || !compare_flat4 (kFlatInput, 1, 1.0e-3f)
      || !compare_flat4 (kFlatInput, 2, 1.0e-4f)
      || !compare_flat4 (kBoundaryInput, 1, 0.0f))
    {
      return 1;
    }

  std::printf ("Plot compare passed for smo121, plotsave and flat4\n");
  return 0;
}
