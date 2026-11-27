#!/usr/bin/env python3
"""Build reproducible multi-station FT2/FT4 WAVs for parity tests."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import wave
import array
import random
import sys

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--mode',choices=('ft2','ft4'),required=True)
p.add_argument('--generator',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--seed',type=int,default=20261115)
a=p.parse_args()
stations=list(zip(('K1ABC','W2XYZ','N3ABC','K4ABC','W5ABC','N6ABC','K7ABC','W8ABC'),
                  (400,700,1000,1300,1600,1900,2200,2500)))
signal=None
with tempfile.TemporaryDirectory(prefix='decodium-ft24-busy-') as tmp:
    for call,freq in stations:
        target=Path(tmp)/(call+'.wav')
        subprocess.run([str(a.generator.resolve()),'--message',f'CQ {call} FN42',
                        '--freq',str(freq),'--gain','.025',str(target)],check=True,stdout=subprocess.DEVNULL)
        with wave.open(str(target)) as source:
            if (source.getnchannels(),source.getsampwidth(),source.getframerate())!=(1,2,12000):
                raise RuntimeError('unexpected WAV format')
            samples=array.array('h',source.readframes(source.getnframes()))
            if sys.byteorder!='little':samples.byteswap()
        if signal is None:signal=[0.0]*len(samples)
        if len(samples)!=len(signal):raise RuntimeError('incompatible lengths')
        for i,sample in enumerate(samples):signal[i]+=sample/32767.0
rng=random.Random(a.seed)
signal=[sample+rng.gauss(0,.012) for sample in signal]
peak=max(abs(sample) for sample in signal)
if peak>=1:raise RuntimeError('fixture clips')
a.output.parent.mkdir(parents=True,exist_ok=True)
with wave.open(str(a.output),'wb') as out:
    out.setnchannels(1);out.setsampwidth(2);out.setframerate(12000)
    samples=array.array('h',(round(value*32767) for value in signal))
    if sys.byteorder!='little':samples.byteswap()
    out.writeframes(samples.tobytes())
manifest={'mode':a.mode,'file':str(a.output),'sha256':hashlib.sha256(a.output.read_bytes()).hexdigest(),
          'generatorSha256':hashlib.sha256(a.generator.read_bytes()).hexdigest(),'seed':a.seed,
          'noiseRms':.012,'signalGain':.025,'peakAbs':peak,
          'stations':[{'message':f'CQ {call} FN42','freq':freq} for call,freq in stations],
          'scope':'synthetic multi-station signal; not calibrated RF sensitivity'}
a.output.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest))
