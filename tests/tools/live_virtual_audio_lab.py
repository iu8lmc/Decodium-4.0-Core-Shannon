import os, subprocess, tempfile, time, json, sys, threading, wave
from pathlib import Path
# Run inside dbus-run-session; this temporarily instruments the build-tree QML.
root=Path(__file__).resolve().parents[2]; qml=root/'build-baseline/qml/decodium/Main.qml'; original=qml.read_text(); binary=sys.argv[1] if len(sys.argv)>1 else 'decodium'
mode=os.environ.get('DECODIUM_LAB_MODE','FT8')
if mode not in ('FT8','FT4','FT2'):raise ValueError('unsupported live lab mode')
if mode!='FT8' and os.environ.get('DECODIUM_LAB_TWO_SLOTS')=='1':raise ValueError('two-slot fixture is FT8 only')
if mode!='FT2' and os.environ.get('DECODIUM_LAB_FT2_REPEAT')=='1':raise ValueError('FT2 repeat fixture requires FT2')
if os.environ.get('DECODIUM_LAB_FT2_OMIT_WEAK')=='1' and os.environ.get('DECODIUM_LAB_FT2_REPEAT')!='1':raise ValueError('FT2 silence control requires repeat fixture')
status_qml=root/'build-baseline/qml/decodium/components/StatusBar.qml'; original_status=status_qml.read_text()
probe='''
 Timer {interval:3000;running:true;repeat:true;onTriggered:{var rows=bridge.decodeList;var out=[];for(var i=0;i<rows.length;i++)out.push({message:String(rows[i].message),freq:String(rows[i].freq),time:String(rows[i].time),aptype:String(rows[i].aptype),dt:String(rows[i].dt)});var wf=waterfallEmbeddedLoader.item;var labels=wf ? wf.spectrumDecodeLabels : [];console.log("[LIVE-PROBE] "+JSON.stringify({epochMs:Date.now(),mode:bridge.mode,monitor:bridge.monitoring,lowCpu:bridge.lowCpuModeEnabled,deep:bridge.deepSearchEnabled,tx:bridge.transmitting,input:bridge.audioInputDevice,waterfall:mainWindow.waterfallPanelVisible,meterOpen:decometerFloatingWindow.visible && !!decometerFloatingLoader.item,logOpen:logFloatingWindow.visible && !!logNativePopupLoader.item,labels:labels,rows:out}))}}
'''
with tempfile.TemporaryDirectory(prefix='decodium-live-') as tmp:
 d=Path(tmp);runtime=d/'run';runtime.mkdir(mode=0o700);env=os.environ.copy();env.update(XDG_RUNTIME_DIR=str(runtime),PIPEWIRE_RUNTIME_DIR=str(runtime),PULSE_SERVER='unix:'+str(runtime/'pulse/native'),XDG_STATE_HOME=str(d/'state'),XDG_CONFIG_HOME=str(d/'config'),DECODIUM_LAB_SETTINGS_DIR=str(d/'settings'),DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',QT_QPA_PLATFORM='offscreen',NO_AT_BRIDGE='1',QT_USE_PORTAL='0',GTK_USE_PORTAL='0');env.pop('PIPEWIRE_REMOTE',None)
 settings=d/'settings/Decodium/Decodium3.ini';settings.parent.mkdir(parents=True);settings.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[LiveLab]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n')
 procs=[];app=None;log=None
 try:
  for name in ('pipewire','pipewire-pulse','wireplumber'):
   f=(d/(name+'.log')).open('w');procs.append((subprocess.Popen([name,'-p','policy'] if name=='wireplumber' else [name],env=env,stdout=f,stderr=subprocess.STDOUT),f))
  for i in range(80):
   info=subprocess.run(['pactl','info'],env=env,text=True,capture_output=True)
   if info.returncode==0:break
   time.sleep(.1)
  if info.returncode or 'unix:'+str(runtime/'pulse/native') not in info.stdout:raise RuntimeError('server not private: '+info.stdout)
  subprocess.run(['pactl','load-module','module-null-sink','sink_name=decodium_lab','sink_properties=device.description=DecodiumLab'],env=env,check=True,capture_output=True)
  subprocess.run(['pactl','load-module','module-remap-source','master=decodium_lab.monitor','source_name=decodium_mic','source_properties=device.description=DecodiumVirtualInput'],env=env,check=True,capture_output=True)
  time.sleep(1); instrumented=original.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1)
  if os.environ.get('DECODIUM_LAB_OPEN_METER') == '1':
   instrumented=instrumented.replace('GlassApplicationWindow {','GlassApplicationWindow {\n Timer {interval:5000;running:true;repeat:false;onTriggered:mainWindow.openDecometerWindow()}\n',1)
  if os.environ.get('DECODIUM_LAB_OPEN_LOG') == '1':
   instrumented=instrumented.replace('GlassApplicationWindow {','GlassApplicationWindow {\n Timer {interval:5000;running:true;repeat:false;onTriggered:mainWindow.openLogWindow()}\n',1)
  if os.environ.get('DECODIUM_LAB_ACTIVE_SWITCH') == '1':
   if mode!='FT8' or os.environ.get('DECODIUM_LAB_TWO_SLOTS')!='1':raise RuntimeError('active switch requires two FT8 slots')
   active_probe='''
 property bool labHeardAudio:false
 property bool labDidActiveSwitch:false
 Timer {interval:50;running:!labDidActiveSwitch;repeat:true;onTriggered:if(bridge.monitoring && bridge.audioLevel>0.005)labHeardAudio=true}
 Connections {target:bridge;function onDecodingChanged(){
  if(bridge.decoding && labHeardAudio && !labDidActiveSwitch)labActiveChange.start()
 }}
 Timer {id:labActiveChange;interval:1;onTriggered:{if(!bridge.decoding || labDidActiveSwitch)return;labDidActiveSwitch=true;bridge.mode="FT4";
  console.log("[LIVE-ACTIVE-SWITCH] "+JSON.stringify({mode:bridge.mode,low:bridge.lowCpuModeEnabled,monitor:bridge.monitoring,tx:bridge.transmitting}));labActiveReturn.start()}}
 Timer {id:labActiveReturn;interval:1800;onTriggered:{bridge.mode="FT8";console.log("[LIVE-ACTIVE-RETURN] "+JSON.stringify({mode:bridge.mode,low:bridge.lowCpuModeEnabled,monitor:bridge.monitoring,tx:bridge.transmitting}))}}
'''
   instrumented=instrumented.replace('GlassApplicationWindow {','GlassApplicationWindow {'+active_probe,1)
  if os.environ.get('DECODIUM_LAB_FT_SWITCH') == '1':
   switch_modes={'FT8':['FT4','FT2','FT8'],'FT4':['FT2','FT8','FT4'],'FT2':['FT4','FT8','FT2']}[mode]
   switch_probe='''
 Timer {interval:14000;running:true;onTriggered:labFtSwitch.start()}
 Timer {id:labFtSwitch;interval:2000;repeat:true;property int step:0;property var modes:__MODES__;
  onTriggered:{if(step>=modes.length){console.log("[LIVE-SWITCH-DONE]");stop();return}
    bridge.mode=modes[step++];console.log("[LIVE-SWITCH] "+JSON.stringify({mode:bridge.mode,monitor:bridge.monitoring,low:bridge.lowCpuModeEnabled,tx:bridge.transmitting}));}}
'''.replace('__MODES__',json.dumps(switch_modes))
   instrumented=instrumented.replace('GlassApplicationWindow {','GlassApplicationWindow {'+switch_probe,1)
  if os.environ.get('DECODIUM_LAB_STATUS_PULSES') in ('static', 'animated'):
   status_anchor='    id: statusBarComponent\n'
   status_condition=' && !(bridge && bridge.lowCpuModeEnabled)'
   if original_status.count(status_anchor)!=1 or original_status.count(status_condition)!=2:raise RuntimeError('unexpected status animation conditions')
   status_probe='''
    Timer {interval:500;running:true;repeat:true;onTriggered:{console.log('[LIVE-DEC-LED] '+JSON.stringify({low:bridge.lowCpuModeEnabled,visible:statusBarComponent.visible,decoding:statusBarComponent.decoding,decPulse:pulseAnimation.running,ftPulse:ftPulseAnimation.running,tx:bridge.transmitting,tune:bridge.tuning}))}}
'''
   instrumented_status=original_status
   if os.environ['DECODIUM_LAB_STATUS_PULSES']=='animated':instrumented_status=instrumented_status.replace(status_condition,'')
   status_qml.write_text(instrumented_status.replace(status_anchor,status_anchor+status_probe,1))
  if os.environ.get('DECODIUM_LAB_DISABLE_TIMING_ANIM') == '1':
   old='Behavior on width { NumberAnimation { duration: 50 } }'
   if instrumented.count(old)!=1:raise RuntimeError('unexpected timing bar animation')
   instrumented=instrumented.replace(old,'Behavior on width { enabled: false; NumberAnimation { duration: 50 } }',1)
  if os.environ.get('DECODIUM_LAB_TIMING_INTERVAL'):
   interval=int(os.environ['DECODIUM_LAB_TIMING_INTERVAL'])
   if not 50 <= interval <= 500:raise ValueError('invalid visual interval')
   old='interval: bridge && bridge.lowCpuModeEnabled ? 100 : 50'
   if instrumented.count(old)!=1:raise RuntimeError('unexpected timing bar interval')
   instrumented=instrumented.replace(old,'interval: bridge && bridge.lowCpuModeEnabled ? %d : 50'%interval,1)
  if os.environ.get('DECODIUM_LAB_CAT_TOGGLE') == '1':
   if os.environ.get('DECODIUM_LAB_MONITOR_TOGGLE') != '1':raise RuntimeError('CAT toggle requires monitor toggle')
   old='running: bridge.catConnected && !bridge.lowCpuModeEnabled'
   if instrumented.count(old)!=1:raise RuntimeError('unexpected CAT animation condition')
   instrumented=instrumented.replace(old,'running: true && !bridge.lowCpuModeEnabled',1)
  if os.environ.get('DECODIUM_LAB_HIDE_WATERFALL') == '1':
   instrumented=instrumented.replace('GlassApplicationWindow {','GlassApplicationWindow {\n Timer {interval:10000;running:true;repeat:false;onTriggered:mainWindow.waterfallPanelVisible=false}\n',1)
  minimized=os.environ.get('DECODIUM_LAB_MINIMIZED') == '1'
  force_minimized=os.environ.get('DECODIUM_LAB_MINIMIZED_ANIMATED') == '1'
  minimized_count=int(os.environ.get('DECODIUM_LAB_MINIMIZED_COUNT','10'))
  if minimized_count not in (1,10):raise ValueError('minimized count must be 1 or 10')
  if force_minimized and not minimized:raise RuntimeError('animated minimized probe requires minimized windows')
  if minimized:
   names=('waterfall','logWindow','astroWindow','liveMap','macroDialog','rigControl','period1','period2','rxFreq','txPanel')
   for name in names:
    condition='running: '+name+'Minimized && bridge && !bridge.lowCpuModeEnabled && !mainWindow.ft2LinkModeActive && bridge.uiQuality !== "Low"'
    if instrumented.count(condition)!=1:raise RuntimeError('unexpected minimized animation: '+name)
    replacement=condition.replace(' && !bridge.lowCpuModeEnabled','') if force_minimized else condition
    instrumented=instrumented.replace(condition,'id: labMinPulse_'+name+'\n                        '+replacement,1)
   minimized_probe='''
 Timer {interval:10000;running:true;repeat:false;onTriggered:{mainWindow.waterfallMinimized=true;mainWindow.logWindowMinimized=true;mainWindow.astroWindowMinimized=true;mainWindow.liveMapMinimized=true;mainWindow.macroDialogMinimized=true;mainWindow.rigControlMinimized=true;mainWindow.period1Minimized=true;mainWindow.period2Minimized=true;mainWindow.rxFreqMinimized=true;mainWindow.txPanelMinimized=true;labMinSample.start()}}
 Timer {id:labMinSample;interval:1000;running:false;repeat:true;onTriggered:{var pulses=[labMinPulse_waterfall,labMinPulse_logWindow,labMinPulse_astroWindow,labMinPulse_liveMap,labMinPulse_macroDialog,labMinPulse_rigControl,labMinPulse_period1,labMinPulse_period2,labMinPulse_rxFreq,labMinPulse_txPanel];var items=[waterfallRestoreMA.parent,logRestoreMA.parent,astroRestoreMA.parent,liveMapRestoreMA.parent,macroRestoreMA.parent,rigRestoreMA.parent,p1RestoreMA.parent,p2RestoreMA.parent,rxRestoreMA.parent,txRestoreMA.parent];console.log("[MINIMIZED-PROBE] "+JSON.stringify({running:pulses.filter(function(x){return x.running}).length,visible:items.filter(function(x){return x.visible}).length}))}}
'''
   if minimized_count==1:
    for name in names:
     if name!='logWindow':minimized_probe=minimized_probe.replace('mainWindow.'+name+'Minimized=true;','')
   instrumented=instrumented.replace('GlassApplicationWindow {','GlassApplicationWindow {'+minimized_probe,1)
  if os.environ.get('DECODIUM_LAB_MONITOR_TOGGLE') == '1':
   toggle='''
 Timer {interval:11000;running:true;repeat:false;onTriggered:{bridge.lowCpuModeEnabled=false;monitorToggle.start()}}
 Timer {id:monitorToggle;interval:650;running:false;repeat:true;property int step:0;property var result:({});onTriggered:{
   if(step===0){result.normal=monitorButtonPulse.running && waterfallMonitorPulse.running;result.catNormal=catConnectionPulse.running;bridge.lowCpuModeEnabled=true}
   else if(step===1){result.stopped=!monitorButtonPulse.running && !waterfallMonitorPulse.running;result.catStopped=!catConnectionPulse.running;result.catOpacity=catConnectionIndicator.opacity;result.iconOpacity=monitorButtonIcon.opacity;result.waterfallOpacity=waterfallMonitorIndicator.opacity;bridge.lowCpuModeEnabled=false}
   else if(step===2){result.resumed=monitorButtonPulse.running && waterfallMonitorPulse.running;result.catResumed=catConnectionPulse.running;bridge.lowCpuModeEnabled=true}
   else {result.stoppedAgain=!monitorButtonPulse.running && !waterfallMonitorPulse.running;result.catStoppedAgain=!catConnectionPulse.running;result.catOpacityAgain=catConnectionIndicator.opacity;result.iconOpacityAgain=monitorButtonIcon.opacity;result.waterfallOpacityAgain=waterfallMonitorIndicator.opacity;console.log("[MONITOR-TOGGLE] "+JSON.stringify(result));stop()};step++
 }}
'''
   instrumented=instrumented.replace('GlassApplicationWindow {','GlassApplicationWindow {'+toggle,1)
  if os.environ.get('DECODIUM_LAB_DEEP_SEARCH') == '1':
   instrumented=instrumented.replace('GlassApplicationWindow {','GlassApplicationWindow {\n Timer {interval:5000;running:true;repeat:false;onTriggered:bridge.deepSearchEnabled=true}\n',1)
  qml.write_text(instrumented)
  variant='hidden' if os.environ.get('DECODIUM_LAB_HIDE_WATERFALL') == '1' else 'visible'
  if mode!='FT8':variant+='-'+mode.lower()
  if os.environ.get('DECODIUM_LAB_DISABLE_TIMING_ANIM') == '1':variant+='-no-timing-animation'
  if os.environ.get('DECODIUM_LAB_TIMING_INTERVAL'):variant+='-timing-'+os.environ['DECODIUM_LAB_TIMING_INTERVAL']
  if os.environ.get('DECODIUM_LAB_CAT_TOGGLE') == '1':variant+='-cat-toggle'
  if minimized:variant+='-minimized-'+str(minimized_count)+('-animated' if force_minimized else '-static')
  if os.environ.get('DECODIUM_LAB_TWO_SLOTS') == '1':variant+='-two-slots'
  if os.environ.get('DECODIUM_LAB_FT2_REPEAT') == '1':variant+='-ft2-repeat'
  if os.environ.get('DECODIUM_LAB_FT2_OMIT_WEAK') == '1':variant+='-silent-repeat'
  if os.environ.get('DECODIUM_LAB_OPEN_METER') == '1':variant+='-meter-open'
  if os.environ.get('DECODIUM_LAB_OPEN_LOG') == '1':variant+='-log-open'
  if os.environ.get('DECODIUM_LAB_FT_SWITCH') == '1':variant+='-ft-switch'
  if os.environ.get('DECODIUM_LAB_ACTIVE_SWITCH') == '1':variant+='-active-switch'
  if os.environ.get('DECODIUM_LAB_FIRST_WAV'):variant+='-custom-first'
  if os.environ.get('DECODIUM_LAB_LOWCPU_FT8_FOLLOWUP') == '1':variant+='-followup'
  if os.environ.get('DECODIUM_LAB_LOWCPU_FT8_FOLLOWUP_MS'):variant+='-'+os.environ['DECODIUM_LAB_LOWCPU_FT8_FOLLOWUP_MS']+'ms'
  if os.environ.get('DECODIUM_LAB_STATUS_PULSES'):variant+='-status-'+os.environ['DECODIUM_LAB_STATUS_PULSES']
  logpath=root/('build-baseline/performance/results/live-audio-'+binary+'-'+variant+'.log');log=logpath.open('w')
  warm_slots=int(os.environ.get('DECODIUM_LAB_WARM_SLOTS','0'))
  if warm_slots not in (0,1):raise ValueError('warm slots must be 0 or 1')
  repeated_slots=int(os.environ.get('DECODIUM_LAB_REPEAT_SECOND_SLOTS','2' if os.environ.get('DECODIUM_LAB_ACTIVE_SWITCH')=='1' else '1'))
  if repeated_slots not in (1,2,4):raise ValueError('unsupported second-slot repetition')
  extra_active_ms=(repeated_slots-1)*15000+5000 if os.environ.get('DECODIUM_LAB_ACTIVE_SWITCH')=='1' else 0
  cmd=[str(root/'build-baseline'/binary),'--test-mode','--rig-name','LiveLab','--lab-no-cat','--lab-audio-input','DecodiumVirtualInput','--lab-monitor-ms','9000','--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode',mode,'--lab-quit-ms',str(85000+warm_slots*15000+extra_active_ms)]
  spawn_epoch_ms=int(time.time()*1000);app=subprocess.Popen(cmd,env=env,stdout=log,stderr=subprocess.STDOUT);start=time.monotonic();ready=False
  while time.monotonic()-start<40 and app.poll() is None:
   time.sleep(.5); data=logpath.read_text(errors='replace')
   lines=[line for line in data.splitlines() if '[LIVE-PROBE] ' in line]
   if lines:
    state=json.loads(lines[-1].split('[LIVE-PROBE] ',1)[1])
    if state['mode']==mode and state['monitor'] and state['lowCpu'] and not state['tx'] and state['input']=='DecodiumVirtualInput' and state['waterfall']==(os.environ.get('DECODIUM_LAB_HIDE_WATERFALL') != '1') and (os.environ.get('DECODIUM_LAB_OPEN_METER') != '1' or state['meterOpen']) and (os.environ.get('DECODIUM_LAB_OPEN_LOG') != '1' or state['logOpen']) and (os.environ.get('DECODIUM_LAB_FT_SWITCH') != '1' or '[LIVE-SWITCH-DONE]' in data) and (os.environ.get('DECODIUM_LAB_DEEP_SEARCH') != '1' or state['deep']) and (os.environ.get('DECODIUM_LAB_MONITOR_TOGGLE') != '1' or '[MONITOR-TOGGLE]' in data) and (not minimized or '[MINIMIZED-PROBE]' in data):ready=True;ready_epoch_ms=state['epochMs'];break
  if not ready:raise RuntimeError('monitor did not become ready')
  if os.environ.get('DECODIUM_LAB_FT_SWITCH') == '1':
   switches=[json.loads(line.split('[LIVE-SWITCH] ',1)[1]) for line in data.splitlines() if '[LIVE-SWITCH] ' in line]
   if [s['mode'] for s in switches]!=switch_modes or not all(s['monitor'] and s['low'] and not s['tx'] for s in switches):raise RuntimeError('mode switch state: '+repr(switches))
  if os.environ.get('DECODIUM_LAB_MONITOR_TOGGLE') == '1':
   results=[json.loads(line.split('[MONITOR-TOGGLE] ',1)[1]) for line in data.splitlines() if '[MONITOR-TOGGLE] ' in line]
   if len(results)!=1 or not all(results[0].get(k) for k in ('normal','stopped','resumed','stoppedAgain')) or not all(results[0].get(k,0)>.99 for k in ('iconOpacity','waterfallOpacity','iconOpacityAgain','waterfallOpacityAgain')):raise RuntimeError('monitor toggle failed: '+repr(results))
   if os.environ.get('DECODIUM_LAB_CAT_TOGGLE') == '1' and (not all(results[0].get(k) for k in ('catNormal','catStopped','catResumed','catStoppedAgain')) or not all(results[0].get(k,0)>.99 for k in ('catOpacity','catOpacityAgain'))):raise RuntimeError('CAT toggle failed: '+repr(results))
  start_offset=float(os.environ.get('DECODIUM_LAB_START_OFFSET','0.3'))
  if not 0.0 <= start_offset <= 1.0:raise ValueError('start offset must be between 0 and 1 second')
  delay=15-(time.time()%15)+start_offset+warm_slots*15;print('Ready, playback in %.1fs'%delay,flush=True);time.sleep(delay)
  def cpu_ticks():
   fields=Path('/proc/%d/stat'%app.pid).read_text().rsplit(')',1)[1].split();return int(fields[11])+int(fields[12])
  stop_sample=threading.Event(); thread_totals={}; previous={}; rss_samples=[]
  def sample_threads():
   tasks=Path('/proc')/str(app.pid)/'task'
   while not stop_sample.is_set():
    try:
     status=(Path('/proc')/str(app.pid)/'status').read_text()
     rss_line=next((line for line in status.splitlines() if line.startswith('VmRSS:')),None)
     if rss_line:rss_samples.append(int(rss_line.split()[1]))
     for task in tasks.iterdir():
      try:
       stat=(task/'stat').read_text(); fields=stat.rsplit(')',1)[1].split()
       identity=(task.name, fields[19]); name=(task/'comm').read_text().strip()
       if task.name == str(app.pid):name += ' (main)'
       count=int(fields[11])+int(fields[12]); before=previous.get(identity,count)
       thread_totals[name]=thread_totals.get(name,0)+max(0,count-before);previous[identity]=count
      except (FileNotFoundError, ProcessLookupError):pass
    except (FileNotFoundError, ProcessLookupError):break
    stop_sample.wait(.25)
  sampler=threading.Thread(target=sample_threads,daemon=True);sampler.start()
  offset=logpath.stat().st_size;cpu_start=cpu_ticks();wall_start=time.monotonic();playback_epoch_ms=int(time.time()*1000)
  fixture=Path(os.environ['DECODIUM_LAB_FIRST_WAV']).resolve() if os.environ.get('DECODIUM_LAB_FIRST_WAV') else root/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav'
  if not fixture.is_file():raise FileNotFoundError(fixture)
  if os.environ.get('DECODIUM_LAB_TWO_SLOTS') == '1':
   fixture2=Path(os.environ['DECODIUM_LAB_SECOND_WAV']).resolve() if os.environ.get('DECODIUM_LAB_SECOND_WAV') else root/'tests/bridge_refactor_fixtures/ft8-9h1sr-1300.wav';combined=d/'two-slots.wav'
   with wave.open(str(fixture),'rb') as first, wave.open(str(fixture2),'rb') as second:
    if first.getparams()!=second.getparams():raise RuntimeError('incompatible fixtures')
    with wave.open(str(combined),'wb') as out:
     out.setparams(first.getparams());out.writeframes(first.readframes(first.getnframes()));second_frames=second.readframes(second.getnframes())
     for repeat in range(repeated_slots):out.writeframes(second_frames)
   fixture=combined
  if os.environ.get('DECODIUM_LAB_FT2_REPEAT') == '1':
   strong=root/'build-baseline/performance/wavs-ft2-ft4/ft2-history-strong.wav'
   weak=root/'build-baseline/performance/wavs-ft2-ft4/ft2-weak19-seed1102.wav';combined=d/'ft2-repeat.wav'
   if not strong.is_file():
    strong=d/'ft2-history-strong.wav'
    subprocess.run([str(root/'build-baseline/tests/ft2_make_test_wav'),'--message','CQ K1ABC FN42',
                    '--freq','1500','--gain','0.05',str(strong)],check=True,stdout=subprocess.DEVNULL)
   with wave.open(str(strong),'rb') as first, wave.open(str(weak),'rb') as second:
    if first.getparams()!=second.getparams():raise RuntimeError('incompatible FT2 fixtures')
    with wave.open(str(combined),'wb') as out:
     out.setparams(first.getparams());out.writeframes(first.readframes(first.getnframes()))
     out.writeframes(bytes(first.getnframes()*first.getsampwidth()*first.getnchannels()))
     if os.environ.get('DECODIUM_LAB_FT2_OMIT_WEAK') == '1':
      out.writeframes(bytes(second.getnframes()*second.getsampwidth()*second.getnchannels()))
     else:
      out.writeframes(second.readframes(second.getnframes()))
   fixture=combined
  print('Playing fixture',flush=True);subprocess.run(['paplay','--device=decodium_lab',str(fixture)],env=env,check=True,timeout=30+15*repeated_slots)
  time.sleep(13); data=logpath.read_bytes()[offset:].decode(errors='replace');
  states=[json.loads(line.split('[LIVE-PROBE] ',1)[1]) for line in data.splitlines() if '[LIVE-PROBE] ' in line]
  if os.environ.get('DECODIUM_LAB_STATUS_PULSES'):
   led_states=[json.loads(line.split('[LIVE-DEC-LED] ',1)[1]) for line in data.splitlines() if '[LIVE-DEC-LED] ' in line]
   animated=os.environ['DECODIUM_LAB_STATUS_PULSES']=='animated'
   if not led_states or not all(s['low'] and s['visible'] and not s['tx'] and not s['tune'] and s['decPulse']==(animated and s['decoding']) for s in led_states):raise RuntimeError('unexpected status state: '+repr(led_states))
   print('DEC-LED '+json.dumps({'samples':len(led_states),'decodingSamples':sum(s['decoding'] for s in led_states),'pulseSamples':sum(s['decPulse'] for s in led_states),'ftPulseSamples':sum(s['ftPulse'] for s in led_states)}),flush=True)
  if not states or not all((s['mode']==mode or (os.environ.get('DECODIUM_LAB_ACTIVE_SWITCH')=='1' and s['mode']=='FT4')) and s['monitor'] and s['lowCpu'] and not s['tx'] and s['input']=='DecodiumVirtualInput' and s['waterfall']==(os.environ.get('DECODIUM_LAB_HIDE_WATERFALL') != '1') and (os.environ.get('DECODIUM_LAB_OPEN_METER') != '1' or s['meterOpen']) and (os.environ.get('DECODIUM_LAB_OPEN_LOG') != '1' or s['logOpen']) and (os.environ.get('DECODIUM_LAB_DEEP_SEARCH') != '1' or s['deep']) for s in states):raise RuntimeError('unexpected receiver state; inspect '+str(logpath))
  if minimized:
   minimized_states=[json.loads(line.split('[MINIMIZED-PROBE] ',1)[1]) for line in data.splitlines() if '[MINIMIZED-PROBE] ' in line]
   if not minimized_states or not all(s['visible']==minimized_count and s['running']==(minimized_count if force_minimized else 0) for s in minimized_states):raise RuntimeError('unexpected minimized indicators; inspect '+str(logpath))
  def found(spec):
   message,freq=spec.rsplit('|',1)
   return any(any(row['message']==message and abs(int(row['freq'])-int(freq))<=2 for row in s['rows']) for s in states)
  first_expected=os.environ.get('DECODIUM_LAB_EXPECT_FIRST','CQ K1ABC FN42|1500')
  if os.environ.get('DECODIUM_LAB_ACTIVE_SWITCH') != '1' and not found(first_expected):raise RuntimeError('first fixture was not decoded; inspect '+str(logpath))
  if os.environ.get('DECODIUM_LAB_ACTIVE_SWITCH') == '1':
   for marker,wanted in (('[LIVE-ACTIVE-SWITCH] ','FT4'),('[LIVE-ACTIVE-RETURN] ','FT8')):
    observed=[json.loads(line.split(marker,1)[1]) for line in data.splitlines() if marker in line]
    if len(observed)!=1 or observed[0]!={'mode':wanted,'low':True,'monitor':True,'tx':False}:raise RuntimeError('active switch failed: '+repr(observed))
  if os.environ.get('DECODIUM_LAB_EXPECT_EXTRA') and not found(os.environ['DECODIUM_LAB_EXPECT_EXTRA']):raise RuntimeError('extra message was not decoded; inspect '+str(logpath))
  if os.environ.get('DECODIUM_LAB_TWO_SLOTS') == '1' and not found(os.environ.get('DECODIUM_LAB_EXPECT_SECOND','CQ 9H1SR JM75|1300')):raise RuntimeError('second slot was not decoded; inspect '+str(logpath))
  if '[Bridge ERROR]' in data:raise RuntimeError('audio or bridge error; inspect '+str(logpath))
  cpu_end=cpu_ticks(); wall_end=time.monotonic(); stop_sample.set();sampler.join()
  print('MEASUREMENT '+json.dumps({'binary':binary,'spawnEpochMs':spawn_epoch_ms,'readyProbeEpochMs':ready_epoch_ms,'playbackEpochMs':playback_epoch_ms,'cpuSeconds':(cpu_end-cpu_start)/os.sysconf('SC_CLK_TCK'),'wallSeconds':wall_end-wall_start,'peakRssKiB':max(rss_samples) if rss_samples else None,'threadCpuSeconds':{k:round(v/os.sysconf('SC_CLK_TCK'),2) for k,v in sorted(thread_totals.items(),key=lambda item:-item[1]) if v}}),flush=True); app.terminate();app.wait(timeout=10);print('Completed; log '+str(logpath),flush=True)
 finally:
  if app and app.poll() is None:app.terminate();app.wait(timeout=10)
  if log:log.close()
  qml.write_text(original)
  status_qml.write_text(original_status)
  for proc,f in reversed(procs):
   proc.terminate()
   try:proc.wait(timeout=5)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
   f.close()
