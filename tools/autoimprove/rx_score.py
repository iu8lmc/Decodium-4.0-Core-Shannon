#!/usr/bin/env python3
"""Decodium RX scoreboard: one command that measures the receiver the same way every time.

Used by the self-improvement loop (see tools/autoimprove/README.md): every candidate
change is measured with this script and compared with the baseline by `compare`.
Nothing here transmits, opens a serial port or touches the user's profile: it only runs
the offline bench executables on synthetic scenes with known truth.

  rx_score.py run     --build build_689 --label base [--suite ft2,ft8] [--jobs 5]
  rx_score.py compare results/base.json results/new.json
  rx_score.py noise   --build build_689 --label noise1     (same as run; compare two of
                                                            them to see the run-to-run noise)

Suites
  ft2  ft2_async_bench: 240 s FT2 scenes with truth (TRAIN seeds are the ones a change is
       tuned on, HELDOUT seeds are only ever used to confirm). Metrics: transmissions
       decoded, false lines, CPU per dispatch, latency, 50 % threshold.
  ft8  decode_bench/bench.py, single FT8 signal at known SNR (prod profile): 50 % threshold.
  ft2real  synthetic FT2 slots with known truth summed into REAL band recordings (the
       lab method mescola_ft2.py + verita_ft2.py): the receiver sees real QRM and filter
       shapes, which pure AWGN scenes do not have. TRAIN uses one recording, HELDOUT another.
"""
import argparse
import concurrent.futures as cf
import hashlib
import json
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
RESULTS = os.path.join(HERE, 'results')
DEFAULT_WORK = os.path.join(os.environ.get('TEMP', '/tmp'), 'decodium_autoimprove')
MINGW_BIN = 'C:/msys64/mingw64/bin'

# name -> (seed, extra gen options).  Fixed forever: changing them invalidates every baseline.
FT2_TRAIN = {'t7': (7, []), 't11': (11, []), 't13': (13, [])}
FT2_HELDOUT = {'h101': (101, []), 'h102': (102, ['--overlap=1'])}
FT2_GEN = ['--seconds=240', '--rate=20']


def env():
    e = dict(os.environ)
    e['PATH'] = MINGW_BIN + os.pathsep + e.get('PATH', '')
    return e


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for blk in iter(lambda: f.read(1 << 20), b''):
            h.update(blk)
    return h.hexdigest()[:16]


def git_head():
    try:
        return subprocess.check_output(['git', '-C', REPO, 'rev-parse', '--short', 'HEAD'], text=True).strip() \
            + ('+dirty' if subprocess.check_output(['git', '-C', REPO, 'status', '--porcelain', '-uno'], text=True).strip() else '')
    except Exception:
        return 'unknown'


# ----------------------------------------------------------------------------- FT2 suite
def ft2_scene(bench, work, name, seed, extra):
    """Scenes are generated once and then reused, so a baseline and a candidate see the
    same audio even if the TX encoder changes; the hash is stored in the results."""
    os.makedirs(work, exist_ok=True)
    wav = os.path.join(work, name + '.wav')
    truth = os.path.join(work, name + '.txt')
    if not (os.path.isfile(wav) and os.path.isfile(truth)):
        subprocess.run([bench, 'gen', wav, truth, '--seed=%d' % seed] + FT2_GEN + extra,
                       check=True, env=env(), capture_output=True)
    return wav, truth


def parse_ft2(out):
    def num(rx, cast=float, default=None):
        m = re.search(rx, out)
        return cast(m.group(1).replace(',', '.')) if m else default
    r = {}
    m = re.search(r'trasmissioni (\d+), decodificate (\d+) \(([\d.]+)%\).*?falsi (\d+)', out)
    if m:
        r['tx'], r['decoded'], r['pct'], r['false'] = int(m.group(1)), int(m.group(2)), float(m.group(3)), int(m.group(4))
    r['cpu_mean_ms'] = num(r'CPU per dispatch: media (\d+) ms', int)
    r['cpu_p95_ms'] = num(r'p95 (\d+) ms', int)
    r['lat_mean_s'] = num(r'latenza \(mostrata - fine TX\): media ([\d.]+) s')
    r['lat_p95_s'] = num(r'latenza .*?p95 ([\d.]+) s')
    r['thr50_db'] = num(r'soglia 50%: (-?[\d.]+) dB')
    r['lost_repeat'] = num(r'ripetizioni legittime \d+, perse (\d+)', int)
    return r


