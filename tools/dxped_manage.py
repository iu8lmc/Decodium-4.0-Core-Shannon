#!/usr/bin/env python3
"""
Decodium ASYMX — DXpedition Certificate & Verified List Manager

Comandi:
  add       Aggiunge una DXpedition al database
  list      Mostra tutte le DXpedition nel database
  remove    Rimuove una DXpedition dal database
  cert      Genera il certificato .dxcert per una DXpedition
  publish   Firma e genera verified_dxpeds.json per il server
  keygen    Genera una nuova coppia di chiavi Ed25519

Esempi:
  python dxped_manage.py add VP8PJ 199 "South Shetland" "IU8LMC,W1AW,K3LR" 2026-04-01 2026-04-15 4
  python dxped_manage.py list
  python dxped_manage.py cert VP8PJ
  python dxped_manage.py publish
  python dxped_manage.py remove VP8PJ
  python dxped_manage.py keygen
"""

import json
import hmac
import hashlib
import sys
import os
import base64
import subprocess
import tempfile
from datetime import datetime, timezone, timedelta

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DB_FILE = os.path.join(SCRIPT_DIR, "dxped_database.json")
PRIVATE_KEY = os.path.join(SCRIPT_DIR, "dxped_private.pem")
PUBLIC_KEY = os.path.join(SCRIPT_DIR, "dxped_public.pem")
OUTPUT_DIR = SCRIPT_DIR

# HMAC key for individual .dxcert files (must match DXpedCertificate.hpp)
HMAC_KEY = b"D3c0d1uM-ASYMX-DXp3d-2026-IU8LMC"


# ─── Database ────────────────────────────────────────────────────────

def load_db():
    if os.path.exists(DB_FILE):
        with open(DB_FILE) as f:
            return json.load(f)
    return {"dxpeditions": []}

def save_db(db):
    with open(DB_FILE, 'w') as f:
        json.dump(db, f, indent=2)
    print(f"  Database salvato: {DB_FILE}")

def find_dxped(db, callsign):
    for dx in db["dxpeditions"]:
        if dx["callsign"] == callsign.upper():
            return dx
    return None


# ─── Ed25519 Signing (via OpenSSL CLI) ───────────────────────────────

def sign_ed25519(payload_bytes, private_key_path):
    """Firma payload con Ed25519 usando openssl CLI."""
    with tempfile.NamedTemporaryFile(delete=False, suffix='.bin') as f:
        f.write(payload_bytes)
        payload_file = f.name
    sig_file = payload_file + '.sig'
    try:
        subprocess.run([
            'openssl', 'pkeyutl', '-sign',
            '-inkey', private_key_path,
            '-rawin',
            '-in', payload_file,
            '-out', sig_file
        ], check=True, capture_output=True)
        with open(sig_file, 'rb') as f:
            return f.read()
    finally:
        os.unlink(payload_file)
        if os.path.exists(sig_file):
            os.unlink(sig_file)


# ─── Comandi ─────────────────────────────────────────────────────────

def cmd_add(args):
    """Aggiunge una DXpedition al database."""
    if len(args) < 4:
        print("Uso: dxped_manage.py add <callsign> <dxcc_entity> <dxcc_name> <operators> [start] [end] [max_slots]")
        print()
        print("Esempio:")
        print('  python dxped_manage.py add VP8PJ 199 "South Shetland" "IU8LMC,W1AW,K3LR" 2026-04-01 2026-04-15 4')
        return

    callsign = args[0].upper()
    dxcc_entity = int(args[1])
    dxcc_name = args[2]
    operators = [op.strip().upper() for op in args[3].split(',')]
    start = args[4] if len(args) > 4 else datetime.now(timezone.utc).strftime("%Y-%m-%d")
    end = args[5] if len(args) > 5 else (datetime.now(timezone.utc) + timedelta(days=30)).strftime("%Y-%m-%d")
    max_slots = int(args[6]) if len(args) > 6 else 2

    # Normalizza date
    if len(start) == 10:
        start += "T00:00:00Z"
    if len(end) == 10:
        end += "T23:59:59Z"

    db = load_db()
    existing = find_dxped(db, callsign)
    if existing:
        print(f"  Aggiornamento DXpedition esistente: {callsign}")
        db["dxpeditions"].remove(existing)

    entry = {
        "callsign": callsign,
        "dxcc_entity": dxcc_entity,
        "dxcc_name": dxcc_name,
        "operators": operators,
        "activation_start": start,
        "activation_end": end,
        "max_slots": max_slots,
        "added": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    }
    db["dxpeditions"].append(entry)
    save_db(db)

    print(f"\n  DXpedition aggiunta:")
    print(f"    Callsign:   {callsign}")
    print(f"    DXCC:       {dxcc_name} ({dxcc_entity})")
    print(f"    Operatori:  {', '.join(operators)}")
    print(f"    Periodo:    {start[:10]} — {end[:10]}")
    print(f"    Max Slots:  {max_slots}")


