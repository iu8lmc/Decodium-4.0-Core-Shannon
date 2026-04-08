#include "commons.h"
#include "Detector/Fil4Filter.hpp"

#include <cstdint>

namespace
{
constexpr int kMaxNumSymbols = 250;
constexpr int kNumCwSymbols = 250;
}

int itone[kMaxNumSymbols] {};
int icw[kNumCwSymbols] {};
dec_data_t dec_data {};

extern "C" float gran_ ();

float gran ()
{
  return gran_ ();
}

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
