#include "DecodiumBridgeTxFormat.h"
#include <QList>
#include "src/core/PrecisionTime.hpp"

void decodiumBridgeAudioLevelLog(const QString&);

static QString sampleFormatName(QAudioFormat::SampleFormat fmt)
{
    switch (fmt) {
    case QAudioFormat::UInt8: return "UInt8";
    case QAudioFormat::Int16: return "Int16";
    case QAudioFormat::Int32: return "Int32";
    case QAudioFormat::Float: return "Float";
    case QAudioFormat::Unknown:
    default:
        return "Unknown";
    }
}

QString audioFormatToString(const QAudioFormat& fmt)
{
    if (!fmt.isValid()) {
        return QStringLiteral("invalid");
    }
    return QString("rate=%1 ch=%2 fmt=%3 bytes=%4")
        .arg(fmt.sampleRate())
        .arg(fmt.channelCount())
        .arg(sampleFormatName(fmt.sampleFormat()))
        .arg(fmt.bytesPerFrame());
}

static QAudioFormat makeAudioFormat(int sampleRate, int channelCount, QAudioFormat::SampleFormat sampleFormat)
{
    channelCount = channelCount <= 1 ? 1 : 2;
    QAudioFormat fmt;
    fmt.setSampleRate(sampleRate);
    fmt.setChannelCount(channelCount);
    if (channelCount == 1) {
        fmt.setChannelConfig(QAudioFormat::ChannelConfigMono);
    } else if (channelCount == 2) {
        fmt.setChannelConfig(QAudioFormat::ChannelConfigStereo);
    }
    fmt.setSampleFormat(sampleFormat);
    return fmt;
}

bool sameAudioFormat(const QAudioFormat& a, const QAudioFormat& b)
{
    return a.sampleRate() == b.sampleRate()
        && a.channelCount() == b.channelCount()
        && a.sampleFormat() == b.sampleFormat();
}

static int txPcmChannelCountForDeviceFormat(const QAudioFormat& preferred)
{
    return preferred.channelCount() <= 1 ? 1 : 2;
}

