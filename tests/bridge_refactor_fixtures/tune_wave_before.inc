static QVector<float> buildTuneWaveform(double freq, int sampleRate, int durationSeconds)
{
    int const nFrames = sampleRate * durationSeconds;
    double const twoPi = 2.0 * 3.14159265358979323846;
    double const dphi = twoPi * freq / static_cast<double>(sampleRate);

    QVector<float> wave;
    wave.resize(nFrames);
    double phi = 0.0;
    for (int i = 0; i < nFrames; ++i) {
        wave[i] = static_cast<float>(std::sin(phi));
        phi += dphi;
        if (phi > twoPi) phi -= twoPi;
    }
    return wave;
}
