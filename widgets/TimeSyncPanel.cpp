// -*- Mode: C++ -*-
#include "TimeSyncPanel.h"
#include "ui_TimeSyncPanel.h"
#include "Network/NtpClient.hpp"
#include "PrecisionTime.hpp"
#include "WindowGeometryUtils.hpp"

#include <QSettings>
#include <QCloseEvent>
#include <QListWidgetItem>
#include <cmath>

TimeSyncPanel::TimeSyncPanel(QSettings *settings, NtpClient *ntpClient, QWidget *parent)
  : QDialog{parent}
  , settings_{settings}
  , ntpClient_{ntpClient}
  , ui{new Ui::TimeSyncPanel}
{
  ui->setupUi(this);
  setWindowTitle("Time Synchronization");
  setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);

  // Sync initial state from NtpClient
  if (ntpClient_) {
    lastOffsetMs_ = ntpClient_->offsetMs();
    synced_ = ntpClient_->isSynced();
    ui->cbNtpEnabled->setChecked(true);  // will be overridden by syncNtpEnabled()
  }

  // Clock timer — update every 100ms
  connect(&clockTimer_, &QTimer::timeout, this, &TimeSyncPanel::onClockTick);
  clockTimer_.start(100);

  // Initial clock display
  onClockTick();

  read_settings();
}

TimeSyncPanel::~TimeSyncPanel()
{
  write_settings();
  delete ui;
}

void TimeSyncPanel::onClockTick()
{
  // System raw time
  qint64 sysMs = preciseCurrentMSecsSinceEpoch();
  QDateTime sysUtc = QDateTime::fromMSecsSinceEpoch(sysMs, Qt::UTC);

  // NTP-corrected time
  qint64 correctedMs = sysMs - qRound64(lastOffsetMs_);
  QDateTime correctedUtc = QDateTime::fromMSecsSinceEpoch(correctedMs, Qt::UTC);

  int fracMs = static_cast<int>(correctedMs % 1000);
  if (fracMs < 0) fracMs += 1000;

  ui->lblUtcClock->setText(
    correctedUtc.toString("hh:mm:ss") +
    QString(".%1 UTC").arg(fracMs, 3, 10, QLatin1Char('0')));

  ui->lblUtcDate->setText(correctedUtc.toString("yyyy-MM-dd"));

  int sysFracMs = static_cast<int>(sysMs % 1000);
  if (sysFracMs < 0) sysFracMs += 1000;
  ui->lblSystemClock->setText(
    QString("System: %1.%2")
    .arg(sysUtc.toString("hh:mm:ss"))
    .arg(sysFracMs, 3, 10, QLatin1Char('0')));
}

void TimeSyncPanel::updateNtpOffset(double offsetMs, int serverCount)
{
  lastOffsetMs_ = offsetMs;

  int rounded = qRound(offsetMs);
  QString text = QString("%1%2 ms  (%3 servers)")
    .arg(rounded > 0 ? "+" : "")
    .arg(rounded)
    .arg(serverCount);
  ui->lblNtpOffset->setText(text);

  // Color-code the offset
  if (qAbs(offsetMs) < 20.0)
    ui->lblNtpOffset->setStyleSheet("color: #00ff00; font-weight: bold;");
  else if (qAbs(offsetMs) < 100.0)
    ui->lblNtpOffset->setStyleSheet("color: #ffff00; font-weight: bold;");
  else
    ui->lblNtpOffset->setStyleSheet("color: #ff4444; font-weight: bold;");

  // Update last sync time
  QDateTime now = QDateTime::currentDateTimeUtc();
  ui->lblLastSync->setText(now.toString("hh:mm:ss") + " UTC");

  // Add to history
  OffsetEntry entry;
  entry.time = now;
  entry.offsetMs = offsetMs;
  entry.serverCount = serverCount;
  history_.prepend(entry);
  if (history_.size() > MAX_HISTORY) {
    history_.resize(MAX_HISTORY);
  }

  // Update history list widget
  QString histLine = QString("%1  %2%3 ms  (%4 srv)")
    .arg(now.toString("hh:mm:ss"))
    .arg(rounded > 0 ? "+" : "")
    .arg(rounded)
    .arg(serverCount);

  auto *item = new QListWidgetItem(histLine);
  if (qAbs(offsetMs) < 20.0)
    item->setForeground(QColor("#00ff00"));
  else if (qAbs(offsetMs) < 100.0)
    item->setForeground(QColor("#ffff00"));
  else
    item->setForeground(QColor("#ff4444"));

  ui->listHistory->insertItem(0, item);
  if (ui->listHistory->count() > MAX_HISTORY) {
    delete ui->listHistory->takeItem(ui->listHistory->count() - 1);
  }
}

