#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

VERSION="${VERSION:-}"
HAMLIB_VERSION="${HAMLIB_VERSION:-4.7.1}"
HAMLIB_TAG="${HAMLIB_TAG:-${HAMLIB_VERSION}}"
HAMLIB_PREFIX="${HAMLIB_PREFIX:-${ROOT_DIR}/.ci/cache/hamlib-linux-x86_64-${HAMLIB_VERSION}}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-linux-appimage-x64}"
APPDIR="${APPDIR:-${ROOT_DIR}/AppDir-linux-x86_64}"
OUTPUT_DIR="${OUTPUT_DIR:-${ROOT_DIR}/dist-linux-appimage}"
TOOLS_DIR="${TOOLS_DIR:-${ROOT_DIR}/.ci/tools}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"

if [[ -z "${VERSION}" ]]; then
  if [[ -f "${ROOT_DIR}/fork_release_version.txt" ]]; then
    VERSION="$(tr -d '\r\n' < "${ROOT_DIR}/fork_release_version.txt")"
  else
    VERSION="0.0.0-local"
  fi
fi
VERSION="${VERSION#v}"

log() {
  printf '\n>>> %s\n' "$*"
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "error: required command not found: $1" >&2
    exit 1
  }
}

require_cmd cmake
require_cmd make
require_cmd curl
require_cmd tar
require_cmd pkg-config
require_cmd sha256sum
require_cmd file

QMAKE="$(command -v qmake6 || command -v qmake || true)"
if [[ -z "${QMAKE}" ]]; then
  echo "error: qmake6/qmake not found in build environment" >&2
  exit 1
fi

log "Build context"
echo "Root:           ${ROOT_DIR}"
echo "Version:        ${VERSION}"
echo "Hamlib:         ${HAMLIB_VERSION}"
echo "Hamlib prefix:  ${HAMLIB_PREFIX}"
echo "Build dir:      ${BUILD_DIR}"
echo "Output dir:     ${OUTPUT_DIR}"
echo "Jobs:           ${JOBS}"
echo "QMake:          ${QMAKE} ($("${QMAKE}" -query QT_VERSION 2>/dev/null || true))"

export PATH="${HAMLIB_PREFIX}/bin:${PATH}"
export PKG_CONFIG_PATH="${HAMLIB_PREFIX}/lib/pkgconfig:${HAMLIB_PREFIX}/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="${HAMLIB_PREFIX}/lib:${HAMLIB_PREFIX}/lib64:${LD_LIBRARY_PATH:-}"
export CMAKE_PREFIX_PATH="${HAMLIB_PREFIX}${CMAKE_PREFIX_PATH:+;${CMAKE_PREFIX_PATH}}"

hamlib_ready=false
if [[ -x "${HAMLIB_PREFIX}/bin/rigctl" ]] \
   && [[ -f "${HAMLIB_PREFIX}/include/hamlib/rig.h" ]] \
   && pkg-config --exists hamlib; then
  cached_version="$(pkg-config --modversion hamlib || true)"
  if [[ "${cached_version}" == "${HAMLIB_VERSION}" ]]; then
    hamlib_ready=true
  fi
fi

if [[ "${hamlib_ready}" == true ]]; then
  log "Using cached Hamlib ${HAMLIB_VERSION}"
  "${HAMLIB_PREFIX}/bin/rigctl" --version || true
else
  log "Building Hamlib ${HAMLIB_VERSION} into cache"
  rm -rf "${HAMLIB_PREFIX}"
  mkdir -p "${HAMLIB_PREFIX}"
  tmpdir="$(mktemp -d)"
  trap 'rm -rf "${tmpdir}"' EXIT
  cd "${tmpdir}"
  curl -fsSL -o "hamlib-${HAMLIB_VERSION}.tar.gz" \
    "https://github.com/Hamlib/Hamlib/releases/download/${HAMLIB_TAG}/hamlib-${HAMLIB_VERSION}.tar.gz"
  tar -xzf "hamlib-${HAMLIB_VERSION}.tar.gz"
  cd "hamlib-${HAMLIB_VERSION}"
  ./configure --prefix="${HAMLIB_PREFIX}" --disable-static --enable-shared
  make -j"${JOBS}"
  make install
  "${HAMLIB_PREFIX}/bin/rigctl" --version || true
