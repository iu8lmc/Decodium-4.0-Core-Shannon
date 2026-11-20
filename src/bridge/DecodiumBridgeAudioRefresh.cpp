#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include <QAudioDevice>
#include <QDateTime>
#include <QDebug>
#include <limits>

bool decodiumBridgeAudioDeviceSavedIdentityMatches(const QString&, const QString&, const QAudioDevice&);
bool decodiumBridgeResolveAudioDeviceByIdentity(const QList<QAudioDevice>&, const QString&, const QString&, QAudioDevice*, QString*);
bool decodiumBridgeHasAudioDeviceNamed(const QList<QAudioDevice>&, const QString&);
QString decodiumBridgeAudioDeviceIdSettingForLog(QString);
QString decodiumBridgeLinuxPulseSourceNameFromDisplay(QString);
QString decodiumBridgeLinuxPulseSinkNameFromDisplay(QString);
void decodiumBridgeAudioLevelLog(const QString&);
#define audioDeviceSavedIdentityMatches decodiumBridgeAudioDeviceSavedIdentityMatches
#define tryResolveAudioDeviceByIdentity decodiumBridgeResolveAudioDeviceByIdentity
#define hasAudioDeviceNamed decodiumBridgeHasAudioDeviceNamed
#define audioDeviceIdSettingForLog decodiumBridgeAudioDeviceIdSettingForLog
#define linuxPulseSourceNameFromDisplay decodiumBridgeLinuxPulseSourceNameFromDisplay
#define linuxPulseSinkNameFromDisplay decodiumBridgeLinuxPulseSinkNameFromDisplay
#define bridgeLog decodiumBridgeAudioLevelLog

