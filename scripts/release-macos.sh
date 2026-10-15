#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/release-macos.sh [<version>] [--publish] [--repo owner/repo]
                           [--compat-macos 15.0] [--skip-compat-check]
                           [--codesign-identity "-"]
                           [--asset-suffix macos-sequoia-arm64]

Examples:
  scripts/release-macos.sh
  scripts/release-macos.sh <version> --publish --repo elisir80/Decodium-4.0-Core-Shannon

What it does:
  1) Configures the project in ./build
  2) Builds the project in ./build
  3) Generates macOS DMG via CPack (DragNDrop)
  4) Verifies bundle compatibility (absolute deps + minos threshold)
  5) Re-signs the app bundle (ad-hoc by default)
  6) Creates versioned assets:
       decodium4-ft2-<version>-<asset-suffix>.dmg
       decodium4-ft2-<version>-<asset-suffix>-sha256.txt
  7) Optionally creates/updates the GitHub release when --publish is used
EOF
}

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION_RAW=""

if [[ $# -gt 0 && "$1" != --* ]]; then
  VERSION_RAW="$1"
  shift
fi

VERSION="$("${REPO_ROOT}/scripts/ci/resolve-release-version.sh" "${VERSION_RAW}")"
"${REPO_ROOT}/scripts/ci/validate-repository-layout.sh"

PUBLISH=0
REPO="elisir80/Decodium-4.0-Core-Shannon"
COMPAT_MACOS="15.0"
SKIP_COMPAT_CHECK=0
CODESIGN_IDENTITY="${CODESIGN_IDENTITY:--}"
ASSET_SUFFIX=""

version_gt() {
  local lhs="$1"
  local rhs="$2"
  [[ "$(printf '%s\n%s\n' "$lhs" "$rhs" | sort -V | tail -n1)" == "$lhs" && "$lhs" != "$rhs" ]]
}

check_bundle_compatibility() {
  local app_bundle="$1"
  local compat_macos="$2"
  local has_absolute_deps=0
  local has_bad_minos=0
  local has_bad_bundle_paths=0
  local has_bad_rpaths=0

  while IFS= read -r file_path; do
    if ! file "$file_path" | grep -q "Mach-O"; then
      continue
    fi

    local current_id=""
    current_id="$(otool -D "$file_path" 2>/dev/null | awk 'NR==2 {print $1; exit}')"

    if [[ "$file_path" == "$app_bundle/Contents/Frameworks/"* && -n "$current_id" && "$current_id" != @rpath/* ]]; then
      echo "error: non-@rpath install id in bundled framework/dylib:"
      echo "  ${file_path} -> ${current_id}"
      has_bad_bundle_paths=1
    fi

    while IFS= read -r dep_path; do
      if [[ -n "$current_id" && "$dep_path" == "$current_id" ]]; then
        continue
      fi
      case "$dep_path" in
        /opt/*|/usr/local/*|/Users/*)
          echo "error: absolute runtime dependency in bundle:"
          echo "  ${file_path} -> ${dep_path}"
          has_absolute_deps=1
          ;;
        @*Frameworks/*)
          echo "error: stale @executable_path/@loader_path Frameworks reference in bundle:"
          echo "  ${file_path} -> ${dep_path}"
          has_bad_bundle_paths=1
          ;;
      esac
    done < <(otool -L "$file_path" | awk 'NR>1 {print $1}')

    while IFS= read -r rpath; do
      case "$rpath" in
        @loader_path*|@executable_path*)
          ;;
        *)
          echo "error: unsafe runtime search path in bundle:"
          echo "  ${file_path} -> ${rpath}"
          has_bad_rpaths=1
          ;;
      esac
    done < <(otool -l "$file_path" | awk '/LC_RPATH/{flag=1; next} flag && $1=="path"{print $2; flag=0}')

    local minos
    minos="$(otool -l "$file_path" | awk '/LC_BUILD_VERSION/{s=1} s && $1=="minos"{print $2; exit}')"
    if [[ -n "$minos" ]] && version_gt "$minos" "$compat_macos"; then
      echo "error: incompatible min deployment target:"
      echo "  ${file_path} -> minos ${minos} (required <= ${compat_macos})"
      has_bad_minos=1
    fi
  done < <(find "$app_bundle" -type f)

  if [[ -d "$app_bundle/Contents/MacOS/sounds" ]]; then
    echo "error: sounds directory still lives in Contents/MacOS"
    has_bad_bundle_paths=1
  fi
  if [[ ! -d "$app_bundle/Contents/Resources/sounds" ]]; then
    echo "error: missing sounds directory in Contents/Resources"
    has_bad_bundle_paths=1
  fi
  while IFS= read -r file_path; do
    [[ -n "$file_path" ]] || continue
    echo "error: non-executable resource file remains in Contents/MacOS:"
    echo "  ${file_path}"
    has_bad_bundle_paths=1
  done < <(find "$app_bundle/Contents/MacOS" -maxdepth 1 -type f ! -perm -111 -print 2>/dev/null)
  for tool_name in rigctl-wsjtx rigctld-wsjtx rigctlcom-wsjtx; do
    if [[ -L "$app_bundle/Contents/MacOS/$tool_name" ]]; then
      echo "error: bundled Hamlib tool is still a symlink:"
      echo "  $app_bundle/Contents/MacOS/$tool_name"
      has_bad_bundle_paths=1
    fi
  done

  if [[ "$has_absolute_deps" -ne 0 || "$has_bad_minos" -ne 0 || "$has_bad_bundle_paths" -ne 0 || "$has_bad_rpaths" -ne 0 ]]; then
    return 1
  fi
  return 0
}

verify_app_identity() {
  local app_bundle="$1"
  local main_exec=""
  local bundle_id=""
  local bundle_name=""
  local display_name=""
  local required_qml_path=""
  local qt_gui=""
  local qt_dbus=""

  main_exec="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "${app_bundle}/Contents/Info.plist" 2>/dev/null || true)"
  bundle_id="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "${app_bundle}/Contents/Info.plist" 2>/dev/null || true)"
  bundle_name="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleName' "${app_bundle}/Contents/Info.plist" 2>/dev/null || true)"
  display_name="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleDisplayName' "${app_bundle}/Contents/Info.plist" 2>/dev/null || true)"

  if [[ "$(basename "${app_bundle}")" != "Decodium4.app" ]]; then
    echo "error: macOS app bundle must be Decodium4.app, got $(basename "${app_bundle}")"
    return 1
  fi
  if [[ "${main_exec}" != "Decodium4" ]]; then
    echo "error: CFBundleExecutable must be Decodium4, got ${main_exec:-<empty>}"
    return 1
  fi
  if [[ "${bundle_id}" != "org.decodium.decodium4" ]]; then
    echo "error: CFBundleIdentifier must be org.decodium.decodium4, got ${bundle_id:-<empty>}"
    return 1
  fi
  if [[ "${bundle_name}" != "Decodium4" ]]; then
    echo "error: CFBundleName must be Decodium4, got ${bundle_name:-<empty>}"
    return 1
  fi
  if [[ "${display_name}" != "Decodium4" ]]; then
    echo "error: CFBundleDisplayName must be Decodium4, got ${display_name:-<empty>}"
    return 1
  fi
  if [[ ! -x "${app_bundle}/Contents/MacOS/Decodium4" ]]; then
    echo "error: missing executable ${app_bundle}/Contents/MacOS/Decodium4"
    return 1
  fi
  if ! strings "${app_bundle}/Contents/MacOS/Decodium4" \
    | awk 'index($0, "QML OK - entering event loop") {found=1} END {exit found ? 0 : 1}'; then
    echo "error: Contents/MacOS/Decodium4 must be the Decodium QML runtime, not the legacy FT2 UI"
    return 1
  fi
  if [[ -e "${app_bundle}/Contents/MacOS/ft2" ]]; then
    echo "error: stale ft2 executable still present in ${app_bundle}/Contents/MacOS"
    return 1
  fi
  if [[ -d "${app_bundle}/Contents/MacOS/qml" ]]; then
    echo "error: QML runtime files must not remain under Contents/MacOS"
    return 1
  fi
  while IFS= read -r stale_resource_path; do
    [[ -n "${stale_resource_path}" ]] || continue
    echo "error: non-executable resource file must not remain under Contents/MacOS: ${stale_resource_path}"
    return 1
  done < <(find "${app_bundle}/Contents/MacOS" -maxdepth 1 -type f ! -perm -111 -print 2>/dev/null)
  for required_qml_path in \
    "${app_bundle}/Contents/Resources/qml/decodium/BootLoader.qml" \
    "${app_bundle}/Contents/Resources/qml/QtQuick/Controls/qmldir" \
    "${app_bundle}/Contents/Resources/qml/QtQuick/Controls/Material/qmldir" \
    "${app_bundle}/Contents/Resources/qml/QtQuick/Dialogs/qmldir" \
    "${app_bundle}/Contents/Resources/qml/QtQuick/Effects/qmldir" \
    "${app_bundle}/Contents/Resources/qml/QtQuick/Layouts/qmldir" \
    "${app_bundle}/Contents/Resources/qml/QtQuick/Templates/qmldir" \
    "${app_bundle}/Contents/Resources/qml/QtQuick/Window/qmldir" \
    "${app_bundle}/Contents/Resources/qml/QtQml/qmldir" \
    "${app_bundle}/Contents/Resources/qml/Qt/labs/folderlistmodel/qmldir" \
    "${app_bundle}/Contents/Resources/qml/QML/qmldir"; do
    if [[ ! -f "${required_qml_path}" ]]; then
      echo "error: missing Qt QML runtime import in app bundle: ${required_qml_path}"
      return 1
    fi
  done
  if ! find "${app_bundle}/Contents/Resources/qml/QtQuick/Controls" -type f -name '*qtquickcontrols2plugin*.dylib' -print -quit 2>/dev/null | grep -q .; then
    echo "error: missing Qt Quick Controls QML plugin in app bundle"
    return 1
  fi
  for required_image_plugin in \
    libqgif.dylib libqjpeg.dylib libqtiff.dylib libqwebp.dylib; do
    if [[ ! -f "${app_bundle}/Contents/PlugIns/imageformats/${required_image_plugin}" ]]; then
      echo "error: missing Qt image plugin in app bundle: ${required_image_plugin}"
      return 1
    fi
  done
  if [[ ! -f "${app_bundle}/Contents/PlugIns/sqldrivers/libqsqlite.dylib" ]]; then
    echo "error: missing Qt SQLite driver in app bundle: libqsqlite.dylib"
    return 1
  fi
  if ! find "${app_bundle}/Contents/PlugIns/tls" -maxdepth 1 \( -type f -o -type l \) -name '*.dylib' -print -quit 2>/dev/null | grep -q .; then
    echo "warning: missing Qt TLS plugins in app bundle"
  fi
  if [[ -d "${app_bundle}/Contents/Frameworks/QtMultimedia.framework" ]] \
    && ! find "${app_bundle}/Contents/PlugIns/multimedia" -maxdepth 1 \( -type f -o -type l \) -name '*.dylib' -print -quit 2>/dev/null | grep -q .; then
    echo "warning: missing Qt multimedia plugins in app bundle"
  fi
  qt_gui="${app_bundle}/Contents/Frameworks/QtGui.framework/Versions/A/QtGui"
  qt_dbus="${app_bundle}/Contents/Frameworks/QtDBus.framework/Versions/A/QtDBus"
  if [[ -f "${qt_gui}" ]] \
    && otool -L "${qt_gui}" | awk 'NR>1 {print $1}' | grep -Fxq '@rpath/QtDBus.framework/Versions/A/QtDBus' \
    && [[ ! -f "${qt_dbus}" ]]; then
    echo "error: missing QtDBus.framework required by bundled QtGui"
    return 1
  fi
}

main_executable_for_app() {
  local app_bundle="$1"
  local executable_name=""

  executable_name="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "${app_bundle}/Contents/Info.plist" 2>/dev/null || true)"
  if [[ -n "${executable_name}" && -x "${app_bundle}/Contents/MacOS/${executable_name}" ]]; then
    printf '%s\n' "${app_bundle}/Contents/MacOS/${executable_name}"
    return 0
  fi

  find "${app_bundle}/Contents/MacOS" -maxdepth 1 -type f -perm -111 | sort | head -n1
}

sign_app_bundle() {
  local app_bundle="$1"
  local sign_identity="$2"
  local main_exec

  main_exec="$(main_executable_for_app "${app_bundle}")"
  if [[ -z "${main_exec}" ]]; then
    echo "error: unable to locate main executable in ${app_bundle}"
    return 1
  fi

  if ! command -v codesign >/dev/null 2>&1; then
    echo "error: codesign tool not found"
    return 1
  fi

  echo "Signing app bundle with identity: ${sign_identity}"
  # Sign inner code objects first (inside-out). This avoids fragile --deep
  # behavior and keeps runtime Mach-O signatures valid after install_name_tool.
  while IFS= read -r code_file; do
    [[ -n "${code_file}" ]] || continue
    if [[ "${code_file}" == "${main_exec}" ]]; then
      continue
    fi
    if ! file "${code_file}" | grep -q "Mach-O"; then
      continue
    fi
    codesign --force --sign "${sign_identity}" --timestamp=none "${code_file}" >/dev/null
  done < <(find "${app_bundle}/Contents" -type f 2>/dev/null | sort)

  while IFS= read -r bundle_dir; do
    [[ -n "${bundle_dir}" ]] || continue
    codesign --force --sign "${sign_identity}" --timestamp=none "${bundle_dir}" >/dev/null
  done < <(find "${app_bundle}/Contents" -type d \
    \( -name "*.framework" -o -name "*.bundle" -o -name "*.app" -o -name "*.xpc" -o -name "*.appex" \) 2>/dev/null \
    | awk '{print length($0) " " $0}' | sort -rn | cut -d' ' -f2-)

  codesign --force --sign "${sign_identity}" --timestamp=none "${app_bundle}" >/dev/null

  while IFS= read -r verify_file; do
    [[ -n "${verify_file}" ]] || continue
    if [[ "${verify_file}" == "${main_exec}" ]]; then
      continue
    fi
    if ! file "${verify_file}" | grep -q "Mach-O"; then
      continue
    fi
    codesign --verify --verbose=2 "${verify_file}" >/dev/null
  done < <(find "${app_bundle}/Contents" -type f 2>/dev/null | sort)

  codesign --verify --strict --verbose=2 "${app_bundle}" >/dev/null
}

create_dmg_from_staged_root() {
  local staged_root="$1"
  local out_dmg="$2"
  local vol_name="$3"
  local tmp_dmg="${out_dmg}.tmp.dmg"
  local staging_parent=""
  local staging_copy=""
  local attempt=0
  local create_status=0

  staging_parent="$(mktemp -d "${TMPDIR:-/tmp}/decodium-dmg.XXXXXX")"
  staging_copy="${staging_parent}/src"
  ditto "${staged_root}" "${staging_copy}"

  rm -f "${out_dmg}" "${tmp_dmg}"
  for attempt in 1 2 3; do
    if hdiutil create \
      -volname "${vol_name}" \
      -srcfolder "${staging_copy}" \
      -fs HFS+ \
      -format UDZO \
      "${tmp_dmg}" >/dev/null; then
      mv -f "${tmp_dmg}" "${out_dmg}"
      rm -rf "${staging_parent}"
      return 0
    fi

    create_status=$?
    echo "warning: hdiutil create failed on attempt ${attempt}/3 (status ${create_status}); retrying..."
    rm -f "${tmp_dmg}"
    sync || true
    sleep 3
  done

  rm -rf "${staging_parent}"
  return "${create_status}"
}

detach_mountpoint_if_present() {
  local mountpoint="$1"
  local attempts="${2:-5}"
  local dev=""

  for ((i=1; i<=attempts; ++i)); do
    dev="$(hdiutil info | awk -v mp="${mountpoint}" 'index($0, mp) {print $1; exit}')"
    if [[ -z "${dev}" ]]; then
      return 0
    fi

    echo "warning: detaching leftover image ${dev} mounted at ${mountpoint} (attempt ${i}/${attempts})"
    hdiutil detach "${dev}" >/dev/null 2>&1 || hdiutil detach -force "${dev}" >/dev/null 2>&1 || true
    sleep 2
  done

  dev="$(hdiutil info | awk -v mp="${mountpoint}" 'index($0, mp) {print $1; exit}')"
  [[ -z "${dev}" ]]
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --publish)
      PUBLISH=1
      shift
      ;;
    --repo)
      if [[ $# -lt 2 ]]; then
        echo "error: --repo requires a value"
        exit 1
      fi
      REPO="$2"
      shift 2
      ;;
    --compat-macos)
      if [[ $# -lt 2 ]]; then
        echo "error: --compat-macos requires a value"
        exit 1
      fi
      COMPAT_MACOS="$2"
      shift 2
      ;;
    --skip-compat-check)
      SKIP_COMPAT_CHECK=1
      shift
      ;;
    --codesign-identity)
      if [[ $# -lt 2 ]]; then
        echo "error: --codesign-identity requires a value"
        exit 1
      fi
      CODESIGN_IDENTITY="$2"
      shift 2
      ;;
    --asset-suffix)
      if [[ $# -lt 2 ]]; then
        echo "error: --asset-suffix requires a value"
        exit 1
      fi
      ASSET_SUFFIX="$2"
      shift 2
      ;;
    *)
      echo "error: unknown argument: $1"
      usage
      exit 1
      ;;
  esac
done

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
PREFIX="decodium4-ft2"
APP_BUNDLE_NAME="Decodium4.app"
APP_VOLUME_NAME="Decodium4"
ARCH="$(uname -m)"

if [[ "$ARCH" == "x86_64" ]]; then
  ARCH_LABEL="x86_64"
elif [[ "$ARCH" == "arm64" || "$ARCH" == "aarch64" ]]; then
  ARCH_LABEL="arm64"
else
  ARCH_LABEL="$ARCH"
fi

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"

echo "[1/7] Configuring project (macOS target ${COMPAT_MACOS})..."
cmake_args=(
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$COMPAT_MACOS"
  -DFORK_RELEASE_VERSION="$VERSION"
  # Published macOS releases include the native SSTV workspace and HAMDRM.
  # Keep them explicit so a reused build directory cannot retain a feature-off cache.
  -DDECODIUM_ENABLE_SSTV=ON
  -DDECODIUM_ENABLE_HAMDRM=ON
  -DBUILD_TESTING=OFF
  -DWSJT_GENERATE_DOCS=OFF
  -DWSJT_SKIP_MANPAGES=ON
  -DWSJT_BUILD_UTILS=OFF
)

# Respect externally provided CMake prefix paths (e.g. Homebrew Qt on GitHub Actions).
if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
  cmake_args+=("-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
fi
case "${DECODIUM_REQUIRE_RTLSDR:-OFF}" in
  1|ON|on|TRUE|true|YES|yes)
    cmake_args+=("-DDECODIUM_REQUIRE_RTLSDR=ON")
    ;;
esac
BOOST_ROOT_VALUE="${Boost_ROOT:-${BOOST_ROOT:-}}"
if [[ -n "${BOOST_ROOT_VALUE}" ]]; then
  cmake_args+=("-DBoost_ROOT=${BOOST_ROOT_VALUE}" "-DBOOST_ROOT=${BOOST_ROOT_VALUE}")
fi
if [[ -n "${Boost_NO_SYSTEM_PATHS:-}" ]]; then
  cmake_args+=("-DBoost_NO_SYSTEM_PATHS=${Boost_NO_SYSTEM_PATHS}")
fi
if [[ -n "${BOOST_INCLUDEDIR:-}" ]]; then
  cmake_args+=("-DBOOST_INCLUDEDIR=${BOOST_INCLUDEDIR}")
fi
if [[ -n "${BOOST_LIBRARYDIR:-}" ]]; then
  cmake_args+=("-DBOOST_LIBRARYDIR=${BOOST_LIBRARYDIR}")
fi
if [[ -n "${FFTW3_ROOT_DIR:-}" ]]; then
  cmake_args+=("-DFFTW3_ROOT_DIR=${FFTW3_ROOT_DIR}")
fi
# Ensure CMake uses the real gfortran binary path (not a generic symlink),
# so bundle fixup can resolve the correct GCC runtime directories.
if [[ -n "${FC:-}" ]]; then
  cmake_args+=("-DCMAKE_Fortran_COMPILER=${FC}")
fi
if [[ -n "${Hamlib_INCLUDE_DIR:-}" ]]; then
  cmake_args+=("-DHamlib_INCLUDE_DIR=${Hamlib_INCLUDE_DIR}")
fi
if [[ -n "${Hamlib_LIBRARY:-}" ]]; then
  cmake_args+=("-DHamlib_LIBRARY=${Hamlib_LIBRARY}")
fi
if [[ -n "${RtlSdr_INCLUDE_DIR:-}" ]]; then
  cmake_args+=("-DRtlSdr_INCLUDE_DIR=${RtlSdr_INCLUDE_DIR}")
fi
if [[ -n "${RtlSdr_LIBRARY:-}" ]]; then
  cmake_args+=("-DRtlSdr_LIBRARY=${RtlSdr_LIBRARY}")
fi
if [[ -n "${RIGCTL_EXE:-}" ]]; then
  cmake_args+=("-DRIGCTL_EXE=${RIGCTL_EXE}")
fi
if [[ -n "${RIGCTLD_EXE:-}" ]]; then
  cmake_args+=("-DRIGCTLD_EXE=${RIGCTLD_EXE}")
fi
if [[ -n "${RIGCTLCOM_EXE:-}" ]]; then
  cmake_args+=("-DRIGCTLCOM_EXE=${RIGCTLCOM_EXE}")
fi

cmake \
  -S "$ROOT_DIR" \
  -B "$BUILD_DIR" \
  "${cmake_args[@]}"

for release_feature in DECODIUM_ENABLE_SSTV DECODIUM_ENABLE_HAMDRM; do
  if ! grep -Fxq "${release_feature}:BOOL=ON" "${BUILD_DIR}/CMakeCache.txt"; then
    echo "error: advertised macOS release must enable ${release_feature}" >&2
    exit 1
  fi
done

echo "[2/7] Building project..."
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "[3/7] Generating DMG with CPack..."
CPACK_VOLUME_MOUNT="/Volumes/Decodium 4.0 Core Shannon"
detach_mountpoint_if_present "${CPACK_VOLUME_MOUNT}" || true
cpack_status=0
(
  cd "$BUILD_DIR"
  cpack -G DragNDrop
) || cpack_status=$?
STAGED_APP="$(cd "$BUILD_DIR" && ls -td _CPack_Packages/Darwin/DragNDrop/*/"${APP_BUNDLE_NAME}" 2>/dev/null | head -n1 || true)"
if [[ -z "$STAGED_APP" ]]; then
  STAGED_APP="$(cd "$BUILD_DIR" && ls -td _CPack_Packages/Darwin/DragNDrop/*/ft2.app 2>/dev/null | head -n1 || true)"
fi
if [[ -z "$STAGED_APP" ]]; then
  echo "error: unable to locate staged ${APP_BUNDLE_NAME} from CPack output"
  echo "error: cpack exit status: ${cpack_status}"
  exit 1
fi
if [[ "${cpack_status}" -ne 0 ]]; then
  echo "warning: CPack returned status ${cpack_status}, but staged app exists; continuing with manual DMG packaging."
fi
detach_mountpoint_if_present "${CPACK_VOLUME_MOUNT}" || true
STAGED_APP_ABS="${BUILD_DIR}/${STAGED_APP}"
if [[ "$(basename "${STAGED_APP_ABS}")" != "${APP_BUNDLE_NAME}" ]]; then
  RENAMED_APP_ABS="$(dirname "${STAGED_APP_ABS}")/${APP_BUNDLE_NAME}"
  rm -rf "${RENAMED_APP_ABS}"
  mv "${STAGED_APP_ABS}" "${RENAMED_APP_ABS}"
  STAGED_APP_ABS="${RENAMED_APP_ABS}"
  STAGED_APP="${STAGED_APP_ABS#${BUILD_DIR}/}"
fi
STAGED_ROOT_ABS="$(dirname "${STAGED_APP_ABS}")"

echo "[4/7] Normalizing macOS bundle layout and runtime paths..."
"${ROOT_DIR}/scripts/normalize-macos-app.sh" "${STAGED_APP_ABS}"
verify_app_identity "${STAGED_APP_ABS}"
case "${DECODIUM_REQUIRE_RTLSDR:-OFF}" in
  1|ON|on|TRUE|true|YES|yes)
    if ! find "${STAGED_APP_ABS}/Contents/Frameworks" -maxdepth 1 \
        -type f -name 'librtlsdr*.dylib' -print -quit | grep -q .; then
      echo "error: required RTL-SDR runtime library was not bundled"
      exit 1
    fi
    if ! otool -L "${STAGED_APP_ABS}/Contents/MacOS/Decodium4" \
        | awk 'NR>1 {print $1}' | grep -Eq '^@rpath/librtlsdr'; then
      echo "error: Decodium4 is not linked to the bundled RTL-SDR runtime"
      exit 1
    fi
    echo "RTL-SDR runtime bundled and linked"
    ;;
esac

if ! find "${STAGED_APP_ABS}/Contents/Frameworks" -maxdepth 1 \
    -type f -name 'libopenjp2*.dylib' -print -quit | grep -q .; then
  echo "error: HAMDRM OpenJPEG runtime library was not bundled"
  exit 1
fi
if ! otool -L "${STAGED_APP_ABS}/Contents/MacOS/Decodium4" \
    | awk 'NR>1 {print $1}' | grep -Eq '^@rpath/libopenjp2'; then
  echo "error: Decodium4 is not linked to the bundled OpenJPEG runtime"
  exit 1
fi
if [[ ! -f "${STAGED_APP_ABS}/Contents/Resources/doc/wsjtx/THIRD_PARTY_LICENSES_OPENJPEG.md" ]]; then
  echo "error: OpenJPEG third-party notice is missing from the app bundle"
  exit 1
fi
echo "HAMDRM OpenJPEG runtime and notice bundled"

if [[ "$SKIP_COMPAT_CHECK" -eq 0 ]]; then
  echo "[5/7] Checking bundle compatibility target macOS ${COMPAT_MACOS}..."
  if ! check_bundle_compatibility "${BUILD_DIR}/${STAGED_APP}" "$COMPAT_MACOS"; then
    echo
    echo "Bundle compatibility check failed."
    echo "Use --skip-compat-check to override (not recommended for release)."
    exit 1
  fi
else
  echo "[5/7] Skipping compatibility checks (--skip-compat-check)."
fi

if [[ -z "$ASSET_SUFFIX" ]]; then
  ASSET_SUFFIX="macos-${ARCH_LABEL}"
fi

DMG_OUT="${PREFIX}-${VERSION}-${ASSET_SUFFIX}.dmg"
SHA_OUT="${PREFIX}-${VERSION}-${ASSET_SUFFIX}-sha256.txt"

echo "[6/7] Re-signing app bundle..."
sign_app_bundle "${STAGED_APP_ABS}" "${CODESIGN_IDENTITY}"

echo "[7/7] Creating release assets..."
create_dmg_from_staged_root "${STAGED_ROOT_ABS}" "${BUILD_DIR}/${DMG_OUT}" "${APP_VOLUME_NAME}"
(
  cd "$BUILD_DIR"
  shasum -a 256 "$DMG_OUT" > "$SHA_OUT"
)

echo "Assets ready:"
ls -lh "${BUILD_DIR}/${DMG_OUT}" "${BUILD_DIR}/${SHA_OUT}"
echo
cat "${BUILD_DIR}/${SHA_OUT}"

if [[ "$PUBLISH" -eq 1 ]]; then
  if ! command -v gh >/dev/null; then
    echo "error: gh CLI not found. Install it first (brew install gh)."
    exit 1
  fi

  NOTES_FILE="$(mktemp)"
  cat >"$NOTES_FILE" <<EOF
# Decodium 4 FT2 ${VERSION} (macOS)

## English (UK)
This release includes fork updates up to \`${VERSION}\`.

If the app does not start on macOS, run from Terminal:
\`sudo xattr -r -d com.apple.quarantine /Applications/Decodium4.app\`

See \`CHANGELOG.md\` for full details.

Assets:
- \`${DMG_OUT}\`
- \`${SHA_OUT}\`

## Italiano
Questa release include aggiornamenti fork fino a \`${VERSION}\`.

Se l'app non si avvia su macOS, esegui da Terminale:
\`sudo xattr -r -d com.apple.quarantine /Applications/Decodium4.app\`

Per i dettagli completi, vedi \`CHANGELOG.md\`.

Asset:
- \`${DMG_OUT}\`
- \`${SHA_OUT}\`

## Espanol
Esta release incluye actualizaciones del fork hasta \`${VERSION}\`.

Si la app no inicia en macOS, ejecuta en Terminal:
\`sudo xattr -r -d com.apple.quarantine /Applications/Decodium4.app\`

Para todos los detalles, ver \`CHANGELOG.md\`.

Artefactos:
- \`${DMG_OUT}\`
- \`${SHA_OUT}\`
EOF

  echo "[publish] Publishing release to ${REPO}..."
  if gh release view "$VERSION" --repo "$REPO" >/dev/null 2>&1; then
    gh release upload "$VERSION" \
      "${BUILD_DIR}/${DMG_OUT}" \
      "${BUILD_DIR}/${SHA_OUT}" \
      --repo "$REPO" \
      --clobber
    gh release edit "$VERSION" --repo "$REPO" --notes-file "$NOTES_FILE"
  else
    gh release create "$VERSION" \
      "${BUILD_DIR}/${DMG_OUT}" \
      "${BUILD_DIR}/${SHA_OUT}" \
      --repo "$REPO" \
      --title "Decodium 4 FT2 ${VERSION} (macOS)" \
      --notes-file "$NOTES_FILE"
  fi

  rm -f "$NOTES_FILE"
  echo "Release published."
fi
