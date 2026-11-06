// SPDX-License-Identifier: GPL-3.0-or-later
// AGC software dell'audio ricevuto, portato da DECODIUM SDR (src/dsp/Agc.*,
// stesso autore): rivelatore di inviluppo con attacco rapido, tempo di hang e
// decadimento secondo il preset, soglia AGC-T sotto la quale il guadagno non
// sale. In piu' del programma d'origine c'e' il modo Auto: la soglia non si
// regola a mano, segue il rumore di fondo misurato (il minimo dell'inviluppo,
// che sale in fretta e scende piano) con un margine di 6 dB.
//
// Serve all'ascolto del ricevitore (radio remota, DecoPort/Decolink): lavora
// solo sulla copia che va all'altoparlante, mai sull'audio del decoder.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace decodium::ssb {

class RxAgc {
public:
    enum Mode { Off = 0, Auto, Fast, Medium, Slow, Long };

    void configure(double sampleRate)
    {
        rate = sampleRate > 0.0 ? sampleRate : 12000.0;
        recompute();
        reset();
    }
    void setMode(int m)
    {
        const Mode next = static_cast<Mode>(std::clamp(m, 0, int(Long)));
        if (next == mode_)
            return;
        mode_ = next;
        recompute();
    }
    int mode() const { return mode_; }
    // Soglia AGC-T in dBFS (da -120 a -20); ignorata nel modo Auto.
    void setThresholdDb(double db) { thresholdDb_ = std::clamp(db, -120.0, -20.0); }
    double thresholdDb() const { return thresholdDb_; }
    void setManualGainDb(double db) { manualGainDb_ = std::clamp(db, -40.0, 40.0); }
    float gainDb() const { return 20.0f * std::log10(std::max(gain, 1e-9f)); }
    float envelopeDb() const { return 20.0f * std::log10(std::max(envelope, 1e-9f)); }

    void reset()
    {
        envelope = 0.0f;
        gain = 1.0f;
        hangCounter = 0;
        floorLevel = 0.0f;
    }

    // Audio in virgola mobile, +-1 a fondo scala.
    void process(float* audio, std::size_t n)
    {
        if (mode_ == Off) {
            const float g = float(std::pow(10.0, manualGainDb_ / 20.0));
            for (std::size_t i = 0; i < n; ++i)
                audio[i] *= g;
            gain = g;
            return;
        }
        const float maxGain = float(std::pow(10.0, maxGainDb / 20.0));
        float threshold = float(std::pow(10.0, thresholdDb_ / 20.0));
        for (std::size_t i = 0; i < n; ++i) {
            const float x = std::fabs(audio[i]);
            if (x > envelope) {
                envelope += (x - envelope) * attackCoeff;
                hangCounter = hangSamples;
            } else if (hangCounter > 0) {
                --hangCounter;
            } else {
                envelope += (x - envelope) * decayCoeff;
            }
            if (mode_ == Auto) {
                // Il fondo sale piano e scende subito: segue il rumore, non il parlato.
                if (envelope < floorLevel || floorLevel <= 0.0f)
                    floorLevel += (envelope - floorLevel) * 0.01f;
                else
                    floorLevel += (envelope - floorLevel) * floorRise;
                threshold = std::max(floorLevel * 2.0f, 1e-5f);
            }
            const float effective = std::max(envelope, threshold);
            float target = effective > 1e-9f ? kTarget / effective : maxGain;
            target = std::clamp(target, 0.0f, maxGain);
            gain += (target - gain) * gainSmooth;
            audio[i] *= gain;
        }
    }

private:
    static float onePole(double seconds, double sampleRate)
    {
        return seconds <= 0.0 ? 1.0f : float(1.0 - std::exp(-1.0 / (seconds * sampleRate)));
    }
    void recompute()
    {
        double decay = 0.5, hang = 0.25;
        switch (mode_) {
        case Fast:   decay = 0.20; hang = 0.10; break;
        case Slow:   decay = 1.20; hang = 0.50; break;
        case Long:   decay = 2.50; hang = 1.00; break;
        case Auto:   decay = 0.40; hang = 0.20; break;
        default:     break;
        }
        attackCoeff = onePole(0.002, rate);
        decayCoeff = onePole(decay, rate);
        gainSmooth = onePole(0.005, rate);
        floorRise = onePole(6.0, rate);
        hangSamples = unsigned(hang * rate);
    }

    static constexpr float kTarget = 0.30f;
    double rate = 12000.0;
    Mode mode_ = Off;
    double thresholdDb_ = -80.0;
    double manualGainDb_ = 0.0;
    double maxGainDb = 60.0;
    float attackCoeff = 1.0f, decayCoeff = 0.0f, gainSmooth = 0.0f, floorRise = 0.0f;
    unsigned hangSamples = 0, hangCounter = 0;
    float envelope = 0.0f, gain = 1.0f, floorLevel = 0.0f;
};

}  // namespace decodium::ssb
