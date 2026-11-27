#include "DecodiumBridge.h"
#include "Audio/soundout.h"
#include <QAudioDevice>
#include <QAudioFormat>

void decodiumBridgeDecodeFlowLog(const QString&);
QAudioFormat decodiumBridgeTxPrepareChooseFormat(const QAudioDevice&);
QString decodiumBridgeTxPrepareAudioFormat(const QAudioFormat&);
#define bridgeLog decodiumBridgeDecodeFlowLog
#define chooseTxAudioFormat decodiumBridgeTxPrepareChooseFormat
#define audioFormatToString decodiumBridgeTxPrepareAudioFormat

void DecodiumBridge::updateSoundOutputDevice()
{
    bool requestedDeviceFound = false;
    QAudioDevice outDev = resolveTxOutputDevice(&requestedDeviceFound);

    const QAudioFormat fmt = chooseTxAudioFormat(outDev);
    m_soundOutput->setFormat(outDev, static_cast<unsigned>(qMax(1, fmt.channelCount())), 16384);
    bridgeLog("updateSoundOutputDevice: " + outDev.description() +
              " found=" + QString::number(requestedDeviceFound) +
              " fmt=" + audioFormatToString(fmt));
}
