#include "lib/ft2link/FT2LinkSession.hpp"

#include <algorithm>

namespace decodium
{
namespace ft2link
{
namespace
{
std::vector<std::uint8_t> slicePayload (std::vector<std::uint8_t> const& message,
                                        std::size_t begin, std::size_t length)
{
  std::size_t const end = std::min (message.size (), begin + length);
  return std::vector<std::uint8_t> (message.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (begin),
                                    message.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (end));
}
}

OutboundTransfer::OutboundTransfer (Profile profile, std::uint16_t sessionId,
                                    std::vector<std::uint8_t> const& message)
  : m_profile {profile}
  , m_sessionId {sessionId}
{
  std::size_t const capacity = std::max<std::size_t> (1, profilePayloadCapacity (profile));
  std::size_t const frameCount = std::max<std::size_t> (1, (message.size () + capacity - 1) / capacity);
  m_frames.reserve (frameCount);

  for (std::size_t index = 0; index < frameCount; ++index)
    {
      Frame frame;
      frame.type = FrameType::Data;
      frame.profile = profile;
      frame.sessionId = sessionId;
      frame.sequence = static_cast<std::uint16_t> (index);
      frame.payload = slicePayload (message, index * capacity, capacity);
      if (index + 1 == frameCount)
        {
          frame.flags = static_cast<std::uint8_t> (frame.flags | FlagEndOfMessage);
        }
      m_frames.push_back (PendingFrame {frame, false, 0, 0});
    }
}

void OutboundTransfer::setWindowSize (std::size_t frames)
{
  m_windowSize = std::max<std::size_t> (1, frames);
}

void OutboundTransfer::setRetryMs (std::uint64_t retryMs)
{
  m_retryMs = std::max<std::uint64_t> (1, retryMs);
}

void OutboundTransfer::setMaxAttempts (int attempts)
{
  m_maxAttempts = std::max (1, attempts);
}

std::vector<Frame> OutboundTransfer::framesToSend (std::uint64_t nowMs)
{
  std::vector<Frame> out;
  if (m_failed)
    {
      return out;
    }

  std::size_t inFlight = 0;
  for (PendingFrame const& pending : m_frames)
    {
      if (!pending.acknowledged && pending.attempts > 0)
        {
          ++inFlight;
        }
    }

  for (PendingFrame& pending : m_frames)
    {
      if (pending.acknowledged)
        {
          continue;
        }
      bool const due = pending.attempts == 0 || nowMs >= pending.nextRetryMs;
      if (!due)
        {
          continue;
        }
      if (pending.attempts == 0 && inFlight >= m_windowSize)
        {
          continue;
        }
      if (pending.attempts >= m_maxAttempts)
        {
          m_failed = true;
          break;
        }

      ++pending.attempts;
      pending.nextRetryMs = nowMs + m_retryMs;
      out.push_back (pending.frame);
      if (pending.attempts == 1)
        {
          ++inFlight;
        }
      if (out.size () >= m_windowSize)
        {
          break;
        }
    }

  return out;
}

void OutboundTransfer::makeUnacknowledgedDue (std::uint64_t nowMs)
{
  for (PendingFrame& pending : m_frames)
    {
      if (!pending.acknowledged && pending.attempts > 0)
        {
          pending.nextRetryMs = nowMs;
        }
    }
}

void OutboundTransfer::handleAckFrame (Frame const& ack)
{
  if (ack.type != FrameType::Ack || ack.sessionId != m_sessionId || ack.profile != m_profile)
    {
      return;
    }

  for (PendingFrame& pending : m_frames)
    {
      std::uint16_t const sequence = pending.frame.sequence;
      if (sequence < ack.ackBase)
        {
          pending.acknowledged = true;
          continue;
        }

      std::uint16_t const delta = static_cast<std::uint16_t> (sequence - ack.ackBase);
      if (delta < 16u && (ack.ackBitmap & (1u << delta)) != 0)
        {
          pending.acknowledged = true;
        }
    }
}

bool OutboundTransfer::complete () const
{
  return !m_frames.empty ()
      && std::all_of (m_frames.begin (), m_frames.end (),
                      [] (PendingFrame const& pending) { return pending.acknowledged; });
}

bool OutboundTransfer::failed () const
{
  return m_failed;
}

std::size_t OutboundTransfer::frameCount () const
{
  return m_frames.size ();
}

std::size_t OutboundTransfer::acknowledgedCount () const
{
  return static_cast<std::size_t> (std::count_if (m_frames.begin (), m_frames.end (),
      [] (PendingFrame const& pending) { return pending.acknowledged; }));
}

int OutboundTransfer::attemptsForSequence (std::uint16_t sequence) const
{
  for (PendingFrame const& pending : m_frames)
    {
      if (pending.frame.sequence == sequence)
        {
          return pending.attempts;
        }
    }
  return 0;
}

InboundTransfer::InboundTransfer (Profile profile, std::uint16_t sessionId)
  : m_profile {profile}
  , m_sessionId {sessionId}
{
}

bool InboundTransfer::receive (Frame const& frame)
{
  if (frame.type != FrameType::Data || frame.profile != m_profile || frame.sessionId != m_sessionId)
    {
      return false;
    }
  if (frame.payload.size () > profilePayloadCapacity (m_profile))
    {
      return false;
    }

  m_received[frame.sequence] = frame.payload;
  if ((frame.flags & FlagEndOfMessage) != 0)
    {
      m_haveLastSequence = true;
      m_lastSequence = frame.sequence;
    }
  return true;
}

Frame InboundTransfer::makeAckFrame () const
{
  std::uint16_t const base = firstMissingSequence ();
  std::uint16_t bitmap = 0;
  for (std::uint16_t bit = 0; bit < 16u; ++bit)
    {
      std::uint16_t const sequence = static_cast<std::uint16_t> (base + bit);
      if (m_received.find (sequence) != m_received.end ())
        {
          bitmap = static_cast<std::uint16_t> (bitmap | (1u << bit));
        }
    }
  return ft2link::makeAckFrame (m_profile, m_sessionId, base, bitmap);
}

bool InboundTransfer::complete () const
{
  if (!m_haveLastSequence)
    {
      return false;
    }
  for (std::uint16_t sequence = 0; sequence <= m_lastSequence; ++sequence)
    {
      if (m_received.find (sequence) == m_received.end ())
        {
          return false;
        }
      if (sequence == 0xffffu)
        {
          break;
        }
    }
  return true;
}

std::vector<std::uint8_t> InboundTransfer::message () const
{
  std::vector<std::uint8_t> out;
  if (!complete ())
    {
      return out;
    }
  for (std::uint16_t sequence = 0; sequence <= m_lastSequence; ++sequence)
    {
      std::map<std::uint16_t, std::vector<std::uint8_t> >::const_iterator it = m_received.find (sequence);
      if (it != m_received.end ())
        {
          out.insert (out.end (), it->second.begin (), it->second.end ());
        }
      if (sequence == 0xffffu)
        {
          break;
        }
    }
  return out;
}

std::size_t InboundTransfer::receivedCount () const
{
  return m_received.size ();
}

std::uint16_t InboundTransfer::firstMissingSequence () const
{
  std::uint16_t sequence = 0;
  while (m_received.find (sequence) != m_received.end ())
    {
      if (sequence == 0xffffu)
        {
          return sequence;
        }
      ++sequence;
    }
  return sequence;
}

}
}
