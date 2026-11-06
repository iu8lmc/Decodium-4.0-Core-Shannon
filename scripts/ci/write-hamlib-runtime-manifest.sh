#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Record the exact Hamlib runtime library shipped in a release payload.  This
# is deliberately generated from the staged package, not from the build
# prefix, so a successful manifest is proof that the library is really there.
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 <output-json> <hamlib-version> <runtime-root> <windows|macos|linux>" >&2
  exit 64
fi

output_file="$1"
hamlib_version="$2"
runtime_root="${3%/}"
platform="$4"

if [[ -z "${hamlib_version}" || ! -d "${runtime_root}" ]]; then
  echo "error: Hamlib manifest requires a version and an existing runtime root" >&2
  exit 64
fi

case "${platform}" in
  windows) library_pattern=( -iname 'libhamlib*.dll' ) ;;
  macos)   library_pattern=( -name 'libhamlib*.dylib' ) ;;
  linux)   library_pattern=( -name 'libhamlib.so*' ) ;;
  *)
    echo "error: unsupported platform '${platform}'" >&2
    exit 64
    ;;
esac

json_escape() {
  printf '%s' "$1" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g'
}

sha256_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

runtime_libraries=()
# macOS ships Bash 3.2, which has no mapfile/readarray.  Process substitution
# keeps this loop in the current shell, unlike a pipeline into while-read.
while IFS= read -r path; do
  runtime_libraries+=("${path}")
done < <(find "${runtime_root}" -type f "${library_pattern[@]}" -print | LC_ALL=C sort)

if [[ "${#runtime_libraries[@]}" -eq 0 ]]; then
  echo "error: no Hamlib runtime library found under ${runtime_root}" >&2
  exit 1
fi

mkdir -p "$(dirname "${output_file}")"
tmp_file="${output_file}.tmp.$$"
trap 'rm -f "${tmp_file}"' EXIT

{
  printf '{\n'
  printf '  "component": "Hamlib",\n'
  printf '  "version": "%s",\n' "$(json_escape "${hamlib_version}")"
  printf '  "platform": "%s",\n' "$(json_escape "${platform}")"
  printf '  "libraries": [\n'
  for index in "${!runtime_libraries[@]}"; do
    library="${runtime_libraries[$index]}"
    relative_path="${library#"${runtime_root}/"}"
    checksum="$(sha256_file "${library}")"
    bytes="$(wc -c < "${library}" | tr -d '[:space:]')"
    comma=','
    if [[ "${index}" -eq $(( ${#runtime_libraries[@]} - 1 )) ]]; then
      comma=''
    fi
    printf '    {"path": "%s", "sha256": "%s", "bytes": %s}%s\n' \
      "$(json_escape "${relative_path}")" "${checksum}" "${bytes}" "${comma}"
  done
  printf '  ]\n'
  printf '}\n'
} > "${tmp_file}"

mv -f "${tmp_file}" "${output_file}"
trap - EXIT
echo "Hamlib runtime manifest written: ${output_file}"
