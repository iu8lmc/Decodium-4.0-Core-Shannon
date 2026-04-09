// -*- Mode: C++ -*-
#include "IonosphericForecastWindow.h"

#include "SettingsGroup.hpp"
#include "WindowGeometryUtils.hpp"

#include <QApplication>
#include <QBuffer>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSettings>
#include <QShowEvent>
#include <QHideEvent>
#include <QTextBrowser>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>
#include <QXmlStreamReader>
#include <QPixmap>

namespace
{
QUrl const kSolarXmlUrl {"https://www.hamqsl.com/solarxml.php"};
QUrl const kSolarHtmlUrl {"https://www.hamqsl.com/solar.html"};
QUrl const kSolarSunUrl {"https://www.hamqsl.com/solarsun.php"};
int const kRefreshMs = 15 * 60 * 1000;
qint64 const kMaxSolarXmlReplyBytes = 256 * 1024;
qint64 const kMaxSolarImageReplyBytes = 2 * 1024 * 1024;

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

IonosphericForecastWindow::IonosphericForecastWindow(QSettings * settings, QWidget * parent)
  : QDialog {parent}
  , settings_ {settings}
  , network_ {new QNetworkAccessManager {this}}
{
  setWindowTitle(QApplication::applicationName() + " - " + tr("Ionospheric Forecast"));
  setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);
  setMinimumSize(980, 700);
  resize(1180, 800);

  auto * root = new QVBoxLayout {this};
  root->setContentsMargins(10, 10, 10, 10);
  root->setSpacing(8);

  auto * titleLabel = new QLabel {tr("HamQSL Solar / Ionospheric Conditions"), this};
  QFont titleFont = titleLabel->font();
  titleFont.setBold(true);
  titleFont.setPointSizeF(titleFont.pointSizeF() + 1.5);
  titleLabel->setFont(titleFont);

  refreshButton_ = new QPushButton {tr("Refresh"), this};
  auto * openSourceButton = new QPushButton {tr("Open Source Page"), this};

  auto * headerRow = new QHBoxLayout {};
  headerRow->addWidget(titleLabel, 1);
  headerRow->addWidget(refreshButton_);
  headerRow->addWidget(openSourceButton);
  root->addLayout(headerRow);

  sourceLabel_ = new QLabel {tr("Source: %1").arg(kSolarXmlUrl.toString()), this};
  sourceLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  sourceLabel_->setWordWrap(true);
  root->addWidget(sourceLabel_);

  reportView_ = new QTextBrowser {this};
  reportView_->setReadOnly(true);
  reportView_->setOpenExternalLinks(true);
  reportView_->setMinimumWidth(900);
  root->addWidget(reportView_, 1);

  statusLabel_ = new QLabel {tr("Waiting for first update..."), this};
  statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  root->addWidget(statusLabel_);

  connect(refreshButton_, &QPushButton::clicked, this, &IonosphericForecastWindow::refreshNow);
  connect(openSourceButton, &QPushButton::clicked, this, [] {
      QDesktopServices::openUrl(kSolarHtmlUrl);
    });
  connect(network_, &QNetworkAccessManager::finished, this, &IonosphericForecastWindow::onNetworkFinished);
  connect(&refreshTimer_, &QTimer::timeout, this, &IonosphericForecastWindow::refreshNow);

  refreshTimer_.setInterval(kRefreshMs);
  refreshTimer_.start();

  read_settings();
  if (width() < 1000 || height() < 720)
    {
      resize(qMax(width(), 1120), qMax(height(), 760));
    }
  refreshNow();
}

IonosphericForecastWindow::~IonosphericForecastWindow()
{
  write_settings();
}

void IonosphericForecastWindow::closeEvent(QCloseEvent * event)
{
  write_settings();
  event->accept();
}

void IonosphericForecastWindow::showEvent(QShowEvent * event)
{
  QDialog::showEvent(event);
  Q_EMIT windowVisibleChanged(isVisible());
}

void IonosphericForecastWindow::hideEvent(QHideEvent * event)
{
  QDialog::hideEvent(event);
  Q_EMIT windowVisibleChanged(isVisible());
}

