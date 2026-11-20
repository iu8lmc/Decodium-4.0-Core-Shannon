#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Passa gli slot con verita' nota in Decodium e misura che cosa succede.

Tre misure, non una:
  * DECODIFICA: il messaggio giusto e' uscito? (FER per livello di SNR)
  * AGGANCIO:   il demodulatore ha trovato il candidato giusto, e con quale
                errore di frequenza e DT? E' il dato che una simulazione pura
                non da'.
  * FALSI:      righe uscite che non erano nella verita'.

Gli slot li fa mescola_ft2.py; il dump degli LLR (doc/fastldpc/llr_dump.md) e'
opzionale e serve ai confronti offline fra decodificatori.

  python verita_ft2.py --slot cartella [--llr file.bin] [--ndepth 3]
"""
import argparse
import json
import os
import re
import subprocess
import sys

BANCO = os.path.join(os.environ.get("DECODIUM_AI_BUILD", "build_689"), "tests", "ft4_stage_compare.exe")
RE_FILE = re.compile(r"^file: (.*)$")
RE_DEC = re.compile(r'decoded="([^"]*)"')
RE_FREQ = re.compile(r"freq=\s*(-?\d+)")
RE_DT = re.compile(r"dt=\s*(-?[\d.]+)")
RE_SNR = re.compile(r"snr=\s*(-?\d+)")


def _un_file(cartella, nome, ndepth, mycall, env):
    args = [BANCO, "--stages", "4", "--depth", str(ndepth), "--nfa", "300", "--nfb", "2800",
            "--mycall", mycall, nome]
    p = subprocess.run(args, cwd=cartella, capture_output=True, text=True,
                       encoding="utf-8", errors="replace", env=env)
    out = []
    for riga in (p.stdout or "").splitlines():
        m = RE_DEC.search(riga)
        if m:
            f = RE_FREQ.search(riga)
            d = RE_DT.search(riga)
            s = RE_SNR.search(riga)
            out.append({
                "msg": " ".join(m.group(1).split()),
                "freq": float(f.group(1)) if f else None,
                "dt": float(d.group(1)) if d else None,
                "snr": int(s.group(1)) if s else None,
            })
    return nome, out


def esegui(cartella, files, ndepth, llr, mycall, ap_msg):
    """ft4_stage_compare takes ONE wav per run (unlike ft2_stage_compare), so one process per slot."""
    import concurrent.futures as cf
    env = dict(os.environ)
    env["PATH"] = r"C:\msys64\mingw64\bin;" + env.get("PATH", "")
    with cf.ThreadPoolExecutor(max_workers=8) as ex:
        return dict(ex.map(lambda f: _un_file(cartella, f, ndepth, mycall, env), files))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--slot", required=True, help="cartella con verita.json e slot_*.wav")
    ap.add_argument("--llr", default=None, help="file di dump degli LLR da scrivere")
    ap.add_argument("--ndepth", type=int, default=3)
    ap.add_argument("--mycall", default="IU8LMC")
    ap.add_argument("--blocco", type=int, default=60, help="slot per invocazione del banco")
    ap.add_argument("--ap-msg", type=int, default=0, choices=[0, 1],
                    help="tipo 8 fra slot: 0 (default) perche' gli slot sono indipendenti")
    a = ap.parse_args()

    verita = json.load(open(os.path.join(a.slot, "verita.json"), encoding="utf-8"))
    files = [v["file"] for v in verita]
    righe = {}
    for i in range(0, len(files), a.blocco):
        righe.update(esegui(a.slot, files[i:i + a.blocco], a.ndepth, a.llr, a.mycall, a.ap_msg))
        print("  banco: %d/%d slot" % (min(i + a.blocco, len(files)), len(files)), flush=True)

    livelli = {}
    falsi = []
    for v in verita:
        out = righe.get(v["file"], [])
        liv = livelli.setdefault(v["snr_db"], {"n": 0, "ok": 0, "err_f": [], "err_dt": [], "snr": []})
        liv["n"] += 1
        giusta = [r for r in out if r["msg"] == v["messaggio"]]
        if giusta:
            liv["ok"] += 1
            r = giusta[0]
            if r["freq"] is not None:
                liv["err_f"].append(r["freq"] - v["freq_hz"])
            if r["dt"] is not None:
                liv["err_dt"].append(r["dt"] - v["dt_s"])
            if r["snr"] is not None:
                liv["snr"].append(r["snr"])
        for r in out:
            if r["msg"] != v["messaggio"]:
                falsi.append((v["file"], r["msg"], r["snr"]))

    def mediana(x):
        return sorted(x)[len(x) // 2] if x else float("nan")

    print("\n| SNR chiesto | slot | decodificati | FER | SNR riportato | err freq | err DT |")
    print("|---|---|---|---|---|---|---|")
    for snr in sorted(livelli, reverse=True):
        d = livelli[snr]
        print("| %+.0f dB | %d | %d | %.2f | %s | %s | %s |" % (
            snr, d["n"], d["ok"], 1.0 - d["ok"] / max(1, d["n"]),
            ("%+.0f" % mediana(d["snr"])) if d["snr"] else "-",
            ("%+.1f Hz" % mediana(d["err_f"])) if d["err_f"] else "-",
            ("%+.3f s" % mediana(d["err_dt"])) if d["err_dt"] else "-"))
    print("\nrighe non previste: %d" % len(falsi))
    for f, m, s in falsi[:15]:
        print("   %s  \"%s\"  snr %s" % (f, m, s))

    if a.llr:
        aggancio(a.slot, verita, righe, a.llr)


def aggancio(cartella, verita, righe, percorso_llr):
    """Il demodulatore ha trovato il candidato giusto? E' la domanda che la
    decodifica da sola non risponde: un frame perso puo' essere perso dal
    decodificatore (il candidato c'era) oppure dal sincronismo (non c'era)."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import leggi_llr

    rec = list(leggi_llr.leggi(percorso_llr))
    cicli = {}
    for r in rec:
        cicli.setdefault(round(r["t"], 6), []).append(r)
    ordine = sorted(cicli)
    if len(ordine) != len(verita):
        print("\naggancio: %d cicli nel dump contro %d slot, salto l'analisi"
              % (len(ordine), len(verita)))
        return

    # Due tolleranze, non una: entro 5 Hz il candidato e' utilizzabile (il
    # sincronismo FT2 lavora entro +-12-16 Hz); fra 5 e 40 Hz il candidato
    # c'e' ma e' SPOSTATO, e il decodificatore parte dal posto sbagliato. Sono
    # due guasti diversi e vanno contati separatamente.
    print("\n| SNR chiesto | slot | candidato entro 5 Hz | spostato 5-40 Hz | nessuno | err freq | err DT | candidati per slot |")
    print("|---|---|---|---|---|---|---|---|")
    per_livello = {}
    for v, t in zip(verita, ordine):
        d = per_livello.setdefault(v["snr_db"], {"n": 0, "visto": 0, "spostato": 0,
                                                 "ef": [], "edt": [], "cand": []})
        d["n"] += 1
        d["cand"].append(len(cicli[t]))
        vicini = [r for r in cicli[t] if abs(r["freq"] - v["freq_hz"]) <= 5.0]
        larghi = [r for r in cicli[t] if 5.0 < abs(r["freq"] - v["freq_hz"]) <= 40.0]
        if vicini:
            r = min(vicini, key=lambda x: abs(x["freq"] - v["freq_hz"]))
            d["visto"] += 1
            d["ef"].append(r["freq"] - v["freq_hz"])
            d["edt"].append(r["dt"] - v["dt_s"])
        elif larghi:
            d["spostato"] += 1
    for snr in sorted(per_livello, reverse=True):
        d = per_livello[snr]
        med = lambda x: sorted(x)[len(x) // 2] if x else float("nan")
        niente = d["n"] - d["visto"] - d["spostato"]
        print("| %+.0f dB | %d | %d (%.0f%%) | %d (%.0f%%) | %d (%.0f%%) | %+.1f Hz | %+.3f s | %.0f |" % (
            snr, d["n"], d["visto"], 100.0 * d["visto"] / max(1, d["n"]),
            d["spostato"], 100.0 * d["spostato"] / max(1, d["n"]),
            niente, 100.0 * niente / max(1, d["n"]),
            med(d["ef"]), med(d["edt"]), sum(d["cand"]) / max(1, len(d["cand"]))))


if __name__ == "__main__":
    main()
