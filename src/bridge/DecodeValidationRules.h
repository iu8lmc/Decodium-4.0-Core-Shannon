#pragma once
#include <QString>
#include <QStringList>
namespace decodium::decode_validation {
bool isPlainSignalReportToken(QString const&);
bool isRogerSignalReportToken(QString const&);
bool isQsoSignoffToken(QString const&);
bool isDirectedDecodeTerminalToken(QString const&);
bool looksLikeMalformedRGrid(QString const&);
bool hasPortableOperatingDesignator(QString const&);
bool isPlausibleDecodedCallsign(QString const&);
bool isSyntheticLabCallsign(QString const&);
int digitRunCount(QString const&);
bool isPlausibleFt2CqCallsign(QString const&);
bool isCqRoutingModifierToken(QString const&);
bool isValidFt2CqTrailingPayload(QStringList const&, int, QString*);
bool hasHighConfidenceGhostPrefix(QString const&);
bool directedPeerLooksStructurallyGhost(QString const&);
bool isDirectedDecodePayloadValid(QStringList const&, bool, QString*);
bool isResolvablePlaceholderDirectedPayload(QStringList const&, bool);
bool decodeListModel_isTelemetryHexToken(QString const&);
bool decodeListModel_isTelemetryOnlyMessage(QString const&);
bool isValidDecode(QString const&, int, QString const&, float);
}

bool decodiumBridgeDecodeTelemetryOnly(QString const&);
