// -*- Mode: C++ -*-
#ifndef Q65DECODEWORKER_HPP
#define Q65DECODEWORKER_HPP

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QVector>

namespace decodium
{
namespace q65
{

struct DecodeRequest
{
  quint64 serial {0};
  QVector<short> audio;
  int nqd0 {1};
  int nutc {0};
  int ntrperiod {60};
  int nsubmode {0};
  int nfqso {0};
  int ntol {0};
  int ndepth {1};
  int nfa {0};
  int nfb {0};
  int nclearave {0};
  bool coherentAvgEnabled {false};
  int single_decode {0};
  int nagain {0};
  int max_drift {0};
  int lnewdat {1};
  float emedelay {0.0f};
  QByteArray mycall;
  QByteArray hiscall;
  QByteArray hisgrid;
  int nqsoprogress {0};
  int ncontest {0};
  int lapcqonly {0};
};

class Q65DecodeWorker final : public QObject
{
  Q_OBJECT

public:
  explicit Q65DecodeWorker (QObject * parent = nullptr);

  void decode (DecodeRequest const& request);

Q_SIGNALS:
  void decodeReady (quint64 serial, QStringList rows);
  void coherentCount (int signalsInCache);
};

}
}

#endif
