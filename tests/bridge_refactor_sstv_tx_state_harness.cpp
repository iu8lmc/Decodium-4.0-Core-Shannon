#include <QCoreApplication>
#include <QStringList>
#include <QVariantMap>
#include <cmath>
#include <iostream>
namespace decodium::sstv {
enum class SstvTxState { Disabled, Idle };
struct SstvTxCoordinatorSnapshot {struct {SstvTxState state=SstvTxState::Idle;bool releaseRequired=false;} stateMachine;bool audioLeaseRetained=false;double headroom=0.5;double pcmPeak=0.4;unsigned clippedFrames=0;};
struct Coordinator {SstvTxCoordinatorSnapshot value;SstvTxCoordinatorSnapshot snapshot()const{return value;}struct SM {bool active()const{return false;}};SM stateMachine()const{return {};}};
}
struct Studio {bool busyValue=false,prepared=false,source=false;bool busy()const{return busyValue;}bool preparedReady()const{return prepared;}bool sourceReady()const{return source;}};
struct Device {bool null=false;bool isNull()const{return null;}};
struct Modulator {bool active=false;bool isActive()const{return active;}};
class DecodiumBridge {public:
 static QString tr(const char*s){return QString::fromUtf8(s);} Studio* m_sstvStudioController=nullptr;decodium::sstv::Coordinator* m_sstvTxCoordinator=nullptr;void *m_soundOutput=nullptr,*m_txAudioSink=nullptr,*m_txPcmBuffer=nullptr; Modulator* m_modulator=nullptr; Device m_sstvTxOutputDevice;unsigned m_sstvTxOutputChannels=2;
 bool m_sstvTxShuttingDown=false,m_shuttingDown=false,m_sstvOwnsBridgeTx=false,m_transmitting=false,m_ssbTxActive=false,m_tuning=false,m_txEnabled=false,m_autoCqRepeat=false,m_deferredManualSyncTx=false,m_txRequested=false,m_pttPending=false,m_pttConfirmed=false,m_bridgeAudioLegacyTxActive=false,m_ft2LinkTxActive=false,m_cwTxActive=false,m_manualTxHold=false,m_bridgeAudioTuneActive=false,m_legacyBridgeAudioTxStartPending=false,m_txPlaybackReleasePending=false,m_txAudioRestartPending=false,m_decoPortUseRemote=false,m_sstvTxOutputPinned=false,vox=false,ptt=false,control=false,tci=false; long m_txPlaybackHoldUntilMs=0,m_txPlaybackHardDeadlineMs=0;QString m_sstvTxState="Idle",m_sstvTxError;double m_sstvTxProgress=0.25;
 bool sstvTxUsesVoxPtt()const{return vox;}bool sstvTxPttActive()const{return ptt;}bool sstvTxCanControlPtt()const{return control;}bool usingTciAudioInput()const{return tci;}
 bool sstvTxCanStart()const;QString sstvTxUnavailableReason()const;QString sstvTxPreflightRejection()const;QStringList sstvTxPreflightBlockers()const;bool sstvTxGlobalPreflightReady()const;bool sstvTxAudioOnlyAllowed()const;QVariantMap sstvTxDiagnostics()const;bool sstvTxActive()const;QString sstvTxState()const;double sstvTxProgress()const noexcept;QString sstvTxError()const;
};
#include "bridge_refactor_sstv_tx_state.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;auto check=[](bool ok,const char* name){if(!ok){std::cerr<<name<<" failed\n";std::exit(1);}std::cout<<name<<'\n';};
#if DECODIUM_HAS_SSTV
 check(!b.sstvTxCanStart()&&b.sstvTxUnavailableReason().contains("not available"),"studio-missing");Studio studio;b.m_sstvStudioController=&studio;check(b.sstvTxUnavailableReason().contains("Load"),"source-missing");studio.source=true;check(b.sstvTxUnavailableReason().contains("Preview"),"preview-missing");studio.busyValue=true;check(b.sstvTxUnavailableReason().contains("Wait"),"busy");studio.busyValue=false;studio.prepared=true;check(!b.sstvTxPreflightBlockers().isEmpty()&&!b.sstvTxGlobalPreflightReady(),"preflight-missing");decodium::sstv::Coordinator co;b.m_sstvTxCoordinator=&co;b.m_soundOutput=&co;check(b.sstvTxCanStart()&&b.sstvTxGlobalPreflightReady()&&b.sstvTxAudioOnlyAllowed(),"ready-local");b.m_tuning=true;b.m_txEnabled=true;auto blockers=b.sstvTxPreflightBlockers();check(blockers.contains("the tune tone is active")&&blockers.contains("Enable TX is armed for another mode")&&!b.sstvTxCanStart(),"multiple-blockers");check(b.sstvTxPreflightRejection().contains("tune tone"),"rejection");b.m_tuning=false;b.m_txEnabled=false;co.value.clippedFrames=2;auto d=b.sstvTxDiagnostics();check(d.value("clipping").toBool()&&d.value("preflightReady").toBool(),"diagnostics");b.m_sstvOwnsBridgeTx=true;check(b.sstvTxActive(),"active");check(b.sstvTxState()=="Idle"&&b.sstvTxProgress()==0.25&&b.sstvTxError().isEmpty(),"state");b.tci=true;check(!b.sstvTxAudioOnlyAllowed()&&b.sstvTxPreflightBlockers().contains("audio is coming from TCI"),"tci-blocked");
#else
 check(!b.sstvTxCanStart()&&!b.sstvTxGlobalPreflightReady()&&!b.sstvTxAudioOnlyAllowed(),"disabled-ready");check(b.sstvTxDiagnostics().isEmpty()&&!b.sstvTxActive()&&b.sstvTxState()=="Disabled"&&b.sstvTxProgress()==0.0&&b.sstvTxError().isEmpty(),"disabled-state");check(b.sstvTxUnavailableReason().contains("not available"),"disabled-reason");
#endif
}
