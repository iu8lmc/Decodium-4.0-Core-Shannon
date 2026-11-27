#!/usr/bin/env python3
"""Exercise TX audio failure and cache paths without opening an audio device."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/txprepare_before.inc').read_text();new='bool DecodiumBridge::ensureTxAudioPrepared('+(r/'src/bridge/DecodiumBridgeTxPrepare.cpp').read_text().split('bool DecodiumBridge::ensureTxAudioPrepared(',1)[1]
for a,b in [('decodiumBridgeTxPreparePcm(wave, format, m_audioOutputChannel)','buildTxPcmBuffer(wave, format, boundedAudioChannel(m_audioOutputChannel))'),('decodiumBridgeTxPrepareTimelineLog(', 'txTimelineLog('),('decodiumBridgeTxPrepareAudioFormat(', 'audioFormatToString('),('decodiumBridgeTxPrepareSameFormat(', 'sameAudioFormat('),('decodiumBridgeTxPrepareChooseFormat(', 'chooseTxAudioFormat('),('decodiumBridgeAudioDeviceIdSettingForLog(', 'audioDeviceIdSettingForLog('),('decodiumBridgeAudioDeviceIdForSettings(', 'audioDeviceIdForSettings('),('decodiumBridgeAudioLevelLog(', 'bridgeLog(')]:new=new.replace(a,b)
if old!=new:raise SystemExit('TX preparation differs')
src=r'''
#include <QString>
#include <QStringList>
#include <QVector>
#include <QByteArray>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QElapsedTimer>
#include <iostream>
#include "src/bridge/MamTxPayloadPolicy.h"
static bool pcmFails=false;static int builds=0, fallbackLogs=0;
void txTimelineLog(QString const&){} void bridgeLog(QString const&s){if(s.contains("fallback to default"))++fallbackLogs;}
QString audioFormatToString(QAudioFormat const&){return "fmt";}bool sameAudioFormat(QAudioFormat const&a,QAudioFormat const&b){return a==b;}
QAudioFormat chooseTxAudioFormat(QAudioDevice const&){QAudioFormat f;f.setSampleRate(48000);f.setChannelCount(1);f.setSampleFormat(QAudioFormat::Int16);return f;}
QString audioDeviceIdForSettings(QAudioDevice const&){return {};}QString audioDeviceIdSettingForLog(QString s){return s;}int boundedAudioChannel(int n){return n;}
QByteArray buildTxPcmBuffer(QVector<float> const&,QAudioFormat const&,int){return pcmFails?QByteArray{}:QByteArray("PCM");}
QVector<float> buildTxWaveformForMessage(QString const&,QString const& m,int,double,QString*e){++builds;if(m=="BAD"){*e="bad wave";return {};}return {0.1f,0.2f};}
QVector<float> generateMultiStreamFtxWave(QString const&,QStringList const&,QVector<int> const&,QString*,QVector<float> const&){return {0.3f};}
namespace decodium { namespace txwave { QVector<float> generateCwWaveWpm(QString const&,int,int){return {0.4f};} } }
struct Cache {QString mode,message,outputDeviceName,outputDeviceDescription;int txAudioFrequency=0,periodMs=0,outputChannel=0;bool tciAudio=false,multiStream=false;QAudioFormat outputFormat;QVector<float>wave;QByteArray pcm;};
class DecodiumBridge{public:
 bool m_ft2LinkTxActive=false,m_cwTxActive=false,m_telemetryTxActive=false,m_mamPowerBudget=false,m_cachedTxOutputDeviceValid=false;QString m_mode="FT2",m_audioOutputDevice="missing",m_audioOutputDeviceId,m_cachedTxOutputDeviceName,m_cachedTxOutputDeviceId;int m_cwWpm=20,m_audioOutputChannel=0,invalidations=0;QAudioDevice m_cachedTxOutputDevice;QVector<float>m_pendingFt2LinkWave,m_mamGains;QStringList m_mamMessages;QVector<int>m_mamF0sHz;Cache m_txAudioCache;
 bool usingTciAudioInput()const{return false;}int effectivePeriodMsForMode(QString const&)const{return 3750;}bool multiStreamActive()const{return false;}QAudioDevice resolveTxOutputDevice(bool*found){*found=false;return {};}
 void invalidateTxAudioCache(){m_txAudioCache=Cache{};++invalidations;}void scheduleIdleAudioBufferRelease(int){}
 bool ensureTxAudioPrepared(QString const&,int,bool,QVector<float>*,QByteArray*,QAudioFormat*,QAudioDevice*,QString*);
};
#include "method.inc"
int main(){DecodiumBridge b;QString e;auto run=[&](QString m,bool pcm){e.clear();bool ok=b.ensureTxAudioPrepared(m,1500,pcm,nullptr,nullptr,nullptr,nullptr,&e);std::cout<<ok<<':'<<e.toStdString()<<':'<<builds<<':'<<b.invalidations<<':'<<fallbackLogs<<'\n';};run("BAD",false);pcmFails=true;run("GOOD",true);pcmFails=false;run("GOOD",true);run("GOOD",true);b.invalidateTxAudioCache();run("GOOD",false);run("GOOD",false);b.m_mode="FT4";run("GOOD",false);b.m_ft2LinkTxActive=true;run("LINK",false);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Multimedia'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-txprepare-errors-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(r),str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
if outputs[0]!=outputs[1]:raise SystemExit('TX preparation errors A/B mismatch')
lines=outputs[0].splitlines();assert len(lines)==8 and lines[0].startswith('0:bad wave:1:1:0') and lines[1].startswith('0:Audio TX:') and lines[2].startswith('1::3:2:2') and lines[3].startswith('1::3:2:2') and lines[5].startswith('1::4:3:2') and lines[6].startswith('1::5:3:2') and 'Piano TX FT2-Link vuoto' in lines[7],outputs[0]
print('TX preparation A/B OK: bad waveform, missing device, PCM failure, fallback, cache hit and empty FT2-Link plan')
