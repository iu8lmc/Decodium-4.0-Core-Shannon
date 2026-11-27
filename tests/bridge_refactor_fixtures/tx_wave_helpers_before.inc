static bool bridgeGeneratedTxWaveMode(QString const& mode)
{
    QString const normalized = mode.trimmed().toUpper();
    return normalized == QStringLiteral("FT8")
        || normalized == QStringLiteral("FT4")
        || normalized == QStringLiteral("FT2")
        || normalized == QStringLiteral("JT65")
        || normalized == QStringLiteral("JT9")
        || normalized == QStringLiteral("MSK144")
        || normalized == QStringLiteral("Q65")
        || normalized.startsWith(QStringLiteral("Q65-"))
        || normalized == QStringLiteral("FST4")
        || normalized == QStringLiteral("FST4W")
        || normalized.startsWith(QStringLiteral("FST4-"))
        || normalized.startsWith(QStringLiteral("FST4W-"))
        || normalized == QStringLiteral("WSPR");
}
bool decodiumBridgeGeneratedTxWaveMode(const QString& mode)
{
    return bridgeGeneratedTxWaveMode(mode);
}


static int normalizeWsprPowerDbm(int dbm)
{
    static constexpr int kWsprPowerDbmValues[] = {
        0, 3, 7, 10, 13, 17, 20, 23, 27, 30,
        33, 37, 40, 43, 47, 50, 53, 57, 60
    };

    int best = kWsprPowerDbmValues[0];
    int bestDelta = std::abs(dbm - best);
    for (int value : kWsprPowerDbmValues) {
        int const delta = std::abs(dbm - value);
        if (delta < bestDelta) {
            best = value;
            bestDelta = delta;
        }
    }
    return best;
}

static QString wsprPowerDbmLabel(int dbm)
{
    switch (normalizeWsprPowerDbm(dbm)) {
    case 0:  return QStringLiteral("0 dBm  1 mW");
    case 3:  return QStringLiteral("3 dBm  2 mW");
    case 7:  return QStringLiteral("7 dBm  5 mW");
    case 10: return QStringLiteral("10 dBm  10 mW");
    case 13: return QStringLiteral("13 dBm  20 mW");
    case 17: return QStringLiteral("17 dBm  50 mW");
    case 20: return QStringLiteral("20 dBm  100 mW");
    case 23: return QStringLiteral("23 dBm  200 mW");
    case 27: return QStringLiteral("27 dBm  500 mW");
    case 30: return QStringLiteral("30 dBm  1 W");
    case 33: return QStringLiteral("33 dBm  2 W");
    case 37: return QStringLiteral("37 dBm  5 W");
    case 40: return QStringLiteral("40 dBm  10 W");
    case 43: return QStringLiteral("43 dBm  20 W");
    case 47: return QStringLiteral("47 dBm  50 W");
    case 50: return QStringLiteral("50 dBm  100 W");
    case 53: return QStringLiteral("53 dBm  200 W");
    case 57: return QStringLiteral("57 dBm  500 W");
    case 60: return QStringLiteral("60 dBm  1 kW");
    default: return QStringLiteral("37 dBm  5 W");
    }
}

static double wsprPowerDbmToWatts(int dbm)
{
    return std::pow(10.0, (normalizeWsprPowerDbm(dbm) - 30.0) / 10.0);
}

int decodiumBridgeNormalizeWsprPowerDbm(int value) { return normalizeWsprPowerDbm(value); }
QString decodiumBridgeWsprPowerDbmLabel(int value) { return wsprPowerDbmLabel(value); }
double decodiumBridgeWsprPowerDbmToWatts(int value) { return wsprPowerDbmToWatts(value); }

static QString txWaveMetricsSummary(QVector<float> const& wave)
{
    double peak = 0.0;
    double avgAbs = 0.0;
    qsizetype nonzero = 0;
    for (float const sample : wave) {
        double const value = std::abs(static_cast<double>(sample));
        peak = std::max(peak, value);
        avgAbs += value;
        if (value > 1.0e-7) {
            ++nonzero;
        }
    }
    if (!wave.isEmpty()) {
        avgAbs /= static_cast<double>(wave.size());
    }

    qint64 const durationMs = wave.isEmpty()
        ? 0
        : qRound64(static_cast<double>(wave.size()) * 1000.0 / 48000.0);

    return QStringLiteral("samples=%1 duration_ms=%2 peak=%3 avgAbs=%4 nonzero=%5/%6")
        .arg(wave.size())
        .arg(durationMs)
        .arg(peak, 0, 'f', 6)
        .arg(avgAbs, 0, 'f', 6)
        .arg(nonzero)
        .arg(wave.size());
}

QString decodiumBridgeTxWaveMetricsSummary(QVector<float> const& wave) { return txWaveMetricsSummary(wave); }

static double txWavePeak(QVector<float> const& wave)
{
    double peak = 0.0;
    for (float const sample : wave) {
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
    }
    return peak;
}

double decodiumBridgeTxWavePeak(QVector<float> const& wave) { return txWavePeak(wave); }
