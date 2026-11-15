#!/usr/bin/env python3
"""Compare Qt disk-cache behavior for small QML components.

Set DECODIUM_QML_BINARY to the Qt qml executable if it is not on PATH.
"""
import os, subprocess, tempfile
from pathlib import Path
qt=os.environ.get('DECODIUM_QML_BINARY', 'qml')
cases={"tooltip_template":'Rectangle { Templates.ToolTip.visible: false; Templates.ToolTip.text: "x" }', "tooltip_attached":'Rectangle { ToolTip.visible: false; ToolTip.text: "x" }', "tooltip_object":'Rectangle { ToolTip { visible: false; text: "x" } }', 'rect':'Rectangle {}' ,'text':'Rectangle { Text { text: "Hello" } }','row':'Rectangle { Row {} }','connect':'Rectangle { property var engine; Connections { target: engine; ignoreUnknownSignals: true } }','hover':'Rectangle { HoverHandler {} }','tap':'Rectangle { TapHandler {} }','color':'Rectangle { required property var engine; property color activeColor: "#00e59b" }','style':'Rectangle { property color activeColor: "#00e59b"; border.color: activeColor; radius: 10 }'}
with tempfile.TemporaryDirectory(prefix='qml-isolate-') as d:
 p=Path(d); env=os.environ.copy(); env.update(QT_QPA_PLATFORM='offscreen',QML_DISK_CACHE_PATH=str(p/'cache'),QT_LOGGING_RULES='qt.qml.diskcache.debug=true')
 (p/'cache').mkdir()
 for name,body in cases.items():
  target=p/(name+'.qml'); target.write_text('import QtQuick\nimport QtQuick.Controls\nimport QtQuick.Templates as Templates\n'+body+'\n')
  launcher=p/('launcher_'+name+'.qml'); launcher.write_text('import QtQuick\nQtObject { Component.onCompleted: { var c=Qt.createComponent("'+target.as_uri()+'"); console.log("probe",c.status,c.errorString()); Qt.quit() } }')
  runs=[subprocess.run([str(qt),str(launcher)],env=env,capture_output=True,text=True,timeout=20,check=True) for _ in range(2)]
  text=runs[1].stderr+runs[1].stdout
  print(name,'MISMATCH' if 'Checksum mismatch' in text else 'OK',text.strip().splitlines()[-1],flush=True)
