#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${repo_root}"

version="$(scripts/ci/resolve-release-version.sh)"

required_runtime_files=(
  ALLCALL7.TXT
  CALL3.TXT
  cty.dat
  cty.dat_copyright.txt
  eclipse.txt
  grid.dat
  sat.dat
)

for file in "${required_runtime_files[@]}"; do
  if [[ ! -s "resources/runtime/${file}" ]]; then
    echo "Missing required runtime file: resources/runtime/${file}" >&2
    exit 1
  fi
done

required_files=(
  CMake/templates/CMakeCPackOptions.cmake.in
  CMake/templates/decodium.qrc.in
  CMake/templates/decodium_config.h.in
  packaging/linux/message_aggregator.desktop
  packaging/linux/wsjtx.desktop
  packaging/package_description.txt
  "doc/GITHUB_RELEASE_BODY_${version}.md"
)

for file in "${required_files[@]}"; do
  if [[ ! -s "${file}" ]]; then
    echo "Missing required repository file: ${file}" >&2
    exit 1
  fi
done

obsolete_root_entries=(
  .cirrus.yml
  ALLCALL7.TXT
  BUGS
  CALL3.TXT
  CMakeCPackOptions.cmake.in
  NEWS
  README
  THANKS
  aethersdr
  cty.dat
  cty.dat_copyright.txt
  decodium.qrc.in
  decodium_config.h.in
  decodium_qml.rc
  eclipse.txt
  g4wjs.txt
  grid.dat
  jt9.txt
  message_aggregator.desktop
  package_description.txt
  sat.dat
  wsjtx.desktop
)

for entry in "${obsolete_root_entries[@]}"; do
  if [[ -e "${entry}" || -L "${entry}" ]]; then
    echo "Obsolete root entry reintroduced: ${entry}" >&2
    exit 1
  fi
done

echo "Repository layout valid for version ${version}."
