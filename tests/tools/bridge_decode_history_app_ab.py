#!/usr/bin/env python3
"""Compare populated history query and ADIF export in isolated applications."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
module = root / 'src/bridge/DecodiumBridgeDecodeHistory.cpp'
sstv = root / 'src/bridge/DecodiumBridgeSstv.cpp'
qml = root / 'build/qml/decodium/Main.qml'
original_module, original_sstv, original_qml = (p.read_text() for p in (module, sstv, qml))
baseline = subprocess.check_output(['git', 'show', 'v1.0.737:src/bridge/DecodiumBridge.cpp'], cwd=root, text=True)
start = '// =====================================================================\n// === Phase 5.3'
end = 'QVariantMap DecodiumBridge::primaryScreenAvailableGeometry() const'
old = baseline[baseline.index(start):baseline.index(end, baseline.index(start))].replace('bridgeLog(', 'decodiumBridgeLogbookLog(')
prefix = original_module[:original_module.index(start)]
if original_module[original_module.index(start):] != old: raise RuntimeError('history source differs from baseline')
anchor = 'QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
injection = '''
#if DECODIUM_HAS_SSTV
 { Q_UNUSED(values); QSqlDatabase db=QSqlDatabase::database(); QString path=db.databaseName();
   if(!path.contains(QStringLiteral(".qttest")) || !path.contains(qEnvironmentVariable("DECODIUM_LAB_RIG"))) return {{"safe",false}};
   QSqlQuery q(db); bool a=q.exec(QStringLiteral("INSERT INTO sessions(started_utc,operator,station,decodium_ver) VALUES(1,'LAB','LAB','1')"));
   qint64 sid=q.lastInsertId().toLongLong(); q.prepare(QStringLiteral("INSERT INTO decodes(ts_utc,band,freq_hz,mode,callsign_dx,callsign_de,grid,snr_db,dt_s,df_hz,message,session_id) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)"));
   for (QVariant v : QVariantList{qint64(1760000000000LL),QStringLiteral("20M"),14074000,QStringLiteral("FT8"),QStringLiteral("K1ABC"),QStringLiteral("9H1SR"),QStringLiteral("FN42"),-12,0.2,1200,QStringLiteral("CQ K1ABC FN42"),sid}) q.addBindValue(v);
   bool b=q.exec(); q.clear(); q.prepare(QStringLiteral("INSERT INTO decodes(ts_utc,band,freq_hz,mode,callsign_dx,callsign_de,grid,snr_db,dt_s,df_hz,message,session_id) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)"));
   for (QVariant v : QVariantList{qint64(1760000001000LL),QStringLiteral("40M"),7074000,QStringLiteral("FT8"),QStringLiteral("W1XYZ"),QStringLiteral("9H1SR"),QStringLiteral("FN31"),-8,0.1,1300,QStringLiteral("CQ W1XYZ FN31"),sid}) q.addBindValue(v);
   bool c=q.exec(); return {{"safe",true},{"inserted",a&&b&&c},{"error",q.lastError().text()}}; }
#endif
'''
if original_sstv.count(anchor) != 1: raise RuntimeError('unexpected test insertion point')
marker = 'GlassApplicationWindow {'
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
rigs, results = [], []
try:
    sstv.write_text(original_sstv.replace('#include "DecodiumBridge.h"', '#include "DecodiumBridge.h"\n#include <QSqlDatabase>\n#include <QSqlQuery>\n#include <QSqlError>', 1).replace(anchor, anchor + injection, 1))
    for label in ('before', 'after'):
        module.write_text(prefix + old if label == 'before' else original_module)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        rig = 'HistoryAB' + secrets.token_hex(5); rigs.append(rig)
        with tempfile.TemporaryDirectory(prefix='decodium-history-app-') as tmp:
            output = Path(tmp) / 'history.adi'
            env = os.environ.copy(); env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_LAB_RIG=rig, DECODIUM_DISABLE_QML_CACHE='1', QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
            probe = '''\n Timer {interval:10000;running:true;repeat:false;onTriggered:{
                var injected=bridge.logSstvQso({}); var filters={callsign:"K1ABC"}; var rows=bridge.queryDecodeHistory(filters);
                var ok=bridge.exportAdifFromHistory(filters,__PATH__);
                var missing=bridge.exportAdifFromHistory({callsign:"NO_SUCH_CALL"},__EMPTY_PATH__);
                var badPath=bridge.exportAdifFromHistory(filters,__BAD_PATH__);
                console.log("[HISTORY-APP] "+JSON.stringify({injected:injected,count:rows.length,call:rows.length?rows[0].callsign_dx:"",exported:ok,missing:missing,badPath:badPath,stats:bridge.queryDecodeStats().total,bands:bridge.queryDecodeBands(),modes:bridge.queryDecodeModes(),forty:bridge.queryDecodeHistory({band:"40M"}).length,invalid:bridge.queryDecodeHistory({callsign:"' OR 1=1 --"}).length,tx:bridge.transmitting,monitor:bridge.monitoring}));Qt.quit();}}\n'''.replace('__PATH__', json.dumps(str(output))).replace('__EMPTY_PATH__', json.dumps(str(Path(tmp) / 'empty.adi'))).replace('__BAD_PATH__', json.dumps(str(Path(tmp) / 'nonexistent' / 'export.adi')))
            if original_qml.count(marker) != 1: raise RuntimeError('unexpected QML root')
            qml.write_text(original_qml.replace(marker, marker + probe, 1))
            sinks = []
            for port in (2237, 2239):
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); sock.bind(('127.0.0.1', port)); sinks.append(sock)
            command = ['sandbox-exec', '-p', policy, str(root / 'build/decodium'), '--test-mode', '--rig-name', rig, '--lab-no-cat', '--lab-no-monitor', '--lab-quit-ms', '30000']
            run = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, timeout=70)
            combined = run.stdout + '\n' + run.stderr
            rows = [line.split('[HISTORY-APP] ', 1)[1] for line in combined.splitlines() if '[HISTORY-APP] ' in line]
            if run.returncode or len(rows) != 1 or '[LAB] CAT auto-connect disabled' not in combined: raise RuntimeError(combined[-9000:])
            value = json.loads(rows[0])
            if not value['injected'].get('safe') or not value['injected'].get('inserted') or value['count'] != 1 or value['call'] != 'K1ABC' or not value['exported'] or value['missing'] or value['badPath'] or value['stats'] != 2 or value['bands'] != ['20M', '40M'] or value['modes'] != ['FT8'] or value['forty'] != 1 or value['invalid'] != 0 or value['tx'] or value['monitor']: raise RuntimeError(str(value))
            text = output.read_text()
            if '<CALL:5>K1ABC' not in text or '<FREQ:9>14.074000' not in text or text.count('<EOR>') != 1 or 'W1XYZ' in text: raise RuntimeError('unexpected ADIF: ' + text)
            empty_text = (Path(tmp) / 'empty.adi').read_text()
            if '<EOH>' not in empty_text or '<EOR>' in empty_text: raise RuntimeError('unexpected empty ADIF: ' + empty_text)
            results.append((value, text, empty_text)); print(label, json.dumps(value, sort_keys=True), flush=True)
    if results[0] != results[1]: raise RuntimeError('A/B history results differ')
finally:
    module.write_text(original_module); sstv.write_text(original_sstv); qml.write_text(original_qml)
    for rig in rigs:
        for parent in (Path.home() / '.qttest/Library/Application Support/IU8LMC', Path.home() / '.qttest/Library/Caches/IU8LMC'):
            path = parent / ('Decodium - ' + rig + ' - test')
            if path.is_dir(): shutil.rmtree(path)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
