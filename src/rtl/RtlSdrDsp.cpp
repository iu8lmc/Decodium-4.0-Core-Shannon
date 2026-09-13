// SPDX-License-Identifier: GPL-3.0-or-later

#include "RtlSdrDsp.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kDecoderFilterTaps = 63;
constexpr double kPi = 3.14159265358979323846;

double sinc(double x)
{
    return std::abs(x) < 1.0e-12 ? 1.0 : std::sin(kPi * x) / (kPi * x);
}

bool isKnownRtlRate(int rate)
{
    // These rates are accepted by the common librtlsdr drivers and divide
    // cleanly into the decoder or 48 kHz receiver audio paths below.
    return rate == 240000 || rate == 288000 || rate == 480000
        || rate == 960000 || rate == 1200000 || rate == 1920000;
}
}

RtlSdrDsp::RtlSdrDsp()
{
    configure(240000);
}

bool RtlSdrDsp::isSupportedSampleRate(int inputSampleRate, int outputSampleRate)
{
    return isKnownRtlRate(inputSampleRate)
        && outputSampleRate > 0
        && inputSampleRate % outputSampleRate == 0
        && inputSampleRate / outputSampleRate >= 2;
}

bool RtlSdrDsp::isSupportedSampleRateForDemodulator(int inputSampleRate,
                                                    Demodulator demodulator)
{
    const int outputRate = demodulator == Demodulator::WeakSignal
        ? kDecoderSampleRate : kReceiverAudioSampleRate;
    if (!isSupportedSampleRate(inputSampleRate, outputRate)) {
        return false;
    }
    // Broadcast FM needs enough RF bandwidth for the 150–200 kHz wide
    // channel.  960 kS/s gives a useful ±480 kHz RF view and a proper margin.
    return demodulator != Demodulator::WideFm || inputSampleRate >= 960000;
}

QString RtlSdrDsp::demodulatorName(Demodulator demodulator)
{
    switch (demodulator) {
    case Demodulator::WeakSignal: return QStringLiteral("Weak signal / FT8 audio");
    case Demodulator::WideFm: return QStringLiteral("Wide FM");
    case Demodulator::NarrowFm: return QStringLiteral("Narrow FM");
    case Demodulator::Am: return QStringLiteral("AM");
    case Demodulator::Usb: return QStringLiteral("USB");
    case Demodulator::Lsb: return QStringLiteral("LSB");
    case Demodulator::Cw: return QStringLiteral("CW");
    }
    return QStringLiteral("Unknown");
}

bool RtlSdrDsp::configure(int inputSampleRate, int outputSampleRate, double audioGain)
{
    if (!isSupportedSampleRate(inputSampleRate, outputSampleRate)) {
        m_inputSampleRate = 0;
        m_outputDecimation = 0;
        return false;
    }

    m_inputSampleRate = inputSampleRate;
    m_outputSampleRate = outputSampleRate;
    m_channelDecimation = 1;
    m_demodSampleRate = inputSampleRate;
    m_outputDecimation = inputSampleRate / outputSampleRate;
    m_decoderDecimation = m_outputDecimation;
    m_demodulator = Demodulator::WeakSignal;
    m_spectrumInverted = false;
    m_audioGain = qBound(0.05, audioGain, 50.0);
    m_channelNcoStep = 0.0;
    m_channelNcoStepCos = 1.0;
    m_channelNcoStepSin = 0.0;
    rebuildFilters();
    reset();
    return true;
}

