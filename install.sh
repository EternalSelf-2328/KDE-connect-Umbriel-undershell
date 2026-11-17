#!/usr/bin/env bash
# Build and install the source-pinned private engine and native UI; no global service/network changes.
set -euo pipefail
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="$HOME/.local"
AUTOSTART=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --help|-h)
      echo "Usage: ./install.sh [--prefix DIR] [--autostart]"
      echo "Builds, tests and installs Umbriel Connect; --autostart enables its user services on next graphical login."
      echo "Dependencies and Android build: docs/BUILDING.md"
      exit 0 ;;
    --prefix) [[ $# -ge 2 ]] || { echo "Missing prefix" >&2; exit 2; }; PREFIX="$2"; shift 2 ;;
    --autostart) AUTOSTART=1; shift ;;
    *) echo "Unknown option: $1 (see --help)" >&2; exit 2 ;;
  esac
done
[[ "$PREFIX" = /* ]] || { echo "Use an absolute installation prefix" >&2; exit 2; }
git -C "$SRC" submodule update --init --recursive
bash "$SRC/scripts/engine/build.sh" "$PREFIX"
bash "$SRC/tests/engine.sh"
cmake -S "$SRC" -B "$SRC/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build "$SRC/build" --parallel "${UMBRIEL_BUILD_JOBS:-2}"
ctest --test-dir "$SRC/build" --output-on-failure
# Existing trust, user settings, VPN, compositor and running windows are preserved.
if [[ -e "$PREFIX/bin/umbriel-connect" ]]; then
  BACKUP="$PREFIX/share/umbriel-connect/backups/$(date +%Y%m%d-%H%M%S)"
  install -d -m 700 "$BACKUP" "$BACKUP/bin" "$BACKUP/private"
  UC_TRUST="${XDG_CONFIG_HOME:-$HOME/.config}/umbriel-connect/remote/clients.json"
  if [[ -f "$UC_TRUST" ]]; then
    cp -p "$UC_TRUST" "$BACKUP/private/clients.json"
    chmod 600 "$BACKUP/private/clients.json"
  fi
  UC_BUS="$PREFIX/share/dbus-1/services/org.eternalself.UmbrielConnect.Engine.service"
  [[ ! -f "$UC_BUS" ]] || cp -p "$UC_BUS" "$BACKUP/private/"
  for name in umbriel-connect umbriel-connect-remote umbriel-remote-session; do
    [[ ! -f "$PREFIX/bin/$name" ]] || cp -a "$PREFIX/bin/$name" "$BACKUP/bin/"
  done
  [[ ! -d "$PREFIX/lib/umbriel-connect/engine" ]] || cp -a "$PREFIX/lib/umbriel-connect/engine" "$BACKUP/engine"
  echo "Previous binaries and private trust snapshot: $BACKUP"
  echo "When rolling back before 0.4.1, also restore its pre-v2 clients.json snapshot; never reuse delegated v2 trust with the old receiver."
fi
cmake --install "$SRC/build/engine" --strip
cmake --install "$SRC/build" --strip
echo "Installed: $PREFIX/bin/umbriel-connect"
echo "Pair the full phone app with the new engine; old KDE Connect trust is not imported."
echo "Reopen the app to load the update. No running desktop/VPN session was restarted."

if [[ "$AUTOSTART" == 1 ]]; then
  systemctl --user daemon-reload
  systemctl --user enable "$PREFIX/share/systemd/user/umbriel-connect-engine.service" "$PREFIX/share/systemd/user/umbriel-connect-remote.service"
  echo "Umbriel Connect will start on graphical login. To replace running instances now, close them and start both user services."
fi
