#!/usr/bin/env python3
"""A/B frequency management using real frequency/band models and temporary files."""
import os,shlex,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def run(*args,**kwargs):return subprocess.run(args,cwd=ROOT,check=True,text=True,**kwargs)
def section(s,a,b=None):
 i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
baseline=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',stdout=subprocess.PIPE).stdout
current=(ROOT/'src/bridge/DecodiumBridgeWorkingFrequencies.cpp').read_text()
a='namespace\n{\nusing WorkingFrequencyItem';m='QVariantList DecodiumBridge::workingFrequencyRows()';old_helpers=section(baseline,a,m);new_helpers=section(current,a,'Radio::FrequencyDelta decodiumBridgeParseFrequencyOffsetHzValue(')
old_methods=section(baseline,m,'QVariantList DecodiumBridge::stationFrequencyRows()');new_methods=section(current,m).replace('decodiumBridgeLogbookLog(', 'bridgeLog(').replace('decodiumBridgeCanonicalApplicationDecodeMode(', 'canonicalApplicationDecodeMode(').replace('decodiumBridgeIsRadioOnlyModeLabel(', 'isRadioOnlyModeLabel(')
if old_helpers!=new_helpers or old_methods!=new_methods:raise SystemExit('working frequency source mismatch')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Widgets','Qt6Network','Qt6Multimedia','Qt6Sql','Qt6Xml'],text=True));boost_dir=os.environ.get('BOOST_LIB_DIR','/opt/homebrew/lib' if sys.platform=='darwin' else '/usr/lib');compiler=shlex.split(os.environ.get('CXX','c++'));results=[]
with tempfile.TemporaryDirectory(prefix='decodium-workingfreq-ab-') as tmp:
 d=Path(tmp)
 for label,helpers,methods in (('A',old_helpers,old_methods),('B',new_helpers,new_methods)):
  (d/'bridge_refactor_workingfreq_helpers.inc').write_text(helpers);(d/'bridge_refactor_workingfreq_methods.inc').write_text(methods);exe=d/('workingfreq-'+label+('.exe' if os.name=='nt' else ''))
  run(*compiler,'-std=c++17','-DBOOST_LOG_DYN_LINK','-I'+str(d),'-I.','-Isrc/radio','-Isrc/core','-Ibuild','tests/bridge_refactor_workingfreq_harness.cpp','src/core/Logger.cpp','-Lbuild','-lwsjt_qt','-L'+boost_dir,'-lboost_log_setup','-lboost_log','-lboost_thread','-lboost_filesystem','-lboost_atomic','-lboost_chrono',*flags,'-o',str(exe))
  results.append(run(str(exe),stdout=subprocess.PIPE,stderr=subprocess.PIPE).stdout)
 if results[0]!=results[1]:raise SystemExit('working frequency A/B output mismatch')
 print('working frequency real models and files: A/B OK\n'+results[1],end='')
