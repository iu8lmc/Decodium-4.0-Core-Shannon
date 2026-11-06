#!/usr/bin/env bash
# Build only a narrow adapter against an independently maintained SSTV codec.
set -euo pipefail
if [[ $# -lt 2 || $# -gt 3 || ( $# -eq 3 && "$3" != '--nominal-avt-clock' && "$3" != '--avt90-320-diagnostic' ) ]]; then
  echo "usage: $0 REVENANT_CHECKOUT OUTPUT [--nominal-avt-clock|--avt90-320-diagnostic]" >&2
  exit 2
fi
repo_dir="$(cd "$(dirname "$0")/../../.." && pwd)"
reference_dir="$(cd "$1" && pwd)"
output_path="$2"
pinned=67da9f8767b2a250792a6f5a5b2a69f5bc992b6d
if [[ "$(git -C "$reference_dir" rev-parse HEAD)" != "$pinned" ]]; then
  echo "Unexpected Revenant revision; expected $pinned" >&2
  exit 2
fi
if ! git -C "$reference_dir" diff --quiet HEAD -- core/decode core/dsp/synth/sstv_mod.cpp core/dsp/synth/sstv_mod.h core/dsp/types.h core/error.h; then
  echo "Revenant codec files have local changes" >&2
  exit 2
fi
source_file="$reference_dir/core/decode/sstv.cpp"
if [[ "${3:-}" == '--nominal-avt-clock' || "${3:-}" == '--avt90-320-diagnostic' ]]; then
  tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/decodium-revenant-build.XXXXXXXX")"
  trap 'rm -rf "$tmp_dir"' EXIT
  cp "$source_file" "$tmp_dir/sstv.cpp"
  if [[ "$3" == '--nominal-avt-clock' ]]; then
    patch -s "$tmp_dir/sstv.cpp" < "$repo_dir/tests/sstv/fixtures/revenant-avt-nominal-clock-diagnostic.patch"
  else
    patch -s "$tmp_dir/sstv.cpp" < "$repo_dir/tests/sstv/fixtures/revenant-avt90-320-diagnostic.patch"
  fi
  source_file="$tmp_dir/sstv.cpp"
fi
"${CXX:-c++}" -std=c++23 -O2 -I"$reference_dir" \
  "$repo_dir/tests/sstv/tools/revenant_sstv_oracle.cpp" \
  "$source_file" "$reference_dir/core/dsp/synth/sstv_mod.cpp" \
  -o "$output_path"
