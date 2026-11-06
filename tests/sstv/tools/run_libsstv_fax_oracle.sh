#!/usr/bin/env bash
# Differential FAX480 decoder check using a separately acquired libsstv tree.
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/../../.." && pwd)"
reference_root="${LIBSSTV_REFERENCE_ROOT:-/tmp/decodium-libsstv-reference}"
expected=193157a993ac34bfa074074004c9ddadcfe6fd15
[[ "$(git -C "$reference_root" rev-parse HEAD)" == "$expected" ]]
for path in src/encoder.c src/sstv.c src/sstv.h src/libsstv.template.h src/luts.c src/luts.h; do
    git -C "$reference_root" diff --quiet HEAD -- "$path" || { printf 'Modified reference: %s\n' "$path" >&2; exit 1; }
done
tmp="$(mktemp -d "${TMPDIR:-/tmp}/decodium-libsstv-fax.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
cmake -S "$reference_root" -B "$tmp/build" -DBUILD_TOOLS=OFF >/dev/null
cmake --build "$tmp/build" --target sstv_static -j4 >/dev/null
cmake --build "$repo_root/build" --target decode_libsstv_fax -j4
cc -std=c11 -I"$reference_root/include" "$repo_root/tests/sstv/tools/libsstv_fax480_fixture.c" \
   "$reference_root/lib/libsstv.a" -lm -o "$tmp/generate"
for variant in 0 1; do
    "$tmp/generate" "$variant" "$tmp/$variant.pcm"
    "$repo_root/build/tests/sstv/decode_libsstv_fax" "$tmp/$variant.pcm" "$variant"
done
