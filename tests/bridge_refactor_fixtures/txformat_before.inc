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

static QString audioFormatToString(const QAudioFormat& fmt)
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

static bool sameAudioFormat(const QAudioFormat& a, const QAudioFormat& b)
{
    return a.sampleRate() == b.sampleRate()
        && a.channelCount() == b.channelCount()
        && a.sampleFormat() == b.sampleFormat();
}

static int txPcmChannelCountForDeviceFormat(const QAudioFormat& preferred)
{
    return preferred.channelCount() <= 1 ? 1 : 2;
}

static QAudioFormat chooseTxAudioFormat(const QAudioDevice& device)
{
    const QAudioFormat preferred = device.preferredFormat();
    const int requestedChannels = txPcmChannelCountForDeviceFormat(preferred);
    const QAudioFormat requested = makeAudioFormat(48000, requestedChannels, QAudioFormat::Int16);

    bridgeLog("TX audio requested: " + audioFormatToString(requested));
    bridgeLog("TX audio preferred: " + audioFormatToString(preferred) +
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
            bridgeLog("TX audio chosen: " + audioFormatToString(candidate) +
                      " dev=" + device.description());
            return candidate;
        }
    }

    bridgeLog("TX audio fallback to requested format: " + audioFormatToString(requested) +
              " dev=" + device.description());
    return requested;
}

