#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec bash "$ROOT/tests/private-session.sh" ctest --test-dir "$ROOT/build/engine" --output-on-failure "$@"