void TimeSyncPanel::updateNtpSyncStatus(bool synced, QString const& statusText)
{
  synced_ = synced;
  if (synced) {
    ui->lblNtpStatus->setText("Synced");
    ui->lblNtpStatus->setStyleSheet("color: #00ff00; font-weight: bold;");
  } else {
    ui->lblNtpStatus->setText("Not synced");
    ui->lblNtpStatus->setStyleSheet("color: #ff4444; font-weight: bold;");
  }
  ui->lblNtpStatus->setToolTip(statusText);
}

void TimeSyncPanel::updateSoundcardDrift(double driftMsPerPeriod, double driftPpm)
{
  // UI updates removed per user request
  (void)driftMsPerPeriod;
  (void)driftPpm;
}

void TimeSyncPanel::updateDecodeTiming(QVector<double> const& dtSamples,
                                        double avgDt,
                                        double dtCorrectionMs,
                                        double decodeLatencyMs,
                                        int sampleCount,
                                        double driftPpm,
                                        int ntpDtDivergence,
                                        double emaFactor)
{
  (void)driftPpm;
  lastAvgDt_ = avgDt;
  lastDtCorrection_ = dtCorrectionMs;
  lastDecodeLatency_ = decodeLatencyMs;

  // Avg DT with color (green if <0.3s, yellow <0.5s, red >=0.5s)
  ui->lblAvgDt->setText(QString("%1%2 s")
    .arg(avgDt > 0 ? "+" : "").arg(avgDt, 0, 'f', 3));
  if (qAbs(avgDt) < 0.3)
    ui->lblAvgDt->setStyleSheet("color:#00ff00;");
  else if (qAbs(avgDt) < 0.5)
    ui->lblAvgDt->setStyleSheet("color:#ffff00;");
  else
    ui->lblAvgDt->setStyleSheet("color:#ff4444;");

  // DT Correction with trend indicator
  correctionHistory_.append(dtCorrectionMs);
  if (correctionHistory_.size() > MAX_CORRECTION_HISTORY)
    correctionHistory_.removeFirst();
  QString trend;
  if (correctionHistory_.size() >= 3) {
    double recent = correctionHistory_.last();
    double older = correctionHistory_[correctionHistory_.size() - 3];
    double delta = recent - older;
    if (delta > 2.0) trend = " ^";      // increasing
    else if (delta < -2.0) trend = " v"; // decreasing
    else trend = " =";                   // stable
  }
  ui->lblDtCorrection->setText(QString("%1%2 ms%3")
    .arg(dtCorrectionMs > 0 ? "+" : "").arg(dtCorrectionMs, 0, 'f', 1).arg(trend));

  // Decode count
  ui->lblDecodeCount->setText(QString::number(sampleCount));

  // Decode latency with color
  ui->lblDecodeLatency->setText(QString("%1 ms").arg(decodeLatencyMs, 0, 'f', 0));
  if (decodeLatencyMs < 2000)
    ui->lblDecodeLatency->setStyleSheet("color:#00ff00;");
  else if (decodeLatencyMs < 5000)
    ui->lblDecodeLatency->setStyleSheet("color:#ffff00;");
  else
    ui->lblDecodeLatency->setStyleSheet("color:#ff4444;");

  // Last DTs list
  QStringList dtList;
  for (auto dt : dtSamples) {
    dtList << QString("%1%2").arg(dt > 0 ? "+" : "").arg(dt, 0, 'f', 2);
  }
  ui->lblLastDts->setText(dtList.join(", "));

  // #8: Convergence indicator — green if stable for 3+ periods
  if (qAbs(avgDt) < 0.1 && sampleCount > 0) {
    stablePeriodsCount_++;
  } else {
    stablePeriodsCount_ = 0;
  }
  if (stablePeriodsCount_ >= 3) {
    ui->lblConvergence->setText("LOCKED");
    ui->lblConvergence->setStyleSheet("color:#00ff00; font-weight:bold;");
  } else if (stablePeriodsCount_ >= 1) {
    ui->lblConvergence->setText("Converging...");
    ui->lblConvergence->setStyleSheet("color:#ffff00;");
  } else if (sampleCount > 0) {
    ui->lblConvergence->setText("Adjusting");
    ui->lblConvergence->setStyleSheet("color:#ff8800;");
  } else {
    ui->lblConvergence->setText("No data");
    ui->lblConvergence->setStyleSheet("color:#888;");
  }

  // #8: EMA factor display (shows adaptive state)
  QString emaState;
  if (emaFactor >= 0.25) emaState = " (warm-up)";
  else if (emaFactor <= 0.12) emaState = " (stable)";
  else emaState = " (tracking)";
  ui->lblEmaFactor->setText(QString::number(emaFactor, 'f', 2) + emaState);

  // #8: Soundcard drift compensation display - Removed per user request
  /*
  if (qAbs(driftPpm) > 0.1) {
    ui->lblDriftApplied->setText(QString("%1%2 ppm (active)")
      .arg(driftPpm > 0 ? "+" : "").arg(driftPpm, 0, 'f', 1));
    ui->lblDriftApplied->setStyleSheet("color:#00ff00;");
  } else {
    ui->lblDriftApplied->setText("< 0.1 ppm (idle)");
    ui->lblDriftApplied->setStyleSheet("color:#888;");
  }
  */

  // #8: Warning display for NTP vs DT divergence
  if (ntpDtDivergence >= 5) {
    ui->lblWarning->setText("NTP/DT diverging! Check clock source.");
    ui->lblWarning->setStyleSheet("color:#ff4444; font-weight:bold;");
  } else if (ntpDtDivergence >= 3) {
    ui->lblWarning->setText("NTP/DT mismatch detected");
    ui->lblWarning->setStyleSheet("color:#ffff00;");
  } else {
    ui->lblWarning->setText("None");
    ui->lblWarning->setStyleSheet("color:#00ff00;");
  }
}

