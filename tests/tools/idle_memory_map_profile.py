#!/usr/bin/env python3
"""Inspect resident mappings of an isolated idle LOW CPU FT8 session on Linux."""
from collections import defaultdict
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='decodium-map-profile-') as tmp:
 rig='Mem'+secrets.token_hex(5);ini=Path(tmp)/'Decodium'/'Decodium3.ini';ini.parent.mkdir(parents=True)
 ini.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n'%rig)
 env=dict(os.environ,QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_FORCE_STANDALONE_UDP='1')
 cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms','40000']
 log=root/'build-baseline/performance/results/idle-memory-map.log'
 with log.open('w') as f:
  proc=subprocess.Popen(cmd,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT)
  try:
   time.sleep(23)
   if proc.poll() is not None:raise RuntimeError('app exited before sampling')
   lines=(Path('/proc')/str(proc.pid)/'smaps').read_text().splitlines();groups=defaultdict(lambda:{'rssKiB':0,'privateKiB':0,'mappings':0})
   current=None; current_map=None; anonymous=[]
   for line in lines:
    if '-' in line.split(' ',1)[0] and not line.startswith(('VmFlags','THPeligible')):
     fields=line.split(None,5);name=fields[5] if len(fields)>5 else '[anonymous]'
     if name.startswith('/'):
      key=name
     elif name.startswith('[stack'):key='[stack]'
     elif name.startswith('[heap'):key='[heap]'
     else:key='[anonymous/other]'
     current=groups[key];current['mappings']+=1
     start_hex,end_hex=fields[0].split('-',1)
     current_map={'kind':key,'sizeKiB':(int(end_hex,16)-int(start_hex,16))//1024,'rssKiB':0}
     if key in ('[anonymous/other]','[heap]','[stack]'):anonymous.append(current_map)
    elif current is not None:
     if line.startswith('Rss:'):
      value=int(line.split()[1]);current['rssKiB']+=value;current_map['rssKiB']=value
     elif line.startswith(('Private_Clean:','Private_Dirty:')):current['privateKiB']+=int(line.split()[1])
   cpu_fields=(Path('/proc')/str(proc.pid)/'stat').read_text().rsplit(')',1)[1].split()
   result={'totalRssKiB':sum(x['rssKiB'] for x in groups.values()),'groups':sorted((dict(name=k,**v) for k,v in groups.items()),key=lambda x:x['rssKiB'],reverse=True)[:30], 'largestAnonymous':sorted(anonymous,key=lambda x:x['rssKiB'],reverse=True)[:15], 'lowCpuRequested':True,'monitor':False,'renderer':'software offscreen','sampleAfterSeconds':23,'cpuSeconds':(int(cpu_fields[11])+int(cpu_fields[12]))/os.sysconf('SC_CLK_TCK'),'allocatorArenaMax':env.get('MALLOC_ARENA_MAX','default')}
   (root/'build-baseline/performance/results/idle-memory-map.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,sort_keys=True),flush=True)
  finally:
   if proc.poll() is None:proc.terminate();proc.wait(timeout=10)
 if 'Main.qml created as top-level window' not in log.read_text():raise RuntimeError('main window not confirmed')
