#!/usr/bin/env python3
"""Explore equivalent-looking SQLite grid backfill queries on synthetic data."""
import sqlite3
import time

db = sqlite3.connect(':memory:')
db.executescript('''
CREATE TABLE map_qso(id INTEGER PRIMARY KEY, grid TEXT, grid4 TEXT, grid6 TEXT);
CREATE TABLE map_qso_grid(qso_id INTEGER, grid TEXT, grid4 TEXT, grid6 TEXT,
                          is_primary INTEGER, PRIMARY KEY(qso_id,grid));
''')
db.executemany('INSERT INTO map_qso VALUES(?,?,?,?)',
               ((i, 'JN70AA', 'JN70', 'JN70AA') for i in range(1, 100001)))
old = '''INSERT OR IGNORE INTO map_qso_grid(qso_id,grid,grid4,grid6,is_primary)
         SELECT id,upper(grid),upper(grid4),nullif(upper(grid6),''),1
         FROM map_qso WHERE coalesce(grid4,'')<>'' '''
candidate = old + '''AND NOT EXISTS (SELECT 1 FROM map_qso_grid g
                     WHERE g.qso_id=map_qso.id AND g.grid=upper(map_qso.grid))'''
# Compare fresh, incomplete and already populated states, including nulls and
# a pre-existing entry whose attributes differ from the current QSO.
for state in ('fresh', 'partial', 'complete'):
    left = sqlite3.connect(':memory:')
    left.executescript('''CREATE TABLE map_qso(id INTEGER PRIMARY KEY, grid TEXT, grid4 TEXT, grid6 TEXT);
CREATE TABLE map_qso_grid(qso_id INTEGER, grid TEXT NOT NULL, grid4 TEXT NOT NULL,
                          grid6 TEXT, is_primary INTEGER, PRIMARY KEY(qso_id,grid));''')
    left.executemany('INSERT INTO map_qso VALUES(?,?,?,?)', [
        (1, 'jn70aa', 'jn70', 'jn70aa'), (2, 'FN42', 'FN42', ''),
        (3, None, 'JN70', None), (4, 'JN70', '', None),
        (5, 'IO91aa', 'IO91', None), (6, 'JN70', None, None)])
    if state != 'fresh':
        left.execute("INSERT INTO map_qso_grid VALUES(1,'JN70AA','ZZ99',NULL,0)")
    if state == 'complete':
        left.execute("INSERT INTO map_qso_grid VALUES(2,'FN42','FN42',NULL,1)")
        left.execute("INSERT INTO map_qso_grid VALUES(5,'IO91AA','IO91',NULL,1)")
    left.commit()
    right = sqlite3.connect(':memory:')
    left.backup(right)
    left.execute(old)
    right.execute(candidate)
    actual = left.execute('SELECT * FROM map_qso_grid ORDER BY qso_id,grid').fetchall()
    assert actual == right.execute('SELECT * FROM map_qso_grid ORDER BY qso_id,grid').fetchall(), state
    left.close()
    right.close()
db.execute(old)
for label, sql in [('old', old), ('candidate', candidate), ('candidate', candidate), ('old', old)]:
    start = time.process_time()
    db.execute(sql)
    print(label, round((time.process_time() - start) * 1000, 2), 'ms')
print('rows', db.execute('SELECT count(*) FROM map_qso_grid').fetchone()[0])
