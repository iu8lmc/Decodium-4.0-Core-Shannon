#!/usr/bin/env python3
"""Observe a real Q65 native decode across a bridge mode switch (macOS lab).

Temporary source/QML hooks are restored and the normal app rebuilt in finally.
No decoder cancellation is added. Uses the established isolated run_case harness.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from measure_runtime_baseline import ROOT, run_case

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--weak-wav',type=Path,required=True)
p.add_argument('--recovery-wav',type=Path,required=True)
p.add_argument('--coherent-average',action='store_true',help='Verify that averaging requests retain normal completion during retirement')
a=p.parse_args()
if subprocess.run(['pgrep','-x','decodium.*|Decodium4'],capture_output=True).returncode==0:
    p.error('Another Decodium instance is running')
out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
weak=a.weak_wav.resolve(strict=True);recovery=a.recovery_wav.resolve(strict=True)
core=ROOT/'Detector/FtxQ65Decoder.cpp'
bridge=ROOT/'src/bridge/DecodiumBridgeSstv.cpp'
qml=ROOT/'build/qml/decodium/Main.qml'
originals={x:x.read_bytes() for x in (core,bridge,qml)}
build=['cmake','--build','build','--target','decodium_qml','-j','2']
anchor=b'    QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};'
assert originals[core].count(anchor)==1
hook=r'''
    static int labInvocation=0;
    int const labCall=++labInvocation;
    bool const lab=QStandardPaths::isTestModeEnabled();
    auto labLog=[&](QString event) {
      if(lab) qInfo().noquote()<<"[Q65MODE] "+QJsonDocument(QJsonObject{
        {"event",event},{"call",labCall},{"epoch",double(QDateTime::currentMSecsSinceEpoch())}}).toJson(QJsonDocument::Compact);
    };
    labLog("native-start");
    auto labEnd=qScopeGuard([&]{labLog("native-end");});
    if(lab && labCall==1) {
      auto* app=QCoreApplication::instance();
      QMetaObject::invokeMethod(app,[app]{
        QTimer::singleShot(40,app,[app]{
          QObject* target=app->property("q65ModeLabBridge").value<QObject*>();
          if(!target)return;
          qint64 const begin=QDateTime::currentMSecsSinceEpoch();
          bool const ok=target->setProperty("mode",QStringLiteral("FT8"));
          qInfo().noquote()<<"[Q65MODE] "+QJsonDocument(QJsonObject{
            {"event","mode-switch"},{"epoch",double(QDateTime::currentMSecsSinceEpoch())},
            {"duration_ms",double(QDateTime::currentMSecsSinceEpoch()-begin)},
            {"ok",ok},{"mode",target->property("mode").toString()}}).toJson(QJsonDocument::Compact);
        });
      },Qt::QueuedConnection);
    }
'''.encode()
bridge_anchor=b'QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert originals[bridge].count(bridge_anchor)==1
bridge_hook=b'''\n    if(QCoreApplication::arguments().contains(QStringLiteral("--test-mode")) && values.value("__q65ModeLab").toBool()) {
        QCoreApplication::instance()->setProperty("q65ModeLabBridge",QVariant::fromValue<QObject*>(this));
        return {{"registered",true}};
    }
'''
probe=r'''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
   bridge.logSstvQso({__q65ModeLab:true});bridge.mode="Q65-15A";
   bridge.filtersBypassed=true;bridge.autoSeq=false;bridge.ndepth=3;
   bridge.coherentAvgEnabled=false;bridge.deepSearchEnabled=false;
   bridge.ftThreadsAuto=false;bridge.ftThreads=2;bridge.lowCpuModeEnabled=false;
   bridge.openWavForDecode(__WEAK__); observe.start();
 }}
 Timer {id:observe;interval:50;repeat:true;property int ticks:0;onTriggered:{
   if(bridge.mode==="FT8") {stop();recover.start();return;}
   if(++ticks>400){stop();console.log('[Q65MODE] '+JSON.stringify({event:'timeout-switch'}));Qt.quit();}
 }}
 Timer {id:recover;interval:3000;repeat:false;onTriggered:{
   console.log('[Q65MODE] '+JSON.stringify({event:'old-mode-idle',epoch:Date.now(),rows:bridge.decodeList.length,tx:bridge.transmitting}));
   bridge.mode="Q65-15A";bridge.openWavForDecode(__RECOVERY__);waitRecovery.start();
 }}
 Timer {id:waitRecovery;interval:100;repeat:true;property int ticks:0;property bool loggedRows:false;onTriggered:{
   var rows=bridge.decodeList;
   if(rows.length && !loggedRows){loggedRows=true;console.log('[Q65MODE] '+JSON.stringify({event:'visible-rows',rows:rows}));}
   // Q65 currently retains its q<decode-type>[average-count] annotation in message.
   for(var i=0;i<rows.length;i++)if(/^K1ABC W9XYZ EN37(?: q\d{1,2})?$/.test(String(rows[i].message).replace(/\s+/g,' ').trim())){
     stop();console.log('[Q65MODE] '+JSON.stringify({event:'recovered',epoch:Date.now(),message:rows[i].message,tx:bridge.transmitting,mode:bridge.mode}));Qt.quit();return;
   }
   if(++ticks>200){stop();console.log('[Q65MODE] '+JSON.stringify({event:'timeout-recovery'}));Qt.quit();}
 }}
'''.replace('__WEAK__',json.dumps(str(weak))).replace('__RECOVERY__',json.dumps(str(recovery)))
if a.coherent_average:
    probe=probe.replace('bridge.coherentAvgEnabled=false;', 'bridge.coherentAvgEnabled=true;')
probe=probe.encode()
try:
    includes=b'#include <QCoreApplication>\n#include <QDateTime>\n#include <QJsonDocument>\n#include <QJsonObject>\n#include <QTimer>\n#include <QScopeGuard>\n#include <QStandardPaths>\n#include <QVariant>\n'
    core.write_bytes(includes+originals[core].replace(anchor,anchor+hook))
    bridge.write_bytes(originals[bridge].replace(bridge_anchor,bridge_anchor+bridge_hook))
    with (out/'build.log').open('w') as log:
        subprocess.run(build,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
    binary_sha=hashlib.sha256((ROOT/'build/decodium').read_bytes()).hexdigest()
    marker=b'GlassApplicationWindow {'
    assert originals[qml].count(marker)==1
    qml.write_bytes(originals[qml].replace(marker,marker+probe))
    valid=run_case(ROOT/'build/decodium',out,'native',1,70)
    log=(out/'native-1.log').read_text()
    events=[json.loads(line.split('[Q65MODE] ',1)[1]) for line in log.splitlines() if '[Q65MODE] ' in line]
    switches=[e for e in events if e['event']=='mode-switch']
    starts=[e for e in events if e['event']=='native-start' and e['call']==1]
    ends=[e for e in events if e['event']=='native-end' and e['call']==1]
    recovered=[e for e in events if e['event']=='recovered']
    idle=[e for e in events if e['event']=='old-mode-idle']
    valid=valid and len(switches)==len(starts)==len(ends)==len(recovered)==len(idle)==1
    active_at_switch=False
    if valid:
        begin=switches[0]['epoch']-switches[0]['duration_ms']
        active_at_switch=starts[0]['epoch'] <= begin < ends[0]['epoch']
        valid=active_at_switch and all(s['ok'] and s['mode']=='FT8' for s in switches) and not recovered[0]['tx'] and not idle[0]['tx'] and idle[0]['rows']==0
    report=dict(valid=valid,events=events,instrumented_binary_sha256=binary_sha,
                coherent_average=a.coherent_average,
                active_at_switch=active_at_switch,
                remaining_native_ms=ends[0]['epoch']-switches[0]['epoch'] if switches and ends else None,
                scope='Actual bridge mode switch and file replay; no RF or Windows evidence')
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    if not valid:raise RuntimeError('Q65 application mode-switch probe failed')
finally:
    for path,data in originals.items():path.write_bytes(data)
    with (out/'restore-build.log').open('w') as log:
        subprocess.run(build,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
    assert all(path.read_bytes()==data for path,data in originals.items())
