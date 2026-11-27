#!/usr/bin/env python3
"""Compare manual NTP sync behavior with a simulated NTP client."""
import os,shlex,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/ntp_manual_before.inc').read_text()
source=(root/'src/bridge/DecodiumBridgeTimeSync.cpp').read_text()
new=source[source.index('void DecodiumBridge::syncNtpNow()'):].replace('decodiumBridgeNtpLog(', 'bridgeLog(')
assert old==new
harness=r'''
#include <QtCore>
#include <iostream>
QStringList events;
void bridgeLog(const QString& s){events<<"log:"+s;}
struct Ntp {void syncNow(){events<<"sync";}};
class DecodiumBridge {public:
 Ntp ntp,*m_ntpClient=&ntp; bool m_ntpEnabled=false; QString m_ntpCustomServer,m_mode="FT8";
 void statusMessage(const QString&s){events<<"status:"+s;}
 void configureNtpClientForMode(const QString&s){events<<"configure:"+s;}
 void syncNtpNow();
};
#include "manual.inc"
void show(const char* label){std::cout<<label<<":"<<events.join("|").toStdString()<<"\n";events.clear();}
int main(){DecodiumBridge b;b.m_ntpClient=nullptr;b.syncNtpNow();show("missing");b.m_ntpClient=&b.ntp;b.syncNtpNow();show("disabled");b.m_ntpEnabled=true;b.syncNtpNow();show("default");b.m_ntpCustomServer="127.0.0.1";b.m_mode="FT4";b.syncNtpNow();show("custom");}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-ntp-manual-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(harness);out=[]
 for code in (old,new):
  (p/'manual.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1]
lines=out[0].splitlines();assert lines[0]=='missing:' and 'disabilitato' in lines[1] and not lines[1].endswith('|sync')
assert 'configure:FT8' in lines[2] and 'server pubblici' in lines[2] and lines[2].endswith('|sync')
assert 'configure:FT4' in lines[3] and '127.0.0.1' in lines[3] and lines[3].endswith('|sync')
print('Manual NTP A/B passed: absent client, disabled, default and custom server')
