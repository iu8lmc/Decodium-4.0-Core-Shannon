// SPDX-License-Identifier: GPL-3.0-or-later

#include "RtlSdrInput.h"
#include "RtlSdrCapabilities.h"
#include "RtlSdrDsp.h"

#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>

#include <atomic>
#include <cstring>

#ifndef DECODIUM_HAS_RTLSDR
#define DECODIUM_HAS_RTLSDR 0
#endif

#if DECODIUM_HAS_RTLSDR
#include <rtl-sdr.h>
#endif

namespace {
#if DECODIUM_HAS_RTLSDR
QString rtlError(const QString& operation, int result)
{
    return QStringLiteral("RTL-SDR: %1 failed (%2)").arg(operation).arg(result);
}
#endif
}

class RtlSdrReader final : public QThread
{
    Q_OBJECT

public:
    explicit RtlSdrReader(const RtlSdrInput::Config& config, QObject *parent = nullptr)
        : QThread(parent), m_config(config)
    {
        setObjectName(QStringLiteral("RtlSdrReader"));
    }

    void requestStop()
    {
        m_stopRequested.store(true, std::memory_order_release);
#if DECODIUM_HAS_RTLSDR
        QMutexLocker locker(&m_deviceMutex);
        if (m_device) {
            rtlsdr_cancel_async(m_device);
        }
#endif
    }

