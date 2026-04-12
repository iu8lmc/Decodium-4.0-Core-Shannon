#include "countriesworked.h"

#include <set>

#include "pimpl_impl.hpp"

class CountriesWorkedimpl final
{
public:
  // element is country-name+band where the '+' is actual a character
  // which allows an efficient lower_bound() search for country-name+
  // to check for ATNOs
  std::set<QString> worked_;
};

void CountriesWorked::add (QString const& country, QString const& band)
{
  m_->worked_.insert (country + '+' + band);
}    

bool CountriesWorked::contains (QString const& country, QString const& band) const
{
  return m_->worked_.end () != m_->worked_.lower_bound (country + '+' + band);
}
