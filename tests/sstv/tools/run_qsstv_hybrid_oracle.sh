#!/usr/bin/env bash
# Compile the independently sourced QSSTV codec with synthetic inputs.
set -euo pipefail
if [[ $# != 1 ]]; then echo "usage: $0 QSSTV_SOURCE_CHECKOUT" >&2; exit 2; fi
repo_dir="$(cd "$(dirname "$0")/../../.." && pwd)"
reference="$(cd "$1" && pwd)"
[[ "$(git -C "$reference" rev-parse HEAD)" == 31c323ccb848bade1dbab0a96890073f7b9455ea ]] || { echo 'unexpected QSSTV revision' >&2; exit 1; }
git -C "$reference" diff --quiet HEAD -- src/utils/hybridcrypt.cpp src/utils/hybridcrypt.h || { echo 'QSSTV codec modified' >&2; exit 1; }
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/decodium-qsstv-hybrid.XXXXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT
cat > "$tmp_dir/configparams.h" <<'EOF'
#include <QString>
extern QString hybridFtpRemoteHost, hybridFtpLogin, hybridFtpPassword, hybridFtpRemoteDirectory;
#define LOGALL 0
inline void addToLog(const QString&, int) {}
EOF
# Qt 6 rejects indexing an empty QString at a nonzero position. Allocate the
# destination explicitly; this only repairs the reference's reversal helper.
python3 - "$reference/src/utils/hybridcrypt.cpp" "$tmp_dir/hybridcrypt.cpp" <<'PY'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
old = '  QString t;\n  for (i=0,j=s.length()-1;'
new = '  QString t(s.length(), QChar(0));\n  for (i=0,j=s.length()-1;'
if source.count(old) != 1:
    raise SystemExit('unexpected QSSTV reversal helper')
pathlib.Path(sys.argv[2]).write_text(source.replace(old, new))
PY
# shellcheck disable=SC2207
qt_flags=($(pkg-config --cflags --libs Qt6Core))
"${CXX:-c++}" -std=c++17 -I"$tmp_dir" -I"$reference/src/utils" \
  "$repo_dir/tests/sstv/tools/qsstv_hybrid_oracle.cpp" "$tmp_dir/hybridcrypt.cpp" \
  "${qt_flags[@]}" -o "$tmp_dir/oracle"
for variant in a b; do
  "$tmp_dir/oracle" "$variant" "$tmp_dir/$variant.bin"
  cmp "$tmp_dir/$variant.bin" "$repo_dir/tests/sstv/fixtures/qsstv-hybrid-server-$variant.bin"
done
printf 'PASS QSSTV hybrid server A/B fixtures\n'
