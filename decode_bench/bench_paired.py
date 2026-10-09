#!/usr/bin/env python3
"""
decode_bench/bench_paired.py — confronto APPAIATO fra due build del decoder.

Perche' esiste: ft8sim NON e' deterministico (verificato 2026-08-27: due
invocazioni con gli stessi identici parametri producono wav con md5 diversi).
Quindi due run separati di bench.py sulle due build confrontano soglie misurate
su realizzazioni di rumore DIVERSE, e con poche prove la differenza fra le due
soglie e' dominata dal rumore statistico.

Qui i wav si generano UNA VOLTA per ogni SNR e si danno agli STESSI file a
entrambi i binari. Il confronto diventa appaiato: la varianza del rumore si
cancella, e le discordanze (uno decodifica, l'altro no) si contano una per una
e si valutano col test dei segni di McNemar.

Uso:
    py bench_paired.py --a <exe_A> --b <exe_B> --label-a base --label-b fix \
                       --snr -18:-24:-1 --trials 30 --profile deep
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bench  # riusa profili, generazione, parsing e soglia


def mcnemar_sign_p(b, c):
    """p-value esatto a due code del test dei segni sulle discordanze.

    b = casi vinti da A e persi da B, c = il contrario. Sotto l'ipotesi nulla
    (le due build sono equivalenti) ogni discordanza e' una moneta equa.
    """
    n = b + c
    if n == 0:
        return 1.0
    from math import comb
    k = min(b, c)
    tail = sum(comb(n, i) for i in range(0, k + 1))
    return min(1.0, 2.0 * tail / (2 ** n))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", required=True, help="Path build A (riferimento).")
    ap.add_argument("--b", required=True, help="Path build B (candidata).")
    ap.add_argument("--label-a", default="A")
    ap.add_argument("--label-b", default="B")
    ap.add_argument("--ft8sim", default=bench.DEFAULT_FT8SIM)
    ap.add_argument("--profile", default="deep", choices=list(bench.PROFILES.keys()))
    ap.add_argument("--snr", default="-18:-24:-1")
    ap.add_argument("--trials", type=int, default=30)
    ap.add_argument("--message", default=bench.DEFAULT_MESSAGE)
    ap.add_argument("--f0", type=float, default=bench.DEFAULT_F0)
    ap.add_argument("--dt", type=float, default=0.2)
    ap.add_argument("--fdop", type=float, default=0.0)
    ap.add_argument("--delay", type=float, default=0.0)
    ap.add_argument("--max-ms", type=int, default=8000)
    ap.add_argument("--band", type=int, default=250)
    ap.add_argument("--timeout", type=int, default=180)
    ap.add_argument("--csv", default=None)
    ap.add_argument("--env-a", default="",
                    help="Variabili d'ambiente per il solo binario A, forma NOME=VAL,NOME=VAL. "
                         "Serve a confrontare la STESSA build in due configurazioni scelte a "
                         "runtime, sugli stessi wav.")
    ap.add_argument("--env-b", default="",
                    help="Come --env-a, per il binario B.")
    ap.add_argument("--extra", default="",
                    help="Opzioni extra passate a ft8_stage_compare, es. "
                         "\"--maxosd 3 --norder 3\". Vengono APPESE al profilo, "
                         "quindi sovrascrivono i valori del profilo stesso "
                         "(l'ultima occorrenza vince nel parser Qt).")
    args = ap.parse_args()

    for p in (args.a, args.b):
        if not os.path.isfile(p):
            sys.exit("ERRORE: binario non trovato: %s" % p)

    opts = list(bench.PROFILES[args.profile])
    if args.extra.strip():
        opts += args.extra.split()
    nfa = int(args.f0 - args.band)
    nfb = int(args.f0 + args.band)
    snrs = bench.parse_snr_range(args.snr)
    target = bench.norm_msg(args.message)

    print("=" * 72)
    print(" decode_bench PAIRED  |  A=%s  B=%s" % (args.label_a, args.label_b))
    print("   messaggio : '%s'  f0=%g Hz  dt=%g" % (args.message, args.f0, args.dt))
    print("   profilo   : %s%s  max-ms=%d  banda=%d..%d Hz"
          % (args.profile, ("  extra=[%s]" % args.extra.strip()) if args.extra.strip() else "",
             args.max_ms, nfa, nfb))
    print("   SNR sweep : %s  trials=%d  (STESSI wav per entrambe le build)"
          % (snrs, args.trials))
    print("=" * 72)

    def parse_env(spec):
        env = dict(os.environ)
        for item in spec.split(","):
            item = item.strip()
            if not item:
                continue
            k, _, v = item.partition("=")
            env[k.strip()] = v.strip()
        return env

    env_a = parse_env(args.env_a)
    env_b = parse_env(args.env_b)

    def decode_with(exe, wav, env):
        """Come bench.decode_decodium, ma con un ambiente scelto per processo."""
        cmd = [exe] + opts + ["--nfqso", str(int(args.f0)), "--nfa", str(nfa),
                              "--nfb", str(nfb), "--max-ms", str(args.max_ms), wav]
        try:
            res = subprocess.run(cmd, capture_output=True, text=True,
                                 timeout=args.timeout, env=env)
        except subprocess.TimeoutExpired:
            return set()
        out = set()
        for line in res.stdout.splitlines():
            m = bench._DECODED_RE.search(line)
            if m:
                msg = bench.norm_msg(m.group(1))
                if msg:
                    out.add(msg)
        return out

    workdir = tempfile.mkdtemp(prefix="benchpair_")
    rows = []
    tot_b_only = tot_c_only = tot_both = tot_none = 0
    t0 = time.time()
    try:
        print("  SNR |  %-9s |  %-9s | solo A  solo B" % (args.label_a, args.label_b))
        print("  " + "-" * 52)
        for snr in snrs:
            wavs = bench.gen_signals(args.ft8sim, args.message, snr, args.trials,
                                     args.f0, args.dt, args.fdop, args.delay, workdir)
            hits_a = hits_b = only_a = only_b = both = none = 0
            for w in wavs:
                # --nfqso vuole un INTERO: passando il float 1500.0 il
                # decoder esce subito con "invalid --nfqso value" e ogni
                # prova risulta un non-decode silenzioso.
                da = decode_with(args.a, w, env_a)
                db = decode_with(args.b, w, env_b)
                ga, gb = target in da, target in db
                hits_a += ga
                hits_b += gb
                if ga and gb:
                    both += 1
                elif ga:
                    only_a += 1
                elif gb:
                    only_b += 1
                else:
                    none += 1
            pa = hits_a / float(args.trials)
            pb = hits_b / float(args.trials)
            tot_b_only += only_a
            tot_c_only += only_b
            tot_both += both
            tot_none += none
            rows.append((snr, hits_a, pa, hits_b, pb, only_a, only_b))
            print("  %4g | %3d/%-3d %4.2f | %3d/%-3d %4.2f |   %2d      %2d"
                  % (snr, hits_a, args.trials, pa, hits_b, args.trials, pb,
                     only_a, only_b))

        print("  " + "-" * 52)
        th_a = bench.threshold_50([(r[0], r[2]) for r in rows])
        th_b = bench.threshold_50([(r[0], r[4]) for r in rows])
        fmt = lambda t: ("%.2f dB" % t) if isinstance(t, float) else str(t)
        print("  SOGLIA %-10s : %s" % (args.label_a, fmt(th_a)))
        print("  SOGLIA %-10s : %s" % (args.label_b, fmt(th_b)))
        if isinstance(th_a, float) and isinstance(th_b, float):
            d = th_a - th_b
            if abs(d) < 0.005:
                verso = "soglie coincidenti"
            elif d > 0:
                verso = "%s piu' sensibile di %.2f dB" % (args.label_b, d)
            else:
                verso = "%s piu' sensibile di %.2f dB" % (args.label_a, -d)
            print("  differenza          : %+.2f dB  (%s)" % (d, verso))
        p = mcnemar_sign_p(tot_b_only, tot_c_only)
        print("  discordanze         : solo %s=%d  solo %s=%d  (entrambi=%d, nessuno=%d)"
              % (args.label_a, tot_b_only, args.label_b, tot_c_only, tot_both, tot_none))
        print("  McNemar (segni)     : p = %.4f  ->  %s"
              % (p, "differenza significativa" if p < 0.05
                 else "NON distinguibili con questi dati"))
        print("  tempo totale: %d s" % int(time.time() - t0))
        print("=" * 72)

        csv_path = args.csv or os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                            "results_paired_%s_vs_%s.csv"
                                            % (args.label_a, args.label_b))
        with open(csv_path, "w", encoding="utf-8") as f:
            f.write("snr,hits_%s,p_%s,hits_%s,p_%s,only_%s,only_%s,trials\n"
                    % (args.label_a, args.label_a, args.label_b, args.label_b,
                       args.label_a, args.label_b))
            for r in rows:
                f.write("%g,%d,%.4f,%d,%.4f,%d,%d,%d\n"
                        % (r[0], r[1], r[2], r[3], r[4], r[5], r[6], args.trials))
        print("CSV -> %s" % csv_path)
    finally:
        shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    main()
