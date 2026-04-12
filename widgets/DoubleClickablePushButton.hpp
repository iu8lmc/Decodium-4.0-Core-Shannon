#ifndef DOUBLE_CLICKABLE_PUSH_BUTTON_HPP_
#define DOUBLE_CLICKABLE_PUSH_BUTTON_HPP_

#include <QPushButton>

//
// DoubleClickablePushButton - QPushButton that emits a mouse double
//                 click signal
//
//  Clients  should be  aware of  the QWidget::mouseDoubleClickEvent()
//  notes about receipt of mouse press and mouse release events.
//
class DoubleClickablePushButton
  : public QPushButton
{
  Q_OBJECT

public:
  DoubleClickablePushButton (QWidget * = nullptr);

  Q_SIGNAL void doubleClicked ();

protected:
  void mouseDoubleClickEvent (QMouseEvent *) override;
};

#endif
