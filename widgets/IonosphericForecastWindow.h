// -*- Mode: C++ -*-
#ifndef IONOSPHERICFORECASTWINDOW_H
#define IONOSPHERICFORECASTWINDOW_H

#include <QDialog>
#include <QTimer>
#include <QVector>

class QCloseEvent;
class QPushButton;
class QTextBrowser;
class QNetworkAccessManager;
class QNetworkReply;
class QSettings;
class QLabel;
class QShowEvent;
class QHideEvent;

class IonosphericForecastWindow final : public QDialog
{
  Q_OBJECT

public:
  explicit IonosphericForecastWindow(QSettings * settings, QWidget * parent = nullptr);
  ~IonosphericForecastWindow() override;

Q_SIGNALS:
  void windowVisibleChanged(bool visible);

protected:
  void closeEvent(QCloseEvent * event) override;
  void showEvent(QShowEvent * event) override;
  void hideEvent(QHideEvent * event) override;

private Q_SLOTS:
  void refreshNow();
  void onNetworkFinished(QNetworkReply * reply);

private:
  struct BandCondition
  {
    QString name;
    QString time;
    QString value;
  };

  struct VhfCondition
  {
    QString name;
    QString location;
    QString value;
  };

  struct SolarSnapshot
  {
    QString updated;
    QString solarflux;
    QString aindex;
    QString kindex;
    QString xray;
    QString sunspots;
    QString solarwind;
    QString muf;
    QString geomagfield;
    QString signalnoise;
    QVector<BandCondition> bands;
    QVector<VhfCondition> vhf;
  };

  bool parseSolarXml(QByteArray const& xml, SolarSnapshot * out, QString * error) const;
  QString buildHtml(SolarSnapshot const& snapshot) const;
  static QString normalizeText(QString const& text);
  static QString conditionColor(QString const& text);
  static QString displayBandName(QString const& band);
  static QString displayVhfName(QString const& name, QString const& location);
  void updateSunImage(QByteArray const& payload);
  void setStatus(QString const& status, bool error = false);
  void read_settings();
  void write_settings();

  QSettings * settings_ {nullptr};
  QLabel * statusLabel_ {nullptr};
  QLabel * sourceLabel_ {nullptr};
  QPushButton * refreshButton_ {nullptr};
  QTextBrowser * reportView_ {nullptr};
  QTimer refreshTimer_;
  QNetworkAccessManager * network_ {nullptr};
  SolarSnapshot lastSnapshot_;
  bool requestInFlight_ {false};
  bool hasSnapshot_ {false};
  bool hasSunImage_ {false};
  QString sunImageDataUri_;
};

#endif // IONOSPHERICFORECASTWINDOW_H