def run_ft2_one(bench, work, name, seed, extra, held):
    wav, truth = ft2_scene(bench, work, name, seed, extra)
    t0 = time.time()
    p = subprocess.run([bench, 'run', wav, truth], env=env(), capture_output=True, text=True)
    res = parse_ft2(p.stdout)
    res.update(name=name, heldout=held, scene_sha=sha(wav), secs=round(time.time() - t0), rc=p.returncode)
    if 'decoded' not in res:
        res['error'] = (p.stdout + p.stderr)[-400:]
    return res


def run_ft2(build, work, jobs):
    bench = os.path.join(build, 'tests', 'ft2_async_bench.exe')
    if not os.path.isfile(bench):
        sys.exit('missing %s (ninja ft2_async_bench)' % bench)
    scenes = [(n, s, x, False) for n, (s, x) in FT2_TRAIN.items()] + [(n, s, x, True) for n, (s, x) in FT2_HELDOUT.items()]
    # make every scene before going parallel
    for n, s, x, _ in scenes:
        ft2_scene(bench, work, n, s, x)
    with cf.ThreadPoolExecutor(max_workers=jobs) as ex:
        futs = [ex.submit(run_ft2_one, bench, work, n, s, x, h) for n, s, x, h in scenes]
        rows = [f.result() for f in futs]

    def agg(sel):
        rr = [r for r in rows if sel(r) and 'decoded' in r]
        return {
            'tx': sum(r['tx'] for r in rr), 'decoded': sum(r['decoded'] for r in rr),
            'false': sum(r['false'] for r in rr),
            'cpu_mean_ms': round(sum(r['cpu_mean_ms'] for r in rr) / max(1, len(rr))),
            'lat_mean_s': round(sum(r['lat_mean_s'] for r in rr) / max(1, len(rr)), 3),
            'thr50_db': round(sum(r['thr50_db'] for r in rr if r['thr50_db'] is not None) / max(1, len(rr)), 2),
        }
    return {'scenes': rows, 'train': agg(lambda r: not r['heldout']), 'heldout': agg(lambda r: r['heldout'])}


# ----------------------------------------------------------------------------- FT8 suite
def run_ft8(build, snr, trials):
    exe = os.path.join(build, 'tests', 'ft8_stage_compare.exe')
    if not os.path.isfile(exe):
        sys.exit('missing %s (ninja ft8_stage_compare)' % exe)
    bench = os.path.join(REPO, 'decode_bench', 'bench.py')
    csv = os.path.join(os.environ.get('TEMP', '/tmp'), 'rx_score_ft8.csv')
    t0 = time.time()
    p = subprocess.run([sys.executable, bench, '--decodium', exe, '--profile', 'prod', '--snr', snr,
                        '--trials', str(trials), '--label', 'rx_score', '--csv', csv],
                       env=env(), capture_output=True, text=True)
    m = re.search(r'SOGLIA Decodium \(prod\)\s*:\s*(-?[\d.]+)', p.stdout)
    hits = re.findall(r'^\s*(-\d+)\s*\|\s*(\d+)/(\d+)', p.stdout, re.M)
    r = {'thr50_db': float(m.group(1)) if m else None, 'secs': round(time.time() - t0),
         'points': {s: [int(h), int(t)] for s, h, t in hits}}
    if m is None:
        r['error'] = (p.stdout + p.stderr)[-400:]
    return r


