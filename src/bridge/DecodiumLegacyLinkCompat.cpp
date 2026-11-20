#include "Detector/Fil4Filter.hpp"

#include <cstdint>

extern "C" void fil4_ (int16_t * id1, int32_t * n1, int16_t * id2, int32_t * n2, short int *)
{
  if (!id1 || !n1 || !id2 || !n2)
    {
      return;
    }

  int32_t output_count = 0;
  fil4_cpp (id1, *n1, id2, output_count);
  *n2 = output_count;
}

// The Qt/C++ astro path resolves JPLEPH directly; the legacy startup hook
// only initialized the old Fortran common block.
extern "C" void jpl_setup_ (char *, fortran_charlen_t)
{
}
