// -*- Mode: C++ -*-
#include "DXClusterWindow.h"

#include "SettingsGroup.hpp"
#include "WindowGeometryUtils.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QShowEvent>
#include <QHideEvent>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
QUrl const kSourcePage {"https://www.hamqth.com/developers.php#dxcluster"};
int const kRefreshMs = 15 * 1000;
int const kMaxSpots = 120;
}

DXClusterWindow::DXClusterWindow(QSettings * settings, QWidget * parent)
  : QDialog {parent}
  , settings_ {settings}
  , network_ {new QNetworkAccessManager {this}}
{
  setWindowTitle(QApplication::applicationName() + " - " + tr("DX Cluster"));
  setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);
  setMinimumSize(980, 620);
  resize(1180, 760);

  auto * root = new QVBoxLayout {this};
  root->setContentsMargins(10, 10, 10, 10);
  root->setSpacing(8);

  titleLabel_ = new QLabel {tr("DX Cluster Spots"), this};
  QFont titleFont = titleLabel_->font();
  titleFont.setBold(true);
  titleFont.setPointSizeF(titleFont.pointSizeF() + 1.5);
  titleLabel_->setFont(titleFont);

  refreshButton_ = new QPushButton {tr("Refresh"), this};
  auto * openSourceButton = new QPushButton {tr("Open Source Page"), this};

  auto * headerRow = new QHBoxLayout {};
  headerRow->addWidget(titleLabel_, 1);
  headerRow->addWidget(refreshButton_);
  headerRow->addWidget(openSourceButton);
  root->addLayout(headerRow);

  sourceLabel_ = new QLabel {tr("Source: %1").arg(kSourcePage.toString()), this};
  sourceLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  sourceLabel_->setWordWrap(true);
  root->addWidget(sourceLabel_);

  auto * controlsRow = new QHBoxLayout {};
  bandLabel_ = new QLabel {tr("Band: -"), this};
  followAppBandCheck_ = new QCheckBox {tr("Follow app band"), this};
  followAppBandCheck_->setChecked(true);
  bandFilter_ = new QComboBox {this};
  bandFilter_->addItems(QStringList {}
                        << "160M" << "80M" << "60M" << "40M" << "30M" << "20M"
                        << "17M" << "15M" << "12M" << "10M" << "6M" << "4M"
                        << "2M" << "70CM");
  bandFilter_->setEnabled(false);
  modeFilter_ = new QComboBox {this};
  modeFilter_->addItems(QStringList {}
                        << "All" << "FT8" << "FT4" << "FT2" << "Q65"
                        << "CW" << "SSB" << "RTTY" << "PSK"
                        << "JT65" << "JT9" << "FST4" << "MSK144"
                        << "AM" << "FM" << "Other");
  controlsRow->addWidget(bandLabel_);
  controlsRow->addSpacing(12);
  controlsRow->addWidget(followAppBandCheck_);
  controlsRow->addSpacing(12);
  controlsRow->addWidget(new QLabel {tr("Cluster band:"), this});
  controlsRow->addWidget(bandFilter_);
  controlsRow->addSpacing(12);
  controlsRow->addWidget(new QLabel {tr("Mode filter:"), this});
  controlsRow->addWidget(modeFilter_);
  controlsRow->addStretch(1);
  root->addLayout(controlsRow);

  table_ = new QTableWidget {this};
  table_->setColumnCount(10);
  table_->setHorizontalHeaderLabels(QStringList {}
                                    << tr("UTC")
                                    << tr("Freq (kHz)")
                                    << tr("DX Call")
                                    << tr("Spotter")
                                    << tr("Mode")
                                    << tr("Comment")
                                    << tr("Cont")
                                    << tr("Country")
                                    << tr("LoTW")
                                    << tr("eQSL"));
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setAlternatingRowColors(true);
  table_->verticalHeader()->setVisible(false);
  table_->horizontalHeader()->setStretchLastSection(false);
  table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
  table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(9, QHeaderView::ResizeToContents);
  root->addWidget(table_, 1);

  statusLabel_ = new QLabel {tr("Waiting for first update..."), this};
  statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  root->addWidget(statusLabel_);

  connect(refreshButton_, &QPushButton::clicked, this, &DXClusterWindow::refreshNow);
  connect(openSourceButton, &QPushButton::clicked, this, [] {
      QDesktopServices::openUrl(kSourcePage);
    });
  connect(bandFilter_, SIGNAL(currentIndexChanged(int)), this, SLOT(onBandFilterChanged(int)));
  connect(followAppBandCheck_, &QCheckBox::toggled, this, &DXClusterWindow::onFollowAppBandChanged);
  connect(modeFilter_, SIGNAL(currentIndexChanged(int)), this, SLOT(onModeFilterChanged(int)));
  connect(network_, &QNetworkAccessManager::finished, this, &DXClusterWindow::onNetworkFinished);
  connect(&refreshTimer_, &QTimer::timeout, this, &DXClusterWindow::refreshNow);

  refreshTimer_.setInterval(kRefreshMs);
  refreshTimer_.start();

  read_settings();
  if (width() < 1000 || height() < 620)
    {
      resize(qMax(width(), 1080), qMax(height(), 700));
    }
}

