#!/usr/bin/env python3
"""Observe a real FST4 native decode across a bridge mode switch (macOS lab).

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
a=p.parse_args()
if subprocess.run(['pgrep','-x','decodium.*|Decodium4'],capture_output=True).returncode==0:
    p.error('Another Decodium instance is running')
out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
weak=a.weak_wav.resolve(strict=True);recovery=a.recovery_wav.resolve(strict=True)
core=ROOT/'Detector/FST4DecodeWorker.cpp'
bridge=ROOT/'src/bridge/DecodiumBridgeSstv.cpp'
qml=ROOT/'build/qml/decodium/Main.qml'
originals={x:x.read_bytes() for x in (core,bridge,qml)}
build=['cmake','--build','build','--target','decodium_qml','-j','2']
anchor=b'  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};\n  if (decodeCancelled ()) return {};'
assert originals[core].count(anchor)==1
hook=r'''
    static int labInvocation=0;
    int const labCall=++labInvocation;
    bool const lab=QStandardPaths::isTestModeEnabled();
    auto labLog=[&](QString event) {
      if(lab) qInfo().noquote()<<"[FST4MODE] "+QJsonDocument(QJsonObject{
        {"event",event},{"call",labCall},{"epoch",double(fst4LabNow())}}).toJson(QJsonDocument::Compact);
    };
    labLog("native-start");
    auto labEnd=qScopeGuard([&]{labLog("native-end");});
    if(lab && labCall==1) {
      auto* app=QCoreApplication::instance();
      QMetaObject::invokeMethod(app,[app]{
        QTimer::singleShot(40,app,[app]{
          QObject* target=app->property("fst4ModeLabBridge").value<QObject*>();
          if(!target)return;
          double const begin=fst4LabNow();
          bool const ok=target->setProperty("mode",QStringLiteral("FT8"));
          qInfo().noquote()<<"[FST4MODE] "+QJsonDocument(QJsonObject{
            {"event","mode-switch"},{"epoch",double(fst4LabNow())},
            {"duration_ms",double(fst4LabNow()-begin)},
            {"ok",ok},{"mode",target->property("mode").toString()}}).toJson(QJsonDocument::Compact);
        });
      },Qt::QueuedConnection);
    }
'''.encode()
bridge_anchor=b'QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
assert originals[bridge].count(bridge_anchor)==1
bridge_hook=b'''\n    if(QCoreApplication::arguments().contains(QStringLiteral("--test-mode")) && values.value("__fst4ModeLab").toBool()) {
        QCoreApplication::instance()->setProperty("fst4ModeLabBridge",QVariant::fromValue<QObject*>(this));
        return {{"registered",true}};
    }
'''
probe=r'''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
   bridge.logSstvQso({__fst4ModeLab:true});bridge.mode="FST4-60";
   bridge.filtersBypassed=true;bridge.autoSeq=false;bridge.ndepth=3;
   bridge.coherentAvgEnabled=false;bridge.deepSearchEnabled=false;
   bridge.ftThreadsAuto=false;bridge.ftThreads=2;bridge.lowCpuModeEnabled=false;
   bridge.openWavForDecode(__WEAK__); observe.start();
 }}
 Timer {id:observe;interval:50;repeat:true;property int ticks:0;onTriggered:{
   if(bridge.mode==="FT8") {stop();recover.start();return;}
   if(++ticks>400){stop();console.log('[FST4MODE] '+JSON.stringify({event:'timeout-switch'}));Qt.quit();}
 }}
 Timer {id:recover;interval:3000;repeat:false;onTriggered:{
   console.log('[FST4MODE] '+JSON.stringify({event:'old-mode-idle',epoch:Date.now(),rows:bridge.decodeList.length,tx:bridge.transmitting}));
   bridge.mode="FST4-60";bridge.openWavForDecode(__RECOVERY__);waitRecovery.start();
 }}
 Timer {id:waitRecovery;interval:100;repeat:true;property int ticks:0;property bool loggedRows:false;onTriggered:{
   var rows=bridge.decodeList;
   if(rows.length && !loggedRows){loggedRows=true;console.log('[FST4MODE] '+JSON.stringify({event:'visible-rows',rows:rows}));}
   for(var i=0;i<rows.length;i++)if(/^CQ K9KFR EN71$/.test(String(rows[i].message).replace(/\s+/g,' ').trim())){
     stop();console.log('[FST4MODE] '+JSON.stringify({event:'recovered',epoch:Date.now(),message:rows[i].message,tx:bridge.transmitting,mode:bridge.mode}));Qt.quit();return;
   }
   if(++ticks>200){stop();console.log('[FST4MODE] '+JSON.stringify({event:'timeout-recovery'}));Qt.quit();}
 }}
'''.replace('__WEAK__',json.dumps(str(weak))).replace('__RECOVERY__',json.dumps(str(recovery)))
probe=probe.encode()
try:
    includes=b'#include <QCoreApplication>\n#include <QDateTime>\n#include <QJsonDocument>\n#include <QJsonObject>\n#include <QTimer>\n#include <QScopeGuard>\n#include <QStandardPaths>\n#include <QVariant>\n'
    includes += b'#include <chrono>\nstatic double fst4LabNow() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }\n'
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
    events=[json.loads(line.split('[FST4MODE] ',1)[1]) for line in log.splitlines() if '[FST4MODE] ' in line]
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
                active_at_switch=active_at_switch,
                native_end_relative_to_setter_return_ms=ends[0]['epoch']-switches[0]['epoch'] if switches and ends else None,
                native_ms_from_setter_start=ends[0]['epoch']-(switches[0]['epoch']-switches[0]['duration_ms']) if switches and ends else None,
                clock_note='Native and setter epoch fields are monotonic milliseconds; QML epoch fields are wall-clock milliseconds',
                scope='Actual bridge mode switch and file replay; no RF or Windows evidence')
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    if not valid:raise RuntimeError('FST4 application mode-switch probe failed')
finally:
    for path,data in originals.items():path.write_bytes(data)
    with (out/'restore-build.log').open('w') as log:
        subprocess.run(build,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
    assert all(path.read_bytes()==data for path,data in originals.items())
