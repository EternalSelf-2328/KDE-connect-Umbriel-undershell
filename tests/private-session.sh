#!/usr/bin/env bash
# Isolated session bus without desktop service activation or access to the live display/system bus.
set -euo pipefail
CASE="$(mktemp -d /tmp/uc-test.XXXXXX)"
trap 'rm -rf "$CASE"' EXIT
mkdir -p "$CASE/config" "$CASE/data" "$CASE/cache" "$CASE/run"
chmod 700 "$CASE/run"
cat > "$CASE/bus.conf" <<'XML'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=/tmp</listen>
  <policy context="default"><allow send_destination="*"/><allow receive_sender="*"/><allow own="*"/></policy>
</busconfig>
XML
env -u WAYLAND_DISPLAY -u DISPLAY XDG_CONFIG_HOME="$CASE/config" XDG_DATA_HOME="$CASE/data" XDG_CACHE_HOME="$CASE/cache" XDG_RUNTIME_DIR="$CASE/run" \
  UMBRIEL_CONNECT_LANGUAGE=es DBUS_SYSTEM_BUS_ADDRESS="unix:path=$CASE/no-system-bus" GIO_USE_VFS=local QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software SOLID_DISABLE_KDECONNECT=1 \
  dbus-run-session --config-file="$CASE/bus.conf" -- "$@"
