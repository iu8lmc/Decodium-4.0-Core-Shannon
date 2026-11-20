#!/usr/bin/env python3
"""Measure idle application process and thread CPU after startup on Linux."""
import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument('--binary',type=Path,default=root/'build-baseline/decodium')
p.add_argument('--low-cpu',action='store_true')
p.add_argument('--settle-seconds',type=int,default=18)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--log',type=Path,required=True)
a=p.parse_args()
ticks=os.sysconf('SC_CLK_TCK')
def cpu(path):
    text=path.read_text(); fields=text.split(') ',1)[1].split()
    return (int(fields[11])+int(fields[12]))/ticks

def sample(pid):
    base=Path('/proc')/str(pid)
    process=cpu(base/'stat'); threads={}
    for task in (base/'task').iterdir():
        try:
            name=(task/'comm').read_text().strip()
            threads[task.name]=(name,cpu(task/'stat'))
        except (FileNotFoundError,ProcessLookupError):
            continue
    status=(base/'status').read_text().splitlines()
    rss=next((int(x.split()[1])*1024 for x in status if x.startswith('VmRSS:')),None)
    return process,threads,rss

with tempfile.TemporaryDirectory(prefix='decodium-idle-profile-') as tmp:
    rig='Idle'+secrets.token_hex(5)
    ini=Path(tmp)/'Decodium'/'Decodium3.ini';ini.parent.mkdir(parents=True)
    low=str(a.low_cpu).lower()
    ini.write_text('[General]\nLowCpuMode=%s\nlowCpuModeEnabled=%s\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=%s\nlowCpuModeEnabled=%s\n'%(low,low,rig,low,low))
    env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_FORCE_STANDALONE_UDP='1')
    cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor',
         '--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms',str((a.settle_seconds+16)*1000)]
    with a.log.open('w') as output:
        proc=subprocess.Popen(cmd,env=env,stdout=output,stderr=subprocess.STDOUT)
        try:
            time.sleep(a.settle_seconds)
            if proc.poll() is not None:raise RuntimeError('application exited before sampling')
            start=time.monotonic(); first=sample(proc.pid)
            time.sleep(10)
            second=sample(proc.pid); duration=time.monotonic()-start
            exit_code=proc.wait(timeout=15)
            if exit_code:raise RuntimeError('application exited with %s'%exit_code)
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:proc.wait(timeout=5)
                except subprocess.TimeoutExpired:proc.kill();proc.wait()
    log=a.log.read_text()
    if 'Main.qml created as top-level window' not in log or '[LAB] CAT auto-connect disabled' not in log:
        raise RuntimeError('application did not reach main window')
    rows=[]
    for tid,(name,end) in second[1].items():
        if tid in first[1] and first[1][tid][0]==name:
            delta=round(end-first[1][tid][1],3)
            if delta>0:rows.append({'thread':name,'tid':int(tid),'cpuSeconds':delta})
    result={'lowCpuRequested':a.low_cpu,'settleSeconds':a.settle_seconds,'durationSeconds':round(duration,3),
            'processCpuSeconds':round(second[0]-first[0],3),'rssBytes':second[2],
            'threads':sorted(rows,key=lambda x:x['cpuSeconds'],reverse=True)}
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,sort_keys=True))
