#ifndef EQUALIZATION_TOOLS_DIALOG_HPP__
#define EQUALIZATION_TOOLS_DIALOG_HPP__

#include <QObject>

#include "pimpl_h.hpp"

class QWidget;
class QSettings;
class QDir;

class EqualizationToolsDialog
  : public QObject
{
  Q_OBJECT

public:
  explicit EqualizationToolsDialog (QSettings *
                                    , QDir const& data_directory
                                    , QVector<double> const& coefficients
                                    , QWidget * = nullptr);
  Q_SLOT void show ();

  Q_SIGNAL void phase_equalization_changed (QVector<double> const&);

private:
  class impl;
  pimpl<impl> m_;
};

#endif
