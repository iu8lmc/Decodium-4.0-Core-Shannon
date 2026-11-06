#!/usr/bin/env python3
"""One-at-a-time sweep of decoder environment switches on the fast `ft2real` suite.

Each variant is one environment assignment on top of the default binary, so no rebuild is
needed.  A variant that gains on TRAIN and does not lose on HELDOUT is only a CANDIDATE: it
still has to pass the full `rx_score.py run/compare` (AWGN scenes, CPU, latency) before it is
kept.

  python tools/autoimprove/sweep_env.py --build build_689 --base real1 [--jobs 4]
"""
import argparse
import concurrent.futures as cf
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
RESULTS = os.path.join(HERE, 'results')

# name -> environment (all on top of the default binary)
VARIANTS = {
    'coerente': {'DECODIUM_FT2_COERENTE': '1'},
    'drift': {'DECODIUM_FT2_DRIFT_SEARCH': '1'},
    'ap_soft': {'DECODIUM_FT2_AP_SOFT': '1'},
    'ap_cq_off': {'DECODIUM_FT2_AP_CQ': '0'},
    'sync16': {'DECODIUM_FT2_SYNC_HZ': '16'},
    'sync8': {'DECODIUM_FT2_SYNC_HZ': '8'},
    'hard22': {'DECODIUM_LDPC_MAX_HARD': '22'},
    'hard38': {'DECODIUM_LDPC_MAX_HARD': '38'},
    'hard46': {'DECODIUM_LDPC_MAX_HARD': '46'},
    'nfilt1000': {'DECODIUM_FT2_SUB_NFILT': '1000'},
    'nfilt4000': {'DECODIUM_FT2_SUB_NFILT': '4000'},
    'fondo_celle9': {'DECODIUM_FONDO_CELLE': '9'},
    'fondo_celle39': {'DECODIUM_FONDO_CELLE': '39'},
    'rescue_big': {'DECODIUM_FT2_STAGE7_LOCAL_RESCUE_BUDGET': '64'},
}


def run_one(build, name, extra):
    e = dict(os.environ)
    e.update(extra)
    label = 'sw_' + name
    subprocess.run([sys.executable, os.path.join(HERE, 'rx_score.py'), 'run', '--build', build, '--label', label,
                    '--suite', 'ft2real'], env=e, capture_output=True, text=True)
    p = os.path.join(RESULTS, label + '.json')
    return name, (json.load(open(p)) if os.path.isfile(p) else None)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', required=True)
    ap.add_argument('--base', required=True, help='label of a baseline ft2real result in results/')
    ap.add_argument('--jobs', type=int, default=4)
    ap.add_argument('--only', default='')
    a = ap.parse_args()
    base = json.load(open(os.path.join(RESULTS, a.base + '.json')))['suites']['ft2real']
    names = [n for n in VARIANTS if not a.only or n in a.only.split(',')]
    with cf.ThreadPoolExecutor(max_workers=a.jobs) as ex:
        res = list(ex.map(lambda n: run_one(os.path.abspath(a.build), n, VARIANTS[n]), names))
    print('baseline %s: train %d/%d  heldout %d/%d  false %s/%s' % (
        a.base, base['train']['ok'], base['train']['n'], base['heldout']['ok'], base['heldout']['n'],
        base['train']['false'], base['heldout']['false']))
    print('%-14s %7s %7s %6s   verdict' % ('variant', 'train', 'heldout', 'false'))
    for name, r in res:
        if not r:
            print('%-14s  (no result)' % name)
            continue
        f = r['suites']['ft2real']
        dt = f['train']['ok'] - base['train']['ok']
        dh = f['heldout']['ok'] - base['heldout']['ok']
        df = (f['train']['false'] or 0) + (f['heldout']['false'] or 0) - (base['train']['false'] or 0) - (base['heldout']['false'] or 0)
        tag = 'CANDIDATE' if dt >= 3 and dh >= 0 and df <= 0 else ('mixed' if dt > 0 or dh > 0 else '-')
        print('%-14s %+7d %+7d %+6d   %s' % (name, dt, dh, df, tag))


if __name__ == '__main__':
    main()
