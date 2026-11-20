#!/usr/bin/env python3
"""Compare producer tap shutdown with real Qt signal connections."""
import os, shlex, subprocess, tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def body(source, name):
    start=source.index('void DecodiumBridge::'+name+'()')
    return source[start:source.index('\n}',start)+2]
old=(R/'tests/bridge_refactor_fixtures/sstv_producer_taps_before.inc').read_text()
new=(R/'src/bridge/DecodiumBridgeSstvProducerTaps.cpp').read_text()
harness=r'''#include <QtCore>
#include <cstdio>
#define DECODIUM_HAS_SSTV 1
struct Relay { int calls=0; void disableAndDrain(){++calls;} };
class DecodiumBridge : public QObject { public:
 QMetaObject::Connection m_sstvLocalAudioTap,m_sstvLegacyAudioTap,m_sstvTciAudioTap,m_sstvDecoPortAudioTap,m_sstvRtlPcmTap,m_sstvRtlAudioTap;
 Relay* m_sstvAudioRelay=nullptr;
 void disconnectSstvProducerTaps(); void disableSstvProducerTaps();
};
#include "taps.inc"
int main(int argc,char** argv){ QCoreApplication app(argc,argv); QTimer sender; DecodiumBridge bridge; Relay relay; int calls=0;
 auto bind=[&](){return QObject::connect(&sender,&QTimer::timeout,&bridge,[&](){++calls;});};
 bridge.m_sstvLocalAudioTap=bind();bridge.m_sstvLegacyAudioTap=bind();bridge.m_sstvTciAudioTap=bind();bridge.m_sstvDecoPortAudioTap=bind();bridge.m_sstvRtlPcmTap=bind();bridge.m_sstvRtlAudioTap=bind();
 QMetaObject::invokeMethod(&sender,"timeout"); if(calls!=6)return 1;
 bridge.m_sstvAudioRelay=&relay;bridge.disableSstvProducerTaps(); QMetaObject::invokeMethod(&sender,"timeout");
 if(calls!=6||relay.calls!=1||bridge.m_sstvLocalAudioTap||bridge.m_sstvLegacyAudioTap||bridge.m_sstvTciAudioTap||bridge.m_sstvDecoPortAudioTap||bridge.m_sstvRtlPcmTap||bridge.m_sstvRtlAudioTap)return 2;
 bridge.disableSstvProducerTaps();if(relay.calls!=2)return 3;
 bridge.m_sstvAudioRelay=nullptr;bridge.m_sstvLocalAudioTap=bind();bridge.disconnectSstvProducerTaps();QMetaObject::invokeMethod(&sender,"timeout");if(calls!=6)return 4;
 std::printf("six disconnected; repeat drained twice; null relay safe\n");
}'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-sstv-taps-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text(harness);out=[]
 for source in (old,new):
  (d/'taps.inc').write_text('\n'.join(body(source,n) for n in ('disconnectSstvProducerTaps','disableSstvProducerTaps')))
  subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(d),str(d/'test.cpp'),*flags,'-o',str(d/'test')],check=True)
  out.append(subprocess.check_output([str(d/'test')],text=True))
 if out[0]!=out[1]:raise RuntimeError('A/B output differs')
 print('SSTV tap shutdown A/B: '+out[0].strip())
