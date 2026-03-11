#!/usr/bin/env python3
"""Sign a verified DXpedition callsign list with Ed25519.

Usage:
  python dxped_sign_list.py callsigns.txt dxped_private.pem [output.json]

callsigns.txt = one callsign per line (blank/# lines ignored)
Output: verified_dxpeds.json with Ed25519 signature
"""

import json
import sys
import base64
import subprocess
import tempfile
import os
from datetime import datetime, timedelta

def sign_with_openssl(payload_bytes, private_key_pem_path):
    """Sign payload using openssl CLI (Ed25519)."""
    with tempfile.NamedTemporaryFile(delete=False, suffix='.bin') as f:
        f.write(payload_bytes)
        payload_file = f.name
    sig_file = payload_file + '.sig'
    try:
        subprocess.run([
            'openssl', 'pkeyutl', '-sign',
            '-inkey', private_key_pem_path,
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

def main():
    if len(sys.argv) < 3:
        print("Usage: dxped_sign_list.py <callsigns.txt> <private_key.pem> [output.json]")
        print("\nExample:")
        print("  python dxped_sign_list.py my_calls.txt dxped_private.pem verified_dxpeds.json")
        sys.exit(1)

    calls_file = sys.argv[1]
    key_file = sys.argv[2]
    out_file = sys.argv[3] if len(sys.argv) > 3 else "verified_dxpeds.json"

    # Read callsigns
    callsigns = []
    with open(calls_file) as f:
        for line in f:
            line = line.strip().upper()
            if line and not line.startswith('#'):
                callsigns.append(line)

    callsigns = sorted(set(callsigns))

    # Build payload (same structure Qt will reconstruct)
    now = datetime.utcnow()
    obj = {
        "callsigns": callsigns,
        "expires": (now + timedelta(days=90)).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "updated": now.strftime("%Y-%m-%dT%H:%M:%SZ"),
        "version": 2
    }

    # Canonical JSON: sorted keys, compact, no trailing newline
    payload = json.dumps(obj, separators=(',', ':'), sort_keys=True).encode('utf-8')

    # Sign with Ed25519
    sig = sign_with_openssl(payload, key_file)
    sig_b64 = base64.b64encode(sig).decode('ascii')

    # Add signature to output
    obj["signature"] = sig_b64

    with open(out_file, 'w') as f:
        json.dump(obj, f, indent=2)

    print(f"Signed list generated: {out_file}")
    print(f"  Callsigns: {', '.join(callsigns)}")
    print(f"  Updated: {obj['updated']}")
    print(f"  Expires: {obj['expires']}")
    print(f"  Signature: {sig_b64[:32]}...")

if __name__ == "__main__":
    main()
