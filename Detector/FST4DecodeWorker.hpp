// -*- Mode: C++ -*-
#ifndef FST4DECODEWORKER_HPP
#define FST4DECODEWORKER_HPP

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QVector>

namespace decodium
{
namespace fst4
{

struct DecodeRequest
{
  quint64 serial {0};
  QString mode;
  QVector<short> audio;
  QByteArray dataDir;
  int nutc {0};
  int nqsoprogress {0};
  int nfa {0};
  int nfb {0};
  int nfqso {0};
  int ndepth {1};
  int ntrperiod {15};
  int nexp_decode {0};
  int ntol {0};
  float emedelay {0.0f};
  int nagain {0};
  int lapcqonly {0};
  QByteArray mycall;
  QByteArray hiscall;
  int iwspr {0};
  int lprinthash22 {0};
};

class FST4DecodeWorker final : public QObject
{
  Q_OBJECT

public:
  explicit FST4DecodeWorker (QObject * parent = nullptr);

  void decode (DecodeRequest const& request);

Q_SIGNALS:
  void decodeReady (quint64 serial, QStringList rows);
};

QStringList decodeFst4Rows (DecodeRequest const& request);

}
}

#endif
