// -*- Mode: C++ -*-
#ifndef QSYMESSAGECREATOR_H
#define QSYMESSAGECREATOR_H
#include <QWidget>
#include <QObject>
#include <QCloseEvent>
#include <QTabWidget>
#include <QMap>

class QSettings;
class Configuration;
namespace Ui {
  class QSYMessageCreator;
}

class QSYMessageCreator
  : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(int kHzVHF READ getkHzVHF WRITE setkHzVHF)
  Q_PROPERTY(QString bandVHF READ getbandVHF WRITE setbandVHF)
  Q_PROPERTY(QChar modeVHF READ getmodeVHF WRITE setmodeVHF)
  Q_PROPERTY(int kHzHF READ getkHzHF WRITE setkHzHF)
  Q_PROPERTY(QString bandHF READ getbandHF WRITE setbandHF)
  Q_PROPERTY(QChar modeHF READ getmodeHF WRITE setmodeHF)
  Q_PROPERTY(int kHzEME READ getkHzEME WRITE setkHzEME)
  Q_PROPERTY(QString bandEME READ getbandEME WRITE setbandEME)
  Q_PROPERTY(QChar modeEME READ getmodeEME WRITE setmodeEME)

public:
  explicit QSYMessageCreator(QSettings * settings, Configuration const *, QWidget * parent = 0);
  ~QSYMessageCreator();
  QString WriteMessage(QString call, QString band, QChar mode);
  QString bandVHF;
  QChar modeVHF;
  int kHzVHF;
  QString bandHF;
  QChar modeHF;
  int kHzHF;
  QString bandEME;
  QChar modeEME;
  int kHzEME;
  int MHzVHFInt;
  bool firstTime = true;
  QChar initVHFMode;

  QString getbandVHF () const {return m_bandVHF;}
  QChar getmodeVHF () const {return m_modeVHF;}
  int getkHzVHF () const {return m_kHzVHF;}
  QString getbandHF () const {return m_bandHF;}
  QChar getmodeHF () const {return m_modeHF;}
  int getkHzHF () const {return m_kHzHF;}
  QString getbandEME () const {return m_bandEME;}
  QChar getmodeEME () const {return m_modeEME;}
  int getkHzEME () const {return m_kHzEME;}

  void setbandVHF (QString bandVHF) {m_bandVHF = bandVHF;}
  void setmodeVHF (QChar modeVHF) { m_modeVHF = modeVHF;}
  void setkHzVHF  (int kHzVHF) {m_kHzVHF = kHzVHF;}
  void setbandHF (QString bandHF) {m_bandHF = bandHF;}
  void setmodeHF (QChar modeHF) { m_modeHF = modeHF;}
  void setkHzHF  (int kHzHF) {m_kHzHF = kHzHF;}
  void setbandEME (QString bandEME) {m_bandEME = bandEME;}
  void setmodeEME (QChar modeEME) { m_modeEME = modeEME;}
  void setkHzEME  (int kHzEME) {m_kHzEME = kHzEME;}
  QChar modeFromSpinBox (QString band, int MHzVHFInt);
  void setupfmSpinBox(QString band);
  void setMode(QString band, QChar mode, int region);
  QChar getMode(QString band, int region);
  int getBandEdge(QString band);

  QMap<QPair<QString, QChar>, int> kHzFreqMap;
  QMap<QString,int> MHzFreqMap;

  QTabWidget tabWidget;

  Q_SLOT void getDxBase(QString value);

protected:
  void showEvent(QShowEvent *event) override {
    QWidget::showEvent(event); // Call the base class implementation
  }
  void closeEvent(QCloseEvent *event) override;

signals:
  void sendMessage(const QString &value);
  void sendChkBoxChange(const bool &value);
  void sendQSYMessageCreatorStatus(const bool &value);

private:
  QSettings * settings_;
  Configuration const * configuration_;
  Ui::QSYMessageCreator *ui;
  QString getBand();
  void setBand(QString band);
  void setup();

  int m_kHzVHF = 123;
  QString m_bandVHF = "D";
  QChar m_modeVHF = 'V';
  int m_kHzHF = 123;
  QString m_bandHF = "D";
  QChar m_modeHF = 'V';
  int m_kHzEME = 123;
  QString m_bandEME = "D";
  QChar m_modeEME = 'V';

  void read_settings();
  void write_settings();
  void send_message(const QString message);
  QString getGenMessage();


private slots:
  void onTabChanged();
  void on_genButton_clicked();
  void setkHzBox(QString theBand, QChar theMode, int tabNum);//, QMap<QPair<QString,QString>,int> kHzFreqMap);
  void on_button1_clicked();
  void on_button2_clicked();
  void on_button3_clicked();
  void on_saveMHzButton_clicked();
  void on_saveMHzButton_2_clicked();
  void on_saveMHzButton_3_clicked();
  void setQSYMessageCreatorStatusFalse();
  void onfmSpinBoxValueChanged();
  void onkHzBoxValueChanged();
  void onkHzBox2ValueChanged();
  void onkHzBox3ValueChanged();
};

#endif //QSYMESSAGECREATOR_H
