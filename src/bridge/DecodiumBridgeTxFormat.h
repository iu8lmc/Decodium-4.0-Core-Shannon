#pragma once
#include <QAudio>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QString>
QString audioFormatToString(const QAudioFormat&);
bool sameAudioFormat(const QAudioFormat&, const QAudioFormat&);
QAudioFormat chooseTxAudioFormat(const QAudioDevice&);
QString audioStateToString(QAudio::State);
QString audioErrorToString(QAudio::Error);
qsizetype txAudioBufferBytesForMode(const QAudioFormat&, const QString&, bool, bool);
int txAudioPrecomputeRetryDelayMs(const QString&, bool);
