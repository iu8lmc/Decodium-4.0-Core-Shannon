#ifndef RTTYDETECTOR_HPP
#define RTTYDETECTOR_HPP

#include <QObject>
#include <QByteArray>
#include <vector>
#include <complex>

#include "Decoder/BaudotDecoder.hpp"

namespace decodium {
namespace rtty {

class RTTYDetector : public QObject {
    Q_OBJECT

public:
    RTTYDetector(unsigned frameRate, double baudRate = 45.45, 
                 double markFreq = 2125.0, double spaceFreq = 2295.0, 
                 QObject* parent = nullptr);

    /// Set RX decoding parameters interactively
    void setParameters(double baudRate, double markFreq, double spaceFreq);

    /// Feed incoming 16-bit PCM audio from the soundcard
    Q_SLOT void processAudio(const qint16* data, int numSamples);

    Q_SIGNAL void charDecoded(QChar c);
    Q_SIGNAL void textDecoded(QString text);

private:
    unsigned m_frameRate;
    double m_baudRate;
    double m_markFreq;
    double m_spaceFreq;

    // Sliding DFT / Goertzel parameters
    int m_samplesPerSymbol;
    int m_samplesPerHalfSymbol;
    int m_samplesUntilDecision;
    int m_idleMarkSamples;
    
    // NCO / Mixers
    double m_markPhase;
    double m_spacePhase;
    static constexpr double TWO_PI = 6.28318530717958647692;

    // Coherent low-pass accumulators
    std::complex<double> m_markFilter;
    std::complex<double> m_spaceFilter;

    // Bit framing state machine
    enum class RxState {
        Idle,
        StartBit,
        DataBits,
        StopBit
    };
    RxState m_state;
    int m_bitIndex;
    uint8_t m_currentSymbol;
    
    // State machine logic
    BaudotHandler m_baudot;

    void resetDecoderState();
};

} // namespace rtty
} // namespace decodium

#endif // RTTYDETECTOR_HPP
