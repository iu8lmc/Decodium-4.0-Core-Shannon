#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/decodium-crypto-gate.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
arch="$(uname -m)"
case "$arch" in arm64) other=x86_64 ;; x86_64) other=arm64 ;; *) exit 77 ;; esac
app="$tmp/Test.app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Frameworks" "$app/Contents/Resources/doc/wsjtx"
printf 'int crypto_fixture(void) { return 42; }\n' > "$tmp/lib.c"
printf 'extern int crypto_fixture(void); int main(void) { return crypto_fixture() != 42; }\n' > "$tmp/main.c"
printf 'test fixture licence\n' > "$app/Contents/Resources/doc/wsjtx/OPENSSL_LICENSE.txt"
lib="$app/Contents/Frameworks/libcrypto.3.dylib"
exe="$app/Contents/MacOS/Decodium4"
gate="$root/scripts/ci/verify-macos-sstv-crypto.sh"
for minimum in 12.0 13.0; do
  clang -arch "$arch" -mmacosx-version-min="$minimum" -dynamiclib "$tmp/lib.c" \
    -install_name @rpath/libcrypto.3.dylib -o "$lib"
  clang -arch "$arch" -mmacosx-version-min=13.0 "$tmp/main.c" "$lib" -o "$exe"
  bash "$gate" "$app" 13.0 "$arch"
done
if bash "$gate" "$app" 12.0 "$arch"; then echo 'accepted too-new library' >&2; exit 1; fi
if bash "$gate" "$app" 13.0 "$other"; then echo 'accepted wrong architecture' >&2; exit 1; fi
mv "$lib" "$tmp/external.dylib"
ln -s "$tmp/external.dylib" "$lib"
if bash "$gate" "$app" 13.0 "$arch"; then echo 'accepted external library symlink' >&2; exit 1; fi
rm "$lib"
mv "$tmp/external.dylib" "$lib"
install_name_tool -change @rpath/libcrypto.3.dylib /tmp/not-bundled-libcrypto.3.dylib "$exe"
if bash "$gate" "$app" 13.0 "$arch"; then echo 'accepted external crypto' >&2; exit 1; fi
rm "$app/Contents/Resources/doc/wsjtx/OPENSSL_LICENSE.txt"
if bash "$gate" "$app" 13.0 "$arch"; then echo 'accepted missing licence' >&2; exit 1; fi
echo 'PASS macOS SSTV crypto gate A/B and negative checks'