DXClusterWindow::~DXClusterWindow()
{
  write_settings();
}

void DXClusterWindow::closeEvent(QCloseEvent * event)
{
  write_settings();
  event->accept();
}

void DXClusterWindow::showEvent(QShowEvent * event)
{
  QDialog::showEvent(event);
  Q_EMIT windowVisibleChanged(isVisible());
}

void DXClusterWindow::hideEvent(QHideEvent * event)
{
  QDialog::hideEvent(event);
  Q_EMIT windowVisibleChanged(isVisible());
}

void DXClusterWindow::setStatus(QString const& status, bool error)
{
  if (error)
    {
      statusLabel_->setStyleSheet("QLabel{color:#b04040;}");
    }
  else
    {
      statusLabel_->setStyleSheet({});
    }
  statusLabel_->setText(status);
}

QString DXClusterWindow::normalizeBand(QString const& bandName)
{
  auto b = bandName.trimmed().toUpper();
  if (b.isEmpty() || b == "OOB")
    {
      return QString {};
    }
  if (!b.contains('M') && !b.contains("CM"))
    {
      b += "M";
    }
  return b;
}

QString DXClusterWindow::normalizeMode(QString const& comment)
{
  auto c = comment.simplified().toUpper();
  if (c.contains("FT8")) return "FT8";
  if (c.contains("FT4")) return "FT4";
  if (c.contains("FT2")) return "FT2";
  if (c.contains("Q65")) return "Q65";
  if (c.contains("FST4")) return "FST4";
  if (c.contains("JT65")) return "JT65";
  if (c.contains("JT9")) return "JT9";
  if (c.contains("MSK144") || c.contains(" MSK")) return "MSK144";
  if (c.contains("RTTY")) return "RTTY";
  if (c.contains("PSK")) return "PSK";
  if (c.contains("CW")) return "CW";
  if (c.contains("SSB") || c.contains("USB") || c.contains("LSB") || c.contains("PHONE")) return "SSB";
  if (c.contains(" AM")) return "AM";
  if (c.contains(" FM")) return "FM";
  return "Other";
}

QString DXClusterWindow::yesNoFlag(QString const& flag)
{
  auto f = flag.trimmed().toUpper();
  if (f == "L" || f == "E" || f == "Y" || f == "YES" || f == "1")
    {
      return "Yes";
    }
  return QString {};
}

bool DXClusterWindow::matchesModeFilter(Spot const& spot) const
{
  if (!modeFilter_)
    {
      return true;
    }
  auto selected = modeFilter_->currentText().trimmed().toUpper();
  if (selected.isEmpty() || selected == "ALL")
    {
      return true;
    }
  return spot.mode.toUpper() == selected;
}

bool DXClusterWindow::parsePayload(QByteArray const& payload, QVector<Spot> * out, QString * error) const
{
  if (!out)
    {
      if (error) *error = "Invalid output buffer";
      return false;
    }

  QVector<Spot> parsed;
  QString text = QString::fromUtf8(payload);
  auto lines = text.split(QRegularExpression {"[\\r\\n]+"}, Qt::SkipEmptyParts);
  for (auto const& rawLine : lines)
    {
      auto line = rawLine.trimmed();
      if (line.isEmpty())
        {
          continue;
        }
      if (line.startsWith("<") || line.startsWith("ERROR", Qt::CaseInsensitive))
        {
          continue;
        }

      auto fields = line.split('^');
      if (fields.size() < 10)
        {
          continue;
        }

      Spot spot;
      spot.spotter = fields.value(0).trimmed();
      spot.frequency = fields.value(1).trimmed();
      spot.dxCall = fields.value(2).trimmed();
      spot.comment = fields.value(3).trimmed();
      spot.dateTime = fields.value(4).trimmed();
      spot.lotw = fields.value(5).trimmed();
      spot.eqsl = fields.value(6).trimmed();
      spot.continent = fields.value(7).trimmed();
      spot.band = normalizeBand(fields.value(8).trimmed());
      spot.country = fields.value(9).trimmed();
      spot.adif = fields.value(10).trimmed();
      spot.mode = normalizeMode(spot.comment);
      parsed.push_back(spot);
    }

  if (parsed.isEmpty())
    {
      if (error) *error = "No spots returned by source";
      return false;
    }

  *out = parsed;
  return true;
}

