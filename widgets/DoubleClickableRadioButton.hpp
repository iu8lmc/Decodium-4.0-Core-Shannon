#ifndef DOUBLE_CLICKABLE_RADIO_BUTTON_HPP_
#define DOUBLE_CLICKABLE_RADIO_BUTTON_HPP_

#include <QRadioButton>

//
// DoubleClickableRadioButton - QRadioButton that emits a mouse double
//                 click signal
//
//  Clients  should be  aware of  the QWidget::mouseDoubleClickEvent()
//  notes about receipt of mouse press and mouse release events.
//
class DoubleClickableRadioButton
  : public QRadioButton
{
  Q_OBJECT

public:
  DoubleClickableRadioButton (QWidget * = nullptr);

  Q_SIGNAL void doubleClicked ();

protected:
  void mouseDoubleClickEvent (QMouseEvent *) override;
};

#endif
