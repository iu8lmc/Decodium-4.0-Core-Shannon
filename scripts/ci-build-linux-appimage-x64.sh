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
APPIMAGE_ARCH="${APPIMAGE_ARCH:-x86_64}"
APPIMAGE_OUTPUT_ARCH="${APPIMAGE_OUTPUT_ARCH:-${APPIMAGE_ARCH}}"
LINUXDEPLOY_ARCH="${LINUXDEPLOY_ARCH:-${APPIMAGE_ARCH}}"
RTLSDR_VERSION="${RTLSDR_VERSION:-2.0.2}"
RTLSDR_PREFIX="${RTLSDR_PREFIX:-${ROOT_DIR}/.ci/cache/librtlsdr-linux-${APPIMAGE_ARCH}-${RTLSDR_VERSION}}"
LINUXDEPLOY_URL="${LINUXDEPLOY_URL:-https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage}"
LINUXDEPLOY_QT_PLUGIN_URL="${LINUXDEPLOY_QT_PLUGIN_URL:-https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage}"
APPIMAGETOOL_URL="${APPIMAGETOOL_URL:-https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${APPIMAGE_ARCH}.AppImage}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"

VERSION="$("${ROOT_DIR}/scripts/ci/resolve-release-version.sh" "${VERSION}")"
"${ROOT_DIR}/scripts/ci/validate-repository-layout.sh"

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
require_cmd ldd

QMAKE="$(command -v qmake6 || command -v qmake || true)"
if [[ -z "${QMAKE}" ]]; then
  echo "error: qmake6/qmake not found in build environment" >&2
  exit 1
fi

QT_PLUGIN_DIR_FOR_BUILD="$("${QMAKE}" -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
QT_LIB_DIR_FOR_BUILD="$("${QMAKE}" -query QT_INSTALL_LIBS 2>/dev/null || true)"
QT_PREFIX_FOR_BUILD="$("${QMAKE}" -query QT_INSTALL_PREFIX 2>/dev/null || true)"

qt_plugin_tree_writable() {
  [[ -n "${QT_PLUGIN_DIR_FOR_BUILD}" && -d "${QT_PLUGIN_DIR_FOR_BUILD}" && -w "${QT_PLUGIN_DIR_FOR_BUILD}" ]]
}

qt_plugin_subdir_writable() {
  local plugin_subdir="$1"
  qt_plugin_tree_writable && [[ -d "${QT_PLUGIN_DIR_FOR_BUILD}/${plugin_subdir}" && -w "${QT_PLUGIN_DIR_FOR_BUILD}/${plugin_subdir}" ]]
}

restore_disabled_qt_plugin() {
  local plugin_subdir="$1"
  local plugin_name="$2"
  local disabled_subdir="${plugin_subdir}-disabled"

  if [[ -z "${QT_PLUGIN_DIR_FOR_BUILD}" ]]; then
    return
  fi
  if ! qt_plugin_tree_writable; then
    return
  fi
  if [[ -f "${QT_PLUGIN_DIR_FOR_BUILD}/${plugin_subdir}/${plugin_name}" ]]; then
    return
  fi
  if [[ -f "${QT_PLUGIN_DIR_FOR_BUILD}/${disabled_subdir}/${plugin_name}" ]]; then
    mkdir -p "${QT_PLUGIN_DIR_FOR_BUILD}/${plugin_subdir}"
    cp -a "${QT_PLUGIN_DIR_FOR_BUILD}/${disabled_subdir}/${plugin_name}" \
      "${QT_PLUGIN_DIR_FOR_BUILD}/${plugin_subdir}/${plugin_name}"
  fi
}

restore_disabled_qt_plugin imageformats libqtiff.so
if [[ -n "${QT_PLUGIN_DIR_FOR_BUILD}" && -d "${QT_PLUGIN_DIR_FOR_BUILD}/sqldrivers-disabled" ]]; then
  if qt_plugin_tree_writable; then
    mkdir -p "${QT_PLUGIN_DIR_FOR_BUILD}/sqldrivers"
    cp -a "${QT_PLUGIN_DIR_FOR_BUILD}/sqldrivers-disabled"/libqsql*.so \
      "${QT_PLUGIN_DIR_FOR_BUILD}/sqldrivers/" 2>/dev/null || true
  else
    echo "Skipping Qt SQL driver restore: ${QT_PLUGIN_DIR_FOR_BUILD} is not writable"
  fi
fi

