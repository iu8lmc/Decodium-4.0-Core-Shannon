#pragma once
#include "Audio/AudioDevice.hpp"
#include <QAudioFormat>
#include <QByteArray>
#include <QVector>

AudioDevice::Channel boundedAudioChannel(int);
bool txChannelCarriesSample(int, int, AudioDevice::Channel);
QByteArray buildTxPcmBuffer(const QVector<float>&, const QAudioFormat&, AudioDevice::Channel = AudioDevice::Both);
