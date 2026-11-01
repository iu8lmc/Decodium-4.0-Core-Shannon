#!/usr/bin/env bash
set -euo pipefail
if [[ $# -lt 1 || $# -gt 3 ]]; then
  echo "usage: $0 <decodium-binary> [--tamper | --tamper-payload a|b | --tamper-manifest a|b | --downgrade-manifest a|b]" >&2
  exit 2
fi
binary="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
mode="${2:-}"
variant="${3:-}"
if [[ ( -n "$mode" && "$mode" != --tamper && "$mode" != --tamper-payload && "$mode" != --tamper-manifest && "$mode" != --downgrade-manifest ) \
   || ( ( "$mode" == --tamper-payload || "$mode" == --tamper-manifest || "$mode" == --downgrade-manifest ) && "$variant" != a && "$variant" != b ) \
   || ( "$mode" != --tamper-payload && "$mode" != --tamper-manifest && "$mode" != --downgrade-manifest && -n "$variant" ) ]]; then
  echo 'invalid mode' >&2; exit 2
fi
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/decodium-linux-sharing.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/data" "$tmp/config" "$tmp/cache" "$tmp/runtime"
chmod 700 "$tmp/runtime"
export XDG_DATA_HOME="$tmp/data" XDG_CONFIG_HOME="$tmp/config" XDG_CACHE_HOME="$tmp/cache"
export XDG_RUNTIME_DIR="$tmp/runtime" DECODIUM_LAB_ALLOW_SECRET_SERVICE=1
export QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software QT_QUICK_CONTROLS_STYLE=Basic
unset GNOME_KEYRING_CONTROL SSH_AUTH_SOCK

# The keyring and session bus are disposable and separate from the user's
# normal login session. The password and images are public test fixtures.
dbus-run-session -- bash -c '
  set -euo pipefail
  printf "temporary-test-keyring\n" | gnome-keyring-daemon --login >/dev/null
  gnome-keyring-daemon --start --components=secrets >/dev/null
  args=()
  if [[ -n "$2" ]]; then args+=("$2"); fi
  if [[ -n "$4" ]]; then args+=("$4"); fi
  python3 "$3/tests/sstv/tools/run_share_app_transfer_lab.py" "${args[@]}" "$1" \
    "$3/tests/sstv/fixtures/localhost-test-only.crt" \
    "$3/tests/sstv/fixtures/localhost-test-only.key" \
    "$3/tests/sstv/fixtures/qsstv-image-a.jpg" \
    "$3/tests/sstv/fixtures/qsstv-image-b.jpg"
' _ "$binary" "$mode" "$root" "$variant"
