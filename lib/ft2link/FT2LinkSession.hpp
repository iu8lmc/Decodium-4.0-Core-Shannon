#ifndef DECODIUM_FT2LINK_SESSION_HPP
#define DECODIUM_FT2LINK_SESSION_HPP

#include "lib/ft2link/FT2LinkFrame.hpp"

#include <cstdint>
#include <map>
#include <vector>

namespace decodium
{
namespace ft2link
{

class OutboundTransfer
{
public:
  OutboundTransfer (Profile profile, std::uint16_t sessionId,
                    std::vector<std::uint8_t> const& message);

  void setWindowSize (std::size_t frames);
  void setRetryMs (std::uint64_t retryMs);
  void setMaxAttempts (int attempts);

  std::vector<Frame> framesToSend (std::uint64_t nowMs);
  void makeUnacknowledgedDue (std::uint64_t nowMs);
  void handleAckFrame (Frame const& ack);

  bool complete () const;
  bool failed () const;
  std::size_t frameCount () const;
  std::size_t acknowledgedCount () const;
  int attemptsForSequence (std::uint16_t sequence) const;

private:
  struct PendingFrame
  {
    Frame frame;
    bool acknowledged {false};
    int attempts {0};
    std::uint64_t nextRetryMs {0};
  };

  Profile m_profile;
  std::uint16_t m_sessionId;
  std::size_t m_windowSize {4};
  std::uint64_t m_retryMs {8000};
  int m_maxAttempts {6};
  bool m_failed {false};
  std::vector<PendingFrame> m_frames;
};

class InboundTransfer
{
public:
  InboundTransfer (Profile profile, std::uint16_t sessionId);

  bool receive (Frame const& frame);
  Frame makeAckFrame () const;

  bool complete () const;
  std::vector<std::uint8_t> message () const;
  std::size_t receivedCount () const;

private:
  std::uint16_t firstMissingSequence () const;

  Profile m_profile;
  std::uint16_t m_sessionId;
  bool m_haveLastSequence {false};
  std::uint16_t m_lastSequence {0};
  std::map<std::uint16_t, std::vector<std::uint8_t> > m_received;
};

}
}

#endif