fi

hamlib_lib_dirs=()
for hamlib_lib_dir in "${HAMLIB_PREFIX}/lib" "${HAMLIB_PREFIX}/lib64"; do
  if [[ -d "${hamlib_lib_dir}" ]]; then
    hamlib_lib_dirs+=("${hamlib_lib_dir}")
  fi
done
if [[ "${#hamlib_lib_dirs[@]}" -eq 0 ]]; then
  echo "error: unable to locate Hamlib library directories under ${HAMLIB_PREFIX}" >&2
  exit 1
fi

HAMLIB_LIBRARY="$(find "${hamlib_lib_dirs[@]}" \
  -maxdepth 1 \( -name 'libhamlib.so' -o -name 'libhamlib.so.*' \) 2>/dev/null \
  | sort | head -n1)"
if [[ -z "${HAMLIB_LIBRARY}" ]]; then
  echo "error: unable to locate libhamlib under ${HAMLIB_PREFIX}" >&2
  exit 1
fi

log "Configure CMake"
rm -rf "${BUILD_DIR}" "${APPDIR}"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
  -DHamlib_INCLUDE_DIR="${HAMLIB_PREFIX}/include" \
  -DHamlib_LIBRARY="${HAMLIB_LIBRARY}" \
  -DRIGCTL_EXE="${HAMLIB_PREFIX}/bin/rigctl" \
  -DRIGCTLD_EXE="${HAMLIB_PREFIX}/bin/rigctld" \
  -DRIGCTLCOM_EXE="${HAMLIB_PREFIX}/bin/rigctlcom" \
  -DFORK_RELEASE_VERSION="${VERSION}" \
  -DQT_MAJOR_VERSION=6 \
  -DWSJT_GENERATE_DOCS=OFF \
  -DWSJT_SKIP_MANPAGES=ON \
  -DWSJT_BUILD_UTILS=OFF

log "Verify AppImage GPU build path"
RHI_FLAG="$(cat "${BUILD_DIR}/decodium_qt_rhi_texture_upload.enabled" 2>/dev/null || echo 0)"
QSB_FLAG="$(cat "${BUILD_DIR}/decodium_qsb_shaders.enabled" 2>/dev/null || echo 0)"
echo "DECODIUM_QT_RHI_TEXTURE_UPLOAD=${RHI_FLAG}"
echo "DECODIUM_QSB_SHADERS=${QSB_FLAG}"
if [[ "${RHI_FLAG}" != "1" ]]; then
  echo "error: AppImage build did not enable DECODIUM_QT_RHI_TEXTURE_UPLOAD=1; install Qt6 GuiPrivate/private headers or use a non-release fallback build" >&2
  exit 1
fi
if [[ "${QSB_FLAG}" != "1" ]]; then
  echo "error: AppImage build did not enable QSB shaders; install qt6-shadertools-dev" >&2
  exit 1
fi

log "Build"
cmake --build "${BUILD_DIR}" --target wsjtx decodium_app wsprd message_aggregator --parallel "${JOBS}"

log "Assemble AppDir"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
cmake --install "${BUILD_DIR}" --prefix "${APPDIR}/usr"
cp "${ROOT_DIR}/icons/Unix/decodium_icon.png" \
  "${APPDIR}/usr/share/icons/hicolor/256x256/apps/decodium.png"
find "${APPDIR}" -name '._*' -o -name '.DS_Store' | xargs -r rm -f

cat > "${APPDIR}/usr/share/applications/decodium.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Decodium 4
Comment=Amateur Radio Weak Signal Operating
Exec=decodium
Icon=decodium
Terminal=false
Categories=AudioVideo;Audio;HamRadio;
StartupNotify=true
DESKTOP

