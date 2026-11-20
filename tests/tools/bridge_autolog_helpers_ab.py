#!/usr/bin/env python3
"""Compare autolog partner selection and duplicate-cache behaviour."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/autolog_helpers_before.inc').read_text()
new=(root/'src/bridge/DecodiumBridgeAutologHelpers.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QHash>
#include <QTextStream>
#include "MessageTokenRules.hpp"
#include "Radio.hpp"
using decodium::seq::normalizeCallToken;
using decodium::seq::isPlaceholderCallToken;
#include "helpers.inc"
int main(){bool calls=inferPartnerFromDirectedMessage("IU8LMC K1ABC -10","IU8LMC","IU8LMC")=="K1ABC"&&inferPartnerFromDirectedMessage("K1ABC IU8LMC RR73","IU8LMC","IU8LMC")=="K1ABC"&&inferPartnerFromDirectedMessage("CQ IU8LMC FN20","IU8LMC","IU8LMC").isEmpty(); QDateTime now=QDateTime::currentDateTimeUtc();bool windows=isWithinRecentDuplicateWindow(now.addSecs(-90),now)&&!isWithinRecentDuplicateWindow(now.addSecs(-91),now)&&isWithinRecentAutoCqPartnerWindow(now.addSecs(-600),now)&&!isWithinRecentAutoCqPartnerWindow(now.addSecs(-601),now)&&!isWithinRecentDuplicateWindow(now.addSecs(1),now);QHash<QString,QDateTime> cache;for(int i=0;i<513;i++)cache.insert(QString::number(i),now.addSecs(-1801));cache.insert("keep",now);pruneRecentDuplicateCache(cache,now);bool prune=cache.size()==1&&cache.contains("keep");QTextStream(stdout)<<"calls="<<calls<<" windows="<<windows<<" prune="<<prune<<Qt::endl;return calls&&windows&&prune?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-autolog-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outputs=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label
  files=[d/'main.cpp',root/'Sequencer/MessageTokenRules.cpp',root/'src/radio/Radio.cpp']
  cmd=['c++','-std=c++17','-I'+str(d),'-I'+str(root/'Sequencer'),'-I'+str(root/'src/radio'),'-I'+str(root/'src/core'),'-I'+str(root),*[str(f) for f in files],*flags,'-o',str(exe)]
  subprocess.run(cmd,check=True);result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outputs.append(result.stdout)
 assert outputs[0]==outputs[1]
