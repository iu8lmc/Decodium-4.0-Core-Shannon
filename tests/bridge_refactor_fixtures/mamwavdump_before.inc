bool DecodiumBridge::mamDumpTestWav(const QString& path, const QStringList& messages,
                                    const QVector<int>& f0sHz, int sampleRate)
{
    QString err;
    QVector<float> w = generateMultiStreamFtxWave(m_mode, messages, f0sHz, &err);
    if (w.isEmpty()) {
        bridgeLog(QStringLiteral("MAM dump test WAV: generazione fallita: %1").arg(err));
        return false;
    }
    bool const ok = writeMono16WavFile(path, w, sampleRate);
    bridgeLog(QStringLiteral("MAM dump test WAV %1: %2 (%3 samples)")
                  .arg(ok ? QStringLiteral("saved") : QStringLiteral("failed"), path)
                  .arg(w.size()));
    return ok;
}

