// -*- Mode: C++ -*-
#ifndef FT2DECODEWORKER_HPP
#define FT2DECODEWORKER_HPP

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <memory>

#include "Detector/Ft2AsyncSottrazione.hpp"


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
  // FT2 asincrono F3: intervallo di ibest (campioni a 1333,33 Hz
  // dall'inizio della finestra) dei frame completati dall'ultimo giro.
  // ibHi < ibLo = ricerca completa di sempre.
  int ibLo {0};
  int ibHi {-1};
  // FT2 asincrono F5: la risposta attesa del corrispondente, a
  // frequenza nota e con l'inizio in [expectLo, expectHi] (stessa scala di
  // ibLo/ibHi). expectHi < expectLo = nessuna attesa.
  float expectF {0.0f};
  int expectLo {0};
  int expectHi {-1};
  // FT2 asincrono F4: posizione assoluta (campioni dal via del ring)
  // del campione dopo l'ultimo della finestra; -1 = sconosciuta, F4 spento.
  qint64 audioEnd {-1};
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
  // FT2 asincrono F4: segnali gia' decodificati da sottrarre alle
  // finestre successive (solo nel thread del worker).
  std::unique_ptr<AsyncSottrazione> m_sottrazione;
};

}
}

#endif
