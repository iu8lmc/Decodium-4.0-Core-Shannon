#!/usr/bin/env python3
"""Compare and time actual legacy decode normalization with a source snapshot."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--qt-root',type=Path,default=Path(os.environ.get('DECODIUM_QT_ROOT','/home/the-matrix/.local/opt/decodium-qt/6.11.0/gcc_64')))
a=p.parse_args()
def extract(source,name):
    start=source.index('QString stripLegacyDecodeAppendage(')
    end=source.index('\nQString stripDecodeApAnnotation(',start)
    return source[start:end].replace('stripLegacyDecodeAppendage',name,1)
old=extract(a.baseline.read_text(),'oldNormalize')
new=extract((root/'src/bridge/DecodeMessageNormalization.cpp').read_text(),'newNormalize')
program=r'''
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>
OLD
NEW
int main(){
 QStringList inputs={"1234 -10 0.2 1500 ~ CQ K1ABC FN42  a2", "123456 +12 -0.3 4000 # W1AW K1ABC RR73  test", "TU; 1234 -2 1.0 50 ~ CQ K1ABC", "1234 -1 0.1 500 @ CQ\\t TEST", "CQ K1ABC FN42", "", "１２３４ -1 0 999 ~ W1AW", "  foo  bar  ",QString::fromUtf8("1234 -7 0.2 1500 ~ CQ K1ABC FN42 garbage")};
 std::mt19937 rng(417);for(int n=0;n<5000;++n){QString s=QString::number(rng()%999999)+" -"+QString::number(rng()%30)+" 0.2 "+QString::number(rng()%5000)+" ~ CQ "+QString::number(rng())+(n%3?"  a2":"");inputs.append(s);}
 for(auto const& in:inputs){auto x=oldNormalize(in),y=newNormalize(in);if(x!=y){std::cerr<<"mismatch\n";return 1;}}
 auto run=[&](bool current){int total=0;auto t=std::chrono::steady_clock::now();for(int n=0;n<100000;++n)total+=(current?newNormalize(inputs[n%inputs.size()]):oldNormalize(inputs[n%inputs.size()])).size();auto stop=std::chrono::steady_clock::now();return std::pair<double,int>{std::chrono::duration<double,std::milli>(stop-t).count(),total};};
 for(int i=0;i<4;++i){bool first=i%2;auto x=run(first),y=run(!first);if(x.second!=y.second)return 2;std::cout<<"order="<<(first?"new-old":"old-new")<<" first_ms="<<x.first<<" second_ms="<<y.first<<"\n";}
 std::cout<<"identical="<<inputs.size()<<"\n";
}
'''.replace('OLD',old).replace('NEW',new)
qt=a.qt_root;env=dict(os.environ,PKG_CONFIG_PATH=str(qt/'lib/pkgconfig')+os.pathsep+os.environ.get('PKG_CONFIG_PATH',''))
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],env=env,text=True))
with tempfile.TemporaryDirectory(prefix='decode-normalize-ab-') as tmp:
 d=Path(tmp);cpp=d/'test.cpp';exe=d/'test';cpp.write_text(program)
 subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-Os','-std=c++17',str(cpp),'-o',str(exe),*flags,'-Wl,-rpath,'+str(qt/'lib')],check=True)
 subprocess.run([str(exe)],check=True)
