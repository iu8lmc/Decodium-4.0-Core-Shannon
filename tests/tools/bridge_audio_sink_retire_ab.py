#!/usr/bin/env python3
"""Compare sink retirement using an actual BlackHole QAudioSink."""
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
fixture = (root / 'tests/bridge_refactor_fixtures/audio_sink_retire_before.inc').read_text()
module = (root / 'src/bridge/DecodiumBridgeAudioSinkRetire.cpp').read_text()
normal = module.replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(').replace('decodiumBridgeTxPrepareTimelineLog(', 'txTimelineLog(')
start = normal.index('static bool deferCoreAudioSinkDelete()')
assert normal[start:] == fixture
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Multimedia'], text=True))
harness = r'''
#include <QAudioSink>
#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QMediaDevices>
#include <QOperatingSystemVersion>
#include <QPointer>
#include <QTextStream>
#include <QTimer>
void bridgeLog(const QString&){}
void txTimelineLog(const QString&){}
QString audioStateToString(QAudio::State){return {};}
QString audioErrorToString(QAudio::Error){return {};}
#include "retire.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QAudioDevice device;for(const auto& d:QMediaDevices::audioOutputs())if(d.description()==QStringLiteral("BlackHole 2ch"))device=d;if(device.isNull())return 2;auto* orphan=new QBuffer;orphan->setData(QByteArray(100,'x'));orphan->open(QIODevice::ReadOnly);QPointer<QBuffer> orphanGuard(orphan);retireAudioSink(nullptr,orphan,QStringLiteral("null"));QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);bool nullBuffer=!orphanGuard;auto format=device.preferredFormat();auto* sink=new QAudioSink(device,format);auto* buffer=new QBuffer;buffer->setData(QByteArray(format.bytesForDuration(1000000),0));buffer->open(QIODevice::ReadOnly);QPointer<QAudioSink> sinkGuard(sink);QPointer<QBuffer> bufferGuard(buffer);sink->start(buffer);QEventLoop loop;QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec();bool started=sink->state()!=QAudio::StoppedState;bool deferred=deferCoreAudioSinkDelete();retireAudioSink(sink,buffer,QStringLiteral("test"));QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);bool correctSink=deferred?bool(sinkGuard):!sinkGuard;bool correctBuffer=!bufferGuard;bool muted=!deferred||!sinkGuard||sinkGuard->state()==QAudio::StoppedState||sinkGuard->volume()==0.0;bool pass=nullBuffer&&started&&correctSink&&correctBuffer&&muted;QTextStream(stdout)<<"nullBuffer="<<nullBuffer<<" started="<<started<<" deferred="<<deferred<<" sink="<<correctSink<<" buffer="<<correctBuffer<<" muted="<<muted<<Qt::endl;return pass?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-audio-retire-ab-') as tmp:
    directory = Path(tmp)
    (directory / 'main.cpp').write_text(harness)
    outputs = []
    for label, body in [('before', fixture), ('after', normal[start:])]:
        (directory / 'retire.inc').write_text(body)
        binary = directory / label
        subprocess.run(['c++', '-std=c++17', '-I' + str(directory), str(directory / 'main.cpp'), *flags, '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True, check=True, timeout=15)
        print(label, result.stdout.strip(), flush=True)
        outputs.append(result.stdout)
    assert outputs[0] == outputs[1]
