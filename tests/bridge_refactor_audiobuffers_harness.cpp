#include <QCoreApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <QVector>
#include <QStringList>
#include <QByteArray>
#include <cassert>
#include <iostream>
#include "MamTxPayloadPolicy.h"
void bridgeLog(QString const&){}
class DecodiumBridge {public:
 struct TxAudioCache{QVector<float> wave;QByteArray pcm;};TxAudioCache m_txAudioCache;QStringList m_mamMessages;QVector<int> m_mamF0sHz;QVector<float> m_mamGains;
 QTimer* m_audioBufferReleaseTimer=nullptr;bool m_transmitting=false,m_tuning=false,m_bridgeAudioLegacyTxActive=false,m_ft2LinkTxActive=false,m_monitoring=false,m_monitorRequested=false,m_pendingTimeSyncDecodeActive=false,m_forcedDecodeAudioSnapshotActive=false,m_ft2LinkModernRxDrainScheduled=false,m_ft2LinkLegacyRxDrainScheduled=false;void* m_txPcmBuffer=nullptr;
 QByteArray m_txPcmData;QString m_pendingFt2LinkText;QStringList m_pendingFt2LinkPlan;QVector<float> m_pendingFt2LinkWave;QMutex m_audioBufferMutex;QVector<short> m_audioBuffer,m_pendingTimeSyncDecodeAudio,m_forcedDecodeAudioSnapshot,m_ft2LinkModernRxPending,m_ft2LinkLegacyRxPending,m_spectrumBuf;QVector<float> m_lastPanadapterData;int signalCount=0;
 void mamTxStreamsChanged(){++signalCount;}void invalidateTxAudioCache();void clearMamPendingTxPayload(QString const&);void scheduleIdleAudioBufferRelease(int);void releaseIdleAudioBuffers();
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;QTimer timer;timer.setSingleShot(true);b.m_audioBufferReleaseTimer=&timer;
 b.scheduleIdleAudioBufferRelease(1);assert(timer.interval()==5000);b.scheduleIdleAudioBufferRelease(999999);assert(timer.interval()==300000);
 b.m_txAudioCache.wave={1,2};b.m_txPcmData="pcm";b.m_audioBuffer={1,2};b.m_transmitting=true;b.releaseIdleAudioBuffers();assert(!b.m_txAudioCache.wave.empty()&&!b.m_audioBuffer.empty()&&timer.interval()==30000);
 b.m_transmitting=false;b.m_monitoring=true;b.m_pendingFt2LinkText="pending";b.m_pendingFt2LinkWave={1};b.releaseIdleAudioBuffers();assert(b.m_txAudioCache.wave.empty()&&b.m_txPcmData.isEmpty()&&!b.m_audioBuffer.empty()&&!b.m_pendingFt2LinkWave.empty());
 b.m_monitoring=false;b.m_pendingFt2LinkText.clear();b.m_pendingTimeSyncDecodeActive=true;b.m_pendingTimeSyncDecodeAudio={2};b.m_ft2LinkModernRxDrainScheduled=true;b.m_ft2LinkModernRxPending={3};b.m_spectrumBuf={4};b.releaseIdleAudioBuffers();assert(b.m_audioBuffer.empty()&&b.m_pendingFt2LinkWave.empty()&&!b.m_pendingTimeSyncDecodeAudio.empty()&&!b.m_ft2LinkModernRxPending.empty()&&b.m_spectrumBuf.empty());
 b.m_mamMessages<<"CQ TEST";b.m_mamF0sHz<<1500;b.m_mamGains<<1;b.m_txAudioCache.wave={1};b.clearMamPendingTxPayload("test");assert(b.m_mamMessages.empty()&&b.m_mamF0sHz.empty()&&b.m_mamGains.empty()&&b.m_txAudioCache.wave.empty()&&b.signalCount==1);b.clearMamPendingTxPayload("again");assert(b.signalCount==1);
 std::cout<<"idle buffer TX/RX guards, pending data preservation and MAM invalidation: OK\n";}
