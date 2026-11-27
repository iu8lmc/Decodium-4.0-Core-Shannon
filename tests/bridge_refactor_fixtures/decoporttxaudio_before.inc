void DecodiumBridge::decoPortPlayTxAudio(const QVector<short>& samples, bool fromRtty)
{
    if (m_rttyTxActive != fromRtty)
        return;
    if (samples.isEmpty() || m_transmitting || m_tuning)
        return;

    if (!m_decoPortTxOutThread) {
        m_decoPortTxOutThread = new QThread(this);
        m_decoPortTxOutThread->setObjectName(QStringLiteral("DecoPortTxOut"));
        m_decoPortTxOut = new RtlSdrAudioOutput;
        m_decoPortTxOut->moveToThread(m_decoPortTxOutThread);
        connect(m_decoPortTxOutThread, &QThread::finished,
                m_decoPortTxOut, &QObject::deleteLater);
        connect(m_decoPortTxOut, &RtlSdrAudioOutput::error, this,
                [this](const QString& message) {
            bridgeLog(QStringLiteral("DecoPort transmit audio error: %1").arg(message));
            ssbStop();
            if (m_rttyTxActive)
                rttyAlzaPtt(false);
        }, Qt::QueuedConnection);
        m_decoPortTxOutThread->start();
    }

    if (m_decoPortTxOutRate == 0) {
        bool found = false;
        QAudioDevice const outDev = resolveTxOutputDevice(&found);
        if (outDev.id().isEmpty()) {
            bridgeLog(QStringLiteral("DecoPort transmit audio dropped: no output device"));
            if (m_rttyTxActive) rttyAlzaPtt(false);
            return;
        }
        auto supportsRate = [&outDev](int rate) {
            for (QAudioFormat::SampleFormat sf : {QAudioFormat::Int16, QAudioFormat::Float}) {
                for (int ch : {1, 2}) {
                    QAudioFormat f;
                    f.setSampleRate(rate);
                    f.setChannelCount(ch);
                    f.setSampleFormat(sf);
                    if (outDev.isFormatSupported(f))
                        return true;
                }
            }
            return false;
        };
        // 48 kHz per primo: e' la frequenza a cui girano i codec USB delle
        // radio, e portarci i 12 kHz per interpolazione lascia le immagini
        // sopra i 6 kHz, dove il filtro SSB della radio le toglie comunque.
        m_decoPortTxOutRate = supportsRate(48000) ? 48000
                            : (supportsRate(SAMPLE_RATE) ? SAMPLE_RATE : 0);
        if (m_decoPortTxOutRate == 0) {
            bridgeLog(QStringLiteral("DecoPort transmit audio dropped: [%1] takes neither 48000 nor 12000 Hz")
                          .arg(outDev.description()));
            if (m_rttyTxActive) rttyAlzaPtt(false);
            return;
        }
        QPointer<RtlSdrAudioOutput> out(m_decoPortTxOut);
        int const rate = m_decoPortTxOutRate;
        QMetaObject::invokeMethod(m_decoPortTxOut, [out, outDev, rate]() {
            if (out) out->start(outDev, rate);
        }, Qt::QueuedConnection);
        bridgeLog(QStringLiteral("DecoPort transmit audio to [%1] at %2 Hz")
                      .arg(outDev.description()).arg(m_decoPortTxOutRate));
    }

    QVector<short> out_samples;
    if (m_decoPortTxOutRate == SAMPLE_RATE) {
        out_samples = samples;
    } else {
        // Interpolazione lineare, non ripetizione: qui il segnale va in aria e
        // i gradini di una ripetizione sarebbero larghezza di banda regalata.
        int const factor = m_decoPortTxOutRate / SAMPLE_RATE;
        out_samples.reserve(samples.size() * factor);
        for (int i = 0; i < samples.size(); ++i) {
            int const a = samples[i];
            int const b = (i + 1 < samples.size()) ? samples[i + 1] : samples[i];
            for (int k = 0; k < factor; ++k)
                out_samples.append(static_cast<short>(a + (b - a) * k / factor));
        }
    }

    QPointer<RtlSdrAudioOutput> out(m_decoPortTxOut);
    int const rate = m_decoPortTxOutRate;
    QMetaObject::invokeMethod(m_decoPortTxOut, [out, out_samples, rate]() {
        if (out) out->enqueueSamples(out_samples, rate);
    }, Qt::QueuedConnection);

    // Finche' l'audio scorre il PTT resta su; e' questo che rinnova la scadenza.
    if (m_decoPortRemoteKeyed && m_decoPortTxGuard)
        m_decoPortTxGuard->start();
}

