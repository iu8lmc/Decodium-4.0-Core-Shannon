#!/usr/bin/env python3
"""Compare delayed legacy bridge audio start gating."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/legacyaudiostart_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeLegacyAudioStart.cpp').read_text();mark='quint64 DecodiumBridge::armPendingLegacyBridgeAudioStart(';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeLegacyAudioStartLog(', 'bridgeLog(');assert old==new
source=r'''
#include <QCoreApplication>
#include <QString>
#include <iostream>
void bridgeLog(const QString&){}
class DecodiumBridge{public:bool m_legacyBridgeAudioTxStartPending=false,m_shuttingDown=false,m_txEnabled=true,m_deferredManualSyncTx=false,m_transmitting=false,bridge=true;quint64 m_legacyBridgeAudioTxStartSerial=0;bool shouldUseBridgeAudioForLegacyDigitalTx()const{return bridge;}quint64 armPendingLegacyBridgeAudioStart(const QString&);bool consumePendingLegacyBridgeAudioStart(quint64,const QString&);void cancelPendingLegacyBridgeAudioStart(const QString&);};
#include "methods.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(int k=0;k<7;k++){DecodiumBridge b;auto serial=b.armPendingLegacyBridgeAudioStart("test");if(k==1)serial++;if(k==2)b.m_shuttingDown=true;if(k==3)b.bridge=false;if(k==4)b.m_txEnabled=false;if(k==5){b.m_txEnabled=false;b.m_deferredManualSyncTx=true;}if(k==6){b.m_txEnabled=false;b.m_transmitting=true;}bool ok=b.consumePendingLegacyBridgeAudioStart(serial,"test");std::cout<<ok<<':'<<b.m_legacyBridgeAudioTxStartPending<<':'<<b.m_legacyBridgeAudioTxStartSerial<<'\n';}DecodiumBridge b;auto serial=b.armPendingLegacyBridgeAudioStart("test");b.cancelPendingLegacyBridgeAudioStart("test");std::cout<<b.consumePendingLegacyBridgeAudioStart(serial,"test")<<':'<<b.m_legacyBridgeAudioTxStartPending<<':'<<b.m_legacyBridgeAudioTxStartSerial<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-legacyaudio-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='1:0:1\n0:1:1\n0:0:1\n0:0:1\n0:0:1\n1:0:1\n1:0:1\n0:0:2\n',outputs
print('Legacy delayed audio A/B: valid, stale, shutdown, TX authorization and cancellation passed')
