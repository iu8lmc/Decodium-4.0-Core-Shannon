#include "DecodiumBridge.h"
#include "src/rtl/RtlSdrInput.h"
#include <QDateTime>
#include <QTimer>

void decodiumBridgeRtlInitLog(const QString&);
#define bridgeLog decodiumBridgeRtlInitLog

void DecodiumBridge::initialiseRtlSdr()
{
    m_rtlSdrInput = new RtlSdrInput(this);
    m_rtlSdrRetuneTimer = new QTimer(this);
    m_rtlSdrRetuneTimer->setSingleShot(true);
    connect(m_rtlSdrRetuneTimer, &QTimer::timeout, this, [this]() {
        if (!m_monitoring
            || m_transmitting
            || m_tuning
            || !rtlSdrEnabled()
            || !getSetting(QStringLiteral("RtlSdrFollowDial"), true).toBool()) {
            return;
        }
        bridgeLog(QStringLiteral("RTL-SDR applying debounced dial retune: %1 Hz")
                      .arg(qRound64(m_frequency)));
        startRtlSdrCapture();
    });
    connect(m_rtlSdrInput, &RtlSdrInput::statusChanged, this, [this](const QString& message) {
        if (m_rtlSdrStatus != message) {
            m_rtlSdrStatus = message;
            emit rtlSdrStatusChanged();
        }
        emit statusMessage(message);
    });
    connect(m_rtlSdrInput, &RtlSdrInput::configurationAdjusted, this,
            [this](const RtlSdrInput::Config& config, const QString& reason) {
        if (config.mode == RtlSdrInput::Mode::SdrRadio
            && getSetting(QStringLiteral("RtlSdrMode"), QStringLiteral("sdr"))
                   .toString().compare(QStringLiteral("direct"), Qt::CaseInsensitive) == 0) {
            setSetting(QStringLiteral("RtlSdrMode"), QStringLiteral("sdr"));
        }
        bridgeLog(QStringLiteral("RTL-SDR configuration adjusted: %1").arg(reason));
    });
    connect(m_rtlSdrInput, &RtlSdrInput::error, this, [this](const QString& message) {
        m_rtlSdrStatus = message;
        emit rtlSdrStatusChanged();
        emit errorMessage(message);
    });
    connect(m_rtlSdrInput, &RtlSdrInput::started, this, [this](const QString& device) {
        const RtlSdrInput::Config active = m_rtlSdrInput->activeConfig();
        const bool rfViewWasVisible = rtlSdrRfView();
        m_rtlSdrRfSampleRate = active.sampleRate;
        emit rtlSdrRfSampleRateChanged();
        emit rtlSdrRfCenterFrequencyChanged();
        emit rtlSdrRfSelectedFrequencyChanged();
        if (!rfViewWasVisible) {
            emit rtlSdrRfViewChanged();
        }
        m_activeRxInputDeviceName = QStringLiteral("RTL-SDR: %1").arg(device);
        m_activeRxInputDeviceId = QString::number(getSetting(QStringLiteral("RtlSdrDeviceIndex"), 0).toInt());
        emit sstvRxSourceDeviceChanged();
        m_rxAudioStartupStartMs = QDateTime::currentMSecsSinceEpoch();
        m_pendingRxAudioStartupHealthLog = true;
        m_audioWatchdogIgnoreUntilMs = m_rxAudioStartupStartMs + 5000;
        const QString message = QStringLiteral("RTL-SDR RX active: %1 (%2)")
                                    .arg(device, RtlSdrInput::demodulatorName(active.demodulator));
        if (m_rtlSdrStatus != message) {
            m_rtlSdrStatus = message;
            emit rtlSdrStatusChanged();
        }
        emit statusMessage(message);
        if (getSetting(QStringLiteral("RtlSdrFollowDial"), true).toBool()) {
            const qint64 desiredRf = qRound64(m_frequency);
            if (qAbs(desiredRf - static_cast<qint64>(m_rtlSdrRfSelectedFrequencyHz)) > 1) {
                bridgeLog(QStringLiteral(
                    "RTL-SDR startup frequency guard: dial=%1 active_rf=%2; scheduling correction")
                              .arg(desiredRf)
                              .arg(m_rtlSdrRfSelectedFrequencyHz));
                scheduleRtlSdrRetune();
            }
        }
    });
    connect(m_rtlSdrInput, &RtlSdrInput::retuned, this,
            [this](quint32 centerFrequencyHz, int channelOffsetHz) {
        const bool rfViewWasVisible = rtlSdrRfView();
        m_rtlSdrSpectrumSerial.fetch_add(1);
        m_rtlSdrIqCount = 0;
        m_rtlSdrIqWritePos = 0;
        m_lastPanadapterData.clear();
        emit rtlSdrRfCenterFrequencyChanged();
        emit rtlSdrRfSelectedFrequencyChanged();
        if (!rfViewWasVisible) {
            emit rtlSdrRfViewChanged();
        }
        bridgeLog(QStringLiteral("RTL-SDR in-place retune complete: dial=%1 input=%2 tuner=%3 Hz")
                      .arg(m_rtlSdrRfSelectedFrequencyHz)
                      .arg(static_cast<qint64>(centerFrequencyHz) + channelOffsetHz)
                      .arg(centerFrequencyHz));

        if (getSetting(QStringLiteral("RtlSdrFollowDial"), true).toBool()) {
            const qint64 desiredRf = qRound64(m_frequency);
            if (qAbs(desiredRf - static_cast<qint64>(m_rtlSdrRfSelectedFrequencyHz)) > 1) {
                bridgeLog(QStringLiteral(
                    "RTL-SDR retune frequency guard: dial=%1 active_rf=%2; coalescing latest dial")
                              .arg(desiredRf)
                              .arg(m_rtlSdrRfSelectedFrequencyHz));
                scheduleRtlSdrRetune();
            }
        }
    });
    connect(m_rtlSdrInput, &RtlSdrInput::runningChanged, this, [this](bool) {
        emit rtlSdrRunningChanged();
        emit rtlSdrRfViewChanged();
    });
    connect(m_rtlSdrInput, &RtlSdrInput::stopped, this, [this]() {
        // Do not hide an error that was emitted immediately before the reader
        // stopped.  Normal manual stops and configuration changes get clear
        // non-modal feedback in the Audio settings section.
        if (!m_rtlSdrStatus.contains(QStringLiteral(" failed "), Qt::CaseInsensitive)) {
            const QString message = QStringLiteral("RTL-SDR receiver stopped.");
            if (m_rtlSdrStatus != message) {
                m_rtlSdrStatus = message;
                emit rtlSdrStatusChanged();
            }
            emit statusMessage(message);
        }
    });
    connect(m_rtlSdrInput, &RtlSdrInput::iqSamplesReady,
            this, &DecodiumBridge::onRtlSdrIqSamplesReady);
    connect(m_rtlSdrInput, &RtlSdrInput::pcmSamplesReady,
            this, &DecodiumBridge::onRtlSdrPcmSamplesReady);
    connect(m_rtlSdrInput, &RtlSdrInput::audioSamplesReady,
            this, &DecodiumBridge::onRtlSdrAudioSamplesReady);
    refreshRtlSdrDevices();
}
