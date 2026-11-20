#!/usr/bin/env python3
"""Compare Decometer repaint policy during isolated two-slot FT8 reception."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

root=Path(__file__).resolve().parents[2]
meter=root/'build-baseline/qml/decodium/components/DecometerWindow.qml'
source=root/'qml/decodium/components/DecometerWindow.qml'
original=meter.read_text();current=source.read_text()
start=current.index('        // The gauge is expensive to paint')
end=current.index('\n    }\n\n    function setRangeAuto',start)
reference=current[:start]+'        gauge.requestPaint()'+current[end:]
out=root/'build-baseline/performance/results';results=[]
mode=sys.argv[1].upper() if len(sys.argv)>1 else 'FT8'
if mode not in ('FT8','FT8_BUSY','FT4','FT2'):raise ValueError('unsupported mode')
try:
    for i,name in enumerate(('reference','current','current','reference')):
        meter.write_text(reference if name=='reference' else current)
        env=dict(os.environ,QT_QUICK_BACKEND='software',DECODIUM_LAB_OPEN_METER='1')
        if mode=='FT8':env['DECODIUM_LAB_TWO_SLOTS']='1'
        elif mode=='FT8_BUSY':
            env.update(DECODIUM_LAB_MODE='FT8',DECODIUM_LAB_FIRST_WAV=str(root/'build-baseline/performance/wavs-ft8-reference/181201_180245.wav'),DECODIUM_LAB_EXPECT_FIRST='W3RGA KM6JD RR73|1879')
        else:
            env.update(DECODIUM_LAB_MODE=mode,DECODIUM_LAB_FIRST_WAV=str(root/f'build-baseline/performance/wavs-ft2-ft4/busy-{mode.lower()}-20261115.wav'),DECODIUM_LAB_EXPECT_FIRST='CQ K1ABC FN42|400')
        run=subprocess.run(['dbus-run-session','--','python3','tests/tools/live_virtual_audio_lab.py','decodium'],cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
        if run.returncode:raise RuntimeError(run.stdout)
        samples=[json.loads(l.split('MEASUREMENT ',1)[1]) for l in run.stdout.splitlines() if l.startswith('MEASUREMENT ')]
        if len(samples)!=1:raise RuntimeError(run.stdout)
        log=out/('live-audio-decodium-visible-two-slots-meter-open.log' if mode=='FT8' else ('live-audio-decodium-visible-meter-open-custom-first.log' if mode=='FT8_BUSY' else f'live-audio-decodium-visible-{mode.lower()}-meter-open-custom-first.log'))
        if mode in ('FT2','FT4'):
            states=[json.loads(l.split('[LIVE-PROBE] ',1)[1]) for l in log.read_text().splitlines() if '[LIVE-PROBE] ' in l]
            received={r['message'] for s in states for r in s['rows']}
            expected={'CQ K1ABC FN42','CQ W2XYZ FN42','CQ N3ABC FN42','CQ K4ABC FN42','CQ W5ABC FN42','CQ N6ABC FN42','CQ K7ABC FN42','CQ W8ABC FN42'}
            if not expected<=received:raise RuntimeError('missing FT2 messages: '+repr(expected-received))
        if mode=='FT8_BUSY':
            states=[json.loads(l.split('[LIVE-PROBE] ',1)[1]) for l in log.read_text().splitlines() if '[LIVE-PROBE] ' in l]
            samples[0]['received']=sorted({(r['message'],r['freq']) for s in states for r in s['rows']})
        suffix='' if mode=='FT8' else '-'+mode.lower().replace('_','-')
        shutil.copy2(log,out/f'decometer-rx-cpu{suffix}-{i}.log')
        result={'variant':name,**samples[0]};results.append(result)
        print(json.dumps(result,sort_keys=True),flush=True)
        (out/f'decometer-rx-cpu{suffix}-ab.json').write_text(json.dumps(results,indent=2)+'\n')
finally:
    meter.write_text(original)
