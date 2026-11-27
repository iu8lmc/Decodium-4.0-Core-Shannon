#include "DecodiumBridge.h"
#include "DecodiumBridgeTraceScope.h"
#include <QDateTime>
#include <QDebug>
#include <QMediaDevices>

void decodiumBridgeAudioLevelLog(const QString&);
QString decodiumCacheDeviceId(const QAudioDevice&);
QString decodiumCacheAudioFormat(const QAudioFormat&);
#ifdef Q_OS_LINUX
QStringList decodiumCachePulseSources();
QStringList decodiumCachePulseSinks();
QStringList decodiumCacheAlsaCards();
bool decodiumCacheLooksUsb(const QString&);
QString decodiumCacheSourceName(const QString&);
QString decodiumCacheSourceDisplay(const QString&);
QString decodiumCacheSinkName(const QString&);
const char* decodiumCacheSinkPrefix();
#endif

static QStringList audioDeviceDescriptions(QList<QAudioDevice> const& devices)
{
    QStringList descriptions;
    descriptions.reserve(devices.size());
    for (QAudioDevice const& device : devices) {
        QString const description = device.description().trimmed();
        if (!description.isEmpty() && !descriptions.contains(description, Qt::CaseInsensitive)) {
            descriptions.append(description);
        }
    }
    return descriptions;
}

static QString audioDeviceCacheSignature(QList<QAudioDevice> const& inputs,
                                         QList<QAudioDevice> const& outputs,
                                         QAudioDevice const& defaultInput,
                                         QAudioDevice const& defaultOutput)
{
    QStringList tokens;
    tokens.reserve(inputs.size() + outputs.size() + 2);
    tokens << QStringLiteral("di=%1").arg(decodiumCacheDeviceId(defaultInput));
    tokens << QStringLiteral("do=%1").arg(decodiumCacheDeviceId(defaultOutput));
    for (QAudioDevice const& device : inputs) {
        tokens << QStringLiteral("i=%1|%2")
                      .arg(device.description().trimmed(), decodiumCacheDeviceId(device));
    }
    for (QAudioDevice const& device : outputs) {
        tokens << QStringLiteral("o=%1|%2")
                      .arg(device.description().trimmed(), decodiumCacheDeviceId(device));
    }
    return tokens.join(QLatin1Char('\n'));
}

