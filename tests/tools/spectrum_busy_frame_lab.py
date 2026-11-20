#!/usr/bin/env python3
"""Reproduce an in-flight visual FFT followed by a skipped busy timer tick."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
sstv=root/'src/bridge/DecodiumBridgeSstv.cpp'
timer=root/'src/bridge/DecodiumBridgeSpectrumTimer.cpp'
qml=root/'build-baseline/qml/decodium/Main.qml'
original_sstv,original_timer,original_qml=sstv.read_text(),timer.read_text(),qml.read_text()
anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
early='''            // A busy worker cannot accept this visual frame. Avoid allocating,
            // copying and possibly amplifying it; the atomic claim below still
            // handles a concurrent change after this early check.
            if (m_panadapterComputeBusy.load(std::memory_order_acquire))
                return;

'''
gpu='''            if (gpuPanadapterFft) {
                uint64_t const serial = ++m_panadapterComputeSerial;
'''
cpu='''            uint64_t const serial = ++m_panadapterComputeSerial;
            quint64 const monitorSessionSnapshot = m_periodTimerSessionId;
            m_lastPanadapterFrameMs = nowMs;'''
freq='''            bool const gpuPanadapterFftAllowed ='''
if any((original_sstv.count(anchor)!=1,original_timer.count(early)!=1,
        original_timer.count(gpu)!=1,original_timer.count(cpu)!=1,original_timer.count(freq)!=1,
        original_qml.count('GlassApplicationWindow {')!=1)):
    raise RuntimeError('unexpected source structure')
old_timer=(original_timer.replace(early,'',1)
           .replace(gpu,'            if (gpuPanadapterFft) {\n',1)
           .replace(cpu,'            quint64 const monitorSessionSnapshot = m_periodTimerSessionId;\n            m_lastPanadapterFrameMs = nowMs;',1)
           .replace(freq,'            uint64_t const serial = ++m_panadapterComputeSerial;\n\n'+freq,1))
injection='''
#if DECODIUM_HAS_SSTV
 if (QStandardPaths::isTestModeEnabled() && values.value("action").toString()==QStringLiteral("lab-start-spectrum-busy")) {
   m_sstvWorkspaceActive=false; m_mode=QStringLiteral("FT8");
   if(m_legacyStateTimer) { m_legacyStateTimer->stop(); }
   if(m_spectrumTimer) { m_spectrumTimer->stop(); }
   m_monitoring=true; m_transmitting=false; m_tuning=false; m_spectrumVisible=true;
   m_legacyPcmSpectrumFeed=true; m_forceGpuPanadapterFft.store(false);
   m_gpuPanadapterFftAvailable.store(false); m_gpuPanadapterFftStallGuard.store(false);
   m_lowCpuModeEnabled=true;
#if defined(DECODIUM_QML_PANADAPTER_DIRECT)
   m_panadapterItems.clear();
#endif
   m_nfa=200; m_nfb=3000; m_wfRingPos=4096; m_lastPanadapterData.clear();
   for(int i=0;i<4096;++i)m_wfRing[i]=static_cast<short>(2000.0*std::sin(2.0*3.141592653589793*1000.0*i/12000.0));
   m_panadapterComputeBusy.store(false); m_lastPanadapterFrameMs=QDateTime::currentMSecsSinceEpoch()-5000;
   auto start=m_panadapterComputeSerial.load(); onSpectrumTimer();
   auto submitted=m_panadapterComputeSerial.load(); bool started=m_panadapterComputeBusy.load();
   m_lastPanadapterFrameMs=QDateTime::currentMSecsSinceEpoch()-5000; onSpectrumTimer();
   auto afterBusy=m_panadapterComputeSerial.load();
   return {{"started",started},{"advanced",submitted>start},{"serialPreserved",submitted==afterBusy},
           {"stillBusy",m_panadapterComputeBusy.load()},{"cat",m_catConnected},{"tx",m_transmitting}};
 }
 if (QStandardPaths::isTestModeEnabled() && values.value("action").toString()==QStringLiteral("lab-finish-spectrum-busy"))
   return {{"busy",m_panadapterComputeBusy.load()},{"frame",!m_lastPanadapterData.isEmpty()},
           {"cat",m_catConnected},{"tx",m_transmitting}};
 if (QStandardPaths::isTestModeEnabled() && values.value("action").toString()==QStringLiteral("lab-bench-spectrum-busy")) {
   m_panadapterComputeBusy.store(true); m_lastPanadapterFrameMs=QDateTime::currentMSecsSinceEpoch()-5000;
   QElapsedTimer clock; clock.start();
   for(int i=0;i<5000;++i) onSpectrumTimer();
   auto elapsedNs=clock.nsecsElapsed(); m_panadapterComputeBusy.store(false);
   return {{"elapsedNs",elapsedNs},{"iterations",5000},{"cat",m_catConnected},{"tx",m_transmitting}};
 }
 if (QStandardPaths::isTestModeEnabled() && values.value("action").toString()==QStringLiteral("lab-start-spectrum-reband")) {
   m_nfa=200; m_nfb=3000; m_lastPanadapterData.clear();
   m_lastPanadapterFrameMs=QDateTime::currentMSecsSinceEpoch()-5000;
   onSpectrumTimer(); bool started=m_panadapterComputeBusy.load(); m_nfa=500;
   return {{"started",started},{"cat",m_catConnected},{"tx",m_transmitting}};
 }
 if (QStandardPaths::isTestModeEnabled() && values.value("action").toString()==QStringLiteral("lab-retry-spectrum-reband")) {
   bool rejected=!m_panadapterComputeBusy.load() && m_lastPanadapterData.isEmpty();
   m_lastPanadapterFrameMs=QDateTime::currentMSecsSinceEpoch()-5000;
   onSpectrumTimer();
   return {{"oldRejected",rejected},{"retryStarted",m_panadapterComputeBusy.load()},
           {"cat",m_catConnected},{"tx",m_transmitting}};
 }
 if (QStandardPaths::isTestModeEnabled() && values.value("action").toString()==QStringLiteral("lab-start-spectrum-restart")) {
   m_lastPanadapterData.clear(); m_lastPanadapterFrameMs=QDateTime::currentMSecsSinceEpoch()-5000;
   onSpectrumTimer(); bool started=m_panadapterComputeBusy.load();
   // stopRx/startRx advance this session ID; simulate that transition without
   // starting audio capture or accessing a radio.
   ++m_periodTimerSessionId;
   return {{"started",started},{"cat",m_catConnected},{"tx",m_transmitting}};
 }
#endif
'''
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
   var v=bridge.logSstvQso({action:"lab-start-spectrum-busy"});
   console.log('[SPECTRUM-BUSY-START] '+JSON.stringify(v)); finishBusy.start();
 }}
 Timer {id:finishBusy;interval:1500;running:false;repeat:false;onTriggered:{
   var v=bridge.logSstvQso({action:"lab-finish-spectrum-busy"});
   console.log('[SPECTRUM-BUSY-END] '+JSON.stringify(v));
   var b=bridge.logSstvQso({action:"lab-bench-spectrum-busy"});
   console.log('[SPECTRUM-BUSY-BENCH] '+JSON.stringify(b));
   var r=bridge.logSstvQso({action:"lab-start-spectrum-reband"});
   console.log('[SPECTRUM-REBAND-START] '+JSON.stringify(r)); retryReband.start();
 }}
 Timer {id:retryReband;interval:1500;running:false;repeat:false;onTriggered:{
   var r=bridge.logSstvQso({action:"lab-retry-spectrum-reband"});
   console.log('[SPECTRUM-REBAND-RETRY] '+JSON.stringify(r)); finishReband.start();
 }}
 Timer {id:finishReband;interval:1500;running:false;repeat:false;onTriggered:{
   var r=bridge.logSstvQso({action:"lab-finish-spectrum-busy"});
   console.log('[SPECTRUM-REBAND-END] '+JSON.stringify(r));
   var s=bridge.logSstvQso({action:"lab-start-spectrum-restart"});
   console.log('[SPECTRUM-RESTART-START] '+JSON.stringify(s)); retryRestart.start();
 }}
 Timer {id:retryRestart;interval:1500;running:false;repeat:false;onTriggered:{
   var r=bridge.logSstvQso({action:"lab-retry-spectrum-reband"});
   console.log('[SPECTRUM-RESTART-RETRY] '+JSON.stringify(r)); finishRestart.start();
 }}
 Timer {id:finishRestart;interval:1500;running:false;repeat:false;onTriggered:{
   var r=bridge.logSstvQso({action:"lab-finish-spectrum-busy"});
   console.log('[SPECTRUM-RESTART-END] '+JSON.stringify(r)); Qt.quit();
 }}
'''
results={}
try:
    sstv.write_text(original_sstv.replace('#include "DecodiumBridge.h"',
        '#include "DecodiumBridge.h"\n#include <cmath>\n#include <QElapsedTimer>\n#include <QStandardPaths>\n#include <QTimer>',1).replace(anchor,anchor+injection,1))
    for name,variant in (('before',old_timer),('after',original_timer)):
        timer.write_text(variant)
        build_log=root/'build-baseline/performance'/('spectrum-busy-'+name+'-build.log')
        with build_log.open('w') as log:
            subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,
                           check=True,stdout=log,stderr=subprocess.STDOUT)
        qml.write_text(original_qml.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1))
        with tempfile.TemporaryDirectory(prefix='decodium-spectrum-busy-') as tmp:
            env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',
                                             DECODIUM_FORCE_STANDALONE_UDP='1')
            cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name','SpectrumBusy'+secrets.token_hex(5),
                 '--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','30000']
            app=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
            def read(tag):
                rows=[json.loads(line.split(tag,1)[1]) for line in app.stdout.splitlines() if tag in line]
                if len(rows)!=1:raise RuntimeError(name+' missing '+tag+'\n'+app.stdout[-6000:])
                return rows[0]
            if app.returncode or '[LAB] CAT auto-connect disabled' not in app.stdout:raise RuntimeError(app.stdout[-6000:])
            start,end=read('[SPECTRUM-BUSY-START] '),read('[SPECTRUM-BUSY-END] ')
            if not start['started'] or not start['advanced'] or not start['stillBusy'] or end['busy'] or start['cat'] or start['tx'] or end['cat'] or end['tx']:
                raise RuntimeError(name+' invalid scenario '+repr((start,end)))
            bench=read('[SPECTRUM-BUSY-BENCH] ')
            if bench['iterations']!=5000 or bench['cat'] or bench['tx']:
                raise RuntimeError('invalid benchmark state '+repr(bench))
            rebandStart=read('[SPECTRUM-REBAND-START] ')
            rebandRetry=read('[SPECTRUM-REBAND-RETRY] ')
            rebandEnd=read('[SPECTRUM-REBAND-END] ')
            if not rebandStart['started'] or not rebandRetry['oldRejected'] or not rebandRetry['retryStarted'] or rebandEnd['busy'] or not rebandEnd['frame']:
                raise RuntimeError('band change was not handled: '+repr((rebandStart,rebandRetry,rebandEnd)))
            restartStart=read('[SPECTRUM-RESTART-START] ')
            restartRetry=read('[SPECTRUM-RESTART-RETRY] ')
            restartEnd=read('[SPECTRUM-RESTART-END] ')
            if not restartStart['started'] or not restartRetry['oldRejected'] or not restartRetry['retryStarted'] or restartEnd['busy'] or not restartEnd['frame']:
                raise RuntimeError('monitor restart was not handled: '+repr((restartStart,restartRetry,restartEnd)))
            results[name]={'start':start,'end':end,'benchmark':bench,
                           'rebandStart':rebandStart,'rebandRetry':rebandRetry,'rebandEnd':rebandEnd,
                           'restartStart':restartStart,'restartRetry':restartRetry,'restartEnd':restartEnd}
    if results['before']['start']['serialPreserved'] or results['before']['end']['frame']:
        raise RuntimeError('negative control did not reproduce dropped frame: '+repr(results))
    if not results['after']['start']['serialPreserved'] or not results['after']['end']['frame']:
        raise RuntimeError('current build failed to preserve frame: '+repr(results))
    print(json.dumps(results,sort_keys=True))
finally:
    sstv.write_text(original_sstv);timer.write_text(original_timer);qml.write_text(original_qml)
    subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,
                   check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
