#!/usr/bin/env bash
# Execute exact QSSTV reference methods in an isolated differential harness.
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/../../.." && pwd)"
reference_root="${QSSTV_REFERENCE_ROOT:-/tmp/decodium-qsstv-reference}"
expected_revision=8c27d6d169d8c6c197eb47c2089870e39bc06a02
[[ "$(git -C "$reference_root" rev-parse HEAD)" == "$expected_revision" ]]
for path in src/drmrx/sourcedecoder.cpp src/drmtx/drmtx.cpp; do
  git -C "$reference_root" diff --quiet HEAD -- "$path" || { printf 'Modified reference file: %s\n' "$path" >&2; exit 1; }
done
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/decodium-qsstv-bsr.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
python3 - "$reference_root" "$test_dir/qsstv_bsr_methods.inc" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]); parts=[]
for name, start, end in (
 ('src/drmrx/sourcedecoder.cpp','bool sourceDecoder::storeBSR(', 'transportBlock *sourceDecoder::getTransporPtr('),
 ('src/drmtx/drmtx.cpp','int drmTx::processFIX(', 'void drmTx::initDRMBSR(')):
 text=(root/name).read_text(); pos=text.index(start); parts.append(text[pos:text.index(end,pos)])
Path(sys.argv[2]).write_text('\n'.join(parts))
PY
cmake --build "$repo_root/build" --target decodium_sstv_hamdrm_core -j4
clang++ -std=c++17 -O2 -I"$repo_root" -I"$test_dir" $(pkg-config --cflags Qt6Core) \
  "$repo_root/tests/sstv/tools/qsstv_bsr_oracle.cpp" \
  "$repo_root/build/src/sstv/digital/libdecodium_sstv_hamdrm_core.a" \
  $(pkg-config --libs Qt6Core) -o "$test_dir/qsstv-bsr"
"$test_dir/qsstv-bsr"