# ----------------------------------------------------------------------------- FT2 on real noise
REC_DIR = os.path.join(os.path.expanduser('~'), 'Documents', 'Decodium', 'recordings')
# name -> (background recording, seed, extra mescola options)
FT2REAL_SETS = {
    'rtrain': ('decodium_20260907_180610.wav', 20260916, []),
    'rheld': ('decodium_20260909_145733.wav', 777, []),
    'rheld2': ('decodium_20260905_003849.wav', 4242, []),
}
FT2REAL_LEVELS = '-12,-9,-6,-3'
FT2REAL_PER_LEVEL = 50
# FT4 on the same recordings (7.5 s slots, a bit more sensitive: levels one step lower)
FT4REAL_SETS = {
    'f4train': ('decodium_20260907_180610.wav', 31, []),
    'f4held': ('decodium_20260909_145733.wav', 32, []),
    'f4held2': ('decodium_20260905_003849.wav', 33, []),
}
FT4REAL_LEVELS = '-15,-12,-9,-6'


def _real_cfg(name):
    if name in FT4REAL_SETS:
        return FT4REAL_SETS[name], 'ft4', FT4REAL_LEVELS
    return FT2REAL_SETS[name], 'ft2', FT2REAL_LEVELS


def ft2real_set(build, work, name):
    (rec, seed, extra), mode, levels = _real_cfg(name)
    out = os.path.join(work, 'real_' + name)
    if not os.path.isfile(os.path.join(out, 'verita.json')):
        e = env()
        e['DECODIUM_AI_BUILD'] = build
        bg = os.path.join(REC_DIR, rec)
        if not os.path.isfile(bg):
            sys.exit('background recording missing: %s' % bg)
        subprocess.run([sys.executable, os.path.join(HERE, 'mescola_%s.py' % mode), '--sfondo', bg, '--out', out,
                        '--livelli', levels, '--per-livello', str(FT2REAL_PER_LEVEL), '--seed', str(seed)] + extra,
                       check=True, env=e, capture_output=True)
    return out


def run_ft2real_one(build, work, name):
    out = ft2real_set(build, work, name)
    e = env()
    e['DECODIUM_AI_BUILD'] = build
    t0 = time.time()
    mode = _real_cfg(name)[1]
    p = subprocess.run([sys.executable, os.path.join(HERE, 'verita_%s.py' % mode), '--slot', out, '--ndepth', '3'],
                       env=e, capture_output=True, text=True, encoding='utf-8', errors='replace')
    levels = {}
    for m in re.finditer(r'^\|\s*([+-]\d+) dB \|\s*(\d+) \|\s*(\d+) \|', p.stdout, re.M):
        levels[m.group(1)] = [int(m.group(3)), int(m.group(2))]
    fm = re.search(r'righe non previste: (\d+)', p.stdout)
    r = {'name': name, 'levels': levels, 'ok': sum(v[0] for v in levels.values()),
         'n': sum(v[1] for v in levels.values()), 'false': int(fm.group(1)) if fm else None,
         'set_sha': sha(os.path.join(out, 'slot_00000.wav')), 'secs': round(time.time() - t0)}
    if not levels:
        r['error'] = (p.stdout + p.stderr)[-400:]
    return r


def run_ft2real(build, jobs, sets=None):
    work = DEFAULT_WORK
    names = list(sets or FT2REAL_SETS)
    for n in names:
        ft2real_set(build, work, n)
    with cf.ThreadPoolExecutor(max_workers=min(jobs, len(names))) as ex:
        rows = list(ex.map(lambda n: run_ft2real_one(build, work, n), names))
    held = rows[1:]
    merged = {'name': 'held', 'ok': sum(r['ok'] for r in held), 'n': sum(r['n'] for r in held),
              'false': sum((r['false'] or 0) for r in held), 'set_sha': '+'.join(r['set_sha'] for r in held)}
    return {'sets': rows, 'train': rows[0], 'heldout': merged}


