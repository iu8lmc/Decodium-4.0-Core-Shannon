#ifndef REVISION_UTILS_HPP__
#define REVISION_UTILS_HPP__

#include <QString>

QString revision (QString const& svn_rev_string = QString {});
QString version (bool include_patch = true);
QString fork_release_version ();
QString testVer ();     //avt 1/29/26
QString program_title (QString const& revision = QString {});

#endif
