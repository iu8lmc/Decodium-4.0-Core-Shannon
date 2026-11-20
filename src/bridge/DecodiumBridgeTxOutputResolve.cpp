#include "DecodiumBridgeAudioDeviceIdentity.h"
#include <QAudioDevice>
#include <QMediaDevices>
#include <QDebug>
#include <QList>
#include <QString>

bool decodiumBridgeResolveAudioDeviceByIdentity(const QList<QAudioDevice>&, const QString&, const QString&, QAudioDevice*, QString*);
void decodiumBridgeTxOutputResolveLog(const QString&);
#define bridgeLog decodiumBridgeTxOutputResolveLog
#ifdef Q_OS_LINUX
QString decodiumBridgeLinuxPulseSinkNameFromDisplay(QString);
bool decodiumBridgeLinuxSetDefaultPulseSink(const QString&, QString*);
#define linuxPulseSinkNameFromDisplay decodiumBridgeLinuxPulseSinkNameFromDisplay
#define linuxSetDefaultPulseSink decodiumBridgeLinuxSetDefaultPulseSink
#endif

static QAudioDevice findOutputDevice(const QString& name,
                                     const QString& id,
                                     QList<QAudioDevice> const& outputs,
                                     QAudioDevice const& defaultOutput,
                                     bool* requestedDeviceFound,
                                     QString* matchReason)
{
    QString const requestedName = name.trimmed();
#ifdef Q_OS_LINUX
    const QString pulseSinkName = linuxPulseSinkNameFromDisplay(requestedName);
    if (!pulseSinkName.isEmpty()) {
        QString pulseError;
        if (linuxSetDefaultPulseSink(pulseSinkName, &pulseError)) {
            bridgeLog(QStringLiteral("findOutputDevice: selected Linux Pulse/PipeWire sink [%1], using Qt default output after pactl default-sink")
                          .arg(pulseSinkName));
            qInfo().noquote() << "[AUDIODEV] selected Pulse/PipeWire sink:" << pulseSinkName;
            if (requestedDeviceFound) *requestedDeviceFound = true;
            if (matchReason) *matchReason = QStringLiteral("linux-pulse-sink");
            return QMediaDevices::defaultAudioOutput();
        }

        bridgeLog(QStringLiteral("findOutputDevice: failed to select Linux Pulse/PipeWire sink [%1]: %2")
                      .arg(pulseSinkName, pulseError));
        qWarning().noquote() << "[AUDIODEV] failed to select Pulse/PipeWire sink:"
                             << pulseSinkName << pulseError;
    }
#endif
    QAudioDevice matchedDevice;
    QString reason;
    if (decodiumBridgeResolveAudioDeviceByIdentity(outputs, requestedName, id, &matchedDevice, &reason)) {
        if (requestedDeviceFound) *requestedDeviceFound = true;
        if (matchReason) *matchReason = reason;
        if (reason.contains(QStringLiteral("normalized"), Qt::CaseInsensitive)) {
            bridgeLog(QStringLiteral("findOutputDevice: normalized match requested=[%1] requested_id=%2 actual=[%3] actual_id=%4 reason=%5")
                          .arg(requestedName,
                               decodiumBridgeAudioDeviceIdSettingForLog(id),
                               matchedDevice.description(),
                               decodiumBridgeAudioDeviceIdSettingForLog(decodiumBridgeAudioDeviceIdForSettings(matchedDevice)),
                               reason));
        }
        return matchedDevice;
    }

    if (requestedDeviceFound) *requestedDeviceFound = requestedName.isEmpty();
    if (matchReason) *matchReason = requestedName.isEmpty() ? QStringLiteral("default") : reason;
    return defaultOutput;
}

QAudioDevice decodiumBridgeFindTxOutputDevice(const QString& name, const QString& id, const QList<QAudioDevice>& outputs, const QAudioDevice& fallback, bool* found, QString* reason)
{ return findOutputDevice(name, id, outputs, fallback, found, reason); }

