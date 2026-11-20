#!/usr/bin/env python3
"""Watch the authorised Windows builds and collect their exact portable artifacts.

Never starts, cancels, or retries a workflow. Transient observation failures do
not mean a remote job stopped. Safe to resume collection of an existing run.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

REPO = 'elisir80/Decodium-4.0-Core-Shannon'
ARTIFACT = 'decodium-windows-x64-portable'


def gh_json(arguments):
    result = subprocess.run(['gh', *arguments], check=True, capture_output=True, text=True, timeout=60)
    return json.loads(result.stdout)


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def collect(role, run, output, specification):
    run_id, expected = specification
    if run['headSha'] != expected:
        raise RuntimeError(f'Unexpected {role} source commit: {run["headSha"]}')
    if run['conclusion'] != 'success':
        logs = subprocess.run(['gh', 'run', 'view', str(run_id), '--repo', REPO, '--log-failed'],
                              capture_output=True, text=True, timeout=60)
        (output / f'{role}-failure.log').write_text(logs.stdout + logs.stderr)
        raise RuntimeError(f'{role} workflow concluded {run["conclusion"]}; see failure log')
    metadata = gh_json(['api', f'repos/{REPO}/actions/runs/{run_id}/artifacts'])
    matches = [a for a in metadata['artifacts'] if a['name'] == ARTIFACT and not a['expired']]
    if len(matches) != 1 or matches[0]['size_in_bytes'] <= 0:
        raise RuntimeError(f'Missing, empty or ambiguous {role} portable artifact')
    artifact = matches[0]
    destination = output / role
    marker = output / f'{role}-download.json'
    if destination.exists():
        saved = json.loads(marker.read_text())
        if saved['artifact']['id'] != artifact['id'] or digest(destination / 'decodium.exe') != saved['executableSha256']:
            raise RuntimeError(f'Existing {role} download does not match the expected artifact')
        return saved
    temporary = Path(tempfile.mkdtemp(prefix=f'{role}-download-', dir=output))
    try:
        subprocess.run(['gh', 'run', 'download', str(run_id), '--repo', REPO, '--name', ARTIFACT,
                        '--dir', str(temporary)], check=True, timeout=600)
        for name in ('decodium.exe', 'qml/decodium/Main.qml'):
            if not (temporary / name).is_file() or not (temporary / name).stat().st_size:
                raise RuntimeError(f'Missing {role} runtime file: {name}')
        if not list(temporary.glob('*.dll')):
            raise RuntimeError(f'Missing {role} Windows runtime DLLs')
        saved = dict(runId=run_id, url=run['url'], headSha=expected, conclusion=run['conclusion'],
                     artifact={key: artifact.get(key) for key in ('id', 'name', 'digest', 'size_in_bytes', 'created_at')},
                     executableSha256=digest(temporary / 'decodium.exe'))
        temporary.rename(destination)
        marker.write_text(json.dumps(saved, indent=2) + '\n')
        return saved
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)  # only this invocation's private partial download


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--runs-json', type=Path, required=True)
    args = p.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    specifications = json.loads(args.runs_json.read_text())
    builds = {role: (specifications[role]['runId'], specifications[role]['headSha']) for role in ('before','after')}
    evidence = {}
    while len(evidence) < len(builds):
        for role, (run_id, expected) in builds.items():
            if role in evidence:
                continue
            try:
                run = gh_json(['run', 'view', str(run_id), '--repo', REPO,
                               '--json', 'status,conclusion,headSha,url,jobs'])
            except (subprocess.SubprocessError, json.JSONDecodeError) as error:
                print(json.dumps(dict(role=role, runId=run_id, observation_error=str(error))), flush=True)
                continue
            if run['headSha'] != expected:
                raise RuntimeError(f'{role} run source does not match')
            (output / f'{role}-run.json').write_text(json.dumps(run, indent=2) + '\n')
            active = [s['name'] for j in run['jobs'] for s in j.get('steps', []) if s['status'] == 'in_progress']
            print(json.dumps(dict(role=role, runId=run_id, status=run['status'], conclusion=run['conclusion'], active=active)), flush=True)
            if run['status'] == 'completed':
                evidence[role] = collect(role, run, output, builds[role])
                (output / 'ci-evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
                print(json.dumps(dict(role=role, downloaded=True, executableSha256=evidence[role]['executableSha256'])), flush=True)
        if len(evidence) < len(builds):
            time.sleep(30)
    print(json.dumps(dict(complete=True, evidence=str(output / 'ci-evidence.json'))), flush=True)


if __name__ == '__main__':
    main()
