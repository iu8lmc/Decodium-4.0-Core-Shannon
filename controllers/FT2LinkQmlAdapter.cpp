#include "controllers/FT2LinkQmlAdapter.hpp"

#include "lib/ft2link/FT2LinkAudio.hpp"
#include "lib/ft2link/FT2LinkFrame.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QString>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimeZone>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

namespace
{
using decodium::ft2link::AppSession;
using decodium::ft2link::AppSessionState;
using decodium::ft2link::ChatDeliveryState;
using decodium::ft2link::ChatMessage;
using decodium::ft2link::ChatMessageDirection;
using decodium::ft2link::Frame;
using decodium::ft2link::LinkCapabilities;
using decodium::ft2link::Profile;
using decodium::ft2link::StationAdvertisement;
using decodium::ft2link::StationIdentity;
using decodium::ft2link::W2300RateMode;
using decodium::ft2link::WideAudioPipelineResult;
using decodium::ft2link::WideTxAudioPlan;

constexpr quint64 kMinBeaconIntervalMs = 60000u;
constexpr quint64 kPathFinderMaxAgeMs = 24u * 60u * 60u * 1000u;
constexpr int kLocalStoreVersion = 1;
constexpr int kMaxRelayHopCount = 9;

QString utcMinuteText (quint64 atMs);

bool shouldAutoLoadLocalStore ()
{
  QString const appName = QCoreApplication::applicationName ().toLower ();
  return !appName.startsWith (QStringLiteral ("test_"))
      && !appName.contains (QStringLiteral ("test-ft2link"))
      && !appName.contains (QStringLiteral ("test_ft2link"));
}

template<typename T>
QVariant numericVariant (T value)
{
  return QVariant::fromValue<qulonglong> (
      static_cast<qulonglong> (value));
}

quint64 jsonU64 (QJsonObject const& object, QString const& key,
                 quint64 fallback = 0u)
{
  QJsonValue const value = object.value (key);
  if (value.isUndefined () || value.isNull ())
    {
      return fallback;
    }
  return value.toVariant ().toULongLong ();
}

quint32 jsonU32 (QJsonObject const& object, QString const& key,
                 quint32 fallback = 0u)
{
  quint64 const value = jsonU64 (object, key, fallback);
  return value > std::numeric_limits<quint32>::max ()
      ? fallback
      : static_cast<quint32> (value);
}

quint16 jsonU16 (QJsonObject const& object, QString const& key,
                 quint16 fallback = 0u)
{
  quint64 const value = jsonU64 (object, key, fallback);
  return value > std::numeric_limits<quint16>::max ()
      ? fallback
      : static_cast<quint16> (value);
}

int jsonInt (QJsonObject const& object, QString const& key,
             int fallback = 0)
{
  QJsonValue const value = object.value (key);
  if (value.isUndefined () || value.isNull ())
    {
      return fallback;
    }
  return value.toInt (fallback);
}

QString jsonString (QJsonObject const& object, QString const& key)
{
  return object.value (key).toString ().trimmed ();
}

QJsonObject jsonObjectFromMap (QVariantMap const& map)
{
  return QJsonObject::fromVariantMap (map);
}

std::string toStdString (QString const& value)
{
  return value.trimmed ().toUpper ().toStdString ();
}

std::string toStdFreeText (QString const& value)
{
  return value.trimmed ().toStdString ();
}

Profile profileFromInt (int value)
{
  switch (value)
    {
    case 1: return Profile::Wide500;
    case 2: return Profile::Wide2300;
    default: return Profile::Wide2300;
    }
}

W2300RateMode rateModeFromInt (int value)
{
  return value == 1 ? W2300RateMode::Robust : W2300RateMode::Fast;
}

QString sessionStateName (AppSessionState state)
{
  switch (state)
    {
    case AppSessionState::Calling: return QStringLiteral ("Calling");
    case AppSessionState::Connected: return QStringLiteral ("Connected");
    case AppSessionState::Rejected: return QStringLiteral ("Rejected");
    case AppSessionState::Closed: return QStringLiteral ("Closed");
    }
  return QStringLiteral ("Unknown");
}

QString messageDirectionName (ChatMessageDirection direction)
{
  switch (direction)
    {
    case ChatMessageDirection::Outgoing: return QStringLiteral ("Outgoing");
    case ChatMessageDirection::Incoming: return QStringLiteral ("Incoming");
    case ChatMessageDirection::System: return QStringLiteral ("System");
    }
  return QStringLiteral ("Unknown");
}

QString deliveryStateName (ChatDeliveryState state)
{
  switch (state)
    {
    case ChatDeliveryState::Pending: return QStringLiteral ("Pending");
    case ChatDeliveryState::Delivered: return QStringLiteral ("Delivered");
    case ChatDeliveryState::Received: return QStringLiteral ("Received");
    case ChatDeliveryState::Failed: return QStringLiteral ("Failed");
    }
  return QStringLiteral ("Unknown");
}

QByteArray toByteArray (std::vector<std::uint8_t> const& bytes)
{
  return QByteArray (
      reinterpret_cast<char const*> (bytes.data ()),
      static_cast<int> (bytes.size ()));
}

QVector<float> toSampleVector (std::vector<float> const& samples)
{
  QVector<float> out;
  out.reserve (static_cast<qsizetype> (samples.size ()));
  for (float sample : samples)
    {
      out.push_back (sample);
    }
  return out;
}

std::vector<std::uint8_t> toBytes (QByteArray const& bytes)
{
  std::vector<std::uint8_t> out;
  out.reserve (static_cast<std::size_t> (bytes.size ()));
  for (char byte : bytes)
    {
      out.push_back (static_cast<std::uint8_t> (byte));
    }
  return out;
}

QString sanitizedContactTag (QString const& tag)
{
  QString clean = tag.simplified ().toUpper ().left (16);
  clean.replace (QLatin1Char (','), QLatin1Char (' '));
  return clean.simplified ();
}

QString sanitizedContactComment (QString const& comment)
{
  return comment.simplified ().left (240);
}

QString sanitizedCqType (QString const& type)
{
  QString const upper = type.trimmed ().toUpper ();
  if (upper == QStringLiteral ("CHAT") || upper == QStringLiteral ("NET")
      || upper == QStringLiteral ("EMCOMM") || upper == QStringLiteral ("TEST")
      || upper == QStringLiteral ("QSY"))
    {
      return upper;
    }
  return QStringLiteral ("CQ");
}

QString cqTypeNameFromCode (quint16 code)
{
  switch (code & 0x000fu)
    {
    case 1u: return QStringLiteral ("CHAT");
    case 2u: return QStringLiteral ("NET");
    case 3u: return QStringLiteral ("EMCOMM");
    case 4u: return QStringLiteral ("TEST");
    case 5u: return QStringLiteral ("QSY");
    default: return QStringLiteral ("CQ");
    }
}

QString sanitizedCqLocator (QString const& locator)
{
  QString out;
  QString const upper = locator.trimmed ().toUpper ();
  for (QChar ch : upper)
    {
      if (ch.isLetterOrNumber ())
        {
          out.append (ch);
        }
      if (out.size () >= 8)
        {
          break;
        }
    }
  return out;
}

QString sanitizedClusterNodeId (QString const& nodeId)
{
  QString clean = nodeId.simplified ().toUpper ();
  clean.replace (QRegularExpression {QStringLiteral ("[^A-Z0-9_.:/-]")},
                 QStringLiteral ("-"));
  clean = clean.left (32);
  return clean.isEmpty () ? QStringLiteral ("LOCAL") : clean;
}

QString sanitizedClusterBand (QString const& band)
{
  QString clean = band.simplified ().toUpper ();
  clean.replace (QRegularExpression {QStringLiteral ("[^A-Z0-9_.:/ -]")},
                 QString {});
  clean = clean.left (32).simplified ();
  return clean.isEmpty () ? QStringLiteral ("LOCAL") : clean;
}

QString clusterKey (QString const& nodeId,
                    QString const& band,
                    qint64 dialFrequencyHz,
                    QString const& call)
{
  return sanitizedClusterNodeId (nodeId)
      + QLatin1Char ('|') + sanitizedClusterBand (band)
      + QLatin1Char ('|') + QString::number (std::max<qint64> (
          0, dialFrequencyHz))
      + QLatin1Char ('|') + call.trimmed ().toUpper ();
}

QVariantMap capabilitiesMap (LinkCapabilities const& capabilities)
{
  QVariantMap map;
  map.insert (QStringLiteral ("supportsW500"), capabilities.supportsW500);
  map.insert (QStringLiteral ("supportsW2300"), capabilities.supportsW2300);
  map.insert (QStringLiteral ("supportsW2300Fast"), capabilities.supportsW2300Fast);
  map.insert (QStringLiteral ("supportsW2300Robust"), capabilities.supportsW2300Robust);
  map.insert (QStringLiteral ("preferredProfile"),
              static_cast<int> (capabilities.preferredProfile));
  map.insert (QStringLiteral ("preferredProfileName"),
              QString::fromStdString (decodium::ft2link::profileName (
                  capabilities.preferredProfile)));
  map.insert (QStringLiteral ("preferredW2300RateMode"),
              static_cast<int> (capabilities.preferredW2300RateMode));
  return map;
}

QVariantMap stationMap (StationAdvertisement const& advertisement,
                        QString const& tag = QString {})
{
  QVariantMap map;
  map.insert (QStringLiteral ("call"),
              QString::fromStdString (advertisement.station.call));
  map.insert (QStringLiteral ("locator"),
              QString::fromStdString (advertisement.station.locator));
  map.insert (QStringLiteral ("name"),
              QString::fromStdString (advertisement.station.name));
  map.insert (QStringLiteral ("tag"), tag.trimmed ());
  map.insert (QStringLiteral ("cq"), advertisement.cq);
  map.insert (QStringLiteral ("cqType"),
              QString::fromStdString (advertisement.cqType));
  map.insert (QStringLiteral ("cqLocator"),
              QString::fromStdString (advertisement.cqLocator));
  map.insert (QStringLiteral ("cqSlotId"), advertisement.cqSlotId);
  map.insert (QStringLiteral ("cqSlotOffsetHz"), advertisement.cqSlotOffsetHz);
  map.insert (QStringLiteral ("cqSlotSizeHz"), advertisement.cqSlotSizeHz);
  QString const slotLabel = advertisement.cqSlotId > 0
      ? QStringLiteral ("S+%1").arg (advertisement.cqSlotId)
      : (advertisement.cqSlotId < 0
         ? QStringLiteral ("S%1").arg (advertisement.cqSlotId)
         : QString {});
  map.insert (QStringLiteral ("cqSlotLabel"), slotLabel);
  map.insert (QStringLiteral ("heardAtMs"),
              QVariant::fromValue<qulonglong> (advertisement.heardAtMs));
  map.insert (QStringLiteral ("capabilities"),
              capabilitiesMap (advertisement.capabilities));
  return map;
}

QVariantMap sessionMap (AppSession const& session)
{
  QVariantMap map;
  map.insert (QStringLiteral ("sessionId"), session.sessionId);
  map.insert (QStringLiteral ("remoteCall"),
              QString::fromStdString (session.remoteCall));
  map.insert (QStringLiteral ("state"), static_cast<int> (session.state));
  map.insert (QStringLiteral ("stateName"), sessionStateName (session.state));
  map.insert (QStringLiteral ("accepted"), session.negotiated.accepted);
  map.insert (QStringLiteral ("profile"),
              static_cast<int> (session.negotiated.profile));
  map.insert (QStringLiteral ("profileName"),
              QString::fromStdString (decodium::ft2link::profileName (
                  session.negotiated.profile)));
  map.insert (QStringLiteral ("w2300RateMode"),
              static_cast<int> (session.negotiated.w2300RateMode));
  map.insert (QStringLiteral ("messageCount"),
              static_cast<int> (session.messages.size ()));
  map.insert (QStringLiteral ("openedAtMs"),
              QVariant::fromValue<qulonglong> (session.openedAtMs));
  map.insert (QStringLiteral ("updatedAtMs"),
              QVariant::fromValue<qulonglong> (session.updatedAtMs));
  return map;
}

QVariantMap messageMap (ChatMessage const& message)
{
  QVariantMap map;
  map.insert (QStringLiteral ("direction"), static_cast<int> (message.direction));
  map.insert (QStringLiteral ("directionName"), messageDirectionName (message.direction));
  map.insert (QStringLiteral ("delivery"), static_cast<int> (message.delivery));
  map.insert (QStringLiteral ("deliveryName"), deliveryStateName (message.delivery));
  map.insert (QStringLiteral ("atMs"), QVariant::fromValue<qulonglong> (message.atMs));
  map.insert (QStringLiteral ("text"), QString::fromStdString (message.text));
  return map;
}

QVariantMap cannedMessageMap (QString const& label,
                              QString const& templateText,
                              QString const& tip,
                              bool custom = false)
{
  QVariantMap map;
  map.insert (QStringLiteral ("label"), label);
  map.insert (QStringLiteral ("templateText"), templateText);
  map.insert (QStringLiteral ("tip"), tip);
  map.insert (QStringLiteral ("custom"), custom);
  return map;
}

QString sanitizedCannedLabel (QString const& value)
{
  QString label = value.simplified ().toUpper ();
  label.replace (QRegularExpression {QStringLiteral ("[^A-Z0-9_?+-]")},
                 QStringLiteral (""));
  return label.left (12);
}

QString sanitizedCannedText (QString const& value, int maxLength)
{
  QString text = value;
  text.replace (QLatin1Char ('\r'), QLatin1Char (' '));
  text = text.simplified ();
  return text.left (maxLength);
}

QString sanitizedAlertTag (QString const& value)
{
  QString tag = value.simplified ().toUpper ();
  tag.remove (QLatin1Char ('['));
  tag.remove (QLatin1Char (']'));
  tag.replace (QRegularExpression {QStringLiteral ("[^A-Z0-9_+-]")},
               QStringLiteral (""));
  return tag.left (24);
}

QStringList defaultAlertTags ()
{
  return {
    QStringLiteral ("SOS"),
    QStringLiteral ("MAYDAY"),
    QStringLiteral ("EMERGENCY"),
    QStringLiteral ("URGENT"),
    QStringLiteral ("MEDICAL"),
    QStringLiteral ("EVAC"),
    QStringLiteral ("QSY")
  };
}

QStringList parseAlertTagsText (QString const& tagsText)
{
  QStringList parsed;
  QStringList const parts = tagsText.split (
      QRegularExpression {QStringLiteral ("[,;\\s]+")},
      Qt::SkipEmptyParts);
  for (QString const& part : parts)
    {
      QString const tag = sanitizedAlertTag (part);
      if (tag.size () >= 2 && !parsed.contains (tag))
        {
          parsed.push_back (tag);
        }
      if (parsed.size () >= 24)
        {
          break;
        }
    }
  return parsed;
}

QVariantMap qsySlotMap (int slotId, int offsetHz, QString const& tag)
{
  QVariantMap map;
  QString const direction = offsetHz > 0
      ? QStringLiteral ("UP")
      : QStringLiteral ("DN");
  map.insert (QStringLiteral ("slotId"), slotId);
  map.insert (QStringLiteral ("offsetHz"), offsetHz);
  map.insert (QStringLiteral ("direction"), direction);
  map.insert (QStringLiteral ("label"),
              QStringLiteral ("%1 %2").arg (
                  offsetHz > 0 ? QStringLiteral ("+")
                               : QStringLiteral ("-"),
                  QString::number (std::abs (offsetHz))));
  map.insert (QStringLiteral ("tag"), tag);
  map.insert (QStringLiteral ("tip"),
              QStringLiteral ("Insert QSY %1 %2 Hz invitation")
                  .arg (direction, QString::number (std::abs (offsetHz))));
  return map;
}

qint64 parseFrequencyHz (QString const& value)
{
  QString digits = value.trimmed ();
  digits.remove (QRegularExpression {QStringLiteral ("[^0-9]")});
  bool ok = false;
  qint64 const hz = digits.toLongLong (&ok);
  if (!ok || hz < 100000 || hz > 1000000000)
    {
      return 0;
    }
  return hz;
}

int parseUtcMinute (QString const& value)
{
  QString digits = value.trimmed ();
  digits.remove (QRegularExpression {QStringLiteral ("[^0-9]")});
  if (digits.size () == 3)
    {
      digits.prepend (QLatin1Char ('0'));
    }
  if (digits.size () != 4)
    {
      return -1;
    }
  bool ok = false;
  int const hhmm = digits.toInt (&ok);
  if (!ok)
    {
      return -1;
    }
  int const hour = hhmm / 100;
  int const minute = hhmm % 100;
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
    {
      return -1;
    }
  return hour * 60 + minute;
}

QString utcMinuteRangeText (int startMinute, int endMinute)
{
  auto one = [] (int minute) {
    minute = std::clamp (minute, 0, 1439);
    return QStringLiteral ("%1%2")
        .arg (minute / 60, 2, 10, QLatin1Char ('0'))
        .arg (minute % 60, 2, 10, QLatin1Char ('0'));
  };
  return QStringLiteral ("%1-%2").arg (one (startMinute), one (endMinute));
}

QString sanitizedScheduleAction (QString const& value)
{
  QString action = value.simplified ().toUpper ();
  action.replace (QRegularExpression {QStringLiteral ("[^A-Z0-9_-]")},
                  QString {});
  if (action == QStringLiteral ("CQ")
      || action == QStringLiteral ("BEACON")
      || action == QStringLiteral ("CALLING")
      || action == QStringLiteral ("DATA")
      || action == QStringLiteral ("QUIET")
      || action == QStringLiteral ("EMCOMM"))
    {
      return action;
    }
  return QStringLiteral ("CALLING");
}

bool scheduleContainsMinute (int startMinute, int endMinute, int minute)
{
  startMinute = std::clamp (startMinute, 0, 1439);
  endMinute = std::clamp (endMinute, 0, 1439);
  minute = std::clamp (minute, 0, 1439);
  if (startMinute <= endMinute)
    {
      return minute >= startMinute && minute <= endMinute;
    }
  return minute >= startMinute || minute <= endMinute;
}

QVariantMap frequencyPresetMap (qint64 dialFrequencyHz,
                                QString const& band,
                                QString const& label)
{
  QVariantMap map;
  map.insert (QStringLiteral ("dialFrequencyHz"),
              QVariant::fromValue<qlonglong> (dialFrequencyHz));
  map.insert (QStringLiteral ("band"), band.trimmed ());
  map.insert (QStringLiteral ("label"), label.trimmed ());
  map.insert (QStringLiteral ("display"),
              QStringLiteral ("%1 Hz | %2 | %3")
                  .arg (dialFrequencyHz)
                  .arg (band.trimmed (), label.trimmed ()));
  return map;
}

QVariantMap allowedQsyRangeMap (qint64 fromHz,
                                qint64 toHz,
                                QString const& label)
{
  QVariantMap map;
  map.insert (QStringLiteral ("fromHz"),
              QVariant::fromValue<qlonglong> (fromHz));
  map.insert (QStringLiteral ("toHz"),
              QVariant::fromValue<qlonglong> (toHz));
  map.insert (QStringLiteral ("label"), label.trimmed ());
  map.insert (QStringLiteral ("display"),
              QStringLiteral ("%1-%2 Hz %3")
                  .arg (fromHz)
                  .arg (toHz)
                  .arg (label.trimmed ()));
  return map;
}

QVariantMap frequencyScheduleEntryMap (int startMinute,
                                       int endMinute,
                                       QString const& action,
                                       qint64 dialFrequencyHz,
                                       QString const& label,
                                       QString const& cqType,
                                       bool active,
                                       quint64 nowMs)
{
  QVariantMap map;
  map.insert (QStringLiteral ("startMinute"), startMinute);
  map.insert (QStringLiteral ("endMinute"), endMinute);
  map.insert (QStringLiteral ("utcRange"),
              utcMinuteRangeText (startMinute, endMinute));
  map.insert (QStringLiteral ("action"), sanitizedScheduleAction (action));
  map.insert (QStringLiteral ("dialFrequencyHz"),
              QVariant::fromValue<qlonglong> (dialFrequencyHz));
  map.insert (QStringLiteral ("label"), label.trimmed ());
  map.insert (QStringLiteral ("cqType"), sanitizedCqType (cqType));
  map.insert (QStringLiteral ("active"), active);
  map.insert (QStringLiteral ("evaluatedAtMs"),
              QVariant::fromValue<qulonglong> (nowMs));
  map.insert (QStringLiteral ("display"),
              QStringLiteral ("%1 | %2 | %3 | %4 | %5")
                  .arg (utcMinuteRangeText (startMinute, endMinute),
                        sanitizedScheduleAction (action),
                        QString::number (dialFrequencyHz),
                        label.trimmed (),
                        sanitizedCqType (cqType)));
  return map;
}

std::vector<FT2LinkQmlAdapter::FrequencyPreset> defaultFrequencyPresets ()
{
  return {
    {14105000, QStringLiteral ("20m"), QStringLiteral ("Main")},
    {7105000, QStringLiteral ("40m"), QStringLiteral ("Main")},
    {14108750, QStringLiteral ("20m"), QStringLiteral ("Sunday roundtable")},
    {1995000, QStringLiteral ("160m"), QString {}},
    {3595000, QStringLiteral ("80m"), QString {}},
    {5355000, QStringLiteral ("60m"), QStringLiteral ("Non US")},
    {10133000, QStringLiteral ("30m"), QString {}},
    {18107000, QStringLiteral ("17m"), QString {}},
    {21105000, QStringLiteral ("15m"), QString {}},
    {24927000, QStringLiteral ("12m"), QString {}},
    {28105000, QStringLiteral ("10m"), QString {}},
    {50330000, QStringLiteral ("6m"), QString {}},
    {144170000, QStringLiteral ("2m"), QStringLiteral ("SSB")},
    {144950000, QStringLiteral ("2m"), QStringLiteral ("FM")},
    {432550000, QStringLiteral ("70cm"), QStringLiteral ("SSB")},
    {439600000, QStringLiteral ("70cm"), QStringLiteral ("FM")}
  };
}

std::vector<FT2LinkQmlAdapter::AllowedQsyRange> defaultAllowedQsyRanges ()
{
  return {
    {14101250, 14108750, QStringLiteral ("20m")},
    {7101250, 7108750, QStringLiteral ("40m")},
    {1991250, 1998000, QStringLiteral ("160m")},
    {3591250, 3598000, QStringLiteral ("80m")},
    {10129250, 10136750, QStringLiteral ("30m")},
    {18103250, 18110750, QStringLiteral ("17m")},
    {21101250, 21108750, QStringLiteral ("15m")},
    {24923250, 24930750, QStringLiteral ("12m")},
    {28101250, 28108750, QStringLiteral ("10m")},
    {50326250, 50333750, QStringLiteral ("6m")},
    {144000000, 146000000, QStringLiteral ("2m")},
    {430000000, 434000000, QStringLiteral ("70cm")}
  };
}

std::vector<FT2LinkQmlAdapter::FrequencyPreset> parseFrequencyPresetsText (
    QString const& text)
{
  std::vector<FT2LinkQmlAdapter::FrequencyPreset> presets;
  QStringList const entries = text.split (
      QRegularExpression {QStringLiteral ("[,;\\n]+")},
      Qt::SkipEmptyParts);
  for (QString const& entry : entries)
    {
      QStringList const fields = entry.split (QLatin1Char ('|'));
      qint64 const hz = parseFrequencyHz (fields.value (0));
      if (hz <= 0)
        {
          continue;
        }
      QString const band = fields.size () > 1
          ? fields.value (1).simplified ().left (24)
          : QString {};
      QString const label = fields.size () > 2
          ? fields.mid (2).join (QStringLiteral (" ")).simplified ().left (64)
          : QString {};
      bool duplicate = false;
      for (FT2LinkQmlAdapter::FrequencyPreset const& preset : presets)
        {
          duplicate = duplicate || preset.dialFrequencyHz == hz;
        }
      if (!duplicate)
        {
          presets.push_back ({hz, band, label});
        }
      if (presets.size () >= 32u)
        {
          break;
        }
    }
  return presets;
}

std::vector<FT2LinkQmlAdapter::AllowedQsyRange> parseAllowedQsyRangesText (
    QString const& text)
{
  std::vector<FT2LinkQmlAdapter::AllowedQsyRange> ranges;
  QStringList const entries = text.split (
      QRegularExpression {QStringLiteral ("[,;\\n]+")},
      Qt::SkipEmptyParts);
  for (QString const& entry : entries)
    {
      QStringList const fields = entry.split (QLatin1Char ('|'));
      QRegularExpressionMatch const match =
          QRegularExpression {
            QStringLiteral ("([0-9.]+)\\s*[-:]\\s*([0-9.]+)")
          }.match (fields.value (0));
      if (!match.hasMatch ())
        {
          continue;
        }
      qint64 fromHz = parseFrequencyHz (match.captured (1));
      qint64 toHz = parseFrequencyHz (match.captured (2));
      if (fromHz <= 0 || toHz <= 0)
        {
          continue;
        }
      if (fromHz > toHz)
        {
          std::swap (fromHz, toHz);
        }
      QString const label = fields.size () > 1
          ? fields.mid (1).join (QStringLiteral (" ")).simplified ().left (64)
          : QString {};
      ranges.push_back ({fromHz, toHz, label});
      if (ranges.size () >= 32u)
        {
          break;
        }
    }
  return ranges;
}

std::vector<FT2LinkQmlAdapter::FrequencyScheduleEntry> parseFrequencyScheduleText (
    QString const& text)
{
  std::vector<FT2LinkQmlAdapter::FrequencyScheduleEntry> entries;
  QStringList const rows = text.split (
      QRegularExpression {QStringLiteral ("[,;\\n]+")},
      Qt::SkipEmptyParts);
  for (QString const& row : rows)
    {
      QStringList const fields = row.split (QLatin1Char ('|'));
      if (fields.size () < 3)
        {
          continue;
        }
      QStringList const rangeParts = fields.value (0).split (
          QRegularExpression {QStringLiteral ("\\s*[-:]\\s*")},
          Qt::SkipEmptyParts);
      if (rangeParts.size () != 2)
        {
          continue;
        }
      int const startMinute = parseUtcMinute (rangeParts.value (0));
      int const endMinute = parseUtcMinute (rangeParts.value (1));
      qint64 const hz = parseFrequencyHz (fields.value (2));
      if (startMinute < 0 || endMinute < 0 || hz <= 0)
        {
          continue;
        }
      FT2LinkQmlAdapter::FrequencyScheduleEntry entry;
      entry.startMinute = startMinute;
      entry.endMinute = endMinute;
      entry.action = sanitizedScheduleAction (fields.value (1));
      entry.dialFrequencyHz = hz;
      entry.label = fields.size () > 3
          ? fields.value (3).simplified ().left (64)
          : QString {};
      entry.cqType = fields.size () > 4
          ? sanitizedCqType (fields.value (4))
          : QStringLiteral ("CQ");
      entries.push_back (entry);
      if (entries.size () >= 48u)
        {
          break;
        }
    }
  return entries;
}

QString frequencyPresetsToText (
    std::vector<FT2LinkQmlAdapter::FrequencyPreset> const& presets)
{
  QStringList entries;
  for (FT2LinkQmlAdapter::FrequencyPreset const& preset : presets)
    {
      entries.push_back (QStringLiteral ("%1|%2|%3")
                             .arg (preset.dialFrequencyHz)
                             .arg (preset.band, preset.label));
    }
  return entries.join (QStringLiteral (", "));
}

QString allowedQsyRangesToText (
    std::vector<FT2LinkQmlAdapter::AllowedQsyRange> const& ranges)
{
  QStringList entries;
  for (FT2LinkQmlAdapter::AllowedQsyRange const& range : ranges)
    {
      entries.push_back (QStringLiteral ("%1-%2|%3")
                             .arg (range.fromHz)
                             .arg (range.toHz)
                             .arg (range.label));
    }
  return entries.join (QStringLiteral (", "));
}

QString frequencyScheduleToText (
    std::vector<FT2LinkQmlAdapter::FrequencyScheduleEntry> const& schedule)
{
  QStringList entries;
  for (FT2LinkQmlAdapter::FrequencyScheduleEntry const& entry : schedule)
    {
      entries.push_back (QStringLiteral ("%1|%2|%3|%4|%5")
                             .arg (utcMinuteRangeText (entry.startMinute,
                                                       entry.endMinute),
                                   sanitizedScheduleAction (entry.action),
                                   QString::number (entry.dialFrequencyHz),
                                   entry.label.trimmed (),
                                   sanitizedCqType (entry.cqType)));
    }
  return entries.join (QStringLiteral (", "));
}

QString adifField (QString const& name, QString const& value)
{
  QString clean = value;
  clean.replace (QLatin1Char ('\r'), QLatin1Char (' '));
  clean.replace (QLatin1Char ('\n'), QLatin1Char (' '));
  clean = clean.trimmed ();
  if (name.trimmed ().isEmpty () || clean.isEmpty ())
    {
      return {};
    }

  return QStringLiteral ("<%1:%2>%3 ")
      .arg (name.trimmed ().toUpper (),
            QString::number (clean.toUtf8 ().size ()),
            clean);
}

QVariantMap broadcastMap (QString const& fromCall,
                          QString const& text,
                          QString const& source,
                          QStringList const& alertTags,
                          quint64 atMs)
{
  QVariantMap map;
  map.insert (QStringLiteral ("fromCall"), fromCall);
  map.insert (QStringLiteral ("text"), text);
  map.insert (QStringLiteral ("source"), source);
  map.insert (QStringLiteral ("alertTags"), alertTags);
  map.insert (QStringLiteral ("alert"), !alertTags.isEmpty ());
  map.insert (QStringLiteral ("atMs"),
              QVariant::fromValue<qulonglong> (atMs));
  return map;
}

QVariantMap alertMap (QString const& fromCall,
                      QString const& text,
                      QString const& source,
                      QString const& tag,
                      quint64 atMs)
{
  QVariantMap map;
  map.insert (QStringLiteral ("fromCall"), fromCall);
  map.insert (QStringLiteral ("text"), text);
  map.insert (QStringLiteral ("source"), source);
  map.insert (QStringLiteral ("tag"), tag);
  map.insert (QStringLiteral ("atMs"),
              QVariant::fromValue<qulonglong> (atMs));
  return map;
}

QString normalizeCallsign (QString const& value)
{
  return value.trimmed ().toUpper ();
}

bool callsignMatches (QString const& lhs, QString const& rhs)
{
  return !rhs.isEmpty () && normalizeCallsign (lhs) == rhs;
}

QString sanitizedBlockedCall (QString const& value)
{
  QString call = value.simplified ().toUpper ();
  call.replace (QRegularExpression {QStringLiteral ("[^A-Z0-9/.-]")},
                QString {});
  return call.left (24);
}

QStringList parseBlockedCallsText (QString const& callsText)
{
  QStringList parsed;
  QStringList const parts = callsText.split (
      QRegularExpression {QStringLiteral ("[,;\\s]+")},
      Qt::SkipEmptyParts);
  for (QString const& part : parts)
    {
      QString const call = sanitizedBlockedCall (part);
      if (call.size () >= 2 && !parsed.contains (call))
        {
          parsed.push_back (call);
        }
      if (parsed.size () >= 200)
        {
          break;
        }
    }
  parsed.sort ();
  return parsed;
}

QVariantMap timelineEntryMap (QString const& type,
                              QString const& label,
                              QString const& direction,
                              QString const& peer,
                              QString const& state,
                              QString const& summary,
                              QString const& details,
                              quint64 atMs,
                              quint32 id = 0u,
                              quint16 sessionId = 0u)
{
  QVariantMap map;
  map.insert (QStringLiteral ("type"), type);
  map.insert (QStringLiteral ("label"), label);
  map.insert (QStringLiteral ("direction"), direction);
  map.insert (QStringLiteral ("peer"), normalizeCallsign (peer));
  map.insert (QStringLiteral ("state"), state);
  map.insert (QStringLiteral ("summary"), summary.simplified ());
  map.insert (QStringLiteral ("details"), details.simplified ());
  map.insert (QStringLiteral ("atMs"),
              QVariant::fromValue<qulonglong> (atMs));
  map.insert (QStringLiteral ("id"), id);
  map.insert (QStringLiteral ("sessionId"), sessionId);
  return map;
}

QString compactFieldsSummary (QVariantMap const& fields)
{
  QStringList parts;
  for (QVariantMap::const_iterator it = fields.begin ();
       it != fields.end ();
       ++it)
    {
      QString const key = it.key ().trimmed ();
      QString const value = it.value ().toString ().simplified ();
      if (!key.isEmpty () && !value.isEmpty ())
        {
          parts.push_back (QStringLiteral ("%1=%2").arg (key, value));
        }
    }
  return parts.join (QStringLiteral ("; ")).left (180);
}

QStringList splitWords (QString const& text)
{
  return text.simplified ().toUpper ().split (
      QLatin1Char (' '), Qt::SkipEmptyParts);
}

bool parsePathFinderRequest (QString const& text,
                             QString* targetCall,
                             QString* requestorCall)
{
  QStringList const parts = splitWords (text);
  if (parts.size () >= 2 && parts[0] == QStringLiteral ("P?"))
    {
      if (targetCall)
        {
          *targetCall = normalizeCallsign (parts[1]);
        }
      if (requestorCall)
        {
          *requestorCall = parts.size () >= 3 ? normalizeCallsign (parts[2])
                                              : QString {};
        }
      return true;
    }
  if (parts.size () >= 2 && parts[0] == QStringLiteral ("PATH?"))
    {
      if (targetCall)
        {
          *targetCall = normalizeCallsign (parts[1]);
        }
      QString requestor;
      for (qsizetype i = 2; i + 1 < parts.size (); ++i)
        {
          if (parts[i] == QStringLiteral ("FROM"))
            {
              requestor = normalizeCallsign (parts[i + 1]);
              break;
            }
        }
      if (requestorCall)
        {
          *requestorCall = requestor;
        }
      return true;
    }
  return false;
}

bool parsePathFinderResponse (QString const& text,
                              QString* targetCall,
                              QString* viaCall,
                              QString* locator,
                              int* ageMinutes)
{
  QStringList const parts = splitWords (text);
  if (parts.size () >= 3 && parts[0] == QStringLiteral ("P!"))
    {
      if (targetCall)
        {
          *targetCall = normalizeCallsign (parts[1]);
        }
      if (viaCall)
        {
          *viaCall = normalizeCallsign (parts[2]);
        }
      QString parsedLocator;
      int parsedAge = -1;
      for (qsizetype i = 3; i < parts.size (); ++i)
        {
          QString const part = parts[i];
          if (part.endsWith (QLatin1Char ('M')))
            {
              bool ok = false;
              int const value = part.left (part.size () - 1).toInt (&ok);
              if (ok)
                {
                  parsedAge = value;
                  continue;
                }
            }
          if (parsedLocator.isEmpty ())
            {
              parsedLocator = part;
            }
        }
      if (locator)
        {
          *locator = parsedLocator;
        }
      if (ageMinutes)
        {
          *ageMinutes = parsedAge;
        }
      return true;
    }
  if (parts.size () >= 4 && parts[0] == QStringLiteral ("PATH!"))
    {
      QString parsedVia;
      QString parsedLocator;
      int parsedAge = -1;
      for (qsizetype i = 2; i + 1 < parts.size (); ++i)
        {
          if (parts[i] == QStringLiteral ("VIA"))
            {
              parsedVia = normalizeCallsign (parts[i + 1]);
            }
          else if (parts[i] == QStringLiteral ("LOC"))
            {
              parsedLocator = parts[i + 1];
            }
          else if (parts[i] == QStringLiteral ("AGE"))
            {
              QString value = parts[i + 1];
              if (value.endsWith (QLatin1Char ('M')))
                {
                  value.chop (1);
                }
              bool ok = false;
              int const age = value.toInt (&ok);
              if (ok)
                {
                  parsedAge = age;
                }
            }
        }
      if (targetCall)
        {
          *targetCall = normalizeCallsign (parts[1]);
        }
      if (viaCall)
        {
          *viaCall = parsedVia;
        }
      if (locator)
        {
          *locator = parsedLocator;
        }
      if (ageMinutes)
        {
          *ageMinutes = parsedAge;
        }
      return !parsedVia.isEmpty ();
    }
  return false;
}

bool isHexText (QString const& text)
{
  if (text.size () % 2 != 0)
    {
      return false;
    }
  for (QChar const ch : text)
    {
      ushort const code = ch.toLower ().unicode ();
      if (!ch.isDigit () && (code < 'a' || code > 'f'))
        {
          return false;
        }
    }
  return true;
}

QString mailboxEncodePart (QString const& value)
{
  return QString::fromLatin1 (value.toUtf8 ().toHex ());
}

QString mailboxDecodePart (QString const& value)
{
  if (!isHexText (value))
    {
      return {};
    }
  return QString::fromUtf8 (QByteArray::fromHex (value.toLatin1 ()));
}

QString mailboxFlags (bool urgent, bool emcomm)
{
  QString flags;
  if (urgent)
    {
      flags += QLatin1Char ('U');
    }
  if (emcomm)
    {
      flags += QLatin1Char ('E');
    }
  return flags.isEmpty () ? QStringLiteral ("N") : flags;
}

bool mailboxFlagSet (QString const& flags, QChar flag)
{
  return flags.trimmed ().toUpper ().contains (flag);
}

QString makeMailboxEnvelope (QString const& toCall,
                             QString const& fromCall,
                             QString const& subject,
                             QString const& body,
                             bool urgent = false,
                             bool emcomm = false)
{
  if (urgent || emcomm)
    {
      return QStringLiteral ("FT2M2|%1|%2|%3|%4|%5")
          .arg (normalizeCallsign (toCall),
                normalizeCallsign (fromCall),
                mailboxFlags (urgent, emcomm),
                mailboxEncodePart (subject.trimmed ()),
                mailboxEncodePart (body.trimmed ()));
    }
  return QStringLiteral ("FT2M1|%1|%2|%3|%4")
      .arg (normalizeCallsign (toCall),
            normalizeCallsign (fromCall),
            mailboxEncodePart (subject.trimmed ()),
            mailboxEncodePart (body.trimmed ()));
}

QString makeRelayMailboxEnvelope (QString const& toCall,
                                  QString const& viaCall,
                                  QString const& fromCall,
                                  QString const& subject,
                                  QString const& body,
                                  bool urgent = false,
                                  bool emcomm = false,
                                  int hopCount = 1)
{
  int const hops = std::clamp (hopCount, 1, kMaxRelayHopCount);
  return QStringLiteral ("FT2RLY1|%1|%2|%3|%4|%5|%6|%7")
      .arg (normalizeCallsign (toCall),
            normalizeCallsign (viaCall),
            normalizeCallsign (fromCall),
            mailboxFlags (urgent, emcomm),
            QString::number (hops),
            mailboxEncodePart (subject.trimmed ()),
            mailboxEncodePart (body.trimmed ()));
}

bool parseMailboxEnvelope (QString const& text,
                           QString* toCall,
                           QString* fromCall,
                           QString* subject,
                           QString* body,
                           bool* urgent = nullptr,
                           bool* emcomm = nullptr)
{
  QStringList const parts = text.trimmed ().split (QLatin1Char ('|'));
  bool const v1 = parts.size () == 5 && parts[0] == QStringLiteral ("FT2M1");
  bool const v2 = parts.size () == 6 && parts[0] == QStringLiteral ("FT2M2");
  if (!v1 && !v2)
    {
      return false;
    }

  int const subjectIndex = v2 ? 4 : 3;
  int const bodyIndex = v2 ? 5 : 4;
  QString const decodedSubject = mailboxDecodePart (parts[subjectIndex]);
  QString const decodedBody = mailboxDecodePart (parts[bodyIndex]);
  if (!isHexText (parts[subjectIndex]) || !isHexText (parts[bodyIndex])
      || decodedBody.trimmed ().isEmpty ())
    {
      return false;
    }

  if (toCall)
    {
      *toCall = normalizeCallsign (parts[1]);
    }
  if (fromCall)
    {
      *fromCall = normalizeCallsign (parts[2]);
    }
  if (subject)
    {
      *subject = decodedSubject.trimmed ();
    }
  if (body)
    {
      *body = decodedBody.trimmed ();
    }
  if (urgent)
    {
      *urgent = v2 && mailboxFlagSet (parts[3], QLatin1Char ('U'));
    }
  if (emcomm)
    {
      *emcomm = v2 && mailboxFlagSet (parts[3], QLatin1Char ('E'));
    }
  return true;
}

bool parseRelayMailboxEnvelope (QString const& text,
                                QString* toCall,
                                QString* viaCall,
                                QString* fromCall,
                                QString* subject,
                                QString* body,
                                bool* urgent = nullptr,
                                bool* emcomm = nullptr,
                                int* hopCount = nullptr)
{
  QStringList const parts = text.trimmed ().split (QLatin1Char ('|'));
  if (parts.size () != 8 || parts[0] != QStringLiteral ("FT2RLY1"))
    {
      return false;
    }

  bool hopsOk = false;
  int const parsedHops = parts[5].toInt (&hopsOk);
  QString const decodedSubject = mailboxDecodePart (parts[6]);
  QString const decodedBody = mailboxDecodePart (parts[7]);
  if (!hopsOk || parsedHops < 1 || parsedHops > kMaxRelayHopCount
      || !isHexText (parts[6]) || !isHexText (parts[7])
      || decodedBody.trimmed ().isEmpty ())
    {
      return false;
    }

  if (toCall)
    {
      *toCall = normalizeCallsign (parts[1]);
    }
  if (viaCall)
    {
      *viaCall = normalizeCallsign (parts[2]);
    }
  if (fromCall)
    {
      *fromCall = normalizeCallsign (parts[3]);
    }
  if (subject)
    {
      *subject = decodedSubject.trimmed ();
    }
  if (body)
    {
      *body = decodedBody.trimmed ();
    }
  if (urgent)
    {
      *urgent = mailboxFlagSet (parts[4], QLatin1Char ('U'));
    }
  if (emcomm)
    {
      *emcomm = mailboxFlagSet (parts[4], QLatin1Char ('E'));
    }
  if (hopCount)
    {
      *hopCount = parsedHops;
    }
  return true;
}

QString makeFormEnvelope (QString const& toCall,
                          QString const& fromCall,
                          QString const& formType,
                          QVariantMap const& fields)
{
  QJsonObject const object = QJsonObject::fromVariantMap (fields);
  QByteArray const json = QJsonDocument (object).toJson (QJsonDocument::Compact);
  return QStringLiteral ("FT2FORM1|%1|%2|%3|%4")
      .arg (normalizeCallsign (toCall),
            normalizeCallsign (fromCall),
            formType.trimmed ().toUpper (),
            QString::fromLatin1 (json.toHex ()));
}

bool parseFormEnvelope (QString const& text,
                        QString* toCall,
                        QString* fromCall,
                        QString* formType,
                        QVariantMap* fields)
{
  QStringList const parts = text.trimmed ().split (QLatin1Char ('|'));
  if (parts.size () != 5 || parts[0] != QStringLiteral ("FT2FORM1"))
    {
      return false;
    }
  if (!isHexText (parts[4]))
    {
      return false;
    }

  QByteArray const jsonBytes = QByteArray::fromHex (parts[4].toLatin1 ());
  QJsonParseError parseError;
  QJsonDocument const document = QJsonDocument::fromJson (jsonBytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject ())
    {
      return false;
    }

  if (toCall)
    {
      *toCall = normalizeCallsign (parts[1]);
    }
  if (fromCall)
    {
      *fromCall = normalizeCallsign (parts[2]);
    }
  if (formType)
    {
      *formType = parts[3].trimmed ().toUpper ();
    }
  if (fields)
    {
      *fields = document.object ().toVariantMap ();
    }
  return true;
}

QString sha256Hex (QByteArray const& bytes)
{
  return QString::fromLatin1 (
      QCryptographicHash::hash (bytes, QCryptographicHash::Sha256).toHex ());
}

QString makeFileEnvelope (QString const& toCall,
                          QString const& fromCall,
                          QString const& fileName,
                          QString const& content)
{
  QByteArray const contentBytes = content.toUtf8 ();
  return QStringLiteral ("FT2FILE1|%1|%2|%3|%4|%5|%6")
      .arg (normalizeCallsign (toCall),
            normalizeCallsign (fromCall),
            mailboxEncodePart (fileName.trimmed ()),
            QString::number (contentBytes.size ()),
            sha256Hex (contentBytes),
            QString::fromLatin1 (contentBytes.toBase64 ()));
}

bool parseFileEnvelope (QString const& text,
                        QString* toCall,
                        QString* fromCall,
                        QString* fileName,
                        QString* content,
                        QString* sha256)
{
  QStringList const parts = text.trimmed ().split (QLatin1Char ('|'));
  if (parts.size () != 7 || parts[0] != QStringLiteral ("FT2FILE1"))
    {
      return false;
    }
  if (!isHexText (parts[3]))
    {
      return false;
    }

  bool sizeOk = false;
  int const declaredSize = parts[4].toInt (&sizeOk);
  QByteArray const contentBytes = QByteArray::fromBase64 (parts[6].toLatin1 ());
  QString const computedSha = sha256Hex (contentBytes);
  if (!sizeOk || declaredSize < 0 || contentBytes.size () != declaredSize
      || computedSha.compare (parts[5], Qt::CaseInsensitive) != 0)
    {
      return false;
    }

  if (toCall)
    {
      *toCall = normalizeCallsign (parts[1]);
    }
  if (fromCall)
    {
      *fromCall = normalizeCallsign (parts[2]);
    }
  if (fileName)
    {
      *fileName = mailboxDecodePart (parts[3]).trimmed ();
    }
  if (content)
    {
      *content = QString::fromUtf8 (contentBytes);
    }
  if (sha256)
    {
      *sha256 = computedSha;
    }
  return true;
}

QString makeBulletinEnvelope (QString const& fromCall,
                              QString const& group,
                              QString const& title,
                              QString const& body)
{
  QString normalizedGroup = group.trimmed ().toUpper ();
  if (normalizedGroup.isEmpty ())
    {
      normalizedGroup = QStringLiteral ("ALL");
    }
  return QStringLiteral ("FT2BBS1|%1|%2|%3|%4")
      .arg (normalizeCallsign (fromCall),
            normalizedGroup,
            mailboxEncodePart (title.trimmed ()),
            mailboxEncodePart (body.trimmed ()));
}

bool parseBulletinEnvelope (QString const& text,
                            QString* fromCall,
                            QString* group,
                            QString* title,
                            QString* body)
{
  QStringList const parts = text.trimmed ().split (QLatin1Char ('|'));
  if (parts.size () != 5 || parts[0] != QStringLiteral ("FT2BBS1")
      || !isHexText (parts[3]) || !isHexText (parts[4]))
    {
      return false;
    }

  QString const decodedTitle = mailboxDecodePart (parts[3]).trimmed ();
  QString const decodedBody = mailboxDecodePart (parts[4]).trimmed ();
  if (decodedBody.isEmpty ())
    {
      return false;
    }
  if (fromCall)
    {
      *fromCall = normalizeCallsign (parts[1]);
    }
  if (group)
    {
      QString normalizedGroup = parts[2].trimmed ().toUpper ();
      *group = normalizedGroup.isEmpty ()
          ? QStringLiteral ("ALL")
          : normalizedGroup;
    }
  if (title)
    {
      *title = decodedTitle.isEmpty ()
          ? QStringLiteral ("Bulletin")
          : decodedTitle;
    }
  if (body)
    {
      *body = decodedBody;
    }
  return true;
}

QVariantMap mailboxMap (quint32 id,
                        QString const& direction,
                        QString const& fromCall,
                        QString const& toCall,
                        QString const& subject,
                        QString const& body,
                        QString const& state,
                        quint64 atMs,
                        quint64 updatedAtMs,
                        quint64 relayNotifiedAtMs = 0,
                        bool urgent = false,
                        bool emcomm = false,
                        QString const& relayViaCall = QString {},
                        int relayHopCount = 0,
                        QString const& relayProtocol = QString {},
                        QString const& emailGatewayState = QString {},
                        QString const& emailGatewayDetail = QString {},
                        quint64 emailGatewayAtMs = 0)
{
  QVariantMap map;
  map.insert (QStringLiteral ("id"), id);
  map.insert (QStringLiteral ("direction"), direction);
  map.insert (QStringLiteral ("fromCall"), fromCall);
  map.insert (QStringLiteral ("toCall"), toCall);
  map.insert (QStringLiteral ("subject"), subject);
  map.insert (QStringLiteral ("body"), body);
  map.insert (QStringLiteral ("state"), state);
  map.insert (QStringLiteral ("atMs"),
              QVariant::fromValue<qulonglong> (atMs));
  map.insert (QStringLiteral ("updatedAtMs"),
              QVariant::fromValue<qulonglong> (updatedAtMs));
  map.insert (QStringLiteral ("relayNotifiedAtMs"),
              QVariant::fromValue<qulonglong> (relayNotifiedAtMs));
  map.insert (QStringLiteral ("relayReady"),
              state == QStringLiteral ("Relay ready"));
  map.insert (QStringLiteral ("urgent"), urgent);
  map.insert (QStringLiteral ("emcomm"), emcomm);
  map.insert (QStringLiteral ("relayViaCall"),
              normalizeCallsign (relayViaCall));
  map.insert (QStringLiteral ("relayHopCount"),
              std::clamp (relayHopCount, 0, kMaxRelayHopCount));
  map.insert (QStringLiteral ("relayProtocol"),
              relayProtocol.trimmed ().isEmpty ()
              ? QStringLiteral ("MAIL")
              : relayProtocol.trimmed ().toUpper ());
  map.insert (QStringLiteral ("relayEnvelope"),
              relayProtocol.trimmed ().toUpper () == QStringLiteral ("FT2RLY1"));
  map.insert (QStringLiteral ("emailGatewayState"),
              emailGatewayState.trimmed ());
  map.insert (QStringLiteral ("emailGatewayDetail"),
              emailGatewayDetail.trimmed ());
  map.insert (QStringLiteral ("emailGatewayAtMs"),
              QVariant::fromValue<qulonglong> (emailGatewayAtMs));
  map.insert (QStringLiteral ("unread"),
              direction == QStringLiteral ("Incoming")
              && state != QStringLiteral ("Read"));
  QStringList priority;
  if (urgent)
    {
      priority.push_back (QStringLiteral ("URGENT"));
    }
  if (emcomm)
    {
      priority.push_back (QStringLiteral ("EMCOMM"));
    }
  map.insert (QStringLiteral ("priority"),
              priority.isEmpty ()
              ? QStringLiteral ("NORMAL")
              : priority.join (QStringLiteral ("+")));
  return map;
}

QString firstEmailAddress (QString const& text)
{
  QRegularExpression expression (
      QStringLiteral ("[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,}"),
      QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatch const match = expression.match (text);
  return match.hasMatch () ? match.captured (0).trimmed () : QString {};
}

QString sanitizedEmailAddress (QString const& value)
{
  QString const email = firstEmailAddress (value);
  return email.left (254);
}

QString safeMailHeaderValue (QString value)
{
  value.replace (QRegularExpression {QStringLiteral ("[\\r\\n]+")},
                 QStringLiteral (" "));
  return value.simplified ();
}

QString encodedMailHeaderValue (QString const& value)
{
  QString const clean = safeMailHeaderValue (value);
  bool ascii = true;
  for (QChar const ch : clean)
    {
      if (ch.unicode () > 0x7fu)
        {
          ascii = false;
          break;
        }
    }
  if (ascii)
    {
      return clean;
    }
  return QStringLiteral ("=?UTF-8?B?%1?=").arg (
      QString::fromLatin1 (clean.toUtf8 ().toBase64 ()));
}

QString safeEmailFileNamePart (QString value)
{
  value = value.simplified ().toUpper ();
  value.replace (QRegularExpression {QStringLiteral ("[^A-Z0-9_.-]")},
                 QStringLiteral ("_"));
  value = value.left (48);
  return value.isEmpty () ? QStringLiteral ("MAIL") : value;
}

QString rfc2822DateText (quint64 atMs)
{
  QDateTime at = atMs > 0u
      ? QDateTime::fromMSecsSinceEpoch (static_cast<qint64> (atMs),
                                        QTimeZone(QByteArrayLiteral("UTC"))).toUTC ()
      : QDateTime::currentDateTimeUtc ();
  if (!at.isValid ())
    {
      at = QDateTime::currentDateTimeUtc ();
    }
  return at.toString (Qt::RFC2822Date);
}

QString plainEmailBody (QString const& direction,
                        QString const& fromCall,
                        QString const& toCall,
                        QString const& subject,
                        QString const& body,
                        QString const& state,
                        QString const& priority,
                        quint64 updatedAtMs)
{
  QString text;
  text += QStringLiteral ("FT2-Link VMail gateway\r\n");
  text += QStringLiteral ("Direction: %1\r\n").arg (direction);
  text += QStringLiteral ("From: %1\r\n").arg (fromCall);
  text += QStringLiteral ("To: %1\r\n").arg (toCall);
  text += QStringLiteral ("State: %1\r\n").arg (state);
  text += QStringLiteral ("Priority: %1\r\n").arg (priority);
  text += QStringLiteral ("UTC: %1\r\n").arg (utcMinuteText (updatedAtMs));
  text += QStringLiteral ("Subject: %1\r\n\r\n").arg (subject);
  text += body;
  if (!text.endsWith (QStringLiteral ("\r\n")))
    {
      text += QStringLiteral ("\r\n");
    }
  return text;
}

QVariantMap beaconHistoryMap (QString const& direction,
                              QString const& call,
                              QString const& locator,
                              QString const& name,
                              QString const& profileName,
                              bool cq,
                              QString const& cqType,
                              QString const& cqLocator,
                              int cqSlotId,
                              int cqSlotSizeHz,
                              QString const& source,
                              quint64 atMs)
{
  QVariantMap map;
  map.insert (QStringLiteral ("direction"), direction);
  map.insert (QStringLiteral ("call"), call);
  map.insert (QStringLiteral ("locator"), locator);
  map.insert (QStringLiteral ("name"), name);
  map.insert (QStringLiteral ("profileName"), profileName);
  map.insert (QStringLiteral ("cq"), cq);
  map.insert (QStringLiteral ("cqType"),
              cqType.isEmpty () ? QStringLiteral ("CQ") : cqType);
  map.insert (QStringLiteral ("cqLocator"), cqLocator);
  map.insert (QStringLiteral ("cqSlotId"), cqSlotId);
  map.insert (QStringLiteral ("cqSlotSizeHz"), cqSlotSizeHz);
  QString const slotLabel = cqSlotId > 0
      ? QStringLiteral ("S+%1").arg (cqSlotId)
      : (cqSlotId < 0 ? QStringLiteral ("S%1").arg (cqSlotId)
                      : QString {});
  map.insert (QStringLiteral ("cqSlotLabel"), slotLabel);
  map.insert (QStringLiteral ("source"), source);
  map.insert (QStringLiteral ("atMs"),
              QVariant::fromValue<qulonglong> (atMs));
  return map;
}

QVariantMap clusterLastHeardMap (QString const& call,
                                 QString const& locator,
                                 QString const& name,
                                 QString const& profileName,
                                 QString const& event,
                                 QString const& source,
                                 QString const& nodeId,
                                 QString const& band,
                                 qint64 dialFrequencyHz,
                                 bool cq,
                                 QString const& cqType,
                                 quint64 firstHeardMs,
                                 quint64 lastHeardMs,
                                 int heardCount)
{
  QVariantMap map;
  map.insert (QStringLiteral ("call"), call);
  map.insert (QStringLiteral ("locator"), locator);
  map.insert (QStringLiteral ("name"), name);
  map.insert (QStringLiteral ("profileName"), profileName);
  map.insert (QStringLiteral ("event"), event);
  map.insert (QStringLiteral ("source"), source);
  map.insert (QStringLiteral ("nodeId"), sanitizedClusterNodeId (nodeId));
  map.insert (QStringLiteral ("band"), sanitizedClusterBand (band));
  map.insert (QStringLiteral ("dialFrequencyHz"),
              QVariant::fromValue<qlonglong> (dialFrequencyHz));
  map.insert (QStringLiteral ("cq"), cq);
  map.insert (QStringLiteral ("cqType"),
              cqType.trimmed ().isEmpty () ? QStringLiteral ("CQ")
                                           : cqType.trimmed ().toUpper ());
  map.insert (QStringLiteral ("firstHeardMs"),
              QVariant::fromValue<qulonglong> (firstHeardMs));
  map.insert (QStringLiteral ("lastHeardMs"),
              QVariant::fromValue<qulonglong> (lastHeardMs));
  map.insert (QStringLiteral ("heardCount"), heardCount);
  map.insert (QStringLiteral ("lastHeardUtc"), utcMinuteText (lastHeardMs));
  map.insert (QStringLiteral ("key"),
              clusterKey (nodeId, band, dialFrequencyHz, call));
  return map;
}

QVariantMap bulletinMap (quint32 id,
                         QString const& direction,
                         QString const& fromCall,
                         QString const& group,
                         QString const& title,
                         QString const& body,
                         QString const& state,
                         quint64 atMs,
                         quint64 updatedAtMs)
{
  QVariantMap map;
  map.insert (QStringLiteral ("id"), id);
  map.insert (QStringLiteral ("direction"), direction);
  map.insert (QStringLiteral ("fromCall"), fromCall);
  map.insert (QStringLiteral ("group"), group);
  map.insert (QStringLiteral ("title"), title);
  map.insert (QStringLiteral ("body"), body);
  map.insert (QStringLiteral ("state"), state);
  map.insert (QStringLiteral ("atMs"),
              QVariant::fromValue<qulonglong> (atMs));
  map.insert (QStringLiteral ("updatedAtMs"),
              QVariant::fromValue<qulonglong> (updatedAtMs));
  return map;
}

QVariantMap contactHistoryMap (QString const& call,
                               QString const& locator,
                               QString const& name,
                               QString const& tag,
                               QString const& comment,
                               QString const& lastEvent,
                               QString const& lastProfileName,
                               quint64 firstHeardMs,
                               quint64 lastHeardMs,
                               int qsoCount,
                               int messageCount,
                               int mailCount,
                               int formCount,
                               int fileCount,
                               int bulletinCount,
                               int broadcastCount,
                               int alertCount)
{
  QVariantMap map;
  map.insert (QStringLiteral ("call"), call);
  map.insert (QStringLiteral ("locator"), locator);
  map.insert (QStringLiteral ("name"), name);
  map.insert (QStringLiteral ("tag"), tag);
  map.insert (QStringLiteral ("comment"), comment);
  map.insert (QStringLiteral ("lastEvent"), lastEvent);
  map.insert (QStringLiteral ("lastProfileName"), lastProfileName);
  map.insert (QStringLiteral ("firstHeardMs"),
              QVariant::fromValue<qulonglong> (firstHeardMs));
  map.insert (QStringLiteral ("lastHeardMs"),
              QVariant::fromValue<qulonglong> (lastHeardMs));
  map.insert (QStringLiteral ("qsoCount"), qsoCount);
  map.insert (QStringLiteral ("messageCount"), messageCount);
  map.insert (QStringLiteral ("mailCount"), mailCount);
  map.insert (QStringLiteral ("formCount"), formCount);
  map.insert (QStringLiteral ("fileCount"), fileCount);
  map.insert (QStringLiteral ("bulletinCount"), bulletinCount);
  map.insert (QStringLiteral ("broadcastCount"), broadcastCount);
  map.insert (QStringLiteral ("alertCount"), alertCount);
  return map;
}

QVariantMap qsoLogMap (quint16 sessionId,
                       QString const& remoteCall,
                       QString const& profileName,
                       QString const& rateName,
                       QString const& state,
                       QString const& lastEvent,
                       quint64 openedAtMs,
                       quint64 updatedAtMs,
                       quint64 closedAtMs,
                       int messageCount)
{
  QVariantMap map;
  map.insert (QStringLiteral ("sessionId"), sessionId);
  map.insert (QStringLiteral ("remoteCall"), remoteCall);
  map.insert (QStringLiteral ("profileName"), profileName);
  map.insert (QStringLiteral ("rateName"), rateName);
  map.insert (QStringLiteral ("state"), state);
  map.insert (QStringLiteral ("lastEvent"), lastEvent);
  map.insert (QStringLiteral ("openedAtMs"),
              QVariant::fromValue<qulonglong> (openedAtMs));
  map.insert (QStringLiteral ("updatedAtMs"),
              QVariant::fromValue<qulonglong> (updatedAtMs));
  map.insert (QStringLiteral ("closedAtMs"),
              QVariant::fromValue<qulonglong> (closedAtMs));
  map.insert (QStringLiteral ("messageCount"), messageCount);
  return map;
}

QString sanitizedLogbookTarget (QString const& target)
{
  QString clean = target.trimmed ().toUpper ();
  if (clean == QStringLiteral ("UDP") || clean == QStringLiteral ("TCP")
      || clean == QStringLiteral ("N1MM") || clean == QStringLiteral ("QRZ")
      || clean == QStringLiteral ("CLOUDLOG") || clean == QStringLiteral ("ALL"))
    {
      return clean;
    }
  return QStringLiteral ("ALL");
}

QString sanitizedLogbookState (QString const& state)
{
  QString clean = state.trimmed ().toUpper ();
  if (clean == QStringLiteral ("QUEUED"))
    {
      return QStringLiteral ("Queued");
    }
  if (clean == QStringLiteral ("SUBMITTED"))
    {
      return QStringLiteral ("Submitted");
    }
  if (clean == QStringLiteral ("SENT"))
    {
      return QStringLiteral ("Sent");
    }
  if (clean == QStringLiteral ("FAILED"))
    {
      return QStringLiteral ("Failed");
    }
  if (clean == QStringLiteral ("SKIPPED"))
    {
      return QStringLiteral ("Skipped");
    }
  return QStringLiteral ("Queued");
}

QVariantMap logbookUploadMap (quint32 id,
                              quint16 sessionId,
                              QString const& remoteCall,
                              QString const& target,
                              QString const& state,
                              QString const& detail,
                              QString const& adif,
                              QString const& adifSha256,
                              quint64 queuedAtMs,
                              quint64 updatedAtMs)
{
  QVariantMap map;
  map.insert (QStringLiteral ("id"), id);
  map.insert (QStringLiteral ("sessionId"), sessionId);
  map.insert (QStringLiteral ("remoteCall"), remoteCall);
  map.insert (QStringLiteral ("target"), sanitizedLogbookTarget (target));
  map.insert (QStringLiteral ("state"), sanitizedLogbookState (state));
  map.insert (QStringLiteral ("detail"), detail.left (240));
  map.insert (QStringLiteral ("adif"), adif);
  map.insert (QStringLiteral ("adifSha256"), adifSha256);
  map.insert (QStringLiteral ("queuedAtMs"),
              QVariant::fromValue<qulonglong> (queuedAtMs));
  map.insert (QStringLiteral ("updatedAtMs"),
              QVariant::fromValue<qulonglong> (updatedAtMs));
  map.insert (QStringLiteral ("queuedUtc"), utcMinuteText (queuedAtMs));
  map.insert (QStringLiteral ("updatedUtc"), utcMinuteText (updatedAtMs));
  return map;
}

QVariantMap pingMap (QString const& direction,
                     QString const& remoteCall,
                     QString const& state,
                     quint16 token,
                     quint64 atMs,
                     quint64 rttMs)
{
  QVariantMap map;
  map.insert (QStringLiteral ("direction"), direction);
  map.insert (QStringLiteral ("remoteCall"), remoteCall);
  map.insert (QStringLiteral ("state"), state);
  map.insert (QStringLiteral ("token"), token);
  map.insert (QStringLiteral ("atMs"),
              QVariant::fromValue<qulonglong> (atMs));
  map.insert (QStringLiteral ("rttMs"),
              QVariant::fromValue<qulonglong> (rttMs));
  return map;
}

QVariantMap pathReportMap (quint32 id,
                           QString const& direction,
                           QString const& remoteCall,
                           QString const& locator,
                           bool snrValid,
                           int snrDb,
                           bool qualityValid,
                           double quality,
                           double frequencyOffsetHz,
                           QString const& profileName,
                           QString const& rateName,
                           QString const& source,
                           quint64 atMs)
{
  QDateTime const at =
      QDateTime::fromMSecsSinceEpoch (static_cast<qint64> (atMs),
                                      QTimeZone(QByteArrayLiteral("UTC"))).toUTC ();
  QVariantMap map;
  map.insert (QStringLiteral ("id"), id);
  map.insert (QStringLiteral ("direction"), direction);
  map.insert (QStringLiteral ("remoteCall"), remoteCall);
  map.insert (QStringLiteral ("locator"), locator);
  map.insert (QStringLiteral ("snrValid"), snrValid);
  map.insert (QStringLiteral ("snrDb"), snrDb);
  map.insert (QStringLiteral ("qualityValid"), qualityValid);
  map.insert (QStringLiteral ("quality"), quality);
  map.insert (QStringLiteral ("frequencyOffsetHz"), frequencyOffsetHz);
  map.insert (QStringLiteral ("profileName"), profileName);
  map.insert (QStringLiteral ("rateName"), rateName);
  map.insert (QStringLiteral ("source"), source);
  map.insert (QStringLiteral ("bandName"), QStringLiteral ("--"));
  map.insert (QStringLiteral ("dialFrequencyHz"),
              QVariant::fromValue<qulonglong> (0u));
  map.insert (QStringLiteral ("atMs"),
              QVariant::fromValue<qulonglong> (atMs));
  map.insert (QStringLiteral ("atUtc"),
              at.isValid ()
              ? at.toString (QStringLiteral ("yyyy-MM-dd HH:mm'Z'"))
              : QStringLiteral ("--"));
  map.insert (QStringLiteral ("hourUtc"),
              at.isValid () ? at.time ().hour () : -1);
  return map;
}

std::vector<int> snrReportsInText (QString const& text)
{
  std::vector<int> reports;
  QRegularExpression expression (
      QStringLiteral ("<R\\s*([+-]?\\d{1,2})>"),
      QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatchIterator it = expression.globalMatch (text);
  while (it.hasNext ())
    {
      QRegularExpressionMatch const match = it.next ();
      bool ok = false;
      int const value = match.captured (1).toInt (&ok);
      if (ok)
        {
          reports.push_back (std::max (-99, std::min (99, value)));
        }
    }
  return reports;
}

QVariantMap formMap (quint32 id,
                     QString const& direction,
                     QString const& fromCall,
                     QString const& toCall,
                     QString const& formType,
                     QVariantMap const& fields,
                     QString const& state,
                     quint64 atMs,
                     quint64 updatedAtMs)
{
  QVariantMap map;
  map.insert (QStringLiteral ("id"), id);
  map.insert (QStringLiteral ("direction"), direction);
  map.insert (QStringLiteral ("fromCall"), fromCall);
  map.insert (QStringLiteral ("toCall"), toCall);
  map.insert (QStringLiteral ("formType"), formType);
  map.insert (QStringLiteral ("fields"), fields);
  map.insert (QStringLiteral ("state"), state);
  map.insert (QStringLiteral ("atMs"),
              QVariant::fromValue<qulonglong> (atMs));
  map.insert (QStringLiteral ("updatedAtMs"),
              QVariant::fromValue<qulonglong> (updatedAtMs));
  return map;
}

QVariantMap fileTransferMap (quint32 id,
                             QString const& direction,
                             QString const& fromCall,
                             QString const& toCall,
                             QString const& fileName,
                             QString const& content,
                             QString const& sha256,
                             QString const& state,
                             quint64 atMs,
                             quint64 updatedAtMs)
{
  QByteArray const bytes = content.toUtf8 ();
  QVariantMap map;
  map.insert (QStringLiteral ("id"), id);
  map.insert (QStringLiteral ("direction"), direction);
  map.insert (QStringLiteral ("fromCall"), fromCall);
  map.insert (QStringLiteral ("toCall"), toCall);
  map.insert (QStringLiteral ("fileName"), fileName);
  map.insert (QStringLiteral ("content"), content);
  map.insert (QStringLiteral ("sizeBytes"), bytes.size ());
  map.insert (QStringLiteral ("sha256"), sha256);
  map.insert (QStringLiteral ("state"), state);
  map.insert (QStringLiteral ("atMs"),
              QVariant::fromValue<qulonglong> (atMs));
  map.insert (QStringLiteral ("updatedAtMs"),
              QVariant::fromValue<qulonglong> (updatedAtMs));
  return map;
}

QString compactTextPreview (QString const& text, int maxChars = 96)
{
  QString preview = text.simplified ();
  if (preview.size () > maxChars)
    {
      preview = preview.left (maxChars - 3) + QStringLiteral ("...");
    }
  return preview;
}

QString utcMinuteText (quint64 atMs)
{
  if (atMs == 0u)
    {
      return QStringLiteral ("--");
    }
  return QDateTime::fromMSecsSinceEpoch (
      static_cast<qint64> (atMs), QTimeZone(QByteArrayLiteral("UTC")))
      .toUTC ()
      .toString (QStringLiteral ("yyyy-MM-dd HH:mm'Z'"));
}

QString variantListJsonText (QVariantList const& list)
{
  return QString::fromUtf8 (
      QJsonDocument {QJsonArray::fromVariantList (list)}
          .toJson (QJsonDocument::Indented));
}

bool isImageFileName (QString const& fileName)
{
  QString const lower = fileName.trimmed ().toLower ();
  return lower.endsWith (QStringLiteral (".png"))
      || lower.endsWith (QStringLiteral (".jpg"))
      || lower.endsWith (QStringLiteral (".jpeg"))
      || lower.endsWith (QStringLiteral (".gif"))
      || lower.endsWith (QStringLiteral (".bmp"))
      || lower.endsWith (QStringLiteral (".webp"));
}

QVariantMap formTemplateMap (QString const& id,
                             QString const& label,
                             QString const& fields)
{
  QVariantMap map;
  map.insert (QStringLiteral ("id"), id);
  map.insert (QStringLiteral ("label"), label);
  map.insert (QStringLiteral ("fields"), fields);
  return map;
}

void replaceToken (QString* text, QString const& token, QString const& value)
{
  if (!text)
    {
      return;
    }
  text->replace (token, value, Qt::CaseInsensitive);
}

bool containsControlTag (QString const& text, QString const& tag)
{
  return text.contains (tag, Qt::CaseInsensitive);
}

QString firstControlTagValue (QString const& text, QString const& name)
{
  QRegularExpression expression (
      QStringLiteral ("<%1:([^>]{1,96})>").arg (
          QRegularExpression::escape (name)),
      QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatch const match = expression.match (text);
  return match.hasMatch () ? match.captured (1).trimmed () : QString {};
}

QStringList controlTagValues (QString const& text, QString const& name)
{
  QRegularExpression expression (
      QStringLiteral ("<%1:([^>]{1,256})>").arg (
          QRegularExpression::escape (name)),
      QRegularExpression::CaseInsensitiveOption);
  QStringList values;
  QRegularExpressionMatchIterator it = expression.globalMatch (text);
  while (it.hasNext ())
    {
      QRegularExpressionMatch const match = it.next ();
      QString const value = match.captured (1).trimmed ();
      if (!value.isEmpty ())
        {
          values.push_back (value);
        }
    }
  return values;
}

bool containsPlainCommand (QString const& text, QString const& command)
{
  QRegularExpression expression (
      QStringLiteral ("(^|[^A-Z0-9])%1(?=$|[^A-Z0-9])").arg (
          QRegularExpression::escape (command)),
      QRegularExpression::CaseInsensitiveOption);
  return expression.match (text).hasMatch ();
}

QString firstControlTagMatch (QString const& text, QString const& pattern)
{
  QRegularExpression expression (
      pattern, QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatch const match = expression.match (text);
  return match.hasMatch () ? match.captured (1).trimmed () : QString {};
}

QVariantMap transportMetricsMap (WideAudioPipelineResult const& result)
{
  decodium::ft2link::AudioThroughputMetrics const& metrics =
      result.throughput;

  QVariantMap map;
  map.insert (QStringLiteral ("complete"), result.complete);
  map.insert (QStringLiteral ("failed"), result.failed);
  map.insert (QStringLiteral ("error"), QString::fromStdString (result.error));
  map.insert (QStringLiteral ("profile"),
              static_cast<int> (metrics.profile));
  map.insert (QStringLiteral ("profileName"),
              QString::fromStdString (decodium::ft2link::profileName (
                  metrics.profile)));
  map.insert (QStringLiteral ("payloadBytes"),
              numericVariant (metrics.payloadBytes));
  map.insert (QStringLiteral ("burstCount"),
              numericVariant (metrics.burstCount));
  map.insert (QStringLiteral ("ackBurstCount"),
              numericVariant (metrics.ackBurstCount));
  map.insert (QStringLiteral ("decodedBurstCount"),
              numericVariant (metrics.decodedBurstCount));
  map.insert (QStringLiteral ("decodedAckBurstCount"),
              numericVariant (metrics.decodedAckBurstCount));
  map.insert (QStringLiteral ("droppedBurstCount"),
              numericVariant (metrics.droppedBurstCount));
  map.insert (QStringLiteral ("droppedAckBurstCount"),
              numericVariant (metrics.droppedAckBurstCount));
  map.insert (QStringLiteral ("retryBurstCount"),
              numericVariant (metrics.retryBurstCount));
  map.insert (QStringLiteral ("sessionDurationMs"),
              numericVariant (metrics.sessionDurationMs));
  map.insert (QStringLiteral ("dataTransmitMs"),
              numericVariant (metrics.dataTransmitMs));
  map.insert (QStringLiteral ("ackTransmitMs"),
              numericVariant (metrics.ackTransmitMs));
  map.insert (QStringLiteral ("activeTransmitMs"),
              numericVariant (metrics.activeTransmitMs));
  map.insert (QStringLiteral ("effectivePayloadBps"),
              metrics.effectivePayloadBytesPerSecond);
  map.insert (QStringLiteral ("effectivePayloadBitps"),
              metrics.effectivePayloadBitsPerSecond);
  map.insert (QStringLiteral ("activePayloadBps"),
              metrics.activePayloadBytesPerSecond);
  map.insert (QStringLiteral ("activePayloadBitps"),
              metrics.activePayloadBitsPerSecond);
  map.insert (QStringLiteral ("channelUtilization"),
              metrics.channelUtilization);
  map.insert (QStringLiteral ("totalSamples"),
              numericVariant (result.totalSamples));
  map.insert (QStringLiteral ("estimatedAudioSeconds"),
              static_cast<double> (result.totalSamples) / 12000.0);
  return map;
}

QVariantMap w2300LiveMetricsMap (
    std::uint16_t sessionId,
    decodium::ft2link::W2300DecodeMetrics const& metrics,
    W2300RateMode nextRateMode,
    quint64 nowMs)
{
  QVariantMap map;
  map.insert (QStringLiteral ("liveRx"), true);
  map.insert (QStringLiteral ("profile"),
              static_cast<int> (Profile::Wide2300));
  map.insert (QStringLiteral ("profileName"),
              QStringLiteral ("W2300"));
  map.insert (QStringLiteral ("sessionId"), sessionId);
  map.insert (QStringLiteral ("updatedAtMs"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (nowMs)));
  map.insert (QStringLiteral ("w2300RateMode"),
              static_cast<int> (metrics.rateMode));
  map.insert (QStringLiteral ("w2300RateModeName"),
              QString::fromLatin1 (decodium::ft2link::w2300RateModeName (
                  metrics.rateMode)));
  map.insert (QStringLiteral ("nextW2300RateMode"),
              static_cast<int> (nextRateMode));
  map.insert (QStringLiteral ("nextW2300RateModeName"),
              QString::fromLatin1 (decodium::ft2link::w2300RateModeName (
                  nextRateMode)));
  map.insert (QStringLiteral ("quality"), metrics.quality);
  map.insert (QStringLiteral ("estimatedFrequencyOffsetHz"),
              metrics.estimatedFrequencyOffsetHz);
  map.insert (QStringLiteral ("estimatedCenterFrequencyHz"),
              metrics.estimatedCenterFrequencyHz);
  map.insert (QStringLiteral ("packetBytes"),
              numericVariant (metrics.packetBytes));
  map.insert (QStringLiteral ("symbolCount"),
              numericVariant (metrics.symbolCount));
  map.insert (QStringLiteral ("repetitionFactor"), metrics.repetitionFactor);
  map.insert (QStringLiteral ("rawBitRate"), metrics.rawBitRate);
  map.insert (QStringLiteral ("payloadBitRate"), metrics.payloadBitRate);
  return map;
}

QVariantMap radioTxPlanMap (WideTxAudioPlan const& plan, bool armed)
{
  QVariantMap map;
  map.insert (QStringLiteral ("ok"), plan.ok);
  map.insert (QStringLiteral ("armed"), armed);
  map.insert (QStringLiteral ("error"), QString::fromStdString (plan.error));
  map.insert (QStringLiteral ("profile"),
              static_cast<int> (plan.profile));
  map.insert (QStringLiteral ("profileName"),
              QString::fromStdString (decodium::ft2link::profileName (
                  plan.profile)));
  map.insert (QStringLiteral ("w2300RateMode"),
              static_cast<int> (plan.w2300RateMode));
  map.insert (QStringLiteral ("w2300RateModeName"),
              QString::fromLatin1 (decodium::ft2link::w2300RateModeName (
                  plan.w2300RateMode)));
  map.insert (QStringLiteral ("sampleRate"), plan.sampleRate);
  map.insert (QStringLiteral ("payloadBytes"),
              numericVariant (plan.throughput.payloadBytes));
  map.insert (QStringLiteral ("frameCount"),
              numericVariant (plan.frames.size ()));
  map.insert (QStringLiteral ("burstCount"),
              numericVariant (plan.bursts.size ()));
  map.insert (QStringLiteral ("sampleCount"),
              numericVariant (plan.samples.size ()));
  map.insert (QStringLiteral ("totalSamples"),
              numericVariant (plan.totalSamples));
  map.insert (QStringLiteral ("audioSeconds"),
              plan.sampleRate > 0.0
              ? static_cast<double> (plan.totalSamples) / plan.sampleRate
              : 0.0);
  map.insert (QStringLiteral ("activePayloadBps"),
              plan.throughput.activePayloadBytesPerSecond);
  map.insert (QStringLiteral ("activePayloadBitps"),
              plan.throughput.activePayloadBitsPerSecond);
  map.insert (QStringLiteral ("format"),
              QStringLiteral ("float32 mono"));
  return map;
}

bool buildAckAudio (Frame const& ack,
                    W2300RateMode w2300RateMode,
                    QVector<float>* samples,
                    QVariantMap* plan,
                    QString* error)
{
  if (!samples || !plan)
    {
      if (error)
        {
          *error = QStringLiteral ("missing FT2-Link ACK audio output");
        }
      return false;
    }
  if (ack.type != decodium::ft2link::FrameType::Ack)
    {
      if (error)
        {
          *error = QStringLiteral ("FT2-Link ACK audio requires an ACK frame");
        }
      return false;
    }
  if (ack.profile != Profile::Wide500 && ack.profile != Profile::Wide2300)
    {
      if (error)
        {
          *error = QStringLiteral ("FT2-Link ACK audio requires W500 or W2300");
        }
      return false;
    }

  std::string buildError;
  std::size_t sampleCount = 0;
  if (ack.profile == Profile::Wide2300)
    {
      decodium::ft2link::W2300WaveformConfig config;
      config.sampleRate = 48000.0;
      config.rateMode = w2300RateMode;
      decodium::ft2link::W2300TxAudioBuffer txAudio {480, 480, 0};
      decodium::ft2link::W2300AudioBurstTrace trace;
      if (!txAudio.appendFrame (ack, 1, config, &trace, &buildError))
        {
          if (error)
            {
              *error = QString::fromStdString (buildError);
            }
          return false;
        }
      *samples = toSampleVector (txAudio.samples ());
      sampleCount = trace.sampleCount;
    }
  else
    {
      decodium::ft2link::W500WaveformConfig config;
      config.sampleRate = 48000.0;
      decodium::ft2link::W500TxAudioBuffer txAudio {480, 480, 0};
      decodium::ft2link::W500AudioBurstTrace trace;
      if (!txAudio.appendFrame (ack, 1, config, &trace, &buildError))
        {
          if (error)
            {
              *error = QString::fromStdString (buildError);
            }
          return false;
        }
      *samples = toSampleVector (txAudio.samples ());
      sampleCount = trace.sampleCount;
    }

  if (samples->isEmpty ())
    {
      if (error)
        {
          *error = QStringLiteral ("FT2-Link ACK audio produced no samples");
        }
      return false;
    }

  QVariantMap ackPlan;
  ackPlan.insert (QStringLiteral ("ok"), true);
  ackPlan.insert (QStringLiteral ("armed"), false);
  ackPlan.insert (QStringLiteral ("autoAck"), true);
  ackPlan.insert (QStringLiteral ("kind"), QStringLiteral ("ACK"));
  ackPlan.insert (QStringLiteral ("profile"), static_cast<int> (ack.profile));
  ackPlan.insert (QStringLiteral ("profileName"),
                  QString::fromStdString (decodium::ft2link::profileName (
                      ack.profile)));
  ackPlan.insert (QStringLiteral ("w2300RateMode"),
                  static_cast<int> (w2300RateMode));
  ackPlan.insert (QStringLiteral ("w2300RateModeName"),
                  QString::fromLatin1 (decodium::ft2link::w2300RateModeName (
                      w2300RateMode)));
  ackPlan.insert (QStringLiteral ("sessionId"), ack.sessionId);
  ackPlan.insert (QStringLiteral ("ackBase"), ack.ackBase);
  ackPlan.insert (QStringLiteral ("ackBitmap"), ack.ackBitmap);
  ackPlan.insert (QStringLiteral ("frameCount"), 1);
  ackPlan.insert (QStringLiteral ("burstCount"), 1);
  ackPlan.insert (QStringLiteral ("sampleRate"), 48000.0);
  ackPlan.insert (QStringLiteral ("sampleCount"),
                  QVariant::fromValue<qulonglong> (
                      static_cast<qulonglong> (samples->size ())));
  ackPlan.insert (QStringLiteral ("totalSamples"),
                  QVariant::fromValue<qulonglong> (
                      static_cast<qulonglong> (samples->size ())));
  ackPlan.insert (QStringLiteral ("burstSamples"),
                  QVariant::fromValue<qulonglong> (
                      static_cast<qulonglong> (sampleCount)));
  ackPlan.insert (QStringLiteral ("audioSeconds"),
                  static_cast<double> (samples->size ()) / 48000.0);
  ackPlan.insert (QStringLiteral ("format"), QStringLiteral ("float32 mono"));
  *plan = ackPlan;
  return true;
}

bool buildNarrowControlAudio (Frame const& frame,
                              QString const& kind,
                              QVector<float>* samples,
                              QVariantMap* plan,
                              QString* error)
{
  if (!samples || !plan)
    {
      if (error)
        {
          *error = QStringLiteral ("missing FT2-Link control audio output");
        }
      return false;
    }
  if (frame.profile != Profile::Narrow)
    {
      if (error)
        {
          *error = QStringLiteral ("FT2-Link control audio requires NARROW");
        }
      return false;
    }

  decodium::ft2link::NarrowWaveformConfig config;
  config.sampleRate = 48000.0;
  std::string buildError;
  std::vector<float> const wave =
      decodium::ft2link::generateNarrowFrameWaveform (
          frame, config, &buildError);
  if (wave.empty ())
    {
      if (error)
        {
          *error = buildError.empty ()
              ? QStringLiteral ("FT2-Link control audio produced no samples")
              : QString::fromStdString (buildError);
        }
      return false;
    }

  *samples = toSampleVector (wave);

  QVariantMap controlPlan;
  controlPlan.insert (QStringLiteral ("ok"), true);
  controlPlan.insert (QStringLiteral ("armed"), false);
  controlPlan.insert (QStringLiteral ("autoAck"),
                      frame.type == decodium::ft2link::FrameType::HelloAck
                      || frame.type == decodium::ft2link::FrameType::PingAck);
  controlPlan.insert (QStringLiteral ("kind"), kind);
  controlPlan.insert (QStringLiteral ("profile"), static_cast<int> (frame.profile));
  controlPlan.insert (QStringLiteral ("profileName"),
                      QString::fromStdString (decodium::ft2link::profileName (
                          frame.profile)));
  controlPlan.insert (QStringLiteral ("frameType"),
                      static_cast<int> (frame.type));
  controlPlan.insert (QStringLiteral ("frameTypeName"),
                      QString::fromStdString (decodium::ft2link::frameTypeName (
                          frame.type)));
  if (frame.type == decodium::ft2link::FrameType::Beacon)
    {
      LinkCapabilities ignoredCapabilities;
      decodium::ft2link::HandshakeIdentity beaconIdentity;
      bool parsedCq = false;
      std::string ignoredError;
      decodium::ft2link::parseBeaconFrame (
          frame, &ignoredCapabilities, &beaconIdentity, &parsedCq, &ignoredError);
      bool const cq = parsedCq;
      controlPlan.insert (QStringLiteral ("cq"),
                          cq);
      controlPlan.insert (QStringLiteral ("cqType"),
                          cq ? cqTypeNameFromCode (frame.ackBitmap)
                             : QString {});
      controlPlan.insert (QStringLiteral ("cqLocator"),
                          cq ? QString::fromStdString (beaconIdentity.locator)
                             : QString {});
    }
  controlPlan.insert (QStringLiteral ("sessionId"), frame.sessionId);
  controlPlan.insert (QStringLiteral ("sequence"), frame.sequence);
  controlPlan.insert (QStringLiteral ("ackBase"), frame.ackBase);
  controlPlan.insert (QStringLiteral ("ackBitmap"), frame.ackBitmap);
  controlPlan.insert (QStringLiteral ("frameCount"), 1);
  controlPlan.insert (QStringLiteral ("burstCount"), 1);
  controlPlan.insert (QStringLiteral ("sampleRate"), config.sampleRate);
  controlPlan.insert (QStringLiteral ("sampleCount"),
                      QVariant::fromValue<qulonglong> (
                          static_cast<qulonglong> (samples->size ())));
  controlPlan.insert (QStringLiteral ("totalSamples"),
                      QVariant::fromValue<qulonglong> (
                          static_cast<qulonglong> (samples->size ())));
  controlPlan.insert (QStringLiteral ("audioSeconds"),
                      static_cast<double> (samples->size ()) / config.sampleRate);
  controlPlan.insert (QStringLiteral ("format"), QStringLiteral ("float32 mono"));
  *plan = controlPlan;
  return true;
}
}

FT2LinkQmlAdapter::FT2LinkQmlAdapter (QObject* parent)
  : QObject {parent}
{
  if (QCoreApplication::instance ())
    {
      QCoreApplication::instance ()->installEventFilter (this);
    }
  m_lastOperatorActivityMs =
      static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ());

  auto persistOnChange = [this] {
    persistLocalStore ();
  };
  connect (this, &FT2LinkQmlAdapter::broadcastsChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::alertsChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::mailboxChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::formsChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::fileTransfersChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::bulletinsChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::qsoLogChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::logbookOutboxChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::contactHistoryChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::pingLogChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::pathReportsChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::beaconHistoryChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::clusterLastHeardChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::frequencyPlanChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::presenceChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::qsoAutomationChanged,
           this, persistOnChange);
  connect (this, &FT2LinkQmlAdapter::blockListChanged,
           this, persistOnChange);

  m_radioTxQueueTimer.setSingleShot (true);
  connect (&m_radioTxQueueTimer, &QTimer::timeout, this, [this] {
    drainRadioTxQueue (
        static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ()));
  });

  m_liveOutboundRetryTimer.setSingleShot (true);
  connect (&m_liveOutboundRetryTimer, &QTimer::timeout, this, [this] {
    runLiveOutboundRetryCheck ();
  });

  m_autoBeaconTimer.setSingleShot (true);
  connect (&m_autoBeaconTimer, &QTimer::timeout, this, [this] {
    runAutoBeaconTick ();
  });

  m_liveChannelTimer.setSingleShot (true);
  connect (&m_liveChannelTimer, &QTimer::timeout, this, [this] {
    if (!m_liveChannelBusy)
      {
        return;
      }
    m_liveChannelBusy = false;
    emit liveChannelChanged ();
    drainRadioTxQueue (
        static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ()));
  });

  m_frequencyPresets = defaultFrequencyPresets ();
  m_allowedQsyRanges = defaultAllowedQsyRanges ();
  m_localStorePath = defaultLocalStorePath ();
  if (shouldAutoLoadLocalStore ())
    {
      m_localStorePersistenceEnabled = true;
      loadLocalStore ();
    }
}

FT2LinkQmlAdapter::~FT2LinkQmlAdapter ()
{
  if (QCoreApplication::instance ())
    {
      QCoreApplication::instance ()->removeEventFilter (this);
    }
}

bool FT2LinkQmlAdapter::eventFilter (QObject* watched, QEvent* event)
{
  Q_UNUSED (watched);
  if (!event)
    {
      return false;
    }

  bool inputEvent = false;
  switch (event->type ())
    {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TabletPress:
    case QEvent::TabletMove:
    case QEvent::TabletRelease:
      inputEvent = true;
      break;
    default:
      break;
    }

  if (inputEvent
      && recordOperatorActivity (
          static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ())))
    {
      emit presenceChanged ();
      persistLocalStore ();
    }

  return QObject::eventFilter (watched, event);
}

bool FT2LinkQmlAdapter::recordOperatorActivity (quint64 nowMs)
{
  if (nowMs == 0u)
    {
      nowMs = static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ());
    }

  bool const clearAutoAway = m_autoAwayActivated;
  if (!clearAutoAway
      && m_lastOperatorActivityMs > 0u
      && nowMs >= m_lastOperatorActivityMs
      && nowMs - m_lastOperatorActivityMs < 15000u)
    {
      return false;
    }

  m_lastOperatorActivityMs = nowMs;
  if (clearAutoAway)
    {
      m_autoAwayActivated = false;
      m_awayEnabled = false;
      return true;
    }
  return false;
}

int FT2LinkQmlAdapter::stationCount () const
{
  return knownStationCount ();
}

int FT2LinkQmlAdapter::sessionCount () const
{
  return static_cast<int> (m_model.sessions ().size ());
}

quint16 FT2LinkQmlAdapter::activeSessionId () const
{
  return m_activeSessionId;
}

QString FT2LinkQmlAdapter::lastError () const
{
  return m_lastError;
}

QString FT2LinkQmlAdapter::transportState () const
{
  return m_transportState;
}

bool FT2LinkQmlAdapter::transportBusy () const
{
  return m_transportBusy;
}

QVariantMap FT2LinkQmlAdapter::lastTransportMetrics () const
{
  return m_lastTransportMetrics;
}

bool FT2LinkQmlAdapter::radioTxArmed () const
{
  return m_radioTxArmed;
}

QVariantMap FT2LinkQmlAdapter::lastRadioTxPlan () const
{
  return m_lastRadioTxPlan;
}

bool FT2LinkQmlAdapter::autoBeaconEnabled () const
{
  return m_autoBeaconEnabled;
}

int FT2LinkQmlAdapter::autoBeaconIntervalSeconds () const
{
  return m_autoBeaconIntervalSeconds;
}

bool FT2LinkQmlAdapter::autoBeaconCq () const
{
  return m_autoBeaconCq;
}

bool FT2LinkQmlAdapter::liveChannelBusy () const
{
  return isLiveChannelBusy (
      static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ()));
}

double FT2LinkQmlAdapter::liveChannelRms () const
{
  return m_liveChannelRms;
}

double FT2LinkQmlAdapter::liveChannelPeak () const
{
  return m_liveChannelPeak;
}

int FT2LinkQmlAdapter::broadcastCount () const
{
  return static_cast<int> (m_broadcasts.size ());
}

int FT2LinkQmlAdapter::alertCount () const
{
  return static_cast<int> (m_alerts.size ());
}

int FT2LinkQmlAdapter::mailboxCount () const
{
  return static_cast<int> (m_mailbox.size ());
}

int FT2LinkQmlAdapter::mailboxUnreadCount () const
{
  int unread = 0;
  for (MailboxMessage const& message : m_mailbox)
    {
      if (message.direction == QStringLiteral ("Incoming")
          && message.state != QStringLiteral ("Read"))
        {
          ++unread;
        }
    }
  return unread;
}

int FT2LinkQmlAdapter::relayQueueCount () const
{
  int count = 0;
  for (MailboxMessage const& message : m_mailbox)
    {
      bool const relayCandidate =
          message.direction == QStringLiteral ("Parked")
          || message.direction == QStringLiteral ("Relay");
      bool const activeState =
          message.state == QStringLiteral ("Parked")
          || message.state == QStringLiteral ("Relay ready")
          || message.state == QStringLiteral ("Pending relay")
          || message.state == QStringLiteral ("Failed");
      if (relayCandidate && activeState && !message.body.trimmed ().isEmpty ())
        {
          ++count;
        }
    }
  return count;
}

int FT2LinkQmlAdapter::formCount () const
{
  return static_cast<int> (m_forms.size ());
}

int FT2LinkQmlAdapter::fileTransferCount () const
{
  return static_cast<int> (m_fileTransfers.size ());
}

int FT2LinkQmlAdapter::bulletinCount () const
{
  return static_cast<int> (m_bulletins.size ());
}

int FT2LinkQmlAdapter::qsoLogCount () const
{
  return static_cast<int> (m_qsoLog.size ());
}

int FT2LinkQmlAdapter::logbookOutboxCount () const
{
  return static_cast<int> (m_logbookOutbox.size ());
}

int FT2LinkQmlAdapter::contactCount () const
{
  return static_cast<int> (m_contactHistory.size ());
}

int FT2LinkQmlAdapter::pingCount () const
{
  return static_cast<int> (m_pingLog.size ());
}

int FT2LinkQmlAdapter::pathReportCount () const
{
  return static_cast<int> (m_pathReports.size ());
}

int FT2LinkQmlAdapter::beaconHistoryCount () const
{
  return static_cast<int> (m_beaconHistory.size ());
}

int FT2LinkQmlAdapter::clusterLastHeardCount () const
{
  return static_cast<int> (m_clusterLastHeard.size ());
}

int FT2LinkQmlAdapter::frequencyScheduleCount () const
{
  return static_cast<int> (m_frequencySchedule.size ());
}

QString FT2LinkQmlAdapter::localStorePath () const
{
  return resolvedLocalStorePath ();
}

bool FT2LinkQmlAdapter::localStoreLoaded () const
{
  return m_localStoreLoaded;
}

QString FT2LinkQmlAdapter::lastLocalStoreError () const
{
  return m_lastLocalStoreError;
}

bool FT2LinkQmlAdapter::awayEnabled () const
{
  return m_awayEnabled;
}

bool FT2LinkQmlAdapter::awayAcceptsQsy () const
{
  return m_awayAcceptsQsy;
}

QString FT2LinkQmlAdapter::awayMessage () const
{
  return m_awayMessage;
}

bool FT2LinkQmlAdapter::welcomeEnabled () const
{
  return m_welcomeEnabled;
}

QString FT2LinkQmlAdapter::welcomeMessage () const
{
  return m_welcomeMessage;
}

bool FT2LinkQmlAdapter::autoReplyEnabled () const
{
  return m_autoReplyEnabled;
}

bool FT2LinkQmlAdapter::autoAwayEnabled () const
{
  return m_autoAwayEnabled;
}

int FT2LinkQmlAdapter::autoAwayMinutes () const
{
  return m_autoAwayMinutes;
}

bool FT2LinkQmlAdapter::autoAwayActive () const
{
  return m_autoAwayActivated;
}

int FT2LinkQmlAdapter::callIdIntervalMinutes () const
{
  return m_callIdIntervalMinutes;
}

int FT2LinkQmlAdapter::autoDisconnectMinutes () const
{
  return m_autoDisconnectMinutes;
}

bool FT2LinkQmlAdapter::incomingPingsEnabled () const
{
  return m_incomingPingsEnabled;
}

bool FT2LinkQmlAdapter::lastHeardPeekingEnabled () const
{
  return m_lastHeardPeekingEnabled;
}

bool FT2LinkQmlAdapter::lastConnectionsPeekingEnabled () const
{
  return m_lastConnectionsPeekingEnabled;
}

bool FT2LinkQmlAdapter::parkedVmailPeekingEnabled () const
{
  return m_parkedVmailPeekingEnabled;
}

bool FT2LinkQmlAdapter::vmailParkingEnabled () const
{
  return m_vmailParkingEnabled;
}

bool FT2LinkQmlAdapter::snrReportSendingEnabled () const
{
  return m_snrReportSendingEnabled;
}

bool FT2LinkQmlAdapter::verboseSnrAutoAcceptEnabled () const
{
  return m_verboseSnrAutoAcceptEnabled;
}

bool FT2LinkQmlAdapter::infoInquireEnabled () const
{
  return m_infoInquireEnabled;
}

int FT2LinkQmlAdapter::blockedCallCount () const
{
  return m_blockedCalls.size ();
}

int FT2LinkQmlAdapter::typingPeerCount () const
{
  return static_cast<int> (m_typingPeers.size ());
}

void FT2LinkQmlAdapter::setLocalStation (QString const& call,
                                         QString const& locator,
                                         QString const& name)
{
  StationIdentity station;
  station.call = toStdString (call);
  station.locator = toStdString (locator);
  station.name = toStdFreeText (name);
  m_model.setLocalStation (station);
}

void FT2LinkQmlAdapter::setLocalOperatorProfile (QString const& qth,
                                                 QString const& email,
                                                 QString const& ice,
                                                 QString const& rig,
                                                 QString const& antenna,
                                                 QString const& power,
                                                 QString const& gps)
{
  m_localProfile.qth = qth.simplified ().left (64);
  m_localProfile.email = email.simplified ().left (96);
  m_localProfile.ice = ice.simplified ().left (128);
  m_localProfile.rig = rig.simplified ().left (96);
  m_localProfile.antenna = antenna.simplified ().left (96);
  m_localProfile.power = power.simplified ().left (32);
  m_localProfile.gps = gps.simplified ().left (96);
}

void FT2LinkQmlAdapter::setLocalCapabilities (bool supportsW500,
                                              bool supportsW2300,
                                              bool supportsW2300Fast,
                                              bool supportsW2300Robust,
                                              int preferredProfile,
                                              int preferredW2300RateMode)
{
  LinkCapabilities capabilities;
  capabilities.supportsW500 = supportsW500;
  capabilities.supportsW2300 = supportsW2300;
  capabilities.supportsW2300Fast = supportsW2300Fast;
  capabilities.supportsW2300Robust = supportsW2300Robust;
  capabilities.preferredProfile = profileFromInt (preferredProfile);
  capabilities.preferredW2300RateMode = rateModeFromInt (preferredW2300RateMode);
  m_model.setLocalCapabilities (capabilities);
}

bool FT2LinkQmlAdapter::observeStation (QString const& call,
                                        QString const& locator,
                                        QString const& name,
                                        bool cq,
                                        bool supportsW500,
                                        bool supportsW2300,
                                        bool supportsW2300Fast,
                                        bool supportsW2300Robust,
                                        int preferredProfile,
                                        int preferredW2300RateMode,
                                        quint64 heardAtMs)
{
  int const before = knownStationCount ();
  QString const normalizedCall = normalizeCallsign (call);
  if (isCallBlocked (normalizedCall))
    {
      setLastError (QStringLiteral ("FT2-Link blocked callsign %1")
                    .arg (normalizedCall));
      return false;
    }

  StationAdvertisement advertisement;
  advertisement.station.call = toStdString (normalizedCall);
  advertisement.station.locator = toStdString (locator);
  advertisement.station.name = toStdFreeText (name);
  advertisement.cq = cq;
  advertisement.heardAtMs = heardAtMs;
  advertisement.capabilities.supportsW500 = supportsW500;
  advertisement.capabilities.supportsW2300 = supportsW2300;
  advertisement.capabilities.supportsW2300Fast = supportsW2300Fast;
  advertisement.capabilities.supportsW2300Robust = supportsW2300Robust;
  advertisement.capabilities.preferredProfile = profileFromInt (preferredProfile);
  advertisement.capabilities.preferredW2300RateMode =
      rateModeFromInt (preferredW2300RateMode);

  std::string error;
  if (!m_model.observeStation (advertisement, &error))
    {
      setLastError (QString::fromStdString (error));
      return false;
    }

  if (cq)
    {
      ++m_cqsReceived;
    }
  else
    {
      ++m_beaconsReceived;
    }
  recordBeaconHistory (QStringLiteral ("RX"), advertisement,
                       QStringLiteral ("OBS"), heardAtMs);
  recordClusterLastHeard (
      normalizedCall,
      locator,
      name,
      QString::fromStdString (decodium::ft2link::profileName (
          advertisement.capabilities.preferredProfile)),
      cq ? QStringLiteral ("CQ") : QStringLiteral ("heard"),
      QStringLiteral ("OBS"),
      cq,
      QString::fromStdString (advertisement.cqType),
      heardAtMs);
  touchContact (normalizedCall,
                heardAtMs,
                cq ? QStringLiteral ("CQ") : QStringLiteral ("heard"),
                locator,
                name,
                QString::fromStdString (decodium::ft2link::profileName (
                    advertisement.capabilities.preferredProfile)));
  notifyParkedMailboxForCall (normalizedCall, heardAtMs);
  clearLastError ();
  if (knownStationCount () != before)
    {
      emit stationCountChanged ();
    }
  return true;
}

QVariantList FT2LinkQmlAdapter::activeStations (quint64 nowMs,
                                                quint64 maxAgeMs,
                                                bool cqOnly) const
{
  QVariantList list;
  std::vector<StationAdvertisement> active = m_model.activeStations (
      nowMs, maxAgeMs, cqOnly);
  for (StationAdvertisement const& station : active)
    {
      QString const call = normalizeCallsign (
          QString::fromStdString (station.station.call));
      if (isCallBlocked (call))
        {
          continue;
        }
      QString tag;
      std::map<QString, ContactHistory>::const_iterator const contact =
          m_contactHistory.find (call);
      if (contact != m_contactHistory.end ())
        {
          tag = contact->second.tag;
        }
      QVariantMap map = stationMap (station, tag);
      QVariantMap const mailSummary =
          parkedMailboxSummaryForCallInternal (call, nowMs);
      QVariantMap const pathRelay =
          pathRelayCandidateForStation (call, nowMs);
      map.insert (QStringLiteral ("mailboxSummary"), mailSummary);
      map.insert (QStringLiteral ("parkedMailboxCount"),
                  mailSummary.value (QStringLiteral ("count")).toInt ());
      map.insert (QStringLiteral ("relayReadyMailboxCount"),
                  mailSummary.value (QStringLiteral ("relayReadyCount")).toInt ());
      map.insert (QStringLiteral ("urgentMailboxCount"),
                  mailSummary.value (QStringLiteral ("urgentCount")).toInt ());
      map.insert (QStringLiteral ("emcommMailboxCount"),
                  mailSummary.value (QStringLiteral ("emcommCount")).toInt ());
      map.insert (QStringLiteral ("pathRelay"), pathRelay);
      map.insert (QStringLiteral ("pathRelayTarget"),
                  pathRelay.value (QStringLiteral ("targetCall")).toString ());
      map.insert (QStringLiteral ("pathRelayMailboxCount"),
                  pathRelay.value (QStringLiteral ("parkedMailboxCount")).toInt ());
      QVariantMap const relayWorkflow =
          relayWorkflowForStationInternal (call, nowMs);
      map.insert (QStringLiteral ("relayWorkflow"), relayWorkflow);
      map.insert (QStringLiteral ("relayWorkflowLine"),
                  relayWorkflow.value (QStringLiteral ("line")).toString ());
      map.insert (QStringLiteral ("relayWorkflowBadge"),
                  relayWorkflow.value (QStringLiteral ("badge")).toString ());
      map.insert (QStringLiteral ("relayWorkflowReady"),
                  relayWorkflow.value (QStringLiteral ("readyToForward")).toBool ());
      list.push_back (map);
    }
  return list;
}

QByteArray FT2LinkQmlAdapter::startSessionHelloBytes (QString const& remoteCall,
                                                      quint64 nowMs)
{
  int const before = sessionCount ();
  std::string error;
  QString const normalizedRemote = normalizeCallsign (remoteCall);
  if (isCallBlocked (normalizedRemote))
    {
      setLastError (QStringLiteral ("FT2-Link blocked callsign %1")
                    .arg (normalizedRemote));
      return {};
    }
  Frame hello = m_model.startSession (
      toStdString (normalizedRemote), nowMs, &error);
  if (!error.empty ())
    {
      setLastError (QString::fromStdString (error));
      return {};
    }

  m_activeSessionId = hello.sessionId;
  emit activeSessionChanged ();
  if (sessionCount () != before)
    {
      emit sessionCountChanged ();
    }
  emit sessionsChanged ();
  recordQsoSession (hello.sessionId, nowMs, QStringLiteral ("Calling"));
  clearLastError ();
  return toByteArray (decodium::ft2link::serializeFrame (hello));
}

bool FT2LinkQmlAdapter::transmitBeaconRadio (bool cq, quint64 nowMs)
{
  return queueBeaconRadio (cq, nowMs, true, false);
}

bool FT2LinkQmlAdapter::transmitCqSlotRadio (int slotId,
                                             int slotSizeHz,
                                             quint64 nowMs)
{
  return queueBeaconRadio (true, nowMs, true, false, slotId, slotSizeHz);
}

bool FT2LinkQmlAdapter::transmitSpecialCqRadio (QString const& cqType,
                                                QString const& cqLocator,
                                                int slotId,
                                                int slotSizeHz,
                                                quint64 nowMs)
{
  return queueBeaconRadio (
      true,
      nowMs,
      true,
      false,
      slotId,
      slotSizeHz,
      sanitizedCqType (cqType),
      sanitizedCqLocator (cqLocator));
}

int FT2LinkQmlAdapter::beaconCooldownSeconds (quint64 nowMs) const
{
  if (m_lastBeaconTxMs == 0u || nowMs >= m_lastBeaconTxMs + kMinBeaconIntervalMs)
    {
      return 0;
    }
  quint64 const remainingMs = (m_lastBeaconTxMs + kMinBeaconIntervalMs) - nowMs;
  return static_cast<int> ((remainingMs + 999u) / 1000u);
}

bool FT2LinkQmlAdapter::transmitBroadcastRadio (QString const& text, quint64 nowMs)
{
  if (!m_radioTxArmed)
    {
      setLastError (QStringLiteral ("FT2-Link broadcast TX is not armed"));
      return false;
    }

  QString const trimmed = text.trimmed ();
  QByteArray const payloadBytes = trimmed.toUtf8 ();
  if (payloadBytes.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link broadcast message is empty"));
      return false;
    }
  if (static_cast<std::size_t> (payloadBytes.size ())
      > decodium::ft2link::profilePayloadCapacity (Profile::Narrow))
    {
      setLastError (QStringLiteral (
          "FT2-Link broadcast exceeds NARROW payload capacity"));
      return false;
    }

  Frame frame;
  frame.type = decodium::ft2link::FrameType::Broadcast;
  frame.profile = Profile::Narrow;
  frame.flags = decodium::ft2link::FlagEndOfMessage;
  frame.payload.assign (
      reinterpret_cast<std::uint8_t const*> (payloadBytes.constData ()),
      reinterpret_cast<std::uint8_t const*> (payloadBytes.constData ())
          + payloadBytes.size ());

  if (!requestControlRadioTx (frame, QStringLiteral ("BCAST"), QString {}, nowMs))
    {
      return false;
    }

  recordBroadcast (QString::fromStdString (m_model.localStation ().call),
                   trimmed,
                   nowMs,
                   QStringLiteral ("TX"));
  setTransportState (QStringLiteral ("BCAST TX"));
  setRadioTxArmed (false);
  clearLastError ();
  return true;
}

bool FT2LinkQmlAdapter::transmitPathFinderRadio (QString const& targetCall,
                                                 quint64 nowMs)
{
  if (!m_radioTxArmed)
    {
      setLastError (QStringLiteral ("FT2-Link path finder TX is not armed"));
      return false;
    }

  QString const target = normalizeCallsign (targetCall);
  QString const localCall = normalizeCallsign (
      QString::fromStdString (m_model.localStation ().call));
  if (target.isEmpty ())
    {
      setLastError (QStringLiteral (
          "FT2-Link path finder requires a target callsign"));
      return false;
    }
  if (localCall.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link path finder requires MYCALL"));
      return false;
    }

  QString const text = QStringLiteral ("P? %1 %2").arg (target, localCall);
  QByteArray const payloadBytes = text.toUtf8 ();
  if (static_cast<std::size_t> (payloadBytes.size ())
      > decodium::ft2link::profilePayloadCapacity (Profile::Narrow))
    {
      setLastError (QStringLiteral (
          "FT2-Link path finder exceeds NARROW payload capacity"));
      return false;
    }

  Frame frame;
  frame.type = decodium::ft2link::FrameType::Broadcast;
  frame.profile = Profile::Narrow;
  frame.flags = decodium::ft2link::FlagEndOfMessage;
  frame.payload.assign (
      reinterpret_cast<std::uint8_t const*> (payloadBytes.constData ()),
      reinterpret_cast<std::uint8_t const*> (payloadBytes.constData ())
          + payloadBytes.size ());

  if (!requestControlRadioTx (frame, QStringLiteral ("PATH"), target, nowMs))
    {
      return false;
    }

  recordBroadcast (localCall, text, nowMs, QStringLiteral ("TX"));
  setTransportState (QStringLiteral ("PATH TX"));
  setRadioTxArmed (false);
  clearLastError ();
  return true;
}

QVariantMap FT2LinkQmlAdapter::pathFinderCandidate (
    QString const& targetCall,
    quint64 nowMs) const
{
  QString const target = normalizeCallsign (targetCall);
  QVariantMap map;
  map.insert (QStringLiteral ("targetCall"), target);
  map.insert (QStringLiteral ("canRespond"), false);
  if (target.isEmpty ())
    {
      return map;
    }

  std::map<QString, ContactHistory>::const_iterator contact =
      m_contactHistory.find (target);
  if (contact == m_contactHistory.end () || contact->second.lastHeardMs == 0u)
    {
      return map;
    }
  quint64 const ageMs = nowMs >= contact->second.lastHeardMs
      ? nowMs - contact->second.lastHeardMs
      : 0u;
  if (ageMs > kPathFinderMaxAgeMs)
    {
      map.insert (QStringLiteral ("heardAtMs"),
                  QVariant::fromValue<qulonglong> (
                      static_cast<qulonglong> (contact->second.lastHeardMs)));
      map.insert (QStringLiteral ("ageMinutes"),
                  QVariant::fromValue<qulonglong> (
                      static_cast<qulonglong> (ageMs / 60000u)));
      return map;
    }

  map.insert (QStringLiteral ("canRespond"), true);
  map.insert (QStringLiteral ("locator"), contact->second.locator);
  map.insert (QStringLiteral ("name"), contact->second.name);
  map.insert (QStringLiteral ("lastEvent"), contact->second.lastEvent);
  map.insert (QStringLiteral ("heardAtMs"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (contact->second.lastHeardMs)));
  map.insert (QStringLiteral ("ageMinutes"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (ageMs / 60000u)));
  return map;
}

bool FT2LinkQmlAdapter::transmitPathFinderResponseRadio (
    QString const& targetCall,
    quint64 nowMs)
{
  if (!m_radioTxArmed)
    {
      setLastError (QStringLiteral (
          "FT2-Link path finder response TX is not armed"));
      return false;
    }

  QString const target = normalizeCallsign (targetCall);
  QString const localCall = normalizeCallsign (
      QString::fromStdString (m_model.localStation ().call));
  QVariantMap const candidate = pathFinderCandidate (target, nowMs);
  if (target.isEmpty ())
    {
      setLastError (QStringLiteral (
          "FT2-Link path finder response requires a target callsign"));
      return false;
    }
  if (localCall.isEmpty ())
    {
      setLastError (QStringLiteral (
          "FT2-Link path finder response requires MYCALL"));
      return false;
    }
  if (!candidate.value (QStringLiteral ("canRespond")).toBool ())
    {
      setLastError (QStringLiteral (
          "FT2-Link path finder has no recent target history"));
      return false;
    }

  QString text = QStringLiteral ("P! %1 %2").arg (target, localCall);
  QString const locator = candidate.value (QStringLiteral ("locator"))
      .toString ().trimmed ().toUpper ();
  if (!locator.isEmpty ()
      && text.size () + 1 + locator.size ()
          <= static_cast<int> (
              decodium::ft2link::profilePayloadCapacity (Profile::Narrow)))
    {
      text += QLatin1Char (' ');
      text += locator;
    }
  QString const age = QStringLiteral ("%1M").arg (
      std::min<qulonglong> (
          candidate.value (QStringLiteral ("ageMinutes")).toULongLong (),
          999ull));
  if (text.size () + 1 + age.size ()
      <= static_cast<int> (
          decodium::ft2link::profilePayloadCapacity (Profile::Narrow)))
    {
      text += QLatin1Char (' ');
      text += age;
    }

  QByteArray const payloadBytes = text.toUtf8 ();
  Frame frame;
  frame.type = decodium::ft2link::FrameType::Broadcast;
  frame.profile = Profile::Narrow;
  frame.flags = decodium::ft2link::FlagEndOfMessage;
  frame.payload.assign (
      reinterpret_cast<std::uint8_t const*> (payloadBytes.constData ()),
      reinterpret_cast<std::uint8_t const*> (payloadBytes.constData ())
          + payloadBytes.size ());

  if (!requestControlRadioTx (
          frame, QStringLiteral ("PATH_ACK"), target, nowMs))
    {
      return false;
    }

  recordBroadcast (localCall, text, nowMs, QStringLiteral ("TX"));
  setTransportState (QStringLiteral ("PATH RESP TX"));
  setRadioTxArmed (false);
  clearLastError ();
  return true;
}

bool FT2LinkQmlAdapter::transmitPingRadio (QString const& remoteCall,
                                           quint64 nowMs)
{
  if (!m_radioTxArmed)
    {
      setLastError (QStringLiteral ("FT2-Link PING TX is not armed"));
      return false;
    }

  QString const target = normalizeCallsign (remoteCall);
  QString const localCall = normalizeCallsign (
      QString::fromStdString (m_model.localStation ().call));
  if (target.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link PING requires a target callsign"));
      return false;
    }
  if (isCallBlocked (target))
    {
      setLastError (QStringLiteral ("FT2-Link blocked callsign %1")
                    .arg (target));
      return false;
    }
  if (localCall.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link PING requires MYCALL"));
      return false;
    }

  quint16 const token = m_nextPingToken++;
  if (m_nextPingToken == 0u)
    {
      m_nextPingToken = 1u;
    }

  QByteArray const payloadBytes = localCall.toUtf8 ();
  if (static_cast<std::size_t> (payloadBytes.size ())
      > decodium::ft2link::profilePayloadCapacity (Profile::Narrow))
    {
      setLastError (QStringLiteral ("FT2-Link PING callsign is too long"));
      return false;
    }

  Frame frame;
  frame.type = decodium::ft2link::FrameType::Ping;
  frame.profile = Profile::Narrow;
  frame.flags = decodium::ft2link::FlagEndOfMessage;
  frame.sequence = token;
  frame.payload.assign (
      reinterpret_cast<std::uint8_t const*> (payloadBytes.constData ()),
      reinterpret_cast<std::uint8_t const*> (payloadBytes.constData ())
          + payloadBytes.size ());

  if (!requestControlRadioTx (frame, QStringLiteral ("PING"), target, nowMs))
    {
      return false;
    }

  m_pendingPings[token] = std::make_pair (target, nowMs);
  recordPing (QStringLiteral ("Outgoing"),
              target,
              token,
              QStringLiteral ("Pending"),
              nowMs);
  setTransportState (QStringLiteral ("PING TX"));
  setRadioTxArmed (false);
  clearLastError ();
  return true;
}

bool FT2LinkQmlAdapter::configureAutoBeacon (bool enabled,
                                             int intervalSeconds,
                                             bool cq,
                                             quint64 nowMs)
{
  int const clampedInterval = std::max (60, intervalSeconds);
  if (!enabled)
    {
      bool const changed = m_autoBeaconEnabled
          || m_autoBeaconIntervalSeconds != clampedInterval
          || m_autoBeaconCq != cq;
      m_autoBeaconEnabled = false;
      m_autoBeaconIntervalSeconds = clampedInterval;
      m_autoBeaconCq = cq;
      m_autoBeaconTimer.stop ();
      if (changed)
        {
          emit autoBeaconChanged ();
        }
      clearLastError ();
      return true;
    }

  if (m_autoBeaconEnabled)
    {
      bool const changed = m_autoBeaconIntervalSeconds != clampedInterval
          || m_autoBeaconCq != cq;
      m_autoBeaconIntervalSeconds = clampedInterval;
      m_autoBeaconCq = cq;
      if (changed)
        {
          emit autoBeaconChanged ();
        }
      scheduleAutoBeacon (nowMs);
      clearLastError ();
      return true;
    }

  if (!m_radioTxArmed)
    {
      setLastError (QStringLiteral ("FT2-Link auto beacon enable requires ARM"));
      return false;
    }
  if (m_model.localStation ().call.empty ())
    {
      setLastError (QStringLiteral ("FT2-Link auto beacon requires a local callsign"));
      return false;
    }

  m_autoBeaconEnabled = true;
  m_autoBeaconIntervalSeconds = clampedInterval;
  m_autoBeaconCq = cq;
  emit autoBeaconChanged ();

  bool const beaconIntervalReady = m_lastBeaconTxMs == 0u
      || nowMs >= m_lastBeaconTxMs + kMinBeaconIntervalMs;
  if (beaconIntervalReady)
    {
      if (!queueBeaconRadio (cq, nowMs, true, true))
        {
          m_autoBeaconEnabled = false;
          m_autoBeaconTimer.stop ();
          emit autoBeaconChanged ();
          return false;
        }
    }
  else
    {
      setTransportState (cq ? QStringLiteral ("AUTO CQ WAIT")
                            : QStringLiteral ("AUTO BEACON WAIT"));
      setRadioTxArmed (false);
      clearLastError ();
    }
  scheduleAutoBeacon (nowMs);
  return true;
}

bool FT2LinkQmlAdapter::queueBeaconRadio (bool cq,
                                          quint64 nowMs,
                                          bool requireArm,
                                          bool automatic,
                                          int cqSlotId,
                                          int cqSlotSizeHz,
                                          QString const& cqType,
                                          QString const& cqLocator)
{
  if (requireArm && !m_radioTxArmed)
    {
      setLastError (QStringLiteral ("FT2-Link beacon radio TX is not armed"));
      return false;
    }
  if (m_model.localStation ().call.empty ())
    {
      setLastError (QStringLiteral ("FT2-Link beacon requires a local callsign"));
      return false;
    }

  if (automatic
      && m_lastBeaconTxMs != 0u
      && nowMs < m_lastBeaconTxMs + kMinBeaconIntervalMs)
    {
      setTransportState (cq ? QStringLiteral ("CQ WAIT")
                            : QStringLiteral ("BEACON WAIT"));
      setLastError (QStringLiteral ("FT2-Link beacon interval is still active"));
      return false;
    }

  int const normalizedSlotSizeHz = qBound (100, cqSlotSizeHz, 5000);
  int const normalizedSlotId = qBound (-10, cqSlotId, 10);
  QString const normalizedCqType = cq ? sanitizedCqType (cqType)
                                      : QString {};
  QString const normalizedCqLocator = cq ? sanitizedCqLocator (cqLocator)
                                         : QString {};
  Frame const beacon = m_model.makeLocalBeaconFrame (
      cq,
      normalizedSlotId,
      normalizedSlotSizeHz,
      toStdString (normalizedCqType),
      toStdString (normalizedCqLocator));
  QString kind = automatic
      ? (cq ? QStringLiteral ("AUTO CQ") : QStringLiteral ("AUTO BEACON"))
      : (cq ? QStringLiteral ("CQ") : QStringLiteral ("BEACON"));
  if (cq && !normalizedCqType.isEmpty ()
      && normalizedCqType != QStringLiteral ("CQ"))
    {
      kind += QLatin1Char (' ');
      kind += normalizedCqType;
    }
  if (!requestControlRadioTx (beacon, kind, QString {}, nowMs))
    {
      return false;
    }

  if (cq)
    {
      ++m_cqsSent;
    }
  else
    {
      ++m_beaconsSent;
    }
  StationAdvertisement localAdvertisement;
  localAdvertisement.station = m_model.localStation ();
  localAdvertisement.capabilities = m_model.localCapabilities ();
  localAdvertisement.cq = cq;
  localAdvertisement.heardAtMs = nowMs;
  localAdvertisement.cqType = toStdString (normalizedCqType);
  localAdvertisement.cqLocator = toStdString (normalizedCqLocator);
  localAdvertisement.cqSlotId = normalizedSlotId;
  localAdvertisement.cqSlotSizeHz = normalizedSlotSizeHz;
  recordBeaconHistory (QStringLiteral ("TX"), localAdvertisement,
                       automatic ? QStringLiteral ("AUTO")
                                 : QStringLiteral ("MANUAL"),
                       nowMs);
  persistLocalStore ();
  m_lastBeaconTxMs = nowMs;
  setTransportState (automatic
                     ? (cq ? QStringLiteral ("AUTO CQ TX")
                           : QStringLiteral ("AUTO BEACON TX"))
                     : (cq
                        ? (normalizedSlotId == 0
                           ? QStringLiteral ("%1 TX").arg (kind)
                           : QStringLiteral ("%1 SLOT %2 TX")
                                 .arg (kind,
                                       normalizedSlotId > 0
                                       ? QStringLiteral ("+%1").arg (normalizedSlotId)
                                       : QString::number (normalizedSlotId)))
                        : QStringLiteral ("BEACON TX")));
  if (requireArm)
    {
      setRadioTxArmed (false);
    }
  clearLastError ();
  return true;
}

bool FT2LinkQmlAdapter::startSessionRadioHandshake (QString const& remoteCall,
                                                    quint64 nowMs)
{
  if (!m_radioTxArmed)
    {
      setLastError (QStringLiteral ("FT2-Link HELLO radio TX is not armed"));
      return false;
    }

  int const before = sessionCount ();
  std::string error;
  QString const normalizedRemote = normalizeCallsign (remoteCall);
  if (isCallBlocked (normalizedRemote))
    {
      setLastError (QStringLiteral ("FT2-Link blocked callsign %1")
                    .arg (normalizedRemote));
      return false;
    }
  Frame hello = m_model.startSession (
      toStdString (normalizedRemote), nowMs, &error);
  if (!error.empty ())
    {
      setLastError (QString::fromStdString (error));
      return false;
    }

  m_activeSessionId = hello.sessionId;
  emit activeSessionChanged ();
  if (sessionCount () != before)
    {
      emit sessionCountChanged ();
    }
  emit sessionsChanged ();
  recordQsoSession (hello.sessionId, nowMs, QStringLiteral ("HELLO TX"));

  bool const queued = requestControlRadioTx (
      hello, QStringLiteral ("HELLO"), normalizedRemote, nowMs);
  if (queued)
    {
      setTransportState (QStringLiteral ("HELLO TX"));
      setRadioTxArmed (false);
      clearLastError ();
    }
  return queued;
}

bool FT2LinkQmlAdapter::receiveHelloAckBytes (QByteArray const& helloAckBytes,
                                              quint64 nowMs)
{
  Frame helloAck;
  std::string error;
  if (!decodium::ft2link::parseFrame (toBytes (helloAckBytes), &helloAck, &error)
      || !m_model.receiveHelloAck (helloAck, nowMs, &error))
    {
      setLastError (QString::fromStdString (error));
      emit sessionsChanged ();
      return false;
    }

  m_activeSessionId = helloAck.sessionId;
  emit activeSessionChanged ();
  emit sessionsChanged ();
  recordQsoSession (helloAck.sessionId, nowMs, QStringLiteral ("Connected"));
  clearLastError ();
  return true;
}

QByteArray FT2LinkQmlAdapter::answerHelloBytes (QString const& remoteCall,
                                                QByteArray const& helloBytes,
                                                quint64 nowMs)
{
  int const before = sessionCount ();
  Frame hello;
  std::string error;
  if (!decodium::ft2link::parseFrame (toBytes (helloBytes), &hello, &error))
    {
      setLastError (QString::fromStdString (error));
      return {};
    }

  Frame helloAck;
  QString resolvedBlockedRemote;
  if (rejectBlockedHello (
          remoteCall, hello, nowMs, &helloAck, &resolvedBlockedRemote))
    {
      m_activeSessionId = hello.sessionId;
      emit activeSessionChanged ();
      if (sessionCount () != before)
        {
          emit sessionCountChanged ();
        }
      emit sessionsChanged ();
      recordQsoSession (
          hello.sessionId, nowMs, QStringLiteral ("HELLO blocked"));
      setLastError (QStringLiteral ("FT2-Link blocked callsign %1")
                    .arg (resolvedBlockedRemote));
      return toByteArray (decodium::ft2link::serializeFrame (helloAck));
    }

  bool const accepted = m_model.answerHello (
      toStdString (remoteCall), hello, nowMs, &helloAck, &error);
  if (!accepted && helloAck.type != decodium::ft2link::FrameType::HelloAck)
    {
      setLastError (QString::fromStdString (error));
      return {};
    }

  m_activeSessionId = hello.sessionId;
  emit activeSessionChanged ();
  if (sessionCount () != before)
    {
      emit sessionCountChanged ();
    }
  emit sessionsChanged ();
  recordQsoSession (hello.sessionId,
                    nowMs,
                    accepted ? QStringLiteral ("HELLO RX")
                             : QStringLiteral ("HELLO rejected"));
  bool const presenceQueued = accepted
      ? queuePresenceMessage (hello.sessionId, nowMs)
      : true;
  if (accepted && presenceQueued)
    {
      clearLastError ();
    }
  else if (!accepted)
    {
      setLastError (QString::fromStdString (error));
    }
  return toByteArray (decodium::ft2link::serializeFrame (helloAck));
}

bool FT2LinkQmlAdapter::queueOutgoingText (quint16 sessionId,
                                           QString const& text,
                                           quint64 nowMs)
{
  std::string error;
  if (!m_model.queueOutgoingText (
          sessionId, text.trimmed ().toStdString (), nowMs, &error))
    {
      setLastError (QString::fromStdString (error));
      return false;
    }
  emit messagesChanged (sessionId);
  emit sessionsChanged ();
  recordQsoSession (sessionId, nowMs, QStringLiteral ("message queued"));
  recordSnrReportsForText (sessionId,
                           QStringLiteral ("Outgoing"),
                           text,
                           QStringLiteral ("TAG"),
                           nowMs);
  clearLastError ();
  return true;
}

bool FT2LinkQmlAdapter::transmitTextLocalAudio (quint16 sessionId,
                                                QString const& text,
                                                quint64 nowMs,
                                                bool modelAckAudio,
                                                bool dropFirstDataBurst,
                                                bool dropFirstAckBurst)
{
  if (m_transportBusy)
    {
      setLastError (QStringLiteral ("FT2-Link transport is already busy"));
      return false;
    }

  AppSession const* session = m_model.session (sessionId);
  if (!session)
    {
      setLastError (QStringLiteral (
          "FT2-Link local audio transport references an unknown session"));
      return false;
    }
  if (session->state != AppSessionState::Connected)
    {
      setLastError (QStringLiteral (
          "FT2-Link local audio transport requires a connected session"));
      return false;
    }
  if (session->negotiated.profile != Profile::Wide500
      && session->negotiated.profile != Profile::Wide2300)
    {
      setLastError (QStringLiteral (
          "FT2-Link local audio transport requires a wide profile"));
      return false;
    }

  QByteArray const payloadBytes = text.trimmed ().toUtf8 ();
  if (payloadBytes.isEmpty ())
    {
      setLastError (QStringLiteral (
          "FT2-Link local audio transport message is empty"));
      return false;
    }

  std::size_t const messageIndex = session->messages.size ();
  std::string error;
  if (!m_model.queueOutgoingText (
          sessionId,
          std::string (payloadBytes.constData (),
                       static_cast<std::size_t> (payloadBytes.size ())),
          nowMs,
          &error))
    {
      setLastError (QString::fromStdString (error));
      return false;
    }

  emit messagesChanged (sessionId);
  emit sessionsChanged ();
  recordQsoSession (sessionId, nowMs, QStringLiteral ("local audio TX"));
  recordSnrReportsForText (sessionId,
                           QStringLiteral ("Outgoing"),
                           text,
                           QStringLiteral ("TAG"),
                           nowMs);
  setTransportBusy (true);
  setTransportState (QStringLiteral ("Transmitting"));

  std::vector<std::uint8_t> payload;
  payload.reserve (static_cast<std::size_t> (payloadBytes.size ()));
  for (char byte : payloadBytes)
    {
      payload.push_back (static_cast<std::uint8_t> (byte));
    }

  decodium::ft2link::WideAudioPipelineOptions options;
  options.profile = session->negotiated.profile;
  options.initialW2300RateMode = session->negotiated.w2300RateMode;
  options.performHandshake = false;
  options.modelAckAudio = modelAckAudio;
  options.windowSize = 4;
  if (dropFirstDataBurst)
    {
      options.dropFirstAttemptSequences.push_back (0u);
    }
  if (dropFirstAckBurst)
    {
      options.dropFirstAckForSequences.push_back (0u);
    }

  WideAudioPipelineResult const result =
      decodium::ft2link::runWideAudioPipeline (payload, sessionId, options);

  m_lastTransportMetrics = transportMetricsMap (result);
  emit transportMetricsChanged ();

  bool const delivered = result.complete && !result.failed
      && result.receivedMessage == payload;
  if (delivered)
    {
      m_model.markOutgoingDelivered (sessionId, messageIndex, nowMs, &error);
      setTransportState (QStringLiteral ("Delivered"));
      clearLastError ();
    }
  else
    {
      m_model.markOutgoingFailed (sessionId, messageIndex, nowMs, &error);
      setTransportState (QStringLiteral ("Failed"));
      setLastError (result.error.empty ()
                    ? QStringLiteral ("FT2-Link local audio transport failed")
                    : QString::fromStdString (result.error));
    }

  emit messagesChanged (sessionId);
  emit sessionsChanged ();
  recordQsoSession (
      sessionId,
      nowMs,
      delivered ? QStringLiteral ("message delivered")
                : QStringLiteral ("message failed"));
  setTransportBusy (false);
  return delivered;
}

bool FT2LinkQmlAdapter::transmitApplicationPayloadRadio (
    quint16 sessionId,
    QString const& payloadText,
    QString const& logText,
    QString const& displayMessage,
    QString const& state,
    QVariantMap const& planExtras,
    quint64 nowMs)
{
  if (!m_radioTxArmed)
    {
      setLastError (QStringLiteral ("FT2-Link application TX is not armed"));
      return false;
    }

  QString const payloadTrimmed = payloadText.trimmed ();
  QString const logTrimmed = logText.trimmed ();
  if (payloadTrimmed.isEmpty () || logTrimmed.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link application payload is empty"));
      return false;
    }

  AppSession const* session = m_model.session (sessionId);
  if (!session || session->state != AppSessionState::Connected)
    {
      setLastError (QStringLiteral (
          "FT2-Link application TX requires a connected session"));
      return false;
    }
  if (session->negotiated.profile != Profile::Wide500
      && session->negotiated.profile != Profile::Wide2300)
    {
      setLastError (QStringLiteral (
          "FT2-Link application TX requires a wide profile"));
      return false;
    }

  bool const planMatches = m_preparedRadioTxSessionId == sessionId
      && m_preparedRadioTxText == payloadTrimmed
      && !m_preparedRadioTxSamples.empty ();
  W2300RateMode const currentRateMode =
      session->negotiated.profile == Profile::Wide2300
      ? currentLiveW2300RateMode (*session)
      : W2300RateMode::Fast;
  bool const rateMatches = session->negotiated.profile != Profile::Wide2300
      || m_preparedRadioTxW2300RateMode == currentRateMode;
  if ((!planMatches || !rateMatches)
      && !prepareRadioTxAudio (sessionId, payloadTrimmed, nowMs))
    {
      return false;
    }

  QByteArray const logBytes = logTrimmed.toUtf8 ();
  std::size_t const messageIndex = session->messages.size ();
  std::string error;
  if (!m_model.queueOutgoingText (
          sessionId,
          std::string (logBytes.constData (),
                       static_cast<std::size_t> (logBytes.size ())),
          nowMs,
          &error))
    {
      setLastError (QString::fromStdString (error));
      return false;
    }

  QByteArray const payloadBytes = payloadTrimmed.toUtf8 ();
  std::vector<std::uint8_t> payload;
  payload.reserve (static_cast<std::size_t> (payloadBytes.size ()));
  for (char byte : payloadBytes)
    {
      payload.push_back (static_cast<std::uint8_t> (byte));
    }

  std::unique_ptr<decodium::ft2link::OutboundTransfer> outbound (
      new decodium::ft2link::OutboundTransfer (
          session->negotiated.profile, sessionId, payload));
  outbound->setWindowSize (4);
  m_liveOutbound[sessionId] = std::move (outbound);
  m_liveOutboundMessageIndex[sessionId] = messageIndex;

  QVector<float> samples = toSampleVector (m_preparedRadioTxSamples);
  QVariantMap plan = m_lastRadioTxPlan;
  plan.insert (QStringLiteral ("armed"), true);
  plan.insert (QStringLiteral ("sessionId"), sessionId);
  plan.insert (QStringLiteral ("text"), logTrimmed);
  plan.insert (QStringLiteral ("application"), true);
  plan.insert (QStringLiteral ("requestedAtMs"),
               QVariant::fromValue<qulonglong> (
                   static_cast<qulonglong> (nowMs)));
  for (QVariantMap::const_iterator it = planExtras.constBegin ();
       it != planExtras.constEnd ();
       ++it)
    {
      plan.insert (it.key (), it.value ());
    }

  emit messagesChanged (sessionId);
  emit sessionsChanged ();
  recordQsoSession (sessionId, nowMs, state);
  recordSnrReportsForText (sessionId,
                           QStringLiteral ("Outgoing"),
                           logTrimmed,
                           QStringLiteral ("TAG"),
                           nowMs);
  setTransportState (state);
  enqueueRadioTx (displayMessage,
                  samples,
                  plan,
                  nowMs,
                  false,
                  sessionId,
                  false);
  scheduleLiveOutboundRetry (sessionId,
                             displayMessage,
                             samples,
                             plan,
                             payload,
                             session->negotiated.profile,
                             messageIndex,
                             nowMs);
  clearLastError ();
  setRadioTxArmed (false);
  return true;
}

bool FT2LinkQmlAdapter::transmitMailboxRadio (quint16 sessionId,
                                              QString const& toCall,
                                              QString const& subject,
                                              QString const& body,
                                              quint64 nowMs)
{
  return transmitMailboxRadioTyped (
      sessionId, toCall, subject, body, false, false, nowMs);
}

bool FT2LinkQmlAdapter::transmitMailboxRadioTyped (quint16 sessionId,
                                                   QString const& toCall,
                                                   QString const& subject,
                                                   QString const& body,
                                                   bool urgent,
                                                   bool emcomm,
                                                   quint64 nowMs)
{
  if (!m_radioTxArmed)
    {
      setLastError (QStringLiteral ("FT2-Link mailbox TX is not armed"));
      return false;
    }

  AppSession const* session = m_model.session (sessionId);
  if (!session || session->state != AppSessionState::Connected)
    {
      setLastError (QStringLiteral (
          "FT2-Link mailbox TX requires a connected session"));
      return false;
    }
  if (session->negotiated.profile != Profile::Wide500
      && session->negotiated.profile != Profile::Wide2300)
    {
      setLastError (QStringLiteral (
          "FT2-Link mailbox TX requires a wide profile"));
      return false;
    }

  QString const from = normalizeCallsign (
      QString::fromStdString (m_model.localStation ().call));
  QString resolvedTo = normalizeCallsign (toCall);
  if (resolvedTo.isEmpty ())
    {
      resolvedTo = QString::fromStdString (session->remoteCall);
    }
  resolvedTo = normalizeCallsign (resolvedTo);
  QString const subjectText = subject.trimmed ().isEmpty ()
      ? QStringLiteral ("FT2-Link mail")
      : subject.trimmed ();
  QString const bodyText = body.trimmed ();

  if (from.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link mailbox TX requires MYCALL"));
      return false;
    }
  if (resolvedTo.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link mailbox TX requires a recipient"));
      return false;
    }
  if (bodyText.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link mailbox body is empty"));
      return false;
    }

  QString const envelope = makeMailboxEnvelope (
      resolvedTo, from, subjectText, bodyText, urgent, emcomm);
  bool const planMatches = m_preparedRadioTxSessionId == sessionId
      && m_preparedRadioTxText == envelope
      && !m_preparedRadioTxSamples.empty ();
  W2300RateMode const currentRateMode =
      session->negotiated.profile == Profile::Wide2300
      ? currentLiveW2300RateMode (*session)
      : W2300RateMode::Fast;
  bool const rateMatches = session->negotiated.profile != Profile::Wide2300
      || m_preparedRadioTxW2300RateMode == currentRateMode;
  if ((!planMatches || !rateMatches)
      && !prepareRadioTxAudio (sessionId, envelope, nowMs))
    {
      return false;
    }

  QString const displayText = QStringLiteral ("MAIL to %1: %2")
      .arg (resolvedTo,
            (urgent || emcomm)
            ? QStringLiteral ("%1%2%3")
              .arg (urgent ? QStringLiteral ("URGENT ") : QString {},
                    emcomm ? QStringLiteral ("EMCOMM ") : QString {},
                    subjectText)
            : subjectText);
  QByteArray const displayBytes = displayText.toUtf8 ();
  std::size_t const messageIndex = session->messages.size ();
  std::string error;
  if (!m_model.queueOutgoingText (
          sessionId,
          std::string (displayBytes.constData (),
                       static_cast<std::size_t> (displayBytes.size ())),
          nowMs,
          &error))
    {
      setLastError (QString::fromStdString (error));
      return false;
    }

  QByteArray const payloadBytes = envelope.toUtf8 ();
  std::vector<std::uint8_t> payload;
  payload.reserve (static_cast<std::size_t> (payloadBytes.size ()));
  for (char byte : payloadBytes)
    {
      payload.push_back (static_cast<std::uint8_t> (byte));
    }

  std::unique_ptr<decodium::ft2link::OutboundTransfer> outbound (
      new decodium::ft2link::OutboundTransfer (
          session->negotiated.profile, sessionId, payload));
  outbound->setWindowSize (4);
  m_liveOutbound[sessionId] = std::move (outbound);
  m_liveOutboundMessageIndex[sessionId] = messageIndex;
  quint32 const mailboxId = recordMailbox (
      QStringLiteral ("Outgoing"),
      from,
      resolvedTo,
      subjectText,
      bodyText,
      QStringLiteral ("Pending"),
      nowMs,
      urgent,
      emcomm);
  m_liveOutboundMailboxId[sessionId] = mailboxId;
  m_liveOutboundMailboxDeliveredState[sessionId] = QStringLiteral ("Delivered");
  m_liveOutboundFormId.erase (sessionId);
  m_liveOutboundFileTransferId.erase (sessionId);
  m_liveOutboundBulletinId.erase (sessionId);

  QVector<float> samples = toSampleVector (m_preparedRadioTxSamples);
  QVariantMap plan = m_lastRadioTxPlan;
  plan.insert (QStringLiteral ("armed"), true);
  plan.insert (QStringLiteral ("sessionId"), sessionId);
  plan.insert (QStringLiteral ("text"), displayText);
  plan.insert (QStringLiteral ("mailbox"), true);
  plan.insert (QStringLiteral ("mailboxId"), mailboxId);
  plan.insert (QStringLiteral ("mailboxUrgent"), urgent);
  plan.insert (QStringLiteral ("mailboxEmcomm"), emcomm);
  plan.insert (QStringLiteral ("requestedAtMs"),
               QVariant::fromValue<qulonglong> (
                   static_cast<qulonglong> (nowMs)));

  emit messagesChanged (sessionId);
  emit sessionsChanged ();
  setTransportState (QStringLiteral ("MAIL TX"));
  QString const displayMessage = QStringLiteral ("FT2-Link MAIL ") + resolvedTo;
  enqueueRadioTx (displayMessage,
                  samples,
                  plan,
                  nowMs,
                  false,
                  sessionId,
                  false);
  scheduleLiveOutboundRetry (sessionId,
                             displayMessage,
                             samples,
                             plan,
                             payload,
                             session->negotiated.profile,
                             messageIndex,
                             nowMs);
  clearLastError ();
  setRadioTxArmed (false);
  return true;
}

bool FT2LinkQmlAdapter::parkMailbox (QString const& toCall,
                                     QString const& subject,
                                     QString const& body,
                                     quint64 nowMs)
{
  return parkMailboxTyped (toCall, subject, body, false, false, nowMs);
}

bool FT2LinkQmlAdapter::parkMailboxTyped (QString const& toCall,
                                          QString const& subject,
                                          QString const& body,
                                          bool urgent,
                                          bool emcomm,
                                          quint64 nowMs)
{
  QString const from = normalizeCallsign (
      QString::fromStdString (m_model.localStation ().call));
  QString const resolvedTo = normalizeCallsign (toCall);
  QString const subjectText = subject.trimmed ().isEmpty ()
      ? QStringLiteral ("FT2-Link mail")
      : subject.trimmed ();
  QString const bodyText = body.trimmed ();

  if (from.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link mailbox park requires MYCALL"));
      return false;
    }
  if (resolvedTo.isEmpty ())
    {
      setLastError (QStringLiteral (
          "FT2-Link mailbox park requires a recipient"));
      return false;
    }
  if (bodyText.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link mailbox body is empty"));
      return false;
    }
  if (makeMailboxEnvelope (resolvedTo, from, subjectText, bodyText, urgent, emcomm)
          .toUtf8 ().size () > 4096)
    {
      setLastError (QStringLiteral ("FT2-Link mailbox exceeds 4096 bytes"));
      return false;
    }

  quint32 const mailboxId = recordMailbox (
      QStringLiteral ("Parked"),
      from,
      resolvedTo,
      subjectText,
      bodyText,
      QStringLiteral ("Parked"),
      nowMs,
      urgent,
      emcomm);
  if (mailboxId == 0u)
    {
      setLastError (QStringLiteral ("FT2-Link mailbox body is empty"));
      return false;
    }
  setTransportState (QStringLiteral ("MAIL PARK"));
  clearLastError ();
  return true;
}

QVariantMap FT2LinkQmlAdapter::relayMailboxForSession (
    quint16 sessionId) const
{
  AppSession const* session = m_model.session (sessionId);
  if (!session || session->state != AppSessionState::Connected)
    {
      return {};
    }
  QString const peer = normalizeCallsign (
      QString::fromStdString (session->remoteCall));
  if (peer.isEmpty ())
    {
      return {};
    }

  MailboxMessage const* fallback = nullptr;
  for (MailboxMessage const& message : m_mailbox)
    {
      if (message.toCall != peer || message.body.trimmed ().isEmpty ())
        {
          continue;
        }
      bool const relayCandidate =
          message.direction == QStringLiteral ("Parked")
          || message.direction == QStringLiteral ("Relay");
      if (!relayCandidate)
        {
          continue;
        }
      if (message.state == QStringLiteral ("Relay ready"))
        {
          return mailboxMap (message.id,
                             message.direction,
                             message.fromCall,
                             message.toCall,
                             message.subject,
                             message.body,
                             message.state,
                             message.atMs,
                             message.updatedAtMs,
                             message.relayNotifiedAtMs,
                             message.urgent,
                             message.emcomm,
                             message.relayViaCall,
                             message.relayHopCount,
                             message.relayProtocol,
                             message.emailGatewayState,
                             message.emailGatewayDetail,
                             message.emailGatewayAtMs);
        }
      if (!fallback && message.state == QStringLiteral ("Parked"))
        {
          fallback = &message;
        }
    }

  if (!fallback)
    {
      return {};
    }
  return mailboxMap (fallback->id,
                     fallback->direction,
                     fallback->fromCall,
                     fallback->toCall,
	                     fallback->subject,
	                     fallback->body,
	                     fallback->state,
	                     fallback->atMs,
	                     fallback->updatedAtMs,
	                     fallback->relayNotifiedAtMs,
	                     fallback->urgent,
	                     fallback->emcomm,
	                     fallback->relayViaCall,
	                     fallback->relayHopCount,
	                     fallback->relayProtocol,
	                     fallback->emailGatewayState,
	                     fallback->emailGatewayDetail,
	                     fallback->emailGatewayAtMs);
}

bool FT2LinkQmlAdapter::transmitRelayMailboxRadio (quint16 sessionId,
                                                   quint32 mailboxId,
                                                   quint64 nowMs)
{
  MailboxMessage* relay = nullptr;
  for (MailboxMessage& message : m_mailbox)
    {
      if (message.id == mailboxId)
        {
          relay = &message;
          break;
        }
    }
  if (!relay)
    {
      setLastError (QStringLiteral ("FT2-Link relay mailbox item not found"));
      return false;
    }
  if (relay->body.trimmed ().isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link relay mailbox body is empty"));
      return false;
    }
  bool const relayCandidate =
      relay->direction == QStringLiteral ("Parked")
      || relay->direction == QStringLiteral ("Relay");
  if (!relayCandidate
      || (relay->state != QStringLiteral ("Parked")
          && relay->state != QStringLiteral ("Relay ready")
          && relay->state != QStringLiteral ("Failed")))
    {
      setLastError (QStringLiteral (
          "FT2-Link relay mailbox item is not parked"));
      return false;
    }

  AppSession const* session = m_model.session (sessionId);
  if (!session || session->state != AppSessionState::Connected)
    {
      setLastError (QStringLiteral (
          "FT2-Link relay TX requires a connected session"));
      return false;
    }
  QString const peer = normalizeCallsign (
      QString::fromStdString (session->remoteCall));
  if (relay->relayHopCount >= kMaxRelayHopCount)
    {
      updateMailboxState (mailboxId, QStringLiteral ("Failed"), nowMs);
      setLastError (QStringLiteral (
          "FT2-Link relay hop limit reached for %1").arg (relay->toCall));
      return false;
    }
  int const nextHopCount = std::clamp (
      relay->relayHopCount + 1, 1, kMaxRelayHopCount);
  QString const envelope = makeRelayMailboxEnvelope (
      relay->toCall,
      peer,
      relay->fromCall,
      relay->subject,
      relay->body,
      relay->urgent,
      relay->emcomm,
      nextHopCount);
  QString const priorityPrefix =
      QStringLiteral ("%1%2")
          .arg (relay->urgent ? QStringLiteral ("URGENT ") : QString {},
                relay->emcomm ? QStringLiteral ("EMCOMM ") : QString {});
  QString const displayText = QStringLiteral ("RELAY MAIL to %1: %2")
      .arg (relay->toCall, priorityPrefix + relay->subject);
  QVariantMap planExtras;
  planExtras.insert (QStringLiteral ("mailbox"), true);
  planExtras.insert (QStringLiteral ("relay"), true);
  planExtras.insert (QStringLiteral ("mailboxId"), mailboxId);
  planExtras.insert (QStringLiteral ("mailboxFrom"), relay->fromCall);
  planExtras.insert (QStringLiteral ("mailboxTo"), relay->toCall);
  planExtras.insert (QStringLiteral ("mailboxUrgent"), relay->urgent);
  planExtras.insert (QStringLiteral ("mailboxEmcomm"), relay->emcomm);
  planExtras.insert (QStringLiteral ("relayProtocol"),
                     QStringLiteral ("FT2RLY1"));
  planExtras.insert (QStringLiteral ("relayViaCall"), peer);
  planExtras.insert (QStringLiteral ("relayHopCount"), nextHopCount);

  if (!transmitApplicationPayloadRadio (
          sessionId,
          envelope,
          displayText,
          QStringLiteral ("FT2-Link RELAY ") + relay->toCall,
          QStringLiteral ("MAIL RELAY"),
          planExtras,
          nowMs))
    {
      return false;
    }

  QString const deliveredState = peer == relay->toCall
      ? QStringLiteral ("Delivered")
      : QStringLiteral ("Forwarded");
  m_liveOutboundMailboxId[sessionId] = mailboxId;
  m_liveOutboundMailboxDeliveredState[sessionId] = deliveredState;
  m_liveOutboundFormId.erase (sessionId);
  m_liveOutboundFileTransferId.erase (sessionId);
  m_liveOutboundBulletinId.erase (sessionId);
  relay->relayViaCall = peer;
  relay->relayHopCount = nextHopCount;
  relay->relayProtocol = QStringLiteral ("FT2RLY1");
  updateMailboxState (mailboxId, QStringLiteral ("Pending relay"), nowMs);
  return true;
}

bool FT2LinkQmlAdapter::transmitFormRadio (quint16 sessionId,
                                           QString const& toCall,
                                           QString const& formType,
                                           QVariantMap const& fields,
                                           quint64 nowMs)
{
  AppSession const* session = m_model.session (sessionId);
  if (!session || session->state != AppSessionState::Connected)
    {
      setLastError (QStringLiteral (
          "FT2-Link form TX requires a connected session"));
      return false;
    }

  QString const from = normalizeCallsign (
      QString::fromStdString (m_model.localStation ().call));
  QString resolvedTo = normalizeCallsign (toCall);
  if (resolvedTo.isEmpty ())
    {
      resolvedTo = QString::fromStdString (session->remoteCall);
    }
  resolvedTo = normalizeCallsign (resolvedTo);
  QString const type = formType.trimmed ().isEmpty ()
      ? QStringLiteral ("ICS213")
      : formType.trimmed ().toUpper ();
  QVariantMap normalizedFields = fields;
  normalizedFields.insert (QStringLiteral ("formType"), type);

  if (from.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link form TX requires MYCALL"));
      return false;
    }
  if (resolvedTo.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link form TX requires a recipient"));
      return false;
    }
  if (normalizedFields.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link form fields are empty"));
      return false;
    }

  QString const envelope = makeFormEnvelope (
      resolvedTo, from, type, normalizedFields);
  if (envelope.toUtf8 ().size () > 4096)
    {
      setLastError (QStringLiteral ("FT2-Link form exceeds 4096 bytes"));
      return false;
    }

  QString const displayText = QStringLiteral ("FORM %1 to %2")
      .arg (type, resolvedTo);
  QVariantMap planExtras;
  planExtras.insert (QStringLiteral ("form"), true);
  planExtras.insert (QStringLiteral ("formType"), type);

  if (!transmitApplicationPayloadRadio (
          sessionId,
          envelope,
          displayText,
          QStringLiteral ("FT2-Link FORM ") + type,
          QStringLiteral ("FORM TX"),
          planExtras,
          nowMs))
    {
      return false;
    }

  quint32 const formId = recordForm (
      QStringLiteral ("Outgoing"),
      from,
      resolvedTo,
      type,
      normalizedFields,
      QStringLiteral ("Pending"),
      nowMs);
  m_liveOutboundFormId[sessionId] = formId;
  m_liveOutboundMailboxId.erase (sessionId);
  m_liveOutboundMailboxDeliveredState.erase (sessionId);
  m_liveOutboundFileTransferId.erase (sessionId);
  m_liveOutboundBulletinId.erase (sessionId);
  return true;
}

bool FT2LinkQmlAdapter::transmitFileRadio (quint16 sessionId,
                                           QString const& toCall,
                                           QString const& fileName,
                                           QString const& content,
                                           quint64 nowMs)
{
  AppSession const* session = m_model.session (sessionId);
  if (!session || session->state != AppSessionState::Connected)
    {
      setLastError (QStringLiteral (
          "FT2-Link file TX requires a connected session"));
      return false;
    }

  QString const from = normalizeCallsign (
      QString::fromStdString (m_model.localStation ().call));
  QString resolvedTo = normalizeCallsign (toCall);
  if (resolvedTo.isEmpty ())
    {
      resolvedTo = QString::fromStdString (session->remoteCall);
    }
  resolvedTo = normalizeCallsign (resolvedTo);
  QString safeName = fileName.trimmed ();
  safeName.replace (QLatin1Char ('/'), QLatin1Char ('_'));
  safeName.replace (QLatin1Char ('\\'), QLatin1Char ('_'));
  if (safeName.isEmpty ())
    {
      safeName = QStringLiteral ("ft2link.txt");
    }
  QString const contentText = content.trimmed ();
  QByteArray const contentBytes = contentText.toUtf8 ();

  if (from.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link file TX requires MYCALL"));
      return false;
    }
  if (resolvedTo.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link file TX requires a recipient"));
      return false;
    }
  if (contentBytes.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link file content is empty"));
      return false;
    }
  if (contentBytes.size () > 4096)
    {
      setLastError (QStringLiteral ("FT2-Link file content exceeds 4096 bytes"));
      return false;
    }

  QString const checksum = sha256Hex (contentBytes);
  QString const envelope = makeFileEnvelope (
      resolvedTo, from, safeName, contentText);
  QString const displayText = QStringLiteral ("FILE %1 to %2")
      .arg (safeName, resolvedTo);
  QVariantMap planExtras;
  planExtras.insert (QStringLiteral ("file"), true);
  planExtras.insert (QStringLiteral ("fileName"), safeName);
  planExtras.insert (QStringLiteral ("fileBytes"), contentBytes.size ());
  planExtras.insert (QStringLiteral ("sha256"), checksum);

  if (!transmitApplicationPayloadRadio (
          sessionId,
          envelope,
          displayText,
          QStringLiteral ("FT2-Link FILE ") + safeName,
          QStringLiteral ("FILE TX"),
          planExtras,
          nowMs))
    {
      return false;
    }

  quint32 const transferId = recordFileTransfer (
      QStringLiteral ("Outgoing"),
      from,
      resolvedTo,
      safeName,
      contentText,
      checksum,
      QStringLiteral ("Pending"),
      nowMs);
  m_liveOutboundFileTransferId[sessionId] = transferId;
  m_liveOutboundMailboxId.erase (sessionId);
  m_liveOutboundMailboxDeliveredState.erase (sessionId);
  m_liveOutboundFormId.erase (sessionId);
  m_liveOutboundBulletinId.erase (sessionId);
  return true;
}

bool FT2LinkQmlAdapter::transmitBulletinRadio (quint16 sessionId,
                                               QString const& group,
                                               QString const& title,
                                               QString const& body,
                                               quint64 nowMs)
{
  AppSession const* session = m_model.session (sessionId);
  if (!session || session->state != AppSessionState::Connected)
    {
      setLastError (QStringLiteral (
          "FT2-Link bulletin TX requires a connected session"));
      return false;
    }

  QString const from = normalizeCallsign (
      QString::fromStdString (m_model.localStation ().call));
  QString normalizedGroup = group.trimmed ().toUpper ();
  if (normalizedGroup.isEmpty ())
    {
      normalizedGroup = QStringLiteral ("ALL");
    }
  QString const titleText = title.trimmed ().isEmpty ()
      ? QStringLiteral ("Bulletin")
      : title.trimmed ();
  QString const bodyText = body.trimmed ();
  if (from.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link bulletin TX requires MYCALL"));
      return false;
    }
  if (bodyText.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link bulletin body is empty"));
      return false;
    }

  QString const envelope = makeBulletinEnvelope (
      from, normalizedGroup, titleText, bodyText);
  if (envelope.toUtf8 ().size () > 4096)
    {
      setLastError (QStringLiteral ("FT2-Link bulletin exceeds 4096 bytes"));
      return false;
    }

  QString const displayText = QStringLiteral ("BBS %1: %2")
      .arg (normalizedGroup, titleText);
  QVariantMap planExtras;
  planExtras.insert (QStringLiteral ("bulletin"), true);
  planExtras.insert (QStringLiteral ("group"), normalizedGroup);

  if (!transmitApplicationPayloadRadio (
          sessionId,
          envelope,
          displayText,
          QStringLiteral ("FT2-Link BBS ") + normalizedGroup,
          QStringLiteral ("BBS TX"),
          planExtras,
          nowMs))
    {
      return false;
    }

  quint32 const bulletinId = recordBulletin (
      QStringLiteral ("Outgoing"),
      from,
      normalizedGroup,
      titleText,
      bodyText,
      QStringLiteral ("Pending"),
      nowMs);
  m_liveOutboundBulletinId[sessionId] = bulletinId;
  m_liveOutboundMailboxId.erase (sessionId);
  m_liveOutboundMailboxDeliveredState.erase (sessionId);
  m_liveOutboundFormId.erase (sessionId);
  m_liveOutboundFileTransferId.erase (sessionId);
  return true;
}

void FT2LinkQmlAdapter::setRadioTxArmed (bool armed)
{
  if (m_radioTxArmed == armed)
    {
      return;
    }
  m_radioTxArmed = armed;
  m_lastRadioTxPlan.insert (QStringLiteral ("armed"), m_radioTxArmed);
  emit radioTxArmedChanged ();
  emit radioTxPlanChanged ();
}

bool FT2LinkQmlAdapter::prepareRadioTxAudio (quint16 sessionId,
                                             QString const& text,
                                             quint64 nowMs)
{
  (void) nowMs;

  AppSession const* session = m_model.session (sessionId);
  if (!session)
    {
      setLastError (QStringLiteral (
          "FT2-Link radio TX references an unknown session"));
      return false;
    }
  if (session->state != AppSessionState::Connected)
    {
      setLastError (QStringLiteral (
          "FT2-Link radio TX requires a connected session"));
      return false;
    }
  if (session->negotiated.profile != Profile::Wide500
      && session->negotiated.profile != Profile::Wide2300)
    {
      setLastError (QStringLiteral (
          "FT2-Link radio TX requires a wide profile"));
      return false;
    }

  QByteArray const payloadBytes = text.trimmed ().toUtf8 ();
  if (payloadBytes.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link radio TX message is empty"));
      return false;
    }

  std::vector<std::uint8_t> payload;
  payload.reserve (static_cast<std::size_t> (payloadBytes.size ()));
  for (char byte : payloadBytes)
    {
      payload.push_back (static_cast<std::uint8_t> (byte));
    }

  decodium::ft2link::WideTxAudioPlanOptions options;
  options.profile = session->negotiated.profile;
  options.w2300RateMode = session->negotiated.w2300RateMode;
  QString rateSource = QStringLiteral ("negotiated");
  if (session->negotiated.profile == Profile::Wide2300)
    {
      options.w2300RateMode = currentLiveW2300RateMode (*session);
      if (m_liveW2300RateControllers.find (sessionId)
          != m_liveW2300RateControllers.end ())
        {
          rateSource = QStringLiteral ("live_rx");
        }
    }
  options.sampleRate = 48000.0;

  WideTxAudioPlan const plan =
      decodium::ft2link::buildWideTxAudioPlan (payload, sessionId, options);
  m_lastRadioTxPlan = radioTxPlanMap (plan, m_radioTxArmed);
  m_lastRadioTxPlan.insert (QStringLiteral ("w2300RateSource"), rateSource);
  m_lastRadioTxPlan.insert (
      QStringLiteral ("liveRateAdapted"),
      session->negotiated.profile == Profile::Wide2300
      && options.w2300RateMode != session->negotiated.w2300RateMode);
  std::map<std::uint16_t, decodium::ft2link::W2300DecodeMetrics>::const_iterator
      metrics = m_lastLiveW2300Metrics.find (sessionId);
  if (metrics != m_lastLiveW2300Metrics.end ())
    {
      m_lastRadioTxPlan.insert (QStringLiteral ("lastRxQuality"),
                                metrics->second.quality);
      m_lastRadioTxPlan.insert (
          QStringLiteral ("lastRxFrequencyOffsetHz"),
          metrics->second.estimatedFrequencyOffsetHz);
    }
  m_preparedRadioTxSamples = plan.samples;
  m_preparedRadioTxSessionId = plan.ok ? sessionId : 0u;
  m_preparedRadioTxText = plan.ok ? text.trimmed () : QString {};
  m_preparedRadioTxW2300RateMode = options.w2300RateMode;
  emit radioTxPlanChanged ();

  if (!plan.ok)
    {
      setLastError (plan.error.empty ()
                    ? QStringLiteral ("FT2-Link radio TX plan failed")
                    : QString::fromStdString (plan.error));
      return false;
    }

  clearLastError ();
  return true;
}

bool FT2LinkQmlAdapter::transmitPreparedRadioTxAudio (quint16 sessionId,
                                                      QString const& text,
                                                      quint64 nowMs)
{
  if (!m_radioTxArmed)
    {
      setLastError (QStringLiteral ("FT2-Link radio TX is not armed"));
      return false;
    }

  QString const payloadText = text.trimmed ();
  if (payloadText.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link radio TX message is empty"));
      return false;
    }

  AppSession const* session = m_model.session (sessionId);
  if (!session || session->state != AppSessionState::Connected)
    {
      setLastError (QStringLiteral (
          "FT2-Link radio TX requires a connected session"));
      return false;
    }

  bool const planMatches = m_preparedRadioTxSessionId == sessionId
      && m_preparedRadioTxText == payloadText
      && !m_preparedRadioTxSamples.empty ();
  W2300RateMode const currentRateMode =
      session->negotiated.profile == Profile::Wide2300
      ? currentLiveW2300RateMode (*session)
      : W2300RateMode::Fast;
  bool const rateMatches = session->negotiated.profile != Profile::Wide2300
      || m_preparedRadioTxW2300RateMode == currentRateMode;
  if ((!planMatches || !rateMatches)
      && !prepareRadioTxAudio (sessionId, payloadText, nowMs))
    {
      return false;
    }

  QByteArray const payloadBytes = payloadText.toUtf8 ();
  std::size_t const messageIndex = session->messages.size ();
  std::string error;
  if (!m_model.queueOutgoingText (
          sessionId,
          std::string (payloadBytes.constData (),
                       static_cast<std::size_t> (payloadBytes.size ())),
          nowMs,
          &error))
    {
      setLastError (QString::fromStdString (error));
      return false;
    }

  std::vector<std::uint8_t> payload;
  payload.reserve (static_cast<std::size_t> (payloadBytes.size ()));
  for (char byte : payloadBytes)
    {
      payload.push_back (static_cast<std::uint8_t> (byte));
    }
  std::unique_ptr<decodium::ft2link::OutboundTransfer> outbound (
      new decodium::ft2link::OutboundTransfer (
          session->negotiated.profile, sessionId, payload));
  outbound->setWindowSize (4);
  m_liveOutbound[sessionId] = std::move (outbound);
  m_liveOutboundMessageIndex[sessionId] = messageIndex;
  m_liveOutboundMailboxId.erase (sessionId);
  m_liveOutboundMailboxDeliveredState.erase (sessionId);
  m_liveOutboundFormId.erase (sessionId);
  m_liveOutboundFileTransferId.erase (sessionId);
  m_liveOutboundBulletinId.erase (sessionId);

  QVector<float> samples = toSampleVector (m_preparedRadioTxSamples);

  QVariantMap plan = m_lastRadioTxPlan;
  plan.insert (QStringLiteral ("armed"), true);
  plan.insert (QStringLiteral ("sessionId"), sessionId);
  plan.insert (QStringLiteral ("text"), payloadText);
  plan.insert (QStringLiteral ("requestedAtMs"),
               QVariant::fromValue<qulonglong> (
                   static_cast<qulonglong> (nowMs)));

  emit messagesChanged (sessionId);
  emit sessionsChanged ();
  recordQsoSession (sessionId, nowMs, QStringLiteral ("RF TX"));
  recordSnrReportsForText (sessionId,
                           QStringLiteral ("Outgoing"),
                           payloadText,
                           QStringLiteral ("TAG"),
                           nowMs);
  setTransportState (QStringLiteral ("Radio TX"));
  QString const displayMessage = QStringLiteral ("FT2-Link ") + payloadText;
  enqueueRadioTx (displayMessage,
                  samples,
                  plan,
                  nowMs,
                  false,
                  sessionId,
                  false);
  scheduleLiveOutboundRetry (sessionId,
                             displayMessage,
                             samples,
                             plan,
                             payload,
                             session->negotiated.profile,
                             messageIndex,
                             nowMs);
  clearLastError ();
  setRadioTxArmed (false);
  return true;
}

bool FT2LinkQmlAdapter::ingestRxSamples (QVector<short> const& samples,
                                         QString const& remoteCall,
                                         quint64 nowMs)
{
  if (samples.isEmpty ())
    {
      return false;
    }

  std::vector<float> chunk;
  chunk.reserve (static_cast<std::size_t> (samples.size ()));
  for (short sample : samples)
    {
      chunk.push_back (static_cast<float> (sample) / 32768.0f);
    }
  observeRxEnergy (chunk, nowMs);

  bool decodedAny = false;
  m_liveNarrowRxSamples.insert (
      m_liveNarrowRxSamples.end (), chunk.begin (), chunk.end ());
  for (int attempt = 0; attempt < 2; ++attempt)
    {
      Frame decoded;
      decodium::ft2link::NarrowDecodeMetrics metrics;
      std::string decodeError;
      decodium::ft2link::NarrowWaveformConfig config;
      if (!decodium::ft2link::decodeNarrowFrameWaveformWithMetrics (
              m_liveNarrowRxSamples, &decoded, &metrics, config, &decodeError))
        {
          break;
        }

      decodedAny = true;
      std::size_t const nsps = static_cast<std::size_t> (
          config.sampleRate / config.symbolRate + 0.5);
      std::size_t const consumed = std::min (
          m_liveNarrowRxSamples.size (),
          metrics.sampleOffset + metrics.symbolCount * nsps);
      m_liveNarrowRxSamples.erase (
          m_liveNarrowRxSamples.begin (),
          m_liveNarrowRxSamples.begin ()
              + static_cast<std::vector<float>::difference_type> (consumed));

      if (!ingestRadioFrameBytes (
              toByteArray (decodium::ft2link::serializeFrame (decoded)),
              remoteCall,
              nowMs,
              true))
        {
          return false;
        }
    }

  bool hasConnectedWideSession = false;
  std::vector<AppSession> const sessions = m_model.sessions ();
  for (AppSession const& session : sessions)
    {
      if (session.state == AppSessionState::Connected
          && (session.negotiated.profile == Profile::Wide500
              || session.negotiated.profile == Profile::Wide2300))
        {
          hasConnectedWideSession = true;
          break;
        }
    }
  if (!hasConnectedWideSession)
    {
      constexpr std::size_t kMaxBufferedNarrowRxSamples = 240000u;
      if (m_liveNarrowRxSamples.size () > kMaxBufferedNarrowRxSamples)
        {
          m_liveNarrowRxSamples.clear ();
        }
      return decodedAny;
    }

  m_liveW500Rx.append (chunk);
  m_liveW2300Rx.append (chunk);

  for (int attempt = 0; attempt < 4; ++attempt)
    {
      Frame decoded;
      decodium::ft2link::W2300DecodeMetrics metrics;
      std::string decodeError;
      if (!m_liveW2300Rx.decodeNext (&decoded, &metrics, &decodeError))
        {
          break;
        }
      decodedAny = true;
      observeLiveW2300Metrics (decoded, metrics, nowMs);
      if (!ingestRadioFrameBytes (
              toByteArray (decodium::ft2link::serializeFrame (decoded)),
              remoteCall,
              nowMs,
              true))
        {
          return false;
        }
    }

  for (int attempt = 0; attempt < 4; ++attempt)
    {
      Frame decoded;
      decodium::ft2link::W500DecodeMetrics metrics;
      std::string decodeError;
      if (!m_liveW500Rx.decodeNext (&decoded, &metrics, &decodeError))
        {
          break;
        }
      decodedAny = true;
      if (!ingestRadioFrameBytes (
              toByteArray (decodium::ft2link::serializeFrame (decoded)),
              remoteCall,
              nowMs,
              true))
        {
          return false;
        }
    }

  constexpr std::size_t kMaxBufferedRxSamples = 240000u;
  if (!decodedAny
      && (m_liveW500Rx.bufferedSamples () > kMaxBufferedRxSamples
          || m_liveW2300Rx.bufferedSamples () > kMaxBufferedRxSamples))
    {
      m_liveW500Rx.clear ();
      m_liveW2300Rx.clear ();
      setTransportState (QStringLiteral ("RX resync"));
    }

  return decodedAny;
}

bool FT2LinkQmlAdapter::ingestRadioFrameBytes (QByteArray const& frameBytes,
                                               QString const& remoteCall,
                                               quint64 nowMs,
                                               bool autoAck)
{
  Frame frame;
  std::string error;
  if (!decodium::ft2link::parseFrame (toBytes (frameBytes), &frame, &error))
    {
      setLastError (QString::fromStdString (error));
      return false;
    }

  if (frame.type == decodium::ft2link::FrameType::Beacon)
    {
      LinkCapabilities ignoredCapabilities;
      decodium::ft2link::HandshakeIdentity beaconIdentity;
      bool ignoredCq = false;
      std::string beaconParseError;
      if (decodium::ft2link::parseBeaconFrame (
              frame,
              &ignoredCapabilities,
              &beaconIdentity,
              &ignoredCq,
              &beaconParseError)
          && isCallBlocked (QString::fromStdString (beaconIdentity.call)))
        {
          setTransportState (ignoredCq
                             ? QStringLiteral ("BLOCKED CQ")
                             : QStringLiteral ("BLOCKED BEACON"));
          clearLastError ();
          return true;
        }

      StationAdvertisement advertisement;
      if (!m_model.observeBeacon (frame, nowMs, &advertisement, &error))
        {
          setLastError (QString::fromStdString (error));
          return false;
        }
      touchContact (QString::fromStdString (advertisement.station.call),
                    nowMs,
                    advertisement.cq ? QStringLiteral ("CQ RX")
                                     : QStringLiteral ("BEACON RX"),
                    QString::fromStdString (advertisement.station.locator),
                    QString::fromStdString (advertisement.station.name),
                    QString::fromStdString (decodium::ft2link::profileName (
                        advertisement.capabilities.preferredProfile)));
      recordBeaconHistory (QStringLiteral ("RX"), advertisement,
                           QStringLiteral ("RF"), nowMs);
      recordClusterLastHeard (
          QString::fromStdString (advertisement.station.call),
          QString::fromStdString (advertisement.station.locator),
          QString::fromStdString (advertisement.station.name),
          QString::fromStdString (decodium::ft2link::profileName (
              advertisement.capabilities.preferredProfile)),
          advertisement.cq ? QStringLiteral ("CQ RX")
                           : QStringLiteral ("BEACON RX"),
          QStringLiteral ("RF"),
          advertisement.cq,
          QString::fromStdString (advertisement.cqType),
          nowMs);
      notifyParkedMailboxForCall (
          QString::fromStdString (advertisement.station.call), nowMs);
      emit stationCountChanged ();
      setTransportState (advertisement.cq
                         ? QStringLiteral ("CQ RX")
                         : QStringLiteral ("BEACON RX"));
      clearLastError ();
      return true;
    }

  if (frame.type == decodium::ft2link::FrameType::Broadcast)
    {
      if (frame.profile != Profile::Narrow)
        {
          setLastError (QStringLiteral (
              "FT2-Link broadcast requires NARROW profile"));
          return false;
        }
      QString const text = QString::fromUtf8 (
          reinterpret_cast<char const*> (frame.payload.data ()),
          static_cast<int> (frame.payload.size ())).trimmed ();
      if (text.isEmpty ())
        {
          setLastError (QStringLiteral ("FT2-Link broadcast payload is empty"));
          return false;
        }
      QString fromCall = remoteCall.trimmed ().toUpper ();
      if (fromCall.isEmpty ())
        {
          fromCall = QStringLiteral ("UNKNOWN");
        }
      if (isCallBlocked (fromCall))
        {
          setTransportState (QStringLiteral ("BLOCKED BCAST"));
          clearLastError ();
          return true;
        }
      QStringList const tags = detectAlertTags (text);
      recordBroadcast (fromCall, text, nowMs, QStringLiteral ("RX"));
      bool const pathFinder = handlePathFinderBroadcast (fromCall, text, nowMs);
      setTransportState (pathFinder
                         ? QStringLiteral ("PATH RX")
                         : (tags.isEmpty ()
                         ? QStringLiteral ("BCAST RX")
                         : QStringLiteral ("ALERT RX")));
      clearLastError ();
      return true;
    }

  if (frame.type == decodium::ft2link::FrameType::Ping)
    {
      if (frame.profile != Profile::Narrow)
        {
          setLastError (QStringLiteral ("FT2-Link PING requires NARROW profile"));
          return false;
        }
      QString fromCall = QString::fromUtf8 (
          reinterpret_cast<char const*> (frame.payload.data ()),
          static_cast<int> (frame.payload.size ())).trimmed ().toUpper ();
      if (fromCall.isEmpty ())
        {
          fromCall = remoteCall.trimmed ().toUpper ();
        }
      if (fromCall.isEmpty ())
        {
          fromCall = QStringLiteral ("UNKNOWN");
        }
      if (isCallBlocked (fromCall))
        {
          setTransportState (QStringLiteral ("BLOCKED PING"));
          clearLastError ();
          return true;
        }
      if (!m_incomingPingsEnabled)
        {
          recordPing (QStringLiteral ("Incoming"),
                      fromCall,
                      frame.sequence,
                      QStringLiteral ("Rejected"),
                      nowMs);
          setTransportState (QStringLiteral ("PING REJECTED"));
          clearLastError ();
          return true;
        }

      recordPing (QStringLiteral ("Incoming"),
                  fromCall,
                  frame.sequence,
                  QStringLiteral ("Received"),
                  nowMs);
      setTransportState (QStringLiteral ("PING RX"));

      if (autoAck)
        {
          QString const localCall = normalizeCallsign (
              QString::fromStdString (m_model.localStation ().call));
          QByteArray const payloadBytes = localCall.toUtf8 ();
          Frame pingAck;
          pingAck.type = decodium::ft2link::FrameType::PingAck;
          pingAck.profile = Profile::Narrow;
          pingAck.flags = decodium::ft2link::FlagEndOfMessage;
          pingAck.sequence = frame.sequence;
          pingAck.payload.assign (
              reinterpret_cast<std::uint8_t const*> (payloadBytes.constData ()),
              reinterpret_cast<std::uint8_t const*> (payloadBytes.constData ())
                  + payloadBytes.size ());
          if (!requestControlRadioTx (
                  pingAck, QStringLiteral ("PING_ACK"), fromCall, nowMs))
            {
              return false;
            }
        }

      clearLastError ();
      return true;
    }

  if (frame.type == decodium::ft2link::FrameType::PingAck)
    {
      if (frame.profile != Profile::Narrow)
        {
          setLastError (QStringLiteral (
              "FT2-Link PING_ACK requires NARROW profile"));
          return false;
        }
      QString fromCall = QString::fromUtf8 (
          reinterpret_cast<char const*> (frame.payload.data ()),
          static_cast<int> (frame.payload.size ())).trimmed ().toUpper ();
      if (fromCall.isEmpty ())
        {
          fromCall = remoteCall.trimmed ().toUpper ();
        }
      if (fromCall.isEmpty ())
        {
          fromCall = QStringLiteral ("UNKNOWN");
        }

      quint64 rttMs = 0u;
      std::map<quint16, std::pair<QString, quint64> >::iterator pending =
          m_pendingPings.find (frame.sequence);
      if (pending != m_pendingPings.end ())
        {
          if (nowMs >= pending->second.second)
            {
              rttMs = nowMs - pending->second.second;
            }
          if (fromCall == QStringLiteral ("UNKNOWN"))
            {
              fromCall = pending->second.first;
            }
          m_pendingPings.erase (pending);
        }

      recordPing (QStringLiteral ("Incoming"),
                  fromCall,
                  frame.sequence,
                  rttMs > 0u ? QStringLiteral ("Reply")
                             : QStringLiteral ("Reply unmatched"),
                  nowMs,
                  rttMs);
      setTransportState (rttMs > 0u
                         ? QStringLiteral ("PING OK")
                         : QStringLiteral ("PING ACK RX"));
      clearLastError ();
      return true;
    }

  if (frame.type == decodium::ft2link::FrameType::Hello)
    {
      int const before = sessionCount ();
      Frame helloAck;
      QString resolvedBlockedRemote;
      if (rejectBlockedHello (
              remoteCall, frame, nowMs, &helloAck, &resolvedBlockedRemote))
        {
          m_activeSessionId = frame.sessionId;
          emit activeSessionChanged ();
          if (sessionCount () != before)
            {
              emit sessionCountChanged ();
            }
          emit sessionsChanged ();
          recordQsoSession (
              frame.sessionId, nowMs, QStringLiteral ("HELLO blocked"));
          setTransportState (QStringLiteral ("HELLO blocked"));
          if (autoAck
              && !requestControlRadioTx (
                  helloAck,
                  QStringLiteral ("HELLO_ACK"),
                  resolvedBlockedRemote,
                  nowMs))
            {
              return false;
            }
          setLastError (QStringLiteral ("FT2-Link blocked callsign %1")
                        .arg (resolvedBlockedRemote));
          return true;
        }

      bool const accepted = m_model.answerHello (
          toStdString (remoteCall), frame, nowMs, &helloAck, &error);
      if (!accepted && helloAck.type != decodium::ft2link::FrameType::HelloAck)
        {
          setLastError (QString::fromStdString (error));
          return false;
        }
      m_activeSessionId = frame.sessionId;
      emit activeSessionChanged ();
      if (sessionCount () != before)
        {
          emit sessionCountChanged ();
        }
      emit sessionsChanged ();
      recordQsoSession (frame.sessionId,
                        nowMs,
                        accepted ? QStringLiteral ("HELLO RX")
                                 : QStringLiteral ("HELLO rejected"));
      setTransportState (accepted
                         ? QStringLiteral ("HELLO RX")
                         : QStringLiteral ("HELLO rejected"));
      QString resolvedRemote = remoteCall.trimmed ().toUpper ();
      AppSession const* answeredSession = m_model.session (frame.sessionId);
      if (answeredSession && !answeredSession->remoteCall.empty ())
        {
          resolvedRemote = QString::fromStdString (answeredSession->remoteCall);
        }
      if (autoAck && helloAck.type == decodium::ft2link::FrameType::HelloAck
          && !requestControlRadioTx (
              helloAck, QStringLiteral ("HELLO_ACK"), resolvedRemote, nowMs))
        {
          return false;
        }
      bool const presenceQueued = accepted
          ? queuePresenceMessage (frame.sessionId, nowMs)
          : true;
      if (accepted && presenceQueued)
        {
          clearLastError ();
        }
      else if (!accepted)
        {
          setLastError (QString::fromStdString (error));
        }
      return accepted;
    }

  if (frame.type == decodium::ft2link::FrameType::HelloAck)
    {
      if (!m_model.receiveHelloAck (frame, nowMs, &error))
        {
          setLastError (QString::fromStdString (error));
          emit sessionsChanged ();
          return false;
        }
      m_activeSessionId = frame.sessionId;
      emit activeSessionChanged ();
      emit sessionsChanged ();
      recordQsoSession (frame.sessionId, nowMs, QStringLiteral ("Connected"));
      setTransportState (QStringLiteral ("Connected"));
      clearLastError ();
      return true;
    }

  AppSession const* session = m_model.session (frame.sessionId);
  if (!session)
    {
      setLastError (QStringLiteral (
          "FT2-Link RX frame references an unknown session"));
      return false;
    }
  if (session->state != AppSessionState::Connected)
    {
      setLastError (QStringLiteral (
          "FT2-Link RX frame requires a connected session"));
      return false;
    }

  if (frame.type == decodium::ft2link::FrameType::Ack)
    {
      std::map<std::uint16_t, std::unique_ptr<decodium::ft2link::OutboundTransfer> >::iterator
          transfer = m_liveOutbound.find (frame.sessionId);
      if (transfer == m_liveOutbound.end () || !transfer->second)
        {
          setTransportState (QStringLiteral ("ACK ignored"));
          clearLastError ();
          return true;
        }

      transfer->second->handleAckFrame (frame);
      if (transfer->second->complete ())
        {
          std::map<std::uint16_t, std::size_t>::const_iterator messageIndex =
              m_liveOutboundMessageIndex.find (frame.sessionId);
          if (messageIndex != m_liveOutboundMessageIndex.end ())
            {
              if (!m_model.markOutgoingDelivered (
                      frame.sessionId, messageIndex->second, nowMs, &error))
                {
                  setLastError (QString::fromStdString (error));
                  return false;
                }
            }
          m_liveOutbound.erase (frame.sessionId);
          m_liveOutboundMessageIndex.erase (frame.sessionId);
          std::map<std::uint16_t, quint32>::const_iterator mailboxId =
              m_liveOutboundMailboxId.find (frame.sessionId);
          if (mailboxId != m_liveOutboundMailboxId.end ())
            {
              QString deliveredState = QStringLiteral ("Delivered");
              std::map<std::uint16_t, QString>::const_iterator state =
                  m_liveOutboundMailboxDeliveredState.find (frame.sessionId);
              if (state != m_liveOutboundMailboxDeliveredState.end ()
                  && !state->second.trimmed ().isEmpty ())
                {
                  deliveredState = state->second;
                }
              updateMailboxState (
                  mailboxId->second, deliveredState, nowMs);
              m_liveOutboundMailboxId.erase (frame.sessionId);
              m_liveOutboundMailboxDeliveredState.erase (frame.sessionId);
            }
          std::map<std::uint16_t, quint32>::const_iterator formId =
              m_liveOutboundFormId.find (frame.sessionId);
          if (formId != m_liveOutboundFormId.end ())
            {
              updateFormState (
                  formId->second, QStringLiteral ("Delivered"), nowMs);
              m_liveOutboundFormId.erase (frame.sessionId);
            }
          std::map<std::uint16_t, quint32>::const_iterator fileTransferId =
              m_liveOutboundFileTransferId.find (frame.sessionId);
          if (fileTransferId != m_liveOutboundFileTransferId.end ())
            {
              updateFileTransferState (
                  fileTransferId->second, QStringLiteral ("Delivered"), nowMs);
              m_liveOutboundFileTransferId.erase (frame.sessionId);
            }
          std::map<std::uint16_t, quint32>::const_iterator bulletinId =
              m_liveOutboundBulletinId.find (frame.sessionId);
          if (bulletinId != m_liveOutboundBulletinId.end ())
            {
              updateBulletinState (
                  bulletinId->second, QStringLiteral ("Delivered"), nowMs);
              m_liveOutboundBulletinId.erase (frame.sessionId);
            }
          m_liveOutboundRetries.erase (frame.sessionId);
          emit messagesChanged (frame.sessionId);
          emit sessionsChanged ();
          recordQsoSession (
              frame.sessionId, nowMs, QStringLiteral ("ACK delivered"));
          setTransportState (QStringLiteral ("Delivered"));
        }
      else
        {
          setTransportState (QStringLiteral ("Waiting ACK"));
        }
      clearLastError ();
      return true;
    }

  if (frame.type != decodium::ft2link::FrameType::Data)
    {
      setLastError (QStringLiteral ("FT2-Link RX frame type is not handled"));
      return false;
    }
  if (frame.profile != session->negotiated.profile)
    {
      setLastError (QStringLiteral (
          "FT2-Link RX DATA profile does not match the session"));
      return false;
    }

  bool const deliveredBefore =
      m_liveInboundDelivered.find (frame.sessionId) != m_liveInboundDelivered.end ()
      && m_liveInboundDelivered[frame.sessionId];
  std::uint64_t const deliveredAt =
      m_liveInboundDeliveredAtMs.find (frame.sessionId) != m_liveInboundDeliveredAtMs.end ()
      ? m_liveInboundDeliveredAtMs[frame.sessionId]
      : 0u;
  bool const resetForNewMessage =
      deliveredBefore
      && frame.sequence == 0u
      && deliveredAt > 0u
      && nowMs > deliveredAt + 4000u;
  if (m_liveInbound.find (frame.sessionId) == m_liveInbound.end ()
      || !m_liveInbound[frame.sessionId]
      || resetForNewMessage)
    {
      m_liveInbound[frame.sessionId].reset (
          new decodium::ft2link::InboundTransfer (
              frame.profile, frame.sessionId));
      m_liveInboundDelivered[frame.sessionId] = false;
      m_liveInboundDeliveredAtMs[frame.sessionId] = 0u;
    }

  decodium::ft2link::InboundTransfer* inbound =
      m_liveInbound[frame.sessionId].get ();
  if (!inbound || !inbound->receive (frame))
    {
      setLastError (QStringLiteral ("FT2-Link RX DATA frame was rejected"));
      return false;
    }

  Frame const ack = inbound->makeAckFrame ();
  bool const alreadyDelivered =
      m_liveInboundDelivered.find (frame.sessionId) != m_liveInboundDelivered.end ()
      && m_liveInboundDelivered[frame.sessionId];
  bool closeAfterAck = false;
  if (inbound->complete () && !alreadyDelivered)
    {
      std::vector<std::uint8_t> const message = inbound->message ();
      QString const text = message.empty ()
          ? QString {}
          : QString::fromUtf8 (
              reinterpret_cast<char const*> (message.data ()),
              static_cast<int> (message.size ())).trimmed ();
      QString displayText = text;
      QString fileTo;
      QString fileFrom;
      QString fileName;
      QString fileContent;
      QString fileSha256;
      QString formTo;
      QString formFrom;
      QString formType;
      QVariantMap formFields;
      QString bulletinFrom;
      QString bulletinGroup;
      QString bulletinTitle;
      QString bulletinBody;
      QString mailTo;
      QString mailFrom;
      QString mailSubject;
      QString mailBody;
      QString mailVia;
      int mailHopCount = 0;
      bool mailUrgent = false;
      bool mailEmcomm = false;
      bool mailRelayEnvelope = false;
      if (parseFileEnvelope (
              text, &fileTo, &fileFrom, &fileName, &fileContent, &fileSha256))
        {
          recordFileTransfer (QStringLiteral ("Incoming"),
                              fileFrom,
                              fileTo,
                              fileName,
                              fileContent,
                              fileSha256,
                              QStringLiteral ("Received"),
                              nowMs);
          displayText = QStringLiteral ("FILE from %1: %2")
              .arg (fileFrom, fileName);
        }
      else if (parseFormEnvelope (
                   text, &formTo, &formFrom, &formType, &formFields))
        {
          recordForm (QStringLiteral ("Incoming"),
                      formFrom,
                      formTo,
                      formType,
                      formFields,
                      QStringLiteral ("Received"),
                      nowMs);
          displayText = QStringLiteral ("FORM %1 from %2")
              .arg (formType, formFrom);
        }
      else if (parseBulletinEnvelope (
                   text,
                   &bulletinFrom,
                   &bulletinGroup,
                   &bulletinTitle,
                   &bulletinBody))
        {
          recordBulletin (QStringLiteral ("Incoming"),
                          bulletinFrom,
                          bulletinGroup,
                          bulletinTitle,
                          bulletinBody,
                          QStringLiteral ("Received"),
                          nowMs);
          displayText = QStringLiteral ("BBS %1 from %2: %3")
              .arg (bulletinGroup, bulletinFrom, bulletinTitle);
        }
      else if ((mailRelayEnvelope = parseRelayMailboxEnvelope (
              text,
              &mailTo,
              &mailVia,
              &mailFrom,
              &mailSubject,
              &mailBody,
              &mailUrgent,
              &mailEmcomm,
              &mailHopCount))
          || parseMailboxEnvelope (
              text,
              &mailTo,
              &mailFrom,
              &mailSubject,
              &mailBody,
              &mailUrgent,
              &mailEmcomm))
        {
          QString const localCall = normalizeCallsign (
              QString::fromStdString (m_model.localStation ().call));
          QString const subjectText = mailSubject.isEmpty ()
              ? QStringLiteral ("FT2-Link mail")
              : mailSubject;
          bool const relayForOther = !localCall.isEmpty ()
              && !mailTo.isEmpty ()
              && mailTo != localCall;
          bool const relayHopLimit =
              relayForOther && mailRelayEnvelope
              && mailHopCount >= kMaxRelayHopCount;
          if (relayHopLimit)
            {
              displayText = QStringLiteral (
                  "RELAY MAIL hop limit for %1 from %2: %3")
                  .arg (mailTo, mailFrom, subjectText);
            }
          else if (relayForOther && !m_vmailParkingEnabled)
            {
              displayText = QStringLiteral ("RELAY MAIL rejected for %1 from %2: %3")
                  .arg (mailTo, mailFrom, subjectText);
            }
          else
            {
              recordMailbox (relayForOther ? QStringLiteral ("Relay")
                                            : QStringLiteral ("Incoming"),
                             mailFrom,
                             mailTo,
                             subjectText,
                             mailBody,
                             relayForOther ? QStringLiteral ("Parked")
                                           : QStringLiteral ("Received"),
                             nowMs,
                             mailUrgent,
                             mailEmcomm,
                             mailRelayEnvelope ? mailVia : QString {},
                             mailRelayEnvelope ? mailHopCount : 0,
                             mailRelayEnvelope ? QStringLiteral ("FT2RLY1")
                                               : QString {});
              QString const priorityPrefix =
                  QStringLiteral ("%1%2")
                      .arg (mailUrgent ? QStringLiteral ("URGENT ") : QString {},
                            mailEmcomm ? QStringLiteral ("EMCOMM ") : QString {});
              if (relayForOther)
                {
                  displayText = mailRelayEnvelope
                      ? QStringLiteral ("RELAY MAIL for %1 via %2 from %3: %4")
                            .arg (mailTo,
                                  mailVia.isEmpty () ? localCall : mailVia,
                                  mailFrom,
                                  priorityPrefix + subjectText)
                      : QStringLiteral ("RELAY MAIL for %1 from %2: %3")
                            .arg (mailTo, mailFrom, priorityPrefix + subjectText);
                }
              else
                {
                  displayText = mailRelayEnvelope
                      ? QStringLiteral ("RELAY MAIL delivered via %1 from %2: %3")
                            .arg (mailVia.isEmpty () ? QStringLiteral ("--") : mailVia,
                                  mailFrom,
                                  priorityPrefix + subjectText)
                      : QStringLiteral ("MAIL from %1: %2")
                            .arg (mailFrom, priorityPrefix + subjectText);
                }
            }
        }
      QByteArray const textBytes = displayText.toUtf8 ();
      if (!textBytes.isEmpty ()
          && !m_model.appendIncomingText (
              frame.sessionId,
              std::string (textBytes.constData (),
                           static_cast<std::size_t> (textBytes.size ())),
              nowMs,
              &error))
        {
          setLastError (QString::fromStdString (error));
          return false;
        }
      if (!handleIncomingControlTags (
              frame.sessionId, text, nowMs, &closeAfterAck))
        {
          return false;
        }
      m_liveInboundDelivered[frame.sessionId] = true;
      m_liveInboundDeliveredAtMs[frame.sessionId] = nowMs;
      m_activeSessionId = frame.sessionId;
      emit activeSessionChanged ();
      emit messagesChanged (frame.sessionId);
      emit sessionsChanged ();
      recordQsoSession (frame.sessionId, nowMs, QStringLiteral ("Received"));
      setTransportState (QStringLiteral ("Received"));
    }
  else
    {
      setTransportState (QStringLiteral ("DATA RX"));
    }

  if (autoAck && !requestAckRadioTx (ack, *session, nowMs))
    {
      return false;
    }
  if (closeAfterAck && !closeSession (frame.sessionId, nowMs))
    {
      return false;
    }

  clearLastError ();
  return true;
}

bool FT2LinkQmlAdapter::appendIncomingText (quint16 sessionId,
                                            QString const& text,
                                            quint64 nowMs)
{
  QString displayText = text.trimmed ();
  QString const originalText = displayText;
  QString fileTo;
  QString fileFrom;
  QString fileName;
  QString fileContent;
  QString fileSha256;
  QString formTo;
  QString formFrom;
  QString formType;
  QVariantMap formFields;
  QString bulletinFrom;
  QString bulletinGroup;
  QString bulletinTitle;
  QString bulletinBody;
  QString mailTo;
  QString mailFrom;
  QString mailSubject;
  QString mailBody;
  QString mailVia;
  int mailHopCount = 0;
  bool mailUrgent = false;
  bool mailEmcomm = false;
  bool mailRelayEnvelope = false;
  if (parseFileEnvelope (
          displayText, &fileTo, &fileFrom, &fileName, &fileContent, &fileSha256))
    {
      recordFileTransfer (QStringLiteral ("Incoming"),
                          fileFrom,
                          fileTo,
                          fileName,
                          fileContent,
                          fileSha256,
                          QStringLiteral ("Received"),
                          nowMs);
      displayText = QStringLiteral ("FILE from %1: %2")
          .arg (fileFrom, fileName);
    }
  else if (parseFormEnvelope (
               displayText, &formTo, &formFrom, &formType, &formFields))
    {
      recordForm (QStringLiteral ("Incoming"),
                  formFrom,
                  formTo,
                  formType,
                  formFields,
                  QStringLiteral ("Received"),
                  nowMs);
      displayText = QStringLiteral ("FORM %1 from %2")
          .arg (formType, formFrom);
    }
  else if (parseBulletinEnvelope (
               displayText,
               &bulletinFrom,
               &bulletinGroup,
               &bulletinTitle,
               &bulletinBody))
    {
      recordBulletin (QStringLiteral ("Incoming"),
                      bulletinFrom,
                      bulletinGroup,
                      bulletinTitle,
                      bulletinBody,
                      QStringLiteral ("Received"),
                      nowMs);
      displayText = QStringLiteral ("BBS %1 from %2: %3")
          .arg (bulletinGroup, bulletinFrom, bulletinTitle);
    }
  else if ((mailRelayEnvelope = parseRelayMailboxEnvelope (
          displayText,
          &mailTo,
          &mailVia,
          &mailFrom,
          &mailSubject,
          &mailBody,
          &mailUrgent,
          &mailEmcomm,
          &mailHopCount))
      || parseMailboxEnvelope (
          displayText,
          &mailTo,
          &mailFrom,
          &mailSubject,
          &mailBody,
          &mailUrgent,
          &mailEmcomm))
    {
      QString const localCall = normalizeCallsign (
          QString::fromStdString (m_model.localStation ().call));
      QString const subjectText = mailSubject.isEmpty ()
          ? QStringLiteral ("FT2-Link mail")
          : mailSubject;
      bool const relayForOther = !localCall.isEmpty ()
          && !mailTo.isEmpty ()
          && mailTo != localCall;
      bool const relayHopLimit =
          relayForOther && mailRelayEnvelope
          && mailHopCount >= kMaxRelayHopCount;
      if (relayHopLimit)
        {
          displayText = QStringLiteral (
              "RELAY MAIL hop limit for %1 from %2: %3")
              .arg (mailTo, mailFrom, subjectText);
        }
      else if (relayForOther && !m_vmailParkingEnabled)
        {
          displayText = QStringLiteral ("RELAY MAIL rejected for %1 from %2: %3")
              .arg (mailTo, mailFrom, subjectText);
        }
      else
        {
          recordMailbox (relayForOther ? QStringLiteral ("Relay")
                                        : QStringLiteral ("Incoming"),
                         mailFrom,
                         mailTo,
                         subjectText,
                         mailBody,
                         relayForOther ? QStringLiteral ("Parked")
                                       : QStringLiteral ("Received"),
                         nowMs,
                         mailUrgent,
                         mailEmcomm,
                         mailRelayEnvelope ? mailVia : QString {},
                         mailRelayEnvelope ? mailHopCount : 0,
                         mailRelayEnvelope ? QStringLiteral ("FT2RLY1")
                                           : QString {});
          QString const priorityPrefix =
              QStringLiteral ("%1%2")
                  .arg (mailUrgent ? QStringLiteral ("URGENT ") : QString {},
                        mailEmcomm ? QStringLiteral ("EMCOMM ") : QString {});
          if (relayForOther)
            {
              displayText = mailRelayEnvelope
                  ? QStringLiteral ("RELAY MAIL for %1 via %2 from %3: %4")
                        .arg (mailTo,
                              mailVia.isEmpty () ? localCall : mailVia,
                              mailFrom,
                              priorityPrefix + subjectText)
                  : QStringLiteral ("RELAY MAIL for %1 from %2: %3")
                        .arg (mailTo, mailFrom, priorityPrefix + subjectText);
            }
          else
            {
              displayText = mailRelayEnvelope
                  ? QStringLiteral ("RELAY MAIL delivered via %1 from %2: %3")
                        .arg (mailVia.isEmpty () ? QStringLiteral ("--") : mailVia,
                              mailFrom,
                              priorityPrefix + subjectText)
                  : QStringLiteral ("MAIL from %1: %2")
                        .arg (mailFrom, priorityPrefix + subjectText);
            }
        }
    }

  std::string error;
  if (!m_model.appendIncomingText (
          sessionId, displayText.toStdString (), nowMs, &error))
    {
      setLastError (QString::fromStdString (error));
      return false;
    }
  bool disconnectRequested = false;
  if (!handleIncomingControlTags (
          sessionId, originalText, nowMs, &disconnectRequested))
    {
      return false;
    }
  emit messagesChanged (sessionId);
  emit sessionsChanged ();
  recordQsoSession (sessionId, nowMs, QStringLiteral ("incoming text"));
  if (disconnectRequested && !closeSession (sessionId, nowMs))
    {
      return false;
    }
  clearLastError ();
  return true;
}

bool FT2LinkQmlAdapter::closeSession (quint16 sessionId, quint64 nowMs)
{
  AppSession const* beforeClose = m_model.session (sessionId);
  QString const remoteCall = beforeClose
      ? normalizeCallsign (QString::fromStdString (beforeClose->remoteCall))
      : QString {};
  std::string error;
  if (!m_model.closeSession (sessionId, nowMs, &error))
    {
      setLastError (QString::fromStdString (error));
      return false;
    }
  if (!remoteCall.isEmpty ())
    {
      setTypingPeer (remoteCall, false, nowMs);
    }
  m_liveOutbound.erase (sessionId);
  m_liveOutboundMessageIndex.erase (sessionId);
  m_liveOutboundMailboxId.erase (sessionId);
  m_liveOutboundMailboxDeliveredState.erase (sessionId);
  m_liveOutboundFormId.erase (sessionId);
  m_liveOutboundFileTransferId.erase (sessionId);
  m_liveOutboundBulletinId.erase (sessionId);
  m_liveInbound.erase (sessionId);
  m_liveInboundDelivered.erase (sessionId);
  m_liveInboundDeliveredAtMs.erase (sessionId);
  m_liveOutboundRetries.erase (sessionId);
  m_liveW2300RateControllers.erase (sessionId);
  m_lastLiveW2300Metrics.erase (sessionId);
  m_lastCallIdQueuedAtMs.erase (sessionId);
  emit sessionsChanged ();
  recordQsoSession (sessionId, nowMs, QStringLiteral ("Closed"));
  clearLastError ();
  return true;
}

QVariantList FT2LinkQmlAdapter::cannedMessages () const
{
  QVariantList list;
  list.push_back (cannedMessageMap (
      QStringLiteral ("INFO"),
      QStringLiteral ("DE <MYCALL> GRID <MYGRID>"),
      QStringLiteral ("Insert local call and grid")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("NAME"),
      QStringLiteral ("NAME <NAME>"),
      QStringLiteral ("Insert local operator name")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("QTH"),
      QStringLiteral ("<QTH:<QTH>>"),
      QStringLiteral ("Insert local QTH")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("LINK"),
      QStringLiteral ("FT2-LINK <PROFILE> <RATE> UTC <UTC>"),
      QStringLiteral ("Insert link profile and UTC time")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("73"),
      QStringLiteral ("73 <CALL> DE <MYCALL>"),
      QStringLiteral ("Insert closing message")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("MYDATA"),
      QStringLiteral ("<NAME:<NAME>> <QTH:<QTH>> <LOC:<MYGRID>>"),
      QStringLiteral ("Insert VarAC-style name, QTH and locator tags")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("RIG"),
      QStringLiteral ("<RIG:<RIG>> <ANT:<ANT>> <PWR:<PWR>>"),
      QStringLiteral ("Insert station equipment tags")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("EMAIL"),
      QStringLiteral ("<EM:<EMAIL>>"),
      QStringLiteral ("Insert email tag")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("ICE"),
      QStringLiteral ("<ICE:<ICE>>"),
      QStringLiteral ("Insert ice breaker tag")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("GPS"),
      QStringLiteral ("<GPS:<GPSLOC>>"),
      QStringLiteral ("Insert GPS/location tag")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("UTC"),
      QStringLiteral ("<UTCDT>"),
      QStringLiteral ("Insert UTC date and time")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("TU"),
      QStringLiteral ("TU!"),
      QStringLiteral ("Insert thank-you gesture")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("LIKE"),
      QStringLiteral ("LIKE!"),
      QStringLiteral ("Insert like gesture")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("DING"),
      QStringLiteral ("DING"),
      QStringLiteral ("Insert audible-alert gesture")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("TYP"),
      QStringLiteral ("<TYP>"),
      QStringLiteral ("Tell partner you are typing")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("TYP0"),
      QStringLiteral ("<TYP0>"),
      QStringLiteral ("Clear partner typing indicator")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("SR"),
      QStringLiteral ("<SR>"),
      QStringLiteral ("Request an SNR report")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("INFO?"),
      QStringLiteral ("<INFO>"),
      QStringLiteral ("Request partner info")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("LOC?"),
      QStringLiteral ("<LOCR>"),
      QStringLiteral ("Request partner locator")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("LHR"),
      QStringLiteral ("<LHR>"),
      QStringLiteral ("Request partner last-heard list")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("LHC"),
      QStringLiteral ("<LHC:CALL>"),
      QStringLiteral ("Request partner last-heard for a callsign")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("FSR"),
      QStringLiteral ("<FSR>"),
      QStringLiteral ("Request partner frequency schedule")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("VM?"),
      QStringLiteral ("<VRP>"),
      QStringLiteral ("Ask if partner has parked VMail for this call")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("LC?"),
      QStringLiteral ("<LCR>"),
      QStringLiteral ("Request partner recent connections")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("GPS?"),
      QStringLiteral ("<GPSR>"),
      QStringLiteral ("Request partner GPS location")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("BBS?"),
      QStringLiteral ("<BLR>"),
      QStringLiteral ("Request partner BBS file list")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("GET"),
      QStringLiteral ("<BG:file.txt>"),
      QStringLiteral ("Request a BBS file by name")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("VER"),
      QStringLiteral ("<VER>"),
      QStringLiteral ("Request FT2-Link version/capability string")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("AWAY"),
      QStringLiteral ("<AWAY> QRX DE <MYCALL>"),
      QStringLiteral ("Send away status")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("QSY+"),
      QStringLiteral ("<QSYU>"),
      QStringLiteral ("Invite partner 750 Hz up")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("QSY-"),
      QStringLiteral ("<QSYD>"),
      QStringLiteral ("Invite partner 750 Hz down")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("VSNR"),
      QStringLiteral ("<VSI>"),
      QStringLiteral ("Invite verbose SNR mode")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("TL"),
      QStringLiteral ("<TL>"),
      QStringLiteral ("Send post-QSY test link tag")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("AI"),
      QStringLiteral ("AI: "),
      QStringLiteral ("Start an AI gateway request manually")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("NOAI"),
      QStringLiteral ("<DISAI>"),
      QStringLiteral ("Close AI gateway mode manually")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("SFRD"),
      QStringLiteral ("<SFRD>"),
      QStringLiteral ("Legacy file ready-to-receive tag")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("SMR"),
      QStringLiteral ("<SMR>"),
      QStringLiteral ("Legacy VMail received tag")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("NOPLAY"),
      QStringLiteral ("<PJ>"),
      QStringLiteral ("Reject HamPlay invitation manually")));
  list.push_back (cannedMessageMap (
      QStringLiteral ("DISC"),
      QStringLiteral ("73 <CALL> DE <MYCALL> <DISC>"),
      QStringLiteral ("Insert closing message with disconnect tag")));
  for (CannedMessage const& message : m_customCannedMessages)
    {
      list.push_back (cannedMessageMap (
          message.label, message.templateText, message.tip, true));
    }
  return list;
}

QVariantList FT2LinkQmlAdapter::customCannedMessages () const
{
  QVariantList list;
  for (CannedMessage const& message : m_customCannedMessages)
    {
      list.push_back (cannedMessageMap (
          message.label, message.templateText, message.tip, true));
    }
  return list;
}

QVariantMap FT2LinkQmlAdapter::addOrUpdateCannedMessage (
    QString const& label,
    QString const& templateText,
    QString const& tip)
{
  QVariantMap result;
  QString const cleanLabel = sanitizedCannedLabel (label);
  QString const cleanTemplate = sanitizedCannedText (templateText, 512);
  QString const cleanTip = sanitizedCannedText (tip, 96);

  result.insert (QStringLiteral ("ok"), false);
  result.insert (QStringLiteral ("label"), cleanLabel);
  if (cleanLabel.isEmpty () || cleanTemplate.isEmpty ())
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("Label and template are required"));
      return result;
    }

  auto const match = std::find_if (
      m_customCannedMessages.begin (),
      m_customCannedMessages.end (),
      [&cleanLabel] (CannedMessage const& message) {
        return message.label == cleanLabel;
      });

  bool updated = false;
  if (match != m_customCannedMessages.end ())
    {
      match->templateText = cleanTemplate;
      match->tip = cleanTip;
      updated = true;
    }
  else
    {
      if (m_customCannedMessages.size () >= 24u)
        {
          result.insert (QStringLiteral ("error"),
                         QStringLiteral ("Maximum custom preset count reached"));
          return result;
        }
      m_customCannedMessages.push_back (
          CannedMessage {cleanLabel, cleanTemplate, cleanTip});
    }

  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("updated"), updated);
  result.insert (QStringLiteral ("templateText"), cleanTemplate);
  result.insert (QStringLiteral ("tip"), cleanTip);
  emit cannedMessagesChanged ();
  persistLocalStore ();
  return result;
}

QVariantMap FT2LinkQmlAdapter::deleteCannedMessage (QString const& label)
{
  QVariantMap result;
  QString const cleanLabel = sanitizedCannedLabel (label);
  result.insert (QStringLiteral ("ok"), false);
  result.insert (QStringLiteral ("label"), cleanLabel);
  auto const match = std::find_if (
      m_customCannedMessages.begin (),
      m_customCannedMessages.end (),
      [&cleanLabel] (CannedMessage const& message) {
        return message.label == cleanLabel;
      });
  if (match == m_customCannedMessages.end ())
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("Custom preset not found"));
      return result;
    }

  m_customCannedMessages.erase (match);
  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("error"), QString {});
  emit cannedMessagesChanged ();
  persistLocalStore ();
  return result;
}

QVariantMap FT2LinkQmlAdapter::resetCannedMessages ()
{
  QVariantMap result;
  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("removed"),
                 QVariant::fromValue<qulonglong> (
                     static_cast<qulonglong> (
                         m_customCannedMessages.size ())));
  m_customCannedMessages.clear ();
  emit cannedMessagesChanged ();
  persistLocalStore ();
  return result;
}

QString FT2LinkQmlAdapter::checkInMessage (QString const& city,
                                           QString const& region,
                                           QString const& channel,
                                           QString const& weather,
                                           quint64 nowMs) const
{
  StationIdentity const& local = m_model.localStation ();
  QString const myCall = QString::fromStdString (local.call).trimmed ();
  QString myName = QString::fromStdString (local.name).trimmed ();
  if (myName.isEmpty ())
    {
      myName = myCall;
    }

  QString const cleanCity = city.trimmed ().isEmpty ()
      ? m_localProfile.qth.trimmed ()
      : city.trimmed ();
  QStringList parts;
  if (!myCall.isEmpty ())
    {
      parts.push_back (myCall);
    }
  if (!myName.isEmpty ())
    {
      parts.push_back (myName);
    }
  if (!cleanCity.isEmpty ())
    {
      parts.push_back (cleanCity);
    }
  if (!region.trimmed ().isEmpty ())
    {
      parts.push_back (region.trimmed ());
    }
  if (!channel.trimmed ().isEmpty ())
    {
      parts.push_back (
          QStringLiteral ("(%1)").arg (channel.trimmed ().toUpper ()));
    }

  QString text = parts.join (QStringLiteral (", "));
  QString const cleanWeather = weather.trimmed ();
  if (!cleanWeather.isEmpty ())
    {
      QDateTime const timestamp =
          QDateTime::fromMSecsSinceEpoch (
              static_cast<qint64> (nowMs), QTimeZone(QByteArrayLiteral("UTC")));
      QString const utc = timestamp.isValid ()
          ? timestamp.toUTC ().toString (QStringLiteral ("HHmm'Z'"))
          : QDateTime::currentDateTimeUtc ().toString (
              QStringLiteral ("HHmm'Z'"));
      text += QStringLiteral ("\n%1, %2").arg (utc, cleanWeather);
    }
  return text.trimmed ();
}

QString FT2LinkQmlAdapter::expandCannedMessage (
    QString const& templateText,
    quint16 sessionId,
    quint64 nowMs) const
{
  QString text = templateText.trimmed ();
  StationIdentity const& local = m_model.localStation ();
  QString const myCall = QString::fromStdString (local.call);
  QString const myGrid = QString::fromStdString (local.locator);
  QString myName = QString::fromStdString (local.name).trimmed ();
  if (myName.isEmpty ())
    {
      myName = myCall;
    }
  QString const myQth = m_localProfile.qth.isEmpty ()
      ? myGrid
      : m_localProfile.qth;

  QString remoteCall;
  QString remoteGrid;
  QString remoteName;
  QString profileName;
  QString rateName;
  AppSession const* session = m_model.session (sessionId);
  if (session)
    {
      remoteCall = QString::fromStdString (session->remoteCall);
      profileName = QString::fromStdString (
          decodium::ft2link::profileName (session->negotiated.profile));
      rateName = QString::fromLatin1 (
          decodium::ft2link::w2300RateModeName (
              session->negotiated.w2300RateMode));
      if (session->negotiated.profile != Profile::Wide2300)
        {
          rateName.clear ();
        }
    }
  if (!remoteCall.isEmpty ())
    {
      std::map<QString, ContactHistory>::const_iterator const contact =
          m_contactHistory.find (remoteCall.trimmed ().toUpper ());
      if (contact != m_contactHistory.end ())
        {
          remoteGrid = contact->second.locator;
          remoteName = contact->second.name;
        }
    }

  QDateTime const timestamp =
      QDateTime::fromMSecsSinceEpoch (
          static_cast<qint64> (nowMs), QTimeZone(QByteArrayLiteral("UTC")));
  QString const utc = timestamp.isValid ()
      ? timestamp.toUTC ().toString (QStringLiteral ("HHmm'Z'"))
      : QDateTime::currentDateTimeUtc ().toString (QStringLiteral ("HHmm'Z'"));
  QDateTime const safeUtc = timestamp.isValid ()
      ? timestamp.toUTC ()
      : QDateTime::currentDateTimeUtc ();
  QDateTime const safeLocal = safeUtc.toLocalTime ();
  QString const utcDateTime =
      safeUtc.toString (QStringLiteral ("yyyy-MM-dd HH:mm'Z'"));
  QString const utcDate = safeUtc.toString (QStringLiteral ("yyyy-MM-dd"));
  QString const utcTime = safeUtc.toString (QStringLiteral ("HH:mm'Z'"));
  QString const localDateTime =
      safeLocal.toString (QStringLiteral ("yyyy-MM-dd HH:mm"));

  replaceToken (&text, QStringLiteral ("<MYCALL>"), myCall);
  replaceToken (&text, QStringLiteral ("<MYGRID>"), myGrid);
  replaceToken (&text, QStringLiteral ("<MYLOC>"), myGrid);
  replaceToken (&text, QStringLiteral ("<NAME>"), myName);
  replaceToken (&text, QStringLiteral ("<QTH>"), myQth);
  replaceToken (&text, QStringLiteral ("<EMAIL>"), m_localProfile.email);
  replaceToken (&text, QStringLiteral ("<ICE>"), m_localProfile.ice);
  replaceToken (&text, QStringLiteral ("<RIG>"), m_localProfile.rig);
  replaceToken (&text, QStringLiteral ("<ANT>"), m_localProfile.antenna);
  replaceToken (&text, QStringLiteral ("<PWR>"), m_localProfile.power);
  replaceToken (&text, QStringLiteral ("<GPSLOC>"), m_localProfile.gps);
  replaceToken (&text, QStringLiteral ("<GPS>"), m_localProfile.gps);
  replaceToken (&text, QStringLiteral ("<CALL>"), remoteCall);
  replaceToken (&text, QStringLiteral ("<HCALL>"), remoteCall);
  replaceToken (&text, QStringLiteral ("<HLOC>"), remoteGrid);
  replaceToken (&text, QStringLiteral ("<HNAME>"), remoteName);
  replaceToken (&text, QStringLiteral ("<PROFILE>"), profileName);
  replaceToken (&text, QStringLiteral ("<RATE>"), rateName);
  replaceToken (&text, QStringLiteral ("<UTC>"), utc);
  replaceToken (&text, QStringLiteral ("<UTCDT>"), utcDateTime);
  replaceToken (&text, QStringLiteral ("<UTCD>"), utcDate);
  replaceToken (&text, QStringLiteral ("<UTCT>"), utcTime);
  replaceToken (&text, QStringLiteral ("<MYDT>"), localDateTime);
  return text.simplified ();
}

QVariantList FT2LinkQmlAdapter::qsySlots (int slotSizeHz,
                                          int slotsEachSide) const
{
  int const safeSlotSizeHz = qBound (100, slotSizeHz, 5000);
  int const safeSlotsEachSide = qBound (1, slotsEachSide, 10);
  QVariantList list;
  for (int slot = 1; slot <= safeSlotsEachSide; ++slot)
    {
      int const upOffsetHz = slot * safeSlotSizeHz;
      int const downOffsetHz = -upOffsetHz;
      list.push_back (qsySlotMap (
          slot, upOffsetHz, qsyTagForOffset (upOffsetHz)));
      list.push_back (qsySlotMap (
          -slot, downOffsetHz, qsyTagForOffset (downOffsetHz)));
    }
  return list;
}

QString FT2LinkQmlAdapter::qsyTagForOffset (int offsetHz) const
{
  if (offsetHz == 750)
    {
      return QStringLiteral ("<QSYU>");
    }
  if (offsetHz == -750)
    {
      return QStringLiteral ("<QSYD>");
    }
  if (offsetHz == 0)
    {
      return QString {};
    }

  int const decaHz = qRound (static_cast<double> (offsetHz) / 10.0);
  return QStringLiteral ("<Q:%1>").arg (decaHz > 0
                                        ? QStringLiteral ("+%1").arg (decaHz)
                                        : QString::number (decaHz));
}

QString FT2LinkQmlAdapter::qsyFrequencyTag (qint64 dialFrequencyHz) const
{
  if (dialFrequencyHz <= 0)
    {
      return QString {};
    }
  return QStringLiteral ("<QF:%1>").arg (dialFrequencyHz);
}

QVariantMap FT2LinkQmlAdapter::qsyPlanForText (
    QString const& text,
    qint64 currentDialFrequencyHz) const
{
  QVariantMap map;
  map.insert (QStringLiteral ("valid"), false);
  map.insert (QStringLiteral ("acceptTag"), QStringLiteral ("<QSYR>"));
  map.insert (QStringLiteral ("rejectTag"), QStringLiteral ("<QSYJ>"));
  map.insert (QStringLiteral ("outOfRangeTag"), QStringLiteral ("<QJO>"));

  QString const trimmed = text.simplified ();
  int offsetHz = 0;
  qint64 targetDialHz = 0;
  QString sourceTag;
  QString kind;

  if (containsControlTag (trimmed, QStringLiteral ("<QSYU>")))
    {
      offsetHz = 750;
      sourceTag = QStringLiteral ("<QSYU>");
      kind = QStringLiteral ("offset");
    }
  else if (containsControlTag (trimmed, QStringLiteral ("<QSYD>")))
    {
      offsetHz = -750;
      sourceTag = QStringLiteral ("<QSYD>");
      kind = QStringLiteral ("offset");
    }
  else
    {
      QString const qsyOffset = firstControlTagValue (
          trimmed, QStringLiteral ("Q"));
      if (!qsyOffset.isEmpty ())
        {
          bool ok = false;
          int const decaHz = qsyOffset.toInt (&ok);
          if (ok && decaHz != 0)
            {
              offsetHz = decaHz * 10;
              sourceTag = QStringLiteral ("<Q:%1>").arg (qsyOffset);
              kind = QStringLiteral ("offset");
            }
        }
    }

  QString const qsyFrequency = firstControlTagValue (
      trimmed, QStringLiteral ("QF"));
  if (!qsyFrequency.isEmpty ())
    {
      bool ok = false;
      qint64 const parsed = qsyFrequency.toLongLong (&ok);
      if (ok && parsed > 0)
        {
          targetDialHz = parsed;
          sourceTag = QStringLiteral ("<QF:%1>").arg (qsyFrequency);
          kind = QStringLiteral ("frequency");
          if (currentDialFrequencyHz > 0)
            {
              qint64 const delta = targetDialHz - currentDialFrequencyHz;
              if (delta >= std::numeric_limits<int>::min ()
                  && delta <= std::numeric_limits<int>::max ())
                {
                  offsetHz = static_cast<int> (delta);
                }
            }
        }
    }

  if (kind.isEmpty ())
    {
      QRegularExpression obsoleteExpression (
          QStringLiteral ("<QSYF>(\\d{4,12})</QSYF>"),
          QRegularExpression::CaseInsensitiveOption);
      QRegularExpressionMatch const obsoleteMatch =
          obsoleteExpression.match (trimmed);
      if (obsoleteMatch.hasMatch ())
        {
          bool ok = false;
          qint64 const parsed = obsoleteMatch.captured (1).toLongLong (&ok);
          if (ok && parsed > 0)
            {
              targetDialHz = parsed;
              sourceTag = QStringLiteral ("<QSYF>");
              kind = QStringLiteral ("frequency");
              if (currentDialFrequencyHz > 0)
                {
                  qint64 const delta = targetDialHz - currentDialFrequencyHz;
                  if (delta >= std::numeric_limits<int>::min ()
                      && delta <= std::numeric_limits<int>::max ())
                    {
                      offsetHz = static_cast<int> (delta);
                    }
                }
            }
        }
    }

  if (kind.isEmpty ())
    {
      return map;
    }

  if (targetDialHz <= 0 && currentDialFrequencyHz > 0 && offsetHz != 0)
    {
      targetDialHz = currentDialFrequencyHz + offsetHz;
    }

  QStringList parts;
  if (offsetHz != 0)
    {
      parts.push_back (QStringLiteral ("%1%2 Hz")
                           .arg (offsetHz > 0 ? QStringLiteral ("+")
                                               : QString {})
                           .arg (offsetHz));
    }
  if (targetDialHz > 0)
    {
      parts.push_back (QStringLiteral ("%1 Hz").arg (targetDialHz));
    }
  QString const summary = parts.isEmpty ()
      ? QStringLiteral ("QSY invite")
      : QStringLiteral ("QSY %1").arg (parts.join (QStringLiteral (" -> ")));

  map.insert (QStringLiteral ("valid"), true);
  map.insert (QStringLiteral ("kind"), kind);
  map.insert (QStringLiteral ("sourceTag"), sourceTag);
  map.insert (QStringLiteral ("offsetHz"), offsetHz);
  map.insert (QStringLiteral ("hasTargetFrequency"), targetDialHz > 0);
  map.insert (QStringLiteral ("dialFrequencyHz"),
              QVariant::fromValue<qlonglong> (targetDialHz));
  bool const allowed = targetDialHz > 0 && qsyFrequencyAllowed (targetDialHz);
  map.insert (QStringLiteral ("allowed"), allowed);
  map.insert (QStringLiteral ("rangeChecked"), targetDialHz > 0);
  map.insert (QStringLiteral ("rangeStatus"),
              targetDialHz > 0
              ? (allowed ? QStringLiteral ("Allowed")
                         : QStringLiteral ("Out of range"))
              : QStringLiteral ("No target frequency"));
  map.insert (QStringLiteral ("summary"), summary);
  return map;
}

QVariantList FT2LinkQmlAdapter::frequencyPresets () const
{
  QVariantList list;
  for (FrequencyPreset const& preset : m_frequencyPresets)
    {
      list.push_back (frequencyPresetMap (
          preset.dialFrequencyHz, preset.band, preset.label));
    }
  return list;
}

QString FT2LinkQmlAdapter::frequencyPresetsText () const
{
  return frequencyPresetsToText (m_frequencyPresets);
}

QVariantMap FT2LinkQmlAdapter::setFrequencyPresets (QString const& text)
{
  QVariantMap result;
  std::vector<FrequencyPreset> const parsed = parseFrequencyPresetsText (text);
  result.insert (QStringLiteral ("ok"), !parsed.empty ());
  result.insert (QStringLiteral ("count"),
                 QVariant::fromValue<qulonglong> (
                     static_cast<qulonglong> (parsed.size ())));
  if (parsed.empty ())
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("No valid frequency presets"));
      return result;
    }
  m_frequencyPresets = parsed;
  result.insert (QStringLiteral ("text"), frequencyPresetsText ());
  emit frequencyPlanChanged ();
  persistLocalStore ();
  return result;
}

QVariantMap FT2LinkQmlAdapter::resetFrequencyPresets ()
{
  QVariantMap result;
  m_frequencyPresets = defaultFrequencyPresets ();
  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("count"),
                 QVariant::fromValue<qulonglong> (
                     static_cast<qulonglong> (m_frequencyPresets.size ())));
  result.insert (QStringLiteral ("text"), frequencyPresetsText ());
  emit frequencyPlanChanged ();
  persistLocalStore ();
  return result;
}

QVariantList FT2LinkQmlAdapter::allowedQsyRanges () const
{
  QVariantList list;
  for (AllowedQsyRange const& range : m_allowedQsyRanges)
    {
      list.push_back (allowedQsyRangeMap (
          range.fromHz, range.toHz, range.label));
    }
  return list;
}

QString FT2LinkQmlAdapter::allowedQsyRangesText () const
{
  return allowedQsyRangesToText (m_allowedQsyRanges);
}

QVariantMap FT2LinkQmlAdapter::setAllowedQsyRanges (QString const& text)
{
  QVariantMap result;
  std::vector<AllowedQsyRange> const parsed = parseAllowedQsyRangesText (text);
  result.insert (QStringLiteral ("ok"), !parsed.empty ());
  result.insert (QStringLiteral ("count"),
                 QVariant::fromValue<qulonglong> (
                     static_cast<qulonglong> (parsed.size ())));
  if (parsed.empty ())
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("No valid QSY ranges"));
      return result;
    }
  m_allowedQsyRanges = parsed;
  result.insert (QStringLiteral ("text"), allowedQsyRangesText ());
  emit frequencyPlanChanged ();
  persistLocalStore ();
  return result;
}

QVariantMap FT2LinkQmlAdapter::resetAllowedQsyRanges ()
{
  QVariantMap result;
  m_allowedQsyRanges = defaultAllowedQsyRanges ();
  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("count"),
                 QVariant::fromValue<qulonglong> (
                     static_cast<qulonglong> (m_allowedQsyRanges.size ())));
  result.insert (QStringLiteral ("text"), allowedQsyRangesText ());
  emit frequencyPlanChanged ();
  persistLocalStore ();
  return result;
}

QVariantList FT2LinkQmlAdapter::frequencySchedule () const
{
  QVariantList list;
  quint64 const nowMs = static_cast<quint64> (
      QDateTime::currentMSecsSinceEpoch ());
  FrequencyScheduleEntry const* active = activeFrequencyScheduleEntry (nowMs);
  for (FrequencyScheduleEntry const& entry : m_frequencySchedule)
    {
      list.push_back (frequencyScheduleMap (
          entry, active == &entry, nowMs));
    }
  return list;
}

QString FT2LinkQmlAdapter::frequencyScheduleText () const
{
  return frequencyScheduleToText (m_frequencySchedule);
}

QVariantMap FT2LinkQmlAdapter::setFrequencySchedule (QString const& text)
{
  QVariantMap result;
  QString const trimmed = text.trimmed ();
  if (trimmed.isEmpty ())
    {
      m_frequencySchedule.clear ();
      result.insert (QStringLiteral ("ok"), true);
      result.insert (QStringLiteral ("count"), 0);
      result.insert (QStringLiteral ("text"), QString {});
      emit frequencyPlanChanged ();
      persistLocalStore ();
      return result;
    }

  std::vector<FrequencyScheduleEntry> const parsed =
      parseFrequencyScheduleText (trimmed);
  result.insert (QStringLiteral ("ok"), !parsed.empty ());
  result.insert (QStringLiteral ("count"),
                 QVariant::fromValue<qulonglong> (
                     static_cast<qulonglong> (parsed.size ())));
  if (parsed.empty ())
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("No valid frequency schedule entries"));
      return result;
    }
  m_frequencySchedule = parsed;
  result.insert (QStringLiteral ("text"), frequencyScheduleText ());
  emit frequencyPlanChanged ();
  persistLocalStore ();
  return result;
}

QVariantMap FT2LinkQmlAdapter::resetFrequencySchedule ()
{
  QVariantMap result;
  m_frequencySchedule.clear ();
  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("count"), 0);
  result.insert (QStringLiteral ("text"), QString {});
  emit frequencyPlanChanged ();
  persistLocalStore ();
  return result;
}

QVariantMap FT2LinkQmlAdapter::activeFrequencySchedule (quint64 nowMs) const
{
  FrequencyScheduleEntry const* active = activeFrequencyScheduleEntry (nowMs);
  if (!active)
    {
      QVariantMap map;
      map.insert (QStringLiteral ("active"), false);
      map.insert (QStringLiteral ("evaluatedAtMs"),
                  QVariant::fromValue<qulonglong> (nowMs));
      return map;
    }
  return frequencyScheduleMap (*active, true, nowMs);
}

bool FT2LinkQmlAdapter::qsyFrequencyAllowed (qint64 dialFrequencyHz) const
{
  if (dialFrequencyHz <= 0)
    {
      return false;
    }
  for (AllowedQsyRange const& range : m_allowedQsyRanges)
    {
      if (dialFrequencyHz >= range.fromHz && dialFrequencyHz <= range.toHz)
        {
          return true;
        }
    }
  return false;
}

QVariantMap FT2LinkQmlAdapter::callingFrequencyGuard (
    QString const& action,
    qint64 dialFrequencyHz,
    qint64 callingFrequencyHz) const
{
  return callingFrequencyGuardAt (
      action,
      dialFrequencyHz,
      callingFrequencyHz,
      static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ()));
}

QVariantMap FT2LinkQmlAdapter::callingFrequencyGuardAt (
    QString const& action,
    qint64 dialFrequencyHz,
    qint64 callingFrequencyHz,
    quint64 nowMs) const
{
  QString const normalizedAction = action.trimmed ().toUpper ();
  QVariantMap map;
  map.insert (QStringLiteral ("action"), normalizedAction);
  map.insert (QStringLiteral ("dialFrequencyHz"),
              QVariant::fromValue<qlonglong> (dialFrequencyHz));
  map.insert (QStringLiteral ("callingFrequencyHz"),
              QVariant::fromValue<qlonglong> (callingFrequencyHz));
  map.insert (QStringLiteral ("configured"), callingFrequencyHz > 0);
  map.insert (QStringLiteral ("nearCallingFrequency"), false);
  map.insert (QStringLiteral ("allowed"), true);
  map.insert (QStringLiteral ("blocked"), false);
  map.insert (QStringLiteral ("warning"), QString {});
  map.insert (QStringLiteral ("message"), QString {});
  map.insert (QStringLiteral ("scheduleActive"), false);

  FrequencyScheduleEntry const* schedule = activeFrequencyScheduleEntry (nowMs);
  qint64 protectedFrequencyHz = callingFrequencyHz;
  if (schedule)
    {
      map.insert (QStringLiteral ("scheduleActive"), true);
      map.insert (QStringLiteral ("scheduleAction"),
                  sanitizedScheduleAction (schedule->action));
      map.insert (QStringLiteral ("scheduleLabel"),
                  schedule->label.trimmed ());
      map.insert (QStringLiteral ("scheduleDialFrequencyHz"),
                  QVariant::fromValue<qlonglong> (
                      schedule->dialFrequencyHz));
      map.insert (QStringLiteral ("scheduleCqType"),
                  sanitizedCqType (schedule->cqType));
      if (schedule->dialFrequencyHz > 0)
        {
          protectedFrequencyHz = schedule->dialFrequencyHz;
        }
    }

  if (dialFrequencyHz <= 0 || protectedFrequencyHz <= 0)
    {
      map.insert (QStringLiteral ("warning"),
                  QStringLiteral ("Calling frequency guard not configured"));
      return map;
    }

  qint64 const deltaHz = std::llabs (dialFrequencyHz - protectedFrequencyHz);
  map.insert (QStringLiteral ("deltaHz"),
              QVariant::fromValue<qlonglong> (deltaHz));
  map.insert (QStringLiteral ("protectedFrequencyHz"),
              QVariant::fromValue<qlonglong> (protectedFrequencyHz));
  bool const nearCallingFrequency = deltaHz <= 250;
  map.insert (QStringLiteral ("nearCallingFrequency"), nearCallingFrequency);
  if (!nearCallingFrequency)
    {
      return map;
    }

  static QStringList const narrowAllowed {
    QStringLiteral ("CQ"),
    QStringLiteral ("BEACON"),
    QStringLiteral ("AUTO CQ"),
    QStringLiteral ("AUTO BEACON"),
    QStringLiteral ("BCAST"),
    QStringLiteral ("PATH"),
    QStringLiteral ("PATH?"),
    QStringLiteral ("PATH!"),
    QStringLiteral ("PING")
  };
  if (narrowAllowed.contains (normalizedAction))
    {
      map.insert (QStringLiteral ("warning"),
                  QStringLiteral ("NARROW control on calling frequency"));
      return map;
    }

  if (schedule)
    {
      QString const scheduleAction = sanitizedScheduleAction (schedule->action);
      bool const protectedBySchedule =
          scheduleAction == QStringLiteral ("CALLING")
          || scheduleAction == QStringLiteral ("CQ")
          || scheduleAction == QStringLiteral ("BEACON")
          || scheduleAction == QStringLiteral ("EMCOMM")
          || scheduleAction == QStringLiteral ("QUIET");
      if (scheduleAction == QStringLiteral ("DATA"))
        {
          map.insert (QStringLiteral ("warning"),
                      QStringLiteral ("DATA window active in frequency schedule"));
          return map;
        }
      if (protectedBySchedule)
        {
          QString const message = QStringLiteral (
              "Frequency schedule guard: %1 window %2 protects %3 before %4 wide data TX")
              .arg (scheduleAction,
                    utcMinuteRangeText (schedule->startMinute,
                                        schedule->endMinute),
                    QString::number (protectedFrequencyHz),
                    normalizedAction.isEmpty ()
                    ? QStringLiteral ("this")
                    : normalizedAction);
          map.insert (QStringLiteral ("allowed"), false);
          map.insert (QStringLiteral ("blocked"), true);
          map.insert (QStringLiteral ("message"), message);
          return map;
        }
    }

  QString const message = QStringLiteral (
      "Calling frequency guard: move off CF before %1 wide data TX")
      .arg (normalizedAction.isEmpty ()
            ? QStringLiteral ("this")
            : normalizedAction);
  map.insert (QStringLiteral ("allowed"), false);
  map.insert (QStringLiteral ("blocked"), true);
  map.insert (QStringLiteral ("message"), message);
  return map;
}

QVariantMap FT2LinkQmlAdapter::presence () const
{
  QVariantMap map;
  map.insert (QStringLiteral ("awayEnabled"), m_awayEnabled);
  map.insert (QStringLiteral ("awayAcceptsQsy"), m_awayAcceptsQsy);
  map.insert (QStringLiteral ("awayMessage"), m_awayMessage);
  map.insert (QStringLiteral ("welcomeEnabled"), m_welcomeEnabled);
  map.insert (QStringLiteral ("welcomeMessage"), m_welcomeMessage);
  map.insert (QStringLiteral ("autoReplyEnabled"), m_autoReplyEnabled);
  map.insert (QStringLiteral ("autoAwayEnabled"), m_autoAwayEnabled);
  map.insert (QStringLiteral ("autoAwayMinutes"), m_autoAwayMinutes);
  map.insert (QStringLiteral ("autoAwayActive"), m_autoAwayActivated);
  map.insert (QStringLiteral ("callIdIntervalMinutes"),
              m_callIdIntervalMinutes);
  map.insert (QStringLiteral ("autoDisconnectMinutes"),
              m_autoDisconnectMinutes);
  map.insert (QStringLiteral ("incomingPingsEnabled"),
              m_incomingPingsEnabled);
  map.insert (QStringLiteral ("lastHeardPeekingEnabled"),
              m_lastHeardPeekingEnabled);
  map.insert (QStringLiteral ("lastConnectionsPeekingEnabled"),
              m_lastConnectionsPeekingEnabled);
  map.insert (QStringLiteral ("parkedVmailPeekingEnabled"),
              m_parkedVmailPeekingEnabled);
  map.insert (QStringLiteral ("vmailParkingEnabled"),
              m_vmailParkingEnabled);
  map.insert (QStringLiteral ("snrReportSendingEnabled"),
              m_snrReportSendingEnabled);
  map.insert (QStringLiteral ("verboseSnrAutoAcceptEnabled"),
              m_verboseSnrAutoAcceptEnabled);
  map.insert (QStringLiteral ("infoInquireEnabled"),
              m_infoInquireEnabled);
  map.insert (QStringLiteral ("lastOperatorActivityMs"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_lastOperatorActivityMs)));
  map.insert (QStringLiteral ("activeTag"),
              m_awayEnabled
              ? (m_awayAcceptsQsy ? QStringLiteral ("<AWQ>")
                                   : QStringLiteral ("<AWAY>"))
              : QString {});
  return map;
}

QVariantMap FT2LinkQmlAdapter::configurePresence (
    bool awayEnabledValue,
    bool awayAcceptsQsyValue,
    QString const& awayMessageValue,
    bool welcomeEnabledValue,
    QString const& welcomeMessageValue)
{
  QString awayText = sanitizedCannedText (awayMessageValue, 240);
  QString welcomeText = sanitizedCannedText (welcomeMessageValue, 240);
  if (awayText.isEmpty ())
    {
      awayText = QStringLiteral ("QRX DE <MYCALL>");
    }
  if (welcomeText.isEmpty ())
    {
      welcomeText = QStringLiteral ("HELLO <CALL> DE <MYCALL>");
    }

  bool const changed = m_awayEnabled != awayEnabledValue
      || m_awayAcceptsQsy != awayAcceptsQsyValue
      || m_awayMessage != awayText
      || m_welcomeEnabled != welcomeEnabledValue
      || m_welcomeMessage != welcomeText
      || m_autoAwayActivated;

  m_awayEnabled = awayEnabledValue;
  m_awayAcceptsQsy = awayAcceptsQsyValue;
  m_awayMessage = awayText;
  m_welcomeEnabled = welcomeEnabledValue;
  m_welcomeMessage = welcomeText;
  m_autoAwayActivated = false;

  QVariantMap result = presence ();
  result.insert (QStringLiteral ("ok"), true);
  if (changed)
    {
      emit presenceChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::autoAwayResult (bool changed,
                                               bool activated,
                                               bool cleared) const
{
  QVariantMap result = presence ();
  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("changed"), changed);
  result.insert (QStringLiteral ("activated"), activated);
  result.insert (QStringLiteral ("cleared"), cleared);
  return result;
}

QVariantMap FT2LinkQmlAdapter::configureAutoReply (bool enabled)
{
  bool const changed = m_autoReplyEnabled != enabled;
  m_autoReplyEnabled = enabled;

  QVariantMap result = presence ();
  result.insert (QStringLiteral ("ok"), true);
  if (changed)
    {
      emit presenceChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::configureAutoAway (bool enabled,
                                                  int minutes,
                                                  quint64 nowMs)
{
  int const clampedMinutes = std::clamp (minutes, 1, 240);
  bool const wasAutoActive = m_autoAwayActivated;
  bool const changed = m_autoAwayEnabled != enabled
      || m_autoAwayMinutes != clampedMinutes
      || (wasAutoActive && !enabled);

  m_autoAwayEnabled = enabled;
  m_autoAwayMinutes = clampedMinutes;
  if (nowMs > 0u)
    {
      m_lastOperatorActivityMs = nowMs;
    }
  if (!enabled && wasAutoActive)
    {
      m_autoAwayActivated = false;
      m_awayEnabled = false;
    }

  QVariantMap result = autoAwayResult (
      changed, false, wasAutoActive && !enabled);
  if (changed)
    {
      emit presenceChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::noteOperatorActivity (quint64 nowMs)
{
  bool const cleared = recordOperatorActivity (nowMs);
  if (cleared)
    {
      emit presenceChanged ();
      persistLocalStore ();
    }
  return autoAwayResult (cleared, false, cleared);
}

QVariantMap FT2LinkQmlAdapter::evaluateAutoAway (quint64 nowMs)
{
  if (nowMs == 0u)
    {
      nowMs = static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ());
    }
  if (m_lastOperatorActivityMs == 0u)
    {
      m_lastOperatorActivityMs = nowMs;
    }
  if (!m_autoAwayEnabled || m_awayEnabled)
    {
      return autoAwayResult (false, false, false);
    }

  quint64 const timeoutMs =
      static_cast<quint64> (m_autoAwayMinutes) * 60u * 1000u;
  bool const expired = nowMs >= m_lastOperatorActivityMs
      && nowMs - m_lastOperatorActivityMs >= timeoutMs;
  if (!expired)
    {
      return autoAwayResult (false, false, false);
    }

  m_awayEnabled = true;
  m_autoAwayActivated = true;
  emit presenceChanged ();
  persistLocalStore ();
  return autoAwayResult (true, true, false);
}

QVariantMap FT2LinkQmlAdapter::qsoAutomation () const
{
  QVariantMap map;
  map.insert (QStringLiteral ("ok"), true);
  map.insert (QStringLiteral ("callIdIntervalMinutes"),
              m_callIdIntervalMinutes);
  map.insert (QStringLiteral ("callIdEnabled"),
              m_callIdIntervalMinutes > 0);
  map.insert (QStringLiteral ("autoDisconnectMinutes"),
              m_autoDisconnectMinutes);
  map.insert (QStringLiteral ("autoDisconnectEnabled"),
              m_autoDisconnectMinutes > 0);
  map.insert (QStringLiteral ("incomingPingsEnabled"),
              m_incomingPingsEnabled);
  map.insert (QStringLiteral ("lastHeardPeekingEnabled"),
              m_lastHeardPeekingEnabled);
  map.insert (QStringLiteral ("lastConnectionsPeekingEnabled"),
              m_lastConnectionsPeekingEnabled);
  map.insert (QStringLiteral ("parkedVmailPeekingEnabled"),
              m_parkedVmailPeekingEnabled);
  map.insert (QStringLiteral ("vmailParkingEnabled"),
              m_vmailParkingEnabled);
  map.insert (QStringLiteral ("snrReportSendingEnabled"),
              m_snrReportSendingEnabled);
  map.insert (QStringLiteral ("verboseSnrAutoAcceptEnabled"),
              m_verboseSnrAutoAcceptEnabled);
  map.insert (QStringLiteral ("infoInquireEnabled"),
              m_infoInquireEnabled);
  map.insert (QStringLiteral ("callIdText"), automaticCallIdText ());
  QVariantMap const privacy = privacyProfile ();
  map.insert (QStringLiteral ("privacyPreset"),
              privacy.value (QStringLiteral ("preset")).toString ());
  map.insert (QStringLiteral ("privacySummary"),
              privacy.value (QStringLiteral ("summary")).toString ());
  return map;
}

QVariantMap FT2LinkQmlAdapter::privacyProfile () const
{
  bool const isOpen =
      m_incomingPingsEnabled
      && m_lastHeardPeekingEnabled
      && m_lastConnectionsPeekingEnabled
      && m_parkedVmailPeekingEnabled
      && m_vmailParkingEnabled
      && m_snrReportSendingEnabled
      && m_infoInquireEnabled;
  bool const isControlled =
      m_incomingPingsEnabled
      && m_lastHeardPeekingEnabled
      && !m_lastConnectionsPeekingEnabled
      && !m_parkedVmailPeekingEnabled
      && m_vmailParkingEnabled
      && m_snrReportSendingEnabled
      && m_infoInquireEnabled;
  bool const isQuiet =
      !m_incomingPingsEnabled
      && !m_lastHeardPeekingEnabled
      && !m_lastConnectionsPeekingEnabled
      && !m_parkedVmailPeekingEnabled
      && !m_vmailParkingEnabled
      && !m_snrReportSendingEnabled
      && !m_infoInquireEnabled;

  QString preset = QStringLiteral ("CUSTOM");
  if (isOpen)
    {
      preset = QStringLiteral ("OPEN");
    }
  else if (isControlled)
    {
      preset = QStringLiteral ("CONTROL");
    }
  else if (isQuiet)
    {
      preset = QStringLiteral ("QUIET");
    }

  QStringList enabled;
  if (m_incomingPingsEnabled)
    {
      enabled << QStringLiteral ("PING");
    }
  if (m_lastHeardPeekingEnabled)
    {
      enabled << QStringLiteral ("LH");
    }
  if (m_lastConnectionsPeekingEnabled)
    {
      enabled << QStringLiteral ("LC");
    }
  if (m_parkedVmailPeekingEnabled)
    {
      enabled << QStringLiteral ("VM");
    }
  if (m_vmailParkingEnabled)
    {
      enabled << QStringLiteral ("PARK");
    }
  if (m_snrReportSendingEnabled)
    {
      enabled << QStringLiteral ("SNR");
    }
  if (m_infoInquireEnabled)
    {
      enabled << QStringLiteral ("INFO");
    }

  QVariantMap map;
  map.insert (QStringLiteral ("ok"), true);
  map.insert (QStringLiteral ("preset"), preset);
  map.insert (QStringLiteral ("incomingPingsEnabled"),
              m_incomingPingsEnabled);
  map.insert (QStringLiteral ("lastHeardPeekingEnabled"),
              m_lastHeardPeekingEnabled);
  map.insert (QStringLiteral ("lastConnectionsPeekingEnabled"),
              m_lastConnectionsPeekingEnabled);
  map.insert (QStringLiteral ("parkedVmailPeekingEnabled"),
              m_parkedVmailPeekingEnabled);
  map.insert (QStringLiteral ("vmailParkingEnabled"),
              m_vmailParkingEnabled);
  map.insert (QStringLiteral ("snrReportSendingEnabled"),
              m_snrReportSendingEnabled);
  map.insert (QStringLiteral ("infoInquireEnabled"),
              m_infoInquireEnabled);
  map.insert (QStringLiteral ("enabled"), enabled);
  map.insert (QStringLiteral ("summary"),
              enabled.isEmpty ()
              ? QStringLiteral ("No automatic disclosure")
              : QStringLiteral ("Shares %1").arg (
                    enabled.join (QStringLiteral (","))));
  return map;
}

QVariantMap FT2LinkQmlAdapter::applyPrivacyPreset (QString const& preset)
{
  QString clean = preset.trimmed ().toUpper ();
  if (clean == QStringLiteral ("NORMAL")
      || clean == QStringLiteral ("BALANCED")
      || clean == QStringLiteral ("CONTROLLED"))
    {
      clean = QStringLiteral ("CONTROL");
    }
  if (clean != QStringLiteral ("OPEN")
      && clean != QStringLiteral ("CONTROL")
      && clean != QStringLiteral ("QUIET"))
    {
      clean = QStringLiteral ("CONTROL");
    }

  bool incoming = true;
  bool lastHeard = true;
  bool lastConnections = true;
  bool parkedVmail = true;
  bool vmailParking = true;
  bool snrReport = true;
  bool infoInquire = true;
  if (clean == QStringLiteral ("CONTROL"))
    {
      lastConnections = false;
      parkedVmail = false;
    }
  else if (clean == QStringLiteral ("QUIET"))
    {
      incoming = false;
      lastHeard = false;
      lastConnections = false;
      parkedVmail = false;
      vmailParking = false;
      snrReport = false;
      infoInquire = false;
    }

  bool const changed =
      m_incomingPingsEnabled != incoming
      || m_lastHeardPeekingEnabled != lastHeard
      || m_lastConnectionsPeekingEnabled != lastConnections
      || m_parkedVmailPeekingEnabled != parkedVmail
      || m_vmailParkingEnabled != vmailParking
      || m_snrReportSendingEnabled != snrReport
      || m_infoInquireEnabled != infoInquire;

  m_incomingPingsEnabled = incoming;
  m_lastHeardPeekingEnabled = lastHeard;
  m_lastConnectionsPeekingEnabled = lastConnections;
  m_parkedVmailPeekingEnabled = parkedVmail;
  m_vmailParkingEnabled = vmailParking;
  m_snrReportSendingEnabled = snrReport;
  m_infoInquireEnabled = infoInquire;

  QVariantMap result = privacyProfile ();
  result.insert (QStringLiteral ("changed"), changed);
  result.insert (QStringLiteral ("requestedPreset"), clean);
  if (changed)
    {
      emit qsoAutomationChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::qsoAutomationResult (
    bool changed,
    int callIdsQueued,
    int sessionsClosed) const
{
  QVariantMap result = qsoAutomation ();
  result.insert (QStringLiteral ("changed"), changed);
  result.insert (QStringLiteral ("callIdsQueued"), callIdsQueued);
  result.insert (QStringLiteral ("sessionsClosed"), sessionsClosed);
  return result;
}

QVariantMap FT2LinkQmlAdapter::configureQsoAutomation (
    int callIdIntervalMinutes,
    int autoDisconnectMinutes)
{
  int const cleanCallId = std::clamp (callIdIntervalMinutes, 0, 240);
  int const cleanDisconnect = std::clamp (autoDisconnectMinutes, 0, 240);
  bool const changed = m_callIdIntervalMinutes != cleanCallId
      || m_autoDisconnectMinutes != cleanDisconnect;

  m_callIdIntervalMinutes = cleanCallId;
  m_autoDisconnectMinutes = cleanDisconnect;

  QVariantMap result = qsoAutomationResult (changed, 0, 0);
  if (changed)
    {
      emit qsoAutomationChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::configureIncomingPings (bool enabled)
{
  bool const changed = m_incomingPingsEnabled != enabled;
  m_incomingPingsEnabled = enabled;
  QVariantMap result = qsoAutomationResult (changed, 0, 0);
  if (changed)
    {
      emit qsoAutomationChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::configureLastHeardPeeking (bool enabled)
{
  bool const changed = m_lastHeardPeekingEnabled != enabled;
  m_lastHeardPeekingEnabled = enabled;
  QVariantMap result = qsoAutomationResult (changed, 0, 0);
  if (changed)
    {
      emit qsoAutomationChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::configureLastConnectionsPeeking (bool enabled)
{
  bool const changed = m_lastConnectionsPeekingEnabled != enabled;
  m_lastConnectionsPeekingEnabled = enabled;
  QVariantMap result = qsoAutomationResult (changed, 0, 0);
  if (changed)
    {
      emit qsoAutomationChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::configureParkedVmailPeeking (bool enabled)
{
  bool const changed = m_parkedVmailPeekingEnabled != enabled;
  m_parkedVmailPeekingEnabled = enabled;
  QVariantMap result = qsoAutomationResult (changed, 0, 0);
  if (changed)
    {
      emit qsoAutomationChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::configureVmailParking (bool enabled)
{
  bool const changed = m_vmailParkingEnabled != enabled;
  m_vmailParkingEnabled = enabled;
  QVariantMap result = qsoAutomationResult (changed, 0, 0);
  if (changed)
    {
      emit qsoAutomationChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::configureSnrReportSending (bool enabled)
{
  bool const changed = m_snrReportSendingEnabled != enabled;
  m_snrReportSendingEnabled = enabled;
  QVariantMap result = qsoAutomationResult (changed, 0, 0);
  if (changed)
    {
      emit qsoAutomationChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::configureVerboseSnrAutoAccept (bool enabled)
{
  bool const changed = m_verboseSnrAutoAcceptEnabled != enabled;
  m_verboseSnrAutoAcceptEnabled = enabled;
  QVariantMap result = qsoAutomationResult (changed, 0, 0);
  if (changed)
    {
      emit qsoAutomationChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::configureInfoInquire (bool enabled)
{
  bool const changed = m_infoInquireEnabled != enabled;
  m_infoInquireEnabled = enabled;
  QVariantMap result = qsoAutomationResult (changed, 0, 0);
  if (changed)
    {
      emit qsoAutomationChanged ();
      persistLocalStore ();
    }
  return result;
}

QString FT2LinkQmlAdapter::automaticCallIdText () const
{
  QString const localCall = normalizeCallsign (
      QString::fromStdString (m_model.localStation ().call));
  return localCall.isEmpty ()
      ? QString {}
      : QStringLiteral ("DE %1 <ID>").arg (localCall);
}

bool FT2LinkQmlAdapter::isAutomaticCallIdText (QString const& text) const
{
  QString const clean = text.simplified ().toUpper ();
  QString const exact = automaticCallIdText ().simplified ().toUpper ();
  if (!exact.isEmpty () && clean == exact)
    {
      return true;
    }

  QRegularExpression expression (
      QStringLiteral ("^DE\\s+[A-Z0-9/.-]{2,24}\\s+<ID>$"),
      QRegularExpression::CaseInsensitiveOption);
  return expression.match (clean).hasMatch ();
}

quint64 FT2LinkQmlAdapter::sessionLastRealActivityMs (
    AppSession const& session) const
{
  quint64 last = session.openedAtMs;
  for (ChatMessage const& message : session.messages)
    {
      QString const text = QString::fromStdString (message.text);
      bool const automaticCallId =
          message.direction == ChatMessageDirection::Outgoing
          && isAutomaticCallIdText (text);
      if (!automaticCallId)
        {
          last = std::max (last, static_cast<quint64> (message.atMs));
        }
    }
  return last;
}

QVariantMap FT2LinkQmlAdapter::evaluateQsoAutomation (quint64 nowMs)
{
  if (nowMs == 0u)
    {
      nowMs = static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ());
    }

  int callIdsQueued = 0;
  int sessionsClosed = 0;
  QString const callIdText = automaticCallIdText ();
  std::vector<AppSession> const sessions = m_model.sessions ();
  for (AppSession const& session : sessions)
    {
      if (session.state != AppSessionState::Connected)
        {
          continue;
        }

      quint64 const realActivityMs = sessionLastRealActivityMs (session);
      if (m_autoDisconnectMinutes > 0
          && nowMs >= realActivityMs
          && nowMs - realActivityMs
              >= static_cast<quint64> (m_autoDisconnectMinutes) * 60000u)
        {
          appendSystemText (
              session.sessionId,
              QStringLiteral ("AUTO DISCONNECT after %1 min idle")
                  .arg (m_autoDisconnectMinutes),
              nowMs);
          if (closeSession (session.sessionId, nowMs))
            {
              m_lastCallIdQueuedAtMs.erase (session.sessionId);
              ++sessionsClosed;
            }
          continue;
        }

      if (m_callIdIntervalMinutes <= 0 || callIdText.isEmpty ())
        {
          continue;
        }

      bool pendingAutomaticId = false;
      for (ChatMessage const& message : session.messages)
        {
          if (message.direction == ChatMessageDirection::Outgoing
              && message.delivery == ChatDeliveryState::Pending
              && isAutomaticCallIdText (
                  QString::fromStdString (message.text)))
            {
              pendingAutomaticId = true;
              break;
            }
        }
      if (pendingAutomaticId)
        {
          continue;
        }

      quint64 baselineMs = session.openedAtMs;
      std::map<quint16, quint64>::const_iterator it =
          m_lastCallIdQueuedAtMs.find (session.sessionId);
      if (it != m_lastCallIdQueuedAtMs.end ())
        {
          baselineMs = it->second;
        }
      if (nowMs < baselineMs
          || nowMs - baselineMs
              < static_cast<quint64> (m_callIdIntervalMinutes) * 60000u)
        {
          continue;
        }

      std::string error;
      if (!m_model.queueOutgoingText (
              session.sessionId,
              callIdText.toStdString (),
              nowMs,
              &error))
        {
          setLastError (QString::fromStdString (error));
          continue;
        }
      m_lastCallIdQueuedAtMs[session.sessionId] = nowMs;
      emit messagesChanged (session.sessionId);
      recordQsoSession (
          session.sessionId,
          nowMs,
          QStringLiteral ("call id queued"));
      ++callIdsQueued;
    }

  bool const changed = callIdsQueued > 0 || sessionsClosed > 0;
  if (changed)
    {
      emit sessionsChanged ();
      clearLastError ();
    }
  return qsoAutomationResult (changed, callIdsQueued, sessionsClosed);
}

QStringList FT2LinkQmlAdapter::blockedCalls () const
{
  return m_blockedCalls;
}

QString FT2LinkQmlAdapter::blockedCallsText () const
{
  return m_blockedCalls.join (QStringLiteral (", "));
}

QVariantMap FT2LinkQmlAdapter::setBlockedCalls (QString const& callsText)
{
  QStringList parsed = parseBlockedCallsText (callsText);
  QVariantMap result;
  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("count"), parsed.size ());
  result.insert (QStringLiteral ("text"), parsed.join (QStringLiteral (", ")));

  if (parsed == m_blockedCalls)
    {
      return result;
    }

  m_blockedCalls = parsed;
  emit blockListChanged ();
  emit stationCountChanged ();
  persistLocalStore ();
  return result;
}

QVariantMap FT2LinkQmlAdapter::addBlockedCall (QString const& call)
{
  QStringList calls = m_blockedCalls;
  QString const clean = sanitizedBlockedCall (call);
  QVariantMap result;
  result.insert (QStringLiteral ("ok"), false);
  result.insert (QStringLiteral ("call"), clean);
  if (clean.size () < 2)
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("A callsign is required"));
      return result;
    }
  if (!calls.contains (clean))
    {
      calls.push_back (clean);
      calls.sort ();
    }
  m_blockedCalls = calls;
  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("count"), m_blockedCalls.size ());
  result.insert (QStringLiteral ("text"), blockedCallsText ());
  emit blockListChanged ();
  emit stationCountChanged ();
  persistLocalStore ();
  return result;
}

QVariantMap FT2LinkQmlAdapter::deleteBlockedCall (QString const& call)
{
  QString const clean = sanitizedBlockedCall (call);
  QVariantMap result;
  result.insert (QStringLiteral ("ok"), false);
  result.insert (QStringLiteral ("call"), clean);
  if (clean.isEmpty ())
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("A callsign is required"));
      return result;
    }
  int const removed = m_blockedCalls.removeAll (clean);
  result.insert (QStringLiteral ("ok"), removed > 0);
  result.insert (QStringLiteral ("removed"), removed);
  result.insert (QStringLiteral ("count"), m_blockedCalls.size ());
  result.insert (QStringLiteral ("text"), blockedCallsText ());
  if (removed > 0)
    {
      emit blockListChanged ();
      emit stationCountChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::clearBlockedCalls ()
{
  QVariantMap result;
  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("count"), 0);
  result.insert (QStringLiteral ("text"), QString {});
  if (m_blockedCalls.isEmpty ())
    {
      return result;
    }
  m_blockedCalls.clear ();
  emit blockListChanged ();
  emit stationCountChanged ();
  persistLocalStore ();
  return result;
}

bool FT2LinkQmlAdapter::isCallBlocked (QString const& call) const
{
  QString const clean = sanitizedBlockedCall (call);
  return !clean.isEmpty () && m_blockedCalls.contains (clean);
}

QVariantList FT2LinkQmlAdapter::broadcasts () const
{
  QVariantList list;
  for (BroadcastMessage const& message : m_broadcasts)
    {
      list.push_back (broadcastMap (
          message.fromCall,
          message.text,
          message.source,
          message.alertTags,
          message.atMs));
    }
  return list;
}

QVariantList FT2LinkQmlAdapter::alertEvents () const
{
  QVariantList list;
  for (AlertEvent const& alert : m_alerts)
    {
      list.push_back (alertMap (
          alert.fromCall,
          alert.text,
          alert.source,
          alert.tag,
          alert.atMs));
    }
  return list;
}

QStringList FT2LinkQmlAdapter::alertTags () const
{
  QStringList tags = defaultAlertTags ();
  for (QString const& tag : m_customAlertTags)
    {
      if (!tags.contains (tag))
        {
          tags.push_back (tag);
        }
    }
  return tags;
}

QStringList FT2LinkQmlAdapter::customAlertTags () const
{
  return m_customAlertTags;
}

QVariantMap FT2LinkQmlAdapter::setCustomAlertTags (QString const& tagsText)
{
  QVariantMap result;
  QStringList const parsed = parseAlertTagsText (tagsText);
  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("tags"), parsed);
  result.insert (QStringLiteral ("count"), parsed.size ());
  bool const changed = parsed != m_customAlertTags;
  m_customAlertTags = parsed;
  if (changed)
    {
      emit alertTagsChanged ();
      persistLocalStore ();
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::clearCustomAlertTags ()
{
  QVariantMap result;
  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("removed"), m_customAlertTags.size ());
  if (!m_customAlertTags.isEmpty ())
    {
      m_customAlertTags.clear ();
      emit alertTagsChanged ();
      persistLocalStore ();
    }
  return result;
}

bool FT2LinkQmlAdapter::setContactTag (QString const& call,
                                       QString const& tag,
                                       quint64 nowMs)
{
  QString const normalizedCall = normalizeCallsign (call);
  if (normalizedCall.isEmpty ())
    {
      setLastError (QStringLiteral ("FT2-Link callsign tag requires a call"));
      return false;
    }

  QString const cleanTag = sanitizedContactTag (tag);
  ContactHistory& contact = m_contactHistory[normalizedCall];
  bool const isNew = contact.call.isEmpty ();
  if (isNew)
    {
      contact.call = normalizedCall;
      contact.firstHeardMs = nowMs;
    }
  contact.tag = cleanTag;
  contact.lastEvent = contact.tag.isEmpty ()
      ? QStringLiteral ("tag cleared")
      : QStringLiteral ("tag");
  contact.lastHeardMs = nowMs;
  emit contactHistoryChanged ();
  clearLastError ();
  return true;
}

bool FT2LinkQmlAdapter::setContactDetails (QString const& call,
                                           QString const& locator,
                                           QString const& name,
                                           QString const& tag,
                                           QString const& comment,
                                           quint64 nowMs)
{
  QString const normalizedCall = normalizeCallsign (call);
  if (normalizedCall.isEmpty ())
    {
      setLastError (QStringLiteral (
          "FT2-Link contact details require a call"));
      return false;
    }

  ContactHistory& contact = m_contactHistory[normalizedCall];
  bool const isNew = contact.call.isEmpty ();
  if (isNew)
    {
      contact.call = normalizedCall;
      contact.firstHeardMs = nowMs;
    }
  contact.locator = locator.simplified ().toUpper ().left (12);
  contact.name = name.simplified ().left (48);
  contact.tag = sanitizedContactTag (tag);
  contact.comment = sanitizedContactComment (comment);
  contact.lastEvent = QStringLiteral ("contact edited");
  emit contactHistoryChanged ();
  clearLastError ();
  return true;
}

QVariantList FT2LinkQmlAdapter::mailbox () const
{
  QVariantList list;
  for (MailboxMessage const& message : m_mailbox)
    {
      list.push_back (mailboxMap (
          message.id,
          message.direction,
          message.fromCall,
          message.toCall,
          message.subject,
          message.body,
          message.state,
          message.atMs,
          message.updatedAtMs,
          message.relayNotifiedAtMs,
          message.urgent,
          message.emcomm,
          message.relayViaCall,
          message.relayHopCount,
          message.relayProtocol,
          message.emailGatewayState,
          message.emailGatewayDetail,
          message.emailGatewayAtMs));
    }
  return list;
}

QVariantList FT2LinkQmlAdapter::relayQueue (quint64 nowMs) const
{
  QVariantList list;
  for (MailboxMessage const& message : m_mailbox)
    {
      bool const relayCandidate =
          message.direction == QStringLiteral ("Parked")
          || message.direction == QStringLiteral ("Relay");
      bool const activeState =
          message.state == QStringLiteral ("Parked")
          || message.state == QStringLiteral ("Relay ready")
          || message.state == QStringLiteral ("Pending relay")
          || message.state == QStringLiteral ("Failed");
      if (!relayCandidate || !activeState || message.body.trimmed ().isEmpty ())
        {
          continue;
        }

      QVariantMap map = mailboxMap (
          message.id,
          message.direction,
          message.fromCall,
          message.toCall,
          message.subject,
          message.body,
          message.state,
          message.atMs,
          message.updatedAtMs,
          message.relayNotifiedAtMs,
          message.urgent,
          message.emcomm,
          message.relayViaCall,
          message.relayHopCount,
          message.relayProtocol,
          message.emailGatewayState,
          message.emailGatewayDetail,
          message.emailGatewayAtMs);
      QVariantMap const hint = pathRelayCandidate (message.toCall, nowMs);
      if (hint.value (QStringLiteral ("canRelay")).toBool ())
        {
          map.insert (QStringLiteral ("suggestedRelayCall"),
                      hint.value (QStringLiteral ("relayCall")).toString ());
          map.insert (QStringLiteral ("suggestedRelayLocator"),
                      hint.value (QStringLiteral ("locator")).toString ());
          map.insert (QStringLiteral ("suggestedRelayHintAgeMinutes"),
                      hint.value (QStringLiteral ("hintAgeMinutes")));
        }
      list.push_back (map);
    }
  return list;
}

QVariantList FT2LinkQmlAdapter::formTemplates () const
{
  QVariantList list;
  list.push_back (formTemplateMap (
      QStringLiteral ("ICS213"),
      QStringLiteral ("ICS-213"),
      QStringLiteral ("to=\nfrom=\nsubject=\nmessage=")));
  list.push_back (formTemplateMap (
      QStringLiteral ("SITREP"),
      QStringLiteral ("SITREP"),
      QStringLiteral ("location=\nstatus=\nneeds=\nresources=")));
  list.push_back (formTemplateMap (
      QStringLiteral ("CHECKIN"),
      QStringLiteral ("Check-in"),
      QStringLiteral ("operator=\nlocation=\ncondition=\nremarks=")));
  return list;
}

QVariantList FT2LinkQmlAdapter::forms () const
{
  QVariantList list;
  for (FormMessage const& form : m_forms)
    {
      list.push_back (formMap (
          form.id,
          form.direction,
          form.fromCall,
          form.toCall,
          form.formType,
          form.fields,
          form.state,
          form.atMs,
          form.updatedAtMs));
    }
  return list;
}

QVariantList FT2LinkQmlAdapter::fileTransfers () const
{
  QVariantList list;
  for (FileTransfer const& transfer : m_fileTransfers)
    {
      list.push_back (fileTransferMap (
          transfer.id,
          transfer.direction,
          transfer.fromCall,
          transfer.toCall,
          transfer.fileName,
          transfer.content,
          transfer.sha256,
          transfer.state,
          transfer.atMs,
          transfer.updatedAtMs));
    }
  return list;
}

QVariantList FT2LinkQmlAdapter::receivedFiles () const
{
  QVariantList list;
  for (FileTransfer const& transfer : m_fileTransfers)
    {
      if (transfer.direction != QStringLiteral ("Incoming"))
        {
          continue;
        }
      quint64 const receivedAt = transfer.updatedAtMs > 0u
          ? transfer.updatedAtMs
          : transfer.atMs;
      QVariantMap map = fileTransferMap (
          transfer.id,
          transfer.direction,
          transfer.fromCall,
          transfer.toCall,
          transfer.fileName,
          transfer.content,
          transfer.sha256,
          transfer.state,
          transfer.atMs,
          transfer.updatedAtMs);
      map.insert (QStringLiteral ("senderCall"), transfer.fromCall);
      map.insert (QStringLiteral ("receivedAtMs"),
                  QVariant::fromValue<qulonglong> (receivedAt));
      map.insert (QStringLiteral ("receivedUtc"), utcMinuteText (receivedAt));
      map.insert (QStringLiteral ("preview"),
                  compactTextPreview (transfer.content));
      map.insert (QStringLiteral ("imageLike"),
                  isImageFileName (transfer.fileName));
      list.push_back (map);
    }

  std::sort (list.begin (), list.end (), [] (QVariant const& lhs,
                                             QVariant const& rhs) {
    return lhs.toMap ().value (QStringLiteral ("receivedAtMs")).toULongLong ()
        > rhs.toMap ().value (QStringLiteral ("receivedAtMs")).toULongLong ();
  });
  return list;
}

QVariantList FT2LinkQmlAdapter::bulletins () const
{
  QVariantList list;
  for (Bulletin const& bulletin : m_bulletins)
    {
      list.push_back (bulletinMap (
          bulletin.id,
          bulletin.direction,
          bulletin.fromCall,
          bulletin.group,
          bulletin.title,
          bulletin.body,
          bulletin.state,
          bulletin.atMs,
          bulletin.updatedAtMs));
    }
  return list;
}

QVariantList FT2LinkQmlAdapter::qsoLog () const
{
  QVariantList list;
  for (std::map<quint16, QsoLogEntry>::const_iterator it = m_qsoLog.begin ();
       it != m_qsoLog.end ();
       ++it)
    {
      QsoLogEntry const& entry = it->second;
      list.prepend (qsoLogMap (
          entry.sessionId,
          entry.remoteCall,
          entry.profileName,
          entry.rateName,
          entry.state,
          entry.lastEvent,
          entry.openedAtMs,
          entry.updatedAtMs,
          entry.closedAtMs,
          entry.messageCount));
    }
  return list;
}

QVariantList FT2LinkQmlAdapter::logbookOutbox () const
{
  QVariantList list;
  for (LogbookUpload const& upload : m_logbookOutbox)
    {
      list.prepend (logbookUploadMap (
          upload.id,
          upload.sessionId,
          upload.remoteCall,
          upload.target,
          upload.state,
          upload.detail,
          upload.adif,
          upload.adifSha256,
          upload.queuedAtMs,
          upload.updatedAtMs));
    }
  return list;
}

QVariantList FT2LinkQmlAdapter::contactHistory () const
{
  QVariantList list;
  for (std::map<QString, ContactHistory>::const_iterator it =
           m_contactHistory.begin ();
       it != m_contactHistory.end ();
       ++it)
    {
      ContactHistory const& contact = it->second;
      list.push_back (contactHistoryMap (
          contact.call,
          contact.locator,
          contact.name,
          contact.tag,
          contact.comment,
          contact.lastEvent,
          contact.lastProfileName,
          contact.firstHeardMs,
          contact.lastHeardMs,
          contact.qsoCount,
          contact.messageCount,
          contact.mailCount,
          contact.formCount,
          contact.fileCount,
          contact.bulletinCount,
          contact.broadcastCount,
          contact.alertCount));
    }
  std::sort (list.begin (), list.end (), [] (QVariant const& lhs,
                                             QVariant const& rhs) {
    return lhs.toMap ().value (QStringLiteral ("lastHeardMs")).toULongLong ()
        > rhs.toMap ().value (QStringLiteral ("lastHeardMs")).toULongLong ();
  });
  return list;
}

QVariantList FT2LinkQmlAdapter::contactTimeline (QString const& call) const
{
  QString const target = normalizeCallsign (call);
  QVariantList list;
  if (target.isEmpty ())
    {
      return list;
    }

  std::vector<AppSession> const sessions = m_model.sessions ();
  for (AppSession const& session : sessions)
    {
      QString const remote = normalizeCallsign (
          QString::fromStdString (session.remoteCall));
      if (remote != target)
        {
          continue;
        }
      for (ChatMessage const& message : session.messages)
        {
          list.push_back (timelineEntryMap (
              QStringLiteral ("CHAT"),
              messageDirectionName (message.direction),
              messageDirectionName (message.direction),
              remote,
              deliveryStateName (message.delivery),
              QString::fromStdString (message.text),
              QStringLiteral ("session #%1").arg (
                  QString::number (session.sessionId, 16).toUpper ()),
              message.atMs,
              0u,
              session.sessionId));
        }
    }

  for (std::map<quint16, QsoLogEntry>::const_iterator it = m_qsoLog.begin ();
       it != m_qsoLog.end ();
       ++it)
    {
      QsoLogEntry const& entry = it->second;
      if (entry.remoteCall != target)
        {
          continue;
        }
      QString summary = entry.profileName;
      if (!entry.rateName.isEmpty ())
        {
          summary += QStringLiteral (" ") + entry.rateName;
        }
      summary += QStringLiteral (" msg ") + QString::number (entry.messageCount);
      list.push_back (timelineEntryMap (
          QStringLiteral ("QSO"),
          QStringLiteral ("QSO"),
          QStringLiteral (""),
          entry.remoteCall,
          entry.state,
          summary,
          entry.lastEvent,
          entry.updatedAtMs > 0u ? entry.updatedAtMs : entry.openedAtMs,
          0u,
          entry.sessionId));
    }

  for (BroadcastMessage const& message : m_broadcasts)
    {
      if (!callsignMatches (message.fromCall, target))
        {
          continue;
        }
      list.push_back (timelineEntryMap (
          QStringLiteral ("BCAST"),
          message.alertTags.isEmpty () ? QStringLiteral ("BCAST")
                                       : QStringLiteral ("ALERT"),
          QStringLiteral ("Incoming"),
          message.fromCall,
          message.source,
          message.text,
          message.alertTags.join (QStringLiteral (",")),
          message.atMs));
    }

  for (AlertEvent const& alert : m_alerts)
    {
      if (!callsignMatches (alert.fromCall, target))
        {
          continue;
        }
      list.push_back (timelineEntryMap (
          QStringLiteral ("ALERT"),
          alert.tag,
          QStringLiteral ("Incoming"),
          alert.fromCall,
          alert.source,
          alert.text,
          QStringLiteral ("alert tag %1").arg (alert.tag),
          alert.atMs));
    }

  for (MailboxMessage const& message : m_mailbox)
    {
      bool const fromTarget = callsignMatches (message.fromCall, target);
      bool const toTarget = callsignMatches (message.toCall, target);
      if (!fromTarget && !toTarget)
        {
          continue;
        }
      list.push_back (timelineEntryMap (
          QStringLiteral ("MAIL"),
          QStringLiteral ("MAIL"),
          message.direction,
          fromTarget ? message.fromCall : message.toCall,
          message.state,
          message.subject.isEmpty () ? QStringLiteral ("(no subject)")
                                     : message.subject,
          message.body,
          message.updatedAtMs > 0u ? message.updatedAtMs : message.atMs,
          message.id));
    }

  for (FormMessage const& form : m_forms)
    {
      bool const fromTarget = callsignMatches (form.fromCall, target);
      bool const toTarget = callsignMatches (form.toCall, target);
      if (!fromTarget && !toTarget)
        {
          continue;
        }
      list.push_back (timelineEntryMap (
          QStringLiteral ("FORM"),
          form.formType,
          form.direction,
          fromTarget ? form.fromCall : form.toCall,
          form.state,
          compactFieldsSummary (form.fields),
          QStringLiteral ("form %1").arg (form.formType),
          form.updatedAtMs > 0u ? form.updatedAtMs : form.atMs,
          form.id));
    }

  for (FileTransfer const& transfer : m_fileTransfers)
    {
      bool const fromTarget = callsignMatches (transfer.fromCall, target);
      bool const toTarget = callsignMatches (transfer.toCall, target);
      if (!fromTarget && !toTarget)
        {
          continue;
        }
      list.push_back (timelineEntryMap (
          QStringLiteral ("FILE"),
          QStringLiteral ("FILE"),
          transfer.direction,
          fromTarget ? transfer.fromCall : transfer.toCall,
          transfer.state,
          transfer.fileName,
          QStringLiteral ("%1 bytes sha %2")
              .arg (QString::number (transfer.content.toUtf8 ().size ()),
                    transfer.sha256.left (12)),
          transfer.updatedAtMs > 0u ? transfer.updatedAtMs : transfer.atMs,
          transfer.id));
    }

  for (Bulletin const& bulletin : m_bulletins)
    {
      if (!callsignMatches (bulletin.fromCall, target))
        {
          continue;
        }
      list.push_back (timelineEntryMap (
          QStringLiteral ("BBS"),
          bulletin.group,
          bulletin.direction,
          bulletin.fromCall,
          bulletin.state,
          bulletin.title,
          bulletin.body,
          bulletin.updatedAtMs > 0u ? bulletin.updatedAtMs : bulletin.atMs,
          bulletin.id));
    }

  for (PingRecord const& ping : m_pingLog)
    {
      if (!callsignMatches (ping.remoteCall, target))
        {
          continue;
        }
      QString details;
      if (ping.rttMs > 0u)
        {
          details = QStringLiteral ("%1 ms").arg (ping.rttMs);
        }
      list.push_back (timelineEntryMap (
          QStringLiteral ("PING"),
          QStringLiteral ("PING"),
          ping.direction,
          ping.remoteCall,
          ping.state,
          QStringLiteral ("token %1").arg (ping.token),
          details,
          ping.atMs));
    }

  for (PathReport const& report : m_pathReports)
    {
      if (!callsignMatches (report.remoteCall, target))
        {
          continue;
        }
      QString summary = report.snrValid
          ? QStringLiteral ("SNR %1 dB").arg (report.snrDb)
          : QStringLiteral ("quality %1").arg (report.quality, 0, 'f', 2);
      QString details = report.source;
      if (!report.profileName.isEmpty ())
        {
          details += details.isEmpty () ? report.profileName
                                        : QStringLiteral (" ") + report.profileName;
        }
      if (report.qualityValid)
        {
          details += QStringLiteral (" off %1 Hz").arg (
              report.frequencyOffsetHz, 0, 'f', 1);
        }
      list.push_back (timelineEntryMap (
          QStringLiteral ("PATH"),
          QStringLiteral ("PATH"),
          report.direction,
          report.remoteCall,
          report.snrValid ? QStringLiteral ("SNR")
                          : QStringLiteral ("Metric"),
          summary,
          details,
          report.atMs,
          report.id));
    }

  std::sort (list.begin (), list.end (), [] (QVariant const& lhs,
                                             QVariant const& rhs) {
    return lhs.toMap ().value (QStringLiteral ("atMs")).toULongLong ()
        > rhs.toMap ().value (QStringLiteral ("atMs")).toULongLong ();
  });
  return list;
}

QString FT2LinkQmlAdapter::qslCard (quint16 sessionId) const
{
  std::map<quint16, QsoLogEntry>::const_iterator logged =
      m_qsoLog.find (sessionId);
  AppSession const* session = m_model.session (sessionId);
  if (logged == m_qsoLog.end () && !session)
    {
      return {};
    }

  QString const myCall = QString::fromStdString (m_model.localStation ().call);
  QString const myGrid = QString::fromStdString (m_model.localStation ().locator);
  QString remoteCall;
  QString profileName;
  QString rateName;
  quint64 openedAtMs = 0u;
  int messageCount = 0;
  if (logged != m_qsoLog.end ())
    {
      QsoLogEntry const& entry = logged->second;
      remoteCall = entry.remoteCall;
      profileName = entry.profileName;
      rateName = entry.rateName;
      openedAtMs = entry.openedAtMs;
      messageCount = entry.messageCount;
    }
  else if (session)
    {
      remoteCall = QString::fromStdString (session->remoteCall);
      profileName = QString::fromStdString (
          decodium::ft2link::profileName (session->negotiated.profile));
      rateName = QString::fromLatin1 (
          decodium::ft2link::w2300RateModeName (
              session->negotiated.w2300RateMode));
      openedAtMs = session->openedAtMs;
      messageCount = static_cast<int> (session->messages.size ());
    }

  QDateTime const timestamp =
      QDateTime::fromMSecsSinceEpoch (
          static_cast<qint64> (openedAtMs), QTimeZone(QByteArrayLiteral("UTC")));
  QString const utc = timestamp.isValid ()
      ? timestamp.toUTC ().toString (QStringLiteral ("yyyy-MM-dd HHmm'Z'"))
      : QDateTime::currentDateTimeUtc ().toString (
          QStringLiteral ("yyyy-MM-dd HHmm'Z'"));

  QString text = QStringLiteral (
      "QSL FT2-Link %1 <> %2 UTC %3 PROFILE %4")
      .arg (myCall.isEmpty () ? QStringLiteral ("MYCALL") : myCall,
            remoteCall.isEmpty () ? QStringLiteral ("CALL") : remoteCall,
            utc,
            profileName.isEmpty () ? QStringLiteral ("--") : profileName);
  if (!rateName.isEmpty ())
    {
      text += QStringLiteral (" ") + rateName;
    }
  if (!myGrid.isEmpty ())
    {
      text += QStringLiteral (" GRID ") + myGrid;
    }
  text += QStringLiteral (" MSG ") + QString::number (messageCount);
  text += QStringLiteral (" 73");
  return text;
}

QString FT2LinkQmlAdapter::adifRecord (quint16 sessionId) const
{
  std::map<quint16, QsoLogEntry>::const_iterator logged =
      m_qsoLog.find (sessionId);
  AppSession const* session = m_model.session (sessionId);
  if (logged == m_qsoLog.end () && !session)
    {
      return {};
    }

  QString const myCall = normalizeCallsign (
      QString::fromStdString (m_model.localStation ().call));
  QString const myGrid = QString::fromStdString (
      m_model.localStation ().locator).trimmed ().toUpper ();
  QString remoteCall;
  QString profileName;
  QString rateName;
  QString state;
  quint64 openedAtMs = 0u;
  quint64 updatedAtMs = 0u;
  quint64 closedAtMs = 0u;
  int messageCount = 0;
  if (logged != m_qsoLog.end ())
    {
      QsoLogEntry const& entry = logged->second;
      remoteCall = entry.remoteCall;
      profileName = entry.profileName;
      rateName = entry.rateName;
      state = entry.state;
      openedAtMs = entry.openedAtMs;
      updatedAtMs = entry.updatedAtMs;
      closedAtMs = entry.closedAtMs;
      messageCount = entry.messageCount;
    }
  else if (session)
    {
      remoteCall = normalizeCallsign (QString::fromStdString (
          session->remoteCall));
      profileName = QString::fromStdString (
          decodium::ft2link::profileName (session->negotiated.profile));
      rateName = QString::fromLatin1 (
          decodium::ft2link::w2300RateModeName (
              session->negotiated.w2300RateMode));
      state = sessionStateName (session->state);
      openedAtMs = session->openedAtMs;
      updatedAtMs = session->updatedAtMs;
      messageCount = static_cast<int> (session->messages.size ());
    }

  if (remoteCall.isEmpty ())
    {
      return {};
    }

  QDateTime const opened =
      QDateTime::fromMSecsSinceEpoch (
          static_cast<qint64> (openedAtMs), QTimeZone(QByteArrayLiteral("UTC"))).toUTC ();
  QDateTime const ended =
      QDateTime::fromMSecsSinceEpoch (
          static_cast<qint64> (closedAtMs > 0u ? closedAtMs : updatedAtMs),
          QTimeZone(QByteArrayLiteral("UTC"))).toUTC ();
  QDateTime const safeOpened = opened.isValid ()
      ? opened
      : QDateTime::currentDateTimeUtc ();
  QDateTime const safeEnded = ended.isValid () ? ended : safeOpened;

  QString remoteGrid;
  QString remoteName;
  std::map<QString, ContactHistory>::const_iterator const contact =
      m_contactHistory.find (remoteCall);
  if (contact != m_contactHistory.end ())
    {
      remoteGrid = contact->second.locator;
      remoteName = contact->second.name;
    }

  QString record;
  record += adifField (QStringLiteral ("CALL"), remoteCall);
  record += adifField (QStringLiteral ("QSO_DATE"),
                       safeOpened.toString (QStringLiteral ("yyyyMMdd")));
  record += adifField (QStringLiteral ("TIME_ON"),
                       safeOpened.toString (QStringLiteral ("HHmmss")));
  record += adifField (QStringLiteral ("TIME_OFF"),
                       safeEnded.toString (QStringLiteral ("HHmmss")));
  record += adifField (QStringLiteral ("MODE"), QStringLiteral ("MFSK"));
  record += adifField (QStringLiteral ("SUBMODE"), QStringLiteral ("FT2"));
  record += adifField (QStringLiteral ("STATION_CALLSIGN"), myCall);
  record += adifField (QStringLiteral ("MY_GRIDSQUARE"), myGrid);
  record += adifField (QStringLiteral ("GRIDSQUARE"), remoteGrid);
  record += adifField (QStringLiteral ("NAME"), remoteName);
  record += adifField (QStringLiteral ("APP_DECODIUM_MODE"),
                       QStringLiteral ("FT2-LINK"));
  record += adifField (QStringLiteral ("APP_DECODIUM_PROFILE"), profileName);
  record += adifField (QStringLiteral ("APP_DECODIUM_RATE"), rateName);
  record += adifField (QStringLiteral ("APP_DECODIUM_SESSION_ID"),
                       QString::number (sessionId));
  record += adifField (QStringLiteral ("APP_DECODIUM_MESSAGE_COUNT"),
                       QString::number (messageCount));
  record += adifField (QStringLiteral ("COMMENT"),
                       QStringLiteral ("FT2-Link %1 %2 state %3 messages %4")
                           .arg (profileName.isEmpty ()
                                 ? QStringLiteral ("--")
                                 : profileName,
                                 rateName,
                                 state.isEmpty () ? QStringLiteral ("--")
                                                  : state,
                                 QString::number (messageCount)));
  record += QStringLiteral ("<EOR>");
  return record;
}

QString FT2LinkQmlAdapter::adifLog () const
{
  QString log = QStringLiteral (
      "Decodium FT2-Link ADIF\n<ADIF_VER:5>3.1.4\n<PROGRAMID:8>Decodium\n<EOH>\n");
  for (std::map<quint16, QsoLogEntry>::const_iterator it = m_qsoLog.begin ();
       it != m_qsoLog.end ();
       ++it)
    {
      QString const record = adifRecord (it->first).trimmed ();
      if (!record.isEmpty ())
        {
          log += record + QStringLiteral ("\n");
        }
    }
  return log;
}

QString FT2LinkQmlAdapter::adifLogPath () const
{
  return resolvedAdifLogPath ();
}

QVariantMap FT2LinkQmlAdapter::writeAdifLogFile (QString const& path) const
{
  QVariantMap result;
  QString const resolved = resolvedAdifLogPath (path);
  QString const adif = adifLog ();
  QByteArray const bytes = adif.toUtf8 ();

  result.insert (QStringLiteral ("ok"), false);
  result.insert (QStringLiteral ("path"), resolved);
  result.insert (QStringLiteral ("bytes"),
                 QVariant::fromValue<qulonglong> (
                     static_cast<qulonglong> (bytes.size ())));
  result.insert (QStringLiteral ("sha256"), sha256Hex (bytes));
  result.insert (QStringLiteral ("qsoCount"),
                 QVariant::fromValue<qulonglong> (
                     static_cast<qulonglong> (m_qsoLog.size ())));

  QFileInfo const info {resolved};
  QDir const dir = info.absoluteDir ();
  if (!dir.exists () && !QDir {}.mkpath (dir.absolutePath ()))
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("Cannot create ADIF directory %1")
                         .arg (dir.absolutePath ()));
      return result;
    }

  QSaveFile file {resolved};
  if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate))
    {
      result.insert (QStringLiteral ("error"), file.errorString ());
      return result;
    }
  if (file.write (bytes) != bytes.size ())
    {
      result.insert (QStringLiteral ("error"), file.errorString ());
      return result;
    }
  if (!file.commit ())
    {
      result.insert (QStringLiteral ("error"), file.errorString ());
      return result;
    }

  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("error"), QString {});
  return result;
}

QVariantMap FT2LinkQmlAdapter::queueLogbookUpload (quint16 sessionId,
                                                   QString const& target,
                                                   quint64 nowMs)
{
  QVariantMap result;
  result.insert (QStringLiteral ("ok"), false);
  result.insert (QStringLiteral ("queued"), false);
  result.insert (QStringLiteral ("duplicate"), false);

  QString const record = adifRecord (sessionId).trimmed ();
  if (sessionId == 0u || record.isEmpty ())
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("No ADIF record for session %1")
                         .arg (sessionId));
      return result;
    }

  QString remoteCall;
  std::map<quint16, QsoLogEntry>::const_iterator const logged =
      m_qsoLog.find (sessionId);
  if (logged != m_qsoLog.end ())
    {
      remoteCall = logged->second.remoteCall;
    }
  else if (AppSession const* session = m_model.session (sessionId))
    {
      remoteCall = normalizeCallsign (
          QString::fromStdString (session->remoteCall));
    }
  if (remoteCall.isEmpty ())
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("No remote call for session %1")
                         .arg (sessionId));
      return result;
    }

  if (nowMs == 0u)
    {
      nowMs = static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ());
    }
  QString const cleanTarget = sanitizedLogbookTarget (target);
  QString const digest = sha256Hex (record.toUtf8 ());
  for (LogbookUpload const& upload : m_logbookOutbox)
    {
      if (upload.sessionId == sessionId
          && upload.target == cleanTarget
          && upload.adifSha256 == digest
          && upload.state != QStringLiteral ("Failed"))
        {
          result = logbookUploadMap (
              upload.id,
              upload.sessionId,
              upload.remoteCall,
              upload.target,
              upload.state,
              upload.detail,
              upload.adif,
              upload.adifSha256,
              upload.queuedAtMs,
              upload.updatedAtMs);
          result.insert (QStringLiteral ("ok"), true);
          result.insert (QStringLiteral ("queued"), false);
          result.insert (QStringLiteral ("duplicate"), true);
          return result;
        }
    }

  LogbookUpload upload;
  upload.id = m_nextLogbookUploadId++;
  if (upload.id == 0u)
    {
      upload.id = m_nextLogbookUploadId++;
    }
  upload.sessionId = sessionId;
  upload.remoteCall = remoteCall;
  upload.target = cleanTarget;
  upload.state = QStringLiteral ("Queued");
  upload.detail = QStringLiteral ("Ready");
  upload.adif = record;
  upload.adifSha256 = digest;
  upload.queuedAtMs = nowMs;
  upload.updatedAtMs = nowMs;
  m_logbookOutbox.push_back (upload);
  pruneLogbookOutbox ();
  emit logbookOutboxChanged ();

  result = logbookUploadMap (
      upload.id,
      upload.sessionId,
      upload.remoteCall,
      upload.target,
      upload.state,
      upload.detail,
      upload.adif,
      upload.adifSha256,
      upload.queuedAtMs,
      upload.updatedAtMs);
  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("queued"), true);
  result.insert (QStringLiteral ("duplicate"), false);
  return result;
}

QVariantMap FT2LinkQmlAdapter::queueAllLogbookUploads (QString const& target,
                                                       quint64 nowMs)
{
  QVariantMap result;
  int queued = 0;
  int duplicates = 0;
  int failed = 0;
  QVariantList items;
  for (std::map<quint16, QsoLogEntry>::const_iterator it = m_qsoLog.begin ();
       it != m_qsoLog.end ();
       ++it)
    {
      QVariantMap const item = queueLogbookUpload (it->first, target, nowMs);
      items.push_back (item);
      if (item.value (QStringLiteral ("queued")).toBool ())
        {
          ++queued;
        }
      else if (item.value (QStringLiteral ("duplicate")).toBool ())
        {
          ++duplicates;
        }
      else if (!item.value (QStringLiteral ("ok")).toBool ())
        {
          ++failed;
        }
    }

  result.insert (QStringLiteral ("ok"), failed == 0);
  result.insert (QStringLiteral ("queued"), queued);
  result.insert (QStringLiteral ("duplicates"), duplicates);
  result.insert (QStringLiteral ("failed"), failed);
  result.insert (QStringLiteral ("target"), sanitizedLogbookTarget (target));
  result.insert (QStringLiteral ("items"), items);
  result.insert (QStringLiteral ("outboxCount"),
                 QVariant::fromValue<qulonglong> (
                     static_cast<qulonglong> (m_logbookOutbox.size ())));
  return result;
}

QVariantMap FT2LinkQmlAdapter::markLogbookUpload (quint32 uploadId,
                                                  QString const& state,
                                                  QString const& detail,
                                                  quint64 nowMs)
{
  QVariantMap result;
  result.insert (QStringLiteral ("ok"), false);
  if (uploadId == 0u)
    {
      result.insert (QStringLiteral ("error"), QStringLiteral ("Missing upload id"));
      return result;
    }
  if (nowMs == 0u)
    {
      nowMs = static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ());
    }
  QString const cleanState = sanitizedLogbookState (state);
  for (LogbookUpload& upload : m_logbookOutbox)
    {
      if (upload.id != uploadId)
        {
          continue;
        }
      upload.state = cleanState;
      upload.detail = detail.simplified ().left (240);
      upload.updatedAtMs = nowMs;
      emit logbookOutboxChanged ();
      result = logbookUploadMap (
          upload.id,
          upload.sessionId,
          upload.remoteCall,
          upload.target,
          upload.state,
          upload.detail,
          upload.adif,
          upload.adifSha256,
          upload.queuedAtMs,
          upload.updatedAtMs);
      result.insert (QStringLiteral ("ok"), true);
      return result;
    }

  result.insert (QStringLiteral ("error"),
                 QStringLiteral ("Upload id %1 not found").arg (uploadId));
  return result;
}

QVariantMap FT2LinkQmlAdapter::logbookUploadPayload (quint32 uploadId) const
{
  QVariantMap result;
  result.insert (QStringLiteral ("ok"), false);
  for (LogbookUpload const& upload : m_logbookOutbox)
    {
      if (upload.id != uploadId)
        {
          continue;
        }
      result = logbookUploadMap (
          upload.id,
          upload.sessionId,
          upload.remoteCall,
          upload.target,
          upload.state,
          upload.detail,
          upload.adif,
          upload.adifSha256,
          upload.queuedAtMs,
          upload.updatedAtMs);
      result.insert (QStringLiteral ("ok"), !upload.adif.trimmed ().isEmpty ());
      return result;
    }
  result.insert (QStringLiteral ("error"),
                 QStringLiteral ("Upload id %1 not found").arg (uploadId));
  return result;
}

QString FT2LinkQmlAdapter::logbookOutboxText () const
{
  QString text = QStringLiteral ("FT2-Link logbook outbox\n");
  text += QStringLiteral ("Total: %1\n").arg (m_logbookOutbox.size ());
  for (QVariant const& value : logbookOutbox ())
    {
      QVariantMap const item = value.toMap ();
      text += QStringLiteral ("#%1 %2 %3 %4 %5 %6\n")
          .arg (item.value (QStringLiteral ("id")).toUInt ())
          .arg (item.value (QStringLiteral ("state")).toString (),
                item.value (QStringLiteral ("target")).toString (),
                item.value (QStringLiteral ("remoteCall")).toString (),
                item.value (QStringLiteral ("queuedUtc")).toString (),
                item.value (QStringLiteral ("detail")).toString ());
    }
  return text;
}

QString FT2LinkQmlAdapter::chatHistoryLog () const
{
  QString text = QStringLiteral ("FT2-Link chat history\n");
  std::vector<AppSession> const sessions = m_model.sessions ();
  if (sessions.empty ())
    {
      if (m_qsoLog.empty ())
        {
          text += QStringLiteral ("No chat sessions\n");
          return text;
        }
      text += QStringLiteral (
          "No live chat transcript; persisted QSO summaries follow\n");
      for (std::map<quint16, QsoLogEntry>::const_iterator it =
               m_qsoLog.begin ();
           it != m_qsoLog.end ();
           ++it)
        {
          QsoLogEntry const& entry = it->second;
          text += QStringLiteral (
              "#%1 %2 %3 %4 opened %5 updated %6 messages %7\n")
              .arg (QString::number (entry.sessionId),
                    entry.remoteCall,
                    entry.state,
                    entry.profileName + (entry.rateName.isEmpty ()
                                         ? QString {}
                                         : QStringLiteral (" ")
                                               + entry.rateName),
                    utcMinuteText (entry.openedAtMs),
                    utcMinuteText (entry.updatedAtMs),
                    QString::number (entry.messageCount));
        }
      return text;
    }

  for (AppSession const& session : sessions)
    {
      QString const profileName = QString::fromStdString (
          decodium::ft2link::profileName (session.negotiated.profile));
      QString const rateName = QString::fromLatin1 (
          decodium::ft2link::w2300RateModeName (
              session.negotiated.w2300RateMode));
      text += QStringLiteral (
          "\n#%1 %2 %3 %4 opened %5 updated %6\n")
          .arg (QString::number (session.sessionId),
                QString::fromStdString (session.remoteCall),
                sessionStateName (session.state),
                profileName + (rateName.isEmpty ()
                               ? QString {}
                               : QStringLiteral (" ") + rateName),
                utcMinuteText (session.openedAtMs),
                utcMinuteText (session.updatedAtMs));

      for (ChatMessage const& message : session.messages)
        {
          QString line = QString::fromStdString (message.text);
          line.replace (QLatin1Char ('\r'), QLatin1Char (' '));
          line.replace (QLatin1Char ('\n'), QLatin1Char (' '));
          text += QStringLiteral ("[%1] %2/%3 %4\n")
              .arg (utcMinuteText (message.atMs),
                    messageDirectionName (message.direction),
                    deliveryStateName (message.delivery),
                    line.trimmed ());
        }
    }
  return text;
}

QString FT2LinkQmlAdapter::mailboxText () const
{
  QString text = QStringLiteral ("FT2-Link mailbox\n");
  text += QStringLiteral ("Unread: %1 / Total: %2\n")
      .arg (QString::number (mailboxUnreadCount ()),
            QString::number (static_cast<qulonglong> (m_mailbox.size ())));
  if (m_mailbox.empty ())
    {
      text += QStringLiteral ("No mail\n");
      return text;
    }

  for (MailboxMessage const& message : m_mailbox)
    {
      QStringList flags;
      if (message.urgent)
        {
          flags.push_back (QStringLiteral ("URGENT"));
        }
      if (message.emcomm)
        {
          flags.push_back (QStringLiteral ("EMCOMM"));
        }
      if (message.direction == QStringLiteral ("Incoming")
          && message.state != QStringLiteral ("Read"))
        {
          flags.push_back (QStringLiteral ("UNREAD"));
        }
      text += QStringLiteral (
          "\n#%1 [%2] %3 %4 -> %5 %6\nSubject: %7\n%8\n")
          .arg (QString::number (message.id),
                message.state,
                utcMinuteText (message.updatedAtMs),
                message.fromCall,
                message.toCall,
                flags.isEmpty ()
                ? QStringLiteral ("NORMAL")
                : flags.join (QStringLiteral ("+")),
                message.subject,
                message.body);
      if (!message.relayProtocol.isEmpty ()
          || !message.relayViaCall.isEmpty ()
          || message.relayHopCount > 0)
        {
          text += QStringLiteral ("Relay: %1 via %2 hops %3\n")
              .arg (message.relayProtocol.isEmpty ()
                    ? QStringLiteral ("MAIL")
                    : message.relayProtocol,
                    message.relayViaCall.isEmpty ()
                    ? QStringLiteral ("--")
                    : message.relayViaCall,
                    QString::number (message.relayHopCount));
        }
      if (!message.emailGatewayState.isEmpty ())
        {
          text += QStringLiteral ("Email gateway: %1 %2 %3\n")
              .arg (message.emailGatewayState,
                    utcMinuteText (message.emailGatewayAtMs),
                    message.emailGatewayDetail);
        }
    }
  return text;
}

QString FT2LinkQmlAdapter::relayQueueText (quint64 nowMs) const
{
  QString text = QStringLiteral ("FT2-Link relay queue\n");
  QVariantList const queue = relayQueue (nowMs);
  text += QStringLiteral ("Pending: %1\n")
      .arg (QString::number (queue.size ()));
  if (queue.isEmpty ())
    {
      text += QStringLiteral ("No relay mail pending\n");
      return text;
    }

  for (QVariant const& value : queue)
    {
      QVariantMap const item = value.toMap ();
      QString const suggested = item.value (
          QStringLiteral ("suggestedRelayCall")).toString ();
      text += QStringLiteral (
          "\n#%1 [%2] %3 %4 -> %5 %6 via %7 hops %8\nSubject: %9\n%10\n")
          .arg (QString::number (item.value (
                    QStringLiteral ("id")).toUInt ()),
                item.value (QStringLiteral ("state")).toString (),
                utcMinuteText (item.value (
                    QStringLiteral ("updatedAtMs")).toULongLong ()),
                item.value (QStringLiteral ("fromCall")).toString (),
                item.value (QStringLiteral ("toCall")).toString (),
                item.value (QStringLiteral ("relayProtocol")).toString (),
                item.value (QStringLiteral ("relayViaCall")).toString ().isEmpty ()
                ? (suggested.isEmpty () ? QStringLiteral ("--") : suggested)
                : item.value (QStringLiteral ("relayViaCall")).toString (),
                QString::number (item.value (
                    QStringLiteral ("relayHopCount")).toInt ()),
                item.value (QStringLiteral ("subject")).toString (),
                item.value (QStringLiteral ("body")).toString ());
    }
  return text;
}

bool FT2LinkQmlAdapter::markMailboxEmailGateway (quint32 messageId,
                                                 QString const& state,
                                                 QString const& detail,
                                                 quint64 nowMs)
{
  if (messageId == 0u)
    {
      setLastError (QStringLiteral ("FT2-Link email gateway mailbox id is invalid"));
      return false;
    }
  QString cleanState = state.trimmed ();
  if (cleanState.isEmpty ())
    {
      cleanState = QStringLiteral ("Queued");
    }
  cleanState = cleanState.left (32);
  QString cleanDetail = detail.simplified ().left (240);
  for (MailboxMessage& message : m_mailbox)
    {
      if (message.id != messageId)
        {
          continue;
        }
      message.emailGatewayState = cleanState;
      message.emailGatewayDetail = cleanDetail;
      message.emailGatewayAtMs = nowMs;
      message.updatedAtMs = nowMs;
      emit mailboxChanged ();
      persistLocalStore ();
      clearLastError ();
      return true;
    }
  setLastError (QStringLiteral ("FT2-Link email gateway mailbox item not found"));
  return false;
}

QVariantMap FT2LinkQmlAdapter::mailboxEmailGateway (
    quint32 messageId,
    QString const& fallbackToEmail) const
{
  QVariantMap result;
  result.insert (QStringLiteral ("ok"), false);
  result.insert (QStringLiteral ("messageId"), messageId);
  result.insert (QStringLiteral ("mailtoReady"), false);
  result.insert (QStringLiteral ("needsRecipient"), true);

  MailboxMessage const* selected = nullptr;
  for (MailboxMessage const& message : m_mailbox)
    {
      if (message.id == messageId)
        {
          selected = &message;
          break;
        }
    }
  if (!selected)
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("Mailbox item %1 not found")
                         .arg (messageId));
      return result;
    }

  QString toEmail = sanitizedEmailAddress (fallbackToEmail);
  QString const haystack = QStringLiteral ("%1 %2 %3 %4 %5")
      .arg (selected->toCall,
            selected->fromCall,
            selected->subject,
            selected->body,
            m_localProfile.email);
  if (toEmail.isEmpty ())
    {
      toEmail = sanitizedEmailAddress (haystack);
    }
  if (toEmail.isEmpty () && selected->direction == QStringLiteral ("Incoming"))
    {
      toEmail = sanitizedEmailAddress (m_localProfile.email);
    }

  QStringList priorityParts;
  if (selected->urgent)
    {
      priorityParts << QStringLiteral ("URGENT");
    }
  if (selected->emcomm)
    {
      priorityParts << QStringLiteral ("EMCOMM");
    }
  QString const priority = priorityParts.isEmpty ()
      ? QStringLiteral ("NORMAL")
      : priorityParts.join (QStringLiteral ("+"));
  QString const subjectPrefix = priority == QStringLiteral ("NORMAL")
      ? QStringLiteral ("FT2-Link VMail")
      : QStringLiteral ("FT2-Link %1 VMail").arg (priority);
  QString const cleanSubject = selected->subject.trimmed ().isEmpty ()
      ? QStringLiteral ("No subject")
      : selected->subject.trimmed ();
  QString const subject = QStringLiteral ("%1 %2->%3: %4")
      .arg (subjectPrefix,
            selected->fromCall,
            selected->toCall,
            cleanSubject);
  QString const body = plainEmailBody (
      selected->direction,
      selected->fromCall,
      selected->toCall,
      cleanSubject,
      selected->body,
      selected->state,
      priority,
      selected->updatedAtMs > 0u ? selected->updatedAtMs : selected->atMs);

  QString const fromEmail = sanitizedEmailAddress (m_localProfile.email);
  QString eml;
  eml += QStringLiteral ("From: %1\r\n").arg (
      fromEmail.isEmpty ()
      ? QStringLiteral ("Decodium FT2-Link <ft2-link@localhost>")
      : QStringLiteral ("Decodium FT2-Link <%1>").arg (fromEmail));
  eml += QStringLiteral ("To: %1\r\n").arg (
      toEmail.isEmpty () ? QStringLiteral ("undisclosed-recipients:;")
                         : toEmail);
  eml += QStringLiteral ("Subject: %1\r\n").arg (
      encodedMailHeaderValue (subject));
  eml += QStringLiteral ("Date: %1\r\n").arg (
      rfc2822DateText (
          selected->updatedAtMs > 0u ? selected->updatedAtMs
                                     : selected->atMs));
  eml += QStringLiteral ("MIME-Version: 1.0\r\n");
  eml += QStringLiteral ("Content-Type: text/plain; charset=UTF-8\r\n");
  eml += QStringLiteral ("Content-Transfer-Encoding: 8bit\r\n");
  eml += QStringLiteral ("X-Decodium-FT2Link-Mailbox-Id: %1\r\n").arg (
      selected->id);
  eml += QStringLiteral ("X-Decodium-FT2Link-From-Call: %1\r\n").arg (
      safeMailHeaderValue (selected->fromCall));
  eml += QStringLiteral ("X-Decodium-FT2Link-To-Call: %1\r\n").arg (
      safeMailHeaderValue (selected->toCall));
  eml += QStringLiteral ("X-Decodium-FT2Link-Priority: %1\r\n\r\n").arg (
      safeMailHeaderValue (priority));
  eml += body;

  QUrl mailto;
  if (!toEmail.isEmpty ())
    {
      mailto.setScheme (QStringLiteral ("mailto"));
      mailto.setPath (toEmail);
      QUrlQuery query;
      query.addQueryItem (QStringLiteral ("subject"), subject);
      query.addQueryItem (QStringLiteral ("body"), body);
      mailto.setQuery (query);
    }

  QString const fileName = QStringLiteral ("FT2-Link_VMail_%1_%2_%3.eml")
      .arg (selected->id)
      .arg (safeEmailFileNamePart (selected->toCall))
      .arg (safeEmailFileNamePart (cleanSubject));

  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("messageId"), selected->id);
  result.insert (QStringLiteral ("fromCall"), selected->fromCall);
  result.insert (QStringLiteral ("toCall"), selected->toCall);
  result.insert (QStringLiteral ("toEmail"), toEmail);
  result.insert (QStringLiteral ("subject"), subject);
  result.insert (QStringLiteral ("body"), body);
  result.insert (QStringLiteral ("eml"), eml);
  result.insert (QStringLiteral ("emlFileName"), fileName);
  result.insert (QStringLiteral ("mailtoUrl"),
                 toEmail.isEmpty () ? QString {} : mailto.toString ());
  result.insert (QStringLiteral ("mailtoReady"), !toEmail.isEmpty ());
  result.insert (QStringLiteral ("needsRecipient"), toEmail.isEmpty ());
  if (toEmail.isEmpty ())
    {
      result.insert (QStringLiteral ("warning"),
                     QStringLiteral ("No Internet email address found"));
    }
  return result;
}

QString FT2LinkQmlAdapter::mailboxEmailGatewayText (
    quint32 messageId,
    QString const& fallbackToEmail) const
{
  QVariantMap const draft = mailboxEmailGateway (messageId, fallbackToEmail);
  if (!draft.value (QStringLiteral ("ok")).toBool ())
    {
      return draft.value (QStringLiteral ("error")).toString ();
    }
  return draft.value (QStringLiteral ("eml")).toString ();
}

QString FT2LinkQmlAdapter::operationalLog () const
{
  QString text = statisticsText ();
  auto appendJson = [&text] (QString const& title, QVariantList const& list) {
    text += QStringLiteral ("\n[%1]\n").arg (title);
    text += variantListJsonText (list);
    if (!text.endsWith (QLatin1Char ('\n')))
      {
        text += QLatin1Char ('\n');
      }
  };

  appendJson (QStringLiteral ("QSO"), qsoLog ());
  appendJson (QStringLiteral ("CONTACTS"), contactHistory ());
  appendJson (QStringLiteral ("BROADCASTS"), broadcasts ());
  appendJson (QStringLiteral ("ALERTS"), alertEvents ());
  appendJson (QStringLiteral ("MAILBOX"), mailbox ());
  appendJson (QStringLiteral ("RELAY_QUEUE"), relayQueue (
      static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ())));
  appendJson (QStringLiteral ("FORMS"), forms ());
  appendJson (QStringLiteral ("FILES"), fileTransfers ());
  appendJson (QStringLiteral ("BULLETINS"), bulletins ());
  appendJson (QStringLiteral ("LOGBOOK_OUTBOX"), logbookOutbox ());
  appendJson (QStringLiteral ("PING"), pingLog ());
  appendJson (QStringLiteral ("PATH"), pathReports ());
  appendJson (QStringLiteral ("CLUSTER"), clusterLastHeard ());
  return text;
}

QString FT2LinkQmlAdapter::localStoreJson () const
{
  return QString::fromUtf8 (serializeLocalStore ());
}

QString FT2LinkQmlAdapter::logsBundleText () const
{
  QString text = QStringLiteral ("FT2-Link logs bundle\n\n");
  text += QStringLiteral ("--- STATISTICS ---\n");
  text += statisticsText ();
  text += QStringLiteral ("\n--- ADIF ---\n");
  text += adifLog ();
  text += QStringLiteral ("\n--- LOGBOOK OUTBOX ---\n");
  text += logbookOutboxText ();
  text += QStringLiteral ("\n--- CHAT HISTORY ---\n");
  text += chatHistoryLog ();
  text += QStringLiteral ("\n--- MAILBOX ---\n");
  text += mailboxText ();
  text += QStringLiteral ("\n--- RELAY QUEUE ---\n");
  text += relayQueueText (
      static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ()));
  text += QStringLiteral ("\n--- CQ/BEACON HISTORY ---\n");
  text += beaconHistoryText ();
  text += QStringLiteral ("\n--- CLUSTER LAST HEARD ---\n");
  text += clusterLastHeardText ();
  text += QStringLiteral ("\n--- OPERATIONAL LOG ---\n");
  text += operationalLog ();
  text += QStringLiteral ("\n--- LOCAL STORE JSON ---\n");
  text += localStoreJson ();
  return text;
}

QVariantList FT2LinkQmlAdapter::pingLog () const
{
  QVariantList list;
  for (PingRecord const& ping : m_pingLog)
    {
      list.prepend (pingMap (
          ping.direction,
          ping.remoteCall,
          ping.state,
          ping.token,
          ping.atMs,
          ping.rttMs));
    }
  return list;
}

QVariantList FT2LinkQmlAdapter::pathReports () const
{
  QVariantList list;
  for (PathReport const& report : m_pathReports)
    {
      list.prepend (pathReportMap (
          report.id,
          report.direction,
          report.remoteCall,
          report.locator,
          report.snrValid,
          report.snrDb,
          report.qualityValid,
          report.quality,
          report.frequencyOffsetHz,
          report.profileName,
          report.rateName,
          report.source,
          report.atMs));
    }
  return list;
}

QVariantList FT2LinkQmlAdapter::beaconHistory () const
{
  QVariantList list;
  for (BeaconHistoryEntry const& entry : m_beaconHistory)
    {
      list.prepend (beaconHistoryMap (
          entry.direction,
          entry.call,
          entry.locator,
          entry.name,
          entry.profileName,
          entry.cq,
          entry.cqType,
          entry.cqLocator,
          entry.cqSlotId,
          entry.cqSlotSizeHz,
          entry.source,
          entry.atMs));
    }
  return list;
}

QString FT2LinkQmlAdapter::beaconHistoryText () const
{
  QString text;
  text += QStringLiteral ("FT2-Link CQ/Beacon history\n");
  text += QStringLiteral ("Total: %1\n").arg (m_beaconHistory.size ());
  for (QVariant const& value : beaconHistory ())
    {
      QVariantMap const item = value.toMap ();
      QStringList fields;
      fields << item.value (QStringLiteral ("direction")).toString ();
      fields << (item.value (QStringLiteral ("cq")).toBool ()
                 ? item.value (QStringLiteral ("cqType")).toString ()
                 : QStringLiteral ("BEACON"));
      fields << item.value (QStringLiteral ("call")).toString ();
      fields << item.value (QStringLiteral ("locator")).toString ();
      fields << item.value (QStringLiteral ("profileName")).toString ();
      QString const slot = item.value (QStringLiteral ("cqSlotLabel")).toString ();
      if (!slot.isEmpty ())
        {
          fields << slot;
        }
      QString const source = item.value (QStringLiteral ("source")).toString ();
      if (!source.isEmpty ())
        {
          fields << source;
        }
      text += fields.join (QStringLiteral (" | "));
      text += QLatin1Char ('\n');
    }
  return text;
}

QVariantMap FT2LinkQmlAdapter::clusterConfig () const
{
  QVariantMap map;
  qint64 const dial = m_clusterDialFrequencyHz > 0
      ? m_clusterDialFrequencyHz
      : 0;
  map.insert (QStringLiteral ("enabled"), m_clusterEnabled);
  map.insert (QStringLiteral ("nodeId"), effectiveClusterNodeId ());
  map.insert (QStringLiteral ("configuredNodeId"),
              sanitizedClusterNodeId (m_clusterNodeId));
  map.insert (QStringLiteral ("band"), clusterBandLabel (dial));
  map.insert (QStringLiteral ("configuredBand"),
              sanitizedClusterBand (m_clusterBand));
  map.insert (QStringLiteral ("dialFrequencyHz"),
              QVariant::fromValue<qlonglong> (dial));
  map.insert (QStringLiteral ("count"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_clusterLastHeard.size ())));
  return map;
}

QVariantMap FT2LinkQmlAdapter::configureCluster (bool enabled,
                                                 QString const& nodeId,
                                                 QString const& band,
                                                 qint64 dialFrequencyHz)
{
  QVariantMap result;
  QString const cleanNode = sanitizedClusterNodeId (nodeId);
  QString const cleanBand = sanitizedClusterBand (band);
  qint64 const cleanDial = std::max<qint64> (0, dialFrequencyHz);
  bool const changed = m_clusterEnabled != enabled
      || m_clusterNodeId != cleanNode
      || m_clusterBand != cleanBand
      || m_clusterDialFrequencyHz != cleanDial;

  m_clusterEnabled = enabled;
  m_clusterNodeId = cleanNode;
  m_clusterBand = cleanBand;
  m_clusterDialFrequencyHz = cleanDial;
  result = clusterConfig ();
  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("changed"), changed);
  if (changed)
    {
      emit clusterLastHeardChanged ();
    }
  return result;
}

QVariantList FT2LinkQmlAdapter::clusterLastHeard () const
{
  QVariantList list;
  for (std::map<QString, ClusterLastHeardEntry>::const_iterator it =
           m_clusterLastHeard.begin ();
       it != m_clusterLastHeard.end ();
       ++it)
    {
      ClusterLastHeardEntry const& entry = it->second;
      list.push_back (clusterLastHeardMap (
          entry.call,
          entry.locator,
          entry.name,
          entry.profileName,
          entry.event,
          entry.source,
          entry.nodeId,
          entry.band,
          entry.dialFrequencyHz,
          entry.cq,
          entry.cqType,
          entry.firstHeardMs,
          entry.lastHeardMs,
          entry.heardCount));
    }
  std::sort (list.begin (), list.end (), [] (QVariant const& lhs,
                                             QVariant const& rhs) {
    QVariantMap const l = lhs.toMap ();
    QVariantMap const r = rhs.toMap ();
    quint64 const left = l.value (
        QStringLiteral ("lastHeardMs")).toULongLong ();
    quint64 const right = r.value (
        QStringLiteral ("lastHeardMs")).toULongLong ();
    if (left != right)
      {
        return left > right;
      }
    return l.value (QStringLiteral ("call")).toString ()
        < r.value (QStringLiteral ("call")).toString ();
  });
  return list;
}

QString FT2LinkQmlAdapter::clusterLastHeardText () const
{
  QVariantMap const config = clusterConfig ();
  QString text = QStringLiteral ("FT2-Link cluster last heard\n");
  text += QStringLiteral ("Node: %1  Band: %2  Dial: %3  Enabled: %4\n")
      .arg (config.value (QStringLiteral ("nodeId")).toString (),
            config.value (QStringLiteral ("band")).toString (),
            QString::number (config.value (
                QStringLiteral ("dialFrequencyHz")).toLongLong ()),
            config.value (QStringLiteral ("enabled")).toBool ()
            ? QStringLiteral ("yes")
            : QStringLiteral ("no"));
  text += QStringLiteral ("Total: %1\n").arg (m_clusterLastHeard.size ());
  for (QVariant const& value : clusterLastHeard ())
    {
      QVariantMap const item = value.toMap ();
      text += QStringLiteral ("%1 | %2 | %3 Hz | %4 | %5 | %6 | %7 | %8\n")
          .arg (item.value (QStringLiteral ("call")).toString (),
                item.value (QStringLiteral ("band")).toString (),
                QString::number (item.value (
                    QStringLiteral ("dialFrequencyHz")).toLongLong ()),
                item.value (QStringLiteral ("nodeId")).toString (),
                item.value (QStringLiteral ("event")).toString (),
                item.value (QStringLiteral ("source")).toString (),
                item.value (QStringLiteral ("lastHeardUtc")).toString (),
                QString::number (item.value (
                    QStringLiteral ("heardCount")).toInt ()));
    }
  return text;
}

QString FT2LinkQmlAdapter::clusterExportJson () const
{
  QJsonObject root;
  root.insert (QStringLiteral ("version"), kLocalStoreVersion);
  root.insert (QStringLiteral ("type"),
               QStringLiteral ("ft2link-cluster-last-heard"));
  root.insert (QStringLiteral ("exportedAtMs"),
               QString::number (
                   static_cast<qulonglong> (
                       QDateTime::currentMSecsSinceEpoch ())));
  root.insert (QStringLiteral ("config"),
               QJsonObject::fromVariantMap (clusterConfig ()));
  QJsonArray entries;
  for (QVariant const& value : clusterLastHeard ())
    {
      entries.append (QJsonObject::fromVariantMap (value.toMap ()));
    }
  root.insert (QStringLiteral ("clusterLastHeard"), entries);
  return QString::fromUtf8 (
      QJsonDocument {root}.toJson (QJsonDocument::Indented));
}

QVariantMap FT2LinkQmlAdapter::importClusterLastHeard (
    QString const& jsonText,
    quint64 nowMs)
{
  Q_UNUSED (nowMs);

  QVariantMap result;
  result.insert (QStringLiteral ("ok"), false);
  QByteArray const bytes = jsonText.toUtf8 ();
  QJsonParseError parseError;
  QJsonDocument const document = QJsonDocument::fromJson (bytes, &parseError);
  if (parseError.error != QJsonParseError::NoError)
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("Invalid cluster JSON: %1")
                         .arg (parseError.errorString ()));
      return result;
    }

  QJsonArray array;
  if (document.isArray ())
    {
      array = document.array ();
    }
  else if (document.isObject ())
    {
      QJsonObject const root = document.object ();
      array = root.value (QStringLiteral ("clusterLastHeard")).toArray ();
    }
  if (array.isEmpty ())
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("No clusterLastHeard records"));
      return result;
    }

  int imported = 0;
  int merged = 0;
  int skipped = 0;
  for (QJsonValue const& value : array)
    {
      QJsonObject const object = value.toObject ();
      QString const call = normalizeCallsign (
          object.value (QStringLiteral ("call")).toString ());
      if (call.isEmpty () || isCallBlocked (call))
        {
          ++skipped;
          continue;
        }
      QString const node = sanitizedClusterNodeId (
          object.value (QStringLiteral ("nodeId")).toString ());
      QString const band = sanitizedClusterBand (
          object.value (QStringLiteral ("band")).toString ());
      qint64 const dial = static_cast<qint64> (
          jsonU64 (object, QStringLiteral ("dialFrequencyHz")));
      QString const key = clusterKey (node, band, dial, call);
      ClusterLastHeardEntry incoming;
      incoming.call = call;
      incoming.locator = jsonString (
          object, QStringLiteral ("locator")).toUpper ().left (12);
      incoming.name = jsonString (object, QStringLiteral ("name")).left (48);
      incoming.profileName = jsonString (
          object, QStringLiteral ("profileName")).left (24);
      incoming.event = jsonString (
          object, QStringLiteral ("event")).left (48);
      incoming.source = jsonString (
          object, QStringLiteral ("source")).left (24);
      incoming.nodeId = node;
      incoming.band = band;
      incoming.dialFrequencyHz = std::max<qint64> (0, dial);
      incoming.cq = object.value (QStringLiteral ("cq")).toBool (false);
      incoming.cqType = jsonString (
          object, QStringLiteral ("cqType")).toUpper ().left (16);
      if (incoming.cqType.isEmpty ())
        {
          incoming.cqType = QStringLiteral ("CQ");
        }
      incoming.firstHeardMs = jsonU64 (
          object, QStringLiteral ("firstHeardMs"));
      incoming.lastHeardMs = jsonU64 (
          object, QStringLiteral ("lastHeardMs"));
      incoming.heardCount = std::max (
          1, jsonInt (object, QStringLiteral ("heardCount"), 1));
      if (incoming.lastHeardMs == 0u)
        {
          ++skipped;
          continue;
        }

      std::map<QString, ClusterLastHeardEntry>::iterator existing =
          m_clusterLastHeard.find (key);
      if (existing == m_clusterLastHeard.end ())
        {
          m_clusterLastHeard[key] = incoming;
          ++imported;
          continue;
        }

      ClusterLastHeardEntry& current = existing->second;
      bool const incomingNewer = incoming.lastHeardMs >= current.lastHeardMs;
      if (incomingNewer)
        {
          if (!incoming.locator.isEmpty ())
            {
              current.locator = incoming.locator;
            }
          if (!incoming.name.isEmpty ())
            {
              current.name = incoming.name;
            }
          if (!incoming.profileName.isEmpty ())
            {
              current.profileName = incoming.profileName;
            }
          current.event = incoming.event;
          current.source = incoming.source;
          current.cq = incoming.cq;
          current.cqType = incoming.cqType;
          current.lastHeardMs = incoming.lastHeardMs;
        }
      if (current.firstHeardMs == 0u
          || (incoming.firstHeardMs > 0u
              && incoming.firstHeardMs < current.firstHeardMs))
        {
          current.firstHeardMs = incoming.firstHeardMs;
        }
      current.heardCount = std::max (current.heardCount,
                                     incoming.heardCount);
      ++merged;
    }

  while (m_clusterLastHeard.size () > 300u)
    {
      std::map<QString, ClusterLastHeardEntry>::iterator oldest =
          m_clusterLastHeard.end ();
      for (std::map<QString, ClusterLastHeardEntry>::iterator it =
               m_clusterLastHeard.begin ();
           it != m_clusterLastHeard.end ();
           ++it)
        {
          if (oldest == m_clusterLastHeard.end ()
              || it->second.lastHeardMs < oldest->second.lastHeardMs)
            {
              oldest = it;
            }
        }
      if (oldest == m_clusterLastHeard.end ())
        {
          break;
        }
      m_clusterLastHeard.erase (oldest);
    }

  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("imported"), imported);
  result.insert (QStringLiteral ("merged"), merged);
  result.insert (QStringLiteral ("skipped"), skipped);
  result.insert (QStringLiteral ("total"),
                 QVariant::fromValue<qulonglong> (
                     static_cast<qulonglong> (m_clusterLastHeard.size ())));
  result.insert (QStringLiteral ("sha256"), sha256Hex (bytes));
  emit clusterLastHeardChanged ();
  clearLastError ();
  return result;
}

QVariantMap FT2LinkQmlAdapter::writeClusterShareFile (QString const& path) const
{
  QVariantMap result;
  result.insert (QStringLiteral ("ok"), false);

  QString resolved = path.trimmed ();
  if (resolved.isEmpty ())
    {
      QFileInfo const storeInfo {resolvedLocalStorePath ()};
      resolved = storeInfo.absoluteDir ().absoluteFilePath (
          QStringLiteral ("ft2link_cluster_share.json"));
    }
  resolved = QDir::cleanPath (QFileInfo (resolved).absoluteFilePath ());
  QFileInfo const info {resolved};
  QDir const dir {info.absolutePath ()};
  if (!dir.exists () && !QDir ().mkpath (info.absolutePath ()))
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("Cannot create cluster share directory: %1")
                         .arg (info.absolutePath ()));
      result.insert (QStringLiteral ("path"), resolved);
      return result;
    }

  QString const json = clusterExportJson ();
  QByteArray const bytes = json.toUtf8 ();
  QSaveFile file {resolved};
  if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate))
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("Cannot write cluster share file: %1")
                         .arg (file.errorString ()));
      result.insert (QStringLiteral ("path"), resolved);
      return result;
    }
  if (file.write (bytes) != bytes.size () || !file.commit ())
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("Cannot commit cluster share file: %1")
                         .arg (file.errorString ()));
      result.insert (QStringLiteral ("path"), resolved);
      return result;
    }

  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("path"), resolved);
  result.insert (QStringLiteral ("bytes"), bytes.size ());
  result.insert (QStringLiteral ("records"),
                 QVariant::fromValue<qulonglong> (
                     static_cast<qulonglong> (m_clusterLastHeard.size ())));
  result.insert (QStringLiteral ("sha256"), sha256Hex (bytes));
  return result;
}

QVariantMap FT2LinkQmlAdapter::mergeClusterShareFile (QString const& path,
                                                      quint64 nowMs)
{
  QVariantMap result;
  result.insert (QStringLiteral ("ok"), false);

  QString resolved = path.trimmed ();
  if (resolved.isEmpty ())
    {
      QFileInfo const storeInfo {resolvedLocalStorePath ()};
      resolved = storeInfo.absoluteDir ().absoluteFilePath (
          QStringLiteral ("ft2link_cluster_share.json"));
    }
  resolved = QDir::cleanPath (QFileInfo (resolved).absoluteFilePath ());
  QFile file {resolved};
  if (!file.exists ())
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("Cluster share file does not exist"));
      result.insert (QStringLiteral ("path"), resolved);
      return result;
    }
  if (file.size () > 2 * 1024 * 1024)
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("Cluster share file is too large"));
      result.insert (QStringLiteral ("path"), resolved);
      return result;
    }
  if (!file.open (QIODevice::ReadOnly))
    {
      result.insert (QStringLiteral ("error"),
                     QStringLiteral ("Cannot read cluster share file: %1")
                         .arg (file.errorString ()));
      result.insert (QStringLiteral ("path"), resolved);
      return result;
    }

  QByteArray const bytes = file.readAll ();
  result = importClusterLastHeard (QString::fromUtf8 (bytes), nowMs);
  result.insert (QStringLiteral ("path"), resolved);
  result.insert (QStringLiteral ("bytes"), bytes.size ());
  result.insert (QStringLiteral ("fileSha256"), sha256Hex (bytes));
  return result;
}

QVariantMap FT2LinkQmlAdapter::syncClusterShareFile (QString const& path,
                                                     quint64 nowMs)
{
  QVariantMap result;
  result.insert (QStringLiteral ("ok"), false);

  QVariantMap pull = mergeClusterShareFile (path, nowMs);
  bool const pullOk = pull.value (QStringLiteral ("ok")).toBool ();
  QString const pullError = pull.value (QStringLiteral ("error")).toString ();
  bool const missingFile =
      !pullOk
      && pullError.contains (QStringLiteral ("does not exist"),
                             Qt::CaseInsensitive);
  if (!pullOk && !missingFile)
    {
      result.insert (QStringLiteral ("error"), pullError);
      result.insert (QStringLiteral ("path"),
                     pull.value (QStringLiteral ("path")).toString ());
      result.insert (QStringLiteral ("pull"), pull);
      result.insert (QStringLiteral ("pullOk"), false);
      result.insert (QStringLiteral ("pushOk"), false);
      return result;
    }

  QString const resolvedPath =
      pull.value (QStringLiteral ("path")).toString ().trimmed ();
  QVariantMap push = writeClusterShareFile (
      resolvedPath.isEmpty () ? path : resolvedPath);
  bool const pushOk = push.value (QStringLiteral ("ok")).toBool ();

  result.insert (QStringLiteral ("ok"), pushOk);
  result.insert (QStringLiteral ("action"),
                 missingFile ? QStringLiteral ("push-new")
                             : QStringLiteral ("pull-push"));
  result.insert (QStringLiteral ("path"),
                 push.value (QStringLiteral ("path")).toString ());
  result.insert (QStringLiteral ("pull"), pull);
  result.insert (QStringLiteral ("push"), push);
  result.insert (QStringLiteral ("pullOk"), pullOk);
  result.insert (QStringLiteral ("pushOk"), pushOk);
  result.insert (QStringLiteral ("imported"),
                 pull.value (QStringLiteral ("imported")).toInt ());
  result.insert (QStringLiteral ("merged"),
                 pull.value (QStringLiteral ("merged")).toInt ());
  result.insert (QStringLiteral ("skipped"),
                 pull.value (QStringLiteral ("skipped")).toInt ());
  result.insert (QStringLiteral ("records"),
                 push.value (QStringLiteral ("records")).toULongLong ());
  result.insert (QStringLiteral ("sha256"),
                 push.value (QStringLiteral ("sha256")).toString ());
  if (!pushOk)
    {
      result.insert (QStringLiteral ("error"),
                     push.value (QStringLiteral ("error")).toString ());
    }
  return result;
}

QVariantMap FT2LinkQmlAdapter::pathAnalysis (QString const& call,
                                             QString const& locator) const
{
  QString const targetCall = normalizeCallsign (call);
  QString const targetLocator = locator.trimmed ().toUpper ();

  struct Aggregate
  {
    int count {0};
    int snrCount {0};
    int snrSum {0};
    int minSnr {std::numeric_limits<int>::max ()};
    int maxSnr {std::numeric_limits<int>::min ()};
  };

  auto addSnr = [] (Aggregate& aggregate, int snr) {
    ++aggregate.count;
    ++aggregate.snrCount;
    aggregate.snrSum += snr;
    aggregate.minSnr = std::min (aggregate.minSnr, snr);
    aggregate.maxSnr = std::max (aggregate.maxSnr, snr);
  };

  auto aggregateMap = [] (QString const& key, Aggregate const& aggregate) {
    QVariantMap map;
    map.insert (QStringLiteral ("key"), key);
    map.insert (QStringLiteral ("count"), aggregate.count);
    map.insert (QStringLiteral ("snrCount"), aggregate.snrCount);
    map.insert (QStringLiteral ("avgSnr"),
                aggregate.snrCount > 0
                ? static_cast<double> (aggregate.snrSum)
                    / static_cast<double> (aggregate.snrCount)
                : 0.0);
    map.insert (QStringLiteral ("minSnr"),
                aggregate.snrCount > 0 ? aggregate.minSnr : 0);
    map.insert (QStringLiteral ("maxSnr"),
                aggregate.snrCount > 0 ? aggregate.maxSnr : 0);
    return map;
  };

  Aggregate total;
  std::map<int, Aggregate> byHour;
  std::map<QString, Aggregate> byCall;
  std::map<QString, Aggregate> byLocator;
  QVariantList recent;
  for (std::vector<PathReport>::const_reverse_iterator it = m_pathReports.rbegin ();
       it != m_pathReports.rend ();
       ++it)
    {
      PathReport const& report = *it;
      if (!targetCall.isEmpty () && report.remoteCall != targetCall)
        {
          continue;
        }
      if (!targetLocator.isEmpty ()
          && !report.locator.startsWith (targetLocator))
        {
          continue;
        }

      ++total.count;
      if (report.snrValid)
        {
          ++total.snrCount;
          total.snrSum += report.snrDb;
          total.minSnr = std::min (total.minSnr, report.snrDb);
          total.maxSnr = std::max (total.maxSnr, report.snrDb);

          QDateTime const at = QDateTime::fromMSecsSinceEpoch (
              static_cast<qint64> (report.atMs), QTimeZone(QByteArrayLiteral("UTC"))).toUTC ();
          if (at.isValid ())
            {
              addSnr (byHour[at.time ().hour ()], report.snrDb);
            }
          addSnr (byCall[report.remoteCall], report.snrDb);
          if (!report.locator.isEmpty ())
            {
              addSnr (byLocator[report.locator.left (4)], report.snrDb);
            }
        }

      if (recent.size () < 20)
        {
          recent.push_back (pathReportMap (
              report.id,
              report.direction,
              report.remoteCall,
              report.locator,
              report.snrValid,
              report.snrDb,
              report.qualityValid,
              report.quality,
              report.frequencyOffsetHz,
              report.profileName,
              report.rateName,
              report.source,
              report.atMs));
        }
    }

  QVariantList hourly;
  int bestHour = -1;
  double bestHourAvg = -1000.0;
  int bestHourCount = 0;
  for (std::map<int, Aggregate>::const_iterator it = byHour.begin ();
       it != byHour.end ();
       ++it)
    {
      QVariantMap item = aggregateMap (
          QStringLiteral ("%1Z").arg (it->first, 2, 10, QLatin1Char ('0')),
          it->second);
      item.insert (QStringLiteral ("hourUtc"), it->first);
      double const average = item.value (QStringLiteral ("avgSnr")).toDouble ();
      if (it->second.snrCount > 0 && average > bestHourAvg)
        {
          bestHourAvg = average;
          bestHour = it->first;
          bestHourCount = it->second.snrCount;
        }
      hourly.push_back (item);
    }

  QVariantList calls;
  for (std::map<QString, Aggregate>::const_iterator it = byCall.begin ();
       it != byCall.end ();
       ++it)
    {
      calls.push_back (aggregateMap (it->first, it->second));
    }
  std::sort (calls.begin (), calls.end (), [] (QVariant const& lhs,
                                               QVariant const& rhs) {
    QVariantMap const lhsMap = lhs.toMap ();
    QVariantMap const rhsMap = rhs.toMap ();
    double const lhsAvg = lhsMap.value (QStringLiteral ("avgSnr")).toDouble ();
    double const rhsAvg = rhsMap.value (QStringLiteral ("avgSnr")).toDouble ();
    if (lhsAvg == rhsAvg)
      {
        return lhsMap.value (QStringLiteral ("snrCount")).toInt ()
            > rhsMap.value (QStringLiteral ("snrCount")).toInt ();
      }
    return lhsAvg > rhsAvg;
  });

  QVariantList locators;
  for (std::map<QString, Aggregate>::const_iterator it = byLocator.begin ();
       it != byLocator.end ();
       ++it)
    {
      locators.push_back (aggregateMap (it->first, it->second));
    }
  std::sort (locators.begin (), locators.end (), [] (QVariant const& lhs,
                                                     QVariant const& rhs) {
    return lhs.toMap ().value (QStringLiteral ("avgSnr")).toDouble ()
        > rhs.toMap ().value (QStringLiteral ("avgSnr")).toDouble ();
  });

  QVariantMap map = aggregateMap (QStringLiteral ("TOTAL"), total);
  map.insert (QStringLiteral ("filterCall"), targetCall);
  map.insert (QStringLiteral ("filterLocator"), targetLocator);
  map.insert (QStringLiteral ("bestHourUtc"), bestHour);
  map.insert (QStringLiteral ("bestHourAvgSnr"),
              bestHour >= 0 ? bestHourAvg : 0.0);
  map.insert (QStringLiteral ("bestHourCount"), bestHourCount);
  map.insert (QStringLiteral ("hourly"), hourly);
  map.insert (QStringLiteral ("calls"), calls);
  map.insert (QStringLiteral ("locators"), locators);
  map.insert (QStringLiteral ("recentReports"), recent);
  map.insert (QStringLiteral ("bandTracked"), false);
  return map;
}

QVariantMap FT2LinkQmlAdapter::statistics () const
{
  QVariantMap map;
  auto insertU64 = [&map] (QString const& key, quint64 value) {
    map.insert (key, QVariant::fromValue<qulonglong> (value));
  };

  QStringList qsoCalls;
  quint64 qsoMessages = 0u;
  quint64 longestQsoMessages = 0u;
  quint64 longestQsoMinutes = 0u;
  quint64 lastActivityMs = 0u;
  for (std::map<quint16, QsoLogEntry>::const_iterator it = m_qsoLog.begin ();
       it != m_qsoLog.end ();
       ++it)
    {
      QsoLogEntry const& entry = it->second;
      QString const call = normalizeCallsign (entry.remoteCall);
      if (!call.isEmpty () && !qsoCalls.contains (call))
        {
          qsoCalls.push_back (call);
        }
      qsoMessages += static_cast<quint64> (std::max (0, entry.messageCount));
      longestQsoMessages = std::max<quint64> (
          longestQsoMessages,
          static_cast<quint64> (std::max (0, entry.messageCount)));
      quint64 const endMs = entry.closedAtMs > 0u
          ? entry.closedAtMs
          : (entry.updatedAtMs > 0u ? entry.updatedAtMs : entry.openedAtMs);
      if (entry.openedAtMs > 0u && endMs >= entry.openedAtMs)
        {
          quint64 const minutes =
              (endMs - entry.openedAtMs + 59999u) / 60000u;
          longestQsoMinutes = std::max (longestQsoMinutes, minutes);
        }
      lastActivityMs = std::max (lastActivityMs, endMs);
    }

  quint64 logbookQueued = 0u;
  quint64 logbookSubmitted = 0u;
  quint64 logbookSent = 0u;
  quint64 logbookFailed = 0u;
  quint64 logbookSkipped = 0u;
  for (LogbookUpload const& upload : m_logbookOutbox)
    {
      if (upload.state == QStringLiteral ("Queued"))
        {
          ++logbookQueued;
        }
      else if (upload.state == QStringLiteral ("Submitted"))
        {
          ++logbookSubmitted;
        }
      else if (upload.state == QStringLiteral ("Sent"))
        {
          ++logbookSent;
        }
      else if (upload.state == QStringLiteral ("Failed"))
        {
          ++logbookFailed;
        }
      else if (upload.state == QStringLiteral ("Skipped"))
        {
          ++logbookSkipped;
        }
      lastActivityMs = std::max (
          lastActivityMs,
          upload.updatedAtMs > 0u ? upload.updatedAtMs : upload.queuedAtMs);
    }

  quint64 liveChatSent = 0u;
  quint64 liveChatReceived = 0u;
  quint64 liveChatSystem = 0u;
  std::vector<AppSession> const sessions = m_model.sessions ();
  for (AppSession const& session : sessions)
    {
      lastActivityMs = std::max (
          lastActivityMs, static_cast<quint64> (session.openedAtMs));
      for (ChatMessage const& message : session.messages)
        {
          lastActivityMs = std::max (
              lastActivityMs, static_cast<quint64> (message.atMs));
          switch (message.direction)
            {
            case ChatMessageDirection::Outgoing: ++liveChatSent; break;
            case ChatMessageDirection::Incoming: ++liveChatReceived; break;
            case ChatMessageDirection::System: ++liveChatSystem; break;
            }
        }
    }

  quint64 broadcastsSent = 0u;
  quint64 broadcastsReceived = 0u;
  for (BroadcastMessage const& message : m_broadcasts)
    {
      if (message.source == QStringLiteral ("TX"))
        {
          ++broadcastsSent;
        }
      else
        {
          ++broadcastsReceived;
        }
      lastActivityMs = std::max (lastActivityMs, message.atMs);
    }

  quint64 mailboxIncoming = 0u;
  quint64 mailboxOutgoing = 0u;
  quint64 mailboxRelay = 0u;
  for (MailboxMessage const& message : m_mailbox)
    {
      if (message.direction == QStringLiteral ("Incoming"))
        {
          ++mailboxIncoming;
        }
      else if (message.direction == QStringLiteral ("Outgoing"))
        {
          ++mailboxOutgoing;
        }
      else
        {
          ++mailboxRelay;
        }
      lastActivityMs = std::max (
          lastActivityMs,
          message.updatedAtMs > 0u ? message.updatedAtMs : message.atMs);
    }

  quint64 formsIncoming = 0u;
  quint64 formsOutgoing = 0u;
  for (FormMessage const& form : m_forms)
    {
      if (form.direction == QStringLiteral ("Incoming"))
        {
          ++formsIncoming;
        }
      else
        {
          ++formsOutgoing;
        }
      lastActivityMs = std::max (
          lastActivityMs,
          form.updatedAtMs > 0u ? form.updatedAtMs : form.atMs);
    }

  quint64 filesIncoming = 0u;
  quint64 filesOutgoing = 0u;
  quint64 receivedFileBytes = 0u;
  for (FileTransfer const& transfer : m_fileTransfers)
    {
      if (transfer.direction == QStringLiteral ("Incoming"))
        {
          ++filesIncoming;
          receivedFileBytes += static_cast<quint64> (
              transfer.content.toUtf8 ().size ());
        }
      else
        {
          ++filesOutgoing;
        }
      lastActivityMs = std::max (
          lastActivityMs,
          transfer.updatedAtMs > 0u ? transfer.updatedAtMs : transfer.atMs);
    }

  quint64 bulletinsIncoming = 0u;
  quint64 bulletinsOutgoing = 0u;
  for (Bulletin const& bulletin : m_bulletins)
    {
      if (bulletin.direction == QStringLiteral ("Incoming"))
        {
          ++bulletinsIncoming;
        }
      else
        {
          ++bulletinsOutgoing;
        }
      lastActivityMs = std::max (
          lastActivityMs,
          bulletin.updatedAtMs > 0u ? bulletin.updatedAtMs : bulletin.atMs);
    }

  quint64 pingsSent = 0u;
  quint64 pingsReceived = 0u;
  quint64 pingReplies = 0u;
  for (PingRecord const& ping : m_pingLog)
    {
      if (ping.direction == QStringLiteral ("Outgoing"))
        {
          ++pingsSent;
        }
      else
        {
          ++pingsReceived;
        }
      if (ping.state == QStringLiteral ("Reply"))
        {
          ++pingReplies;
        }
      lastActivityMs = std::max (lastActivityMs, ping.atMs);
    }

  quint64 snrsSent = 0u;
  quint64 snrsReceived = 0u;
  int snrSentSum = 0;
  int snrReceivedSum = 0;
  int snrSentMin = std::numeric_limits<int>::max ();
  int snrSentMax = std::numeric_limits<int>::min ();
  int snrReceivedMin = std::numeric_limits<int>::max ();
  int snrReceivedMax = std::numeric_limits<int>::min ();
  quint64 pathQualityReports = 0u;
  for (PathReport const& report : m_pathReports)
    {
      if (report.snrValid)
        {
          if (report.direction == QStringLiteral ("Outgoing"))
            {
              ++snrsSent;
              snrSentSum += report.snrDb;
              snrSentMin = std::min (snrSentMin, report.snrDb);
              snrSentMax = std::max (snrSentMax, report.snrDb);
            }
          else
            {
              ++snrsReceived;
              snrReceivedSum += report.snrDb;
              snrReceivedMin = std::min (snrReceivedMin, report.snrDb);
              snrReceivedMax = std::max (snrReceivedMax, report.snrDb);
            }
        }
      if (report.qualityValid)
        {
          ++pathQualityReports;
        }
      lastActivityMs = std::max (lastActivityMs, report.atMs);
    }
  for (BeaconHistoryEntry const& entry : m_beaconHistory)
    {
      lastActivityMs = std::max (lastActivityMs, entry.atMs);
    }
  for (std::map<QString, ClusterLastHeardEntry>::const_iterator it =
           m_clusterLastHeard.begin ();
       it != m_clusterLastHeard.end ();
       ++it)
    {
      lastActivityMs = std::max (lastActivityMs, it->second.lastHeardMs);
    }

  for (AlertEvent const& alert : m_alerts)
    {
      lastActivityMs = std::max (lastActivityMs, alert.atMs);
    }
  for (std::map<QString, ContactHistory>::const_iterator it =
           m_contactHistory.begin ();
       it != m_contactHistory.end ();
       ++it)
    {
      lastActivityMs = std::max (lastActivityMs, it->second.lastHeardMs);
    }

  insertU64 (QStringLiteral ("qsoTotal"),
             static_cast<quint64> (m_qsoLog.size ()));
  insertU64 (QStringLiteral ("qsoDistinctCallsigns"),
             static_cast<quint64> (qsoCalls.size ()));
  insertU64 (QStringLiteral ("logbookOutboxTotal"),
             static_cast<quint64> (m_logbookOutbox.size ()));
  insertU64 (QStringLiteral ("logbookQueued"), logbookQueued);
  insertU64 (QStringLiteral ("logbookSubmitted"), logbookSubmitted);
  insertU64 (QStringLiteral ("logbookSent"), logbookSent);
  insertU64 (QStringLiteral ("logbookFailed"), logbookFailed);
  insertU64 (QStringLiteral ("logbookSkipped"), logbookSkipped);
  insertU64 (QStringLiteral ("contactCount"),
             static_cast<quint64> (m_contactHistory.size ()));
  insertU64 (QStringLiteral ("longestQsoMinutes"), longestQsoMinutes);
  insertU64 (QStringLiteral ("longestQsoMessages"), longestQsoMessages);
  insertU64 (QStringLiteral ("chatMessagesLogged"), qsoMessages);
  insertU64 (QStringLiteral ("chatMessagesSent"), liveChatSent);
  insertU64 (QStringLiteral ("chatMessagesReceived"), liveChatReceived);
  insertU64 (QStringLiteral ("chatMessagesSystem"), liveChatSystem);
  insertU64 (QStringLiteral ("beaconsSent"), m_beaconsSent);
  insertU64 (QStringLiteral ("beaconsReceived"), m_beaconsReceived);
  insertU64 (QStringLiteral ("cqsSent"), m_cqsSent);
  insertU64 (QStringLiteral ("cqsReceived"), m_cqsReceived);
  insertU64 (QStringLiteral ("beaconHistoryTotal"),
             static_cast<quint64> (m_beaconHistory.size ()));
  insertU64 (QStringLiteral ("clusterLastHeardTotal"),
             static_cast<quint64> (m_clusterLastHeard.size ()));
  insertU64 (QStringLiteral ("broadcastsSent"), broadcastsSent);
  insertU64 (QStringLiteral ("broadcastsReceived"), broadcastsReceived);
  insertU64 (QStringLiteral ("alertsTotal"),
             static_cast<quint64> (m_alerts.size ()));
  insertU64 (QStringLiteral ("mailboxIncoming"), mailboxIncoming);
  insertU64 (QStringLiteral ("mailboxOutgoing"), mailboxOutgoing);
  insertU64 (QStringLiteral ("mailboxRelay"), mailboxRelay);
  insertU64 (QStringLiteral ("relayQueueCount"),
             static_cast<quint64> (relayQueueCount ()));
  insertU64 (QStringLiteral ("formsIncoming"), formsIncoming);
  insertU64 (QStringLiteral ("formsOutgoing"), formsOutgoing);
  insertU64 (QStringLiteral ("fileTransfersTotal"),
             static_cast<quint64> (m_fileTransfers.size ()));
  insertU64 (QStringLiteral ("filesReceived"), filesIncoming);
  insertU64 (QStringLiteral ("filesSent"), filesOutgoing);
  insertU64 (QStringLiteral ("receivedFileBytes"), receivedFileBytes);
  insertU64 (QStringLiteral ("bulletinsIncoming"), bulletinsIncoming);
  insertU64 (QStringLiteral ("bulletinsOutgoing"), bulletinsOutgoing);
  insertU64 (QStringLiteral ("pingsSent"), pingsSent);
  insertU64 (QStringLiteral ("pingsReceived"), pingsReceived);
  insertU64 (QStringLiteral ("pingReplies"), pingReplies);
  insertU64 (QStringLiteral ("pathReportsTotal"),
             static_cast<quint64> (m_pathReports.size ()));
  insertU64 (QStringLiteral ("customCannedMessages"),
             static_cast<quint64> (m_customCannedMessages.size ()));
  insertU64 (QStringLiteral ("customAlertTags"),
             static_cast<quint64> (m_customAlertTags.size ()));
  insertU64 (QStringLiteral ("blockedCalls"),
             static_cast<quint64> (m_blockedCalls.size ()));
  insertU64 (QStringLiteral ("frequencyPresets"),
             static_cast<quint64> (m_frequencyPresets.size ()));
  insertU64 (QStringLiteral ("allowedQsyRanges"),
             static_cast<quint64> (m_allowedQsyRanges.size ()));
  insertU64 (QStringLiteral ("frequencySchedule"),
             static_cast<quint64> (m_frequencySchedule.size ()));
  insertU64 (QStringLiteral ("presenceSettings"),
             m_awayEnabled || m_welcomeEnabled || m_autoReplyEnabled
             || m_autoAwayEnabled ? 1u : 0u);
  insertU64 (QStringLiteral ("qsoAutomationSettings"),
             m_callIdIntervalMinutes > 0 || m_autoDisconnectMinutes > 0
             || !m_incomingPingsEnabled || !m_lastHeardPeekingEnabled
             || !m_lastConnectionsPeekingEnabled
             || !m_parkedVmailPeekingEnabled || !m_vmailParkingEnabled
             || !m_snrReportSendingEnabled || m_verboseSnrAutoAcceptEnabled
             || !m_infoInquireEnabled ? 1u : 0u);
  map.insert (QStringLiteral ("awayEnabled"), m_awayEnabled);
  map.insert (QStringLiteral ("awayAcceptsQsy"), m_awayAcceptsQsy);
  map.insert (QStringLiteral ("welcomeEnabled"), m_welcomeEnabled);
  map.insert (QStringLiteral ("autoReplyEnabled"), m_autoReplyEnabled);
  map.insert (QStringLiteral ("autoAwayEnabled"), m_autoAwayEnabled);
  map.insert (QStringLiteral ("autoAwayMinutes"), m_autoAwayMinutes);
  map.insert (QStringLiteral ("autoAwayActive"), m_autoAwayActivated);
  map.insert (QStringLiteral ("callIdIntervalMinutes"),
              m_callIdIntervalMinutes);
  map.insert (QStringLiteral ("autoDisconnectMinutes"),
              m_autoDisconnectMinutes);
  map.insert (QStringLiteral ("incomingPingsEnabled"),
              m_incomingPingsEnabled);
  map.insert (QStringLiteral ("lastHeardPeekingEnabled"),
              m_lastHeardPeekingEnabled);
  map.insert (QStringLiteral ("lastConnectionsPeekingEnabled"),
              m_lastConnectionsPeekingEnabled);
  map.insert (QStringLiteral ("parkedVmailPeekingEnabled"),
              m_parkedVmailPeekingEnabled);
  map.insert (QStringLiteral ("vmailParkingEnabled"),
              m_vmailParkingEnabled);
  map.insert (QStringLiteral ("snrReportSendingEnabled"),
              m_snrReportSendingEnabled);
  map.insert (QStringLiteral ("verboseSnrAutoAcceptEnabled"),
              m_verboseSnrAutoAcceptEnabled);
  map.insert (QStringLiteral ("infoInquireEnabled"),
              m_infoInquireEnabled);
  map.insert (QStringLiteral ("clusterEnabled"), m_clusterEnabled);
  map.insert (QStringLiteral ("clusterNodeId"), effectiveClusterNodeId ());
  map.insert (QStringLiteral ("clusterBand"),
              clusterBandLabel (m_clusterDialFrequencyHz));
  map.insert (QStringLiteral ("clusterDialFrequencyHz"),
              QVariant::fromValue<qlonglong> (m_clusterDialFrequencyHz));
  insertU64 (QStringLiteral ("pathQualityReports"), pathQualityReports);
  insertU64 (QStringLiteral ("snrsSent"), snrsSent);
  insertU64 (QStringLiteral ("snrsReceived"), snrsReceived);
  map.insert (QStringLiteral ("snrSentAvg"),
              snrsSent > 0u ? static_cast<double> (snrSentSum)
                  / static_cast<double> (snrsSent) : 0.0);
  map.insert (QStringLiteral ("snrReceivedAvg"),
              snrsReceived > 0u ? static_cast<double> (snrReceivedSum)
                  / static_cast<double> (snrsReceived) : 0.0);
  map.insert (QStringLiteral ("snrSentMin"),
              snrsSent > 0u ? snrSentMin : 0);
  map.insert (QStringLiteral ("snrSentMax"),
              snrsSent > 0u ? snrSentMax : 0);
  map.insert (QStringLiteral ("snrReceivedMin"),
              snrsReceived > 0u ? snrReceivedMin : 0);
  map.insert (QStringLiteral ("snrReceivedMax"),
              snrsReceived > 0u ? snrReceivedMax : 0);
  insertU64 (QStringLiteral ("lastActivityMs"), lastActivityMs);
  insertU64 (QStringLiteral ("storeRecordsTotal"),
             static_cast<quint64> (
                 m_broadcasts.size () + m_alerts.size () + m_mailbox.size ()
                 + m_forms.size () + m_fileTransfers.size ()
                 + m_bulletins.size () + m_qsoLog.size ()
                 + m_logbookOutbox.size ()
                 + m_pingLog.size () + m_pathReports.size ()
                 + m_beaconHistory.size ()
                 + m_clusterLastHeard.size ()
                 + m_contactHistory.size ()
                 + m_customCannedMessages.size ()
                 + m_customAlertTags.size ()
                 + m_blockedCalls.size ()
                 + m_frequencyPresets.size ()
                 + m_allowedQsyRanges.size ()
                 + m_frequencySchedule.size ()
                 + (m_awayEnabled || m_welcomeEnabled || m_autoReplyEnabled
                    || m_autoAwayEnabled || m_callIdIntervalMinutes > 0
                    || m_autoDisconnectMinutes > 0 || !m_incomingPingsEnabled
                    || !m_lastHeardPeekingEnabled
                    || !m_lastConnectionsPeekingEnabled
                    || !m_parkedVmailPeekingEnabled || !m_vmailParkingEnabled
                    || !m_snrReportSendingEnabled
                    || m_verboseSnrAutoAcceptEnabled || !m_infoInquireEnabled
                    ? 1u : 0u)));
  map.insert (QStringLiteral ("lastActivityUtc"), utcMinuteText (lastActivityMs));
  map.insert (QStringLiteral ("snrTracked"),
              snrsSent > 0u || snrsReceived > 0u);
  return map;
}

QString FT2LinkQmlAdapter::statisticsText () const
{
  QVariantMap const stats = statistics ();
  auto u64 = [&stats] (char const* key) {
    return stats.value (QString::fromLatin1 (key)).toULongLong ();
  };

  QString text = QStringLiteral ("FT2-Link statistics\n");
  text += QStringLiteral ("QSO total: %1\n").arg (u64 ("qsoTotal"));
  text += QStringLiteral ("QSO distinct callsigns: %1\n").arg (
      u64 ("qsoDistinctCallsigns"));
  text += QStringLiteral ("Logbook outbox: %1 queued %2 submitted %3 sent %4 failed %5 skipped %6\n")
      .arg (u64 ("logbookOutboxTotal"))
      .arg (u64 ("logbookQueued"))
      .arg (u64 ("logbookSubmitted"))
      .arg (u64 ("logbookSent"))
      .arg (u64 ("logbookFailed"))
      .arg (u64 ("logbookSkipped"));
  text += QStringLiteral ("Contacts: %1\n").arg (u64 ("contactCount"));
  text += QStringLiteral ("Longest QSO minutes: %1\n").arg (
      u64 ("longestQsoMinutes"));
  text += QStringLiteral ("Longest QSO messages: %1\n").arg (
      u64 ("longestQsoMessages"));
  text += QStringLiteral ("Beacons sent: %1\n").arg (u64 ("beaconsSent"));
  text += QStringLiteral ("Beacons received: %1\n").arg (
      u64 ("beaconsReceived"));
  text += QStringLiteral ("CQs sent: %1\n").arg (u64 ("cqsSent"));
  text += QStringLiteral ("CQs received: %1\n").arg (u64 ("cqsReceived"));
  text += QStringLiteral ("Chat messages logged: %1\n").arg (
      u64 ("chatMessagesLogged"));
  text += QStringLiteral ("Live chat sent: %1\n").arg (
      u64 ("chatMessagesSent"));
  text += QStringLiteral ("Live chat received: %1\n").arg (
      u64 ("chatMessagesReceived"));
  text += QStringLiteral ("Pings sent: %1\n").arg (u64 ("pingsSent"));
  text += QStringLiteral ("Pings received: %1\n").arg (
      u64 ("pingsReceived"));
  text += QStringLiteral ("Path reports: %1\n").arg (
      u64 ("pathReportsTotal"));
  text += QStringLiteral ("CQ/beacon history: %1\n").arg (
      u64 ("beaconHistoryTotal"));
  text += QStringLiteral ("Cluster last heard: %1\n").arg (
      u64 ("clusterLastHeardTotal"));
  text += QStringLiteral ("Cluster node/band: %1 / %2 / %3 Hz / %4\n")
      .arg (stats.value (QStringLiteral ("clusterNodeId")).toString (),
            stats.value (QStringLiteral ("clusterBand")).toString (),
            QString::number (stats.value (
                QStringLiteral ("clusterDialFrequencyHz")).toLongLong ()),
            stats.value (QStringLiteral ("clusterEnabled")).toBool ()
            ? QStringLiteral ("on")
            : QStringLiteral ("off"));
  text += QStringLiteral ("Custom canned messages: %1\n").arg (
      u64 ("customCannedMessages"));
  text += QStringLiteral ("Custom alert tags: %1\n").arg (
      u64 ("customAlertTags"));
  text += QStringLiteral ("Blocked calls: %1\n").arg (
      u64 ("blockedCalls"));
  text += QStringLiteral ("Frequency presets: %1\n").arg (
      u64 ("frequencyPresets"));
  text += QStringLiteral ("Allowed QSY ranges: %1\n").arg (
      u64 ("allowedQsyRanges"));
  text += QStringLiteral ("Frequency schedule windows: %1\n").arg (
      u64 ("frequencySchedule"));
  text += QStringLiteral ("Presence: away %1 / welcome %2\n")
      .arg (stats.value (QStringLiteral ("awayEnabled")).toBool ()
            ? QStringLiteral ("on") : QStringLiteral ("off"),
            stats.value (QStringLiteral ("welcomeEnabled")).toBool ()
            ? QStringLiteral ("on") : QStringLiteral ("off"));
  text += QStringLiteral ("Auto reply queue: %1\n").arg (
      stats.value (QStringLiteral ("autoReplyEnabled")).toBool ()
      ? QStringLiteral ("on") : QStringLiteral ("off"));
  text += QStringLiteral ("Auto away: %1 / %2 min / active %3\n")
      .arg (stats.value (QStringLiteral ("autoAwayEnabled")).toBool ()
            ? QStringLiteral ("on") : QStringLiteral ("off"))
      .arg (stats.value (QStringLiteral ("autoAwayMinutes")).toInt ())
      .arg (stats.value (QStringLiteral ("autoAwayActive")).toBool ()
            ? QStringLiteral ("yes") : QStringLiteral ("no"));
  text += QStringLiteral ("Call ID interval: %1 min\n").arg (
      stats.value (QStringLiteral ("callIdIntervalMinutes")).toInt ());
  text += QStringLiteral ("Auto disconnect: %1 min\n").arg (
      stats.value (QStringLiteral ("autoDisconnectMinutes")).toInt ());
  text += QStringLiteral ("Incoming pings: %1\n").arg (
      stats.value (QStringLiteral ("incomingPingsEnabled")).toBool ()
      ? QStringLiteral ("on") : QStringLiteral ("off"));
  text += QStringLiteral ("Last-heard peeking: %1\n").arg (
      stats.value (QStringLiteral ("lastHeardPeekingEnabled")).toBool ()
      ? QStringLiteral ("on") : QStringLiteral ("off"));
  text += QStringLiteral ("Last-connections peeking: %1\n").arg (
      stats.value (QStringLiteral ("lastConnectionsPeekingEnabled")).toBool ()
      ? QStringLiteral ("on") : QStringLiteral ("off"));
  text += QStringLiteral ("Parked VMail peeking: %1\n").arg (
      stats.value (QStringLiteral ("parkedVmailPeekingEnabled")).toBool ()
      ? QStringLiteral ("on") : QStringLiteral ("off"));
  text += QStringLiteral ("VMail parking: %1\n").arg (
      stats.value (QStringLiteral ("vmailParkingEnabled")).toBool ()
      ? QStringLiteral ("on") : QStringLiteral ("off"));
  text += QStringLiteral ("SNR report suggestions: %1\n").arg (
      stats.value (QStringLiteral ("snrReportSendingEnabled")).toBool ()
      ? QStringLiteral ("on") : QStringLiteral ("off"));
  text += QStringLiteral ("Verbose SNR auto-accept: %1\n").arg (
      stats.value (QStringLiteral ("verboseSnrAutoAcceptEnabled")).toBool ()
      ? QStringLiteral ("on") : QStringLiteral ("off"));
  text += QStringLiteral ("Info inquiries: %1\n").arg (
      stats.value (QStringLiteral ("infoInquireEnabled")).toBool ()
      ? QStringLiteral ("on") : QStringLiteral ("off"));
  text += QStringLiteral ("Broadcasts sent: %1\n").arg (
      u64 ("broadcastsSent"));
  text += QStringLiteral ("Broadcasts received: %1\n").arg (
      u64 ("broadcastsReceived"));
  text += QStringLiteral ("Mail incoming/outgoing/relay: %1/%2/%3\n")
      .arg (u64 ("mailboxIncoming"))
      .arg (u64 ("mailboxOutgoing"))
      .arg (u64 ("mailboxRelay"));
  text += QStringLiteral ("Relay queue pending: %1\n").arg (
      u64 ("relayQueueCount"));
  text += QStringLiteral ("Forms incoming/outgoing: %1/%2\n")
      .arg (u64 ("formsIncoming"))
      .arg (u64 ("formsOutgoing"));
  text += QStringLiteral ("Received files: %1\n").arg (
      u64 ("filesReceived"));
  text += QStringLiteral ("Received file bytes: %1\n").arg (
      u64 ("receivedFileBytes"));
  text += QStringLiteral ("Bulletins incoming/outgoing: %1/%2\n")
      .arg (u64 ("bulletinsIncoming"))
      .arg (u64 ("bulletinsOutgoing"));
  text += QStringLiteral ("Alerts: %1\n").arg (u64 ("alertsTotal"));
  text += QStringLiteral ("Last activity UTC: %1\n").arg (
      stats.value (QStringLiteral ("lastActivityUtc")).toString ());
  if (stats.value (QStringLiteral ("snrTracked")).toBool ())
    {
      text += QStringLiteral ("SNRs sent max/avg/min: %1/%2/%3\n")
          .arg (stats.value (QStringLiteral ("snrSentMax")).toInt ())
          .arg (stats.value (QStringLiteral ("snrSentAvg")).toDouble (),
                0, 'f', 1)
          .arg (stats.value (QStringLiteral ("snrSentMin")).toInt ());
      text += QStringLiteral ("SNRs received max/avg/min: %1/%2/%3\n")
          .arg (stats.value (QStringLiteral ("snrReceivedMax")).toInt ())
          .arg (stats.value (QStringLiteral ("snrReceivedAvg")).toDouble (),
                0, 'f', 1)
          .arg (stats.value (QStringLiteral ("snrReceivedMin")).toInt ());
    }
  else
    {
      text += QStringLiteral ("SNR history: not tracked yet\n");
    }
  return text;
}

QVariantMap FT2LinkQmlAdapter::localStoreAudit () const
{
  QVariantMap map;
  QStringList warnings;
  QStringList errors;

  QString const resolved = resolvedLocalStorePath ();
  QString const adifPath = resolvedAdifLogPath ();
  QByteArray const serialized = serializeLocalStore ();
  QByteArray const adifBytes = adifLog ().toUtf8 ();
  QVariantMap const stats = statistics ();

  map.insert (QStringLiteral ("schemaVersion"), kLocalStoreVersion);
  map.insert (QStringLiteral ("storePath"), resolved);
  map.insert (QStringLiteral ("adifPath"), adifPath);
  map.insert (QStringLiteral ("localStoreLoaded"), m_localStoreLoaded);
  map.insert (QStringLiteral ("lastLocalStoreError"), m_lastLocalStoreError);
  map.insert (QStringLiteral ("serializedBytes"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (serialized.size ())));
  map.insert (QStringLiteral ("serializedSha256"), sha256Hex (serialized));
  map.insert (QStringLiteral ("adifBytes"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (adifBytes.size ())));
  map.insert (QStringLiteral ("adifSha256"), sha256Hex (adifBytes));
  map.insert (QStringLiteral ("adifFileExists"), QFileInfo::exists (adifPath));
  map.insert (QStringLiteral ("recordCount"),
              stats.value (QStringLiteral ("storeRecordsTotal")));
  map.insert (QStringLiteral ("qsoCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_qsoLog.size ())));
  map.insert (QStringLiteral ("logbookOutboxCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_logbookOutbox.size ())));
  map.insert (QStringLiteral ("logbookQueued"),
              stats.value (QStringLiteral ("logbookQueued")));
  map.insert (QStringLiteral ("logbookSubmitted"),
              stats.value (QStringLiteral ("logbookSubmitted")));
  map.insert (QStringLiteral ("logbookSent"),
              stats.value (QStringLiteral ("logbookSent")));
  map.insert (QStringLiteral ("logbookFailed"),
              stats.value (QStringLiteral ("logbookFailed")));
  map.insert (QStringLiteral ("contactCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_contactHistory.size ())));
  map.insert (QStringLiteral ("mailboxCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_mailbox.size ())));
  map.insert (QStringLiteral ("formCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_forms.size ())));
  map.insert (QStringLiteral ("fileTransferCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_fileTransfers.size ())));
  map.insert (QStringLiteral ("bulletinCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_bulletins.size ())));
  map.insert (QStringLiteral ("pingCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_pingLog.size ())));
  map.insert (QStringLiteral ("pathReportCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_pathReports.size ())));
  map.insert (QStringLiteral ("beaconHistoryCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_beaconHistory.size ())));
  map.insert (QStringLiteral ("clusterLastHeardCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_clusterLastHeard.size ())));
  map.insert (QStringLiteral ("broadcastCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_broadcasts.size ())));
  map.insert (QStringLiteral ("alertCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_alerts.size ())));
  map.insert (QStringLiteral ("customCannedCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_customCannedMessages.size ())));
  map.insert (QStringLiteral ("customAlertTagCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_customAlertTags.size ())));
  map.insert (QStringLiteral ("blockedCallCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_blockedCalls.size ())));
  map.insert (QStringLiteral ("frequencyPresetCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_frequencyPresets.size ())));
  map.insert (QStringLiteral ("allowedQsyRangeCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_allowedQsyRanges.size ())));
  map.insert (QStringLiteral ("frequencyScheduleCount"),
              QVariant::fromValue<qulonglong> (
                  static_cast<qulonglong> (m_frequencySchedule.size ())));
  map.insert (QStringLiteral ("awayEnabled"), m_awayEnabled);
  map.insert (QStringLiteral ("awayAcceptsQsy"), m_awayAcceptsQsy);
  map.insert (QStringLiteral ("welcomeEnabled"), m_welcomeEnabled);
  map.insert (QStringLiteral ("autoReplyEnabled"), m_autoReplyEnabled);
  map.insert (QStringLiteral ("autoAwayEnabled"), m_autoAwayEnabled);
  map.insert (QStringLiteral ("autoAwayMinutes"), m_autoAwayMinutes);
  map.insert (QStringLiteral ("autoAwayActive"), m_autoAwayActivated);
  map.insert (QStringLiteral ("callIdIntervalMinutes"),
              m_callIdIntervalMinutes);
  map.insert (QStringLiteral ("autoDisconnectMinutes"),
              m_autoDisconnectMinutes);
  map.insert (QStringLiteral ("incomingPingsEnabled"),
              m_incomingPingsEnabled);
  map.insert (QStringLiteral ("lastHeardPeekingEnabled"),
              m_lastHeardPeekingEnabled);
  map.insert (QStringLiteral ("lastConnectionsPeekingEnabled"),
              m_lastConnectionsPeekingEnabled);
  map.insert (QStringLiteral ("parkedVmailPeekingEnabled"),
              m_parkedVmailPeekingEnabled);
  map.insert (QStringLiteral ("vmailParkingEnabled"),
              m_vmailParkingEnabled);
  map.insert (QStringLiteral ("snrReportSendingEnabled"),
              m_snrReportSendingEnabled);
  map.insert (QStringLiteral ("verboseSnrAutoAcceptEnabled"),
              m_verboseSnrAutoAcceptEnabled);
  map.insert (QStringLiteral ("infoInquireEnabled"),
              m_infoInquireEnabled);
  map.insert (QStringLiteral ("clusterEnabled"), m_clusterEnabled);
  map.insert (QStringLiteral ("clusterNodeId"), effectiveClusterNodeId ());
  map.insert (QStringLiteral ("clusterBand"),
              clusterBandLabel (m_clusterDialFrequencyHz));
  map.insert (QStringLiteral ("clusterDialFrequencyHz"),
              QVariant::fromValue<qlonglong> (m_clusterDialFrequencyHz));

  QFileInfo const info {resolved};
  map.insert (QStringLiteral ("storeExists"), info.exists ());
  map.insert (QStringLiteral ("storeDirectory"), info.dir ().absolutePath ());
  map.insert (QStringLiteral ("backupDirectory"),
              info.dir ().filePath (QStringLiteral ("backups")));
  if (info.exists ())
    {
      map.insert (QStringLiteral ("storeBytes"),
                  QVariant::fromValue<qulonglong> (
                      static_cast<qulonglong> (std::max<qint64> (
                          0, info.size ()))));
      QFile file {resolved};
      if (file.open (QIODevice::ReadOnly))
        {
          QJsonParseError parseError;
          QJsonDocument const document =
              QJsonDocument::fromJson (file.readAll (), &parseError);
          if (parseError.error != QJsonParseError::NoError
              || !document.isObject ())
            {
              errors.push_back (QStringLiteral (
                  "store JSON is not readable: %1")
                  .arg (parseError.errorString ()));
            }
          else
            {
              int const diskVersion = document.object ().value (
                  QStringLiteral ("version")).toInt (0);
              map.insert (QStringLiteral ("diskVersion"), diskVersion);
              if (diskVersion < 1 || diskVersion > kLocalStoreVersion)
                {
                  errors.push_back (QStringLiteral (
                      "unsupported disk store version %1")
                      .arg (diskVersion));
                }
            }
        }
      else
        {
          errors.push_back (QStringLiteral ("cannot read store: %1")
                            .arg (file.errorString ()));
        }
    }
  else
    {
      map.insert (QStringLiteral ("storeBytes"),
                  QVariant::fromValue<qulonglong> (0u));
      warnings.push_back (QStringLiteral (
          "store file does not exist yet; save will create it"));
    }

  if (m_qsoLog.empty () && m_contactHistory.empty ()
      && m_mailbox.empty () && m_fileTransfers.empty ()
      && m_clusterLastHeard.empty ())
    {
      warnings.push_back (QStringLiteral ("no durable FT2-Link records yet"));
    }

  map.insert (QStringLiteral ("warnings"), warnings);
  map.insert (QStringLiteral ("errors"), errors);
  map.insert (QStringLiteral ("ok"), errors.isEmpty ());
  map.insert (QStringLiteral ("summary"),
              errors.isEmpty ()
              ? QStringLiteral ("OK")
              : QStringLiteral ("ERROR"));
  return map;
}

QVariantMap FT2LinkQmlAdapter::backupLocalStore (QString const& directory)
{
  QVariantMap result;
  QString const resolved = resolvedLocalStorePath ();
  QFileInfo const storeInfo {resolved};
  QString const requestedDirectory = directory.trimmed ();
  QDir backupDir {requestedDirectory.isEmpty ()
                  ? storeInfo.dir ().filePath (QStringLiteral ("backups"))
                  : requestedDirectory};
  if (!backupDir.exists ()
      && !QDir ().mkpath (backupDir.absolutePath ()))
    {
      QString const error = QStringLiteral (
          "Cannot create FT2-Link backup directory: %1")
          .arg (backupDir.absolutePath ());
      result.insert (QStringLiteral ("ok"), false);
      result.insert (QStringLiteral ("error"), error);
      setLastError (error);
      return result;
    }

  QString const stamp = QDateTime::currentDateTimeUtc ().toString (
      QStringLiteral ("yyyyMMdd-HHmmss"));
  QString const backupPath = backupDir.filePath (
      QStringLiteral ("ft2link-state-%1.json").arg (stamp));
  QByteArray const data = serializeLocalStore ();

  QSaveFile file {backupPath};
  if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate))
    {
      QString const error = QStringLiteral (
          "Cannot write FT2-Link backup: %1").arg (file.errorString ());
      result.insert (QStringLiteral ("ok"), false);
      result.insert (QStringLiteral ("path"), backupPath);
      result.insert (QStringLiteral ("error"), error);
      setLastError (error);
      return result;
    }
  if (file.write (data) != data.size () || !file.commit ())
    {
      QString const error = QStringLiteral (
          "Cannot commit FT2-Link backup: %1").arg (file.errorString ());
      result.insert (QStringLiteral ("ok"), false);
      result.insert (QStringLiteral ("path"), backupPath);
      result.insert (QStringLiteral ("error"), error);
      setLastError (error);
      return result;
    }

  result.insert (QStringLiteral ("ok"), true);
  result.insert (QStringLiteral ("path"), backupPath);
  result.insert (QStringLiteral ("directory"), backupDir.absolutePath ());
  result.insert (QStringLiteral ("bytes"),
                 QVariant::fromValue<qulonglong> (
                     static_cast<qulonglong> (data.size ())));
  result.insert (QStringLiteral ("sha256"), sha256Hex (data));
  clearLastError ();
  return result;
}

QVariantMap FT2LinkQmlAdapter::fixLocalStore (bool makeBackup)
{
  QVariantMap result;
  QVariantMap backup;
  if (makeBackup)
    {
      backup = backupLocalStore (QString {});
      result.insert (QStringLiteral ("backup"), backup);
      if (!backup.value (QStringLiteral ("ok")).toBool ())
        {
          result.insert (QStringLiteral ("ok"), false);
          result.insert (QStringLiteral ("error"),
                         backup.value (QStringLiteral ("error")).toString ());
          return result;
        }
    }

  bool const saved = saveLocalStore ();
  result.insert (QStringLiteral ("ok"), saved);
  result.insert (QStringLiteral ("path"), resolvedLocalStorePath ());
  result.insert (QStringLiteral ("backupCreated"), makeBackup);
  result.insert (QStringLiteral ("audit"), localStoreAudit ());
  if (!saved)
    {
      result.insert (QStringLiteral ("error"), m_lastLocalStoreError);
    }
  else
    {
      result.insert (QStringLiteral ("summary"),
                     QStringLiteral ("FT2-Link store rewritten"));
    }
  return result;
}

void FT2LinkQmlAdapter::clearBroadcasts ()
{
  if (m_broadcasts.empty ())
    {
      return;
    }
  m_broadcasts.clear ();
  emit broadcastsChanged ();
}

void FT2LinkQmlAdapter::clearAlertEvents ()
{
  if (m_alerts.empty ())
    {
      return;
    }
  m_alerts.clear ();
  emit alertsChanged ();
}

bool FT2LinkQmlAdapter::markMailboxRead (quint32 messageId,
                                         bool read,
                                         quint64 nowMs)
{
  if (messageId == 0u)
    {
      setLastError (QStringLiteral ("FT2-Link mailbox item id is invalid"));
      return false;
    }

  for (MailboxMessage& message : m_mailbox)
    {
      if (message.id != messageId)
        {
          continue;
        }
      if (message.direction != QStringLiteral ("Incoming"))
        {
          setLastError (QStringLiteral (
              "FT2-Link mailbox read state applies only to incoming mail"));
          return false;
        }

      QString const state = read ? QStringLiteral ("Read")
                                 : QStringLiteral ("Received");
      if (message.state == state)
        {
          clearLastError ();
          return true;
        }
      message.state = state;
      message.updatedAtMs = nowMs;
      emit mailboxChanged ();
      clearLastError ();
      return true;
    }

  setLastError (QStringLiteral ("FT2-Link mailbox item not found"));
  return false;
}

bool FT2LinkQmlAdapter::deleteMailboxMessage (quint32 messageId)
{
  if (messageId == 0u)
    {
      setLastError (QStringLiteral ("FT2-Link mailbox item id is invalid"));
      return false;
    }

  for (std::map<std::uint16_t, quint32>::const_iterator it =
           m_liveOutboundMailboxId.begin ();
       it != m_liveOutboundMailboxId.end ();
       ++it)
    {
      if (it->second == messageId)
        {
          setLastError (QStringLiteral (
              "FT2-Link mailbox item is still in an active transfer"));
          return false;
        }
    }

  for (std::vector<MailboxMessage>::iterator it = m_mailbox.begin ();
       it != m_mailbox.end ();
       ++it)
    {
      if (it->id != messageId)
        {
          continue;
        }
      m_mailbox.erase (it);
      emit mailboxChanged ();
      clearLastError ();
      return true;
    }

  setLastError (QStringLiteral ("FT2-Link mailbox item not found"));
  return false;
}

void FT2LinkQmlAdapter::clearMailbox ()
{
  if (m_mailbox.empty ())
    {
      return;
    }
  m_mailbox.clear ();
  emit mailboxChanged ();
}

void FT2LinkQmlAdapter::clearForms ()
{
  if (m_forms.empty ())
    {
      return;
    }
  m_forms.clear ();
  emit formsChanged ();
}

void FT2LinkQmlAdapter::clearFileTransfers ()
{
  if (m_fileTransfers.empty ())
    {
      return;
    }
  m_fileTransfers.clear ();
  emit fileTransfersChanged ();
}

void FT2LinkQmlAdapter::clearBulletins ()
{
  if (m_bulletins.empty ())
    {
      return;
    }
  m_bulletins.clear ();
  emit bulletinsChanged ();
}

void FT2LinkQmlAdapter::clearQsoLog ()
{
  if (m_qsoLog.empty ())
    {
      return;
    }
  m_qsoLog.clear ();
  emit qsoLogChanged ();
}

void FT2LinkQmlAdapter::clearLogbookOutbox ()
{
  if (m_logbookOutbox.empty ())
    {
      return;
    }
  m_logbookOutbox.clear ();
  emit logbookOutboxChanged ();
}

void FT2LinkQmlAdapter::clearContactHistory ()
{
  if (m_contactHistory.empty ())
    {
      return;
    }
  m_contactHistory.clear ();
  emit contactHistoryChanged ();
}

void FT2LinkQmlAdapter::clearPingLog ()
{
  if (m_pingLog.empty () && m_pendingPings.empty ())
    {
      return;
    }
  m_pingLog.clear ();
  m_pendingPings.clear ();
  emit pingLogChanged ();
}

void FT2LinkQmlAdapter::clearPathReports ()
{
  if (m_pathReports.empty ())
    {
      return;
    }
  m_pathReports.clear ();
  emit pathReportsChanged ();
}

void FT2LinkQmlAdapter::clearBeaconHistory ()
{
  if (m_beaconHistory.empty ())
    {
      return;
    }
  m_beaconHistory.clear ();
  emit beaconHistoryChanged ();
}

void FT2LinkQmlAdapter::clearClusterLastHeard ()
{
  if (m_clusterLastHeard.empty ())
    {
      return;
    }
  m_clusterLastHeard.clear ();
  emit clusterLastHeardChanged ();
}

void FT2LinkQmlAdapter::setLocalStorePath (QString const& path)
{
  QString const normalized = path.trimmed ().isEmpty ()
      ? defaultLocalStorePath ()
      : path.trimmed ();
  bool const changed = m_localStorePath != normalized
      || !m_localStorePersistenceEnabled;
  m_localStorePath = normalized;
  m_localStorePersistenceEnabled = true;
  if (changed)
    {
      emit localStoreChanged ();
    }
}

bool FT2LinkQmlAdapter::loadLocalStore (QString const& path)
{
  QString const resolved = resolvedLocalStorePath (path);
  if (!path.trimmed ().isEmpty ())
    {
      m_localStorePath = resolved;
      m_localStorePersistenceEnabled = true;
    }
  else if (m_localStorePath.isEmpty ())
    {
      m_localStorePath = resolved;
    }
  m_localStorePersistenceEnabled = true;

  QFile file {resolved};
  if (!file.exists ())
    {
      setLocalStoreState (resolved, false, QString {});
      return true;
    }
  if (!file.open (QIODevice::ReadOnly))
    {
      QString const error = QStringLiteral ("Cannot read FT2-Link store: %1")
          .arg (file.errorString ());
      setLocalStoreState (resolved, false, error);
      setLastError (error);
      return false;
    }

  QString error;
  QByteArray const bytes = file.readAll ();
  m_loadingLocalStore = true;
  bool const ok = applyLocalStoreBytes (bytes, &error);
  if (ok)
    {
      emit broadcastsChanged ();
      emit alertsChanged ();
      emit mailboxChanged ();
      emit formsChanged ();
      emit fileTransfersChanged ();
      emit bulletinsChanged ();
      emit qsoLogChanged ();
      emit contactHistoryChanged ();
      emit pingLogChanged ();
      emit pathReportsChanged ();
      emit beaconHistoryChanged ();
      emit clusterLastHeardChanged ();
    }
  m_loadingLocalStore = false;

  if (!ok)
    {
      setLocalStoreState (resolved, false, error);
      setLastError (error);
      return false;
    }

  setLocalStoreState (resolved, true, QString {});
  clearLastError ();
  return true;
}

bool FT2LinkQmlAdapter::saveLocalStore (QString const& path)
{
  QString const resolved = resolvedLocalStorePath (path);
  if (!path.trimmed ().isEmpty ())
    {
      m_localStorePath = resolved;
    }
  else if (m_localStorePath.isEmpty ())
    {
      m_localStorePath = resolved;
    }
  m_localStorePersistenceEnabled = true;

  QFileInfo const info {resolved};
  QDir const dir = info.dir ();
  if (!dir.exists () && !QDir ().mkpath (dir.absolutePath ()))
    {
      QString const error = QStringLiteral (
          "Cannot create FT2-Link store directory: %1")
          .arg (dir.absolutePath ());
      setLocalStoreState (resolved, false, error);
      setLastError (error);
      return false;
    }

  QSaveFile file {resolved};
  if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate))
    {
      QString const error = QStringLiteral ("Cannot write FT2-Link store: %1")
          .arg (file.errorString ());
      setLocalStoreState (resolved, false, error);
      setLastError (error);
      return false;
    }

  QByteArray const data = serializeLocalStore ();
  if (file.write (data) != data.size ())
    {
      QString const error = QStringLiteral ("Cannot write FT2-Link store: %1")
          .arg (file.errorString ());
      setLocalStoreState (resolved, false, error);
      setLastError (error);
      return false;
    }
  if (!file.commit ())
    {
      QString const error = QStringLiteral ("Cannot commit FT2-Link store: %1")
          .arg (file.errorString ());
      setLocalStoreState (resolved, false, error);
      setLastError (error);
      return false;
    }

  setLocalStoreState (resolved, true, QString {});
  return true;
}

QVariantMap FT2LinkQmlAdapter::sessionInfo (quint16 sessionId) const
{
  AppSession const* session = m_model.session (sessionId);
  return session ? sessionMap (*session) : QVariantMap {};
}

QVariantList FT2LinkQmlAdapter::sessions () const
{
  QVariantList list;
  std::vector<AppSession> sessions = m_model.sessions ();
  for (AppSession const& session : sessions)
    {
      list.push_back (sessionMap (session));
    }
  return list;
}

QVariantList FT2LinkQmlAdapter::messages (quint16 sessionId) const
{
  QVariantList list;
  AppSession const* session = m_model.session (sessionId);
  if (!session)
    {
      return list;
    }
  for (ChatMessage const& message : session->messages)
    {
      list.push_back (messageMap (message));
  }
  return list;
}

QVariantList FT2LinkQmlAdapter::typingIndicators (quint64 nowMs)
{
  expireTypingIndicators (nowMs);

  QVariantList list;
  for (std::map<QString, quint64>::const_iterator it = m_typingPeers.begin ();
       it != m_typingPeers.end ();
       ++it)
    {
      QVariantMap map;
      map.insert (QStringLiteral ("call"), it->first);
      map.insert (QStringLiteral ("expiresAtMs"),
                  QVariant::fromValue<qulonglong> (
                      static_cast<qulonglong> (it->second)));
      list.push_back (map);
    }
  return list;
}

QString FT2LinkQmlAdapter::typingSummary (quint64 nowMs)
{
  expireTypingIndicators (nowMs);
  if (m_typingPeers.empty ())
    {
      return {};
    }

  QStringList calls;
  for (std::map<QString, quint64>::const_iterator it = m_typingPeers.begin ();
       it != m_typingPeers.end ();
       ++it)
    {
      calls.push_back (it->first);
      if (calls.size () >= 3)
        {
          break;
        }
    }
  if (m_typingPeers.size () > 3u)
    {
      calls.push_back (QStringLiteral ("+%1")
                       .arg (static_cast<int> (m_typingPeers.size () - 3u)));
    }
  return QStringLiteral ("%1 typing").arg (calls.join (QStringLiteral (", ")));
}

QStringList FT2LinkQmlAdapter::detectAlertTags (QString const& text) const
{
  QStringList matches;
  for (QString const& tag : alertTags ())
    {
      if (text.contains (tag, Qt::CaseInsensitive))
        {
          matches.push_back (tag);
        }
    }
  return matches;
}

void FT2LinkQmlAdapter::recordBroadcast (QString const& fromCall,
                                         QString const& text,
                                         quint64 nowMs,
                                         QString const& source)
{
  QString const normalizedCall = fromCall.trimmed ().toUpper ();
  QString const normalizedText = text.trimmed ();
  if (normalizedText.isEmpty ())
    {
      return;
    }

  BroadcastMessage message;
  message.fromCall = normalizedCall.isEmpty ()
      ? QStringLiteral ("UNKNOWN")
      : normalizedCall;
  message.text = normalizedText;
  message.source = source;
  message.atMs = nowMs;
  message.alertTags = detectAlertTags (normalizedText);
  m_broadcasts.push_back (message);
  if (m_broadcasts.size () > 100u)
    {
      m_broadcasts.erase (m_broadcasts.begin ());
    }
  emit broadcastsChanged ();
  touchContact (message.fromCall,
                nowMs,
                message.alertTags.isEmpty () ? QStringLiteral ("broadcast")
                                             : QStringLiteral ("alert"));
  ContactHistory& broadcastContact = m_contactHistory[message.fromCall];
  ++broadcastContact.broadcastCount;

  for (QString const& tag : message.alertTags)
    {
      AlertEvent alert;
      alert.fromCall = message.fromCall;
      alert.text = message.text;
      alert.source = message.source;
      alert.tag = tag;
      alert.atMs = nowMs;
      m_alerts.push_back (alert);
    }
  if (m_alerts.size () > 100u)
    {
      m_alerts.erase (
          m_alerts.begin (),
          m_alerts.begin () + static_cast<std::ptrdiff_t> (m_alerts.size () - 100u));
    }
  if (!message.alertTags.isEmpty ())
    {
      broadcastContact.alertCount += message.alertTags.size ();
      emit contactHistoryChanged ();
      emit alertsChanged ();
    }
}

void FT2LinkQmlAdapter::recordPathFinderAlert (QString const& fromCall,
                                               QString const& text,
                                               quint64 nowMs)
{
  QString const normalizedCall = normalizeCallsign (fromCall);
  QString const normalizedText = text.trimmed ();
  if (normalizedText.isEmpty ())
    {
      return;
    }

  AlertEvent alert;
  alert.fromCall = normalizedCall.isEmpty () ? QStringLiteral ("UNKNOWN")
                                             : normalizedCall;
  alert.text = normalizedText;
  alert.source = QStringLiteral ("Path");
  alert.tag = QStringLiteral ("PATH");
  alert.atMs = nowMs;
  m_alerts.push_back (alert);
  if (m_alerts.size () > 100u)
    {
      m_alerts.erase (m_alerts.begin ());
    }
  emit alertsChanged ();

  touchContact (alert.fromCall, nowMs, QStringLiteral ("path"));
  ContactHistory& contact = m_contactHistory[alert.fromCall];
  ++contact.alertCount;
  emit contactHistoryChanged ();
}

bool FT2LinkQmlAdapter::handlePathFinderBroadcast (QString const& fromCall,
                                                   QString const& text,
                                                   quint64 nowMs)
{
  QString target;
  QString requestor;
  if (parsePathFinderRequest (text, &target, &requestor))
    {
      if (requestor.isEmpty ())
        {
          requestor = normalizeCallsign (fromCall);
        }
      QVariantMap const candidate = pathFinderCandidate (target, nowMs);
      if (candidate.value (QStringLiteral ("canRespond")).toBool ())
        {
          QString const age = QString::number (
              candidate.value (QStringLiteral ("ageMinutes")).toULongLong ());
          QString const locator = candidate.value (
              QStringLiteral ("locator")).toString ();
          QString message = QStringLiteral (
              "PATH request from %1 for %2: heard %3m ago")
              .arg (requestor.isEmpty () ? QStringLiteral ("UNKNOWN")
                                         : requestor,
                    target,
                    age);
          if (!locator.isEmpty ())
            {
              message += QStringLiteral (" at %1").arg (locator);
            }
          recordPathFinderAlert (requestor, message, nowMs);
        }
      return true;
    }

  QString via;
  QString locator;
  int ageMinutes = -1;
  if (parsePathFinderResponse (text, &target, &via, &locator, &ageMinutes))
    {
      QString message = QStringLiteral ("PATH found %1 via %2")
          .arg (target, via.isEmpty () ? normalizeCallsign (fromCall) : via);
      if (!locator.isEmpty ())
        {
          message += QStringLiteral (" %1").arg (locator);
        }
      if (ageMinutes >= 0)
        {
          message += QStringLiteral (" age %1m").arg (ageMinutes);
        }
      QString const pathCall = via.isEmpty () ? fromCall : via;
      recordPathFinderAlert (pathCall, message, nowMs);
      QString const normalizedRelay = normalizeCallsign (pathCall);
      if (!target.isEmpty () && !normalizedRelay.isEmpty ())
        {
          PathRelayHint hint;
          hint.targetCall = target;
          hint.relayCall = normalizedRelay;
          hint.locator = locator.trimmed ().toUpper ();
          hint.ageMinutes = ageMinutes;
          hint.atMs = nowMs;
          bool replaced = false;
          for (PathRelayHint& existing : m_pathRelayHints)
            {
              if (existing.targetCall == hint.targetCall
                  && existing.relayCall == hint.relayCall)
                {
                  existing = hint;
                  replaced = true;
                  break;
                }
            }
          if (!replaced)
            {
              m_pathRelayHints.push_back (hint);
            }
          if (m_pathRelayHints.size () > 100u)
            {
              m_pathRelayHints.erase (m_pathRelayHints.begin ());
            }
        }
      if (!locator.isEmpty ())
        {
          touchContact (pathCall, nowMs, QStringLiteral ("path response"),
                        locator);
        }
      return true;
    }

  return false;
}

quint32 FT2LinkQmlAdapter::recordMailbox (QString const& direction,
                                          QString const& fromCall,
                                          QString const& toCall,
                                          QString const& subject,
                                          QString const& body,
                                          QString const& state,
                                          quint64 nowMs,
                                          bool urgent,
                                          bool emcomm,
                                          QString const& relayViaCall,
                                          int relayHopCount,
                                          QString const& relayProtocol)
{
  MailboxMessage message;
  message.id = m_nextMailboxId++;
  if (m_nextMailboxId == 0u)
    {
      m_nextMailboxId = 1u;
    }
  message.direction = direction;
  message.fromCall = normalizeCallsign (fromCall);
  message.toCall = normalizeCallsign (toCall);
  message.subject = subject.trimmed ().isEmpty ()
      ? QStringLiteral ("FT2-Link mail")
      : subject.trimmed ();
  message.body = body.trimmed ();
  message.state = state;
  message.urgent = urgent;
  message.emcomm = emcomm;
  message.relayViaCall = normalizeCallsign (relayViaCall);
  message.relayHopCount = std::clamp (relayHopCount, 0, kMaxRelayHopCount);
  message.relayProtocol = relayProtocol.trimmed ().toUpper ();
  message.atMs = nowMs;
  message.updatedAtMs = nowMs;
  if (message.body.isEmpty ())
    {
      return 0u;
    }

  m_mailbox.push_back (message);
  if (m_mailbox.size () > 100u)
    {
      m_mailbox.erase (m_mailbox.begin ());
    }
  emit mailboxChanged ();
  QString peer = message.direction == QStringLiteral ("Incoming")
      ? message.fromCall
      : message.toCall;
  if (message.direction == QStringLiteral ("Relay")
      && !message.relayViaCall.isEmpty ()
      && message.relayViaCall != message.toCall)
    {
      peer = message.relayViaCall;
    }
  touchContact (peer, nowMs, QStringLiteral ("mail"));
  ++m_contactHistory[peer].mailCount;
  emit contactHistoryChanged ();
  return message.id;
}

bool FT2LinkQmlAdapter::updateMailboxState (quint32 messageId,
                                            QString const& state,
                                            quint64 nowMs)
{
  if (messageId == 0u)
    {
      return false;
    }
  for (MailboxMessage& message : m_mailbox)
    {
      if (message.id != messageId)
        {
          continue;
        }
      if (message.state == state && message.updatedAtMs == nowMs)
        {
          return true;
        }
      message.state = state;
      message.updatedAtMs = nowMs;
      emit mailboxChanged ();
      return true;
    }
  return false;
}

void FT2LinkQmlAdapter::notifyParkedMailboxForCall (QString const& call,
                                                    quint64 nowMs)
{
  QString const target = normalizeCallsign (call);
  if (target.isEmpty ())
    {
      return;
    }

  bool mailboxUpdated = false;
  bool alertUpdated = false;
  for (MailboxMessage& message : m_mailbox)
    {
      if (message.toCall != target
          || message.state != QStringLiteral ("Parked")
          || message.relayNotifiedAtMs != 0u)
        {
          continue;
        }
      message.state = QStringLiteral ("Relay ready");
      message.updatedAtMs = nowMs;
      message.relayNotifiedAtMs = nowMs;
      mailboxUpdated = true;

      AlertEvent alert;
      alert.fromCall = target;
      alert.text = QStringLiteral ("Parked mail ready: %1")
          .arg (message.subject);
      alert.source = QStringLiteral ("Relay");
      alert.tag = QStringLiteral ("MAIL");
      alert.atMs = nowMs;
      m_alerts.push_back (alert);
      if (m_alerts.size () > 100u)
        {
          m_alerts.erase (m_alerts.begin ());
        }
      alertUpdated = true;
      touchContact (target, nowMs, QStringLiteral ("mail relay ready"));
    }

  if (mailboxUpdated)
    {
      emit mailboxChanged ();
      emit contactHistoryChanged ();
    }
  if (alertUpdated)
    {
      emit alertsChanged ();
    }
}

void FT2LinkQmlAdapter::recordBeaconHistory (
    QString const& direction,
    StationAdvertisement const& advertisement,
    QString const& source,
    quint64 nowMs)
{
  BeaconHistoryEntry entry;
  entry.direction = direction.trimmed ().isEmpty ()
      ? QStringLiteral ("RX")
      : direction.trimmed ().toUpper ();
  entry.call = normalizeCallsign (
      QString::fromStdString (advertisement.station.call));
  entry.locator = QString::fromStdString (
      advertisement.station.locator).trimmed ().toUpper ();
  entry.name = QString::fromStdString (advertisement.station.name).trimmed ();
  entry.profileName = QString::fromStdString (
      decodium::ft2link::profileName (
          advertisement.capabilities.preferredProfile));
  entry.cq = advertisement.cq;
  entry.cqType = QString::fromStdString (advertisement.cqType).trimmed ();
  if (entry.cqType.isEmpty ())
    {
      entry.cqType = QStringLiteral ("CQ");
    }
  entry.cqLocator = QString::fromStdString (
      advertisement.cqLocator).trimmed ().toUpper ();
  entry.cqSlotId = advertisement.cqSlotId;
  entry.cqSlotSizeHz = advertisement.cqSlotSizeHz;
  entry.source = source.trimmed ().isEmpty ()
      ? QStringLiteral ("RF")
      : source.trimmed ();
  entry.atMs = nowMs;
  if (entry.call.isEmpty ())
    {
      return;
    }

  m_beaconHistory.push_back (entry);
  if (m_beaconHistory.size () > 100u)
    {
      m_beaconHistory.erase (m_beaconHistory.begin ());
    }
  emit beaconHistoryChanged ();
}

QString FT2LinkQmlAdapter::clusterBandLabel (qint64 dialFrequencyHz) const
{
  QString const configured = sanitizedClusterBand (m_clusterBand);
  if (configured != QStringLiteral ("LOCAL")
      || m_clusterDialFrequencyHz <= 0)
    {
      return configured;
    }

  qint64 const dial = dialFrequencyHz > 0 ? dialFrequencyHz
                                          : m_clusterDialFrequencyHz;
  if (dial <= 0)
    {
      return QStringLiteral ("LOCAL");
    }

  for (FrequencyPreset const& preset : m_frequencyPresets)
    {
      if (std::llabs (preset.dialFrequencyHz - dial) <= 500)
        {
          QString const band = sanitizedClusterBand (preset.band);
          if (band != QStringLiteral ("LOCAL"))
            {
              return band;
            }
        }
    }

  for (AllowedQsyRange const& range : m_allowedQsyRanges)
    {
      if (dial >= range.fromHz && dial <= range.toHz)
        {
          QString const label = sanitizedClusterBand (range.label);
          if (label != QStringLiteral ("LOCAL"))
            {
              return label;
            }
        }
    }

  struct BandGuess
  {
    qint64 fromHz;
    qint64 toHz;
    char const* label;
  };
  static BandGuess const guesses[] = {
    {1800000, 2000000, "160M"},
    {3500000, 4000000, "80M"},
    {5330000, 5410000, "60M"},
    {7000000, 7300000, "40M"},
    {10100000, 10150000, "30M"},
    {14000000, 14350000, "20M"},
    {18068000, 18168000, "17M"},
    {21000000, 21450000, "15M"},
    {24890000, 24990000, "12M"},
    {28000000, 29700000, "10M"},
    {50000000, 54000000, "6M"}
  };
  for (BandGuess const& guess : guesses)
    {
      if (dial >= guess.fromHz && dial <= guess.toHz)
        {
          return QString::fromLatin1 (guess.label);
        }
    }
  return QStringLiteral ("LOCAL");
}

QString FT2LinkQmlAdapter::effectiveClusterNodeId () const
{
  QString const configured = sanitizedClusterNodeId (m_clusterNodeId);
  if (configured != QStringLiteral ("LOCAL"))
    {
      return configured;
    }

  QString const localCall = normalizeCallsign (
      QString::fromStdString (m_model.localStation ().call));
  QString const host = sanitizedClusterNodeId (QSysInfo::machineHostName ());
  if (!localCall.isEmpty () && host != QStringLiteral ("LOCAL"))
    {
      return QStringLiteral ("%1/%2").arg (localCall, host).left (32);
    }
  if (!localCall.isEmpty ())
    {
      return localCall.left (32);
    }
  return host;
}

QVariantMap FT2LinkQmlAdapter::frequencyScheduleMap (
    FrequencyScheduleEntry const& entry,
    bool active,
    quint64 nowMs) const
{
  return frequencyScheduleEntryMap (
      entry.startMinute,
      entry.endMinute,
      entry.action,
      entry.dialFrequencyHz,
      entry.label,
      entry.cqType,
      active,
      nowMs);
}

FT2LinkQmlAdapter::FrequencyScheduleEntry const*
FT2LinkQmlAdapter::activeFrequencyScheduleEntry (quint64 nowMs) const
{
  if (m_frequencySchedule.empty ())
    {
      return nullptr;
    }
  QDateTime const at = QDateTime::fromMSecsSinceEpoch (
      static_cast<qint64> (nowMs), QTimeZone(QByteArrayLiteral("UTC"))).toUTC ();
  if (!at.isValid ())
    {
      return nullptr;
    }
  int const minute = at.time ().hour () * 60 + at.time ().minute ();
  for (FrequencyScheduleEntry const& entry : m_frequencySchedule)
    {
      if (scheduleContainsMinute (
              entry.startMinute, entry.endMinute, minute))
        {
          return &entry;
        }
    }
  return nullptr;
}

void FT2LinkQmlAdapter::recordClusterLastHeard (
    QString const& call,
    QString const& locator,
    QString const& name,
    QString const& profileName,
    QString const& event,
    QString const& source,
    bool cq,
    QString const& cqType,
    quint64 nowMs)
{
  if (!m_clusterEnabled)
    {
      return;
    }

  QString const normalizedCall = normalizeCallsign (call);
  QString const localCall = normalizeCallsign (
      QString::fromStdString (m_model.localStation ().call));
  if (normalizedCall.isEmpty () || normalizedCall == localCall
      || isCallBlocked (normalizedCall))
    {
      return;
    }

  qint64 const dial = m_clusterDialFrequencyHz > 0
      ? m_clusterDialFrequencyHz
      : 0;
  QString const band = clusterBandLabel (dial);
  QString const node = effectiveClusterNodeId ();
  QString const key = clusterKey (node, band, dial, normalizedCall);

  ClusterLastHeardEntry& entry = m_clusterLastHeard[key];
  bool const isNew = entry.call.isEmpty ();
  entry.call = normalizedCall;
  if (!locator.trimmed ().isEmpty ())
    {
      entry.locator = locator.trimmed ().toUpper ().left (12);
    }
  if (!name.trimmed ().isEmpty ())
    {
      entry.name = name.simplified ().left (48);
    }
  if (!profileName.trimmed ().isEmpty ())
    {
      entry.profileName = profileName.trimmed ().left (24);
    }
  entry.event = event.trimmed ().isEmpty ()
      ? QStringLiteral ("heard")
      : event.trimmed ().left (48);
  entry.source = source.trimmed ().isEmpty ()
      ? QStringLiteral ("LOCAL")
      : source.trimmed ().left (24);
  entry.nodeId = node;
  entry.band = band;
  entry.dialFrequencyHz = dial;
  entry.cq = cq;
  entry.cqType = cqType.trimmed ().isEmpty ()
      ? QStringLiteral ("CQ")
      : cqType.trimmed ().toUpper ().left (16);
  if (isNew || entry.firstHeardMs == 0u
      || (nowMs > 0u && nowMs < entry.firstHeardMs))
    {
      entry.firstHeardMs = nowMs;
    }
  entry.lastHeardMs = std::max (entry.lastHeardMs, nowMs);
  entry.heardCount = std::max (0, entry.heardCount) + 1;

  if (m_clusterLastHeard.size () > 300u)
    {
      std::map<QString, ClusterLastHeardEntry>::iterator oldest =
          m_clusterLastHeard.end ();
      for (std::map<QString, ClusterLastHeardEntry>::iterator it =
               m_clusterLastHeard.begin ();
           it != m_clusterLastHeard.end ();
           ++it)
        {
          if (oldest == m_clusterLastHeard.end ()
              || it->second.lastHeardMs < oldest->second.lastHeardMs)
            {
              oldest = it;
            }
        }
      if (oldest != m_clusterLastHeard.end ())
        {
          m_clusterLastHeard.erase (oldest);
        }
    }
  emit clusterLastHeardChanged ();
}

QVariantMap FT2LinkQmlAdapter::parkedMailboxSummaryForCallInternal (
    QString const& call,
    quint64 nowMs) const
{
  QString const target = normalizeCallsign (call);
  QVariantMap map;
  map.insert (QStringLiteral ("targetCall"), target);
  map.insert (QStringLiteral ("count"), 0);
  map.insert (QStringLiteral ("parkedCount"), 0);
  map.insert (QStringLiteral ("relayReadyCount"), 0);
  map.insert (QStringLiteral ("pendingRelayCount"), 0);
  map.insert (QStringLiteral ("urgentCount"), 0);
  map.insert (QStringLiteral ("emcommCount"), 0);
  map.insert (QStringLiteral ("mailboxId"), 0u);
  map.insert (QStringLiteral ("hasMailbox"), false);
  if (target.isEmpty ())
    {
      return map;
    }

  int count = 0;
  int parkedCount = 0;
  int relayReadyCount = 0;
  int pendingRelayCount = 0;
  int urgentCount = 0;
  int emcommCount = 0;
  MailboxMessage const* newest = nullptr;
  for (MailboxMessage const& message : m_mailbox)
    {
      if (message.toCall != target || message.body.trimmed ().isEmpty ())
        {
          continue;
        }
      bool const relayCandidate =
          message.direction == QStringLiteral ("Parked")
          || message.direction == QStringLiteral ("Relay");
      if (!relayCandidate)
        {
          continue;
        }
      bool const activeState =
          message.state == QStringLiteral ("Parked")
          || message.state == QStringLiteral ("Relay ready")
          || message.state == QStringLiteral ("Pending relay")
          || message.state == QStringLiteral ("Failed");
      if (!activeState)
        {
          continue;
        }

      ++count;
      if (message.state == QStringLiteral ("Relay ready"))
        {
          ++relayReadyCount;
        }
      else if (message.state == QStringLiteral ("Pending relay"))
        {
          ++pendingRelayCount;
        }
      else
        {
          ++parkedCount;
        }
      if (message.urgent)
        {
          ++urgentCount;
        }
      if (message.emcomm)
        {
          ++emcommCount;
        }
      if (!newest || message.updatedAtMs > newest->updatedAtMs)
        {
          newest = &message;
        }
    }

  map.insert (QStringLiteral ("count"), count);
  map.insert (QStringLiteral ("parkedCount"), parkedCount);
  map.insert (QStringLiteral ("relayReadyCount"), relayReadyCount);
  map.insert (QStringLiteral ("pendingRelayCount"), pendingRelayCount);
  map.insert (QStringLiteral ("urgentCount"), urgentCount);
  map.insert (QStringLiteral ("emcommCount"), emcommCount);
  map.insert (QStringLiteral ("hasMailbox"), count > 0);
  if (newest)
    {
      quint64 const referenceMs = newest->updatedAtMs > 0u
          ? newest->updatedAtMs
          : newest->atMs;
      quint64 const ageMs = nowMs >= referenceMs ? nowMs - referenceMs : 0u;
      map.insert (QStringLiteral ("mailboxId"), newest->id);
      map.insert (QStringLiteral ("subject"), newest->subject);
      map.insert (QStringLiteral ("state"), newest->state);
      map.insert (QStringLiteral ("direction"), newest->direction);
      map.insert (QStringLiteral ("urgent"), newest->urgent);
      map.insert (QStringLiteral ("emcomm"), newest->emcomm);
      map.insert (QStringLiteral ("ageMinutes"),
                  QVariant::fromValue<qulonglong> (
                      static_cast<qulonglong> (ageMs / 60000u)));
    }
  return map;
}

QVariantMap FT2LinkQmlAdapter::parkedMailboxSummaryForCall (
    QString const& call,
    quint64 nowMs) const
{
  return parkedMailboxSummaryForCallInternal (call, nowMs);
}

QVariantMap FT2LinkQmlAdapter::pathRelayCandidate (
    QString const& targetCall,
    quint64 nowMs) const
{
  QString const target = normalizeCallsign (targetCall);
  QVariantMap map;
  map.insert (QStringLiteral ("targetCall"), target);
  map.insert (QStringLiteral ("canRelay"), false);
  map.insert (QStringLiteral ("parkedMailboxCount"), 0);
  map.insert (QStringLiteral ("mailboxId"), 0u);
  if (target.isEmpty ())
    {
      return map;
    }

  QVariantMap const mailSummary =
      parkedMailboxSummaryForCallInternal (target, nowMs);
  map.insert (QStringLiteral ("parkedMailboxCount"),
              mailSummary.value (QStringLiteral ("count")).toInt ());
  map.insert (QStringLiteral ("mailboxId"),
              mailSummary.value (QStringLiteral ("mailboxId")).toUInt ());
  map.insert (QStringLiteral ("mailboxSubject"),
              mailSummary.value (QStringLiteral ("subject")).toString ());
  map.insert (QStringLiteral ("mailboxState"),
              mailSummary.value (QStringLiteral ("state")).toString ());

  for (std::vector<PathRelayHint>::const_reverse_iterator it =
           m_pathRelayHints.rbegin ();
       it != m_pathRelayHints.rend ();
       ++it)
    {
      if (it->targetCall != target)
        {
          continue;
        }
      quint64 const hintAgeMs = nowMs >= it->atMs ? nowMs - it->atMs : 0u;
      if (hintAgeMs > kPathFinderMaxAgeMs)
        {
          continue;
        }
      map.insert (QStringLiteral ("canRelay"), true);
      map.insert (QStringLiteral ("relayCall"), it->relayCall);
      map.insert (QStringLiteral ("locator"), it->locator);
      map.insert (QStringLiteral ("heardAgeMinutes"), it->ageMinutes);
      map.insert (QStringLiteral ("hintAgeMinutes"),
                  QVariant::fromValue<qulonglong> (
                      static_cast<qulonglong> (hintAgeMs / 60000u)));
      map.insert (QStringLiteral ("hintAtMs"),
                  QVariant::fromValue<qulonglong> (
                      static_cast<qulonglong> (it->atMs)));
      map.insert (QStringLiteral ("readyToForward"),
                  mailSummary.value (QStringLiteral ("count")).toInt () > 0);
      return map;
    }

  return map;
}

QVariantMap FT2LinkQmlAdapter::pathRelayCandidateForStation (
    QString const& relayCall,
    quint64 nowMs) const
{
  QString const relay = normalizeCallsign (relayCall);
  QVariantMap map;
  map.insert (QStringLiteral ("relayCall"), relay);
  map.insert (QStringLiteral ("canRelay"), false);
  if (relay.isEmpty ())
    {
      return map;
    }

  for (std::vector<PathRelayHint>::const_reverse_iterator it =
           m_pathRelayHints.rbegin ();
       it != m_pathRelayHints.rend ();
       ++it)
    {
      if (it->relayCall != relay)
        {
          continue;
        }
      quint64 const hintAgeMs = nowMs >= it->atMs ? nowMs - it->atMs : 0u;
      if (hintAgeMs > kPathFinderMaxAgeMs)
        {
          continue;
        }
      QVariantMap const mailSummary =
          parkedMailboxSummaryForCallInternal (it->targetCall, nowMs);
      if (mailSummary.value (QStringLiteral ("count")).toInt () <= 0)
        {
          continue;
        }
      map.insert (QStringLiteral ("canRelay"), true);
      map.insert (QStringLiteral ("targetCall"), it->targetCall);
      map.insert (QStringLiteral ("locator"), it->locator);
      map.insert (QStringLiteral ("heardAgeMinutes"), it->ageMinutes);
      map.insert (QStringLiteral ("hintAgeMinutes"),
                  QVariant::fromValue<qulonglong> (
                      static_cast<qulonglong> (hintAgeMs / 60000u)));
      map.insert (QStringLiteral ("parkedMailboxCount"),
                  mailSummary.value (QStringLiteral ("count")).toInt ());
      map.insert (QStringLiteral ("mailboxId"),
                  mailSummary.value (QStringLiteral ("mailboxId")).toUInt ());
      map.insert (QStringLiteral ("mailboxSubject"),
                  mailSummary.value (QStringLiteral ("subject")).toString ());
      return map;
    }

  return map;
}

QVariantMap FT2LinkQmlAdapter::relayWorkflowForStation (
    QString const& relayCall,
    quint64 nowMs) const
{
  return relayWorkflowForStationInternal (relayCall, nowMs);
}

QVariantMap FT2LinkQmlAdapter::relayWorkflowForStationInternal (
    QString const& relayCall,
    quint64 nowMs) const
{
  QString const relay = normalizeCallsign (relayCall);
  QVariantMap map;
  map.insert (QStringLiteral ("relayCall"), relay);
  map.insert (QStringLiteral ("canRelay"), false);
  map.insert (QStringLiteral ("readyToForward"), false);
  map.insert (QStringLiteral ("mailboxId"), 0u);
  map.insert (QStringLiteral ("parkedMailboxCount"), 0);
  map.insert (QStringLiteral ("urgentCount"), 0);
  map.insert (QStringLiteral ("emcommCount"), 0);
  map.insert (QStringLiteral ("pendingRelayCount"), 0);
  map.insert (QStringLiteral ("state"), QStringLiteral ("Idle"));
  map.insert (QStringLiteral ("priority"), QStringLiteral ("NORMAL"));
  map.insert (QStringLiteral ("badge"), QString {});
  map.insert (QStringLiteral ("line"), QStringLiteral ("No relay workflow"));
  if (relay.isEmpty ())
    {
      return map;
    }

  QVariantMap const pathRelay = pathRelayCandidateForStation (relay, nowMs);
  if (!pathRelay.value (QStringLiteral ("canRelay")).toBool ())
    {
      return map;
    }

  QString const target =
      normalizeCallsign (pathRelay.value (QStringLiteral ("targetCall")).toString ());
  QVariantMap const mailSummary =
      parkedMailboxSummaryForCallInternal (target, nowMs);
  int const mailCount = mailSummary.value (QStringLiteral ("count")).toInt ();
  int const urgentCount =
      mailSummary.value (QStringLiteral ("urgentCount")).toInt ();
  int const emcommCount =
      mailSummary.value (QStringLiteral ("emcommCount")).toInt ();
  int const pendingRelayCount =
      mailSummary.value (QStringLiteral ("pendingRelayCount")).toInt ();
  QString const priority = emcommCount > 0
      ? QStringLiteral ("EMCOMM")
      : (urgentCount > 0 ? QStringLiteral ("URGENT")
                         : QStringLiteral ("NORMAL"));
  QString const state = pendingRelayCount > 0
      ? QStringLiteral ("Pending")
      : QStringLiteral ("Ready");
  QString const subject =
      mailSummary.value (QStringLiteral ("subject")).toString ().simplified ();
  qulonglong const heardAge =
      pathRelay.value (QStringLiteral ("heardAgeMinutes")).toULongLong ();
  qulonglong const hintAge =
      pathRelay.value (QStringLiteral ("hintAgeMinutes")).toULongLong ();
  QString const locator =
      pathRelay.value (QStringLiteral ("locator")).toString ().trimmed ().toUpper ();
  QString const badge = QStringLiteral ("RLY>%1%2")
      .arg (target,
            priority == QStringLiteral ("EMCOMM")
                ? QStringLiteral ("!")
                : (priority == QStringLiteral ("URGENT")
                   ? QStringLiteral ("*")
                   : QString {}));

  QStringList lineParts;
  lineParts << QStringLiteral ("Relay %1 -> %2").arg (relay, target);
  lineParts << QStringLiteral ("mail %1").arg (mailCount);
  if (priority != QStringLiteral ("NORMAL"))
    {
      lineParts << priority;
    }
  if (!locator.isEmpty ())
    {
      lineParts << locator;
    }
  lineParts << QStringLiteral ("heard %1m").arg (heardAge);
  lineParts << QStringLiteral ("hint %1m").arg (hintAge);
  if (!subject.isEmpty ())
    {
      lineParts << QStringLiteral ("\"%1\"").arg (subject.left (36));
    }

  map.insert (QStringLiteral ("canRelay"), true);
  map.insert (QStringLiteral ("readyToForward"), mailCount > 0);
  map.insert (QStringLiteral ("targetCall"), target);
  map.insert (QStringLiteral ("locator"), locator);
  map.insert (QStringLiteral ("heardAgeMinutes"),
              QVariant::fromValue<qulonglong> (heardAge));
  map.insert (QStringLiteral ("hintAgeMinutes"),
              QVariant::fromValue<qulonglong> (hintAge));
  map.insert (QStringLiteral ("parkedMailboxCount"), mailCount);
  map.insert (QStringLiteral ("mailboxId"),
              mailSummary.value (QStringLiteral ("mailboxId")).toUInt ());
  map.insert (QStringLiteral ("mailboxSubject"), subject);
  map.insert (QStringLiteral ("mailboxState"),
              mailSummary.value (QStringLiteral ("state")).toString ());
  map.insert (QStringLiteral ("urgentCount"), urgentCount);
  map.insert (QStringLiteral ("emcommCount"), emcommCount);
  map.insert (QStringLiteral ("pendingRelayCount"), pendingRelayCount);
  map.insert (QStringLiteral ("priority"), priority);
  map.insert (QStringLiteral ("state"), state);
  map.insert (QStringLiteral ("badge"), badge);
  map.insert (QStringLiteral ("line"), lineParts.join (QStringLiteral (" / ")));
  map.insert (QStringLiteral ("action"),
              QStringLiteral ("CALL_RELAY_THEN_RELAY_MAIL"));
  return map;
}

quint32 FT2LinkQmlAdapter::recordForm (QString const& direction,
                                       QString const& fromCall,
                                       QString const& toCall,
                                       QString const& formType,
                                       QVariantMap const& fields,
                                       QString const& state,
                                       quint64 nowMs)
{
  FormMessage form;
  form.id = m_nextFormId++;
  if (m_nextFormId == 0u)
    {
      m_nextFormId = 1u;
    }
  form.direction = direction;
  form.fromCall = normalizeCallsign (fromCall);
  form.toCall = normalizeCallsign (toCall);
  form.formType = formType.trimmed ().isEmpty ()
      ? QStringLiteral ("ICS213")
      : formType.trimmed ().toUpper ();
  form.fields = fields;
  form.state = state;
  form.atMs = nowMs;
  form.updatedAtMs = nowMs;

  m_forms.push_back (form);
  if (m_forms.size () > 100u)
    {
      m_forms.erase (m_forms.begin ());
    }
  emit formsChanged ();
  QString const peer = form.direction == QStringLiteral ("Incoming")
      ? form.fromCall
      : form.toCall;
  touchContact (peer, nowMs, QStringLiteral ("form"));
  ++m_contactHistory[peer].formCount;
  emit contactHistoryChanged ();
  return form.id;
}

bool FT2LinkQmlAdapter::updateFormState (quint32 formId,
                                         QString const& state,
                                         quint64 nowMs)
{
  if (formId == 0u)
    {
      return false;
    }
  for (FormMessage& form : m_forms)
    {
      if (form.id != formId)
        {
          continue;
        }
      form.state = state;
      form.updatedAtMs = nowMs;
      emit formsChanged ();
      return true;
    }
  return false;
}

quint32 FT2LinkQmlAdapter::recordFileTransfer (QString const& direction,
                                               QString const& fromCall,
                                               QString const& toCall,
                                               QString const& fileName,
                                               QString const& content,
                                               QString const& sha256,
                                               QString const& state,
                                               quint64 nowMs)
{
  FileTransfer transfer;
  transfer.id = m_nextFileTransferId++;
  if (m_nextFileTransferId == 0u)
    {
      m_nextFileTransferId = 1u;
    }
  transfer.direction = direction;
  transfer.fromCall = normalizeCallsign (fromCall);
  transfer.toCall = normalizeCallsign (toCall);
  transfer.fileName = fileName.trimmed ().isEmpty ()
      ? QStringLiteral ("ft2link.txt")
      : fileName.trimmed ();
  transfer.content = content;
  transfer.sha256 = sha256.trimmed ();
  transfer.state = state;
  transfer.atMs = nowMs;
  transfer.updatedAtMs = nowMs;

  m_fileTransfers.push_back (transfer);
  if (m_fileTransfers.size () > 100u)
    {
      m_fileTransfers.erase (m_fileTransfers.begin ());
    }
  emit fileTransfersChanged ();
  QString const peer = transfer.direction == QStringLiteral ("Incoming")
      ? transfer.fromCall
      : transfer.toCall;
  touchContact (peer, nowMs, QStringLiteral ("file"));
  ++m_contactHistory[peer].fileCount;
  emit contactHistoryChanged ();
  return transfer.id;
}

bool FT2LinkQmlAdapter::updateFileTransferState (quint32 transferId,
                                                 QString const& state,
                                                 quint64 nowMs)
{
  if (transferId == 0u)
    {
      return false;
    }
  for (FileTransfer& transfer : m_fileTransfers)
    {
      if (transfer.id != transferId)
        {
          continue;
        }
      transfer.state = state;
      transfer.updatedAtMs = nowMs;
      emit fileTransfersChanged ();
      return true;
    }
  return false;
}

quint32 FT2LinkQmlAdapter::recordBulletin (QString const& direction,
                                           QString const& fromCall,
                                           QString const& group,
                                           QString const& title,
                                           QString const& body,
                                           QString const& state,
                                           quint64 nowMs)
{
  Bulletin bulletin;
  bulletin.id = m_nextBulletinId++;
  if (m_nextBulletinId == 0u)
    {
      m_nextBulletinId = 1u;
    }
  bulletin.direction = direction;
  bulletin.fromCall = normalizeCallsign (fromCall);
  QString normalizedGroup = group.trimmed ().toUpper ();
  bulletin.group = normalizedGroup.isEmpty ()
      ? QStringLiteral ("ALL")
      : normalizedGroup;
  bulletin.title = title.trimmed ().isEmpty ()
      ? QStringLiteral ("Bulletin")
      : title.trimmed ();
  bulletin.body = body.trimmed ();
  bulletin.state = state;
  bulletin.atMs = nowMs;
  bulletin.updatedAtMs = nowMs;
  if (bulletin.body.isEmpty ())
    {
      return 0u;
    }

  m_bulletins.push_back (bulletin);
  if (m_bulletins.size () > 100u)
    {
      m_bulletins.erase (m_bulletins.begin ());
    }
  emit bulletinsChanged ();
  touchContact (bulletin.fromCall, nowMs, QStringLiteral ("bbs"));
  ++m_contactHistory[bulletin.fromCall].bulletinCount;
  emit contactHistoryChanged ();
  return bulletin.id;
}

bool FT2LinkQmlAdapter::updateBulletinState (quint32 bulletinId,
                                             QString const& state,
                                             quint64 nowMs)
{
  if (bulletinId == 0u)
    {
      return false;
    }
  for (Bulletin& bulletin : m_bulletins)
    {
      if (bulletin.id != bulletinId)
        {
          continue;
        }
      bulletin.state = state;
      bulletin.updatedAtMs = nowMs;
      emit bulletinsChanged ();
      return true;
    }
  return false;
}

void FT2LinkQmlAdapter::recordPing (QString const& direction,
                                    QString const& remoteCall,
                                    quint16 token,
                                    QString const& state,
                                    quint64 nowMs,
                                    quint64 rttMs)
{
  QString normalizedCall = normalizeCallsign (remoteCall);
  if (normalizedCall.isEmpty ())
    {
      normalizedCall = QStringLiteral ("UNKNOWN");
    }

  PingRecord ping;
  ping.direction = direction;
  ping.remoteCall = normalizedCall;
  ping.token = token;
  ping.state = state;
  ping.atMs = nowMs;
  ping.rttMs = rttMs;
  m_pingLog.push_back (ping);
  if (m_pingLog.size () > 100u)
    {
      m_pingLog.erase (m_pingLog.begin ());
    }
  emit pingLogChanged ();

  touchContact (normalizedCall, nowMs, QStringLiteral ("ping"));
}

void FT2LinkQmlAdapter::recordPathReport (
    QString const& direction,
    QString const& remoteCall,
    QString const& locator,
    bool snrValid,
    int snrDb,
    bool qualityValid,
    double quality,
    double frequencyOffsetHz,
    QString const& profileName,
    QString const& rateName,
    QString const& source,
    quint64 nowMs)
{
  QString normalizedCall = normalizeCallsign (remoteCall);
  if (normalizedCall.isEmpty ())
    {
      normalizedCall = QStringLiteral ("UNKNOWN");
    }
  if (!snrValid && !qualityValid)
    {
      return;
    }

  PathReport report;
  report.id = m_nextPathReportId++;
  if (m_nextPathReportId == 0u)
    {
      m_nextPathReportId = 1u;
    }
  report.direction = direction.trimmed ().isEmpty ()
      ? QStringLiteral ("Incoming")
      : direction.trimmed ();
  report.remoteCall = normalizedCall;
  report.locator = locator.trimmed ().toUpper ();
  if (report.locator.isEmpty ())
    {
      std::map<QString, ContactHistory>::const_iterator contact =
          m_contactHistory.find (normalizedCall);
      if (contact != m_contactHistory.end ())
        {
          report.locator = contact->second.locator;
        }
    }
  report.snrValid = snrValid;
  report.snrDb = std::max (-99, std::min (99, snrDb));
  report.qualityValid = qualityValid;
  report.quality = quality;
  report.frequencyOffsetHz = frequencyOffsetHz;
  report.profileName = profileName.trimmed ();
  report.rateName = rateName.trimmed ();
  report.source = source.trimmed ().isEmpty ()
      ? QStringLiteral ("TAG")
      : source.trimmed ();
  report.atMs = nowMs;

  m_pathReports.push_back (report);
  if (m_pathReports.size () > 200u)
    {
      m_pathReports.erase (m_pathReports.begin ());
    }
  emit pathReportsChanged ();

  QString const event = report.snrValid
      ? QStringLiteral ("SNR %1 dB").arg (report.snrDb)
      : QStringLiteral ("path metric");
  touchContact (normalizedCall, nowMs, event, report.locator);
}

void FT2LinkQmlAdapter::recordSnrReportsForText (
    quint16 sessionId,
    QString const& direction,
    QString const& text,
    QString const& source,
    quint64 nowMs)
{
  std::vector<int> const reports = snrReportsInText (text);
  if (reports.empty ())
    {
      return;
    }

  AppSession const* session = m_model.session (sessionId);
  QString remoteCall = session
      ? normalizeCallsign (QString::fromStdString (session->remoteCall))
      : QStringLiteral ("UNKNOWN");
  QString locator;
  QString profileName;
  QString rateName;
  if (session)
    {
      profileName = QString::fromStdString (
          decodium::ft2link::profileName (session->negotiated.profile));
      if (session->negotiated.profile == Profile::Wide2300)
        {
          rateName = QString::fromLatin1 (
              decodium::ft2link::w2300RateModeName (
                  session->negotiated.w2300RateMode));
        }
    }
  std::map<QString, ContactHistory>::const_iterator contact =
      m_contactHistory.find (remoteCall);
  if (contact != m_contactHistory.end ())
    {
      locator = contact->second.locator;
    }

  for (int snr : reports)
    {
      recordPathReport (direction,
                        remoteCall,
                        locator,
                        true,
                        snr,
                        false,
                        0.0,
                        0.0,
                        profileName,
                        rateName,
                        source,
                        nowMs);
    }
}

bool FT2LinkQmlAdapter::appendSystemText (quint16 sessionId,
                                          QString const& text,
                                          quint64 nowMs)
{
  QString const trimmed = text.trimmed ();
  if (trimmed.isEmpty ())
    {
      return true;
    }

  std::string error;
  QByteArray const bytes = trimmed.toUtf8 ();
  if (!m_model.appendSystemText (
          sessionId,
          std::string (bytes.constData (),
                       static_cast<std::size_t> (bytes.size ())),
          nowMs,
          &error))
    {
      setLastError (QString::fromStdString (error));
      return false;
    }

  emit messagesChanged (sessionId);
  emit sessionsChanged ();
  return true;
}

QString FT2LinkQmlAdapter::localPresenceMessage (quint16 sessionId,
                                                 quint64 nowMs) const
{
  if (m_awayEnabled)
    {
      QString const tag = m_awayAcceptsQsy
          ? QStringLiteral ("<AWQ>")
          : QStringLiteral ("<AWAY>");
      QString const body = expandCannedMessage (
          m_awayMessage, sessionId, nowMs).simplified ();
      return body.isEmpty ()
          ? tag
          : QStringLiteral ("%1 %2").arg (tag, body).simplified ();
    }

  if (!m_welcomeEnabled)
    {
      return {};
    }

  return expandCannedMessage (
      m_welcomeMessage, sessionId, nowMs).simplified ();
}

bool FT2LinkQmlAdapter::queuePresenceMessage (quint16 sessionId, quint64 nowMs)
{
  QString const text = localPresenceMessage (sessionId, nowMs);
  if (text.trimmed ().isEmpty ())
    {
      return true;
    }

  std::string error;
  QByteArray const bytes = text.toUtf8 ();
  if (!m_model.queueOutgoingText (
          sessionId,
          std::string (bytes.constData (),
                       static_cast<std::size_t> (bytes.size ())),
          nowMs,
          &error))
    {
      setLastError (QString::fromStdString (error));
      return false;
    }

  emit messagesChanged (sessionId);
  emit sessionsChanged ();
  recordQsoSession (
      sessionId,
      nowMs,
      m_awayEnabled ? QStringLiteral ("away queued")
                    : QStringLiteral ("welcome queued"));
  return true;
}

bool FT2LinkQmlAdapter::queueSuggestedReplies (
    quint16 sessionId,
    QStringList const& replies,
    quint64 nowMs)
{
  if (!m_autoReplyEnabled)
    {
      return true;
    }

  QStringList uniqueReplies;
  for (QString const& reply : replies)
    {
      QString const clean = reply.simplified ();
      if (!clean.isEmpty () && !uniqueReplies.contains (clean))
        {
          uniqueReplies.push_back (clean);
        }
      if (uniqueReplies.size () >= 8)
        {
          break;
        }
    }
  if (uniqueReplies.isEmpty ())
    {
      return true;
    }

  QString text = uniqueReplies.join (QStringLiteral (" "));
  if (text.size () > 900)
    {
      text = text.left (900).trimmed ();
    }

  std::string error;
  QByteArray const bytes = text.toUtf8 ();
  if (!m_model.queueOutgoingText (
          sessionId,
          std::string (bytes.constData (),
                       static_cast<std::size_t> (bytes.size ())),
          nowMs,
          &error))
    {
      setLastError (QString::fromStdString (error));
      return false;
    }

  emit messagesChanged (sessionId);
  emit sessionsChanged ();
  recordQsoSession (
      sessionId, nowMs, QStringLiteral ("auto-reply queued"));
  return true;
}

QString FT2LinkQmlAdapter::lastHeardTagReply (quint64 nowMs) const
{
  if (!m_lastHeardPeekingEnabled)
    {
      return QStringLiteral ("<LHJ>");
    }

  QStringList calls;
  std::vector<StationAdvertisement> active =
      m_model.activeStations (nowMs, 300000u, false);
  QString const localCall = normalizeCallsign (
      QString::fromStdString (m_model.localStation ().call));
  for (StationAdvertisement const& station : active)
    {
      QString const call = normalizeCallsign (
          QString::fromStdString (station.station.call));
      if (call.isEmpty () || call == localCall || calls.contains (call)
          || isCallBlocked (call))
        {
          continue;
        }
      calls.push_back (call);
      if (calls.size () >= 8)
        {
          break;
        }
    }

  return calls.isEmpty ()
      ? QStringLiteral ("<LHE>")
      : QStringLiteral ("<LH:%1>").arg (calls.join (QStringLiteral (",")));
}

QString FT2LinkQmlAdapter::lastHeardSpecificTagReply (
    QString const& call,
    quint64 nowMs) const
{
  if (!m_lastHeardPeekingEnabled)
    {
      return QStringLiteral ("<LHJ>");
    }

  QString const wanted = normalizeCallsign (call);
  if (wanted.isEmpty () || isCallBlocked (wanted))
    {
      return QStringLiteral ("<LHCE>");
    }

  std::map<QString, ContactHistory>::const_iterator const contact =
      m_contactHistory.find (wanted);
  if (contact == m_contactHistory.end () || contact->second.lastHeardMs == 0u)
    {
      return QStringLiteral ("<LHCE>");
    }

  quint64 const ageMs = nowMs > contact->second.lastHeardMs
      ? nowMs - contact->second.lastHeardMs
      : 0u;
  QString locator = contact->second.locator.trimmed ().toUpper ();
  locator.replace (QLatin1Char ('|'), QLatin1Char ('_'));
  locator.replace (QLatin1Char ('>'), QLatin1Char ('_'));
  return QStringLiteral ("<LHC:%1|%2|%3m>")
      .arg (wanted,
            locator.isEmpty () ? QStringLiteral ("--") : locator,
            QString::number (ageMs / 60000u));
}

bool FT2LinkQmlAdapter::rejectBlockedHello (
    QString const& remoteCall,
    Frame const& hello,
    quint64 nowMs,
    Frame* helloAck,
    QString* resolvedRemoteCall)
{
  LinkCapabilities remoteCapabilities;
  decodium::ft2link::HandshakeIdentity remoteIdentity;
  std::string parseError;
  if (!decodium::ft2link::parseHelloFrame (
          hello, &remoteCapabilities, &remoteIdentity, &parseError))
    {
      setLastError (QString::fromStdString (parseError));
      return false;
    }

  QString resolved = normalizeCallsign (remoteCall);
  if (resolved.isEmpty ())
    {
      resolved = normalizeCallsign (
          QString::fromStdString (remoteIdentity.call));
    }
  if (resolvedRemoteCall)
    {
      *resolvedRemoteCall = resolved;
    }
  if (!isCallBlocked (resolved))
    {
      return false;
    }

  std::string error;
  if (!m_model.rejectHello (
          toStdString (resolved),
          hello,
          nowMs,
          helloAck,
          "FT2-Link callsign is blocked",
          &error))
    {
      setLastError (QString::fromStdString (error));
      return false;
    }
  return true;
}

QString FT2LinkQmlAdapter::frequencyScheduleTagReply () const
{
  FrequencyScheduleEntry const* active = activeFrequencyScheduleEntry (
      static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ()));
  if (active)
    {
      QStringList parts;
      QString const label = active->label.trimmed ().isEmpty ()
          ? active->action.trimmed ()
          : active->label.trimmed ();
      parts.push_back (label.isEmpty () ? QStringLiteral ("QRV") : label);
      parts.push_back (QString::number (active->dialFrequencyHz));
      parts.push_back (sanitizedScheduleAction (active->action));
      QString const cq = sanitizedCqType (active->cqType);
      if (!cq.isEmpty () && cq != QStringLiteral ("CQ"))
        {
          parts.push_back (cq);
        }
      return QStringLiteral ("<FS:%1>").arg (
          parts.join (QStringLiteral (" ")));
    }

  if (m_frequencyPresets.empty ())
    {
      return QStringLiteral ("<FSO>");
    }

  FrequencyPreset const& preset = m_frequencyPresets.front ();
  QStringList parts;
  parts.push_back (preset.band.trimmed ().isEmpty ()
                   ? QStringLiteral ("QRV")
                   : preset.band.trimmed ());
  parts.push_back (QString::number (preset.dialFrequencyHz));
  if (!preset.label.trimmed ().isEmpty ())
    {
      parts.push_back (preset.label.trimmed ());
    }
  return QStringLiteral ("<FS:%1>").arg (parts.join (QStringLiteral (" ")));
}

QString FT2LinkQmlAdapter::parkedMailboxTagReply (
    QString const& remoteCall) const
{
  if (!m_parkedVmailPeekingEnabled)
    {
      return QStringLiteral ("<VRPJ>");
    }

  QString const target = normalizeCallsign (remoteCall);
  if (target.isEmpty ())
    {
      return QStringLiteral ("<VRPJ>");
    }

  int waiting = 0;
  for (MailboxMessage const& message : m_mailbox)
    {
      if (message.toCall == target
          && (message.state == QStringLiteral ("Parked")
              || message.state == QStringLiteral ("Relay ready")
              || message.state == QStringLiteral ("Pending relay")))
        {
          ++waiting;
        }
    }

  return waiting > 0
      ? QStringLiteral ("<VW> %1 parked VMail waiting").arg (waiting)
      : QStringLiteral ("<VRPJ>");
}

QString FT2LinkQmlAdapter::lastConnectionsTagReply () const
{
  if (!m_lastConnectionsPeekingEnabled)
    {
      return QStringLiteral ("<LCJ>");
    }

  std::vector<QsoLogEntry const*> entries;
  for (std::map<quint16, QsoLogEntry>::const_iterator it = m_qsoLog.begin ();
       it != m_qsoLog.end ();
       ++it)
    {
      if (!it->second.remoteCall.trimmed ().isEmpty ())
        {
          entries.push_back (&it->second);
        }
    }
  std::sort (entries.begin (), entries.end (),
             [] (QsoLogEntry const* lhs, QsoLogEntry const* rhs) {
    return lhs->updatedAtMs > rhs->updatedAtMs;
  });

  QStringList calls;
  for (QsoLogEntry const* entry : entries)
    {
      QString const call = normalizeCallsign (entry->remoteCall);
      if (call.isEmpty () || calls.contains (call))
        {
          continue;
        }
      calls.push_back (call);
      if (calls.size () >= 5)
        {
          break;
        }
    }

  return calls.isEmpty ()
      ? QStringLiteral ("<LCJ>")
      : QStringLiteral ("<LC:%1>").arg (calls.join (QStringLiteral (",")));
}

QString FT2LinkQmlAdapter::bbsFileListReply (quint64 nowMs) const
{
  Q_UNUSED (nowMs);

  QStringList seen;
  QStringList tags;
  for (std::vector<FileTransfer>::const_reverse_iterator it =
           m_fileTransfers.rbegin ();
       it != m_fileTransfers.rend ();
       ++it)
    {
      if (it->content.trimmed ().isEmpty ())
        {
          continue;
        }
      QString safeName = it->fileName.trimmed ();
      safeName.replace (QLatin1Char ('|'), QLatin1Char ('_'));
      safeName.replace (QLatin1Char ('>'), QLatin1Char ('_'));
      if (safeName.isEmpty ())
        {
          continue;
        }
      QString const key = safeName.toLower ();
      if (seen.contains (key))
        {
          continue;
        }
      seen.push_back (key);

      QString date = QDateTime::fromMSecsSinceEpoch (
          static_cast<qint64> (it->atMs), QTimeZone(QByteArrayLiteral("UTC")))
          .date ().toString (Qt::ISODate);
      if (date.isEmpty ())
        {
          date = QStringLiteral ("1970-01-01");
        }
      int const size = it->content.toUtf8 ().size ();
      tags.push_back (QStringLiteral ("<BL:%1|%2|%3>")
                      .arg (safeName, date, QString::number (size)));
      if (tags.size () >= 5)
        {
          break;
        }
    }

  return tags.isEmpty () ? QStringLiteral ("<BLJ>")
                         : tags.join (QStringLiteral (" "));
}

bool FT2LinkQmlAdapter::bbsFileAvailable (QString const& fileName) const
{
  QString const wanted = fileName.trimmed ().toLower ();
  if (wanted.isEmpty ())
    {
      return false;
    }
  for (FileTransfer const& transfer : m_fileTransfers)
    {
      if (transfer.fileName.trimmed ().toLower () == wanted
          && !transfer.content.trimmed ().isEmpty ())
        {
          return true;
        }
    }
  return false;
}

void FT2LinkQmlAdapter::setTypingPeer (QString const& call,
                                       bool typing,
                                       quint64 nowMs)
{
  QString const normalizedCall = normalizeCallsign (call);
  if (normalizedCall.isEmpty ())
    {
      return;
    }

  constexpr quint64 kTypingIndicatorTtlMs = 12000u;
  int const before = static_cast<int> (m_typingPeers.size ());
  if (typing)
    {
      m_typingPeers[normalizedCall] = nowMs + kTypingIndicatorTtlMs;
    }
  else
    {
      m_typingPeers.erase (normalizedCall);
    }
  if (before != static_cast<int> (m_typingPeers.size ()) || typing)
    {
      emit typingIndicatorsChanged ();
    }
}

bool FT2LinkQmlAdapter::expireTypingIndicators (quint64 nowMs)
{
  bool changed = false;
  for (std::map<QString, quint64>::iterator it = m_typingPeers.begin ();
       it != m_typingPeers.end ();)
    {
      if (nowMs >= it->second)
        {
          it = m_typingPeers.erase (it);
          changed = true;
        }
      else
        {
          ++it;
        }
    }
  if (changed)
    {
      emit typingIndicatorsChanged ();
    }
  return changed;
}

bool FT2LinkQmlAdapter::handleIncomingControlTags (
    quint16 sessionId,
    QString const& text,
    quint64 nowMs,
    bool* disconnectRequested)
{
  if (disconnectRequested)
    {
      *disconnectRequested = false;
    }

  QString const trimmed = text.trimmed ();
  if (trimmed.isEmpty ())
    {
      return true;
    }

  AppSession const* session = m_model.session (sessionId);
  QString const remoteCall = session
      ? normalizeCallsign (QString::fromStdString (session->remoteCall))
      : QStringLiteral ("REMOTE");
  QString const displayCall = remoteCall.isEmpty ()
      ? QStringLiteral ("REMOTE")
      : remoteCall;

  QStringList notices;
  QStringList autoReplies;
  auto addAutoReply = [&autoReplies] (QString const& text) {
    QString const clean = text.simplified ();
    if (!clean.isEmpty () && !autoReplies.contains (clean))
      {
        autoReplies.push_back (clean);
      }
  };
  QString const reportedName = firstControlTagValue (
      trimmed, QStringLiteral ("NAME"));
  QString const reportedLocator = firstControlTagValue (
      trimmed, QStringLiteral ("LOC")).toUpper ();
  QString const reportedQth = firstControlTagValue (
      trimmed, QStringLiteral ("QTH"));
  QString const reportedEmail = firstControlTagValue (
      trimmed, QStringLiteral ("EM"));
  QString const reportedRig = firstControlTagValue (
      trimmed, QStringLiteral ("RIG"));
  QString const reportedAntenna = firstControlTagValue (
      trimmed, QStringLiteral ("ANT"));
  QString const reportedPower = firstControlTagValue (
      trimmed, QStringLiteral ("PWR"));
  QString const reportedIce = firstControlTagValue (
      trimmed, QStringLiteral ("ICE"));
  QString const reportedGps = firstControlTagValue (
      trimmed, QStringLiteral ("GPS"));
  if (!reportedName.isEmpty () || !reportedLocator.isEmpty ())
    {
      touchContact (displayCall,
                    nowMs,
                    QStringLiteral ("tag info"),
                    reportedLocator,
                    reportedName);
      QStringList fields;
      if (!reportedName.isEmpty ())
        {
          fields.push_back (QStringLiteral ("name %1").arg (reportedName));
        }
      if (!reportedLocator.isEmpty ())
        {
          fields.push_back (QStringLiteral ("locator %1").arg (
              reportedLocator));
        }
      notices.push_back (QStringLiteral ("TAG %1 reported %2")
                         .arg (displayCall, fields.join (
                             QStringLiteral (", "))));
    }
  if (!reportedQth.isEmpty ()
      || !reportedEmail.isEmpty ()
      || !reportedRig.isEmpty ()
      || !reportedAntenna.isEmpty ()
      || !reportedPower.isEmpty ()
      || !reportedIce.isEmpty ()
      || !reportedGps.isEmpty ())
    {
      QStringList fields;
      if (!reportedQth.isEmpty ())
        {
          fields.push_back (QStringLiteral ("QTH %1").arg (reportedQth));
        }
      if (!reportedEmail.isEmpty ())
        {
          fields.push_back (QStringLiteral ("email %1").arg (reportedEmail));
        }
      if (!reportedRig.isEmpty ())
        {
          fields.push_back (QStringLiteral ("rig %1").arg (reportedRig));
        }
      if (!reportedAntenna.isEmpty ())
        {
          fields.push_back (QStringLiteral ("antenna %1").arg (reportedAntenna));
        }
      if (!reportedPower.isEmpty ())
        {
          fields.push_back (QStringLiteral ("power %1").arg (reportedPower));
        }
      if (!reportedIce.isEmpty ())
        {
          fields.push_back (QStringLiteral ("ice %1").arg (reportedIce));
        }
      if (!reportedGps.isEmpty ())
        {
          fields.push_back (QStringLiteral ("GPS %1").arg (reportedGps));
        }
      notices.push_back (QStringLiteral ("TAG %1 profile %2")
                         .arg (displayCall, fields.join (
                             QStringLiteral (", "))));
    }

  QString const snr = firstControlTagMatch (
      trimmed, QStringLiteral ("<R\\s*([+-]?\\d{1,2})>"));
  if (!snr.isEmpty ())
    {
      recordSnrReportsForText (sessionId,
                               QStringLiteral ("Incoming"),
                               trimmed,
                               QStringLiteral ("TAG"),
                               nowMs);
      notices.push_back (QStringLiteral ("TAG %1 reports SNR %2 dB")
                         .arg (displayCall, snr));
    }

  if (containsControlTag (trimmed, QStringLiteral ("<AWQ>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 is away and accepts QSY invitations").arg (displayCall));
    }
  else if (containsControlTag (trimmed, QStringLiteral ("<AWAY>")))
    {
      notices.push_back (QStringLiteral ("TAG %1 is away").arg (displayCall));
    }

  if (containsControlTag (trimmed, QStringLiteral ("<TYP0>")))
    {
      setTypingPeer (displayCall, false, nowMs);
      notices.push_back (QStringLiteral (
          "TAG %1 stopped typing").arg (displayCall));
    }
  else if (containsControlTag (trimmed, QStringLiteral ("<TYP>")))
    {
      setTypingPeer (displayCall, true, nowMs);
      notices.push_back (QStringLiteral (
          "TAG %1 is typing").arg (displayCall));
    }

  QStringList gestures;
  if (containsPlainCommand (trimmed, QStringLiteral ("HIHI!")))
    {
      gestures.push_back (QStringLiteral ("HIHI"));
    }
  if (containsPlainCommand (trimmed, QStringLiteral ("TU!")))
    {
      gestures.push_back (QStringLiteral ("TU"));
    }
  if (containsPlainCommand (trimmed, QStringLiteral ("LIKE!")))
    {
      gestures.push_back (QStringLiteral ("LIKE"));
    }
  if (containsPlainCommand (trimmed, QStringLiteral ("BYE!")))
    {
      gestures.push_back (QStringLiteral ("BYE"));
    }
  if (containsPlainCommand (trimmed, QStringLiteral ("COOL!")))
    {
      gestures.push_back (QStringLiteral ("COOL"));
    }
  if (containsPlainCommand (trimmed, QStringLiteral ("FB!")))
    {
      gestures.push_back (QStringLiteral ("FB"));
    }
  if (!gestures.isEmpty ())
    {
      notices.push_back (QStringLiteral ("TAG %1 gesture %2")
                         .arg (displayCall, gestures.join (
                             QStringLiteral (", "))));
    }

  QStringList sounds;
  if (containsPlainCommand (trimmed, QStringLiteral ("DING")))
    {
      sounds.push_back (QStringLiteral ("DING"));
    }
  if (containsPlainCommand (trimmed, QStringLiteral ("RING")))
    {
      sounds.push_back (QStringLiteral ("RING"));
    }
  if (containsPlainCommand (trimmed, QStringLiteral ("HIHIW!")))
    {
      sounds.push_back (QStringLiteral ("HIHIW"));
    }
  if (containsPlainCommand (trimmed, QStringLiteral ("HIHIM!")))
    {
      sounds.push_back (QStringLiteral ("HIHIM"));
    }
  if (!sounds.isEmpty ())
    {
      notices.push_back (QStringLiteral (
          "TAG %1 sound cue %2; no local sound played automatically")
                         .arg (displayCall, sounds.join (
                             QStringLiteral (", "))));
    }

  if (containsControlTag (trimmed, QStringLiteral ("<Q>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 accepts a QSY invitation").arg (displayCall));
    }

  if (containsControlTag (trimmed, QStringLiteral ("<QSYU>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 invites QSY up 750 Hz; verify slot before changing frequency")
                         .arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<QSYD>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 invites QSY down 750 Hz; verify slot before changing frequency")
                         .arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<QSYR>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 accepted QSY invitation").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<QSYJ>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 rejected QSY invitation").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<QJO>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 rejected QSO or QSY due to allowed range/state").arg (displayCall));
    }

  QString const qsyOffset = firstControlTagValue (
      trimmed, QStringLiteral ("Q"));
  if (!qsyOffset.isEmpty ())
    {
      notices.push_back (QStringLiteral (
          "TAG %1 invites QSY offset %2; verify band plan and slot first")
                         .arg (displayCall, qsyOffset));
    }
  QString const qsyFrequency = firstControlTagValue (
      trimmed, QStringLiteral ("QF"));
  if (!qsyFrequency.isEmpty ())
    {
      notices.push_back (QStringLiteral (
          "TAG %1 invites QSY frequency %2; verify band plan and slot first")
                         .arg (displayCall, qsyFrequency));
    }

  if (containsControlTag (trimmed, QStringLiteral ("<SR>")))
    {
      if (m_snrReportSendingEnabled)
        {
          notices.push_back (QStringLiteral (
              "TAG %1 requested SNR; suggested reply: <R+00> after checking RX metrics")
                             .arg (displayCall));
        }
      else
        {
          notices.push_back (QStringLiteral (
              "TAG %1 requested SNR; SNR report sending is disabled")
                             .arg (displayCall));
        }
    }
  if (containsControlTag (trimmed, QStringLiteral ("<INFO>")))
    {
      if (m_infoInquireEnabled)
        {
          QString const reply = expandCannedMessage (
              QStringLiteral (
                  "<NAME:<NAME>> <QTH:<QTH>> <LOC:<MYGRID>> <RIG:<RIG>> <ANT:<ANT>> <PWR:<PWR>>"),
              sessionId,
              nowMs);
          notices.push_back (QStringLiteral (
              "TAG %1 requested info; suggested reply: %2")
                             .arg (displayCall, reply));
          addAutoReply (reply);
        }
      else
        {
          notices.push_back (QStringLiteral (
              "TAG %1 requested info; information inquiries are disabled")
                             .arg (displayCall));
        }
    }
  if (containsControlTag (trimmed, QStringLiteral ("<LOCR>")))
    {
      if (m_infoInquireEnabled)
        {
          QString const reply = QStringLiteral ("<LOC:%1>").arg (
              QString::fromStdString (m_model.localStation ().locator));
          notices.push_back (QStringLiteral (
              "TAG %1 requested locator; suggested reply: <LOC:%2>")
                             .arg (displayCall,
                                   QString::fromStdString (
                                       m_model.localStation ().locator)));
          addAutoReply (reply);
        }
      else
        {
          notices.push_back (QStringLiteral (
              "TAG %1 requested locator; information inquiries are disabled")
                             .arg (displayCall));
        }
    }
  if (containsControlTag (trimmed, QStringLiteral ("<LHR>")))
    {
      QString const reply = lastHeardTagReply (nowMs);
      notices.push_back (QStringLiteral (
          "TAG %1 requested last-heard; suggested reply: %2")
                         .arg (displayCall, reply));
      addAutoReply (reply);
    }
  for (QString const& lastHeardCall : controlTagValues (
           trimmed, QStringLiteral ("LHC")))
    {
      QString const cleanValue = lastHeardCall.trimmed ();
      if (cleanValue.contains (QLatin1Char ('|')))
        {
          notices.push_back (QStringLiteral (
              "TAG %1 last-heard detail %2")
                             .arg (displayCall, cleanValue));
        }
      else
        {
          QString const reply = lastHeardSpecificTagReply (cleanValue, nowMs);
          notices.push_back (QStringLiteral (
              "TAG %1 requested last-heard for %2; suggested reply: %3")
                             .arg (displayCall,
                                   normalizeCallsign (cleanValue),
                                   reply));
          addAutoReply (reply);
        }
    }
  for (QString const& lastHeardList : controlTagValues (
           trimmed, QStringLiteral ("LH")))
    {
      notices.push_back (QStringLiteral ("TAG %1 last-heard list %2")
                         .arg (displayCall, lastHeardList));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<LHE>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 has no last-heard stations").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<LHJ>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 rejected last-heard request").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<LHCE>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 has no last-heard data for that callsign").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<FSR>")))
    {
      QString const reply = frequencyScheduleTagReply ();
      notices.push_back (QStringLiteral (
          "TAG %1 requested frequency schedule; suggested reply: %2")
                         .arg (displayCall, reply));
      addAutoReply (reply);
    }
  QString const schedule = firstControlTagValue (
      trimmed, QStringLiteral ("FS"));
  if (!schedule.isEmpty ())
    {
      notices.push_back (QStringLiteral (
          "TAG %1 frequency schedule %2").arg (displayCall, schedule));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<FSO>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 reports frequency scheduler is off").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<VRP>")))
    {
      QString const reply = parkedMailboxTagReply (displayCall);
      notices.push_back (QStringLiteral (
          "TAG %1 requested parked VMail peek; suggested reply: %2")
                         .arg (displayCall, reply));
      addAutoReply (reply);
    }
  if (containsControlTag (trimmed, QStringLiteral ("<VRPJ>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 rejected parked VMail peek").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<VW>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 reports waiting VMail; QSY/session may be needed").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<LCR>")))
    {
      QString const reply = lastConnectionsTagReply ();
      notices.push_back (QStringLiteral (
          "TAG %1 requested recent connections; suggested reply: %2")
                         .arg (displayCall, reply));
      addAutoReply (reply);
    }
  QString const recentConnections = firstControlTagValue (
      trimmed, QStringLiteral ("LC"));
  if (!recentConnections.isEmpty ())
    {
      notices.push_back (QStringLiteral (
          "TAG %1 recent connections %2").arg (displayCall, recentConnections));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<LCJ>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 rejected recent-connections request").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<GPSR>")))
    {
      if (m_infoInquireEnabled)
        {
          QString const gps = m_localProfile.gps.trimmed ();
          notices.push_back (gps.isEmpty ()
              ? QStringLiteral (
                  "TAG %1 requested GPS; no local GPS profile configured")
                  .arg (displayCall)
              : QStringLiteral (
                  "TAG %1 requested GPS; suggested reply: <GPS:%2>")
                  .arg (displayCall, gps));
          if (!gps.isEmpty ())
            {
              addAutoReply (QStringLiteral ("<GPS:%1>").arg (gps));
            }
        }
      else
        {
          notices.push_back (QStringLiteral (
              "TAG %1 requested GPS; information inquiries are disabled")
                             .arg (displayCall));
        }
    }
  if (containsControlTag (trimmed, QStringLiteral ("<VER>")))
    {
      QString const reply = expandCannedMessage (
          QStringLiteral ("FT2-Link Decodium v0.1 <PROFILE> <RATE>"),
          sessionId,
          nowMs);
      notices.push_back (QStringLiteral (
          "TAG %1 requested version; suggested reply: %2")
                         .arg (displayCall, reply));
      addAutoReply (reply);
    }

  if (containsControlTag (trimmed, QStringLiteral ("<VSI>")))
    {
      if (m_verboseSnrAutoAcceptEnabled)
        {
          notices.push_back (QStringLiteral (
              "TAG %1 invites verbose SNR mode; auto-accept reply: <VSIR>")
                             .arg (displayCall));
          addAutoReply (QStringLiteral ("<VSIR>"));
        }
      else
        {
          notices.push_back (QStringLiteral (
              "TAG %1 invites verbose SNR mode; reply <VSIR> or <VSIJ> manually")
                             .arg (displayCall));
        }
    }
  if (containsControlTag (trimmed, QStringLiteral ("<VSIR>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 accepted verbose SNR mode").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<VSIJ>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 rejected verbose SNR mode").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<VSS>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 exited verbose SNR mode").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<IE>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 reports idle timeout and may disconnect").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<AE>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 reports away idle timeout and may disconnect").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<TL>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 sent test-link marker; reply manually if copied").arg (displayCall));
    }

  QRegularExpression const aiCommandExpression (
      QStringLiteral ("(^|\\s)AI\\s*:"),
      QRegularExpression::CaseInsensitiveOption);
  if (trimmed.contains (aiCommandExpression))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 sent AI gateway request; FT2-Link does not auto-run AI")
                         .arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<DISAI>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 closed AI gateway mode").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<A>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 advertises AI gateway availability").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<EA>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 advertises email and AI gateway availability").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<ACIE>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 sent AI gateway response").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<AIJ>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 rejected AI gateway request").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<AIE>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 reports AI gateway error").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<AIL>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 reports AI gateway limit exceeded").arg (displayCall));
    }

  QString const fullCallsign = firstControlTagValue (
      trimmed, QStringLiteral ("FC"));
  if (!fullCallsign.isEmpty ())
    {
      notices.push_back (QStringLiteral (
          "TAG %1 full callsign %2").arg (displayCall, fullCallsign));
    }

  for (QString const& fileHeader : controlTagValues (
           trimmed, QStringLiteral ("SF")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 legacy send-file header %2; FT2-Link native file transfer uses FILE TX")
                         .arg (displayCall, fileHeader));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<SFRD>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 is ready to receive a legacy file").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<SFOK>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 reports legacy file received").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<SFFA>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 reports legacy file receive failed").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<SFAB>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 rejected legacy file transfer").arg (displayCall));
    }
  QStringList const legacyFilePackets = controlTagValues (
      trimmed, QStringLiteral ("SFB"));
  if (containsControlTag (trimmed, QStringLiteral ("<SFB>"))
      || !legacyFilePackets.isEmpty ())
    {
      notices.push_back (QStringLiteral (
          "TAG %1 legacy file data packet ignored by FT2-Link native file layer")
                         .arg (displayCall));
    }

  if (containsControlTag (trimmed, QStringLiteral ("<SM>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 legacy VMail header detected").arg (displayCall));
    }
  QString const legacyMailTo = firstControlTagValue (
      trimmed, QStringLiteral ("TO"));
  QString const legacyMailFrom = firstControlTagValue (
      trimmed, QStringLiteral ("FRM"));
  QString const legacyMailTime = firstControlTagValue (
      trimmed, QStringLiteral ("TME"));
  QString const legacyMailSubject = firstControlTagValue (
      trimmed, QStringLiteral ("SBJ"));
  QString const legacyMailBody = firstControlTagValue (
      trimmed, QStringLiteral ("MSG"));
  if (!legacyMailTo.isEmpty ()
      || !legacyMailFrom.isEmpty ()
      || !legacyMailSubject.isEmpty ()
      || !legacyMailBody.isEmpty ()
      || !legacyMailTime.isEmpty ())
    {
      QStringList parts;
      if (!legacyMailTo.isEmpty ())
        {
          parts.push_back (QStringLiteral ("to %1").arg (legacyMailTo));
        }
      if (!legacyMailFrom.isEmpty ())
        {
          parts.push_back (QStringLiteral ("from %1").arg (legacyMailFrom));
        }
      if (!legacyMailSubject.isEmpty ())
        {
          parts.push_back (QStringLiteral ("subject %1").arg (legacyMailSubject));
        }
      if (!legacyMailTime.isEmpty ())
        {
          parts.push_back (QStringLiteral ("time %1").arg (legacyMailTime));
        }
      if (!legacyMailBody.isEmpty ())
        {
          parts.push_back (QStringLiteral ("body %1 chars")
                           .arg (legacyMailBody.size ()));
        }
      notices.push_back (QStringLiteral (
          "TAG %1 legacy VMail fields %2")
                         .arg (displayCall,
                               parts.join (QStringLiteral (", "))));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<EG>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 legacy VMail requests email gateway relay").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<EJ>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 rejected email gateway relay").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<U>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 marks legacy VMail urgent").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<SMR>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 reports legacy VMail received").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<SMF>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 reports legacy VMail failed").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<SMFP>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 does not accept parked legacy VMail").arg (displayCall));
    }

  for (QString const& game : controlTagValues (trimmed, QStringLiteral ("P")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 invites HamPlay %2; no game engine is active, reply <PJ> manually")
                         .arg (displayCall, game));
    }
  for (QString const& move : controlTagValues (
           trimmed, QStringLiteral ("PM")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 HamPlay move %2 ignored").arg (displayCall, move));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<PA>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 aborted HamPlay").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<PE>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 invites HamPlay rematch; no game engine is active")
                         .arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<PJ>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 rejected HamPlay invitation").arg (displayCall));
    }
  if (containsControlTag (trimmed, QStringLiteral ("<PR>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 accepted HamPlay invitation").arg (displayCall));
    }

  if (containsControlTag (trimmed, QStringLiteral ("<BLR>")))
    {
      QString const reply = bbsFileListReply (nowMs);
      notices.push_back (QStringLiteral (
          "TAG %1 requested BBS file list; suggested reply: %2")
                         .arg (displayCall, reply));
      addAutoReply (reply);
    }
  for (QString const& listing : controlTagValues (
           trimmed, QStringLiteral ("BL")))
    {
      QStringList const parts = listing.split (QLatin1Char ('|'));
      if (parts.size () >= 3)
        {
          notices.push_back (QStringLiteral (
              "TAG %1 BBS file %2 date %3 size %4 bytes")
                             .arg (displayCall,
                                   parts[0].trimmed (),
                                   parts[1].trimmed (),
                                   parts[2].trimmed ()));
        }
      else
        {
          notices.push_back (QStringLiteral ("TAG %1 BBS listing %2")
                             .arg (displayCall, listing));
        }
    }
  if (containsControlTag (trimmed, QStringLiteral ("<BLJ>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 rejected BBS file list request").arg (displayCall));
    }
  for (QString const& requestedFile : controlTagValues (
           trimmed, QStringLiteral ("BG")))
    {
      QString const fileName = requestedFile.trimmed ();
      if (bbsFileAvailable (fileName))
        {
          notices.push_back (QStringLiteral (
              "TAG %1 requested BBS file %2; available locally, use FILE TX or reply <BGJ>")
                             .arg (displayCall, fileName));
        }
      else
        {
          notices.push_back (QStringLiteral (
              "TAG %1 requested BBS file %2; not found, suggested reply: <BGJ>")
                             .arg (displayCall, fileName));
          addAutoReply (QStringLiteral ("<BGJ>"));
        }
    }
  if (containsControlTag (trimmed, QStringLiteral ("<BGJ>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 rejected BBS file download request").arg (displayCall));
    }

  if (containsControlTag (trimmed, QStringLiteral ("<DISC>")))
    {
      notices.push_back (QStringLiteral (
          "TAG %1 requested disconnect; session will close after delivery")
                         .arg (displayCall));
      if (disconnectRequested)
        {
          *disconnectRequested = true;
        }
    }

  if (!queueSuggestedReplies (sessionId, autoReplies, nowMs))
    {
      return false;
    }
  if (m_autoReplyEnabled && !autoReplies.isEmpty ())
    {
      notices.push_back (QStringLiteral (
          "AUTO REPLY queued %1 item(s), pending manual RF TX")
                         .arg (std::min<int> (
                             static_cast<int> (autoReplies.size ()), 8)));
    }

  for (QString const& notice : notices)
    {
      if (!appendSystemText (sessionId, notice, nowMs))
        {
          return false;
        }
    }
  if (!notices.isEmpty ())
    {
      recordQsoSession (sessionId, nowMs, QStringLiteral ("tag event"));
  }
  return true;
}

void FT2LinkQmlAdapter::touchContact (QString const& call,
                                      quint64 nowMs,
                                      QString const& event,
                                      QString const& locator,
                                      QString const& name,
                                      QString const& profileName)
{
  QString const normalizedCall = normalizeCallsign (call);
  if (normalizedCall.isEmpty ())
    {
      return;
    }
  if (isCallBlocked (normalizedCall))
    {
      return;
    }

  ContactHistory& contact = m_contactHistory[normalizedCall];
  bool const isNew = contact.call.isEmpty ();
  if (isNew)
    {
      contact.call = normalizedCall;
      contact.firstHeardMs = nowMs;
    }
  if (!locator.trimmed ().isEmpty ())
    {
      contact.locator = locator.trimmed ().toUpper ();
    }
  if (!name.trimmed ().isEmpty ())
    {
      contact.name = name.trimmed ();
    }
  if (!profileName.trimmed ().isEmpty ())
    {
      contact.lastProfileName = profileName.trimmed ();
    }
  contact.lastEvent = event.trimmed ().isEmpty ()
      ? QStringLiteral ("heard")
      : event.trimmed ();
  contact.lastHeardMs = nowMs;
  emit contactHistoryChanged ();
}

void FT2LinkQmlAdapter::recordQsoSession (quint16 sessionId,
                                          quint64 nowMs,
                                          QString const& event)
{
  AppSession const* session = m_model.session (sessionId);
  if (!session)
    {
      return;
    }
  QString const remoteCall = normalizeCallsign (
      QString::fromStdString (session->remoteCall));
  if (remoteCall.isEmpty ())
    {
      return;
    }

  bool const isNew = m_qsoLog.find (sessionId) == m_qsoLog.end ();
  QsoLogEntry& entry = m_qsoLog[sessionId];
  entry.sessionId = sessionId;
  entry.remoteCall = remoteCall;
  entry.profileName = QString::fromStdString (
      decodium::ft2link::profileName (session->negotiated.profile));
  entry.rateName = QString::fromLatin1 (
      decodium::ft2link::w2300RateModeName (
          session->negotiated.w2300RateMode));
  if (session->negotiated.profile != Profile::Wide2300)
    {
      entry.rateName.clear ();
    }
  entry.state = sessionStateName (session->state);
  entry.lastEvent = event.trimmed ().isEmpty ()
      ? QStringLiteral ("session")
      : event.trimmed ();
  entry.openedAtMs = session->openedAtMs;
  entry.updatedAtMs = std::max<quint64> (nowMs, session->updatedAtMs);
  entry.messageCount = static_cast<int> (session->messages.size ());
  if (session->state == AppSessionState::Closed)
    {
      entry.closedAtMs = nowMs;
    }

  touchContact (remoteCall, nowMs, entry.lastEvent, QString {}, QString {},
                entry.profileName);
  if (isNew)
    {
      ContactHistory& contact = m_contactHistory[remoteCall];
      ++contact.qsoCount;
      contact.messageCount = std::max (contact.messageCount,
                                       entry.messageCount);
      emit contactHistoryChanged ();
    }
  else
    {
      ContactHistory& contact = m_contactHistory[remoteCall];
      contact.messageCount = std::max (contact.messageCount,
                                       entry.messageCount);
      emit contactHistoryChanged ();
    }
  emit qsoLogChanged ();
}

void FT2LinkQmlAdapter::pruneLogbookOutbox ()
{
  while (m_logbookOutbox.size () > 200u)
    {
      std::vector<LogbookUpload>::iterator oldest =
          m_logbookOutbox.end ();
      for (std::vector<LogbookUpload>::iterator it = m_logbookOutbox.begin ();
           it != m_logbookOutbox.end ();
           ++it)
        {
          if (oldest == m_logbookOutbox.end ()
              || it->queuedAtMs < oldest->queuedAtMs)
            {
              oldest = it;
            }
        }
      if (oldest == m_logbookOutbox.end ())
        {
          break;
        }
      m_logbookOutbox.erase (oldest);
    }
}

W2300RateMode FT2LinkQmlAdapter::currentLiveW2300RateMode (
    AppSession const& session) const
{
  std::map<std::uint16_t, decodium::ft2link::W2300RateController>::const_iterator
      controller = m_liveW2300RateControllers.find (session.sessionId);
  if (controller == m_liveW2300RateControllers.end ())
    {
      return session.negotiated.w2300RateMode;
    }
  return controller->second.currentMode ();
}

void FT2LinkQmlAdapter::observeLiveW2300Metrics (
    Frame const& frame,
    decodium::ft2link::W2300DecodeMetrics const& metrics,
    quint64 nowMs)
{
  if (frame.profile != Profile::Wide2300)
    {
      return;
    }

  AppSession const* session = m_model.session (frame.sessionId);
  if (!session || session->state != AppSessionState::Connected
      || session->negotiated.profile != Profile::Wide2300)
    {
      return;
    }

  std::pair<std::map<std::uint16_t, decodium::ft2link::W2300RateController>::iterator, bool>
      inserted = m_liveW2300RateControllers.insert (
          std::make_pair (
              frame.sessionId,
              decodium::ft2link::W2300RateController (
                  session->negotiated.w2300RateMode)));
  inserted.first->second.observe (metrics);
  W2300RateMode const nextRateMode = inserted.first->second.currentMode ();
  m_lastLiveW2300Metrics[frame.sessionId] = metrics;
  m_lastTransportMetrics = w2300LiveMetricsMap (
      frame.sessionId, metrics, nextRateMode, nowMs);
  emit transportMetricsChanged ();
  QString const remoteCall = normalizeCallsign (
      QString::fromStdString (session->remoteCall));
  QString locator;
  std::map<QString, ContactHistory>::const_iterator contact =
      m_contactHistory.find (remoteCall);
  if (contact != m_contactHistory.end ())
    {
      locator = contact->second.locator;
    }
  recordPathReport (
      QStringLiteral ("Incoming"),
      remoteCall,
      locator,
      false,
      0,
      true,
      metrics.quality,
      metrics.estimatedFrequencyOffsetHz,
      QString::fromStdString (decodium::ft2link::profileName (frame.profile)),
      QString::fromLatin1 (
          decodium::ft2link::w2300RateModeName (nextRateMode)),
      QStringLiteral ("W2300"),
      nowMs);
}

bool FT2LinkQmlAdapter::isLiveChannelBusy (quint64 nowMs) const
{
  return m_liveChannelBusy && nowMs < m_liveChannelBusyUntilMs;
}

void FT2LinkQmlAdapter::observeRxEnergy (
    std::vector<float> const& samples,
    quint64 nowMs)
{
  if (samples.empty ())
    {
      return;
    }

  double sumSquares = 0.0;
  double peak = 0.0;
  for (float sample : samples)
    {
      double const value = static_cast<double> (sample);
      sumSquares += value * value;
      peak = std::max (peak, std::fabs (value));
    }
  double const rms = std::sqrt (
      sumSquares / static_cast<double> (samples.size ()));

  constexpr double kBusyRmsThreshold = 0.012;
  constexpr double kBusyPeakThreshold = 0.080;
  constexpr quint64 kBusyHoldMs = 750u;
  bool const energyBusy = rms >= kBusyRmsThreshold
      || peak >= kBusyPeakThreshold;
  quint64 const previousBusyUntil = m_liveChannelBusyUntilMs;
  bool const previousBusy = isLiveChannelBusy (nowMs);
  if (energyBusy)
    {
      m_liveChannelBusyUntilMs = std::max (
          m_liveChannelBusyUntilMs, nowMs + kBusyHoldMs);
      m_liveChannelBusy = true;
      m_liveChannelTimer.start (
          static_cast<int> (std::min<quint64> (
              m_liveChannelBusyUntilMs > nowMs
              ? m_liveChannelBusyUntilMs - nowMs
              : 1u,
              2147483647u)));
    }
  else if (nowMs >= m_liveChannelBusyUntilMs)
    {
      m_liveChannelBusy = false;
    }

  bool const rmsChanged = std::fabs (m_liveChannelRms - rms) > 0.0005;
  bool const peakChanged = std::fabs (m_liveChannelPeak - peak) > 0.001;
  m_liveChannelRms = rms;
  m_liveChannelPeak = peak;
  if (previousBusy != isLiveChannelBusy (nowMs)
      || previousBusyUntil != m_liveChannelBusyUntilMs
      || rmsChanged
      || peakChanged)
    {
      emit liveChannelChanged ();
    }
}

bool FT2LinkQmlAdapter::requestAckRadioTx (Frame const& ack,
                                           AppSession const& session,
                                           quint64 nowMs)
{
  QVector<float> samples;
  QVariantMap plan;
  QString error;
  W2300RateMode const ackRateMode =
      ack.profile == Profile::Wide2300
      ? W2300RateMode::Robust
      : session.negotiated.w2300RateMode;
  if (!buildAckAudio (ack, ackRateMode, &samples, &plan, &error))
    {
      setLastError (error);
      return false;
    }

  plan.insert (QStringLiteral ("requestedAtMs"),
               QVariant::fromValue<qulonglong> (
                   static_cast<qulonglong> (nowMs)));
  plan.insert (QStringLiteral ("remoteCall"),
               QString::fromStdString (session.remoteCall));

  enqueueRadioTx (QStringLiteral ("FT2-Link ACK"),
                  samples,
                  plan,
                  nowMs,
                  true,
                  ack.sessionId,
                  false);
  return true;
}

bool FT2LinkQmlAdapter::requestControlRadioTx (Frame const& frame,
                                               QString const& kind,
                                               QString const& remoteCall,
                                               quint64 nowMs)
{
  QVector<float> samples;
  QVariantMap plan;
  QString error;
  if (!buildNarrowControlAudio (frame, kind, &samples, &plan, &error))
    {
      setLastError (error);
      return false;
    }
  plan.insert (QStringLiteral ("requestedAtMs"),
               QVariant::fromValue<qulonglong> (
                   static_cast<qulonglong> (nowMs)));
  plan.insert (QStringLiteral ("remoteCall"), remoteCall.trimmed ().toUpper ());

  enqueueRadioTx (QStringLiteral ("FT2-Link ") + kind,
                  samples,
                  plan,
                  nowMs,
                  kind == QStringLiteral ("HELLO_ACK")
                  || kind == QStringLiteral ("PING_ACK"),
                  frame.sessionId,
                  false);
  return true;
}

void FT2LinkQmlAdapter::enqueueRadioTx (QString const& displayMessage,
                                        QVector<float> const& samples,
                                        QVariantMap const& plan,
                                        quint64 nowMs,
                                        bool priority,
                                        quint16 sessionId,
                                        bool cancelIfNoOutbound)
{
  RadioTxQueueItem item;
  item.displayMessage = displayMessage;
  item.samples = samples;
  item.plan = plan;
  item.sessionId = sessionId;
  item.cancelIfNoOutbound = cancelIfNoOutbound;
  item.priority = priority;

  quint64 const effectiveNow = nowMs == 0u
      ? static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ())
      : nowMs;
  bool const channelBusy = isLiveChannelBusy (effectiveNow);
  if (m_radioTxQueue.empty () && m_radioTxBusyUntilMs <= effectiveNow
      && !channelBusy)
    {
      QVariantMap emittedPlan = item.plan;
      emittedPlan.insert (QStringLiteral ("queued"), false);
      emit radioTxAudioRequested (item.displayMessage, item.samples, emittedPlan);

      double const audioSeconds = emittedPlan.value (
          QStringLiteral ("audioSeconds")).toDouble ();
      quint64 const durationMs = std::max<quint64> (
          250u,
          static_cast<quint64> (audioSeconds * 1000.0 + 0.5) + 250u);
      m_radioTxBusyUntilMs = effectiveNow + durationMs;
      m_lastRadioTxSessionId = item.sessionId;
      scheduleRadioQueueDrain (effectiveNow);
      return;
    }

  item.plan.insert (QStringLiteral ("queued"), true);
  if (channelBusy)
    {
      item.plan.insert (QStringLiteral ("lbtDeferred"), true);
      item.plan.insert (QStringLiteral ("channelBusyUntilMs"),
                        QVariant::fromValue<qulonglong> (
                            static_cast<qulonglong> (
                                m_liveChannelBusyUntilMs)));
      setTransportState (QStringLiteral ("LBT wait"));
    }
  if (priority)
    {
      std::deque<RadioTxQueueItem>::iterator insertAt =
          std::find_if (m_radioTxQueue.begin (),
                        m_radioTxQueue.end (),
                        [] (RadioTxQueueItem const& queued) {
                          return !queued.priority;
                        });
      m_radioTxQueue.insert (insertAt, item);
    }
  else
    {
      m_radioTxQueue.push_back (item);
    }
  scheduleRadioQueueDrain (effectiveNow);
}

void FT2LinkQmlAdapter::drainRadioTxQueue (quint64 nowMs)
{
  quint64 const effectiveNow = nowMs == 0u
      ? static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ())
      : nowMs;
  if (m_radioTxBusyUntilMs > effectiveNow)
    {
      scheduleRadioQueueDrain (effectiveNow);
      return;
    }
  if (isLiveChannelBusy (effectiveNow))
    {
      setTransportState (QStringLiteral ("LBT wait"));
      scheduleRadioQueueDrain (effectiveNow);
      return;
    }
  std::size_t selectedIndex = std::numeric_limits<std::size_t>::max ();
  std::size_t fallbackIndex = std::numeric_limits<std::size_t>::max ();
  for (std::size_t index = 0u; index < m_radioTxQueue.size ();)
    {
      RadioTxQueueItem const& queued = m_radioTxQueue[index];
      if (queued.cancelIfNoOutbound
          && m_liveOutbound.find (queued.sessionId) == m_liveOutbound.end ())
        {
          m_radioTxQueue.erase (
              m_radioTxQueue.begin ()
              + static_cast<std::deque<RadioTxQueueItem>::difference_type> (
                  index));
          continue;
        }
      if (queued.priority)
        {
          selectedIndex = index;
          break;
        }
      if (fallbackIndex == std::numeric_limits<std::size_t>::max ())
        {
          fallbackIndex = index;
        }
      if (queued.sessionId == 0u
          || queued.sessionId != m_lastRadioTxSessionId)
        {
          selectedIndex = index;
          break;
        }
      ++index;
    }
  if (selectedIndex == std::numeric_limits<std::size_t>::max ())
    {
      selectedIndex = fallbackIndex;
    }
  if (selectedIndex == std::numeric_limits<std::size_t>::max ())
    {
      return;
    }

  std::deque<RadioTxQueueItem>::iterator selected =
      m_radioTxQueue.begin ()
      + static_cast<std::deque<RadioTxQueueItem>::difference_type> (
          selectedIndex);
  RadioTxQueueItem item = *selected;
  m_radioTxQueue.erase (selected);

  QVariantMap emittedPlan = item.plan;
  emittedPlan.insert (QStringLiteral ("queued"), true);
  emit radioTxAudioRequested (item.displayMessage, item.samples, emittedPlan);

  double const audioSeconds = emittedPlan.value (
      QStringLiteral ("audioSeconds")).toDouble ();
  quint64 const durationMs = std::max<quint64> (
      250u,
      static_cast<quint64> (audioSeconds * 1000.0 + 0.5) + 250u);
  m_radioTxBusyUntilMs = effectiveNow + durationMs;
  m_lastRadioTxSessionId = item.sessionId;
  scheduleRadioQueueDrain (effectiveNow);
}

void FT2LinkQmlAdapter::scheduleRadioQueueDrain (quint64 nowMs)
{
  if (m_radioTxQueue.empty ())
    {
      return;
    }
  quint64 const effectiveNow = nowMs == 0u
      ? static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ())
      : nowMs;
  quint64 readyAtMs = m_radioTxBusyUntilMs;
  if (isLiveChannelBusy (effectiveNow))
    {
      readyAtMs = std::max (readyAtMs, m_liveChannelBusyUntilMs);
    }
  quint64 const delayMs = readyAtMs > effectiveNow
      ? readyAtMs - effectiveNow
      : 0u;
  m_radioTxQueueTimer.start (
      static_cast<int> (std::min<quint64> (delayMs, 2147483647u)));
}

void FT2LinkQmlAdapter::scheduleLiveOutboundRetry (
    quint16 sessionId,
    QString const& displayMessage,
    QVector<float> const& samples,
    QVariantMap const& plan,
    std::vector<std::uint8_t> const& payload,
    Profile profile,
    std::size_t messageIndex,
    quint64 nowMs)
{
  LiveOutboundRetry retry;
  retry.displayMessage = displayMessage;
  retry.samples = samples;
  retry.plan = plan;
  retry.payload = payload;
  retry.profile = profile;
  retry.messageIndex = messageIndex;
  retry.attempts = 1u;
  retry.nextRetryMs = nowMs + 8000u;
  m_liveOutboundRetries[sessionId] = retry;
  scheduleLiveOutboundRetryCheck (nowMs);
}

void FT2LinkQmlAdapter::runLiveOutboundRetryCheck ()
{
  quint64 const nowMs =
      static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ());
  std::vector<std::uint16_t> failed;
  for (std::map<std::uint16_t, LiveOutboundRetry>::iterator it =
           m_liveOutboundRetries.begin ();
       it != m_liveOutboundRetries.end ();
       ++it)
    {
      if (it->second.nextRetryMs > nowMs)
        {
          continue;
        }
      if (m_liveOutbound.find (it->first) == m_liveOutbound.end ())
        {
          failed.push_back (it->first);
          continue;
        }
      if (it->second.attempts >= 3u)
        {
          std::string error;
          m_model.markOutgoingFailed (
              it->first, it->second.messageIndex, nowMs, &error);
          std::map<std::uint16_t, quint32>::const_iterator mailboxId =
              m_liveOutboundMailboxId.find (it->first);
          if (mailboxId != m_liveOutboundMailboxId.end ())
            {
              updateMailboxState (
                  mailboxId->second, QStringLiteral ("Failed"), nowMs);
            }
          std::map<std::uint16_t, quint32>::const_iterator formId =
              m_liveOutboundFormId.find (it->first);
          if (formId != m_liveOutboundFormId.end ())
            {
              updateFormState (
                  formId->second, QStringLiteral ("Failed"), nowMs);
            }
          std::map<std::uint16_t, quint32>::const_iterator fileTransferId =
              m_liveOutboundFileTransferId.find (it->first);
          if (fileTransferId != m_liveOutboundFileTransferId.end ())
            {
              updateFileTransferState (
                  fileTransferId->second, QStringLiteral ("Failed"), nowMs);
            }
          std::map<std::uint16_t, quint32>::const_iterator bulletinId =
              m_liveOutboundBulletinId.find (it->first);
          if (bulletinId != m_liveOutboundBulletinId.end ())
            {
              updateBulletinState (
                  bulletinId->second, QStringLiteral ("Failed"), nowMs);
            }
          emit messagesChanged (it->first);
          emit sessionsChanged ();
          recordQsoSession (
              it->first, nowMs, QStringLiteral ("RF retry failed"));
          setTransportState (QStringLiteral ("RF retry failed"));
          failed.push_back (it->first);
          continue;
        }

      ++it->second.attempts;
      it->second.nextRetryMs = nowMs + 8000u;
      QVariantMap retryPlan = it->second.plan;
      QVector<float> retrySamples = it->second.samples;
      if (it->second.profile == Profile::Wide2300 && it->second.attempts > 1u)
        {
          decodium::ft2link::WideTxAudioPlanOptions retryOptions;
          retryOptions.profile = Profile::Wide2300;
          retryOptions.w2300RateMode = W2300RateMode::Robust;
          retryOptions.sampleRate = 48000.0;
          WideTxAudioPlan const robustPlan =
              decodium::ft2link::buildWideTxAudioPlan (
                  it->second.payload, it->first, retryOptions);
          if (robustPlan.ok)
            {
              retrySamples = toSampleVector (robustPlan.samples);
              QVariantMap rebuiltPlan = radioTxPlanMap (robustPlan, true);
              rebuiltPlan.insert (QStringLiteral ("sessionId"), it->first);
              rebuiltPlan.insert (QStringLiteral ("text"),
                                  it->second.plan.value (
                                      QStringLiteral ("text")));
              rebuiltPlan.insert (QStringLiteral ("requestedAtMs"),
                                  it->second.plan.value (
                                      QStringLiteral ("requestedAtMs")));
              rebuiltPlan.insert (QStringLiteral ("retryRateAdapted"), true);
              retryPlan = rebuiltPlan;
            }
          else
            {
              retryPlan.insert (
                  QStringLiteral ("retryRateAdaptError"),
                  QString::fromStdString (robustPlan.error));
            }
        }
      retryPlan.insert (QStringLiteral ("retryAttempt"),
                        static_cast<int> (it->second.attempts));
      enqueueRadioTx (it->second.displayMessage,
                      retrySamples,
                      retryPlan,
                      nowMs,
                      false,
                      it->first,
                      true);
      setTransportState (QStringLiteral ("RF retry"));
    }

  for (std::uint16_t sessionId : failed)
    {
      m_liveOutboundRetries.erase (sessionId);
      m_liveOutbound.erase (sessionId);
      m_liveOutboundMessageIndex.erase (sessionId);
      m_liveOutboundMailboxId.erase (sessionId);
      m_liveOutboundMailboxDeliveredState.erase (sessionId);
      m_liveOutboundFormId.erase (sessionId);
      m_liveOutboundFileTransferId.erase (sessionId);
      m_liveOutboundBulletinId.erase (sessionId);
    }
  scheduleLiveOutboundRetryCheck (nowMs);
}

void FT2LinkQmlAdapter::scheduleLiveOutboundRetryCheck (quint64 nowMs)
{
  if (m_liveOutboundRetries.empty ())
    {
      return;
    }
  quint64 nextMs = std::numeric_limits<quint64>::max ();
  for (std::map<std::uint16_t, LiveOutboundRetry>::const_iterator it =
           m_liveOutboundRetries.begin ();
       it != m_liveOutboundRetries.end ();
       ++it)
    {
      nextMs = std::min (nextMs, it->second.nextRetryMs);
    }
  quint64 const delayMs = nextMs > nowMs ? nextMs - nowMs : 0u;
  m_liveOutboundRetryTimer.start (
      static_cast<int> (std::min<quint64> (delayMs, 2147483647u)));
}

void FT2LinkQmlAdapter::runAutoBeaconTick ()
{
  if (!m_autoBeaconEnabled)
    {
      return;
    }

  quint64 const nowMs =
      static_cast<quint64> (QDateTime::currentMSecsSinceEpoch ());
  quint64 const intervalMs =
      static_cast<quint64> (std::max (60, m_autoBeaconIntervalSeconds)) * 1000u;
  if (m_lastBeaconTxMs != 0u && nowMs < m_lastBeaconTxMs + intervalMs)
    {
      scheduleAutoBeacon (nowMs);
      return;
    }

  if (!queueBeaconRadio (m_autoBeaconCq, nowMs, false, true))
    {
      if (m_model.localStation ().call.empty ())
        {
          m_autoBeaconEnabled = false;
          m_autoBeaconTimer.stop ();
          emit autoBeaconChanged ();
          return;
        }
    }
  scheduleAutoBeacon (nowMs);
}

void FT2LinkQmlAdapter::scheduleAutoBeacon (quint64 nowMs)
{
  if (!m_autoBeaconEnabled)
    {
      return;
    }

  quint64 const intervalMs =
      static_cast<quint64> (std::max (60, m_autoBeaconIntervalSeconds)) * 1000u;
  quint64 const nextMs = m_lastBeaconTxMs == 0u
      ? nowMs + intervalMs
      : m_lastBeaconTxMs + intervalMs;
  quint64 const delayMs = nextMs > nowMs ? nextMs - nowMs : 1000u;
  m_autoBeaconTimer.start (
      static_cast<int> (std::min<quint64> (delayMs, 2147483647u)));
}

void FT2LinkQmlAdapter::setLastError (QString const& error)
{
  if (m_lastError == error)
    {
      return;
    }
  m_lastError = error;
  emit lastErrorChanged ();
}

void FT2LinkQmlAdapter::clearLastError ()
{
  setLastError ({});
}

void FT2LinkQmlAdapter::setTransportState (QString const& state)
{
  if (m_transportState == state)
    {
      return;
    }
  m_transportState = state;
  emit transportStateChanged ();
}

void FT2LinkQmlAdapter::setTransportBusy (bool busy)
{
  if (m_transportBusy == busy)
    {
      return;
    }
  m_transportBusy = busy;
  emit transportStateChanged ();
}

QString FT2LinkQmlAdapter::defaultLocalStorePath () const
{
  QString root =
      QStandardPaths::writableLocation (QStandardPaths::AppDataLocation);
  if (root.trimmed ().isEmpty ())
    {
      root = QDir::home ().absoluteFilePath (
          QStringLiteral (".decodium"));
    }
  return QDir {root}.absoluteFilePath (
      QStringLiteral ("ft2link/state-v1.json"));
}

QString FT2LinkQmlAdapter::resolvedLocalStorePath (QString const& path) const
{
  if (!path.trimmed ().isEmpty ())
    {
      return path.trimmed ();
    }
  if (!m_localStorePath.trimmed ().isEmpty ())
    {
      return m_localStorePath.trimmed ();
    }
  return defaultLocalStorePath ();
}

QString FT2LinkQmlAdapter::defaultAdifLogPath () const
{
  QFileInfo const storeInfo {defaultLocalStorePath ()};
  return storeInfo.absoluteDir ().absoluteFilePath (
      QStringLiteral ("ft2link_qso_log.adi"));
}

QString FT2LinkQmlAdapter::resolvedAdifLogPath (QString const& path) const
{
  if (!path.trimmed ().isEmpty ())
    {
      return path.trimmed ();
    }
  if (!m_localStorePath.trimmed ().isEmpty ())
    {
      QFileInfo const storeInfo {m_localStorePath.trimmed ()};
      return storeInfo.absoluteDir ().absoluteFilePath (
          QStringLiteral ("ft2link_qso_log.adi"));
    }
  return defaultAdifLogPath ();
}

QByteArray FT2LinkQmlAdapter::serializeLocalStore () const
{
  QJsonObject root;
  root.insert (QStringLiteral ("version"), kLocalStoreVersion);
  root.insert (QStringLiteral ("updatedAtMs"),
               QString::number (
                   static_cast<qulonglong> (
                       QDateTime::currentMSecsSinceEpoch ())));
  root.insert (QStringLiteral ("nextPingToken"),
               QString::number (m_nextPingToken));
  root.insert (QStringLiteral ("nextMailboxId"),
               QString::number (m_nextMailboxId));
  root.insert (QStringLiteral ("nextFormId"),
               QString::number (m_nextFormId));
  root.insert (QStringLiteral ("nextFileTransferId"),
               QString::number (m_nextFileTransferId));
  root.insert (QStringLiteral ("nextBulletinId"),
               QString::number (m_nextBulletinId));
  root.insert (QStringLiteral ("nextPathReportId"),
               QString::number (m_nextPathReportId));
  root.insert (QStringLiteral ("nextLogbookUploadId"),
               QString::number (m_nextLogbookUploadId));
  root.insert (QStringLiteral ("beaconsSent"),
               QString::number (m_beaconsSent));
  root.insert (QStringLiteral ("beaconsReceived"),
               QString::number (m_beaconsReceived));
  root.insert (QStringLiteral ("cqsSent"),
               QString::number (m_cqsSent));
  root.insert (QStringLiteral ("cqsReceived"),
               QString::number (m_cqsReceived));
  root.insert (QStringLiteral ("awayEnabled"),
               m_autoAwayActivated ? false : m_awayEnabled);
  root.insert (QStringLiteral ("awayAcceptsQsy"), m_awayAcceptsQsy);
  root.insert (QStringLiteral ("awayMessage"), m_awayMessage);
  root.insert (QStringLiteral ("welcomeEnabled"), m_welcomeEnabled);
  root.insert (QStringLiteral ("welcomeMessage"), m_welcomeMessage);
  root.insert (QStringLiteral ("autoReplyEnabled"), m_autoReplyEnabled);
  root.insert (QStringLiteral ("autoAwayEnabled"), m_autoAwayEnabled);
  root.insert (QStringLiteral ("autoAwayMinutes"), m_autoAwayMinutes);
  root.insert (QStringLiteral ("callIdIntervalMinutes"),
               m_callIdIntervalMinutes);
  root.insert (QStringLiteral ("autoDisconnectMinutes"),
               m_autoDisconnectMinutes);
  root.insert (QStringLiteral ("incomingPingsEnabled"),
               m_incomingPingsEnabled);
  root.insert (QStringLiteral ("lastHeardPeekingEnabled"),
               m_lastHeardPeekingEnabled);
  root.insert (QStringLiteral ("lastConnectionsPeekingEnabled"),
               m_lastConnectionsPeekingEnabled);
  root.insert (QStringLiteral ("parkedVmailPeekingEnabled"),
               m_parkedVmailPeekingEnabled);
  root.insert (QStringLiteral ("vmailParkingEnabled"),
               m_vmailParkingEnabled);
  root.insert (QStringLiteral ("snrReportSendingEnabled"),
               m_snrReportSendingEnabled);
  root.insert (QStringLiteral ("verboseSnrAutoAcceptEnabled"),
               m_verboseSnrAutoAcceptEnabled);
  root.insert (QStringLiteral ("infoInquireEnabled"),
               m_infoInquireEnabled);
  root.insert (QStringLiteral ("clusterEnabled"), m_clusterEnabled);
  root.insert (QStringLiteral ("clusterNodeId"),
               sanitizedClusterNodeId (m_clusterNodeId));
  root.insert (QStringLiteral ("clusterBand"),
               sanitizedClusterBand (m_clusterBand));
  root.insert (QStringLiteral ("clusterDialFrequencyHz"),
               QString::number (m_clusterDialFrequencyHz));

  QJsonArray broadcasts;
  for (BroadcastMessage const& message : m_broadcasts)
    {
      broadcasts.append (jsonObjectFromMap (broadcastMap (
          message.fromCall,
          message.text,
          message.source,
          message.alertTags,
          message.atMs)));
    }
  root.insert (QStringLiteral ("broadcasts"), broadcasts);

  QJsonArray alerts;
  for (AlertEvent const& alert : m_alerts)
    {
      alerts.append (jsonObjectFromMap (alertMap (
          alert.fromCall,
          alert.text,
          alert.source,
          alert.tag,
          alert.atMs)));
    }
  root.insert (QStringLiteral ("alerts"), alerts);

  QJsonArray mailbox;
  for (MailboxMessage const& message : m_mailbox)
    {
      mailbox.append (jsonObjectFromMap (mailboxMap (
          message.id,
          message.direction,
          message.fromCall,
          message.toCall,
          message.subject,
          message.body,
          message.state,
          message.atMs,
          message.updatedAtMs,
          message.relayNotifiedAtMs,
          message.urgent,
          message.emcomm,
          message.relayViaCall,
          message.relayHopCount,
          message.relayProtocol,
          message.emailGatewayState,
          message.emailGatewayDetail,
          message.emailGatewayAtMs)));
    }
  root.insert (QStringLiteral ("mailbox"), mailbox);

  QJsonArray forms;
  for (FormMessage const& form : m_forms)
    {
      forms.append (jsonObjectFromMap (formMap (
          form.id,
          form.direction,
          form.fromCall,
          form.toCall,
          form.formType,
          form.fields,
          form.state,
          form.atMs,
          form.updatedAtMs)));
    }
  root.insert (QStringLiteral ("forms"), forms);

  QJsonArray fileTransfers;
  for (FileTransfer const& transfer : m_fileTransfers)
    {
      fileTransfers.append (jsonObjectFromMap (fileTransferMap (
          transfer.id,
          transfer.direction,
          transfer.fromCall,
          transfer.toCall,
          transfer.fileName,
          transfer.content,
          transfer.sha256,
          transfer.state,
          transfer.atMs,
          transfer.updatedAtMs)));
    }
  root.insert (QStringLiteral ("fileTransfers"), fileTransfers);

  QJsonArray bulletins;
  for (Bulletin const& bulletin : m_bulletins)
    {
      bulletins.append (jsonObjectFromMap (bulletinMap (
          bulletin.id,
          bulletin.direction,
          bulletin.fromCall,
          bulletin.group,
          bulletin.title,
          bulletin.body,
          bulletin.state,
          bulletin.atMs,
          bulletin.updatedAtMs)));
    }
  root.insert (QStringLiteral ("bulletins"), bulletins);

  QJsonArray contacts;
  for (std::map<QString, ContactHistory>::const_iterator it =
           m_contactHistory.begin ();
       it != m_contactHistory.end ();
       ++it)
    {
      ContactHistory const& contact = it->second;
      contacts.append (jsonObjectFromMap (contactHistoryMap (
          contact.call,
          contact.locator,
          contact.name,
          contact.tag,
          contact.comment,
          contact.lastEvent,
          contact.lastProfileName,
          contact.firstHeardMs,
          contact.lastHeardMs,
          contact.qsoCount,
          contact.messageCount,
          contact.mailCount,
          contact.formCount,
          contact.fileCount,
          contact.bulletinCount,
          contact.broadcastCount,
          contact.alertCount)));
    }
  root.insert (QStringLiteral ("contactHistory"), contacts);

  QJsonArray qsoLog;
  for (std::map<quint16, QsoLogEntry>::const_iterator it =
           m_qsoLog.begin ();
       it != m_qsoLog.end ();
       ++it)
    {
      QsoLogEntry const& entry = it->second;
      qsoLog.append (jsonObjectFromMap (qsoLogMap (
          entry.sessionId,
          entry.remoteCall,
          entry.profileName,
          entry.rateName,
          entry.state,
          entry.lastEvent,
          entry.openedAtMs,
          entry.updatedAtMs,
          entry.closedAtMs,
          entry.messageCount)));
    }
  root.insert (QStringLiteral ("qsoLog"), qsoLog);

  QJsonArray logbookOutbox;
  for (LogbookUpload const& upload : m_logbookOutbox)
    {
      logbookOutbox.append (jsonObjectFromMap (logbookUploadMap (
          upload.id,
          upload.sessionId,
          upload.remoteCall,
          upload.target,
          upload.state,
          upload.detail,
          upload.adif,
          upload.adifSha256,
          upload.queuedAtMs,
          upload.updatedAtMs)));
    }
  root.insert (QStringLiteral ("logbookOutbox"), logbookOutbox);

  QJsonArray pingLog;
  for (PingRecord const& ping : m_pingLog)
    {
      pingLog.append (jsonObjectFromMap (pingMap (
          ping.direction,
          ping.remoteCall,
          ping.state,
          ping.token,
          ping.atMs,
          ping.rttMs)));
    }
  root.insert (QStringLiteral ("pingLog"), pingLog);

  QJsonArray pathReports;
  for (PathReport const& report : m_pathReports)
    {
      pathReports.append (jsonObjectFromMap (pathReportMap (
          report.id,
          report.direction,
          report.remoteCall,
          report.locator,
          report.snrValid,
          report.snrDb,
          report.qualityValid,
          report.quality,
          report.frequencyOffsetHz,
          report.profileName,
          report.rateName,
          report.source,
          report.atMs)));
    }
  root.insert (QStringLiteral ("pathReports"), pathReports);

  QJsonArray beaconHistory;
  for (BeaconHistoryEntry const& entry : m_beaconHistory)
    {
      beaconHistory.append (jsonObjectFromMap (beaconHistoryMap (
          entry.direction,
          entry.call,
          entry.locator,
          entry.name,
          entry.profileName,
          entry.cq,
          entry.cqType,
          entry.cqLocator,
          entry.cqSlotId,
          entry.cqSlotSizeHz,
          entry.source,
          entry.atMs)));
    }
  root.insert (QStringLiteral ("beaconHistory"), beaconHistory);

  QJsonArray clusterLastHeard;
  for (std::map<QString, ClusterLastHeardEntry>::const_iterator it =
           m_clusterLastHeard.begin ();
       it != m_clusterLastHeard.end ();
       ++it)
    {
      ClusterLastHeardEntry const& entry = it->second;
      clusterLastHeard.append (jsonObjectFromMap (clusterLastHeardMap (
          entry.call,
          entry.locator,
          entry.name,
          entry.profileName,
          entry.event,
          entry.source,
          entry.nodeId,
          entry.band,
          entry.dialFrequencyHz,
          entry.cq,
          entry.cqType,
          entry.firstHeardMs,
          entry.lastHeardMs,
          entry.heardCount)));
    }
  root.insert (QStringLiteral ("clusterLastHeard"), clusterLastHeard);

  QJsonArray cannedMessages;
  for (CannedMessage const& message : m_customCannedMessages)
    {
      QJsonObject object;
      object.insert (QStringLiteral ("label"), message.label);
      object.insert (QStringLiteral ("templateText"), message.templateText);
      object.insert (QStringLiteral ("tip"), message.tip);
      cannedMessages.append (object);
    }
  root.insert (QStringLiteral ("customCannedMessages"), cannedMessages);

  QJsonArray customAlertTags;
  for (QString const& tag : m_customAlertTags)
    {
      customAlertTags.append (tag);
    }
  root.insert (QStringLiteral ("customAlertTags"), customAlertTags);

  QJsonArray blockedCalls;
  for (QString const& call : m_blockedCalls)
    {
      blockedCalls.append (call);
    }
  root.insert (QStringLiteral ("blockedCalls"), blockedCalls);

  QJsonArray frequencyPresets;
  for (FrequencyPreset const& preset : m_frequencyPresets)
    {
      QJsonObject object;
      object.insert (QStringLiteral ("dialFrequencyHz"),
                     QString::number (preset.dialFrequencyHz));
      object.insert (QStringLiteral ("band"), preset.band);
      object.insert (QStringLiteral ("label"), preset.label);
      frequencyPresets.append (object);
    }
  root.insert (QStringLiteral ("frequencyPresets"), frequencyPresets);

  QJsonArray allowedQsyRanges;
  for (AllowedQsyRange const& range : m_allowedQsyRanges)
    {
      QJsonObject object;
      object.insert (QStringLiteral ("fromHz"), QString::number (range.fromHz));
      object.insert (QStringLiteral ("toHz"), QString::number (range.toHz));
      object.insert (QStringLiteral ("label"), range.label);
      allowedQsyRanges.append (object);
    }
  root.insert (QStringLiteral ("allowedQsyRanges"), allowedQsyRanges);

  QJsonArray frequencySchedule;
  for (FrequencyScheduleEntry const& entry : m_frequencySchedule)
    {
      QJsonObject object;
      object.insert (QStringLiteral ("startMinute"), entry.startMinute);
      object.insert (QStringLiteral ("endMinute"), entry.endMinute);
      object.insert (QStringLiteral ("action"),
                     sanitizedScheduleAction (entry.action));
      object.insert (QStringLiteral ("dialFrequencyHz"),
                     QString::number (entry.dialFrequencyHz));
      object.insert (QStringLiteral ("label"), entry.label);
      object.insert (QStringLiteral ("cqType"), sanitizedCqType (entry.cqType));
      frequencySchedule.append (object);
    }
  root.insert (QStringLiteral ("frequencySchedule"), frequencySchedule);

  return QJsonDocument {root}.toJson (QJsonDocument::Indented);
}

bool FT2LinkQmlAdapter::applyLocalStoreBytes (QByteArray const& bytes,
                                              QString* error)
{
  QJsonParseError parseError;
  QJsonDocument const document = QJsonDocument::fromJson (bytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject ())
    {
      if (error)
        {
          *error = QStringLiteral ("Invalid FT2-Link store JSON: %1")
              .arg (parseError.errorString ());
        }
      return false;
    }

  QJsonObject const root = document.object ();
  int const version = root.value (QStringLiteral ("version")).toInt (0);
  if (version < 1 || version > kLocalStoreVersion)
    {
      if (error)
        {
          *error = QStringLiteral ("Unsupported FT2-Link store version: %1")
              .arg (version);
        }
      return false;
    }

  std::vector<BroadcastMessage> broadcasts;
  std::vector<AlertEvent> alerts;
  std::vector<MailboxMessage> mailbox;
  std::vector<FormMessage> forms;
  std::vector<FileTransfer> fileTransfers;
  std::vector<Bulletin> bulletins;
  std::map<QString, ContactHistory> contacts;
  std::map<quint16, QsoLogEntry> qsoLog;
  std::vector<PingRecord> pingLog;
  std::vector<PathReport> pathReports;
  std::vector<BeaconHistoryEntry> beaconHistory;
  std::map<QString, ClusterLastHeardEntry> clusterLastHeard;
  std::vector<LogbookUpload> logbookOutbox;
  std::vector<CannedMessage> customCannedMessages;
  QStringList customAlertTags;
  QStringList blockedCalls;
  std::vector<FrequencyPreset> frequencyPresets = defaultFrequencyPresets ();
  std::vector<AllowedQsyRange> allowedQsyRanges = defaultAllowedQsyRanges ();
  std::vector<FrequencyScheduleEntry> frequencySchedule;
  bool const awayEnabledValue =
      root.value (QStringLiteral ("awayEnabled")).toBool (false);
  bool const awayAcceptsQsyValue =
      root.value (QStringLiteral ("awayAcceptsQsy")).toBool (false);
  QString awayMessageValue = sanitizedCannedText (
      root.value (QStringLiteral ("awayMessage")).toString (
          QStringLiteral ("QRX DE <MYCALL>")),
      240);
  bool const welcomeEnabledValue =
      root.value (QStringLiteral ("welcomeEnabled")).toBool (false);
  QString welcomeMessageValue = sanitizedCannedText (
      root.value (QStringLiteral ("welcomeMessage")).toString (
          QStringLiteral ("HELLO <CALL> DE <MYCALL>")),
      240);
  bool const autoReplyEnabledValue =
      root.value (QStringLiteral ("autoReplyEnabled")).toBool (false);
  bool const autoAwayEnabledValue =
      root.value (QStringLiteral ("autoAwayEnabled")).toBool (false);
  int const autoAwayMinutesValue = std::clamp (
      root.value (QStringLiteral ("autoAwayMinutes")).toInt (10),
      1,
      240);
  int const callIdIntervalMinutesValue = std::clamp (
      root.value (QStringLiteral ("callIdIntervalMinutes")).toInt (0),
      0,
      240);
  int const autoDisconnectMinutesValue = std::clamp (
      root.value (QStringLiteral ("autoDisconnectMinutes")).toInt (0),
      0,
      240);
  bool const incomingPingsEnabledValue =
      root.value (QStringLiteral ("incomingPingsEnabled")).toBool (true);
  bool const lastHeardPeekingEnabledValue =
      root.value (QStringLiteral ("lastHeardPeekingEnabled")).toBool (true);
  bool const lastConnectionsPeekingEnabledValue =
      root.value (QStringLiteral ("lastConnectionsPeekingEnabled")).toBool (true);
  bool const parkedVmailPeekingEnabledValue =
      root.value (QStringLiteral ("parkedVmailPeekingEnabled")).toBool (true);
  bool const vmailParkingEnabledValue =
      root.value (QStringLiteral ("vmailParkingEnabled")).toBool (true);
  bool const snrReportSendingEnabledValue =
      root.value (QStringLiteral ("snrReportSendingEnabled")).toBool (true);
  bool const verboseSnrAutoAcceptEnabledValue =
      root.value (QStringLiteral ("verboseSnrAutoAcceptEnabled")).toBool (false);
  bool const infoInquireEnabledValue =
      root.value (QStringLiteral ("infoInquireEnabled")).toBool (true);
  bool const clusterEnabledValue =
      root.value (QStringLiteral ("clusterEnabled")).toBool (true);
  QString const clusterNodeIdValue = sanitizedClusterNodeId (
      root.value (QStringLiteral ("clusterNodeId")).toString ());
  QString const clusterBandValue = sanitizedClusterBand (
      root.value (QStringLiteral ("clusterBand")).toString ());
  qint64 const clusterDialFrequencyHzValue = static_cast<qint64> (
      jsonU64 (root, QStringLiteral ("clusterDialFrequencyHz")));
  if (awayMessageValue.isEmpty ())
    {
      awayMessageValue = QStringLiteral ("QRX DE <MYCALL>");
    }
  if (welcomeMessageValue.isEmpty ())
    {
      welcomeMessageValue = QStringLiteral ("HELLO <CALL> DE <MYCALL>");
    }
  quint32 maxMailboxId = 0u;
  quint32 maxFormId = 0u;
  quint32 maxFileTransferId = 0u;
  quint32 maxBulletinId = 0u;
  quint32 maxPathReportId = 0u;
  quint32 maxLogbookUploadId = 0u;

  for (QJsonValue const& value : root.value (
           QStringLiteral ("broadcasts")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      BroadcastMessage message;
      message.fromCall = normalizeCallsign (
          object.value (QStringLiteral ("fromCall")).toString ());
      if (message.fromCall.isEmpty ())
        {
          message.fromCall = QStringLiteral ("UNKNOWN");
        }
      message.text = jsonString (object, QStringLiteral ("text"));
      if (message.text.isEmpty ())
        {
          continue;
        }
      message.source = jsonString (object, QStringLiteral ("source"));
      message.atMs = jsonU64 (object, QStringLiteral ("atMs"));
      QJsonArray const tags = object.value (
          QStringLiteral ("alertTags")).toArray ();
      for (QJsonValue const& tagValue : tags)
        {
          QString const tag = tagValue.toString ().trimmed ().toUpper ();
          if (!tag.isEmpty ())
            {
              message.alertTags.push_back (tag);
            }
        }
      broadcasts.push_back (message);
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("alerts")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      AlertEvent alert;
      alert.fromCall = normalizeCallsign (
          object.value (QStringLiteral ("fromCall")).toString ());
      if (alert.fromCall.isEmpty ())
        {
          alert.fromCall = QStringLiteral ("UNKNOWN");
        }
      alert.text = jsonString (object, QStringLiteral ("text"));
      alert.source = jsonString (object, QStringLiteral ("source"));
      alert.tag = jsonString (object, QStringLiteral ("tag")).toUpper ();
      alert.atMs = jsonU64 (object, QStringLiteral ("atMs"));
      if (!alert.text.isEmpty () && !alert.tag.isEmpty ())
        {
          alerts.push_back (alert);
        }
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("mailbox")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      MailboxMessage message;
      message.id = jsonU32 (object, QStringLiteral ("id"));
      if (message.id == 0u)
        {
          continue;
        }
      message.direction = jsonString (object, QStringLiteral ("direction"));
      message.fromCall = normalizeCallsign (
          object.value (QStringLiteral ("fromCall")).toString ());
      message.toCall = normalizeCallsign (
          object.value (QStringLiteral ("toCall")).toString ());
      message.subject = jsonString (object, QStringLiteral ("subject"));
      message.body = object.value (QStringLiteral ("body")).toString ();
      message.state = jsonString (object, QStringLiteral ("state"));
      message.atMs = jsonU64 (object, QStringLiteral ("atMs"));
      message.updatedAtMs = jsonU64 (
          object, QStringLiteral ("updatedAtMs"), message.atMs);
      message.relayNotifiedAtMs = jsonU64 (
          object, QStringLiteral ("relayNotifiedAtMs"));
      message.urgent = object.value (QStringLiteral ("urgent")).toBool (false);
      message.emcomm = object.value (QStringLiteral ("emcomm")).toBool (false);
      message.relayViaCall = normalizeCallsign (
          object.value (QStringLiteral ("relayViaCall")).toString ());
      message.relayHopCount = std::clamp (
          jsonInt (object, QStringLiteral ("relayHopCount")),
          0,
          kMaxRelayHopCount);
      message.relayProtocol = jsonString (
          object, QStringLiteral ("relayProtocol")).toUpper ().left (16);
      message.emailGatewayState = jsonString (
          object, QStringLiteral ("emailGatewayState")).left (32);
      message.emailGatewayDetail = jsonString (
          object, QStringLiteral ("emailGatewayDetail")).left (240);
      message.emailGatewayAtMs = jsonU64 (
          object, QStringLiteral ("emailGatewayAtMs"));
      if (message.body.trimmed ().isEmpty ())
        {
          continue;
        }
      mailbox.push_back (message);
      maxMailboxId = std::max (maxMailboxId, message.id);
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("forms")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      FormMessage form;
      form.id = jsonU32 (object, QStringLiteral ("id"));
      if (form.id == 0u)
        {
          continue;
        }
      form.direction = jsonString (object, QStringLiteral ("direction"));
      form.fromCall = normalizeCallsign (
          object.value (QStringLiteral ("fromCall")).toString ());
      form.toCall = normalizeCallsign (
          object.value (QStringLiteral ("toCall")).toString ());
      form.formType = jsonString (
          object, QStringLiteral ("formType")).toUpper ();
      form.fields = object.value (
          QStringLiteral ("fields")).toObject ().toVariantMap ();
      form.state = jsonString (object, QStringLiteral ("state"));
      form.atMs = jsonU64 (object, QStringLiteral ("atMs"));
      form.updatedAtMs = jsonU64 (
          object, QStringLiteral ("updatedAtMs"), form.atMs);
      forms.push_back (form);
      maxFormId = std::max (maxFormId, form.id);
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("fileTransfers")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      FileTransfer transfer;
      transfer.id = jsonU32 (object, QStringLiteral ("id"));
      if (transfer.id == 0u)
        {
          continue;
        }
      transfer.direction = jsonString (object, QStringLiteral ("direction"));
      transfer.fromCall = normalizeCallsign (
          object.value (QStringLiteral ("fromCall")).toString ());
      transfer.toCall = normalizeCallsign (
          object.value (QStringLiteral ("toCall")).toString ());
      transfer.fileName = jsonString (object, QStringLiteral ("fileName"));
      transfer.content = object.value (
          QStringLiteral ("content")).toString ();
      transfer.sha256 = jsonString (object, QStringLiteral ("sha256"));
      transfer.state = jsonString (object, QStringLiteral ("state"));
      transfer.atMs = jsonU64 (object, QStringLiteral ("atMs"));
      transfer.updatedAtMs = jsonU64 (
          object, QStringLiteral ("updatedAtMs"), transfer.atMs);
      fileTransfers.push_back (transfer);
      maxFileTransferId = std::max (maxFileTransferId, transfer.id);
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("bulletins")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      Bulletin bulletin;
      bulletin.id = jsonU32 (object, QStringLiteral ("id"));
      if (bulletin.id == 0u)
        {
          continue;
        }
      bulletin.direction = jsonString (object, QStringLiteral ("direction"));
      bulletin.fromCall = normalizeCallsign (
          object.value (QStringLiteral ("fromCall")).toString ());
      bulletin.group = jsonString (
          object, QStringLiteral ("group")).toUpper ();
      bulletin.title = jsonString (object, QStringLiteral ("title"));
      bulletin.body = object.value (QStringLiteral ("body")).toString ();
      bulletin.state = jsonString (object, QStringLiteral ("state"));
      bulletin.atMs = jsonU64 (object, QStringLiteral ("atMs"));
      bulletin.updatedAtMs = jsonU64 (
          object, QStringLiteral ("updatedAtMs"), bulletin.atMs);
      if (bulletin.body.trimmed ().isEmpty ())
        {
          continue;
        }
      bulletins.push_back (bulletin);
      maxBulletinId = std::max (maxBulletinId, bulletin.id);
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("contactHistory")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      ContactHistory contact;
      contact.call = normalizeCallsign (
          object.value (QStringLiteral ("call")).toString ());
      if (contact.call.isEmpty ())
        {
          continue;
        }
      contact.locator = jsonString (
          object, QStringLiteral ("locator")).toUpper ();
      contact.name = jsonString (object, QStringLiteral ("name"));
      contact.tag = sanitizedContactTag (
          object.value (QStringLiteral ("tag")).toString ());
      contact.comment = sanitizedContactComment (
          object.value (QStringLiteral ("comment")).toString ());
      contact.lastEvent = jsonString (
          object, QStringLiteral ("lastEvent"));
      contact.lastProfileName = jsonString (
          object, QStringLiteral ("lastProfileName"));
      contact.firstHeardMs = jsonU64 (
          object, QStringLiteral ("firstHeardMs"));
      contact.lastHeardMs = jsonU64 (
          object, QStringLiteral ("lastHeardMs"));
      contact.qsoCount = jsonInt (object, QStringLiteral ("qsoCount"));
      contact.messageCount = jsonInt (
          object, QStringLiteral ("messageCount"));
      contact.mailCount = jsonInt (object, QStringLiteral ("mailCount"));
      contact.formCount = jsonInt (object, QStringLiteral ("formCount"));
      contact.fileCount = jsonInt (object, QStringLiteral ("fileCount"));
      contact.bulletinCount = jsonInt (
          object, QStringLiteral ("bulletinCount"));
      contact.broadcastCount = jsonInt (
          object, QStringLiteral ("broadcastCount"));
      contact.alertCount = jsonInt (object, QStringLiteral ("alertCount"));
      contacts[contact.call] = contact;
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("qsoLog")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      QsoLogEntry entry;
      entry.sessionId = jsonU16 (object, QStringLiteral ("sessionId"));
      entry.remoteCall = normalizeCallsign (
          object.value (QStringLiteral ("remoteCall")).toString ());
      if (entry.sessionId == 0u || entry.remoteCall.isEmpty ())
        {
          continue;
        }
      entry.profileName = jsonString (
          object, QStringLiteral ("profileName"));
      entry.rateName = jsonString (object, QStringLiteral ("rateName"));
      entry.state = jsonString (object, QStringLiteral ("state"));
      entry.lastEvent = jsonString (
          object, QStringLiteral ("lastEvent"));
      entry.openedAtMs = jsonU64 (
          object, QStringLiteral ("openedAtMs"));
      entry.updatedAtMs = jsonU64 (
          object, QStringLiteral ("updatedAtMs"), entry.openedAtMs);
      entry.closedAtMs = jsonU64 (
          object, QStringLiteral ("closedAtMs"));
      entry.messageCount = jsonInt (
          object, QStringLiteral ("messageCount"));
      qsoLog[entry.sessionId] = entry;
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("logbookOutbox")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      LogbookUpload upload;
      upload.id = jsonU32 (object, QStringLiteral ("id"));
      upload.sessionId = jsonU16 (object, QStringLiteral ("sessionId"));
      upload.remoteCall = normalizeCallsign (
          object.value (QStringLiteral ("remoteCall")).toString ());
      upload.target = sanitizedLogbookTarget (
          object.value (QStringLiteral ("target")).toString ());
      upload.state = sanitizedLogbookState (
          object.value (QStringLiteral ("state")).toString ());
      upload.detail = jsonString (
          object, QStringLiteral ("detail")).left (240);
      upload.adif = object.value (QStringLiteral ("adif")).toString ().trimmed ();
      upload.adifSha256 = jsonString (
          object, QStringLiteral ("adifSha256")).toLower ();
      upload.queuedAtMs = jsonU64 (
          object, QStringLiteral ("queuedAtMs"));
      upload.updatedAtMs = jsonU64 (
          object, QStringLiteral ("updatedAtMs"), upload.queuedAtMs);
      if (upload.id == 0u || upload.sessionId == 0u
          || upload.remoteCall.isEmpty () || upload.adif.isEmpty ())
        {
          continue;
        }
      QString const digest = sha256Hex (upload.adif.toUtf8 ());
      if (upload.adifSha256.isEmpty ()
          || upload.adifSha256 != digest)
        {
          upload.adifSha256 = digest;
        }
      logbookOutbox.push_back (upload);
      maxLogbookUploadId = std::max (maxLogbookUploadId, upload.id);
      if (logbookOutbox.size () >= 200u)
        {
          break;
        }
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("pingLog")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      PingRecord ping;
      ping.direction = jsonString (
          object, QStringLiteral ("direction"));
      ping.remoteCall = normalizeCallsign (
          object.value (QStringLiteral ("remoteCall")).toString ());
      if (ping.remoteCall.isEmpty ())
        {
          ping.remoteCall = QStringLiteral ("UNKNOWN");
        }
      ping.state = jsonString (object, QStringLiteral ("state"));
      ping.token = jsonU16 (object, QStringLiteral ("token"));
      ping.atMs = jsonU64 (object, QStringLiteral ("atMs"));
      ping.rttMs = jsonU64 (object, QStringLiteral ("rttMs"));
      pingLog.push_back (ping);
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("pathReports")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      PathReport report;
      report.id = jsonU32 (object, QStringLiteral ("id"));
      if (report.id == 0u)
        {
          continue;
        }
      report.direction = jsonString (
          object, QStringLiteral ("direction"));
      report.remoteCall = normalizeCallsign (
          object.value (QStringLiteral ("remoteCall")).toString ());
      if (report.remoteCall.isEmpty ())
        {
          report.remoteCall = QStringLiteral ("UNKNOWN");
        }
      report.locator = jsonString (
          object, QStringLiteral ("locator")).toUpper ();
      report.snrValid = object.value (
          QStringLiteral ("snrValid")).toBool (false);
      report.snrDb = jsonInt (object, QStringLiteral ("snrDb"));
      report.qualityValid = object.value (
          QStringLiteral ("qualityValid")).toBool (false);
      report.quality = object.value (
          QStringLiteral ("quality")).toDouble (0.0);
      report.frequencyOffsetHz = object.value (
          QStringLiteral ("frequencyOffsetHz")).toDouble (0.0);
      report.profileName = jsonString (
          object, QStringLiteral ("profileName"));
      report.rateName = jsonString (
          object, QStringLiteral ("rateName"));
      report.source = jsonString (object, QStringLiteral ("source"));
      report.atMs = jsonU64 (object, QStringLiteral ("atMs"));
      if (!report.snrValid && !report.qualityValid)
        {
          continue;
        }
      pathReports.push_back (report);
      maxPathReportId = std::max (maxPathReportId, report.id);
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("beaconHistory")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      BeaconHistoryEntry entry;
      entry.direction = jsonString (
          object, QStringLiteral ("direction")).toUpper ();
      entry.call = normalizeCallsign (
          object.value (QStringLiteral ("call")).toString ());
      if (entry.call.isEmpty ())
        {
          continue;
        }
      entry.locator = jsonString (
          object, QStringLiteral ("locator")).toUpper ();
      entry.name = jsonString (object, QStringLiteral ("name"));
      entry.profileName = jsonString (
          object, QStringLiteral ("profileName"));
      entry.cq = object.value (QStringLiteral ("cq")).toBool (false);
      entry.cqType = jsonString (
          object, QStringLiteral ("cqType")).toUpper ();
      if (entry.cqType.isEmpty ())
        {
          entry.cqType = QStringLiteral ("CQ");
        }
      entry.cqLocator = jsonString (
          object, QStringLiteral ("cqLocator")).toUpper ();
      entry.cqSlotId = std::clamp (
          jsonInt (object, QStringLiteral ("cqSlotId")), -10, 10);
      entry.cqSlotSizeHz = std::clamp (
          jsonInt (object, QStringLiteral ("cqSlotSizeHz")), 0, 5000);
      entry.source = jsonString (object, QStringLiteral ("source"));
      entry.atMs = jsonU64 (object, QStringLiteral ("atMs"));
      beaconHistory.push_back (entry);
      if (beaconHistory.size () >= 100u)
        {
          break;
        }
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("clusterLastHeard")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      ClusterLastHeardEntry entry;
      entry.call = normalizeCallsign (
          object.value (QStringLiteral ("call")).toString ());
      if (entry.call.isEmpty ())
        {
          continue;
        }
      entry.locator = jsonString (
          object, QStringLiteral ("locator")).toUpper ().left (12);
      entry.name = jsonString (object, QStringLiteral ("name")).left (48);
      entry.profileName = jsonString (
          object, QStringLiteral ("profileName")).left (24);
      entry.event = jsonString (
          object, QStringLiteral ("event")).left (48);
      entry.source = jsonString (
          object, QStringLiteral ("source")).left (24);
      entry.nodeId = sanitizedClusterNodeId (
          object.value (QStringLiteral ("nodeId")).toString ());
      entry.band = sanitizedClusterBand (
          object.value (QStringLiteral ("band")).toString ());
      entry.dialFrequencyHz = static_cast<qint64> (
          jsonU64 (object, QStringLiteral ("dialFrequencyHz")));
      entry.cq = object.value (QStringLiteral ("cq")).toBool (false);
      entry.cqType = jsonString (
          object, QStringLiteral ("cqType")).toUpper ().left (16);
      if (entry.cqType.isEmpty ())
        {
          entry.cqType = QStringLiteral ("CQ");
        }
      entry.firstHeardMs = jsonU64 (
          object, QStringLiteral ("firstHeardMs"));
      entry.lastHeardMs = jsonU64 (
          object, QStringLiteral ("lastHeardMs"));
      entry.heardCount = std::max (
          1, jsonInt (object, QStringLiteral ("heardCount"), 1));
      if (entry.lastHeardMs == 0u)
        {
          continue;
        }
      clusterLastHeard[clusterKey (
          entry.nodeId,
          entry.band,
          entry.dialFrequencyHz,
          entry.call)] = entry;
    }
  while (clusterLastHeard.size () > 300u)
    {
      std::map<QString, ClusterLastHeardEntry>::iterator oldest =
          clusterLastHeard.end ();
      for (std::map<QString, ClusterLastHeardEntry>::iterator it =
               clusterLastHeard.begin ();
           it != clusterLastHeard.end ();
           ++it)
        {
          if (oldest == clusterLastHeard.end ()
              || it->second.lastHeardMs < oldest->second.lastHeardMs)
            {
              oldest = it;
            }
        }
      if (oldest == clusterLastHeard.end ())
        {
          break;
        }
      clusterLastHeard.erase (oldest);
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("customCannedMessages")).toArray ())
    {
      QJsonObject const object = value.toObject ();
      CannedMessage message;
      message.label = sanitizedCannedLabel (
          object.value (QStringLiteral ("label")).toString ());
      message.templateText = sanitizedCannedText (
          object.value (QStringLiteral ("templateText")).toString (), 512);
      message.tip = sanitizedCannedText (
          object.value (QStringLiteral ("tip")).toString (), 96);
      if (message.label.isEmpty () || message.templateText.isEmpty ())
        {
          continue;
        }
      auto const duplicate = std::find_if (
          customCannedMessages.begin (),
          customCannedMessages.end (),
          [&message] (CannedMessage const& existing) {
            return existing.label == message.label;
          });
      if (duplicate == customCannedMessages.end ())
        {
          customCannedMessages.push_back (message);
        }
      if (customCannedMessages.size () >= 24u)
        {
          break;
        }
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("customAlertTags")).toArray ())
    {
      QString const tag = sanitizedAlertTag (value.toString ());
      if (tag.size () >= 2 && !customAlertTags.contains (tag))
        {
          customAlertTags.push_back (tag);
        }
      if (customAlertTags.size () >= 24)
        {
          break;
        }
    }

  for (QJsonValue const& value : root.value (
           QStringLiteral ("blockedCalls")).toArray ())
    {
      QString const call = sanitizedBlockedCall (value.toString ());
      if (call.size () >= 2 && !blockedCalls.contains (call))
        {
          blockedCalls.push_back (call);
        }
      if (blockedCalls.size () >= 200)
        {
          break;
        }
    }
  blockedCalls.sort ();

  QJsonArray const storedFrequencyPresets =
      root.value (QStringLiteral ("frequencyPresets")).toArray ();
  if (!storedFrequencyPresets.isEmpty ())
    {
      std::vector<FrequencyPreset> parsed;
      for (QJsonValue const& value : storedFrequencyPresets)
        {
          QJsonObject const object = value.toObject ();
          qint64 const hz = static_cast<qint64> (
              jsonU64 (object, QStringLiteral ("dialFrequencyHz")));
          if (hz <= 0)
            {
              continue;
            }
          parsed.push_back ({
              hz,
              jsonString (object, QStringLiteral ("band")).left (24),
              jsonString (object, QStringLiteral ("label")).left (64)
          });
          if (parsed.size () >= 32u)
            {
              break;
            }
        }
      if (!parsed.empty ())
        {
          frequencyPresets = parsed;
        }
    }

  QJsonArray const storedAllowedQsyRanges =
      root.value (QStringLiteral ("allowedQsyRanges")).toArray ();
  if (!storedAllowedQsyRanges.isEmpty ())
    {
      std::vector<AllowedQsyRange> parsed;
      for (QJsonValue const& value : storedAllowedQsyRanges)
        {
          QJsonObject const object = value.toObject ();
          qint64 fromHz = static_cast<qint64> (
              jsonU64 (object, QStringLiteral ("fromHz")));
          qint64 toHz = static_cast<qint64> (
              jsonU64 (object, QStringLiteral ("toHz")));
          if (fromHz <= 0 || toHz <= 0)
            {
              continue;
            }
          if (fromHz > toHz)
            {
              std::swap (fromHz, toHz);
            }
          parsed.push_back ({
              fromHz,
              toHz,
              jsonString (object, QStringLiteral ("label")).left (64)
          });
          if (parsed.size () >= 32u)
            {
              break;
            }
        }
      if (!parsed.empty ())
        {
          allowedQsyRanges = parsed;
        }
    }

  QJsonArray const storedFrequencySchedule =
      root.value (QStringLiteral ("frequencySchedule")).toArray ();
  if (!storedFrequencySchedule.isEmpty ())
    {
      std::vector<FrequencyScheduleEntry> parsed;
      for (QJsonValue const& value : storedFrequencySchedule)
        {
          QJsonObject const object = value.toObject ();
          FrequencyScheduleEntry entry;
          entry.startMinute = std::clamp (
              jsonInt (object, QStringLiteral ("startMinute")), 0, 1439);
          entry.endMinute = std::clamp (
              jsonInt (object, QStringLiteral ("endMinute")), 0, 1439);
          entry.action = sanitizedScheduleAction (
              object.value (QStringLiteral ("action")).toString ());
          entry.dialFrequencyHz = static_cast<qint64> (
              jsonU64 (object, QStringLiteral ("dialFrequencyHz")));
          entry.label = jsonString (
              object, QStringLiteral ("label")).left (64);
          entry.cqType = sanitizedCqType (
              object.value (QStringLiteral ("cqType")).toString ());
          if (entry.dialFrequencyHz <= 0)
            {
              continue;
            }
          parsed.push_back (entry);
          if (parsed.size () >= 48u)
            {
              break;
            }
        }
      frequencySchedule = parsed;
    }
  else
    {
      QString const scheduleText = jsonString (
          root, QStringLiteral ("frequencyScheduleText"));
      if (!scheduleText.trimmed ().isEmpty ())
        {
          frequencySchedule = parseFrequencyScheduleText (scheduleText);
        }
    }

  auto trimVector = [] (auto& vector) {
    if (vector.size () > 100u)
      {
        vector.erase (
            vector.begin (),
            vector.begin ()
            + static_cast<typename std::decay<decltype (vector)>::type::difference_type> (
                vector.size () - 100u));
      }
  };
  trimVector (broadcasts);
  trimVector (alerts);
  trimVector (mailbox);
  trimVector (forms);
  trimVector (fileTransfers);
  trimVector (bulletins);
  trimVector (pingLog);
  if (pathReports.size () > 200u)
    {
      pathReports.erase (
          pathReports.begin (),
          pathReports.begin ()
          + static_cast<std::vector<PathReport>::difference_type> (
              pathReports.size () - 200u));
    }

  auto nextU32 = [] (quint32 stored, quint32 maxSeen) {
    quint64 candidate = std::max<quint64> (
        stored, static_cast<quint64> (maxSeen) + 1u);
    if (candidate == 0u
        || candidate > std::numeric_limits<quint32>::max ())
      {
        candidate = 1u;
      }
    return static_cast<quint32> (candidate);
  };

  m_broadcasts = broadcasts;
  m_alerts = alerts;
  m_mailbox = mailbox;
  m_forms = forms;
  m_fileTransfers = fileTransfers;
  m_bulletins = bulletins;
  m_contactHistory = contacts;
  m_qsoLog = qsoLog;
  m_logbookOutbox = logbookOutbox;
  m_pingLog = pingLog;
  m_pathReports = pathReports;
  m_beaconHistory = beaconHistory;
  m_clusterLastHeard = clusterLastHeard;
  m_customCannedMessages = customCannedMessages;
  m_customAlertTags = customAlertTags;
  m_blockedCalls = blockedCalls;
  m_frequencyPresets = frequencyPresets;
  m_allowedQsyRanges = allowedQsyRanges;
  m_frequencySchedule = frequencySchedule;
  m_awayEnabled = awayEnabledValue;
  m_awayAcceptsQsy = awayAcceptsQsyValue;
  m_awayMessage = awayMessageValue;
  m_welcomeEnabled = welcomeEnabledValue;
  m_welcomeMessage = welcomeMessageValue;
  m_autoReplyEnabled = autoReplyEnabledValue;
  m_autoAwayEnabled = autoAwayEnabledValue;
  m_autoAwayMinutes = autoAwayMinutesValue;
  m_autoAwayActivated = false;
  m_callIdIntervalMinutes = callIdIntervalMinutesValue;
  m_autoDisconnectMinutes = autoDisconnectMinutesValue;
  m_incomingPingsEnabled = incomingPingsEnabledValue;
  m_lastHeardPeekingEnabled = lastHeardPeekingEnabledValue;
  m_lastConnectionsPeekingEnabled = lastConnectionsPeekingEnabledValue;
  m_parkedVmailPeekingEnabled = parkedVmailPeekingEnabledValue;
  m_vmailParkingEnabled = vmailParkingEnabledValue;
  m_snrReportSendingEnabled = snrReportSendingEnabledValue;
  m_verboseSnrAutoAcceptEnabled = verboseSnrAutoAcceptEnabledValue;
  m_infoInquireEnabled = infoInquireEnabledValue;
  m_clusterEnabled = clusterEnabledValue;
  m_clusterNodeId = clusterNodeIdValue;
  m_clusterBand = clusterBandValue;
  m_clusterDialFrequencyHz = clusterDialFrequencyHzValue;
  m_lastCallIdQueuedAtMs.clear ();
  m_pendingPings.clear ();
  m_nextMailboxId = nextU32 (
      jsonU32 (root, QStringLiteral ("nextMailboxId"), 1u),
      maxMailboxId);
  m_nextFormId = nextU32 (
      jsonU32 (root, QStringLiteral ("nextFormId"), 1u),
      maxFormId);
  m_nextFileTransferId = nextU32 (
      jsonU32 (root, QStringLiteral ("nextFileTransferId"), 1u),
      maxFileTransferId);
  m_nextBulletinId = nextU32 (
      jsonU32 (root, QStringLiteral ("nextBulletinId"), 1u),
      maxBulletinId);
  m_nextPathReportId = nextU32 (
      jsonU32 (root, QStringLiteral ("nextPathReportId"), 1u),
      maxPathReportId);
  m_nextLogbookUploadId = nextU32 (
      jsonU32 (root, QStringLiteral ("nextLogbookUploadId"), 1u),
      maxLogbookUploadId);
  m_nextPingToken = jsonU16 (
      root, QStringLiteral ("nextPingToken"), 1u);
  if (m_nextPingToken == 0u)
    {
      m_nextPingToken = 1u;
    }
  m_beaconsSent = jsonU64 (root, QStringLiteral ("beaconsSent"));
  m_beaconsReceived = jsonU64 (root, QStringLiteral ("beaconsReceived"));
  m_cqsSent = jsonU64 (root, QStringLiteral ("cqsSent"));
  m_cqsReceived = jsonU64 (root, QStringLiteral ("cqsReceived"));
  emit cannedMessagesChanged ();
  emit alertTagsChanged ();
  emit blockListChanged ();
  emit frequencyPlanChanged ();
  emit beaconHistoryChanged ();
  emit clusterLastHeardChanged ();
  emit logbookOutboxChanged ();
  emit presenceChanged ();
  emit qsoAutomationChanged ();
  return true;
}

void FT2LinkQmlAdapter::setLocalStoreState (QString const& path,
                                            bool loaded,
                                            QString const& error)
{
  bool const changed = m_localStorePath != path
      || m_localStoreLoaded != loaded
      || m_lastLocalStoreError != error;
  m_localStorePath = path;
  m_localStoreLoaded = loaded;
  m_lastLocalStoreError = error;
  if (changed)
    {
      emit localStoreChanged ();
    }
}

void FT2LinkQmlAdapter::persistLocalStore ()
{
  if (m_loadingLocalStore || !m_localStorePersistenceEnabled)
    {
      return;
    }
  saveLocalStore ();
  writeAdifLogFile ();
}

int FT2LinkQmlAdapter::knownStationCount () const
{
  int count = 0;
  std::vector<StationAdvertisement> const active = m_model.activeStations (
      std::numeric_limits<std::uint64_t>::max (),
      std::numeric_limits<std::uint64_t>::max ());
  for (StationAdvertisement const& station : active)
    {
      if (!isCallBlocked (
              QString::fromStdString (station.station.call)))
        {
          ++count;
        }
    }
  return count;
}
