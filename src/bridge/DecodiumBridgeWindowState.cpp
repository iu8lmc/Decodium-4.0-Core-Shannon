#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include <QRunnable>
#include <QSettings>
#include <QThreadPool>

QThreadPool& decodiumBridgeSettingsWriteThreadPool();
void decodiumBridgeWindowStateLog(const QString& message);

QVariantMap DecodiumBridge::loadWindowState(const QString& key) const
{
    QVariantMap state;
    QString const normalizedKey = key.trimmed();
    if (normalizedKey.isEmpty()) {
        return state;
    }

    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    s.beginGroup(QStringLiteral("WindowState/%1").arg(normalizedKey));
    auto const maybeInsertInt = [&] (const char* field) {
        if (s.contains(QLatin1String(field))) {
            state.insert(QLatin1String(field), s.value(QLatin1String(field)).toInt());
        }
    };
    auto const maybeInsertBool = [&] (const char* field) {
        if (s.contains(QLatin1String(field))) {
            state.insert(QLatin1String(field), s.value(QLatin1String(field)).toBool());
        }
    };

    maybeInsertInt("x");
    maybeInsertInt("y");
    maybeInsertInt("width");
    maybeInsertInt("height");
    maybeInsertBool("detached");
    maybeInsertBool("minimized");
    maybeInsertBool("maximized");
    s.endGroup();
    return state;
}

void DecodiumBridge::saveWindowState(const QString& key,
                                     int x,
                                     int y,
                                     int width,
                                     int height,
                                     bool detached,
                                     bool minimized)
{
    QString const normalizedKey = key.trimmed();
    if (normalizedKey.isEmpty()) {
        return;
    }

    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    s.beginGroup(QStringLiteral("WindowState/%1").arg(normalizedKey));
    s.setValue(QStringLiteral("x"), x);
    s.setValue(QStringLiteral("y"), y);
    s.setValue(QStringLiteral("width"), width);
    s.setValue(QStringLiteral("height"), height);
    s.setValue(QStringLiteral("detached"), detached);
    s.setValue(QStringLiteral("minimized"), minimized);
    s.endGroup();
    s.sync();
}

void DecodiumBridge::saveWindowStatesAsync(const QVariantMap& states,
                                           const QVariantMap& layoutSettings)
{
    if (states.isEmpty() && layoutSettings.isEmpty()) {
        return;
    }

    // Copy the plain-value snapshot and create QSettings inside the writer
    // thread.  QSettings is a QObject: creating it on the GUI thread and
    // deleting it in a worker can leave its deferred UpdateRequest event
    // queued against a dead object, causing a crash in QSettings::event().
    // Keeping its full lifetime in one worker thread also moves all INI work
    // away from the panadapter/UI event loop.
    decodiumBridgeSettingsWriteThreadPool().start(QRunnable::create([states, layoutSettings]() {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        decodium::beginActiveSettingsProfile(settings);

        for (auto it = layoutSettings.constBegin(); it != layoutSettings.constEnd(); ++it) {
            QString const key = it.key().trimmed();
            if (!key.isEmpty()) {
                settings.setValue(key, it.value());
            }
        }

        static constexpr const char* fields[] = {
            "x", "y", "width", "height", "detached", "minimized", "maximized"
        };
        for (auto it = states.constBegin(); it != states.constEnd(); ++it) {
            QString const key = it.key().trimmed();
            QVariantMap const state = it.value().toMap();
            if (key.isEmpty() || state.isEmpty()) {
                continue;
            }

            settings.beginGroup(QStringLiteral("WindowState/%1").arg(key));
            for (const char* field : fields) {
                QString const fieldName = QString::fromLatin1(field);
                auto const value = state.constFind(fieldName);
                if (value != state.constEnd()) {
                    settings.setValue(fieldName, value.value());
                }
            }
            settings.endGroup();
        }

        settings.sync();
    }));
}

// 1.0.263 (fork-only) — Reset Layout
// Cancella tutto il gruppo "WindowState" da QSettings (rimuove coords/dimensioni
// di tutte le finestre floating salvate) e segnala al QML di ripristinare i
// default e ri-dockare le finestre. Usato quando una o piu' finestre finiscono
// su un monitor disconnesso o fuori dall'area visibile.
void DecodiumBridge::resetWindowLayout()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(s);
    s.remove(QStringLiteral("WindowState"));
    s.sync();
    decodiumBridgeWindowStateLog(QStringLiteral("resetWindowLayout: tutte le WindowState/* cancellate, signal emesso"));
    emit windowLayoutResetRequested();
}

