#!/usr/bin/env python3
"""Compile the extracted Linux branch without changing host audio settings."""
from pathlib import Path
import shlex, subprocess
root = Path(__file__).resolve().parents[2]
flags = shlex.split(subprocess.check_output(['pkg-config','--cflags','Qt6Core','Qt6Multimedia'], text=True))
subprocess.run(['c++','-std=c++17','-fsyntax-only','-DQ_OS_LINUX','-I'+str(root/'src/bridge'),str(root/'src/bridge/DecodiumBridgeTxOutputResolve.cpp'),*flags],check=True)
print('Linux branch syntax and declared symbol signatures: OK; not linked or run on Linux')
