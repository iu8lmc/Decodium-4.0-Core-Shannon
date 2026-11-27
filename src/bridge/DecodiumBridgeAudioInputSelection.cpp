#include "DecodiumBridge.h"
#include <QDebug>
#include <QMediaDevices>

void decodiumBridgeAudioLevelLog(const QString&);
bool decodiumBridgeResolveAudioDeviceByIdentity(QList<QAudioDevice> const&, QString const&, QString const&, QAudioDevice*, QString*);
#ifdef Q_OS_LINUX
QString decodiumBridgeLinuxPulseSourceNameFromDisplay(QString);
bool decodiumBridgeLinuxSetDefaultPulseSource(const QString&, QString*);
#endif

QAudioDevice DecodiumBridge::resolveRxInputDevice(const QString& requestedName,
                                                  const QString& requestedId,
                                                  bool* requestedDeviceFound,
                                                  QString* matchReason)
{
    QString const requested = requestedName.trimmed();
    QList<QAudioDevice> const inputs =
        cachedAudioInputs(QStringLiteral("resolveRxInputDevice"), false);
    QAudioDevice selectedDevice =
        cachedDefaultAudioInput(QStringLiteral("resolveRxInputDevice"), false);
    if (selectedDevice.description().trimmed().isEmpty() && !inputs.isEmpty()) {
        selectedDevice = inputs.constFirst();
    }

#ifdef Q_OS_LINUX
    const QString pulseSourceName = decodiumBridgeLinuxPulseSourceNameFromDisplay(requested);
    if (!pulseSourceName.isEmpty()) {
        QString pulseError;
        if (decodiumBridgeLinuxSetDefaultPulseSource(pulseSourceName, &pulseError)) {
            decodiumBridgeAudioLevelLog(QStringLiteral("startAudioCapture: selected Linux Pulse/PipeWire source [%1], using Qt default input after pactl default-source")
                          .arg(pulseSourceName));
            qInfo().noquote() << "[AUDIODEV] selected Pulse/PipeWire source:" << pulseSourceName;
            if (requestedDeviceFound) {
                *requestedDeviceFound = true;
            }
            if (matchReason) {
                *matchReason = QStringLiteral("linux-pulse-source");
            }
            return QMediaDevices::defaultAudioInput();
        }

        decodiumBridgeAudioLevelLog(QStringLiteral("startAudioCapture: failed to select Linux Pulse/PipeWire source [%1]: %2")
                      .arg(pulseSourceName, pulseError));
        qWarning().noquote() << "[AUDIODEV] failed to select Pulse/PipeWire source:"
                             << pulseSourceName << pulseError;
    }
#endif

    QAudioDevice matchedDevice;
    QString reason;
    if (decodiumBridgeResolveAudioDeviceByIdentity(inputs, requested, requestedId, &matchedDevice, &reason)) {
        if (requestedDeviceFound) {
            *requestedDeviceFound = true;
        }
        if (matchReason) {
            *matchReason = reason;
        }
        return matchedDevice;
    }

    if (requestedDeviceFound) {
        *requestedDeviceFound = requested.isEmpty();
    }
    if (matchReason) {
        *matchReason = requested.isEmpty() ? QStringLiteral("default") : reason;
    }
    return selectedDevice;
}

