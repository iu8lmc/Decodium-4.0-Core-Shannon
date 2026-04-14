#include "soundout.h"

#include <QDateTime>
#include <qmath.h>
#include <QDebug>

#include "Audio/AudioDevice.hpp"

#include "moc_soundout.cpp"

namespace
{
QString formatSummary(QAudioFormat const& format)
{
  return QStringLiteral("rate=%1 ch=%2 sample=%3 bytes=%4 cfg=%5")
      .arg(format.sampleRate())
      .arg(format.channelCount())
      .arg(static_cast<int>(format.sampleFormat()))
      .arg(format.bytesPerFrame())
      .arg(static_cast<quint32>(format.channelConfig()));
}
}

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
  m_pumpTimer.stop();
  m_streamDevice.clear();
  m_sourceDevice.clear();
  m_pendingWrite.clear();

  if (!m_device.id().isEmpty()) {
    // Keep TX format fixed to 48 kHz / Int16 to match the modulator output.
    // On macOS many devices report Float32 as their native format; Qt/CoreAudio
    // can still convert this stream internally, so we log unsupported-native
    // formats instead of aborting early.
    QAudioFormat format;
    format.setChannelCount(static_cast<int>(m_channels));
    format.setChannelConfig(m_channels == 1
                                ? QAudioFormat::ChannelConfigMono
                                : QAudioFormat::ChannelConfigStereo);
    format.setSampleRate(48000);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioFormat const preferred = m_device.preferredFormat();
    Q_EMIT status(QStringLiteral("TX audio fmt req=%1 pref=%2 dev=%3")
                      .arg(formatSummary(format))
                      .arg(formatSummary(preferred))
                      .arg(m_device.description()));

    if (!format.isValid()) {
      Q_EMIT error(tr("Requested output audio format is not valid."));
      return;
    }
    if (!m_device.isFormatSupported(format)) {
      Q_EMIT status(tr("TX audio: device does not natively support %1 Hz / Int16 "
                       "(preferred: %2 Hz / %3 ch) – relying on Qt/CoreAudio conversion")
                    .arg(format.sampleRate())
                    .arg(preferred.sampleRate())
                    .arg(preferred.channelCount()));
    }

    m_stream.reset(new QAudioSink(m_device, format));
    checkStream();
    m_stream->setVolume(static_cast<float>(m_volume));
    error_ = false;
    Q_EMIT status(QStringLiteral("TX audio sink opened fmt=%1 dev=%2")
                      .arg(formatSummary(m_stream->format()))
                      .arg(m_device.description()));

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

#if defined(Q_OS_MAC)
  m_sourceDevice = source;
  m_streamDevice = m_stream->start();
  if (!m_streamDevice) {
    Q_EMIT error(tr("Unable to open the Qt audio sink device for writing."));
    return;
  }
  pumpAudio();
  if (m_sourceDevice || !m_pendingWrite.isEmpty()) {
    m_pumpTimer.start();
  }
#else
  m_stream->start(source);
#endif
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
  m_pumpTimer.stop();
  m_pendingWrite.clear();
  m_streamDevice.clear();
  m_sourceDevice.clear();
  if (m_stream) {
    m_stream->reset();
    checkStream();
  }
}

void SoundOutput::stop()
{
  m_pumpTimer.stop();
  m_pendingWrite.clear();
  m_streamDevice.clear();
  m_sourceDevice.clear();
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

void SoundOutput::pumpAudio()
{
#if defined(Q_OS_MAC)
  if (!m_stream || !m_streamDevice) {
    m_pumpTimer.stop();
    return;
  }

  constexpr qsizetype maxChunkBytes = 16384;
  const int bytesPerFrame = qMax(1, m_stream->format().bytesPerFrame());

  while (true) {
    if (!m_pendingWrite.isEmpty()) {
      const qsizetype allowed = qMin<qsizetype>(m_stream->bytesFree(), m_pendingWrite.size());
      const qsizetype writable = allowed - (allowed % bytesPerFrame);
      if (writable <= 0) {
        break;
      }

      const qint64 written = m_streamDevice->write(m_pendingWrite.constData(), writable);
      if (written < 0) {
        m_pumpTimer.stop();
        Q_EMIT error(tr("An error occurred during write to the audio output device."));
        return;
      }

      if (written > 0) {
        m_pendingWrite.remove(0, static_cast<int>(written));
      }

      if (!m_pendingWrite.isEmpty()) {
        break;
      }
    }

    if (!m_sourceDevice) {
      if (m_pendingWrite.isEmpty()) {
        m_pumpTimer.stop();
      }
      break;
    }

    qsizetype request = qMin<qsizetype>(m_stream->bytesFree(), maxChunkBytes);
    request -= request % bytesPerFrame;
    if (request <= 0) {
      break;
    }

    QByteArray chunk(static_cast<int>(request), Qt::Uninitialized);
    const qint64 read = m_sourceDevice->read(chunk.data(), request);
    if (read < 0) {
      m_pumpTimer.stop();
      Q_EMIT error(tr("An error occurred during write to the audio output device."));
      return;
    }

    if (read == 0) {
      m_sourceDevice.clear();
      if (m_pendingWrite.isEmpty()) {
        m_pumpTimer.stop();
      }
      break;
    }

    chunk.truncate(static_cast<int>(read));
    const qint64 written = m_streamDevice->write(chunk.constData(), chunk.size());
    if (written < 0) {
      m_pumpTimer.stop();
      Q_EMIT error(tr("An error occurred during write to the audio output device."));
      return;
    }

    if (written < chunk.size()) {
      m_pendingWrite = chunk.mid(static_cast<int>(written));
      break;
    }
  }
#endif
}

void SoundOutput::handleStateChanged(QAudio::State newState)
{
  switch (newState) {
  case QAudio::IdleState:
    pumpAudio();
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
