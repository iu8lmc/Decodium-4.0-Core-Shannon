// -*- Mode: C++ -*-
#include "Detector/FST4DecodeWorker.hpp"

#include <algorithm>

#include <QMutexLocker>

#include "commons.h"
#include "Detector/FortranRuntimeGuard.hpp"
extern "C"
{
  void fst4_async_decode_(short iwave[], int* nutc, int* nqsoprogress, int* nfa, int* nfb,
                          int* nfqso, int* ndepth, int* ntrperiod, int* nexp_decode,
                          int* ntol, float* emedelay, int* nagain, int* lapcqonly,
                          char mycall[], char hiscall[], int* iwspr, int* lprinthash22,
                          int snrs[], float dts[], float freqs[], int naps[], float quals[],
                          signed char bits77[], char decodeds[], float fmids[],
                          float w50s[], int* nout,
                          fortran_charlen_t, fortran_charlen_t,
                          fortran_charlen_t);
}

namespace
{
  constexpr int kFst4MaxLines {200};
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
                     int nap, float qual, char const* decodeds, int index,
                     int ntrperiod, float fmid, float w50)
  {
    QString decoded = decode_fallback (decodeds, index);
    if (decoded.size () < kDecodedChars)
      {
        decoded = decoded.leftJustified (kDecodedChars, QLatin1Char {' '});
      }
    decoded = decoded.left (kDecodedChars);
    if (nap != 0 && qual < 0.17f && decoded.size () >= kDecodedChars)
      {
        decoded[kDecodedChars - 1] = QLatin1Char {'?'};
      }

    QString const annot = nap != 0
        ? QStringLiteral ("a%1").arg (nap).leftJustified (2, QLatin1Char {' '})
        : QStringLiteral ("  ");

    QString row = QStringLiteral ("%1%2%3 `  %4 %5")
        .arg (snr, 4)
        .arg (dt, 5, 'f', 1)
        .arg (qRound (freq), 5)
        .arg (decoded)
        .arg (annot);
    if (ntrperiod >= 60 && fmid != -999.0f)
      {
        row += QStringLiteral ("%1").arg (w50, 6, 'f', w50 < 0.95f ? 3 : 2);
      }
    if (!utcPrefix.isEmpty ())
      {
        row.prepend (utcPrefix);
      }
    return row;
  }

  QStringList build_rows (QString const& utcPrefix, int nout,
                          int const* snrs, float const* dts, float const* freqs,
                          int const* naps, float const* quals, char const* decodeds,
                          float const* fmids, float const* w50s, int ntrperiod)
  {
    QStringList rows;
    rows.reserve (qBound (0, nout, kFst4MaxLines));
    for (int i = 0; i < qBound (0, nout, kFst4MaxLines); ++i)
      {
        rows << build_row (utcPrefix, snrs[i], dts[i], freqs[i], naps[i], quals[i],
                           decodeds, i, ntrperiod, fmids[i], w50s[i]);
      }
    return rows;
  }
}

namespace decodium
{
namespace fst4
{

FST4DecodeWorker::FST4DecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void FST4DecodeWorker::decode (DecodeRequest const& request)
{
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};

  QVector<short> localAudio = request.audio;
  int snrs[kFst4MaxLines] {};
  float dts[kFst4MaxLines] {};
  float freqs[kFst4MaxLines] {};
  int naps[kFst4MaxLines] {};
  float quals[kFst4MaxLines] {};
  signed char bits77[kFst4MaxLines * kBitsPerMessage] {};
  char decodeds[kFst4MaxLines * kDecodedChars] {};
  float fmids[kFst4MaxLines] {};
  float w50s[kFst4MaxLines] {};

  int nutc = qMax (0, request.nutc);
  int nqsoprogress = qBound (0, request.nqsoprogress, 6);
  int nfa = qBound (0, request.nfa, 5000);
  int nfb = qMax (nfa + 50, qBound (0, request.nfb, 5000));
  int nfqso = qBound (0, request.nfqso, 5000);
  int ndepth = qBound (1, request.ndepth, 3);
  int ntrperiod = qBound (15, request.ntrperiod, 1800);
  int nexp_decode = request.nexp_decode;
  int ntol = qMax (0, request.ntol);
  float emedelay = request.emedelay;
  int nagain = request.nagain ? 1 : 0;
  int lapcqonly = request.lapcqonly ? 1 : 0;
  int iwspr = request.iwspr ? 1 : 0;
  int lprinthash22 = request.lprinthash22 ? 1 : 0;
  int nout = 0;

  auto mycall = to_fortran_field (request.mycall, 12);
  auto hiscall = to_fortran_field (request.hiscall, 12);

  fst4_async_decode_ (localAudio.data (), &nutc, &nqsoprogress, &nfa, &nfb,
                      &nfqso, &ndepth, &ntrperiod, &nexp_decode, &ntol,
                      &emedelay, &nagain, &lapcqonly, mycall.data (),
                      hiscall.data (), &iwspr, &lprinthash22,
                      &snrs[0], &dts[0], &freqs[0], &naps[0], &quals[0],
                      &bits77[0], &decodeds[0], &fmids[0], &w50s[0], &nout,
                      static_cast<fortran_charlen_t> (12),
                      static_cast<fortran_charlen_t> (12),
                      static_cast<fortran_charlen_t> (kFst4MaxLines * kDecodedChars));

  QString const utcPrefix = format_decode_utc (request.nutc);
  Q_EMIT decodeReady (request.serial, build_rows (utcPrefix, nout, snrs, dts, freqs, naps, quals,
                                                  decodeds, fmids, w50s, request.ntrperiod));
}

}
}
