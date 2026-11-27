#!/usr/bin/env python3
"""Compare idle memory mappings with default and two glibc malloc arenas."""
import json
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2];out=root/'build-baseline/performance/results';results=[]
for i,setting in enumerate(('default','2','2','default')):
 env=os.environ.copy()
 if setting=='default':env.pop('MALLOC_ARENA_MAX',None)
 else:env['MALLOC_ARENA_MAX']=setting
 run=subprocess.run(['python3','tests/tools/idle_memory_map_profile.py'],cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
 if run.returncode:raise RuntimeError(run.stdout)
 data=json.loads(run.stdout.strip());(out/f'idle-allocator-arena-{i}.json').write_text(json.dumps(data,indent=2)+'\n')
 groups={x['name']:x['rssKiB'] for x in data['groups']}
 result={'setting':setting,'rssKiB':data['totalRssKiB'],'heapKiB':groups.get('[heap]'), 'anonymousKiB':groups.get('[anonymous/other]'),'cpuSeconds':data['cpuSeconds']};results.append(result);print(json.dumps(result,sort_keys=True),flush=True)
(out/'idle-allocator-arena-ab.json').write_text(json.dumps(results,indent=2)+'\n')