log "Prepare linuxdeploy"
mkdir -p "${TOOLS_DIR}"
LINUXDEPLOY="${TOOLS_DIR}/linuxdeploy-x86_64.AppImage"
QT_PLUGIN="${TOOLS_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"
if [[ ! -x "${LINUXDEPLOY}" ]]; then
  curl -fsSL -o "${LINUXDEPLOY}" \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
  chmod +x "${LINUXDEPLOY}"
fi
if [[ ! -x "${QT_PLUGIN}" ]]; then
  curl -fsSL -o "${QT_PLUGIN}" \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
  chmod +x "${QT_PLUGIN}"
fi
ln -sf "${QT_PLUGIN}" "${TOOLS_DIR}/linuxdeploy-plugin-qt"

export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="${QMAKE}"
export QML_SOURCES_PATHS="${ROOT_DIR}/qml"
export ARCH=x86_64
export PATH="${TOOLS_DIR}:${PATH}"

linuxdeploy_args=(
  --appdir "${APPDIR}"
  --desktop-file "${APPDIR}/usr/share/applications/decodium.desktop"
  --icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/decodium.png"
  --executable "${APPDIR}/usr/bin/decodium"
  --executable "${APPDIR}/usr/bin/wsprd"
  --executable "${APPDIR}/usr/bin/message_aggregator"
  --library "${HAMLIB_LIBRARY}"
)
for helper in rigctl-wsjtx rigctld-wsjtx rigctlcom-wsjtx; do
  if [[ -x "${APPDIR}/usr/bin/${helper}" ]]; then
    linuxdeploy_args+=(--executable "${APPDIR}/usr/bin/${helper}")
  fi
done
linuxdeploy_args+=(--plugin qt)

"${LINUXDEPLOY}" "${linuxdeploy_args[@]}"

log "Bundle QML modules"
mkdir -p "${APPDIR}/usr/bin/qml"
QT_QML_DIR="$("${QMAKE}" -query QT_INSTALL_QML 2>/dev/null || true)"
if [[ -n "${QT_QML_DIR}" && -d "${QT_QML_DIR}" ]]; then
  cp -a "${QT_QML_DIR}/." "${APPDIR}/usr/bin/qml/"
fi
cp -a "${BUILD_DIR}/qml/." "${APPDIR}/usr/bin/qml/"

for qml_module in \
  QtQuick/qmldir \
  QtQuick/Controls/qmldir \
  QtQuick/Controls/Material/qmldir \
  QtQuick/Dialogs/qmldir \
  QtQuick/Layouts/qmldir \
  QtQuick/Templates/qmldir \
  QtQuick/Window/qmldir \
  QtCore/qmldir \
  QtQml/qmldir \
  QtQml/Models/qmldir \
  QtQml/WorkerScript/qmldir; do
  test -f "${APPDIR}/usr/bin/qml/${qml_module}"
done

find "${APPDIR}" -name '._*' -o -name '.DS_Store' | xargs -r rm -f

log "Create AppImage"
(
  cd "${ROOT_DIR}"
  rm -f ./*.AppImage
  "${LINUXDEPLOY}" --appdir "${APPDIR}" --output appimage
)

APPIMAGE_NAME="decodium4-ft2-${VERSION}-linux-x86_64.AppImage"
APPIMAGE_SRC="$(find "${ROOT_DIR}" -maxdepth 1 -name '*.AppImage' ! -name 'linuxdeploy*.AppImage' | head -n1)"
if [[ -z "${APPIMAGE_SRC}" ]]; then
  echo "error: linuxdeploy did not create an AppImage" >&2
  exit 1
fi

mkdir -p "${OUTPUT_DIR}"
mv "${APPIMAGE_SRC}" "${OUTPUT_DIR}/${APPIMAGE_NAME}"
sha256sum "${OUTPUT_DIR}/${APPIMAGE_NAME}" > "${OUTPUT_DIR}/${APPIMAGE_NAME}.sha256.txt"

log "Done"
ls -lh "${OUTPUT_DIR}/${APPIMAGE_NAME}" "${OUTPUT_DIR}/${APPIMAGE_NAME}.sha256.txt"
file "${OUTPUT_DIR}/${APPIMAGE_NAME}"
