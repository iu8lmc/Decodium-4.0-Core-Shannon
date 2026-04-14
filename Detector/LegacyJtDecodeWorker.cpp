// -*- Mode: C++ -*-
#include "Detector/LegacyJtDecodeWorker.hpp"

#include "Detector/JT4Decoder.hpp"
#include "Detector/JT65Decoder.hpp"
#include "Detector/JT9NarrowDecoder.hpp"
#include "Detector/JT9WideDecoder.hpp"

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
      // Fully C++ — no Fortran runtime lock needed
      Q_EMIT decodeReady (request.serial, decodium::jt65::decode_async_jt65 (request, &m_jt65State));
      return;
    }

  if (request.mode == "JT4")
    {
      // Fully C++ — no Fortran runtime lock needed
      Q_EMIT decodeReady (request.serial,
                          decodium::jt4::decode_async_jt4 (request, &m_jt4State));
      return;
    }

  if (request.mode == "JT9")
    {
      // Fully C++ — no Fortran runtime lock needed
      Q_EMIT decodeReady (request.serial,
                          decodium::jt9narrow::decode_async_jt9_narrow (request, &m_jt9NarrowState));
      return;
    }

  Q_EMIT decodeReady (request.serial, {});
}

}
}
