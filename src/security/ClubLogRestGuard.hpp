#pragma once
#include <QCryptographicHash>
#include <QSettings>

namespace secure_settings {
struct ClubLogRestRestriction {
    bool refused{false};
    qint64 retryNotBeforeMs{0};
};

inline QString clubLogRestrictionKey(const QString& token)
{
    return QStringLiteral("ClubLogRestAccess/") + QString::fromLatin1(
        QCryptographicHash::hash(token.toUtf8(), QCryptographicHash::Sha256).toHex());
}

inline ClubLogRestRestriction readClubLogRestriction(QSettings& settings, const QString& token)
{
    if (!token.startsWith(QStringLiteral("cl_"))) return {};
    settings.sync();
    const QString key = clubLogRestrictionKey(token);
    return {settings.value(key + QStringLiteral("/refused"), false).toBool(),
            settings.value(key + QStringLiteral("/retryNotBeforeMs"), 0).toLongLong()};
}

inline void recordClubLogRestriction(QSettings& settings, const QString& token, bool refused, qint64 retryNotBeforeMs)
{
    if (!token.startsWith(QStringLiteral("cl_"))) return;
    const QString key = clubLogRestrictionKey(token);
    if (refused) settings.setValue(key + QStringLiteral("/refused"), true);
    if (retryNotBeforeMs > 0) {
        const qint64 previous = settings.value(key + QStringLiteral("/retryNotBeforeMs"), 0).toLongLong();
        settings.setValue(key + QStringLiteral("/retryNotBeforeMs"), qMax(previous, retryNotBeforeMs));
    }
    settings.sync();
}
}
