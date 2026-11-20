#include "DecodiumBridgeAudioDeviceIdentity.h"

static QString audioDeviceIdForLog(const QAudioDevice& device)
{
    const QByteArray id = device.id();
    return id.isEmpty() ? QStringLiteral("<empty>") : QString::fromLatin1(id.toHex());
}

static QString audioDeviceIdForSettings(const QAudioDevice& device)
{
    const QByteArray id = device.id();
    return id.isEmpty() ? QString() : QString::fromLatin1(id.toHex());
}

QString decodiumBridgeAudioDeviceIdForSettings(const QAudioDevice& device) { return audioDeviceIdForSettings(device); }

static QString audioDeviceIdSettingForLog(QString id)
{
    id = id.trimmed();
    return id.isEmpty() ? QStringLiteral("<unset>") : id;
}

QString decodiumBridgeAudioDeviceIdSettingForLog(QString id) { return audioDeviceIdSettingForLog(id); }

static bool audioDeviceIdMatches(const QAudioDevice& device, QString const& savedId)
{
    QString const candidate = audioDeviceIdForSettings(device);
    QString const requested = savedId.trimmed();
    return !candidate.isEmpty()
        && !requested.isEmpty()
        && candidate.compare(requested, Qt::CaseInsensitive) == 0;
}

static QString audioFormatForLog(const QAudioFormat& format)
{
    return QStringLiteral("rate=%1 channels=%2 sampleFormat=%3")
        .arg(format.sampleRate())
        .arg(format.channelCount())
        .arg(static_cast<int>(format.sampleFormat()));
}



QString decodiumBridgeAudioDeviceIdForLog(const QAudioDevice& device) { return audioDeviceIdForLog(device); }
bool decodiumBridgeAudioDeviceIdMatches(const QAudioDevice& device, const QString& id) { return audioDeviceIdMatches(device, id); }
QString decodiumBridgeAudioFormatForLog(const QAudioFormat& format) { return audioFormatForLog(format); }
