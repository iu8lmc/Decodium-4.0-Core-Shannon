#!/usr/bin/env python3
"""Compare QSO filters in the real app using an isolated synthetic cache."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
qso=root/'src/bridge/DecodiumBridgeQsoCache.cpp'
sstv=root/'src/bridge/DecodiumBridgeSstv.cpp'
main=root/'build-baseline/qml/decodium/Main.qml'
original_qso,original_sstv,original_main=qso.read_text(),sstv.read_text(),main.read_text()
baseline=subprocess.check_output(['git','show','HEAD:src/bridge/DecodiumBridgeQsoCache.cpp'],cwd=root,text=True)
old_block=baseline[baseline.index('    auto matchesDate = '):baseline.index('    if (timer.elapsed() > 250)')]
qso_anchor='    static const QRegularExpression nonDigits {R"([^0-9])"};\n'
sstv_anchor='QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
if any((original_qso.count(qso_anchor)!=1, original_sstv.count(sstv_anchor)!=1,
        original_main.count('GlassApplicationWindow {')!=1)):
    raise RuntimeError('unexpected source structure')
injection='''
#if DECODIUM_HAS_SSTV
 if (QStandardPaths::isTestModeEnabled() && values.value("action").toString()==QStringLiteral("lab-qso-search-ab")) {
   QString path=effectiveAdifLogPath(); QFileInfo info(path); QVariantList rows; rows.reserve(12000);
   for(int i=0;i<12000;++i) rows.append(QVariantMap{
      {"id",i},{"call",i%5==0?QStringLiteral("K1ABC"):QStringLiteral("W9XYZ")},
      {"grid",i%3==0?QStringLiteral("FN42"):QStringLiteral("JM75")},
      {"band",i%2==0?QStringLiteral("20m"):QStringLiteral("40m")},
      {"mode",i%3==0?QStringLiteral("FT8"):QStringLiteral("FT4")},
      {"comment",i%4==0?QStringLiteral("ALPHA"):QStringLiteral("BETA")},
      {"dateTime",i%2==0?QStringLiteral("2026-11-07 12:00"):QStringLiteral("2025-01-02 08:30")}});
   {QMutexLocker locker(&m_qsoSearchCacheMutex);m_qsoSearchCacheReady=true;
    m_qsoSearchCachePath=path;m_qsoSearchCacheModified=info.exists()?info.lastModified():QDateTime();
    m_qsoSearchCacheSize=info.exists()?info.size():0;m_qsoSearchCacheRows=rows;}
   QVariantList outcomes; QVariantList reference; bool same=true;
   for(int pass=0;pass<4;++pass) {
     bool old=pass==0||pass==3; qputenv("DECODIUM_LAB_QSO_OLD",old?"1":"0");
     QElapsedTimer clock;clock.start(); QVariantList combined;
     for(int repeat=0;repeat<3;++repeat) {
       combined.append(QVariant(searchQsos("K1ABC",QString(),QString(),QString(),QString())));
       combined.append(QVariant(searchQsos(QString(),QStringLiteral("20m"),QStringLiteral("FT8"),QString(),QString())));
       combined.append(QVariant(searchQsos(QStringLiteral("ALPHA"),QString(),QString(),QStringLiteral("2025-01-01"),QStringLiteral("2026-12-31"))));
     }
     qint64 elapsed=clock.nsecsElapsed();
     if(pass==0)reference=combined;else if(combined!=reference)same=false;
     outcomes.append(QVariantMap{{"old",old},{"elapsedMs",elapsed/1000000.0},
       {"callRows",combined[0].toList().size()},{"bandRows",combined[1].toList().size()},
       {"dateRows",combined[2].toList().size()}});
   }
   qunsetenv("DECODIUM_LAB_QSO_OLD");invalidateQsoSearchCache();
   return {{"same",same},{"outcomes",outcomes},{"cat",m_catConnected},{"tx",m_transmitting}};
 }
#endif
'''
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
   var result=bridge.logSstvQso({action:"lab-qso-search-ab"});
   console.log("[QSO-LIVE-AB] "+JSON.stringify(result));Qt.quit();
 }}
'''
try:
    qso.write_text(original_qso.replace(qso_anchor,
        '    if (qEnvironmentVariableIntValue("DECODIUM_LAB_QSO_OLD") == 1) {\n'
        +old_block+'        return results;\n    }\n'+qso_anchor,1))
    sstv.write_text(original_sstv.replace('#include "DecodiumBridge.h"',
        '#include "DecodiumBridge.h"\n#include <QElapsedTimer>\n#include <QFileInfo>\n#include <QMutexLocker>\n#include <QStandardPaths>',1)
        .replace(sstv_anchor,sstv_anchor+injection,1))
    with (root/'build-baseline/performance/qso-live-ab-build.log').open('w') as log:
        subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,
                       stdout=log,stderr=subprocess.STDOUT,check=True)
    main.write_text(original_main.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1))
    with tempfile.TemporaryDirectory(prefix='decodium-qso-live-ab-') as tmp:
        env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',
            DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name','QsoSearch'+secrets.token_hex(5),
             '--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','40000']
        app=subprocess.run(cmd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=55)
        rows=[json.loads(x.split('[QSO-LIVE-AB] ',1)[1]) for x in app.stdout.splitlines() if '[QSO-LIVE-AB] ' in x]
        if app.returncode or len(rows)!=1 or not rows[0]['same'] or rows[0]['cat'] or rows[0]['tx']:
            raise RuntimeError(app.stdout[-8000:])
        result=rows[0]
        if len(result['outcomes'])!=4 or not all(x['callRows']>0 and x['bandRows']>0 and x['dateRows']>0 for x in result['outcomes']):
            raise RuntimeError('invalid dataset: '+repr(result))
        (root/'build-baseline/performance/results/qso-search-live-ab.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,sort_keys=True))
finally:
    qso.write_text(original_qso);sstv.write_text(original_sstv);main.write_text(original_main)
    subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,
                   stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
