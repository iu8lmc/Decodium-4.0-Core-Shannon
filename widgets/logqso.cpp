#include "logqso.h"

#include <QLocale>
#include <QString>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QDir>
#include <QCoreApplication>
#include <QTimer>
#include <QPushButton>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QPointer>
#include <QCheckBox>
#include <QComboBox>
#include <QSet>
#include <QVBoxLayout>

#include "HelpTextWindow.hpp"
#include "logbook/logbook.h"
#include "MessageBox.hpp"
#include "Configuration.hpp"
#include "Radio.hpp"
#include "models/Bands.hpp"
#include "models/CabrilloLog.hpp"
#include "validators/MaidenheadLocatorValidator.hpp"
#include "WindowGeometryUtils.hpp"
#include "Logger.hpp"

#include "ui_logqso.h"
#include "moc_logqso.cpp"

namespace
{
  QString sanitize_operator_value (QString const& in)
  {
    QString out;
    out.reserve (in.size ());
    for (auto const& ch : in)
      {
        if (ch.isNull () || ch == QChar {'<'} || ch == QChar {'>'} || ch == QChar {'\r'} || ch == QChar {'\n'})
          {
            continue;
          }
        if (ch.isPrint ())
          {
            out.append (ch);
          }
      }
    return out.trimmed ();
  }

  QString normalized_operator_call (QString const& operator_call, QString const& station_call)
  {
    auto const safeOperator = sanitize_operator_value (operator_call).trimmed ().toUpper ();
    auto const safeStation = sanitize_operator_value (station_call).trimmed ().toUpper ();
    if (safeOperator.isEmpty ())
      {
        return {};
      }

    auto const operatorBase = Radio::base_callsign (safeOperator).trimmed ().toUpper ();
    auto const stationBase = Radio::base_callsign (safeStation).trimmed ().toUpper ();
    if (operatorBase.isEmpty () || !Radio::is_callsign (operatorBase))
      {
        return {};
      }

    if (!stationBase.isEmpty () && operatorBase == stationBase)
      {
        return {};
      }

    return safeOperator;
  }

  struct LoggingMode
  {
    bool prompt_to_log;
    bool auto_log;
  };

  LoggingMode current_logging_mode (QSettings * settings, bool fallback_prompt_to_log, bool fallback_auto_log)
  {
    bool prompt_to_log = fallback_prompt_to_log;
    bool auto_log = fallback_auto_log;
    bool has_prompt_to_log = false;

    if (settings)
      {
        settings->sync ();
        has_prompt_to_log = settings->contains ("PromptToLog");
        prompt_to_log = settings->value ("PromptToLog", prompt_to_log).toBool ();
        auto_log = settings->value ("AutoLog", auto_log).toBool ();
      }

    if (prompt_to_log == auto_log)
      {
        if (prompt_to_log && has_prompt_to_log)
          {
            auto_log = false;
          }
        else
          {
            prompt_to_log = false;
            auto_log = true;
          }
      }

    return {prompt_to_log, auto_log};
  }

  auto const sat_file_name = "sat.dat";

  QStringList satellite_data_candidates (Configuration const * config)
  {
    QStringList candidates {
      QDir {QStandardPaths::writableLocation (QStandardPaths::AppLocalDataLocation)}.absoluteFilePath (sat_file_name),
      QDir {QStandardPaths::writableLocation (QStandardPaths::AppDataLocation)}.absoluteFilePath (sat_file_name)
    };

    if (config)
      {
        candidates << config->writeable_data_dir ().absoluteFilePath (sat_file_name)
                   << config->data_dir ().absoluteFilePath (sat_file_name);
      }

    auto const appDir = QCoreApplication::applicationDirPath ();
    candidates << QDir {appDir}.absoluteFilePath (sat_file_name)
               << QDir {appDir}.absoluteFilePath ("../sat.dat")
               << QDir {appDir}.absoluteFilePath ("../Resources/sat.dat")
               << QDir {appDir}.absoluteFilePath ("../share/Decodium/sat.dat")
               << QDir {appDir}.absoluteFilePath ("../share/wsjtx/sat.dat")
               << QDir::current ().absoluteFilePath (sat_file_name);
#ifdef CMAKE_SOURCE_DIR
    candidates << QDir {QStringLiteral (CMAKE_SOURCE_DIR)}.absoluteFilePath (sat_file_name);
#endif
    candidates << QStringLiteral (":/sat.dat");
    return candidates;
  }

