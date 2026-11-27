#!/usr/bin/env python3
"""Check extraction identity, optionally compare two real application binaries."""
import argparse, subprocess
from pathlib import Path
R = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--before-binary', type=Path)
p.add_argument('--after-binary', type=Path, default=R/'build/decodium')
a = p.parse_args()
old = (R/'tests/bridge_refactor_fixtures/decode_enrichment_before.inc').read_text()
s = (R/'src/bridge/DecodiumBridgeDecodeEnrichment.cpp').read_text()
signature = 'QString DecodiumBridge::lookupUsStateForDecode('
new = signature + s.split(signature, 1)[1]
new = new.replace('decodiumBridgeExtractRightCallsign(msg)', 'extractRightCallsign(msg)')
if old != new: raise SystemExit('Unexpected change in extraction')
print('Enrichment methods match the pre-extraction snapshot (existing callsign wrapper).', flush=True)
if a.before_binary:
 for variant in ('a', 'b'):
  outputs=[]
  for binary in (a.before_binary, a.after_binary):
   outputs.append(subprocess.check_output(['python3',str(R/'tests/tools/bridge_enrichment_runtime_lab.py'),'--binary',str(binary.resolve()),'--variant',variant],text=True))
  if outputs[0] != outputs[1]: raise SystemExit(f'{variant}: app outputs differ: {outputs}')
  print(f'{variant}: {outputs[0].strip()}',flush=True)
