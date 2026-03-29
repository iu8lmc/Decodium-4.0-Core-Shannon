// -*- Mode: C++ -*-
#ifndef FT2DECODEWORKER_HPP
#define FT2DECODEWORKER_HPP

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QVector>
#include <atomic>

namespace decodium
{
namespace ft2
{

struct AsyncDecodeRequest
{
  QVector<short> audio;
  int nqsoprogress {0};
  int nfqso {0};
  int nfa {0};
  int nfb {0};
  int ndepth {1};
  int ncontest {0};
  QByteArray mycall;
  QByteArray hiscall;
};

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
  int ncontest {0};
  QByteArray mycall;
  QByteArray hiscall;
};

class FT2DecodeWorker final : public QObject
{
  Q_OBJECT

public:
  explicit FT2DecodeWorker (QObject * parent = nullptr);

  void decodeAsync (AsyncDecodeRequest const& request);
  void decode (DecodeRequest const& request);
  void markLatestDecodeSerial (quint64 serial);
  void cancelCurrentDecode ();
  void beginShutdown ();

Q_SIGNALS:
  void asyncDecodeReady (QStringList rows);
  void decodeReady (quint64 serial, QStringList rows);

private:
  std::atomic<quint64> m_latestDecodeSerial {0};
  std::atomic<bool> m_shuttingDown {false};
};

}
}

#endif
