#include "AudioDevice.hpp"

bool AudioDevice::initialize (OpenMode mode, Channel channel, int sourceChannelCount)
{
  if (isOpen ())
    {
      close ();
    }

  m_channel = channel;
  m_sourceChannelCount = sourceChannelCount > 0 ? sourceChannelCount : (Mono == channel ? 1 : 2);

  // open and ensure we are unbuffered if possible
  return QIODevice::open (mode | QIODevice::Unbuffered);
}
