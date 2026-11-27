// -*- Mode: C++ -*-
#ifndef WSPRDECODEWORKER_HPP
#define WSPRDECODEWORKER_HPP

#include <QObject>
#include <QStringList>
#include <atomic>

namespace decodium
{
namespace wspr
{

struct DecodeRequest
{
  quint64 serial {0};
  QStringList arguments;
};

class WSPRDecodeWorker final : public QObject
{
  Q_OBJECT

public:
  explicit WSPRDecodeWorker (QObject * parent = nullptr);

  // Called from the bridge thread. Serial numbers are monotonic across sessions.
  void retireThrough (quint64 serial) noexcept
  {
    m_retiredSerial.store (serial, std::memory_order_release);
  }

  void decode (DecodeRequest const& request);

Q_SIGNALS:
  void decodeReady (quint64 serial, QStringList rows, QStringList diagnostics, int exitCode);

private:
  std::atomic<quint64> m_retiredSerial {0};
};

}
}

#endif