void IonosphericForecastWindow::setStatus(QString const& status, bool error)
{
  if (error)
    {
      statusLabel_->setStyleSheet("QLabel{color:#ff6666;}");
    }
  else
    {
      statusLabel_->setStyleSheet("QLabel{color:#cfd8e3;}");
    }
  statusLabel_->setText(status);
}

void IonosphericForecastWindow::refreshNow()
{
  if (requestInFlight_)
    {
      return;
    }

  requestInFlight_ = true;
  refreshButton_->setEnabled(false);
  setStatus(tr("Updating from %1 ...").arg(kSolarXmlUrl.toString()));

  QNetworkRequest request {kSolarXmlUrl};
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    QString("%1/%2")
                    .arg(QApplication::applicationName(),
                         QApplication::applicationVersion().isEmpty() ? QStringLiteral("dev")
                                                                      : QApplication::applicationVersion()));
#if QT_VERSION >= QT_VERSION_CHECK (6, 0, 0)
  request.setAttribute (QNetworkRequest::RedirectPolicyAttribute,
                        QNetworkRequest::NoLessSafeRedirectPolicy);
#elif QT_VERSION >= QT_VERSION_CHECK (5, 9, 0)
  request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  auto * xmlReply = network_->get(request);
  xmlReply->setProperty("request_kind", QStringLiteral("xml"));

  QNetworkRequest imageRequest {kSolarSunUrl};
  imageRequest.setHeader(QNetworkRequest::UserAgentHeader,
                         QString("%1/%2")
                         .arg(QApplication::applicationName(),
                              QApplication::applicationVersion().isEmpty() ? QStringLiteral("dev")
                                                                           : QApplication::applicationVersion()));
#if QT_VERSION >= QT_VERSION_CHECK (6, 0, 0)
  imageRequest.setAttribute (QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
#elif QT_VERSION >= QT_VERSION_CHECK (5, 9, 0)
  imageRequest.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  auto * sunReply = network_->get(imageRequest);
  sunReply->setProperty("request_kind", QStringLiteral("sun"));
}

void IonosphericForecastWindow::onNetworkFinished(QNetworkReply * reply)
{
  if (!reply)
    {
      setStatus(tr("Update failed: empty network reply"), true);
      return;
    }

  auto kind = reply->property("request_kind").toString();

  if (kind == "sun")
    {
      auto err = reply->error();
      QString payloadError;
      auto payload = (err == QNetworkReply::NoError)
          ? read_reply_payload_limited(reply, kMaxSolarImageReplyBytes, &payloadError)
          : QByteArray {};
      reply->deleteLater();

      if (err == QNetworkReply::NoError)
        {
          if (payloadError.isEmpty ())
            {
              updateSunImage(payload);
            }
          else
            {
              hasSunImage_ = false;
              sunImageDataUri_.clear();
              if (hasSnapshot_)
                {
                  reportView_->setHtml(buildHtml(lastSnapshot_));
                }
            }
        }
      else if (!hasSunImage_)
        {
          sunImageDataUri_.clear();
          if (hasSnapshot_)
            {
              reportView_->setHtml(buildHtml(lastSnapshot_));
            }
        }
      return;
    }

  requestInFlight_ = false;
  refreshButton_->setEnabled(true);

  if (kind != "xml")
    {
      reply->deleteLater();
      return;
    }

  auto err = reply->error();
  auto errText = reply->errorString();
  QString payloadError;
  auto payload = (err == QNetworkReply::NoError)
      ? read_reply_payload_limited(reply, kMaxSolarXmlReplyBytes, &payloadError)
      : QByteArray {};
  reply->deleteLater();

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

  SolarSnapshot snapshot;
  QString parseError;
  if (!parseSolarXml(payload, &snapshot, &parseError))
    {
      setStatus(tr("Update failed: %1").arg(parseError), true);
      return;
    }

  lastSnapshot_ = snapshot;
  hasSnapshot_ = true;
  reportView_->setHtml(buildHtml(lastSnapshot_));

  QString status = tr("Updated: %1 UTC")
                   .arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss"));
  if (!snapshot.updated.isEmpty())
    {
      status += tr(" | Feed time: %1").arg(snapshot.updated);
    }
  setStatus(status, false);
}

