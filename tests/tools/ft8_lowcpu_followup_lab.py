#!/usr/bin/env python3
"""Compare test-only LOW CPU FT8 follow-up against the normal file path."""
import argparse,collections,json,os,re,secrets,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--wav',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--logs-dir',type=Path,required=True);p.add_argument('--followup-ms',type=int,default=0);p.add_argument('--allow-empty-fast',action='store_true',help='allow an empty fast pass for weak-signal experiments');g=p.add_mutually_exclusive_group();g.add_argument('--disable-deep-mid-fast',action='store_true');g.add_argument('--leave-low-mid-fast',action='store_true');g.add_argument('--disable-deep-during-followup',action='store_true');a=p.parse_args()
if a.followup_ms and not 500 <= a.followup_ms <= 2800:raise ValueError('follow-up budget outside test range')
qml=root/'build-baseline/qml/decodium/Main.qml';original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1 or not a.wav.is_file():raise RuntimeError('unexpected QML or input')
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{bridge.deepSearchEnabled=true;waitSlot.start()}}
 Timer {id:waitSlot;interval:100;running:false;repeat:true;onTriggered:{if(Date.now()%15000>200)return;stop();result.started=Date.now();bridge.openWavForDecode(__WAV__)}}
 Connections {target:bridge;function onDecodingChanged(){if(bridge.decoding){result.seen=true;__ON_FAST_START__}else if(result.seen && !result.running){result.start();__ON_FAST_END__}}}
 Timer {id:disableDeep;interval:150;running:false;repeat:false;onTriggered:{__MID_FAST_ACTION__ console.log("[LOWCPU-CHANGED] "+JSON.stringify({decoding:bridge.decoding,deep:bridge.deepSearchEnabled,low:bridge.lowCpuModeEnabled}))}}
 Timer {id:result;interval:300;running:false;repeat:true;property bool seen:false;property int step:0;property double started:0;property var fast:[];
  function rows(){var result=[];var src=bridge.decodeList;for(var i=0;i<src.length;i++)result.push({message:String(src[i].message),freq:String(src[i].freq)});return result}
  onTriggered:{if(++step===2)fast=rows();if(step<20)return;console.log("[LOWCPU-FOLLOWUP] "+JSON.stringify({fast:fast,final:rows(),low:bridge.lowCpuModeEnabled,tx:bridge.transmitting,elapsedMs:Date.now()-started}));stop();Qt.quit()}
 }
'''.replace('__WAV__',json.dumps(str(a.wav.resolve()))).replace('__ON_FAST_START__','if(!result.running && !disableDeep.running)disableDeep.start();' if a.disable_deep_mid_fast or a.leave_low_mid_fast else '').replace('__ON_FAST_END__','disableDeep.start();' if a.disable_deep_during_followup else '').replace('__MID_FAST_ACTION__','bridge.lowCpuModeEnabled=false;' if a.leave_low_mid_fast else 'bridge.deepSearchEnabled=false;')
def counter(rows):return collections.Counter((str(r['freq']).strip(),' '.join(r['message'].upper().split())) for r in rows)
def run(experiment):
 with tempfile.TemporaryDirectory(prefix='decodium-lowcpu-followup-') as tmp:
  rig='LowFollow'+secrets.token_hex(5);ini=Path(tmp)/'Decodium'/'Decodium3.ini';ini.parent.mkdir(parents=True)
  ini.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n'%rig)
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_HASH_LIFECYCLE_DIAGNOSTICS='1')
  if experiment:
   env['DECODIUM_LAB_LOWCPU_FT8_FOLLOWUP']='1'
   if a.followup_ms:env['DECODIUM_LAB_LOWCPU_FT8_FOLLOWUP_MS']=str(a.followup_ms)
  else:env.pop('DECODIUM_LAB_LOWCPU_FT8_FOLLOWUP',None)
  cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms','70000']
  proc=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=85)
  label='experiment' if experiment else 'baseline';a.logs_dir.mkdir(parents=True,exist_ok=True);(a.logs_dir/(a.wav.stem+'-'+label+'.log')).write_text(proc.stdout)
  lines=[line.split('[LOWCPU-FOLLOWUP] ',1)[1] for line in proc.stdout.splitlines() if '[LOWCPU-FOLLOWUP] ' in line]
  if proc.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in proc.stdout:raise RuntimeError(label+' failed: '+proc.stdout[-6000:])
  result=json.loads(lines[0]);result['dispatches']=re.findall(r'\[FT8DISPATCH\].*',proc.stdout);result['begins']=re.findall(r'\[HASHLIFE\] mode=FT8 decoder_begin serial=\s*(\d+)',proc.stdout)
  result['decoderEnds']=[{'serial':int(serial),'elapsedMs':int(ms),'invalidated':bool(int(invalidated))} for serial,ms,invalidated in re.findall(r'\[HASHLIFE\] mode=FT8 decoder_end serial=\s*(\d+) elapsed_ms=\s*(\d+) invalidated=\s*(\d+)',proc.stdout)]
  if result['low']==a.leave_low_mid_fast or result['tx'] or (not result['fast'] and not a.allow_empty_fast):raise RuntimeError('unexpected state '+repr(result))
  if experiment and not any('runFollowup=1' in line for line in result['dispatches']):raise RuntimeError('follow-up was not scheduled '+repr(result))
  if experiment and not (a.disable_deep_mid_fast or a.leave_low_mid_fast) and len(result['begins'])<2:raise RuntimeError('follow-up did not execute '+repr(result))
  if a.disable_deep_mid_fast or a.leave_low_mid_fast:
   changed=[json.loads(line.split('[LOWCPU-CHANGED] ',1)[1]) for line in proc.stdout.splitlines() if '[LOWCPU-CHANGED] ' in line]
   if len(changed)!=1 or not changed[0]['decoding'] or (a.disable_deep_mid_fast and changed[0]['deep']) or (a.leave_low_mid_fast and changed[0]['low']):raise RuntimeError('setting did not change during fast decode')
   if experiment and len(result['begins'])!=1:raise RuntimeError('follow-up ran after setting changed '+repr(result))
  if a.disable_deep_during_followup:
   changed=[json.loads(line.split('[LOWCPU-CHANGED] ',1)[1]) for line in proc.stdout.splitlines() if '[LOWCPU-CHANGED] ' in line]
   if len(changed)!=1 or changed[0]['deep'] or changed[0]['decoding']:raise RuntimeError('Deep Search was not disabled after the fast decode')
   if experiment and (len(result['begins'])<2 or len(result['decoderEnds'])<2):raise RuntimeError('follow-up activity not observable')
  return result
try:
 qml.write_text(original.replace(marker,marker+probe,1))
 baseline=run(False);candidate=run(True)
 fast=counter(baseline['final']);after=counter(candidate['final']);missing=fast-after;added=after-fast
 result={'baseline':baseline,'experiment':candidate,'missing':[{'freq':f,'message':m,'count':n} for (f,m),n in sorted(missing.items())],'added':[{'freq':f,'message':m,'count':n} for (f,m),n in sorted(added.items())]}
 a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,sort_keys=True))
 if missing:raise RuntimeError('baseline rows missing')
finally:qml.write_text(original)
