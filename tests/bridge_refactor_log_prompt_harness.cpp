#include <QtCore>
#include <cstdio>
class DecodiumBridge {public:
 bool m_nextLogClusterSpotOverrideValid=false,m_nextLogClusterSpotEnabled=false,m_promptLogCommentOverrideValid=false;
 QString m_promptLogComment,m_promptLogGrid,lastKey,lastValue;QDateTime m_promptLogOn,m_promptLogOff,m_pendingAutoLogOn,m_pendingAutoLogOff;
 void setSetting(const QString&k,const QVariant&v){lastKey=k;lastValue=v.toString();}
 void setNextLogClusterSpotEnabled(bool);void clearNextLogClusterSpotOverride();void setNextLogComment(const QString&);void setNextLogGrid(const QString&);void setNextLogTimes(const QString&,const QString&);
};
#include "bridge_refactor_log_prompt.inc"
static void check(bool v,const char*s){if(!v){fprintf(stderr,"FAIL %s\n",s);exit(1);}puts(s);}
int main(){DecodiumBridge b;b.setNextLogClusterSpotEnabled(true);check(b.m_nextLogClusterSpotOverrideValid&&b.m_nextLogClusterSpotEnabled,"spot-enable");b.clearNextLogClusterSpotOverride();check(!b.m_nextLogClusterSpotOverrideValid&&!b.m_nextLogClusterSpotEnabled,"spot-clear");
 b.setNextLogComment("  hello  ");check(b.m_promptLogComment=="hello"&&b.m_promptLogCommentOverrideValid&&b.lastKey=="LogComments"&&b.lastValue=="hello","comment");b.setNextLogGrid(" jm75aa ");check(b.m_promptLogGrid=="JM75AA","grid");
 b.setNextLogTimes("2026-11-07 12:30:00Z","2026-11-07T12:31:00Z");check(b.m_promptLogOn.toString(Qt::ISODate)=="2026-11-07T12:30:00Z"&&b.m_promptLogOff.toString(Qt::ISODate)=="2026-11-07T12:31:00Z","datetime");
 b.setNextLogTimes("20261107 123200","1231");check(b.m_promptLogOn.toString(Qt::ISODate)=="2026-11-07T12:32:00Z"&&b.m_promptLogOff==b.m_promptLogOn,"clamp-reversed");
 b.setNextLogTimes("12:40","12:41:05");check(b.m_promptLogOn.toString(Qt::ISODate)=="2026-11-07T12:40:00Z"&&b.m_promptLogOff.toString(Qt::ISODate)=="2026-11-07T12:41:05Z","time-only");
#ifdef VERIFY_UTC_FIX
 b.setNextLogTimes("2026-11-07 12:30:00 UTC","2026-11-07T12:31:00Z");check(b.m_promptLogOn.toString(Qt::ISODate)=="2026-11-07T12:30:00Z"&&b.m_promptLogOff.toString(Qt::ISODate)=="2026-11-07T12:31:00Z","utc-suffix");
 b.setNextLogTimes("12:40","12:41:05");
#endif
 auto old=b.m_promptLogOn;b.setNextLogTimes("not a time","");check(b.m_promptLogOn==old&&b.m_promptLogOff.toString(Qt::ISODate)=="2026-11-07T12:41:05Z","fallback");}
