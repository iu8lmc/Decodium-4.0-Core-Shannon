#!/usr/bin/env python3
"""Compare amplifier discovery, port conflict and configuration behaviour."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/amplifierconfig_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeAmplifierConfig.cpp').read_text();mark='QVariantMap DecodiumBridge::cercaAmplificatore()';new=(mark+s.split(mark,1)[1]).replace('decodiumBridgeAmplifierLog(', 'bridgeLog(').replace('decodiumBridgeAmplifierPortClashes(', 'amplifierPortClashesWithCat(');assert old==new
source=r'''
#include <QObject>
#include <QDebug>
#include <QVariantMap>
#include <QStringList>
#include <iostream>
void bridgeLog(const QString&){}
struct DecodiumTransceiverManager {QString serialPort(){return " /dev/cat ";}QString pttPort(){return "/dev/ptt";}};
bool amplifierPortClashesWithCat(DecodiumTransceiverManager*c,const QString&p){if(!c||p.trimmed().isEmpty())return false;auto w=p.trimmed().toUpper();return w==c->serialPort().trimmed().toUpper()||w==c->pttPort().trimmed().toUpper();}
struct DecodiumAmplifier{struct Found{QString porta,modello;int baud;QString descrizione;};static QList<Found> found;static QStringList exclusions;static QList<Found> cerca(const QStringList&l){exclusions=l;return found;}bool en=false,pass=false;int baud=0,poll=0;QString port,mfr;void configure(bool e,const QString&p,int b,bool v,int t){en=e;port=p;baud=b;pass=v;poll=t;}void setManufacturer(const QString&s){mfr=s;}};QList<DecodiumAmplifier::Found> DecodiumAmplifier::found;QStringList DecodiumAmplifier::exclusions;
class DecodiumBridge{public:DecodiumTransceiverManager*m_hamlibCat=nullptr;DecodiumAmplifier*m_amplifier=nullptr;QVariantMap settings;void setSetting(const QString&k,const QVariant&v){settings[k]=v;}QVariantMap cercaAmplificatore();void configureAmplifier(bool,const QString&,int,bool,int);void configureAmplifierManufacturer(const QString&);};
#include "methods.inc"
int main(){DecodiumBridge b;DecodiumTransceiverManager c;DecodiumAmplifier amp;b.m_hamlibCat=&c;b.m_amplifier=&amp;auto none=b.cercaAmplificatore();std::cout<<none["trovato"].toBool()<<':'<<DecodiumAmplifier::exclusions.join(',').toStdString()<<'\n';b.configureAmplifier(true," /DEV/CAT ",9600,false,500);std::cout<<amp.en<<':'<<b.settings["AmpEnabled"].toBool()<<'\n';b.configureAmplifier(true,"/DEV/PTT",9600,false,500);std::cout<<amp.en<<'\n';DecodiumAmplifier::found={{"/dev/amp","SPE",19200,"unit"}};auto found=b.cercaAmplificatore();std::cout<<found["trovato"].toBool()<<':'<<found["porta"].toString().toStdString()<<':'<<amp.en<<':'<<amp.baud<<':'<<amp.poll<<'\n';b.configureAmplifierManufacturer("  ");std::cout<<amp.mfr.toStdString()<<'\n';b.configureAmplifierManufacturer("  ACOM  ");std::cout<<amp.mfr.toStdString()<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-ampconfig-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True,stderr=subprocess.DEVNULL))
assert outputs[0]==outputs[1]=='0:/dev/cat,/dev/ptt\n0:0\n0\n1:/dev/amp:1:19200:500\nSPE\nACOM\n',outputs
print('Amplifier A/B: discovery, CAT/PTT conflict, configuration and manufacturer passed')
