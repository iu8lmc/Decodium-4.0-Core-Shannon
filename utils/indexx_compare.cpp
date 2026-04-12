#include <QCoreApplication>

#include <array>
#include <cstdio>

#include "widgets/LegacyUiHelpers.hpp"

extern "C" void indexx_ (float arr[], int* n, int indx[]);

namespace
{

template <size_t N>
bool compare_indexx (std::array<float, N> const& input)
{
  std::array<float, N> fortran_values = input;
  std::array<int, N> fortran_indices {};
  std::array<int, N> cpp_indices {};
  int count = static_cast<int> (input.size ());

  indexx_ (fortran_values.data (), &count, fortran_indices.data ());
  decodium::legacy::index_sort_ascending (input.data (), count, cpp_indices.data ());

  for (int i = 0; i < count; ++i)
    {
      if (fortran_indices[static_cast<size_t> (i)] != cpp_indices[static_cast<size_t> (i)])
        {
          std::fprintf (stderr,
                        "indexx mismatch index=%d: fortran=%d cxx=%d\n",
                        i, fortran_indices[static_cast<size_t> (i)],
                        cpp_indices[static_cast<size_t> (i)]);
          return false;
        }
    }

  return true;
}

}

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};

  std::array<float, 8> const ascending {{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f}};
  std::array<float, 8> const descending {{8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f}};
  std::array<float, 8> const duplicates {{4.0f, 1.0f, 4.0f, 3.0f, 1.0f, 2.0f, 2.0f, 5.0f}};
  std::array<float, 10> const mixed {{
    3.5f, -1.0f, 8.0f, 3.5f, 0.0f, 0.0f, 9.0f, -4.0f, 2.25f, 2.25f
  }};

  if (!compare_indexx (ascending)
      || !compare_indexx (descending)
      || !compare_indexx (duplicates)
      || !compare_indexx (mixed))
    {
      return 1;
    }

  std::printf ("Index compare passed for 4 datasets\n");
  return 0;
}