    void requestRetune(quint32 centerFrequencyHz, int channelOffsetHz)
    {
        m_pendingChannelOffsetHz.store(channelOffsetHz, std::memory_order_relaxed);
        m_pendingCenterFrequencyHz.store(centerFrequencyHz, std::memory_order_relaxed);
        m_retuneRequestSerial.fetch_add(1, std::memory_order_release);
#if DECODIUM_HAS_RTLSDR
        // Interrupt read_async. The reader thread keeps the USB handle open,
        // applies the latest requested frequency, resets its DSP state and
        // resumes acquisition without blocking the GUI or re-enumerating USB.
        QMutexLocker locker(&m_deviceMutex);
        if (m_device) {
            rtlsdr_cancel_async(m_device);
        }
#endif
    }

signals:
    void readerStarted(QString deviceDescription);
    void readerStopped();
    void readerRetuned(quint32 centerFrequencyHz, int channelOffsetHz);
    void readerConfigurationAdjusted(RtlSdrInput::Config config, QString reason);
    void readerError(QString message);
    void readerStatus(QString message);
    void readerIq(QVector<short> samples, int sampleRate, quint32 centerFrequencyHz);
    void readerPcm(QVector<short> samples);
    void readerAudio(QVector<short> samples, int sampleRate);

protected:
    void run() override
    {
#if !DECODIUM_HAS_RTLSDR
        emit readerError(QStringLiteral("RTL-SDR support is not included in this build."));
        emit readerStopped();
        return;
#else
        if (!RtlSdrDsp::isSupportedSampleRateForDemodulator(m_config.sampleRate,
                                                             m_config.demodulator)) {
            const int outputRate = m_config.demodulator == RtlSdrDsp::Demodulator::WeakSignal
                ? RtlSdrDsp::kDecoderSampleRate : RtlSdrDsp::kReceiverAudioSampleRate;
            emit readerError(QStringLiteral("RTL-SDR: sample rate %1 Hz is not supported for %2 (requires an integer path to %3 Hz audio).")
                                 .arg(m_config.sampleRate)
                                 .arg(RtlSdrDsp::demodulatorName(m_config.demodulator))
                                 .arg(outputRate));
            emit readerStopped();
            return;
        }

        rtlsdr_dev_t *device = nullptr;
        const int openResult = rtlsdr_open(&device, static_cast<uint32_t>(m_config.deviceIndex));
        if (openResult != 0 || !device) {
            emit readerError(rtlError(QStringLiteral("opening device %1").arg(m_config.deviceIndex), openResult));
            emit readerStopped();
            return;
        }

        {
            QMutexLocker locker(&m_deviceMutex);
            m_device = device;
        }

        auto closeDevice = [this, &device]() {
            if (!device) return;
            rtlsdr_set_bias_tee(device, 0);
            {
                QMutexLocker locker(&m_deviceMutex);
                m_device = nullptr;
            }
            rtlsdr_close(device);
            device = nullptr;
        };
        auto configure = [this](const QString& operation, int result) -> bool {
            if (result == 0) return true;
            emit readerError(rtlError(operation, result));
            return false;
        };

        // A stop may be requested while the USB open call is in progress.
        // Do not enter the blocking async read in that case.
        if (m_stopRequested.load(std::memory_order_acquire)) {
            closeDevice();
            emit readerStopped();
            return;
        }

        char manufacturer[256] {};
        char product[256] {};
        char serial[256] {};
        const int usbResult = rtlsdr_get_usb_strings(device, manufacturer, product, serial);
        const QString deviceDescription = usbResult == 0
            ? QStringLiteral("%1 %2 (%3)")
                  .arg(QString::fromLocal8Bit(manufacturer),
                       QString::fromLocal8Bit(product),
                       QString::fromLocal8Bit(serial))
            : QStringLiteral("device %1").arg(m_config.deviceIndex);

        if (m_config.mode == RtlSdrInput::Mode::DirectSampling) {
            const qint64 selectedFrequency = static_cast<qint64>(m_config.centerFrequencyHz)
                + m_config.channelOffsetHz;
            const auto blockReason = decodium::rtl_sdr::directSamplingBlockReason(
                deviceDescription, selectedFrequency);
            QString adjustmentReason;
            if (blockReason
                == decodium::rtl_sdr::DirectSamplingBlockReason::BlogV4UsesUpconverter) {
                adjustmentReason = QStringLiteral(
                    "RTL-SDR Blog V4 has no usable direct-sampling antenna path; "
                    "using its tuner and automatic HF upconverter instead.");
            } else if (blockReason
                       == decodium::rtl_sdr::DirectSamplingBlockReason::OutsideHfRange) {
                adjustmentReason = QStringLiteral(
                    "RTL-SDR Direct Sampling is limited to 500 kHz–24 MHz; "
                    "using SDR Radio at %1 Hz instead.").arg(selectedFrequency);
            }
            if (!adjustmentReason.isEmpty()) {
                m_config.mode = RtlSdrInput::Mode::SdrRadio;
                emit readerConfigurationAdjusted(m_config, adjustmentReason);
            }
        }

        // Direct Sampling is a hardware selection, not an alternate software
        // path.  Q ADC is the conventional setting for RTL-SDR HF direct mode.
        const int directMode = m_config.mode == RtlSdrInput::Mode::DirectSampling ? 2 : 0;
        if (!configure(QStringLiteral("selecting %1 mode")
                           .arg(RtlSdrInput::modeName(m_config.mode)),
                       rtlsdr_set_direct_sampling(device, directMode))
            || !configure(QStringLiteral("setting sample rate"),
                          rtlsdr_set_sample_rate(device, static_cast<uint32_t>(m_config.sampleRate)))
            || !configure(QStringLiteral("setting frequency"),
                          rtlsdr_set_center_freq(device, m_config.centerFrequencyHz))) {
            closeDevice();
            emit readerStopped();
            return;
        }

        // These controls are tuner/driver dependent.  In particular, some
        // otherwise working V4 driver builds reject a zero PPM correction.
        // They must never prevent receive-only operation; surface the detail
        // in the live status instead.
        if (m_config.ppmCorrection != 0
            && rtlsdr_set_freq_correction(device, m_config.ppmCorrection) != 0) {
            emit readerStatus(QStringLiteral("RTL-SDR: PPM correction is not accepted by this driver; using 0 PPM."));
        }
        if (rtlsdr_set_agc_mode(device, m_config.digitalAgc ? 1 : 0) != 0) {
            emit readerStatus(QStringLiteral("RTL-SDR: digital AGC control unavailable; continuing with the driver default."));
        }
        if (rtlsdr_set_bias_tee(device, m_config.biasTee ? 1 : 0) != 0) {
            emit readerStatus(QStringLiteral("RTL-SDR: bias-tee control unavailable on this receiver; continuing without it."));
        }

        // Direct sampling bypasses the tuner completely, so tuner AGC and
        // tuner gain calls are both meaningless and can fail on some drivers.
        if (m_config.mode != RtlSdrInput::Mode::DirectSampling) {
            if (m_config.gainTenthsDb < 0) {
                if (!configure(QStringLiteral("enabling tuner AGC"), rtlsdr_set_tuner_gain_mode(device, 0))) {
                    closeDevice();
                    emit readerStopped();
                    return;
                }
            } else if (!configure(QStringLiteral("setting manual tuner gain"), rtlsdr_set_tuner_gain_mode(device, 1))
                       || !configure(QStringLiteral("setting tuner gain"),
                                     rtlsdr_set_tuner_gain(device, m_config.gainTenthsDb))) {
                closeDevice();
                emit readerStopped();
                return;
            }
        }

        if (!configure(QStringLiteral("resetting USB sample buffer"), rtlsdr_reset_buffer(device))) {
            closeDevice();
            emit readerStopped();
            return;
        }

        RtlSdrDsp dsp;
        if (!dsp.configure(m_config.sampleRate, m_config.demodulator, m_config.audioGain,
                           m_config.channelOffsetHz, m_config.spectrumInverted,
                           m_config.ssbVoiceBandwidthHz, m_config.ssbAgcMode,
                           m_config.ssbNotchFrequencyHz, m_config.ssbNoiseReduction)) {
            emit readerError(QStringLiteral("RTL-SDR: could not configure the %1 DSP path.")
                                 .arg(RtlSdrDsp::demodulatorName(m_config.demodulator)));
            closeDevice();
            emit readerStopped();
            return;
        }
        m_dsp = &dsp;
        // The raw USB callback is as fast as 100+ times/s at WFM rates.  A
        // panadapter only needs recent IQ around 20 times/s, so rate-limit the
        // cross-thread RF frames while keeping demodulation lossless locally.
        m_iqEmitEvery = qMax(1, m_config.sampleRate / (8192 * 20));
        m_iqEmitCounter = 0;
        emit readerStarted(deviceDescription);
        const qint64 selectedFrequency = static_cast<qint64>(m_config.centerFrequencyHz)
            + m_config.channelOffsetHz;
        emit readerStatus(QStringLiteral("RTL-SDR %1 / %2 active: input %3 Hz (%4 %5 Hz), %6 sps")
                              .arg(RtlSdrInput::modeName(m_config.mode))
                              .arg(RtlSdrInput::demodulatorName(m_config.demodulator))
                              .arg(selectedFrequency)
                              .arg(m_config.mode == RtlSdrInput::Mode::DirectSampling
                                       ? QStringLiteral("ADC centre")
                                       : QStringLiteral("tuner"))
                              .arg(m_config.centerFrequencyHz)
                              .arg(m_config.sampleRate));

        quint64 appliedRetuneSerial = 0;
        for (;;) {
            const int readResult = rtlsdr_read_async(device, &RtlSdrReader::readCallback,
                                                      this, 0, 16384);
            if (m_stopRequested.load(std::memory_order_acquire)) {
                break;
            }

            const quint64 requestedRetuneSerial =
                m_retuneRequestSerial.load(std::memory_order_acquire);
            if (requestedRetuneSerial != appliedRetuneSerial) {
                const quint32 requestedCenter =
                    m_pendingCenterFrequencyHz.load(std::memory_order_relaxed);
                const int requestedOffset =
                    m_pendingChannelOffsetHz.load(std::memory_order_relaxed);

                if (requestedCenter == 0
                    || rtlsdr_set_center_freq(device, requestedCenter) != 0) {
                    emit readerStatus(QStringLiteral(
                        "RTL-SDR retune to %1 Hz failed; keeping the previous frequency.")
                                          .arg(requestedCenter));
                    appliedRetuneSerial = requestedRetuneSerial;
                    continue;
                }
                if (!dsp.configure(m_config.sampleRate, m_config.demodulator,
                                   m_config.audioGain, requestedOffset,
                                   m_config.spectrumInverted,
                                   m_config.ssbVoiceBandwidthHz, m_config.ssbAgcMode,
                                   m_config.ssbNotchFrequencyHz,
                                   m_config.ssbNoiseReduction)) {
                    emit readerError(QStringLiteral(
                        "RTL-SDR: could not reset the DSP path after retuning."));
                    break;
                }
                const int resetResult = rtlsdr_reset_buffer(device);
                if (resetResult != 0) {
                    emit readerStatus(rtlError(QStringLiteral(
                        "resetting USB sample buffer after retune"), resetResult));
                }

                m_config.centerFrequencyHz = requestedCenter;
                m_config.channelOffsetHz = requestedOffset;
                m_iqEmitCounter = 0;
                m_audioEmitBuffer.clear();
                m_dspTelemetryStartMs = 0;
                m_dspProcessedSamples = 0;
                m_dspProcessNs = 0;
                appliedRetuneSerial = requestedRetuneSerial;

                const qint64 retunedRf = static_cast<qint64>(requestedCenter)
                    + requestedOffset;
                emit readerRetuned(requestedCenter, requestedOffset);
                emit readerStatus(QStringLiteral(
                    "RTL-SDR retuned asynchronously: input %1 Hz (%2 %3 Hz)")
                                      .arg(retunedRf)
                                      .arg(m_config.mode == RtlSdrInput::Mode::DirectSampling
                                               ? QStringLiteral("ADC centre")
                                               : QStringLiteral("tuner"))
                                      .arg(requestedCenter));
                continue;
            }

            if (readResult != 0) {
                emit readerError(rtlError(QStringLiteral("reading samples"), readResult));
            }
            break;
        }
        m_dsp = nullptr;
        closeDevice();
        emit readerStopped();
#endif
    }

private:
#if DECODIUM_HAS_RTLSDR
    static void readCallback(unsigned char *buffer, uint32_t length, void *context)
    {
        auto *self = static_cast<RtlSdrReader *>(context);
        if (!self || self->m_stopRequested.load(std::memory_order_acquire) || !self->m_dsp) {
            return;
        }
        const bool emitIq = (++self->m_iqEmitCounter % self->m_iqEmitEvery) == 0;
        QElapsedTimer processTimer;
        processTimer.start();
        RtlSdrDsp::Result frame = self->m_dsp->processFrame(buffer, static_cast<int>(length), emitIq);
        self->m_dspProcessNs += processTimer.nsecsElapsed();
        self->m_dspProcessedSamples += static_cast<qint64>(length / 2);
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (self->m_dspTelemetryStartMs <= 0) {
            self->m_dspTelemetryStartMs = nowMs;
        }
        if (nowMs - self->m_dspTelemetryStartMs >= 5000
            && self->m_dspProcessedSamples > 0) {
            const double inputDurationNs = 1.0e9 * self->m_dspProcessedSamples
                / static_cast<double>(self->m_config.sampleRate);
            const double loadPercent = inputDurationNs > 0.0
                ? 100.0 * self->m_dspProcessNs / inputDurationNs : 0.0;
            qInfo().noquote()
                << "[RTL-DSP]"
                << "demod=" << RtlSdrInput::demodulatorName(self->m_config.demodulator)
                << "rate=" << self->m_config.sampleRate
                << "samples=" << self->m_dspProcessedSamples
                << "load_pct=" << QString::number(loadPercent, 'f', 1)
                << "realtime=" << (loadPercent < 90.0 ? 1 : 0);
            self->m_dspTelemetryStartMs = nowMs;
            self->m_dspProcessedSamples = 0;
            self->m_dspProcessNs = 0;
        }
        if (self->m_stopRequested.load(std::memory_order_acquire)) {
            return;
        }
        if (!frame.iq.isEmpty()) {
            emit self->readerIq(std::move(frame.iq), self->m_config.sampleRate,
                                self->m_config.centerFrequencyHz);
        }
        if (!frame.decoderPcm.isEmpty()) {
            emit self->readerPcm(std::move(frame.decoderPcm));
        }
        if (!frame.audioPcm.isEmpty()) {
            self->m_audioEmitBuffer += frame.audioPcm;
            constexpr int kReceiverAudioEmitSamples =
                RtlSdrDsp::kReceiverAudioSampleRate / 20; // 50 ms / 20 Hz
            if (self->m_audioEmitBuffer.size() >= kReceiverAudioEmitSamples) {
                QVector<short> audio = std::move(self->m_audioEmitBuffer);
                self->m_audioEmitBuffer.clear();
                self->m_audioEmitBuffer.reserve(kReceiverAudioEmitSamples * 2);
                emit self->readerAudio(std::move(audio),
                                       RtlSdrDsp::kReceiverAudioSampleRate);
            }
        }
    }
#endif