void DXClusterWindow::rebuildTable()
{
  if (!table_)
    {
      return;
    }

  table_->setSortingEnabled(false);
  table_->setRowCount(0);

  for (auto const& spot : spots_)
    {
      if (!matchesModeFilter(spot))
        {
          continue;
        }

      int row = table_->rowCount();
      table_->insertRow(row);

      auto * dateItem = new QTableWidgetItem {spot.dateTime};
      auto * freqItem = new QTableWidgetItem {spot.frequency};
      auto * dxItem = new QTableWidgetItem {spot.dxCall};
      auto * spotterItem = new QTableWidgetItem {spot.spotter};
      auto * modeItem = new QTableWidgetItem {spot.mode};
      auto * commentItem = new QTableWidgetItem {spot.comment};
      auto * contItem = new QTableWidgetItem {spot.continent};
      auto * countryItem = new QTableWidgetItem {spot.country};
      auto * lotwItem = new QTableWidgetItem {yesNoFlag(spot.lotw)};
      auto * eqslItem = new QTableWidgetItem {yesNoFlag(spot.eqsl)};

      QFont dxFont = dxItem->font();
      dxFont.setBold(true);
      dxItem->setFont(dxFont);

      table_->setItem(row, 0, dateItem);
      table_->setItem(row, 1, freqItem);
      table_->setItem(row, 2, dxItem);
      table_->setItem(row, 3, spotterItem);
      table_->setItem(row, 4, modeItem);
      table_->setItem(row, 5, commentItem);
      table_->setItem(row, 6, contItem);
      table_->setItem(row, 7, countryItem);
      table_->setItem(row, 8, lotwItem);
      table_->setItem(row, 9, eqslItem);
    }
}

void DXClusterWindow::applyBand(QString const& bandName, bool refresh)
{
  auto normalized = normalizeBand(bandName);
  if (normalized.isEmpty())
    {
      currentBand_.clear();
      bandLabel_->setText(tr("Band: -"));
      titleLabel_->setText(tr("DX Cluster Spots"));
      spots_.clear();
      rebuildTable();
      setStatus(tr("DX cluster disabled: out of band"), true);
      return;
    }

  if (bandFilter_)
    {
      QSignalBlocker blocker {bandFilter_};
      int idx = bandFilter_->findText(normalized, Qt::MatchFixedString);
      if (idx < 0)
        {
          bandFilter_->addItem(normalized);
          idx = bandFilter_->findText(normalized, Qt::MatchFixedString);
        }
      if (idx >= 0)
        {
          bandFilter_->setCurrentIndex(idx);
        }
    }

  if (currentBand_ == normalized && !spots_.isEmpty())
    {
      if (refresh)
        {
          refreshNow();
        }
      return;
    }

  currentBand_ = normalized;
  bandLabel_->setText(tr("Band: %1").arg(currentBand_));
  titleLabel_->setText(tr("DX Cluster Spots - %1").arg(currentBand_));

  spots_.clear();
  rebuildTable();
  setStatus(tr("Band changed to %1: refreshing...").arg(currentBand_));
  if (refresh)
    {
      refreshNow();
    }
}

void DXClusterWindow::setBand(QString const& bandName, bool forceSyncToAppBand)
{
  appBand_ = normalizeBand(bandName);
  if (appBand_.isEmpty())
    {
      if (forceSyncToAppBand)
        {
          applyBand(QString {}, false);
        }
      return;
    }

  if (!forceSyncToAppBand && followAppBandCheck_ && !followAppBandCheck_->isChecked())
    {
      return;
    }

  applyBand(appBand_, true);
}

void DXClusterWindow::onBandFilterChanged(int)
{
  if (followAppBandCheck_ && followAppBandCheck_->isChecked())
    {
      return;
    }
  if (bandFilter_)
    {
      applyBand(bandFilter_->currentText(), true);
    }
}

void DXClusterWindow::onFollowAppBandChanged(bool checked)
{
  if (bandFilter_)
    {
      bandFilter_->setEnabled(!checked);
    }
  if (checked && !appBand_.isEmpty())
    {
      applyBand(appBand_, true);
    }
}