  struct PropMode
  {
    char const * id_;
    char const * name_;
  };
  constexpr PropMode prop_modes[] =
    {
     {"", ""}
     , {"AS", QT_TRANSLATE_NOOP ("LogQSO", "Aircraft scatter")}
     , {"AUE", QT_TRANSLATE_NOOP ("LogQSO", "Aurora-E")}
     , {"AUR", QT_TRANSLATE_NOOP ("LogQSO", "Aurora")}
     , {"BS", QT_TRANSLATE_NOOP ("LogQSO", "Back scatter")}
     , {"ECH", QT_TRANSLATE_NOOP ("LogQSO", "Echolink")}
     , {"EME", QT_TRANSLATE_NOOP ("LogQSO", "Earth-moon-earth")}
     , {"ES", QT_TRANSLATE_NOOP ("LogQSO", "Sporadic E")}
     , {"F2", QT_TRANSLATE_NOOP ("LogQSO", "F2 Reflection")}
     , {"FAI", QT_TRANSLATE_NOOP ("LogQSO", "Field aligned irregularities")}
     , {"INTERNET", QT_TRANSLATE_NOOP ("LogQSO", "Internet-assisted")}
     , {"ION", QT_TRANSLATE_NOOP ("LogQSO", "Ionoscatter")}
     , {"IRL", QT_TRANSLATE_NOOP ("LogQSO", "IRLP")}
     , {"MS", QT_TRANSLATE_NOOP ("LogQSO", "Meteor scatter")}
     , {"RPT", QT_TRANSLATE_NOOP ("LogQSO", "Non-satellite repeater or transponder")}
     , {"RS", QT_TRANSLATE_NOOP ("LogQSO", "Rain scatter")}
     , {"SAT", QT_TRANSLATE_NOOP ("LogQSO", "Satellite")}
     , {"TEP", QT_TRANSLATE_NOOP ("LogQSO", "Trans-equatorial")}
     , {"TR", QT_TRANSLATE_NOOP ("LogQSO", "Tropospheric ducting")}
    };
  struct SatMode
  {
    char const * id_;
    char const * name_;
  };
  constexpr SatMode sat_modes[] =
    {
      {"", ""}
      , {"A", QT_TRANSLATE_NOOP ("LogQSO", "A")}
      , {"B", QT_TRANSLATE_NOOP ("LogQSO", "B")}
      , {"BS", QT_TRANSLATE_NOOP ("LogQSO", "BS")}
      , {"JA", QT_TRANSLATE_NOOP ("LogQSO", "JA")}
      , {"JD", QT_TRANSLATE_NOOP ("LogQSO", "JD")}
      , {"K", QT_TRANSLATE_NOOP ("LogQSO", "K")}
      , {"KA", QT_TRANSLATE_NOOP ("LogQSO", "KA")}
      , {"KT", QT_TRANSLATE_NOOP ("LogQSO", "KT")}
      , {"L", QT_TRANSLATE_NOOP ("LogQSO", "L")}
      , {"LS", QT_TRANSLATE_NOOP ("LogQSO", "LS")}
      , {"LU", QT_TRANSLATE_NOOP ("LogQSO", "LU")}
      , {"LX", QT_TRANSLATE_NOOP ("LogQSO", "LX")}
      , {"S", QT_TRANSLATE_NOOP ("LogQSO", "S")}
      , {"SX", QT_TRANSLATE_NOOP ("LogQSO", "SX")}
      , {"T", QT_TRANSLATE_NOOP ("LogQSO", "T")}
      , {"US", QT_TRANSLATE_NOOP ("LogQSO", "US")}
      , {"UV", QT_TRANSLATE_NOOP ("LogQSO", "UV")}
      , {"VS", QT_TRANSLATE_NOOP ("LogQSO", "VS")}
      , {"VU", QT_TRANSLATE_NOOP ("LogQSO", "VU")}
    };
}

