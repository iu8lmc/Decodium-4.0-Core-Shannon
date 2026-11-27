#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QString>

QString decodiumBridgeAudioDeviceIdForLog(const QAudioDevice& device);
QString decodiumBridgeAudioDeviceIdForSettings(const QAudioDevice& device);
QString decodiumBridgeAudioDeviceIdSettingForLog(QString id);
bool decodiumBridgeAudioDeviceIdMatches(const QAudioDevice& device, const QString& id);
QString decodiumBridgeAudioFormatForLog(const QAudioFormat& format);