bool RtlSdrDsp::configure(int inputSampleRate, Demodulator demodulator, double audioGain,
                          int channelOffsetHz, bool spectrumInverted,
                          double ssbVoiceBandwidthHz, SsbAgcMode ssbAgcMode,
                          int ssbNotchFrequencyHz,
                          SsbNoiseReductionMode ssbNoiseReduction)
{
    if (!isSupportedSampleRateForDemodulator(inputSampleRate, demodulator)) {
        m_inputSampleRate = 0;
        m_outputDecimation = 0;
        return false;
    }

    m_inputSampleRate = inputSampleRate;
    m_demodulator = demodulator;
    m_outputSampleRate = demodulator == Demodulator::WeakSignal
        ? kDecoderSampleRate : kReceiverAudioSampleRate;
    // Broadcast FM first passes through a real anti-alias channel filter and
    // is then decimated to 240 kS/s before the non-linear discriminator. This
    // keeps out-of-channel RF from turning into broadband audio hiss without
    // returning to the old, prohibitively expensive FIR-per-input-sample path.
    m_channelDecimation = demodulator == Demodulator::WideFm ? 4 : 1;
    m_demodSampleRate = inputSampleRate / m_channelDecimation;
    m_outputDecimation = m_demodSampleRate / m_outputSampleRate;
    m_decoderDecimation = demodulator == Demodulator::WeakSignal ? m_outputDecimation : 0;
    m_spectrumInverted = spectrumInverted;
    m_audioGain = qBound(0.05, audioGain, 50.0);
    m_ssbVoiceBandwidthHz = qBound(1800.0, ssbVoiceBandwidthHz, 4000.0);
    m_ssbAgcMode = ssbAgcMode;
    m_ssbNotchFrequencyHz = qBound(0, ssbNotchFrequencyHz, 4800);
    m_ssbNoiseReduction = ssbNoiseReduction;
    m_channelNcoStep = -2.0 * kPi * static_cast<double>(channelOffsetHz)
        / static_cast<double>(inputSampleRate);
    m_channelNcoStepCos = std::cos(m_channelNcoStep);
    m_channelNcoStepSin = std::sin(m_channelNcoStep);
    rebuildFilters();
    reset();
    return true;
}

void RtlSdrDsp::reset()
{
    m_outputDecimationPhase = 0;
    m_outputDecimationSamples = 0;
    m_channelDecimationPhase = 0;
    m_channelDelayWrite = 0;
    m_decoderDecimationPhase = 0;
    m_decoderDelayWrite = 0;
    m_dcI = 0.0;
    m_dcQ = 0.0;
    m_previousI = 0.0;
    m_previousQ = 0.0;
    m_amDc = 0.0;
    m_deemphasis = 0.0;
    m_fmDc = 0.0;
    m_channelNcoCos = 1.0;
    m_channelNcoSin = 0.0;
    m_channelNcoNormalizeCounter = 0;
    m_channelIState1 = 0.0;
    m_channelIState2 = 0.0;
    m_channelQState1 = 0.0;
    m_channelQState2 = 0.0;
    m_audioState1 = 0.0;
    m_audioState2 = 0.0;
    m_outputDecimationAccumulator = 0.0;
    m_ssbDc = 0.0;
    m_ssbAgcEnvelope = 0.05;
    m_ssbNotchZ1 = 0.0;
    m_ssbNotchZ2 = 0.0;
    m_ssbNoiseFloor = 0.01;
    m_ssbNoiseEnvelope = 0.0;
    std::fill(m_channelDelayI.begin(), m_channelDelayI.end(), 0.0);
    std::fill(m_channelDelayQ.begin(), m_channelDelayQ.end(), 0.0);
    std::fill(m_ssbAudioDelay.begin(), m_ssbAudioDelay.end(), 0.0);
    std::fill(m_decoderDelay.begin(), m_decoderDelay.end(), 0.0);
}

bool RtlSdrDsp::enhancedSsbAudioEnabled() const
{
    return m_demodulator == Demodulator::Usb || m_demodulator == Demodulator::Lsb;
}

