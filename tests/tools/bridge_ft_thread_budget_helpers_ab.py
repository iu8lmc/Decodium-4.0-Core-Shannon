#!/usr/bin/env python3
"""Compare automatic thread reserve and adaptive CPU budgets."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2];old=(root/'tests/bridge_refactor_fixtures/ft_thread_budget_helpers_before.inc').read_text();new=(root/'src/bridge/DecodiumBridgeFtThreadBudget.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QThread>
#include <QTextStream>
#include <QString>
#include "src/core/FtDecodeThreadBudget.hpp"
namespace {
constexpr int kMaxFtDecodeThreads {24};
#include "helpers.inc"
int main(){qunsetenv("DECODIUM_FT_AUTO_UI_RESERVE");bool core=ftAutoUiReserveForCores(1)==0&&ftAutoUiReserveForCores(4)==1&&ftAutoUiReserveForCores(8)==2&&ftAutoUiReserveForCores(16)==3&&autoFtThreadCountForCores(99)==24;qputenv("DECODIUM_FT_AUTO_UI_RESERVE","2");bool env=ftAutoUiReserveForCores(8)==2&&autoFtThreadCountForCores(8)==6;qputenv("DECODIUM_FT_AUTO_UI_RESERVE","99");bool bound=ftAutoUiReserveForCores(4)==3&&autoFtThreadCountForCores(4)==1;qunsetenv("DECODIUM_FT_AUTO_UI_RESERVE");bool adapt=adaptiveInteractiveFtThreadCount(8)>=1&&adaptiveInteractiveFtThreadCount(8)<=8;QTextStream(stdout)<<"cores="<<core<<" env="<<env<<" bound="<<bound<<" adaptive="<<adapt<<Qt::endl;return core&&env&&bound&&adapt?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-thread-budget-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);out=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label;subprocess.run(['c++','-std=c++17','-I'+str(d),'-I'+str(root),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True);v=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,v.stdout.strip(),flush=True);out.append(v.stdout)
 assert out[0]==out[1]
