#include <QtCore>
#include <algorithm>
#include <limits>
#include <utility>
#include "DecodeUtcTime.h"
#include "bridge_refactor_under_test.inc"
int main(){QTextStream o(stdout);QDateTime now(QDate(2026,11,7),QTime(0,0,10),Qt::UTC);QVariantMap before{{"time","235959"}},after{{"time","000005"}},fixed{{"time","000005"},{"timestamp",1000}},tx{{"time","000005"},{"timestamp",1000},{"isTx",true}};o<<"midnight|"<<(decodeDisplaySortMsecs(before,now)<decodeDisplaySortMsecs(after,now))<<'|'<<(decodeDisplaySortMsecs(fixed,now)==1000)<<'|'<<(decodeDisplaySortMsecs(tx,now)!=1000)<<'\n';QVariantList rows{QVariantMap{{"timestamp",2000},{"freq","2000"},{"message","Z"}},QVariantMap{{"timestamp",1000},{"freq","1600"},{"message","B"}},QVariantMap{{"timestamp",1000},{"freq","1500"},{"message","A"}},QVariantMap{{"timestamp",1000},{"freq","1500"},{"message","A"}}};sortDecodeEntriesChronologically(rows);o<<"sort|";for(auto r:rows)o<<r.toMap().value("message").toString();o<<'|'<<rows[0].toMap().value("freq").toString()<<'\n';}
