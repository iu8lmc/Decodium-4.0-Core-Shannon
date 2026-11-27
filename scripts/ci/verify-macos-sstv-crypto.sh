#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 <app-bundle> <minimum-macos> <arm64|x86_64>" >&2
  exit 2
fi
app="$1"
minimum="$2"
arch="$3"
executable="$app/Contents/MacOS/Decodium4"
crypto="$app/Contents/Frameworks/libcrypto.3.dylib"
license="$app/Contents/Resources/doc/wsjtx/OPENSSL_LICENSE.txt"
[[ -f "$executable" && -f "$crypto" && ! -L "$crypto" && -s "$license" && ! -L "$license" ]] || {
  echo "error: missing executable, OpenSSL runtime, or OpenSSL licence" >&2; exit 1;
}
case "$arch" in arm64|x86_64) ;; *) echo "error: unsupported architecture" >&2; exit 1 ;; esac
lipo "$executable" -verify_arch "$arch"
lipo "$crypto" -verify_arch "$arch"
otool -L "$executable" | awk 'NR>1 {print $1}' | grep -Fxq '@rpath/libcrypto.3.dylib' || {
  echo "error: executable is not linked to bundled OpenSSL Crypto" >&2; exit 1;
}
minos="$(otool -l "$crypto" | awk '/LC_BUILD_VERSION/{s=1} s && $1=="minos"{print $2; exit}')"
python3 - "$minos" "$minimum" <<'PY'
import re, sys
actual, required = sys.argv[1:]
if not re.fullmatch(r'\d+(?:\.\d+){1,2}', actual) or not re.fullmatch(r'\d+(?:\.\d+){1,2}', required):
    raise SystemExit('error: missing or invalid OpenSSL minimum macOS version')
def version(s):
    return tuple((list(map(int, s.split('.'))) + [0, 0])[:3])
if version(actual) > version(required):
    raise SystemExit(f'error: OpenSSL needs macOS {actual}; release promises {required}')
PY
echo "SSTV OpenSSL Crypto is bundled for $arch and macOS $minimum (library minimum $minos)"
