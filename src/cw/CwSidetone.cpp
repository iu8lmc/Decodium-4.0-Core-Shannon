#include "CwSidetone.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>

#include <cmath>

namespace decodium::cw {

namespace {
constexpr int kRate = 48000;
constexpr double kRampMs = 5.0;
}

// Genera i campioni su richiesta della scheda audio: la coda dei segmenti
// (tono si'/no, durata in campioni) e' l'unica cosa condivisa col thread GUI.
class SidetoneSource : public QIODevice {
public:
    SidetoneSource() { open(QIODevice::ReadOnly); }

    void push(qint64 samples, bool on)
    {
        if (samples <= 0)
            return;
        QMutexLocker l(&m_lock);
        m_queue.append({samples, on});
    }
    void setTone(int hz) { QMutexLocker l(&m_lock); m_step = 2.0 * M_PI * hz / kRate; }
    void setGain(double g) { QMutexLocker l(&m_lock); m_gain = g; }
    void flush() { QMutexLocker l(&m_lock); m_queue.clear(); }
    bool idle() const { QMutexLocker l(&m_lock); return m_queue.isEmpty() && m_env < 0.001; }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return 1 << 20; }

protected:
    qint64 readData(char* data, qint64 maxLen) override
    {
        auto* out = reinterpret_cast<qint16*>(data);
        const qint64 n = maxLen / 2;
        QMutexLocker l(&m_lock);
        const double rampStep = 1.0 / (kRampMs * kRate / 1000.0);
        for (qint64 i = 0; i < n; ++i) {
            bool on = false;
            if (!m_queue.isEmpty()) {
                on = m_queue.first().on;
                if (--m_queue.first().left <= 0)
                    m_queue.removeFirst();
            }
            m_env += on ? rampStep : -rampStep;
            m_env = std::clamp(m_env, 0.0, 1.0);
            m_phase += m_step;
            if (m_phase > 2.0 * M_PI)
                m_phase -= 2.0 * M_PI;
            // rampa a coseno rialzato: niente colpi di tasto
            const double e = 0.5 - 0.5 * std::cos(M_PI * m_env);
            out[i] = static_cast<qint16>(std::sin(m_phase) * e * m_gain * 32000.0);
        }
        return n * 2;
    }
    qint64 writeData(const char*, qint64) override { return -1; }

private:
    struct Seg { qint64 left; bool on; };
    mutable QMutex m_lock;
    QList<Seg> m_queue;
    double m_phase {0.0};
    double m_step {2.0 * M_PI * 700 / kRate};
    double m_env {0.0};
    double m_gain {0.35};
};

CwSidetone::CwSidetone(QObject* parent)
    : QObject(parent)
    , m_source(std::make_unique<SidetoneSource>())
{
    m_idle.setInterval(3000);
    connect(&m_idle, &QTimer::timeout, this, &CwSidetone::idleCheck);
}

CwSidetone::~CwSidetone()
{
    if (m_sink) {
        m_sink->stop();
        delete m_sink;
    }
}

void CwSidetone::ensureSink()
{
    if (m_sink && m_sink->state() != QAudio::StoppedState)
        return;
    if (m_sink)
        delete m_sink;
    QAudioFormat fmt;
    fmt.setSampleRate(kRate);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);
    QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (dev.isNull())
        return;
    m_sink = new QAudioSink(dev, fmt, this);
    m_sink->setBufferSize(kRate / 10 * 2);   // ~100 ms: la latenza del tono resta bassa
    m_sink->start(m_source.get());
    m_idle.start();
}

void CwSidetone::enqueue(const QList<int>& deltasMs, const QList<bool>& down, int toneHz)
{
    m_source->setTone(toneHz > 0 ? toneHz : 700);
    m_source->setGain(m_volume);
    ensureSink();
    for (int i = 0; i < deltasMs.size() && i < down.size(); ++i) {
        // lo stato che finisce con questo istante e' l'opposto di quello nuovo
        m_source->push(static_cast<qint64>(deltasMs.at(i)) * kRate / 1000, !down.at(i));
    }
}

void CwSidetone::clear()
{
    m_source->flush();
}

void CwSidetone::setVolume(double volume)
{
    m_volume = std::clamp(volume, 0.0, 1.0);
}

void CwSidetone::idleCheck()
{
    if (m_source->idle() && m_sink) {
        m_sink->stop();
        m_idle.stop();
    }
}

}  // namespace decodium::cw
