#!/usr/bin/env python3
"""macOS-only A/B identity test using two full Decodium test-mode processes."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile


def keychains(paths):
    security = ctypes.CDLL('/System/Library/Frameworks/Security.framework/Security')
    core = ctypes.CDLL('/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation')
    ref = ctypes.c_void_p
    security.SecKeychainCreate.argtypes = [ctypes.c_char_p, ctypes.c_uint32,
        ctypes.c_void_p, ctypes.c_bool, ctypes.c_void_p, ctypes.POINTER(ref)]
    security.SecKeychainCreate.restype = ctypes.c_int32
    security.SecKeychainUnlock.argtypes = [ref, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_bool]
    security.SecKeychainUnlock.restype = ctypes.c_int32
    security.SecKeychainDelete.argtypes = [ref]
    security.SecKeychainDelete.restype = ctypes.c_int32
    core.CFRelease.argtypes = [ref]
    security.SecKeychainSetUserInteractionAllowed(False)
    password = b'temporary-test-keychain'
    refs = []
    try:
        for path in paths:
            handle = ref()
            result = security.SecKeychainCreate(os.fsencode(path), len(password),
                password, False, None, ctypes.byref(handle))
            if result or not handle:
                raise RuntimeError('test keychain create failed: {}'.format(result))
            refs.append(handle)
            result = security.SecKeychainUnlock(handle, len(password), password, True)
            if result:
                raise RuntimeError('test keychain unlock failed: {}'.format(result))
        return security, core, refs
    except Exception:
        for handle in refs:
            security.SecKeychainDelete(handle)
            core.CFRelease(handle)
        raise


def run_pair(binary, root, suffix, iteration):
    children = []
    try:
        for label in ('a', 'b'):
            env = os.environ.copy()
            env['QT_QUICK_BACKEND'] = 'software'
            env['DECODIUM_LAB_SETTINGS_DIR'] = str(root / ('settings-' + label))
            args = [str(binary), '--test-mode', '--rig-name', 'E2EE' + suffix + label,
                '--lab-callsign', 'LAB' + label.upper(), '--lab-no-cat', '--lab-no-monitor',
                '--lab-share-keychain', str(root / (label + '.keychain-db')),
                '--lab-share-identity-file', str(root / (label + '-' + str(iteration) + '.json'))]
            log = open(root / (label + '-' + str(iteration) + '.log'), 'wb')
            try:
                process = subprocess.Popen(args, env=env, stdout=log, stderr=subprocess.STDOUT)
            finally:
                log.close()
            children.append(process)
        for process in children:
            if process.wait(timeout=65) != 0:
                raise RuntimeError('Decodium identity actor failed')
    finally:
        for process in children:
            if process.poll() is None:
                process.kill()
                process.wait()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('decodium', type=Path, help='absolute path to the built Decodium executable')
    args = parser.parse_args()
    binary = args.decodium.resolve(strict=True)
    suffix = secrets.token_hex(4)
    with tempfile.TemporaryDirectory(prefix='decodium-share-app-') as temp:
        root = Path(temp)
        security, core, refs = keychains([root / 'a.keychain-db', root / 'b.keychain-db'])
        try:
            gate = subprocess.run([str(binary), '--lab-share-keychain', str(root / 'a.keychain-db'),
                '--lab-share-identity-file', str(root / 'forbidden.json')],
                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            if gate.returncode != 2 or (root / 'forbidden.json').exists():
                raise RuntimeError('identity lab was not rejected outside test mode')
            invalid = subprocess.run([str(binary), '--test-mode',
                '--lab-share-keychain=' + str(root / 'missing.keychain-db'),
                '--lab-share-identity-file=' + str(root / 'forbidden.json')],
                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            if invalid.returncode != 2 or (root / 'forbidden.json').exists():
                raise RuntimeError('identity lab accepted a missing keychain')
            run_pair(binary, root, suffix, 1)
            run_pair(binary, root, suffix, 2)
            def identity(label, iteration):
                value = json.loads((root / (label + '-' + str(iteration) + '.json')).read_text())
                if set(value) != {'publicKey', 'fingerprint'} or any(
                    len(value[key]) != 64 for key in value):
                    raise RuntimeError('invalid public identity result')
                if hashlib.sha256(bytes.fromhex(value['publicKey'])).hexdigest() != value['fingerprint']:
                    raise RuntimeError('identity fingerprint does not match the public key')
                return value
            a1, b1 = identity('a', 1), identity('b', 1)
            if a1 == b1 or a1 != identity('a', 2) or b1 != identity('b', 2):
                raise RuntimeError('identities were not distinct and stable')
            print('PASS: two Decodium test profiles use distinct, persistent identities; production gate rejects lab options')
        finally:
            for handle in refs:
                security.SecKeychainDelete(handle)
                core.CFRelease(handle)


if __name__ == '__main__':
    main()
