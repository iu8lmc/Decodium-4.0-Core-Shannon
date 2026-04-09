#include "soundout.h"

#include <QDateTime>
#include <qmath.h>
#include <QDebug>

#include "Audio/AudioDevice.hpp"

#include "moc_soundout.cpp"

bool SoundOutput::checkStream() const
{
  bool result {false};
  Q_ASSERT_X(m_stream, "SoundOutput", "programming error");
  if (m_stream) {
    switch (m_stream->error()) {
    case QAudio::OpenError:
      Q_EMIT error(tr("An error opening the audio output device has occurred."));
      break;
    case QAudio::IOError:
      Q_EMIT error(tr("An error occurred during write to the audio output device."));
      break;
    case QAudio::UnderrunError:
      // Keep underruns non-fatal and report status.
      Q_EMIT status(tr("Audio output underrun"));
      result = true;
      break;
    case QAudio::FatalError:
      Q_EMIT error(tr("Non-recoverable error, audio output device not usable at this time."));
      break;
    case QAudio::NoError:
      result = true;
      break;
    }
  }
  return result;
}

void SoundOutput::setFormat(QAudioDevice const& device, unsigned channels, int frames_buffered)
{
  Q_ASSERT(0 < channels && channels < 3);
  m_device = device;
  m_channels = channels;
  m_framesBuffered = frames_buffered;
}

void SoundOutput::restart(QIODevice* source)
{
  if (!m_device.id().isEmpty()) {
    QAudioFormat format(m_device.preferredFormat());
    format.setChannelCount(static_cast<int>(m_channels));
    format.setSampleRate(48000);
    format.setSampleFormat(QAudioFormat::Int16);

    if (!format.isValid()) {
      Q_EMIT error(tr("Requested output audio format is not valid."));
      return;
    }
    if (!m_device.isFormatSupported(format)) {
      Q_EMIT error(tr("Requested output audio format is not supported on device."));
      return;
    }

    m_stream.reset(new QAudioSink(m_device, format));
    checkStream();
    m_stream->setVolume(static_cast<float>(m_volume));
    error_ = false;

    connect(m_stream.data(), &QAudioSink::stateChanged,
            this, &SoundOutput::handleStateChanged);
  }

  if (!m_stream) {
    if (!error_) {
      error_ = true;
      Q_EMIT error(tr("No audio output device configured."));
    }
    return;
  } else {
    error_ = false;
  }

  // Buffer sizing: Windows needs a larger buffer to prevent underruns.
  if (m_framesBuffered > 0) {
    m_stream->setBufferSize(
        static_cast<qsizetype>(m_stream->format().bytesForFrames(m_framesBuffered)));
  } else {
    // Default: 16384 frames (~341 ms @ 48 kHz)
    m_stream->setBufferSize(
        static_cast<qsizetype>(m_stream->format().bytesForFrames(16384)));
  }

  m_stream->start(source);
  // diagnostico: stato subito dopo start
  Q_EMIT status(QString("after_start: state=%1 err=%2 bufSize=%3")
    .arg(m_stream->state()).arg(m_stream->error()).arg(m_stream->bufferSize()));
}

void SoundOutput::suspend()
{
  if (m_stream && QAudio::ActiveState == m_stream->state()) {
    m_stream->suspend();
    checkStream();
  }
}

void SoundOutput::resume()
{
  if (m_stream && QAudio::SuspendedState == m_stream->state()) {
    m_stream->resume();
    checkStream();
  }
}

void SoundOutput::reset()
{
  if (m_stream) {
    m_stream->reset();
    checkStream();
  }
}

void SoundOutput::stop()
{
  if (m_stream) {
#if defined(__APPLE__)
    const bool sequoiaOrNewer =
        QOperatingSystemVersion::current() >= QOperatingSystemVersion(QOperatingSystemVersion::MacOS, 15);
    if (!sequoiaOrNewer)
      m_stream->reset();
#else
    m_stream->reset();
#endif
    m_stream->stop();
  }
#if defined(__APPLE__)
  if (QOperatingSystemVersion::current() < QOperatingSystemVersion(QOperatingSystemVersion::MacOS, 15))
    m_stream.reset();
#endif
}

qreal SoundOutput::attenuation() const
{
  return -(20. * qLn(m_volume) / qLn(10.));
}

void SoundOutput::setAttenuation(qreal a)
{
  Q_ASSERT(0. <= a && a <= 999.);
  m_volume = qPow(10.0, -a / 20.0);
  if (m_stream)
    m_stream->setVolume(static_cast<float>(m_volume));
}

void SoundOutput::resetAttenuation()
{
  m_volume = 1.;
  if (m_stream)
    m_stream->setVolume(static_cast<float>(m_volume));
}

void SoundOutput::handleStateChanged(QAudio::State newState)
{
  switch (newState) {
  case QAudio::IdleState:
    Q_EMIT status(tr("Idle"));
    break;
  case QAudio::ActiveState:
    Q_EMIT status(tr("Sending"));
    break;
  case QAudio::SuspendedState:
    Q_EMIT status(tr("Suspended"));
    break;
  case QAudio::StoppedState:
    if (!checkStream())
      Q_EMIT status(tr("Error"));
    else
      Q_EMIT status(tr("Stopped"));
    break;
  default:
    break;
  }
}
