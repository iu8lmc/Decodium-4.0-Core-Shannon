#!/usr/bin/env python3
"""Compare HALT lifecycle with simulated legacy and native collaborators."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/halt_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeHalt.cpp').read_text();mark='void DecodiumBridge::halt()';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0]
added='''    // Direct CW, FT2-Link and telemetry audio can be owned by the bridge
    // while the legacy backend handles ordinary digital TX.
    bool const bridgeOwnedLegacyAudio = usingLegacyBackendForTx()
        && (m_cwTxActive || m_ft2LinkTxActive || m_telemetryTxActive);
    if (bridgeOwnedLegacyAudio) {
        stopTx();
    }

'''
assert old==new.replace(added,'').replace('usingLegacyBackendForTx() && !bridgeOwnedLegacyAudio','usingLegacyBackendForTx()')
source=r'''
#include <QString>
#include <QStringList>
#include <QVector>
#include <iostream>
#ifdef TEST_NONMAC
#undef Q_OS_MAC
#endif
#define emit
QStringList events;void bridgeLog(const QString&s){events<<"log:"+s;}struct Legacy{void stopTransmission(){events<<"legacy-stop";}void startTune(bool){events<<"legacy-tune-off";}};struct Mod{bool active=true;bool isActive(){return active;}};
class DecodiumBridge{public:QVector<int>m_mamSlots={1};bool m_transmitting=true,m_tuning=true,m_bridgeAudioTuneActive=true,m_bridgeAudioLegacyTxActive=true,legacy=true,txEnabled=true,m_cwTxActive=false,m_ft2LinkTxActive=false,m_telemetryTxActive=false;Mod mod,*m_modulator=&mod;Legacy backend,*m_legacyBackend=&backend;
 void invalidateDeferredTxRequests(const QString&){events<<"invalidate";}void engageManualTxHold(const QString&,bool){events<<"hold";}void clearDeferredManualSyncTx(const QString&){events<<"clear-deferred";}void clearPendingAutoSeqTx(const QString&){events<<"clear-seq";}void clearMamPendingTxPayload(const QString&){events<<"clear-mam";}void mamActiveSlotsChanged(){events<<"mam-notify";}void cancelPendingLegacyBridgeAudioStart(const QString&){events<<"cancel-legacy";}bool usingLegacyBackendForTx(){return legacy;}void abortLegacyBridgeTxRequest(const QString&){events<<"ptt-abort";}void stopBridgeAudioForLegacyDigitalTx(const QString&){events<<"audio-stop";}void stopTune(){events<<"stop-tune";}void stopTx(){events<<"stop-tx";m_cwTxActive=false;m_ft2LinkTxActive=false;m_telemetryTxActive=false;}void transmittingChanged(){events<<"tx-notify";}void tuningChanged(){events<<"tune-notify";}void setTxEnabled(bool b){txEnabled=b;events<<"disable";}void scheduleLegacyStateRefreshBurst(){events<<"refresh";}void halt();void haltWithReason(const QString&);};
#include "methods.inc"
void show(const char*n,DecodiumBridge&b){std::cout<<n<<':'<<b.m_mamSlots.size()<<':'<<b.m_transmitting<<':'<<b.m_tuning<<':'<<b.txEnabled<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge a;a.halt();show("legacy",a);DecodiumBridge b;b.legacy=false;b.m_bridgeAudioTuneActive=false;b.m_bridgeAudioLegacyTxActive=false;b.m_tuning=false;b.m_mamSlots.clear();b.haltWithReason(" test ");show("native",b);DecodiumBridge c;c.m_cwTxActive=true;c.halt();show("cw",c);DecodiumBridge d;d.m_ft2LinkTxActive=true;d.halt();show("ft2",d);DecodiumBridge e;e.m_telemetryTxActive=true;e.halt();show("telemetry",e);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-halt-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source)
 for variant in ('mac','other'):
  out=[]
  for body in (old,new):
   (p/'methods.inc').write_text(body);defs=['-DTEST_NONMAC'] if variant=='other' else [];subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',*defs,str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
  baseline=out[0].splitlines();lines=out[1].splitlines();assert len(lines)==5 and baseline[:2]==lines[:2];assert all('stop-tx' not in baseline[i] and 'stop-tx' in lines[i] for i in (2,3,4))
  assert lines[0].startswith('legacy:0:0:0:0|invalidate;hold;clear-deferred;clear-seq;clear-mam;mam-notify;') and 'legacy-stop;legacy-tune-off' in lines[0]
  if variant=='mac':assert 'cancel-legacy;ptt-abort;' in lines[0] and 'audio-stop' in lines[0] and 'stop-tune' in lines[0]
  assert 'stop-tx;stop-tune;disable' in lines[1] and 'mam-notify' not in lines[1]
print('HALT A/B: request cancellation, MAM cleanup, legacy PTT abort and native stop passed')
