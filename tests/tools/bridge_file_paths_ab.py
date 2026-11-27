#!/usr/bin/env python3
"""Compare path helpers and exercise actual temporary directory operations."""
from pathlib import Path
import shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[2]
old = (root / 'tests/bridge_refactor_fixtures/file_paths_before.inc').read_text()
new = (root / 'src/bridge/DecodiumBridgeFilePaths.cpp').read_text()
assert new.endswith(old), 'Path helper implementation changed during extraction'
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
harness = r'''
#include <QCoreApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include "DecodeUtcTime.h"
#include "paths.inc"
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv); QCoreApplication::setApplicationName("DecodiumPathTest");
 QStandardPaths::setTestModeEnabled(true); QTemporaryDir tmp;
 bool expansion=expandedLocalFilePath(" ~ ")==QDir::homePath() && expandedLocalFilePath("~/abc")==QDir::home().absoluteFilePath("abc") && expandedLocalFilePath("  relative/file  ")=="relative/file";
 QString err; QString file=tmp.path()+"/a/b/test.wav"; bool creation=tmp.isValid()&&ensureParentDirectoryForFile(file,&err)&&QDir(tmp.path()+"/a/b").exists()&&ensureParentDirectoryForFile(file,nullptr);
 QFile blocker(tmp.path()+"/block"); bool block=blocker.open(QIODevice::WriteOnly); blocker.close(); err.clear(); bool failure=block&&!ensureParentDirectoryForFile(tmp.path()+"/block/sub/test.wav",&err)&&!err.isEmpty();
 QString tx=buildTxRecordingPath(" ft4 ",false), tune=buildTxRecordingPath("",true); bool recording=tx.contains("/Decodium/recordings/decodium_tx_FT4_")&&tx.endsWith(".wav")&&tune.contains("/Decodium/recordings/decodium_tune_FT8_");
 QString wspr=buildWsprDecodeWavPath(123456,"12:34:00"); bool wav=wspr.contains("/wspr/123456/")&&wspr.endsWith("_1234.wav");
 QTextStream(stdout)<<"expansion="<<expansion<<" creation="<<creation<<" failure="<<failure<<" recording="<<recording<<" wspr="<<wav<<Qt::endl;
 return expansion&&creation&&failure&&recording&&wav ? 0 : 1;
}
'''
with tempfile.TemporaryDirectory(prefix='decodium-file-paths-ab-') as tmp:
    d = Path(tmp)
    (d / 'main.cpp').write_text(harness)
    results = []
    for label, text in [('before', old), ('after', new[-len(old):])]:
        (d / 'paths.inc').write_text(text)
        exe = d / label
        subprocess.run(['c++', '-std=c++17', '-I'+str(d), '-I'+str(root/'src/bridge'), str(d/'main.cpp'), str(root/'src/bridge/DecodeUtcTime.cpp'), *flags, '-o', str(exe)], check=True)
        result = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
        print(label, result.stdout.strip(), flush=True)
        results.append(result.stdout)
    assert results[0] == results[1]
