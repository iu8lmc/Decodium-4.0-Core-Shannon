#!/usr/bin/env python3
"""Exercise weather preview polling while the settings host is shown/hidden."""
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
qml = root/'build-baseline/qml/decodium/Main.qml'
tab = root/'build-baseline/qml/decodium/components/SettingsTab0.qml'
original, original_tab = qml.read_text(), tab.read_text()
anchor = 'onRunningChanged: if (running) weatherPreview.preview = bridge.currentWeatherPreview()'
if original.count('GlassApplicationWindow {') != 1 or original_tab.count(anchor) != 1:
    raise RuntimeError('unexpected QML source')
probe = '''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{
   bridge.setSetting("WeatherApiEnable",true); mainWindow.openSettingsTab(0); hideSettings.start();
 }}
 Timer {id:hideSettings;interval:6000;running:false;repeat:false;onTriggered:{
   settingsFloatingWindow.hideHostedWindow(); reopenSettings.start();
 }}
 Timer {id:reopenSettings;interval:2000;running:false;repeat:false;onTriggered:{
   mainWindow.openSettingsTab(0); finishSettings.start();
 }}
 Timer {id:finishSettings;interval:3000;running:false;repeat:false;onTriggered:{Qt.quit();}}
'''
try:
    with tempfile.TemporaryDirectory(prefix='decodium-weather-preview-') as tmp:
        qml.write_text(original.replace('GlassApplicationWindow {', 'GlassApplicationWindow {'+probe, 1))
        tab.write_text(original_tab.replace(anchor,
            'onRunningChanged: { console.log("[WEATHER-TIMER] " + running); '
            'if (running) weatherPreview.preview = bridge.currentWeatherPreview() }', 1))
        env = os.environ.copy()
        env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_DISABLE_QML_CACHE='1',
                   DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd = [str((root/'build-baseline/decodium').resolve()), '--test-mode', '--rig-name',
               'WeatherPreview'+secrets.token_hex(5), '--lab-no-cat', '--lab-no-monitor',
               '--lab-callsign', 'IU8LMC', '--lab-mode', 'FT8', '--lab-quit-ms', '45000']
        result = subprocess.run(cmd, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=60)
        states = [line.split('[WEATHER-TIMER] ', 1)[1].strip()
                  for line in result.stdout.splitlines() if '[WEATHER-TIMER] ' in line]
        if result.returncode or states[:3] != ['true', 'false', 'true']:
            raise RuntimeError('unexpected timer transitions '+repr(states)+'\n'+result.stdout[-8000:])
        print('timer transitions: '+', '.join(states[:3]))
finally:
    qml.write_text(original)
    tab.write_text(original_tab)
