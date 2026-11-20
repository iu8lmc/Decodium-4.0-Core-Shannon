static QVector<float> buildTxWaveformForMessage(QString const& mode,
                                                QString const& msg,
                                                int txFrequency,
                                                double trPeriodSeconds,
                                                QString* errorOut = nullptr)
{
    QVector<float> wave;
    float const freq = static_cast<float>(txFrequency);
    QString const normalizedMode = mode.trimmed().toUpper();

    if (normalizedMode == QStringLiteral("FT2")) {
        auto enc = decodium::txmsg::encodeFt2(msg);
        if (!enc.ok) {
            if (errorOut) *errorOut = QStringLiteral("Codifica FT2 fallita");
            return {};
        }
        wave = decodium::txwave::generateFt2Wave(enc.tones.constData(), 103, 4 * 288, 48000.0f, freq);
    } else if (normalizedMode == QStringLiteral("FT4")) {
        auto enc = decodium::txmsg::encodeFt4(msg);
        if (!enc.ok) {
            if (errorOut) *errorOut = QStringLiteral("Codifica FT4 fallita");
            return {};
        }
        wave = decodium::txwave::generateFt4Wave(enc.tones.constData(), 103, 4 * 576, 48000.0f, freq);
    } else if (normalizedMode == QStringLiteral("FT8") || normalizedMode.isEmpty()) {
        auto enc = decodium::txmsg::encodeFt8(msg);
        if (!enc.ok) {
            if (errorOut) *errorOut = QStringLiteral("Codifica FT8 fallita");
            return {};
        }
        wave = decodium::txwave::generateFt8Wave(enc.tones.constData(), 79, 7680, 2.0f, 48000.0f, freq);
    } else if (normalizedMode == QStringLiteral("JT65")) {
        auto enc = decodium::txmsg::encodeJt65(msg);
        if (!enc.ok || enc.tones.size() < 126) {
            if (errorOut) *errorOut = QStringLiteral("Codifica JT65 fallita");
            return {};
        }
        wave = decodium::txwave::generateJt65Wave(enc.tones.constData(), 126, 48000.0f, freq);
    } else if (normalizedMode == QStringLiteral("JT4")) {
        auto enc = decodium::txmsg::encodeJt4(msg);
        if (!enc.ok || enc.tones.size() < 206) {
            if (errorOut) *errorOut = QStringLiteral("Codifica JT4 fallita");
            return {};
        }
        wave = decodium::txwave::generateJt4Wave(enc.tones.constData(), 206, 48000.0f, freq);
    } else if (normalizedMode == QStringLiteral("JT9")) {
        auto enc = decodium::txmsg::encodeJt9(msg);
        if (!enc.ok || enc.tones.size() < 85) {
            if (errorOut) *errorOut = QStringLiteral("Codifica JT9 fallita");
            return {};
        }
        // JT9-1: 85 symbols, 6912 samples/symbol at 12 kHz. Render at the
        // bridge's 48 kHz audio rate while preserving 1.736111 Hz spacing.
        static constexpr int kJt9Symbols = 85;
        static constexpr int kJt9SamplesPerSymbol = 4 * 6912;
        static constexpr float kJt9ToneSpacing = 12000.0f / 6912.0f;
        wave = decodium::txwave::generateToneWave(enc.tones.constData(),
                                                   kJt9Symbols,
                                                   kJt9SamplesPerSymbol,
                                                   48000.0f,
                                                   kJt9ToneSpacing,
                                                   freq);
    } else if (normalizedMode == QStringLiteral("MSK144")) {
        auto enc = decodium::txmsg::encodeMsk144(msg);
        if (!enc.ok || enc.tones.isEmpty()) {
            if (errorOut) *errorOut = QStringLiteral("Codifica MSK144 fallita");
            return {};
        }
        int symbolCount = 144;
        if (enc.tones.size() > 40 && enc.tones.at(40) < 0) {
            symbolCount = 40;
        }
        if (enc.tones.size() < symbolCount) {
            if (errorOut) *errorOut = QStringLiteral("Sequenza MSK144 incompleta");
            return {};
        }
        wave = decodium::txwave::generateMsk144Wave(enc.tones.constData(), symbolCount,
                                                     48000.0f, freq,
                                                     qBound(5.0, trPeriodSeconds, 60.0));
    } else if (normalizedMode == QStringLiteral("WSPR")) {
        auto enc = decodium::txmsg::encodeWspr(msg);
        if (!enc.ok || enc.tones.size() < 162) {
            if (errorOut) *errorOut = QStringLiteral("Codifica WSPR fallita");
            return {};
        }
        static constexpr int kWsprSymbols = 162;
        static constexpr int kWsprSamplesPerSymbol = 32768; // 8192 samples at 12 kHz, rendered at 48 kHz
        static constexpr float kWsprToneSpacing = 12000.0f / 8192.0f;
        wave = decodium::txwave::generateToneWave(enc.tones.constData(),
                                                  kWsprSymbols,
                                                  kWsprSamplesPerSymbol,
                                                  48000.0f,
                                                  kWsprToneSpacing,
                                                  freq);
    } else if (normalizedMode == QStringLiteral("FST4")
               || normalizedMode == QStringLiteral("FST4W")
               || normalizedMode.startsWith(QStringLiteral("FST4-"))
               || normalizedMode.startsWith(QStringLiteral("FST4W-"))) {
        bool const wsprHint = normalizedMode.startsWith(QStringLiteral("FST4W"));
        auto enc = decodium::txmsg::encodeFst4WithHint(msg, wsprHint, false);
        if (!enc.ok || enc.tones.size() < 160) {
            if (errorOut) *errorOut = wsprHint
                ? QStringLiteral("Codifica FST4W fallita")
                : QStringLiteral("Codifica FST4 fallita");
            return {};
        }
        static constexpr int kFst4Symbols = 160;
        static constexpr int kFst4Hmod = 1;
        int const fst4SamplesPerSymbol = fst4SamplesPerSymbolAt48k(normalizedMode, trPeriodSeconds);
        float const dfreq = static_cast<float>(kFst4Hmod) * 48000.0f
                            / static_cast<float>(fst4SamplesPerSymbol);
        float const f0 = freq + 1.5f * dfreq;
        wave = decodium::txwave::generateFst4Wave(enc.tones.constData(),
                                                  kFst4Symbols,
                                                  fst4SamplesPerSymbol,
                                                  48000.0f,
                                                  kFst4Hmod,
                                                  f0);
    } else if (normalizedMode == QStringLiteral("Q65") || normalizedMode.startsWith(QStringLiteral("Q65-"))) {
        auto enc = decodium::txmsg::encodeQ65(msg);
        if (!enc.ok || enc.tones.size() < 85) {
            if (errorOut) *errorOut = QStringLiteral("Codifica Q65 fallita");
            return {};
        }
        static constexpr int kQ65Symbols = 85;
        int const q65SamplesPerSymbol = q65SamplesPerSymbolAt48k(normalizedMode);
        float const q65ToneSpacing =
            (48000.0f / static_cast<float>(q65SamplesPerSymbol))
            * static_cast<float>(1 << q65SubmodeIndexFromModeLabel(normalizedMode));
        wave = decodium::txwave::generateToneWave(enc.tones.constData(),
                                                  kQ65Symbols,
                                                  q65SamplesPerSymbol,
                                                  48000.0f,
                                                  q65ToneSpacing,
                                                  freq);
    } else {
        if (errorOut) {
            *errorOut = QStringLiteral("Modo TX non supportato dal bridge audio: %1").arg(normalizedMode);
        }
        return {};
    }

    if (wave.isEmpty() && errorOut) {
        *errorOut = QStringLiteral("Generazione onda TX fallita");
    }
    return wave;
}

