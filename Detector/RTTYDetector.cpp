#include "RTTYDetector.hpp"
#include <cmath>
#include <QDebug>

namespace decodium {
namespace rtty {

namespace {

double smoothingFactor(int samplesPerSymbol)
{
    int const smoothingWindow = qMax(4, samplesPerSymbol / 6);
    return std::exp(-1.0 / static_cast<double>(smoothingWindow));
}

double powerRatio(double a, double b)
{
    double const stronger = std::max(a, b);
    double const weaker = std::max(1e-12, std::min(a, b));
    return stronger / weaker;
}

}

RTTYDetector::RTTYDetector(unsigned frameRate, double baudRate, 
                           double markFreq, double spaceFreq, QObject* parent)
    : QObject(parent),
      m_frameRate(frameRate),
      m_baudRate(baudRate),
      m_markFreq(markFreq),
      m_spaceFreq(spaceFreq),
      m_samplesPerHalfSymbol(0),
      m_samplesUntilDecision(0),
      m_idleMarkSamples(0),
      m_markPhase(0),
      m_spacePhase(0),
      m_markFilter(0, 0),
      m_spaceFilter(0, 0),
      m_state(RxState::Idle),
      m_bitIndex(0),
      m_currentSymbol(0)
{
    m_samplesPerSymbol = static_cast<int>(m_frameRate / m_baudRate);
    m_samplesPerHalfSymbol = qMax(1, m_samplesPerSymbol / 2);
}

void RTTYDetector::setParameters(double baudRate, double markFreq, double spaceFreq) {
    m_baudRate = baudRate;
    m_markFreq = markFreq;
    m_spaceFreq = spaceFreq;
    m_samplesPerSymbol = static_cast<int>(m_frameRate / m_baudRate);
    m_samplesPerHalfSymbol = qMax(1, m_samplesPerSymbol / 2);
    resetDecoderState();
}

void RTTYDetector::resetDecoderState() {
    m_markFilter = std::complex<double>(0, 0);
    m_spaceFilter = std::complex<double>(0, 0);
    m_markPhase = 0.0;
    m_spacePhase = 0.0;
    m_samplesUntilDecision = 0;
    m_idleMarkSamples = 0;
    m_state = RxState::Idle;
    m_bitIndex = 0;
    m_currentSymbol = 0;
    m_baudot.reset();
}

void RTTYDetector::processAudio(const qint16* data, int numSamples) {
    double const markPhaseInc = TWO_PI * m_markFreq / m_frameRate;
    double const spacePhaseInc = TWO_PI * m_spaceFreq / m_frameRate;
    double const alpha = smoothingFactor(m_samplesPerSymbol);
    double const beta = 1.0 - alpha;

    for (int i = 0; i < numSamples; ++i) {
        double const sample = static_cast<double>(data[i]) / 32768.0;

        std::complex<double> const mixedMark = sample * std::polar(1.0, -m_markPhase);
        std::complex<double> const mixedSpace = sample * std::polar(1.0, -m_spacePhase);
        m_markFilter = alpha * m_markFilter + beta * mixedMark;
        m_spaceFilter = alpha * m_spaceFilter + beta * mixedSpace;

        m_markPhase += markPhaseInc;
        if (m_markPhase >= TWO_PI) m_markPhase -= TWO_PI;

        m_spacePhase += spacePhaseInc;
        if (m_spacePhase >= TWO_PI) m_spacePhase -= TWO_PI;

        double const markEnergy = std::norm(m_markFilter);
        double const spaceEnergy = std::norm(m_spaceFilter);
        bool const isMark = markEnergy >= spaceEnergy;
        double const confidence = powerRatio(markEnergy, spaceEnergy);

        switch (m_state) {
        case RxState::Idle:
            if (isMark) {
                m_idleMarkSamples = qMin(m_idleMarkSamples + 1, 4 * m_samplesPerSymbol);
            } else {
                bool const enoughIdle = m_idleMarkSamples >= m_samplesPerHalfSymbol;
                if (enoughIdle && confidence >= 1.05) {
                    m_state = RxState::StartBit;
                    m_samplesUntilDecision = m_samplesPerHalfSymbol;
                    m_currentSymbol = 0;
                    m_bitIndex = 0;
                }
                m_idleMarkSamples = 0;
            }
            break;

        case RxState::StartBit:
            if (--m_samplesUntilDecision <= 0) {
                if (!isMark && confidence >= 1.02) {
                    m_state = RxState::DataBits;
                    m_bitIndex = 0;
                    m_currentSymbol = 0;
                    m_samplesUntilDecision = m_samplesPerSymbol;
                } else {
                    m_state = RxState::Idle;
                    m_idleMarkSamples = isMark ? m_samplesPerHalfSymbol : 0;
                }
            }
            break;

        case RxState::DataBits:
            if (--m_samplesUntilDecision <= 0) {
                if (isMark) {
                    m_currentSymbol |= (1 << m_bitIndex);
                }
                ++m_bitIndex;
                if (m_bitIndex >= 5) {
                    m_state = RxState::StopBit;
                }
                m_samplesUntilDecision = m_samplesPerSymbol;
            }
            break;

        case RxState::StopBit:
            if (--m_samplesUntilDecision <= 0) {
                if (isMark && confidence >= 1.02) {
                    QChar const decoded = m_baudot.decodeSymbol(m_currentSymbol);
                    if (decoded != '\0') {
                        Q_EMIT charDecoded(decoded);
                        Q_EMIT textDecoded(QString(decoded));
                    }
                }
                m_state = RxState::Idle;
                m_idleMarkSamples = isMark ? m_samplesPerHalfSymbol : 0;
            }
            break;
        }
    }
}

} // namespace rtty
} // namespace decodium
