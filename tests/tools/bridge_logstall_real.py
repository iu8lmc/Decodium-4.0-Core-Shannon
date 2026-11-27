#!/usr/bin/env python3
"""Exercise the extracted log-stall translation unit with real Qt events."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-logstall-real-') as tmp:
 out=Path(tmp)/'test'
 subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(R/'tests/bridge_refactor_logstall_real_harness.cpp'),str(R/'src/bridge/DecodiumBridgeLogStallProbe.cpp'),*flags,'-o',str(out)],check=True)
 result=subprocess.run([str(out)],text=True,stdout=subprocess.PIPE,check=True)
 if result.stdout.strip()!='reports|1|1|2|1':raise RuntimeError(result.stdout)
 print(result.stdout,end='')
