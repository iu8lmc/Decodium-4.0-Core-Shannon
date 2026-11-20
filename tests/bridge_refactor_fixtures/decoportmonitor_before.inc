void DecodiumBridge::setDecoPortMonitor(bool on)
{
    if (m_decoPortMonitor == on)
        return;
    m_decoPortMonitor = on;

    if (on) {
        if (!m_decoPortMonitorThread) {
            m_decoPortMonitorThread = new QThread(this);
            m_decoPortMonitorThread->setObjectName(QStringLiteral("DecoPortMonitor"));
            m_decoPortMonitorOut = new RtlSdrAudioOutput;
            m_decoPortMonitorOut->moveToThread(m_decoPortMonitorThread);
            connect(m_decoPortMonitorThread, &QThread::finished,
                    m_decoPortMonitorOut, &QObject::deleteLater);
            connect(m_decoPortMonitorOut, &RtlSdrAudioOutput::error,
                    this, [this](const QString& message) {
                bridgeLog(QStringLiteral("DecoPort monitor error: %1").arg(message));
                setDecoPortMonitor(false);
                emit errorMessage(message);
            }, Qt::QueuedConnection);
            m_decoPortMonitorThread->start();
        }
        m_remoteRxGain = remoteRxVolume() / 100.0;
        QAudioDevice output = cachedDefaultAudioOutput(QStringLiteral("DecoPort remote monitor"), false);
        const QByteArray selected = QByteArray::fromBase64(remoteRxDevice().toLatin1());
        if (!selected.isEmpty()) {
            output = QAudioDevice();
            for (const auto& device : QMediaDevices::audioOutputs()) {
                if (device.id() == selected) { output = device; break; }
            }
        }
        if (output.id().isEmpty()) {
            m_decoPortMonitor = false;
            bridgeLog(QStringLiteral("DecoPort monitor refused: no audio output available"));
            emit errorMessage(tr("Remote RX audio output is unavailable. Choose headphones or speakers."));
            emit decoPortMonitorChanged();
            return;
        }
        // Quale frequenza accetta la scheda si decide qui, una volta: la
        // riproduzione non sa risamplare e rifiuterebbe in silenzio.
        auto supportsRate = [&output](int rate) {
            for (QAudioFormat::SampleFormat sf : {QAudioFormat::Int16, QAudioFormat::Float}) {
                for (int ch : {1, 2}) {
                    QAudioFormat f;
                    f.setSampleRate(rate);
                    f.setChannelCount(ch);
                    f.setSampleFormat(sf);
                    if (output.isFormatSupported(f))
                        return true;
                }
            }
            return false;
        };
        m_decoPortMonitorRate = supportsRate(SAMPLE_RATE) ? SAMPLE_RATE
                              : (supportsRate(48000) ? 48000 : 0);
        if (m_decoPortMonitorRate == 0) {
            m_decoPortMonitor = false;
            bridgeLog(QStringLiteral("DecoPort monitor refused: [%1] takes neither 12000 nor 48000 Hz")
                          .arg(output.description()));
            emit errorMessage(tr("Remote RX audio output does not support 12 or 48 kHz."));
            emit decoPortMonitorChanged();
            return;
        }
        QPointer<RtlSdrAudioOutput> out(m_decoPortMonitorOut);
        int const rate = m_decoPortMonitorRate;
        QMetaObject::invokeMethod(m_decoPortMonitorOut, [out, output, rate]() {
            if (out) out->start(output, rate);
        }, Qt::QueuedConnection);
        bridgeLog(QStringLiteral("DecoPort monitor ON: remote audio to [%1] at %2 Hz")
                      .arg(output.description()).arg(m_decoPortMonitorRate));
    } else if (m_decoPortMonitorOut) {
        QPointer<RtlSdrAudioOutput> out(m_decoPortMonitorOut);
        QMetaObject::invokeMethod(m_decoPortMonitorOut, [out]() {
            if (out) out->stop(QStringLiteral("decoport-monitor-off"));
        }, Qt::QueuedConnection);
        m_decoPortMonitorRate = 0;
        bridgeLog(QStringLiteral("DecoPort monitor OFF"));
    }
    emit decoPortMonitorChanged();
}

