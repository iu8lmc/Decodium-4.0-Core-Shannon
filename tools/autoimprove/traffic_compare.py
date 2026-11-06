#!/usr/bin/env python3
"""Real-traffic A/B: slice a recording into FT2 slots, decode twice (two environments) and
list the messages found only by one side.  There is no truth here, so the check is
plausibility: an extra message that is a well-formed call/grid/report, and that repeats in
neighbouring slots, is a station; an isolated odd one is a false decode.

  python traffic_compare.py --build build_689 --rec <wav 12 kHz mono> --start 600 --slots 400 \
      --env-a DECODIUM_FT2_ALIAS=0 --env-b DECODIUM_FT2_ALIAS=2
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
import wave

import numpy as np

SLOT = 45000


def slice_slots(rec, start_s, n, out):
    os.makedirs(out, exist_ok=True)
    with wave.open(rec, 'rb') as w:
        assert w.getframerate() == 12000 and w.getnchannels() == 1 and w.getsampwidth() == 2
        w.setpos(int(start_s * 12000))
        names = []
        for i in range(n):
            data = w.readframes(SLOT)
            if len(data) < SLOT * 2:
                break
            name = 'slot_%05d.wav' % i
            with wave.open(os.path.join(out, name), 'wb') as o:
                o.setnchannels(1); o.setsampwidth(2); o.setframerate(12000)
                o.writeframes(data)
            names.append(name)
    return names


def decode(build, d, names, extra):
    exe = os.path.join(build, 'tests', 'ft2_stage_compare.exe')
    e = dict(os.environ)
    e['PATH'] = r'C:\msys64\mingw64\bin;' + e.get('PATH', '')
    e['DECODIUM_FT2_AP_MSG'] = '0'
    e.update(extra)
    res = {}
    for i in range(0, len(names), 60):
        p = subprocess.run([exe, '--stages', '7', '--ndepth', '3', '--nfa', '300', '--nfb', '2800',
                            '--mycall', 'IU8LMC'] + names[i:i + 60], cwd=d, env=e, capture_output=True,
                           text=True, encoding='utf-8', errors='replace')
        cur = None
        for line in p.stdout.splitlines():
            m = re.match(r'^file: (.*)$', line.strip())
            if m:
                cur = os.path.basename(m.group(1).strip()); res[cur] = []
                continue
            m = re.search(r'decoded="([^"]*)"', line)
            if m and cur:
                res[cur].append(' '.join(m.group(1).split()))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', required=True)
    ap.add_argument('--rec', required=True)
    ap.add_argument('--start', type=float, default=600.0)
    ap.add_argument('--slots', type=int, default=400)
    ap.add_argument('--env-a', default='DECODIUM_FT2_ALIAS=0')
    ap.add_argument('--env-b', default='DECODIUM_FT2_ALIAS=2')
    a = ap.parse_args()
    d = os.path.join(tempfile.gettempdir(), 'decodium_autoimprove', 'traffic')
    names = slice_slots(a.rec, a.start, a.slots, d)
    ea = dict([a.env_a.split('=', 1)]); eb = dict([a.env_b.split('=', 1)])
    ra, rb = decode(os.path.abspath(a.build), d, names, ea), decode(os.path.abspath(a.build), d, names, eb)
    ca = sum(len(v) for v in ra.values()); cb = sum(len(v) for v in rb.values())
    only_b = [(n, m) for n in names for m in rb.get(n, []) if m not in ra.get(n, [])]
    only_a = [(n, m) for n in names for m in ra.get(n, []) if m not in rb.get(n, [])]
    print('slots %d  lines A %d  lines B %d  only-B %d  only-A %d' % (len(names), ca, cb, len(only_b), len(only_a)))
    allb = {}
    for n in names:
        for m in rb.get(n, []):
            allb.setdefault(m, []).append(n)
    print('--- only in B (count of slots where the same text appears in B):')
    for n, m in only_b:
        print('  %s  %-32s  seen in %d slot(s)' % (n, m, len(allb[m])))
    print('--- only in A:')
    for n, m in only_a:
        print('  %s  %s' % (n, m))


if __name__ == '__main__':
    main()
