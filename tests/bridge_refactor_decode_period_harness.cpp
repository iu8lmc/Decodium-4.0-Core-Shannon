#include <QtCore>
#include <cstdio>
struct Legacy {bool ok=false;QString md="";int period=15;bool available()const{return ok;}QString mode()const{return md;}int trPeriodSeconds()const{return period;}};
class DecodiumBridge {public:Legacy*m_legacyBackend=nullptr;QString m_mode="FT8";static int periodMsForMode(const QString&m){if(m=="FT8")return 15000;if(m=="FT4")return 7500;if(m=="MSK144")return 15000;return 60000;}int effectivePeriodMsForMode(const QString&)const;int periodMilliseconds()const;};
#include "bridge_refactor_decode_period.inc"
static void check(bool v,const char*s){if(!v){fprintf(stderr,"FAIL %s\n",s);exit(1);}puts(s);}
int main(){DecodiumBridge b;check(b.periodMilliseconds()==15000&&b.effectivePeriodMsForMode(" ft4 ")==7500,"standard");Legacy l;l.ok=true;l.md=" msk144 ";b.m_legacyBackend=&l;for(int v:{5,10,15,30}){l.period=v;check(b.effectivePeriodMsForMode("MSK144")==v*1000,"msk-override");}l.period=7;check(b.effectivePeriodMsForMode("MSK144")==15000,"invalid-override");l.ok=false;l.period=5;check(b.effectivePeriodMsForMode("MSK144")==15000,"unavailable");l.ok=true;l.md="FT8";check(b.effectivePeriodMsForMode("MSK144")==15000,"different-mode");}
