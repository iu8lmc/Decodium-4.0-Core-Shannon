static QString decodeDedupKey(QString const& time,
                              QString const& freq,
                              QString const& message)
{
    bool freqOk = false;
    int const freqHz = freq.trimmed().toInt(&freqOk);
    QString const freqToken = freqOk ? QString::number(freqHz) : freq.trimmed();
    return normalizeUtcDisplayToken(time).trimmed()
        + QLatin1Char('|')
        + freqToken
        + QLatin1Char('|')
        + canonicalDecodeMessage(message);
}

static constexpr int kFt2AsyncUiDedupeFallbackWindowMs = 3750;

// FT2 async turbo (100ms windows): lo stage7 DSP può riportare lo stesso messaggio
// in scansioni consecutive con freq leggermente diversa (±1-10 Hz di jitter). Quantizza
// la frequenza in bucket da 20 Hz così gli stessi decode non producono righe duplicate
// nello stesso slot, ma senza attraversare slot FT2 successivi.
static QString decodeDedupKeyFt2Async(QString const& time,
                                      QString const& freq,
                                      QString const& message,
                                      qint64 timestampMs = 0)
{
    bool ok = false;
    int const f = freq.trimmed().toInt(&ok);
    QString const fBucket = ok ? QString::number((f / 20) * 20) : freq.trimmed();
    QString timeToken = normalizeUtcDisplayToken(time).trimmed();
    if (timeToken.isEmpty() && timestampMs > 0) {
        timeToken = QStringLiteral("@ft2slot")
            + QString::number(timestampMs / kFt2AsyncUiDedupeFallbackWindowMs);
    }
    return timeToken
        + QLatin1Char('|')
        + fBucket
        + QLatin1Char('|')
        + canonicalDecodeMessage(message);
}

QString decodiumBridgeDedupKey(const QString& time, const QString& freq, const QString& message)
{
    return decodeDedupKey(time, freq, message);
}
QString decodiumBridgeFt2AsyncDedupKey(const QString& time, const QString& freq, const QString& message, qint64 timestamp)
{
    return decodeDedupKeyFt2Async(time, freq, message, timestamp);
}
