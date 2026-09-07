// -*- Mode: C++ -*-
#include "Detector/LegacyJtDecodeWorker.hpp"

#include "Detector/JT4Decoder.hpp"
#include "Detector/JT65Decoder.hpp"
#include "Detector/JT9NarrowDecoder.hpp"
#include "Detector/JT9WideDecoder.hpp"

#include <QDebug>
#include <QSet>

namespace decodium
{
namespace legacyjt
{
namespace
{
constexpr int kJtLongSampleRate = 12000;
constexpr int kJt4DecodeSamples = 52 * kJtLongSampleRate;

bool legacy_jt_row_has_payload (QString const& row)
{
  QStringList const fields = row.simplified ().split (' ', Qt::SkipEmptyParts);
  return fields.size () >= 8;
}

bool legacy_jt_rows_have_payload (QStringList const& rows)
{
  for (QString const& row : rows)
    if (legacy_jt_row_has_payload (row))
      return true;
  return false;
}

QVector<short> jt4_candidate_window (QVector<short> const& audio, int offsetSamples)
{
  QVector<short> window (kJt4DecodeSamples, 0);
  if (offsetSamples < 0 || offsetSamples >= audio.size ())
    return window;

  int const available = qMin (kJt4DecodeSamples, audio.size () - offsetSamples);
  for (int i = 0; i < available; ++i)
    window[i] = audio[offsetSamples + i];
  return window;
}
}

LegacyJtDecodeWorker::LegacyJtDecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void LegacyJtDecodeWorker::decode (DecodeRequest const& request)
{
  bool const trace = qEnvironmentVariableIsSet ("DECODIUM_JT9_TRACE");
  if (trace && request.mode == "JT9")
    {
      qInfo () << "[LEGACY-JT9] worker start"
               << "serial=" << request.serial
               << "audio=" << request.audio.size ()
               << "npts8=" << request.npts8
               << "nzhsym=" << request.nzhsym
               << "newdat=" << request.newdat
               << "ss=" << request.ss.size ()
               << "nfqso=" << request.nfqso
               << "range=" << request.nfa << "-" << request.nfb;
    }

  if (request.mode == "JT9" && request.nsubmode >= 1 && !request.ss.isEmpty ())
    {
      auto const rows = decodium::jt9wide::decode_wide_jt9 (request);
      if (trace)
        {
          qInfo () << "[LEGACY-JT9] worker done"
                   << "serial=" << request.serial << "rows=" << rows.size () << "path=wide";
        }
      Q_EMIT decodeReady (request.serial, rows);
      return;
    }

  if (request.mode == "JT65")
    {
      // Fully C++ — no Fortran runtime lock needed
      Q_EMIT decodeReady (request.serial, decodium::jt65::decode_async_jt65 (request, &m_jt65State));
      return;
    }

  if (request.mode == "JT4")
    {
      // Fully C++ — no Fortran runtime lock needed
      QStringList rows = decodium::jt4::decode_async_jt4 (request, &m_jt4State);
      if (!legacy_jt_rows_have_payload (rows)
          && request.audio.size () > kJt4DecodeSamples + 2 * kJtLongSampleRate)
        {
          QSet<QString> seen;
          for (QString const& row : std::as_const (rows))
            seen.insert (row);

          // JT4 on-air and virtual-audio loopback can arrive a few seconds off
          // the ideal 52 s decoder aperture. Preserve the normal averaged pass,
          // then run short acquisition-only windows without polluting state.
          static constexpr int kOffsetsSeconds[] {2, 4, 6, 8};
          for (int seconds : kOffsetsSeconds)
            {
              DecodeRequest candidate = request;
              candidate.audio = jt4_candidate_window (request.audio, seconds * kJtLongSampleRate);
              candidate.newdat = 1;
              decodium::jt4::AverageState scratch;
              QStringList const candidateRows =
                  decodium::jt4::decode_async_jt4 (candidate, &scratch);
              bool addedPayload = false;
              for (QString const& row : candidateRows)
                {
                  if (seen.contains (row))
                    continue;
                  seen.insert (row);
                  rows.append (row);
                  addedPayload = addedPayload || legacy_jt_row_has_payload (row);
                }
              if (addedPayload)
                break;
            }
        }
      Q_EMIT decodeReady (request.serial, rows);
      return;
    }

  if (request.mode == "JT9")
    {
      // Fully C++ — no Fortran runtime lock needed
      auto const rows = decodium::jt9narrow::decode_async_jt9_narrow (request, &m_jt9NarrowState);
      if (trace)
        {
          qInfo () << "[LEGACY-JT9] worker done"
                   << "serial=" << request.serial << "rows=" << rows.size () << "path=narrow";
        }
      Q_EMIT decodeReady (request.serial, rows);
      return;
    }

  Q_EMIT decodeReady (request.serial, {});
}

}
}
