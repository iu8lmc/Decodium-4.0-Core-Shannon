#include <QtCore>
#include <cstdio>
struct QAudioDevice { bool null=true; bool isNull() const {return null;} };
static QString lastLog;
static void bridgeLog(const QString& msg){lastLog=msg;}
static void decodiumBridgeKeyboardAudioLog(const QString& msg){lastLog=msg;}
#include "bridge_refactor_keyboard_mode.inc"
static bool decodiumBridgeIsStreamingKeyboardMode(QString const& mode){return isStreamingKeyboardMode(mode);}
class DecodiumBridge { public:
 QString m_mode="RTTY"; bool m_transmitting=false,m_tuning=false,m_decoPortRemoteKeyed=false,m_rttyTxActive=false,m_decoPortUseRemote=false,m_catConnected=false;
 bool m_rttyInAscolto=false,m_cwInAscolto=false,sstv=false,found=true,deviceNull=false;
 int keysOn=0,keysOff=0,audioCalls=0,rttyNotifies=0,cwNotifies=0,remoteMonitor=0; QVector<short> played;
 bool sstvTxActive()const{return sstv;} QAudioDevice resolveTxOutputDevice(bool* out){*out=found;return QAudioDevice{deviceNull};}
 void keySharedAudioTransmitter(bool on,bool){if(on){++keysOn;m_decoPortRemoteKeyed=true;}else{++keysOff;m_decoPortRemoteKeyed=false;m_rttyTxActive=false;}}
 void decoPortPlayTxAudio(const QVector<short>& s,bool){++audioCalls;played=s;}
 void setDecoPortMonitor(bool v){if(v)++remoteMonitor;}
 void rttyInAscoltoChanged(){++rttyNotifies;} void cwInAscoltoChanged(){++cwNotifies;}
 bool rttyCanTransmit(); void rttyAlzaPtt(bool); void rttyMandaAudioTx(const QVector<short>&);
 void setRttyInAscolto(bool); void setCwInAscolto(bool);
};
#include "bridge_refactor_keyboard_audio.inc"
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}puts(name);}
int main(){
 DecodiumBridge b;check(b.rttyCanTransmit(),"rtty-ready");
 b.m_mode=" jtty ";check(b.rttyCanTransmit(),"jtty-ready-case");
 b.m_mode="FT8";check(!b.rttyCanTransmit(),"reject-slot-mode");b.m_mode="RTTY";
 b.m_transmitting=true;check(!b.rttyCanTransmit(),"reject-transmitting");b.m_transmitting=false;
 b.m_tuning=true;check(!b.rttyCanTransmit(),"reject-tuning");b.m_tuning=false;
 b.sstv=true;check(!b.rttyCanTransmit(),"reject-sstv");b.sstv=false;
 b.m_decoPortUseRemote=true;check(!b.rttyCanTransmit(),"reject-remote");b.m_decoPortUseRemote=false;
 b.m_decoPortRemoteKeyed=true;check(!b.rttyCanTransmit(),"reject-other-key");b.m_decoPortRemoteKeyed=false;
 b.found=false;check(!b.rttyCanTransmit(),"reject-missing-device");b.found=true;
 b.deviceNull=true;check(!b.rttyCanTransmit(),"reject-null-device");b.deviceNull=false;
 QVector<short> samples{0,100,-100};b.rttyMandaAudioTx(samples);check(b.audioCalls==0,"ignore-unkeyed-audio");
 b.rttyAlzaPtt(true);check(b.keysOn==1&&b.m_rttyTxActive,"key-on");
 b.rttyAlzaPtt(true);check(b.keysOn==1,"key-on-idempotent");
 b.rttyMandaAudioTx(samples);check(b.audioCalls==1&&b.played==samples,"send-keyed-samples");
 b.m_tuning=true;b.rttyMandaAudioTx(samples);check(b.keysOff==1&&!b.m_rttyTxActive&&b.audioCalls==1,"release-when-busy");b.m_tuning=false;
 b.rttyAlzaPtt(false);check(b.keysOff==1,"key-off-idempotent");
 b.setRttyInAscolto(true);b.setRttyInAscolto(true);b.setRttyInAscolto(false);check(b.rttyNotifies==2,"rtty-monitor-notifications");
 b.m_decoPortUseRemote=true;b.setCwInAscolto(true);b.setCwInAscolto(true);b.setCwInAscolto(false);check(b.cwNotifies==2&&b.remoteMonitor==1,"cw-remote-monitor");
}
