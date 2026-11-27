#!/usr/bin/env python3
"""A/B frequency changes with controlled rig and audio collaborators."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/frequencyupdate_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeFrequencyUpdate.cpp').read_text();mark='void DecodiumBridge::setFrequency(double v)';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QString>
#include <QStringList>
#include <QVariant>
#include <cmath>
#include <iostream>
#define emit
namespace Radio {using Frequency=long long;using FrequencyDelta=long long;}
struct Bands{int find(Radio::Frequency f){return f>0?1:0;}};
QStringList events;struct Remote{bool linked=true;double freq=0;bool isLinked(){return linked;}double frequencyHz(){return freq;}void tune(double f){events<<QString("remote:%1").arg(f,0,'f',0);}};struct Legacy{double f=0;double dialFrequency(){return f;}void setDialFrequency(double v){f=v;events<<QString("legacy:%1").arg(v,0,'f',0);}};struct BandManager{void updateFromFrequency(double){events<<"band";}};
class DecodiumBridge{public:bool m_decoPortUseRemote=false,m_decoPortApplyingRemote=false,m_monitoring=false,m_monitorRequested=false,legacy=false,rtl=false,follow=true;quint64 m_periodTimerSessionId=12;double m_frequency=14074000;int m_currentCatLogicalBand=0;Radio::FrequencyDelta m_currentCatStationOffsetHz=0;Remote remote;Legacy backend,*m_legacyBackend=&backend;BandManager bands,*m_bandManager=&bands;
 Remote* remoteLink(){return &remote;}bool legacyBackendAvailable(){return legacy;}bool rtlSdrEnabled(){return rtl;}QVariant getSetting(const QString&,bool){return follow;}double stationOffsetForFrequencyHz(double){return 100;}void frequencyChanged(){events<<"changed";}void clearDecodeWindowsForBandChange(double,double,const QString&){events<<"clear";}void scheduleMonitorRecovery(const QString&,quint64,bool active){events<<QString("recovery:%1").arg(active);}void scheduleRtlSdrRetune(){events<<"rtl-retune";}void setFrequency(double);};
#include "methods.inc"
void show(const char*n,DecodiumBridge&b){std::cout<<n<<':'<<QString::number(b.m_frequency,'f',0).toStdString()<<':'<<b.m_currentCatLogicalBand<<':'<<b.m_currentCatStationOffsetHz<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge b;b.m_monitoring=true;b.legacy=true;b.backend.f=14074000;b.setFrequency(14074000);show("same",b);b.setFrequency(14075000);show("small",b);b.setFrequency(14200000);show("large",b);b.rtl=true;b.setFrequency(14400000);show("rtl",b);b.follow=false;b.setFrequency(14600000);show("nofollow",b);b.m_decoPortUseRemote=true;b.setFrequency(14700000);show("remote",b);b.m_decoPortApplyingRemote=true;b.setFrequency(14800000);show("remote-echo",b);b.legacy=false;b.setFrequency(-1);show("invalid",b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-frequencyupdate-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==8
assert lines[0].endswith('|band') and 'changed;legacy:14075000;band' in lines[1]
assert 'changed;clear;legacy:14200000;band;recovery:1' in lines[2]
assert 'rtl-retune' in lines[3] and 'recovery:' not in lines[3] and 'rtl-retune' not in lines[4]
assert '|remote:14700000;' in lines[5] and '|remote:' not in lines[6] and '|remote:' not in lines[7]
print('Frequency update A/B: legacy, large/small QSY, RTL follow and remote echo suppression passed')
