#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QList>
#include <QStringList>
#include <QTextStream>
#ifdef SIMULATE_LINUX
#define Q_OS_LINUX 1
#endif
class QAudioFormat {};
class QAudioDevice {QString n;public:QAudioDevice(QString s={}):n(s){}QString description()const{return n;}QAudioFormat preferredFormat()const{return {};}};
class QMediaDevices {public:static QList<QAudioDevice> ins,outs;static QList<QAudioDevice> audioInputs(){return ins;}static QList<QAudioDevice> audioOutputs(){return outs;}static QAudioDevice defaultAudioInput(){return ins.isEmpty()?QAudioDevice():ins.first();}static QAudioDevice defaultAudioOutput(){return outs.isEmpty()?QAudioDevice():outs.first();}};
QList<QAudioDevice> QMediaDevices::ins;QList<QAudioDevice> QMediaDevices::outs;
static QStringList audioDeviceDescriptions(const QList<QAudioDevice>&d){QStringList x;for(auto a:d)x<<a.description();return x;}
static QString audioDeviceCacheSignature(const QList<QAudioDevice>&i,const QList<QAudioDevice>&o,const QAudioDevice&,const QAudioDevice&){return audioDeviceDescriptions(i).join(",")+"/"+audioDeviceDescriptions(o).join(",");}
static QString audioDeviceIdForLog(const QAudioDevice&d){return d.description();}static QString audioFormatForLog(const QAudioFormat&){return "fmt";}static void bridgeLog(const QString&){}
static QStringList linuxPulseSourcesForLog(){return {"source1","source2"};}static QStringList linuxPulseSinksForLog(){return {"sink1"};}static QStringList linuxAlsaCardsForLog(){return {"card"};}static bool audioDeviceLooksUsb(const QString&s){return s.contains("usb");}static QString linuxPulseSourceNameFromShortLine(const QString&s){return s;}static QString linuxPulseSourceDisplayName(const QString&s){return "pulse:"+s;}static QString linuxPulseSinkNameFromShortLine(const QString&s){return s;}static constexpr char kLinuxPulseSinkPrefix[]="pulse:";
class MainThreadTraceScope{public:MainThreadTraceScope(const QString&,const QString&,int){}void addDetail(const QString&){} };class Diagnostics {public:QString last;void setAudioState(const QString&s){last=s;}};
class DecodiumBridge{public:QString m_audioInputDevice,m_audioOutputDevice,m_audioDeviceCacheSignature;QList<QAudioDevice>m_cachedAudioInputs,m_cachedAudioOutputs;QAudioDevice m_cachedDefaultAudioInput,m_cachedDefaultAudioOutput;QStringList m_audioInputDevices,m_audioOutputDevices;qint64 m_audioDeviceCacheRefreshMs=0,m_lastAudioDeviceCacheLogMs=0;bool m_audioDeviceCacheValid=false,m_audioDeviceCacheDirty=true,m_cachedTxOutputDeviceValid=true;Diagnostics* m_diagnostics=nullptr;int inSignals=0,outSignals=0;void audioInputDevicesChanged(){++inSignals;}void audioOutputDevicesChanged(){++outSignals;}void refreshAudioDeviceCache(const QString&,bool,bool);};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream out(stdout);Diagnostics diag;DecodiumBridge b;b.m_diagnostics=&diag;QMediaDevices::ins={QAudioDevice("Mic")};QMediaDevices::outs={QAudioDevice("Speaker")};b.refreshAudioDeviceCache("first",true,true);out<<"first|"<<b.m_audioInputDevices.join(",")<<'|'<<b.m_audioOutputDevices.join(",")<<'|'<<b.inSignals<<'|'<<b.outSignals<<'|'<<b.m_cachedTxOutputDeviceValid<<'|'<<b.m_audioDeviceCacheValid<<'|'<<b.m_audioDeviceCacheDirty<<'\n';b.m_cachedTxOutputDeviceValid=true;b.refreshAudioDeviceCache("same",false,true);out<<"same|"<<b.inSignals<<'|'<<b.outSignals<<'|'<<b.m_cachedTxOutputDeviceValid<<'\n';QMediaDevices::ins.append(QAudioDevice("usb radio"));b.refreshAudioDeviceCache("changed",false,false);out<<"changed-silent|"<<b.inSignals<<'|'<<b.outSignals<<'|'<<b.m_cachedTxOutputDeviceValid<<'|'<<b.m_cachedAudioInputs.size()<<'|'<<diag.last.contains("changed")<<'\n';b.refreshAudioDeviceCache("unchanged",false,true);out<<"unchanged|"<<b.inSignals<<'|'<<b.outSignals<<'\n';}
