#!/usr/bin/env python3
"""Decode a Decodium float-stereo audio capture using QSSTV-Experimental 10.0."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

if len(sys.argv)!=3: raise SystemExit('expected capture.f32 reference.jpg')
raw=Path(sys.argv[1]).resolve(strict=True)
reference=Path(sys.argv[2]).resolve(strict=True)
qsstv=Path(os.environ.get('QSSTV_EXPERIMENTAL_BINARY',
    '/tmp/decodium-qsstv-experimental/QSSTV-Experimental.app/Contents/MacOS/qsstv-experimental')).resolve(strict=True)
if hashlib.sha256(qsstv.read_bytes()).hexdigest()!='b2de73b06d2308e8363864555336f70eb0ba7603e7d3734c091da3da1a0cf1fa':
    raise RuntimeError('QSSTV binary does not match the pinned 10.0 ARM64 release')
root=Path(tempfile.mkdtemp(prefix='decodium-qsstv-drm-decode-'))
subprocess.run(['ffmpeg','-v','error','-f','f32le','-ar','48000','-ac','2','-i',str(raw),
                '-ar','48000','-ac','2',str(root/'capture.wav')],check=True,timeout=30)
env=os.environ.copy();env['QT_QPA_PLATFORM']='cocoa'
with (root/'qsstv.log').open('wb') as log:
    result=subprocess.run([str(qsstv),'--batch','--drm','--out-dir',str(root),
                           str(root/'capture.wav')],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=90)
if result.returncode!=0: raise RuntimeError('QSSTV decode failed; artifacts '+str(root))
expected_profile = os.environ.get('QSSTV_EXPECTED_PROFILE')
if expected_profile and expected_profile not in (root/'qsstv.log').read_text(errors='replace'):
    raise RuntimeError('QSSTV did not report expected profile; artifacts '+str(root))
images=list(root.glob('capture_*_drm.jpg'))
if len(images)!=1: raise RuntimeError('unexpected QSSTV output count; artifacts '+str(root))
if images[0].read_bytes()!=reference.read_bytes():
    raise RuntimeError('QSSTV output differs from reference; artifacts '+str(root))
print('PASS Decodium HAMDRM TX to QSSTV-Experimental 10.0: exact bytes',
      len(images[0].read_bytes()),'artifacts',root,flush=True)
