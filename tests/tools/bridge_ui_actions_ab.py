#!/usr/bin/env python3
"""A/B check UI action code and exercise clipboard plus emitted actions."""
import os, shlex, subprocess, tempfile
from pathlib import Path
R = Path(__file__).resolve().parents[2]
def run(*args, **kw): return subprocess.run(args, check=True, text=True, **kw)
old = run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
start='void DecodiumBridge::copyToClipboard('
end='void DecodiumBridge::syncNtpNow()'
old=old[old.index(start):old.index(end)]
new=(R/'src/bridge/DecodiumBridgeUiActions.cpp').read_text()
new=new[new.index(start):].replace('decodiumBridgeUiActionLog(', 'bridgeLog(')
if old != new: raise RuntimeError('implementation differs from baseline')
harness = r'''#include <QGuiApplication>
#include <QClipboard>
#include <QStringList>
#include <cstdio>
static QStringList logs;
void bridgeLog(QString const& msg) { logs << msg; }
class DecodiumBridge {
public:
 QStringList events;
 void setupSettingsRequested(int tab) { events << QString("setup:%1").arg(tab); }
 void timeSyncSettingsRequested() { events << "time"; }
 void copyToClipboard(QString const&);
 void openSetupSettings(int);
 void openTimeSyncSettings();
};
#include "actions.inc"
int main(int argc,char**argv) {
 QGuiApplication app(argc,argv); DecodiumBridge b;
 b.copyToClipboard(QString::fromUtf8("CQ 9H1SR – test"));
 if (QGuiApplication::clipboard()->text()!=QString::fromUtf8("CQ 9H1SR – test")) return 1;
 b.openSetupSettings(-1); b.openSetupSettings(4); b.openTimeSyncSettings();
 if (b.events != QStringList({"setup:-1","setup:4","time"}) || logs.size()!=3) return 2;
 std::printf("clipboard, setup tabs, time settings and logs: OK\n");
}'''
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Gui','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-ui-actions-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text(harness);outputs=[]
 for source in (old,new):
  (d/'actions.inc').write_text(source)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  env=os.environ.copy();env['QT_QPA_PLATFORM']='offscreen'
  outputs.append(run(str(d/'test'),env=env,stdout=subprocess.PIPE).stdout)
 if outputs[0]!=outputs[1]: raise RuntimeError('A/B outputs differ')
 print(outputs[0],end='')
