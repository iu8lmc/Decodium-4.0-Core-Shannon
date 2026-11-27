#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
reference="${QSSTV_REFERENCE_ROOT:-/tmp/decodium-qsstv-reference}"
expected=8c27d6d169d8c6c197eb47c2089870e39bc06a02
[[ "$(git -C "${reference}" rev-parse HEAD)" == "${expected}" ]]
[[ -z "$(git -C "${reference}" status --porcelain -- src/utils/rs.cpp src/utils/rs.h src/utils/reedsolomoncoder.cpp src/utils/reedsolomoncoder.h)" ]]
temp="$(mktemp -d "${TMPDIR:-/tmp}/decodium-qsstv-rs.XXXXXX")"
trap 'rm -rf "${temp}"' EXIT
# The unrelated config header pulls in the complete GUI; replace only its
# include with the one extern needed by dead-stripped RX code. The reference
# encoder, interleaver and RS methods themselves remain unchanged.
sed 's/#include "configparams.h"/extern QString rxDRMImagesPath;/' \
  "${reference}/src/utils/reedsolomoncoder.cpp" > "${temp}/reedsolomoncoder.cpp"
link_gc=(-Wl,--gc-sections)
if [[ "$(uname -s)" == Darwin ]]; then link_gc=(-Wl,-dead_strip); fi
# pkg-config intentionally supplies separate compiler/linker arguments.
# shellcheck disable=SC2046
"${CXX:-c++}" -w -std=c++17 -ffunction-sections -fdata-sections \
  -I"${root}" -I"${reference}/src" -I"${reference}/src/utils" \
  $(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets) \
  "${root}/tests/sstv/tools/qsstv_rs_oracle.cpp" \
  "${root}/src/sstv/digital/HamDrmRsCodec.cpp" \
  "${temp}/reedsolomoncoder.cpp" "${reference}/src/utils/rs.cpp" \
  "${link_gc[@]}" $(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets) -o "${temp}/oracle"
"${temp}/oracle"