std::vector<double> RtlSdrDsp::buildLowpass(int tapCount, double cutoffHz, int sampleRate)
{
    std::vector<double> taps(static_cast<size_t>(tapCount), 0.0);
    if (tapCount <= 0 || cutoffHz <= 0.0 || sampleRate <= 0) {
        return taps;
    }

    const double normalizedCutoff = cutoffHz / static_cast<double>(sampleRate);
    const int midpoint = (tapCount - 1) / 2;
    double sum = 0.0;
    for (int tap = 0; tap < tapCount; ++tap) {
        const int offset = tap - midpoint;
        const double hamming = 0.54 - 0.46 * std::cos(2.0 * kPi * tap / (tapCount - 1));
        const double value = 2.0 * normalizedCutoff
            * sinc(2.0 * normalizedCutoff * offset) * hamming;
        taps[static_cast<size_t>(tap)] = value;
        sum += value;
    }
    if (std::abs(sum) > 1.0e-12) {
        for (double& tap : taps) {
            tap /= sum;
        }
    }
    return taps;
}

void RtlSdrDsp::rebuildFilters()
{
    m_decoderTaps.clear();
    m_decoderDelay.clear();
    m_channelTaps.clear();
    m_channelDelayI.clear();
    m_channelDelayQ.clear();
    m_ssbAudioTaps.clear();
    m_ssbAudioDelay.clear();
    m_ssbAudioDelayWrite = 0;
    m_channelAlpha = 1.0;
    m_audioAlpha = 1.0;

    if (m_demodulator == Demodulator::WeakSignal) {
        // Decoders receive the familiar 0..5 kHz real PCM stream.  IQ remains
        // intact for RF visualisation; this is intentionally decoder-only.
        m_decoderTaps = buildLowpass(kDecoderFilterTaps,
                                     std::min(5000.0, m_outputSampleRate * 0.42),
                                     m_inputSampleRate);
        m_decoderDelay.assign(m_decoderTaps.size(), 0.0);
        return;
    }

    double cutoffHz = 5000.0;
    double channelCutoffHz = 6000.0;
    switch (m_demodulator) {
    case Demodulator::WideFm: cutoffHz = 15000.0; channelCutoffHz = 100000.0; break;
    case Demodulator::NarrowFm: cutoffHz = 5000.0; channelCutoffHz = 12500.0; break;
    case Demodulator::Am: cutoffHz = 9000.0; channelCutoffHz = 10000.0; break;
    case Demodulator::Usb:
    case Demodulator::Lsb:
        cutoffHz = m_ssbVoiceBandwidthHz;
        channelCutoffHz = qMin(5000.0, m_ssbVoiceBandwidthHz + 700.0);
        break;
    case Demodulator::Cw: cutoffHz = 3500.0; channelCutoffHz = 1500.0; break;
    case Demodulator::WeakSignal: break;
    }
    if (m_demodulator == Demodulator::WideFm) {
        // 90 kHz carries the complete mono broadcast channel (75 kHz peak
        // deviation plus 15 kHz programme audio). A 127-tap Hamming FIR gives
        // the decimator a useful stop band before its new 120 kHz Nyquist
        // limit; convolution is evaluated only once every four input samples.
        m_channelTaps = buildLowpass(127, 90000.0, m_inputSampleRate);
        m_channelDelayI.assign(m_channelTaps.size(), 0.0);
        m_channelDelayQ.assign(m_channelTaps.size(), 0.0);
    }
    if (enhancedSsbAudioEnabled()) {
        // This intentionally runs at the already-decimated 48 kHz audio rate,
        // not at the 240/288 kS/s IQ rate.  It improves SSB intelligibility
        // without changing the RF tuning reference or risking capture backlog.
        m_ssbAudioTaps = buildLowpass(129, m_ssbVoiceBandwidthHz, m_outputSampleRate);
        m_ssbAudioDelay.assign(m_ssbAudioTaps.size(), 0.0);

        m_ssbNotchEnabled = m_ssbNotchFrequencyHz >= 80
            && m_ssbNotchFrequencyHz < m_outputSampleRate / 2 - 100;
        if (m_ssbNotchEnabled) {
            constexpr double kNotchQ = 18.0;
            const double omega = 2.0 * kPi * m_ssbNotchFrequencyHz
                / static_cast<double>(m_outputSampleRate);
            const double alpha = std::sin(omega) / (2.0 * kNotchQ);
            const double a0 = 1.0 + alpha;
            m_ssbNotchB0 = 1.0 / a0;
            m_ssbNotchB1 = -2.0 * std::cos(omega) / a0;
            m_ssbNotchB2 = 1.0 / a0;
            m_ssbNotchA1 = -2.0 * std::cos(omega) / a0;
            m_ssbNotchA2 = (1.0 - alpha) / a0;
        }
    } else {
        m_ssbNotchEnabled = false;
    }
    // Two cascaded one-pole sections are intentionally used here instead of
    // convolving 127 FIR taps for both I and Q at up to 1.92 MS/s. The old
    // direct convolution consumed the reader thread for longer than real time
    // and queued multi-second callbacks onto the UI. Boxcar decimation plus
    // these channel/audio sections keeps the receiver real-time and provides
    // sufficient adjacent-channel rejection for the general-radio path.
    const double boundedChannelCutoff =
        std::min(channelCutoffHz, m_inputSampleRate * 0.45);
    const double boundedAudioCutoff =
        std::min(cutoffHz, m_outputSampleRate * 0.42);
    m_channelAlpha = 1.0 - std::exp(-2.0 * kPi * boundedChannelCutoff
                                    / static_cast<double>(m_inputSampleRate));
    m_audioAlpha = 1.0 - std::exp(-2.0 * kPi * boundedAudioCutoff
                                  / static_cast<double>(m_outputSampleRate));
}

