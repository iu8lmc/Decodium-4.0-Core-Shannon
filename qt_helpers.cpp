#include "qt_helpers.hpp"

#include <QString>
#include <QFont>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QWidget>
#include <QStyle>
#include <QVariant>
#include <QDateTime>

QString font_as_stylesheet (QFont const& font)
{
  QString font_weight;
  switch (font.weight ())
    {
    case QFont::Light: font_weight = "light"; break;
    case QFont::Normal: font_weight = "normal"; break;
    case QFont::DemiBold: font_weight = "demibold"; break;
    case QFont::Bold: font_weight = "bold"; break;
    case QFont::Black: font_weight = "black"; break;
    default: font_weight = "normal"; break;  // Qt6 added Thin/ExtraLight/Medium/ExtraBold
    }
  return QString {
      " font-family: %1;\n"
      " font-size: %2pt;\n"
      " font-style: %3;\n"
      " font-weight: %4;\n"}
  .arg (font.family ())
     .arg (font.pointSize ())
  .arg (font.styleName ())
     .arg (font_weight);
}

QString bundled_resource_path (QString const& relative_path)
{
  QString normalized = QDir::cleanPath (relative_path);
  if (normalized.startsWith ("./"))
    {
      normalized.remove (0, 2);
    }

  QDir const app_dir {QCoreApplication::applicationDirPath ()};

#if defined (Q_OS_MAC)
  QString const mac_resource_candidate = QDir::cleanPath (app_dir.absoluteFilePath ("../Resources/" + normalized));
  if (QFileInfo::exists (mac_resource_candidate))
    {
      return mac_resource_candidate;
    }
#endif

  QString const app_dir_candidate = QDir::cleanPath (app_dir.absoluteFilePath (normalized));
  if (QFileInfo::exists (app_dir_candidate))
    {
      return app_dir_candidate;
    }

#if defined (Q_OS_MAC)
  return QDir::cleanPath (app_dir.absoluteFilePath ("../Resources/" + normalized));
#else
  return app_dir_candidate;
#endif
}

QString bundled_sound_path (QString const& file_name)
{
  return bundled_resource_path (QStringLiteral ("sounds/") + file_name);
}

void update_dynamic_property (QWidget * widget, char const * property, QVariant const& value)
{
  widget->setProperty (property, value);
  widget->style ()->unpolish (widget);
  widget->style ()->polish (widget);
  widget->update ();
}

QDateTime qt_round_date_time_to (QDateTime dt, int milliseconds)
{
  dt.setMSecsSinceEpoch (dt.addMSecs (milliseconds / 2).toMSecsSinceEpoch () / milliseconds * milliseconds);
  return dt;
}

QDateTime qt_truncate_date_time_to (QDateTime dt, int milliseconds)
{
  dt.setMSecsSinceEpoch (dt.toMSecsSinceEpoch () / milliseconds * milliseconds);
  return dt;
}

bool ft2_allow_assisted_directed_reply_context (bool transmitting,
                                                bool calling_cq,
                                                bool auto_reply,
                                                bool has_active_partner)
{
  Q_UNUSED (auto_reply);
  return has_active_partner
      || (transmitting && !calling_cq);
}
