// -*- Mode: C++ -*-
#ifndef METERWIDGET_H
#define METERWIDGET_H

#include <QWidget>
#include <QQueue>

class MeterWidget : public QWidget
{
  Q_OBJECT
  Q_PROPERTY (int value READ value WRITE setValue)

public:
  explicit MeterWidget (QWidget *parent = 0);

  // value property
  int value () const {return m_signal;}
  Q_SLOT void setValue (int value);

  // QWidget implementation
  QSize sizeHint () const override;
  void set_sigPeak(int value);
protected:
  void paintEvent( QPaintEvent * ) override;

private:
  QQueue<int> signalQueue;
  int m_signal;
  int m_noisePeak;
  int m_sigPeak; // peak value for color coding
};

#endif // METERWIDGET_H
