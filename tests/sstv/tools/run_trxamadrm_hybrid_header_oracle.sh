#!/usr/bin/env bash
# Reproduce small hybrid-header fixtures from an independently archived source.
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "usage: $0 TRXAMADRM_3_6_64_ARCHIVE" >&2
  exit 2
fi
repo_dir="$(cd "$(dirname "$0")/../../.." && pwd)"
archive="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
python3 - "$archive" <<'PY'
import hashlib,sys
with open(sys.argv[1], 'rb') as f:
    digest = hashlib.sha256(f.read()).hexdigest()
if digest != 'e001e592dfe742a435bd22220ed24b3c0ad9bf669e3559230f8f0aaca9939dcd':
    raise SystemExit('unexpected archive SHA-256')
PY
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/decodium-trx-hybrid.XXXXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT
tar -xzf "$archive" -C "$tmp_dir"
reference="$tmp_dir/trxamadrmv3_6"
# The source archive is hash-pinned; no network or installed radio is used.
if [[ "$(uname -s)" == Darwin ]]; then
  linker=(-Wl,-dead_strip)
else
  linker=(-Wl,--gc-sections)
fi
# shellcheck disable=SC2207
fftw_flags=($(pkg-config --cflags fftw3))
"${CXX:-c++}" -std=c++17 -O2 -ffunction-sections -fdata-sections -DHAVE_STDINT_H=1 \
  "${fftw_flags[@]}" -I"$reference" "$repo_dir/tests/sstv/tools/trxamadrm_hybrid_header_oracle.cpp" \
  "$reference/common/datadecoding/DABMOThybrid.cpp" "$reference/common/util/CRC.cpp" \
  "$reference/common/util/Utilities.cpp" "${linker[@]}" -o "$tmp_dir/oracle"
for variant in a b; do
  "$tmp_dir/oracle" "$variant" "$tmp_dir/$variant.bin" > /dev/null
  cmp "$tmp_dir/$variant.bin" "$repo_dir/tests/sstv/fixtures/trxamadrm-3.6-hybrid-header-$variant.bin"
  cmp "$tmp_dir/$variant.bin.body" "$repo_dir/tests/sstv/fixtures/trxamadrm-3.6-hybrid-body-$variant.bin"
done
printf 'PASS archived TRXAMADRM hybrid header and body A/B byte fixtures\n'