def cmd_list(args):
    """Mostra tutte le DXpedition nel database."""
    db = load_db()
    if not db["dxpeditions"]:
        print("  Nessuna DXpedition nel database.")
        return

    print(f"\n  {'CALLSIGN':<12} {'DXCC':<25} {'OPERATORI':<25} {'PERIODO':<25} {'SLOTS'}")
    print(f"  {'-'*12} {'-'*25} {'-'*25} {'-'*25} {'-'*5}")
    for dx in db["dxpeditions"]:
        print(f"  {dx['callsign']:<12} {dx['dxcc_name']:<25} {','.join(dx['operators']):<25} {dx['activation_start'][:10]}—{dx['activation_end'][:10]}  {dx['max_slots']}")
    print(f"\n  Totale: {len(db['dxpeditions'])} DXpedition")


def cmd_remove(args):
    """Rimuove una DXpedition dal database."""
    if not args:
        print("Uso: dxped_manage.py remove <callsign>")
        return

    callsign = args[0].upper()
    db = load_db()
    existing = find_dxped(db, callsign)
    if not existing:
        print(f"  DXpedition {callsign} non trovata nel database.")
        return

    db["dxpeditions"].remove(existing)
    save_db(db)
    print(f"  DXpedition {callsign} rimossa.")


def cmd_cert(args):
    """Genera il certificato .dxcert per una DXpedition."""
    if not args:
        print("Uso: dxped_manage.py cert <callsign> [output_file]")
        return

    callsign = args[0].upper()
    output = args[1] if len(args) > 1 else os.path.join(OUTPUT_DIR, f"{callsign}.dxcert")

    db = load_db()
    dx = find_dxped(db, callsign)
    if not dx:
        print(f"  ERRORE: DXpedition {callsign} non trovata nel database.")
        print(f"  Prima aggiungi con: dxped_manage.py add {callsign} ...")
        return

    # Costruisci il certificato (formato compatibile con DXpedCertificate.hpp)
    obj = {
        "version": 1,
        "callsign": dx["callsign"],
        "dxcc_entity": dx["dxcc_entity"],
        "dxcc_name": dx["dxcc_name"],
        "operators": dx["operators"],
        "activation_start": dx["activation_start"],
        "activation_end": dx["activation_end"],
        "max_slots": dx["max_slots"],
        "issued_by": "Decodium Authority",
        "issued_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    }

    # HMAC-SHA256 signature (per compatibilita' con DXpedCertificate.hpp)
    payload = json.dumps(obj, separators=(',', ':'), sort_keys=True).encode()
    sig = hmac.new(HMAC_KEY, payload, hashlib.sha256).hexdigest()
    obj["signature"] = sig

    with open(output, 'w') as f:
        json.dump(obj, f, indent=2)

    cert_hash = hashlib.sha256(payload).hexdigest()[:8]

    print(f"\n  Certificato generato: {output}")
    print(f"    Callsign:   {dx['callsign']}")
    print(f"    DXCC:       {dx['dxcc_name']} ({dx['dxcc_entity']})")
    print(f"    Operatori:  {', '.join(dx['operators'])}")
    print(f"    Max Slots:  {dx['max_slots']}")
    print(f"    Periodo:    {dx['activation_start'][:10]} — {dx['activation_end'][:10]}")
    print(f"    Hash:       {cert_hash}")
    print(f"    Firma HMAC: {sig[:16]}...")
    print(f"\n  Consegna questo file all'operatore DXpedition.")
    print(f"  L'operatore lo carica in Decodium: Menu > DXped > Load Certificate")


def cmd_publish(args):
    """Firma e genera verified_dxpeds.json per il server."""
    if not os.path.exists(PRIVATE_KEY):
        print(f"  ERRORE: Chiave privata non trovata: {PRIVATE_KEY}")
        print(f"  Genera con: dxped_manage.py keygen")
        return

    db = load_db()
    if not db["dxpeditions"]:
        print("  ERRORE: Nessuna DXpedition nel database.")
        return

    # Filtra solo DXpedition non scadute
    now = datetime.now(timezone.utc)
    active = []
    for dx in db["dxpeditions"]:
        end_str = dx["activation_end"]
        try:
            end_dt = datetime.fromisoformat(end_str.replace('Z', '+00:00'))
            if end_dt >= now:
                active.append(dx["callsign"])
            else:
                print(f"  SKIP (scaduta): {dx['callsign']} — scaduta il {end_str[:10]}")
        except Exception:
            active.append(dx["callsign"])  # include if date parsing fails

    if not active:
        print("  ERRORE: Nessuna DXpedition attiva (tutte scadute).")
        return

    callsigns = sorted(set(active))

    # Costruisci payload (stessa struttura che Qt ricostruisce)
    obj = {
        "callsigns": callsigns,
        "expires": (now + timedelta(days=90)).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "updated": now.strftime("%Y-%m-%dT%H:%M:%SZ"),
        "version": 2
    }

    # JSON canonico (chiavi ordinate, compatto) — deve corrispondere a Qt QJsonDocument::Compact
    payload = json.dumps(obj, separators=(',', ':'), sort_keys=True).encode('utf-8')

    # Firma Ed25519
    sig = sign_ed25519(payload, PRIVATE_KEY)
    sig_b64 = base64.b64encode(sig).decode('ascii')
    obj["signature"] = sig_b64

    output = args[0] if args else os.path.join(OUTPUT_DIR, "verified_dxpeds.json")
    with open(output, 'w') as f:
        json.dump(obj, f, indent=2)

    print(f"\n  Lista firmata generata: {output}")
    print(f"    Callsign verificati: {', '.join(callsigns)}")
    print(f"    Aggiornata:          {obj['updated']}")
    print(f"    Scade:               {obj['expires']}")
    print(f"    Firma Ed25519:       {sig_b64[:32]}...")
    print(f"\n  PROSSIMO PASSO:")
    print(f"    Carica {output} su https://ft2.it/verified_dxpeds.json")


def cmd_keygen(args):
    """Genera una nuova coppia di chiavi Ed25519."""
    if os.path.exists(PRIVATE_KEY):
        print(f"  ATTENZIONE: Chiave privata gia' esistente: {PRIVATE_KEY}")
        resp = input("  Sovrascrivere? (s/N): ").strip().lower()
        if resp != 's':
            print("  Annullato.")
            return

    subprocess.run([
        'openssl', 'genpkey', '-algorithm', 'Ed25519',
        '-out', PRIVATE_KEY
    ], check=True)

    subprocess.run([
        'openssl', 'pkey', '-in', PRIVATE_KEY,
        '-pubout', '-out', PUBLIC_KEY
    ], check=True)

    # Estrai la chiave pubblica raw per embedding C++
    result = subprocess.run([
        'openssl', 'pkey', '-in', PUBLIC_KEY, '-pubin', '-text', '-noout'
    ], capture_output=True, text=True, check=True)

    print(f"\n  Chiavi generate:")
    print(f"    Privata: {PRIVATE_KEY}  (TIENI SEGRETA!)")
    print(f"    Pubblica: {PUBLIC_KEY}")
    print(f"\n  {result.stdout}")
    print(f"  IMPORTANTE: Aggiorna la chiave pubblica in VerifiedDxpedList.hpp")
    print(f"  IMPORTANTE: NON committare dxped_private.pem su GitHub!")


def cmd_help():
    """Mostra l'help."""
    print(__doc__)


# ─── Main ────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) < 2:
        cmd_help()
        return

    cmd = sys.argv[1].lower()
    args = sys.argv[2:]

    commands = {
        "add": cmd_add,
        "list": cmd_list,
        "ls": cmd_list,
        "remove": cmd_remove,
        "rm": cmd_remove,
        "del": cmd_remove,
        "cert": cmd_cert,
        "certificate": cmd_cert,
        "publish": cmd_publish,
        "sign": cmd_publish,
        "keygen": cmd_keygen,
        "help": lambda a: cmd_help(),
    }

    if cmd in commands:
        commands[cmd](args)
    else:
        print(f"  Comando sconosciuto: {cmd}")
        cmd_help()


if __name__ == "__main__":
    main()
