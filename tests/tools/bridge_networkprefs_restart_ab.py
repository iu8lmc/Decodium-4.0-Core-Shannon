#!/usr/bin/env python3
"""Compare UDP interface persistence in isolated app sessions."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
module = root / 'src/bridge/DecodiumBridgeNetworkPreferences.cpp'
settings = root / 'src/bridge/DecodiumBridgeLegacySettings.cpp'
qml = root / 'build/qml/decodium/Main.qml'
original_module, original_settings, original_qml = (p.read_text() for p in (module, settings, qml))
baseline = subprocess.check_output(['git', 'show', 'v1.0.737:src/bridge/DecodiumBridge.cpp'], cwd=root, text=True)
start, end = 'int DecodiumBridge::remoteWebSocketPort() const', 'void DecodiumBridge::saveSettings()'
old = baseline[baseline.index(start):baseline.index(end, baseline.index(start))]
prefix = original_module[:original_module.index(start)]
if original_module[original_module.index(start):] != old: raise RuntimeError('network preferences differ from baseline')
begin, finish = 'QString DecodiumBridge::legacyIniPath() const\n{', '\nQString DecodiumBridge::legacyConfigGroupName() const'
isolated = original_settings[:original_settings.index(begin)] + begin + '''
    return QDir(qEnvironmentVariable("DECODIUM_LAB_SETTINGS_DIR")).filePath(QStringLiteral("network-test.ini"));
}
''' + original_settings[original_settings.index(finish):]
marker = 'GlassApplicationWindow {'
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
rigs, results = [], []
try:
    settings.write_text(isolated)
    for label in ('before', 'after'):
        module.write_text(prefix + old if label == 'before' else original_module)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        rig = 'NetworkPrefsAB' + secrets.token_hex(5); rigs.append(rig)
        with tempfile.TemporaryDirectory(prefix='decodium-network-prefs-') as tmp:
            env = os.environ.copy(); env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1', QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
            sinks = []
            for port in (2237, 2239):
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); sock.bind(('127.0.0.1', port)); sinks.append(sock)
            observed = []
            for phase in ('fallback', 'write', 'read', 'clear', 'empty'):
                actions = {'fallback': 'bridge.setSetting("RemoteHttpPort",24001);bridge.setSetting("RemoteWsPort",0);', 'write': 'bridge.setUdpInterfaceName(" lab-interface ");bridge.setSetting("RemoteWsPort",25000);', 'read': '', 'clear': 'bridge.setUdpInterfaceName("");', 'empty': ''}
                probe = '''\n Timer {interval:9000;running:true;repeat:false;onTriggered:{__ACTION__
                 console.log("[NETWORK-PREFS] "+JSON.stringify({interfaceName:bridge.udpInterfaceName(),port:bridge.remoteWebSocketPort(),tx:bridge.transmitting,monitor:bridge.monitoring}));Qt.quit();}}\n'''.replace('__ACTION__', actions[phase])
                if original_qml.count(marker) != 1: raise RuntimeError('unexpected QML root')
                qml.write_text(original_qml.replace(marker, marker + probe, 1))
                command = ['sandbox-exec', '-p', policy, str(root / 'build/decodium'), '--test-mode', '--rig-name', rig, '--lab-no-cat', '--lab-no-monitor', '--lab-quit-ms', '30000']
                run = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, timeout=70)
                combined = run.stdout + '\n' + run.stderr
                rows = [line.split('[NETWORK-PREFS] ', 1)[1] for line in combined.splitlines() if '[NETWORK-PREFS] ' in line]
                if run.returncode or len(rows) != 1 or '[LAB] CAT auto-connect disabled' not in combined: raise RuntimeError(combined[-9000:])
                value = json.loads(rows[0]); expected = {'interfaceName': 'lab-interface' if phase in ('write', 'read') else '', 'port': 24000 if phase == 'fallback' else 25000, 'tx': False, 'monitor': False}
                if value != expected: raise RuntimeError(f'{label} {phase}: {value}')
                observed.append(value)
            results.append(observed); print(label, json.dumps(observed, sort_keys=True), flush=True)
    if results[0] != results[1]: raise RuntimeError('A/B network preferences differ')
finally:
    module.write_text(original_module); settings.write_text(original_settings); qml.write_text(original_qml)
    for rig in rigs:
        for parent in (Path.home() / '.qttest/Library/Application Support/IU8LMC', Path.home() / '.qttest/Library/Caches/IU8LMC'):
            path = parent / ('Decodium - ' + rig + ' - test')
            if path.is_dir(): shutil.rmtree(path)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