void TimeSyncPanel::syncNtpEnabled(bool enabled)
{
  ui->cbNtpEnabled->blockSignals(true);
  ui->cbNtpEnabled->setChecked(enabled);
  ui->cbNtpEnabled->blockSignals(false);
}

void TimeSyncPanel::syncCustomServer(QString const& server)
{
  ui->editCustomServer->blockSignals(true);
  ui->editCustomServer->setText(server);
  ui->editCustomServer->blockSignals(false);
}

void TimeSyncPanel::on_btnSyncNow_clicked()
{
  Q_EMIT syncNowRequested();
}

void TimeSyncPanel::on_btnClearHistory_clicked()
{
  history_.clear();
  ui->listHistory->clear();
}

void TimeSyncPanel::on_cbNtpEnabled_toggled(bool checked)
{
  Q_EMIT ntpEnableToggled(checked);
}

void TimeSyncPanel::on_editCustomServer_editingFinished()
{
  Q_EMIT customServerChanged(ui->editCustomServer->text().trimmed());
}

void TimeSyncPanel::closeEvent(QCloseEvent *event)
{
  write_settings();
  QDialog::closeEvent(event);
}

void TimeSyncPanel::read_settings()
{
  settings_->beginGroup("TimeSyncPanel");
  WindowGeometryUtils::restore_window_geometry(this, settings_->value("geometry").toByteArray());
  settings_->endGroup();
}

void TimeSyncPanel::write_settings()
{
  settings_->beginGroup("TimeSyncPanel");
  settings_->setValue("geometry", saveGeometry());
  settings_->endGroup();
}
