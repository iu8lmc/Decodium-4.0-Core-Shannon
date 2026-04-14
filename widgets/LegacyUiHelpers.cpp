#include "LegacyUiHelpers.hpp"

#include <algorithm>

namespace decodium
{
namespace legacy
{

void index_sort_ascending (float const* values, int count, int* indices)
{
  if (!indices || count <= 0)
    {
      return;
    }

  if (!values)
    {
      return;
    }

  constexpr int kInsertionThreshold = 7;
  constexpr int kStackSize = 50;

  for (int j = 0; j < count; ++j)
    {
      indices[j] = j + 1;
    }

  int istack[kStackSize] {};
  int jstack = 0;
  int l = 1;
  int ir = count;

  while (true)
    {
      if (ir - l < kInsertionThreshold)
        {
          for (int j = l + 1; j <= ir; ++j)
            {
              int const indxt = indices[j - 1];
              float const a = values[indxt - 1];
              int i = 0;
              for (i = j - 1; i >= 1; --i)
                {
                  if (values[indices[i - 1] - 1] <= a)
                    {
                      break;
                    }
                  indices[i] = indices[i - 1];
                }
              indices[i] = indxt;
            }

          if (jstack == 0)
            {
              return;
            }

          ir = istack[jstack - 1];
          l = istack[jstack - 2];
          jstack -= 2;
        }
      else
        {
          int const k = (l + ir) / 2;
          std::swap (indices[k - 1], indices[l]);

          if (values[indices[l] - 1] > values[indices[ir - 1] - 1])
            {
              std::swap (indices[l], indices[ir - 1]);
            }

          if (values[indices[l - 1] - 1] > values[indices[ir - 1] - 1])
            {
              std::swap (indices[l - 1], indices[ir - 1]);
            }

          if (values[indices[l] - 1] > values[indices[l - 1] - 1])
            {
              std::swap (indices[l], indices[l - 1]);
            }

          int i = l + 1;
          int j = ir;
          int const indxt = indices[l - 1];
          float const a = values[indxt - 1];

          while (true)
            {
              do
                {
                  ++i;
                }
              while (values[indices[i - 1] - 1] < a);

              do
                {
                  --j;
                }
              while (values[indices[j - 1] - 1] > a);

              if (j < i)
                {
                  break;
                }
              std::swap (indices[i - 1], indices[j - 1]);
            }

          indices[l - 1] = indices[j - 1];
          indices[j - 1] = indxt;
          jstack += 2;
          if (jstack > kStackSize)
            {
              return;
            }
          if (ir - i + 1 >= j - l)
            {
              istack[jstack - 1] = ir;
              istack[jstack - 2] = i;
              ir = j - 1;
            }
          else
            {
              istack[jstack - 1] = j - 1;
              istack[jstack - 2] = l;
              l = i;
            }
        }
    }
}

}
}
