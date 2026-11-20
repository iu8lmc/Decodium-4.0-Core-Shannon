#!/usr/bin/env bash
# Build the pinned Isobar CLI; optional diagnostics are isolated from checkout.
set -euo pipefail
if [[ $# -lt 2 || $# -gt 3 || ( $# -eq 3 && "$3" != '--no-tracking' && "$3" != '--trace-lag' && "$3" != '--trace-phase' ) ]]; then
  echo "usage: $0 ISOBAR_CHECKOUT OUTPUT [--no-tracking|--trace-lag|--trace-phase]" >&2
  exit 2
fi
repo_dir="$(cd "$(dirname "$0")/../../.." && pwd)"
reference_dir="$(cd "$1" && pwd)"
output_path="$2"
pinned=111bd184a962b82ebde72aa07a34f9905e5b2f89
if [[ "$(git -C "$reference_dir" rev-parse HEAD)" != "$pinned" ]] || \
   ! git -C "$reference_dir" diff --quiet HEAD -- core cli/isobar-decode.cpp; then
  echo "Isobar revision differs from $pinned or its source has local changes" >&2
  exit 2
fi
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/decodium-isobar-build.XXXXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT
mkdir "$tmp_dir/cli"
ln -s "$reference_dir/core" "$tmp_dir/core"
cp "$reference_dir/cli/isobar-decode.cpp" "$tmp_dir/cli/isobar-decode.cpp"
if [[ "${3:-}" == '--no-tracking' ]]; then
  patch -s -d "$tmp_dir" -p1 < "$repo_dir/tests/sstv/fixtures/isobar-no-tracking-diagnostic.patch"
fi
sources=(wavfile decoder syncscan synfile ratefit fft filters resample tonedetect live)
source_paths=()
for source in "${sources[@]}"; do source_paths+=("$reference_dir/core/$source.cpp"); done
if [[ "${3:-}" == '--trace-lag' || "${3:-}" == '--trace-phase' ]]; then
  mkdir "$tmp_dir/diagnostic"
  cp "$reference_dir/core/live.cpp" "$tmp_dir/diagnostic/live.cpp"
  trace_kind="${3#--trace-}"
  patch -s "$tmp_dir/diagnostic/live.cpp" < "$repo_dir/tests/sstv/fixtures/isobar-${trace_kind}-trace-diagnostic.patch"
  source_paths[${#source_paths[@]}-1]="$tmp_dir/diagnostic/live.cpp"
fi
"${CXX:-c++}" -std=c++17 -O2 -I "$reference_dir/core" "$tmp_dir/cli/isobar-decode.cpp" "${source_paths[@]}" -o "$output_path"
