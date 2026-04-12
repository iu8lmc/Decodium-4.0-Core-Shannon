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
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QShowEvent>
#include <QHideEvent>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <functional>

namespace
{
QString const kDefaultClusterHost {QStringLiteral ("iq8do.aricaserta.it")};
int const kDefaultClusterPort = 7300;
int const kRefreshMs = 15 * 1000;
int const kMaxSpots = 120;
qint64 const kMaxDxClusterReplyBytes = 256 * 1024;

QString configured_cluster_host (QSettings * settings)
{
  QString host {kDefaultClusterHost};
  int port {kDefaultClusterPort};
  if (settings)
    {
      auto configuredHost = settings->value (QStringLiteral ("DXClusterHost"), host).toString ().trimmed ();
      if (!configuredHost.isEmpty ())
        {
          host = configuredHost;
        }
      bool ok {false};
      auto configuredPort = settings->value (QStringLiteral ("DXClusterPort"), port).toInt (&ok);
      if (ok)
        {
          port = qBound (1, configuredPort, 65535);
        }
    }
  if ((host.compare (QStringLiteral ("www.hamqth.com"), Qt::CaseInsensitive) == 0 && port == 443)
      || (host.compare (QStringLiteral ("ik5pwj-6.dyndns.org"), Qt::CaseInsensitive) == 0 && port == 8000)
      || (host.compare (QStringLiteral ("dxc.sv5fri.eu"), Qt::CaseInsensitive) == 0 && port == 7300))
    {
      return kDefaultClusterHost;
    }
  return host;
}

int configured_cluster_port (QSettings * settings)
{
  int port {kDefaultClusterPort};
  if (settings)
    {
      bool ok {false};
      auto configuredPort = settings->value (QStringLiteral ("DXClusterPort"), port).toInt (&ok);
      if (ok)
        {
          port = qBound (1, configuredPort, 65535);
        }
    }
  auto const host = settings
      ? settings->value (QStringLiteral ("DXClusterHost"), kDefaultClusterHost).toString ().trimmed ()
      : QString {kDefaultClusterHost};
  if ((host.compare (QStringLiteral ("www.hamqth.com"), Qt::CaseInsensitive) == 0 && port == 443)
      || (host.compare (QStringLiteral ("ik5pwj-6.dyndns.org"), Qt::CaseInsensitive) == 0 && port == 8000)
      || (host.compare (QStringLiteral ("dxc.sv5fri.eu"), Qt::CaseInsensitive) == 0 && port == 7300))
    {
      return kDefaultClusterPort;
    }
  return port;
}

QString configured_cluster_endpoint (QSettings * settings)
{
  return QStringLiteral ("%1:%2")
    .arg (configured_cluster_host (settings), QString::number (configured_cluster_port (settings)));
}

QString configured_cluster_login (QSettings * settings, QString const& activeMyCall)
{
  if (settings)
    {
      auto configuredLogin = settings->value (QStringLiteral ("DXClusterViewLogin"), QString {}).toString ().trimmed ().toUpper ();
      if (!configuredLogin.isEmpty ())
        {
          return configuredLogin;
        }
    }
  auto login = activeMyCall.trimmed ().toUpper ();
  if (!login.isEmpty ())
    {
      return login;
    }
  if (!settings) return {};
  return settings->value (QStringLiteral ("MyCall"), QString {}).toString ().trimmed ().toUpper ();
}

QString spider_band_argument (QString const& band)
{
  return band.trimmed ().toLower ();
}

QString feed_summary_text (QSettings * settings)
{
  return QObject::tr ("Cluster feed + AutoSpot submit: %1").arg (configured_cluster_endpoint (settings));
}

bool contains_cluster_prompt (QByteArray const& buffer)
{
  auto const text = QString::fromLatin1 (buffer).simplified ();
  auto const lower = text.toLower ();
  return lower.contains (QStringLiteral ("login:"))
      || lower.contains (QStringLiteral ("call:"))
      || lower.contains (QStringLiteral ("callsign"))
      || lower.contains (QStringLiteral ("enter your"))
      || lower.contains (QStringLiteral ("please enter"))
      || text.endsWith (QLatin1Char (':'))
      || text.endsWith (QLatin1Char ('>'))
      || (lower.contains (QStringLiteral (" de ")) && text.endsWith (QLatin1Char ('>')));
}

bool contains_cluster_error_payload (QByteArray const& buffer)
{
  auto const lower = QString::fromUtf8 (buffer).toLower ();
  return lower.contains (QStringLiteral ("need a callsign"))
      || lower.contains (QStringLiteral ("usage:"))
      || lower.contains (QStringLiteral ("invalid callsign"))
      || lower.contains (QStringLiteral ("unknown command"))
      || lower.contains (QStringLiteral ("sorry"));
}

bool contains_cluster_spot_payload (QByteArray const& buffer)
{
  if (buffer.contains ("DX de "))
    {
      return true;
    }

  static auto const snapshotLineRe = QRegularExpression {
    QStringLiteral (R"((?:^|[\r\n])\s*[0-9.]+\s+\S+\s+\d{2}-[A-Za-z]{3}-\d{4}\s+[0-9]{4}Z)")
  };
  return snapshotLineRe.match (QString::fromUtf8 (buffer)).hasMatch ();
}

void append_cluster_payload (QTcpSocket * socket, QByteArray * telnet_pending, QByteArray * buffer, QByteArray raw)
{
  if (!socket || !buffer) return;

  if (telnet_pending && !telnet_pending->isEmpty ())
    {
      raw.prepend (*telnet_pending);
      telnet_pending->clear ();
    }

  for (int i = 0; i < raw.size ();)
    {
      auto const byte = static_cast<unsigned char> (raw.at (i));
      if (byte != 0xFF)
        {
          buffer->append (raw.at (i));
          ++i;
          continue;
        }

      if (i + 1 >= raw.size ())
        {
          if (telnet_pending) *telnet_pending = raw.mid (i);
          break;
        }

      auto const cmd = static_cast<unsigned char> (raw.at (i + 1));
      if (cmd == 0xFF)
        {
          buffer->append (char (0xFF));
          i += 2;
          continue;
        }

      if (cmd == 0xFA)
        {
          int end = -1;
          for (int j = i + 2; j + 1 < raw.size (); ++j)
            {
              if (static_cast<unsigned char> (raw.at (j)) == 0xFF
                  && static_cast<unsigned char> (raw.at (j + 1)) == 0xF0)
                {
                  end = j + 2;
                  break;
                }
            }
          if (end < 0)
            {
              if (telnet_pending) *telnet_pending = raw.mid (i);
              break;
            }
          i = end;
          continue;
        }

      if (cmd >= 0xFB && cmd <= 0xFE)
        {
          if (i + 2 >= raw.size ())
            {
              if (telnet_pending) *telnet_pending = raw.mid (i);
              break;
            }

          auto const opt = static_cast<unsigned char> (raw.at (i + 2));
          char reply[3] = {char (0xFF), 0, char (opt)};
          bool const accepted = (opt == 1 || opt == 3);

          if (cmd == 0xFB)
            {
              reply[1] = accepted ? char (0xFD) : char (0xFE);
            }
          else if (cmd == 0xFD)
            {
              reply[1] = accepted ? char (0xFB) : char (0xFC);
            }
          else
            {
              i += 3;
              continue;
            }

          socket->write (reply, 3);
          socket->flush ();
          i += 3;
          continue;
        }

      i += 2;
    }
}

bool wait_for_cluster_condition (QTcpSocket * socket, QByteArray * buffer, QByteArray * telnet_pending, int total_ms,
                                 std::function<bool (QByteArray const&)> const& done)
{
  if (!socket || !buffer || !done) return false;

  QElapsedTimer totalTimer;
  totalTimer.start ();

  while (totalTimer.elapsed () < total_ms)
    {
      auto const remaining = qMax (0, total_ms - int (totalTimer.elapsed ()));
      if (socket->bytesAvailable () > 0 || socket->waitForReadyRead (qMin (250, remaining)))
        {
          append_cluster_payload (socket, telnet_pending, buffer, socket->readAll ());
          if (done (*buffer))
            {
              return true;
            }
          continue;
        }
      if (done (*buffer))
        {
          return true;
        }
    }

  if (socket->bytesAvailable () > 0)
    {
      append_cluster_payload (socket, telnet_pending, buffer, socket->readAll ());
    }
  return done (*buffer);
}

bool wait_for_cluster_quiet (QTcpSocket * socket, QByteArray * buffer, QByteArray * telnet_pending, int total_ms, int idle_ms)
{
  if (!socket || !buffer) return false;

  QElapsedTimer totalTimer;
  QElapsedTimer idleTimer;
  totalTimer.start ();
  idleTimer.start ();

  while (totalTimer.elapsed () < total_ms)
    {
      if (socket->bytesAvailable () > 0 || socket->waitForReadyRead (qMin (250, total_ms - int (totalTimer.elapsed ()))))
        {
          append_cluster_payload (socket, telnet_pending, buffer, socket->readAll ());
          idleTimer.restart ();
          continue;
        }
      if (!buffer->isEmpty () && idleTimer.elapsed () >= idle_ms)
        {
          return true;
        }
    }

  if (socket->bytesAvailable () > 0)
    {
      append_cluster_payload (socket, telnet_pending, buffer, socket->readAll ());
    }
  return !buffer->isEmpty ();
}

QByteArray fetch_cluster_snapshot (QSettings * settings, QString const& activeMyCall, QString const& band, QString * error)
{
  auto const host = configured_cluster_host (settings).trimmed ();
  auto const port = configured_cluster_port (settings);
  auto const login = configured_cluster_login (settings, activeMyCall);
  if (host.isEmpty ())
    {
      if (error) *error = QObject::tr ("empty cluster host");
      return {};
    }
  if (login.isEmpty ())
    {
      if (error) *error = QObject::tr ("empty MyCall setting");
      return {};
    }

  QTcpSocket socket;
  socket.connectToHost (host, static_cast<quint16> (port));
  if (!socket.waitForConnected (4000))
    {
      if (error) *error = socket.errorString ();
      return {};
    }

  QByteArray buffer;
  QByteArray telnet_pending;
  wait_for_cluster_condition (&socket, &buffer, &telnet_pending, 2500, contains_cluster_prompt);

  socket.write ((login + QStringLiteral ("\r\n")).toUtf8 ());
  socket.flush ();
  buffer.clear ();
  if (!wait_for_cluster_condition (&socket, &buffer, &telnet_pending, 3500, contains_cluster_prompt))
    {
      if (error) *error = QObject::tr ("cluster login prompt not received");
      socket.abort ();
      return {};
    }

  socket.write (QByteArrayLiteral ("set/page 0\r\n"));
  socket.flush ();
  buffer.clear ();
  if (!wait_for_cluster_condition (&socket, &buffer, &telnet_pending, 1500, contains_cluster_prompt))
    {
      if (error) *error = QObject::tr ("cluster page setup failed");
      socket.abort ();
      return {};
    }

  socket.write (QByteArrayLiteral ("unset/dx\r\n"));
  socket.flush ();
  buffer.clear ();
  if (!wait_for_cluster_condition (&socket, &buffer, &telnet_pending, 1500, contains_cluster_prompt))
    {
      if (error) *error = QObject::tr ("cluster quiet mode setup failed");
      socket.abort ();
      return {};
    }

  buffer.clear ();
  socket.write (QStringLiteral ("show/dx %1 on %2 real\r\n")
                  .arg (QString::number (kMaxSpots), spider_band_argument (band)).toUtf8 ());
  socket.flush ();
  wait_for_cluster_condition (&socket, &buffer, &telnet_pending, 4500,
                              [] (QByteArray const& data)
                              {
                                return contains_cluster_spot_payload (data)
                                    || contains_cluster_error_payload (data);
                              });
  wait_for_cluster_quiet (&socket, &buffer, &telnet_pending, 2500, 900);
  if (!contains_cluster_spot_payload (buffer) && error && error->isEmpty ())
    {
      *error = QObject::tr ("cluster returned no data");
    }

  socket.write (QByteArrayLiteral ("bye\r\n"));
  socket.flush ();
  socket.disconnectFromHost ();
  socket.waitForDisconnected (500);

  if (buffer.size () > kMaxDxClusterReplyBytes)
    {
      buffer = buffer.left (kMaxDxClusterReplyBytes);
    }
  return buffer;
}

QString band_from_frequency_khz (QString const& frequencyText)
{
  bool ok {false};
  auto const khz = frequencyText.trimmed ().toDouble (&ok);
  if (!ok || khz <= 0.0)
    {
      return {};
    }

  if (khz >= 1800.0 && khz <= 2000.0) return QStringLiteral ("160M");
  if (khz >= 3500.0 && khz <= 4000.0) return QStringLiteral ("80M");
  if (khz >= 5300.0 && khz <= 5407.0) return QStringLiteral ("60M");
  if (khz >= 7000.0 && khz <= 7300.0) return QStringLiteral ("40M");
  if (khz >= 10100.0 && khz <= 10150.0) return QStringLiteral ("30M");
  if (khz >= 14000.0 && khz <= 14350.0) return QStringLiteral ("20M");
  if (khz >= 18068.0 && khz <= 18168.0) return QStringLiteral ("17M");
  if (khz >= 21000.0 && khz <= 21450.0) return QStringLiteral ("15M");
  if (khz >= 24890.0 && khz <= 24990.0) return QStringLiteral ("12M");
  if (khz >= 28000.0 && khz <= 29700.0) return QStringLiteral ("10M");
  if (khz >= 50000.0 && khz <= 54000.0) return QStringLiteral ("6M");
  if (khz >= 70000.0 && khz <= 71000.0) return QStringLiteral ("4M");
  if (khz >= 144000.0 && khz <= 148000.0) return QStringLiteral ("2M");
  if (khz >= 420000.0 && khz <= 450000.0) return QStringLiteral ("70CM");
  return {};
}

QByteArray read_reply_payload_limited (QNetworkReply * reply, qint64 max_bytes, QString * error = nullptr)
{
  if (!reply)
    {
      if (error) *error = QObject::tr ("empty reply");
      return {};
    }

  auto const length_header = reply->header (QNetworkRequest::ContentLengthHeader);
  if (length_header.isValid () && length_header.toLongLong () > max_bytes)
    {
      if (error) *error = QObject::tr ("reply too large");
      return {};
    }

  auto const payload = reply->read (max_bytes + 1);
  if (payload.size () > max_bytes || !reply->atEnd ())
    {
      if (error) *error = QObject::tr ("reply exceeds limit");
      return {};
    }

  return payload;
}
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
  auto * openSourceButton = new QPushButton {tr("Open Cluster Docs"), this};

