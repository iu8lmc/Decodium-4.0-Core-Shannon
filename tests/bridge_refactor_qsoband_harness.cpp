#include <QtCore>
#include <cstdio>
#include "Radio.hpp"
constexpr int kMamRecentWorkedWindowSeconds {600};
struct BandManager {QString band;QString currentBandLambda()const{return band;}};
class DecodiumBridge {public:
 double m_frequency=14074000;BandManager* m_bandManager=nullptr;QString m_mode="FT8";int m_maxQsoPerCall=0;QDate m_qsoCountDay;QHash<QString,int> m_qsoCountByKey;QHash<QString,QDateTime> m_recentAutoCqWorkedUtcByKey,m_recentQsoLogUtcByKey;
 QString autoCqBandKeyForFrequency(double)const;bool qsoLimitReached(QString const&,double,QString const&)const;bool isRecentMamWorkedOrLoggedDuplicate(QString const&,double,QString const&)const;
};
#include "bridge_refactor_qsoband.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;BandManager manager;manager.band=" 23cm ";
 for(double f:{-1.,1799999.,1800000.,2000000.,2000001.,3500000.,14000000.,14350000.,14350001.,144000000.,450000000.,999999999.}){printf("band=%.0f:%s\n",f,b.autoCqBandKeyForFrequency(f).toUtf8().constData());}b.m_bandManager=&manager;printf("fallback=%s\n",b.autoCqBandKeyForFrequency(999999999).toUtf8().constData());b.m_bandManager=nullptr;
 b.m_qsoCountByKey["K1ABC|20M|FT8"]=2;for(int limit:{0,1,2,3})for(int age:{0,1}){b.m_maxQsoPerCall=limit;b.m_qsoCountDay=QDateTime::currentDateTimeUtc().date().addDays(-age);printf("limit=%d:%d:%d:%d\n",limit,age,b.qsoLimitReached("k1abc/p",14074000,"ft8"),b.qsoLimitReached("K1ABC",7000000,"FT8"));}
 QDateTime now=QDateTime::currentDateTimeUtc();for(int sec:{-10,0,500,700}){b.m_recentAutoCqWorkedUtcByKey.clear();b.m_recentQsoLogUtcByKey.clear();b.m_recentAutoCqWorkedUtcByKey["K1ABC|20M|FT8"]=now.addSecs(-sec);printf("recent=%d:%d\n",sec,b.isRecentMamWorkedOrLoggedDuplicate("K1ABC/P",14074000,"FT8"));}b.m_recentAutoCqWorkedUtcByKey.clear();b.m_recentQsoLogUtcByKey["K1ABC|20M|FT8"]=now;printf("logged=%d\n",b.isRecentMamWorkedOrLoggedDuplicate("K1ABC",14074000,""));}
