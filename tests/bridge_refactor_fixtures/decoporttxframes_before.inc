bool DecodiumBridge::decoPortSendTxWave(const QVector<float>& wave, double* durationSeconds)
{
    auto* link = remoteLink();
    if (!link || !link->isLinked()) {
        emit errorMessage(tr("Transmit refused: no remote radio connected"));
        return false;
    }
    if (!link->canTransmit()
        || (!m_remoteIsDecolink && !(link->state().stateFlags & decoport::StateCanTransmit))) {
        emit errorMessage(tr("Transmit refused: the remote radio does not accept transmission "
                             "(no CAT for its PTT, or it is already transmitting)"));
        return false;
    }
    if (const QString blocked = link->txBlockedReason(); !blocked.isEmpty()) {
        emit errorMessage(tr("Transmit refused: %1").arg(blocked));
        return false;
    }
    if (wave.isEmpty())
        return false;

    // La forma d'onda del modem sta a 48 kHz e non ha niente sopra i 3 kHz:
    // uno su quattro porta a 12 kHz senza alias, che e' la frequenza che il
    // gateway dichiara per questo flusso.
    int const outCount = wave.size() / 4;
    QVector<short> pcm(outCount);
    for (int i = 0; i < outCount; ++i) {
        float const v = wave[i * 4] * 32767.0f;
        pcm[i] = static_cast<short>(v > 32767.0f ? 32767.0f : (v < -32768.0f ? -32768.0f : v));
    }

    int const frameSamples = SAMPLE_RATE * 40 / 1000;   // 40 ms: sta in un MTU
    m_decoPortTxFrames.clear();
    m_decoPortTxFrames.reserve(outCount / frameSamples + 1);
    for (int off = 0; off < outCount; off += frameSamples)
        m_decoPortTxFrames.append(pcm.mid(off, qMin(frameSamples, outCount - off)));
    m_decoPortTxNextFrame = 0;

    int const leadMs = qMax(200, static_cast<int>(link->txAudioLeadMs()));
    m_decoPortTxStartNs = decoport::nowUnixNs() + static_cast<quint64>(leadMs) * 1000000ull;

    // Il PTT parte subito: deve essere su prima che arrivi il primo campione,
    // ed e' per questo che l'audio ha un anticipo dichiarato.
    link->setPtt(true, 0);

    if (!m_decoPortTxPacer) {
        m_decoPortTxPacer = new QTimer(this);
        m_decoPortTxPacer->setInterval(100);
        connect(m_decoPortTxPacer, &QTimer::timeout,
                this, &DecodiumBridge::decoPortPumpTxFrames);
    }
    m_decoPortTxPacer->start();
    // Il primo scaglione parte adesso, non fra cento millisecondi: l'anticipo
    // dichiarato e' duecento, e mangiarne meta' in attesa del timer sarebbe
    // buttare via proprio il margine che serve.
    decoPortPumpTxFrames();

    double const seconds = static_cast<double>(outCount) / SAMPLE_RATE;
    if (durationSeconds)
        *durationSeconds = seconds;
    bridgeLog(QStringLiteral("DecoPort TX: %1 frames, %2 s of audio, starting in %3 ms on %4")
                  .arg(m_decoPortTxFrames.size())
                  .arg(seconds, 0, 'f', 2)
                  .arg(leadMs)
                  .arg(link->rigLabel()));
    return true;
}

void DecodiumBridge::decoPortPumpTxFrames()
{
    auto* link = remoteLink();
    if (!link || !link->isLinked()) {
        decoPortStopTx(QStringLiteral("link lost"));
        return;
    }
    quint64 const horizon = decoport::nowUnixNs() + 1500000000ull;
    quint64 const frameNs = 40000000ull;
    while (m_decoPortTxNextFrame < m_decoPortTxFrames.size()) {
        quint64 const playAt = m_decoPortTxStartNs
            + static_cast<quint64>(m_decoPortTxNextFrame) * frameNs;
        if (playAt > horizon)
            break;
        link->sendTxAudio(m_decoPortTxFrames[m_decoPortTxNextFrame], playAt);
        ++m_decoPortTxNextFrame;
    }
    if (m_decoPortTxNextFrame >= m_decoPortTxFrames.size() && m_decoPortTxPacer)
        m_decoPortTxPacer->stop();
}

// Chiudere la trasmissione remota vuol dire soprattutto abbassare il PTT: non
// lasciarlo su e' l'unica cosa che qui non si puo' sbagliare.
void DecodiumBridge::decoPortStopTx(const QString& reason)
{
    if (m_decoPortTxPacer)
        m_decoPortTxPacer->stop();
    m_decoPortTxFrames.clear();
    m_decoPortTxNextFrame = 0;
    if (auto* link = remoteLink()) {
        if (link->isLinked())
            link->setPtt(false, 0);
    }
    bridgeLog(QStringLiteral("DecoPort TX stopped: %1").arg(reason));
}

