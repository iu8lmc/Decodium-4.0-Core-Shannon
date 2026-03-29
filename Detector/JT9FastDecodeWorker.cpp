// -*- Mode: C++ -*-
#include "Detector/JT9FastDecodeWorker.hpp"

#include <algorithm>
#include <cstring>

#include <QMutexLocker>

#include "commons.h"
#include "Detector/FortranRuntimeGuard.hpp"

extern "C"
{
  void fast_decode_(short id2[], int narg[], double * trperiod,
                    char msg[], char mycall[], char hiscall[],
                    fortran_charlen_t, fortran_charlen_t, fortran_charlen_t);
}

namespace
{
  constexpr int kFastSampleCount {360000};
  constexpr int kFastMaxLines {100};
  constexpr int kFastLineChars {80};

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
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};

  short int audio[kFastSampleCount] {};
  int const copyCount = std::min (request.audio.size (), kFastSampleCount);
  if (copyCount > 0)
    {
      std::copy_n (request.audio.constBegin (), copyCount, audio);
    }

  int narg[15] {};
  narg[0] = qMax (0, request.nutc);
  narg[1] = qBound (0, request.kdone, kFastSampleCount);
  narg[2] = qBound (0, request.nsubmode, 8);
  narg[3] = request.newdat ? 1 : 0;
  narg[4] = request.minsync;
  narg[5] = qBound (0, request.npick, 2);
  narg[6] = qMax (0, request.t0_ms);
  narg[7] = qMax (narg[6], request.t1_ms);
  narg[8] = qBound (1, request.maxlines, 50);
  narg[9] = 102;
  narg[10] = qBound (0, request.rxfreq, 5000);
  narg[11] = qMax (0, request.ftol);
  narg[12] = 0;
  narg[13] = -1;
  narg[14] = qBound (0, request.aggressive, 10);

  char outlines[kFastMaxLines][kFastLineChars];
  std::memset (outlines, 0, sizeof outlines);

  auto mycall = to_fortran_field (request.mycall, 12);
  auto hiscall = to_fortran_field (request.hiscall, 12);
  double trperiod = request.trperiod;

  fast_decode_ (audio, narg, &trperiod,
                &outlines[0][0], mycall.data (), hiscall.data (),
                static_cast<fortran_charlen_t> (8000),
                static_cast<fortran_charlen_t> (12),
                static_cast<fortran_charlen_t> (12));

  QStringList rows;
  rows.reserve (kFastMaxLines);
  for (int i = 0; i < kFastMaxLines; ++i)
    {
      if (outlines[i][0] == '\0')
        {
          break;
        }

      QByteArray rowBytes {outlines[i], kFastLineChars};
      int end = rowBytes.size ();
      while (end > 0)
        {
          char const c = rowBytes.at (end - 1);
          if (c == '\0' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
            {
              --end;
              continue;
            }
          break;
        }

      if (end > 0)
        {
          rows << QString::fromLatin1 (rowBytes.constData (), end);
        }
    }

  Q_EMIT decodeReady (request.serial, rows);
}

}
}
