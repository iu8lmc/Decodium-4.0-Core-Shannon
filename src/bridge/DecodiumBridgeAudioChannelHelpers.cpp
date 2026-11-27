#include "Audio/AudioDevice.hpp"
#include <QAudioFormat>
#include <QString>

static QString audioChannelSettingValue(int channel)
{
    return QString::fromLatin1(
        AudioDevice::toString(static_cast<AudioDevice::Channel>(qBound(0, channel, 3))));
}

QString decodiumBridgeAudioChannelSettingValue(int channel) { return audioChannelSettingValue(channel); }

#if defined(Q_OS_MAC)
static AudioDevice::Channel txOutputChannelForFormat(const QAudioFormat& format, int configuredChannel)
{
    AudioDevice::Channel const requestedChannel =
        static_cast<AudioDevice::Channel>(qBound(0, configuredChannel, 3));
    if (format.channelCount() <= 1) {
        return AudioDevice::Mono;
    }
    if (requestedChannel == AudioDevice::Mono) {
        return AudioDevice::Both;
    }
    return requestedChannel;
}
AudioDevice::Channel decodiumBridgeTxOutputChannelForFormat(const QAudioFormat& format, int channel) { return txOutputChannelForFormat(format, channel); }
#endif

// ── helpers PTT / freq che delegano al backend attivo ────────────────────────
