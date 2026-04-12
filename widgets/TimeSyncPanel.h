// -*- Mode: C++ -*-
#ifndef TIMESYNCPANEL_H
#define TIMESYNCPANEL_H

#include <QDialog>
#include <QTimer>
#include <QVector>
#include <QDateTime>

class QSettings;
class NtpClient;

namespace Ui {
class TimeSyncPanel;
}

class TimeSyncPanel : public QDialog
{
  Q_OBJECT

public:
  explicit TimeSyncPanel(QSettings *settings, NtpClient *ntpClient, QWidget *parent = nullptr);
  ~TimeSyncPanel() override;

  void updateNtpOffset(double offsetMs, int serverCount);
  void updateNtpSyncStatus(bool synced, QString const& statusText);
  void updateSoundcardDrift(double driftMsPerPeriod, double driftPpm);
  void updateDecodeTiming(QVector<double> const& dtSamples,
                          double avgDt,
                          double dtCorrectionMs,
                          double decodeLatencyMs,
                          int sampleCount,
                          double driftPpm = 0.0,
                          int ntpDtDivergence = 0,
                          double emaFactor = 0.3);
  void syncNtpEnabled(bool enabled);
  void syncCustomServer(QString const& server);

Q_SIGNALS:
  void ntpEnableToggled(bool enabled);
  void customServerChanged(QString const& server);
  void syncNowRequested();

protected:
  void closeEvent(QCloseEvent *event) override;

private Q_SLOTS:
  void onClockTick();
  void on_btnSyncNow_clicked();
  void on_btnClearHistory_clicked();
  void on_cbNtpEnabled_toggled(bool checked);
  void on_editCustomServer_editingFinished();

private:
  struct OffsetEntry {
    QDateTime time;
    double offsetMs;
    int serverCount;
  };

  void read_settings();
  void write_settings();

  QSettings *settings_;
  NtpClient *ntpClient_;
  Ui::TimeSyncPanel *ui;
  QTimer clockTimer_;
  double lastOffsetMs_ {0.0};
  bool synced_ {false};
  QVector<OffsetEntry> history_;
  static constexpr int MAX_HISTORY = 100;

  // Decode timing stats
  double lastAvgDt_ {0.0};
  double lastDtCorrection_ {0.0};
  double lastDecodeLatency_ {0.0};

  // #8: Advanced metrics tracking
  QVector<double> correctionHistory_;   // last N correction values for trend
  int stablePeriodsCount_ {0};         // consecutive periods with |avgDt| < 0.1
  static constexpr int MAX_CORRECTION_HISTORY = 20;
};

#endif // TIMESYNCPANEL_H