    RtlSdrInput::Config m_config;
    std::atomic_bool m_stopRequested {false};
    std::atomic<quint32> m_pendingCenterFrequencyHz {0};
    std::atomic<int> m_pendingChannelOffsetHz {0};
    std::atomic<quint64> m_retuneRequestSerial {0};
    QMutex m_deviceMutex;
#if DECODIUM_HAS_RTLSDR
    rtlsdr_dev_t *m_device {nullptr};
#endif
    RtlSdrDsp *m_dsp {nullptr}; // only accessed by this QThread callback
    int m_iqEmitEvery {1};
    int m_iqEmitCounter {0};
    qint64 m_dspTelemetryStartMs {0};
    qint64 m_dspProcessedSamples {0};
    qint64 m_dspProcessNs {0};
    QVector<short> m_audioEmitBuffer;
};

RtlSdrInput::RtlSdrInput(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<RtlSdrInput::Config>("RtlSdrInput::Config");
}

RtlSdrInput::~RtlSdrInput()
{
    m_startPending = false;
    if (m_reader) {
        disconnect(m_reader, nullptr, this, nullptr);
        m_reader->requestStop();
        m_reader->wait(3000);
        delete m_reader;
        m_reader = nullptr;
    }
}

bool RtlSdrInput::compiledIn()
{
    return DECODIUM_HAS_RTLSDR != 0;
}

