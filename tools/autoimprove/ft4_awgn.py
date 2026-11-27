#!/usr/bin/env python3
"""FT4 on white noise, known truth: decoded count per level for the current environment.

  DECODIUM_FT4_SYNC2D=1 python ft4_awgn.py --build build_689 [--trials 20] [--levels -22:-12:2]

Slots come from ft4_make_test_wav (--snr-db adds AWGN, --seed varies the noise). One decoder process
per slot (ft4_stage_compare takes one wav). The count is what matters: run once per environment.
"""
import argparse
import concurrent.futures as cf
import os
import re
import subprocess
import tempfile


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', required=True)
    ap.add_argument('--trials', type=int, default=20)
    ap.add_argument('--levels', default='-24:-14:2', help='start:stop:step, dB (ft4_make_test_wav convention)')
    ap.add_argument('--freq', type=float, default=1200.0)
    a = ap.parse_args()
    build = os.path.abspath(a.build)
    gen = os.path.join(build, 'tests', 'ft4_make_test_wav.exe')
    dec = os.path.join(build, 'tests', 'ft4_stage_compare.exe')
    env = dict(os.environ)
    env['PATH'] = 'C:\\msys64\\mingw64\\bin;' + env.get('PATH', '')
    lo, hi, st = (int(x) for x in a.levels.split(':'))
    levels = list(range(lo, hi + 1, st))
    work = os.path.join(tempfile.gettempdir(), 'decodium_autoimprove', 'ft4_awgn')
    os.makedirs(work, exist_ok=True)
    msg = 'CQ DL9XYZ JO62'

    def one(args):
        lvl, seed = args
        wav = os.path.join(work, 'w_%d_%d.wav' % (lvl, seed))
        subprocess.run([gen, '--message', msg, '--freq', str(a.freq), '--offset-ms', '600', '--gain', '0.85',
                        '--snr-db', str(lvl), '--seed', str(seed), wav], env=env, capture_output=True)
        p = subprocess.run([dec, '--stages', '4', '--depth', '3', '--nfa', '300', '--nfb', '2800',
                            '--mycall', 'IU8LMC', wav], env=env, capture_output=True, text=True,
                           encoding='utf-8', errors='replace')
        ok = any(m.group(1).split() == msg.split() for m in re.finditer(r'decoded="([^"]*)"', p.stdout))
        os.remove(wav)
        return lvl, ok

    jobs = [(l, s) for l in levels for s in range(1, a.trials + 1)]
    with cf.ThreadPoolExecutor(max_workers=8) as ex:
        res = list(ex.map(one, jobs))
    tot = 0
    row = []
    for l in levels:
        n = sum(1 for lv, ok in res if lv == l and ok)
        tot += n
        row.append('%+d:%d/%d' % (l, n, a.trials))
    print(' '.join(row), ' total', tot)


if __name__ == '__main__':
    main()
