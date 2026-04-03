#include "logbook.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include "Configuration.hpp"
#include "AD1CCty.hpp"
#include "Radio.hpp"
#include "Multiplier.hpp"
#include "logbook/AD1CCty.hpp"
#include "models/CabrilloLog.hpp"
#include "models/FoxLog.hpp"

#include "moc_logbook.cpp"

namespace
{
QString sanitize_adif_value (QString const& in, bool preserve_spaces = true)
{
  QString out;
  out.reserve (in.size ());
  for (auto const& ch : in)
    {
      if (ch.isNull () || ch == QChar {'<'} || ch == QChar {'>'} || ch == QChar {'\r'} || ch == QChar {'\n'})
        {
          continue;
        }
      if (ch.isPrint () || (preserve_spaces && ch.isSpace ()))
        {
          out.append (ch);
        }
    }
  return out.trimmed ();
}

QString exportable_operator_call (QString const& operator_call, QString const& station_call)
{
  auto const safeOperator = sanitize_adif_value (operator_call, false).trimmed ().toUpper ();
  auto const safeStation = sanitize_adif_value (station_call, false).trimmed ().toUpper ();
  if (safeOperator.isEmpty ()) {
    return {};
  }

  auto const operatorBase = Radio::base_callsign (safeOperator).trimmed ().toUpper ();
  auto const stationBase = Radio::base_callsign (safeStation).trimmed ().toUpper ();
  if (operatorBase.isEmpty () || !Radio::is_callsign (operatorBase)) {
    return {};
  }

  if (!stationBase.isEmpty () && operatorBase == stationBase) {
    return {};
  }

  return safeOperator;
}
}

int LogBook::migrateAdif317 (QString const& path)
{
  QFile file {path};
  if (!file.exists ())
    {
      return 0;
    }
  if (!file.open (QIODevice::ReadOnly))
    {
      return 0;
    }

  auto data = file.readAll ();
  file.close ();
  if (!data.toLower ().contains ("<mode:3>ft2"))
    {
      return 0;
    }

  auto const backup_path = path + ".pre317bak";
  if (!QFile::exists (backup_path))
    {
      QFile::copy (path, backup_path);
    }

  QString text = QString::fromUtf8 (data);
  QRegularExpression const legacy_ft2_mode {
    QStringLiteral ("<mode:3>FT2"),
    QRegularExpression::CaseInsensitiveOption
  };
  int const count = text.count (legacy_ft2_mode);
  if (!count)
    {
      return 0;
    }

  text.replace (legacy_ft2_mode, QStringLiteral ("<MODE:4>MFSK <SUBMODE:3>FT2"));

  if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate))
    {
      return -1;
    }
  file.write (text.toUtf8 ());
  file.close ();

  qDebug () << "ADIF 3.17 migrated" << count << "FT2 record(s) in" << path;
  return count;
}

LogBook::LogBook (Configuration const * configuration)
  : config_ {configuration}
  , worked_before_ {configuration}
{
  Q_ASSERT (configuration);
  connect (&worked_before_, &WorkedBefore::finished_loading, this, &LogBook::finished_loading);
}

LogBook::~LogBook ()
{
}

void LogBook::match (QString const& call, QString const& mode, QString const& grid,
                     AD1CCty::Record const& looked_up,
                     bool& callB4,
                     bool& countryB4,
                     bool& gridB4,
                     bool& continentB4,
                     bool& CQZoneB4,
                     bool& ITUZoneB4,
                     QString const& band) const
{
  if (call.size() > 0)
    {
      auto const& mode_to_check = (config_ && !config_->highlight_by_mode ()) ? QString {} : mode;
      callB4 = worked_before_.call_worked (call, mode_to_check, band);
      gridB4 = worked_before_.grid_worked(grid, mode_to_check, band);
      auto const& countryName = looked_up.entity_name;
      if (countryName.size ())
        {
          countryB4 = worked_before_.country_worked (countryName, mode_to_check, band);
          continentB4 = worked_before_.continent_worked (looked_up.continent, mode_to_check, band);
          CQZoneB4 = worked_before_.CQ_zone_worked (looked_up.CQ_zone, mode_to_check, band);
          ITUZoneB4 = worked_before_.ITU_zone_worked (looked_up.ITU_zone, mode_to_check, band);
        }
      else
        {
          countryB4 = true;  // we don't want to flag unknown entities
          continentB4 = true;
          CQZoneB4 = true;
          ITUZoneB4 = true;
        }
    }
}