ensure_linuxdeploy_qt_plugin_dirs() {
  local plugin_subdir

  if [[ -z "${QT_PLUGIN_DIR_FOR_BUILD}" ]]; then
    echo "Skipping Qt plugin directory normalization: qmake did not report QT_INSTALL_PLUGINS"
    return
  fi
  if [[ ! -d "${QT_PLUGIN_DIR_FOR_BUILD}" || ! -w "${QT_PLUGIN_DIR_FOR_BUILD}" ]]; then
    echo "Skipping Qt plugin directory normalization: ${QT_PLUGIN_DIR_FOR_BUILD} is not writable"
    return
  fi

  # linuxdeploy-plugin-qt iterates optional plugin category directories
  # unconditionally for linked Qt modules. Source-built/minimal Qt prefixes can
  # omit empty categories such as printsupport, which aborts the deploy.
  for plugin_subdir in \
    printsupport \
    platformthemes \
    styles \
    sqldrivers \
    tls \
    imageformats \
    iconengines \
    platforminputcontexts \
    xcbglintegrations \
    platforms; do
    mkdir -p "${QT_PLUGIN_DIR_FOR_BUILD}/${plugin_subdir}"
  done
}

log "Build context"
echo "Root:           ${ROOT_DIR}"
echo "Version:        ${VERSION}"
echo "Hamlib:         ${HAMLIB_VERSION}"
echo "Hamlib prefix:  ${HAMLIB_PREFIX}"
echo "RTL-SDR:        ${RTLSDR_VERSION}"
echo "RTL-SDR prefix: ${RTLSDR_PREFIX}"
echo "Build dir:      ${BUILD_DIR}"
echo "Output dir:     ${OUTPUT_DIR}"
echo "Jobs:           ${JOBS}"
echo "AppImage arch:  ${APPIMAGE_ARCH}"
echo "QMake:          ${QMAKE} ($("${QMAKE}" -query QT_VERSION 2>/dev/null || true))"
echo "Qt prefix:      ${QT_PREFIX_FOR_BUILD}"
echo "Qt libs:        ${QT_LIB_DIR_FOR_BUILD}"

export PATH="${HAMLIB_PREFIX}/bin:${PATH}"
export PKG_CONFIG_PATH="${HAMLIB_PREFIX}/lib/pkgconfig:${HAMLIB_PREFIX}/lib64/pkgconfig:${PKG_CONFIG_PATH:-}"
if [[ -n "${QT_LIB_DIR_FOR_BUILD}" && -d "${QT_LIB_DIR_FOR_BUILD}" ]]; then
  export LD_LIBRARY_PATH="${QT_LIB_DIR_FOR_BUILD}:${HAMLIB_PREFIX}/lib:${HAMLIB_PREFIX}/lib64:${RTLSDR_PREFIX}/lib:${LD_LIBRARY_PATH:-}"
else
  export LD_LIBRARY_PATH="${HAMLIB_PREFIX}/lib:${HAMLIB_PREFIX}/lib64:${RTLSDR_PREFIX}/lib:${LD_LIBRARY_PATH:-}"
fi
export CMAKE_PREFIX_PATH="${RTLSDR_PREFIX};${HAMLIB_PREFIX}${CMAKE_PREFIX_PATH:+;${CMAKE_PREFIX_PATH}}"

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

log "Build pinned RTL-SDR Blog driver"
RTLSDR_VERSION="${RTLSDR_VERSION}" RTLSDR_PREFIX="${RTLSDR_PREFIX}" \
  "${ROOT_DIR}/scripts/ci/build-librtlsdr.sh"
RTLSDR_LIBRARY="$(find "${RTLSDR_PREFIX}/lib" -maxdepth 1 -type f -name 'librtlsdr.so*' | sort | head -n1)"
if [[ -z "${RTLSDR_LIBRARY}" || ! -f "${RTLSDR_PREFIX}/include/rtl-sdr.h" ]]; then
  echo "error: unable to locate the installed librtlsdr runtime" >&2
  exit 1
fi

log "Configure CMake"
if [[ "${INCREMENTAL:-0}" == "1" && -d "${BUILD_DIR}" ]]; then
  log "Incremental build enabled: preserving ${BUILD_DIR}"
  rm -rf "${APPDIR}"
else
  rm -rf "${BUILD_DIR}" "${APPDIR}"
fi
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
  -DHamlib_INCLUDE_DIR="${HAMLIB_PREFIX}/include" \
  -DHamlib_LIBRARY="${HAMLIB_LIBRARY}" \
  -DRtlSdr_INCLUDE_DIR="${RTLSDR_PREFIX}/include" \
  -DRtlSdr_LIBRARY="${RTLSDR_LIBRARY}" \
  -DDECODIUM_REQUIRE_RTLSDR=ON \
  -DDECODIUM_ENABLE_SSTV=ON \
  -DDECODIUM_ENABLE_HAMDRM=ON \
  -DRIGCTL_EXE="${HAMLIB_PREFIX}/bin/rigctl" \
  -DRIGCTLD_EXE="${HAMLIB_PREFIX}/bin/rigctld" \
  -DRIGCTLCOM_EXE="${HAMLIB_PREFIX}/bin/rigctlcom" \
  -DFORK_RELEASE_VERSION="${VERSION}" \
  -DQT_MAJOR_VERSION=6 \
  -DWSJT_GENERATE_DOCS=OFF \
  -DWSJT_SKIP_MANPAGES=ON \
  -DWSJT_BUILD_UTILS=OFF

