#!/usr/bin/env python3
"""Two-app FT8 AutoCQ cadence proof over BlackHole, never CAT/RF.

Temporarily instruments the chosen Main.qml. Avoid concurrent use of the same
bundle and rebuild QML after an unclean exit.
"""
import argparse,os,re,secrets,sqlite3,subprocess,tempfile,time
import numpy as np
import sounddevice as sd
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
 p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();binary=a.binary.resolve();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
 if original.count(marker)!=1:raise RuntimeError('unexpected Main.qml')
 suffix=secrets.token_hex(5);rxname='BridgeCqRx'+suffix;txname='BridgeCqTx'+suffix
 probe='''\n    Timer { interval: 2000; running: true; repeat: false; onTriggered: {
      bridge.autoCqBurstCalls=1; bridge.autoCqListenCycles=2; bridge.autoCqMaxCycles=2; bridge.autoCqPauseSec=0;
      console.log("[AUTOCQ-LAB] cadence configured");
    } }
    Connections { target: bridge
      function onTransmittingChanged() { console.log("[AUTOCQ-LAB] state", Date.now(), bridge.transmitting); }
      function onAutoCqRepeatChanged() { console.log("[AUTOCQ-LAB] repeat", Date.now(), bridge.autoCqRepeat); }
    }\n'''
 processes=[]
 try:
  with tempfile.TemporaryDirectory(prefix='decodium-autocq-pair-') as tmp:
   root=Path(tmp);qml.write_text(original.replace(marker,marker+probe,1))
   def env(role):
    d=root/(role+'-settings');d.mkdir();e=os.environ.copy();e.update(DECODIUM_LAB_SETTINGS_DIR=str(d),DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='cocoa',QT_QUICK_BACKEND='software');return e
   common=[str(binary),'--test-mode','--lab-no-cat','--lab-mode','FT8','--lab-grid','JM75','--lab-monitor-ms','2500']
   rx=common+['--rig-name',rxname,'--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 16ch','--lab-callsign','IU8LMC','--lab-quit-ms','190000']
   tx=common+['--rig-name',txname,'--lab-audio-input','BlackHole 16ch','--lab-audio-output','BlackHole 2ch','--lab-callsign','9H1SR','--lab-tx-output-level','70','--lab-tx-period','0','--lab-standard-autocq-ms','12000','--lab-quit-ms','180000']
   files=[]
   output=next(i for i,d in enumerate(sd.query_devices()) if d['name']=='BlackHole 16ch' and d['max_output_channels']>=2)
   generator=np.random.default_rng(1047)
   def noise_callback(outdata,frames,time_info,status):
    outdata[:]=generator.normal(0.0,0.012,(frames,2)).astype('float32')
   noise=sd.OutputStream(device=output,channels=2,samplerate=48000,dtype='float32',callback=noise_callback)
   try:
    noise.start()
    for role,args in [('rx',rx),('tx',tx)]:
     f=(root/(role+'.log')).open('w');files.append(f);processes.append(subprocess.Popen(args,stdout=f,stderr=subprocess.STDOUT,env=env(role)))
     if role=='rx':time.sleep(8)
    db=Path.home()/'.qttest/Library/Application Support/IU8LMC'/('Decodium - '+rxname+' - test')/'db.sqlite'
    deadline=time.monotonic()+185;matches=[];txtext=''
    while time.monotonic()<deadline:
     txtext=(root/'tx.log').read_text(errors='replace')
     states=[(int(t),v=='true') for t,v in re.findall(r'\[AUTOCQ-LAB\] state\s+(\d+)\s+(true|false)',txtext)]
     starts=[t for t,v in states if v]
     stops=[t for t,v in states if not v]
     stopped=bool(re.search(r'\[AUTOCQ-LAB\] repeat\s+\d+\s+false',txtext))
     cadence_ok=len(starts)>=2 and len(stops)>=2 and starts[1]-stops[0]>=28000 and stopped
     if db.exists():
      try:
       with sqlite3.connect('file:'+str(db)+'?mode=ro',uri=True) as conn:matches=[r[0] for r in conn.execute("select message from decodes where message like '%9H1SR%'")]
      except sqlite3.Error:pass
     if cadence_ok and matches:break
     if any(x.poll() is not None for x in processes):break
     time.sleep(.5)
    ok=('[LAB] CAT auto-connect disabled' in txtext and '[AUTOCQ-LAB] cadence configured' in txtext and cadence_ok and bool(matches))
    if not ok:
     print('TX relevant log:')
     for line in txtext.splitlines():
      if re.search(r'AutoCQ|AUTOCQ-LAB|\[LAB\].*(CAT|monitor)|startTx|TX audio',line,re.I):print(line)
     print('RX matches:',matches)
     raise RuntimeError('AutoCQ cadence or RX decode was not observed')
    print('AutoCQ: one-call burst, two-cycle listen, two-call stop, and FT8 RX decode passed; BlackHole only; CAT disabled')
    print(f'first completed TX to second TX: {starts[1]-stops[0]} ms; decoded matching messages: {len(matches)}')
   finally:
    noise.stop();noise.close()
    for process in processes:
     if process.poll() is None:process.terminate()
    for process in processes:
     try:process.wait(timeout=15)
     except subprocess.TimeoutExpired:process.kill();process.wait()
    for f in files:f.close()
 finally:qml.write_text(original)
if __name__=='__main__':main()
