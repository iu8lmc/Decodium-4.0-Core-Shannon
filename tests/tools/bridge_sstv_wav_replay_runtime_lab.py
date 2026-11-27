#!/usr/bin/env python3
"""Compare SSTV WAV replay and cancellation on isolated local synthetic files.

This temporarily instruments the chosen Main.qml; do not run another copy of
that bundle simultaneously. Rebuild the build QML after an unclean exit.
"""
import argparse,json,math,os,secrets,struct,subprocess,tempfile,wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def wav(path,seconds,hz):
 rate=12000
 with wave.open(str(path),'wb') as w:
  w.setnchannels(1);w.setsampwidth(2);w.setframerate(rate)
  block=b''.join(struct.pack('<h',int(9000*math.sin(2*math.pi*hz*i/rate))) for i in range(rate))
  for _ in range(seconds):w.writeframesraw(block)
def main():
 p=argparse.ArgumentParser();p.add_argument('pattern',choices=('a','b'));p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
 if original.count(marker)!=1:raise RuntimeError('unexpected Main.qml')
 rig='BridgeWavReplay'+secrets.token_hex(5)
 try:
  with tempfile.TemporaryDirectory(prefix='decodium-sstv-wav-') as tmp:
   short=Path(tmp)/('tone-'+a.pattern+'.wav');long=Path(tmp)/'cancel.wav';wav(short,4,1900 if a.pattern=='a' else 1500);wav(long,60,1700)
   probe='''\n    Timer { interval: 11000; running: true; repeat: true; property int step:0; property int ticks:0; property double lastProgress:0; onTriggered: {
     var failure="";
     if(step===0) {
       if (!bridge.startSstvWavReplay(__SHORT__)) failure="start:"+bridge.sstvWavReplayError;
       else if (!bridge.sstvWavReplayActive || bridge.sstvWavReplayFileName!==__NAME__) failure="start-state";
       if(failure) { console.log("[WAV-REPLAY-LAB] FAIL "+failure); stop(); return; }
       step=1;interval=200;return;
     }
     if(step===1) {
       if(bridge.sstvWavReplayProgress+0.0001<lastProgress) failure="progress-regressed";
       lastProgress=bridge.sstvWavReplayProgress;
       if(bridge.sstvWavReplayState==="Error") failure="replay:"+bridge.sstvWavReplayError;
       if(!failure && bridge.sstvWavReplayState!=="Completed") { if(++ticks<60)return;failure="completion-timeout"; }
       if(!failure && (bridge.sstvWavReplayActive || bridge.sstvWavReplayProgress<0.999 || bridge.sstvWavReplayError!=="")) failure="completion-state";
       if(!failure && !bridge.startSstvWavReplay(__LONG__)) failure="cancel-start";
       if(!failure) { bridge.cancelSstvWavReplay();step=2;ticks=0;return; }
     } else {
       if(bridge.sstvWavReplayState==="Error") failure="cancel-error:"+bridge.sstvWavReplayError;
       if(!failure && bridge.sstvWavReplayState!=="Cancelled") { if(++ticks<60)return; failure="cancel-timeout:"+bridge.sstvWavReplayState; }
       if(!failure && bridge.sstvWavReplayActive) failure="cancel-active";
       if(!failure) { console.log("[WAV-REPLAY-LAB] PASS");stop();return; }
     }
     console.log("[WAV-REPLAY-LAB] FAIL "+failure);stop();
    } }\n'''.replace('__SHORT__',json.dumps(short.as_uri())).replace('__LONG__',json.dumps(long.as_uri())).replace('__NAME__',json.dumps(short.name))
   qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
   cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','30000']
   result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=110)
   if result.returncode or '[WAV-REPLAY-LAB] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
    print(result.stdout[-16000:]);raise RuntimeError(f'WAV replay probe failed (exit {result.returncode})')
   print(f'{a.pattern}: synthetic WAV replay, progress and cancellation passed; CAT/monitoring disabled')
 finally:qml.write_text(original)
if __name__=='__main__':main()
