#!/usr/bin/env bash
# Compare Decodium RS1-RS4 with independently archived TRXAMADRM encoders.
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "usage: $0 TRXAMADRM_3_6_64_ARCHIVE" >&2
  exit 2
fi
repo_dir="$(cd "$(dirname "$0")/../../.." && pwd)"
archive="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
python3 - "$archive" <<'PY'
import hashlib, sys
with open(sys.argv[1], 'rb') as source:
    digest = hashlib.sha256(source.read()).hexdigest()
if digest != 'e001e592dfe742a435bd22220ed24b3c0ad9bf669e3559230f8f0aaca9939dcd':
    raise SystemExit('unexpected archive SHA-256')
PY
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/decodium-trx-rs.XXXXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT
for level in 1 2 3 4; do
  tar -xzf "$archive" -C "$tmp_dir" \
    "./trxamadrmv3_6/linux/rsencodev0_1/rs${level}encode.c" \
    "./trxamadrmv3_6/linux/rsencodev0_1/rs${level}.c" \
    "./trxamadrmv3_6/linux/rsencodev0_1/rs${level}.h"
done
source_dir="$tmp_dir/trxamadrmv3_6/linux/rsencodev0_1"
for variant in a b; do cp "$repo_dir/tests/sstv/fixtures/qsstv-image-$variant.jpg" "$tmp_dir/$variant.jpg"; done
for level in 1 2 3 4; do
  "${CC:-cc}" -w -O2 "$source_dir/rs${level}encode.c" "$source_dir/rs${level}.c" -o "$tmp_dir/reference"
  "${CC:-cc}" -w -O2 -c "$source_dir/rs${level}.c" -o "$tmp_dir/reference-rs.o"
  "${CXX:-c++}" -std=c++17 -O2 -I"$repo_dir" \
    "$repo_dir/tests/sstv/tools/trxamadrm_rs_oracle.cpp" \
    "$repo_dir/src/sstv/digital/HamDrmRsCodec.cpp" "$tmp_dir/reference-rs.o" -o "$tmp_dir/check"
  for variant in a b; do
    # The historical utility uses small fixed filename buffers; pass a short
    # relative name and run only in the private temporary directory.
    (cd "$tmp_dir" && ./reference "$variant.jpg" && ./check "$variant.jpg" "$variant.rs$level" "$level")
  done
  # Historical encoder requires at least k-7 bytes. Exercise size boundaries
  # on deterministic synthetic payloads independently of JPEG decoding.
  k=$((256 - level * 32))
  for size in "$((k - 7))" "$((k - 1))" "$k" "$((k + 1))" "$((2 * k - 7))" "$((2 * k))" "$((2 * k + 1))"; do
    python3 - "$tmp_dir/t.jpg" "$size" <<'PY'
from pathlib import Path
import sys
Path(sys.argv[1]).write_bytes(bytes((i * 73 + 19) % 256 for i in range(int(sys.argv[2]))))
PY
    (cd "$tmp_dir" && ./reference t.jpg && ./check t.jpg "t.rs$level" "$level" >/dev/null)
  done
  if (cd "$tmp_dir" && ./check a.jpg "b.rs$level" "$level" >/dev/null); then
    echo "RS$level crossed reference unexpectedly matched" >&2
    exit 1
  fi
done
printf 'PASS archived TRXAMADRM RS1-RS4 A/B, block boundaries and crossed-image negatives\n'
