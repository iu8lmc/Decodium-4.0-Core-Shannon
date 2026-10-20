#!/usr/bin/env python3
# Confronto Decodium vs JTDX su una finestra UTC.
# Match cross-decoder per chiave (slot_utc, messaggio_normalizzato): due programmi
# che decodificano la stessa trasmissione producono lo stesso messaggio nello stesso slot.
import sys, re

DECO = r"C:\Users\IU8LMC\AppData\Roaming\IU8LMC\Decodium\all.txt"
JTDX = r"C:\Users\IU8LMC\AppData\Local\JTDX\202606_ALL.TXT"

def norm_msg(m):
    m = re.sub(r"\s+", " ", m.strip().upper())
    # rimuovi marker finali JTDX (^ * # $ & ? e codici a1/q?) ripetuti
    m = re.sub(r"(\s+[\^\*\#\$\&\?])+\s*$", "", m)
    m = re.sub(r"\s+(A\d|Q[0-9?])\s*$", "", m)  # eventuali codici AP-type in coda
    return m.strip()

def parse(path, w0, w1, label, target_yymmdd):
    """Ritorna dict key->snr per le righe RX nella finestra [w0,w1] (UTC HHMMSS int)
    del giorno target_yymmdd (6 cifre)."""
    keys = {}
    n_raw = 0
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.rstrip("\n")
            # formato: [YY]YYMMDD_HHMMSS  <mhz>  Rx  <mode>  <snr>  <dt>  <freq>  <msg>
            #   JTDX:  20260613_114145 -24  0.1 1188 ~ CQ ...   (no "Rx", token diversi)
            m = re.match(r"^(\d{6,8})_(\d{6})\b(.*)$", line)
            if not m:
                continue
            date_str = m.group(1)[-6:]  # normalizza a YYMMDD
            if date_str != target_yymmdd:
                continue
            hhmmss = int(m.group(2))
            if hhmmss < w0 or hhmmss > w1:
                continue
            rest = m.group(3)
            # estrai snr (primo intero con segno) e il messaggio (dopo ~ se JTDX, o dopo freq)
            # SNR: primo numero -?\d+ nei primi token
            toks = rest.split()
            snr = None
            for t in toks[:6]:
                if re.fullmatch(r"[+-]?\d+", t):
                    iv = int(t)
                    if -30 <= iv <= 60:  # range SNR plausibile
                        snr = iv
                        break
            # messaggio: per JTDX dopo '~'; per Decodium dopo "Rx <mode> snr dt freq"
            msg = None
            if "~" in rest:
                msg = rest.split("~", 1)[1]
            else:
                # Decodium: prendi dopo l'ultima occorrenza di freq audio (3-4 cifre) ... più robusto:
                # togli i primi token numerici/mode e tieni il resto come messaggio
                mm = re.search(r"\bRx\b\s+\S+\s+[+-]?\d+\s+[-\d.]+\s+\d+\s+(.*)$", rest)
                if mm:
                    msg = mm.group(1)
            if not msg:
                continue
            msg = norm_msg(msg)
            if not msg or msg in ("RX", "TX"):
                continue
            # ignora righe di TX proprie
            n_raw += 1
            key = (hhmmss // 15, msg)  # raggruppa per slot 15s + messaggio
            # tieni lo snr migliore (più alto) per la chiave
            if key not in keys or (snr is not None and snr > keys[key]):
                keys[key] = snr if snr is not None else -99
    return keys, n_raw

def main():
    w0 = int(sys.argv[1]) if len(sys.argv) > 1 else 114300
    w1 = int(sys.argv[2]) if len(sys.argv) > 2 else 121100
    day = sys.argv[3] if len(sys.argv) > 3 else "260613"
    dk, draw = parse(DECO, w0, w1, "DECO", day)
    jk, jraw = parse(JTDX, w0, w1, "JTDX", day)
    ds, js = set(dk), set(jk)
    inter = ds & js
    deco_only = ds - js
    jtdx_only = js - ds
    def weak(keys, kset, thr=-18):
        return sum(1 for k in kset if keys.get(k, -99) <= thr)
    print(f"=== Finestra UTC {w0} -> {w1} ===")
    print(f"righe RX grezze:   Decodium={draw}  JTDX={jraw}")
    print(f"decode unici:      Decodium={len(ds)}  JTDX={len(js)}   ratio={len(ds)/max(1,len(js))*100:.1f}%")
    print(f"in comune:         {len(inter)}")
    print(f"solo Decodium:     {len(deco_only)}   (Decodium li prende, JTDX no)")
    print(f"solo JTDX (gap):   {len(jtdx_only)}   (JTDX li prende, Decodium no)")
    print(f"--- deboli (SNR<=-18) ---")
    print(f"Decodium deboli:   {weak(dk, ds)}")
    print(f"JTDX deboli:       {weak(jk, js)}")
    print(f"gap deboli (soloJTDX & debole in JTDX): {weak(jk, jtdx_only)}")
    print(f"recuperati deboli (soloDeco & debole):  {weak(dk, deco_only)}")
    # campione del gap per ispezione
    sample = sorted(jtdx_only, key=lambda k: jk.get(k,-99))[:15]
    print("--- campione gap JTDX-only (più deboli) ---")
    for slot, msg in sample:
        print(f"   snr={jk[(slot,msg)]:>4}  {msg}")

if __name__ == "__main__":
    main()
