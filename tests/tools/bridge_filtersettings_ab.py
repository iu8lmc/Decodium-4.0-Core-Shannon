#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
a=(R/'tests/bridge_refactor_fixtures/decode_filter_snapshot_before.inc').read_text().strip();new=(R/'src/bridge/DecodiumBridgeDecodeFilterSettings.cpp').read_text();start='template <typename SettingReader>\nstatic QStringList decodeFilterTerms(';b=new[new.index(start):].strip().replace('decodium::beginActiveSettingsProfile(profileSettings)', 'beginConfiguredBridgeSettingsGroup(profileSettings)').replace('decodiumBridgeAliasedSettingKey(key)', 'aliasedBridgeSettingKey(key)')
if a!=b:raise RuntimeError('filter settings source differs from pre-extraction snapshot')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-filtersettings-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_filtersettings_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_filtersettings.inc').write_text(code)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or '0|1|0|K1ABC||NA,EU' not in out[0] or '2|1|1|9H1SR,K1ABC|IU8LMC|NA,EU' not in out[0] or '3|0|0|||' not in out[0]:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
