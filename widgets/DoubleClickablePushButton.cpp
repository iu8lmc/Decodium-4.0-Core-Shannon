#include "DoubleClickablePushButton.hpp"

#include "moc_DoubleClickablePushButton.cpp"

DoubleClickablePushButton::DoubleClickablePushButton (QWidget * parent)
  : QPushButton {parent}
{
}

void DoubleClickablePushButton::mouseDoubleClickEvent (QMouseEvent * event)
{
  Q_EMIT doubleClicked ();
  QPushButton::mouseDoubleClickEvent (event);
}
