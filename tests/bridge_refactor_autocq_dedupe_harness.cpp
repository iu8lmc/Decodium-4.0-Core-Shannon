#include <QtCore>
#include <cstdio>
namespace Radio {QString base_callsign(const QString&s){return s.trimmed().section('/',0,0);}}
static bool isWithinRecentDuplicateWindow(const QDateTime&a,const QDateTime&b){qint64 d=a.secsTo(b);return d>=0&&d<=90;}
static bool isWithinRecentAutoCqPartnerWindow(const QDateTime&a,const QDateTime&b){qint64 d=a.secsTo(b);return d>=0&&d<=600;}
static bool decodiumBridge_isWithinRecentDuplicateWindow(const QDateTime&a,const QDateTime&b){return isWithinRecentDuplicateWindow(a,b);}
static bool decodiumBridge_isWithinRecentAutoCqPartnerWindow(const QDateTime&a,const QDateTime&b){return isWithinRecentAutoCqPartnerWindow(a,b);}
class DecodiumBridge {public:
 QString m_mode="FT8";QHash<QString,QDateTime> m_recentAutoCqAbandonedUtcByKey,m_recentAutoCqWorkedUtcByKey,m_recentQsoLogUtcByKey;
 QString autoCqBandKeyForFrequency(double f)const{return f==1?"--":"20M";}
 bool isRecentAutoCqDuplicate(const QString&,double,const QString&)const;bool isRecentAutoCqWorkedOrLoggedDuplicate(const QString&,double,const QString&)const;void touchRecentAutoCqWorked(const QString&,double,const QString&);
};
#include "bridge_refactor_autocq_dedupe.inc"
static void check(bool x,const char*s){if(!x){fprintf(stderr,"FAIL %s\n",s);exit(1);}puts(s);}
int main(){DecodiumBridge b;auto now=QDateTime::currentDateTimeUtc();QString key="K1ABC|20M|FT8";
 check(!b.isRecentAutoCqDuplicate("",14074000,"")&&!b.isRecentAutoCqDuplicate("K1ABC",1,""),"invalid");b.m_recentAutoCqAbandonedUtcByKey[key]=now.addSecs(-80);check(b.isRecentAutoCqDuplicate(" k1abc/P ",14074000,"")&&!b.isRecentAutoCqWorkedOrLoggedDuplicate("K1ABC",14074000,""),"abandoned-short");b.m_recentAutoCqAbandonedUtcByKey[key]=now.addSecs(-100);check(!b.isRecentAutoCqDuplicate("K1ABC",14074000,""),"abandoned-expired");b.m_recentAutoCqAbandonedUtcByKey.clear();
 b.m_recentAutoCqWorkedUtcByKey[key]=now.addSecs(-500);check(b.isRecentAutoCqDuplicate("K1ABC",14074000,"")&&b.isRecentAutoCqWorkedOrLoggedDuplicate("K1ABC",14074000,""),"worked-long");b.m_recentAutoCqWorkedUtcByKey[key]=now.addSecs(-700);check(!b.isRecentAutoCqDuplicate("K1ABC",14074000,""),"worked-expired");b.touchRecentAutoCqWorked("K1ABC",14074000,"");check(b.isRecentAutoCqDuplicate("K1ABC",14074000,""),"touch");b.m_recentAutoCqWorkedUtcByKey.clear();b.m_recentQsoLogUtcByKey[key]=now.addSecs(-500);check(b.isRecentAutoCqWorkedOrLoggedDuplicate("K1ABC",14074000,""),"logged");check(!b.isRecentAutoCqDuplicate("K1ABC",14074000,"FT4"),"mode-separation");}
