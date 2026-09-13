#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace decodium {
namespace host_process {

inline void restoreHostEnvironmentVariable(QProcessEnvironment& environment,
                                           QString const& variable,
                                           bool removeWhenSnapshotMissing = true)
{
    QString const backup = QStringLiteral("DECODIUM_HOST_") + variable;
    if (environment.contains(backup)) {
        QString const hostValue = environment.value(backup);
        if (hostValue.isEmpty()) {
            environment.remove(variable);
        } else {
            environment.insert(variable, hostValue);
        }
        environment.remove(backup);
        return;
    }

    // Third-party/extracted AppImage launchers may not provide the host
    // snapshots. In that case it is safer to remove bundle-specific paths
    // before starting a host desktop helper.
    if (removeWhenSnapshotMissing
        && (environment.contains(QStringLiteral("APPIMAGE"))
            || environment.contains(QStringLiteral("APPDIR")))) {
        environment.remove(variable);
    }
}

inline QProcessEnvironment sanitized(QProcessEnvironment environment)
{
#if defined(Q_OS_LINUX)
    // Variables which may point into the AppImage must never leak into host
    // helpers.  KDE's xdg-open path can start Qt/KIO components, so cleaning
    // only the linker and GLib variables is not sufficient.
    const QStringList bundledVariables {
        QStringLiteral("LD_LIBRARY_PATH"),
        QStringLiteral("LD_PRELOAD"),
        QStringLiteral("GIO_EXTRA_MODULES"),
        QStringLiteral("GI_TYPELIB_PATH"),
        QStringLiteral("GSETTINGS_SCHEMA_DIR"),
        QStringLiteral("GTK_PATH"),
        QStringLiteral("XDG_DATA_DIRS"),
        QStringLiteral("QT_PLUGIN_PATH"),
        QStringLiteral("QT_QPA_PLATFORM_PLUGIN_PATH"),
        QStringLiteral("QML_IMPORT_PATH"),
        QStringLiteral("QML2_IMPORT_PATH"),
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("QT_QUICK_CONTROLS_STYLE"),
        QStringLiteral("QT_MEDIA_BACKEND")
    };
    for (QString const& variable : bundledVariables) {
        restoreHostEnvironmentVariable(environment, variable);
    }

    // Desktop/session discovery must survive sanitisation.  The outer
    // AppRun wrapper snapshots these values before linuxdeploy modifies the
    // environment.  Keep the current value when running under an older or
    // third-party wrapper which does not provide a snapshot.
    const QStringList desktopVariables {
        QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
        QStringLiteral("XDG_RUNTIME_DIR"),
        QStringLiteral("PATH"),
        QStringLiteral("DISPLAY"),
        QStringLiteral("XAUTHORITY"),
        QStringLiteral("WAYLAND_DISPLAY"),
        QStringLiteral("XDG_CURRENT_DESKTOP"),
        QStringLiteral("XDG_SESSION_DESKTOP"),
        QStringLiteral("XDG_SESSION_TYPE"),
        QStringLiteral("DESKTOP_SESSION"),
        QStringLiteral("KDE_FULL_SESSION"),
        QStringLiteral("KDE_SESSION_VERSION"),
        QStringLiteral("GNOME_DESKTOP_SESSION_ID"),
        QStringLiteral("BROWSER"),
        QStringLiteral("XDG_CONFIG_HOME"),
        QStringLiteral("XDG_DATA_HOME")
    };
    for (QString const& variable : desktopVariables) {
        restoreHostEnvironmentVariable(environment, variable, false);
    }

    // These identify the application bundle itself.  A browser or desktop
    // helper is a host process and must not mistake Decodium's AppImage for
    // its own runtime environment.
    environment.remove(QStringLiteral("APPIMAGE"));
    environment.remove(QStringLiteral("APPDIR"));
    environment.remove(QStringLiteral("ARGV0"));
    environment.remove(QStringLiteral("OWD"));
#endif
    return environment;
}

inline QProcessEnvironment sanitizedSystemEnvironment()
{
    // Desktop/session variables are restored from the AppRun snapshots when
    // available and otherwise retained for compatibility with older bundles.
    return sanitized(QProcessEnvironment::systemEnvironment());
}

} // namespace host_process
} // namespace decodium
