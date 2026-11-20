#pragma once
#include <QString>
#include <QDateTime>
#include <utility>
QString decodiumBridgeNormalizeRemoteActivityTime(QString);
int decodiumBridgeSecondsFromUtcDisplayToken(QString);
int decodiumBridgeSignedUtcSecondDelta(int, int);
inline QString normalizeUtcDisplayToken(QString raw) { return decodiumBridgeNormalizeRemoteActivityTime(std::move(raw)); }
inline int secondsFromUtcDisplayToken(QString raw) { return decodiumBridgeSecondsFromUtcDisplayToken(std::move(raw)); }
inline int signedUtcSecondDelta(int a, int b) { return decodiumBridgeSignedUtcSecondDelta(a, b); }
QDateTime decodiumBridgeApproxUtcDateTimeForDisplayToken(QString const&);
qint64 decodiumBridgeDecodeEntryTimestampMsecs(QString const&, qint64);
inline QDateTime approxUtcDateTimeForDisplayToken(QString const& token) { return decodiumBridgeApproxUtcDateTimeForDisplayToken(token); }
inline qint64 decodeEntryTimestampMsecs(QString const& token, qint64 fallback) { return decodiumBridgeDecodeEntryTimestampMsecs(token, fallback); }
int utcTokenForSlotStart(qint64, int);
QString utcDisplayTokenForSlotStart(qint64, int);
