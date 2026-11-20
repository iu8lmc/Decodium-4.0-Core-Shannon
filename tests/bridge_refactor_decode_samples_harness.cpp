#include <QtCore>
#include <cstdio>
class DecodiumBridge {public:static constexpr int SAMPLE_RATE=12000;int effectivePeriodMsForMode(const QString&m)const{if(m=="FT8")return 15000;if(m=="FT4")return 7500;if(m.startsWith("Q65")||m.startsWith("FST4"))return 60000;if(m=="WSPR")return 120000;if(m.startsWith("JT"))return 60000;return 0;}int minimumDecodeSamplesForMode(const QString&)const;int targetDecodeSamplesForMode(const QString&)const;};
#include "bridge_refactor_decode_samples.inc"
static void check(bool v,const char*s){if(!v){fprintf(stderr,"FAIL %s\n",s);exit(1);}puts(s);}
int main(){DecodiumBridge b;check(b.minimumDecodeSamplesForMode(" ft8 ")==144000&&b.targetDecodeSamplesForMode("FT8")==180000,"ft8");check(b.minimumDecodeSamplesForMode("ft4")==60000&&b.targetDecodeSamplesForMode("FT4")==90000,"ft4");check(b.minimumDecodeSamplesForMode("Q65-60A")==576000&&b.minimumDecodeSamplesForMode("FST4-60")==576000,"q65-fst4");check(b.minimumDecodeSamplesForMode("WSPR")==1320000&&b.targetDecodeSamplesForMode("WSPR")==1440000,"wspr");check(b.minimumDecodeSamplesForMode("JT65")==624000&&b.minimumDecodeSamplesForMode("unknown")==0&&b.targetDecodeSamplesForMode("unknown")==0,"jt-unknown");}
