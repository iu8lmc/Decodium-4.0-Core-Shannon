#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/normalize-macos-app.sh /path/to/ft2.app

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

APP_BUNDLE="$(cd "$(dirname "${APP_BUNDLE}")" && pwd)/$(basename "${APP_BUNDLE}")"
CONTENTS_DIR="${APP_BUNDLE}/Contents"
MACOS_DIR="${CONTENTS_DIR}/MacOS"
FRAMEWORKS_DIR="${CONTENTS_DIR}/Frameworks"
PLUGINS_DIR="${CONTENTS_DIR}/PlugIns"
RESOURCES_DIR="${CONTENTS_DIR}/Resources"

is_macho() {
  file "$1" | grep -q "Mach-O"
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

  mkdir -p "${RESOURCES_DIR}"

  if [[ -d "${macos_sounds}" ]]; then
    if [[ -d "${resources_sounds}" ]]; then
      rm -rf "${macos_sounds}"
    else
      mv "${macos_sounds}" "${resources_sounds}"
    fi
  fi

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
        if new_dep="$(normalized_framework_dependency "${dep_path}" 2>/dev/null)"; then
          if [[ "${new_dep}" != "${dep_path}" ]]; then
            install_name_tool -change "${dep_path}" "${new_dep}" "${file_path}"
            changed=1
          fi
        fi
      done < <(otool -L "${file_path}" | awk 'NR>1 {print $1}')

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

  if [[ -d "${MACOS_DIR}/sounds" ]]; then
    echo "error: sounds still present in Contents/MacOS"
    exit 1
  fi

  if [[ ! -d "${RESOURCES_DIR}/sounds" ]]; then
    echo "error: missing Contents/Resources/sounds"
    exit 1
  fi

  for file_path in \
    "${MACOS_DIR}/rigctl-wsjtx" \
    "${MACOS_DIR}/rigctld-wsjtx" \
    "${MACOS_DIR}/rigctlcom-wsjtx"; do
    if [[ -L "${file_path}" ]]; then
      echo "error: tool binary is still a symlink: ${file_path}"
      exit 1
    fi
  done

  while IFS= read -r file_path; do
    [[ -n "${file_path}" ]] || continue
    if ! is_macho "${file_path}"; then
      continue
    fi

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
      esac
    done < <(otool -L "${file_path}" | awk 'NR>1 {print $1}')
  done < <(find "${CONTENTS_DIR}" -type f | sort)
}

normalize_bundle_layout
normalize_bundle_macho_paths
validate_bundle

echo "Normalized macOS bundle: ${APP_BUNDLE}"
