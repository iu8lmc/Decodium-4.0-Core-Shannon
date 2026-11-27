#!/usr/bin/env bash
# Developer-only differential test against a separately acquired QSSTV checkout.
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/../../.." && pwd)"
reference_root="${QSSTV_REFERENCE_ROOT:-/tmp/decodium-qsstv-reference}"
expected_revision=8c27d6d169d8c6c197eb47c2089870e39bc06a02
if [[ "$(git -C "$reference_root" rev-parse HEAD)" != "$expected_revision" ]]; then
  printf '%s\n' 'Unexpected QSSTV reference revision' >&2
  exit 1
fi
for path in src/drmrx/msdhardmsc.cpp src/drmrx/msdhardfac.cpp src/drmrx/viterbi_decode.cpp src/drmrx/deinterleaver.cpp src/drmrx/msd_hard.h src/drmrx/msd_hard_sdc.h src/drmrx/viterbi_decode.h; do
  if ! git -C "$reference_root" diff --quiet HEAD -- "$path"; then
    printf 'Modified QSSTV reference file: %s\n' "$path" >&2
    exit 1
  fi
done
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/decodium-qsstv-msc.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
cmake --build "$repo_root/build" --target export_decodium_msc -j4
clang++ -std=c++17 -O2 -I"$reference_root/src/drmrx" \
  "$repo_root/tests/sstv/tools/qsstv_msc_rx_oracle.cpp" \
  "$reference_root/src/drmrx/msdhardmsc.cpp" \
  "$reference_root/src/drmrx/msdhardfac.cpp" \
  "$reference_root/src/drmrx/viterbi_decode.cpp" \
  "$reference_root/src/drmrx/deinterleaver.cpp" \
  -o "$test_dir/qsstv-msc-rx"
"$repo_root/build/tests/sstv/export_decodium_msc" | "$test_dir/qsstv-msc-rx"
