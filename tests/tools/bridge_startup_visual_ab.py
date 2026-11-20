#!/usr/bin/env python3
"""Compare startup visual staging guard decisions against v1.0.737."""
import json,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[2]
old=subprocess.run(['git','show','v1.0.737:qml/decodium/Main.qml'],cwd=R,text=True,stdout=subprocess.PIPE,check=True).stdout
new=(R/'qml/decodium/Main.qml').read_text()
def section(s):
 start=s.index('    function startupVisualStageCanRun(stage) {');end=s.index('    function maybeFinishStartupWaterfallVisualStage',start);return s[start:end]
script='''const f = new Function("bridge", "startupVisualStagingEnabled", "startupElapsedMs", "stage", SOURCE + "\\nreturn startupVisualStageCanRun(stage);");
const states=[{monitoring:false,decoding:false,transmitting:false,tuning:false,mode:"FT8",periodProgress:60},{monitoring:true,decoding:false,transmitting:false,tuning:false,mode:"FT8",periodProgress:60},{monitoring:false,decoding:true,transmitting:false,tuning:false,mode:"FT8",periodProgress:60},{monitoring:false,decoding:false,transmitting:true,tuning:false,mode:"FT8",periodProgress:10},{monitoring:true,decoding:false,transmitting:false,tuning:false,mode:"FT8",periodProgress:10},{monitoring:false,decoding:false,transmitting:false,tuning:true,mode:"FT8",periodProgress:10},{monitoring:true,decoding:false,transmitting:false,tuning:false,mode:"FT4",periodProgress:60}];
console.log(JSON.stringify(states.map(s=>f(s,true,()=>1500,"waterfall"))));'''
results=[]
for content in (old,new):
 code=script.replace('SOURCE',json.dumps(section(content)),1);r=subprocess.run(['node','-e',code],text=True,stdout=subprocess.PIPE,check=True);results.append(json.loads(r.stdout))
expected=[[False,False,False,False,True,False,True],[True,False,False,False,True,False,True]]
if results!=expected:raise RuntimeError(str(results))
print(json.dumps({'baseline':results[0],'current':results[1],'cases':['idle','monitoring','decoding','transmitting','safe_slot','tuning','ft4']},sort_keys=True))