for release_feature in DECODIUM_ENABLE_SSTV DECODIUM_ENABLE_HAMDRM; do
  if ! grep -Fxq "${release_feature}:BOOL=ON" "${BUILD_DIR}/CMakeCache.txt"; then
    echo "error: advertised AppImage release must enable ${release_feature}" >&2
    exit 1
  fi
done

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
cmake --build "${BUILD_DIR}" --target wsjtx decodium_app wsprd udp_daemon message_aggregator wsjtx_app_version --parallel "${JOBS}"

log "Assemble AppDir"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
cmake --install "${BUILD_DIR}" --prefix "${APPDIR}/usr"
if ! find "${APPDIR}/usr/share/doc" -type f \
    -name 'THIRD_PARTY_LICENSES_OPENJPEG.md' -print -quit | grep -q .; then
  echo "error: OpenJPEG third-party notice is missing from AppDir" >&2
  exit 1
fi
mkdir -p "${APPDIR}/usr/lib"
# Keep the SONAME aliases as real files inside AppDir: copying a symlink alone
# would leave it pointing outside the AppImage after packaging.
cp -aL "${RTLSDR_PREFIX}"/lib/librtlsdr.so* "${APPDIR}/usr/lib/"
find "${APPDIR}/usr/lib" -maxdepth 1 -name 'librtlsdr.so*' -print -quit | grep -q .
cp "${ROOT_DIR}/icons/Unix/decodium_icon.png" \
  "${APPDIR}/usr/share/icons/hicolor/256x256/apps/decodium.png"
find "${APPDIR}" -name '._*' -o -name '.DS_Store' | xargs -r rm -f
find "${APPDIR}/usr/share/applications" -name '*.desktop' -type f -exec sed -i 's/\r$//' {} +

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
# TIFF is an accepted SSTV Studio/inbox format.  Keep its Qt image plugin in
# the tree inspected by linuxdeploy so the plugin and its native dependencies
# are copied into the AppImage.  Older packaging temporarily stashed it and
# never restored it before image creation, which made the final payload check
# fail deterministically.
if [[ -z "${QT_PLUGIN_DIR_FOR_BUILD}" \
      || ! -f "${QT_PLUGIN_DIR_FOR_BUILD}/imageformats/libqtiff.so" ]]; then
  echo "error: required Qt TIFF image plugin is unavailable before linuxdeploy" >&2
  exit 1
fi
if [[ ! -f "${QT_PLUGIN_DIR_FOR_BUILD}/sqldrivers/libqsqlite.so" ]]; then
  echo "error: required Qt SQLite driver is unavailable before linuxdeploy" >&2
  exit 1
fi
ensure_linuxdeploy_qt_plugin_dirs
if [[ -n "${QT_PLUGIN_DIR_FOR_BUILD}" && -d "${QT_PLUGIN_DIR_FOR_BUILD}/sqldrivers" ]]; then
  if qt_plugin_subdir_writable sqldrivers; then
    mkdir -p "${QT_PLUGIN_DIR_FOR_BUILD}/sqldrivers-disabled"
    find "${QT_PLUGIN_DIR_FOR_BUILD}/sqldrivers" -maxdepth 1 -type f -name 'libqsql*.so' ! -name 'libqsqlite.so' \
      -exec mv -f {} "${QT_PLUGIN_DIR_FOR_BUILD}/sqldrivers-disabled/" \;
  else
    echo "Skipping temporary Qt SQL driver stash: ${QT_PLUGIN_DIR_FOR_BUILD}/sqldrivers is not writable"
  fi
fi
LINUXDEPLOY="${TOOLS_DIR}/linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage"
QT_PLUGIN="${TOOLS_DIR}/linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage"
APPIMAGETOOL="${TOOLS_DIR}/appimagetool-${APPIMAGE_ARCH}.AppImage"
if [[ ! -x "${LINUXDEPLOY}" ]]; then
  curl -fsSL -o "${LINUXDEPLOY}" "${LINUXDEPLOY_URL}"
  chmod +x "${LINUXDEPLOY}"
fi
if [[ ! -x "${QT_PLUGIN}" ]]; then
  curl -fsSL -o "${QT_PLUGIN}" "${LINUXDEPLOY_QT_PLUGIN_URL}"
  chmod +x "${QT_PLUGIN}"
fi
if [[ "${APPIMAGE_ARCH}" == "aarch64" && ! -x "${APPIMAGETOOL}" ]]; then
  curl -fsSL -o "${APPIMAGETOOL}" "${APPIMAGETOOL_URL}"
  chmod +x "${APPIMAGETOOL}"
fi

