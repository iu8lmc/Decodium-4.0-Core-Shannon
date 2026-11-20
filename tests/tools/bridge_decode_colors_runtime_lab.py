#!/usr/bin/env python3
"""Exercise real Bridge colour settings and restart with CAT disabled.

This instruments the build copy of Main.qml. Run without concurrent build
sessions; after a crash rebuild the QML sync target to restore the build copy.
"""
import argparse
import os
import secrets
import shutil
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

PROPERTIES = ['colorTxMessage', 'colorMyCall', 'colorNewDxccBand', 'colorNewDxcc',
              'colorNewContinentBand', 'colorNewContinent', 'colorNewCqZoneBand',
              'colorNewCqZone', 'colorNewItuZoneBand', 'colorNewItuZone',
              'colorNewGridBand', 'colorNewGrid', 'colorNewCallBand', 'colorNewCall',
              'colorLotwUser', 'colorCQ', 'colorDXEntity', 'color73', 'colorB4',
              'colorDecodeText']

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--qml', type=Path, default=ROOT / 'build/qml/decodium/Main.qml')
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/decodium')
    args = parser.parse_args()
    qml = args.qml.resolve()
    binary = args.binary.resolve()
    original = qml.read_text()
    marker = 'GlassApplicationWindow {'
    if original.count(marker) != 1:
        raise RuntimeError('unexpected Main.qml structure')
    rigs = {label: 'BridgeColor' + label + secrets.token_hex(5) for label in ('A', 'B')}
    try:
        with tempfile.TemporaryDirectory(prefix='decodium-color-runtime-') as tmp:
            for label, color in [('A','#123456'),('B','#ABCDEF')]:
                profile = Path(tmp)/label
                profile.mkdir()
                for phase in ['write', 'restart']:
                    assign = '''
        for (var i=0; i<names.length; ++i) {
            bridge.setDecodeColorEnabled(names[i], false);
            bridge.setDecodeColorBold(names[i], true);
            bridge.setDecodeColorBg(names[i], names[i] === "colorTxMessage" ? "#FD0420" : "__COLOR__");
            bridge.setDecodeColorBgEnabled(names[i], true);
        }
        bridge.b4Strikethrough = false;
''' if phase == 'write' else ''
                    injected = '''\n    Timer { interval: 12000; running: true; repeat: false; onTriggered: {
        var failed = [], names = __NAMES__;
        __ASSIGN__
        for (var i=0; i<names.length; ++i) {
            var name=names[i];
            if (bridge.decodeColorEnabled(name) || bridge.effectiveDecodeColor(name) !== bridge.decodeColorFallback) failed.push(name+":enabled");
            if (!bridge.decodeColorBold(name)) failed.push(name+":bold");
            if (bridge.decodeColorBgValue(name) !== (name === "colorTxMessage" ? "#FD0420" : "__COLOR__") || !bridge.decodeColorBgEnabled(name)) failed.push(name+":background");
        }
        if (bridge.decodeHighlightUserBg({isCQ:true}) !== "__COLOR__") failed.push("highlight");
        if (bridge.decodeHighlightUserBg({isTx:true,isCQ:true}) !== "#FD0420") failed.push("priority");
        if (bridge.b4Strikethrough) failed.push("strikethrough");
        if (bridge.decodeColorEnabled("invalid") !== true || bridge.decodeColorBold("invalid") !== false) failed.push("invalid");
        console.log("[COLOR-RUNTIME-__LABEL__-__PHASE__] " + (failed.length ? "FAIL " + failed.join(",") : "PASS"));
    } }\n'''
                    import json
                    injected = (injected.replace('__NAMES__', json.dumps(PROPERTIES))
                                .replace('__ASSIGN__', assign).replace('__COLOR__', color)
                                .replace('__LABEL__', label).replace('__PHASE__', phase))
                    qml.write_text(original.replace(marker, marker + injected, 1))
                    env = os.environ.copy()
                    env.update(DECODIUM_LAB_SETTINGS_DIR=str(profile), DECODIUM_DISABLE_QML_CACHE='1')
                    cmd=[str(binary),'--test-mode','--rig-name',rigs[label],'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','27000']
                    result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=110)
                    if result.returncode or f'[COLOR-RUNTIME-{label}-{phase}] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
                        print(result.stdout[-12000:])
                        raise RuntimeError(f'{label} {phase}: colour probe failed (exit {result.returncode})')
                    print(f'{label} {phase}: 20 real Bridge colours and preferences passed; CAT disabled', flush=True)
    finally:
        qml.write_text(original)
        for rig in rigs.values():
            for base in ('Application Support','Caches'):
                target=Path.home()/'.qttest/Library'/base/'IU8LMC'/('Decodium - '+rig+' - test')
                if target.is_dir(): shutil.rmtree(target)

if __name__ == '__main__':
    main()
