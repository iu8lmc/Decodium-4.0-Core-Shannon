#ifndef COLORHIGHLIGHTING_H_
#define COLORHIGHLIGHTING_H_

#include <QDialog>
#include <QScopedPointer>

class QSettings;
class DecodeHighlightingModel;

namespace Ui {
  class ColorHighlighting;
}

class ColorHighlighting final
: public QDialog
{
  Q_OBJECT;

 public:
  explicit ColorHighlighting(QSettings *, DecodeHighlightingModel const&, QWidget * parent = nullptr);
  ~ColorHighlighting ();

  Q_SLOT void set_items (DecodeHighlightingModel const&);

 private:
  void read_settings ();
  void write_settings ();

  QScopedPointer<Ui::ColorHighlighting> ui;
  QSettings * settings_;
};

#endif