double RtlSdrDsp::filterSample(double value, const std::vector<double>& taps,
                               std::vector<double>& delay, int& writeIndex)
{
    if (taps.empty() || delay.size() != taps.size()) {
        return value;
    }
    delay[static_cast<size_t>(writeIndex)] = value;
    writeIndex = (writeIndex + 1) % static_cast<int>(taps.size());

    double filtered = 0.0;
    int index = writeIndex - 1;
    if (index < 0) index += static_cast<int>(taps.size());
    for (size_t tap = 0; tap < taps.size(); ++tap) {
        filtered += taps[tap] * delay[static_cast<size_t>(index)];
        if (--index < 0) index = static_cast<int>(taps.size()) - 1;
    }
    return filtered;
}

double RtlSdrDsp::demodulate(double i, double q)
{
    switch (m_demodulator) {
    case Demodulator::WideFm:
    case Demodulator::NarrowFm: {
        const double phase = std::atan2(m_previousI * q - m_previousQ * i,
                                        m_previousI * i + m_previousQ * q);
        m_previousI = i;
        m_previousQ = q;
        // A simple, stable de-emphasis stage.  50 us is broadcast FM; 300 us
        // is the customary narrow-FM value.
        const double tau = m_demodulator == Demodulator::WideFm ? 50e-6 : 300e-6;
        const int fmRate = qMax(1, m_demodSampleRate);
        const double alpha = 1.0 - std::exp(-1.0 / (tau * fmRate));
        m_deemphasis += alpha * (phase - m_deemphasis);
        // A station that is not exactly at the software channel centre leaves
        // a constant discriminator offset. Sending that DC component to the
        // speaker used to drive the following stages into hard clipping and
        // produced the loud hiss heard during live WFM reception. Remove it
        // with a 20 Hz high-pass while retaining all useful broadcast audio.
        const double dcAlpha = 1.0 - std::exp(-2.0 * kPi * 20.0
                                              / static_cast<double>(fmRate));
        m_fmDc += dcAlpha * (m_deemphasis - m_fmDc);
        // Convert discriminator phase to approximately full scale at the
        // modulation's standard peak deviation. This must follow the actual
        // post-decimation discriminator rate: after the new 240 kS/s WFM
        // selector, retaining the former 960 kS/s constant over-amplified the
        // recovered programme and brought peaks back close to clipping.
        const double peakDeviationHz = m_demodulator == Demodulator::WideFm
            ? 75000.0 : 5000.0;
        const double peakPhase = 2.0 * kPi * peakDeviationHz
            / static_cast<double>(fmRate);
        return (m_deemphasis - m_fmDc) / qMax(0.01, peakPhase);
    }
    case Demodulator::Am: {
        const double magnitude = std::sqrt(i * i + q * q);
        m_amDc += 0.0005 * (magnitude - m_amDc);
        return magnitude - m_amDc;
    }
    case Demodulator::Usb:
        return i + q;
    case Demodulator::Lsb:
        return i - q;
    case Demodulator::Cw:
        return i;
    case Demodulator::WeakSignal:
        return i;
    }
    return 0.0;
}

