#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/release-macos.sh <version> [--publish] [--repo owner/repo]
                                 [--compat-macos 15.0] [--skip-compat-check]
                                 [--codesign-identity "-"]
                                 [--asset-suffix macos-sequoia-arm64]

Examples:
  scripts/release-macos.sh v1.3.8
  scripts/release-macos.sh v1.3.8 --publish --repo elisir80/decodium3-build-macos

What it does:
  1) Configures the project in ./build
  2) Builds the project in ./build
  3) Generates macOS DMG via CPack (DragNDrop)
  4) Verifies bundle compatibility (absolute deps + minos threshold)
  5) Re-signs the app bundle (ad-hoc by default)
  6) Creates versioned assets:
       decodium3-ft2-<version>-<asset-suffix>.dmg
       decodium3-ft2-<version>-<asset-suffix>.zip
       decodium3-ft2-<version>-<asset-suffix>-sha256.txt
  7) Optionally creates/updates the GitHub release when --publish is used
EOF
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

VERSION_RAW="$1"
shift

VERSION="$VERSION_RAW"
if [[ "$VERSION" != v* ]]; then
  VERSION="v${VERSION}"
fi

PUBLISH=0
REPO="elisir80/decodium3-build-macos"
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

  while IFS= read -r file_path; do
    if ! file "$file_path" | grep -q "Mach-O"; then
      continue
    fi

    while IFS= read -r dep_path; do
      case "$dep_path" in
        /opt/*|/usr/local/*|/Users/*)
          echo "error: absolute runtime dependency in bundle:"
          echo "  ${file_path} -> ${dep_path}"
          has_absolute_deps=1
          ;;
      esac
    done < <(otool -L "$file_path" | awk 'NR>1 {print $1}')

    local minos
    minos="$(otool -l "$file_path" | awk '/LC_BUILD_VERSION/{s=1} s && $1=="minos"{print $2; exit}')"
    if [[ -n "$minos" ]] && version_gt "$minos" "$compat_macos"; then
      echo "error: incompatible min deployment target:"
      echo "  ${file_path} -> minos ${minos} (required <= ${compat_macos})"
      has_bad_minos=1
    fi
  done < <(find "$app_bundle" -type f)

  if [[ "$has_absolute_deps" -ne 0 || "$has_bad_minos" -ne 0 ]]; then
    return 1
  fi
  return 0
}

sign_app_bundle() {
  local app_bundle="$1"
  local sign_identity="$2"
  local main_exec

  main_exec="${app_bundle}/Contents/MacOS/$(basename "${app_bundle%.app}")"

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
  done < <(find "${app_bundle}/Contents" -type f \
    \( -name "*.dylib" -o -name "*.so" -o -perm -111 \) 2>/dev/null | sort)

  # Leave the main executable signature untouched: on Xcode 16.x, attempting
  # to re-sign it can fail when non-code files live in Contents/MacOS.

  while IFS= read -r bundle_dir; do
    [[ -n "${bundle_dir}" ]] || continue
    codesign --force --sign "${sign_identity}" --timestamp=none "${bundle_dir}" >/dev/null
  done < <(find "${app_bundle}/Contents" -type d \
    \( -name "*.framework" -o -name "*.bundle" -o -name "*.app" -o -name "*.xpc" -o -name "*.appex" \) 2>/dev/null \
    | awk '{print length($0) " " $0}' | sort -rn | cut -d' ' -f2-)

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
}

create_dmg_from_staged_root() {
  local staged_root="$1"
  local out_dmg="$2"
  local vol_name="$3"
  local tmp_dmg="${out_dmg}.tmp.dmg"

  rm -f "${out_dmg}" "${tmp_dmg}"
  hdiutil create \
    -volname "${vol_name}" \
    -srcfolder "${staged_root}" \
    -fs HFS+ \
    -format UDZO \
    "${tmp_dmg}" >/dev/null
  mv -f "${tmp_dmg}" "${out_dmg}"
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
PREFIX="decodium3-ft2"
ARCH="$(uname -m)"

if [[ "$ARCH" == "x86_64" ]]; then
  ARCH_LABEL="x86_64"
elif [[ "$ARCH" == "arm64" || "$ARCH" == "aarch64" ]]; then
  ARCH_LABEL="arm64"
else
  ARCH_LABEL="$ARCH"
fi

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"

echo "[1/6] Configuring project (macOS target ${COMPAT_MACOS})..."
cmake_args=(
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$COMPAT_MACOS"
  -DFORK_RELEASE_VERSION="$VERSION"
  -DWSJT_GENERATE_DOCS=OFF
  -DWSJT_SKIP_MANPAGES=ON
  -DWSJT_BUILD_UTILS=OFF
)

# Respect externally provided CMake prefix paths (e.g. qt@5 on GitHub Actions).
if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
  cmake_args+=("-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
fi
# Ensure CMake uses the real gfortran binary path (not a generic symlink),
# so bundle fixup can resolve the correct GCC runtime directories.
if [[ -n "${FC:-}" ]]; then
  cmake_args+=("-DCMAKE_Fortran_COMPILER=${FC}")
fi

cmake \
  -S "$ROOT_DIR" \
  -B "$BUILD_DIR" \
  "${cmake_args[@]}"

echo "[2/6] Building project..."
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "[3/6] Generating DMG with CPack..."
(
  cd "$BUILD_DIR"
  cpack -G DragNDrop
)

LATEST_DMG="$(cd "$BUILD_DIR" && ls -t *-Darwin.dmg 2>/dev/null | head -n1 || true)"
if [[ -z "$LATEST_DMG" ]]; then
  echo "error: no DMG produced by CPack"
  exit 1
fi
STAGED_APP="$(cd "$BUILD_DIR" && ls -td _CPack_Packages/Darwin/DragNDrop/*/ft2.app 2>/dev/null | head -n1 || true)"
if [[ -z "$STAGED_APP" ]]; then
  echo "error: unable to locate staged ft2.app from CPack output"
  exit 1
