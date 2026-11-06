#!/usr/bin/env bash
# Build the RTL-SDR Blog compatible librtlsdr used by Decodium release jobs.
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

if [[ $# -gt 1 ]]; then
  echo "usage: $0 [install-prefix]" >&2
  exit 2
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RTLSDR_VERSION="${RTLSDR_VERSION:-2.0.2}"
RTLSDR_PREFIX="${1:-${RTLSDR_PREFIX:-${ROOT_DIR}/.ci/cache/librtlsdr-${RTLSDR_VERSION}}}"
RTLSDR_URL="https://github.com/steve-m/librtlsdr/archive/refs/tags/v${RTLSDR_VERSION}.tar.gz"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)}"

for command_name in cmake curl tar; do
  command -v "${command_name}" >/dev/null 2>&1 || {
    echo "error: required command not found: ${command_name}" >&2
    exit 1
  }
done

library_found=false
if [[ -f "${RTLSDR_PREFIX}/include/rtl-sdr.h" ]]; then
  for candidate in \
    "${RTLSDR_PREFIX}/lib/librtlsdr.so" \
    "${RTLSDR_PREFIX}/lib/librtlsdr.dylib" \
    "${RTLSDR_PREFIX}/lib/librtlsdr.dll.a" \
    "${RTLSDR_PREFIX}/bin/librtlsdr.dll"; do
    if [[ -e "${candidate}" ]]; then
      library_found=true
      break
    fi
  done
fi

if [[ "${library_found}" == true ]]; then
  echo "Using cached librtlsdr ${RTLSDR_VERSION}: ${RTLSDR_PREFIX}"
  exit 0
fi

tmpdir="$(mktemp -d)"
cleanup() {
  rm -rf "${tmpdir}"
}
trap cleanup EXIT

echo "Building librtlsdr ${RTLSDR_VERSION} into ${RTLSDR_PREFIX}"
curl -fsSL --retry 3 "${RTLSDR_URL}" -o "${tmpdir}/librtlsdr.tar.gz"
tar -xzf "${tmpdir}/librtlsdr.tar.gz" -C "${tmpdir}"
source_dir="$(find "${tmpdir}" -mindepth 1 -maxdepth 1 -type d -name 'librtlsdr-*' | head -n 1)"
if [[ -z "${source_dir}" || ! -f "${source_dir}/CMakeLists.txt" ]]; then
  echo "error: librtlsdr ${RTLSDR_VERSION} source archive has an unexpected layout" >&2
  exit 1
fi

rm -rf "${RTLSDR_PREFIX}"
cmake_args=(
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_INSTALL_PREFIX="${RTLSDR_PREFIX}"
  -DBUILD_SHARED_LIBS=ON
  -DBUILD_TESTING=OFF
  -DINSTALL_UDEV_RULES=OFF
)
if [[ -n "${MACOSX_DEPLOYMENT_TARGET:-}" ]]; then
  cmake_args+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET}")
fi
if [[ -n "${CMAKE_OSX_ARCHITECTURES:-}" ]]; then
  cmake_args+=("-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}")
fi
cmake -S "${source_dir}" -B "${tmpdir}/build" "${cmake_args[@]}"

# MinGW GCC 16 defaults to C23.  librtlsdr's optional Windows command-line
# tools contain an old getopt implementation that is not C23-compatible, but
# Decodium needs only the shared library and import library.  Build and stage
# exactly those two runtime development artefacts instead of unrelated tools.
if [[ "${MSYSTEM:-}" == MINGW* ]]; then
  cmake --build "${tmpdir}/build" --target rtlsdr --parallel "${JOBS}"

  rtl_dll="$(find "${tmpdir}/build" -type f -name 'librtlsdr.dll' | head -n1)"
  rtl_import_library="$(find "${tmpdir}/build" -type f -name 'librtlsdr.dll.a' | head -n1)"
  rtl_export_header="${source_dir}/include/rtl-sdr_export.h"
  if [[ -z "${rtl_dll}" || -z "${rtl_import_library}" || -z "${rtl_export_header}" ]]; then
    echo "error: MinGW librtlsdr build did not produce the required DLL, import library and export header" >&2
    exit 1
  fi

  mkdir -p "${RTLSDR_PREFIX}/bin" "${RTLSDR_PREFIX}/include" "${RTLSDR_PREFIX}/lib"
  cp "${source_dir}/include/rtl-sdr.h" "${RTLSDR_PREFIX}/include/"
  cp "${rtl_export_header}" "${RTLSDR_PREFIX}/include/"
  cp "${rtl_dll}" "${RTLSDR_PREFIX}/bin/"
  cp "${rtl_import_library}" "${RTLSDR_PREFIX}/lib/"
else
  cmake --build "${tmpdir}/build" --parallel "${JOBS}"
  cmake --install "${tmpdir}/build"
fi

test -f "${RTLSDR_PREFIX}/include/rtl-sdr.h"
find "${RTLSDR_PREFIX}" -type f \( -name 'librtlsdr.so*' -o -name 'librtlsdr*.dylib' -o -name 'librtlsdr.dll' \) -print -quit | grep -q .
echo "librtlsdr ${RTLSDR_VERSION} installed: ${RTLSDR_PREFIX}"
