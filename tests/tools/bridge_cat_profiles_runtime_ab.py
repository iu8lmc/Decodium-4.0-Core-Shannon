#!/usr/bin/env python3
"""Compare CAT profile operations before and after the source extraction.

Uses temporary settings and a synthetic rig. No CAT connection or TX is made.
"""
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
main = ROOT / 'src/bridge/DecodiumBridge.cpp'
module = ROOT / 'src/bridge/DecodiumBridgeCatProfiles.cpp'
original_main = main.read_text()
original_module = module.read_text()

try:
    for placement in ('before', 'after'):
        main.write_text(original_main + '\n' + original_module if placement == 'before' else original_main)
        module.write_text('#include "DecodiumBridge.h"\n' if placement == 'before' else original_module)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=ROOT, check=True)
        for variant in ('a', 'b'):
            result = subprocess.run(['python3', 'tests/tools/bridge_cat_profiles_runtime_lab.py', '--variant', variant],
                                    cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=150)
            print(placement, variant, result.stdout, flush=True)
            if result.returncode:
                raise RuntimeError(f'{placement} {variant} failed')
finally:
    main.write_text(original_main)
    module.write_text(original_module)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=ROOT, check=True)
