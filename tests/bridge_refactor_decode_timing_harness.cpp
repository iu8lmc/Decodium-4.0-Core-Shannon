#include <QtCore>
#include <algorithm>
#include <cmath>
#include <cstdio>
class DecodiumBridge { public:
 QHash<quint64,qint64> m_decodeStartMsBySerial; QHash<quint64,QString> m_decodeModeBySerial,m_decodeUtcTokenBySerial;
 QHash<quint64,quint64> m_decodeSessionBySerial; QHash<quint64,int> m_ft8PendingDeepFollowups;
 int m_ft8DeepFollowupCooldownSlots=0,m_dtLastSampleCount=0,m_totalDecodesForDt=0,m_dtMinSamples=3;
 double m_avgDt=0,m_decodeLatencyMs=0,m_dtSmoothFactor=.5; int avgSignals=0,latencySignals=0,countSignals=0;
 void avgDtChanged(){++avgSignals;} void decodeLatencyMsChanged(){++latencySignals;} void timeSyncSampleCountChanged(){++countSignals;}
 bool isTimeSyncDecodeMode(const QString&) const; void resetTimeSyncDecodeMetrics();
 bool shouldTrackDtSample(int,double,const QString&,const QString&) const;
 void finalizeTimeSyncDecodeCycle(quint64,const QString&,const QVector<double>&);
};
#include "bridge_refactor_decode_timing.inc"
static void check(bool v,const char* label){if(!v){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}std::puts(label);}
int main(){DecodiumBridge b;
 check(b.isTimeSyncDecodeMode(" q65-60 ") && b.isTimeSyncDecodeMode("ft8") && !b.isTimeSyncDecodeMode("SSTV"),"modes");
 check(b.shouldTrackDtSample(-15,.59,"CQ AA1AA","FT8") && !b.shouldTrackDtSample(-16,.1,"CQ AA1AA","FT8") && !b.shouldTrackDtSample(-15,.6,"CQ AA1AA","FT8"),"ft8-threshold");
 check(b.shouldTrackDtSample(-16,-.79,"CQ AA1AA","ft4") && !b.shouldTrackDtSample(-17,.1,"CQ AA1AA","FT4") && !b.shouldTrackDtSample(-16,.8,"CQ AA1AA","FT4") && !b.shouldTrackDtSample(-10,.1,"CQ ?","FT4") && !b.shouldTrackDtSample(-10,.1,"CQ AA1AA","Q65"),"ft4-threshold");
 b.finalizeTimeSyncDecodeCycle(1,"FT8",{.3,.1,.2});check(std::abs(b.m_avgDt-.2)<1e-9 && b.m_totalDecodesForDt==3 && b.m_dtLastSampleCount==3,"median");
 b.finalizeTimeSyncDecodeCycle(2,"FT8",{.4,.5,.6});check(std::abs(b.m_avgDt-.35)<1e-9 && b.m_totalDecodesForDt==6,"smooth");
 b.finalizeTimeSyncDecodeCycle(3,"FT8",{});check(std::abs(b.m_avgDt-.315)<1e-9 && b.m_dtLastSampleCount==0,"decay");
 b.m_decodeStartMsBySerial[4]=123;b.m_decodeModeBySerial[4]="X";b.m_decodeUtcTokenBySerial[4]="X";b.m_decodeSessionBySerial[4]=3;b.finalizeTimeSyncDecodeCycle(4,"SSTV",{1,2,3});check(!b.m_decodeStartMsBySerial.contains(4) && !b.m_decodeModeBySerial.contains(4) && !b.m_decodeSessionBySerial.contains(4) && b.m_totalDecodesForDt==6,"non-sync-cleanup");
 b.m_ft8PendingDeepFollowups[3]=1;b.m_ft8DeepFollowupCooldownSlots=4;b.m_dtLastSampleCount=3;b.m_decodeLatencyMs=5;b.resetTimeSyncDecodeMetrics();check(b.m_avgDt==0 && b.m_decodeLatencyMs==0 && b.m_dtLastSampleCount==0 && b.m_totalDecodesForDt==0 && b.m_ft8PendingDeepFollowups.isEmpty() && b.m_ft8DeepFollowupCooldownSlots==0,"reset");
}
