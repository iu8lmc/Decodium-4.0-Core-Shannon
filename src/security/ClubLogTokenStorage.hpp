#pragma once

#include "SecureSettings.hpp"
#include <QSettings>

namespace secure_settings {
// Never use the legacy plaintext fallback for a personal token. The marker
// prevents an old keychain value from reappearing after a failed replacement.
inline bool storeClubLogToken(QSettings& settings, const QString& service, const QString& token,
                              const Backend& backend = default_backend())
{
    const QString key = QStringLiteral("ClubLogApiKey");
    const QString marker = QStringLiteral("ClubLogTokenNeedsReentry");
    QString error;
    const bool stored = backend.available() && backend.protects_private_values()
                        && backend.store(service, key, token, &error);
    if (stored) {
        settings.setValue(key, placeholder());
        settings.remove(marker);
    } else {
        settings.remove(key);
        settings.setValue(marker, true);
    }
    settings.sync();
    return stored && settings.status() == QSettings::NoError;
}

inline QString loadClubLogCredential(QSettings& settings, const QString& service,
                                     const Backend& backend = default_backend())
{
    const QString key = QStringLiteral("ClubLogApiKey");
    const QString marker = QStringLiteral("ClubLogTokenNeedsReentry");
    const QString plain = settings.value(key).toString().trimmed();
    if (plain.startsWith(QStringLiteral("cl_"))) {
        // An explicitly configured token takes precedence over any older
        // credential. If secure storage fails, use it for this session only.
        storeClubLogToken(settings, service, plain, backend);
        return plain;
    }
    if (settings.value(marker, false).toBool()) return {};
    return load_or_import(&settings, service, key, settings.value(key).toString(), backend).trimmed();
}
}
