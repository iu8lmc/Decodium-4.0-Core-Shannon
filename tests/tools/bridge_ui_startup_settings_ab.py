#!/usr/bin/env python3
"""Compare startup UI settings and style migration in temporary stores."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
fixture=(root/'tests/bridge_refactor_fixtures/ui_startup_settings_before.inc').read_text()
module=(root/'src/bridge/DecodiumBridgeUiStartupSettings.cpp').read_text().replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(')
assert module.endswith('void DecodiumBridge::initialiseUiStartupSettings()\n{\n'+fixture+'}\n')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QCoreApplication>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
void bridgeLog(const QString&){}
class DecodiumBridge{public:QString m_uiQuality,m_uiStyle;bool m_uiFramelessPopouts=false,m_autoDetachFullSpectrum=false;int m_spectrumFpsCap=0;void initialiseUiStartupSettings();};
void DecodiumBridge::initialiseUiStartupSettings(){
#include "ui.inc"
}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir tmp;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,tmp.path());QCoreApplication::setOrganizationName("BridgeLab");QCoreApplication::setApplicationName("BridgeLab");DecodiumBridge a;a.initialiseUiStartupSettings();bool defaults=a.m_uiQuality=="Medium"&&!a.m_uiFramelessPopouts&&a.m_uiStyle=="Material"&&a.m_autoDetachFullSpectrum&&a.m_spectrumFpsCap==20;QSettings normal;normal.setValue("UI/Quality","High");normal.setValue("UI/FramelessPopouts",true);normal.setValue("UI/AutoDetachFullSpectrum",false);normal.setValue("UI/SpectrumFpsCap",14);normal.sync();QSettings style("BridgeLab","Decodium");style.setValue("UI/Style","Default");style.sync();DecodiumBridge b;b.initialiseUiStartupSettings();style.sync();bool migrate=b.m_uiQuality=="High"&&b.m_uiFramelessPopouts&&!b.m_autoDetachFullSpectrum&&b.m_spectrumFpsCap==15&&b.m_uiStyle=="Material"&&style.value("UI/Style")=="Material";normal.setValue("UI/SpectrumFpsCap",50);normal.sync();style.setValue("UI/Style","Fusion");style.sync();DecodiumBridge c;c.initialiseUiStartupSettings();bool supported=c.m_uiStyle=="Fusion"&&c.m_spectrumFpsCap==30;style.setValue("UI/Style","unknown");style.sync();DecodiumBridge d;d.initialiseUiStartupSettings();style.sync();bool fallback=d.m_uiStyle=="Material"&&style.value("UI/Style")=="Material";bool ok=defaults&&migrate&&supported&&fallback;QTextStream(stdout)<<"default="<<defaults<<" migrate="<<migrate<<" supported="<<supported<<" fallback="<<fallback<<Qt::endl;return ok?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-ui-start-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outs=[]
 for label,body in [('before',fixture),('after',module.split('void DecodiumBridge::initialiseUiStartupSettings()\n{\n',1)[1][:-2])]:
  (d/'ui.inc').write_text(body);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outs.append(result.stdout)
 assert outs[0]==outs[1]
