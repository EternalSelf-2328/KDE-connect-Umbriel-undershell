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

"$JDK/bin/javac" -d "$OUT" "$ROOT/android/src/org/eternalself/umbrielremote/TransferScope.java" "$ROOT/tests/TransferScopeTest.java"
"$JDK/bin/java" -cp "$OUT" org.eternalself.umbrielremote.TransferScopeTest

"$JDK/bin/javac" -d "$OUT" "$ROOT/android/src/org/eternalself/umbrielremote/EngineSessionLease.java" "$ROOT/tests/EngineSessionLeaseTest.java"
"$JDK/bin/java" -cp "$OUT" EngineSessionLeaseTest

mkdir -p "$OUT/payloads"
"$JDK/bin/javac" -d "$OUT/payloads" "$ROOT/tests/java-stubs/EngineGate.java" "$ROOT/android/full/EnginePayloads.java" "$ROOT/tests/EnginePayloadsTest.java"
"$JDK/bin/java" -cp "$OUT/payloads" org.eternalself.umbrielremote.EnginePayloadsTest

"$JDK/bin/javac" -d "$OUT" "$ROOT/android/src/org/eternalself/umbrielremote/EngineBackgroundPolicy.java" "$ROOT/tests/EngineBackgroundPolicyTest.java"
"$JDK/bin/java" -cp "$OUT" EngineBackgroundPolicyTest
