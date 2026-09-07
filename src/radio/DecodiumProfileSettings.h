#ifndef DECODIUMPROFILESETTINGS_H
#define DECODIUMPROFILESETTINGS_H

#include <QCoreApplication>
#include <QHash>
#include <QSettings>
#include <QString>
#include <QVariant>

namespace decodium
{

inline QString activeSettingsProfileName()
{
    auto const* app = QCoreApplication::instance();
    if (!app) {
        return {};
    }
    return app->property("decodiumConfigName").toString().trimmed();
}

inline void ensureSettingsProfileInitialized(const QString& profileName)
{
    if (profileName.trimmed().isEmpty()) {
        return;
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("MultiSettings"));
    settings.beginGroup(profileName);
    bool const alreadyInitialized =
        settings.value(QStringLiteral("_ProfileSettingsInitialized"), false).toBool();
    settings.endGroup();
    settings.endGroup();
    if (alreadyInitialized) {
        return;
    }

    QHash<QString, QVariant> rootValues;
    for (QString const& key : settings.allKeys()) {
        if (key.startsWith(QStringLiteral("MultiSettings/"))) {
            continue;
        }
        rootValues.insert(key, settings.value(key));
    }

    settings.beginGroup(QStringLiteral("MultiSettings"));
    settings.beginGroup(profileName);
    for (auto it = rootValues.constBegin(); it != rootValues.constEnd(); ++it) {
        if (!settings.contains(it.key())) {
            settings.setValue(it.key(), it.value());
        }
    }
    settings.setValue(QStringLiteral("_ProfileSettingsInitialized"), true);
    settings.endGroup();
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
    settings.beginGroup(QStringLiteral("MultiSettings"));
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