  auto * headerRow = new QHBoxLayout {};
  headerRow->addWidget(titleLabel_, 1);
  headerRow->addWidget(refreshButton_);
  headerRow->addWidget(openSourceButton);
  root->addLayout(headerRow);

  sourceLabel_ = new QLabel {feed_summary_text (settings_), this};
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
  table_->setWordWrap(false);
  table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  table_->verticalHeader()->setVisible(false);
  auto * header = table_->horizontalHeader();
  header->setStretchLastSection(false);
  header->setSectionsMovable(false);
  header->setMinimumSectionSize(40);
  header->setDefaultSectionSize(100);
  header->setSectionResizeMode(QHeaderView::Interactive);
  applyDefaultColumnWidths();
  root->addWidget(table_, 1);

  statusLabel_ = new QLabel {tr("Waiting for first update..."), this};
  statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  root->addWidget(statusLabel_);

  connect(refreshButton_, &QPushButton::clicked, this, &DXClusterWindow::refreshNow);
  connect(openSourceButton, &QPushButton::clicked, this, [] {
      QDesktopServices::openUrl(QUrl {QStringLiteral ("https://dxspider.org/")});
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

void DXClusterWindow::applyDefaultColumnWidths()
{
  if (!table_)
    {
      return;
    }

  table_->setColumnWidth(0, 140);
  table_->setColumnWidth(1, 95);
  table_->setColumnWidth(2, 95);
  table_->setColumnWidth(3, 80);
  table_->setColumnWidth(4, 60);
  table_->setColumnWidth(5, 460);
  table_->setColumnWidth(6, 55);
  table_->setColumnWidth(7, 150);
  table_->setColumnWidth(8, 55);
  table_->setColumnWidth(9, 55);
}

void DXClusterWindow::setMyCall(QString const& myCall)
{
  myCall_ = myCall.trimmed ().toUpper ();
}

void DXClusterWindow::suspendRefresh(int ms)
{
  refreshTimer_.stop();
  if (ms > 0)
    {
      QTimer::singleShot(ms, this, [this] {
          refreshTimer_.start();
        });
    }
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
  auto const nowUtc = QDateTime::currentDateTimeUtc ();
  auto const realTimeRe = QRegularExpression {
    QStringLiteral (R"(^DX de\s+([^:]+):\s+([0-9.]+)\s+(\S+)\s*(.*?)\s+([0-9]{4})Z\s*$)")
  };
  auto const snapshotRe = QRegularExpression {
    QStringLiteral (R"(^\s*([0-9.]+)\s+(\S+)\s+(\d{2}-[A-Za-z]{3}-\d{4})\s+([0-9]{4})Z\s*(.*?)\s+<([^>]+)>\s*$)")
  };
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

      Spot spot;
      auto match = realTimeRe.match (line);
      if (match.hasMatch ())
        {
          auto const hhmm = match.captured (5);
          auto dt = QDateTime (nowUtc.date (),
                               QTime::fromString (hhmm, QStringLiteral ("hhmm")),
                               Qt::UTC);
          if (dt.isValid () && dt > nowUtc.addSecs (300))
            {
              dt = dt.addDays (-1);
            }
          spot.spotter = match.captured (1).trimmed();
          spot.frequency = match.captured (2).trimmed();
          spot.dxCall = match.captured (3).trimmed();
          spot.comment = match.captured (4).trimmed();
          spot.dateTime = dt.isValid () ? dt.toString (QStringLiteral ("yyyy-MM-dd HH:mm")) : hhmm;
          spot.band = band_from_frequency_khz (spot.frequency);
          spot.mode = normalizeMode (spot.comment);
          if (!currentBand_.isEmpty () && spot.band != currentBand_)
            {
              continue;
            }
          parsed.push_back (spot);
          continue;
        }

      match = snapshotRe.match (line);
      if (match.hasMatch ())
        {
          auto const date = QLocale::c ().toDate (match.captured (3), QStringLiteral ("dd-MMM-yyyy"));
          auto const time = QTime::fromString (match.captured (4), QStringLiteral ("hhmm"));
          auto dt = QDateTime (date, time, Qt::UTC);
          spot.frequency = match.captured (1).trimmed();
          spot.dxCall = match.captured (2).trimmed();
          spot.dateTime = dt.isValid () ? dt.toString (QStringLiteral ("yyyy-MM-dd HH:mm")) : line;
          spot.comment = match.captured (5).trimmed();
          spot.spotter = match.captured (6).trimmed();
          spot.band = band_from_frequency_khz (spot.frequency);
          spot.mode = normalizeMode (spot.comment);
          if (!currentBand_.isEmpty () && spot.band != currentBand_)
            {
              continue;
            }
          parsed.push_back (spot);
        }
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
      if (!currentBand_.isEmpty () && !spot.band.isEmpty () && spot.band != currentBand_)
        {
          continue;
        }
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
  sourceLabel_->setText (feed_summary_text (settings_));
  setStatus(tr("Updating %1 ...").arg(configured_cluster_endpoint (settings_)));

  QString fetchError;
  auto payload = fetch_cluster_snapshot (settings_, myCall_, currentBand_, &fetchError);

  requestInFlight_ = false;
  refreshButton_->setEnabled(true);

  if (!fetchError.isEmpty() && payload.isEmpty())
    {
      setStatus(tr("Update failed: %1").arg(fetchError), true);
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
  QString status = tr("Updated: %1 UTC | Spots: %2 | Band: %3 | Node: %4")
                   .arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss"))
                   .arg(shown)
                   .arg(currentBand_)
                   .arg(configured_cluster_endpoint (settings_));
  if (modeFilter_)
    {
      status += tr(" | Mode: %1").arg(modeFilter_->currentText());
    }
  setStatus(status);
}

void DXClusterWindow::onNetworkFinished(QNetworkReply * reply)
{
  if (!reply)
    {
      setStatus(tr("Update failed: empty network reply"), true);
      return;
  }

  auto replyBand = reply->property("band").toString();
  auto err = reply->error();
  auto errText = reply->errorString();
  QString payloadError;
  auto payload = (err == QNetworkReply::NoError)
      ? read_reply_payload_limited(reply, kMaxDxClusterReplyBytes, &payloadError)
      : QByteArray {};
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
  if (!payloadError.isEmpty())
    {
      setStatus(tr("Update failed: %1").arg(payloadError), true);
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
  auto headerState = settings_->value("header_state").toByteArray();
  if (!headerState.isEmpty() && table_ && table_->horizontalHeader())
    {
      table_->horizontalHeader()->restoreState(headerState);
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
  settings_->setValue("header_state", table_ && table_->horizontalHeader()
                                     ? table_->horizontalHeader()->saveState()
                                     : QByteArray {});
  settings_->setValue("follow_app_band", followAppBandCheck_ ? followAppBandCheck_->isChecked() : true);
  settings_->setValue("cluster_band", currentBand_);
  settings_->setValue("mode_filter", modeFilter_ ? modeFilter_->currentText() : QString {"All"});
}
