#!/usr/bin/env python3
"""Compare remote radio state synchronization and telemetry masks."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/remoteradiostate_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeRemoteRadioState.cpp').read_text();mark='void DecodiumBridge::onDecoPortRemoteState()';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QString>
#include <cmath>
#include <iostream>
#ifndef DECODIUM_HAS_SSTV
#define DECODIUM_HAS_SSTV 0
#endif
namespace decodium { namespace sstv { enum class SstvAudioSourceKind { DecoPort }; } }
namespace decoport{enum{FieldForwardPower=1,FieldSwr=2,FieldAlc=4};}
struct State{int mask=0;double forwardPowerW()const{return 25;}double swr()const{return 2;}double alcPct()const{return .5;}};struct Link{bool linked=false,keyed=false;double freq=0;QString mode,name;State st;bool isLinked(){return linked;}QString modeName(){return mode;}QString rigLabel(){return name;}State state(){return st;}bool ptt(){return keyed;}double frequencyHz(){return freq;}};struct Band{int updates=0;void updateFromFrequency(double){updates++;}};
class DecodiumBridge{public:bool m_decoPortUseRemote=false,m_catConnected=false,m_decoPortApplyingRemote=false;QString m_catMode,m_catRigName;double m_frequency=100;Link*link=nullptr;Band*m_bandManager=nullptr;int connected=0,mode=0,name=0,telemetry=0,frequency=0,selected=0;unsigned stream=0,nextStream=77;unsigned currentSstvAudioStreamId(decodium::sstv::SstvAudioSourceKind){return nextStream;}void selectSstvRxSource(decodium::sstv::SstvAudioSourceKind,unsigned id){selected++;stream=id;}double p=0,w=0,a=0;bool valid=false;Link*remoteLink(){return link;}void catConnectedChanged(){connected++;}void catModeChanged(){mode++;}void catRigNameChanged(){name++;}void updateRigTelemetry(double x,double y,double z,bool v){p=x;w=y;a=z;valid=v;telemetry++;}void setFrequency(double f){m_frequency=f;frequency++;}void onDecoPortRemoteState();};
#include "methods.inc"
int main(){DecodiumBridge b;Link l;Band band;b.link=&l;b.m_bandManager=&band;b.onDecoPortRemoteState();b.m_decoPortUseRemote=true;b.onDecoPortRemoteState();std::cout<<b.telemetry<<':'<<b.frequency<<'\n';l.linked=true;l.mode="USB";l.name="remote";l.freq=100.5;b.onDecoPortRemoteState();std::cout<<b.connected<<':'<<b.mode<<':'<<b.name<<':'<<b.telemetry<<':'<<b.frequency<<':'<<b.p<<':'<<b.w<<':'<<b.valid<<'\n';l.st.mask=7;l.keyed=true;l.freq=200;b.nextStream=88;b.onDecoPortRemoteState();std::cout<<b.connected<<':'<<b.mode<<':'<<b.name<<':'<<b.telemetry<<':'<<b.frequency<<':'<<b.p<<':'<<b.w<<':'<<b.a<<':'<<b.valid<<':'<<band.updates<<':'<<b.m_decoPortApplyingRemote<<':'<<b.selected<<':'<<b.stream<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-remotestate-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source)
 for enabled in (0,1):
  outputs=[]
  for body in (old,new):
   (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',f'-DDECODIUM_HAS_SSTV={enabled}',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
  expected=f"0:0\n1:1:1:1:0:0:0:0\n1:1:1:2:1:25:2:0.5:1:1:0:{2 if enabled else 0}:{88 if enabled else 0}\n"
  assert outputs[0]==outputs[1]==expected,outputs
print('Remote state A/B: connection, telemetry, frequency and SSTV stream selection with SSTV off/on passed')
