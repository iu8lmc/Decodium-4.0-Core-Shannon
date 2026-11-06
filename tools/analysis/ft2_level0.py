#!/usr/bin/env python3
# Livello 0 - analisi log Decodium per valutare gestione FT2 (read-only)
import os, re, sys, time
from collections import Counter, defaultdict

HOME = os.path.expanduser("~")
ADI  = os.path.join(HOME, "AppData", "Local", "IU8LMC", "Decodium", "decodium_log.adi")
ALL  = os.path.join(HOME, "AppData", "Roaming", "IU8LMC", "Decodium", "all.txt")

def pct(sorted_vals, p):
    if not sorted_vals: return 0
    k = (len(sorted_vals)-1) * p
    f = int(k)
    c = min(f+1, len(sorted_vals)-1)
    if f == c: return sorted_vals[f]
    return sorted_vals[f] + (sorted_vals[c]-sorted_vals[f])*(k-f)

# ---------- PARTE A: ADI (QSO chiusi) ----------
print("="*70)
print("PARTE A - QSO CHIUSI (decodium_log.adi)")
print("="*70)
field_re = re.compile(r"<([A-Za-z_]+):(\d+)(?::[A-Za-z])?>([^<]*)")
qsos = []
with open(ADI, "r", encoding="utf-8", errors="replace") as f:
    data = f.read()
records = data.split("<EOR>")
for rec in records:
    fields = {}
    for m in field_re.finditer(rec):
        name = m.group(1).upper()
        ln   = int(m.group(2))
        val  = m.group(3)[:ln]
        fields[name] = val
    if "CALL" not in fields or "TIME_ON" not in fields:
        continue
    mode = fields.get("SUBMODE") or fields.get("MODE","")
    qsos.append(fields | {"_MODE": mode})

def secs(t):
    t = t.zfill(6)
    return int(t[0:2])*3600 + int(t[2:4])*60 + int(t[4:6])

def duration(q):
    on = secs(q.get("TIME_ON","0"))
    off = secs(q.get("TIME_OFF", q.get("TIME_ON","0")))
    d = off - on
    if d < 0: d += 86400   # wrap mezzanotte
    return d

by_mode = Counter(q["_MODE"] for q in qsos)
dates = sorted(set(q.get("QSO_DATE","") for q in qsos if q.get("QSO_DATE")))
print(f"QSO totali loggati : {len(qsos)}")
print(f"Intervallo date    : {dates[0] if dates else '?'} -> {dates[-1] if dates else '?'}")
print(f"Per modo           : " + ", ".join(f"{k}={v}" for k,v in by_mode.most_common()))
print()

for mode in ("FT2", "FT8", "FT4"):
    sub = [q for q in qsos if q["_MODE"] == mode]
    if not sub: continue
    durs = sorted(duration(q) for q in sub)
    rr = []
    for q in sub:
        v = q.get("RST_RCVD","")
        try: rr.append(int(v))
        except: pass
    rr.sort()
    long90  = sum(1 for d in durs if d > 90)
    long120 = sum(1 for d in durs if d > 120)
    long180 = sum(1 for d in durs if d > 180)
    print(f"--- {mode} ({len(sub)} QSO) ---")
    print(f"  durata s: min={durs[0]} mediana={pct(durs,.5):.0f} p75={pct(durs,.75):.0f} "
          f"p90={pct(durs,.9):.0f} p95={pct(durs,.95):.0f} max={durs[-1]}")
    print(f"  QSO 'lunghi': >90s={long90} ({100*long90/len(durs):.0f}%)  "
          f">120s={long120} ({100*long120/len(durs):.0f}%)  >180s={long180}")
    if rr:
        weak = sum(1 for x in rr if x <= -15)
        vweak= sum(1 for x in rr if x <= -20)
        print(f"  RST ricevuto: mediana={pct(rr,.5):.0f}dB p10={pct(rr,.1):.0f} min={rr[0]}  "
              f"deboli<=-15={weak} ({100*weak/len(rr):.0f}%) <=-20={vweak}")
    print()

# ---------- PARTE B: all.txt (traffico FT2 + opportunita' picker) ----------
print("="*70)
print("PARTE B - TRAFFICO FT2 (all.txt, streaming)")
print("="*70)
t0 = time.time()
line_re = re.compile(r"^(\d{6})_(\d{6})\s+[\d.]+\s+Rx\s+(\S+)\s+(-?\d+)\s+(-?[\d.]+)\s+(\d+)\s+(.*)$")
mode_lines = Counter()
ft2_total = 0
ft2_cq = 0
ft2_calls = set()
ft2_by_day = Counter()
# per periodo: timestamp -> set di callers CQ (per dimensionare il picker)
ft2_cq_per_period = defaultdict(set)
ft2_snr = []
CALLRE = re.compile(r"^[A-Z0-9]{1,3}[0-9][A-Z0-9]*[A-Z](?:/[A-Z0-9]+)?$")

n = 0
with open(ALL, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        n += 1
        m = line_re.match(line)
        if not m: continue
        day, hms, mode, snr, dt, hz, msg = m.groups()
        mode_lines[mode] += 1
        if mode != "FT2":
            continue
        ft2_total += 1
        ft2_by_day[day] += 1
        try: ft2_snr.append(int(snr))
        except: pass
        toks = msg.split()
        if toks and toks[0] == "CQ":
            ft2_cq += 1
            # caller = primo token "callsign" dopo CQ (salta POTA/DX/direzionali)
            caller = None
            for t in toks[1:]:
                if CALLRE.match(t):
                    caller = t; break
            if caller:
                ft2_calls.add(caller)
                key = day + "_" + hms
                ft2_cq_per_period[key].add(caller)
        else:
            for t in toks:
                if CALLRE.match(t):
                    ft2_calls.add(t)

elapsed = time.time() - t0
print(f"righe lette        : {n:,}  ({elapsed:.1f}s)")
print(f"decode per modo    : " + ", ".join(f"{k}={v:,}" for k,v in mode_lines.most_common()))
print()
print(f"FT2 decode totali  : {ft2_total:,}")
print(f"FT2 righe CQ       : {ft2_cq:,}")
print(f"FT2 call distinti  : {len(ft2_calls):,}")
if ft2_snr:
    ft2_snr.sort()
    print(f"FT2 SNR: mediana={pct(ft2_snr,.5):.0f}dB p10={pct(ft2_snr,.1):.0f} min={ft2_snr[0]} max={ft2_snr[-1]}")
print()
# opportunita' CQ-picker: distribuzione caller per periodo
if ft2_cq_per_period:
    sizes = Counter(len(v) for v in ft2_cq_per_period.values())
    tot_periods = len(ft2_cq_per_period)
    multi = sum(c for s,c in sizes.items() if s >= 2)
    multi3 = sum(c for s,c in sizes.items() if s >= 3)
    print(f"periodi FT2 con >=1 CQ : {tot_periods:,}")
    print(f"  con >=2 CQ contemporanei : {multi:,} ({100*multi/tot_periods:.0f}%)")
    print(f"  con >=3 CQ contemporanei : {multi3:,} ({100*multi3/tot_periods:.0f}%)")
    maxk = max(sizes);
    print(f"  max CQ in un periodo     : {maxk}")
print()
print("FT2 decode per giorno (ultimi 12 con attivita'):")
for d in sorted(ft2_by_day)[-12:]:
    print(f"  20{d[0:2]}-{d[2:4]}-{d[4:6]} : {ft2_by_day[d]:,}")
