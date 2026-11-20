#include <QtCore>
#include <QAudioFormat>
#include <cstdio>
static int fst4PeriodSecondsForMode(QString const& mode,double) {return mode.endsWith("-15")?15:30;}
#include "bridge_refactor_tx_leadin.inc"
static int decodiumBridgeTxSyncLeadInMsForMode(const QString& mode){return txSyncLeadInMsForMode(mode);}
class DecodiumBridge { public:
 QString m_mode="FT8"; bool m_activeFt8AutoTxFullFrame=false, align=true; int period=15000; qint64 now=0;
 bool shouldAlignTxAudioToCurrentSyncSlot()const{return align;}
 int effectivePeriodMsForMode(const QString&)const{return period;}
 qint64 correctedUtcEpochMs()const{return now;}
 qint64 syncTxPcmStartOffsetBytes(const QAudioFormat&,qint64,int*)const;
};
#include "bridge_refactor_tx_timing.inc"
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}puts(name);}
int main(){
 DecodiumBridge b; QAudioFormat f; f.setSampleRate(48000);f.setChannelCount(1);f.setSampleFormat(QAudioFormat::Int16);
 int elapsed=99; b.align=false;check(b.syncTxPcmStartOffsetBytes(f,96000,&elapsed)==0&&elapsed==-1,"unaligned");
 b.align=true;QAudioFormat invalid;check(b.syncTxPcmStartOffsetBytes(invalid,96000,&elapsed)==0&&elapsed==-1,"invalid-format");
 check(b.syncTxPcmStartOffsetBytes(f,0,&elapsed)==0&&elapsed==-1,"empty-pcm");
 b.period=0;check(b.syncTxPcmStartOffsetBytes(f,96000,&elapsed)==0&&elapsed==-1,"invalid-period");
 b.period=15000;b.now=400;check(b.syncTxPcmStartOffsetBytes(f,96000,&elapsed)==0&&elapsed==400,"before-leadin");
 b.now=600;check(b.syncTxPcmStartOffsetBytes(f,96000,&elapsed)==0&&elapsed==600,"short-delay-shift");
 b.now=3000;b.m_activeFt8AutoTxFullFrame=true;check(b.syncTxPcmStartOffsetBytes(f,96000,&elapsed)==0,"ft8-full-frame");
 b.m_activeFt8AutoTxFullFrame=false;b.now=1000;check(b.syncTxPcmStartOffsetBytes(f,960000,&elapsed)==48000,"ft8-late-trim");
 b.now=14999;check(b.syncTxPcmStartOffsetBytes(f,96000,&elapsed)==95998,"clamp-last-frame");
 b.m_mode="Q65";b.period=60000;b.now=1500;check(b.syncTxPcmStartOffsetBytes(f,96000,&elapsed)==0,"q65-safe-shift");
 b.now=3000;check(b.syncTxPcmStartOffsetBytes(f,960000,&elapsed)==192000,"q65-late-trim");
 check(b.syncTxPcmStartOffsetBytes(f,96000,nullptr)==95998,"null-elapsed-pointer");
 b.m_mode="FT4"; b.period=7500; b.now=1000; check(b.syncTxPcmStartOffsetBytes(f,960000,&elapsed)==48000,"ft4-late-trim");
 b.m_mode="JT4"; b.period=60000; b.now=1500; check(b.syncTxPcmStartOffsetBytes(f,96000,&elapsed)==0,"jt4-safe-shift");
 b.m_mode="FST4-15"; b.period=15000; b.now=3000; check(b.syncTxPcmStartOffsetBytes(f,960000,&elapsed)==240000,"fst4-15-leadin");
 b.m_mode="FST4-30"; b.period=30000; b.now=3000; check(b.syncTxPcmStartOffsetBytes(f,960000,&elapsed)==192000,"fst4-30-leadin");
 b.m_mode="FT8"; b.period=15000; b.now=1000; f.setChannelCount(2); check(b.syncTxPcmStartOffsetBytes(f,1920000,&elapsed)==96000,"stereo-frame-alignment");
}
