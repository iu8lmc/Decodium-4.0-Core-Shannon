#pragma once
#include <QString>

namespace decodium::decode_unresolved {
bool messageIsUnresolvedGridOnlyDecode(QString const&, QString* = nullptr);
QString displayMessageWithCallsignsActorFirst(QString const&, bool, QString const&);
bool messageHasUnresolvedPeerPlaceholder(QString const&, QString const&);
bool isFst4wUnresolvedHashDecode(QString const&, QString const&);
}
