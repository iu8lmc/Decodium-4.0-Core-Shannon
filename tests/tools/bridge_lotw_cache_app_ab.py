#!/usr/bin/env python3
"""Compare LoTW cached-user loading in isolated applications without network."""
import json, os, secrets, shutil, socket, subprocess, tempfile
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--empty-cache', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
module = root / 'src/bridge/DecodiumBridgeUsStateLotw.cpp'
qml = root / 'build/qml/decodium/Main.qml'
original_module, original_qml = module.read_text(), qml.read_text()
baseline = subprocess.check_output(['git', 'show', 'v1.0.737:src/bridge/DecodiumBridge.cpp'], cwd=root, text=True)
start, end = 'void DecodiumBridge::setShowUsState(', 'void DecodiumBridge::reloadLotwUsers('
old = baseline[baseline.index(start):baseline.index(end, baseline.index(start))]
current = original_module[original_module.index(start):original_module.index(end, original_module.index(start))]
if old.strip() != current.strip(): raise RuntimeError('LoTW controls differ from baseline')
path_start, path_end = 'static QString sharedLotwUsersCachePath()\n{', '\nstatic LotwUsersLoadResult parseLotwUsersCsv('
isolated = original_module[:original_module.index(path_start)] + path_start + '''
    return qEnvironmentVariable("DECODIUM_LAB_LOTW_CACHE");
}
''' + original_module[original_module.index(path_end):]
marker = 'GlassApplicationWindow {'
probe = '''\n Timer {interval:9000;running:true;repeat:false;onTriggered:{bridge.lotwEnabled=true;waitCache.start();}}
 Timer {id:waitCache;interval:200;repeat:true;property int ticks:0;onTriggered:{
  if ((!bridge.lotwUpdating && bridge.lotwUserCount===__COUNT__) || ++ticks>100) {
   console.log("[LOTW-APP] "+JSON.stringify({enabled:bridge.lotwEnabled,updating:bridge.lotwUpdating,count:bridge.lotwUserCount,first:bridge.isLotwUser("k1abc"),second:bridge.isLotwUser("W1XYZ"),unknown:bridge.isLotwUser("N0CALL"),tx:bridge.transmitting,monitor:bridge.monitoring}));stop();Qt.quit();}}}\n'''.replace('__COUNT__', '0' if args.empty_cache else '2')
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
rigs, results = [], []
try:
    for label in ('before', 'after'):
        content = isolated.replace(current, old, 1) if label == 'before' else isolated
        module.write_text(content)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        rig = 'LotwCacheAB' + secrets.token_hex(5); rigs.append(rig)
        with tempfile.TemporaryDirectory(prefix='decodium-lotw-cache-') as tmp:
            cache = Path(tmp) / 'lotw.csv'; cache.write_text('CALLSIGN,DATE\n' if args.empty_cache else 'CALLSIGN,DATE\nK1ABC,2026-01-01\nw1xyz,2026-01-02\nK1ABC,2026-01-03\n')
            env = os.environ.copy(); env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_LAB_LOTW_CACHE=str(cache), DECODIUM_DISABLE_QML_CACHE='1', QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
            if original_qml.count(marker) != 1: raise RuntimeError('unexpected QML root')
            qml.write_text(original_qml.replace(marker, marker + probe, 1))
            sinks = []
            for port in (2237, 2239):
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); sock.bind(('127.0.0.1', port)); sinks.append(sock)
            command = ['sandbox-exec', '-p', policy, str(root / 'build/decodium'), '--test-mode', '--rig-name', rig, '--lab-no-cat', '--lab-no-monitor', '--lab-quit-ms', '35000']
            run = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, timeout=70)
            combined = run.stdout + '\n' + run.stderr
            rows = [line.split('[LOTW-APP] ', 1)[1] for line in combined.splitlines() if '[LOTW-APP] ' in line]
            if run.returncode or len(rows) != 1 or '[LAB] CAT auto-connect disabled' not in combined: raise RuntimeError(combined[-9000:])
            if args.empty_cache and 'CSV contains no valid callsigns' not in combined:
                raise RuntimeError('invalid cache was not observed by the loader: ' + combined[-9000:])
            value = json.loads(rows[0]); expected = {'enabled': True, 'updating': False, 'count': 0 if args.empty_cache else 2, 'first': not args.empty_cache, 'second': not args.empty_cache, 'unknown': False, 'tx': False, 'monitor': False}
            if value != expected: raise RuntimeError(str(value))
            results.append(value); print(label, json.dumps(value, sort_keys=True), flush=True)
    if results[0] != results[1]: raise RuntimeError('LoTW A/B differs')
finally:
    module.write_text(original_module); qml.write_text(original_qml)
    for rig in rigs:
        for parent in (Path.home() / '.qttest/Library/Application Support/IU8LMC', Path.home() / '.qttest/Library/Caches/IU8LMC'):
            path = parent / ('Decodium - ' + rig + ' - test')
            if path.is_dir(): shutil.rmtree(path)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
