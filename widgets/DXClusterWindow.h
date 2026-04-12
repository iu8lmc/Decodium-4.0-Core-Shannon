// -*- Mode: C++ -*-
#ifndef DXCLUSTERWINDOW_H
#define DXCLUSTERWINDOW_H

#include <QDialog>
#include <QTimer>
#include <QVector>

class QCloseEvent;
class QComboBox;
class QCheckBox;
class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;
class QSettings;
class QTableWidget;
class QShowEvent;
class QHideEvent;

class DXClusterWindow final : public QDialog
{
  Q_OBJECT

public:
  explicit DXClusterWindow(QSettings * settings, QWidget * parent = nullptr);
  ~DXClusterWindow() override;

  void setBand(QString const& bandName, bool forceSyncToAppBand = false);
  void setMyCall(QString const& myCall);
  void suspendRefresh(int ms);

Q_SIGNALS:
  void windowVisibleChanged(bool visible);

protected:
  void closeEvent(QCloseEvent * event) override;
  void showEvent(QShowEvent * event) override;
  void hideEvent(QHideEvent * event) override;

private Q_SLOTS:
  void refreshNow();
  void onNetworkFinished(QNetworkReply * reply);
  void onBandFilterChanged(int);
  void onFollowAppBandChanged(bool checked);
  void onModeFilterChanged(int);

private:
  struct Spot
  {
    QString spotter;
    QString frequency;
    QString dxCall;
    QString comment;
    QString dateTime;
    QString lotw;
    QString eqsl;
    QString continent;
    QString band;
    QString country;
    QString adif;
    QString mode;
  };

  static QString normalizeBand(QString const& bandName);
  static QString normalizeMode(QString const& comment);
  static QString yesNoFlag(QString const& flag);
  void applyBand(QString const& bandName, bool refresh);
  bool matchesModeFilter(Spot const& spot) const;
  void rebuildTable();
  bool parsePayload(QByteArray const& payload, QVector<Spot> * out, QString * error) const;
  void setStatus(QString const& status, bool error = false);
  void read_settings();
  void write_settings();
  void applyDefaultColumnWidths();

  QSettings * settings_ {nullptr};
  QLabel * titleLabel_ {nullptr};
  QLabel * sourceLabel_ {nullptr};
  QLabel * bandLabel_ {nullptr};
  QCheckBox * followAppBandCheck_ {nullptr};
  QComboBox * bandFilter_ {nullptr};
  QComboBox * modeFilter_ {nullptr};
  QPushButton * refreshButton_ {nullptr};
  QTableWidget * table_ {nullptr};
  QLabel * statusLabel_ {nullptr};
  QTimer refreshTimer_;
  QNetworkAccessManager * network_ {nullptr};
  bool requestInFlight_ {false};
  QString appBand_;
  QString currentBand_;
  QString myCall_;
  QVector<Spot> spots_;
};

#endif // DXCLUSTERWINDOW_H
