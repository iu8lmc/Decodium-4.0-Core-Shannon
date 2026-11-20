#!/usr/bin/env python3
# FT8 corpus builder (known truth). SNR in a 2500 Hz reference bandwidth; noise sigma 600 (16-bit samples) at 12 kHz.
# usage: ft8_bench_corpus.py <outdir> <kind> <seed0> <nslots> <snr_lo> <snr_hi> [nsig]
# kinds: awgn | drift (linear drift, total 0.1-0.5 Hz over the burst, random sign) | fade (Rayleigh, Gaussian
#        Doppler spectrum, 2-sigma spread 0.5 Hz) | walk (phase random walk 5-15 deg/symbol) | noise |
#        qrm (30 strong signals -8..+15 dB, random frequencies with overlaps, plus 6 weak ones)
import os, subprocess, sys, wave, random
import numpy as np
# The clean bursts come from the ft8_make_test_wav tool of this repository (tests/ft8_make_test_wav.cpp);
# noise, channel models and mixing are done here. Set FT8_MAKE_TEST_WAV to the built binary.
GEN = os.environ.get("FT8_MAKE_TEST_WAV", "ft8_make_test_wav")
outdir, kind, seed0, nslots = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
lo, hi = float(sys.argv[5]), float(sys.argv[6])
nsig = int(sys.argv[7]) if len(sys.argv) > 7 else 20
os.makedirs(outdir, exist_ok=True); tmp = outdir + "/.tmp"; os.makedirs(tmp, exist_ok=True)
pfx = ["K","W","N","IU","9H","DL","EA","F","G","JA","VK","PY","ZS","OH","SM","LU","UA","OK","HB","ON","PA","SP","YO","LZ","SV","TA","4X","CT","EI","OZ","LA","VE","I","IK","RV","UX","DJ","BG","HL","VU","ZL","CX","XE","KP","OE"]
grids = ["FN42","EN37","JN70","JM75","JO51","JN01","JN18","IO91","PM95","QF56","GG66","KG33","KP20","JO89","GF05","KO85","JO70","JN47","JO20","JO22","FM18","DM79","CN87","EL96","IO83","JP90","KN34","LO15","OL72","PF95"]
tails = ["{g}","-12","R-07","RR73","73","RRR","+03","R+01","-21","R-15","+10","-18","R-22"]
def call(rng):
    L = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    return rng.choice(pfx) + str(rng.randint(0, 9)) + "".join(rng.choice(L) for _ in range(rng.randint(1, 3)))
def message(rng, k):
    a, b = call(rng), call(rng)
    if k % 3 == 0: return "CQ %s %s" % (a, rng.choice(grids))
    return "%s %s %s" % (a, b, rng.choice(tails).format(g=rng.choice(grids)))
NS = 180000; SIGMA = 600.0; FS = 12000.0
def clean(msg, freq, off, tag):
    path = "%s/%s.wav" % (tmp, tag)
    r = subprocess.run([GEN, "--message", msg, "--freq", "%.2f" % freq, "--offset-ms", "%.1f" % off, path],
                       check=True, capture_output=True, text=True, env=dict(os.environ, QT_QPA_PLATFORM="offscreen"))
    sent = [l.split(":", 1)[1].strip() for l in r.stdout.splitlines() if l.startswith("message:")][0]
    w = wave.open(path); d = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64); w.close()
    os.remove(path)
    return d[:NS], sent
def analytic(d):
    X = np.fft.fft(d); n = len(d); X[1:n // 2] *= 2.0; X[n // 2:] = 0.0
    return np.fft.ifft(X)
def amp_for(snr): return np.sqrt(2.0 * (10 ** (snr / 10.0)) * SIGMA * SIGMA * 2500.0 / 6000.0)
def fading(nrng, spread_hz):
    # complex Gaussian process, Gaussian Doppler spectrum with 2-sigma width spread_hz, unit mean power
    n = NS; f = np.fft.fftfreq(n, 1.0 / FS); sd = spread_hz / 2.0
    H = np.exp(-0.25 * (f / sd) ** 2)            # sqrt of the Gaussian PSD
    w = (nrng.normal(size=n) + 1j * nrng.normal(size=n)) * H
    g = np.fft.ifft(w); return g / np.sqrt(np.mean(np.abs(g) ** 2))
def channel(d, chan, rng, nrng, off_ms):
    if chan == "awgn": return d, ""
    z = analytic(d); t = np.arange(NS) / FS
    if chan == "drift":
        tot = rng.uniform(0.1, 0.5) * rng.choice([-1, 1]); t0 = off_ms / 1000.0 + 6.32; dur = 12.64
        ph = np.pi * (tot / dur) * (t - t0) ** 2       # frequency offset 0 at mid-burst, +-tot/2 at the ends
        return (z * np.exp(1j * ph)).real, " drift=%+.2f" % tot
    if chan == "fade":
        return (z * fading(nrng, 0.5)).real, " fade=0.5"
    if chan == "walk":
        sdeg = rng.uniform(5.0, 15.0); nsym = NS // 1920 + 1
        steps = np.cumsum(nrng.normal(0.0, np.radians(sdeg), nsym))
        ph = np.interp(np.arange(NS) / 1920.0, np.arange(nsym), steps)
        return (z * np.exp(1j * ph)).real, " walk=%.1f" % sdeg
    raise SystemExit("bad chan")
def pick_freqs(rng, n, flo, fhi, spacing):
    fr = []
    for _ in range(n):
        for _ in range(2000):
            f = rng.uniform(flo, fhi)
            if all(abs(f - g) >= spacing for g in fr): break
        fr.append(f)
    return fr
def write(name, mix, truth):
    out = np.clip(np.rint(mix), -32768, 32767).astype(np.int16)
    w = wave.open(os.path.join(outdir, name + ".wav"), "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(12000)
    w.writeframes(out.tobytes()); w.close()
    open(os.path.join(outdir, name + ".truth"), "w").write("".join(t + "\n" for t in truth))
for i in range(nslots):
    seed = seed0 + i; rng = random.Random(seed); nrng = np.random.default_rng(seed)
    name = "%s_%06d" % (kind, seed)
    if os.path.exists(os.path.join(outdir, name + ".truth")): continue
    mix = nrng.normal(0.0, SIGMA, NS); truth = []; seen = set()
    def add(k, f, snr, chan):
        off = rng.uniform(200.0, 1000.0)
        while True:
            msg = message(rng, k)
            if msg not in seen: break
        d, sent = clean(msg, f, off, name)
        seen.add(msg)
        d = d / np.abs(d).max()
        d, note = channel(d, chan, rng, nrng, off)
        mix[:] += d * amp_for(snr)
        truth.append("%7.1f %+5.1f %6.1f %s |%s" % (f, snr, off, sent, note))
    if kind in ("awgn", "drift", "fade", "walk"):
        for k, f in enumerate(pick_freqs(rng, nsig, 300, 2800, 110)):
            add(k, f, rng.uniform(lo, hi), kind)
    elif kind == "qrm":
        for k in range(30): add(k, rng.uniform(300, 2800), rng.uniform(-8, 15), rng.choice(["awgn", "drift", "fade"]))
        for k in range(6): add(30 + k, rng.uniform(300, 2800), rng.uniform(lo, hi), "awgn")
    elif kind != "noise": raise SystemExit("bad kind")
    write(name, mix, truth)
