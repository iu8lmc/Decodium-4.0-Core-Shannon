#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include "DecodiumLegacyBackend.h"
#include <QAudioDevice>
#include <QDebug>
#include <QSettings>

void decodiumBridgeDecodeFlowLog(const QString&);
bool decodiumBridgeResolveAudioDeviceByIdentity(const QList<QAudioDevice>&, const QString&, const QString&, QAudioDevice*, QString*);
QString decodiumBridgeAudioDeviceIdForSettings(const QAudioDevice&);
QString decodiumBridgeAudioDeviceIdSettingForLog(QString);
QString decodiumBridgeAudioDeviceSelectionForLog(const QString&, const QString&);
#ifdef Q_OS_LINUX
QString decodiumBridgeLinuxPulseSourceNameFromDisplay(QString);
bool decodiumBridgeLinuxSetDefaultPulseSource(const QString&, QString*);
QString decodiumBridgeLinuxPulseSinkNameFromDisplay(QString);
bool decodiumBridgeLinuxSetDefaultPulseSink(const QString&, QString*);
#endif

void DecodiumBridge::setAudioInputDevice(const QString& v) {
    QString const value = v.trimmed();
    if (m_audioInputDevice != value) {
        QString const previous = m_audioInputDevice;
        QString const previousId = m_audioInputDeviceId;
        m_audioInputDevice = value;
        QAudioDevice matchedDevice;
        QString matchReason;
        if (decodiumBridgeResolveAudioDeviceByIdentity(cachedAudioInputs(QStringLiteral("setAudioInputDevice"), false),
                                            value,
                                            QString(),
                                            &matchedDevice,
                                            &matchReason)) {
            m_audioInputDeviceId = decodiumBridgeAudioDeviceIdForSettings(matchedDevice);
        } else {
            m_audioInputDeviceId.clear();
        }
        decodiumBridgeDecodeFlowLog(QStringLiteral("audioInputDevice set to: %1 id=%2 match=%3 previous_id=%4")
                      .arg(m_audioInputDevice,
                           decodiumBridgeAudioDeviceIdSettingForLog(m_audioInputDeviceId),
                           matchReason,
                           decodiumBridgeAudioDeviceIdSettingForLog(previousId)));
#ifdef Q_OS_LINUX
        const QString pulseSourceName = decodiumBridgeLinuxPulseSourceNameFromDisplay(m_audioInputDevice);
        if (!pulseSourceName.isEmpty()) {
            QString pulseError;
            if (decodiumBridgeLinuxSetDefaultPulseSource(pulseSourceName, &pulseError)) {
                decodiumBridgeDecodeFlowLog(QStringLiteral("audioInputDevice: selected Linux Pulse/PipeWire source [%1]")
                              .arg(pulseSourceName));
                qInfo().noquote() << "[AUDIODEV] selected Pulse/PipeWire source:" << pulseSourceName;
            } else {
                decodiumBridgeDecodeFlowLog(QStringLiteral("audioInputDevice: failed to select Linux Pulse/PipeWire source [%1]: %2")
                              .arg(pulseSourceName, pulseError));
                qWarning().noquote() << "[AUDIODEV] failed to select Pulse/PipeWire source:"
                                     << pulseSourceName << pulseError;
            }
        }
#endif
        syncAudioDeviceSettingsToLegacyIni();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setAudioInputDeviceName(value);
        }
        QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(s);
        s.setValue(QStringLiteral("audioInputDevice"), m_audioInputDevice);
        s.setValue(QStringLiteral("SoundInName"), m_audioInputDevice);
        if (m_audioInputDeviceId.isEmpty()) {
            s.remove(QStringLiteral("audioInputDeviceId"));
        } else {
            s.setValue(QStringLiteral("audioInputDeviceId"), m_audioInputDeviceId);
        }
        s.sync();
        emit audioInputDeviceChanged();
        emit sstvRxSourceDeviceChanged();
        applyAudioInputRuntimeChange(QStringLiteral("device %1 -> %2")
                                         .arg(decodiumBridgeAudioDeviceSelectionForLog(previous, previousId),
                                              decodiumBridgeAudioDeviceSelectionForLog(m_audioInputDevice, m_audioInputDeviceId)));
    }
}
QString DecodiumBridge::audioOutputDevice() const { return m_audioOutputDevice; }
void DecodiumBridge::setAudioOutputDevice(const QString& v) {
    QString const value = v.trimmed();
    if (m_audioOutputDevice == value) {
        QAudioDevice matchedDevice;
        QString matchReason;
        if (decodiumBridgeResolveAudioDeviceByIdentity(cachedAudioOutputs(QStringLiteral("setAudioOutputDevice same-value"), false),
                                            value,
                                            m_audioOutputDeviceId,
                                            &matchedDevice,
                                            &matchReason)) {
            QString const matchedId = decodiumBridgeAudioDeviceIdForSettings(matchedDevice);
            if (!matchedId.isEmpty()
                && matchedId.compare(m_audioOutputDeviceId, Qt::CaseInsensitive) != 0) {
                rememberAudioOutputDeviceIdentity(matchedDevice,
                                                  QStringLiteral("setAudioOutputDevice same-value %1").arg(matchReason),
                                                  true);
            }
        }
        m_cachedTxOutputDeviceValid = false;
        invalidateTxAudioCache();
        return;
    }
    if (m_audioOutputDevice != value) {
        QString const previous = m_audioOutputDevice;
        QString const previousId = m_audioOutputDeviceId;
        m_audioOutputDevice = value;
        QAudioDevice matchedDevice;
        QString matchReason;
        if (decodiumBridgeResolveAudioDeviceByIdentity(cachedAudioOutputs(QStringLiteral("setAudioOutputDevice"), false),
                                            value,
                                            QString(),
                                            &matchedDevice,
                                            &matchReason)) {
            m_audioOutputDeviceId = decodiumBridgeAudioDeviceIdForSettings(matchedDevice);
        } else {
            m_audioOutputDeviceId.clear();
        }
        decodiumBridgeDecodeFlowLog(QStringLiteral("audioOutputDevice set to: %1 id=%2 match=%3 previous_id=%4")
                      .arg(m_audioOutputDevice,
                           decodiumBridgeAudioDeviceIdSettingForLog(m_audioOutputDeviceId),
                           matchReason,
                           decodiumBridgeAudioDeviceIdSettingForLog(previousId)));
#ifdef Q_OS_LINUX
        const QString pulseSinkName = decodiumBridgeLinuxPulseSinkNameFromDisplay(m_audioOutputDevice);
        if (!pulseSinkName.isEmpty()) {
            QString pulseError;
            if (decodiumBridgeLinuxSetDefaultPulseSink(pulseSinkName, &pulseError)) {
                decodiumBridgeDecodeFlowLog(QStringLiteral("audioOutputDevice: selected Linux Pulse/PipeWire sink [%1]")
                              .arg(pulseSinkName));
                qInfo().noquote() << "[AUDIODEV] selected Pulse/PipeWire sink:" << pulseSinkName;
            } else {
                decodiumBridgeDecodeFlowLog(QStringLiteral("audioOutputDevice: failed to select Linux Pulse/PipeWire sink [%1]: %2")
                              .arg(pulseSinkName, pulseError));
                qWarning().noquote() << "[AUDIODEV] failed to select Pulse/PipeWire sink:"
                                     << pulseSinkName << pulseError;
            }
        }
#endif
        syncAudioDeviceSettingsToLegacyIni();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setAudioOutputDeviceName(value);
        }
        QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        decodium::beginActiveSettingsProfile(s);
        s.setValue(QStringLiteral("audioOutputDevice"), m_audioOutputDevice);
        s.setValue(QStringLiteral("SoundOutName"), m_audioOutputDevice);
        if (m_audioOutputDeviceId.isEmpty()) {
            s.remove(QStringLiteral("audioOutputDeviceId"));
        } else {
            s.setValue(QStringLiteral("audioOutputDeviceId"), m_audioOutputDeviceId);
        }
        s.sync();
        emit audioOutputDeviceChanged();
        applyAudioOutputRuntimeChange(QStringLiteral("device %1 -> %2")
                                          .arg(decodiumBridgeAudioDeviceSelectionForLog(previous, previousId),
                                               decodiumBridgeAudioDeviceSelectionForLog(m_audioOutputDevice, m_audioOutputDeviceId)));
    }
}
int DecodiumBridge::audioInputChannel() const { return m_audioInputChannel; }
void DecodiumBridge::setAudioInputChannel(int v) {
    v = qBound(0, v, 3);
    if (m_audioInputChannel != v) {
        int const previous = m_audioInputChannel;
        m_audioInputChannel = v;
        syncAudioDeviceSettingsToLegacyIni();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setAudioInputChannel(v);
        }
        emit audioInputChannelChanged();
        applyAudioInputRuntimeChange(QStringLiteral("channel %1 -> %2").arg(previous).arg(m_audioInputChannel));
    }
}
int DecodiumBridge::audioOutputChannel() const { return m_audioOutputChannel; }
void DecodiumBridge::setAudioOutputChannel(int v) {
    v = qBound(0, v, 3);
    if (m_audioOutputChannel != v) {
        int const previous = m_audioOutputChannel;
        m_audioOutputChannel = v;
        syncAudioDeviceSettingsToLegacyIni();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setAudioOutputChannel(v);
        }
        emit audioOutputChannelChanged();
        applyAudioOutputRuntimeChange(QStringLiteral("channel %1 -> %2").arg(previous).arg(m_audioOutputChannel));
    }
}
