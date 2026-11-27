#!/usr/bin/env python3
"""Inspect the actual FT8 callback during the isolated world-map WAV probe."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
source = root / 'src/bridge/DecodiumBridgeFt8Ready.cpp'
original = source.read_text()
anchor = 'void DecodiumBridge::onFt8DecodeReady(quint64 serial, QStringList rows)\n{'
assert original.count(anchor) == 1
instrumentation = '''
    qWarning().noquote() << "[MAP-WAV-CALLBACK]" << serial << rows.join("|")
        << "early" << m_ft8EarlyDecodeSerials.contains(serial)
        << "mode" << m_decodeModeBySerial.value(serial) << m_mode
        << "session" << m_decodeSessionBySerial.value(serial, m_decodeSessionId)
        << m_decodeSessionId << "sstv" << m_sstvWorkspaceActive;
'''
env = os.environ.copy()
env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
policy = '(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
try:
    summary_anchor = '    // cq_only/mycall_only are rejection counters, not alert matches.'
    assert original.count(summary_anchor) == 1
    summary = '''    qWarning().noquote() << "[MAP-WAV-SUMMARY]" << "accepted" << accepted
        << "parse" << parseFailures << "guardrail" << guardrailFiltered
        << "semantic" << semanticFiltered << "user" << userFiltered
        << "ui" << uiFiltered << "cq" << cqOnlyFiltered
        << "mycall" << myCallOnlyFiltered << "duplicates" << duplicatesSkipped;
'''
    rejection = '        if (!shouldAcceptDecodeEntryByUserFilters(entry, userFilterConfig, false, &userFilterReason)) {'
    assert original.count(rejection) == 1
    source.write_text(original.replace(anchor, anchor + instrumentation, 1)
                      .replace(summary_anchor, summary + summary_anchor, 1)
                      .replace(rejection, rejection + '\n            qWarning().noquote() << "[MAP-WAV-FILTER]" << userFilterReason << "legacyPath" << legacyIniPath() << "labRoot" << qEnvironmentVariable("DECODIUM_LAB_SETTINGS_DIR");', 1))
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL)
    result = subprocess.run(['sandbox-exec', '-p', policy, 'python3', 'tests/tools/bridge_worldmapreplay_wav_diagnostic.py'], cwd=root, env=env, capture_output=True, text=True, timeout=100)
    for line in (result.stdout + '\n' + result.stderr).splitlines():
        if '[MAP-WAV-CALLBACK]' in line or '[MAP-WAV-SUMMARY]' in line or '[MAP-WAV-FILTER]' in line or '[MAP-DIAG]' in line or '[DECODEMETRIC]' in line:
            print(line, flush=True)
    print('probe_exit', result.returncode, flush=True)
finally:
    source.write_text(original)
    subprocess.run(['cmake', '--build', 'build', '--target', 'decodium_qml', '-j', '2'], cwd=root, check=True, stdout=subprocess.DEVNULL)
