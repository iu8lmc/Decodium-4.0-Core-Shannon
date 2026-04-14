// -*- Mode: C++ -*-
#include "Detector/JT9FastDecodeWorker.hpp"

#include <algorithm>

#include "commons.h"

namespace
{
  constexpr int kFastSampleCount {360000};

  QByteArray to_fortran_field (QByteArray value, int width)
  {
    value = value.left (width);
    if (value.size () < width)
      {
        value.append (QByteArray (width - value.size (), ' '));
      }
    return value;
  }
}

namespace decodium
{
namespace jt9fast
{

JT9FastDecodeWorker::JT9FastDecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void JT9FastDecodeWorker::decode (DecodeRequest const& request)
{
  DecodeRequest bounded = request;
  if (bounded.audio.size () > kFastSampleCount)
    {
      bounded.audio.resize (kFastSampleCount);
    }
  bounded.nutc = qMax (0, bounded.nutc);
  bounded.kdone = qBound (0, bounded.kdone, kFastSampleCount);
  bounded.nsubmode = qBound (0, bounded.nsubmode, 8);
  bounded.newdat = bounded.newdat ? 1 : 0;
  bounded.npick = qBound (0, bounded.npick, 2);
  bounded.t0_ms = qMax (0, bounded.t0_ms);
  bounded.t1_ms = qMax (bounded.t0_ms, bounded.t1_ms);
  bounded.maxlines = qBound (1, bounded.maxlines, 50);
  bounded.rxfreq = qBound (0, bounded.rxfreq, 5000);
  bounded.ftol = qMax (0, bounded.ftol);
  bounded.aggressive = qBound (0, bounded.aggressive, 10);
  bounded.mycall = to_fortran_field (bounded.mycall, 12);
  bounded.hiscall = to_fortran_field (bounded.hiscall, 12);

  QStringList const rows = decode_fast9 (bounded, &state_);
  Q_EMIT decodeReady (request.serial, rows);
}

}
}
