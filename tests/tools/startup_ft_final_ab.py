#!/usr/bin/env python3
"""Compare startup log milestones for two Linux application binaries."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[2]
out=root/'build-baseline/performance/results'
results=[]
for index,binary in enumerate(('decodium-before','decodium','decodium','decodium-before')):
    with tempfile.TemporaryDirectory(prefix='decodium-startup-ab-') as tmp:
        rig='Start'+secrets.token_hex(5)
        ini=Path(tmp)/'Decodium/Decodium3.ini';ini.parent.mkdir(parents=True)
        ini.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n[%s]\n_ProfileSettingsInitialized=true\nLowCpuMode=true\nlowCpuModeEnabled=true\n'%rig)
        env=dict(os.environ,DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd=[str(root/'build-baseline'/binary),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8','--lab-quit-ms','25000']
        started=time.monotonic();p=subprocess.Popen(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
        events={};lines=[]
        try:
            for line in p.stdout:
                lines.append(line)
                for label,needle in (('bridge','bridge OK'),('mainCreated','Main.qml created as top-level window'),('waterfallStage','waterfall visual stage ready')):
                    if needle in line and label not in events:events[label]=round(time.monotonic()-started,3)
                if 'waterfallStage' in events:break
            if 'mainCreated' not in events or 'waterfallStage' not in events:
                raise RuntimeError('missing startup milestone: '+''.join(lines[-30:]))
        finally:
            if p.poll() is None:p.terminate()
            p.wait(timeout=10)
        (out/f'ft-startup-final-{index}.log').write_text(''.join(lines))
        result={'binary':binary,'milestonesSeconds':events,'monitor':False,'cat':False}
        results.append(result);print(json.dumps(result,sort_keys=True),flush=True)
        (out/'ft-startup-final-ab.json').write_text(json.dumps(results,indent=2)+'\n')
