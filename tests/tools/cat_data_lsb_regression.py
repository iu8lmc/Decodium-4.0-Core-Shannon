#!/usr/bin/env python3
"""Run the CAT setting mapper against the original compatibility cases and LSB cases."""
from pathlib import Path
import os, shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[2]
source = (root / 'src/bridge/DecodiumBridgeCatModes.cpp').read_text().split('\n', 1)[1]
harness = (root / 'tests/bridge_refactor_cat_modes_harness.cpp').read_text()
anchor = ' b.values["CATMode"]=" None ";'
assert harness.count(anchor) == 1
extra = ''' for(QString alias:{"Data/Pkt LSB","DATA-L","PKTLSB","DIGL"}){b.values["CATMode"]=alias;check(b.configuredCatRigMode()=="DATA-L"&&b.configuredCatRigModeRequestsDataPacket(),"lower-data-alias");}
 b.values["CATMode"]=3;check(b.configuredCatRigMode()=="DATA-L"&&b.configuredCatRigModeRequestsDataPacket(),"numeric-lower");
'''
harness = harness.replace(anchor, extra + anchor)
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
with tempfile.TemporaryDirectory(prefix='cat-data-lsb-') as temporary:
    path = Path(temporary)
    (path / 'bridge_refactor_cat_modes.inc').write_text(source)
    (path / 'main.cpp').write_text(harness)
    subprocess.run([*shlex.split(os.environ.get('CXX', 'c++')), '-std=c++17', str(path / 'main.cpp'), *flags, '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
