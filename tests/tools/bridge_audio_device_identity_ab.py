#!/usr/bin/env python3
"""Compare the original and extracted audio identity logic with controlled IDs."""
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
fixture = (root / 'tests/bridge_refactor_fixtures/audio_device_identity_before.inc').read_text()
module = (root / 'src/bridge/DecodiumBridgeAudioDeviceIdentity.cpp').read_text()
start = module.index('static QString audioDeviceIdForLog(')
end = module.index('QString decodiumBridgeAudioDeviceIdForLog(', start)
assert module[start:end].strip() == fixture.strip()
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Multimedia'], text=True))
harness = r'''
#include <QAudioFormat>
#include <QByteArray>
#include <QString>
#include <QTextStream>
class FakeAudioDevice {
public:
    explicit FakeAudioDevice(QByteArray value = {}) : value_(value) {}
    QByteArray id() const { return value_; }
private:
    QByteArray value_;
};
#include "identity.inc"
int main() {
    FakeAudioDevice empty, device(QByteArray::fromHex("00ab12ff"));
    QAudioFormat format;
    format.setSampleRate(48000); format.setChannelCount(2); format.setSampleFormat(QAudioFormat::Int16);
    bool pass = audioDeviceIdForLog(empty) == "<empty>"
        && audioDeviceIdForSettings(empty).isEmpty()
        && audioDeviceIdForLog(device) == "00ab12ff"
        && audioDeviceIdForSettings(device) == "00ab12ff"
        && audioDeviceIdSettingForLog("  ") == "<unset>"
        && audioDeviceIdSettingForLog("  00AB12FF  ") == "00AB12FF"
        && audioDeviceIdMatches(device, " 00AB12FF ")
        && !audioDeviceIdMatches(device, "00ab12fe")
        && !audioDeviceIdMatches(device, " ")
        && !audioDeviceIdMatches(empty, " ")
        && audioFormatForLog(format) == QStringLiteral("rate=48000 channels=2 sampleFormat=%1").arg(int(QAudioFormat::Int16));
    QTextStream(stdout) << "identity_cases=" << pass << " format=" << audioFormatForLog(format) << Qt::endl;
    return pass ? 0 : 1;
}
'''
with tempfile.TemporaryDirectory(prefix='decodium-audio-identity-ab-') as temp:
    directory = Path(temp)
    (directory / 'main.cpp').write_text(harness)
    outputs = []
    for label, body in [('before', fixture), ('after', module[start:end])]:
        (directory / 'identity.inc').write_text(body.replace('QAudioDevice', 'FakeAudioDevice'))
        exe = directory / label
        subprocess.run(['c++', '-std=c++17', '-I' + str(directory), str(directory / 'main.cpp'), *flags, '-o', str(exe)], check=True)
        result = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
        print(label, result.stdout.strip(), flush=True)
        outputs.append(result.stdout)
    assert outputs[0] == outputs[1]
