// -*- Mode: C++ -*-
#ifndef FT4DECODEWORKER_HPP
#define FT4DECODEWORKER_HPP

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QVector>

namespace decodium
{
namespace ft4
{

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

class FT4DecodeWorker final : public QObject
{
  Q_OBJECT

public:
  explicit FT4DecodeWorker (QObject * parent = nullptr);

  void decode (DecodeRequest const& request);

Q_SIGNALS:
  void decodeReady (quint64 serial, QStringList rows);
};

}
}

#endif