LogQSO::LogQSO(QString const& programTitle, QSettings * settings
               , Configuration const * config, LogBook * log, QWidget *parent)
  : QDialog {parent, Qt::WindowStaysOnTopHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint}
  , ui(new Ui::LogQSO)
  , m_settings (settings)
  , m_config {config}
  , m_log {log}
{
  ui->setupUi(this);
  setAttribute (Qt::WA_QuitOnClose, false);
  setWindowTitle(programTitle + " - Log QSO");
  m_clusterSpotCheckBox = new QCheckBox {tr ("Spot su DX Cluster"), this};
  m_clusterSpotCheckBox->setToolTip (tr ("Invia questo QSO al DX Cluster se la connessione cluster era gia' attiva."));
  if (ui->verticalLayout_5) {
    int const insertAt = qMax (0, ui->verticalLayout_5->count () - 1);
    ui->verticalLayout_5->insertWidget (insertAt, m_clusterSpotCheckBox);
  }
  setClusterSpotState (false, false);
  ui->comboBoxSatellite->addItem ("", "");
  QSet<QString> seen_sat_paths;
  QSet<QString> seen_sat_codes;
  for (auto const& candidate : satellite_data_candidates (m_config))
    {
      auto const clean = candidate.startsWith (":/") ? candidate : QDir::cleanPath (candidate);
      if (clean.isEmpty () || seen_sat_paths.contains (clean))
        {
          continue;
        }
      seen_sat_paths.insert (clean);
      QFileInfo const sat_info {clean};
      if (sat_info.exists () && sat_info.size () > 4LL * 1024LL * 1024LL)
        {
          LOG_WARN (QString {"Skipping sat.dat load, file too large: %1 bytes"}.arg (sat_info.size ()).toStdString ());
          continue;
        }
      QFile file {clean};
      if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
        {
          continue;
        }
      QTextStream stream {&file};
      while (!stream.atEnd ())
        {
          auto const line = stream.readLine ().trimmed ();
          if (line.isEmpty ())
            {
              continue;
            }
          auto const parts = line.split ('|');
          if (parts.size () < 2)
            {
              continue;
            }
          auto const code = parts.at (0).trimmed ();
          auto const name = parts.at (1).trimmed ();
          if (code.isEmpty () || seen_sat_codes.contains (code))
            {
              continue;
            }
          seen_sat_codes.insert (code);
          ui->comboBoxSatellite->addItem (QStringLiteral ("%1 - %2").arg (code, name.isEmpty () ? code : name), code);
        }
      file.close ();
      if (!seen_sat_codes.isEmpty ())
        {
          break;
        }
    }
  for (auto const& prop_mode : prop_modes)
    {
      ui->comboBoxPropMode->addItem (prop_mode.name_, prop_mode.id_);
    }
  for (auto const& sat_mode : sat_modes)
    {
      ui->comboBoxSatMode->addItem (sat_mode.name_, sat_mode.id_);
    }
  connect (ui->comboBoxPropMode, &QComboBox::currentTextChanged, this, &LogQSO::propModeChanged);
  connect (ui->comboBoxSatellite, QOverload<int>::of (&QComboBox::currentIndexChanged), this, [this] (int) {
    ensureSatellitePropMode ();
  });
  connect (ui->comboBoxSatMode, QOverload<int>::of (&QComboBox::currentIndexChanged), this, [this] (int) {
    ensureSatellitePropMode ();
  });
  connect (ui->comments, &QComboBox::currentTextChanged, this, &LogQSO::commentsChanged);
  connect (ui->addButton, &QPushButton::clicked, this, &LogQSO::on_addButton_clicked);
  loadSettings ();
  auto date_time_format = QLocale {}.dateFormat (QLocale::ShortFormat) + " hh:mm:ss";
  ui->start_date_time->setDisplayFormat (date_time_format);
  ui->end_date_time->setDisplayFormat (date_time_format);
  ui->grid->setValidator (new MaidenheadLocatorValidator {this});
}

LogQSO::~LogQSO ()
{
}

void LogQSO::setClusterSpotState(bool available, bool checked)
{
  if (!m_clusterSpotCheckBox) {
    return;
  }
  m_clusterSpotCheckBox->setEnabled (available);
  m_clusterSpotCheckBox->setChecked (available && checked);
}

void LogQSO::setNextPromptOverrides(QString const& comment,
                                    bool commentValid,
                                    QString const& propMode,
                                    QString const& satellite,
                                    QString const& satMode,
                                    bool satelliteValid)
{
  m_nextPromptCommentValid = commentValid;
  m_nextPromptComment = comment;
  m_nextPromptSatelliteValid = satelliteValid;
  m_nextPromptPropMode = propMode;
  m_nextPromptSatellite = satellite;
  m_nextPromptSatMode = satMode;
}

