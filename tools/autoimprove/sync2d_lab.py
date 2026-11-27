#!/usr/bin/env python3
"""Offline prototype: FT2 candidates from a sync-pattern correlation over the spectrogram (the way FT8's
sync2d works), instead of the peak of the slot-average spectrum.

Per slot: symbol-length (288 samples) frames every 72 samples, zero-padded to 1152 (10.4 Hz bins). For every
base frequency bin and start frame, add the power found at the right tone of each of the 16 known sync
symbols (four words of four symbols at symbol offsets 0, 33, 66, 99), each divided by the power of the four
tones of that symbol, so a strong steady interferer does not win just by being strong. Peaks over frequency
(best start frame per bin) are the candidates.  Recall = a candidate within +-12 Hz of the true base freq.
"""
import json
import os
import sys
import wave

import numpy as np

TMP = os.environ.get('TEMP', '/tmp')
SETS = os.path.join(TMP, 'decodium_autoimprove')
SR = 12000
NSPS = 288
HOP = 72                       # 4 frames per symbol
NFFT = 1152
DF = SR / NFFT                 # 10.4167 Hz
WORDS = [(0, [0, 1, 3, 2]), (33, [1, 0, 2, 3]), (66, [2, 3, 1, 0]), (99, [3, 2, 0, 1])]
RADIUS = 12.0


def read_wav(path):
    with wave.open(path, 'rb') as w:
        return np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').astype(np.float64)


def spectrogram(x):
    n = (len(x) - NSPS) // HOP + 1
    win = np.hanning(NSPS)
    idx = np.arange(NSPS)[None, :] + HOP * np.arange(n)[:, None]
    frames = x[idx] * win
    sp = np.fft.rfft(frames, NFFT, axis=1)
    return (sp.real ** 2 + sp.imag ** 2)                      # [frame, bin]


def sync_map(P, fa=300.0, fb=2800.0, normalise='tones'):
    nfr, nb = P.shape
    lo, hi = int(fa / DF), int(fb / DF)
    nstart = nfr - 4 * 103
    if nstart < 1:
        return None
    best = np.full(nb, -1e9)
    arg = np.zeros(nb, dtype=int)
    for t0 in range(0, nstart, 1):
        acc = np.zeros(nb)
        cnt = 0
        for off, tones in WORDS:
            for s, tone in enumerate(tones):
                fr = t0 + 4 * (off + s)               # frame aligned with the symbol
                row = P[fr]
                if normalise == 'tones':
                    # power at the right tone / power over the four tones, per base-bin
                    num = np.zeros(nb); den = np.zeros(nb)
                    for k in range(4):
                        sh = np.zeros(nb); sh[: nb - 4 * k] = row[4 * k:]
                        den += sh
                        if k == tone:
                            num = sh
                    acc += num / np.maximum(den, 1e-12)
                else:
                    sh = np.zeros(nb); sh[: nb - 4 * tone] = row[4 * tone:]
                    acc += sh
                cnt += 1
        m = acc > best
        best[m] = acc[m]; arg[m] = t0
    return best, arg


def peaks(best, k, lo, hi, excl=3):
    order = np.argsort(-best[lo:hi]) + lo
    out = []
    for i in order:
        if best[i] < best[i - 1] or best[i] < best[i + 1]:
            continue
        if any(abs(i - j) <= excl for j in out):
            continue
        out.append(i)
        if len(out) == k:
            break
    return out


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else 'rtrain'
    limit = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    d = os.path.join(SETS, 'real_' + name)
    ver = json.load(open(os.path.join(d, 'verita.json'), encoding='utf-8'))[:limit]
    ks = (1, 2, 3, 5, 8, 11, 15)
    res = {}
    for v in ver:
        x = read_wav(os.path.join(d, v['file']))
        P = spectrogram(x)
        out = sync_map(P)
        if out is None:
            continue
        best, arg = out
        lo, hi = int(300 / DF), int(2800 / DF)
        pk = peaks(best, 15, lo, hi)
        fs = np.array([i * DF for i in pk])            # base freq = bin * df (tone 0)
        r = res.setdefault(v['snr_db'], {k: 0 for k in ks} | {'n': 0})
        r['n'] += 1
        for k in ks:
            if np.min(np.abs(fs[:k] - v['freq_hz'])) <= RADIUS:
                r[k] += 1
    print('== %s' % name)
    for lvl in sorted(res, reverse=True):
        r = res[lvl]
        print('  %+3d dB n=%d  ' % (lvl, r['n']) + ' '.join('k%-2d:%2d' % (k, r[k]) for k in ks))


if __name__ == '__main__':
    main()
