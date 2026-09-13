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
  int nutc {0};
  int nqsoprogress {0};
  int nfqso {0};
  int nfa {0};
  int nfb {0};
  int ndepth {1};
  int threadCount {1};
  int ncontest {0};
  QByteArray mycall;
  QByteArray hiscall;
  QVector<quint32> apHashCache;  // 1.0.294 — snapshot hash28 call viste in banda (AP cache Fase 1)
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
  int threadCount {1};
  int ncontest {0};
  QByteArray mycall;
  QByteArray hiscall;
  QVector<quint32> apHashCache;  // Sprint3-A — AP cache anche sul pass sync (weak-recovery)
};

class FT2DecodeWorker final : public QObject
{
  Q_OBJECT

public:
  explicit FT2DecodeWorker (QObject * parent = nullptr);

  void decodeAsync (AsyncDecodeRequest const& request);
  void decode (DecodeRequest const& request);
  void setDecodeEnabled (bool enabled);
  void markLatestDecodeSerial (quint64 serial);
  void cancelCurrentDecode ();
  void beginShutdown ();

Q_SIGNALS:
  void asyncDecodeReady (QStringList rows);
  void decodeReady (quint64 serial, QStringList rows);

private:
  std::atomic<quint64> m_latestDecodeSerial {0};
  std::atomic<bool> m_decodeEnabled {true};
  std::atomic<bool> m_shuttingDown {false};
};

}
}

#endif
