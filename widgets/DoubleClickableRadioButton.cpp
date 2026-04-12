#include "DoubleClickableRadioButton.hpp"

#include "moc_DoubleClickableRadioButton.cpp"

DoubleClickableRadioButton::DoubleClickableRadioButton (QWidget * parent)
  : QRadioButton {parent}
{
}

void DoubleClickableRadioButton::mouseDoubleClickEvent (QMouseEvent * event)
{
  Q_EMIT doubleClicked ();
  QRadioButton::mouseDoubleClickEvent (event);
}
