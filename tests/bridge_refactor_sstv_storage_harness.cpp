#include <QtCore>
#include <cstdio>
struct Job {bool running=false;QString stateName()const{return "job";}QString lastError()const{return "job-error";}bool busy()const{return running;}};
struct Replay {bool running=false;bool active()const{return running;}QString stateName()const{return "replay";}double progress()const{return .5;}QString fileName()const{return "fixture.wav";}QString lastError()const{return "replay-error";}};
class DecodiumBridge:public QObject {public:
 Job* m_sstvRxAudioJobController=nullptr;Replay* m_sstvWavReplayController=nullptr;bool m_sstvStorageReady=false,m_sstvRxAutoSaveEnabled=false;QString m_sstvRxRawAudioPath="raw.wav",m_sstvRxSaveState="idle",m_sstvRxSaveError;int changes=0,saves=0,settings=0;
 void setSstvRxSaveStatus(const QString&a,const QString&b){m_sstvRxSaveState=a;m_sstvRxSaveError=b;}void setSetting(const QString&,bool){++settings;}void sstvRxAutoSaveChanged(){++changes;}void maybeAutoSaveSstvRxImage(){++saves;}
 bool sstvRxAudioJobBusy()const noexcept;QString sstvRxAudioJobState()const;QString sstvRxAudioJobError()const;QString sstvRxRawAudioPath()const;
 bool sstvStorageReady()const noexcept;bool sstvRxAutoSaveEnabled()const noexcept;void setSstvRxAutoSaveEnabled(bool);QString sstvRxSaveState()const;QString sstvRxSaveError()const;
 bool sstvWavReplayActive()const;QString sstvWavReplayState()const;double sstvWavReplayProgress()const noexcept;QString sstvWavReplayFileName()const;QString sstvWavReplayError()const;
};
#include "bridge_refactor_sstv_storage.inc"
static void check(bool ok,const char* name){if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}puts(name);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;
 check(!b.sstvStorageReady()&&!b.sstvRxAutoSaveEnabled()&&!b.sstvRxAudioJobBusy()&&!b.sstvWavReplayActive(),"defaults");
#if DECODIUM_HAS_SSTV
 check(b.sstvRxAudioJobState()=="Unavailable"&&b.sstvWavReplayState()=="Unavailable"&&b.sstvRxRawAudioPath()=="raw.wav","unavailable");
 b.setSstvRxAutoSaveEnabled(true);check(!b.sstvRxAutoSaveEnabled()&&b.sstvRxSaveState()=="error"&&!b.sstvRxSaveError().isEmpty(),"not-ready");
 b.m_sstvStorageReady=true;b.setSstvRxAutoSaveEnabled(true);check(b.sstvRxAutoSaveEnabled()&&b.settings==1&&b.changes==1&&b.saves==1,"enable");b.setSstvRxAutoSaveEnabled(true);check(b.changes==1&&b.saves==1,"unchanged");b.setSstvRxAutoSaveEnabled(false);check(!b.sstvRxAutoSaveEnabled()&&b.settings==2&&b.changes==2&&b.saves==1,"disable");
 Job job;job.running=true;b.m_sstvRxAudioJobController=&job;Replay replay;replay.running=true;b.m_sstvWavReplayController=&replay;check(b.sstvRxAudioJobBusy()&&b.sstvRxAudioJobState()=="job"&&b.sstvRxAudioJobError()=="job-error","job");check(b.sstvWavReplayActive()&&b.sstvWavReplayState()=="replay"&&b.sstvWavReplayProgress()==.5&&b.sstvWavReplayFileName()=="fixture.wav"&&b.sstvWavReplayError()=="replay-error","replay");
#else
 b.setSstvRxAutoSaveEnabled(true);check(!b.sstvRxAutoSaveEnabled()&&b.sstvRxSaveState()=="idle"&&b.sstvRxRawAudioPath().isEmpty()&&b.sstvWavReplayState()=="Unavailable"&&b.changes==0,"sstv-disabled");
#endif
}
