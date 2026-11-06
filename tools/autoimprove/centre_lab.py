#!/usr/bin/env python3
"""Offline lab for the FT2 candidate frequency estimator.

Reads spectra dumped by a TEMPORARY patch of FtxFt2Stage7.cpp (after ftx_getcandidates2_c, pass 1, append
per slot: int ncand, float savg[576], float sbase[576], float candidate[2*ncand] to %TEMP%/savg_<set>.bin;
not kept in the decoder) for the real-noise sets and measures, against the known truth, how often a candidate lands
within the sync radius (+-12 Hz) of the true base frequency, as a function of how many candidates
are kept.  Estimators are plain functions of (savg, sbase), so new ones can be added here and compared
with the one in the decoder before touching C++.
"""
import json
import os
import struct
import sys

import numpy as np

TMP = os.environ.get('TEMP', '/tmp')
SETS = os.path.join(TMP, 'decodium_autoimprove')
NH1 = 576
DF = 12000.0 / 1152.0
OFFSET = -62.5          # candidate = lump centre + OFFSET
RADIUS = 12.0


def load(name):
    ver = json.load(open(os.path.join(SETS, 'real_' + name, 'verita.json'), encoding='utf-8'))
    raw = open(os.path.join(TMP, 'savg_%s.bin' % name), 'rb').read()
    pos, recs = 0, []
    while pos < len(raw):
        (nc,) = struct.unpack_from('<i', raw, pos); pos += 4
        savg = np.frombuffer(raw, '<f4', NH1, pos); pos += 4 * NH1
        sbase = np.frombuffer(raw, '<f4', NH1, pos); pos += 4 * NH1
        cand = np.frombuffer(raw, '<f4', 2 * nc, pos).reshape(nc, 2); pos += 8 * nc
        recs.append((savg, sbase, cand))
    assert len(recs) == len(ver), (len(recs), len(ver))
    return ver, recs


def freq_of_bin(c):
    return (c + 1) * DF


def bin_of_freq(f):
    return f / DF - 1.0


def recall_current(ver, recs, kmax=15):
    """Recall within RADIUS for the first k candidates of the decoder's own list (ordered by its power)."""
    hit = np.zeros(kmax + 1)
    for v, (savg, sbase, cand) in zip(ver, recs):
        order = np.argsort(-cand[:, 1])
        fs = cand[order, 0]
        for k in range(1, kmax + 1):
            if len(fs[:k]) and np.min(np.abs(fs[:k] - v['freq_hz'])) <= RADIUS:
                hit[k] += 1
    return hit / len(ver)


def excess(savg, sbase):
    """Excess of the average spectrum over the fitted baseline, in dB; 0 where the baseline is not defined."""
    ok = sbase > 0
    out = np.zeros_like(savg, dtype=float)
    out[ok] = 10.0 * np.log10(np.maximum(savg[ok], 1e-30) / sbase[ok])
    return out


def make_template(ver, recs, level=-3.0, half=18):
    rows = []
    for v, (savg, sbase, cand) in zip(ver, recs):
        if v['snr_db'] != level:
            continue
        r = excess(savg, sbase)
        c0 = bin_of_freq(v['freq_hz'] - OFFSET)
        offs = np.arange(-half, half + 1)
        pos = c0 + offs
        ok = (pos >= 0) & (pos < NH1 - 1)
        if not ok.all():
            continue
        i0 = np.floor(pos).astype(int); w = pos - i0
        rows.append(r[i0] * (1 - w) + r[i0 + 1] * w)
    return np.median(np.array(rows), axis=0)


def peaks_from_score(score, k, lo_bin, hi_bin):
    """Local maxima of `score`, best first, at most k, within bins [lo_bin, hi_bin]."""
    idx = [i for i in range(max(1, lo_bin), min(NH1 - 2, hi_bin)) if score[i] >= score[i - 1] and score[i] > score[i + 1]]
    idx.sort(key=lambda i: -score[i])
    out = []
    for i in idx:
        # parabolic refinement
        l, c, r = score[i - 1], score[i], score[i + 1]
        den = l - 2 * c + r
        d = 0.5 * (l - r) / den if den != 0 else 0.0
        out.append((freq_of_bin(i + d) + OFFSET, score[i]))
        if len(out) == k:
            break
    return out


def recall_estimator(ver, recs, score_fn, kmax=15, fa=300.0, fb=2800.0):
    hit = np.zeros(kmax + 1)
    lo, hi = int(bin_of_freq(fa - OFFSET)), int(bin_of_freq(fb - OFFSET))
    for v, (savg, sbase, cand) in zip(ver, recs):
        score = score_fn(savg, sbase)
        pk = peaks_from_score(score, kmax, lo, hi)
        fs = np.array([p[0] for p in pk]) if pk else np.array([1e9])
        for k in range(1, kmax + 1):
            if np.min(np.abs(fs[:k] - v['freq_hz'])) <= RADIUS:
                hit[k] += 1
    return hit / len(ver)


def box_score(savg, sbase, width=7):
    ok = sbase > 0
    r = np.zeros_like(savg, dtype=float)
    r[ok] = savg[ok] / sbase[ok]
    k = np.ones(2 * width + 1) / (2 * width + 1)
    return np.convolve(r, k, mode='same')


def template_score(template):
    t = template - template.mean()
    t = t / np.sqrt((t ** 2).sum())

    def fn(savg, sbase):
        return np.correlate(excess(savg, sbase), t, mode='same')
    return fn


def show(title, arr, ks=(1, 2, 3, 5, 8, 11, 15)):
    print('%-28s' % title + ' '.join('k=%-2d %.2f' % (k, arr[k]) for k in ks))


if __name__ == '__main__':
    verT, recT = load('rtrain')
    tpl = make_template(verT, recT)
    print('template (bins -18..+18):', ' '.join('%.2f' % x for x in tpl))
    for name in ('rtrain', 'rheld', 'rheld2'):
        ver, recs = load(name)
        print('== %s (%d slots)' % (name, len(ver)))
        show('decoder (current)', recall_current(ver, recs))
        show('box 15 (replica)', recall_estimator(ver, recs, box_score))
        show('template matched filter', recall_estimator(ver, recs, template_score(tpl)))


def per_level(ver, recs, fn, k=15):
    """Recall (nearest of the best k peaks within RADIUS) by injected level."""
    lo, hi = int(bin_of_freq(300.0 - OFFSET)), int(bin_of_freq(2800.0 - OFFSET))
    out = {}
    for v, (savg, sbase, cand) in zip(ver, recs):
        if fn is None:
            order = np.argsort(-cand[:, 1])
            fs = cand[order, 0][:k]
        else:
            pk = peaks_from_score(fn(savg, sbase), k, lo, hi)
            fs = np.array([p[0] for p in pk]) if pk else np.array([1e9])
        hit = len(fs) and np.min(np.abs(fs - v['freq_hz'])) <= RADIUS
        a = out.setdefault(v['snr_db'], [0, 0]); a[0] += bool(hit); a[1] += 1
    return out


def lab2():
    tpl = make_template(*load('rtrain'))
    fns = {'decoder': None, 'box15': box_score, 'template': template_score(tpl)}
    for name in ('rtrain', 'rheld', 'rheld2'):
        ver, recs = load(name)
        for k in (11, 20):
            print('== %s k=%d' % (name, k))
            for label, fn in fns.items():
                o = per_level(ver, recs, fn, k)
                print('  %-9s' % label + '  '.join('%+d:%2d' % (l, o[l][0]) for l in sorted(o, reverse=True)))
