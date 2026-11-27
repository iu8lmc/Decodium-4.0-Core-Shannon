#!/usr/bin/env python3
"""Compare fresh and reused map database startup in an isolated test rig."""
import argparse
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import sqlite3
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--binary', type=Path, default=root / 'build-baseline/decodium')
parser.add_argument('--schema-out', type=Path)
parser.add_argument('--synthetic-qsos', type=int, default=0)
args = parser.parse_args()
if not 0 <= args.synthetic_qsos <= 500000:
    raise ValueError('synthetic QSO count outside laboratory limit')
rig = 'MapStartup' + secrets.token_hex(6)
test_root = Path.home() / '.qttest'
app_name = 'Decodium - ' + rig + ' - test'
data_dir = test_root / 'share' / 'IU8LMC' / app_name
cache_dir = test_root / 'cache' / 'IU8LMC' / app_name
database = data_dir / 'map_intelligence.sqlite'
if data_dir.exists() or cache_dir.exists():
    raise RuntimeError('test rig path unexpectedly exists')

try:
    with tempfile.TemporaryDirectory(prefix='decodium-map-startup-') as tmp:
        env = os.environ.copy()
        env.update(DECODIUM_LAB_SETTINGS_DIR=tmp, DECODIUM_STARTUP_PROFILE='1',
                   DECODIUM_FORCE_STANDALONE_UDP='1', DECODIUM_DISABLE_QML_CACHE='1',
                   QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software')
        cmd = [str(args.binary.resolve()), '--test-mode', '--rig-name', rig,
               '--lab-no-cat', '--lab-no-monitor', '--lab-callsign', 'IU8LMC',
               '--lab-mode', 'FT8', '--lab-quit-ms', '15000']
        observations = []
        for attempt in range(2):
            existed = database.is_file()
            run = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=70)
            output = run.stdout + run.stderr
            match = re.search(r'\[STARTUP-PROFILE\] map-intelligence ms=\s*(\d+)', output)
            locations = re.findall(r'Qt AppLocalDataLocation: (.+)', output)
            if run.returncode or not match or not locations or Path(locations[-1].strip()) != data_dir:
                raise RuntimeError('startup failed or used another data path: ' + output[-5000:])
            if existed != bool(attempt) or not database.is_file():
                raise RuntimeError('map database creation/reuse did not match expectation')
            observation = {'databaseExisted': existed, 'mapMs': int(match.group(1)),
                           'databaseBytes': database.stat().st_size}
            if attempt == 0 and args.synthetic_qsos:
                with sqlite3.connect(database) as db:
                    db.executemany('INSERT INTO map_qso(source_key,call,grid,grid4,grid6,band,mode) VALUES(?,?,?,?,?,?,?)',
                                   ((f'synthetic-{i}', 'K1ABC', 'FN42AA', 'FN42', 'FN42AA', '20m', 'FT8')
                                    for i in range(args.synthetic_qsos)))
                    db.execute("INSERT INTO map_qso_grid(qso_id,grid,grid4,grid6,is_primary) SELECT id,grid,grid4,grid6,1 FROM map_qso")
            elif attempt == 1 and args.synthetic_qsos:
                with sqlite3.connect(database) as db:
                    counts = [db.execute('SELECT count(*) FROM '+table).fetchone()[0]
                              for table in ('map_qso', 'map_qso_grid')]
                    invalid = db.execute('''SELECT count(*) FROM map_qso q LEFT JOIN map_qso_grid g ON g.qso_id=q.id
                        WHERE q.source_key<>('synthetic-' || (q.id-1)) OR q.call<>'K1ABC'
                        OR q.grid<>'FN42AA' OR q.grid4<>'FN42' OR q.grid6<>'FN42AA'
                        OR g.qso_id IS NULL OR g.grid<>'FN42AA' OR g.grid4<>'FN42'
                        OR g.grid6<>'FN42AA' OR g.is_primary<>1''').fetchone()[0]
                if counts != [args.synthetic_qsos, args.synthetic_qsos]:
                    raise RuntimeError('synthetic records changed on reopen: '+repr(counts))
                if invalid:
                    raise RuntimeError('synthetic record values changed on reopen: '+str(invalid))
                observation['qsoCount'] = counts[0]
                observation['gridCount'] = counts[1]
                observation['invalidRows'] = invalid
            observations.append(observation)
        if args.schema_out:
            with sqlite3.connect(database) as db:
                schema = db.execute('SELECT type, name, tbl_name, sql FROM sqlite_master ORDER BY type, name').fetchall()
            args.schema_out.write_text(json.dumps(schema, indent=2) + '\n')
        print(json.dumps(observations))
finally:
    for path in (data_dir, cache_dir):
        if path.is_dir():
            shutil.rmtree(path)
