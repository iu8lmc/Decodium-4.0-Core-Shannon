#!/usr/bin/env python3
"""Compare JT/FST4 fallback dispatch guards and duplicate handling."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/jtdecodefallback_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeJtDecodeFallback.cpp').read_text();mark='void DecodiumBridge::maybeDispatchBridgeOwnedJtDecodeFallback()';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeOwnsLegacyJtRxWhenCatSuppressed(', 'bridgeOwnsLegacyJtRxWhenCatSuppressed(').replace('decodiumBridgeOwnsFst4Audio(', 'bridgeOwnsFst4Audio(')
guard='''    // These common FT modes cannot use the JT/FST4 fallback. Avoid repeated
    // environment lookups and string normalization on every spectrum tick.
    if (m_mode == QStringLiteral("FT8") || m_mode == QStringLiteral("FT4")
        || m_mode == QStringLiteral("FT2")) {
        return;
    }
'''
assert new.count(guard)==1 and new.replace(guard,'')==old
source=r'''
#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include <QDebug>
#include <iostream>
bool bridgeOwnsLegacyJtRxWhenCatSuppressed(const QString&m){return m=="JT65";}bool bridgeOwnsFst4Audio(const QString&m){return m=="FST4";}
class DecodiumBridge{public:QString m_mode="JT65",m_bridgeOwnedJtFallbackLastMode;bool m_monitoring=true,m_rxAudioSuspendedForTx=false,m_transmitting=false,m_tuning=false,m_pendingTimeSyncDecodeActive=false;int period=10000,target=10,minimum=5,dispatches=0;long long now=21500,m_bridgeOwnedJtFallbackLastSlot=-1,m_bridgeOwnedJtFallbackLastDispatchMs=0;quint64 m_periodTimerSessionId=7;QMutex m_audioBufferMutex;QVector<short>m_audioBuffer=QVector<short>(5);int effectivePeriodMsForMode(const QString&){return period;}int targetDecodeSamplesForMode(const QString&){return target;}int minimumDecodeSamplesForMode(const QString&){return minimum;}qint64 correctedUtcEpochMs(){return now;}void dispatchTimeSyncDecodeWhenReady(qint64,const QString&,quint64,qint64){dispatches++;}void maybeDispatchBridgeOwnedJtDecodeFallback();};
#include "methods.inc"
int main(){for(int k=0;k<12;k++){DecodiumBridge b;if(k==1)b.m_mode="FT8";if(k==2)b.m_monitoring=false;if(k==3)b.m_rxAudioSuspendedForTx=true;if(k==4)b.m_transmitting=true;if(k==5)b.m_tuning=true;if(k==6)b.m_pendingTimeSyncDecodeActive=true;if(k==7)b.now=21000;if(k==8)b.now=29500;if(k==9)b.m_audioBuffer.clear();if(k==10)b.m_mode="FT4";if(k==11)b.m_mode="FT2";b.maybeDispatchBridgeOwnedJtDecodeFallback();std::cout<<b.dispatches;}std::cout<<'\n';DecodiumBridge b;b.maybeDispatchBridgeOwnedJtDecodeFallback();b.maybeDispatchBridgeOwnedJtDecodeFallback();std::cout<<b.dispatches<<':'<<b.m_bridgeOwnedJtFallbackLastSlot<<':'<<b.m_bridgeOwnedJtFallbackLastMode.toStdString()<<'\n';b.now=31500;b.m_mode="FST4";b.maybeDispatchBridgeOwnedJtDecodeFallback();std::cout<<b.dispatches<<':'<<b.m_bridgeOwnedJtFallbackLastSlot<<':'<<b.m_bridgeOwnedJtFallbackLastMode.toStdString()<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-jtfallback-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True,stderr=subprocess.DEVNULL))
assert outputs[0]==outputs[1]=='100000000000\n1:1:JT65\n2:2:FST4\n',outputs
print('JT/FST4 fallback A/B: mode, state, timing, audio and duplicate guards passed')
