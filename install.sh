#!/usr/bin/env bash
# Native client only. Does not start a second protocol daemon or edit shell configs.
set -euo pipefail
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
case "${1:-}" in
  --help|-h)
    echo "Usage: ./install.sh [--prefix DIR]"
    echo "Builds, tests and installs the C++ client. Default prefix: ~/.local"
    echo "Legacy Caelestia installer: bash scripts/install-legacy.sh --no-shell"
    exit 0 ;;
  --prefix)
    [[ $# == 2 ]] || { echo "Missing installation prefix" >&2; exit 2; }
    PREFIX="$2" ;;
  "") PREFIX="$HOME/.local" ;;
  *) echo "Unknown option: $1 (see --help)" >&2; exit 2 ;;
esac
command -v kdeconnectd >/dev/null || { echo "Install KDE Connect first (Arch: sudo pacman -S kdeconnect)" >&2; exit 1; }
cmake -S "$SRC" -B "$SRC/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build "$SRC/build" --parallel 2
ctest --test-dir "$SRC/build" --output-on-failure
cmake --install "$SRC/build"
echo "Installed: $PREFIX/bin/umbriel-connect"
echo "Optional Noctalia plugin: $PREFIX/share/umbriel-connect/noctalia/umbriel-connect"
echo "Open the app to select a phone and review its remote controls."
