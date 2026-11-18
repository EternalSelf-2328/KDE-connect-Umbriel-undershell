#!/usr/bin/env bash
# Produces an unsigned release; signing keys always remain outside the repository.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CACHE="${UMBRIEL_ANDROID_TOOLS:-$HOME/.cache/umbriel-connect-android}"
JAVA_UC="${UMBRIEL_JDK:-$CACHE/jdk-21.0.13+9}"
[[ -x "$JAVA_UC/bin/java" ]] || { echo "Install JDK 21; set UMBRIEL_JDK to its directory. See docs/BUILDING.md" >&2; exit 1; }
[[ -f "$CACHE/full-sdk/platforms/android-37.0/package.xml" && -f "$CACHE/full-sdk/build-tools/37.0.0/package.xml" ]] || { echo "Install the pinned SDK: python3 scripts/android-full/sdk.py" >&2; exit 1; }
if [[ -e "$ROOT/.git" ]]; then git -C "$ROOT" submodule update --init vendor/kdeconnect-android; fi
python3 "$ROOT/scripts/source_integrity.py" kdeconnect-android
python3 "$ROOT/scripts/android-full/prepare.py"
cd "$ROOT/build/android-full-source"
if [[ $# == 0 ]]; then set -- assembleRelease; fi
JAVA_HOME="$JAVA_UC" PATH="$JAVA_UC/bin:$PATH" ANDROID_HOME="$CACHE/full-sdk" bash gradlew --no-daemon --max-workers=2 "$@"
