#!/usr/bin/env bash
set -euo pipefail

# Build the same pinned OpenSSL for every macOS release target. Homebrew
# bottles can require a newer macOS than the app's advertised minimum.
if [[ $# -ne 3 ]]; then
  echo "usage: $0 <absolute-prefix> <minimum-macos> <arm64|x86_64>" >&2
  exit 2
fi
prefix="$1"
minimum="$2"
arch="$3"
case "$prefix" in /*) ;; *) echo "prefix must be absolute" >&2; exit 2 ;; esac
case "$minimum" in *[!0-9.]*|'') echo "invalid macOS minimum" >&2; exit 2 ;; esac
case "$arch" in
  arm64) target=darwin64-arm64-cc ;;
  x86_64) target=darwin64-x86_64-cc ;;
  *) echo "unsupported architecture: $arch" >&2; exit 2 ;;
esac

version=3.5.8
sha256=a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2
tmp="$(mktemp -d "${TMPDIR:-/tmp}/decodium-openssl.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
archive="$tmp/openssl.tar.gz"
curl --fail --location --retry 3 --output "$archive" \
  "https://www.openssl.org/source/openssl-${version}.tar.gz"
printf '%s  %s\n' "$sha256" "$archive" | shasum -a 256 -c -
tar -xzf "$archive" -C "$tmp"
cd "$tmp/openssl-${version}"
export MACOSX_DEPLOYMENT_TARGET="$minimum"
./Configure "$target" shared no-tests no-apps no-module \
  --prefix="$prefix" --openssldir="$prefix/ssl"
make -j "${DECODIUM_OPENSSL_JOBS:-4}" build_libs
make install_dev
mkdir -p "$prefix/share/licenses/openssl"
cp LICENSE.txt "$prefix/share/licenses/openssl/LICENSE.txt"
