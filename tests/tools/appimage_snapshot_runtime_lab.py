#!/usr/bin/env python3
"""Exercise AppImage-style QML snapshots across changing mount directories."""
import argparse
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument('--binary',type=Path,default=root/'build-baseline/decodium')
p.add_argument('--qml',type=Path,default=root/'build-baseline/qml')
p.add_argument('--appdir-template',type=Path,help='copy an extracted AppImage tree and use its AppRun and bundled Qt')
p.add_argument('--output-dir',type=Path,required=True)
a=p.parse_args()
a.output_dir.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='decodium-appimage-snapshot-') as tmp:
    base=Path(tmp);mounts=[base/'mount-a',base/'mount-b']
    for mount in mounts:
        if a.appdir_template:
            shutil.copytree(a.appdir_template,mount,symlinks=True,copy_function=shutil.copy2)
        bindir=mount/'usr/bin';bindir.mkdir(parents=True,exist_ok=True)
        shutil.copy2(a.binary,bindir/'decodium')
        for name in ('decodium','dialogs','panels'):
            if (bindir/'qml'/name).exists():
                shutil.rmtree(bindir/'qml'/name)
            shutil.copytree(a.qml/name,bindir/'qml'/name,copy_function=shutil.copy2)
    settings=base/'settings';settings.mkdir();cache=base/'cache';cache.mkdir()
    rig='Snapshot'+secrets.token_hex(5)
    results=[]
    for index in range(5):
        mount=mounts[0] if index==0 else mounts[1]
        if index==1:
            # A previous AppImage mount can disappear while another instance uses the snapshot.
            shutil.rmtree(mounts[0])
        if index==2:
            # A damaged snapshot must fall back to the mounted QML tree.
            snapshot=Path(results[1]['snapshot'])
            with (snapshot/'decodium/Main.qml').open('ab') as f:f.write(b'\n// corrupted snapshot probe\n')
        if index==3:
            # Same timestamp, changed content: an AppImage update must get a new snapshot.
            main=mount/'usr/bin/qml/decodium/Main.qml';stamp=main.stat()
            with main.open('ab') as f:f.write(b'\n// snapshot version change probe\n')
            os.utime(main,ns=(stamp.st_atime_ns,stamp.st_mtime_ns))
        env=os.environ.copy()
        env.update(APPDIR=str(mount),DECODIUM_LAB_SETTINGS_DIR=str(settings),
                   QML_DISK_CACHE_PATH=str(cache),QT_LOGGING_RULES='qt.qml.diskcache.debug=true',
                   DECODIUM_FORCE_STANDALONE_UDP='1')
        if a.appdir_template:
            env['APPIMAGE']=str(base/'synthetic.AppImage')
        executable=mount/'AppRun' if a.appdir_template else mount/'usr/bin/decodium'
        cmd=[str(executable),'--test-mode','--rig-name',rig,'--lab-no-cat',
             '--lab-no-monitor','--lab-callsign','IU8LMC','--lab-grid','JM75','--lab-mode','FT8',
             '--lab-quit-ms','17000']
        run=subprocess.run(cmd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
        (a.output_dir/f'snapshot-run-{index+1}.log').write_text(run.stdout)
        matches=re.findall(r'AppImage QML snapshot: (.+)',run.stdout)
        times=re.findall(r'Main.qml created as top-level window in (\d+) ms \(component load\); object creation and window setup (\d+) ms',run.stdout)
        prepare=re.findall(r'AppImage QML snapshot preparation ms=(\d+)',run.stdout)
        fallback='AppImage QML snapshot unavailable; falling back to uncached mounted files' in run.stdout
        if run.returncode or (len(matches)!=1 and not fallback) or len(times)!=1 or len(prepare)!=1 or '[LAB] CAT auto-connect disabled' not in run.stdout:
            raise RuntimeError('snapshot app failed: '+run.stdout[-10000:])
        result={'run':index+1,'snapshot':matches[0].strip() if matches else None,'fallback':fallback,
                'cacheDisabled':'QML disk cache disabled for AppImage runtime' in run.stdout,
                'snapshotPreparationMs':int(prepare[0]),
                'componentMs':int(times[0][0]),'windowMs':int(times[0][1]),
                'cacheMismatch':bool(re.search(r'Checksum mismatch.*Main.qml',run.stdout))}
        results.append(result)
        print(json.dumps(result),flush=True)
    if results[0]['snapshot']!=results[1]['snapshot'] or results[1]['cacheMismatch']:
        raise RuntimeError('unchanged QML did not reuse a stable snapshot')
    if not results[2]['fallback'] or not results[2]['cacheDisabled'] or results[2]['snapshot'] is not None:
        raise RuntimeError('damaged snapshot did not safely fall back')
    if results[3]['snapshot']==results[1]['snapshot'] or results[3]['snapshot']!=results[4]['snapshot'] or results[4]['cacheMismatch']:
        raise RuntimeError('updated QML did not get a separate reusable snapshot')
    (a.output_dir/'snapshot-results.json').write_text(json.dumps(results,indent=2)+'\n')
