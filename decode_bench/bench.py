#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
decode_bench/bench.py — Banco di misura della sensibilita' FT8 in dB su .wav sintetici.

Catena di misura (tutto auto-contenuto, riproducibile):

    ft8sim  ->  segnale FT8 a SNR CALIBRATO + AWGN (N realizzazioni di rumore)
            ->  decoder REALE di Decodium (tests/ft8_stage_compare, stage4)
            ->  [opzionale] jt9 MTft8 (riferimento JTDX-style)
            ->  P(decode) vs SNR  ->  SOGLIA al 50% in dB

La SNR di ft8sim e' nella banda di riferimento 2500 Hz (convenzione WSJT-X),
quindi DIRETTAMENTE confrontabile con la SNR riportata da Decodium e da jt9.

La metrica che conta e' UNA: la **soglia in dB** (la SNR a cui decodifichi il 50%
dei segnali). Piu' bassa = piu' sensibile. Il conteggio totale dei decode NON
e' una metrica (e' gonfiato dai segnali facili): qui ogni slot contiene UN
segnale piantato a SNR noto, e si misura se lo prendi o no.

Uso tipico (PRIMA e DOPO una modifica al decoder, stesso seed di rumore):
    py bench.py --decodium <path ft8_stage_compare.exe> --profile harvest \
                --snr -16:-26:-1 --trials 30 --label baseline --with-jt9
    # ... applichi la modifica, ricompili ...
    py bench.py --decodium <...> --profile harvest --snr -16:-26:-1 --trials 30 \
                --label nuovo --with-jt9
    # confronti le due soglie: un guadagno reale = soglia piu' bassa di >=0.5 dB.

