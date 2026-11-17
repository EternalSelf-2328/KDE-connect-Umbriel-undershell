#!/usr/bin/env bash
# Run the fork's Android integration/UI tests; upstream packages have a separate harness.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
arguments=(testDebugUnitTest)
for source in "$ROOT"/android/tests/*Test.java; do
    name="${source##*/}"
    arguments+=(--tests "org.eternalself.umbrielremote.${name%.java}")
done
exec bash "$ROOT/scripts/android-full/build.sh" "${arguments[@]}"
