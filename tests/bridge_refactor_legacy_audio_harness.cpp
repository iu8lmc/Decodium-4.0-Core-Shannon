#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QVector>
#include <QByteArray>
#include <QDateTime>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "Ft2LinkPcmStats.h"
#define emit
void bridgeLog(const QString&) {}
bool isFt2LinkApplicationMode(QString mode) { return mode == "FT2LINK"; }
struct Recorder {bool active=false;QVector<short> captured;bool recording()const{return active;}void feedSamples(QVector<short> const& v){captured+=v;}};
class DecodiumBridge : public QObject {
public:
 static constexpr int WF_RING_SIZE=8; short m_wfRing[WF_RING_SIZE]{}; int m_wfRingPos=0; Recorder *m_wavManager=nullptr;
 bool m_legacyPcmSpectrumFeed=false,m_monitoring=false,m_transmitting=false,m_tuning=false,m_ft2LinkLegacyRxDrainScheduled=false,modern=false,dedicated=false;
 QString m_mode="FT8";qint64 m_lastLegacyAudioDiagLogMs=0,m_lastFt2LinkLegacyRxTapLogMs=0,m_lastFt2LinkLegacyRxEmitMs=0,m_lastFt2LinkLegacyRxEmitLogMs=0,m_lastLegacyPcmSampleMs=0;
 QVector<short> m_ft2LinkLegacyRxPending,emitted; int emits=0;
 bool useModernSpectrumFeedWithLegacy() const{return modern;} bool useDedicatedModernAudioCaptureWithLegacy() const{return dedicated;}
 void ft2LinkRxSamplesReady(QVector<short> v,quint64){++emits;emitted=v;}
 void onLegacyAudioSamples(QByteArray const&);
};
#include "method.inc"
void check(bool v){if(!v)abort();}
void drain(){QEventLoop loop;QTimer::singleShot(260,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char** argv){QCoreApplication app(argc,argv);DecodiumBridge b;Recorder r;b.m_wavManager=&r;QByteArray pcm;for(qint16 v:{qint16(0),qint16(1234),qint16(-2222),qint16(32767)}){char raw[2];qToLittleEndian<qint16>(v,reinterpret_cast<uchar*>(raw));pcm.append(raw,2);}QVector<short> expected{0,1234,-2222,32767};
 b.onLegacyAudioSamples(QByteArray());b.onLegacyAudioSamples(QByteArray(1,'x'));check(r.captured.isEmpty() && b.m_wfRingPos==0);
 r.active=true;b.m_legacyPcmSpectrumFeed=true;b.onLegacyAudioSamples(pcm);check(r.captured==expected && b.m_wfRingPos==4 && b.m_wfRing[2]==-2222 && b.m_lastLegacyPcmSampleMs>0);
 r.active=false;b.m_legacyPcmSpectrumFeed=false;b.m_mode="FT2LINK";b.m_monitoring=true;b.onLegacyAudioSamples(pcm);check(b.m_ft2LinkLegacyRxPending==expected && b.m_ft2LinkLegacyRxDrainScheduled);drain();check(b.emits==1 && b.emitted==expected && b.m_ft2LinkLegacyRxPending.isEmpty());
 b.onLegacyAudioSamples(pcm);b.m_transmitting=true;drain();check(b.emits==1 && b.m_ft2LinkLegacyRxPending.isEmpty());
 b.m_transmitting=false;b.modern=true;b.dedicated=true;b.onLegacyAudioSamples(pcm);drain();check(b.emits==1 && !b.m_ft2LinkLegacyRxDrainScheduled);
 b.modern=false;b.dedicated=false;QByteArray longPcm(50000,'\1');b.onLegacyAudioSamples(longPcm);check(b.m_ft2LinkLegacyRxPending.size()==24000);drain();check(b.emits==2 && b.emitted.size()==24000);
 std::cout<<"recording, PCM ring, FT2 batching, TX cancellation, dedicated feed and cap passed\n";}
