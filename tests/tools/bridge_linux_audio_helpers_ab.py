#!/usr/bin/env python3
"""Exercise extracted Linux audio helpers against a temporary pactl stub."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
fixture = (root / 'tests/bridge_refactor_fixtures/linux_audio_helpers_before.inc').read_text()
module = (root / 'src/bridge/DecodiumBridgeLinuxAudioHelpers.cpp').read_text()
assert module.endswith(fixture)
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
with tempfile.TemporaryDirectory(prefix='decodium-linux-audio-ab-') as temp:
    directory = Path(temp)
    pactl = directory / 'pactl'
    pactl.write_text('#!/bin/sh\ncase "$*" in\n  "list short sources") [ "$LAB_PACTL_EMPTY" = 1 ] || printf "1\\talsa_input.test\\tmodule\\n2\\talsa_output.test.monitor\\tmodule\\n";;\n  "list short sinks") [ "$LAB_PACTL_EMPTY" = 1 ] || printf "3\\talsa_output.test\\tmodule\\n";;\n  "set-default-source alsa_input.test"|"set-default-sink alsa_output.test") exit 0;;\n  "set-default-source denied"|"set-default-sink denied") printf "synthetic failure\\n" >&2; exit 1;;\n  "set-default-source slow") exec /bin/sleep 4;;\n  *) exit 1;;\nesac\n')
    pactl.chmod(0o755)
    env = os.environ.copy()
    env['PATH'] = str(directory) + os.pathsep + env.get('PATH', '')
    outputs = []
    for label, body in [('before', fixture), ('after', module[-len(fixture):])]:
        # Compile the Linux-only block on this host without changing Qt platform macros.
        (directory / 'linux_audio_helpers.inc').write_text(body.replace('#ifdef Q_OS_LINUX', '#if 1'))
        exe = directory / label
        subprocess.run(['c++', '-std=c++17', '-I' + str(directory), str(root / 'tests/bridge_refactor_linux_audio_harness.cpp'), *flags, '-o', str(exe)], check=True)
        result = subprocess.run([str(exe)], env=env, text=True, capture_output=True, check=True)
        outputs.append(result.stdout)
        print(label, result.stdout.strip(), flush=True)
    assert outputs[0] == outputs[1]
