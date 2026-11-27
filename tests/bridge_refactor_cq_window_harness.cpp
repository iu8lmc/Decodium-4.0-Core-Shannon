#include <QDateTime>
#include <QTime>
#include <QString>
#include <iostream>
#include "DecodeUtcTime.h"
void bridgeLog(QString const&){}
class DecodiumBridge {public:int m_cqAutoReplyArmSecond=-1; qint64 m_cqAutoReplyArmWallMs=0;QString m_mode="FT8";bool m_asyncTxEnabled=false,m_autoCqRepeat=false;int periodMsForMode(QString const& m)const{return m=="FT2"?3750:15000;}void armCqAutoReplyWindow(QString const&);void clearCqAutoReplyWindow(QString const&);bool decodeIsFreshForCqAutoReply(QString const&,QString*)const;};
#include "method.inc"
void check(bool x){if(!x)abort();}QString token(int seconds){return QTime(0,0).addSecs(seconds).toString("hhmmss");}
int main(){DecodiumBridge b;QString why;check(b.decodeIsFreshForCqAutoReply("invalid",&why));b.armCqAutoReplyWindow("test");check(b.m_cqAutoReplyArmSecond>=0 && b.m_cqAutoReplyArmWallMs>0);int now=QTime::currentTime().msecsSinceStartOfDay()/1000;now=QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay()/1000;b.m_cqAutoReplyArmSecond=(now-10+86400)%86400;check(!b.decodeIsFreshForCqAutoReply(token(now-13),&why)&&why.contains("predates"));b.m_mode="FT2";b.m_asyncTxEnabled=true;check(b.decodeIsFreshForCqAutoReply(token(now-13),&why));b.m_cqAutoReplyArmSecond=(now-120+86400)%86400;check(!b.decodeIsFreshForCqAutoReply(token(now-100),&why)&&why.contains("age"));b.clearCqAutoReplyWindow("test");check(b.m_cqAutoReplyArmSecond==-1&&b.m_cqAutoReplyArmWallMs==0&&b.decodeIsFreshForCqAutoReply(token(now-100),nullptr));std::cout<<"arm, FT8 stale, FT2 tolerance, age and clear passed\n";}