void DXClusterWindow::refreshNow()
{
  if (requestInFlight_)
    {
      return;
    }
  if (currentBand_.isEmpty())
    {
      setStatus(tr("No active band selected"), true);
      return;
    }

  requestInFlight_ = true;
  refreshButton_->setEnabled(false);

  auto url = QUrl {QString {"https://www.hamqth.com/dxc_csv.php?limit=%1&band=%2"}
                   .arg(kMaxSpots)
                   .arg(currentBand_)};
  setStatus(tr("Updating %1 ...").arg(url.toString()));

  QNetworkRequest request {url};
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    QString("%1/%2")
                    .arg(QApplication::applicationName(),
                         QApplication::applicationVersion().isEmpty() ? QStringLiteral("dev")
                                                                      : QApplication::applicationVersion()));
#if QT_VERSION >= QT_VERSION_CHECK (5, 9, 0)
  request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif

  auto * reply = network_->get(request);
  reply->setProperty("band", currentBand_);
}

void DXClusterWindow::onNetworkFinished(QNetworkReply * reply)
{
  if (!reply)
    {
      setStatus(tr("Update failed: empty network reply"), true);
      return;
    }

  auto replyBand = reply->property("band").toString();
  auto payload = reply->readAll();
  auto err = reply->error();
  auto errText = reply->errorString();
  reply->deleteLater();

  requestInFlight_ = false;
  refreshButton_->setEnabled(true);

  if (!replyBand.isEmpty() && replyBand != currentBand_)
    {
      return;
    }

  if (err != QNetworkReply::NoError)
    {
      setStatus(tr("Update failed: %1").arg(errText), true);
      return;
    }

  QVector<Spot> parsed;
  QString parseError;
  if (!parsePayload(payload, &parsed, &parseError))
    {
      setStatus(tr("Update failed: %1").arg(parseError), true);
      return;
    }

  spots_ = parsed;
  rebuildTable();

  int shown = table_ ? table_->rowCount() : 0;
  QString status = tr("Updated: %1 UTC | Spots: %2 | Band: %3")
                   .arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss"))
                   .arg(shown)
                   .arg(currentBand_);
  if (modeFilter_)
    {
      status += tr(" | Mode: %1").arg(modeFilter_->currentText());
    }
  setStatus(status);
}

void DXClusterWindow::onModeFilterChanged(int)
{
  rebuildTable();
  if (!currentBand_.isEmpty())
    {
      int shown = table_ ? table_->rowCount() : 0;
      setStatus(tr("Filter applied: %1 | Spots: %2 | Band: %3")
                .arg(modeFilter_->currentText())
                .arg(shown)
                .arg(currentBand_));
    }
}

void DXClusterWindow::read_settings()
{
  if (!settings_)
    {
      return;
    }
  SettingsGroup g {settings_, "DXClusterWindow"};
  move(settings_->value("window/pos", pos()).toPoint());
  auto geometry = settings_->value("geometry").toByteArray();
  if (!geometry.isEmpty())
    {
      WindowGeometryUtils::restore_window_geometry(this, geometry);
    }
  bool followAppBand = settings_->value("follow_app_band", true).toBool();
  auto savedBand = normalizeBand(settings_->value("cluster_band", QString {"20M"}).toString());
  if (!savedBand.isEmpty() && bandFilter_)
    {
      int idx = bandFilter_->findText(savedBand, Qt::MatchFixedString);
      if (idx >= 0)
        {
          QSignalBlocker blocker {bandFilter_};
          bandFilter_->setCurrentIndex(idx);
        }
    }
  followAppBandCheck_->setChecked(followAppBand);
  bandFilter_->setEnabled(!followAppBand);
  if (!followAppBand && !savedBand.isEmpty())
    {
      applyBand(savedBand, false);
    }
  auto savedMode = settings_->value("mode_filter", QString {"All"}).toString();
  int idx = modeFilter_->findText(savedMode);
  if (idx >= 0)
    {
      modeFilter_->setCurrentIndex(idx);
    }
}

void DXClusterWindow::write_settings()
{
  if (!settings_)
    {
      return;
    }
  SettingsGroup g {settings_, "DXClusterWindow"};
  settings_->setValue("window/pos", pos());
  settings_->setValue("geometry", saveGeometry());
  settings_->setValue("follow_app_band", followAppBandCheck_ ? followAppBandCheck_->isChecked() : true);
  settings_->setValue("cluster_band", currentBand_);
  settings_->setValue("mode_filter", modeFilter_ ? modeFilter_->currentText() : QString {"All"});
}
