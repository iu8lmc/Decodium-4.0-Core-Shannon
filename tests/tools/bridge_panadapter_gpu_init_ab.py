#!/usr/bin/env python3
"""Compare startup GPU policy under isolated environment/settings inputs."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
fixture=(root/'tests/bridge_refactor_fixtures/panadapter_gpu_init_before.inc').read_text()
module=(root/'src/bridge/DecodiumBridgePanadapterGpuInit.cpp').read_text()
assert module.endswith('void DecodiumBridge::initialisePanadapterGpuPolicy()\n{\n'+fixture+'}\n')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QCoreApplication>
#include <QDebug>
#include <QTextStream>
#include <QVariant>
#include <atomic>
class DecodiumBridge{public:std::atomic<bool> m_openGlGpuPanadapterFftEnabled{false},m_gpuPanadapterFftAvailable{true},m_forceGpuPanadapterFft{true};bool setting=false;QVariant getSetting(const QString&,bool){return setting;}void initialisePanadapterGpuPolicy();};
void DecodiumBridge::initialisePanadapterGpuPolicy(){
#include "init.inc"
}
void clearEnv(){qunsetenv("DECODIUM_ENABLE_EXPERIMENTAL_OPENGL_GPU_PANADAPTER_FFT");qunsetenv("DECODIUM_DISABLE_GPU_PANADAPTER_FFT");qunsetenv("DECODIUM_DISABLE_GPU_PANADAPTER_FORCE");}
int main(int argc,char**argv){QCoreApplication app(argc,argv);clearEnv();DecodiumBridge a;a.initialisePanadapterGpuPolicy();bool defaults=!a.m_openGlGpuPanadapterFftEnabled&&a.m_gpuPanadapterFftAvailable&&a.m_forceGpuPanadapterFft;DecodiumBridge b;b.setting=true;b.initialisePanadapterGpuPolicy();bool configured=b.m_openGlGpuPanadapterFftEnabled;qputenv("DECODIUM_DISABLE_GPU_PANADAPTER_FFT","1");qputenv("DECODIUM_ENABLE_EXPERIMENTAL_OPENGL_GPU_PANADAPTER_FFT","1");DecodiumBridge c;c.initialisePanadapterGpuPolicy();bool disabled=c.m_openGlGpuPanadapterFftEnabled&&!c.m_gpuPanadapterFftAvailable&&!c.m_forceGpuPanadapterFft;clearEnv();qputenv("DECODIUM_DISABLE_GPU_PANADAPTER_FORCE","1");DecodiumBridge d;d.initialisePanadapterGpuPolicy();bool unforced=d.m_gpuPanadapterFftAvailable&&!d.m_forceGpuPanadapterFft;clearEnv();bool ok=defaults&&configured&&disabled&&unforced;QTextStream(stdout)<<"default="<<defaults<<" configured="<<configured<<" disabled="<<disabled<<" unforced="<<unforced<<Qt::endl;return ok?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-gpu-init-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outs=[]
 for label,body in [('before',fixture),('after',module.split('void DecodiumBridge::initialisePanadapterGpuPolicy()\n{\n',1)[1][:-2])]:
  (d/'init.inc').write_text(body);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outs.append(result.stdout)
 assert outs[0]==outs[1]
