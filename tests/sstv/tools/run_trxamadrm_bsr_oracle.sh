#!/usr/bin/env bash
# Compare Decodium BSR output with the exact archived TRXAMADRM 3.6 parser loop.
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/../../.." && pwd)"
archive="${1:?pass the hash-pinned TRXAMADRM 3.6 archive}"
shift
expected=e001e592dfe742a435bd22220ed24b3c0ad9bf669e3559230f8f0aaca9939dcd
actual="$(shasum -a 256 "$archive" | cut -d ' ' -f 1)"
[[ "$actual" == "$expected" ]] || { printf 'Archive hash mismatch\n' >&2; exit 1; }
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/decodium-trxamadrm-bsr.XXXXXX")"
trap 'for f in oracle trxamadrm_bsr_loop.inc DABMOTbsr.cpp; do [[ ! -e "$test_dir/$f" ]] || rm "$test_dir/$f"; done; rmdir "$test_dir"' EXIT
source_path="$(tar -tzf "$archive" | rg '/common/datadecoding/DABMOTbsr.cpp$')"
tar -xOzf "$archive" "$source_path" > "$test_dir/DABMOTbsr.cpp"
python3 - "$test_dir" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
sources = list(root.rglob('DABMOTbsr.cpp'))
if len(sources) != 1:
    raise SystemExit('expected one DABMOTbsr.cpp')
text = sources[0].read_text()
start = text.index('for (i=0; i < iNumSegStore ; i++)')
end = text.index('printf("after reading bsr.bin', start)
(root / 'trxamadrm_bsr_loop.inc').write_text(text[start:end])
PY
cmake --build "$repo_root/build" --target decodium_sstv_hamdrm_core -j4
clang++ -std=c++17 -O2 -I"$repo_root" -I"$test_dir" $(pkg-config --cflags Qt6Core) \
  "$repo_root/tests/sstv/tools/trxamadrm_bsr_oracle.cpp" \
  "$repo_root/build/src/sstv/digital/libdecodium_sstv_hamdrm_core.a" \
  $(pkg-config --libs Qt6Core) -o "$test_dir/oracle"
"$test_dir/oracle" "$@"
