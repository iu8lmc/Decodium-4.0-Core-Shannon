// -*- Mode: C++ -*-
#ifndef JT9FASTDECODEWORKER_HPP
#define JT9FASTDECODEWORKER_HPP

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QVector>

#include "Detector/JT9FastDecoder.hpp"

namespace decodium
{
namespace jt9fast
{

struct DecodeRequest
{
  quint64 serial {0};
  QVector<short> audio;
  int nutc {0};
  int kdone {0};
  int nsubmode {0};
  int newdat {1};
  int minsync {0};
  int npick {0};
  int t0_ms {0};
  int t1_ms {0};
  int maxlines {2};
  int rxfreq {1500};
  int ftol {50};
  int aggressive {0};
  double trperiod {30.0};
  QByteArray mycall;
  QByteArray hiscall;
};

class JT9FastDecodeWorker final : public QObject
{
  Q_OBJECT

public:
  explicit JT9FastDecodeWorker (QObject * parent = nullptr);

  void decode (DecodeRequest const& request);

Q_SIGNALS:
  void decodeReady (quint64 serial, QStringList rows);

private:
  FastDecodeState state_;
};

}
}

#endif
