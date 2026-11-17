#!/usr/bin/env bash
# Build and install the source-pinned private engine and native UI; no global service/network changes.
set -euo pipefail
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
case "${1:-}" in
  --help|-h)
    echo "Usage: ./install.sh [--prefix DIR]"
    echo "Builds, tests and installs Umbriel Connect and its private engine. Default: ~/.local"
    echo "Dependencies and Android build: docs/BUILDING.md"
    exit 0 ;;
  --prefix) [[ $# == 2 ]] || { echo "Missing prefix" >&2; exit 2; }; PREFIX="$2" ;;
  "") PREFIX="$HOME/.local" ;;
  *) echo "Unknown option: $1 (see --help)" >&2; exit 2 ;;
esac
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
  mkdir -p "$BACKUP/bin"
  for name in umbriel-connect umbriel-connect-remote umbriel-remote-session; do
    [[ ! -f "$PREFIX/bin/$name" ]] || cp -a "$PREFIX/bin/$name" "$BACKUP/bin/"
  done
  [[ ! -d "$PREFIX/lib/umbriel-connect/engine" ]] || cp -a "$PREFIX/lib/umbriel-connect/engine" "$BACKUP/engine"
  echo "Previous binaries: $BACKUP"
fi
cmake --install "$SRC/build/engine" --strip
cmake --install "$SRC/build" --strip
echo "Installed: $PREFIX/bin/umbriel-connect"
echo "Pair the full phone app with the new engine; old KDE Connect trust is not imported."
echo "Reopen the app to load the update. No running desktop/VPN session was restarted."
