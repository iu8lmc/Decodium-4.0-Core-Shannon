#!/usr/bin/env python3
"""Compare diagnostic message formatting."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/diagnostic_helpers_before.inc').read_text()
new=(root/'src/bridge/DecodiumBridgeDiagnostics.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QString>
#include <QTextStream>
#include "helpers.inc"
int main(){bool line=bridgeDiagnosticOneLine(QStringLiteral(" one\r\ntwo "))==QStringLiteral("one\\r | two");bool warn=bridgeStatusLooksLikeWarning("NETWORK ERROR")&&bridgeStatusLooksLikeWarning("operazione fallita")&&bridgeStatusLooksLikeWarning("timeout")&&!bridgeStatusLooksLikeWarning("connected");QTextStream(stdout)<<"line="<<line<<" warn="<<warn<<Qt::endl;return line&&warn?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-tx-wave-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outputs=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outputs.append(result.stdout)
 assert outputs[0]==outputs[1]