resolve_appimage_runner() {
  local appimage="$1"
  local name="$2"
  local extract_dir="${TOOLS_DIR}/${name}-extracted"
  local squashfs="${TOOLS_DIR}/${name}.squashfs"
  local found=0
  local offset

  if [[ -x "${extract_dir}/AppRun" ]]; then
    echo "Using cached extracted ${name} AppImage payload" >&2
    printf '%s\n' "${extract_dir}/AppRun"
    return 0
  fi

  if "${appimage}" --appimage-version >/dev/null 2>&1; then
    printf '%s\n' "${appimage}"
    return 0
  fi

  if ! command -v unsquashfs >/dev/null 2>&1; then
    echo "error: ${name} AppImage cannot run directly and unsquashfs is not installed" >&2
    exit 1
  fi

  rm -rf "${extract_dir}" "${squashfs}"
  while IFS=: read -r offset _; do
    rm -rf "${extract_dir}" "${squashfs}"
    tail -c +$((offset + 1)) "${appimage}" > "${squashfs}"
    if unsquashfs -q -d "${extract_dir}" "${squashfs}" >/dev/null 2>&1; then
      found=1
      break
    fi
  done < <(grep -aob "hsqs" "${appimage}" || true)
  rm -f "${squashfs}"

  if [[ "${found}" != "1" || ! -x "${extract_dir}/AppRun" ]]; then
    echo "error: failed to extract ${name} AppImage payload" >&2
    exit 1
  fi

  echo "Using extracted ${name} AppImage payload" >&2
  printf '%s\n' "${extract_dir}/AppRun"
}

LINUXDEPLOY_RUNNER="$(resolve_appimage_runner "${LINUXDEPLOY}" linuxdeploy)"
QT_PLUGIN_RUNNER="$(resolve_appimage_runner "${QT_PLUGIN}" linuxdeploy-plugin-qt)"
APPIMAGETOOL_RUNNER=""
if [[ "${APPIMAGE_ARCH}" == "aarch64" ]]; then
  APPIMAGETOOL_RUNNER="$(resolve_appimage_runner "${APPIMAGETOOL}" appimagetool)"
fi
if [[ "${QT_PLUGIN_RUNNER}" == */linuxdeploy-plugin-qt-extracted/AppRun ]]; then
  mkdir -p "${TOOLS_DIR}/disabled-appimages"
  if [[ -f "${QT_PLUGIN}" && ! -f "${TOOLS_DIR}/disabled-appimages/linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage.real" ]]; then
    cp -a "${QT_PLUGIN}" "${TOOLS_DIR}/disabled-appimages/linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage.real"
  fi
  for qt_plugin_launcher in "${TOOLS_DIR}/linuxdeploy-plugin-qt" "${QT_PLUGIN}"; do
    {
      printf '#!/usr/bin/env bash\n'
      printf 'exec %q "$@"\n' "${QT_PLUGIN_RUNNER}"
    } > "${qt_plugin_launcher}"
    chmod +x "${qt_plugin_launcher}"
  done
else
  rm -f "${TOOLS_DIR}/linuxdeploy-plugin-qt"
  {
    printf '#!/usr/bin/env bash\n'
    printf 'exec %q "$@"\n' "${QT_PLUGIN_RUNNER}"
  } > "${TOOLS_DIR}/linuxdeploy-plugin-qt"
  chmod +x "${TOOLS_DIR}/linuxdeploy-plugin-qt"
fi

export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="${QMAKE}"
export QML_SOURCES_PATHS="${ROOT_DIR}/qml"
export ARCH="${APPIMAGE_ARCH}"
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

"${LINUXDEPLOY_RUNNER}" "${linuxdeploy_args[@]}"

log "Bundle QML modules"
APP_QML_DIR="${APPDIR}/usr/bin/qml"
QT_RUNTIME_QML_DIR="${APPDIR}/usr/qml"
mkdir -p "${APP_QML_DIR}"
QT_QML_DIR="$("${QMAKE}" -query QT_INSTALL_QML 2>/dev/null || true)"
if [[ -n "${QT_QML_DIR}" && -d "${QT_QML_DIR}" ]]; then
  # Dereference Qt QML symlinks. Some aqt/Qt layouts expose QML plugin .so
  # files as links into the Qt prefix; preserving those links makes the
  # AppImage pass build-time file tests but fail on user machines.
  cp -aL "${QT_QML_DIR}/." "${APP_QML_DIR}/"
fi
cp -a "${BUILD_DIR}/qml/." "${APP_QML_DIR}/"

