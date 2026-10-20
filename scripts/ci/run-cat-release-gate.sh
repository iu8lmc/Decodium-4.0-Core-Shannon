#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Required release gate for the CAT contract.  It intentionally uses the
# release build directory and its pinned Hamlib rather than an unrelated CI
# image, so a package cannot be built after this gate has been skipped.
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 <cmake-build-dir> [parallel-jobs]" >&2
  exit 64
fi

build_dir="$1"
jobs="${2:-2}"
if [[ ! -f "${build_dir}/CTestTestfile.cmake" ]]; then
  echo "error: ${build_dir} was not configured with BUILD_TESTING=ON" >&2
  exit 1
fi

# On Windows, CTest launches the compiled test executables directly.  The
# MSYS2 runner's build environment does not automatically put Qt's private
# prefix nor the pinned Hamlib bin directory on PATH, so every test can fail
# with 0xc0000135 even though compilation succeeded.  Add only directories
# which actually exist; this is harmless on macOS/Linux and keeps the release
# gate tied to the same runtime used by the package.
for cat_runtime_dir in "${QT_PREFIX:-}/bin" "${HAMLIB_PREFIX:-}/bin" /mingw64/bin; do
  if [[ -n "${cat_runtime_dir}" && -d "${cat_runtime_dir}" ]]; then
    PATH="${cat_runtime_dir}:${PATH}"
  fi
done
export PATH

test_targets=(
  test_hamlib_release_contract
  test_cat_configuration_snapshot
  test_transceiver_base_split
  test_cw_cat_text
  test_serial_port_registry
  test_ptt_transition_policy
)
test_pattern='^(test_hamlib_release_contract|test_cat_configuration_snapshot|test_transceiver_base_split|test_cw_cat_text|test_serial_port_registry|test_ptt_transition_policy)$'

echo "Building mandatory CAT release tests with ${jobs} parallel jobs"
cmake --build "${build_dir}" --target "${test_targets[@]}" --parallel "${jobs}"

echo "Running mandatory CAT release tests"
ctest --test-dir "${build_dir}" -R "${test_pattern}" --output-on-failure
