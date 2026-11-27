#!/usr/bin/env python3
"""Exercise SWR admission in isolated apps with synthetic telemetry and CAT off."""
import json, os, secrets, shutil, socket, subprocess, tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
module = root / 'src/bridge/DecodiumBridgeTxProtection.cpp'
sstv = root / 'src/bridge/DecodiumBridgeSstv.cpp'
qml = root / 'build/qml/decodium/Main.qml'
original_module, original_sstv, original_qml = (p.read_text() for p in (module, sstv, qml))
legacy = root / 'src/bridge/DecodiumBridgeLegacyInit.cpp'
legacy_original = legacy.read_text()
legacy_macro = '#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled'
assert legacy_original.count(legacy_macro) == 1
baseline = subprocess.check_output(['git', 'show', 'v1.0.737:src/bridge/DecodiumBridge.cpp'], cwd=root, text=True)
start, end = 'bool DecodiumBridge::checkSwrAllowsTransmission(', '// === SETTINGS ==='
old = baseline[baseline.index(start):baseline.index(end, baseline.index(start))]
for a, b in (('bridgeLog(', 'decodiumBridgeLogbookLog('), ('wsprPowerDbmToWatts(', 'decodiumBridgeWsprPowerDbmToWatts('), ('normalizeWsprPowerDbm(', 'decodiumBridgeNormalizeWsprPowerDbm('), ('wsprPowerDbmLabel(', 'decodiumBridgeWsprPowerDbmLabel(')):
    old = old.replace(a, b)
prefix = original_module[:original_module.index(start)]
if original_module[original_module.index(start):] != old:
    raise RuntimeError('TX protection source differs from baseline')
anchor = 'QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)\n{'
if original_sstv.count(anchor) != 1: raise RuntimeError('unexpected test insertion point')
injection = '''
#if DECODIUM_HAS_SSTV
 { Q_UNUSED(values); setSetting(QStringLiteral("CheckSWR"), false); m_rigSwr=4.0;
   bool disabled=checkSwrAllowsTransmission(QStringLiteral("lab"));
   setSetting(QStringLiteral("CheckSWR"), true); setSetting(QStringLiteral("SWRStopThreshold"), 2.5);
   m_rigSwr=2.5; bool boundary=checkSwrAllowsTransmission(QStringLiteral("lab"));
   m_rigSwr=2.6; bool rejected=!checkSwrAllowsTransmission(QStringLiteral("lab"));
   setSetting(QStringLiteral("SWRStopThreshold"), 0.1); m_rigSwr=1.5;
   bool lowBoundary=checkSwrAllowsTransmission(QStringLiteral("lab"));
   m_rigSwr=1.6; bool lowRejected=!checkSwrAllowsTransmission(QStringLiteral("lab"));
   setSetting(QStringLiteral("SWRStopThreshold"), 99.0); m_rigSwr=5.0;
   bool highBoundary=checkSwrAllowsTransmission(QStringLiteral("lab"));
   m_rigSwr=5.1; bool highRejected=!checkSwrAllowsTransmission(QStringLiteral("lab"));
   setSetting(QStringLiteral("SWRStopThreshold"), 2.5); m_rigSwr=2.6;
   m_transmitting=true; m_cwTxActive=true;
   enforceSwrTransmissionLimit(QStringLiteral("synthetic-lab"));
   bool stopped=!m_transmitting&&!m_cwTxActive;
   m_mode=QStringLiteral("WSPR"); m_wsprPowerDbm=30;
   setSetting(QStringLiteral("PWRandSWR"), true);
   m_rigPowerWatts=6.0; m_transmitting=true; m_cwTxActive=true;
   enforceWsprPowerTransmissionLimit(QStringLiteral("synthetic-lab"));
   bool wsprBoundary=m_transmitting;
   m_rigPowerWatts=6.1; m_lastWsprPowerGuardMs=0;
   enforceWsprPowerTransmissionLimit(QStringLiteral("synthetic-lab"));
   bool wsprStopped=!m_transmitting&&!m_cwTxActive;
   return {{"disabled",disabled},{"boundary",boundary},{"rejected",rejected},
           {"lowBoundary",lowBoundary},{"lowRejected",lowRejected},
           {"highBoundary",highBoundary},{"highRejected",highRejected},
           {"stopped",stopped},{"wsprBoundary",wsprBoundary},{"wsprStopped",wsprStopped},
           {"cat",m_catConnected},{"tx",m_transmitting}}; }
#endif
'''
marker = 'GlassApplicationWindow {'
probe = '''\n Timer {interval:9000;running:true;repeat:false;onTriggered:{console.log("[SWR-APP] "+JSON.stringify(bridge.logSstvQso({})));Qt.quit();}}\n'''
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
rigs, results = [], []
try:
    legacy.write_text(legacy_original.replace(legacy_macro, '#define embeddedLegacyRigControlEnabledForBackend(...) false', 1))
    sstv.write_text(original_sstv.replace(anchor, anchor + injection, 1))
    for label in ('before', 'after'):
        module.write_text(prefix + old if label == 'before' else original_module)
        subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        rig = 'SwrAdmission' + secrets.token_hex(5); rigs.append(rig)
        with tempfile.TemporaryDirectory(prefix='decodium-swr-admission-') as tmp:
            env = os.environ.copy(); env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1', QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
            qml.write_text(original_qml.replace(marker, marker + probe, 1))
            sinks = []
            for port in (2237, 2239):
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); sock.bind(('127.0.0.1', port)); sinks.append(sock)
            command = ['sandbox-exec', '-p', policy, str(root / 'build/decodium'), '--test-mode', '--rig-name', rig, '--lab-no-cat', '--lab-no-monitor', '--lab-quit-ms', '30000']
            run = subprocess.run(command, cwd=root, env=env, capture_output=True, text=True, timeout=70)
            combined = run.stdout + '\n' + run.stderr
            rows = [line.split('[SWR-APP] ', 1)[1] for line in combined.splitlines() if '[SWR-APP] ' in line]
            if run.returncode or len(rows) != 1 or '[LAB] CAT auto-connect disabled' not in combined: raise RuntimeError(combined[-9000:])
            value = json.loads(rows[0]); expected = {'disabled': True, 'boundary': True, 'rejected': True, 'lowBoundary': True, 'lowRejected': True, 'highBoundary': True, 'highRejected': True, 'stopped': True, 'wsprBoundary': True, 'wsprStopped': True, 'cat': False, 'tx': False}
            if value != expected: raise RuntimeError(str(value))
            results.append(value); print(label, json.dumps(value, sort_keys=True), flush=True)
    if results[0] != results[1]: raise RuntimeError('A/B results differ')
finally:
    legacy.write_text(legacy_original)
    module.write_text(original_module); sstv.write_text(original_sstv); qml.write_text(original_qml)
    for rig in rigs:
        for parent in (Path.home() / '.qttest/Library/Application Support/IU8LMC', Path.home() / '.qttest/Library/Caches/IU8LMC'):
            path = parent / ('Decodium - ' + rig + ' - test')
            if path.is_dir(): shutil.rmtree(path)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
