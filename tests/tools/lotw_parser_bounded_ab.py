#!/usr/bin/env python3
"""Compare the LoTW parser on valid and malformed synthetic input."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser()
parser.add_argument('--baseline', type=Path, help='source snapshot before bounded parsing')
parser.add_argument('--qt-root', type=Path, default=Path(os.environ.get('DECODIUM_QT_ROOT', '/home/the-matrix/.local/opt/decodium-qt/6.11.0/gcc_64')))
args=parser.parse_args()
old=args.baseline.read_text() if args.baseline else subprocess.check_output(['git','show','HEAD:src/bridge/DecodiumBridgeUsStateLotw.cpp'],cwd=root,text=True)
new=(root/'src/bridge/DecodiumBridgeUsStateLotw.cpp').read_text()
def extract(text, name):
    start=text.index('static LotwUsersLoadResult parseLotwUsersCsv(')
    end=text.index('\nstatic LotwUsersLoadResult loadLotwUsersFile(',start)
    return text[start:end].replace('parseLotwUsersCsv',name,1)
program=r'''
#include <QByteArray>
#include <QSet>
#include <QString>
#include <QtGlobal>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
struct LotwUsersLoadResult { QSet<QString> users; QString error; };
OLD
NEW
int main(){
 std::mt19937 rng(67121);
 for(int trial=0;trial<300;++trial){
  QByteArray data;int length=rng()%12000;for(int i=0;i<length;++i){int r=rng()%50;data.append(r<3?'\n':r<6?',':char(rng()%256));}
  if(trial%3==0)data.prepend("CALLSIGN,DATE\n  k1abc  ,today\n");
  auto a=oldParse(data),b=newParse(data);if(a.users!=b.users||a.error!=b.error){std::cerr<<"mismatch trial "<<trial<<"\n";return 1;}
 }
 QByteArray malformed; for(int i=0;i<90000;++i)malformed.append("NO-COMMA-ON-THIS-LINE\n");
 auto t1=std::chrono::steady_clock::now();auto a=oldParse(malformed);auto t2=std::chrono::steady_clock::now();auto b=newParse(malformed);auto t3=std::chrono::steady_clock::now();
 if(a.users!=b.users||a.error!=b.error||a.error.isEmpty())return 2;
 auto ms=[](auto x,auto y){return std::chrono::duration<double,std::milli>(y-x).count();};
 std::cout<<"300 mixed byte inputs equal; malformed_lines=90000 old_ms="<<ms(t1,t2)<<" new_ms="<<ms(t2,t3)<<"\n";
 QByteArray normal;for(int i=0;i<250000;++i)normal.append("K"+QByteArray::number(i)+",2026-01-01\n");
 for(int pass=0;pass<4;++pass){
  bool oldFirst=(pass%2==0); LotwUsersLoadResult x,y;
  t1=std::chrono::steady_clock::now();x=oldFirst?oldParse(normal):newParse(normal);t2=std::chrono::steady_clock::now();y=oldFirst?newParse(normal):oldParse(normal);t3=std::chrono::steady_clock::now();
  if(x.users!=y.users||x.error!=y.error||x.users.size()!=250000)return 3;
  std::cout<<"normal_rows="<<x.users.size()<<" order="<<(oldFirst?"old-new":"new-old")<<" first_ms="<<ms(t1,t2)<<" second_ms="<<ms(t2,t3)<<"\n";
 }
}
'''.replace('OLD',extract(old,'oldParse')).replace('NEW',extract(new,'newParse'))
qt=args.qt_root
env=dict(os.environ,PKG_CONFIG_PATH=str(qt/'lib/pkgconfig')+os.pathsep+os.environ.get('PKG_CONFIG_PATH',''))
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],env=env,text=True))
with tempfile.TemporaryDirectory(prefix='lotw-parser-ab-') as tmp:
 d=Path(tmp);cpp=d/'test.cpp';exe=d/'test';cpp.write_text(program)
 subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-O2','-std=c++17',str(cpp),'-o',str(exe),*flags,'-Wl,-rpath,'+str(qt/'lib')],check=True)
 subprocess.run([str(exe)],check=True)