void LogQSO::loadSettings ()
{
  m_settings->beginGroup ("LogQSO");
  WindowGeometryUtils::restore_window_geometry (this, m_settings->value ("geometry", saveGeometry ()).toByteArray ());
  ui->cbTxPower->setChecked (m_settings->value ("SaveTxPower", false).toBool ());
  ui->cbComments->setChecked (m_settings->value ("SaveComments", false).toBool ());
  ui->cbPropMode->setChecked (m_settings->value ("SavePropMode", false).toBool ());
  ui->cbSatellite->setChecked (m_settings->value ("SaveSatellite", false).toBool ());
  ui->cbSatMode->setChecked (m_settings->value ("SaveSatMode", false).toBool ());
  m_comments = m_settings->value ("LogComments", "").toString();
  m_txPower = m_settings->value ("TxPower", "").toString ();

  int prop_index {0};
  if (ui->cbPropMode->isChecked ())
    {
      prop_index = ui->comboBoxPropMode->findData (m_settings->value ("PropMode", "").toString());
    }
  ui->comboBoxPropMode->setCurrentIndex (prop_index);
  int sat_mode_index {0};
  if (ui->cbSatMode->isChecked ())
    {
      sat_mode_index = ui->comboBoxSatMode->findData (m_settings->value ("SatMode", "").toString());
    }
  ui->comboBoxSatMode->setCurrentIndex (sat_mode_index);
  int satellite {0};
  if (ui->cbSatellite->isChecked ())
    {
      satellite = ui->comboBoxSatellite->findData (m_settings->value ("Satellite", "").toString());
    }
  ui->comboBoxSatellite->setCurrentIndex (satellite);
  ensureSatellitePropMode ();
  propModeChanged ();
  m_freqRx = m_settings->value ("FreqRx", "").toString ();
  ui->cbFreqRx->setChecked (m_settings->value ("SaveFreqRx", false).toBool ());

  QString comments_location;  // load the content of comments.txt file to the comments combo box
  QDir dataPath {QStandardPaths::writableLocation (QStandardPaths::AppLocalDataLocation)};
  comments_location = dataPath.exists("comments.txt") ? dataPath.absoluteFilePath("comments.txt") : m_config->data_dir ().absoluteFilePath ("comments.txt");
  QFile file2 {comments_location};
  QFileInfo comments_info {comments_location};
  if (comments_info.exists () && comments_info.isFile () && comments_info.size () <= 2LL * 1024LL * 1024LL
      && file2.open (QIODevice::ReadOnly | QIODevice::Text))
    {
      QTextStream stream2 {&file2};
      while (!stream2.atEnd ())
        {
          ui->comments->addItem (stream2.readLine ());
        }
      file2.close ();
    }
  else
    {
      if (comments_info.exists () && comments_info.size () > 2LL * 1024LL * 1024LL)
        {
          LOG_WARN (QString {"Skipping comments.txt load, file too large: %1 bytes"}
                    .arg (comments_info.size ()).toStdString ());
        }
      ui->comments->addItem ("");
    }
  if (ui->cbComments->isChecked ()) ui->comments->setItemText(ui->comments->currentIndex(), m_comments);

  m_settings->endGroup ();
}

void LogQSO::storeSettings () const
{
  m_settings->beginGroup ("LogQSO");
  m_settings->setValue ("geometry", saveGeometry ());
  m_settings->setValue ("SaveTxPower", ui->cbTxPower->isChecked ());
  m_settings->setValue ("SaveComments", ui->cbComments->isChecked ());
  m_settings->setValue ("SavePropMode", ui->cbPropMode->isChecked ());
  m_settings->setValue ("SaveSatellite", ui->cbSatellite->isChecked ());
  m_settings->setValue ("SaveSatMode", ui->cbSatMode->isChecked ());
  m_settings->setValue ("SaveFreqRx", ui->cbFreqRx->isChecked ());
  m_settings->setValue ("TxPower", m_txPower);
  m_settings->setValue ("LogComments", m_comments);
  m_settings->setValue ("PropMode", ui->comboBoxPropMode->currentData ());
  m_settings->setValue ("Satellite", ui->comboBoxSatellite->currentData ());
  m_settings->setValue ("SatMode", ui->comboBoxSatMode->currentData ());
  m_settings->setValue ("FreqRx", m_freqRx);
  m_settings->endGroup ();
}

