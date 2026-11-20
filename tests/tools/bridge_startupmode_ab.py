#!/usr/bin/env python3
"""Compare startup mode choice and guards with controlled CAT state."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/startupmode_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeStartupMode.cpp').read_text();mark='QString DecodiumBridge::startupModeForFrequency(';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QString>
#include <QStringList>
#include <iostream>
struct FakeDateTime{static qint64 currentMSecsSinceEpoch(){return 1000;}};
#define QDateTime FakeDateTime
QStringList events;void bridgeLog(const QString&s){events<<s;}
struct BandManager{QString result="FT8";QString detectModeForFrequency(double)const{return result;}};
class DecodiumBridge{public:BandManager bands,*m_bandManager=&bands;long long m_startupModeAutoUntilMs=2000;bool m_startupModeAutoPending=true,m_startupModeAutoAuthoritativeApplied=false,m_transmitting=false,m_tuning=false,m_preserveFrequencyOnModeChange=false;QString m_mode="FT4";QString startupModeForFrequency(double)const;void maybeApplyStartupModeFromRigFrequency(double,bool);void setMode(const QString&s){events<<QString("set:%1:preserve=%2").arg(s).arg(m_preserveFrequencyOnModeChange);m_mode=s;}};
#include "methods.inc"
void show(const char*n,DecodiumBridge&b){std::cout<<n<<':'<<b.m_mode.toStdString()<<':'<<b.m_startupModeAutoPending<<':'<<b.m_startupModeAutoAuthoritativeApplied<<':'<<b.m_preserveFrequencyOnModeChange<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(){DecodiumBridge b;b.maybeApplyStartupModeFromRigFrequency(0,false);show("invalid",b);b.m_startupModeAutoUntilMs=999;b.maybeApplyStartupModeFromRigFrequency(100,false);show("expired",b);b.m_startupModeAutoUntilMs=2000;b.m_startupModeAutoPending=true;b.bands.result="";b.maybeApplyStartupModeFromRigFrequency(100,false);show("unknown",b);b.bands.result="FT8";b.maybeApplyStartupModeFromRigFrequency(100,false);show("pending",b);b.m_mode="FT4";b.m_transmitting=true;b.maybeApplyStartupModeFromRigFrequency(100,true);show("tx",b);b.m_transmitting=false;b.m_startupModeAutoAuthoritativeApplied=false;b.m_preserveFrequencyOnModeChange=true;b.maybeApplyStartupModeFromRigFrequency(100,true);show("authoritative",b);b.m_mode="FT4";b.maybeApplyStartupModeFromRigFrequency(100,true);show("once",b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-startupmode-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];lines=out[0].splitlines();assert len(lines)==7
assert lines[0].startswith('invalid:FT4:1:0:0|') and lines[1].startswith('expired:FT4:0:0:0|') and lines[2].startswith('unknown:FT4:1:0:0|')
assert lines[3].startswith('pending:FT8:0:0:0|') and 'set:FT8:preserve=1' in lines[3]
assert lines[4].startswith('tx:FT4:0:1:0|') and 'deferred during TX' in lines[4]
assert lines[5].startswith('authoritative:FT8:0:1:1|') and 'set:FT8:preserve=1' in lines[5] and lines[6]=='once:FT4:0:1:1|'
print('Startup mode A/B: expiry, pending/authoritative selection, TX deferral and frequency preservation passed')
