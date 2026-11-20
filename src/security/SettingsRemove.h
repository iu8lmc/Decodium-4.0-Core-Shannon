// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

namespace secure_settings {

inline bool removeSettingAndSync(QSettings& settings, const QString& key, QString* error)
{
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (error) *error = QObject::tr("DPAPI secure store removal failed");
        return false;
    }
    settings.remove(key);
    settings.sync();
    if (settings.status() == QSettings::NoError) return true;
    if (error) *error = QObject::tr("DPAPI secure store removal failed");
    return false;
}

} // namespace secure_settings
