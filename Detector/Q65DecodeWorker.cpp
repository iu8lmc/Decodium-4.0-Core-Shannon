// -*- Mode: C++ -*-
#include "Detector/Q65DecodeWorker.hpp"
#include "wsjtx_config.h"

#include <algorithm>

#include "commons.h"
extern "C"
{
  void ftx_q65_async_decode_c (short const* iwave, int* nqd0, int* nutc, int* ntrperiod,
                               int* nsubmode, int* nfqso, int* ntol, int* ndepth,
                               int* nfa, int* nfb, int* nclearave, int* single_decode,
                               int* nagain, int* max_drift, int* lnewdat, float* emedelay,
                               char const* mycall, char const* hiscall, char const* hisgrid,
                               int* nqsoprogress, int* ncontest, int* lapcqonly,
                               int syncsnrs[], int snrs[], float dts[], float freqs[],
                               int idecs[], int nuseds[], signed char bits77[],
                               char decodeds[], int* nout,
                               fortran_charlen_t, fortran_charlen_t,
                               fortran_charlen_t, fortran_charlen_t);
}

namespace
{
  constexpr int kQ65MaxLines {200};
  constexpr int kBitsPerMessage {77};
  constexpr int kDecodedChars {37};

  QString format_decode_utc (int nutc)
  {
    if (nutc <= 0)
      {
        return QString {};
      }
    int const width = nutc > 9999 ? 6 : 4;
    return QString::number (nutc).rightJustified (width, QLatin1Char {'0'});
  }

  QByteArray to_fortran_field (QByteArray value, int width)
  {
    value = value.left (width);
    if (value.size () < width)
      {
        value.append (QByteArray (width - value.size (), ' '));
      }
    return value;
  }

  QString decode_fallback (char const* decodeds, int index)
  {
    QByteArray fallback {decodeds + index * kDecodedChars, kDecodedChars};
    int end = fallback.size ();
    while (end > 0 && (fallback.at (end - 1) == ' ' || fallback.at (end - 1) == '\0'))
      {
        --end;
      }
    QString decoded = QString::fromLatin1 (fallback.constData (), end);
    if (decoded.size () < kDecodedChars)
      {
        decoded = decoded.leftJustified (kDecodedChars, QLatin1Char {' '});
      }
    return decoded.left (kDecodedChars);
  }

  QString build_row (QString const& utcPrefix, int snr, float dt, float freq,
                     int idec, int nused, char const* decodeds, int index)
  {
    QString decoded = decode_fallback (decodeds, index);
    if (decoded.size () < kDecodedChars)
      {
        decoded = decoded.leftJustified (kDecodedChars, QLatin1Char {' '});
      }
    decoded = decoded.left (kDecodedChars);

    QString cflags = QStringLiteral ("q  ");
    if (idec >= 0)
      {
        cflags[1] = QLatin1Char ('0' + qBound (0, idec, 9));
        if (nused >= 2)
          {
            cflags[2] = QLatin1Char ('0' + qBound (0, nused, 9));
          }
      }
    else
      {
        cflags = QStringLiteral ("   ");
      }

    QString row = QStringLiteral ("%1%2%3 :  %4 %5")
        .arg (snr, 4)
        .arg (dt, 5, 'f', 1)
        .arg (qRound (freq), 5)
        .arg (decoded)
        .arg (cflags);
    if (!utcPrefix.isEmpty ())
      {
        row.prepend (utcPrefix);
      }
    return row;
  }

  QStringList build_rows (QString const& utcPrefix, int nout,
                          int const* snrs, float const* dts, float const* freqs,
                          int const* idecs, int const* nuseds, char const* decodeds)
  {
    QStringList rows;
    rows.reserve (qBound (0, nout, kQ65MaxLines));
    for (int i = 0; i < qBound (0, nout, kQ65MaxLines); ++i)
      {
        rows << build_row (utcPrefix, snrs[i], dts[i], freqs[i], idecs[i], nuseds[i],
                           decodeds, i);
      }
    return rows;
  }
}

namespace decodium
{
namespace q65
{

Q65DecodeWorker::Q65DecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void Q65DecodeWorker::decode (DecodeRequest const& request)
{
  QVector<short> localAudio = request.audio;
  int syncsnrs[kQ65MaxLines] {};
  int snrs[kQ65MaxLines] {};
  float dts[kQ65MaxLines] {};
  float freqs[kQ65MaxLines] {};
  int idecs[kQ65MaxLines] {};
  int nuseds[kQ65MaxLines] {};
  signed char bits77[kQ65MaxLines * kBitsPerMessage] {};
  char decodeds[kQ65MaxLines * kDecodedChars] {};

  int nqd0 = qMax (1, request.nqd0);
  int nutc = qMax (0, request.nutc);
  int ntrperiod = qBound (15, request.ntrperiod, 300);
  int nsubmode = qBound (0, request.nsubmode, 4);
  int nfqso = qBound (0, request.nfqso, 5000);
  int ntol = qMax (0, request.ntol);
  int ndepth = qMax (1, request.ndepth);
  int nfa = qBound (0, request.nfa, 5000);
  int nfb = qMax (nfa + 50, qBound (0, request.nfb, 5000));
  int nclearave = request.nclearave ? 1 : 0;
  int single_decode = request.single_decode ? 1 : 0;
  int nagain = request.nagain ? 1 : 0;
  int max_drift = qMax (0, request.max_drift);
  int lnewdat = request.lnewdat ? 1 : 0;
  float emedelay = request.emedelay;
  int nqsoprogress = qBound (0, request.nqsoprogress, 6);
  int ncontest = qBound (0, request.ncontest, 16);
  int lapcqonly = request.lapcqonly ? 1 : 0;
  int nout = 0;

  auto mycall = to_fortran_field (request.mycall, 12);
  auto hiscall = to_fortran_field (request.hiscall, 12);
  auto hisgrid = to_fortran_field (request.hisgrid, 6);

  ftx_q65_async_decode_c (localAudio.constData (), &nqd0, &nutc, &ntrperiod, &nsubmode,
                     &nfqso, &ntol, &ndepth, &nfa, &nfb, &nclearave,
                     &single_decode, &nagain, &max_drift, &lnewdat,
                     &emedelay, mycall.constData (), hiscall.constData (), hisgrid.constData (),
                     &nqsoprogress, &ncontest, &lapcqonly,
                     &syncsnrs[0],
                     &snrs[0], &dts[0], &freqs[0], &idecs[0], &nuseds[0],
                     &bits77[0], &decodeds[0], &nout,
                     static_cast<fortran_charlen_t> (12),
                     static_cast<fortran_charlen_t> (12),
                     static_cast<fortran_charlen_t> (6),
                     static_cast<fortran_charlen_t> (kQ65MaxLines * kDecodedChars));

  QString const utcPrefix = format_decode_utc (request.nutc);
  Q_EMIT decodeReady (request.serial, build_rows (utcPrefix, nout, snrs, dts, freqs, idecs,
                                                  nuseds, decodeds));
}

}
}
