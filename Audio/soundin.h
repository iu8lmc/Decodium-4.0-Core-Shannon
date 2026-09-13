// -*- Mode: C++ -*-
#ifndef SOUNDIN_H__
#define SOUNDIN_H__

#include <limits>
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QScopedPointer>
#include <QPointer>
#include <QVector>
#include <QAudioFormat>
#include <QAudioSource>
#include <QAudioDevice>
#include <QByteArray>
#include <QTimer>

#if defined(Q_OS_MACOS)
#include <AudioToolbox/AudioToolbox.h>
#endif

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
#if defined(Q_OS_MACOS)
    m_pullTimer.setParent (this);
    m_pullTimer.setTimerType (Qt::PreciseTimer);
    connect (&m_pullTimer, &QTimer::timeout, this, &SoundInput::pullAudioData);
#endif
  }

  ~SoundInput ();

  // sink must exist from the start call until the next start call or
  // stop call
  bool isActiveFor (QAudioDevice const&, unsigned downSampleFactor, AudioDevice::Channel = AudioDevice::Mono) const;
  Q_SLOT void start(QAudioDevice const&, int framesPerBuffer, AudioDevice * sink, unsigned downSampleFactor, AudioDevice::Channel = AudioDevice::Mono);
  Q_SLOT void restart(QAudioDevice const&, int framesPerBuffer, AudioDevice * sink, unsigned downSampleFactor, AudioDevice::Channel = AudioDevice::Mono);
  Q_SLOT void suspend ();
  Q_SLOT void resume ();
  Q_SLOT void stop ();
  Q_SLOT void reset (bool report_dropped_frames);
  Q_SLOT void setInputGain (float gain);
  float inputGain () const;

  Q_SIGNAL void error (QString message) const;
  // Windows/WASAPI can report a recoverable input IOError after a USB audio
  // endpoint transition. The Decodium bridge handles this without a modal
  // dialog and starts its bounded audio-watchdog recovery path.
  Q_SIGNAL void recoverableError (QString message) const;
  Q_SIGNAL void status (QString message) const;

private:
  // used internally
  Q_SLOT void handleStateChanged (QAudio::State);

  bool checkStream ();
  void emitStatusIfChanged (QString const& message, QAudio::State state);
  void retireCurrentStream ();
#if defined(Q_OS_MACOS)
	  void pullAudioData ();
	  void stopPullAudio ();
	  bool startNativeMacInput (QAudioDevice const&, QAudioFormat const&,
	                            int framesPerBuffer, AudioDevice *,
	                            AudioDevice::Channel, QString *failureReason);
  void stopNativeMacInput ();
  static void audioQueueInputCallback (void *, AudioQueueRef, AudioQueueBufferRef,
                                       AudioTimeStamp const *, UInt32,
                                       AudioStreamPacketDescription const *);
#endif

  QScopedPointer<QAudioSource> m_stream;
  QPointer<AudioDevice> m_sink;
#if defined(Q_OS_MACOS)
  AudioQueueRef m_audioQueue {nullptr};
  QVector<AudioQueueBufferRef> m_audioQueueBuffers;
  QAudioFormat m_nativeInputFormat;
  QPointer<QIODevice> m_pullDevice;
  QByteArray m_pullPendingData;
  QByteArray m_pullReadBuffer;
  int m_pullBytesPerFrame {0};
  quint64 m_pullReadCalls {0};
  quint64 m_pullReadFrames {0};
  quint64 m_pullSignalSamples {0};
  quint64 m_pullSignalNonZero {0};
  qint32 m_pullSignalPeak {0};
  double m_pullSignalEnergy {0.0};
  qint64 m_pullLastTraceMs {-1};
  bool m_usingPullAudio {false};
#endif
  QTimer m_pullTimer;
  qint64 cummulative_lost_usec_;
  qint64 last_dropped_warning_ms_ {-1};
  float m_inputGain {1.0f};
  QString m_deviceDescription;
  QString m_deviceId;
  int m_sampleRate {0};
  int m_channelCount {0};
  int m_channelSelector {static_cast<int>(AudioDevice::Mono)};
  QString m_lastStatusMessage;
  QString m_lastDuplicateStartKey;
  QString m_currentStartKey;
  qint64 m_currentStartRequestedMs {-1};
  QPointer<AudioDevice> m_currentStartSink;
  QAudio::State m_lastReportedState {QAudio::StoppedState};
  qint64 m_lastDebugStateLogMs {-1};
  qint64 m_lastDuplicateStartLogMs {-1};
  int m_suppressedDebugStateLogs {0};
  int m_suppressedDuplicateStartLogs {0};
#if defined(Q_OS_LINUX)
  quint64 m_deferredRestartGeneration {0};
#endif
  bool m_haveReportedState_ {false};
  bool m_expectedSuspend_ {false};
  bool m_startInProgress {false};
};

#endif
