#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/normalize-macos-app.sh /path/to/Decodium4.app

What it does:
  1) Ensures sounds live in Contents/Resources/sounds
  2) Replaces any symlinked rigctl binaries with real files
  3) Rewrites bundled Mach-O install names from @executable_path/.../Frameworks to @rpath/...
  4) Adds the matching LC_RPATH entries for executables, plugins, frameworks and dylibs
  5) Validates the resulting bundle layout
EOF
}

if [[ $# -ne 1 ]]; then
  usage
  exit 1
fi

APP_BUNDLE="$1"
if [[ ! -d "${APP_BUNDLE}" || "${APP_BUNDLE}" != *.app ]]; then
  echo "error: expected a macOS .app bundle path, got: ${APP_BUNDLE}"
  exit 1
fi

APP_BUNDLE="$(cd "$(dirname "${APP_BUNDLE}")" && pwd -P)/$(basename "${APP_BUNDLE}")"
CONTENTS_DIR="${APP_BUNDLE}/Contents"
MACOS_DIR="${CONTENTS_DIR}/MacOS"
FRAMEWORKS_DIR="${CONTENTS_DIR}/Frameworks"
PLUGINS_DIR="${CONTENTS_DIR}/PlugIns"
RESOURCES_DIR="${CONTENTS_DIR}/Resources"
QT_QML_BUNDLE_DIR="${RESOURCES_DIR}/qml"
QTDBUS_RPATH_DEP="@rpath/QtDBus.framework/Versions/A/QtDBus"

is_macho() {
  file "$1" | grep -q "Mach-O"
}

bundled_qt_core_version() {
  local qt_core="${FRAMEWORKS_DIR}/QtCore.framework/Versions/A/QtCore"

  [[ -f "${qt_core}" ]] || return 1
  strings "${qt_core}" \
    | awk '
        /^Qt [0-9]+\.[0-9]+\.[0-9]+ / {print $2; exit}
        /^[0-9]+\.[0-9]+\.[0-9]+$/ {print; exit}
      '
}

qt_qmake_for_bundle() {
  local qmake_bin=""
  local qt_version=""
  local required_qt_version=""
  local checked_versions=""

  required_qt_version="$(bundled_qt_core_version || true)"

  for qmake_bin in \
    "${QT_PREFIX:+${QT_PREFIX}/bin/qmake6}" \
    "${QT_PREFIX:+${QT_PREFIX}/bin/qmake}" \
    "${QTDIR:+${QTDIR}/bin/qmake6}" \
    "${QTDIR:+${QTDIR}/bin/qmake}" \
    "$(command -v qmake6 2>/dev/null || true)" \
    "$(command -v qmake 2>/dev/null || true)"; do
    [[ -n "${qmake_bin}" && -x "${qmake_bin}" ]] || continue
    qt_version="$("${qmake_bin}" -query QT_VERSION 2>/dev/null || true)"
    [[ -n "${qt_version}" ]] || continue

    if [[ -z "${required_qt_version}" || "${qt_version}" == "${required_qt_version}" ]]; then
      printf '%s\n' "${qmake_bin}"
      return 0
    fi

    checked_versions+=$'\n'"  ${qmake_bin}: ${qt_version}"
  done

  if [[ -n "${required_qt_version}" ]]; then
    echo "error: unable to locate qmake matching bundled QtCore ${required_qt_version}" >&2
    if [[ -n "${checked_versions}" ]]; then
      echo "error: checked Qt installations:${checked_versions}" >&2
    fi
  fi

  return 1
}

qt_qml_import_root() {
  local candidate=""
  local qmake_bin=""

  if [[ -n "${QT_QML_DIR:-}" && -d "${QT_QML_DIR}" ]]; then
    printf '%s\n' "${QT_QML_DIR}"
    return 0
  fi

  if qmake_bin="$(qt_qmake_for_bundle)"; then
    candidate="$("${qmake_bin}" -query QT_INSTALL_QML 2>/dev/null || true)"
    if [[ -n "${candidate}" && -d "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  fi

  if [[ -n "$(bundled_qt_core_version || true)" ]]; then
    return 1
  fi

  for candidate in \
    "${QT_PREFIX:+${QT_PREFIX}/share/qt/qml}" \
    "${QTDIR:+${QTDIR}/qml}" \
    "${QTDIR:+${QTDIR}/share/qt/qml}" \
    "/opt/homebrew/share/qt/qml" \
    "/usr/local/share/qt/qml"; do
    [[ -n "${candidate}" && -d "${candidate}" ]] || continue
    printf '%s\n' "${candidate}"
    return 0
  done

  return 1
}

qt_plugin_root() {
  local candidate=""
  local qmake_bin=""

  if [[ -n "${QT_PLUGIN_DIR:-}" && -d "${QT_PLUGIN_DIR}" ]]; then
    printf '%s\n' "${QT_PLUGIN_DIR}"
    return 0
  fi

  if qmake_bin="$(qt_qmake_for_bundle)"; then
    candidate="$("${qmake_bin}" -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
    if [[ -n "${candidate}" && -d "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  fi

  if [[ -n "$(bundled_qt_core_version || true)" ]]; then
    return 1
  fi

  for candidate in \
    "${QT_PREFIX:+${QT_PREFIX}/plugins}" \
    "${QT_PREFIX:+${QT_PREFIX}/share/qt/plugins}" \
    "${QTDIR:+${QTDIR}/plugins}" \
    "${QTDIR:+${QTDIR}/share/qt/plugins}" \
    "/opt/homebrew/share/qt/plugins" \
    "/usr/local/share/qt/plugins"; do
    [[ -n "${candidate}" && -d "${candidate}" ]] || continue
    printf '%s\n' "${candidate}"
    return 0
  done

  return 1
}

copy_qt_qml_imports_into_bundle() {
  local qt_qml_dir=""
  local entry=""
  local module=""
  local src=""
  local dest=""

  qt_qml_dir="$(qt_qml_import_root)" || {
    echo "error: unable to locate Qt QML import directory"
    echo "error: set QT_QML_DIR or ensure qmake6 is available in PATH"
    exit 1
  }

  mkdir -p "${QT_QML_BUNDLE_DIR}"

  for module in QML QtQml QtCore QtQuick Qt; do
    rm -rf "${QT_QML_BUNDLE_DIR}/${module}"
    rm -rf "${MACOS_DIR}/qml/${module}"
  done

  for entry in \
    QML \
    QtQml/qmldir \
    QtQml/libqmlplugin.dylib \
    QtQml/Models \
    QtQml/WorkerScript \
    QtCore/qmldir \
    QtCore/libqtqmlcoreplugin.dylib \
    Qt/labs/folderlistmodel \
    QtQuick/qmldir \
    QtQuick/libqtquick2plugin.dylib \
    QtQuick/Controls/qmldir \
    QtQuick/Controls/libqtquickcontrols2plugin.dylib \
    QtQuick/Controls/Basic \
    QtQuick/Controls/Material \
    QtQuick/Controls/impl \
    QtQuick/Dialogs \
    QtQuick/Effects \
    QtQuick/Layouts \
    QtQuick/Templates \
    QtQuick/Window; do
    src="${qt_qml_dir}/${entry}"
    [[ -e "${src}" ]] || continue

    dest="${QT_QML_BUNDLE_DIR}/${entry}"
    rm -rf "${dest}"
    mkdir -p "$(dirname "${dest}")"
    cp -R -L -p "${src}" "${dest}"
  done
}

move_app_qml_into_resources() {
  local app_qml_src="${MACOS_DIR}/qml"
  local entry=""
  local dest=""

  [[ -d "${app_qml_src}" ]] || return 0

  mkdir -p "${QT_QML_BUNDLE_DIR}"
  while IFS= read -r entry; do
    [[ -n "${entry}" ]] || continue
    dest="${QT_QML_BUNDLE_DIR}/$(basename "${entry}")"
    rm -rf "${dest}"
    mv "${entry}" "${dest}"
  done < <(find "${app_qml_src}" -mindepth 1 -maxdepth 1 -print 2>/dev/null)

  rmdir "${app_qml_src}" 2>/dev/null || true
}

prune_qml_type_metadata() {
  local qmltypes_file=""
  local removed=0

  [[ -d "${QT_QML_BUNDLE_DIR}" ]] || return 0

  while IFS= read -r qmltypes_file; do
    [[ -n "${qmltypes_file}" ]] || continue
    rm -f "${qmltypes_file}"
    removed=1
  done < <(find "${QT_QML_BUNDLE_DIR}" -type f -name 'plugins.qmltypes' -print 2>/dev/null)

  if [[ "${removed}" -ne 0 ]]; then
    echo "Pruned Qt QML type metadata from runtime bundle"
  fi
}

normalize_qml_resource_permissions() {
  local qml_file=""

  [[ -d "${QT_QML_BUNDLE_DIR}" ]] || return 0

  while IFS= read -r qml_file; do
    [[ -n "${qml_file}" ]] || continue
    if ! file "${qml_file}" | grep -q "Mach-O"; then
      chmod a-x "${qml_file}"
    fi
  done < <(find "${QT_QML_BUNDLE_DIR}" -type f -perm -111 -print 2>/dev/null)
}

copy_qt_plugins_into_bundle() {
  local qt_plugins_dir=""
  local category=""
  local src=""
  local dest=""

  qt_plugins_dir="$(qt_plugin_root)" || {
    echo "warning: unable to locate matching Qt plugin directory; TLS and multimedia plugins will not be bundled"
    return 0
  }

  mkdir -p "${PLUGINS_DIR}"

  for category in tls multimedia networkaccess; do
    src="${qt_plugins_dir}/${category}"
    [[ -d "${src}" ]] || continue
    if ! find "${src}" -maxdepth 1 \( -type f -o -type l \) -name '*.dylib' -print -quit 2>/dev/null | grep -q .; then
      continue
    fi

    dest="${PLUGINS_DIR}/${category}"
    rm -rf "${dest}"
    mkdir -p "$(dirname "${dest}")"
    cp -R -L -p "${src}" "${dest}"
  done
}

validate_qt_qml_imports() {
  local missing=0
  local required_path=""
  local symlink_path=""

  for required_path in \
    "${QT_QML_BUNDLE_DIR}/QtQuick/qmldir" \
    "${QT_QML_BUNDLE_DIR}/QtQuick/Controls/qmldir" \
    "${QT_QML_BUNDLE_DIR}/QtQuick/Controls/Material/qmldir" \
    "${QT_QML_BUNDLE_DIR}/QtQuick/Dialogs/qmldir" \
    "${QT_QML_BUNDLE_DIR}/QtQuick/Effects/qmldir" \
    "${QT_QML_BUNDLE_DIR}/QtQuick/Layouts/qmldir" \
    "${QT_QML_BUNDLE_DIR}/QtQuick/Templates/qmldir" \
    "${QT_QML_BUNDLE_DIR}/QtQuick/Window/qmldir" \
    "${QT_QML_BUNDLE_DIR}/QtQml/qmldir" \
    "${QT_QML_BUNDLE_DIR}/Qt/labs/folderlistmodel/qmldir" \
    "${QT_QML_BUNDLE_DIR}/QML/qmldir"; do
    if [[ ! -f "${required_path}" ]]; then
      echo "error: missing bundled Qt QML import: ${required_path}"
      missing=1
    fi
  done

  if ! find "${QT_QML_BUNDLE_DIR}/QtQuick/Controls" -type f -name '*qtquickcontrols2plugin*.dylib' -print -quit 2>/dev/null | grep -q .; then
    echo "error: missing bundled Qt Quick Controls plugin under ${QT_QML_BUNDLE_DIR}/QtQuick/Controls"
    missing=1
  fi

  if [[ ! -f "${QT_QML_BUNDLE_DIR}/decodium/BootLoader.qml" ]]; then
    echo "error: missing Decodium QML runtime under ${QT_QML_BUNDLE_DIR}/decodium"
    missing=1
  fi

  if [[ -d "${MACOS_DIR}/qml" ]]; then
    echo "error: QML files remain under Contents/MacOS: ${MACOS_DIR}/qml"
    missing=1
  fi

  while IFS= read -r symlink_path; do
    [[ -n "${symlink_path}" ]] || continue
    echo "error: symlink remains in bundled QML imports: ${symlink_path}"
    missing=1
  done < <(find "${QT_QML_BUNDLE_DIR}" -type l -print 2>/dev/null)

  if find "${QT_QML_BUNDLE_DIR}" -type f -name 'plugins.qmltypes' -print -quit 2>/dev/null | grep -q .; then
    echo "error: Qt QML type metadata remains in runtime bundle"
    missing=1
  fi

  if [[ "${missing}" -ne 0 ]]; then
    exit 1
  fi
}

validate_qt_runtime_plugins() {
  local missing=0
  local plugin_symlink=""

  if [[ -d "${FRAMEWORKS_DIR}/QtNetwork.framework" ]] \
    && ! find "${PLUGINS_DIR}/tls" -maxdepth 1 \( -type f -o -type l \) -name '*.dylib' -print -quit 2>/dev/null | grep -q .; then
    echo "warning: missing bundled Qt TLS plugins under ${PLUGINS_DIR}/tls"
  fi

  if [[ -d "${FRAMEWORKS_DIR}/QtMultimedia.framework" ]] \
    && ! find "${PLUGINS_DIR}/multimedia" -maxdepth 1 \( -type f -o -type l \) -name '*.dylib' -print -quit 2>/dev/null | grep -q .; then
    echo "warning: missing bundled Qt multimedia plugins under ${PLUGINS_DIR}/multimedia"
  fi

  while IFS= read -r plugin_symlink; do
    [[ -n "${plugin_symlink}" ]] || continue
    echo "error: symlink remains in bundled Qt plugin imports: ${plugin_symlink}"
    missing=1
  done < <(find "${PLUGINS_DIR}/tls" "${PLUGINS_DIR}/multimedia" "${PLUGINS_DIR}/networkaccess" -type l -print 2>/dev/null)

  if [[ "${missing}" -ne 0 ]]; then
    exit 1
  fi
}

resolve_realpath() {
  local target="$1"
  local link_target=""

  while [[ -L "${target}" ]]; do
    link_target="$(readlink "${target}")"
    if [[ "${link_target}" = /* ]]; then
      target="${link_target}"
    else
      target="$(cd "$(dirname "${target}")" && pwd)/${link_target}"
    fi
  done

  printf '%s/%s\n' "$(cd "$(dirname "${target}")" && pwd -P)" "$(basename "${target}")"
}

ensure_real_binary() {
  local tool_path="$1"
  local resolved=""
  local temp_path=""

  [[ -e "${tool_path}" ]] || return 0
  [[ -L "${tool_path}" ]] || return 0

  resolved="$(resolve_realpath "${tool_path}")"
  temp_path="${tool_path}.real"
  cp -f "${resolved}" "${temp_path}"
  chmod 755 "${temp_path}"
  mv -f "${temp_path}" "${tool_path}"
}

is_decodium_qml_executable() {
  local executable_path="$1"

  [[ -x "${executable_path}" ]] || return 1
  strings "${executable_path}" \
    | awk 'index($0, "QML OK - entering event loop") {found=1} END {exit found ? 0 : 1}'
}

promote_decodium_qml_main_executable() {
  local main_exec="${MACOS_DIR}/Decodium4"
  local qml_exec="${MACOS_DIR}/decodium"

  if is_decodium_qml_executable "${main_exec}"; then
    rm -f "${qml_exec}"
    return 0
  fi

  if ! is_decodium_qml_executable "${qml_exec}"; then
    echo "error: Decodium QML executable is missing or invalid: ${qml_exec}"
    echo "error: ${main_exec} would launch the legacy FT2 UI instead of Decodium4"
    exit 1
  fi

  rm -f "${main_exec}"
  mv "${qml_exec}" "${main_exec}"
  chmod 755 "${main_exec}"
}

framework_relative_path() {
  local file_path="$1"
  printf '%s\n' "${file_path#${FRAMEWORKS_DIR}/}"
}

desired_install_id() {
  local file_path="$1"

  if [[ "${file_path}" == "${FRAMEWORKS_DIR}/"* ]]; then
    printf '@rpath/%s\n' "$(framework_relative_path "${file_path}")"
    return 0
  fi

  if [[ "${file_path}" == "${PLUGINS_DIR}/"* ]]; then
    printf '@rpath/%s\n' "${file_path#${CONTENTS_DIR}/}"
    return 0
  fi

  if [[ "${file_path}" == "${QT_QML_BUNDLE_DIR}/"* ]]; then
    printf '@rpath/%s\n' "${file_path#${CONTENTS_DIR}/}"
    return 0
  fi

  return 1
}

framework_rpath_for_file() {
  local file_path="$1"
  local file_dir=""
  local relative_dir=""
  local rpath=""

  if [[ "${file_path}" == "${MACOS_DIR}/"* ]]; then
    printf '%s\n' '@executable_path/../Frameworks'
    return 0
  fi

  if [[ "${file_path}" == "${PLUGINS_DIR}/"* ]]; then
    printf '%s\n' '@loader_path/../../Frameworks'
    return 0
  fi

  if [[ "${file_path}" == "${QT_QML_BUNDLE_DIR}/"* ]]; then
    printf '%s\n' '@executable_path/../Frameworks'
    return 0
  fi

  if [[ "${file_path}" == "${FRAMEWORKS_DIR}/"* ]]; then
    file_dir="$(dirname "${file_path}")"
    relative_dir="${file_dir#${FRAMEWORKS_DIR}}"
    relative_dir="${relative_dir#/}"
    if [[ -z "${relative_dir}" ]]; then
      printf '%s\n' '@loader_path'
      return 0
    fi

    rpath='@loader_path'
    while [[ -n "${relative_dir}" ]]; do
      rpath+='/..'
      if [[ "${relative_dir}" == */* ]]; then
        relative_dir="${relative_dir#*/}"
      else
        relative_dir=""
      fi
    done
    printf '%s\n' "${rpath}"
    return 0
  fi

  return 1
}

expanded_rpaths_for_file() {
  local file_path="$1"
  local file_dir=""
  local rpath=""
  local expanded=""

  file_dir="$(dirname "${file_path}")"
  while IFS= read -r rpath; do
    [[ -n "${rpath}" ]] || continue
    case "${rpath}" in
      @loader_path*)
        expanded="${file_dir}${rpath#@loader_path}"
        ;;
      @executable_path*)
        expanded="${MACOS_DIR}${rpath#@executable_path}"
        ;;
      /*)
        expanded="${rpath}"
        ;;
      *)
        continue
        ;;
    esac

    if [[ -d "${expanded}" ]]; then
      printf '%s\n' "$(cd "${expanded}" && pwd -P)"
    fi
  done < <(otool -l "${file_path}" | awk '/LC_RPATH/{flag=1; next} flag && $1=="path"{print $2; flag=0}')
}

current_rpaths_for_file() {
  local file_path="$1"

  otool -l "${file_path}" | awk '/LC_RPATH/{flag=1; next} flag && $1=="path"{print $2; flag=0}'
}

expanded_rpath_value() {
  local file_path="$1"
  local rpath="$2"
  local file_dir=""

  file_dir="$(dirname "${file_path}")"
  case "${rpath}" in
    @loader_path*)
      printf '%s\n' "${file_dir}${rpath#@loader_path}"
      ;;
    @executable_path*)
      printf '%s\n' "${MACOS_DIR}${rpath#@executable_path}"
      ;;
    /*)
      printf '%s\n' "${rpath}"
      ;;
    *)
      return 1
      ;;
  esac
}

is_safe_bundle_rpath() {
  local file_path="$1"
  local rpath="$2"
  local expanded=""
  local resolved=""

  case "${rpath}" in
    @loader_path*|@executable_path*)
      expanded="$(expanded_rpath_value "${file_path}" "${rpath}")" || return 1
      [[ -d "${expanded}" ]] || return 0
      resolved="$(cd "${expanded}" && pwd -P)"
      [[ "${resolved}" == "${APP_BUNDLE}"/* ]]
      return
      ;;
    *)
      return 1
      ;;
  esac
}

remove_unsafe_rpaths() {
  local file_path="$1"
  local rpath=""

  while IFS= read -r rpath; do
    [[ -n "${rpath}" ]] || continue
    if is_safe_bundle_rpath "${file_path}" "${rpath}"; then
      continue
    fi

    chmod u+w "${file_path}"
    install_name_tool -delete_rpath "${rpath}" "${file_path}" 2>/dev/null || true
  done < <(current_rpaths_for_file "${file_path}")
}

resolve_bundled_rpath_dependency() {
  local file_path="$1"
  local dep_path="$2"
  local dep_suffix=""
  local rpath_root=""
  local candidate=""
  local resolved=""

  [[ "${dep_path}" == @rpath/* ]] || return 1
  dep_suffix="${dep_path#@rpath/}"

  while IFS= read -r rpath_root; do
    [[ -n "${rpath_root}" ]] || continue
    candidate="${rpath_root}/${dep_suffix}"
    [[ -e "${candidate}" ]] || continue
    resolved="$(resolve_realpath "${candidate}")"
    case "${resolved}" in
      "${APP_BUNDLE}"/*)
        printf '%s\n' "${resolved}"
        return 0
        ;;
    esac
  done < <(expanded_rpaths_for_file "${file_path}")

  return 1
}

resolve_first_rpath_dependency() {
  local file_path="$1"
  local dep_path="$2"
  local dep_suffix=""
  local rpath_root=""
  local candidate=""

  [[ "${dep_path}" == @rpath/* ]] || return 1
  dep_suffix="${dep_path#@rpath/}"

  while IFS= read -r rpath_root; do
    [[ -n "${rpath_root}" ]] || continue
    candidate="${rpath_root}/${dep_suffix}"
    [[ -e "${candidate}" ]] || continue
    resolve_realpath "${candidate}"
    return 0
  done < <(expanded_rpaths_for_file "${file_path}")

  return 1
}

normalized_framework_dependency() {
  local dep_path="$1"

  case "${dep_path}" in
    @*Frameworks/*)
      printf '@rpath/%s\n' "${dep_path##*/Frameworks/}"
      return 0
      ;;
  esac

  return 1
}

resolve_external_rpath_dependency() {
  local file_path="$1"
  local dep_path="$2"
  local dep_suffix=""
  local rpath_root=""
  local search_root=""
  local candidate=""

  [[ "${dep_path}" == @rpath/* ]] || return 1
  dep_suffix="${dep_path#@rpath/}"

  while IFS= read -r rpath_root; do
    [[ -n "${rpath_root}" ]] || continue
    candidate="${rpath_root}/${dep_suffix}"
    if [[ -e "${candidate}" ]]; then
      printf '%s\n' "$(resolve_realpath "${candidate}")"
      return 0
    fi
  done < <(expanded_rpaths_for_file "${file_path}")

  if [[ -n "${DECODIUM_BUNDLE_LIBRARY_DIRS:-}" ]]; then
    while IFS= read -r search_root; do
      [[ -n "${search_root}" && -d "${search_root}" ]] || continue
      candidate="${search_root}/${dep_suffix}"
      if [[ -e "${candidate}" ]]; then
        printf '%s\n' "$(resolve_realpath "${candidate}")"
        return 0
      fi
    done < <(tr ':' '\n' <<<"${DECODIUM_BUNDLE_LIBRARY_DIRS}")
  fi

  for search_root in \
    "${QT_PREFIX:+${QT_PREFIX}/lib}" \
    "${QTDIR:+${QTDIR}/lib}" \
    "/opt/homebrew/opt/qt/lib" \
    "/usr/local/opt/qt/lib" \
    "/opt/homebrew/lib" \
    "/usr/local/lib"; do
    [[ -n "${search_root}" && -d "${search_root}" ]] || continue
    candidate="${search_root}/${dep_suffix}"
    if [[ -e "${candidate}" ]]; then
      printf '%s\n' "$(resolve_realpath "${candidate}")"
      return 0
    fi
  done

  return 1
}

copy_absolute_dependency_into_bundle() {
  local dep_path="$1"
  local framework_root=""
  local framework_name=""
  local framework_inside=""
  local dest_root=""
  local dest_path=""
  local dep_name=""

  case "${dep_path}" in
    /System/*|/usr/lib/*)
      return 1
      ;;
    /opt/*|/usr/local/*|/Users/*)
      ;;
    *)
      return 1
      ;;
  esac

  mkdir -p "${FRAMEWORKS_DIR}"

  if [[ "${dep_path}" == *.framework/* ]]; then
    framework_root="${dep_path%%.framework/*}.framework"
    framework_name="$(basename "${framework_root}")"
    framework_inside="${dep_path#${framework_root}/}"
    dest_root="${FRAMEWORKS_DIR}/${framework_name}"
    dest_path="${dest_root}/${framework_inside}"

    if [[ ! -e "${dest_path}" ]]; then
      rm -rf "${dest_root}"
      ditto "${framework_root}" "${dest_root}"
    fi

    printf '@rpath/%s/%s\n' "${framework_name}" "${framework_inside}"
    return 0
  fi

  dep_name="$(basename "${dep_path}")"
  dest_path="${FRAMEWORKS_DIR}/${dep_name}"
  if [[ ! -e "${dest_path}" ]]; then
    cp -fL "${dep_path}" "${dest_path}"
    chmod u+w "${dest_path}"
  fi

  printf '@rpath/%s\n' "${dep_name}"
  return 0
}

copy_missing_rpath_dependency_into_bundle() {
  local file_path="$1"
  local dep_path="$2"
  local resolved=""

  [[ "${dep_path}" == @rpath/* ]] || return 1

  if resolve_bundled_rpath_dependency "${file_path}" "${dep_path}" >/dev/null 2>&1; then
    return 1
  fi

  resolved="$(resolve_external_rpath_dependency "${file_path}" "${dep_path}")" || return 1
  copy_absolute_dependency_into_bundle "${resolved}"
}

qtgui_requires_qtdbus() {
  local qt_gui="$1"

  [[ -f "${qt_gui}" ]] || return 1
  otool -L "${qt_gui}" | awk 'NR>1 {print $1}' | grep -Fxq "${QTDBUS_RPATH_DEP}"
}

ensure_qtgui_dbus_dependency() {
  local qt_gui="${FRAMEWORKS_DIR}/QtGui.framework/Versions/A/QtGui"
  local qt_dbus="${FRAMEWORKS_DIR}/QtDBus.framework/Versions/A/QtDBus"
  local copied=""

  qtgui_requires_qtdbus "${qt_gui}" || return 0
  [[ -f "${qt_dbus}" ]] && return 0

  copied="$(copy_missing_rpath_dependency_into_bundle "${qt_gui}" "${QTDBUS_RPATH_DEP}")" || {
    echo "error: QtGui requires QtDBus, but QtDBus.framework could not be bundled"
    echo "error: missing dependency: ${QTDBUS_RPATH_DEP}"
    exit 1
  }

  echo "Bundled QtDBus dependency for QtGui: ${copied}"
}

validate_qt_runtime_frameworks() {
  local qt_gui="${FRAMEWORKS_DIR}/QtGui.framework/Versions/A/QtGui"
  local qt_dbus="${FRAMEWORKS_DIR}/QtDBus.framework/Versions/A/QtDBus"

  if qtgui_requires_qtdbus "${qt_gui}" && [[ ! -f "${qt_dbus}" ]]; then
    echo "error: missing QtDBus.framework required by bundled QtGui"
    echo "error: expected ${qt_dbus}"
    exit 1
  fi
}

install_id_of() {
  otool -D "$1" 2>/dev/null | awk 'NR==2 {print $1; exit}'
}

ensure_rpath() {
  local file_path="$1"
  local rpath="$2"

  [[ -n "${rpath}" ]] || return 0

  if otool -l "${file_path}" | awk '/LC_RPATH/{flag=1; next} flag && $1=="path"{print $2; flag=0}' | grep -Fxq "${rpath}"; then
    return 0
  fi

  chmod u+w "${file_path}"
  install_name_tool -add_rpath "${rpath}" "${file_path}"
}

normalize_bundle_layout() {
  local macos_sounds="${MACOS_DIR}/sounds"
  local resources_sounds="${RESOURCES_DIR}/sounds"
  local resource_file=""
  local resource_name=""

  mkdir -p "${RESOURCES_DIR}"

  if [[ -d "${macos_sounds}" ]]; then
    if [[ -d "${resources_sounds}" ]]; then
      rm -rf "${macos_sounds}"
    else
      mv "${macos_sounds}" "${resources_sounds}"
    fi
  fi

  while IFS= read -r resource_file; do
    [[ -n "${resource_file}" ]] || continue
    resource_name="$(basename "${resource_file}")"
    rm -f "${RESOURCES_DIR}/${resource_name}"
    mv "${resource_file}" "${RESOURCES_DIR}/${resource_name}"
  done < <(find "${MACOS_DIR}" -maxdepth 1 -type f ! -perm -111 -print 2>/dev/null)

  ensure_real_binary "${MACOS_DIR}/rigctl-wsjtx"
  ensure_real_binary "${MACOS_DIR}/rigctld-wsjtx"
  ensure_real_binary "${MACOS_DIR}/rigctlcom-wsjtx"
}

normalize_bundle_macho_paths() {
  local file_path=""
  local current_id=""
  local new_id=""
  local dep_path=""
  local new_dep=""
  local rpath=""
  local pass=0
  local changed=0

  for pass in {1..8}; do
    changed=0
    while IFS= read -r file_path; do
      [[ -n "${file_path}" ]] || continue
      if ! is_macho "${file_path}"; then
        continue
      fi

      chmod u+w "${file_path}"

      current_id="$(install_id_of "${file_path}")"
      if new_id="$(desired_install_id "${file_path}" 2>/dev/null)"; then
        if [[ -n "${current_id}" && "${current_id}" != "${new_id}" ]]; then
          install_name_tool -id "${new_id}" "${file_path}"
          current_id="${new_id}"
          changed=1
        fi
      fi

      while IFS= read -r dep_path; do
        [[ -n "${dep_path}" ]] || continue
        if [[ -n "${current_id}" && "${dep_path}" == "${current_id}" ]]; then
          continue
        fi
        if new_dep="$(copy_absolute_dependency_into_bundle "${dep_path}" 2>/dev/null)"; then
          if [[ "${new_dep}" != "${dep_path}" ]]; then
            install_name_tool -change "${dep_path}" "${new_dep}" "${file_path}"
            changed=1
          fi
          continue
        fi
        if new_dep="$(copy_missing_rpath_dependency_into_bundle "${file_path}" "${dep_path}" 2>/dev/null)"; then
          if [[ "${new_dep}" != "${dep_path}" ]]; then
            install_name_tool -change "${dep_path}" "${new_dep}" "${file_path}"
          fi
          changed=1
          continue
        fi
        if new_dep="$(normalized_framework_dependency "${dep_path}" 2>/dev/null)"; then
          if [[ "${new_dep}" != "${dep_path}" ]]; then
            install_name_tool -change "${dep_path}" "${new_dep}" "${file_path}"
            changed=1
          fi
        fi
      done < <(otool -L "${file_path}" | awk 'NR>1 {print $1}')

      remove_unsafe_rpaths "${file_path}"

      if rpath="$(framework_rpath_for_file "${file_path}" 2>/dev/null)"; then
        ensure_rpath "${file_path}" "${rpath}"
      fi
    done < <(find "${CONTENTS_DIR}" -type f | sort)

    if [[ "${changed}" -eq 0 ]]; then
      break
    fi
  done

  while IFS= read -r file_path; do
    [[ -n "${file_path}" ]] || continue
    if ! is_macho "${file_path}"; then
      continue
    fi
    current_id="$(install_id_of "${file_path}")"
    while IFS= read -r dep_path; do
      [[ -n "${dep_path}" ]] || continue
      if [[ -n "${current_id}" && "${dep_path}" == "${current_id}" ]]; then
        continue
      fi
      copy_absolute_dependency_into_bundle "${dep_path}" >/dev/null 2>&1 && continue
    done < <(otool -L "${file_path}" | awk 'NR>1 {print $1}')
  done < <(find "${CONTENTS_DIR}" -type f | sort)
}

validate_bundle() {
  local file_path=""
  local dep_path=""
  local current_id=""
  local resolved_dep=""
  local rpath=""
  local main_exec_name=""
  local main_exec_path=""

  if [[ -d "${MACOS_DIR}/sounds" ]]; then
    echo "error: sounds still present in Contents/MacOS"
    exit 1
  fi

  if [[ ! -d "${RESOURCES_DIR}/sounds" ]]; then
    echo "error: missing Contents/Resources/sounds"
    exit 1
  fi

  while IFS= read -r file_path; do
    [[ -n "${file_path}" ]] || continue
    echo "error: non-executable resource file remains in Contents/MacOS: ${file_path}"
    echo "error: move bundled data files to Contents/Resources before signing"
    exit 1
  done < <(find "${MACOS_DIR}" -maxdepth 1 -type f ! -perm -111 -print 2>/dev/null)

  for file_path in \
    "${MACOS_DIR}/rigctl-wsjtx" \
    "${MACOS_DIR}/rigctld-wsjtx" \
    "${MACOS_DIR}/rigctlcom-wsjtx"; do
    if [[ -L "${file_path}" ]]; then
      echo "error: tool binary is still a symlink: ${file_path}"
      exit 1
    fi
  done

  main_exec_name="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "${CONTENTS_DIR}/Info.plist" 2>/dev/null || true)"
  main_exec_path="${MACOS_DIR}/${main_exec_name}"
  if [[ "${main_exec_name}" != "Decodium4" || ! -x "${main_exec_path}" ]]; then
    echo "error: Decodium4.app main executable is invalid: ${main_exec_name:-<empty>}"
    exit 1
  fi
  if ! is_decodium_qml_executable "${main_exec_path}"; then
    echo "error: ${main_exec_path} is not the Decodium QML runtime"
    echo "error: release would open the legacy FT2 UI"
    exit 1
  fi

  while IFS= read -r file_path; do
    [[ -n "${file_path}" ]] || continue
    if ! is_macho "${file_path}"; then
      continue
    fi

    while IFS= read -r rpath; do
      [[ -n "${rpath}" ]] || continue
      if ! is_safe_bundle_rpath "${file_path}" "${rpath}"; then
        echo "error: unsafe LC_RPATH remains in bundle:"
        echo "  ${file_path} -> ${rpath}"
        exit 1
      fi
    done < <(current_rpaths_for_file "${file_path}")

    current_id="$(install_id_of "${file_path}")"
    if [[ "${file_path}" == "${FRAMEWORKS_DIR}/"* && -n "${current_id}" && "${current_id}" != @rpath/* ]]; then
      echo "error: framework or dylib install id is not @rpath-based: ${file_path} -> ${current_id}"
      exit 1
    fi

    while IFS= read -r dep_path; do
      [[ -n "${dep_path}" ]] || continue
      if [[ -n "${current_id}" && "${dep_path}" == "${current_id}" ]]; then
        continue
      fi
      case "${dep_path}" in
        @*Frameworks/*)
          echo "error: stale Frameworks reference remains: ${file_path} -> ${dep_path}"
          exit 1
          ;;
        @rpath/*)
          resolved_dep="$(resolve_first_rpath_dependency "${file_path}" "${dep_path}")" || {
            echo "error: unresolved bundled @rpath dependency:"
            echo "  ${file_path} -> ${dep_path}"
            exit 1
          }
          if [[ "${resolved_dep}" != "${APP_BUNDLE}"/* ]]; then
            echo "error: @rpath dependency resolves outside the app bundle:"
            echo "  ${file_path} -> ${dep_path}"
            echo "  first match: ${resolved_dep}"
            exit 1
          fi
          ;;
      esac
    done < <(otool -L "${file_path}" | awk 'NR>1 {print $1}')
  done < <(find "${CONTENTS_DIR}" -type f | sort)
}

normalize_bundle_layout
promote_decodium_qml_main_executable
copy_qt_qml_imports_into_bundle
move_app_qml_into_resources
prune_qml_type_metadata
normalize_qml_resource_permissions
copy_qt_plugins_into_bundle
ensure_qtgui_dbus_dependency
normalize_bundle_macho_paths
validate_qt_qml_imports
validate_qt_runtime_plugins
validate_qt_runtime_frameworks
validate_bundle

echo "Normalized macOS bundle: ${APP_BUNDLE}"
