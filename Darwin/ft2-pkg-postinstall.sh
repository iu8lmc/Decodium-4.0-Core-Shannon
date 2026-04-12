#!/bin/sh
set -eu

DAEMON_DIR="/Library/LaunchDaemons"
PLIST_NAME="com.ft2.jtdx.sysctl.plist"
TARGET_PLIST="${DAEMON_DIR}/${PLIST_NAME}"
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SOURCE_PLIST="${SCRIPT_DIR}/${PLIST_NAME}"

if [ ! -f "$SOURCE_PLIST" ]; then
  echo "warning: missing ${SOURCE_PLIST}" >&2
  exit 0
fi

# Avoid conflicting startup daemons that can overwrite shm values.
for legacy in com.jtdx.sysctl.plist com.wsjtx.sysctl.plist; do
  legacy_path="${DAEMON_DIR}/${legacy}"
  if [ -f "$legacy_path" ]; then
    /bin/launchctl bootout system "$legacy_path" >/dev/null 2>&1 || true
    /bin/mv -f "$legacy_path" "${legacy_path}.disabled" >/dev/null 2>&1 || true
  fi
done

/bin/cp -f "$SOURCE_PLIST" "$TARGET_PLIST"
/usr/sbin/chown root:wheel "$TARGET_PLIST"
/bin/chmod 644 "$TARGET_PLIST"

/bin/launchctl bootout system "$TARGET_PLIST" >/dev/null 2>&1 || true
/bin/launchctl bootstrap system "$TARGET_PLIST" >/dev/null 2>&1 || true

# Apply values immediately for current session, then daemon keeps them on boot.
/usr/sbin/sysctl -w kern.sysv.shmmax=104857600 >/dev/null 2>&1 || true
/usr/sbin/sysctl -w kern.sysv.shmall=51200 >/dev/null 2>&1 || true

exit 0
