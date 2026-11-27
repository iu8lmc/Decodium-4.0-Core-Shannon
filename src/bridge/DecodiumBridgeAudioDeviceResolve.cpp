#include "DecodiumBridgeAudioDeviceIdentity.h"
#include "DecodiumProfileSettings.h"
#include <QAudioDevice>
#include <QList>
#include <QRegularExpression>
#include <QSettings>
#include <QString>

static QString normalizedAudioDeviceNameForMatch(QString value)
{
    QString normalized = value.trimmed().simplified();
    // Windows may rename the same endpoint as "Speakers (3- USB Audio CODEC )"
    // after replugging. Treat that volatile numeric prefix as non-identifying.
    normalized.replace(QRegularExpression(QStringLiteral(R"(\(\s*\d+\s*-\s*)")),
                       QStringLiteral("("));
    normalized.replace(QRegularExpression(QStringLiteral(R"(^\s*\d+\s*-\s*)")),
                       QString());
    normalized.replace(QRegularExpression(QStringLiteral(R"(\s+\))")),
                       QStringLiteral(")"));
    return normalized;
}

static bool audioDeviceNameExactMatches(QString const& lhs, QString const& rhs)
{
    return !lhs.trimmed().isEmpty()
        && lhs.trimmed().compare(rhs.trimmed(), Qt::CaseInsensitive) == 0;
}

static bool audioDeviceSavedIdentityMatches(QString const& savedName,
                                            QString const& savedId,
                                            QAudioDevice const& device)
{
    QString const deviceId = decodiumBridgeAudioDeviceIdForSettings(device);
    QString const id = savedId.trimmed();
    if (!id.isEmpty() && !deviceId.isEmpty()) {
        return id.compare(deviceId, Qt::CaseInsensitive) == 0;
    }
    return audioDeviceNameExactMatches(savedName, device.description());
}

bool decodiumBridgeAudioDeviceSavedIdentityMatches(QString const& name, QString const& id, QAudioDevice const& device)
{
    return audioDeviceSavedIdentityMatches(name, id, device);
}

static bool tryResolveAudioDeviceByIdentity(QList<QAudioDevice> const& devices,
                                            QString const& requestedName,
                                            QString const& requestedId,
                                            QAudioDevice* resolvedDevice,
                                            QString* matchReason)
{
    if (resolvedDevice) {
        *resolvedDevice = QAudioDevice {};
    }
    auto setReason = [matchReason](QString const& reason) {
        if (matchReason) {
            *matchReason = reason;
        }
    };

    QString const requested = requestedName.trimmed();
    QString const id = requestedId.trimmed();
    if (!id.isEmpty()) {
        for (QAudioDevice const& device : devices) {
            if (decodiumBridgeAudioDeviceIdMatches(device, id)) {
                if (resolvedDevice) {
                    *resolvedDevice = device;
                }
                setReason(QStringLiteral("id"));
                return true;
            }
        }
    }

    if (requested.isEmpty()) {
        setReason(QStringLiteral("default"));
        return false;
    }

    QList<QAudioDevice> exactMatches;
    for (QAudioDevice const& device : devices) {
        if (audioDeviceNameExactMatches(device.description(), requested)) {
            exactMatches.append(device);
        }
    }
    if (exactMatches.size() == 1) {
        if (resolvedDevice) {
            *resolvedDevice = exactMatches.constFirst();
        }
        setReason(id.isEmpty() ? QStringLiteral("exact-name")
                               : QStringLiteral("exact-name-id-missing"));
        return true;
    }
    if (exactMatches.size() > 1) {
        setReason(QStringLiteral("ambiguous-exact-name count=%1").arg(exactMatches.size()));
        return false;
    }

    QString const normalizedRequested = normalizedAudioDeviceNameForMatch(requested);
    QList<QAudioDevice> normalizedMatches;
    if (!normalizedRequested.isEmpty()) {
        for (QAudioDevice const& device : devices) {
            QString const normalizedDevice =
                normalizedAudioDeviceNameForMatch(device.description());
            if (normalizedDevice.compare(normalizedRequested, Qt::CaseInsensitive) == 0) {
                normalizedMatches.append(device);
            }
        }
    }
    if (normalizedMatches.size() == 1) {
        if (resolvedDevice) {
            *resolvedDevice = normalizedMatches.constFirst();
        }
        setReason(id.isEmpty() ? QStringLiteral("normalized-name")
                               : QStringLiteral("normalized-name-id-missing"));
        return true;
    }
    if (normalizedMatches.size() > 1) {
        setReason(QStringLiteral("ambiguous-normalized-name count=%1").arg(normalizedMatches.size()));
        return false;
    }

    setReason(id.isEmpty() ? QStringLiteral("not-found")
                           : QStringLiteral("id-and-name-not-found"));
    return false;
}

bool decodiumBridgeResolveAudioDeviceByIdentity(QList<QAudioDevice> const& devices, QString const& name, QString const& id, QAudioDevice* device, QString* reason)
{
    return tryResolveAudioDeviceByIdentity(devices, name, id, device, reason);
}

static bool hasAudioDeviceNamed(QList<QAudioDevice> const& devices, QString const& name)
{
    if (name.trimmed().isEmpty()) {
        return false;
    }
    QAudioDevice ignored;
    QString reason;
    return tryResolveAudioDeviceByIdentity(devices, name, QString(), &ignored, &reason);
}

bool decodiumBridgeHasAudioDeviceNamed(QList<QAudioDevice> const& devices, QString const& name)
{
    return hasAudioDeviceNamed(devices, name);
}

static QString audioDeviceSelectionForLog(QString const& name, QString const& id)
{
    QString const trimmedName = name.trimmed();
    QString const trimmedId = id.trimmed();
    return QStringLiteral("[%1] id=%2")
        .arg(trimmedName.isEmpty() ? QStringLiteral("<default>") : trimmedName,
             decodiumBridgeAudioDeviceIdSettingForLog(trimmedId));
}

QString decodiumBridgeAudioDeviceSelectionForLog(const QString& name, const QString& id) { return audioDeviceSelectionForLog(name, id); }

static void persistSingleAudioIdentity(bool input, QString const& name, QString const& id)
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(s);
    if (input) {
        s.setValue(QStringLiteral("audioInputDevice"), name);
        s.setValue(QStringLiteral("SoundInName"), name);
        if (id.trimmed().isEmpty()) {
            s.remove(QStringLiteral("audioInputDeviceId"));
        } else {
            s.setValue(QStringLiteral("audioInputDeviceId"), id.trimmed());
        }
    } else {
        s.setValue(QStringLiteral("audioOutputDevice"), name);
        s.setValue(QStringLiteral("SoundOutName"), name);
        if (id.trimmed().isEmpty()) {
            s.remove(QStringLiteral("audioOutputDeviceId"));
        } else {
            s.setValue(QStringLiteral("audioOutputDeviceId"), id.trimmed());
        }
    }
    s.sync();
}

bool decodiumBridgeAudioDeviceNameExactMatches(const QString& lhs, const QString& rhs)
{
    return audioDeviceNameExactMatches(lhs, rhs);
}
void decodiumBridgePersistSingleAudioIdentity(bool input, const QString& name, const QString& id)
{
    persistSingleAudioIdentity(input, name, id);
}

