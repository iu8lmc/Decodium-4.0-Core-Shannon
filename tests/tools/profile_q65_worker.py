#!/usr/bin/env python3
"""Time actual Q65 worker calls in the continuity fixtures; restore test source.

No application, audio device or radio is opened. This measures strong synthetic
15A/60A/60B inputs, not crowded/weak signals or mode-switch cancellation.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import tempfile
import wave

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--runs', type=int, default=3)
p.add_argument('--wav', type=Path, help='Frozen mono 12 kHz 16-bit Q65-15A WAV')
p.add_argument('--allow-misses', action='store_true', help='Profile weak inputs without requiring the expected message; retain every output row')
p.add_argument('--contention', action='store_true', help='Measure a second caller waiting on the real runtime mutex during each decode')
p.add_argument('--coherent-average', action='store_true', help='Replay three distinct UTC slots with averaging enabled; requires --wav')
a = p.parse_args()
if a.coherent_average and not a.wav:
    p.error('--coherent-average requires --wav')
if a.runs < 1:
    p.error('runs must be positive')
if a.allow_misses and not a.wav:
    p.error('--allow-misses requires --wav')
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)
source = ROOT / 'tests/test_q65_decoder_continuity.cpp'
original = source.read_bytes()
needle = b'        worker.decode(request);'
assert original.count(needle) == 2
replacement = r'''        QStringList profileRows;
        auto profileConnection = QObject::connect(&worker, &decodium::q65::Q65DecodeWorker::decodeReady,
            [&profileRows](quint64, QStringList rows) { profileRows = rows; });
        QElapsedTimer profileTimer;
        profileTimer.start();
        worker.decode(request);
        double const profileMs = profileTimer.nsecsElapsed() / 1000000.0;
        QObject::disconnect(profileConnection);
        QJsonObject profileResult {{"serial", int(request.serial)}, {"period", request.ntrperiod},
            {"submode", request.nsubmode}, {"milliseconds", profileMs},
            {"rows", QJsonArray::fromStringList(profileRows)}};
        qInfo().noquote() << "[Q65-PROFILE] " + QJsonDocument(profileResult).toJson(QJsonDocument::Compact);'''.encode()
if a.contention:
    replacement = replacement.replace(b'        worker.decode(request);', r'''        std::atomic<bool> profileFinished {false};
        bool profileObservedBusy = false;
        double profileMutexWaitMs = 0;
        std::thread profileThread([&] {
            worker.decode(request);
            profileFinished.store(true);
        });
        auto& profileMutex = decodium::fortran::runtime_mutex();
        while (!profileFinished.load()) {
            if (profileMutex.tryLock()) {
                profileMutex.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                profileObservedBusy = true;
                QElapsedTimer profileWait;
                profileWait.start();
                profileMutex.lock();
                profileMutexWaitMs = profileWait.nsecsElapsed() / 1000000.0;
                profileMutex.unlock();
                break;
            }
        }
        profileThread.join();
        QVERIFY2(profileObservedBusy, "Did not observe an active decode holding the mutex");'''.encode())
    replacement = replacement.replace(b'{"rows", QJsonArray::fromStringList(profileRows)}',
        b'{"mutex_wait_ms", profileMutexWaitMs}, {"observed_busy", profileObservedBusy}, {"rows", QJsonArray::fromStringList(profileRows)}')
build = ['cmake', '--build', 'build', '--target', 'test_q65_decoder_continuity', '-j', '2']
events = []
instrumented = original.replace(needle, replacement)
fixture = None
if a.coherent_average:
    instrumented = instrumented.replace(b'request.coherentAvgEnabled = false;', b'request.coherentAvgEnabled = true;')
    instrumented = instrumented.replace(b'        request.serial = serial;', b'        request.serial = serial;\n        request.nutc = 120000 + 15 * (serial - 1);')
if a.wav:
    path = a.wav.resolve(strict=True)
    with wave.open(str(path), 'rb') as audio:
        assert (audio.getnchannels(), audio.getframerate(), audio.getsampwidth(), audio.getnframes()) == (1, 12000, 2, 180000)
        pcm = audio.readframes(audio.getnframes())
    raw = out / 'input.pcm'
    raw.write_bytes(pcm)
    fixture = dict(path=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    anchor = b'    QVector<short> const audio = makeQ65Window(message, periodSeconds, submode, 1500);'
    assert instrumented.count(anchor) == 1
    loader = ('    QFile profileInput(QString::fromUtf8(' + json.dumps(str(raw)) + '));\n'
              '    QVERIFY(profileInput.open(QIODevice::ReadOnly));\n'
              '    QByteArray const profilePcm = profileInput.readAll();\n'
              '    QVector<short> audio(profilePcm.size() / 2);\n'
              '    for (qsizetype i = 0; i < audio.size(); ++i) audio[i] = qFromLittleEndian<qint16>(profilePcm.constData() + i * 2);')
    instrumented = instrumented.replace(anchor, loader.encode())
    if a.allow_misses:
        instrumented = instrumented.replace(b'containsMessage(resultRows.at(index), message)',
            b'true /* Profiling only: missing messages are retained in profileRows. */')
try:
    source.write_bytes(b'#include <QElapsedTimer>\n#include <QJsonObject>\n#include <QJsonArray>\n#include <QJsonDocument>\n#include <QFile>\n#include <QtEndian>\n#include <atomic>\n#include <thread>\n#include <chrono>\n#include "Detector/FortranRuntimeGuard.hpp"\n' + instrumented)
    with (out / 'build.log').open('w') as log:
        subprocess.run(build, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen')
    binary = ROOT / 'build/tests/test_q65_decoder_continuity'
    binary_sha = hashlib.sha256(binary.read_bytes()).hexdigest()
    decoder_sources = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                       for name in ('Detector/FtxQ65Decoder.cpp', 'Detector/Q65DecodeWorker.cpp', 'Detector/Q65DecodeWorker.hpp')}
    for run in range(1, a.runs + 1):
        with tempfile.TemporaryDirectory(prefix='decodium-q65-profile-') as work:
            command = [str(binary)]
            if a.wav:
                command.append('decodesThreeConsecutiveFullWindowsWithoutCoherentAveraging')
            result = subprocess.run(command, cwd=work, env=env, capture_output=True, text=True, timeout=180)
        text = result.stdout + result.stderr
        (out / f'run-{run}.log').write_text(text)
        if result.returncode:
            raise RuntimeError(f'Q65 continuity test failed in run {run}')
        rows = [json.loads(line.split('[Q65-PROFILE] ', 1)[1]) for line in text.splitlines() if '[Q65-PROFILE] ' in line]
        assert len(rows) == (3 if a.wav else 5), rows
        events.extend(dict(run=run, **row) for row in rows)
finally:
    source.write_bytes(original)
    with (out / 'restore-build.log').open('w') as log:
        subprocess.run(build, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
assert source.read_bytes() == original
report = dict(coherent_average=a.coherent_average, machine=platform.platform(), logical_cpus=os.cpu_count(), events=events,
              instrumented_binary_sha256=binary_sha, decoder_sources=decoder_sources,
              source_sha256=hashlib.sha256(original).hexdigest(), source_restored=True,
              fixture=fixture, expected_message_required=not a.allow_misses, contention=a.contention,
              scope='Q65 native worker calls; not mode-switch, RF, or old-hardware evidence')
(out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
