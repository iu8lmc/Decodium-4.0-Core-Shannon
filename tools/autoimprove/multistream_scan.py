"""Quanti stream simultanei e a che distanza si decodificano? (lato hound)

N flussi con messaggi diversi, stessa partenza (come uno stream MAM della Fox),
ampiezza uguale, rumore bianco; si misura la quota di flussi decodificata
in funzione della distanza fra le portanti. Serve a tarare la spaziatura
minima del MAM per modo (DecodiumBridge::mamMinStreamGapHz).

  python multistream_scan.py PROVE SNR_LISTA DISTANZE_LISTA STREAM_LISTA [FT2|FT4]
  python multistream_scan.py 40 -11,-13 40,60,85,125,170,250 2,3 FT2

SNR in 2500 Hz PER OGNI flusso. Serve DECODIUM_AI_BUILD (default build_689).
"""
import os, random, sys, tempfile, json, re, subprocess, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
os.environ.setdefault("DECODIUM_AI_BUILD", os.path.join(HERE, "..", "..", "build_689"))
MODO = (sys.argv[5] if len(sys.argv) > 5 else "FT2").upper()
import numpy as np
if MODO == "FT4":
    import mescola_ft4 as M
    import verita_ft4 as V
else:
    import mescola_ft2 as M
    import verita_ft2 as V

SR = 12000
rnd = random.Random(5)
nrng = np.random.default_rng(5)
import tempfile as _tf
base = os.path.join(_tf.gettempdir(), "ms_" + MODO.lower())
shutil.rmtree(base, ignore_errors=True)
os.makedirs(base)
tmp = os.path.join(base, "clean.wav")

N_TRIALS = int(sys.argv[1]) if len(sys.argv) > 1 else 40
SNRS = [float(x) for x in (sys.argv[2] if len(sys.argv) > 2 else "-8,-12").split(",")]
SPACINGS = [float(x) for x in (sys.argv[3] if len(sys.argv) > 3 else "40,60,85,125,170,250").split(",")]
NSTREAMS = [int(x) for x in (sys.argv[4] if len(sys.argv) > 4 else "2,3").split(",")]

msgs = []
for _ in range(N_TRIALS * 4):
    msgs.append(M.genera_messaggio(rnd))


def make(cfg_name, n, spacing, snr, trial):
    f0 = rnd.uniform(700, 1100)
    tot = None
    truth = []
    for k in range(n):
        m = msgs[(trial * 4 + k) % len(msgs)]
        f = f0 + k * spacing
        x = M.genera_pulito(m, f, 0.0, 0.0, tmp)
        # ampiezza: potenza sul tratto attivo
        act = x[np.abs(x) > 1e-3 * np.max(np.abs(x))]
        ps = np.mean(act ** 2)
        x = x / np.sqrt(ps)              # potenza unitaria sul tratto attivo
        truth.append((m, f))
        tot = x if tot is None else tot + x
    # rumore tale che OGNI flusso abbia snr in 2500 Hz: sigma^2 * 2500/(SR/2) * 10^(snr/10) = 1
    sig2 = 1.0 / (10 ** (snr / 10.0)) / (2500.0 / (SR / 2.0))
    tot = tot + nrng.normal(0, np.sqrt(sig2), len(tot))
    tot = tot * (3000.0 / np.sqrt(np.mean(tot ** 2)))
    name = "%s_%03d.wav" % (cfg_name, trial)
    M.scrivi_wav(os.path.join(base, name), tot)
    return name, truth


results = {}
for n in NSTREAMS:
    for sp in SPACINGS:
        for snr in SNRS:
            cfg = "n%d_d%d_s%d" % (n, sp, -snr)
            names, truths = [], {}
            for t in range(N_TRIALS):
                nm, tr = make(cfg, n, sp, snr, t)
                names.append(nm); truths[nm] = tr
            res = V.esegui(base, names, 3, None, "IU8LMC", 0)
            got = tot = 0
            for nm in names:
                dec = {" ".join(r["msg"].split()) for r in res.get(nm, [])}
                for m, f in truths[nm]:
                    tot += 1
                    if " ".join(m.split()) in dec:
                        got += 1
            results[cfg] = (got, tot)
            print("%-14s decodificati %3d/%3d = %.0f%%" % (cfg, got, tot, 100.0 * got / tot), flush=True)
            for nm in names:
                os.remove(os.path.join(base, nm))
