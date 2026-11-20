#!/usr/bin/env python3
"""Compare the native FST4 OSD CRC implementation with a source snapshot."""
import ctypes
import argparse
import os
from pathlib import Path
import random
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--baseline', type=Path, help='source snapshot before the CRC change')
args = parser.parse_args()
old = args.baseline.read_text() if args.baseline else subprocess.check_output(
    ['git', 'show', 'HEAD:Detector/FtxFst4Ldpc.cpp'], cwd=root, text=True)
new = (root/'Detector/FtxFst4Ldpc.cpp').read_text()
tail = r'''
extern "C" void four2a_(std::complex<float>*,int*,int*,int*,int*) {}
extern "C" void gen_fst4wave_(int*,int*,int*,int*,float*,int*,float*,int*,std::complex<float>*,float*) {}
extern "C" int lab_crc(signed char const* bits,int len) { return fst4_crc24_local(bits,len); }
'''
rng = random.Random(43125)
with tempfile.TemporaryDirectory(prefix='fst4-crc-ab-') as tmp:
    folder = Path(tmp); functions = []
    for name, data in [('old',old),('new',new)]:
        source = folder/(name+'.cpp'); library = folder/(name+'.so'); source.write_text(data+'\n'+tail)
        subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-Os','-std=c++17','-fPIC','-shared',
                        '-I'+str(root),'-I'+str(root/'build-baseline'),str(source),'-o',str(library)],check=True)
        lib = ctypes.CDLL(str(library)); fn = lib.lab_crc
        fn.argtypes = [ctypes.POINTER(ctypes.c_byte),ctypes.c_int]; fn.restype = ctypes.c_int
        functions.append((lib,fn))
    tested=0
    for length in list(range(0,110))+[127,128,239,240,512]:
        for trial in range(100):
            values = [rng.randrange(-128,128) if trial%4==0 else rng.randrange(2) for _ in range(max(length,1))]
            arr=(ctypes.c_byte*len(values))(*values)
            a,b=(fn(arr,length) for _,fn in functions)
            if a != b: raise SystemExit('CRC mismatch len=%d trial=%d old=%d new=%d'%(length,trial,a,b))
            tested+=1
    for length in (0,24,25,74,101):
        assert all(fn(None,length)==0 for _,fn in functions)
    print('identical CRCs:', tested, 'vectors, plus null inputs')
