#include "widgets/LegacyUiHelpers.hpp"

extern "C" void indexx_ (float arr[], int* n, int indx[])
{
  if (!n)
    return;

  decodium::legacy::index_sort_ascending (arr, *n, indx);
}
