#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
decode_bench/bench_real.py — Confronto su CATTURE REALI off-air.

Decodifica i TUOI .wav veri (12 kHz mono 16-bit, slot da 15 s — le registrazioni
salvate da Decodium/WSJT-X) con piu' profili Decodium (`deep` vs `harvest`) e,
opzionale, con `jt9` (riferimento JTDX-style), e CONFRONTA gli insiemi di decode
sugli STESSI file. E' il metodo della nota di parita' JTDX (confronto slot-per-
slot), ma automatizzato.

Niente verita' di terra (segnali veri, contenuto ignoto) -> la misura e'
COMPARATIVA, non assoluta. Ma con CRC-14 i falsi decode sono ~1/10^4: i decode
che un profilo prende e un altro no sono quasi tutti **stazioni vere** recuperate.
Quindi:
  - harvest-only  = stazioni che harvest estrae e deep no  -> apporto reale di harvest
  - decodium-only / jt9-only = parita' vs il riferimento
  - la distribuzione SNR dei "harvest-only" mostra se harvest scava sui deboli.

A differenza dei banchi sintetici NON dà una soglia in dB (SNR vero ignoto), ma
dà il conteggio marginale reale, che e' cio' che conta in banda affollata.

Uso:
    py bench_real.py --decodium <ft8_stage_compare.exe> --wavs <dir-o-glob> \
       --profiles deep,harvest --with-jt9 --limit 50 --label run1

Dove prendere i .wav: la cartella "save" di WSJT-X / Decodium (registrazioni slot).
Tipico WSJT-X: %LOCALAPPDATA%/WSJT-X/save  — Decodium: cartella di salvataggio analoga.
Servono .wav 12000 Hz mono 16-bit con timestamp nel nome (YYMMDD_HHMMSS.wav) per
inferire l'ora (il decoder la ricava dal nome file).

Solo standard library (riusa gli helper di bench.py).
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

from bench import decode_decodium, norm_msg, PROFILES, JT9_PROFILES, DEFAULT_JT9

# jt9 stdout: "000001 -18  0.0 1500 ~  K1ABC W9XYZ EN37"
_JT9_RE = re.compile(r"^\s*\d{6}\s+(-?\d+)\s+[-\d.]+\s+\d+\s+~\s+(.*\S)\s*$")


def find_wavs(spec, limit):
    if os.path.isdir(spec):
        wavs = sorted(glob.glob(os.path.join(spec, "*.wav")))
    else:
        wavs = sorted(glob.glob(spec))
    if limit and limit > 0:
        wavs = wavs[:limit]
    return wavs


def decode_jt9_path(jt9, wav_fullpath, opts, workdir, timeout):
    """Decodifica un wav (path completo) con jt9; ritorna (set_msg, snr_map)."""
    cmd = [jt9] + opts + ["-a", ".", "-t", ".", os.path.abspath(wav_fullpath)]
    try:
        res = subprocess.run(cmd, cwd=workdir, capture_output=True, text=True,
                             timeout=timeout)
    except subprocess.TimeoutExpired:
        return set(), {}
    decoded = set()
    snr_map = {}
    for line in res.stdout.splitlines():
        m = _JT9_RE.match(line)
        if m:
            msg = norm_msg(m.group(2))
            decoded.add(msg)
            snr_map.setdefault(msg, int(m.group(1)))
    return decoded, snr_map


