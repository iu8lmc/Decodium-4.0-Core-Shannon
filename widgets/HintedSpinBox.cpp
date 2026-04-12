#include "HintedSpinBox.hpp"

#include <algorithm>

auto HintedSpinBox::values (values_type values, bool set_min_max) -> values_type
{
  // replace any prior values
  std::swap (values, values_);

  // sort values and make unique
  std::sort (values_.begin (), values_.end ());
  auto last = std::unique (values_.begin (), values_.end ());
  values_.erase (last, values_.end ());
  if (set_min_max)
    {
      setMinimum (values_.front ());
      setMaximum (values_.back ());
    }
  return values;                // return old values
}

void HintedSpinBox::stepBy (int steps)
{
  if (values_.size ())
    {
      if (steps > 0)
        {
          auto p = std::upper_bound (values_.begin (), values_.end (), value ());
          if (p != values_.end () && *p <= maximum ())
            {
              setValue (*p);
            }
          else if (wrapping ())
            {
              setValue (values_.front ());
            }
        }
      else if (steps < 0)
        {
          auto p = std::lower_bound (values_.begin (), values_.end (), value ());
          if (p != values_.begin () && *(p - 1) >= minimum ())
            {
              setValue (*(p - 1));
            }
          else if (wrapping ())
            {
              setValue (values_.back ());
            }
        }
    }
  else
    {
      QSpinBox::stepBy (steps); // no values so revert to QSpinBox behaviour
    }
}