QString RtlSdrInput::modeName(Mode mode)
{
    return mode == Mode::DirectSampling
        ? QStringLiteral("Direct Sampling")
        : QStringLiteral("SDR Radio");
}

QString RtlSdrInput::demodulatorName(RtlSdrDsp::Demodulator demodulator)
{
    return RtlSdrDsp::demodulatorName(demodulator);
}

QStringList RtlSdrInput::enumerateDevices(QString *error)
{
    QStringList devices;
    if (error) error->clear();
#if !DECODIUM_HAS_RTLSDR
    if (error) *error = QStringLiteral("RTL-SDR support is not included in this build.");
    return devices;
#else
    const uint32_t count = rtlsdr_get_device_count();
    for (uint32_t index = 0; index < count; ++index) {
        char manufacturer[256] {};
        char product[256] {};
        char serial[256] {};
        const int result = rtlsdr_get_device_usb_strings(index, manufacturer, product, serial);
        QString description = QString::fromLocal8Bit(rtlsdr_get_device_name(index));
        if (result == 0) {
            const QString identity = QStringLiteral("%1 %2").arg(QString::fromLocal8Bit(manufacturer),
                                                                  QString::fromLocal8Bit(product)).trimmed();
            if (!identity.isEmpty()) description = identity;
            if (std::strlen(serial) > 0) description += QStringLiteral(" — %1").arg(QString::fromLocal8Bit(serial));
        }
        devices.append(QStringLiteral("%1: %2").arg(index).arg(description));
    }
    if (devices.isEmpty() && error) {
        *error = QStringLiteral("No RTL-SDR device detected.");
    }
    return devices;
#endif
}