prune_unused_qml_modules() {
  local module_path
  local module_name

  # aqt's Qt 6.11 package can include optional QML modules whose companion
  # libraries are not installed unless their Qt addon is requested. Decodium
  # does not import those modules; pruning them keeps dependency validation
  # focused on the runtime QML surface we actually ship.
  while IFS= read -r -d '' module_path; do
    module_name="$(basename "${module_path}")"
    case "${module_name}" in
      QtQuick|QtQml|QtCore|Qt|decodium|dialogs|panels|QML)
        ;;
      *)
        rm -rf "${module_path}"
        ;;
    esac
  done < <(find "${APP_QML_DIR}" -mindepth 1 -maxdepth 1 -type d -print0)

  if [[ -d "${APP_QML_DIR}/Qt" ]]; then
    while IFS= read -r -d '' module_path; do
      module_name="$(basename "${module_path}")"
      case "${module_name}" in
        labs)
          ;;
        *)
          rm -rf "${module_path}"
          ;;
      esac
    done < <(find "${APP_QML_DIR}/Qt" -mindepth 1 -maxdepth 1 -type d -print0)
  fi

  if [[ -d "${APP_QML_DIR}/Qt/labs" ]]; then
    while IFS= read -r -d '' module_path; do
      module_name="$(basename "${module_path}")"
      case "${module_name}" in
        folderlistmodel)
          ;;
        *)
          rm -rf "${module_path}"
          ;;
      esac
    done < <(find "${APP_QML_DIR}/Qt/labs" -mindepth 1 -maxdepth 1 -type d -print0)
  fi
}

prune_unused_qml_modules

# linuxdeploy-plugin-qt writes qt.conf so QLibraryInfo often resolves QML imports
# from usr/qml, while Decodium loads its own QML from usr/bin/qml. Keep both paths
# pointed at the same complete tree so a partial linuxdeploy QtQuick.Controls copy
# cannot shadow the bundled Material style.
rm -rf "${QT_RUNTIME_QML_DIR}"
ln -s bin/qml "${QT_RUNTIME_QML_DIR}"

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
  test -f "${APP_QML_DIR}/${qml_module}"
  test -f "${QT_RUNTIME_QML_DIR}/${qml_module}"
done
test -f "${APP_QML_DIR}/QtQuick/Controls/Material/libqtquickcontrols2materialstyleplugin.so"
test -f "${APP_QML_DIR}/QtQuick/Controls/Material/impl/libqtquickcontrols2materialstyleimplplugin.so"
test -f "${QT_RUNTIME_QML_DIR}/QtQuick/Controls/Material/libqtquickcontrols2materialstyleplugin.so"
test -f "${QT_RUNTIME_QML_DIR}/QtQuick/Controls/Material/impl/libqtquickcontrols2materialstyleimplplugin.so"

bundle_qml_plugin_dependencies() {
  local plugin
  local dep
  local app_lib_dir="${APPDIR}/usr/lib"
  mkdir -p "${app_lib_dir}"

  while IFS= read -r -d '' plugin; do
    while IFS= read -r dep; do
      [[ -n "${dep}" && -f "${dep}" ]] || continue
      if [[ -n "${QT_PREFIX_FOR_BUILD}" && "${dep}" == "${QT_PREFIX_FOR_BUILD}/"* ]] \
        || [[ -n "${QT_LIB_DIR_FOR_BUILD}" && "${dep}" == "${QT_LIB_DIR_FOR_BUILD}/"* ]]; then
        cp -aL "${dep}" "${app_lib_dir}/"
      fi
    done < <(ldd "${plugin}" 2>/dev/null | awk '/=> \// {print $3} /^\// {print $1}')
  done < <(find "${APP_QML_DIR}" -type f -name '*.so' -print0)
}

verify_qml_plugin_dependencies() {
  local context="$1"
  local qml_root="$2"
  local lib_root="$3"
  local bin_root
  local ld_path
  local failed=0
  local plugin
  bin_root="$(cd "${lib_root}/../bin" 2>/dev/null && pwd || true)"
  ld_path="${lib_root}"
  if [[ -n "${bin_root}" ]]; then
    ld_path="${ld_path}:${bin_root}"
  fi

  while IFS= read -r -d '' plugin; do
    if ! LD_LIBRARY_PATH="${ld_path}" \
        ldd "${plugin}" >/tmp/decodium-qml-ldd.txt 2>&1; then
      cat /tmp/decodium-qml-ldd.txt >&2 || true
      failed=1
      continue
    fi
    if grep -q "not found" /tmp/decodium-qml-ldd.txt; then
      echo "error: unresolved QML plugin dependencies in ${context}: ${plugin}" >&2
      cat /tmp/decodium-qml-ldd.txt >&2
      failed=1
    fi
  done < <(find "${qml_root}" -type f -name '*.so' -print0)
  rm -f /tmp/decodium-qml-ldd.txt
  if [[ "${failed}" != "0" ]]; then
    exit 1
  fi
}

bundle_qml_plugin_dependencies
verify_qml_plugin_dependencies "AppDir" "${APP_QML_DIR}" "${APPDIR}/usr/lib"

if find "${APP_QML_DIR}" -xtype l -print -quit | grep -q .; then
  echo "error: bundled QML tree contains broken symlinks" >&2
  find "${APP_QML_DIR}" -xtype l -print >&2
  exit 1
fi

