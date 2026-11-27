// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecodeUnresolved.h"
#include "Sequencer/MessageTokenRules.hpp"

using decodium::decode_unresolved::messageIsUnresolvedGridOnlyDecode;
using decodium::decode_unresolved::isFst4wUnresolvedHashDecode;
using decodium::seq::decodedDxCallToken;

static QString extractRightCallsign(const QString& msg)
{
    return decodedDxCallToken(msg);
}


bool decodiumBridgeUnresolvedGridOnlyDecode(const QString& message, QString* grid)
{
    return messageIsUnresolvedGridOnlyDecode(message, grid);
}
bool decodiumBridgeFst4wUnresolvedHashDecode(QString const& mode, QString const& message)
{
    return isFst4wUnresolvedHashDecode(mode, message);
}
QString decodiumBridgeExtractRightCallsign(const QString& message)
{
    return extractRightCallsign(message);
}


