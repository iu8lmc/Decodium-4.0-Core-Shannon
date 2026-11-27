#!/usr/bin/env python3
"""Compare all diagnostic forwarding functions, journal and FT2 flush."""
from pathlib import Path
import re, shlex, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/bridge_logging_before.inc').read_text()
new=(root/'src/bridge/DecodiumBridgeLogRouting.cpp').read_text()
assert old in new
names=re.findall(r'void (decodiumBridge\w+Log)\(const QString& message\)',old)
assert len(names)>50 and len(names)==len(set(names))
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
prefix='''#include <QString>\n#include <QDebug>\n#include <QTextStream>\n#include <QObject>\nstatic int diag=0, journal=0, flush=0;\n#define DIAG_INFO(x) do { (void)(x); ++diag; } while(0)\nnamespace DecodiumLogging { void requestDiagnosticFlush(){++flush;} }\nbool decodiumBridgeQsoJournalWanted(const QString& s){return s.contains("QSO");}\nvoid decodiumBridgeQsoJournal(const QString&){++journal;}\n'''
calls='\n'.join(n+'(QStringLiteral("QSO Ft2Link"));' for n in names)
main='''\nint main(){\n'''+calls+'''\n decodiumBridgeMainThreadTimelineLog(QStringLiteral("test"), 12, QStringLiteral("detail"));\n QTextStream(stdout)<<diag<<" "<<journal<<" "<<flush<<Qt::endl;\n return diag=='''+str(len(names)+1)+''' && journal=='''+str(len(names))+''' && flush=='''+str(len(names))+'''?0:1;\n}\n'''
with tempfile.TemporaryDirectory(prefix='decodium-log-routing-ab-') as tmp:
 d=Path(tmp); outputs=[]
 for label,body in [('before',old),('after',new[new.index('static void bridgeLog('):new.index('\nvoid decodiumBridgeTxPrepareTimelineLog(')])]:
  # The fixture includes this internal forwarding helper, called here too.
  code=prefix+body+main.replace('decodiumBridgeMainThreadTimelineLog(QStringLiteral("test"), 12, QStringLiteral("detail"));','txTimelineLog(QStringLiteral("ordinary")); decodiumBridgeMainThreadTimelineLog(QStringLiteral("test"), 12, QStringLiteral("detail"));').replace('diag=='+str(len(names)+1),'diag=='+str(len(names)+2))
  source=d/(label+'.cpp');source.write_text(code);exe=d/label
  subprocess.run(['c++','-std=c++17',str(source),*flags,'-o',str(exe)],check=True)
  p=subprocess.run([str(exe)],capture_output=True,text=True,check=True);outputs.append(p.stdout);print(label,'wrappers=',len(names),'diag/journal/flush=',p.stdout.strip(),flush=True)
 assert outputs[0]==outputs[1]
