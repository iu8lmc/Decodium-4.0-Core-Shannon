#!/usr/bin/env python3
"""Build a test-only executable that can save FT8 decoder input snapshots."""
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[2]
source=root/'src/bridge/DecodiumBridgeFt8Queue.cpp'
original=source.read_text()
anchor='    decodium::ft8::DecodeRequest req;\n'
injection='''    if (QStandardPaths::isTestModeEnabled() && !qEnvironmentVariableIsEmpty("DECODIUM_LAB_FT8_CAPTURE_DIR")) {
        QString const path = QString::fromLocal8Bit(qgetenv("DECODIUM_LAB_FT8_CAPTURE_DIR"))
            + QStringLiteral("/ft8-input-%1.wav").arg(serial);
        if (!writeMono16WavFile(path, audioSnapshot, 12000))
            qWarning() << "[FT8-CAPTURE] failed" << path;
        else
            qInfo() << "[FT8-CAPTURE]" << serial << path << audioSnapshot.size();
    }
'''
if original.count(anchor)!=1:
    raise RuntimeError('unexpected FT8 source structure')
try:
    source.write_text(original.replace('#include "DecodiumBridge.h"', '#include "DecodiumBridge.h"\n#include "DecodiumBridgeWavIO.h"\n#include <QDebug>',1).replace(anchor,injection+anchor,1))
    subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,check=True,stdout=subprocess.DEVNULL)
    shutil.copy2(root/'build-baseline/decodium',root/'build-baseline/decodium-ft8-audio-capture')
finally:
    source.write_text(original)
    subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,check=True,stdout=subprocess.DEVNULL)
