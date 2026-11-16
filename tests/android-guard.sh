#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JDK="${UMBRIEL_JDK:-$HOME/.cache/umbriel-connect-android/jdk-21.0.13+9}"
OUT="$ROOT/build/android-guard"
mkdir -p "$OUT"
"$JDK/bin/javac" -d "$OUT" "$ROOT/android/src/org/eternalself/umbrielremote/SessionGuard.java" "$ROOT/tests/SessionGuardTest.java"
"$JDK/bin/java" -cp "$OUT" SessionGuardTest

"$JDK/bin/javac" -d "$OUT" "$ROOT/android/src/org/eternalself/umbrielremote/MotionBuffer.java" "$ROOT/tests/MotionBufferTest.java"
"$JDK/bin/java" -cp "$OUT" org.eternalself.umbrielremote.MotionBufferTest
