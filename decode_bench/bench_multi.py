#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
decode_bench/bench_multi.py — Banco MULTI-SEGNALE: misura il recupero della
"vittima" debole in uno slot AFFOLLATO (piu' FT8 sovrapposti a pochi Hz).

E' il banco per la LEVA B (cancellazione interferenza co-canale / harvest):
quello che il banco a segnale singolo (bench.py) NON puo' misurare, perche' su
segnale isolato harvest == deep.

Scena (default, mima il cluster 1500/1506/1517 Hz della nota di parita'):
  - 1 VITTIMA debole  @ 1500 Hz, SNR variabile (lo sweep);
  - N INTERFERENTI forti, sovrapposti a pochi Hz (es. +6, +17 Hz), SNR alto.

Costruzione del .wav affollato (corretta: ft8sim NON tiene l'ampiezza fissa fra
SNR diversi, normalizza ogni wav -> non si possono mescolare wav a SNR diversi).
Quindi:
  - genero TUTTI i segnali PULITI con ft8sim (snr alto -> rumore trascurabile,
    ampiezza standard identica per tutti);
  - sommo  vittima*1 + Sum_i g_i*interferente_i , con
        g_i = (rms_vittima/rms_interf_i) * 10^((snr_i - snr_v)/20)
    cioe' porto ogni interferente al suo SNR ASSOLUTO rispetto alla vittima;
  - aggiungo io l'AWGN, con sigma calibrato (--snr-cal) perche' il decoder
    riporti per la vittima esattamente snr_v (banda rif. 2500 Hz). Il rumore e'
    SEMINATO (seed = indice trial) -> realizzazioni RIPRODUCIBILI: i run
    prima/dopo usano lo STESSO rumore -> confronto a bassissima varianza.
  - normalizzo il picco per non clippare (guadagno comune -> SNR invariati).

Metrica: P(vittima recuperata) vs SNR vittima, per i profili `deep` e `harvest`.
APPORTO HARVEST = (P_harvest - P_deep): di quanto piu' spesso harvest estrae la
vittima che deep, sommersa dai forti, perde. E' l'apporto reale della leva B.

Solo standard library (riusa gli helper di bench.py).
"""

import argparse
import array
import glob
import math
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time
import wave

from bench import (decode_decodium, decode_jt9, norm_msg, threshold_50,
                   parse_snr_range, PROFILES, JT9_PROFILES,
                   DEFAULT_FT8SIM, DEFAULT_JT9)

FS = 12000

INTF_MSGS = [
    "CQ DX1ABC IO91",
    "JA1XYZ K5ZZ EM12",
    "OH2AB G3XYZ JO99",
    "VK3ZZ N5ABC DM79",
]
DEFAULT_VICTIM_MSG = "CQ IU8LMC JN70"

# Costante di calibrazione del rumore: sigma = rms_segnale * SNRCAL * 10^(-SNR/20).
# Teorica = sqrt(fs/2 / 2500) = sqrt(6000/2500) = 1.549, ma il decoder definisce
# la potenza-segnale su durata/banda effettive -> si calibra empiricamente
# (--calibrate) contro la SNR che il decoder stesso riporta. Default = teorica.
DEFAULT_SNRCAL = math.sqrt((FS / 2.0) / 2500.0)


def read16(path):
    w = wave.open(path, "rb")
    try:
        raw = w.readframes(w.getnframes())
    finally:
        w.close()
    a = array.array("h")
    a.frombytes(raw)
    return a


def write16(path, samples):
    w = wave.open(path, "wb")
    try:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(FS)
        w.writeframes(array.array("h", samples).tobytes())
    finally:
        w.close()


def rms(a):
    if not a:
        return 0.0
    s = 0.0
    for x in a:
        s += float(x) * float(x)
    return math.sqrt(s / len(a))


def ft8sim_clean(ft8sim, msg, f0, dt, snr, outdir, tag):
    """Genera UN segnale (quasi) pulito; ritorna l'array di campioni."""
    sub = os.path.join(outdir, tag)
    os.makedirs(sub, exist_ok=True)
    for old in glob.glob(os.path.join(sub, "*.wav")):
        os.remove(old)
    res = subprocess.run([ft8sim, msg, str(f0), str(dt), "0", "0", "1", str(snr)],
                         cwd=sub, capture_output=True, text=True)
    if res.returncode != 0:
        raise RuntimeError("ft8sim fallito (%s): %s" % (tag, res.stderr or res.stdout))
    wavs = sorted(glob.glob(os.path.join(sub, "*.wav")))
    if not wavs:
        raise RuntimeError("ft8sim non ha prodotto wav (%s)" % tag)
    return read16(wavs[0])


def build_scene(victim, rms_v, interferers, snr_v, snrcal, seed, out_path):
    """victim = campioni vittima PULITA (rms_v). interferers = [{samples,rms,snr}].
    Somma vittima + interferenti scalati al loro SNR assoluto, aggiunge AWGN
    seminato calibrato per dare alla vittima snr_v, normalizza il picco."""
    n = len(victim)
    for it in interferers:
        n = min(n, len(it["samples"]))
    acc = [float(victim[k]) for k in range(n)]
    for it in interferers:
        g = (rms_v / it["rms"] if it["rms"] else 0.0) * (10.0 ** ((it["snr"] - snr_v) / 20.0))
        s = it["samples"]
        for k in range(n):
            acc[k] += g * float(s[k])
    sigma = rms_v * snrcal * (10.0 ** (-snr_v / 20.0))
    rng = random.Random(seed)
    for k in range(n):
        acc[k] += rng.gauss(0.0, sigma)
    peak = max(1.0, max(abs(v) for v in acc))
    scale = (32000.0 / peak) if peak > 32000.0 else 1.0
    out = array.array("h", bytes(2 * n))
    for k in range(n):
        v = int(round(acc[k] * scale))
        out[k] = 32767 if v > 32767 else (-32768 if v < -32768 else v)
    write16(out_path, out)
    return out_path


def main():
    ap = argparse.ArgumentParser(
        description="Banco MULTI-SEGNALE: recupero vittima in slot affollato "
                    "(misura l'apporto di harvest / leva B).")
    ap.add_argument("--decodium", required=True)
    ap.add_argument("--ft8sim", default=DEFAULT_FT8SIM)
    ap.add_argument("--jt9", default=DEFAULT_JT9)
    ap.add_argument("--with-jt9", action="store_true")
    ap.add_argument("--snr", default="-12:-22:-2",
                    help="Sweep SNR della VITTIMA (default -12:-22:-2).")
    ap.add_argument("--trials", type=int, default=15)
    ap.add_argument("--victim-msg", default=DEFAULT_VICTIM_MSG)
    ap.add_argument("--victim-f0", type=float, default=1500.0)
    ap.add_argument("--victim-dt", type=float, default=0.2)
    ap.add_argument("--intf", default="1460:4,1525:3,1560:5,1590:2",
                    help="Interferenti 'f0:snr[,...]' (messaggi distinti auto). Default: "
                         "4 forti ben separati (banda affollata ma vittima recuperabile). "
                         "Per mascheramento totale usa un cluster stretto, es. '1506:3,1517:3'.")
    ap.add_argument("--intf-dt", type=float, default=0.0)
    ap.add_argument("--clean-snr", type=float, default=40.0,
                    help="SNR ft8sim per i segnali puliti (default 40).")
    ap.add_argument("--snr-cal", type=float, default=DEFAULT_SNRCAL,
                    help="Costante di calibrazione rumore (default teorica %.3f)." % DEFAULT_SNRCAL)
    ap.add_argument("--calibrate", action="store_true",
                    help="Calibra snr-cal empiricamente sulla SNR riportata dal decoder "
                         "(vittima sola), poi procede.")
    ap.add_argument("--profiles", default="deep,harvest")
    ap.add_argument("--max-ms", type=int, default=10000)
    ap.add_argument("--timeout", type=int, default=240)
    ap.add_argument("--label", default="multi")
    ap.add_argument("--workdir", default=None)
    ap.add_argument("--csv", default=None)
    args = ap.parse_args()

    decodium = os.path.abspath(args.decodium)
    for p, name in [(decodium, "ft8_stage_compare"), (args.ft8sim, "ft8sim")]:
        if not os.path.isfile(p):
            sys.exit("ERRORE: %s non trovato: %s" % (name, p))
    if args.with_jt9 and not os.path.isfile(args.jt9):
        sys.exit("ERRORE: jt9 non trovato: %s" % args.jt9)

    profiles = [p.strip() for p in args.profiles.split(",") if p.strip()]
    for p in profiles:
        if p not in PROFILES:
            sys.exit("ERRORE: profilo sconosciuto '%s'" % p)

    intf_spec = []
    for i, tok in enumerate(args.intf.split(",")):
        tok = tok.strip()
        if not tok:
            continue
        f0s, snrs = tok.split(":")
        intf_spec.append((float(f0s), float(snrs), INTF_MSGS[i % len(INTF_MSGS)]))

    own_workdir = args.workdir is None
    workdir = args.workdir or tempfile.mkdtemp(prefix="ft8multi_")
    os.makedirs(workdir, exist_ok=True)
    allcall = os.path.join(os.path.dirname(args.jt9), "ALLCALL7.TXT")
    if args.with_jt9 and os.path.isfile(allcall):
        try:
            shutil.copy(allcall, workdir)
        except OSError:
            pass

    snr_list = parse_snr_range(args.snr)
    target = norm_msg(args.victim_msg)
    all_f0 = [args.victim_f0] + [f for f, _, _ in intf_spec]
    nfa = max(200, int(min(all_f0)) - 100)
    nfb = min(4000, int(max(all_f0)) + 100)

    # --- segnali PULITI (una volta sola) ---
    victim = ft8sim_clean(args.ft8sim, args.victim_msg, args.victim_f0,
                          args.victim_dt, args.clean_snr, workdir, "vclean")
    rms_v = rms(victim)
    interferers = []
    for idx, (f0, snr_i, msg) in enumerate(intf_spec):
        s = ft8sim_clean(args.ft8sim, msg, f0, args.intf_dt, args.clean_snr,
                         workdir, "intf%d" % idx)
        interferers.append({"samples": s, "rms": rms(s), "snr": snr_i, "f0": f0, "msg": msg})

    snrcal = args.snr_cal
    deep_opts = list(PROFILES.get("deep", PROFILES[profiles[0]])) + ["--no-early"]

    # --- calibrazione opzionale: vittima SOLA, aggiusta snrcal su SNR riportata ---
    if args.calibrate:
        cal_snr = -15.0
        test = os.path.join(workdir, "cal.wav")
        build_scene(victim, rms_v, [], cal_snr, snrcal, 12345, test)
        dec, snrmap = decode_decodium(decodium, test, deep_opts, int(args.victim_f0),
                                      nfa, nfb, args.max_ms, args.timeout)
        if target in dec and target in snrmap:
            reported = snrmap[target]
            # sigma ~ 1/10^(reported/20); per portare reported->cal_snr:
            #   snrcal_new = snrcal * 10^((reported - cal_snr)/20)
            old = snrcal
            snrcal = snrcal * (10.0 ** ((reported - cal_snr) / 20.0))
            print("calibrazione: a snr-cal=%.3f la vittima -15 e' riportata snr=%d "
                  "-> snr-cal=%.3f" % (old, reported, snrcal))
        else:
            print("calibrazione: vittima non decodificata a -15 con snr-cal=%.3f "
                  "(uso il default)" % snrcal)

    print("=" * 78)
    print(" decode_bench MULTI  |  label=%s" % args.label)
    print("   vittima    : %r  @%.0f Hz  dt=%.1f" % (args.victim_msg, args.victim_f0, args.victim_dt))
    print("   interferenti:")
    for it in interferers:
        print("                 %r  @%.0f Hz (%+.0f Hz)  snr=%+.0f dB"
              % (it["msg"], it["f0"], it["f0"] - args.victim_f0, it["snr"]))
    print("   profili     : %s   banda=%d..%d Hz  max-ms=%d  snr-cal=%.3f"
          % (",".join(profiles), nfa, nfb, args.max_ms, snrcal))
    print("   sweep SNR   : %s  trials=%d   (SNR vittima, banda rif. 2500 Hz, rumore seminato)"
          % (snr_list, args.trials))
    print("=" * 78)

    cols = "  SNRv |" + "".join(" %-8s P    |" % p for p in profiles)
    if args.with_jt9:
        cols += " jt9      P    |"
    print(cols)
    print("  " + "-" * (len(cols) - 2))

    rows = []
    curves = {p: [] for p in profiles}
    jcurve = []
    t0 = time.time()
    for snr_v in snr_list:
        hits = {p: 0 for p in profiles}
        jhits = 0
        for k in range(args.trials):
            scene = os.path.join(workdir, "scene_%d.wav" % k)
            build_scene(victim, rms_v, interferers, snr_v, snrcal,
                        seed=1000 * int(round(snr_v)) + k, out_path=scene)
            for p in profiles:
                opts = list(PROFILES[p]) + ["--no-early"]
                dec, _ = decode_decodium(decodium, scene, opts, int(args.victim_f0),
                                         nfa, nfb, args.max_ms, args.timeout)
                if target in dec:
                    hits[p] += 1
            if args.with_jt9:
                jdec = decode_jt9(args.jt9, scene, JT9_PROFILES["ref"], workdir, args.timeout)
                if target in jdec:
                    jhits += 1
        row = [snr_v]
        cell = "  %4g |" % snr_v
        for p in profiles:
            pr = hits[p] / args.trials
            curves[p].append((snr_v, pr))
            row.append(hits[p])
            cell += " %3d/%-3d %4.2f|" % (hits[p], args.trials, pr)
        if args.with_jt9:
            jp = jhits / args.trials
            jcurve.append((snr_v, jp))
            row.append(jhits)
            cell += " %3d/%-3d %4.2f|" % (jhits, args.trials, jp)
        rows.append(row)
        print(cell)

    print("  " + "-" * (len(cols) - 2))

    def fmt_thr(t):
        return ("%.2f dB" % t) if isinstance(t, float) else str(t)

    thr = {}
    for p in profiles:
        thr[p] = threshold_50(curves[p])
        print("  SOGLIA recupero (%s) : %s" % (p, fmt_thr(thr[p])))
    if args.with_jt9:
        print("  SOGLIA recupero (jt9)   : %s" % fmt_thr(threshold_50(jcurve)))

    if "deep" in profiles and "harvest" in profiles:
        gaps = [hp - dp for (_, dp), (_, hp) in zip(curves["deep"], curves["harvest"])]
        avg_gap = sum(gaps) / len(gaps) if gaps else 0.0
        print("  ---")
        print("  APPORTO HARVEST: gap medio P(harvest)-P(deep) = %+.2f" % avg_gap)
        if isinstance(thr["deep"], float) and isinstance(thr["harvest"], float):
            print("                   soglia recupero: harvest %+.2f dB piu' bassa di deep"
                  % (thr["deep"] - thr["harvest"]))
        if avg_gap <= 0.02:
            print("  NB: nessun apporto -> interferenti piu' forti/vicini (--intf) o sweep")
            print("      verso SNR vittima piu' bassi (regime dove deep cede ma harvest regge).")
    print("  tempo totale: %.0f s" % (time.time() - t0))
    print("=" * 78)

    csv_path = args.csv or os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                        "results_%s.csv" % args.label)
    with open(csv_path, "w", encoding="utf-8") as f:
        f.write("label,snr_v,trials," + ",".join("hits_%s" % p for p in profiles)
                + (",hits_jt9" if args.with_jt9 else "") + "\n")
        for row in rows:
            f.write("%s,%g,%d," % (args.label, row[0], args.trials)
                    + ",".join(str(x) for x in row[1:]) + "\n")
    print("CSV -> %s" % csv_path)

    if own_workdir:
        shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    main()
