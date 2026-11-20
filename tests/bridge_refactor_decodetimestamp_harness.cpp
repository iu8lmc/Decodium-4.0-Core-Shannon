#include <QtCore>
#include <utility>
#include "DecodeUtcTime.h"
#include "bridge_refactor_under_test.inc"
int main(){QTextStream o(stdout);QDateTime now=QDateTime::currentDateTimeUtc();QString token=now.time().toString("HHmmss");QDateTime value=approxUtcDateTimeForDisplayToken(token);o<<"valid|"<<value.isValid()<<'|'<<(value.time().toString("HHmmss")==token)<<'|'<<(qAbs(value.secsTo(now))<3)<<'\n';o<<"fallback|"<<(decodeEntryTimestampMsecs("",12345)==12345)<<'|'<<(qAbs(decodeEntryTimestampMsecs(token,0)-now.toMSecsSinceEpoch())<3000)<<'\n';}
