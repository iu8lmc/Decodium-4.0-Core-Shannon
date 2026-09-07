// -*- Mode: C++ -*-
#ifndef SOUNDOUT_H__
#define SOUNDOUT_H__

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QAudioSink>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QOperatingSystemVersion>
#include <QPointer>
#include <QScopedPointer>
#include <QTimer>

class QIODevice;

// An instance of this sends audio data to a specified soundcard (Qt6).

struct SoundOutputPlaybackStatus
{
  quint64 sessionId {0};
  quint64 expectedFrames {0};
  quint64 sourceFrames {0};
  quint64 processedFrames {0};
  quint64 leadingQueuedFrames {0};
  quint64 pendingFrames {0};
  quint64 underruns {0};
  bool valid {false};
  bool started {false};
  bool failed {false};
  bool sinkIdle {false};
  bool drained {false};
  QString detail;
};

class SoundOutput
  : public QObject
{
  Q_OBJECT;

public:
  SoundOutput()
    : m_pumpTimer{this}
    , m_framesBuffered{0}
    , m_volume{0.9}
    , error_{false}
  {
    m_pumpTimer.setInterval(5);
    connect(&m_pumpTimer, &QTimer::timeout, this, &SoundOutput::pumpAudio);
  }
  ~SoundOutput() override;

  qreal attenuation() const;

  // Session-scoped playback is used by long, fail-safe transmissions such as
  // SSTV.  The session id is copied into every asynchronous sink diagnostic,
  // while processedUSecs() provides an actual backend-consumption barrier.
  // Existing weak-signal callers keep using restart()/finishPlayback().
  bool restartTrackedPlayback(QIODevice* source,
                              quint64 sessionId,
                              quint64 expectedFrames);
  SoundOutputPlaybackStatus trackedPlaybackStatus(quint64 sessionId) const;
  bool finishTrackedPlayback(quint64 sessionId);
  bool stopTrackedPlayback(quint64 sessionId);

public Q_SLOTS:
  void setFormat(QAudioDevice const& device, unsigned channels, int frames_buffered = 0);
  void restart(QIODevice*);
  void suspend();
  void resume();
  void finishPlayback();
  void reset();
  void stop();
  void setAttenuation(qreal);
  void resetAttenuation();

Q_SIGNALS:
  void error(QString message) const;
  void status(QString message) const;
  void playbackError(quint64 sessionId, QString message) const;
  void playbackUnderrun(quint64 sessionId, QString message) const;

private:
  bool restartImpl(QIODevice* source);
  bool checkStream();
  void emitErrorMessage(QString const& message);
  void emitUnderrunMessage(QString const& message);
  void armTrackedPlaybackClock(bool includeQueuedAudio);
  void clearTrackedPlayback() noexcept;
  void deleteRetiredStreamAfterCoreAudioCallbacks(QAudioSink *stream,
                                                  QString const& reason,
                                                  int delayMs = -1);
  void retireStream(QString const& reason);

private Q_SLOTS:
  void handleStateChanged(QAudio::State);
  void pumpAudio();

private:
  QAudioDevice m_device;
  unsigned m_channels {1};
  QScopedPointer<QAudioSink> m_stream;
  QByteArray m_openDeviceId;
  QPointer<QIODevice> m_streamDevice;
  QPointer<QIODevice> m_sourceDevice;
  QByteArray m_pendingWrite;
  QTimer m_pumpTimer;
  int m_framesBuffered;
  qreal m_volume;
  bool m_coreAudioKeepAlive {false};
  bool error_;
  quint64 m_streamGeneration {0};
  quint64 m_trackedSessionId {0};
  quint64 m_trackedExpectedFrames {0};
  quint64 m_trackedLeadingQueuedFrames {0};
  quint64 m_trackedUnderruns {0};
  qint64 m_trackedProcessedBaselineUs {0};
  QPointer<QIODevice> m_trackedSourceDevice;
  bool m_trackedStarted {false};
  bool m_trackedFailed {false};
  QString m_trackedFailureDetail;
};

#endif