Nessuna dipendenza esterna: solo standard library.
"""

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

# ----------------------------------------------------------------------------
# Default dei tool (sovrascrivibili da CLI)
# ----------------------------------------------------------------------------
DEFAULT_FT8SIM = r"C:\WSJT\wsjtx\bin\ft8sim.exe"
DEFAULT_JT9 = r"C:\WSJT\wsjtx\bin\jt9.exe"
DEFAULT_WSJT_BIN = r"C:\WSJT\wsjtx\bin"

# Messaggio piantato di default = quello canonico dei test di sensibilita' WSJT-X.
# E' una risposta "CALL CALL GRID": l'AP generico (senza mycall) NON la inietta,
# quindi il decode resta cieco e onesto.
DEFAULT_MESSAGE = "K1ABC W9XYZ EN37"
DEFAULT_F0 = 1500.0

# ----------------------------------------------------------------------------
# Profili decoder Decodium -> opzioni di ft8_stage_compare.
# Tutti su --stages 4 (path stage4 reale di produzione) e banda piena 200..4000.
# ----------------------------------------------------------------------------
PROFILES = {
    # Operativo "leggero": depth 3, niente AP, niente harvest. La sensibilita'
    # di base che vede l'utente coi preset minimi.
    "prod": ["--stages", "4", "--depth", "3"],
    # Deep: depth 4 + AP generico + OSD ordine 4. Niente harvest.
    "deep": ["--stages", "4", "--depth", "4", "--lft8apon", "1",
             "--maxosd", "3", "--norder", "4"],
    # Harvest (il "GAL"): deep + subpass low-threshold + 3 cicli + soglie basse.
    # E' la configurazione di massima sensibilita' del fork.
    "harvest": ["--stages", "4", "--depth", "4", "--lft8apon", "1",
                "--subpass", "1", "--cycles", "3", "--low-thresholds", "1",
                "--rxfsens", "3", "--maxosd", "3", "--norder", "4"],
}

# Profili jt9 (riferimento). 'ref' = MTft8 deep stile JTDX.
JT9_PROFILES = {
    "prod": ["-8", "-d", "3"],
    "deep": ["-8", "-d", "3"],
    "harvest": ["-8", "-d", "3", "-M", "-E", "3", "-C", "3", "-R", "3"],
    "ref": ["-8", "-d", "3", "-M", "-E", "3", "-C", "3", "-R", "3"],
}

_WS_RE = re.compile(r"\s+")
_DECODED_RE = re.compile(r'decoded="([^"]*)"')
_DSNR_RE = re.compile(r"snr=\s*(-?\d+)")
# jt9 stdout: "000001 -18  0.0 1500 ~  K1ABC W9XYZ EN37"
_JT9_RE = re.compile(r"^\s*\d{6}\s+(-?\d+)\s+[-\d.]+\s+\d+\s+~\s+(.*\S)\s*$")


def norm_msg(s):
    """Normalizza un messaggio per il confronto: maiuscolo, spazi singoli, trim."""
    return _WS_RE.sub(" ", s.strip().upper())


# ----------------------------------------------------------------------------
# Generazione segnale
# ----------------------------------------------------------------------------
def gen_signals(ft8sim, message, snr, n, f0, dt, fdop, delay, outdir):
    """Genera n .wav a SNR dato. Ritorna la lista dei path .wav generati."""
    for old in glob.glob(os.path.join(outdir, "*.wav")):
        os.remove(old)
    cmd = [ft8sim, message, str(f0), str(dt), str(fdop), str(delay),
           str(n), str(snr)]
    res = subprocess.run(cmd, cwd=outdir, capture_output=True, text=True)
    if res.returncode != 0:
        raise RuntimeError("ft8sim fallito (snr=%s): %s" % (snr, res.stderr or res.stdout))
    wavs = sorted(glob.glob(os.path.join(outdir, "*.wav")))
    if len(wavs) != n:
        raise RuntimeError("ft8sim: attesi %d wav, trovati %d (snr=%s)"
                           % (n, len(wavs), snr))
    return wavs


# ----------------------------------------------------------------------------
# Decodifica Decodium
# ----------------------------------------------------------------------------
def decode_decodium(decodium, wav, opts, nfqso, nfa, nfb, max_ms, timeout):
    """Ritorna (decoded_set, snr_map) dal decoder Decodium su un wav."""
    cmd = [decodium] + opts + ["--nfqso", str(nfqso), "--nfa", str(nfa),
                               "--nfb", str(nfb), "--max-ms", str(max_ms), wav]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return set(), {}
    out = res.stdout
    decoded = set()
    snr_map = {}
    for line in out.splitlines():
        m = _DECODED_RE.search(line)
        if not m:
            continue
        msg = norm_msg(m.group(1))
        if not msg:
            continue
        decoded.add(msg)
        ms = _DSNR_RE.search(line)
        if ms:
            snr_map[msg] = int(ms.group(1))
    return decoded, snr_map


# ----------------------------------------------------------------------------
# Decodifica jt9 (riferimento)
# ----------------------------------------------------------------------------
def decode_jt9(jt9, wav, opts, workdir, timeout):
    """Ritorna il set di messaggi decodificati da jt9 su un wav."""
    cmd = [jt9] + opts + ["-a", ".", "-t", ".", os.path.basename(wav)]
    try:
        res = subprocess.run(cmd, cwd=workdir, capture_output=True, text=True,
                             timeout=timeout)
    except subprocess.TimeoutExpired:
        return set()
    decoded = set()
    for line in res.stdout.splitlines():
        m = _JT9_RE.match(line)
        if m:
            decoded.add(norm_msg(m.group(2)))
    return decoded


# ----------------------------------------------------------------------------
# Soglia: SNR al 50% di P(decode), per interpolazione lineare.
# ----------------------------------------------------------------------------
def threshold_50(points):
    """points = lista (snr, p) ordinata per snr crescente. Ritorna soglia in dB
    o una stringa '< x' / '> x' se la curva non attraversa 0.5."""
    pts = sorted(points, key=lambda t: t[0])
    if not pts:
        return None
    # cerca il primo passaggio dal basso (p<0.5) verso (p>=0.5) salendo di SNR
    prev = None
    for snr, p in pts:
        if prev is not None:
            ps, pp = prev
            if pp < 0.5 <= p:
                if p == pp:
                    return float(snr)
                frac = (0.5 - pp) / (p - pp)
                return ps + frac * (snr - ps)
        prev = (snr, p)
    # nessun attraversamento
    if all(p >= 0.5 for _, p in pts):
        return "<= %g" % pts[0][0]  # gia' sopra il 50% al SNR piu' basso testato
    return ">= %g" % pts[-1][0]     # mai sopra il 50% fino al SNR piu' alto


def parse_snr_range(spec):
    """'-16:-26:-1' -> [-16,-17,...,-26]. Oppure CSV '-18,-20,-22'."""
    if ":" in spec:
        a, b, step = spec.split(":")
        a, b, step = float(a), float(b), float(step)
        out = []
        v = a
        # supporta sia passo negativo (16 -> -26) sia positivo
        n = int(round((b - a) / step)) + 1
        for i in range(n):
            out.append(round(a + i * step, 3))
        return out
    return [float(x) for x in spec.split(",")]


def bar(p, width=20):
    fill = int(round(p * width))
    return "#" * fill + "." * (width - fill)


def main():
    ap = argparse.ArgumentParser(
        description="Banco di misura sensibilita' FT8 in dB su .wav sintetici (ft8sim -> Decodium [-> jt9]).")
    ap.add_argument("--decodium", required=True,
                    help="Path a tests/ft8_stage_compare(.exe)")
    ap.add_argument("--ft8sim", default=DEFAULT_FT8SIM)
    ap.add_argument("--jt9", default=DEFAULT_JT9)
    ap.add_argument("--with-jt9", action="store_true",
                    help="Esegui anche jt9 come colonna di confronto.")
    ap.add_argument("--profile", default="deep", choices=list(PROFILES.keys()),
                    help="Profilo decoder Decodium (default: deep). NB: su segnale "
                         "singolo in AWGN harvest==deep; harvest conviene solo in "
                         "banda affollata (vedi README, modo multi-segnale).")
    ap.add_argument("--jt9-profile", default=None, choices=list(JT9_PROFILES.keys()),
                    help="Profilo jt9 (default: 'ref' = MTft8 deep).")
    ap.add_argument("--snr", default="-16:-26:-1",
                    help="Range SNR 'start:stop:step' o CSV (default -16:-26:-1).")
    ap.add_argument("--trials", type=int, default=30,
                    help="Realizzazioni di rumore per ogni SNR (default 30).")
    ap.add_argument("--message", default=DEFAULT_MESSAGE)
    ap.add_argument("--f0", type=float, default=DEFAULT_F0)
    ap.add_argument("--dt", type=float, default=0.2, help="Time offset ft8sim.")
    ap.add_argument("--fdop", type=float, default=0.0,
                    help="Doppler spread Hz (0=AWGN puro, ~1.0=fading HF).")
    ap.add_argument("--delay", type=float, default=0.0,
                    help="Multipath delay ms (0=nessuno).")
    ap.add_argument("--max-ms", type=int, default=8000,
                    help="Deadline decode Decodium in ms (default 8000; 0=illimitata, "
                         "lenta). Soft-cap: puo' sforare fra le passate.")
    ap.add_argument("--band", type=int, default=250,
                    help="Semi-banda Hz attorno a f0 per il decode (default 250). "
                         "Sul segnale singolo non cambia la sensibilita', ma "
                         "velocizza molto la ricerca candidati.")
    ap.add_argument("--early", action="store_true",
                    help="Includi le passate early sintetiche 41/47 (default: saltate "
                         "con --no-early per dimezzare i tempi; non servono alla soglia).")
    ap.add_argument("--timeout", type=int, default=180,
                    help="Timeout per singolo decode (s).")
    ap.add_argument("--label", default="run", help="Etichetta per CSV/output.")
    ap.add_argument("--workdir", default=None,
                    help="Cartella di lavoro per i wav (default: temp).")
    ap.add_argument("--csv", default=None, help="Path CSV di output.")
    ap.add_argument("--quick", action="store_true",
                    help="Preset rapido: SNR -18:-24:-2, trials 10.")
    args = ap.parse_args()

    if args.quick:
        args.snr = "-18:-24:-2"
        args.trials = 10

    jt9_profile = args.jt9_profile or ("ref" if args.profile == "harvest" else args.profile)

    decodium = os.path.abspath(args.decodium)
    if not os.path.isfile(decodium):
        sys.exit("ERRORE: ft8_stage_compare non trovato: %s" % decodium)
    if not os.path.isfile(args.ft8sim):
        sys.exit("ERRORE: ft8sim non trovato: %s" % args.ft8sim)
    if args.with_jt9 and not os.path.isfile(args.jt9):
        sys.exit("ERRORE: jt9 non trovato: %s" % args.jt9)

    own_workdir = args.workdir is None
    workdir = args.workdir or tempfile.mkdtemp(prefix="ft8bench_")
    os.makedirs(workdir, exist_ok=True)
    # ALLCALL7.TXT serve a jt9 per evitare un warning innocuo: copialo se c'e'.
    allcall = os.path.join(os.path.dirname(args.jt9), "ALLCALL7.TXT")
    if args.with_jt9 and os.path.isfile(allcall):
        try:
            shutil.copy(allcall, workdir)
        except OSError:
            pass

    snr_list = parse_snr_range(args.snr)
    target = norm_msg(args.message)
    opts = list(PROFILES[args.profile])
    if not args.early:
        opts.append("--no-early")
    jopts = JT9_PROFILES[jt9_profile]
    nfa = max(200, int(args.f0) - args.band)
    nfb = min(4000, int(args.f0) + args.band)

    print("=" * 72)
    print(" decode_bench  |  label=%s" % args.label)
    print("   messaggio : %r  f0=%.0f Hz  dt=%.1f  fdop=%.1f  del=%.1f"
          % (args.message, args.f0, args.dt, args.fdop, args.delay))
    print("   Decodium  : profile=%s  max-ms=%d  banda=%d..%d Hz  early=%s"
          % (args.profile, args.max_ms, nfa, nfb, ("si" if args.early else "no")))
    if args.with_jt9:
        print("   jt9 (ref) : profile=%s  (%s)" % (jt9_profile, " ".join(jopts)))
    print("   SNR sweep : %s  trials=%d  (banda rif. 2500 Hz)"
          % (snr_list, args.trials))
    print("=" * 72)
    hdr = "  SNR | D4 hits  P(D4)  | %s" % ("jt9 hits P(jt9)  |" if args.with_jt9 else "")
    print(hdr)
    print("  " + "-" * (len(hdr)))

    rows = []
    d_points = []
    j_points = []
    t0 = time.time()
    for snr in snr_list:
        wavs = gen_signals(args.ft8sim, args.message, snr, args.trials,
                           args.f0, args.dt, args.fdop, args.delay, workdir)
        d_hits = 0
        j_hits = 0
        for wav in wavs:
            dec, _ = decode_decodium(decodium, wav, opts, int(args.f0),
                                     nfa, nfb, args.max_ms, args.timeout)
            if target in dec:
                d_hits += 1
            if args.with_jt9:
                jdec = decode_jt9(args.jt9, wav, jopts, workdir, args.timeout)
                if target in jdec:
                    j_hits += 1
        dp = d_hits / args.trials
        jp = (j_hits / args.trials) if args.with_jt9 else None
        d_points.append((snr, dp))
        if args.with_jt9:
            j_points.append((snr, jp))
        rows.append((snr, d_hits, dp, j_hits, jp))
        if args.with_jt9:
            line = "  %4g | %4d/%-3d %5.2f %s | %4d/%-3d %5.2f %s" % (
                snr, d_hits, args.trials, dp, bar(dp, 12),
                j_hits, args.trials, jp, bar(jp, 12))
        else:
            line = "  %4g | %4d/%-3d %5.2f %s" % (
                snr, d_hits, args.trials, dp, bar(dp))
        print(line)

    d_thr = threshold_50(d_points)
    print("  " + "-" * (len(hdr)))
    def fmt_thr(t):
        return ("%.2f dB" % t) if isinstance(t, float) else str(t)
    print("  SOGLIA Decodium (%s) : %s" % (args.profile, fmt_thr(d_thr)))
    if args.with_jt9:
        j_thr = threshold_50(j_points)
        print("  SOGLIA jt9 (%s)      : %s" % (jt9_profile, fmt_thr(j_thr)))
        if isinstance(d_thr, float) and isinstance(j_thr, float):
            delta = j_thr - d_thr
            verdict = ("Decodium PIU' sensibile di %.2f dB" % delta) if delta > 0 \
                else ("jt9 piu' sensibile di %.2f dB" % (-delta)) if delta < 0 \
                else "pari"
            print("  DELTA (jt9 - Decodium): %+.2f dB  ->  %s" % (delta, verdict))
    print("  tempo totale: %.0f s" % (time.time() - t0))
    print("=" * 72)

    # CSV
    csv_path = args.csv or os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                        "results_%s.csv" % args.label)
    with open(csv_path, "w", encoding="utf-8") as f:
        f.write("label,profile,jt9_profile,message,fdop,delay,trials,snr,"
                "d_hits,d_p,j_hits,j_p\n")
        for snr, dh, dp, jh, jp in rows:
            f.write("%s,%s,%s,%s,%g,%g,%d,%g,%d,%.4f,%s,%s\n" % (
                args.label, args.profile, jt9_profile, args.message.replace(",", " "),
                args.fdop, args.delay, args.trials, snr, dh, dp,
                (str(jh) if jp is not None else ""),
                ("%.4f" % jp if jp is not None else "")))
    print("CSV -> %s" % csv_path)

    if own_workdir:
        shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    main()
