#ifndef CABRILLO_LOG_WINDOW_HPP_
#define CABRILLO_LOG_WINDOW_HPP_

#include "AbstractLogWindow.hpp"
#include "pimpl_h.hpp"

class QSettings;
class Configuration;
class QFont;
class QSqlTableModel;

class CabrilloLogWindow final
  : public AbstractLogWindow
{
public:
  explicit CabrilloLogWindow (QSettings *, Configuration const *, QSqlTableModel * cabrillo_log_model
                              , QWidget * parent = nullptr);
  ~CabrilloLogWindow ();
  Q_SLOT void set_nQSO(int n);

private:
  void log_model_changed (int row) override;

  class impl;
  pimpl<impl> m_;
};

#endif
