#!/usr/bin/env python3
"""Package the Windows A/B harness and frozen corpus; never invent Windows builds."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import os

ROOT = Path(__file__).resolve().parents[2]
BASE = '3529dae3e218ef978b77347795572d96a39e9a55'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--before', type=Path)
    p.add_argument('--after', type=Path)
    p.add_argument('--baseline-ref', help='Original app with common laboratory isolation only')
    p.add_argument('--candidate-ref', help='Pin the source patch to the actual candidate build commit')
    p.add_argument('--ci-evidence', type=Path, help='Verified before/after workflow metadata JSON')
    p.add_argument('--extended-modes', action='store_true', help='Include verified Q65-15A and FST4-60 fixtures')
    p.add_argument('--ft-modes-only', action='store_true', help='Focus on FT8/FT4/FT2 with clean and noisy FT2 fixtures')
    args = p.parse_args()
    if args.extended_modes and args.ft_modes_only:
        p.error('Choose extended modes or FT modes only')
    if bool(args.before) != bool(args.after):
        p.error('Provide both Windows portable bundles, or neither for a staging kit')
    if args.before and (not args.baseline_ref or not args.candidate_ref or not args.ci_evidence):
        p.error('Executable kits require a pinned candidate ref and verified CI evidence')
    baseline_commit = (subprocess.check_output(['git', 'rev-parse', args.baseline_ref + '^{commit}'],
                       cwd=ROOT, text=True).strip() if args.baseline_ref else BASE)
    candidate_commit = (subprocess.check_output(['git', 'rev-parse', args.candidate_ref + '^{commit}'],
                        cwd=ROOT, text=True).strip() if args.candidate_ref else None)
    evidence = json.loads(args.ci_evidence.read_text()) if args.ci_evidence else None
    if evidence:
        for version, expected in [('before', baseline_commit), ('after', candidate_commit)]:
            if evidence[version]['headSha'] != expected or evidence[version]['conclusion'] != 'success':
                p.error('CI evidence must match successful builds of the exact source commits')
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = ROOT / 'tests/tools/windows_performance'
    for path in source.iterdir():
        if path.is_file():
            shutil.copy2(path, out / path.name)
    fixtures = []
    inputs = [
        ('clean-ft8', 'FT8', ROOT / 'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav', 'CQ K1ABC FN42'),
        ('busy-ft8', 'FT8', ROOT / 'build/performance-baseline/ft8-corpus/busy-902.wav', None),
        ('weak-ft8', 'FT8', ROOT / 'build/performance-baseline/ft8-corpus/mixed_weak-902.wav', None),
        ('clean-ft4', 'FT4', ROOT / 'build/performance-baseline/ft4-cycle-9h1sr.wav', 'CQ 9H1SR JM75'),
        ('reference-wspr', 'WSPR', ROOT / 'build/performance-baseline/wspr-reference-150426_0918.wav', 'ND6P DM04 30'),
    ]
    if args.ft_modes_only:
        inputs = [item for item in inputs if item[1] != 'WSPR'] + [
            ('clean-ft2', 'FT2', ROOT / 'build/performance-baseline/ft2-cycle-k1abc.wav', 'CQ K1ABC FN42'),
            ('noisy-ft2', 'FT2', ROOT / 'build/performance-baseline/ft2-noisy-k1abc.wav', 'CQ K1ABC FN42'),
        ]
        readme = out / 'LEGGIMI.txt'
        readme.write_text(readme.read_text().replace('6-10 minuti', '8-12 minuti') +
                         '\nKit FT8/FT4/FT2: eseguibile su qualsiasi PC Windows disponibile.\n'
                         'Il report registra CPU e RAM effettive; non richiede un i5.\n')
    if args.extended_modes:
        inputs += [
            ('reference-fst4', 'FST4-60', ROOT / 'build/performance-baseline/fst4-native-profile/210115_0058.wav', 'CQ K9KFR EN71'),
            ('clean-q65', 'Q65-15A', ROOT / 'build/performance-baseline/q65-weak-corpus/-15/000000_000000.wav', 'K1ABC W9XYZ EN37'),
        ]
        readme = out / 'LEGGIMI.txt'
        readme.write_text(readme.read_text().replace('6-10 minuti', '10-15 minuti') +
                         '\nKit esteso: include anche FST4-60 e Q65-15A. Non sostituisce il primo ZIP.\n')
    (out / 'fixtures').mkdir()
    for label, mode, path, expected in inputs:
        target = out / 'fixtures' / (label + '.wav')
        shutil.copy2(path, target)
        fixtures.append(dict(id=label, mode=mode, file=target.relative_to(out).as_posix(),
                             sha256=sha(target), expected=expected,
                             source=('https://downloads.sourceforge.net/project/wsjt/samples/WSPR/150426_0918.wav'
                                     if mode == 'WSPR' else 'Local frozen test fixture')))
    # A temporary Git index captures tracked and new source files without touching
    # the user's index, making commits, or including build artifacts.
    if candidate_commit:
        patch = subprocess.check_output(['git', 'diff', '--binary', baseline_commit, candidate_commit], cwd=ROOT)
    else:
        with tempfile.TemporaryDirectory(prefix='decodium-kit-index-') as temporary:
            env = dict(os.environ, GIT_INDEX_FILE=str(Path(temporary) / 'index'))
            subprocess.run(['git', 'read-tree', 'HEAD'], cwd=ROOT, env=env, check=True)
            paths = subprocess.check_output(['git', 'ls-files', '-m', '-o', '--exclude-standard', '-z'], cwd=ROOT).split(b'\0')
            paths = [os.fsdecode(path) for path in paths if path]
            if paths:
                subprocess.run(['git', 'add', '--', *paths], cwd=ROOT, env=env, check=True)
            patch = subprocess.check_output(['git', 'diff', '--cached', '--binary', BASE], cwd=ROOT, env=env)
    (out / 'candidate.patch').write_bytes(patch)
    lab_patch = subprocess.check_output(['git', 'diff', '--binary', BASE, baseline_commit], cwd=ROOT)
    (out / 'lab-isolation.patch').write_bytes(lab_patch)
    if args.baseline_ref:
        allowed = {'src/app/main_qml.cpp', 'src/app/DecodiumLabStorage.hpp',
                   'tests/test_lab_storage_isolation.cpp', 'tests/CMakeLists.txt',
                   '.github/workflows/build-windows-x64.yml'}
        changed = set(subprocess.check_output(['git', 'diff', '--name-only', BASE, baseline_commit],
                      cwd=ROOT, text=True).splitlines())
        if not changed <= allowed:
            p.error('Baseline differs from original outside the common laboratory support')
        if candidate_commit:
            for name in ('src/app/main_qml.cpp', 'src/app/DecodiumLabStorage.hpp', 'tests/test_lab_storage_isolation.cpp'):
                if subprocess.check_output(['git','show',baseline_commit+':'+name],cwd=ROOT) != subprocess.check_output(['git','show',candidate_commit+':'+name],cwd=ROOT):
                    p.error('Common laboratory instrumentation differs: '+name)
    builds = {}
    for name, directory in [('before', args.before), ('after', args.after)]:
        target = out / 'versions' / name
        if directory:
            directory = directory.resolve(strict=True)
            for required in ('decodium.exe', 'qml/decodium/Main.qml'):
                if not (directory / required).is_file():
                    p.error(f'Missing {required} in {directory}')
            if not list(directory.glob('*.dll')):
                p.error(f'No runtime DLLs in {directory}')
            if evidence and sha(directory / 'decodium.exe') != evidence[name]['executableSha256']:
                p.error(f'Windows executable does not match the downloaded {name} artifact')
            shutil.copytree(directory, target)
            files = {f.relative_to(target).as_posix(): sha(f) for f in sorted(target.rglob('*')) if f.is_file()}
            builds[name] = dict(sha256=sha(target / 'decodium.exe'), files=files)
        else:
            target.mkdir(parents=True)
            builds[name] = dict(sha256=None, files={})
    if args.before and builds['before']['sha256'] == builds['after']['sha256']:
        p.error('The before and after Windows executables must not be identical')
    manifest = dict(schema=1, extended_modes=args.extended_modes, ft_modes_only=args.ft_modes_only, ready=bool(args.before and args.after), baseline_origin=BASE, baseline_commit=baseline_commit,
                    lab_isolation_patch_sha256=sha(out / 'lab-isolation.patch'),
                    candidate_commit=candidate_commit, ci_evidence=evidence,
                    candidate_patch_sha256=sha(out / 'candidate.patch'), builds=builds, fixtures=fixtures,
                    target='Windows 11 / third-generation Intel i5 / 8 GiB RAM',
                    validation='Windows execution pending; see worklog for local QML and parser checks')
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    if manifest['ready']:
        readme = out / 'LEGGIMI.txt'
        text = readme.read_text()
        start = text.index('STATO:')
        end = text.index('\nQuando le due build verificate sono incluse:')
        text = (text[:start] + 'STATO: entrambe le build Windows verificate sono incluse.\n'
                'Commit, workflow e SHA-256 sono registrati in manifest.json.\n'
                'Il test sul PC di riferimento deve ancora essere eseguito.\n'
                + text[end:])
        readme.write_text(text.replace('Quando le due build verificate sono incluse:', 'COME ESEGUIRE IL TEST:'))
    archive = shutil.make_archive(str(out), 'zip', out.parent, out.name)
    print(json.dumps(dict(archive=archive, ready=manifest['ready'], sha256=sha(Path(archive)))))

if __name__ == '__main__':
    main()
