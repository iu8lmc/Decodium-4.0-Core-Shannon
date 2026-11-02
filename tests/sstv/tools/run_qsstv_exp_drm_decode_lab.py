#!/usr/bin/env python3
"""Decode a Decodium float-stereo audio capture using QSSTV-Experimental 10.0."""
import argparse
import hashlib
import math
import os
import re
from pathlib import Path
import subprocess
import sys
import tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--start',type=float,default=0.0)
parser.add_argument('--duration',type=float)
parser.add_argument('--expect-no-image',action='store_true')
parser.add_argument('capture',type=Path)
parser.add_argument('reference',type=Path)
options=parser.parse_args()
if not math.isfinite(options.start) or options.start < 0 or (options.duration is not None
        and (not math.isfinite(options.duration) or options.duration <= 0)):
    raise SystemExit('invalid capture interval')
raw=options.capture.resolve(strict=True)
reference=options.reference.resolve(strict=True)
qsstv=Path(os.environ.get('QSSTV_EXPERIMENTAL_BINARY',
    '/tmp/decodium-qsstv-experimental/QSSTV-Experimental.app/Contents/MacOS/qsstv-experimental')).resolve(strict=True)
if hashlib.sha256(qsstv.read_bytes()).hexdigest()!='b2de73b06d2308e8363864555336f70eb0ba7603e7d3734c091da3da1a0cf1fa':
    raise RuntimeError('QSSTV binary does not match the pinned 10.0 ARM64 release')
root=Path(tempfile.mkdtemp(prefix='decodium-qsstv-drm-decode-'))
convert=['ffmpeg','-v','error','-f','f32le','-ar','48000','-ac','2','-ss',str(options.start),'-i',str(raw)]
if options.duration is not None: convert += ['-t',str(options.duration)]
subprocess.run([*convert,'-ar','48000','-ac','2',str(root/'capture.wav')],check=True,timeout=30)
env=os.environ.copy();env['QT_QPA_PLATFORM']='cocoa'
with (root/'qsstv.log').open('wb') as log:
    result=subprocess.run([str(qsstv),'--batch','--drm','--out-dir',str(root),
                           str(root/'capture.wav')],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=90)
log=(root/'qsstv.log').read_text(errors='replace')
if options.expect_no_image:
    crc = re.search(r'\bcrc_ok=(\d+)', log)
    segments = re.search(r'\bdata_seg=(\d+)', log)
    if (result.returncode != 1 or 'no image decoded' not in log or list(root.glob('capture_*_drm.*'))
            or not crc or int(crc.group(1)) == 0 or not segments or int(segments.group(1)) == 0):
        raise RuntimeError('QSSTV did not report a clean no-image control; artifacts '+str(root))
    print('PASS QSSTV no-image control; artifacts',root,flush=True)
    sys.exit(0)
if result.returncode!=0: raise RuntimeError('QSSTV decode failed; artifacts '+str(root))
expected_profile = os.environ.get('QSSTV_EXPECTED_PROFILE')
if expected_profile and expected_profile not in log:
    raise RuntimeError('QSSTV did not report expected profile; artifacts '+str(root))
images=list(root.glob('capture_*_drm.jpg'))
if len(images)!=1: raise RuntimeError('unexpected QSSTV output count; artifacts '+str(root))
if images[0].read_bytes()!=reference.read_bytes():
    raise RuntimeError('QSSTV output differs from reference; artifacts '+str(root))
print('PASS Decodium HAMDRM TX to QSSTV-Experimental 10.0: exact bytes',
      len(images[0].read_bytes()),'artifacts',root,flush=True)
