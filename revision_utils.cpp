#include "revision_utils.hpp"

#include <cstring>

#include <QCoreApplication>
#include <QRegularExpression>

#include "scs_version.h"

#ifndef FORK_RELEASE_VERSION
#define FORK_RELEASE_VERSION "dev"
#endif

namespace
{
  QString revision_extract_number (QString const& s)
  {
    QString revision;

    // try and match a number (hexadecimal allowed)
    QRegularExpression re {R"(^[$:]\w+: (r?[\da-f]+[^$]*)\$$)"};
    auto match = re.match (s);
    if (match.hasMatch ())
      {
        revision = match.captured (1);
      }
    return revision;
  }
}

QString revision (QString const& scs_rev_string)
{
  QString result;
  auto revision_from_scs = revision_extract_number (scs_rev_string);

#if defined (CMAKE_BUILD)
  QString scs_info {":Rev: " SCS_VERSION_STR " $"};

  auto revision_from_scs_info = revision_extract_number (scs_info);
  if (!revision_from_scs_info.isEmpty ())
    {
      // we managed to get the revision number from svn info etc.
      result = revision_from_scs_info;
    }
  else if (!revision_from_scs.isEmpty ())
    {
      // fall back to revision passed in if any
      result = revision_from_scs;
    }
  else
    {
      // match anything
      QRegularExpression re {R"(^[$:]\w+: ([^$]*)\$$)"};
      auto match = re.match (scs_info);
      if (match.hasMatch ())
        {
          result = match.captured (1);
        }
    }
#else
  if (!revision_from_scs.isEmpty ())
    {
      // not CMake build so all we have is revision passed
      result = revision_from_scs;
    }
#endif
  return result.trimmed ();
}

QString version (bool include_patch)
{
#if defined (CMAKE_BUILD)
  auto const release_version = fork_release_version ();
  auto const parts = release_version.split ('.');
  QString v {release_version};
  if (!include_patch && parts.size () >= 2)
    {
      v = parts.at (0) + "." + parts.at (1);
    }
#else
  QString v {"Not for Release"};
#endif
  return v;
}

QString fork_release_version ()
{
  return QStringLiteral (FORK_RELEASE_VERSION);
}

QString program_title (QString const& revision)
{
  QString id {QString {"Decodium | Fork by Salvatore Raccampo 9H1SR %1"}
               .arg (fork_release_version ())};
  return id + " " + revision;
}