void IonosphericForecastWindow::updateSunImage(QByteArray const& payload)
{
  QPixmap pix;
  if (!pix.loadFromData(payload))
    {
      hasSunImage_ = false;
      sunImageDataUri_.clear();
      if (hasSnapshot_)
        {
          reportView_->setHtml(buildHtml(lastSnapshot_));
        }
      return;
    }
  QByteArray png;
  QBuffer buffer {&png};
  if (!buffer.open(QIODevice::WriteOnly) || !pix.save(&buffer, "PNG"))
    {
      hasSunImage_ = false;
      sunImageDataUri_.clear();
      if (hasSnapshot_)
        {
          reportView_->setHtml(buildHtml(lastSnapshot_));
        }
      return;
    }
  sunImageDataUri_ = QStringLiteral("data:image/png;base64,%1").arg(QString::fromLatin1(png.toBase64()));
  hasSunImage_ = true;
  if (hasSnapshot_)
    {
      reportView_->setHtml(buildHtml(lastSnapshot_));
    }
}

QString IonosphericForecastWindow::normalizeText(QString const& text)
{
  return text.simplified();
}

QString IonosphericForecastWindow::conditionColor(QString const& text)
{
  auto t = text.trimmed().toLower();
  if (t.contains("good") || t.contains("open") || t.contains("quiet"))
    {
      return "#43d787";
    }
  if (t.contains("fair") || t.contains("minor"))
    {
      return "#f0c65a";
    }
  if (t.contains("poor") || t.contains("closed") || t.contains("storm") || t.contains("major"))
    {
      return "#ff6b6b";
    }
  return "#d5dde6";
}

QString IonosphericForecastWindow::displayBandName(QString const& band)
{
  auto b = band.trimmed();
  if (b.isEmpty())
    {
      return QString {"-"};
    }
  return b;
}

QString IonosphericForecastWindow::displayVhfName(QString const& name, QString const& location)
{
  QString n = name.trimmed();
  QString l = location.trimmed();

  if (l == "northern_hemi") l = "Northern Hemisphere";
  if (l == "north_america") l = "North America";
  if (l == "europe_6m") l = "Europe 6m";
  if (l == "europe_4m") l = "Europe 4m";
  if (l == "europe") l = "Europe";

  if (n == "vhf-aurora") n = "VHF Aurora";
  if (n == "E-Skip") n = "E-Skip";

  if (!l.isEmpty())
    {
      return QString {"%1 (%2)"}.arg(n, l);
    }
  return n;
}

