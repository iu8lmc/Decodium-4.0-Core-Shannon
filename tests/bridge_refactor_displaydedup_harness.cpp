#include <QtCore>
#include "DecodeUtcTime.h"
#include "DecodeChronology.h"
#include "DecodeMessageNormalization.h"
using decodium::decode_message::canonicalDecodeMessage;
#include "bridge_refactor_under_test.inc"
int main(){QVariantList entries{QVariantMap{{"mode","FT8"},{"time","120001"},{"freq","1501"},{"message","CQ K1ABC FN42"},{"db","-20"}},QVariantMap{{"mode","FT8"},{"time","120005"},{"freq","1504"},{"message","CQ K1ABC FN42"},{"db","-10"},{"dxCountry","USA"}},QVariantMap{{"mode","FT8"},{"time","120005"},{"freq","1504"},{"message","CQ K1ABC FN42"},{"isTx",true}},QVariantMap{{"mode","FT8"},{"time","120020"},{"freq","1500"},{"message","CQ K1ABC FN42"}}};coalesceDecodeEntriesForDisplay(entries,"FT8");QTextStream o(stdout);o<<"coalesce|"<<entries.size()<<'|'<<entries[0].toMap().value("dxCountry").toString()<<'|'<<entries[1].toMap().value("isTx").toBool()<<'\n';o<<"slots|"<<displayDedupPeriodMsForMode("FT8")<<'|'<<displayDedupPeriodMsForMode("ft4")<<'|'<<displayDedupPeriodMsForMode("FT2")<<'|'<<displayDedupPeriodMsForMode("CW")<<'\n';o<<"frequency|"<<decodeDisplayFrequencyKey("1504","FT8")<<'|'<<decodeDisplayFrequencyKey("1504","CW")<<'\n';}
