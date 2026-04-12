// -*- Mode: C++ -*-
#ifndef SOUNDIN_H__
#define SOUNDIN_H__

#include <limits>
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QScopedPointer>
#include <QPointer>
#include <QAudioSource>
#include <QAudioDevice>

#include "Audio/AudioDevice.hpp"

class QAudioDevice;
class QAudioSource;

// Gets audio data from sound sample source and passes it to a sink device
class SoundInput
  : public QObject
{
  Q_OBJECT;

public:
  SoundInput (QObject * parent = nullptr)
    : QObject {parent}
    , cummulative_lost_usec_ {std::numeric_limits<qint64>::min ()}
  {
  }

  ~SoundInput ();

  // sink must exist from the start call until the next start call or
  // stop call
  Q_SLOT void start(QAudioDevice const&, int framesPerBuffer, AudioDevice * sink, unsigned downSampleFactor, AudioDevice::Channel = AudioDevice::Mono);
  Q_SLOT void suspend ();
  Q_SLOT void resume ();
  Q_SLOT void stop ();
  Q_SLOT void reset (bool report_dropped_frames);
  void setInputGain (float gain)
  {
    m_inputGain = qMax (0.0f, gain);
    if (m_sink) m_sink->setInputGainLinear (m_inputGain);
    if (m_stream) m_stream->setVolume (1.0f);
  }
  float inputGain () const { return m_inputGain; }

  Q_SIGNAL void error (QString message) const;
  Q_SIGNAL void status (QString message) const;

private:
  // used internally
  Q_SLOT void handleStateChanged (QAudio::State);

  bool checkStream ();

  QScopedPointer<QAudioSource> m_stream;
  QPointer<AudioDevice> m_sink;
  qint64 cummulative_lost_usec_;
  float m_inputGain {1.0f};
  QString m_deviceDescription;
  int m_sampleRate {0};
  int m_channelCount {0};
  int m_channelSelector {static_cast<int>(AudioDevice::Mono)};
};

#endif