bool RtlSdrInput::isRunning() const
{
    return m_running;
}

bool RtlSdrInput::isActive() const
{
    return m_reader != nullptr;
}

RtlSdrInput::Config RtlSdrInput::activeConfig() const
{
    return m_activeConfig;
}

bool RtlSdrInput::retune(quint32 centerFrequencyHz, int channelOffsetHz)
{
    if (!m_reader || !m_running || centerFrequencyHz == 0) {
        return false;
    }
    m_reader->requestRetune(centerFrequencyHz, channelOffsetHz);
    return true;
}

void RtlSdrInput::start(const Config& config)
{
    m_pendingConfig = config;
    m_startPending = true;
    if (!compiledIn()) {
        m_startPending = false;
        emit error(QStringLiteral("RTL-SDR support is not included in this build."));
        return;
    }
    if (m_reader) {
        m_reader->requestStop();
        return;
    }
    startPendingReader();
}

void RtlSdrInput::stop()
{
    m_startPending = false;
    if (m_reader) {
        m_reader->requestStop();
    }
}

void RtlSdrInput::startPendingReader()
{
    if (!m_startPending || m_reader) return;
    m_startPending = false;
    m_activeConfig = m_pendingConfig;
    m_reader = new RtlSdrReader(m_activeConfig, this);
    connect(m_reader, &RtlSdrReader::readerStarted,
            this, &RtlSdrInput::onReaderStarted, Qt::QueuedConnection);
    connect(m_reader, &RtlSdrReader::readerStopped,
            this, &RtlSdrInput::onReaderStopped, Qt::QueuedConnection);
    connect(m_reader, &RtlSdrReader::readerRetuned,
            this, &RtlSdrInput::onReaderRetuned, Qt::QueuedConnection);
    connect(m_reader, &RtlSdrReader::readerConfigurationAdjusted,
            this, &RtlSdrInput::onReaderConfigurationAdjusted, Qt::QueuedConnection);
    connect(m_reader, &RtlSdrReader::readerError,
            this, &RtlSdrInput::onReaderError, Qt::QueuedConnection);
    connect(m_reader, &RtlSdrReader::readerStatus,
            this, &RtlSdrInput::statusChanged, Qt::QueuedConnection);
    connect(m_reader, &RtlSdrReader::readerIq,
            this, &RtlSdrInput::iqSamplesReady, Qt::QueuedConnection);
    connect(m_reader, &RtlSdrReader::readerPcm,
            this, &RtlSdrInput::pcmSamplesProduced, Qt::DirectConnection);
    connect(m_reader, &RtlSdrReader::readerAudio,
            this, &RtlSdrInput::audioSamplesProduced, Qt::DirectConnection);
    connect(m_reader, &RtlSdrReader::readerPcm,
            this, &RtlSdrInput::pcmSamplesReady, Qt::QueuedConnection);
    connect(m_reader, &RtlSdrReader::readerAudio,
            this, &RtlSdrInput::audioSamplesReady, Qt::QueuedConnection);
    connect(m_reader, &QThread::finished, m_reader, &QObject::deleteLater);
    m_reader->start(QThread::HighPriority);
}