log "Bundle supplemental Qt plugins"
QT_PLUGIN_DIR="$("${QMAKE}" -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
copy_qt_plugin_dir() {
  local plugin_dir="$1"
  if [[ -n "${QT_PLUGIN_DIR}" && -d "${QT_PLUGIN_DIR}/${plugin_dir}" ]]; then
    mkdir -p "${APPDIR}/usr/plugins/${plugin_dir}"
    cp -aL "${QT_PLUGIN_DIR}/${plugin_dir}/." "${APPDIR}/usr/plugins/${plugin_dir}/"
    echo "Bundled Qt plugin dir: ${plugin_dir}"
  fi
}

copy_qt_plugin_dir audio
copy_qt_plugin_dir multimedia
copy_qt_plugin_dir wayland-decoration-client
copy_qt_plugin_dir wayland-graphics-integration-client
copy_qt_plugin_dir wayland-shell-integration

if [[ -n "${QT_PLUGIN_DIR}" && -d "${QT_PLUGIN_DIR}/platforms" ]] \
   && compgen -G "${QT_PLUGIN_DIR}/platforms/libqwayland*.so" >/dev/null; then
  mkdir -p "${APPDIR}/usr/plugins/platforms"
  cp -aL "${QT_PLUGIN_DIR}"/platforms/libqwayland*.so "${APPDIR}/usr/plugins/platforms/"
  echo "Bundled Qt Wayland platform plugin"
fi

log "Patch AppImage launcher"
if [[ -L "${APPDIR}/AppRun" || -f "${APPDIR}/AppRun" ]]; then
  rm -f "${APPDIR}/AppRun.decodium-real"
  if [[ -L "${APPDIR}/AppRun" ]]; then
    app_run_target="$(readlink "${APPDIR}/AppRun")"
    rm -f "${APPDIR}/AppRun"
    ln -s "${app_run_target}" "${APPDIR}/AppRun.decodium-real"
  else
    mv "${APPDIR}/AppRun" "${APPDIR}/AppRun.decodium-real"
  fi
  cat > "${APPDIR}/AppRun" <<'APPRUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"

# Preserve the host values before linuxdeploy's AppRun and this wrapper add
# bundle paths.  Decodium restores these snapshots only for host helpers such
# as xdg-open and secret-tool; the application keeps using bundled libraries.
export DECODIUM_HOST_LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}"
export DECODIUM_HOST_LD_PRELOAD="${LD_PRELOAD:-}"
export DECODIUM_HOST_GIO_EXTRA_MODULES="${GIO_EXTRA_MODULES:-}"
export DECODIUM_HOST_GI_TYPELIB_PATH="${GI_TYPELIB_PATH:-}"
export DECODIUM_HOST_GSETTINGS_SCHEMA_DIR="${GSETTINGS_SCHEMA_DIR:-}"
export DECODIUM_HOST_GTK_PATH="${GTK_PATH:-}"
export DECODIUM_HOST_XDG_DATA_DIRS="${XDG_DATA_DIRS:-}"
export DECODIUM_HOST_QT_PLUGIN_PATH="${QT_PLUGIN_PATH:-}"
export DECODIUM_HOST_QT_QPA_PLATFORM_PLUGIN_PATH="${QT_QPA_PLATFORM_PLUGIN_PATH:-}"
export DECODIUM_HOST_QML_IMPORT_PATH="${QML_IMPORT_PATH:-}"
export DECODIUM_HOST_QML2_IMPORT_PATH="${QML2_IMPORT_PATH:-}"
export DECODIUM_HOST_QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-}"
export DECODIUM_HOST_QT_QUICK_CONTROLS_STYLE="${QT_QUICK_CONTROLS_STYLE:-}"
export DECODIUM_HOST_QT_MEDIA_BACKEND="${QT_MEDIA_BACKEND:-}"

# Keep the original desktop session visible to xdg-open, gio and keyring
# helpers even if linuxdeploy's AppRun adjusts the application environment.
export DECODIUM_HOST_DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-}"
export DECODIUM_HOST_XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-}"
export DECODIUM_HOST_PATH="${PATH:-}"
export DECODIUM_HOST_DISPLAY="${DISPLAY:-}"
export DECODIUM_HOST_XAUTHORITY="${XAUTHORITY:-}"
export DECODIUM_HOST_WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-}"
export DECODIUM_HOST_XDG_CURRENT_DESKTOP="${XDG_CURRENT_DESKTOP:-}"
export DECODIUM_HOST_XDG_SESSION_DESKTOP="${XDG_SESSION_DESKTOP:-}"
export DECODIUM_HOST_XDG_SESSION_TYPE="${XDG_SESSION_TYPE:-}"
export DECODIUM_HOST_DESKTOP_SESSION="${DESKTOP_SESSION:-}"
export DECODIUM_HOST_KDE_FULL_SESSION="${KDE_FULL_SESSION:-}"
export DECODIUM_HOST_KDE_SESSION_VERSION="${KDE_SESSION_VERSION:-}"
export DECODIUM_HOST_GNOME_DESKTOP_SESSION_ID="${GNOME_DESKTOP_SESSION_ID:-}"
export DECODIUM_HOST_BROWSER="${BROWSER:-}"
export DECODIUM_HOST_XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-}"
export DECODIUM_HOST_XDG_DATA_HOME="${XDG_DATA_HOME:-}"

