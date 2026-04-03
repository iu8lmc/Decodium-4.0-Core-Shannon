// -*- Mode: C++ -*-
#include "Detector/LegacyJtDecodeWorker.hpp"

#include "Detector/JT65Decoder.hpp"
#include "Detector/JT9WideDecoder.hpp"

#include <algorithm>
#include <cstring>

#include <QMutexLocker>

#include "commons.h"
#include "Detector/FortranRuntimeGuard.hpp"

extern "C"
{
  void legacy_jt_async_decode_(int* nmode, float ss[], short id2[], int* npts8, int* nzhsym,
                               int* nutc, int* nfqso, int* ntol, int* ndepth,
                               int* nfa, int* nfb, int* nfsplit, int* nsubmode,
                               int* nclearave, int* minsync, int* minw,
                               float* emedelay, float* dttol, int* newdat, int* nagain,
                               int* n2pass, int* nrobust, int* ntrials, int* naggressive,
                               int* nexp_decode, int* nqsoprogress, int* ljt65apon,
                               char mycall[], char hiscall[], char hisgrid[], char temp_dir[],
                               char outlines[], int* nout,
                               fortran_charlen_t, fortran_charlen_t, fortran_charlen_t,
                               fortran_charlen_t, fortran_charlen_t);
}

namespace
{
  constexpr int kLegacyJtAudioSamples {NTMAX * RX_SAMPLE_RATE};
  constexpr int kLegacyJtSSize {184 * NSMAX};
  constexpr int kLegacyJtMaxLines {200};
  constexpr int kLegacyJtLineChars {80};

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
namespace legacyjt
{

LegacyJtDecodeWorker::LegacyJtDecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void LegacyJtDecodeWorker::decode (DecodeRequest const& request)
{
  if (request.mode == "JT9" && request.nsubmode >= 1 && !request.ss.isEmpty ())
    {
      Q_EMIT decodeReady (request.serial, decodium::jt9wide::decode_wide_jt9 (request));
      return;
    }

  if (request.mode == "JT65")
    {
      QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};
      Q_EMIT decodeReady (request.serial, decodium::jt65::decode_async_jt65 (request, &m_jt65State));
      return;
    }

  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};

  int nmode = 0;
  if (request.mode == "JT4") nmode = 4;
  if (request.mode == "JT9") nmode = 9;
  if (nmode == 0) {
    Q_EMIT decodeReady (request.serial, {});
    return;
  }

  QVector<float> ss (kLegacyJtSSize);
  if (!request.ss.isEmpty ()) {
    std::copy_n (request.ss.constBegin (), std::min (request.ss.size (), ss.size ()), ss.begin ());
  } else {
    std::fill (ss.begin (), ss.end (), 0.0f);
  }

  QVector<short> audio (kLegacyJtAudioSamples);
  int const copyCount = std::min (request.audio.size (), kLegacyJtAudioSamples);
  if (copyCount > 0) {
    std::copy_n (request.audio.constBegin (), copyCount, audio.begin ());
  }

  char outlines[kLegacyJtMaxLines][kLegacyJtLineChars];
  std::memset (outlines, 0, sizeof outlines);

  int npts8 = qMax (0, request.npts8);
  int nzhsym = qMax (0, request.nzhsym);
  int nutc = qMax (0, request.nutc);
  int nfqso = qBound (0, request.nfqso, 5000);
  int ntol = qMax (0, request.ntol);
  int ndepth = qMax (1, request.ndepth);
  int nfa = qBound (0, request.nfa, 5000);
  int nfb = qMax (nfa + 50, qBound (0, request.nfb, 5000));
  int nfsplit = qBound (0, request.nfsplit, 5000);
  int nsubmode = qMax (0, request.nsubmode);
  int nclearave = request.nclearave ? 1 : 0;
  int minsync = request.minsync;
  int minw = request.minw;
  float emedelay = request.emedelay;
  float dttol = request.dttol;
  int newdat = request.newdat ? 1 : 0;
  int nagain = request.nagain ? 1 : 0;
  int n2pass = qMax (1, request.n2pass);
  int nrobust = request.nrobust ? 1 : 0;
  int ntrials = qMax (0, request.ntrials);
  int naggressive = qMax (0, request.naggressive);
  int nexp_decode = request.nexp_decode;
  int nqsoprogress = qBound (0, request.nqsoprogress, 6);
  int ljt65apon = request.ljt65apon ? 1 : 0;
  int nout = 0;

  auto mycall = to_fortran_field (request.mycall, 12);
  auto hiscall = to_fortran_field (request.hiscall, 12);
  auto hisgrid = to_fortran_field (request.hisgrid, 6);
  QByteArray tempDir = request.tempDir;
  if (tempDir.isEmpty ()) {
    tempDir = QByteArray (1, '.');
  }

  legacy_jt_async_decode_ (&nmode, ss.data (), audio.data (), &npts8, &nzhsym,
                           &nutc, &nfqso, &ntol, &ndepth, &nfa, &nfb, &nfsplit,
                           &nsubmode, &nclearave, &minsync, &minw,
                           &emedelay, &dttol, &newdat, &nagain, &n2pass, &nrobust,
                           &ntrials, &naggressive, &nexp_decode, &nqsoprogress,
                           &ljt65apon, mycall.data (), hiscall.data (), hisgrid.data (),
                           tempDir.data (), &outlines[0][0], &nout,
                           static_cast<fortran_charlen_t> (12),
                           static_cast<fortran_charlen_t> (12),
                           static_cast<fortran_charlen_t> (6),
                           static_cast<fortran_charlen_t> (tempDir.size ()),
                           static_cast<fortran_charlen_t> (kLegacyJtMaxLines * kLegacyJtLineChars));

  QStringList rows;
  rows.reserve (qBound (0, nout, kLegacyJtMaxLines));
  for (int i = 0; i < qBound (0, nout, kLegacyJtMaxLines); ++i) {
    QByteArray rowBytes {outlines[i], kLegacyJtLineChars};
    int end = rowBytes.size ();
    while (end > 0) {
      char const c = rowBytes.at (end - 1);
      if (c == '\0' || c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        --end;
        continue;
      }
      break;
    }
    if (end > 0) {
      rows << QString::fromLatin1 (rowBytes.constData (), end);
    }
  }

  Q_EMIT decodeReady (request.serial, rows);
}

}
}