void RtlSdrInput::onReaderStarted(const QString& deviceDescription)
{
    if (!m_running) {
        m_running = true;
        emit runningChanged(true);
    }
    emit started(deviceDescription);
}

void RtlSdrInput::onReaderStopped()
{
    RtlSdrReader *reader = qobject_cast<RtlSdrReader *>(sender());
    if (reader && reader != m_reader) return;
    if (m_reader) {
        m_reader = nullptr;
    }
    if (m_running) {
        m_running = false;
        emit runningChanged(false);
    }
    emit stopped();
    if (m_startPending) {
        QMetaObject::invokeMethod(this, &RtlSdrInput::startPendingReader, Qt::QueuedConnection);
    }
}

void RtlSdrInput::onReaderRetuned(quint32 centerFrequencyHz, int channelOffsetHz)
{
    m_activeConfig.centerFrequencyHz = centerFrequencyHz;
    m_activeConfig.channelOffsetHz = channelOffsetHz;
    emit retuned(centerFrequencyHz, channelOffsetHz);
}

void RtlSdrInput::onReaderError(const QString& message)
{
    emit error(message);
}

void RtlSdrInput::onReaderConfigurationAdjusted(const RtlSdrInput::Config& config,
                                                const QString& reason)
{
    m_activeConfig = config;
    emit configurationAdjusted(config, reason);
    emit statusChanged(reason);
}

#include "RtlSdrInput.moc"