QAudioFormat chooseTxAudioFormat(const QAudioDevice& device)
{
    const QAudioFormat preferred = device.preferredFormat();
    const int requestedChannels = txPcmChannelCountForDeviceFormat(preferred);
    const QAudioFormat requested = makeAudioFormat(48000, requestedChannels, QAudioFormat::Int16);

    decodiumBridgeAudioLevelLog("TX audio requested: " + audioFormatToString(requested));
    decodiumBridgeAudioLevelLog("TX audio preferred: " + audioFormatToString(preferred) +
              " dev=" + device.description());

    QList<QAudioFormat> candidates;
#if defined(Q_OS_MAC)
    QAudioFormat stereoPreferred48k = makeAudioFormat(48000,
                                                      qMin(2, qMax(1, preferred.channelCount())),
                                                      QAudioFormat::Int16);
    if (preferred.channelCount() > 1)
        candidates.append(stereoPreferred48k);
#elif defined(Q_OS_LINUX)
    if (preferred.channelCount() > 1) {
        candidates.append(makeAudioFormat(48000, 2, QAudioFormat::Int16));
    }
#elif defined(Q_OS_WIN)
    if (preferred.channelCount() > 1) {
        candidates.append(makeAudioFormat(48000, qMin(2, preferred.channelCount()), QAudioFormat::Int16));
    }
#endif

    candidates.append(requested);

    if (preferred.channelCount() > 0) {
        candidates.append(makeAudioFormat(48000, txPcmChannelCountForDeviceFormat(preferred), QAudioFormat::Int16));
    }
    if (preferred.sampleFormat() != QAudioFormat::Unknown) {
        candidates.append(makeAudioFormat(48000, 1, preferred.sampleFormat()));
        if (preferred.channelCount() > 0) {
            candidates.append(makeAudioFormat(48000, txPcmChannelCountForDeviceFormat(preferred), preferred.sampleFormat()));
        }
    }
    if (preferred.sampleRate() > 0) {
        candidates.append(makeAudioFormat(preferred.sampleRate(), 1, QAudioFormat::Int16));
        if (preferred.channelCount() > 0) {
            candidates.append(makeAudioFormat(preferred.sampleRate(), txPcmChannelCountForDeviceFormat(preferred), QAudioFormat::Int16));
        }
        if (preferred.sampleFormat() != QAudioFormat::Unknown) {
            candidates.append(makeAudioFormat(preferred.sampleRate(), 1, preferred.sampleFormat()));
            if (preferred.channelCount() > 0) {
                candidates.append(makeAudioFormat(preferred.sampleRate(), txPcmChannelCountForDeviceFormat(preferred), preferred.sampleFormat()));
            }
        }
    }
    if (preferred.isValid()) {
        candidates.append(makeAudioFormat(preferred.sampleRate(),
                                          txPcmChannelCountForDeviceFormat(preferred),
                                          preferred.sampleFormat()));
    }

    QList<QAudioFormat> uniqueCandidates;
    for (const QAudioFormat& candidate : candidates) {
        if (!candidate.isValid() || candidate.sampleRate() <= 0 || candidate.channelCount() <= 0
            || candidate.sampleFormat() == QAudioFormat::Unknown) {
            continue;
        }
        bool duplicate = false;
        for (const QAudioFormat& existing : uniqueCandidates) {
            if (sameAudioFormat(candidate, existing)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) uniqueCandidates.append(candidate);
    }

    for (const QAudioFormat& candidate : uniqueCandidates) {
        if (device.isFormatSupported(candidate)) {
            decodiumBridgeAudioLevelLog("TX audio chosen: " + audioFormatToString(candidate) +
                      " dev=" + device.description());
            return candidate;
        }
    }

    decodiumBridgeAudioLevelLog("TX audio fallback to requested format: " + audioFormatToString(requested) +
              " dev=" + device.description());
    return requested;
}


QString audioStateToString(QAudio::State state)
{
    switch (state) {
    case QAudio::ActiveState: return QStringLiteral("ActiveState");
    case QAudio::SuspendedState: return QStringLiteral("SuspendedState");
    case QAudio::StoppedState: return QStringLiteral("StoppedState");
    case QAudio::IdleState: return QStringLiteral("IdleState");
    }
    return QStringLiteral("UnknownState(%1)").arg(static_cast<int>(state));
}

QString audioErrorToString(QAudio::Error error)
{
    switch (error) {
    case QAudio::NoError: return QStringLiteral("NoError");
    case QAudio::OpenError: return QStringLiteral("OpenError");
    case QAudio::IOError: return QStringLiteral("IOError");
    case QAudio::UnderrunError: return QStringLiteral("UnderrunError");
    case QAudio::FatalError: return QStringLiteral("FatalError");
    }
    return QStringLiteral("UnknownError(%1)").arg(static_cast<int>(error));
}

qsizetype txAudioBufferBytesForMode(const QAudioFormat& format, const QString& mode,
                                           bool tune, bool conservative)
{
    if (!format.isValid()) {
        return 0;
    }
    static_cast<void>(conservative);

    int frames = tune ? 16384 : (mode == QStringLiteral("FT2") ? 8192 : 16384);
#if defined(Q_OS_WIN)
    // Older Windows machines are more likely to underrun the output callback
    // while FT8/FT4 decode/UI work is still settling. Keep FT2 lower to avoid
    // pushing a short 3.75 s payload too far into the slot.
    frames = tune ? 32768 : (mode == QStringLiteral("FT2") ? 12288 : 32768);
    if (conservative && !tune) {
        frames = (mode == QStringLiteral("FT2")) ? 16384 : 65536;
    }
#elif defined(Q_OS_LINUX)
    // PipeWire/PulseAudio can expose USB radios as stereo profiles and may
    // underrun small pull buffers while the UI/decode threads are active.
    frames = tune ? 65536 : (mode == QStringLiteral("FT2") ? 16384 : 32768);
#endif
    return static_cast<qsizetype>(format.bytesForFrames(frames));
}

int txAudioPrecomputeRetryDelayMs(const QString& mode, bool monitoring)
{
    if (!monitoring) {
        return 0;
    }

    QString const normalized = mode.trimmed().toUpper();
    int periodMs = 0;
    int quietAfterSlotStartMs = 0;
    int quietBeforeSlotEndMs = 0;
    if (normalized == QStringLiteral("FT8")) {
        periodMs = 15000;
        quietAfterSlotStartMs = 8500;
        quietBeforeSlotEndMs = 1200;
    } else if (normalized == QStringLiteral("FT4")) {
        periodMs = 7500;
        quietAfterSlotStartMs = 3500;
        quietBeforeSlotEndMs = 800;
    } else if (normalized == QStringLiteral("FT2")) {
        periodMs = 3750;
        quietAfterSlotStartMs = 900;
        quietBeforeSlotEndMs = 450;
    } else {
        return 0;
    }

    qint64 const nowMs = ntpCorrectedCurrentMSecsSinceEpoch();
    int const msInSlot = static_cast<int>(nowMs % periodMs);
    if (msInSlot < quietAfterSlotStartMs) {
        return qMax(50, quietAfterSlotStartMs - msInSlot);
    }
    int const latestSafeMs = periodMs - quietBeforeSlotEndMs;
    if (msInSlot > latestSafeMs) {
        return qMax(50, periodMs - msInSlot + quietAfterSlotStartMs);
    }
    return 0;
}