QString IonosphericForecastWindow::buildHtml(SolarSnapshot const& snapshot) const
{
  auto esc = [] (QString const& s) { return s.toHtmlEscaped(); };

  QMap<QString, QString> dayByBand;
  QMap<QString, QString> nightByBand;
  QStringList bandOrder;
  for (auto const& b : snapshot.bands)
    {
      auto band = displayBandName(b.name);
      if (!bandOrder.contains(band))
        {
          bandOrder << band;
        }
      auto time = b.time.trimmed().toLower();
      if (time == "night")
        {
          nightByBand.insert(band, b.value);
        }
      else
        {
          dayByBand.insert(band, b.value);
        }
    }

  QString html;
  QTextStream out {&html};
  out << "<html><body style='font-family:Sans-Serif;background:#0f141a;color:#e6edf3;'>\n";
  out << "<table cellspacing='0' cellpadding='0' style='width:100%;border-collapse:collapse;margin-bottom:8px;'>\n";
  out << "<tr>\n";
  out << "<td style='vertical-align:top;padding-right:14px;'>\n";
  out << "<h3 style='margin:0 0 8px 0;'>HamQSL Solar / Ionospheric Snapshot</h3>\n";
  out << "<div style='margin-bottom:10px;color:#98a7b7;'>Feed update: <b>" << esc(snapshot.updated) << "</b></div>\n";
  out << "<table cellspacing='0' cellpadding='5' style='border-collapse:collapse;width:100%;'>\n";
  auto addMetric = [&] (QString const& name, QString const& value) {
      out << "<tr>"
          << "<td style='border:1px solid #23303d;color:#9fb0c2;width:35%;'>" << esc(name) << "</td>"
          << "<td style='border:1px solid #23303d;'>" << esc(value) << "</td>"
          << "</tr>\n";
    };
  addMetric("Solar Flux", snapshot.solarflux);
  addMetric("A-Index", snapshot.aindex);
  addMetric("K-Index", snapshot.kindex);
  addMetric("X-Ray", snapshot.xray);
  addMetric("Sunspots", snapshot.sunspots);
  addMetric("Solar Wind", snapshot.solarwind);
  addMetric("MUF", snapshot.muf);
  addMetric("Geomagnetic Field", snapshot.geomagfield);
  addMetric("Signal Noise", snapshot.signalnoise);
  out << "</table>\n";
  out << "</td>\n";

  out << "<td style='vertical-align:top;text-align:right;width:380px;'>\n";
  if (hasSunImage_ && !sunImageDataUri_.isEmpty())
    {
      out << "<div style='display:inline-block;width:360px;max-width:100%;border:1px solid #23303d;background:#111822;padding:8px;'>"
          << "<img src='" << esc(sunImageDataUri_) << "' alt='Solar image' "
          << "style='width:100%;height:auto;display:block;border:1px solid #2a394a;'/>"
          << "<div style='margin-top:6px;color:#8ea0b4;font-size:11px;'>Source: "
          << "<a href='" << esc(kSolarSunUrl.toString()) << "' style='color:#8ec8ff;'>"
          << esc(kSolarSunUrl.toString()) << "</a></div>"
          << "</div>\n";
    }
  else
    {
      out << "<div style='display:inline-block;width:360px;max-width:100%;border:1px solid #23303d;background:#111822;padding:14px;color:#9fb0c2;text-align:left;'>"
          << "Sun image unavailable</div>\n";
    }
  out << "</td>\n";
  out << "</tr>\n";
  out << "</table>\n";

  out << "<h4 style='margin:14px 0 6px 0;'>HF Band Conditions</h4>\n";
  out << "<table cellspacing='0' cellpadding='5' style='border-collapse:collapse;width:100%;'>\n";
  out << "<tr>"
      << "<th style='border:1px solid #23303d;background:#1b2632;'>Band</th>"
      << "<th style='border:1px solid #23303d;background:#1b2632;'>Day</th>"
      << "<th style='border:1px solid #23303d;background:#1b2632;'>Night</th>"
      << "</tr>\n";

  for (auto const& band : bandOrder)
    {
      auto day = dayByBand.value(band, "N/A");
      auto night = nightByBand.value(band, "N/A");
      out << "<tr>"
          << "<td style='border:1px solid #23303d;'>" << esc(band) << "</td>"
          << "<td style='border:1px solid #23303d;color:" << conditionColor(day) << ";'><b>" << esc(day) << "</b></td>"
          << "<td style='border:1px solid #23303d;color:" << conditionColor(night) << ";'><b>" << esc(night) << "</b></td>"
          << "</tr>\n";
    }
  if (bandOrder.isEmpty())
    {
      out << "<tr><td colspan='3' style='border:1px solid #23303d;'>No HF condition data</td></tr>\n";
    }
  out << "</table>\n";

  out << "<h4 style='margin:14px 0 6px 0;'>VHF Conditions</h4>\n";
  out << "<table cellspacing='0' cellpadding='5' style='border-collapse:collapse;width:100%;'>\n";
  out << "<tr>"
      << "<th style='border:1px solid #23303d;background:#1b2632;'>Phenomenon</th>"
      << "<th style='border:1px solid #23303d;background:#1b2632;'>Status</th>"
      << "</tr>\n";
  if (snapshot.vhf.isEmpty())
    {
      out << "<tr><td colspan='2' style='border:1px solid #23303d;'>No VHF condition data</td></tr>\n";
    }
  else
    {
      for (auto const& c : snapshot.vhf)
        {
          auto item = displayVhfName(c.name, c.location);
          out << "<tr>"
              << "<td style='border:1px solid #23303d;'>" << esc(item) << "</td>"
              << "<td style='border:1px solid #23303d;color:" << conditionColor(c.value) << ";'><b>"
              << esc(c.value) << "</b></td>"
              << "</tr>\n";
        }
    }
  out << "</table>\n";

  out << "<p style='margin-top:12px;color:#98a7b7;'>Source: "
      << "<a href='" << esc(kSolarHtmlUrl.toString()) << "' style='color:#8ec8ff;'>"
      << esc(kSolarHtmlUrl.toString()) << "</a></p>\n";
  out << "</body></html>\n";
  return html;
}

