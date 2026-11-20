#pragma once
#include <QString>

namespace decodium::decode_tokens {
bool isGridToken(QString const&);
bool isTelemetryHexToken(QString const&);
bool looksLikeCallsignToken(QString const&);
}
bool decodiumBridgeLooksLikeCallsignToken(QString const&);
bool decodiumBridgeIsGridToken(QString const&);