void LogQSO::initLogQSO(QString const& hisCall, QString const& hisGrid, QString mode,
                        QString const& rptSent, QString const& rptRcvd,
                        QDateTime const& dateTimeOn, QDateTime const& dateTimeOff,
                        Radio::Frequency dialFreq, bool noSuffix, QString xSent, QString xRcvd,
                        bool externalCtrl, bool promptAlreadyAccepted)    //avt 11/20/20
{
  if(!isHidden()) return;

  QPushButton* okBtn = ui->buttonBox->button(QDialogButtonBox::Ok);
  okBtn->setAutoDefault(true);
  okBtn->setDefault(true);
  okBtn->setFocus();
  QPushButton* caBtn = ui->buttonBox->button(QDialogButtonBox::Cancel);
  caBtn->setAutoDefault(false);
  caBtn->setDefault(false);

  ui->call->setText (hisCall);
  if (m_config->log4digitGrids()) {
    ui->grid->setText (hisGrid.left(4));
  } else {
    ui->grid->setText (hisGrid);
  }
  if (hisGrid == "" && m_config->ZZ00()) ui->grid->setText("ZZ00");
  ui->name->clear ();
  if (ui->cbTxPower->isChecked ())
    {
      ui->txPower->setText (m_txPower);
    }
  else
    {
      ui->txPower->clear ();
    }
  if (ui->cbFreqRx->isChecked ())
    {
      ui->freqRx->setText (m_freqRx);
    }
  else
    {
      ui->freqRx->clear ();
    }
  if (ui->cbComments->isChecked ())
    {
      ui->comments->setItemText(ui->comments->currentIndex(), m_comments);
    }
  else
    {
      ui->comments->setCurrentIndex(0);
      ui->comments->setItemText(ui->comments->currentIndex(), "");
    }
  if (m_config->report_in_comments()) {
    auto t=mode;
    if(rptSent!="") t+="  Sent: " + rptSent;
    if(rptRcvd!="") t+="  Rcvd: " + rptRcvd;
    ui->comments->setCurrentIndex(0);
    ui->comments->setItemText(ui->comments->currentIndex(), t);
  }
  if(noSuffix and mode.mid(0,3)=="JT9") mode="JT9";
  if(m_config->log_as_RTTY() and mode.mid(0,3)=="JT9") mode="RTTY";
  ui->mode->setText(mode);
  ui->sent->setText(rptSent);
  ui->rcvd->setText(rptRcvd);
  ui->start_date_time->setDateTime (dateTimeOn);
  ui->end_date_time->setDateTime (dateTimeOff);
  m_dialFreq=dialFreq;
  m_myCall=m_config->my_callsign();
  m_myGrid=m_config->my_grid();
  ui->band->setText (m_config->bands ()->find (dialFreq));
  ui->loggedOperator->setText (normalized_operator_call (m_config->opCall (), m_myCall));
  ui->exchSent->setText (xSent);
  ui->exchRcvd->setText (xRcvd);
  if (!ui->cbPropMode->isChecked ())
    {
      ui->comboBoxPropMode->setCurrentIndex (-1);
    }
  if (!ui->cbSatellite->isChecked ())
    {
      ui->comboBoxSatellite->setCurrentIndex (-1);
    }
  if (!ui->cbSatMode->isChecked ())
    {
      ui->comboBoxSatMode->setCurrentIndex (-1);
    }
  ensureSatellitePropMode ();
  propModeChanged ();

  using SpOp = Configuration::SpecialOperatingActivity;
  auto special_op = m_config->special_op_id ();

  // put contest name in comments
  if (SpOp::NONE != special_op && SpOp::HOUND != special_op && SpOp::FOX != special_op
      && m_config->Individual_Contest_Name() && !m_config->report_in_comments()
      && m_config->Contest_Name() !="" && !ui->cbComments->isChecked() && m_config->specOp_in_comments()) {
    QString Contest_Name = (m_config->Contest_Name() + " Contest");
    ui->comments->setCurrentIndex(0);
    ui->comments->setItemText(ui->comments->currentIndex(), Contest_Name);
  }
  if (SpOp::NA_VHF == special_op && !m_config->Individual_Contest_Name() && !m_config->report_in_comments()
      && !ui->cbComments->isChecked() && m_config->specOp_in_comments()) {
    QString Contest_Name = ("NA VHF Contest");
    ui->comments->setCurrentIndex(0);
    ui->comments->setItemText(ui->comments->currentIndex(), Contest_Name);
  }
  if (SpOp::EU_VHF == special_op && !m_config->Individual_Contest_Name() && !m_config->report_in_comments()
      && !ui->cbComments->isChecked() && m_config->specOp_in_comments()) {
    QString Contest_Name = ("EU VHF Contest");
    ui->comments->setCurrentIndex(0);
    ui->comments->setItemText(ui->comments->currentIndex(), Contest_Name);
  }
  if (SpOp::WW_DIGI == special_op && !m_config->Individual_Contest_Name() && !m_config->report_in_comments()
      && !ui->cbComments->isChecked() && m_config->specOp_in_comments()) {
    QString Contest_Name = ("WW Digi Contest");
    ui->comments->setCurrentIndex(0);
    ui->comments->setItemText(ui->comments->currentIndex(), Contest_Name);
  }
  if (SpOp::FIELD_DAY == special_op && !m_config->Individual_Contest_Name() && !m_config->report_in_comments()
      && !ui->cbComments->isChecked() && m_config->specOp_in_comments()) {
    QString Contest_Name = ("ARRL Field Day");
    ui->comments->setCurrentIndex(0);
    ui->comments->setItemText(ui->comments->currentIndex(), Contest_Name);
  }
  if (SpOp::RTTY == special_op && !m_config->Individual_Contest_Name() && !m_config->report_in_comments()
      && !ui->cbComments->isChecked() && m_config->specOp_in_comments()) {
    QString Contest_Name = ("FT Roundup messages");
    ui->comments->setCurrentIndex(0);
    ui->comments->setItemText(ui->comments->currentIndex(), Contest_Name);
  }
  if (SpOp::ARRL_DIGI == special_op && !m_config->Individual_Contest_Name() && !m_config->report_in_comments()
      && !ui->cbComments->isChecked() && m_config->specOp_in_comments()) {
    QString Contest_Name = ("ARRL Digi Contest");
    ui->comments->setCurrentIndex(0);
    ui->comments->setItemText(ui->comments->currentIndex(), Contest_Name);
  }
  if (SpOp::HOUND == special_op && !m_config->report_in_comments()
      && !ui->cbComments->isChecked() && m_config->specOp_in_comments()) {
    QString Contest_Name = ("F/H mode");
    if (m_config->superFox()) Contest_Name = ("SF/H mode");
    ui->comments->setCurrentIndex(0);
    ui->comments->setItemText(ui->comments->currentIndex(), Contest_Name);
  }
  if (SpOp::NONE == special_op && !m_config->report_in_comments()
      && !ui->cbComments->isChecked() && m_config->specOp_in_comments()) {
    m_comments = "";
  }

  if (m_nextPromptCommentValid)
    {
      ui->comments->setCurrentIndex (0);
      ui->comments->setItemText (ui->comments->currentIndex (), m_nextPromptComment);
      m_comments = m_nextPromptComment;
      m_comments_temp = m_nextPromptComment;
      m_nextPromptCommentValid = false;
      m_nextPromptComment.clear ();
    }

  if (m_nextPromptSatelliteValid)
    {
      auto setComboData = [] (QComboBox * combo, QString const& value)
      {
        if (!combo)
          {
            return;
          }
        int const index = combo->findData (value.trimmed ());
        combo->setCurrentIndex (index >= 0 ? index : 0);
      };

      QString const propMode = m_nextPromptPropMode.trimmed ();
      QString const satellite = m_nextPromptSatellite.trimmed ();
      QString const satMode = m_nextPromptSatMode.trimmed ();
      ui->cbPropMode->setChecked (!propMode.isEmpty ());
      ui->cbSatellite->setChecked (!satellite.isEmpty ());
      ui->cbSatMode->setChecked (!satMode.isEmpty ());
      setComboData (ui->comboBoxPropMode, propMode);
      setComboData (ui->comboBoxSatellite, satellite);
      setComboData (ui->comboBoxSatMode, satMode);
      ensureSatellitePropMode ();
      propModeChanged ();
      m_nextPromptSatelliteValid = false;
      m_nextPromptPropMode.clear ();
      m_nextPromptSatellite.clear ();
      m_nextPromptSatMode.clear ();
    }

  auto const logging_mode = current_logging_mode (m_settings, m_config->prompt_to_log (), m_config->autoLog ());
  bool const force_log_without_prompt = promptAlreadyAccepted || (externalCtrl && !logging_mode.prompt_to_log);
  if (SpOp::FOX == special_op
      || force_log_without_prompt                                      //avt 11/19/20 UDP listener requires auto logging
      || (logging_mode.auto_log && ((SpOp::NONE < special_op && special_op < SpOp::FOX)
          || SpOp::ARRL_DIGI == special_op || !m_config->contestingOnly ())))
    {
      // allow auto logging in Fox mode and contests
      accept();
    }
  else
    {
      show();
    }
}