void DecodiumBridge::refreshAudioDevicesInternal(const QString& reason,
                                                 bool forceEnumeration,
                                                 bool emitStatus)
{
    m_cachedTxOutputDeviceValid = false;
    invalidateTxAudioCache();
    QString const refreshReason = reason.trimmed().isEmpty()
        ? QStringLiteral("refreshAudioDevices")
        : reason.trimmed();

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 const cacheAgeMs = m_audioDeviceCacheRefreshMs > 0
        ? nowMs - m_audioDeviceCacheRefreshMs
        : std::numeric_limits<qint64>::max();
    bool const useFreshCache =
        !forceEnumeration
        && m_audioDeviceCacheValid
        && !m_audioDeviceCacheDirty
        && cacheAgeMs >= 0
        && cacheAgeMs < 5000;
    if (useFreshCache) {
        qInfo().noquote() << QStringLiteral("[AUDIODEV] %1 using fresh cache age_ms=%2")
                                  .arg(refreshReason)
                                  .arg(cacheAgeMs);
    } else {
        refreshAudioDeviceCache(refreshReason, false, true);
    }

    auto preferredDeviceName = [](QAudioDevice const& preferred,
                                  QList<QAudioDevice> const& devices) -> QString {
        QString const preferredName = preferred.description().trimmed();
        if (!preferredName.isEmpty() && hasAudioDeviceNamed(devices, preferredName)) {
            return preferredName;
        }
        return devices.isEmpty() ? QString() : devices.constFirst().description().trimmed();
    };

    bool const tciAudio = usingTciAudioInput();
    QList<QAudioDevice> const inputs = m_cachedAudioInputs;
#ifdef Q_OS_LINUX
    const bool linuxPulseInputSelected =
        !linuxPulseSourceNameFromDisplay(m_audioInputDevice).isEmpty();
#else
    const bool linuxPulseInputSelected = false;
#endif
    if (tciAudio) {
        applyAudioInputRuntimeChange(QStringLiteral("refresh: TCI audio selected"));
    } else if (linuxPulseInputSelected) {
        applyAudioInputRuntimeChange(QStringLiteral("refresh: Linux Pulse/PipeWire source selected"));
    } else if (!inputs.isEmpty()) {
        QAudioDevice matchedInput;
        QString inputMatchReason;
        bool const inputResolved =
            tryResolveAudioDeviceByIdentity(inputs,
                                            m_audioInputDevice,
                                            m_audioInputDeviceId,
                                            &matchedInput,
                                            &inputMatchReason);
        if (inputResolved) {
            QString const previousInputDevice = m_audioInputDevice;
            QString const previousInputDeviceId = m_audioInputDeviceId;
            bool const rxAlreadyBound =
                m_soundInput
                && audioDeviceSavedIdentityMatches(m_activeRxInputDeviceName,
                                                   m_activeRxInputDeviceId,
                                                   matchedInput);
            rememberAudioInputDeviceIdentity(matchedInput,
                                             QStringLiteral("%1 %2")
                                                 .arg(refreshReason, inputMatchReason),
                                             !m_audioInputDevice.trimmed().isEmpty());
            if (legacyBackendAvailable()) {
                m_legacyBackend->setAudioInputDeviceName(m_audioInputDevice);
                m_legacyBackend->setAudioInputChannel(qBound(0, m_audioInputChannel, 3));
            }
            bool const savedIdentityChanged =
                !audioDeviceSavedIdentityMatches(previousInputDevice,
                                                 previousInputDeviceId,
                                                 matchedInput)
                || !audioDeviceSavedIdentityMatches(m_audioInputDevice,
                                                    m_audioInputDeviceId,
                                                    matchedInput);
            if (savedIdentityChanged || !rxAlreadyBound) {
                applyAudioInputRuntimeChange(QStringLiteral("refresh: rebind selected input"));
            } else {
                bridgeLog(QStringLiteral("refreshAudioDevices: RX input unchanged; keeping active capture [%1]")
                              .arg(matchedInput.description().trimmed()));
            }
        } else if (m_audioInputDevice.trimmed().isEmpty()) {
            QString const fallbackInput =
                preferredDeviceName(m_cachedDefaultAudioInput, inputs);
            if (!fallbackInput.isEmpty()) {
                setAudioInputDevice(fallbackInput);
            }
        } else {
            bridgeLog(QStringLiteral("refreshAudioDevices: keeping unresolved saved input=[%1] id=%2 reason=%3; no silent default rewrite")
                          .arg(m_audioInputDevice,
                               audioDeviceIdSettingForLog(m_audioInputDeviceId),
                               inputMatchReason));
            if (emitStatus) {
                emit statusMessage(QStringLiteral("Audio input salvato non ritrovato o non univoco: %1")
                                       .arg(m_audioInputDevice));
            }
        }
    }

    QList<QAudioDevice> const outputs = m_cachedAudioOutputs;
#ifdef Q_OS_LINUX
    const bool linuxPulseOutputSelected =
        !linuxPulseSinkNameFromDisplay(m_audioOutputDevice).isEmpty();
#else
    const bool linuxPulseOutputSelected = false;
#endif
    if (tciAudio) {
        applyAudioOutputRuntimeChange(QStringLiteral("refresh: TCI audio selected"));
    } else if (linuxPulseOutputSelected) {
        applyAudioOutputRuntimeChange(QStringLiteral("refresh: Linux Pulse/PipeWire sink selected"));
    } else if (!outputs.isEmpty()) {
        QAudioDevice matchedOutput;
        QString outputMatchReason;
        bool const outputResolved =
            tryResolveAudioDeviceByIdentity(outputs,
                                            m_audioOutputDevice,
                                            m_audioOutputDeviceId,
                                            &matchedOutput,
                                            &outputMatchReason);
        if (outputResolved) {
            QString const previousOutputDevice = m_audioOutputDevice;
            QString const previousOutputDeviceId = m_audioOutputDeviceId;
            rememberAudioOutputDeviceIdentity(matchedOutput,
                                              QStringLiteral("%1 %2")
                                                  .arg(refreshReason, outputMatchReason),
                                              !m_audioOutputDevice.trimmed().isEmpty());
            if (legacyBackendAvailable()) {
                m_legacyBackend->setAudioOutputDeviceName(m_audioOutputDevice);
                m_legacyBackend->setAudioOutputChannel(qBound(0, m_audioOutputChannel, 3));
            }
            bool const savedIdentityChanged =
                !audioDeviceSavedIdentityMatches(previousOutputDevice,
                                                 previousOutputDeviceId,
                                                 matchedOutput)
                || !audioDeviceSavedIdentityMatches(m_audioOutputDevice,
                                                    m_audioOutputDeviceId,
                                                    matchedOutput);
            if (savedIdentityChanged) {
                applyAudioOutputRuntimeChange(QStringLiteral("refresh: rebind selected output"));
            } else {
                bridgeLog(QStringLiteral("refreshAudioDevices: TX output unchanged; keeping cached output [%1]")
                              .arg(matchedOutput.description().trimmed()));
            }
        } else if (m_audioOutputDevice.trimmed().isEmpty()) {
            QString const fallbackOutput =
                preferredDeviceName(m_cachedDefaultAudioOutput, outputs);
            if (!fallbackOutput.isEmpty()) {
                setAudioOutputDevice(fallbackOutput);
            }
        } else {
            bridgeLog(QStringLiteral("refreshAudioDevices: keeping unresolved saved output=[%1] id=%2 reason=%3; no silent default rewrite")
                          .arg(m_audioOutputDevice,
                               audioDeviceIdSettingForLog(m_audioOutputDeviceId),
                               outputMatchReason));
            if (emitStatus) {
                emit statusMessage(QStringLiteral("Audio output salvato non ritrovato o non univoco: %1")
                                       .arg(m_audioOutputDevice));
            }
        }
    }

    if (emitStatus) {
        emit statusMessage(QStringLiteral("Dispositivi audio aggiornati: %1 input, %2 output")
                               .arg(m_audioInputDevices.size())
                               .arg(m_audioOutputDevices.size()));
    } else {
        bridgeLog(QStringLiteral("refreshAudioDevices: %1 completed inputs=%2 outputs=%3")
                      .arg(refreshReason)
                      .arg(m_audioInputDevices.size())
                      .arg(m_audioOutputDevices.size()));
    }
}

#undef audioDeviceSavedIdentityMatches
#undef tryResolveAudioDeviceByIdentity
#undef hasAudioDeviceNamed
#undef audioDeviceIdSettingForLog
#undef linuxPulseSourceNameFromDisplay
#undef linuxPulseSinkNameFromDisplay
#undef bridgeLog
