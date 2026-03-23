#ifndef RTTYMODULATOR_HPP
#define RTTYMODULATOR_HPP

#include "Audio/AudioDevice.hpp"
#include <QByteArray>
#include <QQueue>
#include <QMutex>
#include <QVector>

namespace decodium {
namespace rtty {

QVector<float> synthesizeWave(QByteArray const& symbols,
                              unsigned frameRate,
                              double baudRate = 45.45,
                              double markFreq = 2125.0,
                              double spaceFreq = 2295.0,
                              double stopBits = 2.0,
                              int tailBits = 2);

class RTTYModulator : public AudioDevice {
    Q_OBJECT

public:
    struct BitCell {
        bool mark;
        double units;
    };

    RTTYModulator(unsigned frameRate, double baudRate = 45.45, 
                  double markFreq = 2125.0, double spaceFreq = 2295.0,
                  double stopBits = 2.0,
                  QObject* parent = nullptr);

    ~RTTYModulator() override = default;

    /// Enqueue a 5-bit ITA2 symbol (with start and stop bits) to be transmitted
    Q_SLOT void enqueueSymbol(uint8_t symbol);

    /// Enqueue an arbitrary bit (0=Space, 1=Mark)
    Q_SLOT void enqueueBit(bool isMark);

    bool isActive() const { return m_active; }
    void setParameters(double baudRate, double markFreq, double spaceFreq, double stopBits = 2.0);

    Q_SLOT void startTx();
    Q_SLOT void stopTx();

Q_SIGNALS:
    void txComplete() const;

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* /*data*/, qint64 /*maxSize*/) override { return -1; }

private:
    unsigned m_frameRate;
    double m_baudRate;
    double m_markFreq;
    double m_spaceFreq;
    double m_stopBits;

    static constexpr double TWO_PI = 6.28318530717958647692;
    double m_phase;
    double m_samplesPerBit;
    double m_currentBitSamplesRemaining;
    bool m_currentBit;

    bool m_active;
    bool m_payloadStarted;
    bool m_draining;
    double m_amp;

    QQueue<BitCell> m_bitQueue;
    QMutex m_mutex;

    void addSymbolBits(uint8_t symbol);
};

} // namespace rtty
} // namespace decodium

#endif // RTTYMODULATOR_HPP