short RtlSdrDsp::toPcm(double value) const
{
    // The discriminator/envelope paths have a much wider native range than
    // decoder PCM. Keep the user's gain useful without slamming ordinary FM
    // noise or a strong carrier into a hard 16-bit clipper.
    const double receiverScale = m_demodulator == Demodulator::WeakSignal ? 1.0 : 0.25;
    double scaled = value * m_audioGain * receiverScale;
    if (m_demodulator != Demodulator::WeakSignal) {
        scaled = std::tanh(scaled);
    }
    return static_cast<short>(qBound(-32768,
                                     qRound(scaled * 32767.0),
                                     32767));
}

RtlSdrDsp::Result RtlSdrDsp::processFrame(const unsigned char *data, int bytes, bool includeIq)
{
    Result result;
    if (!data || bytes < 2 || !isConfigured()) {
        return result;
    }

    const int sampleCount = bytes / 2;
    if (includeIq) {
        result.iq.reserve(sampleCount * 2);
    }
    if (m_demodulator == Demodulator::WeakSignal) {
        result.decoderPcm.reserve(sampleCount / m_outputDecimation + 2);
    } else {
        result.audioPcm.reserve(sampleCount / m_outputDecimation + 2);
    }

    for (int sample = 0; sample < sampleCount; ++sample) {
        const double rawI = (static_cast<int>(data[2 * sample]) - 127.5) / 127.5;
        const double rawQ = (static_cast<int>(data[2 * sample + 1]) - 127.5) / 127.5;
        // Remove the tuner DC component before handing IQ to either the RF
        // display or a demodulator.  This avoids a false carrier at centre.
        m_dcI += 0.0005 * (rawI - m_dcI);
        m_dcQ += 0.0005 * (rawQ - m_dcQ);
        const double i = rawI - m_dcI;
        const double q = rawQ - m_dcQ;

        if (includeIq) {
            result.iq.append(static_cast<short>(qBound(-32768, qRound(i * 32767.0), 32767)));
            const double displayQ = m_spectrumInverted ? -q : q;
            result.iq.append(static_cast<short>(qBound(-32768, qRound(displayQ * 32767.0), 32767)));
        }

        double channelSourceI = i;
        double channelSourceQ = q;
        if (std::abs(m_channelNcoStep) > 1.0e-12) {
            channelSourceI = i * m_channelNcoCos - q * m_channelNcoSin;
            channelSourceQ = i * m_channelNcoSin + q * m_channelNcoCos;
            const double nextCos = m_channelNcoCos * m_channelNcoStepCos
                - m_channelNcoSin * m_channelNcoStepSin;
            const double nextSin = m_channelNcoSin * m_channelNcoStepCos
                + m_channelNcoCos * m_channelNcoStepSin;
            m_channelNcoCos = nextCos;
            m_channelNcoSin = nextSin;
            if (++m_channelNcoNormalizeCounter >= 4096) {
                m_channelNcoNormalizeCounter = 0;
                const double magnitude = std::hypot(m_channelNcoCos, m_channelNcoSin);
                if (magnitude > 1.0e-12) {
                    m_channelNcoCos /= magnitude;
                    m_channelNcoSin /= magnitude;
                }
            }
        }
        // A conjugated IF restores the normal RF direction for both the
        // panadapter and phase-sensitive FM/SSB demodulators. Channel tuning
        // itself remains on the physical IF coordinates above.
        if (m_spectrumInverted) {
            channelSourceQ = -channelSourceQ;
        }

        if (m_demodulator == Demodulator::WeakSignal) {
            // Preserve the established decoder convention (real low-sideband
            // PCM) but keep it completely separate from the RF IQ display.
            const double filtered = filterSample(channelSourceI, m_decoderTaps,
                                                 m_decoderDelay, m_decoderDelayWrite);
            if (++m_decoderDecimationPhase >= m_decoderDecimation) {
                m_decoderDecimationPhase = 0;
                result.decoderPcm.append(toPcm(filtered));
            }
            continue;
        }

        // Select one RF channel before any demodulation. WFM uses a proper
        // decimating FIR because a discriminator is non-linear: filtering its
        // audio afterwards cannot remove adjacent RF that was already mixed
        // into hiss. Narrow modes retain the inexpensive two-pole selector.
        double channelI = 0.0;
        double channelQ = 0.0;
        if (m_demodulator == Demodulator::WideFm && !m_channelTaps.empty()) {
            m_channelDelayI[static_cast<size_t>(m_channelDelayWrite)] = channelSourceI;
            m_channelDelayQ[static_cast<size_t>(m_channelDelayWrite)] = channelSourceQ;
            m_channelDelayWrite = (m_channelDelayWrite + 1)
                % static_cast<int>(m_channelTaps.size());
            if (++m_channelDecimationPhase < m_channelDecimation) {
                continue;
            }
            m_channelDecimationPhase = 0;
            int delayIndex = m_channelDelayWrite - 1;
            if (delayIndex < 0) delayIndex = static_cast<int>(m_channelTaps.size()) - 1;
            for (size_t tap = 0; tap < m_channelTaps.size(); ++tap) {
                channelI += m_channelTaps[tap] * m_channelDelayI[static_cast<size_t>(delayIndex)];
                channelQ += m_channelTaps[tap] * m_channelDelayQ[static_cast<size_t>(delayIndex)];
                if (--delayIndex < 0) delayIndex = static_cast<int>(m_channelTaps.size()) - 1;
            }
        } else {
            m_channelIState1 += m_channelAlpha * (channelSourceI - m_channelIState1);
            m_channelIState2 += m_channelAlpha * (m_channelIState1 - m_channelIState2);
            m_channelQState1 += m_channelAlpha * (channelSourceQ - m_channelQState1);
            m_channelQState2 += m_channelAlpha * (m_channelQState1 - m_channelQState2);
            channelI = m_channelIState2;
            channelQ = m_channelQState2;
        }
        const double demodulated = demodulate(channelI, channelQ);
        m_outputDecimationAccumulator += demodulated;
        ++m_outputDecimationSamples;
        if (++m_outputDecimationPhase >= m_outputDecimation) {
            m_outputDecimationPhase = 0;
            const double decimated = m_outputDecimationSamples > 0
                ? m_outputDecimationAccumulator / m_outputDecimationSamples : 0.0;
            m_outputDecimationAccumulator = 0.0;
            m_outputDecimationSamples = 0;
            m_audioState1 += m_audioAlpha * (decimated - m_audioState1);
            m_audioState2 += m_audioAlpha * (m_audioState1 - m_audioState2);
            double audio = m_audioState2;
            if (enhancedSsbAudioEnabled()) {
                // The RTL-SDR's DC spur and any residual BFO component are
                // particularly noticeable on SSB speech.  Remove the slow
                // component before the sharp voice-band filter.
                constexpr double kDcCutoffHz = 90.0;
                const double dcAlpha = 1.0 - std::exp(-2.0 * kPi * kDcCutoffHz
                                                      / m_outputSampleRate);
                m_ssbDc += dcAlpha * (audio - m_ssbDc);
                audio = filterSample(audio - m_ssbDc, m_ssbAudioTaps,
                                     m_ssbAudioDelay, m_ssbAudioDelayWrite);

                if (m_ssbNotchEnabled) {
                    const double notched = m_ssbNotchB0 * audio + m_ssbNotchZ1;
                    m_ssbNotchZ1 = m_ssbNotchB1 * audio - m_ssbNotchA1 * notched
                        + m_ssbNotchZ2;
                    m_ssbNotchZ2 = m_ssbNotchB2 * audio - m_ssbNotchA2 * notched;
                    audio = notched;
                }

                // Audio AGC is deliberately confined to RTL-SDR USB/LSB. It
                // gives speech a stable listening level without tracking each
                // syllable and never changes any FT8 decoder samples.
                if (m_ssbAgcMode != SsbAgcMode::Off) {
                    const double magnitude = std::abs(audio);
                    const bool medium = m_ssbAgcMode == SsbAgcMode::Medium;
                    const double envelopeAlpha = magnitude > m_ssbAgcEnvelope
                        ? (medium ? 0.015 : 0.002)
                        : (medium ? 0.0010 : 0.00008);
                    m_ssbAgcEnvelope += envelopeAlpha * (magnitude - m_ssbAgcEnvelope);
                    const double agcGain = qBound(
                        0.25, 0.80 / qMax(0.015, m_ssbAgcEnvelope), 8.0);
                    audio *= agcGain;
                }

                // Optional low-cost noise reduction: a speech-aware expander
                // that learns the quiet noise floor and attenuates it between
                // speech peaks. It is intentionally post-demodulation and
                // restricted to RTL-SDR SSB listening.
                if (m_ssbNoiseReduction != SsbNoiseReductionMode::Off) {
                    const double magnitude = std::abs(audio);
                    const double envelopeAlpha = magnitude > m_ssbNoiseEnvelope
                        ? 0.035 : 0.0012;
                    m_ssbNoiseEnvelope += envelopeAlpha
                        * (magnitude - m_ssbNoiseEnvelope);
                    const bool likelyNoise = magnitude
                        < qMax(0.012, m_ssbNoiseEnvelope * 0.35);
                    const double floorAlpha = likelyNoise ? 0.0025 : 0.00001;
                    m_ssbNoiseFloor += floorAlpha * (magnitude - m_ssbNoiseFloor);
                    m_ssbNoiseFloor = qBound(0.002, m_ssbNoiseFloor, 0.75);

                    const bool medium = m_ssbNoiseReduction
                        == SsbNoiseReductionMode::Medium;
                    const double threshold = m_ssbNoiseFloor * (medium ? 2.8 : 1.8);
                    const double fullLevel = threshold * (medium ? 2.6 : 2.0);
                    const double minimumGain = medium ? 0.18 : 0.55;
                    double reductionGain = minimumGain;
                    if (m_ssbNoiseEnvelope >= fullLevel) {
                        reductionGain = 1.0;
                    } else if (m_ssbNoiseEnvelope > threshold) {
                        const double mix = (m_ssbNoiseEnvelope - threshold)
                            / qMax(0.001, fullLevel - threshold);
                        reductionGain += (1.0 - minimumGain) * mix * mix * (3.0 - 2.0 * mix);
                    }
                    audio *= reductionGain;
                }
            }
            result.audioPcm.append(toPcm(audio));
        }
    }
    return result;
}

QVector<short> RtlSdrDsp::process(const unsigned char *data, int bytes)
{
    return processFrame(data, bytes, false).decoderPcm;
}
