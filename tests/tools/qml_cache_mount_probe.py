#!/usr/bin/env python3
"""Probe QML cache reuse across two simulated AppImage mount paths."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

qml = os.environ.get('DECODIUM_QML_BINARY', 'qml')
with tempfile.TemporaryDirectory(prefix='decodium-qml-mount-') as directory:
    root = Path(directory)
    mounts = [root/'mount-a', root/'mount-b']
    for mount in mounts: mount.mkdir()
    source = mounts[0]/'Probe.qml'
    source.write_text('import QtQuick\nRectangle { property string revision: "first" }\n')
    shutil.copy2(source, mounts[1]/'Probe.qml')
    stable = root/'stable'
    stable.symlink_to(mounts[0], target_is_directory=True)
    cache = root/'cache'; cache.mkdir()
    launcher = root/'launcher.qml'
    launcher.write_text('''import QtQuick
QtObject { Component.onCompleted: {
  var c = Qt.createComponent("%s");
  if (c.status !== Component.Ready) { console.log("[MOUNT-PROBE] error "+c.errorString()); Qt.quit(); return; }
  var o=c.createObject(null); console.log("[MOUNT-PROBE] revision="+o.revision); Qt.quit();
} }
''' % (stable/'Probe.qml').as_uri())
    env=os.environ.copy()
    env.update(QT_QPA_PLATFORM='offscreen',QML_DISK_CACHE_PATH=str(cache),QT_LOGGING_RULES='qt.qml.diskcache.debug=true')
    results=[]
    for index in range(4):
        if index:
            stable.unlink(); stable.symlink_to(mounts[1], target_is_directory=True)
        if index == 2:
            # AppImages can contain files with identical timestamps across
            # builds. Test what happens if content changes without mtime.
            target=mounts[1]/'Probe.qml'
            stamp=target.stat()
            target.write_text('import QtQuick\nRectangle { property string revision: "other" }\n')
            os.utime(target,ns=(stamp.st_atime_ns,stamp.st_mtime_ns))
        if index == 3:
            version_cache=root/'cache-new-content'
            version_cache.mkdir()
            env['QML_DISK_CACHE_PATH']=str(version_cache)
        p=subprocess.run([qml,str(launcher)],env=env,text=True,capture_output=True,timeout=20,check=True)
        text=p.stdout+p.stderr
        observed=next((line.split('revision=',1)[1] for line in text.splitlines() if '[MOUNT-PROBE] revision=' in line),'missing')
        results.append({'run':index+1,'revision':observed,
                        'checksumMismatch':'Checksum mismatch' in text,
                        'errors':[line for line in text.splitlines() if 'Error loading' in line]})
    print(json.dumps(results,indent=2))
    if any(x['revision']=='missing' for x in results):raise RuntimeError('component did not load')
    if results[3]['revision']!='other':raise RuntimeError('new content cache did not load new QML')
