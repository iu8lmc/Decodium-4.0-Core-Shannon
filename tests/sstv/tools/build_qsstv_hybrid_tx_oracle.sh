#!/usr/bin/env bash
# Build the instrumented QSSTV receiver/transmitter and tiny codec adapter.
set -euo pipefail
if [[ $# != 2 ]]; then echo "usage: $0 QSSTV_CHECKOUT NEW_OUTPUT_DIRECTORY" >&2; exit 2; fi
repo_dir="$(cd "$(dirname "$0")/../../.." && pwd)"
source_dir="$(cd "$1" && pwd)"
revision=a84584248d3307bfa081362031ff20f365337633
[[ "$(git -C "$source_dir" rev-parse HEAD)" == "$revision" ]] || { echo 'unexpected QSSTV revision' >&2; exit 1; }
mkdir "$2"
out="$(cd "$2" && pwd)"
mkdir "$out/source" "$out/stub"
git -C "$source_dir" archive "$revision" | tar -xf - -C "$out/source"
git -C "$out/source" apply "$repo_dir/tests/sstv/fixtures/qsstv-experimental-hybrid-tx-oracle.patch"
cmake -S "$out/source/src" -B "$out/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$out/build" -j "${DECODIUM_ORACLE_BUILD_JOBS:-4}"
cat > "$out/stub/configparams.h" <<'EOF'
#include <QString>
extern QString hybridFtpRemoteHost, hybridFtpLogin, hybridFtpPassword, hybridFtpRemoteDirectory;
#define LOGALL 0
inline void addToLog(const QString&, int) {}
EOF
# shellcheck disable=SC2207
qt_flags=($(pkg-config --cflags --libs Qt6Core))
"${CXX:-c++}" -std=c++17 -I"$out/stub" -I"$out/source/src/utils" \
  "$repo_dir/tests/sstv/tools/qsstv_hybrid_oracle.cpp" "$out/source/src/utils/hybridcrypt.cpp" \
  "${qt_flags[@]}" -o "$out/codec-oracle"
printf 'QSSTV build: %s/build\nCodec oracle: %s/codec-oracle\n' "$out" "$out"
