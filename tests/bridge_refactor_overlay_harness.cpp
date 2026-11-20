#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QVariantList>
#include <QVector>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cassert>
#include <iostream>
void bridgeLog(QString const&) {}
class DecodiumBridge : public QObject {
public:
 static constexpr int kPerfFrameRingSize=32;
 double m_frameTimeRing[kPerfFrameRingSize]{}; int m_frameTimeRingPos=0,m_frameTimeRingFilled=0;
 QElapsedTimer m_perfFrameElapsed; bool m_perfFrameElapsedStarted=false;
 double m_lastFrameTimeMs=0,m_meanFrameTimeMs=0,m_p99FrameTimeMs=0,m_decodeRateReceivedHz=0,m_decodeRateCommittedHz=0;
 mutable std::atomic<int> m_decodeRateReceivedCounter{0},m_decodeRateCommittedCounter{0};
 mutable std::atomic<qint64> m_totalFrameSamples{0},m_totalDecodesReceived{0},m_totalDecodesCommitted{0};
 QTimer* m_perfMetricsTimer=nullptr; std::atomic<bool> m_devOverlayActive{false}; QString m_activeRhiBackend="unknown";
 int updates=0,changes=0; void perfMetricsChanged(){++updates;} void devOverlayActiveChanged(){++changes;}
 void recordFrameTimestamp(); void noteDecodeReceived() const; void noteDecodeCommitted(); QVariantList frameTimeSamples() const;
 void setActiveRhiBackend(QString const&); void setDevOverlayActive(bool);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char** argv){ QCoreApplication app(argc,argv); DecodiumBridge b;
 b.setActiveRhiBackend(""); assert(b.m_activeRhiBackend=="unknown"); b.setActiveRhiBackend("Metal"); assert(b.m_activeRhiBackend=="Metal");
 b.setDevOverlayActive(true); assert(b.m_perfMetricsTimer && b.m_perfMetricsTimer->isActive()); b.setDevOverlayActive(true); assert(b.changes==1);
 for(int i=0;i<40;++i)b.recordFrameTimestamp(); assert(b.m_totalFrameSamples==40 && b.frameTimeSamples().size()==32);
 b.noteDecodeReceived();b.noteDecodeReceived();b.noteDecodeCommitted(); assert(b.m_totalDecodesReceived==2 && b.m_totalDecodesCommitted==1);
 QEventLoop loop; QTimer::singleShot(300,&loop,&QEventLoop::quit);loop.exec();
 assert(b.m_decodeRateReceivedHz==8 && b.m_decodeRateCommittedHz==4 && b.m_meanFrameTimeMs>=0 && b.m_p99FrameTimeMs>=0 && b.updates>=2);
 b.setDevOverlayActive(false);assert(!b.m_perfMetricsTimer->isActive());int changes=b.changes;b.setDevOverlayActive(false);assert(b.changes==changes);
 b.setDevOverlayActive(true);assert(b.frameTimeSamples().empty() && b.m_decodeRateReceivedHz==0 && b.m_totalDecodesReceived==2);b.setDevOverlayActive(false);
 std::cout<<"overlay timer, frame ring, rate counters, reset and backend fallback: OK\n"; }
