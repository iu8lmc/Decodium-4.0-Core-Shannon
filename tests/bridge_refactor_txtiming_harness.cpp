#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QString>
#include <cassert>
#include <iostream>
void bridgeLog(QString const&){}bool catSuppressed=false;bool catSuppressedByEnvironment(){return catSuppressed;}int latestFt2AsyncTxStartMs(int period,bool conservative){return period>0?(conservative?1000:2000):0;}
class DecodiumBridge:public QObject {public:
 bool m_cwTxActive=false,m_ft2LinkTxActive=false,m_foxElasticTxNow=false,fox=false,m_asyncTxEnabled=false,m_autoCqRepeat=false,legacy=false,m_ftxImmediateClickTx=false,m_ft2ConservativeTiming=true,m_txEnabled=true,m_manualTxHold=false,m_transmitting=false,m_tuning=false,m_periodicTxCheckScheduled=false;
 QString m_mode="FT8",payload="CQ TEST";int m_currentTx=6,m_txRetryCount=0,precompute=-1,checks=0;qint64 now=0,m_ft2AsyncPartnerSlotMs=0;quint64 m_ft2AsyncSmartTxSerial=0,m_txRequestGeneration=0;
 bool foxElasticActive()const{return fox;}bool usingLegacyBackendForTx()const{return legacy;}int effectivePeriodMsForMode(QString const&)const{return 15000;}int periodMsForMode(QString const&)const{return 3000;}int syncTxCommitDeadlineMs(bool)const{return 1000;}qint64 correctedUtcEpochMs()const{return now;}void scheduleTxAudioPrecompute(int x){precompute=x;}QString buildCurrentTxMessage()const{return payload;}void checkAndStartPeriodicTx(){++checks;}
 bool shouldAlignTxAudioToCurrentSyncSlot()const;bool isSyncTxStartTooLate(int*,int*)const;bool isFt2AsyncTxStartTooLate(int*,int*,int*)const;qint64 safeFt2AsyncTxDelay(qint64,QString*)const;void scheduleFt2AsyncTxAtNextSafeSlot(QString const&,int,int,int);
};
#include "bridge_refactor_under_test.inc"
void wait(int ms){QEventLoop l;QTimer::singleShot(ms,&l,&QEventLoop::quit);l.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;assert(b.shouldAlignTxAudioToCurrentSyncSlot());b.m_cwTxActive=true;assert(!b.shouldAlignTxAudioToCurrentSyncSlot());b.m_cwTxActive=false;b.m_mode="JT4";b.legacy=true;catSuppressed=false;assert(!b.shouldAlignTxAudioToCurrentSyncSlot());catSuppressed=true;assert(b.shouldAlignTxAudioToCurrentSyncSlot());b.m_mode="FT8";b.now=500;int elapsed,latest,delay;assert(!b.isSyncTxStartTooLate(&elapsed,&latest)&&elapsed==500&&latest==1000);b.now=999;assert(!b.isSyncTxStartTooLate(&elapsed,&latest)&&elapsed==999);b.now=1000;assert(b.isSyncTxStartTooLate(&elapsed,&latest)&&elapsed==1000);b.now=15000;assert(!b.isSyncTxStartTooLate(&elapsed,&latest)&&elapsed==0);b.now=1200;assert(b.isSyncTxStartTooLate(&elapsed,&latest));
 b.m_mode="FT2";b.m_asyncTxEnabled=true;b.now=1200;assert(b.isFt2AsyncTxStartTooLate(&elapsed,&latest,&delay)&&elapsed==1200&&latest==1000&&delay==1840);b.now=900;QString reason;assert(b.safeFt2AsyncTxDelay(200,&reason)==2140&&!reason.isEmpty());assert(b.safeFt2AsyncTxDelay(50,&reason)==50&&reason.isEmpty());
 b.now=100;b.scheduleFt2AsyncTxAtNextSafeSlot("test",100,1000,20);assert(b.m_periodicTxCheckScheduled&&b.precompute==20);wait(50);assert(b.checks==1&&!b.m_periodicTxCheckScheduled);b.scheduleFt2AsyncTxAtNextSafeSlot("stale",100,1000,20);++b.m_txRequestGeneration;wait(50);assert(b.checks==1&&!b.m_periodicTxCheckScheduled);
 b.m_ft2AsyncPartnerSlotMs=3000;b.m_txRetryCount=3;b.now=3000;b.scheduleFt2AsyncTxAtNextSafeSlot("phase",0,1000,20);assert(b.m_periodicTxCheckScheduled&&b.precompute==250);++b.m_txRequestGeneration;
 std::cout<<"synchronous/FT2 timing, exact boundaries, safe-slot retry, stale guard and phase lock: OK\n";}
