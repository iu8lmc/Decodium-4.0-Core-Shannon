// -*- Mode: C++ -*-
#ifndef WSPRDECODEWORKER_HPP
#define WSPRDECODEWORKER_HPP

#include <QObject>
#include <QStringList>

namespace decodium
{
namespace wspr
{

struct DecodeRequest
{
  quint64 serial {0};
  QStringList arguments;
};

class WSPRDecodeWorker final : public QObject
{
  Q_OBJECT

public:
  explicit WSPRDecodeWorker (QObject * parent = nullptr);

  void decode (DecodeRequest const& request);

Q_SIGNALS:
  void decodeReady (quint64 serial, QStringList rows, QStringList diagnostics, int exitCode);
};

}
}

#endif