def quantiles(values):
    if not values:
        return (None, None, None)
    v = sorted(values)
    n = len(v)
    return (v[0], v[n // 2], v[-1])


def main():
    ap = argparse.ArgumentParser(
        description="Confronto decode su CATTURE REALI: deep vs harvest [vs jt9].")
    ap.add_argument("--decodium", required=True)
    ap.add_argument("--wavs", required=True,
                    help="Cartella di .wav o glob (es. 'C:/save/*.wav').")
    ap.add_argument("--jt9", default=DEFAULT_JT9)
    ap.add_argument("--with-jt9", action="store_true")
    ap.add_argument("--profiles", default="deep,harvest",
                    help="Profili Decodium da confrontare (default deep,harvest). "
                         "Il primo e' il riferimento per i confronti a coppie.")
    ap.add_argument("--nfa", type=int, default=200, help="Banda bassa Hz (default 200).")
    ap.add_argument("--nfb", type=int, default=4000, help="Banda alta Hz (default 4000).")
    ap.add_argument("--nfqso", type=int, default=1500)
    ap.add_argument("--max-ms", type=int, default=20000,
                    help="Deadline decode in ms (default 20000; banda piena reale e' "
                         "pesante. 0=illimitata = molto lenta).")
    ap.add_argument("--limit", type=int, default=50,
                    help="Max numero di file (default 50; 0=tutti).")
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--label", default="real")
    ap.add_argument("--csv", default=None)
    args = ap.parse_args()

    decodium = os.path.abspath(args.decodium)
    if not os.path.isfile(decodium):
        sys.exit("ERRORE: ft8_stage_compare non trovato: %s" % decodium)
    if args.with_jt9 and not os.path.isfile(args.jt9):
        sys.exit("ERRORE: jt9 non trovato: %s" % args.jt9)

    profiles = [p.strip() for p in args.profiles.split(",") if p.strip()]
    for p in profiles:
        if p not in PROFILES:
            sys.exit("ERRORE: profilo sconosciuto '%s' (validi: %s)" % (p, ",".join(PROFILES)))

    wavs = find_wavs(args.wavs, args.limit)
    if not wavs:
        sys.exit("ERRORE: nessun .wav trovato in %r" % args.wavs)

    workdir = tempfile.mkdtemp(prefix="ft8real_")
    allcall = os.path.join(os.path.dirname(args.jt9), "ALLCALL7.TXT")
    if args.with_jt9 and os.path.isfile(allcall):
        try:
            shutil.copy(allcall, workdir)
        except OSError:
            pass

    print("=" * 78)
    print(" decode_bench REAL  |  label=%s" % args.label)
    print("   file        : %d (%s)" % (len(wavs), args.wavs))
    print("   profili     : %s%s" % (",".join(profiles), "  + jt9" if args.with_jt9 else ""))
    print("   banda       : %d..%d Hz   max-ms=%d   nfqso=%d"
          % (args.nfa, args.nfb, args.max_ms, args.nfqso))
    print("=" * 78)

    # insiemi: chiave = (basename_file, messaggio_normalizzato)
    sets = {p: set() for p in profiles}
    snrs = {p: {} for p in profiles}      # key -> snr
    jset = set()
    jsnr = {}
    t0 = time.time()
    for i, wav in enumerate(wavs):
        base = os.path.basename(wav)
        for p in profiles:
            opts = list(PROFILES[p]) + ["--no-early"]
            dec, smap = decode_decodium(decodium, wav, opts, args.nfqso,
                                        args.nfa, args.nfb, args.max_ms, args.timeout)
            for m in dec:
                k = (base, m)
                sets[p].add(k)
                if m in smap:
                    snrs[p][k] = smap[m]
        if args.with_jt9:
            jdec, jsm = decode_jt9_path(args.jt9, wav, JT9_PROFILES["ref"], workdir, args.timeout)
            for m in jdec:
                k = (base, m)
                jset.add(k)
                if m in jsm:
                    jsnr[k] = jsm[m]
        if (i + 1) % 10 == 0 or i + 1 == len(wavs):
            print("  ... %d/%d file (%.0fs)" % (i + 1, len(wavs), time.time() - t0))

    print("-" * 78)
    print("  Decode unici (aggregato su tutti i file):")
    ref = profiles[0]
    for p in profiles:
        extra = ""
        if p != ref:
            extra = "   (%+d vs %s)" % (len(sets[p]) - len(sets[ref]), ref)
        print("    %-8s : %d%s" % (p, len(sets[p]), extra))
    if args.with_jt9:
        print("    %-8s : %d" % ("jt9", len(jset)))

    # confronti a coppie vs il primo profilo
    print("  ---")
    for p in profiles[1:]:
        common = sets[ref] & sets[p]
        only_p = sets[p] - sets[ref]
        only_ref = sets[ref] - sets[p]
        print("  %s vs %s:  comuni=%d  %s-only=%d  %s-only=%d"
              % (p, ref, len(common), p, len(only_p), ref, len(only_ref)))
        # SNR dei marginali (presi da p e mancati da ref) = i deboli che p scava
        marg_snr = [snrs[p][k] for k in only_p if k in snrs[p]]
        lo, md, hi = quantiles(marg_snr)
        if marg_snr:
            print("    SNR dei %s-only (marginali): min/mediana/max = %s/%s/%s dB  (n=%d)"
                  % (p, lo, md, hi, len(marg_snr)))

    if args.with_jt9:
        print("  ---")
        # Decodium "migliore" = unione di tutti i profili
        dunion = set()
        for p in profiles:
            dunion |= sets[p]
        common = dunion & jset
        only_d = dunion - jset
        only_j = jset - dunion
        print("  Decodium(unione) vs jt9:  comuni=%d  decodium-only=%d  jt9-only=%d"
              % (len(common), len(only_d), len(only_j)))
        # anche il singolo profilo harvest vs jt9, se presente
        if "harvest" in profiles:
            h = sets["harvest"]
            print("  harvest vs jt9:           comuni=%d  harvest-only=%d  jt9-only=%d"
                  % (len(h & jset), len(h - jset), len(jset - h)))

    print("  tempo totale: %.0f s" % (time.time() - t0))
    print("=" * 78)

    # CSV per drill-down: una riga per (file,msg) con chi l'ha preso e a che SNR
    csv_path = args.csv or os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                        "results_real_%s.csv" % args.label)
    all_keys = set()
    for p in profiles:
        all_keys |= sets[p]
    if args.with_jt9:
        all_keys |= jset
    with open(csv_path, "w", encoding="utf-8") as f:
        cols = ["file", "msg"] + ["got_%s" % p for p in profiles] + ["snr_%s" % p for p in profiles]
        if args.with_jt9:
            cols += ["got_jt9", "snr_jt9"]
        f.write(",".join(cols) + "\n")
        for (base, msg) in sorted(all_keys):
            row = [base, msg.replace(",", " ")]
            for p in profiles:
                row.append("1" if (base, msg) in sets[p] else "0")
            for p in profiles:
                row.append(str(snrs[p].get((base, msg), "")))
            if args.with_jt9:
                row.append("1" if (base, msg) in jset else "0")
                row.append(str(jsnr.get((base, msg), "")))
            f.write(",".join(row) + "\n")
    print("CSV -> %s" % csv_path)

    shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    main()
