#!/usr/bin/env python3
"""Check whether legacy Band Activity insertion/filter changed from baseline."""
import subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=subprocess.run(['git','show','v1.0.737:src/bridge/DecodiumBridge.cpp'],cwd=R,check=True,text=True,stdout=subprocess.PIPE).stdout
b=(R/'src/bridge/DecodiumBridge.cpp').read_text();c=(R/'src/bridge/DecodiumBridgeDecodeModelFilter.cpp').read_text()
def sec(s,start,end):i=s.index(start);return s[i:s.index(end,i)]
for name,start,end,new in [
 ('append-unique','    auto appendUniqueEntries = ','    auto updateActiveStationsFromEntries = ',b),
 ('legacy-band-delta','        if (useBandDelta) {','        } else {\n        invalidateLegacyDecodeModelDeltaFastPath();',b),
 ('band-filter','QVariantList DecodiumBridge::filterEntriesForBandActivity(', 'QVariantList DecodiumBridge::filterEntriesForRxDecode(',c)]:
 if sec(a,start,end)!=sec(new,start,end):raise SystemExit(name+' differs')
 print(name+': source equivalent')
