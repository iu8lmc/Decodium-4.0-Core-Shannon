#ifndef HINTED_SPIN_BOX_HPP_
#define HINTED_SPIN_BOX_HPP_

#include <vector>

#include <QSpinBox>

//
// HintedSpinBox - QSpinBox refinement with an optional sequence of
//                 discrete hints
//
//  The  hint  sequence  values  are  used  as  step  values  and  can
//  optionally  define the  minimum and  maximum value.   Intermediate
//  values are  allowed but  the step  up/down arrows  and accelerator
//  keys will only use the hints.
//
//  Ensure that the default minimum  and maximum values are sufficient
//  to allow initial initialization if done before the hints are set.
//
class HintedSpinBox
  : public QSpinBox
{
public:
  typedef std::vector<int> values_type;

  HintedSpinBox (QWidget * parent = nullptr)
    : QSpinBox {parent}
  {
  }

  // Initialize sequence of values allowed.
  //
  // This  spin box  behaves exactly  as  a standard  QSpinBox if  the
  // sequence of values is empty.
  //
  // The minimum and  maximum are automatically set to  the lowest and
  // highest value provided if required.
  //
  // Returns the previous sequence.
  values_type values (values_type values, bool set_min_max = true);

  // Read access to the values.
  values_type const& values () const {return values_;}

protected:
  // override the QSpinBox stepping
  void stepBy (int steps) override;

private:
  values_type values_;
};

#endif
