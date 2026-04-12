#!/usr/bin/env python3
"""Generate a DXpedition certificate (.dxcert) for Decodium ASYMX."""

import json
import hmac
import hashlib
import sys
from datetime import datetime

HMAC_KEY = b"D3c0d1uM-ASYMX-DXp3d-2026-IU8LMC"

def generate_cert(callsign, dxcc_entity, dxcc_name, operators, start, end, max_slots=2, output=None):
    obj = {
        "version": 1,
        "callsign": callsign.upper(),
        "dxcc_entity": dxcc_entity,
        "dxcc_name": dxcc_name,
        "operators": [op.upper() for op in operators],
        "activation_start": start,
        "activation_end": end,
        "max_slots": max_slots,
        "issued_by": "Decodium Authority",
        "issued_at": datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")
    }

    payload = json.dumps(obj, separators=(',', ':'), sort_keys=True).encode()
    sig = hmac.new(HMAC_KEY, payload, hashlib.sha256).hexdigest()
    obj["signature"] = sig

    outfile = output or f"{callsign.upper()}.dxcert"
    with open(outfile, 'w') as f:
        json.dump(obj, f, indent=2)

    cert_hash = hashlib.sha256(payload).hexdigest()[:8]
    print(f"Certificate generated: {outfile}")
    print(f"  Callsign: {callsign.upper()}")
    print(f"  DXCC: {dxcc_name} ({dxcc_entity})")
    print(f"  Operators: {', '.join(op.upper() for op in operators)}")
    print(f"  Max Slots: {max_slots}")
    print(f"  Valid: {start} to {end}")
    print(f"  Hash: {cert_hash}")
    print(f"  Signature: {sig[:16]}...")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: generate_dxcert.py <callsign> [dxcc_entity] [dxcc_name] [operators] [start] [end] [max_slots]")
        print("\nExample:")
        print('  python generate_dxcert.py VP8PJ 199 "South Shetland" "IU8LMC,W1AW,K3LR" 2026-04-01T00:00:00Z 2026-04-15T23:59:59Z 4')
        print("\nDefault test certificate:")
        generate_cert(
            "IU8TEST", 248, "Italy", ["IU8LMC"],
            "2026-01-01T00:00:00Z", "2026-12-31T23:59:59Z", 4,
            "IU8TEST.dxcert"
        )
    else:
        call = sys.argv[1]
        dxcc = int(sys.argv[2]) if len(sys.argv) > 2 else 0
        name = sys.argv[3] if len(sys.argv) > 3 else "Unknown"
        ops = sys.argv[4].split(',') if len(sys.argv) > 4 else [call]
        start = sys.argv[5] if len(sys.argv) > 5 else "2026-01-01T00:00:00Z"
        end = sys.argv[6] if len(sys.argv) > 6 else "2026-12-31T23:59:59Z"
        slots = int(sys.argv[7]) if len(sys.argv) > 7 else 2
        generate_cert(call, dxcc, name, ops, start, end, slots)
