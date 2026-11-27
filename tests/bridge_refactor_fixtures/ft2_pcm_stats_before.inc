struct Ft2LinkPcmStats
{
    double rms {0.0};
    double peak {0.0};
    int nonZero {0};
    int aboveNoise {0};
};

Ft2LinkPcmStats ft2LinkPcmStats(QVector<short> const& samples)
{
    Ft2LinkPcmStats stats;
    if (samples.isEmpty()) {
        return stats;
    }

    double sumSq = 0.0;
    int peakAbs = 0;
    for (short sample : samples) {
        int const value = static_cast<int>(sample);
        int const absSample = value < 0 ? -value : value;
        peakAbs = std::max(peakAbs, absSample);
        sumSq += static_cast<double>(value) * static_cast<double>(value);
        if (absSample > 0) {
            ++stats.nonZero;
        }
        if (absSample >= 33) {
            ++stats.aboveNoise;
        }
    }

    stats.rms = std::sqrt(sumSq / static_cast<double>(samples.size())) / 32768.0;
    stats.peak = static_cast<double>(peakAbs) / 32768.0;
    return stats;
}
