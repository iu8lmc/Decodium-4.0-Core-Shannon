// -*- Mode: C++ -*-
#ifndef FT8DECODEWORKER_HPP
#define FT8DECODEWORKER_HPP

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QVector>

namespace decodium
{
namespace ft8
{

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
  float emedelay {0.0f};
  int ncontest {0};
  int nagain {0};
  int lft8apon {0};
  int lapcqonly {0};
  int napwid {50};
  int ldiskdat {0};
  QByteArray mycall;
  QByteArray hiscall;
  QByteArray hisgrid;
};

class FT8DecodeWorker final : public QObject
{
  Q_OBJECT

public:
  explicit FT8DecodeWorker (QObject * parent = nullptr);

  void decode (DecodeRequest const& request);

Q_SIGNALS:
  void decodeReady (quint64 serial, QStringList rows);
};

}
}

#endif
