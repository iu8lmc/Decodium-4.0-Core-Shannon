#!/usr/bin/env python3
"""Compare CAT/PTT dispatch with synthetic managers and no hardware."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2];old=(root/'tests/bridge_refactor_fixtures/cat_dispatch_before.inc').read_text();module=(root/'src/bridge/DecodiumBridgeCatDispatch.cpp').read_text();assert old in module
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
moc=Path('/opt/homebrew/share/qt/libexec/moc')
harness=r'''
#include <QObject>
#include <QString>
#include <QDebug>
#include <QTextStream>
class RemoteRadioLink{public:bool linked=true;int ptt=0,tuning=0;bool isLinked()const{return linked;}void setPtt(bool,int){++ptt;}void tune(double){++tuning;}};
class DecodiumCat4OmManager:public QObject{Q_OBJECT public:using QObject::QObject;QString method="CAT";bool capable=true;int ptt=0,txptt=0,freq=0,txfreq=0;QString pttMethod()const{return method;}bool canPtt()const{return capable;}void setRigPtt(bool){++ptt;}void setRigTxFrequencyAndPttAsync(double,bool){++txptt;}void setRigFrequency(double){++freq;}void setRigTxFrequency(double){++txfreq;}};
class DecodiumOmniRigManager{public:QString method="CAT";int ptt=0,freq=0,txfreq=0;QString pttMethod()const{return method;}bool canPtt()const{return true;}void setRigPtt(bool){++ptt;}void setRigFrequency(double){++freq;}void setRigTxFrequency(double){++txfreq;}};
class DecodiumTransceiverManager{public:QString method="CAT";int ptt=0,sync=0,async=0,freq=0,txfreq=0;QString pttMethod()const{return method;}bool canPtt()const{return true;}void setRigPtt(bool){++ptt;}void setRigTxFrequencyAndPtt(double,bool){++sync;}void setRigTxFrequencyAndPttAsync(double,bool){++async;}void setRigFrequency(double){++freq;}void setRigTxFrequency(double){++txfreq;}};
class DecodiumLegacyBackend{public:void setRigPtt(bool){}void setDialFrequency(double){}};
bool decodiumBridgeLegacyRigControlFallback(DecodiumLegacyBackend*,const QString&){return false;}
bool decodiumBridgeIsHamlibFamilyBackend(const QString& b){return b.trimmed().toLower()=="hamlib"||b.trimmed().toLower()=="tci";}
#include "helpers.inc"
int main(int argc,char**argv){Q_UNUSED(argc) Q_UNUSED(argv) QObject owner;DecodiumCat4OmManager cat(&owner);DecodiumOmniRigManager omni;DecodiumTransceiverManager h;RemoteRadioLink remote;g_decoPortCatLink=&remote;
 bool remoteOk=activeCatCanPtt(&owner,&h,"decoport")&&!activeCatUsesVoxPtt(&owner,&h,"decoport");activeCatSetPtt(&owner,&h,"decoport",true);activeCatSetFreq(&owner,&h,"decoport",14074000);activeCatSetTxFreq(&owner,&h,"decoport",14075000);activeCatSetFreq(&owner,&h,"decoport",0);bool remoteAsync=activeCatSetTxPttAsync(&owner,&h,"decoport",true,14075000);remoteOk=remoteOk&&remote.ptt==2&&remote.tuning==1&&!remoteAsync;g_decoPortCatLink=nullptr;remoteOk=remoteOk&&!activeCatCanPtt(&owner,&h,"decoport");
 h.method="VOX";bool vox=!activeCatCanPtt(&owner,&h,"hamlib")&&activeCatUsesVoxPtt(&owner,&h,"hamlib");h.method="CAT";bool ham=activeCatCanPtt(&owner,&h,"hamlib");activeCatSetTxPtt(&owner,&h,"hamlib",true,14075000);bool async=activeCatSetTxPttAsync(&owner,&h,"hamlib",true,14075000);activeCatSetFreq(&owner,&h,"hamlib",14074000);activeCatSetTxFreq(&owner,&h,"hamlib",14075000);activeCatSetTxPtt(&owner,&h,"hamlib",false,14075000);bool offAsync=activeCatSetTxPttAsync(&owner,&h,"hamlib",false,14075000);ham=ham&&async&&!offAsync&&h.sync==1&&h.async==1&&h.ptt==1&&h.freq==1&&h.txfreq==1;
 DecodiumCat4OmManager explicitCat;explicitCat.capable=false;bool explicitChoice=activeCatCanPtt(&owner,&h,"cat4om")&&!decodiumBridgeSharedCatCanPttExplicit(&owner,&h,"cat4om",nullptr,nullptr,&explicitCat);bool c=activeCatCanPtt(&owner,&h,"cat4om");activeCatSetPtt(&owner,&h,"cat4om",true);activeCatSetTxPtt(&owner,&h,"cat4om",true,14075000);activeCatSetFreq(&owner,&h,"cat4om",14074000);activeCatSetTxFreq(&owner,&h,"cat4om",14075000);bool catAsync=activeCatSetTxPttAsync(&owner,&h,"cat4om",false,14075000);cat.method=" VOX ";bool catVox=activeCatUsesVoxPtt(&owner,&h,"cat4om");c=c&&catAsync&&catVox&&cat.ptt==1&&cat.txptt==2&&cat.freq==1&&cat.txfreq==1;
 omni.method="VOX";bool o=!activeCatCanPtt(&owner,&h,"omnirig",&omni)&&activeCatUsesVoxPtt(&owner,&h,"omnirig",&omni);omni.method="CAT";activeCatSetPtt(&owner,&h,"omnirig",true,&omni);activeCatSetFreq(&owner,&h,"omnirig",14074000,&omni);o=o&&omni.ptt==1&&omni.freq==1;QTextStream(stdout)<<"remote="<<remoteOk<<" vox="<<vox<<" ham="<<ham<<" cat4om="<<c<<" omni="<<o<<" explicit="<<explicitChoice<<Qt::endl;return remoteOk&&vox&&ham&&c&&o&&explicitChoice?0:1;}
#include "main.moc"
'''
with tempfile.TemporaryDirectory(prefix='decodium-cat-dispatch-ab-') as tmp:
 d=Path(tmp);src=d/'main.cpp';src.write_text(harness);subprocess.run([str(moc),str(src),'-o',str(d/'main.moc')],check=True);out=[]
 for label,body in [('before',old+'\nbool decodiumBridgeSharedCatCanPttExplicit(QObject const* o,DecodiumTransceiverManager* h,const QString& b,DecodiumOmniRigManager* m,DecodiumLegacyBackend* l,DecodiumCat4OmManager* c){return activeCatCanPtt(o,h,b,m,l,c);}\n'),('after',module[module.index('static RemoteRadioLink* g_decoPortCatLink'):module.index('// CAT_DISPATCH_FUNCTIONS_END')])]:
  (d/'helpers.inc').write_text(body);exe=d/label;subprocess.run(['c++','-std=c++17','-I'+str(d),str(src),*flags,'-o',str(exe)],check=True);v=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,v.stdout.strip(),flush=True);out.append(v.stdout)
 assert out[0]==out[1]
