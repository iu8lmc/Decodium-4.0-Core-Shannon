#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 3 ]]; then
  echo "usage: $0 <openssl-prefix> <minimum-macos> <arm64|x86_64>" >&2
  exit 2
fi
prefix="$1"
minimum="$2"
arch="$3"
case "$arch" in arm64|x86_64) ;; *) echo 'unsupported architecture' >&2; exit 2 ;; esac
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/decodium-openssl-runtime.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
lib="$prefix/lib/libcrypto.3.dylib"
clang -arch "$arch" -mmacosx-version-min="$minimum" -Wall -Wextra -Werror \
  -I"$prefix/include" "$root/tests/sstv/tools/test_macos_openssl_runtime.c" \
  "$lib" -o "$tmp/check"
otool -L "$tmp/check" | awk 'NR>1 {print $1}' | grep -Fxq "$lib"
arch -"$arch" "$tmp/check"
