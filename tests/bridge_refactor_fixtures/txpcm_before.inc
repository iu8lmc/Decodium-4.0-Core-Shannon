static AudioDevice::Channel boundedAudioChannel(int channel)
{
    return static_cast<AudioDevice::Channel>(qBound(0, channel, 3));
}

static bool txChannelCarriesSample(int channelIndex, int channelCount, AudioDevice::Channel requestedChannel)
{
    if (channelCount <= 1) {
        return true;
    }
    switch (requestedChannel) {
    case AudioDevice::Left:
        return channelIndex == 0;
    case AudioDevice::Right:
        return channelIndex == 1;
    case AudioDevice::Mono:
    case AudioDevice::Both:
    default:
        return true;
    }
}

static QByteArray buildTxPcmBuffer(const QVector<float>& wave48kMono, const QAudioFormat& format,
                                   AudioDevice::Channel outputChannel = AudioDevice::Both)
{
    if (wave48kMono.isEmpty() || !format.isValid()) return {};

    const int dstRate = qMax(1, format.sampleRate());
    const int dstChannels = qMax(1, format.channelCount());
    const int bytesPerSample = qMax(0, format.bytesPerSample());
    if (bytesPerSample <= 0 || format.sampleFormat() == QAudioFormat::Unknown) return {};

    const qint64 dstFrames = qMax<qint64>(1, qRound64((double)wave48kMono.size() * dstRate / 48000.0));
    const qint64 totalBytes = dstFrames * dstChannels * bytesPerSample;
    if (totalBytes <= 0 || totalBytes > std::numeric_limits<int>::max()) return {};

    QByteArray pcm(static_cast<int>(totalBytes), Qt::Uninitialized);
    char* out = pcm.data();
    const int srcCount = wave48kMono.size();

    for (qint64 frame = 0; frame < dstFrames; ++frame) {
        const double srcPos = (static_cast<double>(frame) * 48000.0) / dstRate;
        const int src0 = qBound(0, static_cast<int>(std::floor(srcPos)), srcCount - 1);
        const int src1 = qMin(src0 + 1, srcCount - 1);
        const float frac = static_cast<float>(srcPos - src0);
        float sample = wave48kMono[src0];
        if (src1 != src0) sample += (wave48kMono[src1] - wave48kMono[src0]) * frac;
        sample = qBound(-1.0f, sample, 1.0f);

        for (int ch = 0; ch < dstChannels; ++ch) {
            float const outSample = txChannelCarriesSample(ch, dstChannels, outputChannel) ? sample : 0.0f;
            switch (format.sampleFormat()) {
            case QAudioFormat::UInt8: {
                const quint8 v = static_cast<quint8>(qRound((outSample * 0.5f + 0.5f) * 255.0f));
                std::memcpy(out, &v, sizeof(v));
                out += sizeof(v);
                break;
            }
            case QAudioFormat::Int16: {
                const qint16 v = static_cast<qint16>(qRound(outSample * 32767.0f));
                std::memcpy(out, &v, sizeof(v));
                out += sizeof(v);
                break;
            }
            case QAudioFormat::Int32: {
                const qint32 v = static_cast<qint32>(qRound64(outSample * 2147483647.0f));
                std::memcpy(out, &v, sizeof(v));
                out += sizeof(v);
                break;
            }
            case QAudioFormat::Float: {
                const float v = outSample;
                std::memcpy(out, &v, sizeof(v));
                out += sizeof(v);
                break;
            }
            case QAudioFormat::Unknown:
            default:
                return {};
            }
        }
    }

    return pcm;
}

