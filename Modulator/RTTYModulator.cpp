#include "RTTYModulator.hpp"
#include <cmath>
#include <QDebug>

namespace decodium {
namespace rtty {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

void appendBitCell(QQueue<RTTYModulator::BitCell>& bitQueue, bool mark, double units)
{
    bitQueue.enqueue(RTTYModulator::BitCell {mark, units});
}

void appendSymbolBits(QQueue<RTTYModulator::BitCell>& bitQueue, uint8_t symbol, double stopBits)
{
    double const clampedStopBits = (stopBits == 1.0 || stopBits == 1.5 || stopBits == 2.0)
        ? stopBits
        : 2.0;

    appendBitCell(bitQueue, false, 1.0); // Start bit (Space)

    for (int i = 0; i < 5; ++i) {
        appendBitCell(bitQueue, (symbol >> i) & 1U, 1.0);
    }

    appendBitCell(bitQueue, true, clampedStopBits); // Stop bit(s)
}

}

QVector<float> synthesizeWave(QByteArray const& symbols,
                              unsigned frameRate,
                              double baudRate,
                              double markFreq,
                              double spaceFreq,
                              double stopBits,
                              int tailBits)
{
    QQueue<RTTYModulator::BitCell> bitQueue;
    double totalBitUnits = 0.0;
    for (auto symbol : symbols) {
        appendSymbolBits(bitQueue, static_cast<uint8_t>(symbol), stopBits);
        totalBitUnits += 6.0 + ((stopBits == 1.0 || stopBits == 1.5 || stopBits == 2.0) ? stopBits : 2.0);
    }
    for (int i = 0; i < tailBits; ++i) {
        appendBitCell(bitQueue, true, 1.0);
        totalBitUnits += 1.0;
    }

    double const samplesPerBit = static_cast<double>(frameRate) / baudRate;
    int const totalSamples = qMax(1, static_cast<int>(std::ceil(totalBitUnits * samplesPerBit)));
    QVector<float> wave(totalSamples);

    double phase = 0.0;
    auto currentCell = bitQueue.isEmpty() ? RTTYModulator::BitCell {true, 1.0} : bitQueue.dequeue();
    bool currentBit = currentCell.mark;
    double currentSamplesRemaining = qMax(1.0, currentCell.units * samplesPerBit);

    for (int sampleIndex = 0; sampleIndex < totalSamples; ++sampleIndex) {
        double const currentFreq = currentBit ? markFreq : spaceFreq;
        double const phaseIncrement = kTwoPi * currentFreq / frameRate;
        wave[sampleIndex] = static_cast<float>(0.8 * std::sin(phase));
        phase += phaseIncrement;
        if (phase >= kTwoPi) {
            phase -= kTwoPi;
        }

        currentSamplesRemaining -= 1.0;
        while (currentSamplesRemaining <= 0.0 && !bitQueue.isEmpty()) {
            currentCell = bitQueue.dequeue();
            currentBit = currentCell.mark;
            currentSamplesRemaining += qMax(1.0, currentCell.units * samplesPerBit);
        }
    }

    return wave;
}

RTTYModulator::RTTYModulator(unsigned frameRate, double baudRate, 
                             double markFreq, double spaceFreq,
                             double stopBits,
                             QObject* parent)
    : AudioDevice(parent),
      m_frameRate(frameRate),
      m_baudRate(baudRate),
      m_markFreq(markFreq),
      m_spaceFreq(spaceFreq),
      m_stopBits(stopBits),
      m_phase(0.0),
      m_currentBitSamplesRemaining(0.0),
      m_currentBit(true), // Idle is Mark
      m_active(false),
      m_payloadStarted(false),
      m_draining(false),
      m_amp(32767.0 * 0.8) // 80% volume roughly
{
    m_samplesPerBit = static_cast<double>(m_frameRate) / m_baudRate;
}

void RTTYModulator::setParameters(double baudRate, double markFreq, double spaceFreq, double stopBits) {
    QMutexLocker locker(&m_mutex);
    m_baudRate = baudRate;
    m_markFreq = markFreq;
    m_spaceFreq = spaceFreq;
    m_stopBits = (stopBits == 1.0 || stopBits == 1.5 || stopBits == 2.0) ? stopBits : 2.0;
    m_samplesPerBit = static_cast<double>(m_frameRate) / m_baudRate;
}

void RTTYModulator::enqueueBit(bool isMark) {
    QMutexLocker locker(&m_mutex);
    appendBitCell(m_bitQueue, isMark, 1.0);
}

void RTTYModulator::addSymbolBits(uint8_t symbol) {
    QMutexLocker locker(&m_mutex);
    appendSymbolBits(m_bitQueue, symbol, m_stopBits);
}

void RTTYModulator::enqueueSymbol(uint8_t symbol) {
    addSymbolBits(symbol);
}

void RTTYModulator::startTx() {
    QMutexLocker locker(&m_mutex);
    m_active = true;
    m_phase = 0.0;
    m_currentBitSamplesRemaining = 0.0;
    m_payloadStarted = false;
    m_draining = false;
    if (!m_bitQueue.isEmpty()) {
        auto const currentCell = m_bitQueue.dequeue();
        m_currentBit = currentCell.mark;
        m_currentBitSamplesRemaining = qMax(1.0, currentCell.units * m_samplesPerBit);
        m_payloadStarted = true;
    } else {
        m_currentBit = true; // start with idle Mark
        m_currentBitSamplesRemaining = m_samplesPerBit;
    }
}

void RTTYModulator::stopTx() {
    QMutexLocker locker(&m_mutex);
    m_active = false;
    m_payloadStarted = false;
    m_draining = false;
    m_currentBit = true;
    m_currentBitSamplesRemaining = 0.0;
    m_bitQueue.clear();
}

qint64 RTTYModulator::readData(char* data, qint64 maxSize) {
    qint16* out = reinterpret_cast<qint16*>(data);
    qint64 samplesToWrite = maxSize / sizeof(qint16);
    qint64 samplesWritten = 0;
    bool emitTxComplete = false;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_active) {
            return 0;
        }

        for (qint64 i = 0; i < samplesToWrite; ++i) {
            while (m_currentBitSamplesRemaining <= 0.0) {
                if (!m_bitQueue.isEmpty()) {
                    auto const nextCell = m_bitQueue.dequeue();
                    m_currentBit = nextCell.mark;
                    m_currentBitSamplesRemaining += qMax(1.0, nextCell.units * m_samplesPerBit);
                    m_payloadStarted = true;
                    m_draining = false;
                } else if (m_payloadStarted) {
                    if (m_draining) {
                        m_active = false;
                        m_payloadStarted = false;
                        emitTxComplete = true;
                        break;
                    }
                    m_currentBit = true;
                    m_currentBitSamplesRemaining += m_samplesPerBit;
                    m_draining = true;
                } else {
                    break;
                }
            }

            if (!m_active) {
                break;
            }

            double const currentFreq = m_currentBit ? m_markFreq : m_spaceFreq;
            double const phaseIncrement = TWO_PI * currentFreq / m_frameRate;

            out[i] = static_cast<qint16>(m_amp * std::sin(m_phase));

            m_phase += phaseIncrement;
            if (m_phase >= TWO_PI) {
                m_phase -= TWO_PI;
            }

            m_currentBitSamplesRemaining -= 1.0;
            ++samplesWritten;
        }
    }

    if (emitTxComplete) {
        Q_EMIT txComplete();
    }

    return samplesWritten * sizeof(qint16);
}

} // namespace rtty
} // namespace decodium
