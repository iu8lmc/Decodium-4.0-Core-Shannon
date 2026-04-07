// -*- Mode: C++ -*-
#ifndef LEGACYJTDECODEWORKER_HPP
#define LEGACYJTDECODEWORKER_HPP

#include "Detector/JT4Decoder.hpp"
#include "Detector/JT65Decoder.hpp"
#include "Detector/JT9NarrowDecoder.hpp"

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QVector>

namespace decodium
{
namespace legacyjt
{

struct DecodeRequest
{
  quint64 serial {0};
  QString mode;
  QVector<float> ss;
  QVector<short> audio;
  int npts8 {0};
  int nzhsym {0};
  int nutc {0};
  int nfqso {0};
  int ntol {0};
  int ndepth {1};
  int nfa {0};
  int nfb {0};
  int nfsplit {0};
  int nsubmode {0};
  int nclearave {0};
  int minsync {0};
  int minw {0};
  float emedelay {0.0f};
  float dttol {0.0f};
  int newdat {1};
  int nagain {0};
  int n2pass {1};
  int nrobust {0};
  int ntrials {0};
  int naggressive {0};
  int nexp_decode {0};
  int nqsoprogress {0};
  int ljt65apon {0};
  QByteArray mycall;
  QByteArray hiscall;
  QByteArray hisgrid;
  QByteArray tempDir;
  QByteArray dataDir;   // elisir80 1.6.1: percorso dati per deep4 (CALL3.TXT etc.)
};

class LegacyJtDecodeWorker final : public QObject
{
  Q_OBJECT

public:
  explicit LegacyJtDecodeWorker (QObject * parent = nullptr);

  void decode (DecodeRequest const& request);

Q_SIGNALS:
  void decodeReady (quint64 serial, QStringList rows);

private:
  decodium::jt65::AverageState     m_jt65State;
  decodium::jt4::AverageState      m_jt4State;
  decodium::jt9narrow::CorrState   m_jt9NarrowState;
};

}
}

#endif
