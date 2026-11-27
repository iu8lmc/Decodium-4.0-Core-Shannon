#!/usr/bin/env python3
import os, shlex, subprocess, tempfile
from pathlib import Path
R = Path(__file__).resolve().parents[2]
a = (R/'tests/bridge_refactor_fixtures/refresh_decode_dxcc_before.inc').read_text()
b = (R/'src/bridge/DecodiumBridgeDecodeRefresh.cpp').read_text().split('\n\n', 1)[1]
if a != b:
    raise RuntimeError('refresh body differs from pre-extraction snapshot')
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
harness = r'''
#include <QVariantMap>
#include <QVariantList>
#include <QString>
#include <iostream>
#define emit
class DecodiumBridge {
public:
 QVariantList m_decodeList, m_rxDecodeList;
 QString m_mode = "FT8";
 int normalizations=0, bandSignals=0, rxSignals=0, enrichments=0;
 void refreshDecodeListDxcc();
 void enrichDecodeEntry(QVariantMap &e) { ++enrichments; e["country"] = e.value("target"); }
 void emitDecodeListChangedThrottled(){++bandSignals;}
 void emitRxDecodeListChangedThrottled(bool immediate){if(immediate) abort(); ++rxSignals;}
};
static int normalized=0;
void decodiumBridgeNormalizeDecodeEntries(QVariantList&, int n, QString const& m) { if(n!=1500 || m!="FT8") abort(); ++normalized; }
#include "method.inc"
QVariant row(const char* target, const char* country, bool tx=false) { QVariantMap m; m["target"]=target; m["country"]=country; m["isTx"]=tx; return m; }
void check(bool x){if(!x) abort();}
int main(){
 DecodiumBridge b; b.m_decodeList={row("US","old"),row("TX","old",true)}; b.m_rxDecodeList={row("Italy","Italy")};
 b.refreshDecodeListDxcc(); check(b.m_decodeList[0].toMap()["country"]=="US" && b.m_decodeList[1].toMap()["country"]=="old");
 check(b.enrichments==2 && normalized==1 && b.bandSignals==1 && b.rxSignals==0);
 b.refreshDecodeListDxcc(); check(b.enrichments==4 && normalized==1 && b.bandSignals==1 && b.rxSignals==0);
 b.m_rxDecodeList={row("Malta","old"),row("US","old")}; b.refreshDecodeListDxcc();
 check(b.enrichments==7 && normalized==2 && b.bandSignals==1 && b.rxSignals==1);
 check(b.m_rxDecodeList[0].toMap()["country"]=="Malta" && b.m_rxDecodeList[1].toMap()["country"]=="US");
 b.m_decodeList.clear(); b.m_rxDecodeList.clear(); b.refreshDecodeListDxcc(); check(normalized==2 && b.enrichments==7);
 std::cout << "A/B refresh: changes, unchanged rows, TX, both lists, empty lists and signals passed\n";
}
'''
with tempfile.TemporaryDirectory(prefix='decodium-refresh-ab-') as t:
 d=Path(t); (d/'test.cpp').write_text(harness); outputs=[]
 for code in (a,b):
  (d/'method.inc').write_text(code)
  subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test')],check=True)
  outputs.append(subprocess.check_output([str(d/'test')],text=True))
 if outputs[0]!=outputs[1]: raise RuntimeError('A/B outputs differ')
 print(outputs[0],end='')