void DecodiumBridge::refreshAudioDeviceCache(const QString& reason,
                                             bool verboseLog,
                                             bool emitSignals)
{
    MainThreadTraceScope trace(QStringLiteral("audio_device_cache_refresh"),
                               QStringLiteral("reason=%1 current_in=[%2] current_out=[%3] verbose=%4")
                                   .arg(reason, m_audioInputDevice, m_audioOutputDevice)
                                   .arg(verboseLog ? 1 : 0),
                               verboseLog ? 25 : 90);

    const QList<QAudioDevice> inputs = QMediaDevices::audioInputs();
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    const QAudioDevice defaultInput = QMediaDevices::defaultAudioInput();
    const QAudioDevice defaultOutput = QMediaDevices::defaultAudioOutput();
    QStringList inputNames = audioDeviceDescriptions(inputs);
    QStringList outputNames = audioDeviceDescriptions(outputs);
    QString const signature =
        audioDeviceCacheSignature(inputs, outputs, defaultInput, defaultOutput);

#ifdef Q_OS_LINUX
    bool qtExposesLikelyUsbInput = false;
    bool qtExposesLikelyUsbOutput = false;
#endif

    if (verboseLog) {
        decodiumBridgeAudioLevelLog(QStringLiteral("=== enumerateAudioDevices INPUT count=%1 default=[%2] id=%3 ===")
                      .arg(inputs.size())
                      .arg(defaultInput.description())
                      .arg(decodiumCacheDeviceId(defaultInput)));
        qInfo().noquote() << QStringLiteral("[AUDIODEV] Qt inputs count=%1 default=[%2] id=%3")
                                 .arg(inputs.size())
                                 .arg(defaultInput.description())
                                 .arg(decodiumCacheDeviceId(defaultInput));
    }

    for (const QAudioDevice& dev : inputs) {
        const QString description = dev.description().trimmed();
#ifdef Q_OS_LINUX
        qtExposesLikelyUsbInput = qtExposesLikelyUsbInput || decodiumCacheLooksUsb(description);
#endif
        if (verboseLog) {
            decodiumBridgeAudioLevelLog(QStringLiteral("  IN: [%1] id=%2 preferred={%3}")
                          .arg(description)
                          .arg(decodiumCacheDeviceId(dev))
                          .arg(decodiumCacheAudioFormat(dev.preferredFormat())));
            qInfo().noquote() << QStringLiteral("[AUDIODEV] IN [%1] id=%2 preferred={%3}")
                                     .arg(description)
                                     .arg(decodiumCacheDeviceId(dev))
                                     .arg(decodiumCacheAudioFormat(dev.preferredFormat()));
        }
    }

    if (verboseLog) {
        decodiumBridgeAudioLevelLog(QStringLiteral("=== enumerateAudioDevices OUTPUT count=%1 default=[%2] id=%3 ===")
                      .arg(outputs.size())
                      .arg(defaultOutput.description())
                      .arg(decodiumCacheDeviceId(defaultOutput)));
        qInfo().noquote() << QStringLiteral("[AUDIODEV] Qt outputs count=%1 default=[%2] id=%3")
                                 .arg(outputs.size())
                                 .arg(defaultOutput.description())
                                 .arg(decodiumCacheDeviceId(defaultOutput));
    }

    for (const QAudioDevice& dev : outputs) {
        const QString description = dev.description().trimmed();
#ifdef Q_OS_LINUX
        qtExposesLikelyUsbOutput = qtExposesLikelyUsbOutput || decodiumCacheLooksUsb(description);
#endif
        if (verboseLog) {
            decodiumBridgeAudioLevelLog(QStringLiteral("  OUT: [%1] id=%2 preferred={%3}")
                          .arg(description)
                          .arg(decodiumCacheDeviceId(dev))
                          .arg(decodiumCacheAudioFormat(dev.preferredFormat())));
            qInfo().noquote() << QStringLiteral("[AUDIODEV] OUT [%1] id=%2 preferred={%3}")
                                     .arg(description)
                                     .arg(decodiumCacheDeviceId(dev))
                                     .arg(decodiumCacheAudioFormat(dev.preferredFormat()));
        }
    }

#ifdef Q_OS_LINUX
    // QMediaDevices may expose only the ALSA-facing names on Linux Mint/
    // PipeWire systems.  MSHV and other PulseAudio clients also show the
    // logical Pulse/PipeWire sources and sinks, so enumerate those for every
    // normal refresh, not only while verbose diagnostics are enabled.
    const QStringList pulseSources = decodiumCachePulseSources();
    const QStringList pulseSinks = decodiumCachePulseSinks();

    if (verboseLog) {
        const QStringList alsaCards = decodiumCacheAlsaCards();
        if (!alsaCards.isEmpty()) {
            decodiumBridgeAudioLevelLog(QStringLiteral("=== Linux ALSA cards visible to process (%1 lines) ===")
                          .arg(alsaCards.size()));
            for (const QString& line : alsaCards) {
                decodiumBridgeAudioLevelLog(QStringLiteral("  ALSA: %1").arg(line));
            }
            if (!qtExposesLikelyUsbInput) {
                decodiumBridgeAudioLevelLog(QStringLiteral("Audio Linux warning: ALSA sees devices but Qt Multimedia did not expose a USB/radio-like input. "
                                         "Check PipeWire/PulseAudio input profiles, application permissions, and AppImage Qt audio plugins."));
            }
        } else {
            decodiumBridgeAudioLevelLog(QStringLiteral("Linux ALSA cards: /proc/asound/cards unavailable or empty"));
        }
        decodiumBridgeAudioLevelLog(QStringLiteral("=== Linux PulseAudio/PipeWire sources via pactl (%1 lines) ===")
                      .arg(pulseSources.size()));
        for (const QString& line : pulseSources) {
            decodiumBridgeAudioLevelLog(QStringLiteral("  PULSE: %1").arg(line));
            qInfo().noquote() << QStringLiteral("[AUDIODEV] PULSE SOURCE %1").arg(line);
        }
        decodiumBridgeAudioLevelLog(QStringLiteral("=== Linux PulseAudio/PipeWire sinks via pactl (%1 lines) ===")
                      .arg(pulseSinks.size()));
        for (const QString& line : pulseSinks) {
            decodiumBridgeAudioLevelLog(QStringLiteral("  PULSE OUT: %1").arg(line));
            qInfo().noquote() << QStringLiteral("[AUDIODEV] PULSE SINK %1").arg(line);
        }
    }

    // Keep the display names separate from QAudioDevice objects: the bridge
    // resolves these entries later through pactl and switches the current
    // Pulse/PipeWire default source/sink before opening Qt's stream.
    for (const QString& line : pulseSources) {
        const QString pulseSourceName = decodiumCacheSourceName(line);
        if (!pulseSourceName.isEmpty()) {
            const QString displayName = decodiumCacheSourceDisplay(pulseSourceName);
            if (!inputNames.contains(displayName, Qt::CaseInsensitive)) {
                inputNames.append(displayName);
            }
        }
    }
    for (const QString& line : pulseSinks) {
        const QString pulseSinkName = decodiumCacheSinkName(line);
        if (!pulseSinkName.isEmpty()) {
            const QString displayName =
                QString::fromLatin1(decodiumCacheSinkPrefix()) + pulseSinkName;
            if (!outputNames.contains(displayName, Qt::CaseInsensitive)) {
                outputNames.append(displayName);
            }
        }
    }

    if (verboseLog) {
        if (!qtExposesLikelyUsbOutput) {
            for (const QString& line : pulseSinks) {
                if (line.contains(QStringLiteral("usb-"), Qt::CaseInsensitive)) {
                    decodiumBridgeAudioLevelLog(QStringLiteral("Audio Linux note: Qt Multimedia did not expose a USB/radio-like output directly; added Pulse/PipeWire sinks as selectable TX outputs."));
                    break;
                }
            }
        }
    }
#endif

    bool const signatureChanged =
        !m_audioDeviceCacheValid || signature != m_audioDeviceCacheSignature;
    bool const inputListChanged = inputNames != m_audioInputDevices;
    bool const outputListChanged = outputNames != m_audioOutputDevices;
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();

    m_cachedAudioInputs = inputs;
    m_cachedAudioOutputs = outputs;
    m_cachedDefaultAudioInput = defaultInput;
    m_cachedDefaultAudioOutput = defaultOutput;
    m_audioInputDevices = inputNames;
    m_audioOutputDevices = outputNames;
    m_audioDeviceCacheSignature = signature;
    m_audioDeviceCacheRefreshMs = nowMs;
    m_audioDeviceCacheDirty = false;
    m_audioDeviceCacheValid = true;
    if (signatureChanged) {
        m_cachedTxOutputDeviceValid = false;
    }

    if (verboseLog) {
        decodiumBridgeAudioLevelLog("=== enumerateAudioDevices END ===");
    }
    if (verboseLog || signatureChanged || nowMs - m_lastAudioDeviceCacheLogMs > 60000) {
        m_lastAudioDeviceCacheLogMs = nowMs;
        qInfo().noquote() << QStringLiteral("[AUDIODEV] cache reason=%1 changed=%2 inputs=%3 outputs=%4 default_in=[%5] default_out=[%6]")
                                 .arg(reason)
                                 .arg(signatureChanged ? 1 : 0)
                                 .arg(inputNames.size())
                                 .arg(outputNames.size())
                                 .arg(defaultInput.description())
                                 .arg(defaultOutput.description());
    }

    trace.addDetail(QStringLiteral("inputs=%1 outputs=%2 changed=%3")
                        .arg(inputs.size())
                        .arg(outputs.size())
                        .arg(signatureChanged ? 1 : 0));

    if (m_diagnostics) {
        m_diagnostics->setAudioState(
            QStringLiteral("Input: %1 · Output: %2 · devices=%3/%4 · refreshed=%5")
                .arg(defaultInput.description().isEmpty()
                         ? QStringLiteral("<none>") : defaultInput.description())
                .arg(defaultOutput.description().isEmpty()
                         ? QStringLiteral("<none>") : defaultOutput.description())
                .arg(inputNames.size())
                .arg(outputNames.size())
                .arg(reason));
    }

    if (emitSignals && inputListChanged) {
        emit audioInputDevicesChanged();
    }
    if (emitSignals && outputListChanged) {
        emit audioOutputDevicesChanged();
    }
}

