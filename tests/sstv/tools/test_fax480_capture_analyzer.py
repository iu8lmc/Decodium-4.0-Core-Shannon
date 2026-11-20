#!/usr/bin/env python3
"""Exercise capture analysis with two profiles, pictures and VOX-like lead-in."""
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix='decodium-fax480-analysis-') as tmp:
    for profile in ('dayton-480', 'qsstv-500'):
        raw=Path(tmp)/(profile+'.f32')
        ref=Path(tmp)/(profile+'.png')
        subprocess.run([sys.argv[1],profile,str(raw),str(ref)],check=True,timeout=30)
        subprocess.run([sys.argv[2],profile,str(raw),str(ref)],check=True,timeout=30)
        truncated=Path(tmp)/(profile+'-truncated.f32')
        with raw.open('rb') as source, truncated.open('wb') as dest:
            dest.write(source.read(3*48000*8))
        bad=subprocess.run([sys.argv[2],profile,str(truncated),str(ref)],
                           capture_output=True,timeout=30)
        if bad.returncode == 0:
            raise RuntimeError('truncated capture was incorrectly accepted')
print('PASS: both FAX480 capture analyzers with VOX-like lead-in')
