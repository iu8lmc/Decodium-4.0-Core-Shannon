#!/usr/bin/env python3
"""Compare OpenMP scheduler and Windows priority logic with synthetic OS calls."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2];old=(root/'tests/bridge_refactor_fixtures/ft_scheduler_before.inc').read_text();new=(root/'src/bridge/DecodiumBridgeFtScheduler.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QString>
#include <QByteArray>
#include <QThread>
#include <QDebug>
#include <QTextStream>
#include <QtGlobal>
#define Q_OS_WIN 1
#define _OPENMP 1
using HANDLE=void*;using DWORD=unsigned long;
constexpr DWORD IDLE_PRIORITY_CLASS=1, BELOW_NORMAL_PRIORITY_CLASS=2,NORMAL_PRIORITY_CLASS=3,ABOVE_NORMAL_PRIORITY_CLASS=4,ERROR_SUCCESS=0;
constexpr int THREAD_PRIORITY_ERROR_RETURN=-999,THREAD_PRIORITY_ABOVE_NORMAL=1;
static int dyn=-1,levels=-1,threads=-1,processSet=0,threadSet=0;static DWORD priority=NORMAL_PRIORITY_CLASS;
void omp_set_dynamic(int v){dyn=v;} void omp_set_max_active_levels(int v){levels=v;} void omp_set_num_threads(int v){threads=v;}
HANDLE GetCurrentProcess(){return nullptr;}HANDLE GetCurrentThread(){return nullptr;}DWORD GetPriorityClass(HANDLE){return priority;}bool SetPriorityClass(HANDLE,DWORD){++processSet;return true;}int GetThreadPriority(HANDLE){return 0;}bool SetThreadPriority(HANDLE,int){++threadSet;return true;}DWORD GetLastError(){return 0;}
int decodiumBridgeFtAutoUiReserveForCores(int c){return c>1?1:0;}
namespace {
constexpr int kMaxFtDecodeThreads{24};
#include "helpers.inc"
int main(int argc,char**argv){QString mode=argc>1?QString::fromLocal8Bit(argv[1]):"default";for(const char* x:{"OMP_WAIT_POLICY","GOMP_SPINCOUNT","OMP_DYNAMIC","GOMP_CPU_AFFINITY","DECODIUM_ENABLE_FT_OPENMP_AFFINITY","DECODIUM_DISABLE_WINDOWS_INTERACTIVE_SCHEDULING"})qunsetenv(x);if(mode=="disabled")qputenv("DECODIUM_DISABLE_WINDOWS_INTERACTIVE_SCHEDULING","1");if(mode=="preserve"){qputenv("OMP_WAIT_POLICY","ACTIVE");priority=99;}applyFtOpenMpThreadLimit(99);bool limits=dyn==0&&levels==1&&threads==24;bool affinity=ftOpenMpAffinityForCores(4)=="1-3";applyFtOpenMpSchedulerProfile();bool env=qgetenv("OMP_DYNAMIC")=="FALSE"&&qgetenv("OMP_WAIT_POLICY")== (mode=="preserve"?"ACTIVE":"PASSIVE");applyWindowsInteractiveSchedulingProfile();bool prio=mode=="disabled"?processSet==0&&threadSet==0:mode=="preserve"?processSet==0&&threadSet==1:processSet==1&&threadSet==1;QTextStream(stdout)<<mode<<" limits="<<limits<<" affinity="<<affinity<<" env="<<env<<" priority="<<prio<<Qt::endl;return limits&&affinity&&env&&prio?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-ft-scheduler-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);out=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label;subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True);rows=[]
  for mode in ('default','preserve','disabled'):
   v=subprocess.run([str(exe),mode],capture_output=True,text=True,check=True);print(label,v.stdout.strip(),flush=True);rows.append(v.stdout)
  out.append(rows)
 assert out[0]==out[1]