void LogQSO::accept()
{
  if (m_accepting) {
    return;
  }
  m_accepting = true;

  auto hisCall = ui->call->text ();
  auto hisGrid = ui->grid->text ();
  auto mode = ui->mode->text ();
  auto rptSent = ui->sent->text ();
  auto rptRcvd = ui->rcvd->text ();
  auto dateTimeOn = ui->start_date_time->dateTime ();
  auto dateTimeOff = ui->end_date_time->dateTime ();
  auto band = ui->band->text ();
  auto name = ui->name->text ();
  m_txPower = ui->txPower->text ();
  auto strDialFreq = QString::number (m_dialFreq / 1.e6,'f',6);
  auto operator_call = normalized_operator_call (ui->loggedOperator->text (), m_myCall);
  auto xsent = ui->exchSent->text ();
  auto xrcvd = ui->exchRcvd->text ();

  if (!m_log) {
    m_accepting = false;
    show ();
    MessageBox::warning_message (this, tr ("Log file error"),
                                 tr ("Log backend is not available"));
    return;
  }

  using SpOp = Configuration::SpecialOperatingActivity;
  auto special_op = m_config->special_op_id ();

  if (special_op == SpOp::NA_VHF or special_op == SpOp::WW_DIGI) {
    if(xrcvd!="" and hisGrid!=xrcvd) hisGrid=xrcvd;
  }

  if ((special_op == SpOp::RTTY and xsent!="" and xrcvd!="")) {
    if(rptSent=="" or !xsent.contains(rptSent+" ")) rptSent=xsent.split(" "
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                                                                        , QString::SkipEmptyParts
#else
                                                                        , Qt::SkipEmptyParts
#endif
                                                                        ).at(0);
    if(rptRcvd=="" or !xrcvd.contains(rptRcvd+" ")) rptRcvd=xrcvd.split(" "
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                                                                        , QString::SkipEmptyParts
#else
                                                                        , Qt::SkipEmptyParts
#endif
                                                                        ).at(0);
  }

  // validate
  if ((SpOp::NONE < special_op && special_op < SpOp::FOX) || (special_op > SpOp::HOUND))
    {
      if (xsent.isEmpty () || xrcvd.isEmpty ())
        {
          m_accepting = false;
          show ();
          MessageBox::warning_message (this, tr ("Invalid QSO Data"),
                                       tr ("Check exchange sent and received"));
          return;               // without accepting
        }

      auto * contestLog = m_log->contest_log ();
      if (!contestLog || !contestLog->add_QSO (m_dialFreq, mode, dateTimeOff, hisCall, xsent, xrcvd))
        {
          m_accepting = false;
          show ();
          MessageBox::warning_message (this, tr ("Invalid QSO Data"),
                                       tr ("Check all fields"));
          return;               // without accepting
        }
    }

  auto prop_mode = ui->comboBoxPropMode->currentData ().toString ();
  auto satellite = ui->comboBoxSatellite->currentData ().toString ();
  auto sat_mode = ui->comboBoxSatMode->currentData ().toString ();
  if (prop_mode != "SAT" && (!satellite.isEmpty () || !sat_mode.isEmpty ())) {
    prop_mode = "SAT";
  }
  // Add sat name and sat mode tags only if "Satellite" is selected as prop mode.
  if (prop_mode != "SAT") {
    satellite = "";
    sat_mode = "";
  }
  m_freqRx = ui->freqRx->text ();
  //Log this QSO to file "wsjtx.log"
  static QFile f {QDir {QStandardPaths::writableLocation (QStandardPaths::AppLocalDataLocation)}.absoluteFilePath ("wsjtx.log")};
  if(!f.open(QIODevice::Text | QIODevice::Append)) {
    MessageBox::warning_message (this, tr ("Log file error"),
                                 tr ("Cannot open \"%1\" for append").arg (f.fileName ()),
                                 tr ("Error: %1").arg (f.errorString ()));
  } else {
    QString logEntry=dateTimeOn.date().toString("yyyy-MM-dd,") +
      dateTimeOn.time().toString("hh:mm:ss,") +
      dateTimeOff.date().toString("yyyy-MM-dd,") +
      dateTimeOff.time().toString("hh:mm:ss,") + hisCall + "," +
      hisGrid + "," + strDialFreq + "," + mode +
      "," + rptSent + "," + rptRcvd + "," + m_txPower +
      "," + m_comments + "," + name + "," + prop_mode +
      "," + satellite + "," + sat_mode + "," + m_freqRx;
    QTextStream out(&f);
    out << logEntry <<
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                 endl
#else
                 Qt::endl
#endif
                 ;
    f.close();
  }

  //Clean up and finish logging
  QByteArray const adif = m_log->QSOToADIF (hisCall
                                            , hisGrid
                                            , mode
                                            , rptSent
                                            , rptRcvd
                                            , dateTimeOn
                                            , dateTimeOff
                                            , band
                                            , m_comments
                                            , name
                                            , strDialFreq
                                            , m_myCall
                                            , m_myGrid
                                            , m_txPower
                                            , operator_call
                                            , xsent
                                            , xrcvd
                                            , prop_mode
                                            , satellite
                                            , sat_mode
                                            , m_freqRx);
  QPointer<LogQSO> self {this};
  Q_EMIT acceptQSO (dateTimeOff
                    , hisCall
                    , hisGrid
                    , m_dialFreq
                    , mode
                    , rptSent
                    , rptRcvd
                    , m_txPower
                    , m_comments
                    , name
                    , dateTimeOn
                    , operator_call
                    , m_myCall
                    , m_myGrid
                    , xsent
                    , xrcvd
                    , prop_mode
	                    , satellite
	                    , sat_mode
	                    , m_freqRx
	                    , adif
	                    , m_clusterSpotCheckBox
	                        && m_clusterSpotCheckBox->isEnabled ()
	                        && m_clusterSpotCheckBox->isChecked ());
  if (!self) {
    return;
  }
  m_accepting = false;
  QDialog::accept();
}

