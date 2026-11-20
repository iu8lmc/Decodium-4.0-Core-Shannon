#pragma once

#include <QString>

namespace decodium
{
namespace network
{
inline QString normalizedUdpClientId(QString id,
                                     QString const& fallback = QStringLiteral ("Decodium"))
{
  id = id.simplified ();
  if (id.isEmpty ())
    {
      // 1.0.538 iu8lmc - ogni destinazione UDP porta il proprio
      // identificativo. Con "WSJTX" i collettori leggevano la nostra
      // versione (1.0.x) come se fosse quella di WSJT-X, la confrontavano
      // con il 2.7.x e scartavano ogni pacchetto con "OLD software
      // version"; alcuni programmi locali pretendono pero' proprio quel
      // nome. Il chiamante decide il ripiego, e i comandi in arrivo
      // indirizzati a "WSJTX"/"WSJT-X" restano accettati come alias.
      id = fallback;
    }
  if (id.size () > 64)
    {
      id = id.left (64).trimmed ();
    }
  return id;
}
}
}
