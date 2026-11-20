#!/usr/bin/env python3
"""Compare real decode visibility and telemetry classification in isolated apps."""
import json
import os
import secrets
import shutil
import socket
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
main = ROOT/'src/bridge/DecodiumBridge.cpp'
sstv = ROOT/'src/bridge/DecodiumBridgeSstv.cpp'
module = ROOT/'src/bridge/DecodiumBridgeDecodeVisibility.cpp'
qml = ROOT/'build/qml/decodium/Main.qml'
original = {p: p.read_text() for p in (main, sstv, module, qml)}
anchor = 'QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert original[sstv].count(anchor) == 1
injection = '''
#if DECODIUM_HAS_SSTV
 { Q_UNUSED(values); m_hideGhostDecodes=false; m_filtersBypassed=false; m_filterCqOnly=false; m_hideTelemetryOnlyDecodes=true;
   setSetting(QStringLiteral("FiltersHideWorkedBand"),true);
   QVariantMap normal{{"message",QStringLiteral("CQ K1ABC FN42")}};
   QVariantMap telemetry{{"message",QStringLiteral("A1B2C3D4E5")}};
   QVariantMap worked{{"message",QStringLiteral("CQ K1ABC FN42")},{"dxIsWorkedBand",true}};
   decodium::decode_ui::WorkedFilterOptions options;options.hideWorkedBand=true;
   bool normalShown=shouldDisplayEntryForBandActivity(normal);
   bool telemetryHidden=!shouldDisplayEntryForBandActivity(telemetry);
   bool workedHidden=!shouldDisplayEntryForBandActivity(worked);
   bool explicitHidden=!shouldDisplayEntryForBandActivity(worked,options);
   m_filtersBypassed=true;bool bypassShown=shouldDisplayEntryForBandActivity(worked);
   return {{"normalShown",normalShown},{"telemetryHidden",telemetryHidden},{"workedHidden",workedHidden},{"explicitHidden",explicitHidden},{"bypassShown",bypassShown},{"cat",m_catConnected},{"tx",m_transmitting}}; }
#endif
''' 
probe = '''
Timer { interval:12000; running:true; repeat:false; onTriggered: {
 console.log('[VISIBILITY-LAB] '+JSON.stringify(bridge.logSstvQso({}))); Qt.quit();
} }
'''
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
rigs = []
try:
    instrumented = original[sstv].replace(anchor, anchor+injection, 1)
    sstv.write_text(instrumented)
    results = []
    for label in ('before', 'after'):
        main.write_text(original[main]+'\n'+original[module] if label == 'before' else original[main])
        module.write_text('#include "DecodiumBridge.h"\n' if label == 'before' else original[module])
        subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=ROOT,check=True)
        marker='GlassApplicationWindow {'
        assert original[qml].count(marker)==1
        qml.write_text(original[qml].replace(marker,marker+probe,1))
        rig='VisibilityLab'+secrets.token_hex(5);rigs.append(rig)
        with tempfile.TemporaryDirectory(prefix='decodium-visibility-') as tmp:
            env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software')
            sockets=[]
            for port in (2237,2239):
                sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sockets.append(sock)
            cmd=['sandbox-exec','-p',policy,str(ROOT/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','22000']
            proc=subprocess.run(cmd,cwd=ROOT,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
            rows=[line.split('[VISIBILITY-LAB] ',1)[1] for line in proc.stdout.splitlines() if '[VISIBILITY-LAB] ' in line]
            if proc.returncode or len(rows)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:
                raise RuntimeError(proc.stdout[-8000:])
            state=json.loads(rows[0]);expected={'normalShown':True,'telemetryHidden':True,'workedHidden':True,'explicitHidden':True,'bypassShown':True,'cat':False,'tx':False}
            if state!=expected:raise RuntimeError('unexpected visibility state '+str(state))
            results.append(state);print(label,json.dumps(state,sort_keys=True),flush=True)
            for sock in sockets: sock.close()
    assert results[0]==results[1]
finally:
    for path,content in original.items(): path.write_text(content)
    for rig in rigs:
        for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
            path=parent/('Decodium - '+rig+' - test')
            if path.is_dir(): shutil.rmtree(path)
    subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
