#!/usr/bin/env python3
"""Compare standalone FT8 depth 2/3 on identical WAVs with two OpenMP threads."""
import argparse, collections, json, os, re, resource, subprocess, time
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--wav',type=Path,nargs='+',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--repeats',type=int,default=2);p.add_argument('--max-ms',type=int,default=2800);a=p.parse_args()
if a.max_ms<=0:raise ValueError('deadline must be positive')
env=os.environ.copy();env.update(OMP_NUM_THREADS='2',OMP_DYNAMIC='FALSE')
pattern=re.compile(r'freq=\s*(-?\d+)\s+.*?decoded="([^"]+)"')
def run(wav,depth):
 cmd=[str(a.binary.resolve()),'--stages','4','--no-early','--depth',str(depth),'--threads','2','--nfa','300','--nfb','2700','--nfqso','1500','--lft8apon','0','--ldiskdat','0','--max-ms',str(a.max_ms),'--candthin','20',str(wav.resolve())]
 before=resource.getrusage(resource.RUSAGE_CHILDREN);start=time.monotonic()
 p=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=30)
 wall=time.monotonic()-start;after=resource.getrusage(resource.RUSAGE_CHILDREN)
 if p.returncode or 'stage 4: nout=' not in p.stdout:raise RuntimeError(p.stdout[-8000:])
 rows=sorted((int(f),' '.join(m.upper().split())) for f,m in pattern.findall(p.stdout))
 return {'rows':rows,'wallSeconds':round(wall,4),'cpuSeconds':round(after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime,4)}
def compare_with_tolerance(base,deep,tolerance):
 missing=[];added=[]
 for message in sorted({r[1] for r in base+deep}):
  left=sorted(r[0] for r in base if r[1]==message);right=sorted(r[0] for r in deep if r[1]==message);i=j=0
  while i<len(left) and j<len(right):
   if left[i]<right[j]-tolerance:missing.append((left[i],message));i+=1
   elif right[j]<left[i]-tolerance:added.append((right[j],message));j+=1
   else:i+=1;j+=1
  missing.extend((f,message) for f in left[i:]);added.extend((f,message) for f in right[j:])
 return sorted(missing),sorted(added)
report=[]
for wav in a.wav:
 rows={'wav':str(wav.resolve()),'deadlineMs':a.max_ms,'runs':[]}
 for i in range(a.repeats):
  results={}
  for depth in ([2,3] if i%2==0 else [3,2]):results[str(depth)]=run(wav,depth)
  base=list(map(tuple,results['2']['rows']));deep=list(map(tuple,results['3']['rows']))
  basecounts=collections.Counter(base);deepcounts=collections.Counter(deep)
  results['missing']=sorted((basecounts-deepcounts).elements());results['added']=sorted((deepcounts-basecounts).elements())
  results['missingWithin2Hz'],results['addedWithin2Hz']=compare_with_tolerance(base,deep,2)
  rows['runs'].append(results)
 report.append(rows);print(json.dumps(rows),flush=True)
a.output.write_text(json.dumps(report,indent=2)+'\n')
