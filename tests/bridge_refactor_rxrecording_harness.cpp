#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <iostream>
struct WavManager {bool active=false,fail=false;int starts=0,stops=0;QString path="/tmp/test.wav";bool recording()const{return active;}QString lastFilePath()const{return path;}void startRecording(){++starts;if(!fail)active=true;}void stopRecording(){++stops;active=false;}};
class DecodiumBridge:public QObject {
public:
 WavManager* m_wavManager=nullptr;bool m_recordRxEnabled=false;int changed=0;QString status,error;
 void recordRxEnabledChanged(){++changed;}void statusMessage(const QString&s){status=s;}void errorMessage(const QString&s){error=s;}
 bool recordRxEnabled() const;void setRecordRxEnabled(bool);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);
 DecodiumBridge a;a.setRecordRxEnabled(false);a.setRecordRxEnabled(true);a.setRecordRxEnabled(true);std::cout<<"no-manager|"<<a.recordRxEnabled()<<"|"<<a.changed<<'\n';
 WavManager m;DecodiumBridge b;b.m_wavManager=&m;b.setRecordRxEnabled(true);std::cout<<"start|"<<b.recordRxEnabled()<<"|"<<m.starts<<"|"<<b.status.contains("avviata")<<'\n';b.setRecordRxEnabled(true);b.setRecordRxEnabled(false);std::cout<<"stop|"<<b.recordRxEnabled()<<"|"<<m.starts<<"|"<<m.stops<<"|"<<b.status.contains("salvata")<<'\n';
 WavManager fail;fail.fail=true;DecodiumBridge c;c.m_wavManager=&fail;c.setRecordRxEnabled(true);std::cout<<"failed|"<<c.recordRxEnabled()<<"|"<<c.changed<<"|"<<!c.error.isEmpty()<<'\n';
 WavManager existing;existing.active=true;DecodiumBridge d;d.m_wavManager=&existing;d.setRecordRxEnabled(true);std::cout<<"existing|"<<d.recordRxEnabled()<<"|"<<existing.starts<<"|"<<d.changed<<'\n';
}
