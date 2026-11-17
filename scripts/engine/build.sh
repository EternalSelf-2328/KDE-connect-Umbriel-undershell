#!/usr/bin/env bash
# Build the pinned C++ engine. No KDE Connect installation, services or root required.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PREFIX="${1:-$HOME/.local}"
ENGINE="$PREFIX/lib/umbriel-connect/engine"
python3 "$ROOT/scripts/engine/prepare.py"
EXTRA=()
if [[ -d "$ROOT/build/deps/usr/share/ECM/cmake" ]]; then EXTRA+=("-DECM_DIR=$ROOT/build/deps/usr/share/ECM/cmake"); fi
cmake -S "$ROOT/build/engine-source" -B "$ROOT/build/engine" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$ENGINE" -DKDE_INSTALL_USE_QT_SYS_PATHS=OFF \
  -DKDE_INSTALL_LIBDIR=lib -DKDE_INSTALL_PLUGINDIR=lib/plugins -DKDE_INSTALL_QMLDIR=lib/qml \
  -DCMAKE_INSTALL_RPATH="$ENGINE/lib" -DBUILD_TESTING=ON -DINSTALL_UFW_APPLICATION_RULE=OFF "${EXTRA[@]}"
cmake --build "$ROOT/build/engine" --parallel "${UMBRIEL_BUILD_JOBS:-2}"
echo "Built private engine. Review tests before installing: cmake --install $ROOT/build/engine --strip"
