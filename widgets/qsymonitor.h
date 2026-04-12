#ifndef QSYMONITOR_H
#define QSYMONITOR_H

#include <QWidget>

class QSettings;
class Configuration;
class QFont;

namespace Ui {
class QSYMonitor;
}

class QSYMonitor : public QWidget
{
  Q_OBJECT

public:
  explicit QSYMonitor(QSettings *, QFont const&, Configuration const * configuration, QWidget *parent = nullptr);
  ~QSYMonitor();
  void changeFont (QFont const&);
  Q_SLOT void getQSYData(QString value);
  void getBandModeFreq(QString theTime, QString theCall, QString value);

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  QSettings * settings_;
  Configuration const * configuration_;
  Ui::QSYMonitor *ui;
  void read_settings ();
  void write_settings ();
  void createQSYLine (QString value);


private slots:
  void on_clearButton_clicked();
};

#endif // QSYMONITOR_H
