static qreal txAttenuationFromSlider(double outputLevel)
{
    // Slider 0..450 → attenuazione legacy reale: 0=0dB (massima uscita), 450=45dB
    return qBound<qreal>(0.0, static_cast<qreal>(outputLevel / 10.0), 45.0);
}

static constexpr qreal kTxAudioHeadroomGain = 0.9;

static float rxInputGainFromLevel(double inputLevel)
{
    double const bounded = qBound(0.0, inputLevel, 100.0);
    if (bounded <= 50.0) {
        return static_cast<float>(bounded / 50.0);
    }
    return static_cast<float>(1.0 + ((bounded - 50.0) / 50.0) * 3.0);
}

static double rxInputLevelFromGain(double gain)
{
    double const bounded = qBound(0.0, gain, 4.0);
    if (bounded <= 1.0) {
        return bounded * 50.0;
    }
    return 50.0 + ((bounded - 1.0) / 3.0) * 50.0;
}

static constexpr double kAutoRxInputMaxLevel = 100.0;

static qreal txGainFromSlider(double outputLevel)
{
    return static_cast<qreal>(kTxAudioHeadroomGain
                              * std::pow(10.0, -txAttenuationFromSlider(outputLevel) / 20.0));
}

static int legacyTxAttenuationFromLevel(double outputLevel)
{
    return qRound(qBound(0.0, outputLevel, 450.0));
}

qreal decodiumBridgeTxAttenuationFromSlider(double value) { return txAttenuationFromSlider(value); }
float decodiumBridgeRxInputGainFromLevel(double value) { return rxInputGainFromLevel(value); }
qreal decodiumBridgeTxGainFromSlider(double value) { return txGainFromSlider(value); }
int decodiumBridgeLegacyTxAttenuationFromLevel(double value) { return legacyTxAttenuationFromLevel(value); }

double decodiumBridgeRxInputLevelFromGain(double value) { return rxInputLevelFromGain(value); }
double decodiumBridgeAutoRxInputMaxLevel() { return kAutoRxInputMaxLevel; }

static double txLevelFromLegacyAttenuation(int attenuation)
{
    return qBound(0.0, static_cast<double>(attenuation), 450.0);
}
double decodiumBridgeTxLevelFromLegacyAttenuation(int value) { return txLevelFromLegacyAttenuation(value); }