void LogQSO::ensureSatellitePropMode ()
{
  bool const has_satellite = !ui->comboBoxSatellite->currentData ().toString ().trimmed ().isEmpty ()
                             || !ui->comboBoxSatMode->currentData ().toString ().trimmed ().isEmpty ();
  if (!has_satellite)
    {
      return;
    }

  int const sat_index = ui->comboBoxPropMode->findData (QStringLiteral ("SAT"));
  if (sat_index >= 0 && ui->comboBoxPropMode->currentIndex () != sat_index)
    {
      ui->comboBoxPropMode->setCurrentIndex (sat_index);
    }
}

void LogQSO::propModeChanged()
{
  if (ui->comboBoxPropMode->currentData() != "SAT") {
      ui->comboBoxSatellite->setCurrentIndex(0);
      ui->comboBoxSatMode->setCurrentIndex(0);
  }
  ui->comboBoxSatellite->setEnabled(true);
  ui->cbSatellite->setEnabled(true);
  ui->comboBoxSatMode->setEnabled(true);
  ui->cbSatMode->setEnabled(true);
}

void LogQSO::commentsChanged(const QString& text)
{
  int index = ui->comments->findText(text);
  if(index != -1)
  {
    ui->comments->setCurrentIndex(index);
  }
  m_comments = (ui->comments->currentIndex(), text);
  m_comments_temp = (ui->comments->currentIndex(), text);
}

