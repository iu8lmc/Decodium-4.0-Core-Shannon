// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QSettings>
#include <QString>

namespace secure_settings {

enum class SettingsBlobState { Missing, Found, Error };

struct SettingsBlobLookup final {
    SettingsBlobState state {SettingsBlobState::Error};
    QByteArray value;
};

// A present but empty encrypted value is corruption, not an absent identity.
inline SettingsBlobLookup lookupSettingsBlob(QSettings& settings, const QString& key)
{
    settings.sync();
    if (settings.status() != QSettings::NoError) return {};
    if (!settings.contains(key)) return {SettingsBlobState::Missing, {}};
    const auto value = settings.value(key).toByteArray();
    if (settings.status() != QSettings::NoError || value.isEmpty()) return {};
    return {SettingsBlobState::Found, value};
}

} // namespace secure_settings
