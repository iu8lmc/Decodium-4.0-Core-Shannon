#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
s=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
a=sec(s,'static QString cleanHashResolveMessage(', 'static QString inferPartnerFromDirectedMessage(');b=sec((R/'src/bridge/LegacyHashResolve.cpp').read_text(),'static QString cleanHashResolveMessage(')
for old,new in [('static bool parseHashResolveAllTxtLine(','bool parseHashResolveAllTxtLine('),('static QStringList hashResolveLogCandidatePaths(','QStringList hashResolveLogCandidatePaths('),('static QString resolveHashMessageFromCandidates(','QString resolveHashMessageFromCandidates('),('normalizeUtcDisplayToken(','decodiumBridgeNormalizeRemoteActivityTime('),('secondsFromUtcDisplayToken(','decodiumBridgeSecondsFromUtcDisplayToken('),('signedUtcSecondDelta(','decodiumBridgeSignedUtcSecondDelta('),('isPlausibleDecodedCallsign(','isPlausibleDecodedCallsignToken(')]:a=a.replace(old,new)
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-legacyhash-ab-') as tmp:
 d=Path(tmp);h=(R/'tests/bridge_refactor_legacyhash_harness.cpp').read_text();prefix='using decodium::decode_message::canonicalDecodeMessage;using decodium::decode_message::stripDecodeApAnnotation;using decodium::seq::normalizeCallToken;using decodium::seq::isPlaceholderCallToken;using decodium::seq::isPlausibleDecodedCallsignToken;\n';out=[]
 for code in (a,b):
  (d/'test.cpp').write_text(h);(d/'bridge_refactor_under_test.inc').write_text(prefix+code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-DINCLUDE_EXTRACT','-I'+str(R/'src/bridge'),str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 (d/'test.cpp').write_text(h);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),'-I'+str(R),str(d/'test.cpp'),str(R/'src/bridge/LegacyHashResolve.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 real=(R/'src/bridge/DecodeUtcTime.cpp').read_text();times=sec(real,'static QString normalizeUtcDisplayToken(');(d/'times.cpp').write_text('#include <QtCore>\n#include <utility>\n'+times)
 (d/'test.cpp').write_text((R/'tests/bridge_refactor_legacyhash_real_harness.cpp').read_text());run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),'-I'+str(R),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),str(d/'test.cpp'),str(d/'times.cpp'),str(R/'src/bridge/LegacyHashResolve.cpp'),str(R/'src/bridge/DecodeMessageNormalization.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if len(set(out))!=1 or out[0].splitlines()!=['parse|1|0|120000|1500|K1ABC K2XYZ RR73','match|K1ABC K2XYZ RR73|fixture','ambiguous|<...> K2XYZ RR73|1','outside|<...> K2XYZ RR73','paths|1|1']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
