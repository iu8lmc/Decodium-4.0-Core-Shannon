#include <QAudioDevice>
#include <QAudioFormat>
#include <QCoreApplication>
#include <QMediaDevices>
#include <iostream>
#include "src/bridge/DecodiumBridgeTxFormat.h"

static void bridgeLog(const QString&) {}
void decodiumBridgeAudioLevelLog(const QString&) {}
#define sampleFormatName originalSampleFormatName
#define audioFormatToString originalAudioFormatToString
#define makeAudioFormat originalMakeAudioFormat
#define sameAudioFormat originalSameAudioFormat
#define txPcmChannelCountForDeviceFormat originalChannelCount
#define chooseTxAudioFormat originalChooseTxAudioFormat
#include "bridge_refactor_fixtures/txformat_before.inc"
#undef sampleFormatName
#undef audioFormatToString
#undef makeAudioFormat
#undef sameAudioFormat
#undef txPcmChannelCountForDeviceFormat
#undef chooseTxAudioFormat

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    for (int rate : {8000, 44100, 48000, 96000}) for (int channels : {1, 2, 8})
        for (auto sample : {QAudioFormat::UInt8, QAudioFormat::Int16, QAudioFormat::Int32, QAudioFormat::Float}) {
            QAudioFormat fmt; fmt.setSampleRate(rate); fmt.setChannelCount(channels); fmt.setSampleFormat(sample);
            if (originalAudioFormatToString(fmt) != audioFormatToString(fmt)
                || originalSameAudioFormat(fmt, fmt) != sameAudioFormat(fmt, fmt)) return 1;
            QAudioFormat changed = fmt; changed.setSampleRate(rate + 1);
            if (originalSameAudioFormat(fmt, changed) != sameAudioFormat(fmt, changed)) return 2;
        }
    QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    devices.append(QAudioDevice{}); // Empty device exercises the fallback path.
    for (const auto& device : devices)
        if (originalChooseTxAudioFormat(device) != chooseTxAudioFormat(device)) return 3;
    std::cout << "TX format A/B OK: formats, audio outputs and empty-device fallback (" << devices.size() << " devices)\n";
    return 0;
}
