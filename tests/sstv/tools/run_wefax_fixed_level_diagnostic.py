#!/usr/bin/env python3
"""Run an external WEFAX Decoder with a fixed level for diagnostics only.

Usage: python run_wefax_fixed_level_diagnostic.py CHECKOUT [decoder options]
This deliberately changes the independent decoder and is not an interoperability
test of the unmodified application.
"""
from pathlib import Path
import runpy
import sys

if len(sys.argv) < 2:
    raise SystemExit('expected WEFAX Decoder checkout and decoder options')
checkout = Path(sys.argv[1]).resolve(strict=True)
entry = checkout / 'wefax_decoder.py'
if not entry.is_file() or not (checkout / 'wefax' / 'decode.py').is_file():
    raise SystemExit('not a WEFAX Decoder checkout')
sys.path.insert(0, str(checkout))
import numpy as np
from wefax import decode


def fixed_level(fr, fs, block_s=8.0):
    return np.full(len(fr), 400.0, np.float32)


decode.level_scale = fixed_level
sys.argv = [str(entry), *sys.argv[2:]]
runpy.run_path(str(entry), run_name='__main__')
