// -*- Mode: C++ -*-
#ifndef SOUNDOUT_H__
#define SOUNDOUT_H__

#include <QObject>
#include <QString>
#include <QAudioSink>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QOperatingSystemVersion>
#include <QScopedPointer>

class QIODevice;

// An instance of this sends audio data to a specified soundcard (Qt6).

class SoundOutput
  : public QObject
{
  Q_OBJECT;

public:
  SoundOutput()
    : m_framesBuffered{0}
    , m_volume{1.0}
    , error_{false}
  {}

  qreal attenuation() const;

public Q_SLOTS:
  void setFormat(QAudioDevice const& device, unsigned channels, int frames_buffered = 0);
  void restart(QIODevice*);
  void suspend();
  void resume();
  void reset();
  void stop();
  void setAttenuation(qreal);
  void resetAttenuation();

Q_SIGNALS:
  void error(QString message) const;
  void status(QString message) const;

private:
  bool checkStream() const;

private Q_SLOTS:
  void handleStateChanged(QAudio::State);

private:
  QAudioDevice m_device;
  unsigned m_channels {1};
  QScopedPointer<QAudioSink> m_stream;
  int m_framesBuffered;
  qreal m_volume;
  bool error_;
};

#endif
