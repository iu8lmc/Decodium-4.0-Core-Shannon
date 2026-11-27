#!/usr/bin/env python3
"""Build an isolated experiment for FT8 period arming after mode changes."""
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[2]
source=root/'src/bridge/DecodiumBridgePeriodTimerArm.cpp'
original=source.read_bytes();text=original.decode()
queue=root/'src/bridge/DecodiumBridgeFt8Queue.cpp'
original_queue=queue.read_bytes();queue_text=original_queue.decode()
dispatch=root/'src/bridge/DecodiumBridgeTimeSyncDispatch.cpp'
original_dispatch=dispatch.read_bytes();dispatch_text=original_dispatch.decode()
anchor='''    if (modeSnapshot == QStringLiteral("WSPR")
'''
experiment='''    if (QStandardPaths::isTestModeEnabled()
        && qEnvironmentVariableIntValue("DECODIUM_LAB_FT8_IMMEDIATE_ARM") == 1
        && reason == QStringLiteral("setMode") && modeSnapshot == QStringLiteral("FT8")) {
        resetRxPeriodAccumulation(true);
        m_periodTicks = 0;
        m_lastPeriodSlot = -1;
        m_periodTimer->start();
        qInfo() << "[FT8-IMMEDIATE-ARM-LAB]";
        return;
    }

'''
if text.count(anchor)!=1:raise RuntimeError('unexpected timer source')
queue_anchor='    decodium::ft8::DecodeRequest req;\n'
capture='''    if (QStandardPaths::isTestModeEnabled() && !qEnvironmentVariableIsEmpty("DECODIUM_LAB_FT8_CAPTURE_DIR")) {
        QString const path = QString::fromLocal8Bit(qgetenv("DECODIUM_LAB_FT8_CAPTURE_DIR"))
            + QStringLiteral("/ft8-input-%1.wav").arg(serial);
        if (!writeMono16WavFile(path, audioSnapshot, 12000)) qWarning() << "[FT8-CAPTURE] failed" << path;
        else qInfo() << "[FT8-CAPTURE]" << serial << path << audioSnapshot.size();
    }
'''
if queue_text.count(queue_anchor)!=1:raise RuntimeError('unexpected FT8 queue')
dispatch_anchor='''    int const minimumSamples = minimumDecodeSamplesForMode(modeSnapshot);
    bool const enoughForMode =
'''
padding='''    if (QStandardPaths::isTestModeEnabled()
        && qEnvironmentVariableIntValue("DECODIUM_LAB_FT8_PARTIAL_PAD") == 1
        && modeSnapshot == QStringLiteral("FT8") && targetSamples > 0
        && finalAudio.size() >= 144000 && finalAudio.size() < targetSamples
        && (qEnvironmentVariableIntValue("DECODIUM_LAB_FT8_PAD_MIN_SAMPLES") <= 0
            || targetSamples - finalAudio.size() >= qEnvironmentVariableIntValue("DECODIUM_LAB_FT8_PAD_MIN_SAMPLES"))) {
        int const missing = targetSamples - finalAudio.size();
        QVector<short> padded(missing, 0);
        padded += finalAudio;
        finalAudio = std::move(padded);
        qInfo() << "[FT8-PARTIAL-PAD-LAB]" << missing;
    }

'''
if dispatch_text.count(dispatch_anchor)!=1:raise RuntimeError('unexpected dispatch source')
consume_anchor='''            && finalAudio.size() >= minimumSamples;'''
consume_lab='''            && finalAudio.size() >= minimumSamples
            && !(QStandardPaths::isTestModeEnabled() && modeSnapshot == QStringLiteral("FT8")
                 && qEnvironmentVariableIntValue("DECODIUM_LAB_FT8_KEEP_BORROW") == 1
                 && (qEnvironmentVariableIntValue("DECODIUM_LAB_FT8_KEEP_MIN_SAMPLES") <= 0
                     || targetSamples - pendingSamples >= qEnvironmentVariableIntValue("DECODIUM_LAB_FT8_KEEP_MIN_SAMPLES")));'''
if dispatch_text.count(consume_anchor)!=1:raise RuntimeError('unexpected borrow policy')
try:
    source.write_bytes(text.replace('#include <QTimer>','#include <QTimer>\n#include <QStandardPaths>\n#include <QDebug>',1).replace(anchor,experiment+anchor,1).encode())
    queue.write_bytes(queue_text.replace('#include "DecodiumBridge.h"','#include "DecodiumBridge.h"\n#include "DecodiumBridgeWavIO.h"\n#include <QDebug>',1).replace(queue_anchor,capture+queue_anchor,1).encode())
    dispatch.write_bytes(dispatch_text.replace('#include <QTimer>','#include <QTimer>\n#include <QStandardPaths>',1).replace(dispatch_anchor,padding+dispatch_anchor,1).replace(consume_anchor,consume_lab,1).encode())
    subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,check=True,stdout=subprocess.DEVNULL)
    shutil.copy2(root/'build-baseline/decodium',root/'build-baseline/decodium-ft8-immediate-arm-lab')
finally:
    source.write_bytes(original)
    queue.write_bytes(original_queue)
    dispatch.write_bytes(original_dispatch)
    subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,check=True,stdout=subprocess.DEVNULL)
