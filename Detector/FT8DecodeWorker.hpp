// -*- Mode: C++ -*-
#ifndef FT8DECODEWORKER_HPP
#define FT8DECODEWORKER_HPP

#include <QObject>
#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <optional>

#include "Decoder/decodedtext.h"

namespace decodium
{
namespace ft8
{

// Stops the asynchronous hash-seed worker before Qt application teardown.
// The worker uses Qt containers and regular expressions and must not outlive
// the Qt runtime.
void shutdownHashSeedWorker ();

struct DecodeRequest
{
  quint64 serial {0};
  QVector<short> audio;
  int nqsoprogress {0};
  int nfqso {0};
  int nftx {0};
  int nutc {0};
  int nfa {0};
  int nfb {0};
  int nzhsym {50};
  int ndepth {1};
  int threadCount {1};
  float emedelay {0.0f};
  int ncontest {0};
  bool superFoxEnabled {false};
  int superFoxTolHz {50};
  int nagain {0};
  int lft8apon {0};
  int lmultift8 {0};
  int lapcqonly {0};
  int napwid {50};
  int ldiskdat {0};
  int ncandthin {30};
  int nft8Cycles {1};
  int nft8RxFreqSensitivity {1};
  int maxDecodeMs {0};
  int availableSamples {0};
  bool hasFreshAudio {true};
  bool lowThresholds {false};
  bool subpass {false};
  bool supplemental {false};
  bool osdFollowup {false};
  bool cpuPressureLimited {false};
  bool coherentAvgEnabled {false};
  bool neuralSyncEnabled {false};
  bool turboFeedbackEnabled {false};
  QByteArray mycall;
  QByteArray hiscall;
  QByteArray hisgrid;
};

// Immutable transport produced by the DSP worker.  Keep the raw legacy row
// for the existing renderer, but expose the fields needed by the UI
// dispatcher so they are not parsed again on the main thread.
struct DecodedEntry
{
  QString row;
  QString message;
  std::optional<DecodedText> decodedText0;
  std::optional<DecodedText> decodedText;
  int snr {0};
  float dt {0.0f};
  int audioFrequency {0};
  int apType {0};
  float quality {0.0f};
  bool addressedToMe {false};
  bool fromActivePartner {false};
  bool qsoExchange {false};

  bool isUiPriority () const
  {
    return addressedToMe || (fromActivePartner && qsoExchange);
  }
};

class FT8DecodeWorker final : public QObject
{
  Q_OBJECT

public:
  explicit FT8DecodeWorker (QObject * parent = nullptr);

  void decode (DecodeRequest const& request);
  void markLatestDecodeSerial (quint64 serial);
  void resetDecoderState ();
  void cancelCurrentDecode ();
  void beginShutdown ();

Q_SIGNALS:
  void decodeReady (quint64 serial, QStringList rows);
  void decodedEntriesReady (quint64 serial, QVector<decodium::ft8::DecodedEntry> entries);
  void neuralSyncHit (double score);
  void turboIterations (int itersUsed);

private:
  std::atomic<quint64> m_latestDecodeSerial {0};
  std::atomic<bool> m_shuttingDown {false};
};

}
}

Q_DECLARE_METATYPE (decodium::ft8::DecodedEntry)
Q_DECLARE_METATYPE (QVector<decodium::ft8::DecodedEntry>)

#endif
