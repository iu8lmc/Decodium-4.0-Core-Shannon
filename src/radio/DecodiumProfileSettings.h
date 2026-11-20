#ifndef DECODIUMPROFILESETTINGS_H
#define DECODIUMPROFILESETTINGS_H

#include <QCoreApplication>
#include <QHash>
#include <QSettings>
#include <QSet>
#include <QString>
#include <QVariant>

namespace decodium
{

inline QString activeSettingsProfileName()
{
    auto const* app = QCoreApplication::instance();
    if (app) {
        QString const commandLineProfile = app->property("decodiumConfigName").toString().trimmed();
        if (!commandLineProfile.isEmpty()) {
            return commandLineProfile;
        }
    }

    // The settings UI selects a saved multi-settings configuration without
    // starting Decodium with `-config`.  Treat that persisted selection as the
    // active profile too; otherwise QML can display a profile value while C++
    // services silently fall back to the stale [General] value.
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    return settings.value(QStringLiteral("CurrentMultiSettingsConfiguration")).toString().trimmed();
}

inline void ensureSettingsProfileInitialized(const QString& profileName)
{
    if (profileName.trimmed().isEmpty()) {
        return;
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    // Decodium's QML settings profiles are stored as top-level INI groups
    // (`CODEXPERF\\MaxCallerRetries`), whereas the old WSJT-X MultiSettings
    // profiles live below `MultiSettings/`.  Prefer the direct group so the
    // control shown in QML and the values used by the bridge are identical.
    settings.beginGroup(profileName);
    bool const alreadyInitialized =
        settings.value(QStringLiteral("_ProfileSettingsInitialized"), false).toBool();
    // A completed profile already contains this marker. Avoid enumerating
    // every key on each settings read; leave the unmarked/legacy discovery
    // path intact and re-read the marker rather than caching profile state.
    bool const directProfileExists = alreadyInitialized
        || !settings.allKeys().isEmpty() || !settings.childGroups().isEmpty();
    settings.endGroup();
    if (directProfileExists || alreadyInitialized) {
      return;
    }

    settings.beginGroup(QStringLiteral("MultiSettings"));
    settings.beginGroup(profileName);
    bool const legacyProfileExists = !settings.allKeys().isEmpty() || !settings.childGroups().isEmpty();
    settings.endGroup();
    settings.endGroup();
    if (legacyProfileExists) return;

    // Other profiles are siblings of the global settings, not global
    // settings themselves. Copying them recursively makes each new profile
    // contain all earlier profiles and can grow the INI exponentially.
    QSet<QString> existingProfiles;
    settings.beginGroup(QStringLiteral("MultiSettings"));
    for (const QString& name : settings.childGroups()) existingProfiles.insert(name);
    settings.endGroup();
    for (const QString& name : settings.childGroups()) {
        if (name == QStringLiteral("MultiSettings")) continue;
        settings.beginGroup(name);
        if (settings.value(QStringLiteral("_ProfileSettingsInitialized"), false).toBool())
            existingProfiles.insert(name);
        settings.endGroup();
    }

    QHash<QString, QVariant> rootValues;
    for (QString const& key : settings.allKeys()) {
        const int separator = key.indexOf(QLatin1Char('/'));
        if (key.startsWith(QStringLiteral("MultiSettings/"))
            || key.startsWith(profileName + QLatin1Char('/'))
            || (separator > 0 && existingProfiles.contains(key.left(separator)))
            || key == QStringLiteral("CurrentMultiSettingsConfiguration")) {
            continue;
        }
        rootValues.insert(key, settings.value(key));
    }

    settings.beginGroup(profileName);
    for (auto it = rootValues.constBegin(); it != rootValues.constEnd(); ++it) {
        if (!settings.contains(it.key())) {
            settings.setValue(it.key(), it.value());
        }
    }
    settings.setValue(QStringLiteral("_ProfileSettingsInitialized"), true);
    settings.endGroup();
    settings.sync();
}

inline bool beginActiveSettingsProfile(QSettings& settings)
{
    QString const profileName = activeSettingsProfileName();
    if (profileName.isEmpty()) {
        return false;
    }

    ensureSettingsProfileInitialized(profileName);
    settings.beginGroup(profileName);
    bool const directProfileExists =
        settings.value(QStringLiteral("_ProfileSettingsInitialized"), false).toBool()
        || !settings.allKeys().isEmpty() || !settings.childGroups().isEmpty();
    settings.endGroup();
    if (!directProfileExists) {
        // Compatibility with profiles written by the historical WSJT-X
        // MultiSettings store. New Decodium profiles always take the direct
        // branch above.
        settings.beginGroup(QStringLiteral("MultiSettings"));
    }
    settings.beginGroup(profileName);
    return true;
}

inline QVariant profiledSettingsValue(const QString& group,
                                      const QString& key,
                                      const QVariant& defaultValue = QVariant {})
{
    QSettings profiled(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    if (beginActiveSettingsProfile(profiled)) {
        if (!group.isEmpty()) {
            profiled.beginGroup(group);
        }
        if (profiled.contains(key)) {
            return profiled.value(key, defaultValue);
        }
    }

    QSettings root(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    if (!group.isEmpty()) {
        root.beginGroup(group);
    }
    return root.value(key, defaultValue);
}

} // namespace decodium

#endif // DECODIUMPROFILESETTINGS_H