# ----------------------------------------------------------------------------- commands
def cmd_run(a):
    build = os.path.abspath(a.build)
    out = {'label': a.label, 'head': git_head(), 'when': time.strftime('%Y-%m-%d %H:%M:%S'), 'suites': {}}
    suites = a.suite.split(',')
    if 'ft2' in suites:
        out['suites']['ft2'] = run_ft2(build, a.work, a.jobs)
    if 'ft2real' in suites:
        out['suites']['ft2real'] = run_ft2real(build, a.jobs)
    if 'ft4real' in suites:
        out['suites']['ft4real'] = run_ft2real(build, a.jobs, FT4REAL_SETS)
    if 'ft8' in suites:
        out['suites']['ft8'] = run_ft8(build, a.ft8_snr, a.ft8_trials)
    os.makedirs(RESULTS, exist_ok=True)
    path = os.path.join(RESULTS, a.label + '.json')
    with open(path, 'w') as f:
        json.dump(out, f, indent=1)
    show(out)
    print('-> %s' % path)


def show(o):
    print('== %s  (%s, %s)' % (o['label'], o['head'], o['when']))
    ft2 = o['suites'].get('ft2')
    if ft2:
        for r in ft2['scenes']:
            print('  ft2 %-5s %s decoded %2s/%2s false %s cpu %4s ms lat %.2f s thr %s dB  (%ss)' % (
                r['name'], 'H' if r['heldout'] else 'T', r.get('decoded'), r.get('tx'), r.get('false'),
                r.get('cpu_mean_ms'), r.get('lat_mean_s') or 0, r.get('thr50_db'), r.get('secs')))
        for k in ('train', 'heldout'):
            g = ft2[k]
            print('  ft2 %-7s decoded %3d/%3d false %d cpu %d ms lat %.2f s thr %.2f dB' % (
                k, g['decoded'], g['tx'], g['false'], g['cpu_mean_ms'], g['lat_mean_s'], g['thr50_db']))
    for key in ('ft2real', 'ft4real'):
      fr = o['suites'].get(key)
      if fr:
        for r in fr['sets']:
            print('  ' + key + ' %-7s decoded %3d/%3d false %s  by level %s  (%ss)' % (
                r['name'], r['ok'], r['n'], r['false'],
                ' '.join('%s:%d/%d' % (k, v[0], v[1]) for k, v in sorted(r['levels'].items())), r['secs']))
    ft8 = o['suites'].get('ft8')
    if ft8:
        print('  ft8 threshold(50%%) %s dB  (%ss)' % (ft8.get('thr50_db'), ft8.get('secs')))


# Acceptance rules for a candidate.  Noise levels come from `noise` runs; edit with data.
RULES = {
    'ft2_train_decoded_min_gain': 2,      # decoded transmissions, summed over the TRAIN scenes
    'ft2_heldout_decoded_min_gain': 0,    # must not lose on scenes the change was never tuned on
    'ft2_false_max_increase': 0,
    'ft2_cpu_max_ratio': 1.10,
    'ft2_lat_max_increase_s': 0.10,
    'ft2real_train_min_gain': 3,          # decoded slots, TRAIN recording
    'ft2real_heldout_min_gain': 0,        # HELDOUT recording (other day, other band conditions)
    'ft2real_false_max_increase': 0,
    'ft8_thr_min_gain_db': 0.3,           # negative threshold = more sensitive; gain = base - new
    'ft8_thr_max_loss_db': 0.2,
}


