#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QTextStream>
static constexpr int kSpecialOpHound=7;
static int legacyTxAttenuationFromLevel(double v){return qRound(v);}
static bool isFt2LinkApplicationMode(QString m){return m=="FT2-LINK";}static bool isStreamingKeyboardMode(const QString&m){return m=="RTTY";}static void bridgeLog(const QString&){}
class Legacy {public:QStringList calls;bool mon=true;void add(QString k,QString v){calls<<k+"="+v;}void setStationIdentity(QString a,QString b){add("identity",a+":"+b);}void setAudioInputDeviceName(QString v){add("in",v);}void setAudioOutputDeviceName(QString v){add("out",v);}
#define SETNUM(fn) void fn(int v){add(#fn,QString::number(v));}
#define SETSTR(fn) void fn(QString v){add(#fn,v);}
 SETNUM(setAudioInputChannel) SETNUM(setAudioOutputChannel) SETNUM(setDecodeDepthBits) SETNUM(setCqOnly) SETNUM(setRxInputLevel) SETNUM(setTxOutputAttenuation) SETNUM(setFt2DecodeEnabled) SETNUM(setMonitoring) SETSTR(setMode) SETNUM(setDialFrequency) SETNUM(setAutoSeq) SETNUM(setAlt12Enabled) SETNUM(setTxFirst) SETNUM(setRxFrequency) SETNUM(setTxFrequency) SETNUM(setHoldTxFreq) SETSTR(setDxCall) SETSTR(setDxGrid) SETNUM(selectTxMessage) SETNUM(setTxWatchdogMinutes)
 bool monitoring(){return mon;}void setTxMessage(int n,QString v){add("msg"+QString::number(n),v);}
};
class DecodiumBridge {public:Legacy*m_legacyBackend=nullptr;bool available=true;QString m_callsign="K1ABC",m_grid="FN42",m_audioInputDevice="in",m_audioOutputDevice="out",m_mode="FT8",m_legacyStartupModeGuard,m_dxCall="K2ABC",m_dxGrid="FN31",m_tx1="1",m_tx2="2",m_tx3="3",m_tx4="4",m_tx5="5",m_tx6="6";int m_audioInputChannel=5,m_audioOutputChannel=-1,m_specialOperationActivity=0,m_txPeriod=1,m_rxFrequency=1200,m_txFrequency=1500,m_currentTx=9,m_txWatchdogMode=1,m_txWatchdogTime=5;double m_rxInputLevel=120,m_txOutputLevel=80,m_frequency=14074000;qint64 m_legacyStartupModeGuardUntilMs=0;bool m_autoSeq=true,m_alt12Enabled=true,m_holdTxFreq=true;int special=0;bool legacyBackendAvailable()const{return available;}int legacyCompatibleDecodeDepthBits()const{return 3;}void syncSpecialOperationToLegacyBackend(){++special;}void syncLegacyBackendDialogState();};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);Legacy l;DecodiumBridge b;b.m_legacyBackend=&l;b.available=false;b.syncLegacyBackendDialogState();o<<"unavailable|"<<l.calls.size()<<'\n';b.available=true;b.syncLegacyBackendDialogState();o<<"ft8|"<<l.calls.join(",")<<'|'<<b.m_legacyStartupModeGuard<<'|'<<(b.m_legacyStartupModeGuardUntilMs>0)<<'|'<<b.special<<'\n';l.calls.clear();b.m_mode="FT2-LINK";b.syncLegacyBackendDialogState();o<<"ft2|"<<l.calls.contains("setMode=FT2")<<'|'<<l.calls.contains("setFt2DecodeEnabled=0")<<'\n';l.calls.clear();b.m_mode="RTTY";b.syncLegacyBackendDialogState();o<<"rtty|"<<l.calls.contains("setMonitoring=0")<<'|'<<b.m_legacyStartupModeGuard.isEmpty()<<'|'<<b.m_legacyStartupModeGuardUntilMs<<'\n';l.calls.clear();b.m_mode="Q65-60A";b.m_specialOperationActivity=7;b.syncLegacyBackendDialogState();o<<"hound|"<<l.calls.contains("setMonitoring=0")<<'|'<<l.calls.contains("setAlt12Enabled=0")<<'|'<<l.calls.contains("setTxFirst=0")<<'\n';}
