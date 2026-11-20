#!/usr/bin/env python3
"""Exercise workspace monitor transitions without starting physical audio."""
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
fixture = (root / 'tests/bridge_refactor_fixtures/sstv_workspace_before.inc').read_text()
module = (root / 'src/bridge/DecodiumBridgeSstvWorkspace.cpp').read_text()
assert module.endswith(fixture)
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
harness = r'''
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QStringList>
#include <QTextStream>
#define DECODIUM_HAS_SSTV 1
class Legacy {
public:
 QStringList bandActivityLines() const{return {"old"};} QStringList rxFrequencyLines() const{return {"old"};}
 void clearBandActivity(){bandCleared=true;} void clearRxFrequency(){rxCleared=true;}
 int bandActivityRevision()const{return 3;} int rxFrequencyRevision()const{return 4;}
 bool bandCleared=false,rxCleared=false;
};
class DecodiumBridge {
public:
 bool m_sstvWorkspaceActive=false,m_monitoring=false,m_monitorRequested=false,m_sstvWorkspaceRestoreMonitoring=false,m_shuttingDown=false,m_transmitting=false,m_tuning=false;
 QTimer* m_decodeReleaseTimer=nullptr; QStringList m_pendingDecodeReleaseQueue; QString m_mode="FT8"; Legacy* m_legacyBackend=nullptr; int m_legacyBandActivityRevision=0,m_legacyRxFrequencyRevision=0; QStringList calls;
 void setMonitoring(bool v){m_monitoring=v;m_monitorRequested=v;calls << (v?"on":"off");}
 void clearDecodeWindowsForModeChange(QString,QString){calls<<"clear";}
 void resetFtxDecodeWorkersForModeChange(QString,QString){calls<<"reset";}
 void resetRxPeriodAccumulation(bool){calls<<"period";}
 void primeLegacyAllTxtCursor(){calls<<"prime";}
 bool enterSstvWorkspace(); void leaveSstvWorkspace();
};
#include "workspace.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);Legacy legacy;DecodiumBridge b;b.m_legacyBackend=&legacy;b.m_monitoring=true;b.m_monitorRequested=true;b.m_pendingDecodeReleaseQueue<<"pending";bool entered=b.enterSstvWorkspace();bool during=entered&&b.m_sstvWorkspaceActive&&b.m_sstvWorkspaceRestoreMonitoring&&b.m_pendingDecodeReleaseQueue.isEmpty();b.leaveSstvWorkspace();bool restored=!b.m_sstvWorkspaceActive&&b.m_monitoring&&legacy.bandCleared&&legacy.rxCleared&&b.m_legacyBandActivityRevision==3&&b.m_legacyRxFrequencyRevision==4&&b.calls==QStringList({"off","on","clear","reset","period","off","prime","on"});DecodiumBridge tx;tx.m_monitoring=true;tx.m_monitorRequested=true;tx.enterSstvWorkspace();tx.m_transmitting=true;tx.leaveSstvWorkspace();bool guarded=!tx.m_monitoring;bool ok=during&&restored&&guarded;QTextStream(stdout)<<"during="<<during<<" restored="<<restored<<" guarded="<<guarded<<Qt::endl;return ok?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-sstv-workspace-ab-') as tmp:
    path = Path(tmp)
    (path / 'main.cpp').write_text(harness)
    outputs = []
    for label, body in [('before', fixture), ('after', module[-len(fixture):])]:
        (path / 'workspace.inc').write_text(body)
        binary = path / label
        subprocess.run(['c++', '-std=c++17', '-I' + str(path), str(path / 'main.cpp'), *flags, '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], text=True, capture_output=True, check=True)
        print(label, result.stdout.strip(), flush=True)
        outputs.append(result.stdout)
    assert outputs[0] == outputs[1]
