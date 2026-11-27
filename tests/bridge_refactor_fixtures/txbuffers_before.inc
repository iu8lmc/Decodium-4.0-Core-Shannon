static QString audioStateToString(QAudio::State state)
{
    switch (state) {
    case QAudio::ActiveState: return QStringLiteral("ActiveState");
    case QAudio::SuspendedState: return QStringLiteral("SuspendedState");
    case QAudio::StoppedState: return QStringLiteral("StoppedState");
    case QAudio::IdleState: return QStringLiteral("IdleState");
    }
    return QStringLiteral("UnknownState(%1)").arg(static_cast<int>(state));
}

static QString audioErrorToString(QAudio::Error error)
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

static qsizetype txAudioBufferBytesForMode(const QAudioFormat& format, const QString& mode,
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

static int txAudioPrecomputeRetryDelayMs(const QString& mode, bool monitoring)
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

