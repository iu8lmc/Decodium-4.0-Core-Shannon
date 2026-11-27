static QVector<float> generateMultiStreamFtxWave(QString const& mode,
                                                 QStringList const& messages,
                                                 QVector<int> const& f0sHz,
                                                 QString* errorOut = nullptr,
                                                 QVector<float> const& gains = {})
{
    if (messages.isEmpty() || messages.size() != f0sHz.size()) {
        if (errorOut) *errorOut = QStringLiteral("MAM multi-stream: input non valido");
        return {};
    }

    // Parametri per modo (identici a buildTxWaveformForMessage). nsym include
    // i simboli di sync; nsps = campioni/simbolo a 48 kHz. acc = nsym*nsps.
    QString const normalizedMode = mode.trimmed().toUpper();
    int nsym = 0;
    int nsps = 0;
    if (normalizedMode == QStringLiteral("FT8") || normalizedMode.isEmpty()) {
        nsym = 79; nsps = 7680;
    } else if (normalizedMode == QStringLiteral("FT4")) {
        nsym = 103; nsps = 4 * 576;
    } else if (normalizedMode == QStringLiteral("FT2")) {
        nsym = 103; nsps = 4 * 288;
    } else {
        if (errorOut) {
            *errorOut = QStringLiteral("MAM multi-stream: modo non supportato %1").arg(normalizedMode);
        }
        return {};
    }

    QVector<float> acc(nsym * nsps, 0.0f);

    for (int i = 0; i < messages.size(); ++i) {
        decodium::txmsg::EncodedMessage enc;
        if (normalizedMode == QStringLiteral("FT4")) {
            enc = decodium::txmsg::encodeFt4(messages.at(i));
        } else if (normalizedMode == QStringLiteral("FT2")) {
            enc = decodium::txmsg::encodeFt2(messages.at(i));
        } else {
            enc = decodium::txmsg::encodeFt8(messages.at(i));
        }
        if (!enc.ok || enc.tones.size() < nsym) {
            if (errorOut) {
                *errorOut = QStringLiteral("MAM multi-stream: codifica %1 fallita per [%2]")
                                .arg(normalizedMode, messages.at(i));
            }
            return {};
        }
        QVector<float> slot;
        float const f0 = static_cast<float>(f0sHz.at(i));
        if (normalizedMode == QStringLiteral("FT4")) {
            slot = decodium::txwave::generateFt4Wave(enc.tones.constData(), nsym, nsps, 48000.0f, f0);
        } else if (normalizedMode == QStringLiteral("FT2")) {
            slot = decodium::txwave::generateFt2Wave(enc.tones.constData(), nsym, nsps, 48000.0f, f0);
        } else {
            slot = decodium::txwave::generateFt8Wave(enc.tones.constData(), nsym, nsps, 2.0f, 48000.0f, f0);
        }
        int const samples = std::min(static_cast<int>(acc.size()),
                                     static_cast<int>(slot.size()));
        // Power budget: each stream carries the amplitude its own link needs.
        float const streamGain = (gains.size() == messages.size()) ? gains.at(i) : 1.0f;
        for (int k = 0; k < samples; ++k) {
            acc[k] += streamGain * slot.at(k);
        }
    }

    // Peak-normalize a 0.98 (come normalize_wave foxcom ma con ~2% di headroom):
    // evita i campioni a fondo scala (Int16 32767) dopo la conversione del sink,
    // senza distorsione percepibile su FT8 (envelope costante, -0.17 dB).
    float peak = 0.0f;
    for (float const sample : acc) {
        peak = std::max(peak, std::abs(sample));
    }
    if (peak > 0.0f) {
        float const norm = 0.98f / peak;
        for (float& sample : acc) {
            sample *= norm;
        }
    }

    return acc;
}