bool IonosphericForecastWindow::parseSolarXml(QByteArray const& xml, SolarSnapshot * out, QString * error) const
{
  if (!out)
    {
      if (error) *error = "Invalid output buffer";
      return false;
    }

  SolarSnapshot snapshot;
  QXmlStreamReader reader {xml};
  bool foundData = false;

  while (!reader.atEnd())
    {
      auto token = reader.readNext();
      if (token != QXmlStreamReader::StartElement)
        {
          continue;
        }

      if (reader.name() != QLatin1String("solardata"))
        {
          continue;
        }

      foundData = true;
      while (reader.readNextStartElement())
        {
          auto name = reader.name().toString();
          if (name == "updated") snapshot.updated = normalizeText(reader.readElementText());
          else if (name == "solarflux") snapshot.solarflux = normalizeText(reader.readElementText());
          else if (name == "aindex") snapshot.aindex = normalizeText(reader.readElementText());
          else if (name == "kindex") snapshot.kindex = normalizeText(reader.readElementText());
          else if (name == "xray") snapshot.xray = normalizeText(reader.readElementText());
          else if (name == "sunspots") snapshot.sunspots = normalizeText(reader.readElementText());
          else if (name == "solarwind") snapshot.solarwind = normalizeText(reader.readElementText());
          else if (name == "muf") snapshot.muf = normalizeText(reader.readElementText());
          else if (name == "geomagfield") snapshot.geomagfield = normalizeText(reader.readElementText());
          else if (name == "signalnoise") snapshot.signalnoise = normalizeText(reader.readElementText());
          else if (name == "calculatedconditions")
            {
              while (reader.readNextStartElement())
                {
                  if (reader.name() == QLatin1String("band"))
                    {
                      auto attrs = reader.attributes();
                      BandCondition band;
                      band.name = normalizeText(attrs.value("name").toString());
                      band.time = normalizeText(attrs.value("time").toString());
                      band.value = normalizeText(reader.readElementText());
                      snapshot.bands.push_back(band);
                    }
                  else
                    {
                      reader.skipCurrentElement();
                    }
                }
            }
          else if (name == "calculatedvhfconditions")
            {
              while (reader.readNextStartElement())
                {
                  if (reader.name() == QLatin1String("phenomenon"))
                    {
                      auto attrs = reader.attributes();
                      VhfCondition vhf;
                      vhf.name = normalizeText(attrs.value("name").toString());
                      vhf.location = normalizeText(attrs.value("location").toString());
                      vhf.value = normalizeText(reader.readElementText());
                      snapshot.vhf.push_back(vhf);
                    }
                  else
                    {
                      reader.skipCurrentElement();
                    }
                }
            }
          else
            {
              reader.skipCurrentElement();
            }
        }
      break;
    }

  if (reader.hasError())
    {
      if (error) *error = reader.errorString();
      return false;
    }

  if (!foundData)
    {
      if (error) *error = "No <solardata> node found in feed";
      return false;
    }

  *out = snapshot;
  return true;
}

void IonosphericForecastWindow::read_settings()
{
  if (!settings_)
    {
      return;
    }
  SettingsGroup g {settings_, "IonosphericForecastWindow"};
  move(settings_->value("window/pos", pos()).toPoint());
  auto geometry = settings_->value("geometry").toByteArray();
  if (!geometry.isEmpty())
    {
      WindowGeometryUtils::restore_window_geometry(this, geometry);
    }
}

void IonosphericForecastWindow::write_settings()
{
  if (!settings_)
    {
      return;
    }
  SettingsGroup g {settings_, "IonosphericForecastWindow"};
  settings_->setValue("window/pos", pos());
  settings_->setValue("geometry", saveGeometry());
}
