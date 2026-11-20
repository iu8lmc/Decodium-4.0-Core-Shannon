#!/usr/bin/env python3
"""Compare legacy audio ownership, selected message and in-flight guards."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/legacyaudiopolicy_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeLegacyAudioPolicy.cpp').read_text();mark='bool DecodiumBridge::shouldUseBridgeAudioForLegacyDigitalTx() const';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QDateTime>
#include <QStringList>
#include <iostream>
class DecodiumBridge{public:QString m_mode="FT8";bool legacy=true,tci=false,special=false,m_cwTxActive=false,multi=false,m_bridgeAudioLegacyTxActive=false,m_transmitting=false,m_tuning=false;QStringList m_mamMessages{" first "};qint64 m_txPlaybackHoldUntilMs=0,m_txPlaybackHardDeadlineMs=0;void*m_txPcmBuffer=nullptr,*m_txAudioSink=nullptr;bool usingLegacyBackendForTx()const{return legacy;}bool usingTciAudioInput()const{return tci;}bool specialOperationRequiresLegacyTx()const{return special;}bool multiStreamActive()const{return multi;}QString buildCurrentTxMessage()const{return "normal";}bool shouldUseBridgeAudioForLegacyDigitalTx()const;QString currentBridgeTxRepresentativeMessage()const;bool legacyBridgeAudioTxInFlight()const;};
#include "methods.inc"
int main(){DecodiumBridge b;for(auto mode:{"FT8","FT4","FT2","JT65","MSK144","Q65","Q65-60A","CW"}){b.m_mode=mode;std::cout<<b.shouldUseBridgeAudioForLegacyDigitalTx();}std::cout<<'\n';b.m_mode=" ft8 ";b.legacy=false;std::cout<<b.shouldUseBridgeAudioForLegacyDigitalTx();b.legacy=true;b.tci=true;std::cout<<b.shouldUseBridgeAudioForLegacyDigitalTx();b.tci=false;b.special=true;std::cout<<b.shouldUseBridgeAudioForLegacyDigitalTx()<<'\n';b.multi=true;std::cout<<b.currentBridgeTxRepresentativeMessage().toStdString()<<':';b.m_cwTxActive=true;std::cout<<b.currentBridgeTxRepresentativeMessage().toStdString()<<'\n';std::cout<<b.legacyBridgeAudioTxInFlight();b.m_txPcmBuffer=&b;b.m_txPlaybackHoldUntilMs=QDateTime::currentMSecsSinceEpoch()+10000;std::cout<<b.legacyBridgeAudioTxInFlight();b.m_txPlaybackHoldUntilMs=0;b.m_transmitting=true;std::cout<<b.legacyBridgeAudioTxInFlight();b.m_transmitting=false;b.m_txPcmBuffer=nullptr;b.m_bridgeAudioLegacyTxActive=true;std::cout<<b.legacyBridgeAudioTxInFlight()<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-legacyaudiopolicy-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='11111110\n000\nfirst:normal\n0111\n',outputs
print('Legacy audio policy A/B: modes, backend guards, message choice and in-flight windows passed')