# The Qt 6.11 Wayland platform plugin can be present but unusable on some
# Ubuntu/AppImage combinations because part of the compositor stack is supplied
# by the host. Prefer XCB unless the user explicitly overrides it.
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
export QT_MEDIA_BACKEND="${QT_MEDIA_BACKEND:-ffmpeg}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/bin:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins:${QT_PLUGIN_PATH:-}"
export QML_IMPORT_PATH="${HERE}/usr/bin/qml:${HERE}/usr/qml:${QML_IMPORT_PATH:-}"
export QML2_IMPORT_PATH="${HERE}/usr/bin/qml:${HERE}/usr/qml:${QML2_IMPORT_PATH:-}"
export QT_QUICK_CONTROLS_STYLE="${QT_QUICK_CONTROLS_STYLE:-Material}"

exec "${HERE}/AppRun.decodium-real" "$@"
APPRUN
  chmod +x "${APPDIR}/AppRun"
  echo "Wrapped AppRun with Linux AppImage runtime defaults"
fi

find "${APPDIR}" -name '._*' -o -name '.DS_Store' | xargs -r rm -f

log "Create AppImage"
(
  cd "${ROOT_DIR}"
  rm -f ./*.AppImage
  if [[ "${APPIMAGE_ARCH}" == "aarch64" ]]; then
    # linuxdeploy's aarch64 output plug-in currently replaces a custom AppRun
    # while packaging.  The AppDir is already fully deployed above, so invoke
    # appimagetool directly and preserve Decodium's host-environment wrapper.
    ARCH="${APPIMAGE_ARCH}" \
      VERSION="${VERSION}" \
      LINUXDEPLOY_OUTPUT_VERSION="${VERSION}" \
      "${APPIMAGETOOL_RUNNER}" \
        "${APPDIR}" \
        "${ROOT_DIR}/Decodium_4-${VERSION}-${APPIMAGE_ARCH}.AppImage"
  else
    "${LINUXDEPLOY_RUNNER}" \
      --appdir "${APPDIR}" \
      --desktop-file "${APPDIR}/usr/share/applications/decodium.desktop" \
      --icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/decodium.png" \
      --output appimage
  fi
)

APPIMAGE_NAME="decodium4-ft2-${VERSION}-linux-${APPIMAGE_OUTPUT_ARCH}.AppImage"
APPIMAGE_SRC="$(find "${ROOT_DIR}" -maxdepth 1 -name '*.AppImage' ! -name 'linuxdeploy*.AppImage' | head -n1)"
if [[ -z "${APPIMAGE_SRC}" ]]; then
  echo "error: linuxdeploy did not create an AppImage" >&2
  exit 1
fi

mkdir -p "${OUTPUT_DIR}"
mv "${APPIMAGE_SRC}" "${OUTPUT_DIR}/${APPIMAGE_NAME}"
sha256sum "${OUTPUT_DIR}/${APPIMAGE_NAME}" > "${OUTPUT_DIR}/${APPIMAGE_NAME}.sha256.txt"

log "Validate final AppImage QML payload"
verify_dir="${ROOT_DIR}/.appimage-qml-verify"
rm -rf "${verify_dir}"
mkdir -p "${verify_dir}"
(
  cd "${verify_dir}"
  APPIMAGE_EXTRACT_AND_RUN=1 "${OUTPUT_DIR}/${APPIMAGE_NAME}" --appimage-extract >/dev/null
)
EXTRACTED_APPDIR="${verify_dir}/squashfs-root"
require_appimage_file() {
  local relative_path="$1"
  if [[ ! -f "${EXTRACTED_APPDIR}/${relative_path}" ]]; then
    echo "error: final AppImage is missing ${relative_path}" >&2
    exit 1
  fi
}

for host_variable in \
  LD_LIBRARY_PATH LD_PRELOAD GIO_EXTRA_MODULES GI_TYPELIB_PATH \
  GSETTINGS_SCHEMA_DIR GTK_PATH XDG_DATA_DIRS \
  QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH QML_IMPORT_PATH QML2_IMPORT_PATH \
  QT_QPA_PLATFORM QT_QUICK_CONTROLS_STYLE QT_MEDIA_BACKEND \
  DBUS_SESSION_BUS_ADDRESS XDG_RUNTIME_DIR PATH DISPLAY XAUTHORITY WAYLAND_DISPLAY \
  XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP XDG_SESSION_TYPE DESKTOP_SESSION \
  KDE_FULL_SESSION KDE_SESSION_VERSION GNOME_DESKTOP_SESSION_ID BROWSER \
  XDG_CONFIG_HOME XDG_DATA_HOME; do
  if ! grep -q "DECODIUM_HOST_${host_variable}" "${EXTRACTED_APPDIR}/AppRun"; then
    echo "error: final AppImage launcher does not preserve ${host_variable}" >&2
    exit 1
  fi
done
require_appimage_file "usr/bin/qml/QtQuick/Controls/Material/qmldir"
require_appimage_file "usr/bin/qml/QtQuick/Controls/Material/libqtquickcontrols2materialstyleplugin.so"
require_appimage_file "usr/bin/qml/QtQuick/Controls/Material/impl/libqtquickcontrols2materialstyleimplplugin.so"
require_appimage_file "usr/qml/QtQuick/Controls/Material/qmldir"
require_appimage_file "usr/qml/QtQuick/Controls/Material/libqtquickcontrols2materialstyleplugin.so"
require_appimage_file "usr/qml/QtQuick/Controls/Material/impl/libqtquickcontrols2materialstyleimplplugin.so"
for image_plugin in libqgif.so libqjpeg.so libqtiff.so libqwebp.so; do
  if ! find "${EXTRACTED_APPDIR}" -type f \
      -path "*/imageformats/${image_plugin}" -print -quit | grep -q .; then
    echo "error: final AppImage is missing Qt image plugin ${image_plugin}" >&2
    exit 1
  fi
done
qsqlite_plugin="$(find "${EXTRACTED_APPDIR}" -type f -path '*/sqldrivers/libqsqlite.so' -print -quit)"
if [[ -z "${qsqlite_plugin}" ]]; then
  echo "error: final AppImage is missing Qt SQLite driver libqsqlite.so" >&2
  exit 1
fi
if ! qsqlite_ldd_output="$(
  LD_LIBRARY_PATH="${EXTRACTED_APPDIR}/usr/lib:${EXTRACTED_APPDIR}/usr/bin" \
    ldd "${qsqlite_plugin}" 2>&1
)"; then
  echo "error: unable to inspect final AppImage Qt SQLite driver dependencies" >&2
  printf '%s\n' "${qsqlite_ldd_output}" >&2
  exit 1