def cmd_compare(a):
    b, n = (json.load(open(p)) for p in (a.base, a.new))
    verdict, why = True, []
    gain = False
    fb, fn = b['suites'].get('ft2'), n['suites'].get('ft2')
    if fb and fn:
        for k in ('train', 'heldout'):
            d = fn[k]['decoded'] - fb[k]['decoded']
            print('ft2 %-7s decoded %+d (%d -> %d)  false %d -> %d  cpu %d -> %d ms  lat %.2f -> %.2f s  thr %.2f -> %.2f dB' % (
                k, d, fb[k]['decoded'], fn[k]['decoded'], fb[k]['false'], fn[k]['false'],
                fb[k]['cpu_mean_ms'], fn[k]['cpu_mean_ms'], fb[k]['lat_mean_s'], fn[k]['lat_mean_s'],
                fb[k]['thr50_db'], fn[k]['thr50_db']))
        dt = fn['train']['decoded'] - fb['train']['decoded']
        dh = fn['heldout']['decoded'] - fb['heldout']['decoded']
        if dt >= RULES['ft2_train_decoded_min_gain']:
            gain = True
        if dh < RULES['ft2_heldout_decoded_min_gain']:
            verdict = False; why.append('held-out lost %d' % -dh)
        for k in ('train', 'heldout'):
            if fn[k]['false'] - fb[k]['false'] > RULES['ft2_false_max_increase']:
                verdict = False; why.append('false lines up (%s)' % k)
            if fn[k]['cpu_mean_ms'] > fb[k]['cpu_mean_ms'] * RULES['ft2_cpu_max_ratio']:
                verdict = False; why.append('cpu up (%s)' % k)
            if fn[k]['lat_mean_s'] - fb[k]['lat_mean_s'] > RULES['ft2_lat_max_increase_s']:
                verdict = False; why.append('latency up (%s)' % k)
        # scenes must be the same audio
        for rb, rn in zip(fb['scenes'], fn['scenes']):
            if rb['scene_sha'] != rn['scene_sha']:
                verdict = False; why.append('scene %s differs' % rb['name'])
    rb, rn = b['suites'].get('ft2real'), n['suites'].get('ft2real')
    if rb and rn:
        for k in ('train', 'heldout'):
            print('ft2real %-7s decoded %d -> %d (of %d)  false %s -> %s' % (
                k, rb[k]['ok'], rn[k]['ok'], rb[k]['n'], rb[k]['false'], rn[k]['false']))
            if rb[k]['set_sha'] != rn[k]['set_sha']:
                verdict = False; why.append('ft2real %s slots differ' % k)
            if (rn[k]['false'] or 0) - (rb[k]['false'] or 0) > RULES['ft2real_false_max_increase']:
                verdict = False; why.append('ft2real false lines up (%s)' % k)
        dt = rn['train']['ok'] - rb['train']['ok']
        dh = rn['heldout']['ok'] - rb['heldout']['ok']
        if dt >= RULES['ft2real_train_min_gain']:
            gain = True
        if dh < RULES['ft2real_heldout_min_gain']:
            verdict = False; why.append('ft2real held-out lost %d' % -dh)
    gb, gn = b['suites'].get('ft8'), n['suites'].get('ft8')
    if gb and gn and gb.get('thr50_db') is not None and gn.get('thr50_db') is not None:
        g = gb['thr50_db'] - gn['thr50_db']
        print('ft8 threshold %.2f -> %.2f dB  (gain %+.2f dB)' % (gb['thr50_db'], gn['thr50_db'], g))
        if g >= RULES['ft8_thr_min_gain_db']:
            gain = True
        if -g > RULES['ft8_thr_max_loss_db']:
            verdict = False; why.append('ft8 threshold worse by %.2f dB' % -g)
    if not gain:
        verdict = False; why.append('no measurable gain')
    print('VERDICT: %s%s' % ('KEEP' if verdict else 'REVERT', '' if verdict else '  (' + '; '.join(why) + ')'))
    return 0 if verdict else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    for name in ('run', 'noise'):
        p = sub.add_parser(name)
        p.add_argument('--build', required=True, help='cmake build dir with tests/ft2_async_bench.exe and ft8_stage_compare.exe')
        p.add_argument('--label', required=True)
        p.add_argument('--suite', default='ft2,ft2real,ft8')
        p.add_argument('--jobs', type=int, default=5)
        p.add_argument('--work', default=DEFAULT_WORK)
        p.add_argument('--ft8-snr', default='-18:-24:-1')
        p.add_argument('--ft8-trials', type=int, default=20)
        p.set_defaults(fn=cmd_run)
    p = sub.add_parser('compare')
    p.add_argument('base')
    p.add_argument('new')
    p.set_defaults(fn=cmd_compare)
    a = ap.parse_args()
    sys.exit(a.fn(a) or 0)


if __name__ == '__main__':
    main()
