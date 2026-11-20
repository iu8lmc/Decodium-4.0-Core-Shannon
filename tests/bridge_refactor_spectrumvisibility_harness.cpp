#include <QCoreApplication>
#include <QDateTime>
#include <QTimer>
#include <QVector>
#include <QString>
#include <iostream>
static void bridgeLog(const QString&){}
struct Remote {bool running=false,waterfall=false;bool isRunning()const{return running;}bool waterfallEnabled()const{return waterfall;}};
class DecodiumBridge:public QObject {public:
 bool m_spectrumVisible=true,m_monitoring=true,m_transmitting=false,m_tuning=false;Remote*m_remoteServer=nullptr;QTimer*m_spectrumTimer=nullptr;int m_lastWaterfallAudioBufferSize=42; qint64 m_lastPanadapterFrameMs=99;QVector<float>m_lastPanadapterData{1,2};int changed=0;
 void spectrumVisibleChanged(){++changed;}void setSpectrumVisible(bool);void rearmSpectrumAfterQsoLog();
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTimer timer;timer.setInterval(250);DecodiumBridge b;b.m_spectrumTimer=&timer;timer.start();b.setSpectrumVisible(false);std::cout<<"hide|"<<timer.isActive()<<"|"<<b.m_lastWaterfallAudioBufferSize<<"|"<<b.m_lastPanadapterFrameMs<<"|"<<b.m_lastPanadapterData.size()<<"|"<<b.changed<<'\n';b.setSpectrumVisible(false);b.setSpectrumVisible(true);std::cout<<"show|"<<timer.isActive()<<"|"<<b.changed<<'\n';Remote r;r.running=true;r.waterfall=true;b.m_remoteServer=&r;b.m_lastWaterfallAudioBufferSize=9;b.setSpectrumVisible(false);std::cout<<"remote|"<<timer.isActive()<<"|"<<b.m_lastWaterfallAudioBufferSize<<'\n';b.setSpectrumVisible(true);timer.stop();b.m_transmitting=true;b.rearmSpectrumAfterQsoLog();std::cout<<"busy|"<<timer.isActive()<<'\n';b.m_transmitting=false;const auto now=QDateTime::currentMSecsSinceEpoch();b.rearmSpectrumAfterQsoLog();std::cout<<"rearm|"<<timer.isActive()<<"|"<<(b.m_lastPanadapterFrameMs<=now-999)<<'\n';timer.stop();b.m_monitoring=false;b.rearmSpectrumAfterQsoLog();std::cout<<"no-monitor|"<<timer.isActive()<<'\n';}