fi
STAGED_APP_ABS="${BUILD_DIR}/${STAGED_APP}"
STAGED_ROOT_ABS="$(dirname "${STAGED_APP_ABS}")"

if [[ "$SKIP_COMPAT_CHECK" -eq 0 ]]; then
  echo "[4/6] Checking bundle compatibility target macOS ${COMPAT_MACOS}..."
  if ! check_bundle_compatibility "${BUILD_DIR}/${STAGED_APP}" "$COMPAT_MACOS"; then
    echo
    echo "Bundle compatibility check failed."
    echo "Use --skip-compat-check to override (not recommended for release)."
    exit 1
  fi
else
  echo "[4/6] Skipping compatibility checks (--skip-compat-check)."
fi

if [[ -z "$ASSET_SUFFIX" ]]; then
  ASSET_SUFFIX="macos-${ARCH_LABEL}"
fi

DMG_OUT="${PREFIX}-${VERSION}-${ASSET_SUFFIX}.dmg"
ZIP_OUT="${PREFIX}-${VERSION}-${ASSET_SUFFIX}.zip"
SHA_OUT="${PREFIX}-${VERSION}-${ASSET_SUFFIX}-sha256.txt"

echo "[5/6] Re-signing app bundle..."
sign_app_bundle "${STAGED_APP_ABS}" "${CODESIGN_IDENTITY}"

echo "[6/6] Creating release assets..."
create_dmg_from_staged_root "${STAGED_ROOT_ABS}" "${BUILD_DIR}/${DMG_OUT}" "ft2"
(
  /usr/bin/ditto -c -k --sequesterRsrc --keepParent "${STAGED_APP_ABS}" "${BUILD_DIR}/${ZIP_OUT}"
  cd "$BUILD_DIR"
  shasum -a 256 "$DMG_OUT" "$ZIP_OUT" > "$SHA_OUT"
)

echo "Assets ready:"
ls -lh "${BUILD_DIR}/${DMG_OUT}" "${BUILD_DIR}/${ZIP_OUT}" "${BUILD_DIR}/${SHA_OUT}"
echo
cat "${BUILD_DIR}/${SHA_OUT}"

if [[ "$PUBLISH" -eq 1 ]]; then
  if ! command -v gh >/dev/null; then
    echo "error: gh CLI not found. Install it first (brew install gh)."
    exit 1
  fi

  NOTES_FILE="$(mktemp)"
  cat >"$NOTES_FILE" <<EOF
# Decodium 3 FT2 ${VERSION} (macOS)

## English (UK)
This release includes fork updates up to \`${VERSION}\`.

If the app does not start on macOS, run from Terminal:
\`sudo xattr -r -d com.apple.quarantine /Applications/ft2.app\`

See \`CHANGELOG.md\` for full details.

Assets:
- \`${DMG_OUT}\`
- \`${ZIP_OUT}\`
- \`${SHA_OUT}\`

## Italiano
Questa release include aggiornamenti fork fino a \`${VERSION}\`.

Se l'app non si avvia su macOS, esegui da Terminale:
\`sudo xattr -r -d com.apple.quarantine /Applications/ft2.app\`

Per i dettagli completi, vedi \`CHANGELOG.md\`.

Asset:
- \`${DMG_OUT}\`
- \`${ZIP_OUT}\`
- \`${SHA_OUT}\`

## Espanol
Esta release incluye actualizaciones del fork hasta \`${VERSION}\`.

Si la app no inicia en macOS, ejecuta en Terminal:
\`sudo xattr -r -d com.apple.quarantine /Applications/ft2.app\`

Para todos los detalles, ver \`CHANGELOG.md\`.

Artefactos:
- \`${DMG_OUT}\`
- \`${ZIP_OUT}\`
- \`${SHA_OUT}\`
EOF

  echo "[publish] Publishing release to ${REPO}..."
  if gh release view "$VERSION" --repo "$REPO" >/dev/null 2>&1; then
    gh release upload "$VERSION" \
      "${BUILD_DIR}/${DMG_OUT}" \
      "${BUILD_DIR}/${ZIP_OUT}" \
      "${BUILD_DIR}/${SHA_OUT}" \
      --repo "$REPO" \
      --clobber
    gh release edit "$VERSION" --repo "$REPO" --notes-file "$NOTES_FILE"
  else
    gh release create "$VERSION" \
      "${BUILD_DIR}/${DMG_OUT}" \
      "${BUILD_DIR}/${ZIP_OUT}" \
      "${BUILD_DIR}/${SHA_OUT}" \
      --repo "$REPO" \
      --title "Decodium 3 FT2 ${VERSION} (macOS)" \
      --notes-file "$NOTES_FILE"
  fi

  rm -f "$NOTES_FILE"
  echo "Release published."
fi
