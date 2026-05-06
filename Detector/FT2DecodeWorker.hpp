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
  // Tier-1 telemetry: emitted right BEFORE asyncDecodeReady, so the
  // matching profile data lands at the bridge slot before the rows handler
  // (Qt::QueuedConnection preserves FIFO across same-receiver same-thread).
  // decoderUs   = duration of the stage7 native decoder call
  // emittedAtNs = std::chrono::steady_clock::now() count(), used by the
  //               bridge to compute the cross-thread queue delay.
  void asyncDecodeProfile (qint64 decoderUs, qint64 emittedAtNs);

  // Tier 1.5 telemetry: granular breakdown del decode_ft2_stage7. Tutti i
  // valori sono in microsecondi e si riferiscono all'ultima invocazione
  // appena conclusa. Emesso subito dopo asyncDecodeProfile.
  void asyncStage7BreakdownProfile (qint64 getcandUs, qint64 demodUs,
                                    qint64 syncUs,    qint64 ldpcUs);

  // Tier 2 telemetry: sub-LDPC breakdown (subset di ldpcUs, non si somma):
  //   priUs = run_decode_passes pass 1-5 totali (LLR variants senza AP)
  //   aprUs = run_decode_passes pass 6+ totali  (a-priori retry)
  //   l91Us = ftx_decode174_91_c cumulativo (BP + OSD nativo Fortran)
  void asyncStage7LdpcBreakdownProfile (qint64 priUs, qint64 aprUs, qint64 l91Us);

  // Tier 3 telemetry: analytics counters dello slot (non microsecondi):
  //   syncCalls       = numero totale chiamate ftx_sync2d_c
  //   decodedCount    = numero decode OK
  //   selectedPassSum = somma dei selected_pass (ipass su cui ogni decode OK)
  // avg pass = selectedPassSum / decodedCount, calcolato dal bridge.
  void asyncStage7Analytics (qint64 syncCalls, qint64 decodedCount, qint64 selectedPassSum);

  void asyncDecodeReady (QStringList rows);
  void decodeReady (quint64 serial, QStringList rows);

private:
  std::atomic<quint64> m_latestDecodeSerial {0};
  std::atomic<bool> m_shuttingDown {false};
};

}
}

#endif
