#!/usr/bin/env bash
# Build the pinned independent Radio Fax decoder without editing its source.
set -euo pipefail
if [[ $# -ne 2 ]]; then
  echo "usage: $0 DECODER_CHECKOUT OUTPUT" >&2
  exit 2
fi
source_dir="$(cd "$1" && pwd)"
output="$2"
pinned=c5321bcb2e196bf33bf34c11cd3a4c86b0bd5358
if [[ "$(git -C "$source_dir" rev-parse HEAD)" != "$pinned" ]] || \
   [[ -n "$(git -C "$source_dir" status --porcelain --untracked-files=all -- src)" ]]; then
  echo "decoder source is modified or not at $pinned" >&2
  exit 2
fi
if [[ -e "$output" || -L "$output" ]]; then
  echo "refusing to overwrite output" >&2
  exit 2
fi
# The upstream sources rely on malloc/qsort declarations without explicitly
# including <cstdlib>; force that standard header for strict C++ builds.
"${CXX:-c++}" -std=c++20 -O2 -include cstdlib \
  "$source_dir/src/fax.cpp" "$source_dir/src/avg.cpp" "$source_dir/src/FaxDecoder.cpp" \
  -o "$output"
