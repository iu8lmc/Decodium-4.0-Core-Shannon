#!/usr/bin/env python3
"""Build a scalar-only reference, then restore the normal local binary."""
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[2]
source=root/'Detector/FtxSync.cpp'
original=source.read_text()
anchor='#define DECODIUM_FT2_SYNC_SSE2 1'
if original.count(anchor)!=1:raise RuntimeError('unexpected source structure')
try:
    source.write_text(original.replace(anchor,'// Scalar-only local benchmark reference',1))
    with (root/'build-baseline/performance/ft2-scalar-reference-build.log').open('w') as log:
        subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,
                       stdout=log,stderr=subprocess.STDOUT,check=True)
    target=root/'build-baseline/decodium-ft2-scalar-reference'
    shutil.copy2(root/'build-baseline/decodium',target)
    print('Scalar-only reference:',target,flush=True)
finally:
    source.write_text(original)
    subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,
                   stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