void LogQSO::on_addButton_clicked()
{
  m_settings->setValue ("LogComments", m_comments_temp);
  if (m_comments_temp != "") {
      QString comments_location = m_config->writeable_data_dir().absoluteFilePath("comments.txt");
      if(QFileInfo::exists(m_config->writeable_data_dir().absoluteFilePath("comments.txt"))) {
      QFile file2 {comments_location};
        if (file2.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
            QTextStream out(&file2);
            out << m_comments_temp              // append new line to comments.txt
    #if QT_VERSION >= QT_VERSION_CHECK (5, 15, 0)
            << Qt::endl
    #else
            << endl
    #endif
            ;
          file2.close();
          MessageBox::information_message (this,
                                           "Your comment has been added to the comments list.\n\n"
                                           "To edit your comments list, open the file\n"
                                           "\"comments.txt\" from your log directory");
        }
      } else {
          QFile file2 {comments_location};
         if (file2.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
             QTextStream out(&file2);
             out << ("\n" + m_comments_temp)    // create file "comments.txt" and add a blank line
    #if QT_VERSION >= QT_VERSION_CHECK (5, 15, 0)
              << Qt::endl
    #else
              << endl
    #endif
            ;
          file2.close();
          MessageBox::information_message (this,
                                           "Your comment has been added to the comments list.\n\n"
                                           "To edit your comments list, open the file\n"
                                           "\"comments.txt\" from your log directory");
         }
      }
      ui->comments->clear();               // clear the comments combo box and reload updated content
      QFile file2 {comments_location};
      QTextStream stream2(&file2);
      if(file2.open (QIODevice::ReadOnly | QIODevice::Text)) {
          while (!stream2.atEnd()) {
              QString line = stream2.readLine();
              ui->comments->addItem (line);
          }
          stream2.flush();
          file2.close();
      }
  }
}

// closeEvent is only called from the system menu close widget for a
// modeless dialog so we use the hideEvent override to store the
// window settings
void LogQSO::hideEvent (QHideEvent * e)
{
  storeSettings ();
  QDialog::hideEvent (e);
}
