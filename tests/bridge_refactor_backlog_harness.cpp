#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QHash>
#include <QSet>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include <limits>
#include <iostream>
class FakeWorker:public QObject {public:int cancelled=0,resets=0;quint64 marked=0;void cancelCurrentDecode(){++cancelled;}void resetDecoderState(){++resets;}void markLatestDecodeSerial(quint64 s){marked=s;}};
namespace decodium { namespace ft8 {void setHashSeedWorkEnabled(bool){}} namespace ft4 {void setHashSeedWorkEnabled(bool){}} }
static bool isFtx77MessageMode(QString const&m){auto s=m.trimmed().toUpper();return s=="FT8"||s=="FT4"||s=="FT2";}
static void bridgeLog(const QString&){}
class DecodiumBridge:public QObject {public:
 QHash<quint64,qint64> m_decodeStartMsBySerial;QHash<quint64,QString> m_decodeModeBySerial,m_decodeUtcTokenBySerial;QHash<quint64,quint64> m_decodeSessionBySerial;QSet<quint64> m_ft8EarlyDecodeSerials,m_ft8DeepInTxSerials;QHash<quint64,int> m_ft8PendingDeepFollowups,m_ft8PendingSubpassHarvest;
 int m_ft8DeepFollowupCooldownSlots=0,m_periodTicks=0,m_periodProgress=0,m_periodTicksMax=60,pressure=0; qint64 m_lastFt8BacklogDrainLogMs=0,m_lastFt4BacklogDrainLogMs=0,m_ft8EarlyDecodeSlot=0,m_ft4EarlyDecodeSlot=0,m_lastPeriodSlot=0,m_pendingTimeSyncDecodeSlot=0;
 bool m_ft8EarlyDecode41Sent=false,m_ft8EarlyDecode47Sent=false,m_ft4EarlyDecodeSent=false,m_asyncDecodePending=false,m_decoding=false,m_pendingTimeSyncDecodeActive=false,m_forcedDecodeAudioSnapshotActive=false;
 FakeWorker *m_ft8Worker=nullptr,*m_ft2Worker=nullptr,*m_ft4Worker=nullptr,*m_q65Worker=nullptr;QString m_pendingTimeSyncDecodeMode;QVector<short> m_pendingTimeSyncDecodeAudio,m_forcedDecodeAudioSnapshot,m_audioBuffer;QMutex m_audioBufferMutex;
 static constexpr int TIMER_MS=250,SAMPLE_RATE=12000;
 void decodingChanged(){} void periodProgressChanged(){} void activateFt4AdaptiveCpuLimit(qint64,const QString&,qint64){++pressure;}
 bool ft8LiveDecodeBacklogActive(qint64,int,quint64,QString*)const;void drainFt8LiveDecodeBacklog(quint64,const QString&);bool ft4LiveDecodeBacklogActive(qint64,int,quint64,QString*)const;void drainFt4LiveDecodeBacklog(quint64,const QString&);void resetEarlyDecodeSchedule();void resetFtxDecodeWorkersForModeChange(const QString&,const QString&);void resetRxPeriodAccumulation(bool);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;FakeWorker ft8,ft2,ft4,q65;b.m_ft8Worker=&ft8;b.m_ft2Worker=&ft2;b.m_ft4Worker=&ft4;b.m_q65Worker=&q65;
 auto now=QDateTime::currentMSecsSinceEpoch();b.m_decodeStartMsBySerial={{1,now-6000},{2,now-1000},{3,now-8000}};b.m_decodeModeBySerial={{1,"FT8"},{2,"FT8"},{3,"FT4"}};QString detail;
 if(!b.ft8LiveDecodeBacklogActive(now,5000,0,&detail)||b.ft8LiveDecodeBacklogActive(now,5000,1,nullptr))return 1;
 b.m_ft8EarlyDecodeSerials.insert(1);b.m_ft8PendingDeepFollowups.insert(1,1);b.drainFt8LiveDecodeBacklog(2,"test");if(b.m_decodeStartMsBySerial.contains(1)||!b.m_decodeStartMsBySerial.contains(2)||!b.m_decodeStartMsBySerial.contains(3)||ft8.cancelled!=1||b.m_ft8DeepFollowupCooldownSlots!=6)return 2;
 b.m_decodeStartMsBySerial.insert(4,now-3000);b.m_decodeStartMsBySerial.insert(5,now-12000);b.m_decodeModeBySerial.insert(4,"FT4");b.m_decodeModeBySerial.insert(5,"FT4");if(!b.ft4LiveDecodeBacklogActive(now,15000,0,&detail))return 3;
 b.drainFt4LiveDecodeBacklog(4,"test");if(!b.m_decodeStartMsBySerial.contains(3)||!b.m_decodeStartMsBySerial.contains(4)||b.m_decodeStartMsBySerial.contains(5)||b.pressure!=1)return 4;
 b.m_asyncDecodePending=true;b.m_decoding=true;b.resetFtxDecodeWorkersForModeChange("FT8","FT4");QCoreApplication::processEvents();if(b.m_decoding||b.m_asyncDecodePending||!b.m_decodeStartMsBySerial.isEmpty()||ft8.cancelled!=2||ft2.cancelled!=1||ft8.resets!=1||ft4.marked!=std::numeric_limits<quint64>::max())return 5;
 b.m_audioBuffer={1,2};b.m_pendingTimeSyncDecodeActive=true;b.resetRxPeriodAccumulation(true);QCoreApplication::processEvents();if(!b.m_audioBuffer.isEmpty()||b.m_pendingTimeSyncDecodeActive||ft8.resets!=2)return 6;
 std::cout<<"FT8/FT4 thresholds and drain; keep and preserve; mode reset and audio buffer: OK\n";
}