bool LogBook::add (QString const& call
                   , QString const& grid
                   , QString const& band
                   , QString const& mode
                   , QByteArray const& ADIF_record)
{
  return worked_before_.add (call, grid, band, mode, ADIF_record);
}

void LogBook::rescan ()
{
  worked_before_.reload ();
}

QString const LogBook::cty_version() const
{
  return worked_before_.cty_version();
}

QByteArray LogBook::QSOToADIF (QString const& hisCall, QString const& hisGrid, QString const& mode,
                               QString const& rptSent, QString const& rptRcvd, QDateTime const& dateTimeOn,
                               QDateTime const& dateTimeOff, QString const& band, QString const& comments,
                               QString const& name, QString const& strDialFreq, QString const& myCall,
                               QString const& myGrid, QString const& txPower, QString const& operator_call,
                               QString const& xSent, QString const& xRcvd, QString const& propmode,
                               QString const& satellite, QString const& satmode, QString const& freqRx)
{
  auto const safeHisCall = sanitize_adif_value (hisCall, false);
  auto const safeHisGrid = sanitize_adif_value (hisGrid, false);
  auto const safeMode = sanitize_adif_value (mode, false);
  auto const safeRptSent = sanitize_adif_value (rptSent, false);
  auto const safeRptRcvd = sanitize_adif_value (rptRcvd, false);
  auto const safeBand = sanitize_adif_value (band, false);
  auto const safeComments = sanitize_adif_value (comments, true);
  auto const safeName = sanitize_adif_value (name, true);
  auto const safeDialFreq = sanitize_adif_value (strDialFreq, false);
  auto const safeMyCall = sanitize_adif_value (myCall, false);
  auto const safeMyGrid = sanitize_adif_value (myGrid, false);
  auto const safeTxPower = sanitize_adif_value (txPower, false);
  auto const safeOperator = exportable_operator_call (operator_call, myCall);
  auto const safeXSent = sanitize_adif_value (xSent, true);
  auto const safeXRcvd = sanitize_adif_value (xRcvd, true);
  auto const safePropmode = sanitize_adif_value (propmode, false);
  auto const safeSatellite = sanitize_adif_value (satellite, false);
  auto const safeSatmode = sanitize_adif_value (satmode, false);
  auto const safeFreqRx = sanitize_adif_value (freqRx, false);

  QString t;
  t = "<call:" + QString::number(safeHisCall.size()) + ">" + safeHisCall;
  t += " <gridsquare:" + QString::number(safeHisGrid.size()) + ">" + safeHisGrid;
  if (safeMode != "FT2" && safeMode != "FT4" && safeMode != "FST4" && safeMode != "Q65")
    {
      t += " <mode:" + QString::number(safeMode.size()) + ">" + safeMode;
    }
  else
    {
      t += " <mode:4>MFSK <submode:" + QString::number(safeMode.size()) + ">" + safeMode;
    }
  t += " <rst_sent:" + QString::number(safeRptSent.size()) + ">" + safeRptSent;
  t += " <rst_rcvd:" + QString::number(safeRptRcvd.size()) + ">" + safeRptRcvd;
  t += " <qso_date:8>" + dateTimeOn.date().toString("yyyyMMdd");
  t += " <time_on:6>" + dateTimeOn.time().toString("hhmmss");
  t += " <qso_date_off:8>" + dateTimeOff.date().toString("yyyyMMdd");
  t += " <time_off:6>" + dateTimeOff.time().toString("hhmmss");
  t += " <band:" + QString::number(safeBand.size()) + ">" + safeBand;
  t += " <freq:" + QString::number(safeDialFreq.size()) + ">" + safeDialFreq;
  t += " <station_callsign:" + QString::number(safeMyCall.size()) + ">" + safeMyCall;
  if(!safeMyGrid.isEmpty()) t += " <my_gridsquare:" + QString::number(safeMyGrid.size()) + ">" + safeMyGrid;
  if(!safeTxPower.isEmpty()) t += " <tx_pwr:" + QString::number(safeTxPower.size()) + ">" + safeTxPower;
  if(!safeComments.isEmpty()) t += " <comment:" + QString::number(safeComments.size()) + ">" + safeComments;
  if(!safeName.isEmpty()) t += " <name:" + QString::number(safeName.size()) + ">" + safeName;
  if(!safeOperator.isEmpty()) t+=" <operator:" + QString::number(safeOperator.size()) + ">" + safeOperator;
  if(!safePropmode.isEmpty()) t += " <prop_mode:" + QString::number(safePropmode.size()) + ">" + safePropmode;
  if(!safeSatellite.isEmpty()) t += " <sat_name:" + QString::number(safeSatellite.size()) + ">" + safeSatellite;
  if(!safeSatmode.isEmpty()) t += " <sat_mode:" + QString::number(safeSatmode.size()) + ">" + safeSatmode;
  if(!safeFreqRx.isEmpty()) t += " <freq_rx:" + QString::number(safeFreqRx.size()) + ">" + safeFreqRx;
  if (safeXSent.size ())
    {
      auto words = safeXSent.split (' '
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                                , QString::SkipEmptyParts
#else
                                , Qt::SkipEmptyParts
#endif
                                );
      if (words.size () > 1)
        {
          if (words.back ().toUInt ())
            {
              // assume last word is a serial if there are at least
              // two words and if it is positive numeric
              t += " <stx:" + QString::number (words.back ().size ()) + '>' + words.back ();
            }
          else
            {
              if (words.front ().toUInt () && words.front ().size () > 3) // EU VHF contest mode
                {
                  auto sn_text = words.front ().mid (2);
                  // assume first word is report+serial if there are
                  // at least two words and if the first word less the
                  // first two characters is a positive numeric
                  t += " <stx:" + QString::number (sn_text.size ()) + '>' + sn_text;
                }
            }
        }
    }
  if (safeXRcvd.size ()) {
    auto words = safeXRcvd.split (' '
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                              , QString::SkipEmptyParts
#else
                              , Qt::SkipEmptyParts
#endif
                              );
    if (words.size () == 2)
      {
        if (words.at (1).toUInt ())
          {
            t += " <srx:" + QString::number (words.at (1).size ()) + ">" + words.at (1);
          }
        else if (words.at (0).toUInt () && words.at (0).size () > 3) // EU VHF contest exchange
          {
            // strip report and set SRX to serial
            t += " <srx:" + QString::number (words.at (0).mid (2).size ()) + ">" + words.at (0).mid (2);
          }
        else
          {
            if (Configuration::SpecialOperatingActivity::FIELD_DAY == config_->special_op_id ())
              {
                // include DX as an ARRL_SECT value even though it is
                // not in the ADIF spec ARRL_SECT enumeration, done
                // because N1MM does the same
                t += " <contest_id:14>ARRL-FIELD-DAY <SRX_STRING:" + QString::number (safeXRcvd.size ()) + '>' + safeXRcvd
                  + " <class:" + QString::number (words.at (0).size ()) + '>'
                  + words.at (0) + " <arrl_sect:" + QString::number (words.at (1).size ()) + '>' + words.at (1);
              }
            else if (Configuration::SpecialOperatingActivity::RTTY == config_->special_op_id ())
              {
                t += " <state:" + QString::number (words.at (1).size ()) + ">" + words.at (1);
              }
          }
      }
  }
  return t.toLatin1();
}

CabrilloLog * LogBook::contest_log ()
{
  // lazy create of Cabrillo log object instance
  if (!contest_log_)
    {
      contest_log_.reset (new CabrilloLog {config_});
      if (!multiplier_)
        {
          multiplier_.reset (new Multiplier {countries ()});
        }
      connect (contest_log_.data (), &CabrilloLog::data_changed, [this] () {
          multiplier_->reload (contest_log_.data ());
        });
    }
  return contest_log_.data ();
}

Multiplier const * LogBook::multiplier () const
{
  // lazy create of Multiplier object instance
  if (!multiplier_)
    {
      multiplier_.reset (new Multiplier {countries ()});
    }
  return multiplier_.data ();
}

FoxLog * LogBook::fox_log ()
{
  // lazy create of Fox log object instance
  if (!fox_log_)
    {
      fox_log_.reset (new FoxLog {config_});
    }
  return fox_log_.data ();
}
