#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
version_file="${repo_root}/fork_release_version.txt"
requested="${1:-}"

if [[ ! -s "${version_file}" ]]; then
  echo "Missing or empty version file: ${version_file}" >&2
  exit 1
fi

version="$(tr -d '\r\n[:space:]' < "${version_file}")"
if [[ ! "${version}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Invalid version in fork_release_version.txt: ${version}" >&2
  exit 1
fi

if [[ -n "${requested}" ]]; then
  requested="${requested#refs/tags/}"
  requested="${requested#v}"
  if [[ "${requested}" != "${version}" ]]; then
    echo "Release version mismatch: requested ${requested}, repository declares ${version}" >&2
    echo "Update fork_release_version.txt and the release notes before building." >&2
    exit 1
  fi
fi

printf '%s\n' "${version}"
