#!/usr/bin/env bash
# Differential probe; QSSTV source remains in its separate checkout.
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/../../.." && pwd)"
reference_root="${QSSTV_REFERENCE_ROOT:-/tmp/decodium-qsstv-reference}"
expected_revision=8c27d6d169d8c6c197eb47c2089870e39bc06a02
[[ "$(git -C "$reference_root" rev-parse HEAD)" == "$expected_revision" ]]
for path in src/drmrx/sourcedecoder.cpp src/drmrx/crc16_bytewise.cpp; do
  git -C "$reference_root" diff --quiet HEAD -- "$path" || { printf 'Modified reference file: %s\n' "$path" >&2; exit 1; }
done
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/decodium-qsstv-packet.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
python3 - "$reference_root/src/drmrx/sourcedecoder.cpp" "$test_dir/qsstv_extracted_packet_methods.inc" <<'PY'
import pathlib, sys
source=pathlib.Path(sys.argv[1]).read_text()
parts=[]
for signature, next_signature in (
    ('bool sourceDecoder::setupDataBlock(', 'void dataBlock::log()'),
    ('bool sourceDecoder::setupDataPacket(', 'void dataPacket::log()')):
    start=source.index(signature)
    end=source.index(next_signature,start)
    parts.append(source[start:end])
pathlib.Path(sys.argv[2]).write_text('\n'.join(parts))
PY
cmake --build "$repo_root/build" --target export_decodium_packets -j4
clang++ -std=c++17 -O2 -Wno-deprecated-declarations -I"$test_dir" $(pkg-config --cflags Qt6Core Qt6Gui) \
  "$repo_root/tests/sstv/tools/qsstv_packet_rx_oracle.cpp" \
  "$reference_root/src/drmrx/crc16_bytewise.cpp" \
  $(pkg-config --libs Qt6Core Qt6Gui) -o "$test_dir/qsstv-packet-rx"
image_a="$repo_root/tests/sstv/fixtures/qsstv-image-a.jpg"
image_b="$repo_root/tests/sstv/fixtures/qsstv-image-b.jpg"
"$repo_root/build/tests/sstv/export_decodium_packets" "$image_a" "$image_b" | "$test_dir/qsstv-packet-rx" "$image_a" "$image_b"
