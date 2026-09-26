// -*- Mode: C++ -*-
#ifndef FT4DECODEWORKER_HPP
#define FT4DECODEWORKER_HPP

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QVector>
#include <atomic>

namespace decodium
{
namespace ft4
{

// Stops the asynchronous hash-seed worker before Qt application teardown.
void shutdownHashSeedWorker ();

struct DecodeRequest
{
  quint64 serial {0};
  QVector<short> audio;
  int nutc {0};
  int nqsoprogress {0};
  int nfqso {0};
  int nfa {0};
  int nfb {0};
  int ndepth {1};
  int threadCount {1};
  int lapcqonly {0};
  int ncontest {0};
  QByteArray mycall;
  QByteArray hiscall;
};

class FT4DecodeWorker final : public QObject
{
  Q_OBJECT

public:
  explicit FT4DecodeWorker (QObject * parent = nullptr);

  void decode (DecodeRequest const& request);
  void markLatestDecodeSerial (quint64 serial);
  void beginShutdown ();

Q_SIGNALS:
  void decodeReady (quint64 serial, QStringList rows);

private:
  std::atomic<quint64> m_latestDecodeSerial {0};
  std::atomic<bool> m_shuttingDown {false};
};

}
}

#endif
