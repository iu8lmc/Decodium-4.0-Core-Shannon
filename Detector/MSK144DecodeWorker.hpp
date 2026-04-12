// -*- Mode: C++ -*-
#ifndef MSK144DECODEWORKER_HPP
#define MSK144DECODEWORKER_HPP

#include <array>

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QVector>

namespace decodium
{
namespace msk144
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
  double trperiod {15.0};
  QByteArray mycall;
  QByteArray hiscall;
  bool shorthandEnabled {false};
  bool trainingEnabled {false};
  bool swlEnabled {false};
};

struct RealtimeDecodeRequest
{
  short const* audio {nullptr};
  int nutc {0};
  float tsec {0.0f};
  int rxfreq {1500};
  int ftol {50};
  int aggressive {0};
  QByteArray mycall;
  QByteArray hiscall;
  bool shorthandEnabled {false};
  bool trainingEnabled {false};
  bool swlEnabled {false};
  bool* trainingEnabledState {nullptr};
  std::array<double, 5> phaseEqCoefficients {{0.0, 0.0, 0.0, 0.0, 0.0}};
  QString dataDir;
};

class MSK144DecodeWorker final : public QObject
{
  Q_OBJECT

public:
  explicit MSK144DecodeWorker (QObject * parent = nullptr);

  void decode (DecodeRequest const& request);

Q_SIGNALS:
  void decodeReady (quint64 serial, QStringList rows);
};

QStringList decodeMsk144Rows (DecodeRequest const& request);
QString decodeMsk144RealtimeBlock (RealtimeDecodeRequest const& request);
void resetMsk144DecoderState ();

}
}

#endif
