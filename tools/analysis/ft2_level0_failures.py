#!/usr/bin/env python3
# Livello 0-bis - misura dei FALLIMENTI FT2 (read-only)
# Proxy: ogni riga Rx "IU8LMC <partner> <scambio>" = il partner e' in QSO con me.
# Se quel partner NON e' nell'ADI (QSO chiuso) entro la stessa giornata -> fallimento.
# Classifico fin dove e' arrivato lo scambio (reply/report/roger/signoff).
import os, re, time
from collections import Counter, defaultdict

HOME = os.path.expanduser("~")
ADI  = os.path.join(HOME, "AppData", "Local", "IU8LMC", "Decodium", "decodium_log.adi")
ALL  = os.path.join(HOME, "AppData", "Roaming", "IU8LMC", "Decodium", "all.txt")
MYCALL = "IU8LMC"
ADI_START = "260517"   # ADI parte qui: episodi prima non sono confrontabili (falsi fallimenti)

def pct(v, p):
    if not v: return 0
    v = sorted(v); k=(len(v)-1)*p; f=int(k); c=min(f+1,len(v)-1)
    return v[f] if f==c else v[f]+(v[c]-v[f])*(k-f)

# ---- ADI: set di (call, giorno) chiusi in FT2 ----
field_re = re.compile(r"<([A-Za-z_]+):(\d+)(?::[A-Za-z])?>([^<]*)")
adi_ft2 = set()      # (CALL, YYMMDD)
adi_ft2_calls = set()
with open(ADI, encoding="utf-8", errors="replace") as f:
    for rec in f.read().split("<EOR>"):
        fields={}
        for m in field_re.finditer(rec):
            fields[m.group(1).upper()] = m.group(3)[:int(m.group(2))]
        mode = fields.get("SUBMODE") or fields.get("MODE","")
        if mode=="FT2" and "CALL" in fields and "QSO_DATE" in fields:
            call=fields["CALL"].upper(); d=fields["QSO_DATE"][2:]  # YYMMDD
            adi_ft2.add((call,d)); adi_ft2_calls.add(call)

# ---- stadio dello scambio dal 3o token ----
REP   = re.compile(r"^R?[+-]\d{2}$")     # -07 +03 R-07
GRID  = re.compile(r"^[A-R]{2}\d{2}$")   # JN71
def stage(tok):
    if tok in ("RR73","73","RRR"): return "signoff"
    if tok.startswith("R") and REP.match(tok): return "roger"   # R-07
    if REP.match(tok): return "report"
    if GRID.match(tok): return "reply"
    return "other"
ORDER = {"reply":1,"report":2,"roger":3,"signoff":4,"other":0}

line_re = re.compile(r"^(\d{6})_(\d{6})\s+[\d.]+\s+Rx\s+(\S+)\s+(-?\d+)\s+(-?[\d.]+)\s+(\d+)\s+(.*)$")
CALLRE = re.compile(r"^[A-Z0-9]{1,3}[0-9][A-Z0-9]*[A-Z](?:/[A-Z0-9]+)?$")

# episodi: chiave (partner, giorno) -> dict(max_stage, lines, first_ts, last_ts, snrs, band? )
episodes = defaultdict(lambda: {"st":0,"n":0,"snr":[], "day":None})
t0=time.time(); n=0; ft2_to_me=0
with open(ALL, encoding="utf-8", errors="replace") as f:
    for line in f:
        n+=1
        m=line_re.match(line)
        if not m: continue
        day,hms,mode,snr,dt,hz,msg = m.groups()
        if mode!="FT2": continue
        if day < ADI_START: continue
        toks=msg.split()
        if len(toks)<2 or toks[0]!=MYCALL: continue   # diretto a ME
        partner=toks[1].upper()
        if not CALLRE.match(partner): continue          # salta <...> hashed / token strani
        ft2_to_me+=1
        st = stage(toks[2]) if len(toks)>=3 else "reply"
        key=(partner,day)
        e=episodes[key]; e["day"]=day
        e["st"]=max(e["st"], ORDER[st]); e["n"]+=1
        try: e["snr"].append(int(snr))
        except: pass

elapsed=time.time()-t0
ST_NAME={0:"other",1:"reply",2:"report",3:"roger",4:"signoff"}

tot=len(episodes)
succ=0; fail=0
fail_by_stage=Counter(); succ_by_stage=Counter()
fail_snr=[]; succ_snr=[]
fail_signoff_examples=[]
for (partner,day),e in episodes.items():
    completed = (partner,day) in adi_ft2
    msnr = min(e["snr"]) if e["snr"] else None  # SNR peggiore visto (debolezza)
    avgsnr = (sum(e["snr"])/len(e["snr"])) if e["snr"] else None
    if completed:
        succ+=1; succ_by_stage[e["st"]]+=1
        if avgsnr is not None: succ_snr.append(avgsnr)
    else:
        fail+=1; fail_by_stage[e["st"]]+=1
        if avgsnr is not None: fail_snr.append(avgsnr)
        if e["st"]==4 and len(fail_signoff_examples)<15:
            fail_signoff_examples.append((day,partner,e["n"], int(avgsnr) if avgsnr is not None else 0))

print("="*72)
print("LIVELLO 0-bis - FALLIMENTI FT2 (proxy da Rx 'IU8LMC <partner> ...')")
print("="*72)
print(f"righe all.txt lette        : {n:,}  ({elapsed:.1f}s)")
print(f"righe FT2 dirette a me      : {ft2_to_me:,}")
print(f"ADI FT2 (call,giorno) unici : {len(adi_ft2)}")
print(f"finestra confronto          : decode >= 20{ADI_START}")
print()
print(f"EPISODI FT2 (partner mi ha risposto, per giorno): {tot}")
print(f"  -> chiusi in ADI (successo): {succ} ({100*succ/tot:.0f}%)")
print(f"  -> NON chiusi   (fallimento): {fail} ({100*fail/tot:.0f}%)")
print()
print("Per stadio MASSIMO raggiunto (quanto era avanzato lo scambio):")
print(f"  {'stadio':10} {'falliti':>8} {'riusciti':>9}")
for s in (4,3,2,1,0):
    print(f"  {ST_NAME[s]:10} {fail_by_stage.get(s,0):>8} {succ_by_stage.get(s,0):>9}")
print()
sg_fail = fail_by_stage.get(4,0)
print(f">>> FALLIMENTI gia' a SIGNOFF (RR73/73 visto ma QSO non loggato): {sg_fail}")
print(f"    = il pattern 'signoff perso / RR73 droppato'. Casi:")
for d,p,nn,s in fail_signoff_examples:
    print(f"      20{d[0:2]}-{d[2:4]}-{d[4:6]}  {p:8}  righe={nn}  SNRmedio={s}dB")
print()
if fail_snr and succ_snr:
    print("SNR medio del partner:")
    print(f"  episodi RIUSCITI : mediana={pct(succ_snr,.5):.0f}dB  p10={pct(succ_snr,.1):.0f}")
    print(f"  episodi FALLITI  : mediana={pct(fail_snr,.5):.0f}dB  p10={pct(fail_snr,.1):.0f}")
    weak_fail=sum(1 for x in fail_snr if x<=-15);
    print(f"  fallimenti con partner debole (<=-15dB): {weak_fail}/{len(fail_snr)} ({100*weak_fail/len(fail_snr):.0f}%)")
