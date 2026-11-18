#!/usr/bin/env bash
# Build/test/stage without changing the host. Dependency installation belongs to containers.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${1:?usage: packaging/build.sh PREFIX STAGING_DIRECTORY}"
STAGE="${2:?usage: packaging/build.sh PREFIX STAGING_DIRECTORY}"
[[ "$PREFIX" = /* && "$STAGE" = /* && "$STAGE" != / ]] || { echo 'Use absolute prefix/staging paths.' >&2; exit 2; }
for tool in openssl ssh-keygen ip; do command -v "$tool" >/dev/null || { echo "Missing test dependency: $tool" >&2; exit 1; }; done
python3 "$ROOT/scripts/source_integrity.py" kdeconnect-kde
bash "$ROOT/scripts/engine/build.sh" "$PREFIX"
bash "$ROOT/tests/engine.sh"
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_INSTALL_LIBDIR=lib -DBUILD_TESTING=ON -DUMBRIEL_REQUIRE_DESKTOP_FEATURES=ON
cmake --build "$ROOT/build" --parallel "${UMBRIEL_BUILD_JOBS:-2}"
ctest --test-dir "$ROOT/build" --output-on-failure
DESTDIR="$STAGE" cmake --install "$ROOT/build/engine" --strip
DESTDIR="$STAGE" cmake --install "$ROOT/build" --strip
mkdir -p "$STAGE$PREFIX/share/licenses/umbriel-connect" "$STAGE$PREFIX/share/doc/umbriel-connect"
cp "$ROOT/LICENSE" "$STAGE$PREFIX/share/licenses/umbriel-connect/"
cp "$ROOT/README.md" "$ROOT/THIRD_PARTY_NOTICES.md" "$STAGE$PREFIX/share/doc/umbriel-connect/"
cp -r "$ROOT/vendor/kdeconnect-kde/LICENSES" "$STAGE$PREFIX/share/licenses/umbriel-connect/engine"