fi
if grep -q 'not found' <<<"${qsqlite_ldd_output}"; then
  echo "error: final AppImage Qt SQLite driver has unresolved dependencies" >&2
  printf '%s\n' "${qsqlite_ldd_output}" >&2
  exit 1
fi
verify_qml_plugin_dependencies "final AppImage" \
  "${EXTRACTED_APPDIR}/usr/bin/qml" \
  "${EXTRACTED_APPDIR}/usr/lib"
rtlsdr_library="$(find "${EXTRACTED_APPDIR}/usr/lib" -maxdepth 1 -name 'librtlsdr.so*' -print -quit)"
if [[ -z "${rtlsdr_library}" ]]; then
  echo "error: final AppImage does not contain librtlsdr" >&2
  exit 1
fi
openjpeg_library="$(find "${EXTRACTED_APPDIR}/usr/lib" -maxdepth 1 -name 'libopenjp2.so*' -print -quit)"
if [[ -z "${openjpeg_library}" ]]; then
  echo "error: final AppImage does not contain libopenjp2 for HAMDRM" >&2
  exit 1
fi
if ! find "${EXTRACTED_APPDIR}/usr/share/doc" -type f \
    -name 'THIRD_PARTY_LICENSES_OPENJPEG.md' -print -quit | grep -q .; then
  echo "error: final AppImage does not contain the OpenJPEG notice" >&2
  exit 1
fi

# Do not pipe ldd directly into grep -q while pipefail is active.  On some
# architectures grep exits as soon as it finds librtlsdr and ldd then receives
# SIGPIPE, making a valid payload check fail with no diagnostic.
decodium_ldd_output="$(
  LD_LIBRARY_PATH="${EXTRACTED_APPDIR}/usr/lib:${EXTRACTED_APPDIR}/usr/bin" \
    ldd "${EXTRACTED_APPDIR}/usr/bin/decodium"
)"
if ! grep -q 'librtlsdr.so' <<<"${decodium_ldd_output}"; then
  echo "error: final AppImage executable is not linked to bundled librtlsdr" >&2
  printf '%s\n' "${decodium_ldd_output}" >&2
  exit 1
fi
if ! grep -q 'libopenjp2.so' <<<"${decodium_ldd_output}"; then
  echo "error: final AppImage executable is not linked to bundled OpenJPEG" >&2
  printf '%s\n' "${decodium_ldd_output}" >&2
  exit 1
fi
rm -rf "${verify_dir}"

log "Done"
ls -lh "${OUTPUT_DIR}/${APPIMAGE_NAME}" "${OUTPUT_DIR}/${APPIMAGE_NAME}.sha256.txt"
file "${OUTPUT_DIR}/${APPIMAGE_NAME}"
