#!/usr/bin/env python3
"""Compare station frequency CRUD using real Qt models and temporary QSettings.

Requires the local wsjt_qt static library and Boost.Log. BOOST_LIB_DIR and
CXX may be supplied for the local toolchain; no user settings are touched.
"""
import os, shlex, subprocess, sys, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def run(*args,**kwargs):return subprocess.run(args,cwd=ROOT,check=True,text=True,**kwargs)
baseline=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',stdout=subprocess.PIPE).stdout
current=(ROOT/'src/bridge/DecodiumBridgeStationFrequencies.cpp').read_text()
def part(s,a,b=None):
 i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
parser=part(baseline,'Radio::FrequencyDelta parseFrequencyOffsetHzValue(', 'void enforcePreferredWorkingFrequency(')
old=part(baseline,'QVariantList DecodiumBridge::stationFrequencyRows()', 'double DecodiumBridge::frequencyCalibrationSlopePpm()')
new=part(current,'QVariantList DecodiumBridge::stationFrequencyRows()').replace('decodiumBridgeLogbookLog(', 'bridgeLog(').replace('decodiumBridgeParseFrequencyOffsetHzValue(', 'parseFrequencyOffsetHzValue(')
if old!=new:raise SystemExit('source mismatch')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Widgets','Qt6Network','Qt6Multimedia','Qt6Sql','Qt6Xml'],text=True))
boost_dir=os.environ.get('BOOST_LIB_DIR','/opt/homebrew/lib' if sys.platform=='darwin' else '/usr/lib')
compiler=shlex.split(os.environ.get('CXX','c++'));results=[]
with tempfile.TemporaryDirectory(prefix='decodium-station-real-') as tmp:
 d=Path(tmp);(d/'bridge_refactor_stationfreq_parser.inc').write_text(parser)
 for label,body in (('A',old),('B',new)):
  (d/'bridge_refactor_stationfreq.inc').write_text(body);exe=d/('station-'+label+('.exe' if os.name=='nt' else ''))
  run(*compiler,'-std=c++17','-DBOOST_LOG_DYN_LINK','-I'+str(d),'-I.','-Isrc/radio','-Isrc/core','-Ibuild','tests/bridge_refactor_stationfreq_real_harness.cpp','src/core/Logger.cpp','-Lbuild','-lwsjt_qt','-L'+boost_dir,'-lboost_log_setup','-lboost_log','-lboost_thread','-lboost_filesystem','-lboost_atomic','-lboost_chrono',*flags,'-o',str(exe))
  results.append(run(str(exe),stdout=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise SystemExit('runtime mismatch')
 print('station real model and QSettings: A/B OK\n'+results[1],end='')
