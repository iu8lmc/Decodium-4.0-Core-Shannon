#!/usr/bin/env python3
"""Load every QML file using DecoTemplates.ToolTip in the real app engine."""
import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--binary', type=Path, default=root/'build-baseline/decodium')
p.add_argument('--qml-root', type=Path, default=root/'build-baseline/qml')
p.add_argument('--log', type=Path, required=True)
a = p.parse_args()
qml_root = a.qml_root.resolve()
main = qml_root/'decodium/Main.qml'
original = main.read_text()
marker = 'GlassApplicationWindow {'
if original.count(marker) != 1:
    raise RuntimeError('unexpected QML root')
paths = [p.resolve().as_uri() for p in qml_root.rglob('*.qml')
         if 'DecoTemplates.ToolTip.' in p.read_text()]
if not paths:
    raise RuntimeError('no tooltip components found')
probe = '''
 Timer {interval:9000; running:true; repeat:false; onTriggered: {
   var urls=__URLS__, failures=[];
   for(var i=0;i<urls.length;i++) {
     var c=Qt.createComponent(urls[i],Component.PreferSynchronous);
     if(c.status!==Component.Ready) failures.push({url:urls[i],status:c.status,error:c.errorString()});
   }
   console.log("[TOOLTIP-COMPONENTS] "+JSON.stringify({count:urls.length,failures:failures,tx:bridge.transmitting}));
   Qt.quit();
 }}
'''.replace('__URLS__', json.dumps(paths))
try:
    with tempfile.TemporaryDirectory(prefix='decodium-tooltip-components-') as tmp:
        main.write_text(original.replace(marker,marker+probe,1))
        env=os.environ.copy()
        env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','Tooltips'+secrets.token_hex(5),
             '--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-grid','JM75',
             '--lab-mode','FT8','--lab-quit-ms','100000']
        result=subprocess.run(cmd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
        a.log.write_text(result.stdout)
        lines=[x.split('[TOOLTIP-COMPONENTS] ',1)[1] for x in result.stdout.splitlines() if '[TOOLTIP-COMPONENTS] ' in x]
        if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:
            raise RuntimeError(result.stdout[-10000:])
        data=json.loads(lines[0])
        if data['count']!=len(paths) or data['failures'] or data['tx']:
            raise RuntimeError('component load failed: '+json.dumps(data,indent=2))
        print(json.dumps(data,sort_keys=True))
finally:
    main.write_text(original)
